/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
//#include "web.h"
#include "as.h"
#include <signal.h>

#define AS_VERSION  "0.1"   /**< version */

const char as_db_name[] = "lora_app";
conf_t conf;
struct _tlist* tlist = NULL;

CURLM *multi_handle;
mqd_t mqwd;

key_envelope_t key_envelope_app;
char myAS_ID[64];
uint32_t next_req_tid;

char joinDomain[64];

char netIdDomain[64];

volatile mainop_e mainOp;

int
get_key_from_js()
{
    int ret = 0;

    return ret;
}

#if 0
{
  "deveui": "1122334455667788",
  "dataFrame": "AB==",
  "port": 1,
  "timestamp": "2015-02-11T10:33:00.578Z",
  "fcnt": 138,
  "rssi": -111,
  "snr": -6,
  "sf_used": 8,
  "id": 278998,
  "live": true,
  "decrypted": false,
  "gtw_info": [
    {
      "gtw_id": "0000000012340000",
      "rssi": -100,
      "snr": 5
    }
  ]
}
#endif /* if 0 */


static int
cayenne_post(const ULMetaData_t* ulmd, const uint8_t* payload, size_t payLen)
{
    CURL* easy;
    int nxfers, ret = -1;
    const char url[] = "https://lora.mydevices.com/v1/networks/semtech/uplink";
    json_object* jobj = json_object_new_object();
    char* enc;
    size_t encLen;
    char str[64];
    struct _gwList* mygwl;
    int8_t snr = SCHAR_MIN;
    int8_t rssi = SCHAR_MIN;
    static unsigned id;

    sprintf(str, "%"PRIx64, ulmd->DevEUI);
    json_object_object_add(jobj, "deveui", json_object_new_string(str));
    enc = (char*)base64_encode(payload, payLen, &encLen);
    json_object_object_add(jobj, "dataFrame", json_object_new_string(enc));
    json_object_object_add(jobj, "port", json_object_new_int(ulmd->FPort));
    json_object_object_add(jobj, "timestamp", json_object_new_string(ulmd->RecvTime));
    json_object_object_add(jobj, "fcnt", json_object_new_int(ulmd->FCntUp));
    json_object_object_add(jobj, "sf_used", json_object_new_int(7));

    json_object_object_add(jobj, "id", json_object_new_int(id));
    json_object_object_add(jobj, "live", json_object_new_boolean(true));
    json_object_object_add(jobj, "decrypted", json_object_new_boolean(false));
    if (ulmd->gwList) {
        json_object* jarray = json_object_new_array();
        for (mygwl = ulmd->gwList; mygwl; mygwl = mygwl->next) {
            json_object* go  = json_object_new_object();
            GWInfo_t* gi = mygwl->GWInfo;
            sprintf(str, "%"PRIx64, gi->id);
            json_object_object_add(go, "gtw_id", json_object_new_string(str));
            json_object_object_add(go, "rssi", json_object_new_int(gi->RSSI));
            json_object_object_add(go, "snr", json_object_new_int(gi->SNR));

            json_object_array_add(jarray, go);
            if (gi->SNR > snr)
                snr = gi->SNR;
            if (gi->RSSI > rssi)
                rssi = gi->RSSI;
        }
        json_object_object_add(jobj, "gtw_info", jarray);
        json_object_object_add(jobj, "rssi", json_object_new_int(rssi));
        json_object_object_add(jobj, "snr", json_object_new_int(snr));
    }


    printf("###### %s\n", json_object_to_json_string(jobj));
    
    easy = curl_easy_init();
    if (!easy)
        return ret;
    curl_multi_add_handle(multi_handle, easy);

    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 0L);

    ret = http_post_url(easy, jobj, url, false);
    CURLMcode mc = curl_multi_perform(multi_handle, &nxfers);
    printf(" %s = curl_multi_perform(),%d ", curl_multi_strerror(mc), nxfers);

    //json_object_put(jobj);

    id++;

    return ret;
} // ..cayenne_post()

static const char*
forward_uplink(MYSQL* sc, const char* where, const ULMetaData_t* ulmd, const uint8_t* payload, size_t payLen)
{

    MYSQL_RES *result;
    MYSQL_ROW row;
    char fwdTo[32];
    char query[512];

    strcpy(query, "SELECT forwardTo FROM activemotes WHERE ");
    strcat(query, where);
    if (mysql_query(sc, query)) {
        printf("%s: %s", query, mysql_error(sc));
        return Other;
    }
    result = mysql_use_result(sc);
    if (result == NULL) {
        printf("activemotes no result, %s", where);
        return Other;
    }
    row = mysql_fetch_row(result);
    if (!row) {
        printf("activemotes no row, %s", where);
        mysql_free_result(result);
        return Other;
    }
    if (row[0])
        strncpy(fwdTo, row[0], sizeof(fwdTo));
    else
        row[0] = 0;

    mysql_free_result(result);

    if (strcmp(fwdTo, Cayenne) == 0)
        cayenne_post(ulmd, payload, payLen);

    return Success;
}

static const char *
save_frame(MYSQL* sc, const ULMetaData_t* ulmd, const uint8_t* decrypted, uint8_t payLen)
{
    unsigned n;
    struct tm timeinfo;
    time_t t;
    char str[128];
    char where[128];
    char query[2048];
    unsigned long id;
    MYSQL_RES *result;
    MYSQL_ROW row;
    struct _gwList* mygwl;
    int8_t snr = SCHAR_MIN;
    int8_t rssi = SCHAR_MIN;
    const char* ret;

    if (ulmd->DevEUI == NONE_DEVEUI) {
        sprintf(where, "DevAddr = %u", ulmd->DevAddr);
        ret = UnknownDevAddr;
    } else {
        sprintf(where, "eui = %"PRIu64, ulmd->DevEUI);
        ret = UnknownDevEUI;
    }

    sprintf(query, "SELECT ID FROM activemotes WHERE %s", where);
    if (mysql_query(sc, query)) {
        printf("%s: %s", query, mysql_error(sc));
        return Other;
    }
    result = mysql_use_result(sc);
    if (result == NULL) {
        printf("activemotes no result, %s", where);
        return Other;
    }
    row = mysql_fetch_row(result);
    if (!row) {
        printf("activemotes no row, %s", where);
        mysql_free_result(result);
        return ret;
    }
    sscanf(row[0], "%lu", &id);

    mysql_free_result(result);

    strptime(ulmd->RecvTime, "%FT%T%Z", &timeinfo);
    timeinfo.tm_isdst = 0;
    t = mktime(&timeinfo);

    //printf("save frame id:%lu, time:%s,%lu  ", id, ulmd->RecvTime, t);

    for (mygwl = ulmd->gwList; mygwl; mygwl = mygwl->next) {
        GWInfo_t* gi = mygwl->GWInfo;
        if (gi->SNR > snr)
            snr = gi->SNR;
        if (gi->RSSI > rssi)
            rssi = gi->RSSI;
    }

    strcpy(query, "INSERT INTO frames (");
    if (ulmd->GWCnt > 0)
        strcat(query, "GWCnt, RSSI, SNR, ");
    if (ulmd->ULFreq > 0)
        strcat(query, "ULFreq, ");
    if (ulmd->DataRate != UCHAR_MAX)
        strcat(query, "DataRate, ");
    if (payLen > 0)
        strcat(query, "FRMPayload, ");

    strcat(query, "ID, RecvTime, FPort, FCntUp, Confirmed) VALUES (");

    if (ulmd->GWCnt > 0) {
        sprintf(str, "%u", ulmd->GWCnt);
        strcat(query, str);
        strcat(query, ", ");
        sprintf(str, "%d", rssi);
        strcat(query, str);
        strcat(query, ", ");
        sprintf(str, "%d", snr);
        strcat(query, str);
        strcat(query, ", ");
    }
    if (ulmd->ULFreq > 0) {
        sprintf(str, "%u", (unsigned)(ulmd->ULFreq * 1000000)); // MHz to Hz
        strcat(query, str);
        strcat(query, ", ");
    }
    if (ulmd->DataRate != UCHAR_MAX) {
        sprintf(str, "%u", ulmd->DataRate);
        strcat(query, str);
        strcat(query, ", ");
    }
    if (payLen > 0) {
        strcat(query, "0x");
        for (n = 0; n < payLen; n++) {
            sprintf(str, "%02x", decrypted[n]);
            strcat(query, str);
        }
        strcat(query, ", ");
    }

    sprintf(str, "%lu", id);
    strcat(query, str);
    strcat(query, ", FROM_UNIXTIME(");
    sprintf(str, "%lu", t);
    strcat(query, str);
    strcat(query, "), ");
    sprintf(str, "%u", ulmd->FPort);
    strcat(query, str);
    strcat(query, ", ");
    sprintf(str, "%u", ulmd->FCntUp);
    strcat(query, str);
    strcat(query, ", ");
    sprintf(str, "%u", ulmd->Confirmed);
    strcat(query, str);
    strcat(query, ")");
    //printf("%s\n", query);

    if (mq_send(mqwd, query, strlen(query)+1, 0) < 0) {
        perror("mq_send");
        return Other;
    }

    forward_uplink(sc, where, ulmd, decrypted, payLen);

    return Success;
} // ..save_frame()

const char*
app_uplink(MYSQL* sc, const char* frmStr, const ULMetaData_t* ulmd, const uint8_t* AppSKeyBin)
{
    int i, o;
    uint8_t decrypted[256];
    uint8_t encrypted[256];
    const char* frmStrEnd = frmStr + strlen(frmStr);

    //printf("app_uplink(%s,)\n", frmStr);

    o = 0;
    while (frmStr < frmStrEnd) {
        unsigned int octet;
        sscanf(frmStr, "%02x", &octet);
        encrypted[o++] = octet;
        frmStr += 2;
    }

    LoRa_Encrypt(1, AppSKeyBin, encrypted, o, ulmd->DevAddr, true, ulmd->FCntUp, decrypted);

    printf("decrypted: ");
    for (i = 0; i < o; i++)
        printf("%02x ", decrypted[i]);
    printf("   ");
    for (i = 0; i < o; i++) {
        if (decrypted[i] < ' ' || decrypted[i] > '~')
            putchar('.');
        else
            putchar(decrypted[i]);
    }
    putchar('\n');

    return save_frame(sc, ulmd, decrypted, o);
} // ..app_uplink()

int
writeDownlinkStatus(MYSQL* sc, uint64_t devEui, uint32_t devAddr, const char* text)
{
    char query[768];
    char where[128];
    int o, i, ret = -1;
    char txt[128];
    size_t len = strlen(text);

    for (o = 0, i = 0; i < len; i++) {
        if (text[i] == '\'') {
            txt[o++] = '\\';
            txt[o++] = '\'';
        } else
            txt[o++] = text[i];
    }
    txt[o] = 0;

    if (devEui != NONE_DEVEUI)
        sprintf(where, "eui = %"PRIu64, devEui);
    else
        sprintf(where, "DevAddr = %u", devAddr);

    sprintf(query, "UPDATE activemotes SET downlinkStatus = '%s' WHERE %s", txt, where);
    if (mysql_query(sc, query)) {
        printf("\e[31m%s: %s\e[0m\n", query, mysql_error(sc));
        return ret;
    }
    i = mysql_affected_rows(sc);
    if (i != 1) {
        MYSQL_RES *result;
        sprintf(query, "SELECT downlinkStatus FROM activemotes WHERE %s", where);
        if (mysql_query(sc, query)) {
            printf("%s: %s", query, mysql_error(sc));
            return ret;
        }
        result = mysql_use_result(sc);
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row) {
                if (row[0] && strcmp(row[0], text) == 0)
                    ret = 0;
            }
            mysql_free_result(result);
        }
        if (ret < 0)
            printf("\e[31m%s: affected rows %d\e[0m\n", query, i);
        return ret;
    } else {
        ret = 0;
        printf("wrote downlinkStatus \"%s\" to %s\n", text, where);
    }

    return ret;
}

void
tlistAdd(const msg_t* msg, uint32_t reqSentTid)
{
    struct _tlist* list;

    printf("add tlist %016"PRIx64" / %08x\n", msg->devEui, msg->devAddr);
    if (tlist) {
        for (list = tlist; list; list = list->next) {
            printf(" (%016"PRIx64" / %08x) ", list->msg.devEui, list->msg.devAddr);
            if (list->msg.devEui == NONE_DEVEUI && list->msg.devAddr == NONE_DEVADDR)
                goto add;
        }

        for (list = tlist; list->next; list = list->next)
            ;

        list->next = malloc(sizeof(struct _tlist));
        list = list->next;
        list->next = NULL;
    } else {
        tlist = malloc(sizeof(struct _tlist));
        tlist->next = NULL;
        list = tlist;
    }

add:
    list->tid = reqSentTid;

    memcpy(&list->msg, msg, sizeof(msg_t));
    if (msg->payLen > 0) {
        list->msg.payload = malloc(msg->payLen);
        memcpy(list->msg.payload, msg->payload, msg->payLen);
    } else
        list->msg.payload = NULL;

    printf("\n");
}

static json_object*
AS_generateDLMetaData(uint64_t devEui, uint32_t devAddr, uint8_t fport, uint32_t fcntdown, bool conf)
{
    char buf[64];

    json_object* ret = json_object_new_object();

    if (devEui != NONE_DEVEUI) {
        sprintf(buf, "%"PRIx64, devEui);
        json_object_object_add(ret, DevEUI, json_object_new_string(buf));
    } else {
        sprintf(buf, "%x", devAddr);
        json_object_object_add(ret, DevAddr, json_object_new_string(buf));
    }

    json_object_object_add(ret, FPort, json_object_new_int(fport));
    json_object_object_add(ret, FCntDown, json_object_new_int(fcntdown));
    if (conf)
        json_object_object_add(ret, Confirmed, json_object_new_boolean(true));

    return ret;
}

int
send_downlink(MYSQL* sc, char* failMsg, size_t sizeof_failMsg, msg_t* msg)
{
    CURL* easy;
    char *ptr, query[512];
    MYSQL_RES *result;
    MYSQL_ROW row;
    uint8_t txFRMPayload[256];
    uint8_t AppSKeyBin[LORA_CYPHERKEYBYTES];
    uint32_t AFCntDown;
    int nxfers = 0, ret = -1;
    json_object* jobj;
    uint32_t reqSentTid;
    char netIDstr[24];
    int o;
    uint32_t NetID24, NwkID, NwkAddr;
    char strbuf[128];
    
    printf("AS send_downlink() ");

    if (msg->devEui != NONE_DEVEUI) {
        /* OTA */
        sprintf(query, "SELECT DevAddr FROM activemotes WHERE eui = %"PRIu64, msg->devEui);
        if (mysql_query(sc, query)) {
            snprintf(failMsg, sizeof_failMsg , "\e[31msend_downlink get DevAddr: %s\e[0m", mysql_error(sc));
            return ret;
        }
        result = mysql_use_result(sc);
        if (result) {
            row = mysql_fetch_row(result);
            if (row && row[0])
                sscanf(row[0], "%u", &msg->devAddr);
            else {
                snprintf(failMsg, sizeof_failMsg ,"%s: no row", query);
                mysql_free_result(result);
                return ret;
            }
            mysql_free_result(result);
        } else {
            snprintf(failMsg, sizeof_failMsg ,"%s: no result", query);
            return ret;
        }
        sprintf(strbuf, "eui = %"PRIu64, msg->devEui);
    } else {
        /* ABP */
        if (msg->devAddr == NONE_DEVADDR) {
            snprintf(failMsg, sizeof_failMsg ,"no devAddr");
            return ret;
        }
        sprintf(strbuf, "DevAddr = %u", msg->devAddr);
        //devEui = NONE_DEVEUI;
    }

    printf(" deveui %016"PRIx64". ", msg->devEui);

    sprintf(query, "SELECT AppSKey, AFCntDown FROM activemotes WHERE %s", strbuf);
    printf("\n%s\n", query);
    if (mysql_query(sc, query)) {
        snprintf(failMsg, sizeof_failMsg , "%s: %s", query, mysql_error(sc));
        return ret;
    }
    result = mysql_use_result(sc);
    if (result == NULL) {
        snprintf(failMsg, sizeof_failMsg, "%s: no result", query);
        return ret;
    }
    row = mysql_fetch_row(result);
    if (!row) {
        snprintf(failMsg, sizeof_failMsg, "%s: no row\n", query);
        mysql_free_result(result);
        return ret;
    }
    if (!row[0]) {
        snprintf(failMsg, sizeof_failMsg, "%s: no AppSKey", query);
        mysql_free_result(result);
        return ret;
    }

    memcpy(AppSKeyBin, row[0], LORA_CYPHERKEYBYTES);

    if (row[1] != NULL) {
        sscanf(row[1], "%u", &AFCntDown);
    } else
        AFCntDown = 0;

    mysql_free_result(result);

    printf("\e[35mencrypting-with-AFCntDown%u, %08x ", AFCntDown, msg->devAddr);
    print_buf(AppSKeyBin, LORA_CYPHERKEYBYTES, "AppSKey");
    printf("\e[0m");

    LoRa_Encrypt(1, AppSKeyBin, msg->payload, msg->payLen, msg->devAddr, false, AFCntDown, txFRMPayload);

    /********** done, make json reqest ******/
    if (parseDevAddr(msg->devAddr, &NetID24, &NwkID ,&NwkAddr) == UINT_MAX) {
        snprintf(failMsg, sizeof_failMsg , "cant get NetID from DevAddr %08x", msg->devAddr);
        return ret;
    }
    jobj = json_object_new_object();

    ptr = query;
    for (o = 0; o < msg->payLen; o++) {
        sprintf(ptr, "%02x", txFRMPayload[o]);
        ptr += 2;
    }
    json_object_object_add(jobj, FRMPayload, json_object_new_string(query));

    json_object_object_add(jobj, DLMetaData, AS_generateDLMetaData(msg->devEui, msg->devAddr, msg->fport, AFCntDown, msg->conf));

    reqSentTid = next_req_tid++;
    sprintf(netIDstr, "%06x", NetID24);
    lib_generate_json(jobj, netIDstr, myAS_ID, reqSentTid, XmitDataReq, NULL);

    printf("toNS: %s\n", json_object_to_json_string(jobj));

    easy = curl_easy_init();
    if (!easy)
        return ret;
    curl_multi_add_handle(multi_handle, easy);

    sprintf(strbuf, "%06x.%s", NetID24, netIdDomain);
    printf("AS posting to %s ---- %s\n", strbuf, json_object_to_json_string(jobj));
    ret = http_post_hostname(easy, jobj, strbuf, true);
    CURLMcode mc = curl_multi_perform(multi_handle, &nxfers);
    printf(" %s = curl_multi_perform(),%d ", curl_multi_strerror(mc), nxfers);

    if (ret == 0) {
        printf("OK\n");
        tlistAdd(msg, reqSentTid);
        if (writeDownlinkStatus(sc, msg->devEui, msg->devAddr, "SENT") < 0)  {
            snprintf(failMsg, sizeof_failMsg, "writeDownlinkStatus() failed ");
        } else
            ret = 0;
    } else {
        snprintf(failMsg, sizeof_failMsg, "%d = http_post_hostname(%s)", ret, strbuf);
        printf("FAIL\n");
    }


    return ret;
} // ..send_downlink()

int as_conf_json(json_object *jobjSrv, conf_t* c)
{
    json_object *obj;

    if (json_object_object_get_ex(jobjSrv, "AS_ID", &obj)) {
        snprintf(myAS_ID, sizeof(myAS_ID), "%s:%u", json_object_get_string(obj), c->httpd_port);
        //strncpy(myAS_ID, json_object_get_string(obj), sizeof(myAS_ID));
    } else {
        printf("no AS_ID\n");
        return -1;
    }

    if (parse_json_KeyEnvelope("KeyEnvelopeApp", jobjSrv, &key_envelope_app) < 0)
        return -1;


    return 0;
}

static int
clear_sent(MYSQL* sc)
{
    struct _tlist* list = NULL;
    MYSQL_RES *result;

    if (mysql_query(sc, "SELECT eui, DevAddr FROM activemotes WHERE downlinkStatus = 'SENT'")) {
        printf("\e[31mactivemotes downlinkStatus : %s\e[0m\n", mysql_error(sc));
        return -1;
    }
    result = mysql_use_result(sc);
    if (result) {
        struct _tlist* my;
        MYSQL_ROW row;
        list = malloc(sizeof(struct _tlist));
        list->msg.devAddr = NONE_DEVADDR;
        list->next = NULL;
        my = list;
        while ((row = mysql_fetch_row(result))) {
            if (row[0]) {
                sscanf(row[0], "%"PRIu64, &my->msg.devEui);
            } else
                my->msg.devEui = NONE_DEVEUI;

            if (row[1]) {
                sscanf(row[1], "%u", &my->msg.devAddr);
            }
           
            my->next = malloc(sizeof(struct _tlist));
            my = my->next;
            my->msg.devAddr = NONE_DEVADDR;
            my->next = NULL;
        }
        mysql_free_result(result);
    } // ..if (result)
    else {
        printf("no sent result\n");
        return -1;
    }

    while (list) {
        struct _tlist* next = list->next;

        if (list->msg.devAddr != NONE_DEVADDR) {
            int i;
            char where[128];
            char query[768];
            if (list->msg.devEui != NONE_DEVEUI)
                sprintf(where, "eui = %"PRIu64, list->msg.devEui);
            else
                sprintf(where, "DevAddr = %u", list->msg.devAddr);
            sprintf(query, "UPDATE activemotes SET downlinkStatus = NULL WHERE %s", where);
            if (mysql_query(sc, query)) {
                printf("%s\n", mysql_error(sc));
                return -1;
            }
            i = mysql_affected_rows(sc);
            printf("%d = mysql_affected_rows()\n", i);
        }

        free(list);
        list = next;
    } // ..while (list)

    return 0;
}

const char* mq_name = "/as";
bool run;
void
intHandler(int dummy)
{
    mq_close(mqwd);
    printf("mq_close\n");
    fflush(stdout);
    mq_unlink(mq_name);
    run = false;
}

int sessionCreate(struct Session* s)
{
    s->sqlConn = mysql_init(NULL);
    if (s->sqlConn == NULL) {
        fprintf(stderr, "Failed to initialize: %s\n", mysql_error(s->sqlConn));
        return -1;
    }

    /* enable re-connect */
    my_bool reconnect = 1;
    if (mysql_options(s->sqlConn, MYSQL_OPT_RECONNECT, &reconnect) != 0) {
        fprintf(stderr, "mysql_options() failed\n");
        return -1;
    }

    printf("database connect %s\n", as_db_name);
    /* Connect to the server */
    if (!mysql_real_connect(s->sqlConn, conf.sql_hostname, conf.sql_username, conf.sql_password, as_db_name, conf.sql_port, NULL, 0))
    {
        fprintf(stderr, "Failed to connect to server: %s\n", mysql_error(s->sqlConn));
        return -1;
    }

    return 0;
}

void sessionEnd(struct Session* s)
{
    mysql_close(s->sqlConn);
}

pid_t pid;

struct MHD_Daemon *
init(const char* conf_filename)
{
    struct mq_attr attr;
    struct MHD_Daemon *ret;

    if (parse_server_config(conf_filename, as_conf_json, &conf) < 0) {
        return NULL;
    }
    strcpy(joinDomain, conf.joinDomain);
    strcpy(netIdDomain, conf.netIdDomain);

    // attr.mq_flags; 0 or O_NONBLOCK
    attr.mq_maxmsg = 7;    // max # of message on queue
    attr.mq_msgsize = MSGSIZE; // max message size in bytes
    // attr.mq_curmsgs; # of messages sitting in queue
    printf("mq mq_maxmsg:%lu mq_msgsize:%lu\n", attr.mq_maxmsg, attr.mq_msgsize);
    mqwd = mq_open(mq_name, O_WRONLY | O_CREAT, 0666, &attr);
    if (mqwd == (mqd_t)-1) {
        perror("mq_open");
        return NULL;
    }

    {
        pid = fork();
        if (pid < 0) {
            perror("fork");
            return NULL;
        } else if (pid == 0) {
            // child process
            printf("child start %u\n", pid);
            exit(child(mq_name, conf.sql_hostname, conf.sql_username, conf.sql_password, conf.sql_port, as_db_name));
        } else {
            // parent process
            //printf("parent mqd %d\n", mqwd);
            signal(SIGINT, intHandler);
        }
    }


    ret = MHD_start_daemon (MHD_USE_ERROR_LOG,
        conf.httpd_port, /* unsigned short port*/
        NULL, NULL, /* MHD_AcceptPolicyCallback apc, void *apc_cls */
        &lib_create_response, NULL, /* MHD_AccessHandlerCallback dh, void *dh_cls */
        MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 15,
        MHD_OPTION_NOTIFY_COMPLETED, &lib_request_completed_callback, NULL,
        MHD_OPTION_END
    );
    if (ret != NULL)
        printf("httpd port %u\n", conf.httpd_port);

    srand(time(NULL));
    next_req_tid = rand();

    return ret;
}

int
main (int argc, char **argv)
{
    MYSQL* mysql;
    struct timeval tv;
    struct timeval *tvp;
    MHD_UNSIGNED_LONG_LONG mhd_timeout;
    struct MHD_Daemon *d;
    fd_set rs;
    fd_set ws;
    fd_set es;
    MHD_socket max;
    int opt;
    char conf_filename[96]; /**< file name of server JSON configuration */

    strcpy(conf_filename, "../app_server/conf.json");  // default conf file

    while ((opt = getopt(argc, argv, "n:t")) != -1) {
        switch (opt) {
            case 'c':
                strncpy(conf_filename, optarg, sizeof(conf_filename));
                break;
            default: /* '?' */
                fprintf(stderr, "Usage: %s [-t nsecs] [-c conf_file] \n", argv[0]);
                return -1;
        }
    }

    d = init(conf_filename);
    if (!d)
        return -1;

    mysql = mysql_init(NULL);
    if (mysql == NULL) {
        fprintf(stderr, "Failed to initialize: %s\n", mysql_error(mysql));
        return -1;
    }

    /* enable re-connect */
    my_bool reconnect = 1;
    if (mysql_options(mysql , MYSQL_OPT_RECONNECT, &reconnect) != 0) {
        fprintf(stderr, "mysql_options() failed\n");
        return -1;
    }

    printf("database connect %s\n", as_db_name);
    /* Connect to the server */
    if (!mysql_real_connect(mysql, conf.sql_hostname, conf.sql_username, conf.sql_password, as_db_name, conf.sql_port, NULL, 0))
    {
        fprintf(stderr, "Failed to connect to server: %s\n", mysql_error(mysql));
        return -1;
    }

    /* clear stale sent state */
    if (clear_sent(mysql) < 0) {
        printf("clear_sent failed\n");
        return -1;
    }

    curl_global_init(CURL_GLOBAL_ALL);

    /*curl = curl_easy_init();
    if (!curl)
        return -1;*/

    multi_handle = curl_multi_init();

    for (run = true; run; ) {
        lib_expire_sessions ();
        max = 0;
        FD_ZERO (&rs);
        FD_ZERO (&ws);
        FD_ZERO (&es);
        if (MHD_YES != MHD_get_fdset (d, &rs, &ws, &es, &max))
            break; /* fatal internal error */
        if (MHD_get_timeout (d, &mhd_timeout) == MHD_YES)
        {
            tv.tv_sec = mhd_timeout / 1000;
            tv.tv_usec = (mhd_timeout - (tv.tv_sec * 1000)) * 1000;
            tvp = &tv;
        }
        else
            tvp = NULL;
        if (-1 == select (max + 1, &rs, &ws, &es, tvp))
        {
            if (EINTR != errno)
                abort ();
        }
        MHD_run (d);
        //printf("\e[34mmainloop\e[0m\n");
        if (mainOp != MAIN_OP_NONE ) {
            if (mainOp == MAIN_OP_GETKEY) {
                printf("MAIN_OP_GETKEY\n");
                get_key_from_js();
            }
            mainOp = MAIN_OP_NONE ;
        }

        curl_service(mysql, multi_handle, 100);

    } // ..while (1)
    MHD_stop_daemon (d);

    curl_global_cleanup();

    return 0;
}

