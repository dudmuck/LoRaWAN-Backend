/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "ns.h"

typedef struct {
    bool sendAppSKey;

    struct {
        unsigned long reqTid;
        char* ansDest;
        json_object* ansJobj;
        const char* rxResult;
    } downlink;
} h_t;

static void
hNS_JoinAnsCallback(MYSQL* sc, mote_t* mote, json_object* jobj, const char* rxResult, const char* senderID, const uint8_t* rfBuf, uint8_t rfLen)
{
    struct mq_attr attr;
    json_object* obj;
    json_object *ko;
    json_object *lo;
    const char* res;
    my_ulonglong id;
    char query[512];
    h_t* h = mote->h;
    char str[64];

    if (hNs_to_sNS_JoinAns(sc,mote, jobj, rxResult, senderID, rfBuf, rfLen) < 0) {
        printf("\e[31mhNS joinAns fail\e[0m\r\n");
        return;
    }

    printf("hNS joinAns ok\n");

    if (!json_object_object_get_ex(jobj, AppSKey, &obj)) {
        printf("\e[31mno %s\e[0m\n", AppSKey);
        return;
    }

    if (!json_object_object_get_ex(obj, AESKey, &ko)) {
        printf("\e[31mno %s in %s\e[0m\n", AESKey, AppSKey);
        return;
    }

    id = getMoteID(sc, mote->devEui, mote->devAddr, &res);
    if (res != Success) {
        printf("\e[31m%s = getMoteID(%"PRIx64", %08x)\n", res, mote->devEui, mote->devAddr);
        return;
    }

    strcpy(query, "UPDATE sessions SET AS_KEK_label = ");

    if (json_object_object_get_ex(obj, KEKLabel, &lo)) {
        strcat(query, "'");
        strcat(query, json_object_get_string(lo));
        strcat(query, "'"); // AESKey encrypted
    } else
        strcat(query, "NULL");  // AESKey in the clear

    strcat(query, ", AS_KEK_key = 0x");
    strcat(query, json_object_get_string(ko));
    strcat(query, " WHERE ID = ");
    sprintf(str, "%llu", id);
    strcat(query, str);
    strcat(query, " ORDER BY createdAt DESC LIMIT 1");

    mq_getattr(mqd, &attr);
    if (mq_send(mqd, query, strlen(query)+1, 0) < 0) {
        perror("\e[31mmq_send\e[0m");
        printf("strlen(query):%zu\n", strlen(query));
    }
    h->sendAppSKey = true;

} // ..hNS_JoinAnsCallback()

int 
hNS_toJS(mote_t* mote, const char* CFListStr)
{
    CURL* easy;
    const char* ReqMessageType;
    uint8_t dlSettings = 0xff;
    char* strPtr;
    char hostname[64];
    uint64_t joinEui = NONE_DEVEUI;
    char str[513];
    int nxfers, ret = -1, n;
    uint32_t tid;
    mhdr_t* mhdr = (mhdr_t*)mote->ULPayloadBin;

    if (mhdr->bits.MType == MTYPE_REJOIN_REQ) {
        ReqMessageType = RejoinReq;
        char query[512];
        rejoin1_req_t *rj1 = (rejoin1_req_t*)mote->ULPayloadBin;
        sprintf(query, "SELECT JoinEUI FROM motes WHERE DevEUI = %"PRIu64, mote->devEui);
        if (!mysql_query(sqlConn_lora_network, query)) {
            MYSQL_RES *result = mysql_use_result(sqlConn_lora_network);
            if (result) {
                MYSQL_ROW row = mysql_fetch_row(result);
                if (row) {
                    if (row[0])
                        sscanf(row[0], "%"PRIu64, &joinEui);
                } else
                    printf("\e[31mget JoinEUI: no row\e[0m\n");

                mysql_free_result(result);
            } else
                printf("\e[31mget JoinEUI: no result\e[0m\n");
        } else
            printf("\e[31mget JoinEUI: %s\e[0m\n", mysql_error(sqlConn_lora_network));

        if (joinEui == NONE_DEVEUI)
            return ret;

        if (rj1->type == 1) {
            printf("\e[31mhNS_toJS type1, previous joineui %016"PRIx64" vs rx %016"PRIx64"\e[0m\n", joinEui, eui_buf_to_uint64(rj1->JoinEUI));
            joinEui = eui_buf_to_uint64(rj1->JoinEUI);
        } else if (rj1->type > 2) {
            printf("\e[31mhNS_toJS invalid rejoin-type%u\e[0m\n", rj1->type);
            return ret;
        }   /* else rejoin types 0,2 use JoinEUI saved from last join ans */

    } else if (mhdr->bits.MType == MTYPE_JOIN_REQ) {
        ReqMessageType = JoinReq;
        join_req_t* rx_jreq_ptr = (join_req_t*)mote->ULPayloadBin;
        joinEui = eui_buf_to_uint64(rx_jreq_ptr->JoinEUI);
    } else
        return ret;

    printElapsed(mote);
    if (next_tid(mote, "hNS_toJS", hNS_JoinAnsCallback, &tid) < 0)
        return ret;

    if (mote->h == NULL)
        mote->h = calloc(1, sizeof(h_t));

    printf("hNS_toJS() ");
    fflush(stdout);
    getJsHostName(joinEui, hostname, joinDomain);

    json_object *jobj = json_object_new_object();

    {
        char destStr[64];

        sprintf(destStr, "%"PRIx64, joinEui);
        lib_generate_json(jobj, destStr, myNetwork_idStr, tid, ReqMessageType, NULL);
    }
    json_object_object_add(jobj, MACVersion, json_object_new_string("1.1"));

    strPtr = str;
    for (n = 0; n < mote->ULPHYPayloadLen; n++) {
        sprintf(strPtr, "%02x", mote->ULPayloadBin[n]);
        strPtr += 2;
    }
    json_object_object_add(jobj, PHYPayload, json_object_new_string(str));

    sprintf(str, "%"PRIx64, mote->devEui);
    json_object_object_add(jobj, DevEUI, json_object_new_string(str));
    sprintf(str, "%x", mote->devAddr);
    json_object_object_add(jobj, DevAddr, json_object_new_string(str));

    if (deviceProfileReq(sqlConn_lora_network, mote->devEui, NONE_DEVADDR, RXDataRate2, str, sizeof(str)) < 0) {
        printf("\e[31mhNS %016"PRIx64" has no %s\e[0m\n", mote->devEui, DeviceProfile);
        return ret;
    }
    sscanf(str, "%u", &n);
    dlSettings = n;
    if (deviceProfileReq(sqlConn_lora_network, mote->devEui, NONE_DEVADDR, RXDROffset1, str, sizeof(str)) < 0) {
        printf("\e[31mhNS %016"PRIx64" has no %s\e[0m\n", mote->devEui, DeviceProfile);
        return ret;
    }
    sscanf(str, "%u", &n);
    dlSettings |= n << 4;

    mote->rxdrOffset1 = n;

    sprintf(str, "%02x", dlSettings);   // RX1DrOffset, RX2DataRate
    json_object_object_add(jobj, DLSettings, json_object_new_string(str));

    if (deviceProfileReq(sqlConn_lora_network, mote->devEui, NONE_DEVADDR, RXDelay1, str, sizeof(str)) < 0) {
        printf("\e[31mhNS %016"PRIx64" has no %s\e[0m\n", mote->devEui, DeviceProfile);
        return ret;
    }
    sscanf(str, "%u", &n);
    if (n == 0)
        n = 1;
    json_object_object_add(jobj, RxDelay, json_object_new_int(n));

    if (CFListStr[0] != 0)
        json_object_object_add(jobj, CFList, json_object_new_string(CFListStr));

    printf("toJS: %s ", json_object_to_json_string(jobj));
    fflush(stdout);

    easy = curl_easy_init();
    if (!easy)
        return ret;

    ret = http_post_hostname(easy, jobj, hostname, true);
    curl_multi_add_handle(multi_handle, easy);
    CURLMcode mc = curl_multi_perform(multi_handle, &nxfers);
    if (mc != CURLM_OK)
        printf(" %s = curl_multi_perform() nxfers:%d", curl_multi_strerror(mc), nxfers);

    return ret;
} // ..hNS_toJS()

static int
sqlGetASID(uint64_t devEui, uint32_t devAddr, char* out, size_t sizeof_out)
{
    int ret = -1;
    MYSQL_RES *result;
    char query[512];
    char where[128];

    if (getMotesWhere(sqlConn_lora_network, devEui, devAddr, where) != Success) {
        printf("\e[31msqlGetASID getMotesWhere failed\e[0m\n");
        return ret;
    }
    strcpy(query, "SELECT RoutingProfiles.AS_ID FROM RoutingProfiles INNER JOIN motes ON RoutingProfiles.RoutingProfileID = motes.ID WHERE ");
    strcat(query, where);
    if (mysql_query(sqlConn_lora_network, query)) {
        fprintf(stderr, "\e[31mhNS RoutingProfiles %s\e[0m\n", mysql_error(sqlConn_lora_network));
        return ret;
    }
    result = mysql_use_result(sqlConn_lora_network);
    if (result != NULL) {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            strncpy(out, row[0], sizeof_out);
            ret = 0;
        } else
            fprintf(stderr, "\e[31mhNS RoutingProfiles \"%s\" no row\e[0m\n", where);
        mysql_free_result(result);
    } else
        fprintf(stderr, "\e[31mhNS RoutingProfiles no result: %s\e[0m\n", query);

    return ret;
}

static void
hNS_XmitDataAnsCallback_downlink(MYSQL* sc, mote_t* mote, json_object* jobj, const char* rxResult, const char* senderID, const uint8_t* rfBuf, uint8_t rfLen)
{
    h_t* h;
    json_object* o;
    unsigned n;

    printf("hNS_XmitDataAnsCallback_downlink()\n");

    if (mote->h == NULL)
        return;

    h = mote->h;

    h->downlink.ansJobj = json_object_new_object();

    if (json_object_object_get_ex(jobj, FCntDown, &o)) {
        n = json_object_get_int(o);
        json_object_object_add(h->downlink.ansJobj, FCntDown, json_object_new_int(n));
    }

    if (json_object_object_get_ex(jobj, DLFreq1, &o)) {
        n = json_object_get_int(o);
        json_object_object_add(h->downlink.ansJobj, DLFreq1, json_object_new_int(n));
    }
    if (json_object_object_get_ex(jobj, DLFreq2, &o)) {
        n = json_object_get_int(o);
        json_object_object_add(h->downlink.ansJobj, DLFreq2, json_object_new_int(n));
    }

    h->downlink.rxResult = rxResult;
}

static void
hNS_XmitDataAnsCallback_uplink(MYSQL* sc, mote_t* mote, json_object* jobj, const char* rxResult, const char* senderID, const uint8_t* rfBuf, uint8_t rfLen)
{
    struct _requesterList *myList;
    sql_t sql;
    const char* sqlResult = sql_motes_query(sc, mote->devEui, mote->devAddr, &sql);
    /*if (rxResult != Success)
        printf("\e[31m");
    printf("hNS_XmitDataAnsCallback_uplink() %s from %s\e[0m. %s ", rxResult, senderID, sql.roamState);*/
    if (sqlResult != Success)
        printf("\e[31mhNS_XmitDataAnsCallback_uplink sql:%s\e[0m ", sqlResult);
    /* when this reply occurs during PROGRESS_LOCAL, answer shouldnt be sent until PROGRESS_JSON finished, and perhaps then by finish() */

    // for hHANDOVER, when answer from AS arrives, sending answer down:
    for (myList = mote->requesterList; myList != NULL; myList = myList->next) {
        requester_t *r = myList->R;
        if (r == NULL) {
            //printf("null ");
            continue;
        }
        if (!r->needsAnswer)
            continue;

        //printf("r->MessageType:%s r->ans_ans:%u hNS-", r->MessageType, r->ans_ans);
        if (r->MessageType == XmitDataReq && r->ans_ans) {
            json_object *jo = json_object_new_object();
            if (r_post_ans_to_ns(&(myList->R), rxResult, jo) < 0) {
                printf("\e[31mhNS http_post to %s failed\e[0m", r->ClientID);
            }
        }
    }

    //printf("\n");
} // ..hNS_XmitDataAnsCallback_uplink()

/* hNS_XmitDataReq_down: this NS is hNS, but sNS is remote */
/* hNS_XmitDataReq_down: null return on success */
const char*
hNS_XmitDataReq_down(mote_t* mote, unsigned long reqTid, const char* requester, const uint8_t* frmPayload, uint8_t frmPayloadLength, DLMetaData_t* dlmd, uint32_t destID)
{
    h_t* h;
    char destIDstr[12];
    char hostname[128];
    json_object* dlo;
    const char* ret;

    json_object* jo = json_object_new_object();

    if (mote->h == NULL)
        mote->h = calloc(1, sizeof(h_t));

    h = mote->h;

    h->downlink.reqTid = reqTid;
    if (h->downlink.ansDest)
        h->downlink.ansDest = realloc(h->downlink.ansDest, strlen(requester)+1);
    else
        h->downlink.ansDest = malloc(strlen(requester)+1);
    strcpy(h->downlink.ansDest, requester);

    dlo = generateDLMetaData(NULL, 0, dlmd, hNS);
    if (dlo) {
        json_object_object_add(jo, DLMetaData, dlo);

        sprintf(destIDstr, "%06x", destID);
        sprintf(hostname, "%06x.%s", destID, netIdDomain);
        //printf(" hNS_XmitDataReq_down->");
        ret = sendXmitDataReq(mote, "hNS_XmitDataReq_down->sendXmitDataReq", jo, destIDstr, hostname, FRMPayload, frmPayload, frmPayloadLength, hNS_XmitDataAnsCallback_downlink);
        if (ret == Success) {
            ret = NULL; //doesnt send reply on success, would answer when downlink is sent
        }
    } else {
        printf("\e[31mhNS_XmitDataReq_down DLMetaData fail\e[0m\n");
        ret = Other;
    }

    return ret;
} // ..hNS_XmitDataReq_down()

json_object *
getAppSKeyEnvelope(MYSQL* sc, uint64_t devEui, uint32_t* outDevAddr, const char** res)
{
    char query[512];
    json_object* ret = NULL;
    MYSQL_ROW row;
    MYSQL_RES *result;
    my_ulonglong id = getMoteID(sc, devEui, NONE_DEVADDR, res);

    if (*res != Success) {
        printf("\e[31m%s = getMoteID(%"PRIx64")\n", *res, devEui);
        return ret;
    }
    sprintf(query, "SELECT AS_KEK_label, HEX(AS_KEK_key), DevAddr FROM sessions WHERE ID = %llu AND createdAt IN (SELECT max(createdAt) FROM sessions)", id);
    if (mysql_query(sc, query)) {
        printf("\e[31m%s ---- %s\e[0m\n", query, mysql_error(sc));
        *res = Other;
        return ret;
    }
    result = mysql_use_result(sc);
    if (!result) {
        printf("\e[31m%s --- no result\e[0m\n", query);
        *res = Other;
        return ret;
    }
    row = mysql_fetch_row(result);
    if (row) {
        *res = Success;
        ret = json_object_new_object();
        if (row[0])
            json_object_object_add(ret, KEKLabel, json_object_new_string(row[0]));
        if (row[1])
            json_object_object_add(ret, AESKey, json_object_new_string(row[1]));

        /* DevAddr should be given also since its used along with AppSKey for encryption */
        if (outDevAddr != NULL)
            sscanf(row[2], "%u", outDevAddr);
    } else
        printf("\e[31m%s --- no row, id %llu\e[0m\n", query, id);

    mysql_free_result(result);

    return ret;
}

//hNS_XmitDataReq_toAS(mote_t* mote, const uint8_t* FRMPayloadBin, uint8_t FRMPayloadLen, json_object* j_ulmd)
const char*
hNS_XmitDataReq_toAS(mote_t* mote, const uint8_t* FRMPayloadBin, uint8_t FRMPayloadLen, const ULMetaData_t* ulmd)
{
    char destIDstr[112];
    const char* ret;
    //ULMetaData_t ulmd;

    printf("hNS_XmitDataReq_toAS() %016"PRIx64" / %08x ", mote->devEui, mote->devAddr);
    if (FRMPayloadLen == 0) {
        printf("\e[31mno-Payload\e[0m ");
        return XmitFailed;
    }
    /*
    if (j_ulmd == NULL) {
        printf("\e[31mno ulmd\e[0m\n");
        return MalformedRequest;
    }
    if (ParseULMetaData(j_ulmd, &ulmd) < 0) {
        printf("\e[31mbad ulmd\e[0m\n");
        return MalformedRequest;
    }*/

    /* send to application server */
    if (sqlGetASID(mote->devEui, mote->devAddr, destIDstr, sizeof(destIDstr)) == 0) {
        h_t* h = mote->h;
        char hostname[128];
        json_object *jo, *uj;
        json_object *envl = NULL;

/*        if (h == NULL) {
            printf("\e[31mXmitDataReq_toAS() NULL h\e[0m\n");
            return -1;
        }*/
        uj = generateULMetaData(ulmd, sNS, true);
        if (!uj) {
            printf("\e[31mhNS_XmitDataReq_toAS generate ulmd fail\e[0m\n");
            return Other;
        }
        jo = json_object_new_object();

        /* h is NULL for ABP (AS already has AppSKey) */
        if (h != NULL && h->sendAppSKey) {
            const char* res;
            printf("sending-AppSKey ");
            envl = getAppSKeyEnvelope(sqlConn_lora_network, mote->devEui, NULL, &res);
            if (envl)
                json_object_object_add(jo, AppSKey, envl);
            else
                printf("\e[31mgetAppSKeyEnvelope() %s\e[0m\n", res);
        }

        json_object_object_add(jo, ULMetaData, uj);
        //ulmd_free(&ulmd);

        strcpy(hostname, "http://");
        strcat(hostname, destIDstr);
        //printf(" hNS_frm_up_toAS->");
        ret = sendXmitDataReq(mote, "hNS_XmitDataReq_toAS->sendXmitDataReq", jo, destIDstr, hostname, FRMPayload, FRMPayloadBin, FRMPayloadLen, hNS_XmitDataAnsCallback_uplink);
        if (ret != Success)
            printf("\e[31m%s = sendXmitDataReq\e[0m\n", ret);
        else if (envl)
            h->sendAppSKey = false;

        return ret;
    } else {
        printf("\e[31msqlGetASID() failed\e[0m\n");
        return Other;
    }
} // ..hNS_XmitDataReq_toAS()

void hNS_uplink_finish(mote_t* mote, sql_t* sql)
{
}


void
hNS_service(mote_t* mote)
{
    h_t* h;

    if (!mote->h)
        return;

    h = mote->h;

    if (h->downlink.ansJobj && h->downlink.rxResult) {
        printf("hNS_service ansdest:%p\n", h->downlink.ansDest);
        if (sendXmitDataAns(true, h->downlink.ansJobj, h->downlink.ansDest, h->downlink.reqTid, h->downlink.rxResult) < 0)
            printf("\e[31mhNS_XmitDataAnsCallback_downlink: sendXmitDataAns() failed\[e0m\n");

        h->downlink.ansJobj = NULL;

        h->downlink.rxResult = NULL;
    }
}
