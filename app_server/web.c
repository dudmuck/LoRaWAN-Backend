/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
//#include "web.h"
#include "as.h"

static const char deleteFrames[] = "deleteFrames";
static const char Forward[] = "Forward:";
static const char None[] = "None";

const char Cayenne[] = "Cayenne";


typedef enum {
    OP_NONE,
    OP_CREATE,
    OP_DELETE,
    OP_STOP,
    OP_APPSKEYREQ,
    OP_DELETE_FRAMES,
    OP_SET_FORWARD,
    OP_DOWNLINK
} op_e;

typedef enum {
    FORM_STATE_START,
    FORM_STATE_TABLE_HEADER,
    FORM_STATE_TABLE_ROWS,
    FORM_STATE_FORM,
    FORM_STATE_END
} form_state_e;

typedef struct {
    char DevEUIstr[24];
    uint64_t DevEUI64;

    char DevAddrStr[16];
    uint32_t DevAddr32;
    char newAppSKey[33];
    char editAppSKey[LORA_CYPHERKEY_STRLEN];
    bool ota;
    op_e op;

    form_state_e form_state;
    MYSQL_RES *result;
    MYSQL_ROW row;
    char target[64];    // devEui + devAddr combo

    bool conf;
    char fport[8];
    uint8_t payload[244];
    uint8_t payloadLen;

    bool inhibit_refresh;
    char* AFCntDownStr;
    bool sent;
} appInfo_t;

struct Session *sessions;

void
browser_post_init(struct Session *session)
{
    appInfo_t* ai = session->appInfo;

    printf("session:%p\n", session);
    printf("appInfo:%p\n", session->appInfo);
    fflush(stdout);
    ai->op = OP_NONE;
    /* unchecked checkbox items are not posted, must clear them here at start */ 
}

int
post_iterator (void *cls,
	       enum MHD_ValueKind kind,
	       const char *key,
	       const char *filename,
	       const char *content_type,
	       const char *transfer_encoding,
	       const char *data, uint64_t off, size_t size)
{
    struct Request *request = cls;
    struct Session *session = request->session;
    appInfo_t* ai = session->appInfo;

    printf("post_iterator(%zu) key \"%s\" ofs%"PRIu64", data \"%s\"\n", size, key, off, data);
    if (size == 0)
        return MHD_YES;

    if (0 == strcmp ("DevEUI", key)) {
        strncpy(ai->DevEUIstr, data, sizeof(ai->DevEUIstr));
        return MHD_YES;
    } else if (0 == strcmp ("DevAddr", key)) {
        strncpy(ai->DevAddrStr, data, sizeof(ai->DevAddrStr));
        return MHD_YES;
    } else if (0 == strcmp ("newSKey", key)) {
        strncpy(ai->newAppSKey, data, sizeof(ai->newAppSKey));
        return MHD_YES;
    } else if (0 == strcmp ("editSKey", key)) {
        strncpy(ai->editAppSKey, data, sizeof(ai->editAppSKey));
        return MHD_YES;
    } else if (0 == strcmp ("mtype", key)) {
        if (0 == strcmp("sendConf", data))
            ai->conf = true;
        else if (0 == strcmp("sendUnconf", data))
            ai->conf = false;
        ai->op = OP_DOWNLINK;
        return MHD_YES;
    } else if (ai->op != OP_DOWNLINK && 0 == strcmp ("action", key)) {
        printf("action:%s\n", data);
        if (0 == strcmp("create", data))
            ai->op = OP_CREATE;
        else if (0 == strcmp("stop", data))
            ai->op = OP_STOP;
        else if (0 == strcmp(AppSKeyReq, data))
            ai->op = OP_APPSKEYREQ;
        else if (0 == strcmp(deleteFrames, data))
            ai->op = OP_DELETE_FRAMES;
        else if (0 == strncmp(Forward, data, strlen(Forward))) {
            ai->op = OP_SET_FORWARD;
            strcpy((char*)ai->payload, data+strlen(Forward));
        } else
            printf("\e[31munknown action %s\e[0m\n", data);
        return MHD_YES;
    } else if (0 == strcmp ("payHex", key)) {
        if (ai->payloadLen == 0) {
            uint8_t* out = ai->payload;
            unsigned n, i;
            unsigned stop = strlen(data);
            for (n = 0; n < stop; n += 2) {
                sscanf(data, "%02x", &i);
                printf("%02x ", i);
                *out++ = i;
                data += 2;
            }
            printf("\n");
            ai->payloadLen = out - ai->payload;
        }
        return MHD_YES;
    } else if (0 == strcmp ("payText", key)) {
        if (ai->payloadLen == 0) {
            strncpy((char*)ai->payload, data, sizeof(ai->payload));
            ai->payloadLen = strlen(data);
        }
        return MHD_YES;
    } else if (0 == strcmp ("port", key)) {
        strncpy(ai->fport, data, sizeof(ai->fport));
        return MHD_YES;
    }
    /******** operation in data, target in key ************/
    else if (0 == strcmp("delete", data)) {
        strncpy(ai->target, key, sizeof(ai->target));
        printf("delete target %s\n", ai->target);
        ai->op = OP_DELETE;
        return MHD_YES;
    }
    fprintf (stderr, "\e[31mUnsupported form value `%s'\e[0m\n", key);
    return MHD_YES;
}

/* updateSKey() used for both ABP manual update, and OTA update upon new session */
static int
updateSKey(bool clrAFCntDown, const uint8_t* editAppSKeyBin, uint64_t devEui, const char* abpDevAddrStr, const uint32_t* otaDevAddr, char* failMsg, size_t sizeof_failMsg)
{
    char query[512];
    char buf[64];

    /* DevAddr is read from in ABP mode, and DevAddr is written to in OTA mode */

    strcpy(query, "UPDATE activemotes SET ");
    if (otaDevAddr) {
        strcat(query, "DevAddr = ");
        sprintf(buf, "%u", *otaDevAddr);
        strcat(query, buf);
        strcat(query, ", ");
    }
    if (clrAFCntDown)
        strcat(query, "AFCntDown = 0, ");
    strcat(query, "AppSKey = 0x");
    for (unsigned n = 0; n < LORA_CYPHERKEYBYTES; n++) {
        sprintf(buf, "%02x", editAppSKeyBin[n]);
        strcat(query, buf);
    }

    strcat(query, " WHERE ");
    if (devEui == NONE_DEVEUI && abpDevAddrStr) {
        strcat(query, "DevAddr = ");
        strcat(query, abpDevAddrStr);
    } else {
        strcat(query, "eui = ");
        sprintf(buf, "%"PRIu64, devEui);
        strcat(query, buf);
    }

    /* non-blocking write because this is called from XmitDataReq at new session */
    return mq_send(mqwd, query, strlen(query)+1, 0);
}

static int
updateDevAddr(MYSQL* sc, uint64_t DevEui64, uint32_t DevAddr32)
{
    char query[512];
    uint32_t sqlDevAddr;
    MYSQL_RES *result;
    MYSQL_ROW row;

    sprintf(query, "SELECT DevAddr FROM activemotes WHERE eui = %"PRIu64, DevEui64);
    if (mysql_query(sc, query)) {
        printf("\e[31mupdateDevAddr() get DevAddr: %s\e[0m\n", mysql_error(sc));
        return -1;
    }
    result = mysql_use_result(sc);
    if (result == NULL) {
        printf("\e[31mno result getting DevAddr for devEui %"PRIx64"\e[0m\n", DevEui64);
        return -1;
    }
    row = mysql_fetch_row(result);
    if (!row) {
        printf("\e[31mno row getting DevAddr for devEui %"PRIx64"\e[0m\n", DevEui64);
        mysql_free_result(result);
        return -1;
    }
    if (row[0])
        sscanf(row[0], "%u", &sqlDevAddr);
    else
        sqlDevAddr = NONE_DEVADDR;

    mysql_free_result(result);

    if (sqlDevAddr == DevAddr32) {
        //printf("devEui %"PRIx64": keeping same devAddr %08x\n", DevEui64, DevAddr32);
        return 0;
    }

    printf("devEui %"PRIx64": new devAddr %08x\n", DevEui64, DevAddr32);

    sprintf(query, "UPDATE activemotes SET DevAddr = %u WHERE eui = %"PRIu64, DevAddr32, DevEui64);
    if (mysql_query(sc, query)) {
        printf("update DevAddr: %s", mysql_error(sc));
        return -1;
    }
    if (mysql_affected_rows(sc) == 0) {
        printf("set DevAddr no rows affected %"PRIu64" / %"PRIx64, DevEui64, DevEui64);
        return -1;
    }
    return 0;
} // ..updateDevAddr()

static int
set_sql_AFCntDown(MYSQL* sc, uint64_t devEui, uint32_t devAddr, uint32_t fcnt)
{
    char where[128];
    char query[512];
    unsigned n;

    if (devEui == NONE_DEVEUI)
        sprintf(where, "DevAddr = %u", devAddr);
    else
        sprintf(where, "eui = %"PRIu64, devEui);

    sprintf(query, "UPDATE activemotes SET AFCntDown = %u WHERE %s", fcnt, where);
    if (mysql_query(sc, query)) {
        printf("\e[31m%s: %s\e[0m\n", query, mysql_error(sc));
        return -1;
    }
    n = mysql_affected_rows(sc);
    printf("setting AFCntDown %u, affected %u\n", fcnt, n);

    return 0;
}

static int
incr_sql_AFCntDown(MYSQL* sc, uint64_t devEui, uint32_t devAddr)
{
    char where[128];
    char query[512];
    unsigned n;

    if (devEui == NONE_DEVEUI)
        sprintf(where, "DevAddr = %u", devAddr);
    else
        sprintf(where, "eui = %"PRIu64, devEui);

    sprintf(query, "UPDATE activemotes SET AFCntDown = AFCntDown + 1 WHERE %s", where);
    if (mysql_query(sc, query)) {
        printf("\e[31m%s: %s\e[0m\n", query, mysql_error(sc));
        return -1;
    }
    n = mysql_affected_rows(sc);
    if (n != 1) {
        printf("\e[31mdidnt affect (%u) AFCntDown: %s\e[0m\n", n, query);
        return -1;
    }

    return 0;
}

static const char*
saveAppSKey(MYSQL* sc, bool clrAFCntDown, json_object* inJobj, uint64_t DevEui64, uint8_t* out)
{
    const char* ret = getKey(inJobj, AppSKey, &key_envelope_app, out);

    if (ret == Success) {
        char query[256];
        char failText[128];
        int cmp = -1;

        sprintf(query, "SELECT AppSKey FROM activemotes WHERE eui = %"PRIu64, DevEui64);
        if (!mysql_query(sc, query)) {
            MYSQL_RES *result = mysql_use_result(sc);
            if (result) {
                MYSQL_ROW row = mysql_fetch_row(result);
                if (row && row[0]) {
                    cmp = memcmp(row[0], out, LORA_CYPHERKEYBYTES);
                    printf("compare:%d\n", cmp);
                    print_buf((uint8_t*)row[0], LORA_CYPHERKEYBYTES, "have-AppSKey");
                    print_buf(out, LORA_CYPHERKEYBYTES, "new-AppSKey");
                }
                mysql_free_result(result);
            }
        }

        if (cmp != 0) {
            json_object *jo;
            const uint32_t *ptr = NULL;
            uint32_t devAddr;
            if (json_object_object_get_ex(inJobj, DevAddr, &jo)) {
                sscanf(json_object_get_string(jo), "%x", &devAddr);
                ptr = &devAddr;
            }
            if (updateSKey(clrAFCntDown, out, DevEui64, NULL, ptr, failText, sizeof(failText))) {
                printf("\e[31m%s\e[0m\n", failText);
                ret  = Other;
            }
        } else if (clrAFCntDown) {
            set_sql_AFCntDown(sc, DevEui64, NONE_DEVADDR, 0);
        }
    }

    return ret;
} // ..saveAppSKey()

static const char*
getAppSKey(MYSQL* sc, const ULMetaData_t* ulmd, uint8_t* out)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    char query[512];
    const char* ret;
    char where[128];

    //printf("getAppSKey() %016"PRIx64" / %08x\n", ulmd->DevEUI, ulmd->DevAddr);

    if (ulmd->DevEUI == NONE_DEVEUI) {
        sprintf(where, "DevAddr = %u", ulmd->DevAddr);
        ret = UnknownDevAddr;
    } else {
        sprintf(where, "eui = %"PRIu64, ulmd->DevEUI);
        ret = UnknownDevEUI;
    }

    sprintf(query, "SELECT AppSKey, AFCntDown FROM activemotes WHERE %s", where);
    SQL_PRINTF("AS %s", query);
    if (mysql_query(sc, query)) {
        printf("getAppSKey Error querying server: %s", mysql_error(sc));
        return ret;
    }
    result = mysql_use_result(sc);
    if (result == NULL) {
        printf("getAppSKey activemotes no result, %s", where);
        return ret;
    }
    row = mysql_fetch_row(result);
    if (!row) {
        printf("getAppSKey activemotes no row, %s\n", where);
        mainOp = MAIN_OP_GETKEY;
        mysql_free_result(result);
        return ret;
    }
    if (row[0] == NULL) {
        mainOp = MAIN_OP_GETKEY;
        mysql_free_result(result);
        return Success;
    }

    memcpy(out, row[0], LORA_CYPHERKEYBYTES);

/*    printf("\e[33mgetAppSKey() ");
    print_buf(out, LORA_CYPHERKEYBYTES, "AppSKey");
    printf(" fcntup:%u, %08x\e[0m\n", ulmd->FCntUp, ulmd->DevAddr);*/

    mysql_free_result(result);

    return Success;
} // ..getAppSKey()

void
ParseJson(MYSQL* sc, const struct sockaddr *client_addr, json_object* inJobj, json_object** ansJobj)
{
    const char* Result;
    const char* _ansMt;
    const char* rxResult = NULL;
    json_object *obj;
    unsigned long trans_id = 0;
    const char* pmt;
    char sender_id[64];

    Result = lib_parse_json(inJobj, &_ansMt, &pmt, sender_id, myAS_ID, &trans_id, &rxResult);
    //printf("AS %s = lib_parse_json()\n", Result);

    if (_ansMt != NULL)
        *ansJobj = json_object_new_object();    //always answering immediately in AS


    if (strcmp(Result, Success) != 0) {
        printf("\e[31mAS ParseJson(): %s = lib_parse_json()\e[0m\n", Result);
        if (_ansMt != NULL)
            goto Ans;
        else
            return;
    }

    if (XmitDataReq == pmt) {
        ULMetaData_t ulmd;
        uint8_t AppSKeyBin[LORA_CYPHERKEYBYTES];
        uint32_t DevAddr32 = NONE_DEVADDR;
        uint64_t DevEui64;
        const char *frmStr, *saveResult;
        json_object* ULobj;

        if (json_object_object_get_ex(inJobj, FRMPayload, &obj)) {
            frmStr = json_object_get_string(obj);
        } else {
            Result = MalformedRequest;
            printf("\e[31m%s: %s required\e[0m\n", XmitDataReq, FRMPayload);
            goto Ans;
        }

        if (json_object_object_get_ex(inJobj, ULMetaData, &ULobj)) {
            json_object* jo;
            if (json_object_object_get_ex(ULobj, DevAddr, &jo))
                sscanf(json_object_get_string(jo), "%x", &DevAddr32);

            if (json_object_object_get_ex(ULobj, DevEUI, &jo)) {
                sscanf(json_object_get_string(jo), "%"PRIx64, &DevEui64);
                if (DevAddr32 != NONE_DEVADDR && updateDevAddr(sc, DevEui64, DevAddr32)) {
                    Result = UnknownDevEUI;
                    goto Ans;
                }
            }
        } else {
            Result = MalformedRequest;
            printf("\e[31m%s: %s required\e[0m\n", XmitDataReq, ULMetaData);
            goto Ans;
        }

        /* if AppSKey present in XmitDataReq, then new session resets AFCntDown */
        saveResult = saveAppSKey(sc, true, inJobj, DevEui64, AppSKeyBin);
        if (saveResult != NULL && saveResult != Success) {
            printf("received uplink --- %s\n", json_object_to_json_string(inJobj));
            Result = saveResult;
            goto Ans;
        }

        if (ParseULMetaData(ULobj, &ulmd) < 0) {
            printf("\e[31mbad ulmd\e[0m\n");
            Result = MalformedRequest;
            goto Ans;
        }

        if (saveResult != NULL) // if saveResult != NULL: new session, AppSKey was given and is in AppSKeyBin[]
            Result = app_uplink(sc, frmStr, &ulmd, AppSKeyBin);
        else {  // saveResult == NULL: continuing session, retrieve saved AppSKey
            Result = getAppSKey(sc, &ulmd, AppSKeyBin);
            if (Result == Success)
                Result = app_uplink(sc, frmStr, &ulmd, AppSKeyBin);
        }

    } // ..if XmitDataReq
    else if (XmitDataAns == pmt) {
        struct _tlist* list;
        printf(" XmitDataAns ");
        for (list = tlist; list; list = list->next) {
            msg_t* msg;
            printf("list->tid %u ", list->tid);
            if (list->tid != trans_id) {
                printf("!= %lu\n", trans_id);
                continue;
            }
            printf("== %lu\n", trans_id);

            msg = &list->msg;
            /* TODO: check for both devEui and devAddr blank */
            printf("get from tlist %016"PRIx64" / %08x\n", msg->devEui, msg->devAddr);
            if (rxResult == Success) {
                if (incr_sql_AFCntDown(sc, msg->devEui, msg->devAddr) == 0) {
                    printf("incremented AFCntDown\n");
                }
            } else if (json_object_object_get_ex(inJobj, FCntDown, &obj)) {
                char failMsg[1024];
                unsigned fcnt = json_object_get_int(obj);

                printf("\e[33mset FCntDown %u\e[0m\n", fcnt);
                if (set_sql_AFCntDown(sc, msg->devEui, msg->devAddr, fcnt) < 0)
                    writeDownlinkStatus(sc, msg->devEui, msg->devAddr, failMsg);
                else if (send_downlink(sc, failMsg, sizeof(failMsg), msg) < 0)
                    writeDownlinkStatus(sc, msg->devEui, msg->devAddr, failMsg);

                printf("downlink try again with fcnt %u\n", fcnt);
                msg->devEui = NONE_DEVEUI;
                msg->devAddr = NONE_DEVADDR;
                if (msg->payload)
                    free(msg->payload);
                break;  // try again instead of writing status
            }

            if (writeDownlinkStatus(sc, msg->devEui, msg->devAddr, rxResult) == 0) {
                msg->devEui = NONE_DEVEUI;
                msg->devAddr = NONE_DEVADDR;
                if (msg->payload)
                    free(msg->payload);
            } else
                printf("\e[31mwriteDownlinkStatus() fail\e[0m\n");
            break;
        } // ..for (list = tlist; list; list = list->next)
    } else if (AppSKeyAns == pmt) {
        struct _tlist* list;
        printf(" AppSKeyAns  ");
        for (list = tlist; list; list = list->next) {
            msg_t* msg;
            printf("list->tid %u ", list->tid);
            if (list->tid != trans_id) {
                printf("!= %lu\n", trans_id);
                continue;
            }
            printf("== %lu\n", trans_id);
            msg = &list->msg;
            printf("get from tlist %016"PRIx64" / %08x\n", msg->devEui, msg->devAddr);
            if (rxResult == Success) {
                const char* res;
                uint8_t AppSKey_bin[LORA_CYPHERKEYBYTES];
                /* false: dont reset AFCntDown
                 * perhaps AFCntDown should be reset if AppSKey has changed */
                res = saveAppSKey(sc, false, inJobj, msg->devEui, AppSKey_bin);
                if (res != Success)
                    printf("\e[31m%s = saveAppSKey()\e[0m\n", res);
            } else
                printf("\e[31m%s %s\e[0m\n", pmt, rxResult);

            msg->devEui = NONE_DEVEUI;
            msg->devAddr = NONE_DEVADDR;
        } // ..for (list = tlist; list; list = list->next)
    } else
        printf("AS \e[31munhandled message type %s\e[0m\n", pmt);

Ans:
    if (_ansMt != NULL && *ansJobj) {
        //printf("AS inMT '%s', ansMt '%s', Result '%s'\n", pmt, _ansMt, Result);
        lib_generate_json(*ansJobj, sender_id, myAS_ID, trans_id, _ansMt, Result);
    }

} // ..ParseJson()

void* create_appInfo()
{
    return calloc(1, sizeof(appInfo_t));
}

const char motesURL[] = "/motes";
const char postFailURL[] = "/postFail";
const char skeyURL[] = "/skey";
const char framesURL[] = "/frames";

static char postFailPage[256];

static int
serve_post_fail(const void *cls,
		const char *mime,
		struct Session *session,
		struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;

    /* unsupported HTTP method */
    response = MHD_create_response_from_buffer (strlen (postFailPage), (void *) postFailPage, MHD_RESPMEM_PERSISTENT);
    if (NULL == response)
        return MHD_NO;
    lib_add_session_cookie (session, response);
    ret = MHD_queue_response (connection, MHD_HTTP_NOT_FOUND, response);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_ENCODING, mime);
    MHD_destroy_response (response);
    return ret;
}

/* AppSKey is only requested for OTA, but requires DevAddr to derive NetID.
 * AppSKeyReq is never used for ABP because session keys are permanent */
static int
requestAppSKey(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    int ret = -1;
    json_object* jobj;
    uint32_t NetID24, NwkID, NwkAddr;
    uint32_t reqSentTid;
    char netIDstr[24];
    msg_t msg;
    char strbuf[128];
    CURL* easy;

    msg.devEui = NONE_DEVEUI;
    msg.devAddr = NONE_DEVADDR;
    msg.payload = NULL;
    msg.payLen = 0;

    printf("requestAppSKey() DevEUIstr \"%s\" DevAddrStr \"%s\" ", ai->DevEUIstr, ai->DevAddrStr);
    if (strlen(ai->DevEUIstr) > 0)
        sscanf(ai->DevEUIstr, "%"PRIu64, &msg.devEui);  /* OTA */
    else {
        snprintf(failMsg, sizeof_failMsg , "need DevEUI ");
        return -1;
    }

    if (strlen(ai->DevAddrStr) > 0)
        sscanf(ai->DevAddrStr, "%u", &msg.devAddr);
    else {
        snprintf(failMsg, sizeof_failMsg , "need DevAddr ");    // only needed to get NetID
        return -1;
    }

    if (parseDevAddr(msg.devAddr, &NetID24, &NwkID, &NwkAddr) == UINT_MAX) {
        snprintf(failMsg, sizeof_failMsg , "cant get NetID from DevAddr %08x", msg.devAddr);
        return -1;
    }
    printf(" DevAddr %08x -> NetID %06x ", msg.devAddr, NetID24);
    jobj = json_object_new_object();

    sprintf(strbuf, "%"PRIx64, msg.devEui);
    json_object_object_add(jobj, DevEUI, json_object_new_string(strbuf));

    reqSentTid = next_req_tid++;
    sprintf(netIDstr, "%06x", NetID24);
    lib_generate_json(jobj, netIDstr, myAS_ID, reqSentTid, AppSKeyReq, NULL);

    printf("toNS: %s\n", json_object_to_json_string(jobj));

    easy = curl_easy_init();
    if (!easy)
        return CURLE_FAILED_INIT;   // TODO appropriate return
    curl_multi_add_handle(multi_handle, easy);

    sprintf(strbuf, "%06x.%s", NetID24, netIdDomain);
    printf("AS posting to %s\n", strbuf);
    ret = http_post_hostname(easy, jobj, strbuf, true);

    if (ret == 0) {
        tlistAdd(&msg, reqSentTid);
    } else {
        snprintf(failMsg, sizeof_failMsg, "%d = http_post_hostname(%s) ", ret, strbuf);
    }

    return ret;
} // ..requestAppSKey()

static int
setForwarding(MYSQL* sc, appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    char query[512];
    unsigned n;

    strcpy(query, "UPDATE activemotes SET forwardTo = ");
    if (strcmp((char*)ai->payload, None) == 0)
        strcat(query, "NULL");
    else {
        strcat(query, "'");
        strcat(query, (char*)ai->payload);
        strcat(query, "'");
    }

    strcat(query, " WHERE ");

    if (strlen(ai->DevEUIstr) > 0) {
        strcat(query, "eui = ");
        strcat(query, ai->DevEUIstr);
    } else if (strlen(ai->DevAddrStr) > 0) {
        strcat(query, "DevAddr = ");
        strcat(query, ai->DevAddrStr);
    }

    if (mysql_query(sc, query)) {
        snprintf(failMsg, sizeof_failMsg, "setForwarding: %s", mysql_error(sc));
        return -1;
    }
    n = mysql_affected_rows(sc);
    if (n != 1) {
        printf("\e[31msetForwarding didnt affect: %u\e[0m\n", n);
        return -1;
    }

    return 0;
}

static int
eraseFrames(MYSQL* sc, appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    MYSQL_RES *result;
    char query[512];
    my_ulonglong id = 0;
    strcpy(query, "SELECT ID from activemotes WHERE ");

    if (strlen(ai->DevEUIstr) > 0) {
        strcat(query, "eui = ");
        strcat(query, ai->DevEUIstr);
    } else if (strlen(ai->DevAddrStr) > 0) {
        strcat(query, "DevAddr = ");
        strcat(query, ai->DevAddrStr);
    }

    if (mysql_query(sc, query)) {
        snprintf(failMsg, sizeof_failMsg, "erase frames: %s", mysql_error(sc));
        return -1;
    }

    result = mysql_use_result(sc);
    if (result) {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row)
            sscanf(row[0], "%llu", &id);
        mysql_free_result(result);
    } else {
        snprintf(failMsg, sizeof_failMsg, "erase frames no result");
        return -1;
    }

    if (id == 0) {
        snprintf(failMsg, sizeof_failMsg, "erase frames couldnt get ID");
        return -1;
    }

    /* have ID, now delete frames */
    sprintf(query, "DELETE FROM frames WHERE ID = %llu", id);
    if (mysql_query(sc, query)) {
        snprintf(failMsg, sizeof_failMsg, "delete frames: %s", mysql_error(sc));
        return -1;
    }
    id = mysql_affected_rows(sc);
    printf("%llu frames deleted\n", id);

    return 0;
} // ..eraseFrames()

int
static sendDownlink(MYSQL* sc, appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    unsigned fport;
    int ret;
    msg_t msg;

    ret = sscanf(ai->fport, "%u", &fport);
    if (ret != 1 || fport == 0) {
        snprintf(failMsg, sizeof_failMsg ,"port value invalid");
        return -1;
    }


    if (strlen(ai->DevEUIstr) > 0)
        sscanf(ai->DevEUIstr, "%"PRIu64, &msg.devEui);  /* OTA */
    else
        msg.devEui = NONE_DEVEUI;

    if (strlen(ai->DevAddrStr) > 0)
        ret = sscanf(ai->DevAddrStr, "%u", &msg.devAddr);   /* ABP */
    else
        msg.devAddr = NONE_DEVADDR;

    printf("sendDownlink() len %d conf:%d\n", ai->payloadLen, ai->conf);
    msg.payload = ai->payload;
    msg.payLen = ai->payloadLen;
    msg.conf = ai->conf;
    msg.fport = fport;
    ret = send_downlink(sc, failMsg, sizeof_failMsg, &msg);//devEui, devAddr, ai->payload, ai->payloadLen, ai->conf, fport);
    printf("%d = send_downlink()\n", ret);

    return ret;
} // ..sendDownlink()


int
deleteMote(MYSQL* sc, char* failMsg, size_t sizeof_failMsg, const char* target)
{
    char devEuiStr[64];
    char devAddrStr[32];
    char query[1024];
    char where[512];

    if (getTarget(target, devEuiStr, sizeof(devEuiStr), devAddrStr, sizeof(devAddrStr)) < 0) {
        snprintf(failMsg, sizeof_failMsg, "getTarget \"%s\"", target);
        return -1;
    }

    if (strlen(devEuiStr) > 0 && strlen(devAddrStr) > 0)
        sprintf(where, "eui = %s AND DevAddr = %s", devEuiStr, devAddrStr);
    else if (strlen(devEuiStr) > 0)
        sprintf(where, "eui = %s", devEuiStr);
    else if (strlen(devAddrStr) > 0)
        sprintf(where, "DevAddr = %s", devAddrStr);

    sprintf(query, "DELETE FROM activemotes WHERE %s", where);
    printf("%s\n", query);
    if (mysql_query(sc, query)) {
        snprintf(failMsg, sizeof_failMsg, "delete from activemotes: %s", mysql_error(sc));
        return -1;
    }
    if (mysql_affected_rows(sc) == 0) {
        snprintf(failMsg, sizeof_failMsg, "no rows affected");
        return -1;
    }

    return 0;
} // ..deleteMote()

int
createMote(MYSQL* sc, appInfo_t* ai, char* failMsg, size_t sizeof_failMsg, bool ota)
{
    char query[1048];

    if (ota) {
        uint64_t devEui64;
        sscanf(ai->DevEUIstr, "%"PRIx64, &devEui64);
        sprintf(query, "INSERT INTO activemotes (eui) VALUES (%"PRIu64")", devEui64);
    } else {
        //unsigned n;
        char str[32];
        uint32_t devAddr32;
        sscanf(ai->DevAddrStr, "%x", &devAddr32);
        //sprintf(query, "INSERT INTO activemotes (DevAddr, AppSKey) VALUES (%u, '%s')", devAddr32, ai->newAppSKey);
        strcpy(query, "INSERT INTO activemotes (DevAddr, AppSKey) VALUES (");
        sprintf(str, "%u", devAddr32);
        strcat(query, str);
        strcat(query, ", 0x");
        /*for (n = 0; n < LORA_CYPHERKEYBYTES; n++) {
            sprintf(str, "%02x", ai->newAppSKey[n]);
            strcat(query, str);
        }*/
        strcat(query, ai->newAppSKey);
        strcat(query, ")");
    }

    printf("%s\n", query);
    if (mysql_query(sc, query)) {
        snprintf(failMsg, sizeof_failMsg, "insert into activemotes: %s", mysql_error(sc));
        return -1;
    }
    if (mysql_affected_rows(sc) == 0) {
        snprintf(failMsg, sizeof_failMsg, "no rows affected");
        return -1;
    }

    return 0;
}


void
browser_post_submitted(const char* url, struct Request* request)
{
    const char* nextURL = ".";
    char failMsg[1024];
    struct Session *session = request->session;
    appInfo_t* ai = session->appInfo;

    failMsg[0] = 0;
    printf("\e[33mAS browser_post_submitted(%s,)\e[0m\n", url);
    if (strcmp(url, motesURL) == 0) {
        if (ai->op == OP_CREATE) {
            printf("post submitted motesURL eui:%s, devaddr:%s, skey:%s\n", ai->DevEUIstr, ai->DevAddrStr, ai->newAppSKey);
            if (strlen(ai->DevEUIstr) > 0 && strlen(ai->DevAddrStr) > 0) {
                snprintf(failMsg, sizeof(failMsg) ,"enter either DevEUI(OTA) or DevAddr(ABP), not both");
            } else if (strlen(ai->DevEUIstr) > 0) {
                if (strlen(ai->newAppSKey) > 0) {
                    snprintf(failMsg, sizeof(failMsg) ,"OTA requires empty AppSKey");
                } else {
                    if (createMote(session->sqlConn, ai, failMsg, sizeof(failMsg), true) == 0)
                        nextURL = motesURL;
                }
            } else if (strlen(ai->DevAddrStr) > 0) {
                if (numHexDigits(ai->newAppSKey) < 32) {
                    snprintf(failMsg, sizeof(failMsg) ,"ABP requires 32-hex-char AppSKey");
                } else {
                    if (createMote(session->sqlConn, ai, failMsg, sizeof(failMsg), false) == 0)
                        nextURL = motesURL;
                }
            } else
                snprintf(failMsg, sizeof(failMsg) ,"enter either DevEUI(OTA) or DevAddr(ABP)");
        } else if (ai->op == OP_DELETE) {
            if (deleteMote(session->sqlConn, failMsg, sizeof(failMsg), ai->target) == 0)
                nextURL = motesURL;
        } else
            snprintf(failMsg, sizeof(failMsg) ,"unknown op %u", ai->op);
    } else if (strcmp(url, skeyURL) == 0) {
        /* ABP mote */
        if (numHexDigits(ai->editAppSKey) < 32)
            snprintf(failMsg, sizeof(failMsg) ,"requires 32-hex-char AppSKey");
        else {
            uint8_t keyBin[LORA_CYPHERKEYBYTES];
            const char* ptr = ai->editAppSKey;
            for (unsigned n = 0; n < LORA_CYPHERKEYBYTES; n++) {
                unsigned o;
                sscanf(ptr, "%02x", &o);
                ptr += 2;
                keyBin[n] = o;
            }
            if (updateSKey(false, keyBin, NONE_DEVEUI, ai->DevAddrStr, NULL, failMsg, sizeof(failMsg)) == 0)
                nextURL = motesURL;
        }
    } else if (strncmp(url, framesURL, strlen(framesURL)) == 0) {
        nextURL = url;  // contains subdir target
        if (ai->op == OP_STOP) {
            //clearDownlink(ai, failMsg, sizeof(failMsg);
            ai->inhibit_refresh = true;
        } else if (ai->op == OP_APPSKEYREQ) {
            //printf("OP_APPSKEYREQ ");
            requestAppSKey(ai, failMsg, sizeof(failMsg));
        } else if (ai->op == OP_DELETE_FRAMES) {
            eraseFrames(session->sqlConn, ai, failMsg, sizeof(failMsg));
        } else if (ai->op == OP_SET_FORWARD) {
            setForwarding(session->sqlConn, ai, failMsg, sizeof(failMsg));
        } else {
            if (sendDownlink(session->sqlConn, ai, failMsg, sizeof(failMsg)) == 0) {
                //nextURL = framesURL;
                // ok
            }
        }
    } else
        snprintf(failMsg, sizeof(failMsg) ,"post URL \"%s\"", url);


    if (strlen(failMsg) > 0) {
        printf("failMsg%zu: \e[31m%s\e[0m\n", strlen(failMsg), failMsg);
        snprintf(postFailPage, sizeof(postFailPage), "<html><head><title>Failed</title></head><body>%s</body></html>", failMsg);
        request->post_url = postFailURL;
    } else
        request->post_url = nextURL;
} // ..browser_post_submitted()

/**
 * Invalid URL page.
 */
#define NOT_FOUND_ERROR "<html><head><title>Not found</title></head><body>AS invalid-url</body></html>"

/**
 * Handler used to generate a 404 reply.
 *
 * @param cls a 'const char *' with the HTML webpage to return
 * @param mime mime type to use
 * @param session session handle
 * @param connection connection to use
 */
static int
not_found_page (const void *cls,
		const char *mime,
		struct Session *session,
		struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;

    /* unsupported HTTP method */
    response = MHD_create_response_from_buffer (strlen (NOT_FOUND_ERROR), (void *) NOT_FOUND_ERROR, MHD_RESPMEM_PERSISTENT);
    if (NULL == response)
        return MHD_NO;
    lib_add_session_cookie (session, response);
    ret = MHD_queue_response (connection, MHD_HTTP_NOT_FOUND, response);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_ENCODING, mime);
    MHD_destroy_response (response);
    return ret;
}

static int
serve_skey(const void *cls,
		const char *mime,
		struct Session *session,
		struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;
    char query[512];
    char buf[2040];
    char body[1536];
    uint32_t devAddr;

    sscanf(session->urlSubDir, "%u", &devAddr);
    strcpy(body, "fail");
    /* passed devAddr as base-10 string */

    sprintf(query, "SELECT HEX(AppSKey) FROM activemotes WHERE DevAddr = %s", session->urlSubDir);
    if (mysql_query(session->sqlConn, query)) {
        sprintf(body, "serve_skey :%s", mysql_error(session->sqlConn));
    } else {
        MYSQL_RES *result = mysql_use_result(session->sqlConn);
        if (!result) {
            sprintf(body, "mysql no result");
        } else {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row)
                sprintf(body, "<form method=\"post\" action=\"%s\"><input type=\"hidden\" name=\"DevAddr\" value=\"%s\"><input type=\"text\" value=\"%s\" size=\"33\" name=\"editSKey\"><input type=\"submit\" value=\"updateSKey\"></form>", skeyURL, session->urlSubDir, row[0]);
            else
                sprintf(body, "mysql no row");
        }
        mysql_free_result(result);
    }


    sprintf(buf, "<html><head><title>edit AppSKey</title></head><body><h3>edit AppSKey of %08x</h3>%s</body></html>", devAddr, body);
    /* unsupported HTTP method */
    response = MHD_create_response_from_buffer (strlen (buf), (void *)buf, MHD_RESPMEM_MUST_COPY);
    if (NULL == response)
        return MHD_NO;
    lib_add_session_cookie (session, response);
    ret = MHD_queue_response (connection, MHD_HTTP_NOT_FOUND, response);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_ENCODING, mime);
    MHD_destroy_response (response);
    return ret;
}

/**
 * Front page. (/)
 */
#define MAIN_PAGE "<html><head><title>App Server</title></head><body><h3>App Server %s</h3><a href=\"motes\">motes</a></body></html>"

static int
main_page (const void *cls,
	      const char *mime,
	      struct Session *session,
	      struct MHD_Connection *connection)
{
    int ret;
    char *reply;
    struct MHD_Response *response;
    size_t len;

    len = strlen (MAIN_PAGE) + strlen(myAS_ID) + 1;
    reply = malloc (len);
    if (NULL == reply)
        return MHD_NO;
    snprintf (reply, len, MAIN_PAGE, myAS_ID);
    /* return static form */
    response = MHD_create_response_from_buffer (strlen (reply), (void *) reply, MHD_RESPMEM_MUST_FREE);
    if (NULL == response)
        return MHD_NO;

    lib_add_session_cookie (session, response);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_ENCODING, mime);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}

const char mote_css[] =
"table.motes{"
"    font-family: Arial;"
"    font-size: 13pt;"
"    border-width: 1px 1px 1px 1px;"
"    border-style: solid;"
"}"
"table.motes td {"
"    text-align: left;"
"    border-width: 1px 1px 1px 1px;"
"    border-style: solid;"
"    padding: 5px;"
"}"
"table.motes td:nth-child(-n+3) {"
"    font-family: monospace;"
"    font-size: 12pt;"
"}"
"input[type=\"text\"] {"
"    font-family: monospace;"
"    font-size: 12pt;"
"}";

static ssize_t
motes_page_iterator(void *cls,
          uint64_t pos,
          char *buf,
          size_t max)
{
    int len = 0;
    char query[512];
    struct Session *session = cls;
    appInfo_t* ai = session->appInfo;

    switch (ai->form_state) {
        case FORM_STATE_START:
            len = sprintf(buf, "<html><head><title>AS motes</title><style>%s</style></head><body><a href=\"/\">top</a><h3>App Server %s</h3><form id=\"form\" method=\"post\" action=\"%s\"><table class=\"motes\" border=\"1\">", mote_css, myAS_ID, motesURL);
            ai->form_state = FORM_STATE_TABLE_HEADER;
            break;
        case FORM_STATE_TABLE_HEADER:
            sprintf(query, "SELECT eui, DevAddr, HEX(AppSKey) FROM activemotes");
            printf("%s\n", query);
            if (mysql_query(session->sqlConn, query)) {
                len = sprintf(buf, "<tr>mysql_query() %s</tr>", mysql_error(session->sqlConn));
                ai->form_state = FORM_STATE_FORM;
                break;
            }
            ai->result = mysql_use_result(session->sqlConn);
            if (ai->result == NULL) {
                len = sprintf(buf, "<tr>no result</tr>");
                ai->form_state = FORM_STATE_FORM;
                break;
            }
            len = sprintf(buf, "<tr><th>DevEUI</th><th>DevAddr</th><th>AppSKey</th><th></th></tr>");
            ai->row = mysql_fetch_row(ai->result);
            if (ai->row) {
                ai->form_state = FORM_STATE_TABLE_ROWS;
                //ai->i = 0;
            } else
                ai->form_state = FORM_STATE_FORM;
            break;
        case FORM_STATE_TABLE_ROWS:
            {
                char devEuiStr[32];
                char devAddrStr[24];
                char keyField[128];
                char target[64];
                char framesLink[128];

                /* target of devEui_devAddr in base-10 string */
                if (ai->row[0] && ai->row[1])
                    sprintf(target, "%s_%s", ai->row[0], ai->row[1]);
                else if (ai->row[0])
                    sprintf(target, "%s_", ai->row[0]);
                else if (ai->row[1])
                    sprintf(target, "_%s", ai->row[1]);

                keyField[0] = 0;
                if (ai->row[1]) {   // does devAddr exist?
                    uint32_t addr32;
                    sscanf(ai->row[1], "%u", &addr32);
                    sprintf(devAddrStr, "%08x", addr32);
                    sprintf(framesLink, "<a href=\"/frames/%s\">frames</a></td></tr>", target);
                } else {
                    devAddrStr[0] = 0;
                    framesLink[0] = 0;  // no devAddr, downlink couldnt be encrypted
                }

                if (ai->row[0]) {   // does devEui exist?
                    uint64_t eui64;
                    sscanf(ai->row[0], "%"PRIu64, &eui64);
                    sprintf(devEuiStr, "%016"PRIx64, eui64);
                    // OTA, display only appSKey
                    if (ai->row[2]) 
                        strcpy(keyField, ai->row[2]);
                } else {
                    devEuiStr[0] = 0;
                    // ABP, add edit link to appSKey
                    if (ai->row[2])
                        sprintf(keyField, "%s<a href=\"/skey/%s\"> edit</a>", ai->row[2], ai->row[1]);
                    else
                        sprintf(keyField, "<a href=\"/skey/%s\"> edit</a>", ai->row[1]);
                }

                len = sprintf(buf, "<tr><td>%s</td><td>%s</td><td>%s</td><td><input type=\"submit\" name=\"%s\" value=\"delete\"></td><td>%s</td></tr>", devEuiStr, devAddrStr, keyField, target, framesLink);
            }
            ai->row = mysql_fetch_row(ai->result);
            if (!ai->row)
                ai->form_state = FORM_STATE_FORM;
            break;
        case FORM_STATE_FORM:
            if (ai->result)
                mysql_free_result(ai->result);
            ai->form_state = FORM_STATE_END;
            len = sprintf(buf, "<tr><td><input type=\"text\" name=\"DevEUI\" size=\"17\" placeholder=\"OTA only\"></td><td><input type=\"text\" name=\"DevAddr\" size=\"9\" placeholder=\"ABP only\"></td><td><input type=\"text\" name=\"newSKey\" size=\"33\" placeholder=\"ABP only, 32 hex digits\"></td><td><input type=\"submit\" value=\"create\" name=\"action\"></td></tr></table></form></body></html>");
            break;
        case FORM_STATE_END:
            return MHD_CONTENT_READER_END_OF_STREAM;
    } // ..switch (form_state)

    return len;
}


static int
serve_motes (const void *cls,
		const char *mime,
		struct Session *session,
		struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;
    appInfo_t* ai = session->appInfo;

    ai->form_state = FORM_STATE_START;
    response = MHD_create_response_from_callback (MHD_SIZE_UNKNOWN,
                                                1024,
                                                &motes_page_iterator,
                                                session,
                                                NULL);
    lib_add_session_cookie (session, response);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}

const char frames_css[] =
"table.frames{"
"    font-family: Arial;"
"    font-size: 13pt;"
"    border-width: 1px 1px 1px 1px;"
"    border-style: solid;"
"}"
"table.frames td {"
"    text-align: left;"
"    border-width: 1px 1px 1px 1px;"
"    border-style: solid;"
"    padding: 5px;"
"}"
"table.frames td:nth-child(n+4):nth-child(-n+5) {"
"    font-family: monospace;"
"    font-size: 12pt;"
"}"
"input[type=\"text\"] {"
"    font-family: monospace;"
"    font-size: 12pt;"
"}";

static ssize_t
frames_page_iterator(void *cls,
          uint64_t pos,
          char *buf,
          size_t max)
{
    char downBuf[256];
    char query[512];
    char where[128];
    char str[64];
    char devEuiStr[64];
    char devAddrStr[32];
    char fwdTo[32];
    int len = 0;
    struct Session *session = cls;
    appInfo_t* ai = session->appInfo;

    switch (ai->form_state) {
        case FORM_STATE_START:
            //printf("frames_page_iterator() \"%s\"\n", session->urlSubDir);
            strcpy(buf, "<html><head><title>AS frames</title><style>");
            strcat(buf, frames_css);
            strcat(buf, "</style></head><body><a href=\"");

            strcat(buf, motesURL);
            strcat(buf, "\">back</a><h3>");
            if (getTarget(session->urlSubDir, devEuiStr, sizeof(devEuiStr), devAddrStr, sizeof(devAddrStr)) < 0) {
                strcat(buf, "getTarget \"");
                strcat(buf, session->urlSubDir);
                strcat(buf, "\"</h3></body></html>");
                len = strlen(buf);
                ai->form_state = FORM_STATE_END;
                break;
            }

            strcat(buf, "App Server ");
            strcat(buf, myAS_ID);
            strcat(buf, " frames<br>");

            if (strlen(devEuiStr) > 0) {
                sscanf(devEuiStr, "%"PRIu64, &ai->DevEUI64);
                ai->ota = true;
                sprintf(str, "%016"PRIx64, ai->DevEUI64);
                strcat(buf, str);
            } else
                ai->DevEUI64 = NONE_DEVEUI;

            if (strlen(devAddrStr) > 0) {
                ai->ota = false;
                sscanf(devAddrStr, "%u", &ai->DevAddr32);
            } else
                ai->DevAddr32 = NONE_DEVADDR;


            if (ai->DevEUI64 == NONE_DEVEUI) {
                sprintf(where, "DevAddr = %u", ai->DevAddr32);
                sprintf(query, "SELECT forwardTo, downlinkStatus, AFCntDown FROM activemotes WHERE %s", where);
            } else {
                sprintf(where, "eui = %"PRIu64, ai->DevEUI64);
                sprintf(query, "SELECT forwardTo, downlinkStatus, AFCntDown, DevAddr FROM activemotes WHERE %s", where);
            }

            if (mysql_query(session->sqlConn, query)) {
                printf("%s: %s", query, mysql_error(session->sqlConn));
                ai->form_state = FORM_STATE_END;
                break;
            }
            downBuf[0] = 0;
            ai->result = mysql_use_result(session->sqlConn);
            ai->sent = false;
            if (ai->result) {
                ai->row = mysql_fetch_row(ai->result);
                if (ai->row) {
                    printf("forwardTo:%s\n", ai->row[0]);
                    if (ai->row[0] == NULL)
                        fwdTo[0] = 0;
                    else
                        strncpy(fwdTo, ai->row[0], sizeof(fwdTo));

                    if (ai->row[1]) {
                        printf("downlink status \"%s\" inhibit_refresh%u\n", ai->row[1], ai->inhibit_refresh);
                        if (!ai->inhibit_refresh && strcmp(ai->row[1], "SENT") == 0) {
                            strcat(downBuf, "<br><input type=\"submit\" value=\"stop\" name=\"action\"><script type=\"text/javascript\"> setTimeout(function () { window.location.href = window.location.href; }, 2 * 1000); </script><br><br><h3>in progress");
                            ai->sent = true;
                        } else {
                            strcat(downBuf, "<br><br><h3>Last downlink result: ");
                            strcat(downBuf, ai->row[1]);
                        }
                        strcat(downBuf, "</h3>");
                    } else
                        printf("NULL downlinkStatus\n");

                    if (ai->row[2]) {
                        ai->AFCntDownStr = malloc(strlen(ai->row[2])+1);
                        strcpy(ai->AFCntDownStr, ai->row[2]);
                    } else
                        ai->AFCntDownStr = NULL;

                    if (ai->DevEUI64 != NONE_DEVEUI && ai->row[3]) {
                        /* possibly sql has newer DevAddr if session changed */
                        //printf("devAddrStr:%s -> %s\n", devAddrStr, ai->row[3]);
                        //printf("%08x ", ai->DevAddr32);
                        sscanf(ai->row[3], "%u", &ai->DevAddr32);
                        //printf("->%08x\n", ai->DevAddr32);
                        sprintf(devAddrStr, "%u", ai->DevAddr32);
                    }

                    mysql_free_result(ai->result);
                    ai->result = NULL;
                } else {
                    strcat(buf, query);
                    strcat(buf, " no row</body></html>");
                    len = strlen(buf);
                    ai->form_state = FORM_STATE_END;
                    mysql_free_result(ai->result);
                    ai->result = NULL;
                    break;
                }
            } else {
                strcat(buf, query);
                strcat(buf, " no result</body></html>");
                len = strlen(buf);
                ai->form_state = FORM_STATE_END;
                break;
            }

            if (strlen(devAddrStr) > 0) {
                if (strlen(devEuiStr) > 0)
                    strcat(buf, " / ");
                sprintf(str, "%08x", ai->DevAddr32);
                strcat(buf, str);
            }

            strcat(buf, "</h3><form id=\"form\" method=\"post\" action=\"");
            strcat(buf, framesURL);
            strcat(buf, "/");
            strcat(buf, session->urlSubDir);
            strcat(buf, "\">");

            strcat(buf, "Forward:<select name=\"action\" onchange=\"this.form.submit()\"><option value=\"");
            strcat(buf, Forward);
            strcat(buf, None);
            strcat(buf, "\"");
            printf("fwdTo:\"%s\"\n", fwdTo);
            if (fwdTo[0] == 0)
                strcat(buf, " selected");
            strcat(buf, ">None</option> <option value=\"");
            strcat(buf, Forward);
            strcat(buf, Cayenne);
            strcat(buf, "\"");
            if (strcmp(fwdTo, Cayenne) == 0)
                strcat(buf, " selected");
            strcat(buf, ">");
            strcat(buf, Cayenne);
            strcat(buf, "</option> </select>");

            strcat(buf, downBuf);
            strcat(buf, "\n<input type=\"hidden\" name=\"DevEUI\" value=\"");
            strcat(buf, devEuiStr);
            strcat(buf, "\"><input type=\"hidden\" name=\"DevAddr\" value=\"");
            strcat(buf, devAddrStr);
            strcat(buf, "\">");
            strcat(buf, "<table class=\"frames\" border=\"1\">\n");

            if (!ai->sent) {
                strcpy(query, "SELECT frames.*, UNIX_TIMESTAMP(RecvTime), HEX(FRMPayload) FROM frames INNER JOIN activemotes ON frames.ID = activemotes.ID WHERE ");
                strcat(query, where);
                strcat(query, " ORDER BY RecvTime DESC LIMIT 20");
                if (mysql_query(session->sqlConn, query)) {
                    strcat(buf, "<tr>");
                    strcat(buf, query);
                    strcat(buf, ": ");
                    strcat(buf, mysql_error(session->sqlConn));
                    strcat(buf, "</tr>");
                    ai->form_state = FORM_STATE_END;
                } else {
                    ai->result = mysql_use_result(session->sqlConn);
                    ai->form_state = FORM_STATE_TABLE_HEADER;
                }
            } else
                ai->form_state = FORM_STATE_TABLE_HEADER;


            len = strlen(buf);

            break;
        case FORM_STATE_TABLE_HEADER:
            strcpy(buf, "<tr>"
"<th></th><th>FCnt</th><th>port</th><th>payload HEX</th><th>payload TEXT</th></tr>"
"<tr>"
"<td><input type=\"submit\" value=\"sendConf\" name=\"mtype\"><br>"
"<input type=\"submit\" value=\"sendUnconf\" name=\"mtype\"></td><td>");
            if (ai->AFCntDownStr) {
                strcat(buf, ai->AFCntDownStr);
                free(ai->AFCntDownStr);
            }
            strcat(buf, "</td><td><input type=\"input\" name=\"port\" size=\"4\" placeholder=\"1-255\"></td>"
"<td><input type=\"text\" name=\"payHex\" placeholder=\"hex digits\"></td>"
"<td><input type=\"input\" name=\"payText\" placeholder=\"ascii characters\"></td></tr>"
);

            ai->form_state = FORM_STATE_FORM;
            if (!ai->sent) {
                strcat(buf, "<tr><th></th><th></th><th></th><th></th><td>");

                if (ai->DevEUI64 != NONE_DEVEUI) {
                    strcat(buf, "<input type=\"submit\" value=\"");
                    strcat(buf, AppSKeyReq);
                    strcat(buf, "\" name=\"action\"></td>");
                }
                strcat(buf, "</td><th>DR</th><th>MHz</th><th>GWCnt</th><th>RSSI</th><th>SNR</th></tr>"
"<tr></tr>"
"<tr></tr>"
"</tr>");
                if (ai->result) {
                    ai->row = mysql_fetch_row(ai->result);
                    if (ai->row)
                        ai->form_state = FORM_STATE_TABLE_ROWS;
                } 
            }

            len = strlen(buf);
            break;
        case FORM_STATE_TABLE_ROWS:
            strcpy(buf, "<tr><td>");
            getAgo(ai->row[12], query);
            strcat(buf, query);
            if (ai->row[9][0] != '0')
                strcat(buf, " (conf)");
            strcat(buf, "</td><td>");
            strcat(buf, ai->row[7]);    // fcnt row[7]
            strcat(buf, "</td><td>");
            strcat(buf, ai->row[11]);    // fport row[11]
            strcat(buf, "</td><td>");
            if (ai->row[13])
                strcat(buf, ai->row[13]); //row[10] optional (default null)
            strcat(buf, "</td><td>");
            if (ai->row[10])
                strcat(buf, ai->row[10]); //row[10] optional (default null)
            strcat(buf, "</td><td>");
            if (ai->row[1])    
                strcat(buf, ai->row[1]);// row[1] optional (default null) DataRate
            strcat(buf, "</td><td>");
            if (ai->row[2]) {
                unsigned n;
                float f;
                sscanf(ai->row[2], "%u", &n);
                f = n / 1000000.0;
                sprintf(query, "%.1f", f);
                strcat(buf, query);// row[2] optional (default null) ULFreq MHz
            }
            strcat(buf, "</td><td>"); 
            if (ai->row[5])    
                strcat(buf, ai->row[5]);  // row[5] optional (default null) GWCnt
            strcat(buf, "</td><td>");
            if (ai->row[3])    
                strcat(buf, ai->row[3]);   // row[3] optional (default null) RSSI
            strcat(buf, "</td><td>");
            if (ai->row[4])    
                strcat(buf, ai->row[4]);    // row[4] optional (default null) SNR
            strcat(buf, "</td>");
            strcat(buf, "</tr>");
            len = strlen(buf);
            ai->row = mysql_fetch_row(ai->result);
            if (!ai->row)
                ai->form_state = FORM_STATE_FORM;
            break;
        case FORM_STATE_FORM:
            if (ai->result) {
                mysql_free_result(ai->result);
            }
            ai->form_state = FORM_STATE_END;
            strcpy(buf, "</table><input type=\"submit\" value=\"");
            strcat(buf, deleteFrames);
            strcat(buf, "\" name=\"action\"></form></body></html>");
            len = strlen(buf);
            //len = sprintf(buf, "</table><input type=\"submit\" value=\"deleteFrames\" name=\"action\"></form></body></html>");
            break;
        case FORM_STATE_END:
            ai->payloadLen = 0; // initialize in anticipation of submit
            return MHD_CONTENT_READER_END_OF_STREAM;
    } // ..switch (form_state)

    return len;
} // ..frames_page_iterator()

static int
serve_frames(const void *cls,
		const char *mime,
		struct Session *session,
		struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;
    appInfo_t* ai = session->appInfo;

    ai->form_state = FORM_STATE_START;
    response = MHD_create_response_from_callback (MHD_SIZE_UNKNOWN,
                                                1024,
                                                &frames_page_iterator,
                                                session,
                                                NULL);
    lib_add_session_cookie (session, response);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}

/**
 * List of all pages served by this HTTP server.
 */
struct Page pages[] =
  { /* url, mime, handler, handler_cls */
    { "/", "text/html",  &main_page, NULL },
    { postFailURL, "text/html", &serve_post_fail, NULL },
    { motesURL, "text/html", &serve_motes, NULL},
    { skeyURL, "text/html", &serve_skey, NULL},
    { framesURL, "text/html", &serve_frames, NULL},
    { NULL, NULL, &not_found_page, NULL } /* 404 */
  };

