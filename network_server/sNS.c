/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "sNS.h"
#include <math.h>

static void
from_mote_mac_cmd_to_string(uint8_t cmd, char* str)
{
    char buf[16];
    strcpy(str, "fromMote-");
    switch (cmd) {
        case MOTE_MAC_RESET_IND          : strcat(str, "RESET_IND"); break;  // 0x01
        case MOTE_MAC_LINK_CHECK_REQ     : strcat(str, "LINK_CHECK_REQ"); break; // 0x02
        case MOTE_MAC_LINK_ADR_ANS       : strcat(str, "LINK_ADR_ANS"); break;  // 0x03
        case MOTE_MAC_DUTY_CYCLE_ANS     : strcat(str, "DUTY_CYCLE_ANS"); break;    // 0x04
        case MOTE_MAC_RX_PARAM_SETUP_ANS : strcat(str, "RX_PARAM_SETUP_ANS"); break; // 0x05
        case MOTE_MAC_DEV_STATUS_ANS     : strcat(str, "DEV_STATUS_ANS"); break;    // 0x06
        case MOTE_MAC_NEW_CHANNEL_ANS    : strcat(str, "NEW_CHANNEL_ANS"); break;   // 0x07
        case MOTE_MAC_RX_TIMING_SETUP_ANS: strcat(str, "RX_TIMING_ANS"); break; // 0x08
        case MOTE_MAC_REKEY_IND          : strcat(str, "REKEY_IND"); break; // 0x0b
        case MOTE_MAC_DEVICE_TIME_REQ    : strcat(str, "DEVICE_TIME_REQ"); break; // 0x0d

        case MOTE_MAC_REJOIN_PARAM_ANS   : strcat(str, "REJOIN_PARAM_ANS"); break;    // 0x0f
        case MOTE_MAC_PING_SLOT_INFO_REQ : strcat(str, "PING_SLOT_INFO_REQ"); break;    // 0x10
        case MOTE_MAC_PING_SLOT_FREQ_ANS : strcat(str, "PING_SLOT_FREQ_ANS"); break;    // 0x11
        case MOTE_MAC_BEACON_TIMING_REQ  : strcat(str, "BEACON_TIMING_REQ"); break; // 0x12
        case MOTE_MAC_BEACON_FREQ_ANS    : strcat(str, "BEACON_FREQ_ANS"); break;   // 0x13
        default:
            sprintf(buf, "\e[31m0x%02x\e[0m ", cmd);
            strcat(str, buf);
            break;
    }
}

static void
to_mote_mac_cmd_to_string(uint8_t cmd, char* str)
{
    char buf[16];
    strcpy(str, "toMote-");
    switch (cmd) {
        case SRV_MAC_RESET_CONF             : strcat(str, "RESET_CONF"); break;
        case SRV_MAC_LINK_CHECK_ANS         : strcat(str, "LINK_CHECK_ANS"); break;
        case SRV_MAC_LINK_ADR_REQ           : strcat(str, "LINK_ADR_REQ"); break;
        case SRV_MAC_DUTY_CYCLE_REQ         : strcat(str, "DUTY_CYCLE_REQ"); break;
        case SRV_MAC_RX_PARAM_SETUP_REQ     : strcat(str, "RX_PARAM_SETUP_REQ"); break;
        case SRV_MAC_DEV_STATUS_REQ         : strcat(str, "DEV_STATUS_REQ"); break;
        case SRV_MAC_NEW_CHANNEL_REQ        : strcat(str, "NEW_CHANNEL_REQ"); break;
        case SRV_MAC_RX_TIMING_SETUP_REQ    : strcat(str, "RX_TIMING_SETUP_REQ"); break;
        case SRV_MAC_REKEY_CONF             : strcat(str, "REKEY_CONF"); break;
        case SRV_MAC_DEVICE_TIME_ANS        : strcat(str, "DEVICE_TIME_ANS"); break;
        case SRV_MAC_FORCE_REJOIN_REQ       : strcat(str, "FORCE_REJOIN_REQ"); break;  // 0x0e
        case SRV_MAC_REJOIN_PARAM_REQ       : strcat(str, "REJOIN_PARAM_REQ"); break;
        case SRV_MAC_PING_SLOT_INFO_ANS     : strcat(str, "PING_SLOT_INFO_ANS"); break;
        case SRV_MAC_PING_SLOT_CHANNEL_REQ  : strcat(str, "PING_SLOT_CHANNEL_REQ"); break;
        case SRV_MAC_BEACON_TIMING_ANS      : strcat(str, "BEACON_TIMING_ANS"); break;
        case SRV_MAC_BEACON_FREQ_REQ        : strcat(str, "BEACON_FREQ_REQ"); break;
        default:
            sprintf(buf, "\e[31m0x%02x\e[0m ", cmd);
            strcat(str, buf);
            break;
    }
}

static int get_cflist(char* out, const char* RFRegion)
{
    uint8_t CFListBin[24];
    struct _region_list* rl;
    uint8_t CFListLen = 0;
    region_t* region = NULL;

    for (rl = region_list; rl != NULL; rl = rl->next) {
        if (rl->region.RFRegion == RFRegion)
            region = &rl->region;
    }
    if (!region) {
        printf("\e[31mregion \"%s\" not found\e[0m\n", RFRegion);
        return -1;
    }

    if (region->regional.get_cflist) {
        uint8_t *a = CFListBin, *b;
        b = region->regional.get_cflist(a);
        CFListLen = b - a;
        printf("CFListLen:%u\n", CFListLen);
    } else
        printf("r->get_cflist == NULL\n");

    if (CFListLen > 0) {
        char* CFListStrPtr = out;
        unsigned n;
        for (n = 0; n < CFListLen; n++) {
            sprintf(CFListStrPtr, "%02x", CFListBin[n]);
            CFListStrPtr += 2;
        }
    } else
        out[0] = 0;

    return 0;
} // ..get_cflist()

static int
take_JoinAns(MYSQL* sc, mote_t* mote, unsigned lifetime, json_object* jobj, const char* senderID)
{
    const char* res;
    //uint32_t zero = 0;
    time_t until;
    time_t* untilPtr = NULL;
    int ret = 0;
    s_t* s = mote->s;
    json_object* obj;
    bool fromJS;

    if (lifetime == 0) {
        printf("zero lifetime\n");
        return -1;
    }
    until = time(NULL) + lifetime;
    untilPtr = &until;

    printf("take_JoinAns() devaddr %08x ", mote->devAddr);
    if (json_object_object_get_ex(jobj, DevAddr, &obj)) {
        sscanf(json_object_get_string(obj), "%x", &mote->devAddr);
        printf("-> %08x\n", mote->devAddr);
    }

    if (strlen(senderID) > 8) { // this is joinAns directly from join server
        char query[256];
        uint64_t joinEui;
        sscanf(senderID, "%"PRIx64, &joinEui);
        sprintf(query, "UPDATE motes SET JoinEUI = %"PRIu64" WHERE DevEUI = %"PRIu64, joinEui, mote->devEui);
        ret = mq_send(mqd, query, strlen(query)+1, 0);
        if (ret < 0)
            perror("take_JoinAns mq_send");
        fromJS = true;
    } else
        fromJS = false;

    printf("fromJS%u ", fromJS);

    if (fromJS)
        res = getKey(jobj, NwkSKey, &key_envelope_ns_js, mote->session.FNwkSIntKeyBin);
    else
        res = get_nsns_key(sc, jobj, NwkSKey, senderID, mote->session.FNwkSIntKeyBin);

    if (res == Success) {
        /* LoRaWAN 1.0 */
        print_buf(mote->session.FNwkSIntKeyBin, LORA_CYPHERKEYBYTES, "NwkSKey");
        memcpy(mote->session.SNwkSIntKeyBin, mote->session.FNwkSIntKeyBin, LORA_CYPHERKEYBYTES);
        memcpy(mote->session.NwkSEncKeyBin, mote->session.FNwkSIntKeyBin, LORA_CYPHERKEYBYTES);
        mote->session.OptNeg = false;
        if (ret == 0) {
            deleteNeverUsedSessions(sc, mote->devEui);
            s->checkOldSessions = true; // perform at first good uplink
            return add_database_session(sc, mote->devEui, untilPtr, mote->session.SNwkSIntKeyBin, mote->session.FNwkSIntKeyBin, mote->session.NwkSEncKeyBin, NULL, NULL, &mote->devAddr, false);
        } else printf("skip-add-database1v0 ");
    } else {
        /* LoRaWAN 1.1 */
        if (fromJS) {
            if (getKey(jobj, SNwkSIntKey, &key_envelope_ns_js, mote->session.SNwkSIntKeyBin) != Success) {
                printf("\e[31mno %s\e[0m\n", SNwkSIntKey);
                ret--;
            }
            if (getKey(jobj, FNwkSIntKey, &key_envelope_ns_js, mote->session.FNwkSIntKeyBin) != Success) {
                printf("\e[31mno %s\e[0m\n", FNwkSIntKey);
                ret--;
            }
            if (getKey(jobj, NwkSEncKey, &key_envelope_ns_js, mote->session.NwkSEncKeyBin) != Success) {
                printf("\e[31mno %s\e[0m\n", NwkSEncKey);
                ret--;
            }
        } else {
            if (get_nsns_key(sc, jobj, SNwkSIntKey, senderID, mote->session.SNwkSIntKeyBin) != Success) {
                printf("\e[31mno %s\e[0m\n", SNwkSIntKey);
                ret--;
            }
            if (get_nsns_key(sc, jobj, FNwkSIntKey, senderID, mote->session.FNwkSIntKeyBin) != Success) {
                printf("\e[31mno %s\e[0m\n", FNwkSIntKey);
                ret--;
            }
            if (get_nsns_key(sc, jobj, NwkSEncKey, senderID, mote->session.NwkSEncKeyBin) != Success) {
                printf("\e[31mno %s\e[0m\n", NwkSEncKey);
                ret--;
            }
        }
        mote->session.OptNeg = true;

        if (ret == 0) {
            deleteNeverUsedSessions(sc, mote->devEui);
            /* TODO: the network server shall discard any uplink frames protected with the new security context that are received after the transmission of JoinAccept and before the first uplink frame that carries a RekeyInd command */
            s->checkOldSessions = true; // perform at first good uplink, upon receiving ReKeyInd
            s->ConfFCntDown = 0;
            return add_database_session(sc, mote->devEui, untilPtr, mote->session.SNwkSIntKeyBin, mote->session.FNwkSIntKeyBin, mote->session.NwkSEncKeyBin, NULL, NULL, &mote->devAddr, true);
        } else printf("skip-add-database ");

    }

    /* since new session, any unsent downlink is from previous session */
    s->downlink.FRMPayloadLen = 0;

    printf(" Returning%u ", ret);
    return ret;
} // ..take_JoinAns()

static void
sNS_HRStartAnsCallback(MYSQL* sc, mote_t* mote, json_object* jobj, const char* rxResult, const char* senderID, const uint8_t* rfBuf, uint8_t rfLen)
{
    //s_t* s = mote->s;
    uint32_t rxNetID;
    unsigned lifetime = 0;
    json_object* obj;

    sscanf(senderID, "%x", &rxNetID);

    printElapsed(mote);
    printf("sNS_HRStartAnsCallback() %s ", rxResult);

    if (json_object_object_get_ex(jobj, Lifetime, &obj)) {
        lifetime = json_object_get_int(obj);
        printf("lifetime%u ", lifetime);
    } else
        printf("\e[31mno-%s\e[0m ", Lifetime);

    if (rxResult == StaleDeviceProfile) {
        printf(" %s ", rxResult);
        if (json_object_object_get_ex(jobj, DeviceProfile, &obj)) {
            json_object* ob;
            if (json_object_object_get_ex(jobj, DeviceProfileTimestamp, &ob)) {
                printf(" theirs:%s ", json_object_get_string(ob));
                if (saveDeviceProfile(sc, mote, obj, json_object_get_string(ob)) == 0) {
                    sNS_sendHRStartReq(sc,mote, rxNetID);
                } else
                    printf("\e[31msNS_HRStartAnsCallback saveDeviceProfile\e[0m\n");
            } else
                printf("\e[31mmissing %s\e[0m\n", DeviceProfileTimestamp);
        } else
            printf("\e[31mmissing %s\e[0m\n", DeviceProfile);
    } else {
        if (rfBuf && rxResult == Success) {
            const mhdr_t* mhdr = (mhdr_t*)rfBuf;

            //print_mtype(mhdr->bits.MType);
            if (mhdr->bits.MType == MTYPE_JOIN_ACC) {
                if (take_JoinAns(sc, mote, lifetime, jobj, senderID) < 0)
                    rxResult = Other;
            } else {
                printf("\e[31mHRStart-");
                print_mtype(mhdr->bits.MType);
                printf("\e[0m ");
            }
        }

        if (json_object_object_get_ex(jobj, DevAddr, &obj)) {
            /* sNS generated a DevAddr for an OTA join, we must save it to recognize future uplinks */
            sscanf(json_object_get_string(obj), "%x", &mote->devAddr);
            printf("->%08x ", mote->devAddr);
        } else
            printf("\e[31mmissing DevAddr\e[0m ");   // new devAddr was assigned but not provided

        xRStartAnsCallback(sc, true, mote, jobj, rxResult, senderID, rfBuf, rfLen, true, lifetime);
    }

} // ..sNS_HRStartAnsCallback()

static int
readServiceProfileFromSql(MYSQL* sc, uint64_t devEui, uint32_t devAddr, ServiceProfile_t* out)
{
    char where[128];
    char query[512];
    MYSQL_RES *result;
    int ret = -1;

    if (!out)
        return -1;  // no place to put profile into

    memset(out, 0, sizeof(ServiceProfile_t));

    if (getMotesWhere(sc, devEui, devAddr, where) != Success) {
        printf("\e[31mreadServiceProfileFromSql getMotesWhere failed\e[0m\n");
        return ret;
    }
    strcpy(query, "SELECT ServiceProfiles.* FROM ServiceProfiles INNER JOIN motes ON ServiceProfiles.ServiceProfileID = motes.ID WHERE ");
    strcat(query, where);
    if (mysql_query(sc, query)) {
        printf("\e[31m%s: %s\e[0m\n", query, mysql_error(sc));
        return ret;
    }
    result = mysql_use_result(sc);
    if (result != NULL) {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            /*int i, num_fields = mysql_num_fields(result);
            for (i = 0; i < num_fields; i++) {
                printf("%d) %s\n", i, row[i]);
            }*/
            sscanf(row[1], "%u", &out->ULRate);
            sscanf(row[2], "%u", &out->ULBucketSize);
            strncpy(out->ULRatePolicy, row[3], sizeof(out->ULRatePolicy));
            sscanf(row[4], "%u", &out->DLRate);
            sscanf(row[5], "%u", &out->DLBucketSize);
            strncpy(out->DLRatePolicy, row[6], sizeof(out->DLRatePolicy));
            out->AddGWMetadata = row[7][0] == '1';
            sscanf(row[8], "%u", &out->DevStatusReqFreq);
            out->ReportDevStatusBattery= row[9][0] == '1';
            out->ReportDevStatusMargin= row[10][0] == '1';
            sscanf(row[11], "%u", &out->DRMin);
            sscanf(row[12], "%u", &out->DRMax);
            strncpy(out->ChannelMask, row[13], sizeof(out->ChannelMask));
            out->PRAllowed= row[14][0] == '1';
            out->HRAllowed = row[15][0] == '1';
            out->RAAllowed = row[16][0] == '1';
            out->NwkGeoLoc = row[17][0] == '1';
            sscanf(row[18], "%f", &out->TargetPER);
            sscanf(row[19], "%u", &out->MinGWDiversity);
            ret = 0;
        } else
            printf("\e[31mreadServiceProfileFromSql %s no row\e[0m\n", where);

        mysql_free_result(result);
    } else {
        unsigned err = mysql_errno(sc);
        printf("\e[31mreadServiceProfileFromSql no result, err %d: %s --- %s\e[0m\n", err, where, mysql_error(sc));
    }

    return ret;
} // ..readServiceProfileFromSql()

int
sNS_sendHRStartReq(MYSQL* sc, mote_t* mote, uint32_t homeNetID)
{
    CURL* easy;
    uint32_t tid;
    char CFListStr[64];
    uint8_t dlSettings;
    int nxfers, ret = -1, n;
    char buf[512];
    char* strPtr;
    char hostname[64];
    json_object *jobj, *uj;

    if (homeNetID == myNetwork_id32) {
        printf("\e[31mHRStartReq %06x\e[0m\n", homeNetID);
        return -1;
    }

    if (next_tid(mote, "sNS_sendHRStartReq", sNS_HRStartAnsCallback, &tid) < 0) {
        printf("\e[31mhrstart-tid-fail\e[0m\n");
        return -1;
    }

    if (!mote->s) {
        mote->s = calloc(1, sizeof(s_t));
        network_controller_mote_init(mote->s, mote->ulmd_local.RFRegion, "sNS_sendHRStartReq");
    }

    if (mote->writtenDeviceProfileTimestamp[0] != 0) {
        strcpy(buf, mote->writtenDeviceProfileTimestamp);
    } else {
        if (getDeviceProfileTimestamp(sc, mote, buf, sizeof(buf), NULL) < 0) {
            printf("\e[31mgetDeviceProfileTimestamp() failed\e[0m\n");
            return ret;
        }
    }
    printf(" sNS_sendHRStartReq(,%06x) ours:%s ", homeNetID, buf);
    uj = generateULMetaData(&mote->ulmd_local, fNS, true);
    if (!uj) {
        printf("\e[31msNS_sendHRStartReq ulmd_local failed\e[0m\n");
        return ret;
    }
    jobj = json_object_new_object();

    json_object_object_add(jobj, DeviceProfileTimestamp, json_object_new_string(buf));

    /* PHYPayload */
    strPtr = buf;
    for (n = 0; n < mote->ULPHYPayloadLen; n++) {
        sprintf(strPtr, "%02x", mote->ULPayloadBin[n]);
        strPtr += 2;
    }
    json_object_object_add(jobj, PHYPayload, json_object_new_string(buf));  // sNS_sendHRStartReq

    /* this NS supports 1.1, NS were sending this to already knows ED mac version */
    json_object_object_add(jobj, MACVersion, json_object_new_string("1.1"));

    json_object_object_add(jobj, ULMetaData, uj);

    if (mote->devAddr != NONE_DEVADDR) {
        sprintf(buf, "%x", mote->devAddr);
        json_object_object_add(jobj, DevAddr, json_object_new_string(buf));
    }

    deviceProfileReq(sc, mote->devEui, mote->devAddr, RXDataRate2, buf, sizeof(buf));
    sscanf(buf, "%u", &n);
    dlSettings = n;
    deviceProfileReq(sc, mote->devEui, mote->devAddr, RXDROffset1, buf, sizeof(buf));
    sscanf(buf, "%u", &n);
    dlSettings |= n << 4;

    sprintf(buf, "%02x", dlSettings);   // RX1DrOffset, RX2DataRate
    json_object_object_add(jobj, DLSettings, json_object_new_string(buf));

    deviceProfileReq(sc, mote->devEui, mote->devAddr, RXDelay1, buf, sizeof(buf));
    sscanf(buf, "%u", &n);
    if (n == 0)
        n = 1;
    json_object_object_add(jobj, RxDelay, json_object_new_int(n));


    get_cflist(CFListStr, mote->ulmd_local.RFRegion);
    if (CFListStr[0] != 0)
        json_object_object_add(jobj, CFList, json_object_new_string(CFListStr));

    sprintf(hostname, "%06x.%s", homeNetID, netIdDomain);

    sprintf(buf, "%x", homeNetID);
    lib_generate_json(jobj, buf, myNetwork_idStr, tid, HRStartReq, NULL);

    printElapsed(mote);
    JSON_PRINTF("toNS  %s ", json_object_to_json_string(jobj));

    easy = curl_easy_init();
    if (!easy)
        return CURLE_FAILED_INIT;   // TODO appropriate return
    curl_multi_add_handle(multi_handle, easy);

    ret = http_post_hostname(easy, jobj, hostname, true);
    CURLMcode mc = curl_multi_perform(multi_handle, &nxfers);
    if (mc != CURLM_OK)
        printf("sNS_sendHRStartReq %s = curl_multi_perform(),%d ", curl_multi_strerror(mc), nxfers);

    return ret;
} // ..sNS_sendHRStartReq()

int sNS_band_conv(uint8_t rw, uint64_t devEui, uint32_t devAddr, float ULFreq, uint8_t ULDataRate, uint8_t rxdrOffset1, const char* ULRFRegion, DLMetaData_t* dlMetaData)
{
    struct _region_list* rl;

    MAC_PRINTF(" sNS_band_conv() ULFreq:%.2fMHz, ULdr:%u RXDROffset1:%u\n", ULFreq, ULDataRate, rxdrOffset1);
    for (rl = region_list; rl != NULL; rl = rl->next) {
        if (rl->region.RFRegion != ULRFRegion)
            continue;

        if (rw == 1) {
            if (rl->region.regional.rx1_band_conv != NULL) {
                rl->region.regional.rx1_band_conv(ULFreq, ULDataRate, rxdrOffset1, dlMetaData);
                return 0;
            } else
                return -1;
        } else if (rw == 2) {
            unsigned n;
            char str[32];
            dlMetaData->DLFreq1 = 0;
            if (deviceProfileReq(sqlConn_lora_network, devEui, devAddr, RXFreq2, str, sizeof(str)) < 0)
                return -1;

            sscanf(str, "%f", &(dlMetaData->DLFreq2));

            if (deviceProfileReq(sqlConn_lora_network, devEui, devAddr, RXDataRate2, str, sizeof(str)) < 0)
                return -1;

            sscanf(str, "%u", &n);
            dlMetaData->DataRate2 = n;
            //dlMetaData->DLFreq2 = rl->region.regional.Rx2Channel.FrequencyHz / 1000000.0;
            //dlMetaData->DataRate2 = rl->region.regional.Rx2Channel.Datarate;
            return 0;
        } else
            return -1;
    }

    return -1;
}

/* return new roam state */
const char*
generateXRStartAnsSuccess(MYSQL* sc, const mote_t* mote, const uint8_t* rfBuf, uint8_t rfLen, json_object* jdl, uint32_t cli_netID, requester_t* r, json_object* ans_jobj, unsigned lifetime)
{
    char buf[512];
    const char* newState = NULL;

    if (lifetime > 0) {
        char query[256];
        const session_t* se = &mote->session;
        bool keysPassive = false;
        bool keysHandover = false;
        key_envelope_t key_envelope;

        json_object* spObj = jsonGetServiceProfile(mote->devEui, NONE_DEVADDR);

        if (spObj)
            json_object_object_add(ans_jobj, ServiceProfile, spObj);
        else {
            printf("\e[31mServiceProfile-fail\e[0m ");
            return NULL;
        }

        if (r->MessageType == PRStartReq) {
            /* stateful fNS */
            if (isNetID(sc, cli_netID, "fMICup") == 1)
                keysPassive = true;

            newState = roamsPASSIVE;
            printf(" -> sPASSIVE ");
        } else if (r->MessageType == HRStartReq) {
            keysHandover = true;

            newState = roamhHANDOVER;
            printf(" -> hHANDOVER ");
        } else if (r->MessageType == XmitDataReq) {
            sql_t sql;
            /* new session was generated during roaming, new keys must be sent */
            sql_motes_query(sc, mote->devEui, mote->devAddr, &sql);
            if (sql.roamState == roamhHANDOVER)
                keysHandover = true;
            else if (sql.roamState == roamsPASSIVE) {
                if (isNetID(sc, cli_netID, "fMICup") == 1)
                    keysPassive = true;
            }
        } else
            printf("\e[31mgenerateXRStartAnsSuccess todo-%s\e[0m ", r->MessageType);

        key_envelope.kek_label = NULL;
        key_envelope.key_bin = NULL;

        sprintf(query, "SELECT KEKlabel, KEK FROM roaming WHERE NetID = %u ", cli_netID);
        if (!mysql_query(sc, query)) {
            MYSQL_RES *result = mysql_use_result(sc);
            if (result) {
                MYSQL_ROW row = mysql_fetch_row(result);
                if (row && row[0] && row[1]) {
                    key_envelope.kek_label = malloc(strlen(row[0])+1);
                    strcpy(key_envelope.kek_label, row[0]);
                    key_envelope.key_len = LORA_CYPHERKEYBYTES;
                    key_envelope.key_bin = malloc(key_envelope.key_len);
                    memcpy(key_envelope.key_bin, row[1], LORA_CYPHERKEYBYTES);
                }
                mysql_free_result(result);
            }
        }

        if (keysPassive) {
            json_object* kobj = create_KeyEnvelope(se->FNwkSIntKeyBin, &key_envelope);
            if (se->OptNeg) {
                json_object_object_add(ans_jobj, FCntUp, json_object_new_int(0));   //assuming this is sent due to Join-accept
                json_object_object_add(ans_jobj, FNwkSIntKey, kobj);
            } else
                json_object_object_add(ans_jobj, NwkSKey, kobj);
        } else if (keysHandover) {
            json_object* kobj = create_KeyEnvelope(se->FNwkSIntKeyBin, &key_envelope);
            if (se->OptNeg) {
                json_object_object_add(ans_jobj, FNwkSIntKey, kobj);
                kobj = create_KeyEnvelope(se->SNwkSIntKeyBin, &key_envelope);
                json_object_object_add(ans_jobj, SNwkSIntKey, kobj);
                kobj = create_KeyEnvelope(se->NwkSEncKeyBin, &key_envelope);
                json_object_object_add(ans_jobj, NwkSEncKey, kobj);
                json_object_object_add(ans_jobj, FCntUp, json_object_new_int(0));   //assuming this is sent due to Join-accept
            } else
                json_object_object_add(ans_jobj, NwkSKey, kobj);
        }

        if (key_envelope.kek_label)
            free(key_envelope.kek_label);
        if (key_envelope.key_bin)
            free(key_envelope.key_bin);

        if (mote->devEui != NONE_DEVEUI) {
            sprintf(buf, "%"PRIx64, mote->devEui);
            json_object_object_add(ans_jobj, DevEUI, json_object_new_string(buf));
        }

        json_object_object_add(ans_jobj, Lifetime, json_object_new_int(lifetime));
    } // ..if (lifetime > 0)
    else printf("zero-lifetime ");

    json_object_object_add(ans_jobj, DLMetaData, jdl); // PRStartAns going to fNS

    if (rfLen > 0) {
        unsigned n;
        char* strPtr = buf;
        mhdr_t* mhdr = (mhdr_t*)rfBuf;
        print_mtype(mhdr->bits.MType);

        /* PHYPayload */
        strPtr = buf;
        for (n = 0; n < rfLen; n++) {
            sprintf(strPtr, "%02x", rfBuf[n]);
            strPtr += 2;
        }
        json_object_object_add(ans_jobj, PHYPayload, json_object_new_string(buf)); // sNS_JoinAnsJson
    }

    /* sending DevAddr even if zero lifetime, so fNS can identify subsequent uplinks */
    sprintf(buf, "%x", mote->devAddr);
    json_object_object_add(ans_jobj, DevAddr, json_object_new_string(buf));
    /* TODO must send DevAddr at join-accept for OTA, but not needed for ABP answer */

    return newState;
} // ..generateXRStartAnsSuccess()

static int
sNS_JoinAnsJson(MYSQL* sc, mote_t* mote, json_object* jdl, const char* result, const uint8_t* rfBuf, uint8_t rfLen)
{
    int ret = -1;
    struct _requesterList* rl;
    unsigned nReq = 0;

    printf("sNS_JoinAnsJson() %08x ", mote->devAddr);

    for (rl = mote->requesterList; rl != NULL; rl = rl->next) {
        const char* newState = NULL;
        char hostname[128];
        uint32_t cli_netID;
        requester_t* r;
        json_object* ans_jobj;
        uint32_t otherNetID;
        const char* ansMt;
        time_t until;
        time_t* untilPtr = NULL;
        CURL* easy;

        if (rl->R == NULL)  {
            printf("rl->R==NULL ");
            continue;
        }

        r = rl->R;
        if (!r->needsAnswer) {
            printf("!r->needsAnswer)");
            continue;
        }

        nReq++;

        if (r->MessageType == XmitDataReq)
            ansMt = XmitDataAns;
        else if (r->MessageType == PRStartReq)
            ansMt = PRStartAns;
        else if (r->MessageType == HRStartReq)
            ansMt = HRStartAns;
        else {
            //ansMt = "<unknown>";
            printf("\e[31msNS_JoinAnsJson r->MessageType \"%s\"\e[0m\n", r->MessageType);
            continue;
        }

        sscanf(r->ClientID, "%x", &otherNetID);
        sscanf(r->ClientID, "%x", &cli_netID);
        sprintf(hostname, "%06x.%s", cli_netID, netIdDomain);
        HTTP_PRINTF("client hostname %s\n", hostname);

        ans_jobj = json_object_new_object();

        if (result == Success) {
            int lifetime = getLifetime(sc, mote->devEui, NONE_DEVADDR);   // our lifetime
            printf(" lifetime%d %s optneg%u ", lifetime, r->MessageType, mote->session.OptNeg);
            if (lifetime < 0) {
                printf("\e[31msNS_JoinAnsJson lifetime fail\e[0m\n");
                result = Other;
            } else {
                if (lifetime > 0) {
                    until = time(NULL) + lifetime;
                    untilPtr = &until;
                }
                newState = generateXRStartAnsSuccess(sc, mote, rfBuf, rfLen, jdl, cli_netID, r, ans_jobj, lifetime);
                if (!newState)
                    result = Other;
            }
        } // ..if (result == Success)

        easy = curl_easy_init();
        if (!easy)
            continue;
        curl_multi_add_handle(multi_handle, easy);

//jaAns:
        lib_generate_json(ans_jobj, r->ClientID, myNetwork_idStr, r->inTid, ansMt, result);
        printf("sNS_JoinAnsJson to %s %s ", r->ClientID, json_object_to_json_string(ans_jobj));
        fflush(stdout);
        ret = http_post_hostname(easy, ans_jobj, hostname, false);
        if (ret == 0) {
            printf("\e[47;30mnewState:");
            if (newState != NULL) {
                bool fMICup;
                if (untilPtr)
                    printf("%s until:%lu ", newState, *untilPtr);
                else
                    printf("until:NULL ");
                printf("other:%06x ", otherNetID);
                if (isNetID(sc, cli_netID, "fMICup") == 1)
                    fMICup = true;
                else
                    fMICup = false;

                mote_update_database_roam(sc, mote->devEui, mote->devAddr, newState, untilPtr, &otherNetID, &fMICup);
            } else
                printf("NULL");
            printf("\e[0m\n");
            r->needsAnswer = false;
        } else
            printf("\e[31msNS_JoinAnsJson http_post_hostname failed\e[0m ");
    } // ..for (rl = mote->requesterList; rl != NULL; rl = rl->next)

    if (nReq == 0)
        printf("\e[31msNS_JoinAnsJson nothing sent\e[0m ");

    deleteOldSessions(sc, mote->devEui, false);

    return ret;
} // ..sNS_JoinAnsJson()

/* starting roaming on conf-unconf, not join */
const char*
sNS_answer_RStart_Success(mote_t* mote, json_object* ans_jobj)
{
    s_t* s = mote->s;
    uint32_t cli_netID;
    json_object* jobj_dlmd;
    int lifetime;
    DLMetaData_t dlMetaData = { 0 };
    requester_t* r = *(mote->bestR);

    printf("sNS_answer_RStart_Success() %016"PRIx64" / %08x: ", mote->devEui, mote->devAddr);
    lifetime = getLifetime(sqlConn_lora_network, mote->devEui, mote->devAddr);   // our lifetime
    if (lifetime < 0) {
        printf("\e[31msNS_answer_RStart_Success lifetime fail\e[0m\n");
        return Other;
    }

    dlMetaData.RXDelay1 = 1;    // get from service profile.  This should only occur on (un)conf
    dlMetaData.DevEUI = mote->devEui;
    dlMetaData.DevAddr = mote->devAddr;
    dlMetaData.ClassMode = 'A'; // roaming start answer is always in response to an uplink

    jobj_dlmd = generateDLMetaData(&r->ulmd, mote->rxdrOffset1, &dlMetaData, sNS);


    sscanf(r->ClientID, "%x", &cli_netID);

    generateXRStartAnsSuccess(sqlConn_lora_network, mote, s->downlink.PHYPayloadBin, s->downlink.PHYPayloadLen, jobj_dlmd, cli_netID, r, ans_jobj, lifetime);

    /* state will be saved after posting answer */

    return Success;
}

void
sNS_answer_RStart_Success_save(mote_t* mote)
{
    time_t until;
    time_t* untilPtr = NULL;
    uint32_t otherNetID;
    bool fMICup;
    const char* newState = NULL;
    int lifetime = getLifetime(sqlConn_lora_network, mote->devEui, mote->devAddr);   // our lifetime
    requester_t* r = *(mote->bestR);

    if (lifetime < 0)
        printf("\e[31mssNS_answer_RStart_Success_save lifetime fail\e[0m\n");

    sscanf(r->ClientID, "%x", &otherNetID);

    if (isNetID(sqlConn_lora_network, otherNetID, "fMICup") == 1)
        fMICup = true;
    else
        fMICup = false;

    if (r->MessageType == HRStartReq) {
        newState = roamhHANDOVER;
        printf(" -> hHANDOVER ");
    } else if (r->MessageType == PRStartReq) {
        newState = roamsPASSIVE;
        printf(" -> sPASSIVE ");
    } else
        printf("\e[31msNS_answer_RStart_Success_save TODO %s\e[0m\n", r->MessageType);


    if (lifetime > 0) {
        until = time(NULL) + lifetime;
        untilPtr = &until;
    }

    mote_update_database_roam(sqlConn_lora_network, mote->devEui, mote->devAddr, newState, untilPtr, &otherNetID, &fMICup);

} // ..sNS_answer_RStart_Success_save()

int
getPingConfig(uint64_t devEui, uint32_t devAddr, DLMetaData_t *out)
{
    unsigned n;
    char str[64];

    if (deviceProfileReq(sqlConn_lora_network, devEui, devAddr, PingSlotDR, str, sizeof(str)) < 0)
        return -1;

    sscanf(str, "%u", &n);
    out->DataRate1 = n;

    if (deviceProfileReq(sqlConn_lora_network, devEui, devAddr, PingSlotFreq, str, sizeof(str)) < 0)
        return -1;

    sscanf(str, "%f", &out->DLFreq1);
    return 0;
}

static const char*
sNS_phy_downlink_local(mote_t* mote, ULMetaData_t* ulmd, uint8_t rxDelay1, char classMode)
{
    const char* ret;
    s_t* s = mote->s;
    DLMetaData_t dlMetaData = { 0 };

    printf("sNS_phy_downlink_local() ");

    if (s->downlink.PHYPayloadLen == 0) {
        printf("\e[31mlocal_phy_downlink zero payload length\e[0m\n");
        return XmitFailed;
    }
    if (ulmd->FNSULToken) {
        unsigned len = strlen(ulmd->FNSULToken);
        if (len > 0) {
            dlMetaData.FNSULToken = malloc(len+1);
            strcpy(dlMetaData.FNSULToken, ulmd->FNSULToken);
        }
    }

    dlMetaData.ClassMode = classMode;
    if (s->ClassB.ping_period > 0 && classMode == 'B') {
        if (getPingConfig(mote->devEui, mote->devAddr, &dlMetaData) < 0)
            return XmitFailed;
        dlMetaData.PingPeriod = s->ClassB.ping_period;
    } else if (classMode == 'A') {
        dlMetaData.RXDelay1 = rxDelay1;
        sNS_band_conv(dl_rxwin, mote->devEui, mote->devAddr, ulmd->ULFreq, ulmd->DataRate, mote->rxdrOffset1, ulmd->RFRegion, &dlMetaData);
    } else if (classMode == 'C') {
        sNS_band_conv(2, mote->devEui, mote->devAddr, ulmd->ULFreq, ulmd->DataRate, mote->rxdrOffset1, ulmd->RFRegion, &dlMetaData);
    } else {
        printf("\e[31mTODO sNS_phy_downlink_local ClassMode %c\e[0m\n", classMode);
        return XmitFailed;
    }

    MAC_PRINTF(" sNS->");
    ret = fNS_downlink(&dlMetaData, 0, NULL, s->downlink.PHYPayloadBin, s->downlink.PHYPayloadLen, "sNS_phy_downlink_local");
    /* NULL = fNS_downlink() for deferred downlink, result to be given later */

    if (ret != NULL && ret != Success) {
        s->DLFreq1 = 0;
        s->DLFreq2 = 0;
        printf("\e[31mlocal phy downlink failed from fNS_downlink()\e[0m\n");
    } else {
        //printf(" clear-downphylen\n");
        s->downlink.PHYPayloadLen = 0; // prevent sending again in sNS_finish_phy_downlink()

        s->DLFreq1 = dlMetaData.DLFreq1;
        s->DLFreq2 = dlMetaData.DLFreq2;
    }

    dlmd_free(&dlMetaData);

    return ret;
} // ..sNS_phy_downlink_local()

static void
wipe_queued_mac_cmds(s_t* s)
{
    /* clear out any mac commands from previous session */
    uint8_t my_out_idx = s->mac_cmd_queue_out_idx;

    while (s->mac_cmd_queue_in_idx != my_out_idx) {
        printf( "%02x,%u ", s->mac_cmd_queue[my_out_idx].buf[0], s->mac_cmd_queue[my_out_idx].len);
        s->mac_cmd_queue[my_out_idx].len = 0;
        s->mac_cmd_queue[my_out_idx].buf[0] = 0;

        if (++my_out_idx == MAC_CMD_QUEUE_SIZE)
            my_out_idx = 0;
    }
    printf("\n");
}

int
hNs_to_sNS_JoinAns(MYSQL* sc, mote_t* mote, json_object* jobj, const char* rxResult, const char* senderID, const uint8_t* rfBuf, uint8_t rfLen)
{
    DLMetaData_t dlMetaData = { 0 };
    const char* result = Other;
    s_t* s;
    int ret = -1;
    json_object* obj;
    unsigned lifetime = 0;

    if (rxResult != Success) {
        printf("\e[31mhNs_to_sNS_JoinAns %s, answering only best\e[0m\n", rxResult);
        if (mote->bestR != NULL) {
            obj = json_object_new_object();
            if (r_post_ans_to_ns(mote->bestR, rxResult, obj) < 0)
                printf("\e[31mr_post_ans_to_ns failed\e[0m\n");
        }
        return ret;
    }
    printf("hNs_to_sNS_JoinAns() ");
    if (mote->bestR == NULL)
        printf("to-local ");
    else
        printf("to-json-remote ");

    s = mote->s;

    if (json_object_object_get_ex(jobj, Lifetime, &obj)) {
        lifetime = json_object_get_int(obj);

        /* Always take session keys, even when handing over sNS,
         * because key transfer might use different envelope */
        if (take_JoinAns(sc, mote, lifetime, jobj, senderID) == 0)
            result = Success;
        else {
            result = Other;
            printf("\e[31mtake_JoinAns failed\e[0m\n");
        }

        result = Success;
    } else {
        printf("\e[31mJoinAns missing %s\e[0m\n", Lifetime);
        result = Other;
    }

    s->session_start_at_uplink = true;

    if (result == Success) {
        dlMetaData.DevEUI = mote->devEui;
        dlMetaData.DevAddr = NONE_DEVADDR;
        dlMetaData.FPort = 255; // not used for Join Accept
        dlMetaData.FCntDown = 0; // not used for Join Accept
        dlMetaData.Confirmed = false; // not used for Join Accept
        dlMetaData.ClassMode = 'A'; // Join Accept always class A
        dlMetaData.RXDelay1 = 5;    // all regions same?
        dlMetaData.HiPriorityFlag = false;
    }

    printElapsed(mote);
    MAC_PRINTF("hNs_to_sNS_JoinAns from %s mote->bestR:%p, devAddr:%08x\n", senderID, mote->bestR, mote->devAddr);
    if (mote->bestR != NULL) {
        requester_t *r = *(mote->bestR);

        /* send over json */
        json_object* jobj_dlmd = generateDLMetaData(&r->ulmd, mote->rxdrOffset1, &dlMetaData, sNS);
        s->DLFreq1 = dlMetaData.DLFreq1;
        s->DLFreq2 = dlMetaData.DLFreq2;

        MAC_PRINTF("hNs_to_sNS_JoinAns send over json\n");
        /* better be just one answer being sent */

        if (jobj_dlmd) {
            ret = sNS_JoinAnsJson(sc, mote, jobj_dlmd, result, rfBuf, rfLen);
            if (ret < 0)
                printf("\e[31mhNs_to_sNS_JoinAns sNS_JoinAnsJson fail\e[0m\n");
        } else
            printf("\e[31mhNs_to_sNS_JoinAns generateDLMetaData failed\e[0m\n");
    } else {
        if (result == Success) {
            MAC_PRINTF("JoinAns-local rfLen%u ", rfLen);
            memcpy(s->downlink.PHYPayloadBin, rfBuf, rfLen);
            s->downlink.PHYPayloadLen = rfLen;
            if (sNS_phy_downlink_local(mote, &mote->ulmd_local, dlMetaData.RXDelay1, 'A') == Success) {
                ret = 0;
            } else
                printf("\e[31mhNs_to_sNS_JoinAns sNS_phy_downlink_local fail\e[0m\n");
        } else
            printf("\e[31msend-local result %s\e[0m\n", result);
    }

    if (ret == 0) {
        wipe_queued_mac_cmds(s);
        s->confDLPHYPayloadLen = 0; // remove any saved confirmed downlink
    }

    dlmd_free(&dlMetaData);
    MAC_PRINTF("ret%d\n", ret);
    return ret;
} // ..hNs_to_sNS_JoinAns()

static int
init_session(mote_t* mote, const char* RFRegion)
{
    struct _region_list* rl;

    for (rl = region_list; rl != NULL; rl = rl->next) {
        if (rl->region.RFRegion == RFRegion) {
            s_t* s = mote->s;
            /* some regions dont have startup mac commands */
            if (rl->region.regional.init_session != NULL) {
                rl->region.regional.init_session(mote, &rl->region.regional);
                s->send_start_mac_cmds = true;
            }

            if (rl->region.regional.enableTxParamSetup) {
                uint8_t cmd_buf[MAC_CMD_SIZE];
                cmd_buf[0] = SRV_MAC_TX_PARAM_SETUP_REQ;
                cmd_buf[1] = rl->region.regional.TxParamSetup.octet;
                put_queue_mac_cmds(s, 2, cmd_buf, true);
            }
            return 0;
        }
    }

    return -1;
}

/** @brief put mac command onto queue, to be sent to mote
 * @param mote end-node
 * @param cmd_len count of bytes in mac command
 * @param cmd_buf mac command buffer to put
 * @param needs_answer true = mote must generate corresponding ANS mac command
 */
void put_queue_mac_cmds(s_t* s, uint8_t cmd_len, uint8_t* cmd_buf, bool needs_answer)
{
    char str[64];
    /* previously sent mac command cleanup:
     * when mac commands are sent (and acked if needed), len is set to 0 instead of incrementing out_idx */
    while (s->mac_cmd_queue[s->mac_cmd_queue_out_idx].buf[0] == 0) {
        MAC_PRINTF("finished-up mac cmd at %d\n", s->mac_cmd_queue_out_idx);
        s->mac_cmd_queue[s->mac_cmd_queue_out_idx].buf[0] = 0xff;    // unused command
        if (++s->mac_cmd_queue_out_idx == MAC_CMD_QUEUE_SIZE)
            s->mac_cmd_queue_out_idx = 0;
    }

    if (cmd_len >= MAC_CMD_SIZE) {
        printf("[31mmac_cmd length %d[0m\n", cmd_len);
        return;
    }

    {   /* test if queue would become full by putting this in */
        uint8_t test_in_idx = s->mac_cmd_queue_in_idx;
        if (++test_in_idx == MAC_CMD_QUEUE_SIZE)
            test_in_idx = 0;

        if (test_in_idx == s->mac_cmd_queue_out_idx) {
            printf("[31mcmd queue would become full[0m\n");
            return;
        }
    }

    /* is this command already in queue? then dont put another in until previous is sent */
    uint8_t my_out_idx = s->mac_cmd_queue_out_idx;
    while (s->mac_cmd_queue_in_idx != my_out_idx) {
        if (s->mac_cmd_queue[my_out_idx].len != 0) {
            if (s->mac_cmd_queue[my_out_idx].buf[0] == cmd_buf[0]) {
                //to_mote_mac_cmd_to_string( s->mac_cmd_queue[s->mac_cmd_queue_in_idx].buf[0] , str);
                to_mote_mac_cmd_to_string(s->mac_cmd_queue[my_out_idx].buf[0] , str);
                MAC_PRINTF("put-mac-cmd %s already in queue\n", str);
                return;
            }
        }

        if (++my_out_idx == MAC_CMD_QUEUE_SIZE)
            my_out_idx = 0;
    }

    s->mac_cmd_queue[s->mac_cmd_queue_in_idx].len = cmd_len;
    s->mac_cmd_queue[s->mac_cmd_queue_in_idx].needs_answer = needs_answer;
    memcpy(s->mac_cmd_queue[s->mac_cmd_queue_in_idx].buf, cmd_buf, cmd_len);
    to_mote_mac_cmd_to_string(s->mac_cmd_queue[s->mac_cmd_queue_in_idx].buf[0], str);
    MAC_PRINTF(" put-mac-cmd %s at %d: ", str, s->mac_cmd_queue_in_idx);
    for (uint8_t i = 0; i < cmd_len; i++)
        MAC_PRINTF("%02x ", cmd_buf[i]);

    if (++s->mac_cmd_queue_in_idx == MAC_CMD_QUEUE_SIZE)
        s->mac_cmd_queue_in_idx = 0;

    MAC_PRINTF("\n");
} // ..put_queue_mac_cmds()

int
sNS_force_rejoin(mote_t* mote, uint8_t type)
{
    uint16_t cmd_payload;
    uint8_t cmd_buf[MAC_CMD_SIZE];
    s_t* s = mote->s;
    uint8_t max_retries = 3;
    uint8_t period = 1;
    ServiceProfile_t sp;
    int ret = readServiceProfileFromSql(sqlConn_lora_network, mote->devEui, mote->devAddr, &sp);

    if (!s || ret < 0)
        return ret;

    cmd_payload = sp.DRMin & 0x0f;
    cmd_payload |= (max_retries & 7) << 8;
    cmd_payload |= (type & 7) << 4;
    cmd_payload |= (period & 7) << 11;

    cmd_buf[0] = SRV_MAC_FORCE_REJOIN_REQ;
    cmd_buf[1] = cmd_payload & 0xff;
    cmd_buf[2] = (cmd_payload << 8) & 0xff;
    printf("sNS_force_rejoin  put-SRV_MAC_FORCE_REJOIN_REQ ");

    wipe_queued_mac_cmds(s);
    put_queue_mac_cmds(s, 3, cmd_buf, false);

    if (type == 2)
        s->type2_rejoin_count = max_retries;

    /* TODO: classB classC send now */
    return 0;
}

/** @brief remove matching mac command from list of mac commands to send
 * typically called when answer mac command received
 * @param mote end-node
 * @param cmd command to remove
 */
void
clear_queued_mac_cmd(s_t* s, uint8_t cmd)
{
    char str[64];
    uint8_t my_out_idx = s->mac_cmd_queue_out_idx;
    while (s->mac_cmd_queue_in_idx != my_out_idx) {

        if (s->mac_cmd_queue[my_out_idx].len != 0 && s->mac_cmd_queue[my_out_idx].buf[0] == cmd)
        {
            to_mote_mac_cmd_to_string(s->mac_cmd_queue[my_out_idx].buf[0], str);
            MAC_PRINTF("clearing mac cmd %s at %d\n", str, my_out_idx);
            s->mac_cmd_queue[my_out_idx].len = 0;
            s->mac_cmd_queue[my_out_idx].buf[0] = 0;
            return;
        }

        if (++my_out_idx == MAC_CMD_QUEUE_SIZE)
            my_out_idx = 0;
    } // ..while mac commands that were sent

    printf("\e[31mmac_cmd not cleared:%02x\e[0m\n", cmd);
}

/** @brief calculate ping offset of mote
 * @param beaconTime [in] seconds sent in beacon payload
 * @param address [in] device address of mote
 * @param pingPeriod [in] periodicity of mote
 * @param pingOffset [out] calculated value
 */
void LoRaMacBeaconComputePingOffset( uint64_t beaconTime, uint32_t address, uint16_t pingPeriod, uint16_t *pingOffset )
{
    aes_context AesContext;
    uint8_t zeroKey[16];
    uint8_t buffer[16];
    uint8_t cipher[16];
    uint32_t result = 0;
    /* Refer to chapter 15.2 of the LoRaWAN specification v1.1. The beacon time
     * GPS time in seconds modulo 2^32
     */
    uint32_t time = ( beaconTime % ( ( ( uint64_t ) 1 ) << 32 ) );

    memset( zeroKey, 0, 16 );
    memset( buffer, 0, 16 );
    memset( cipher, 0, 16 );
    memset( AesContext.ksch, '\0', 240 );

    buffer[0] = ( time ) & 0xFF;
    buffer[1] = ( time >> 8 ) & 0xFF;
    buffer[2] = ( time >> 16 ) & 0xFF;
    buffer[3] = ( time >> 24 ) & 0xFF;

    buffer[4] = ( address ) & 0xFF;
    buffer[5] = ( address >> 8 ) & 0xFF;
    buffer[6] = ( address >> 16 ) & 0xFF;
    buffer[7] = ( address >> 24 ) & 0xFF;

    aes_set_key( zeroKey, 16, &AesContext );
    aes_encrypt( buffer, cipher, &AesContext );

    result = ( ( ( uint32_t ) cipher[0] ) + ( ( ( uint32_t ) cipher[1] ) * 256 ) );

    *pingOffset = ( uint16_t )( result % pingPeriod );
}


#define DEMOD_FLOOR     (-15)

static bool
parse_mac_command(mote_t* mote, const ULMetaData_t* ulmd, const uint8_t* rx_cmd_buf, uint8_t rx_cmd_buf_len)
{
    char str[64];
    uint8_t cmd_buf[MAC_CMD_SIZE];
    uint8_t rx_cmd_buf_idx = 0;
    int i;
    bool ret = false;
    s_t* s = mote->s;
    ULToken_t ult;
    const char* inptr = mote->bestULTokenStr;
    MAC_PRINTF("rx_mac_command(s):");

    if (inptr) {
        for (i = 0; i < sizeof(ult.octets); i++) {
            unsigned oct;
            sscanf(inptr, "%02x", &oct);
            ult.octets[i] = oct;
            inptr += 2;
        }
    } // else wont work: beacon timing request, device time request
    else
        printf("\e[31m parse_mac_cmd no bestULToken\e[0m ");


    /*for (i = 0; i < rx_cmd_buf_len; i++)
        MAC_PRINTF("%02x ", rx_cmd_buf[i]);
    MAC_PRINTF("\n");*/

    while (rx_cmd_buf_idx < rx_cmd_buf_len) {
        float diff;
        uint16_t i_diff;

        from_mote_mac_cmd_to_string(rx_cmd_buf[rx_cmd_buf_idx], str);
        printf(" %s ", str);

        switch (rx_cmd_buf[rx_cmd_buf_idx]) {
            case MOTE_MAC_LINK_CHECK_REQ:   // 0x02
                rx_cmd_buf_idx++;
                printf("MOTE_MAC_LINK_CHECK_REQ\n");
                cmd_buf[0] = SRV_MAC_LINK_CHECK_ANS;
                if (mote->rx_snr < DEMOD_FLOOR)
                    cmd_buf[1] = 0;
                else
                    cmd_buf[1] = mote->rx_snr - DEMOD_FLOOR;

                cmd_buf[2] = ulmd->GWCnt;
                put_queue_mac_cmds(s, 3, cmd_buf, false);
                break;
            case MOTE_MAC_PING_SLOT_INFO_REQ:   // 0x10
                rx_cmd_buf_idx++;

                s->ClassB.ping_slot_info.octet = rx_cmd_buf[rx_cmd_buf_idx++];
                uint32_t ping_nb = 128 / (1 << s->ClassB.ping_slot_info.bits.periodicity);
                s->ClassB.ping_period = BEACON_WINDOW_SLOTS / ping_nb;
                printf("MOTE_MAC_PING_SLOT_INFO_REQ ping_nb:%d, ping_period:%d\n", ping_nb, s->ClassB.ping_period);
                LoRaMacBeaconComputePingOffset(ult.gateway.seconds_at_beacon, mote->devAddr, s->ClassB.ping_period, &s->ClassB.ping_offset);
                cmd_buf[0] = SRV_MAC_PING_SLOT_INFO_ANS;    // 0x10
                put_queue_mac_cmds(s, 1, cmd_buf, false);
                break;
            case MOTE_MAC_BEACON_TIMING_REQ:    // 0x12
                rx_cmd_buf_idx++;
                /* no payload in request */
                diff = (float)(ult.gateway.lgw_trigcnt_at_next_beacon - ult.gateway.count_us) / 30000.0;
                i_diff = (int)floor(diff);
                printf("MOTE_MAC_BEACON_TIMING_REQ slots:%.1f=%.1fms (int:%u,%u)", diff, diff*30.0, i_diff, i_diff*30);
                cmd_buf[0] = SRV_MAC_BEACON_TIMING_ANS;   // 0x12
                cmd_buf[1] = i_diff & 0xff; //lsbyte first byte
                cmd_buf[2] = (i_diff >> 8) & 0xff;

                if (strncmp(ulmd->RFRegion, US902, 5) == 0) {
                    cmd_buf[3] = (ult.gateway.beacon_ch + 1) & 7;   // beacon channel index
                    MAC_PRINTF(" ch%d ", cmd_buf[3]);
                } else
                    cmd_buf[3] = 0;   // beacon channel index

                put_queue_mac_cmds(s, 4, cmd_buf, false);
                printf("%02x %02x %02x\n", cmd_buf[1], cmd_buf[2], cmd_buf[3]);
                break;
            case MOTE_MAC_DEVICE_TIME_REQ:
                rx_cmd_buf_idx++;
                /* no payload in request */
                printf("MOTE_MAC_DEVICE_TIME_REQ ");
                {
                    /* GWInfo[0].ULToken is rx_pkt.count_us */
                    uint32_t us_since_beacon = ult.gateway.count_us - ult.gateway.tstamp_at_beacon;
                    uint32_t secs = us_since_beacon / 1000000;
                    uint32_t subusecs = us_since_beacon % 1000000;
                    printf("us_since:%u, secs_since:%u, subusecs:%u\n", us_since_beacon, secs, subusecs);
                    secs += ult.gateway.seconds_at_beacon;
                    printf("-> secs:%u (%u)", secs, ult.gateway.seconds_at_beacon);
                    cmd_buf[0] = SRV_MAC_DEVICE_TIME_ANS;
                    cmd_buf[1] = secs ; //lsbyte first
                    cmd_buf[2] = secs >> 8;
                    cmd_buf[3] = secs >> 16;
                    cmd_buf[4] = secs >> 24;
                    cmd_buf[5] = subusecs / 3906.5;
                    put_queue_mac_cmds(s, 6, cmd_buf, false);

                    diff = (float)(ult.gateway.lgw_trigcnt_at_next_beacon - ult.gateway.count_us) / 30000.0;
                    printf("slots:%.1f (%uus between beacons)\n", diff, ult.gateway.lgw_trigcnt_at_next_beacon - ult.gateway.tstamp_at_beacon);
                }
                break;
            case MOTE_MAC_LINK_ADR_ANS:
                rx_cmd_buf_idx++;
                i = rx_cmd_buf[rx_cmd_buf_idx++];
                printf("LINK_ADR_ANS status:0x%02x\n", i);
                clear_queued_mac_cmd(s, SRV_MAC_LINK_ADR_REQ);
                break;
            case MOTE_MAC_PING_SLOT_FREQ_ANS:
                rx_cmd_buf_idx++;
                i = rx_cmd_buf[rx_cmd_buf_idx++];
                printf("PING_SLOT_FREQ_ANS status:0x%02x\n", i);
                clear_queued_mac_cmd(s, SRV_MAC_PING_SLOT_CHANNEL_REQ);
                break;
            case MOTE_MAC_BEACON_FREQ_ANS:
                rx_cmd_buf_idx++;
                i = rx_cmd_buf[rx_cmd_buf_idx++];
                printf("BEACON_FREQ_ANS status:0x%02x\n", i);
                clear_queued_mac_cmd(s, SRV_MAC_BEACON_FREQ_REQ);
                break;
            case MOTE_MAC_RX_PARAM_SETUP_ANS:
                rx_cmd_buf_idx++;
                i = rx_cmd_buf[rx_cmd_buf_idx++];
                printf("RX_PARAM_SETUP_ANS status:0x%02x\n", i);
                clear_queued_mac_cmd(s, SRV_MAC_RX_PARAM_SETUP_REQ);
                break;
            case MOTE_MAC_NEW_CHANNEL_ANS:
                rx_cmd_buf_idx++;
                i = rx_cmd_buf[rx_cmd_buf_idx++];
                printf("NEW_CHANNEL_ANS status:0x%02x\n", i);
                clear_queued_mac_cmd(s, SRV_MAC_NEW_CHANNEL_REQ);
                break;
            case MOTE_MAC_DEV_STATUS_ANS:
                rx_cmd_buf_idx++;
                i = rx_cmd_buf[rx_cmd_buf_idx++];
                printf("DEV_STATUS_ANS bat%u ", i);
                i = rx_cmd_buf[rx_cmd_buf_idx++];
                printf("margin%u\n", i);
                break;
            case MOTE_MAC_REKEY_IND:
                rx_cmd_buf_idx++;
                /* TODO: the network server shall discard any uplink frames protected with the new security context that are received after the transmission of JoinAccept and before the first uplink frame that carries a RekeyInd command */
                i = rx_cmd_buf[rx_cmd_buf_idx++];   // mote minor version

                printf("SRV_MAC_REKEY_CONF %u\n", i);
                cmd_buf[0] = SRV_MAC_REKEY_CONF;
                cmd_buf[1] = i;
                if (cmd_buf[1] == 1) {
                    if (mote->devEui == NONE_DEVEUI)
                        printf("\e[31mABP-rekey\e[0m ");
                    else {
                        if (s->checkOldSessions) {
                            deleteOldSessions(sqlConn_lora_network, mote->devEui, false);
                            s->checkOldSessions = false;
                        } else
                            printf("\e[31mrekey-only-after-join-accept\e[0m ");
                    }
                } else
                    printf("\e[31mmac version %u\e[0m ", cmd_buf[1]);

                put_queue_mac_cmds(s, 2, cmd_buf, false);
                break;
            case MOTE_MAC_RESET_IND:
                rx_cmd_buf_idx++;
                i = rx_cmd_buf[rx_cmd_buf_idx++];
                printf("MOTE_MAC_RESET_IND %u ", i);
                /*if (i != mote->OptNeg)
                    MAC_PRINTF("\e[31mMOTE_MAC_RESET_IND %u\e[0m ", i);
                else
                    MAC_PRINTF("MOTE_MAC_RESET_IND %u ", i);*/
                {
                    sql_t sql;
                    sql_motes_query(sqlConn_lora_network, mote->devEui, mote->devAddr, &sql);
                    cmd_buf[0] = SRV_MAC_RESET_CONF;
                    cmd_buf[1] = sql.OptNeg;
                    put_queue_mac_cmds(s, 2, cmd_buf, false);
                }

                s->session_start_at_uplink = true;
                network_controller_mote_init(s, ulmd->RFRegion, "mac-cmd");
                break;
            case MOTE_MAC_DEVICE_MODE_IND:
                rx_cmd_buf_idx++;
                s->classModeInd = rx_cmd_buf[rx_cmd_buf_idx++];
                printf("MOTE_MAC_DEVICE_MODE_IND %u ", s->classModeInd);
                cmd_buf[0] = SRV_MAC_DEVICE_MODE_CONF;
                cmd_buf[1] = s->classModeInd;
                put_queue_mac_cmds(s, 2, cmd_buf, false);
                break;
            case MOTE_MAC_REJOIN_PARAM_ANS:
                rx_cmd_buf_idx++;
                i = rx_cmd_buf[rx_cmd_buf_idx++];
                printf("REJOIN_PARAM_ANS %u ", i);
                clear_queued_mac_cmd(s, SRV_MAC_REJOIN_PARAM_REQ);
                break;
            case MOTE_MAC_TX_PARAM_SETUP_ANS:
                rx_cmd_buf_idx++;
                clear_queued_mac_cmd(s, SRV_MAC_TX_PARAM_SETUP_REQ);
                break;
            default:
                printf("\e[31m%02x\e[0m ", rx_cmd_buf[rx_cmd_buf_idx]);
                rx_cmd_buf_idx++;
                break;
        } // ..switch (<mac_command>)
    } // .. while have mac comannds

    if (s->send_start_mac_cmds) {
        struct _region_list* rl;
        for (rl = region_list; rl != NULL; rl = rl->next) {
            if (rl->region.RFRegion == ulmd->RFRegion && rl->region.regional.parse_start_mac_cmd != NULL) {
                rl->region.regional.parse_start_mac_cmd(rx_cmd_buf, rx_cmd_buf_len, mote);
                return 0;
            }
        }
    }

    return ret;
} // ..parse_mac_command()

static bool
are_mac_cmds_queued(const s_t* s)
{
    uint8_t my_out_idx = s->mac_cmd_queue_out_idx;

    while (s->mac_cmd_queue_in_idx != my_out_idx) {
        if (s->mac_cmd_queue[my_out_idx].len > 0)
            return true;

        if (++my_out_idx == MAC_CMD_QUEUE_SIZE)
            my_out_idx = 0;
    } // ..while mac commands to be sent

    return false;
}

/** @brief get mac commands from mote struct and put into tx packet
 * @return true if mac commands encrypted, false for mac commands in header
 */
static int get_queued_mac_cmds(mote_t* mote, const uint8_t* NwkSEncKey, uint32_t FCnt, bool OptNeg)
{
    char str[64];
    s_t* s = mote->s;
    fhdr_t* tx_fhdr = (fhdr_t*)&s->downlink.PHYPayloadBin[1];
    uint8_t* tx_fopts_ptr = &s->downlink.PHYPayloadBin[sizeof(mhdr_t) + sizeof(fhdr_t)];
    uint8_t buf[16];
    uint8_t buf_idx = 0;
    int ret = 0;
    uint8_t my_out_idx = s->mac_cmd_queue_out_idx;

    tx_fhdr->FCtrl.octet = 0;
    tx_fhdr->FCtrl.dlBits.FOptsLen = 0;
    memset(buf, 0, sizeof(buf));

    MAC_PRINTF("get_queued_mac_cmds() in%u out%u ", s->mac_cmd_queue_in_idx, my_out_idx);
    while (s->mac_cmd_queue_in_idx != my_out_idx) {
        MAC_PRINTF("[%d].len=%d ", my_out_idx, s->mac_cmd_queue[my_out_idx].len);
        if (s->mac_cmd_queue[my_out_idx].len != 0) {
            int n;
            if ((buf_idx + s->mac_cmd_queue[my_out_idx].len) > 15)
                break;  // no more room

            to_mote_mac_cmd_to_string(s->mac_cmd_queue[my_out_idx].buf[0], str);
            MAC_PRINTF("sending mac cmd %s at %d, len%u: ", str, my_out_idx, s->mac_cmd_queue[my_out_idx].len);
            for (n = 0; n < s->mac_cmd_queue[my_out_idx].len; n++) {
                MAC_PRINTF("%02x ", s->mac_cmd_queue[my_out_idx].buf[n]);
                buf[buf_idx++] = s->mac_cmd_queue[my_out_idx].buf[n];
            }

            if (!s->mac_cmd_queue[my_out_idx].needs_answer) {
                /* no reply needed from mote, clear now */
                s->mac_cmd_queue[my_out_idx].len = 0;
                s->mac_cmd_queue[my_out_idx].buf[0] = 0;
                MAC_PRINTF("clearing");
            } else
                ret = 1;    // return value indicating we need answer
        } // ..if mac command present in this slot

        if (++my_out_idx == MAC_CMD_QUEUE_SIZE)
            my_out_idx = 0;

        MAC_PRINTF(" ->%u\n", buf_idx);
    } // ..while mac commands to be sent

    if (buf_idx == 0)
        return 0;

    tx_fhdr->FCtrl.dlBits.FOptsLen = buf_idx;
    //printf(" (get_queued_mac_cmds put %u) ", buf_idx);

    MAC_PRINTF("get_queued_mac_cmds() ");
    if (OptNeg) {
        DEBUG_CRYPT_BUF(NwkSEncKey, 16, "NwkSEncKey");
        DEBUG_CRYPT_BUF(buf, buf_idx, "FOpts-preEnc");
        DEBUG_CRYPT("\e[35mFOpts-Encr[0m ", FCnt);
        printf("mac-encr-NFCntDown%u ", FCnt);
        LoRa_Encrypt(0, NwkSEncKey, buf, buf_idx, mote->devAddr, false, FCnt, tx_fopts_ptr);
    } else {
        DEBUG_CRYPT("FOpts-unEncr\n");
        for (int n = 0; n < buf_idx; n++)
            *tx_fopts_ptr++ = buf[n];
    }

    return ret;
} // .._get_queued_mac_cmds()

static int
incr_sql_NFCntDown(uint32_t devAdr)
{
    char query[128];

    /* only newest session gets frame counter update */
    MAC_PRINTF("\e[35mincr-NFCntDown\e[0m ");
    sprintf(query, "UPDATE sessions SET NFCntDown = NFCntDown + 1 WHERE DevAddr = %u ORDER BY createdAt DESC LIMIT 1", devAdr);
    if (mq_send(mqd, query, strlen(query)+1, 0) < 0)
        perror("incr_sql_NFCntDown mq_send");

    return 0;
}

/* return true for downlink needed to be sent */
static void 
sNS_schedule_downlink(mote_t* mote)
{
    mhdr_t* tx_mhdr;
    fhdr_t* tx_fhdr;
    bool uplink_forces_downlink = false;
    const mhdr_t* rx_mhdr = (mhdr_t*)&mote->ULPayloadBin[0];
    const fhdr_t* rx_fhdr = (fhdr_t*)&mote->ULPayloadBin[1];
    bool using_AFCntDwn;
    uint32_t FCntDown;
    int needAnswer;
    s_t* s = mote->s;
    bool has_rx_fhdr = rx_mhdr->bits.MType == MTYPE_CONF_UP || rx_mhdr->bits.MType == MTYPE_UNCONF_UP;

    MAC_PRINTF("sNS_schedule_downlink() ");
    if (s->downlink.PHYPayloadLen > 0) {
        if (!(mote->session.OptNeg && rx_mhdr->bits.MType == MTYPE_CONF_UP)) {
            /* 1v1: if last uplink was CONF, we need to send down an ack */
            printf("\e[31mpreviously-unsent\e[0m\n");
            return;
        } else {
            printf("\e[31mp1v1 previously-unsent, dropping\e[0m\n");
        }
    }
    /* Conditions for transmit, any one of:
     * 1) mac commands waiting to be sent : from mac_cmds_waiting 
     * 2) uplink rx confirmed MType : from txAck
     * 3) uplink rx ADRACkReq set : from rxADRACKReq
     * 4) mote->downlink.length > 0   or   mote->downlink.send set
     * 5) if last downlink was confirmed type, but uplink didnt have ACK bit set
     * */
    if (has_rx_fhdr) {
        if (rx_fhdr->FCtrl.ulBits.ADRACKReq || (!rx_fhdr->FCtrl.ulBits.ACK && s->confDLPHYPayloadLen > 0))
            uplink_forces_downlink = true;
    } else /* perhaps uplink was RejoinReq */ if (s->confDLPHYPayloadLen > 0)
        uplink_forces_downlink = true;


    if (rx_mhdr->bits.MType != MTYPE_CONF_UP && !uplink_forces_downlink && !are_mac_cmds_queued(s) && s->downlink.FRMPayloadLen == 0) {
        MAC_PRINTF("no downlink to send\n");
        return;  // no downlink necessary
    }

    /* a downlink needs to be sent */
    needAnswer = get_queued_mac_cmds(mote, mote->session.NwkSEncKeyBin, mote->session.NFCntDown, mote->session.OptNeg);
    if (needAnswer < 0) {
        printf("\e[31mschedule_downlink fail get_queued_mac_cmds\e0m");
        return;
    }

    tx_mhdr = (mhdr_t*)&s->downlink.PHYPayloadBin[0];
    tx_fhdr = (fhdr_t*)&s->downlink.PHYPayloadBin[1];
    using_AFCntDwn = false;    // default to using NFCntDown

    s->downlink.FRMSent = false;
    printf(" \e[5mfrm%u->down\e[0m ", s->downlink.FRMPayloadLen);
    if (s->downlink.FRMPayloadLen > 0) {
        /* add user payload */
        bool fail = false;
        int txo = sizeof(mhdr_t) + sizeof(fhdr_t) + tx_fhdr->FCtrl.dlBits.FOptsLen;
        uint8_t* tx_fport_ptr = &s->downlink.PHYPayloadBin[txo];
        uint8_t* txFRMPayload = &s->downlink.PHYPayloadBin[txo+1];

        if (mote->session.OptNeg) {
            using_AFCntDwn = true;
            s->AFCntDown = s->downlink.md.FCntDown;
        } else if (s->downlink.md.FCntDown != mote->session.NFCntDown) {
            printf("\e[31mFCntDown for FRMPayload wrong fromAS:%u NFCntDown:%u\e[0m\n", s->downlink.md.FCntDown, mote->session.NFCntDown);
            fail = true;
        }

        if (!fail) {
            printf(" down-fport%u ", s->downlink.md.FPort);
            *tx_fport_ptr = s->downlink.md.FPort;
            memcpy(txFRMPayload, s->downlink.FRMPayloadBin, s->downlink.FRMPayloadLen);   // buffer was already encrypted
            s->downlink.PHYPayloadLen = s->downlink.FRMPayloadLen + 1; // +1 for fport, FOpts length added in send_downlink()
            s->downlink.FRMSent = true;
            if (/*needAnswer ||*/ s->downlink.md.Confirmed) {
                MAC_PRINTF("conf ");
                tx_mhdr->bits.MType = MTYPE_CONF_DN;
            } else {
                MAC_PRINTF("unconf ");
                tx_mhdr->bits.MType = MTYPE_UNCONF_DN;
            }
        }
    } else {
#ifdef MAC_DEBUG
        uint8_t* tx_fopts_ptr = &s->downlink.PHYPayloadBin[sizeof(mhdr_t) + sizeof(fhdr_t)];
#endif
        /* downlink not triggered by user_downlink */
        /* downlink triggered by FOptsLen > 0 or conf_uplink */
        MAC_PRINTF("no-frm FOptsLen%u: ", tx_fhdr->FCtrl.dlBits.FOptsLen);
        DEBUG_MAC_BUF(tx_fopts_ptr, tx_fhdr->FCtrl.dlBits.FOptsLen, "FOpts");
        if (needAnswer)
            tx_mhdr->bits.MType = MTYPE_CONF_DN;
        else
            tx_mhdr->bits.MType = MTYPE_UNCONF_DN;
    }

    MAC_PRINTF("downlink-");
#ifdef MAC_DEBUG
    if (tx_mhdr->bits.MType == MTYPE_CONF_DN)
        printf("Conf-");
    else if (tx_mhdr->bits.MType == MTYPE_CONF_DN)
        printf("Unconf-");
#endif
    if (using_AFCntDwn) {
        FCntDown = s->AFCntDown;
        MAC_PRINTF("\e[35mAFCntDwn%u incrNFCntDown = false \e[0m-", FCntDown);
        s->incrNFCntDown = false;
    } else {
        FCntDown = mote->session.NFCntDown;
        MAC_PRINTF("\e[35mNFCntDwn%u incrNFCntDown = true \e[0m-", FCntDown);
        s->incrNFCntDown = true;
    }
    tx_fhdr->FCnt = FCntDown;
    tx_fhdr->DevAddr = mote->devAddr;
    MAC_PRINTF(" tx_fhdr->DevAddr:%08x ", tx_fhdr->DevAddr);

    printElapsed(mote);
    printf(" sched-down-phyLen%u", s->downlink.PHYPayloadLen);
    s->downlink.PHYPayloadLen += LORA_MACHEADERLENGTH + sizeof(fhdr_t) + tx_fhdr->FCtrl.dlBits.FOptsLen;
    printf("->%u ", s->downlink.PHYPayloadLen);


    if (rx_mhdr->bits.MType == MTYPE_CONF_UP)
        tx_fhdr->FCtrl.dlBits.ACK = 1;
    else
        tx_fhdr->FCtrl.dlBits.ACK = 0;

    {
        block_t block;
        uint32_t* mic_ptr = (uint32_t*)&s->downlink.PHYPayloadBin[s->downlink.PHYPayloadLen];
        block.b.header = 0x49;
        block.b.dr = 0;
        block.b.ch = 0;
        block.b.dir = DIR_DOWN;
        block.b.DevAddr = mote->devAddr;
        block.b.FCnt = FCntDown;
        block.b.zero8 = 0;
        block.b.lenMsg = s->downlink.PHYPayloadLen;
        if (has_rx_fhdr && s->newFCntUp && mote->session.OptNeg && tx_fhdr->FCtrl.dlBits.ACK) {
            block.b.confFCnt = rx_fhdr->FCnt;
            s->newFCntUp = false;   // single use
        } else
            block.b.confFCnt = 0;
        MAC_PRINTF("\e[36mdownmic-ConFFcntUp%u\e[0m ", block.b.confFCnt);
        *mic_ptr = LoRa_GenerateDataFrameIntegrityCode(&block, mote->session.SNwkSIntKeyBin, s->downlink.PHYPayloadBin);
    }
    s->downlink.PHYPayloadLen += LORA_FRAMEMICBYTES;
    printf("s->downlink.PHYPayloadLen:%u ", s->downlink.PHYPayloadLen);

    if (tx_mhdr->bits.MType == MTYPE_UNCONF_DN) {
        printf("MTYPE_UNCONF_DN ");
        s->ConfFCntDown = 0;
        s->confDLPHYPayloadLen = 0;
        printf("\e[36;5mzero sentFCnt%u\e[0m ", s->ConfFCntDown);
    } else if (tx_mhdr->bits.MType == MTYPE_CONF_DN) {
        printf("MTYPE_CONF_DN ");
        s->ConfFCntDown = tx_fhdr->FCnt; // 1v1: for checking future uplink MIC
        memcpy(s->confDLPHYPayloadBin, s->downlink.PHYPayloadBin, s->downlink.PHYPayloadLen);
        s->confDLPHYPayloadLen = s->downlink.PHYPayloadLen;
        printf("\e[36;5msetting ConfFCntDown%u phylen%u\e[0m ", s->ConfFCntDown, s->confDLPHYPayloadLen);
    }

} // ..sNS_schedule_downlink()

static void
sNS_uplink_frm_answer(MYSQL* sc, mote_t* mote, json_object* jobj, const char* rxResult, const char* senderID, const uint8_t* rfBuf, uint8_t rfLen)
{
    /* only occurs in sHANDOVER, answer would go to local fNS */
    if (rxResult != Success)
        printf("\e[31msNS_uplink_frm_answer() %s from %s rfLen%u\e[0m\n", rxResult, senderID, rfLen);
}

static void
sNS_downlink_phy_answer(MYSQL* sc, mote_t* mote, json_object* jobj, const char* rxResult, const char* senderID, const uint8_t* rfBuf, uint8_t rfLen)
{
    if (rxResult != Success || rfLen > 0)
        printf("\e[31msTODO sNS_downlink_phy_answer() %s from %s rfLen%u\e[0m\n", rxResult, senderID, rfLen);
}

static int
sNS_XmitDataAns_tohNS(s_t* s, const char* result, uint32_t roamingWithNetID)
{
    int ret = -1;
    char destIDstr[12];
    char hostname[64];
    json_object* ansJobj = json_object_new_object();
    CURL* easy;
        
    if (s->DLFreq1 > 0)
        json_object_object_add(ansJobj, DLFreq1, json_object_new_double(s->DLFreq1));
    if (s->DLFreq2 > 0)
        json_object_object_add(ansJobj, DLFreq2, json_object_new_double(s->DLFreq2));

    sprintf(destIDstr, "%x", roamingWithNetID);
    lib_generate_json(ansJobj, destIDstr, myNetwork_idStr, s->downlink.reqTid, XmitDataAns, result);

    easy = curl_easy_init();
    if (easy) {
        curl_multi_add_handle(multi_handle, easy);
        sprintf(hostname, "%06x.%s", roamingWithNetID, netIdDomain);
        ret = http_post_hostname(easy, ansJobj, hostname, false);
    }

    return ret;
} // ..sNS_XmitDataAns_tohNS()

void
answer_app_downlink(mote_t* mote, const char* result, json_object** httpdAnsJobj)
{
    sql_t sql;
    s_t* s = mote->s;

    if (!s->downlink.FRMSent)
        printf("answer_app_downlink(): FRM not sent\n");

    sql_motes_query(sqlConn_lora_network, mote->devEui, mote->devAddr, &sql);

    printf("%s ", sql.roamState);
    printf("s->downlink.reqTid%lu ", s->downlink.reqTid);
    if (sql.roamState == roamsHANDOVER) {
        if (sNS_XmitDataAns_tohNS(s, result, sql.roamingWithNetID) < 0)
            printf("\e[31msNS_XmitDataAns_tohNS() failed\e[0m\n");
        else {
            s->downlink.FRMPayloadLen = 0;
            printf(" \e[5mclear-frm-down\e[0m ");
        }
    } else {
        json_object* ansJobj = json_object_new_object();
        if (s->DLFreq1 > 0)
            json_object_object_add(ansJobj, DLFreq1, json_object_new_double(s->DLFreq1));
        if (s->DLFreq2 > 0)
            json_object_object_add(ansJobj, DLFreq2, json_object_new_double(s->DLFreq2));

        printf("answer_app_downlink ");
        if (httpdAnsJobj == NULL) {
            if (sendXmitDataAns(s->downlink.isAnsDestAS, ansJobj, s->downlink.ansDest_, s->downlink.reqTid, result) < 0)
                printf("\e[31msentXmitDataAns() failed\e[0m\n");
            else {
                s->downlink.FRMPayloadLen = 0;
                printf(" \e[5mclear-frm-down\e[0m ");
            }
        } else
            *httpdAnsJobj = ansJobj;    // sending as answer to post instead of posting answer
    }
} // ..answer_app_downlink()

static bool
check_cmacS(const block_t* block, const uint8_t* sNwkSIntKey, const uint8_t* phyPayload)
{
    uint16_t cmacS;
    uint8_t temp[LORA_AUTHENTICATIONBLOCKBYTES];
    AES_CMAC_CTX cmacctx;
    uint16_t rx_mic;

    if (!sNwkSIntKey) {
        printf("\e[31mcheck_cmacS no SNwkSIntKey\e[0m\n");
        return false;
    }

    rx_mic = phyPayload[block->b.lenMsg+1] << 8;
    rx_mic += phyPayload[block->b.lenMsg];

    DEBUG_MIC_BUF_UP(block->octets, LORA_AUTHENTICATIONBLOCKBYTES, "up-b1");
    DEBUG_MIC_BUF_UP(phyPayload, block->b.lenMsg, "payload");

    AES_CMAC_Init(&cmacctx);
    AES_CMAC_SetKey(&cmacctx, sNwkSIntKey);
    DEBUG_MIC_BUF_UP(sNwkSIntKey, 16, "b1-key");

    AES_CMAC_Update(&cmacctx, block->octets, LORA_AUTHENTICATIONBLOCKBYTES);
    AES_CMAC_Update(&cmacctx, phyPayload, block->b.lenMsg);

    AES_CMAC_Final(temp, &cmacctx);

    memcpy(&cmacS, temp, 2);
    DEBUG_MIC_UP("ConfFCnt%u dr%u ch%u cmacS:%04x vs %04x\n", block->b.confFCnt, block->b.dr, block->b.ch, cmacS, rx_mic);
    return cmacS == rx_mic;
}

static int
get_ch(float MHz, uint8_t DataRate, const char* RFRegion)
{
    struct _region_list* rl;

    for (rl = region_list; rl != NULL; rl = rl->next) {
        if (rl->region.RFRegion == RFRegion && rl->region.regional.get_ch != NULL) {
            return rl->region.regional.get_ch(MHz, DataRate);
        }
    }

    return -1;
}

/* MTYPE_UNCONF_UP || MTYPE_CONF_UP */
/* return true for downlink needs to be sent */
const char *
sNS_uplink(mote_t* mote, const sql_t* sql, ULMetaData_t* ulmd, bool* discard, char* reason)
{
    const char* ret = Success;
    const uint8_t* ulFRMPayloadBin = NULL;
    uint8_t ulFRMPayloadLen = 0;
    uint8_t decrypted[256];
    const uint8_t* rx_fport_ptr = NULL;
    int rxofs, A, B;
    fhdr_t *rx_fhdr;
    s_t* s;
    mhdr_t* mhdr = (mhdr_t*)mote->ULPayloadBin;

    strcpy(reason, "none");
    printElapsed(mote);
    /*MAC_PRINTF("sNS_uplink() ULT%s ", mote->bestULTokenStr);
    fflush(stdout);*/
    MAC_PRINTF("sNS_uplink()  ");

    if (!mote->s) {
        mote->s = calloc(1, sizeof(s_t));
        network_controller_mote_init(mote->s, ulmd->RFRegion, "sNS_uplink");
    }

    s = mote->s;

    printf(" session:");
    if (mote->session.until != ULONG_MAX) {    // if OTA mote
        if (mote->session.until == 0) {
            printf("none\n");
            strcpy(reason, "no-session");
            return Other;
        }

        if (mote->session.expired) {
            printf("expired ");

            if (sql->OptNeg) {
                if (sNS_force_rejoin(mote, 2) < 0)
                    printf("\e[31msNS_force_rejoin failed\e[0m\n");
            } else {
                /* 1v0 end device is booted off */
                int n = deleteOldSessions(sqlConn_lora_network, mote->devEui, true);

                if (s->downlink.FRMPayloadLen > 0)
                    answer_app_downlink(mote, XmitFailed, NULL);

                printf("%d = deleteOldSessions() ", n);
                if (n > 0) {
                    strcpy(reason, "deleteFail");
                    return Other;
                }
            }
        } else
            printf("%lusecs-left ", mote->session.until - time(NULL));
    } else
        printf("forever ");


    printf(" \e[5m%p FRM%u->down\e[0m ", s, s->downlink.FRMPayloadLen);

    rx_fhdr = (fhdr_t*)&mote->ULPayloadBin[1];
    rxofs = sizeof(mhdr_t) + sizeof(fhdr_t) + rx_fhdr->FCtrl.ulBits.FOptsLen;

    MAC_PRINTF("\e[44;37mup-mote-FCnt:0x%04x sql:0x%08x\e[0m ", rx_fhdr->FCnt, mote->session.FCntUp);
    ulmd->FCntUp = (mote->session.FCntUp & 0xffff0000) | rx_fhdr->FCnt;
    A = ulmd->FCntUp - mote->session.FCntUp;
    B = (ulmd->FCntUp + 0x10000) - mote->session.FCntUp;
    if (A < 0 && B > MAX_FCNT_GAP) {
        MAC_PRINTF("\e[31mreplay\e[0m\n");
        strcpy(reason, "replay");
        return Other;
    }
    if (A > MAX_FCNT_GAP && B > MAX_FCNT_GAP) {
        MAC_PRINTF("\e[31mreplay\e[0m\n");
        strcpy(reason, "replay");
        return Other;
    }

    s->newFCntUp = true;
    if (sql->OptNeg) {
        int ch;
        block_t block;
        bool ok;
        ch = get_ch(ulmd->ULFreq, ulmd->DataRate, ulmd->RFRegion);
        if (ch < 0) {
            printf("\e[31msNS couldnt get channel:%f, dr%u, %s\e[0m\n", ulmd->ULFreq, ulmd->DataRate, ulmd->RFRegion);
            return MICFailed;
        }
        block.b.header = 0x49;
        block.b.confFCnt = s->ConfFCntDown;
        block.b.dr = ulmd->DataRate;
        block.b.ch = ch;
        block.b.dir = DIR_UP;
        block.b.DevAddr = rx_fhdr->DevAddr;
        block.b.FCnt = ulmd->FCntUp;
        block.b.zero8 = 0;
        block.b.lenMsg = mote->ULPHYPayloadLen - LORA_FRAMEMICBYTES;
        ok = check_cmacS(&block, mote->session.SNwkSIntKeyBin, mote->ULPayloadBin);
        if (!ok) {
            bool abort = true;
            printf("check_cmacS-failed ");
            if (s->confDLPHYPayloadLen > 0) {
                memcpy(s->downlink.PHYPayloadBin, s->confDLPHYPayloadBin, s->confDLPHYPayloadLen);
                s->downlink.PHYPayloadLen = s->confDLPHYPayloadLen;
                printf(" resending-previous-conf-downlink-for-fcnt%u ", s->ConfFCntDown);
                block.b.confFCnt = s->ConfFCntDown - 1;
                ok = check_cmacS(&block, mote->session.SNwkSIntKeyBin, mote->ULPayloadBin);
                if (ok) {
                    printf("ok-%u ", s->ConfFCntDown-1);
                    abort = false;
                } else {
                    printf("fail-%u ", s->ConfFCntDown-1);
                    block.b.confFCnt = 0;
                    ok = check_cmacS(&block, mote->session.SNwkSIntKeyBin, mote->ULPayloadBin);
                    if (ok) {
                        printf("ok-0 ");
                        abort = false;
                    } else
                        printf("fail-0 ");
                }
            } else {
                /* end-device sent ConfFCnt but this sNS didnt send it confirmed downlink */
                printf(" \e[31mno previously-saved downlink for ConfFCntDown%u\e[0m ", s->ConfFCntDown);
            }
            if (abort) {
                mote->ULPHYPayloadLen = 0;  // prevent further handling of this uplink
                return MICFailed;
            }
        } else {
            s->ConfFCntDown = 0;
        }
    } else {
        uint32_t calculated_mic, rx_mic;
        block_t block;
        block.b.header = 0x49;
        block.b.confFCnt = 0;
        block.b.dr = 0;
        block.b.ch = 0;
        block.b.dir = DIR_UP;
        block.b.DevAddr = rx_fhdr->DevAddr;
        block.b.FCnt = ulmd->FCntUp;
        block.b.zero8 = 0;
        block.b.lenMsg = mote->ULPHYPayloadLen - LORA_FRAMEMICBYTES;
        calculated_mic = LoRa_GenerateDataFrameIntegrityCode(&block, mote->session.SNwkSIntKeyBin, mote->ULPayloadBin);
        rx_mic = mote->ULPayloadBin[mote->ULPHYPayloadLen-1] << 24;
        rx_mic += mote->ULPayloadBin[mote->ULPHYPayloadLen-2] << 16;
        rx_mic += mote->ULPayloadBin[mote->ULPHYPayloadLen-3] << 8;
        rx_mic += mote->ULPayloadBin[mote->ULPHYPayloadLen-4];
        if (calculated_mic != rx_mic) {
            mote->ULPHYPayloadLen = 0;  // prevent further handling of this uplink
            return MICFailed;
        }
    }

    MAC_PRINTF("micOK ");

    if (mote->session.nullNFCntDown) {
        char query[128];
        MAC_PRINTF("initNFCntDown ");
        sprintf(query, "UPDATE sessions SET NFCntDown = 0 WHERE DevAddr = %u ORDER BY createdAt DESC LIMIT 1", mote->devAddr);
        if (mq_send(mqd, query, strlen(query)+1, 0) < 0)
            perror("set NFCntDown mq_send");
        mote->session.nullNFCntDown = false;
    }

    if (!sql->OptNeg && s->checkOldSessions) {
        deleteOldSessions(sqlConn_lora_network, mote->devEui, false);
        s->checkOldSessions = false;
    }

    ulmd->Confirmed = (mhdr->bits.MType == MTYPE_CONF_UP);

    if (rx_fhdr->FCtrl.ulBits.ACK) {
        MAC_PRINTF("ACK ");
        if (s->incrNFCntDown) {
            printf("incrNFCntDown%u ", mote->session.NFCntDown);
            if (incr_sql_NFCntDown(mote->devAddr) == 0) {
                s->incrNFCntDown = false;
                printf("incrNFCntDown = false ");
                mote->session.NFCntDown++; // update our RAM copy to same as that in database
            } else
                printf("\e[31mincrFail\e[0m ");
            printf("NFCntDown%u ", mote->session.NFCntDown);
        }
        if (s->downlink.md.Confirmed && s->downlink.FRMPayloadLen > 0) {
            printf("prev-downlink-confirmed-frm%u resend%ld ", s->downlink.FRMPayloadLen, s->downlink.resendAt - time(NULL));
            s->answer_app_downlink = true;  // to be completed after downlink sent
            if (s->downlink.resendAt != 0) {
                /* this is an ack from a downlink that was sent */
                s->downlink.FRMPayloadLen = 0;
                s->downlink.resendAt = 0;   // no more need to resend
            }
        }
    } else if (s->incrNFCntDown)
        printf("\e[31mnotAck-expecting-ack\e[0m ");
    else
        MAC_PRINTF("notAck ");

    printElapsed(mote);

    if (sql->OptNeg)
        ulmd->FCntDown = s->AFCntDown;
    else
        ulmd->FCntDown = mote->session.NFCntDown;

    if (s->session_start_at_uplink) {
        s->session_start_at_uplink = false;
        if (init_session(mote, ulmd->RFRegion) < 0) {
            MAC_PRINTF("\e[31minit_session() failed\e[0m ");
        }
    }

    /* TODO: if (mote->downlink.mtype == MTYPE_CONF_DN) */
    MAC_PRINTF("rx FOptsLen%u ", rx_fhdr->FCtrl.ulBits.FOptsLen);

    printElapsed(mote);

    if ((mote->ULPHYPayloadLen - LORA_FRAMEMICBYTES) > rxofs) {
        rx_fport_ptr = &mote->ULPayloadBin[rxofs];
        ulFRMPayloadBin = &mote->ULPayloadBin[rxofs+1];
        ulFRMPayloadLen = (mote->ULPHYPayloadLen - LORA_FRAMEMICBYTES) - (rxofs + 1);
        printf(" Fport%u ", *rx_fport_ptr);
    } else
        printf(" no-Fport ");

    if (rx_fport_ptr != NULL && *rx_fport_ptr == 0) {
        LoRa_Encrypt(1, mote->session.NwkSEncKeyBin, ulFRMPayloadBin, ulFRMPayloadLen, rx_fhdr->DevAddr, true, ulmd->FCntUp, decrypted);
        MAC_PRINTF("mac commands encrypted on port 0\n");
        *discard = parse_mac_command(mote, ulmd, decrypted, ulFRMPayloadLen);
    } else {
        if (rx_fhdr->FCtrl.ulBits.FOptsLen > 0) {
            /* mac commands are in header */
            MAC_PRINTF("mac commands in header at %u\n", rxofs);
            rxofs = sizeof(mhdr_t) + sizeof(fhdr_t);
            if (sql->OptNeg) {
                /* lorawan1v1 use NwkSEncKey on FOpts field */
                LoRa_Encrypt(0, mote->session.NwkSEncKeyBin, &mote->ULPayloadBin[rxofs], rx_fhdr->FCtrl.ulBits.FOptsLen, rx_fhdr->DevAddr, true, ulmd->FCntUp, decrypted);
                *discard = parse_mac_command(mote, ulmd, decrypted, rx_fhdr->FCtrl.ulBits.FOptsLen);
            } else
                *discard = parse_mac_command(mote, ulmd, &mote->ULPayloadBin[rxofs], rx_fhdr->FCtrl.ulBits.FOptsLen);

        } /*else
            MAC_PRINTF("rx FOptsLen == 0\n");*/
    }

    network_controller_uplink(s, mote->rx_snr);
    if (rx_fhdr->FCtrl.ulBits.ADR || s->force_adr) {
        network_controller_adr(s, ulmd->DataRate, ulmd->RFRegion);
        s->force_adr = false;
    }

    {
        char query[128];
        sprintf(query, "UPDATE sessions SET FCntUp = %u WHERE DevAddr = %u ORDER BY createdAt DESC LIMIT 1", ulmd->FCntUp, mote->devAddr);
        if (mq_send(mqd, query, strlen(query)+1, 0) < 0)
            perror("sNS_uplink mq_send");
    }

    sNS_schedule_downlink(mote);

    return ret;
} // ..sNS_uplink()

static void
getDevAddr(mote_t* argMote)
{
    struct _mote_list* my_mote_list;
    uint32_t max = (1 << nwkAddrBits) - 1;
    uint32_t nwkAddr;

    for (nwkAddr = 0; nwkAddr < max; nwkAddr++) {
        char query[128];
        MYSQL_RES *result;
        MYSQL_ROW row;
        uint32_t attempt = devAddrBase | nwkAddr;
        bool haveAlready = false;
        for (my_mote_list = mote_list; my_mote_list != NULL; my_mote_list = my_mote_list->next) {
            mote_t* mote = my_mote_list->motePtr;
            if (!mote)
                continue;
            /* check if this already exists in our list */
            if (mote->devAddr == attempt) {
                printf("%08x already in our list\n", attempt);
                haveAlready = true;
                break;
            }
        } // ..list iterator
        if (haveAlready)
            continue;

        /* check if this already exists in our database */
        sprintf(query, "SELECT ID FROM sessions WHERE DevAddr = %u LIMIT 1", attempt);
        if (mysql_query(sqlConn_lora_network, query)) {
            printf("\e[31mgetDevAddr: %s\e[0m\n", mysql_error(sqlConn_lora_network));
            argMote->devAddr = NONE_DEVADDR;
            return;
        }
        result = mysql_use_result(sqlConn_lora_network);
        if (!result) {
            printf("\e[31mgetDevAddr: no reuslt\e[0m\n");
            argMote->devAddr = NONE_DEVADDR;
            return;
        }
        row = mysql_fetch_row(result);
        if (row) {
            printf("haveAlready %08x\n", attempt);
            haveAlready = true;
        } else
            printf("dont have %08x\n", attempt);

        mysql_free_result(result);

        if (!haveAlready) {
            argMote->devAddr = attempt;
            printf("assigning %08x\n", attempt);
            break;
        }
    } // ..for (nwkAddr = 0; nwkAddr < max; nwkAddr++)

}

static int
sNS_JoinReq(mote_t* mote, const char* rfRegion)
{
    struct _requesterList* rl;
    int ret = 0;
    char CFListStr[64];
    int roamLifetime = getLifetime(sqlConn_lora_network, mote->devEui, NONE_DEVADDR);

    printElapsed(mote);
    printf("sNS_JoinReq() ");

    if (!mote->s) {
        mote->s = calloc(1, sizeof(s_t));
        printf("created-s ");
    }

    network_controller_mote_init(mote->s, rfRegion, "sNS_JoinReq");
    if (get_cflist(CFListStr, rfRegion) < 0) {
        return -1;
    }
    ret = hNS_toJS(mote, CFListStr);

    for (rl = mote->requesterList; rl != NULL; rl = rl->next) {
        json_object* jobj;
        requester_t *r = rl->R;
        const char* result;
        /* Sending defer to those not best,
         * or if failed to send join request, sending fail to all.
         * Response to best requestor will be later join answer */
        if (ret == 0 && &(rl->R) == mote->bestR)
            continue;

        jobj = json_object_new_object();

        if (roamLifetime < 0)
            printf("\e[31msNS_JoinReq lifetime fail\e[0m\n");

        if (ret != 0 || roamLifetime < 0)
            result = Other;
        else {
            char buf[16];
            
            result = Deferred;
            if (roamLifetime > 0)
                json_object_object_add(jobj, Lifetime, json_object_new_int(roamLifetime));

            sprintf(buf, "%x", mote->devAddr);  // must instruct to defer any future (un)conf uplinks
            json_object_object_add(jobj, DevAddr, json_object_new_string(buf));
        }

        if (r_post_ans_to_ns(&(rl->R), result, jobj) < 0) {
            printf("\e[31msNS http_post to %s failed\e[0m", r->ClientID);
        }
    }

    return ret;
} // ..sNS_JoinReq()

const char*
assignDevAddrToJS(mote_t* mote, const char* rfRegion)
{
    const char* result = Other;
    getDevAddr(mote);

    if (mote->devAddr != NONE_DEVADDR) {
        if (sNS_JoinReq(mote, rfRegion) < 0)
            printf("\e[31msNS_JoinReq() failed\e[0m\n");
        else
            result = Success;
    } else
        printf("\e[31mgetDevAddr() failed\e[0m ");

    mote->session.until = 0;    // invalidate session stored in RAM
    return result;
}

const char*
sNS_uplink_finish(mote_t* mote, bool jsonFinish, sql_t* sql, bool* discard)
{
    s_t* s;
    mhdr_t* mhdr;
    const char* uplinkResult;
    ULMetaData_t* u;

    printf("sNS_uplink_finish() up-phyLen%u  ", mote->ULPHYPayloadLen);
    s = mote->s;

    if (mote->ULPHYPayloadLen == 0) {
        printf(" sNS_uplink_finish() ULPHYPayloadLen==0\n");
        return NULL;    // no uplink to take, or we're not sNS for this ED
    }

    if (mote->bestR != NULL) {
        requester_t* r = *(mote->bestR);
        u = &r->ulmd;
    } else
        u = &mote->ulmd_local;

    uplinkResult = Other;

    mhdr = (mhdr_t*)mote->ULPayloadBin;
    *discard = false;

    if (s) {
        printf(" \e[36mChMask:%04x\e[0m ", s->ChMask[0]);
    }

    printf(" sNS-");
    print_mtype(mhdr->bits.MType);
    if (mhdr->bits.MType == MTYPE_REJOIN_REQ) {
        rejoin1_req_t *rj1 = (rejoin1_req_t*)mote->ULPayloadBin;
        if (rj1->type == 1) {
            /* periodic recovery */
            printf("sNS-type1-");
            /* ? complete loss of context means no context or expired context ? */
            if (mote->session.expired) {
                printf("expired-toJS ");
                assignDevAddrToJS(mote, u->RFRegion);
            } else if (sql->is_sNS) {
                printf("rejoin%u still have session ", rj1->type);
                /* still have session, perhaps a downlink is waiting */
                sNS_schedule_downlink(mote);
            } else
                printf("\e[31mdropping-%s\e[0m ", sql->roamState);
        } else if (rj1->type == 2 || rj1->type == 0) {
            bool forced = false;
            uint32_t netid, calcMic, rxMic;
            rxMic = mote->ULPayloadBin[mote->ULPHYPayloadLen-1] << 24;
            rxMic += mote->ULPayloadBin[mote->ULPHYPayloadLen-2] << 16;
            rxMic += mote->ULPayloadBin[mote->ULPHYPayloadLen-3] << 8;
            rxMic += mote->ULPayloadBin[mote->ULPHYPayloadLen-4];
            // TODO cmac = aes128_cmac(SNwkSIntKey, MHDR | Rejoin Type | NetID | DevEUI | RJcount0)
rejoinTry:
            LoRa_GenerateJoinFrameIntegrityCode(true, mote->session.SNwkSIntKeyBin, mote->ULPayloadBin, mote->ULPHYPayloadLen-LORA_FRAMEMICBYTES, (uint8_t*)&calcMic);
            printf(" rxMic:%08x calcMic:%08x ", rxMic, calcMic);
            if (rxMic == calcMic) {
                rejoin02_req_t* r2 = (rejoin02_req_t*)mote->ULPayloadBin;
                /* permit any rejoin-type2 when in expired hNS-handover state */
                printf(" type%u ", rj1->type);
                if (rj1->type == 2) {
                    if (sql->roamState == roamhHANDOVER && sql->roamExpired) {
                        forced = true;
                        goto accept;
                    }
                    if (s->type2_rejoin_count == 0) {
                        printf("\e[31m(type2_rejoin_count==0 %s expired%u)\e[0m ", sql->roamState, sql->roamExpired);
                        return Other;   // this sNS didnt request type2 rejoin
                    } else
                        s->type2_rejoin_count--;
                }
accept:
                /* according to 6.2.4.4, sent to JS if session expired */
                netid = r2->NetID[0];
                netid |= r2->NetID[1] << 8;
                netid |= r2->NetID[2] << 16;
                if (netid == myNetwork_id32) {
                    if (forced || mote->session.expired) {
                        printf("expired-toJS ");
                        uplinkResult = assignDevAddrToJS(mote, u->RFRegion);
                    } else if (sql->is_sNS) {
                        printf("rejoin%u %lus remaining in session ", r2->type, mote->session.until - time(NULL));
                        /* still have session, perhaps a downlink is waiting */
                        sNS_schedule_downlink(mote);
                    } else
                        printf("\e[31mdropping-%s\e[0m ", sql->roamState);
                } else
                    printf("\e[31mrejoin%u not my net %06x\e[0m\n", r2->type, netid);
            } else {
                printf(" \e[31mrxMic:%08x != calcMic:%08x next:%u\e[0m ", rxMic, calcMic, mote->session.next);
                if (mote->session.next) {
                    if (getSession(sqlConn_lora_network, mote->devEui, mote->devAddr, ++mote->nth, &mote->session) < 0) {
                        printf("\e[31mgetLatestSession fail\e[0m\n");
                        return Other;
                    }
                    goto rejoinTry;
                }
            }
        } // ..if (rj1->type == 2 || rj1->type == 0)

    } else if (mhdr->bits.MType == MTYPE_JOIN_REQ) {
        uint32_t fwdto_netId;
        if (!mote->session.expired)
            printf("join-before-session-end ");

        uplinkResult = sql_motes_query_item(sqlConn_lora_network, mote->devEui, NONE_DEVADDR, "fwdToNetID", &fwdto_netId);
        if (uplinkResult != Success)
            printf("\e[31m%s = sql_motes_query_item()\e[0m\n", uplinkResult );
        else {
            if (fwdto_netId == NONE_NETID || fwdto_netId == myNetwork_id32)
                uplinkResult = assignDevAddrToJS(mote, u->RFRegion);
            else
                printf("\e[31mfNS should have taken care of NS %06x\e[0m\n", fwdto_netId);
        }

    } else if (jsonFinish && (mhdr->bits.MType == MTYPE_UNCONF_UP || mhdr->bits.MType == MTYPE_CONF_UP)) {
        MAC_PRINTF(" sNS-(un)conf sql->roamState :%s ", sql->roamState);

        if (sql->roamState == roamsPASSIVE) {
            if (mote->bestR == NULL) {
                printf("best_r==NULL-localBest ");
                /* our own gateway has better signal than roaming partner */
                printf(" \e[31mStopPassive\e[0m ");
                s->RStop = true;
            } else
                printf("best_r-remoteBest ");
        } else if (sql->roamState == roamhHANDOVER) {
            /* Cannot compare local uplink with remote because it only comes when
             * there is FRMPayload and probably doesnt have signal quality */
        }

        if (sql->roamState == roamsPASSIVE || sql->roamState == roamNONE) {
            char txt[32];
            uplinkResult = sNS_uplink(mote, sql, u, discard, txt);
            s = mote->s;
            /* sNS_uplink() null return when result expected later from json answer */
            MAC_PRINTF("sNS %s ", sql->roamState);
            if (uplinkResult) {
                if (uplinkResult != Success)
                    printf(" sNS_uplink_finish \e[31msNS %s = sNS_uplink() %s\e[0m\n", uplinkResult, txt);
            } else
                MAC_PRINTF("NULL");
            MAC_PRINTF(" = sNS_uplink() downlink.phyLen%u\e[0m\n", s->downlink.PHYPayloadLen);
        } else
            printf("not-sNS-%s\n", sql->roamState);    /* this NS is not sNS for this end-device */

    } // ..else if (mhdr->bits.MType == MTYPE_UNCONF_UP || mhdr->bits.MType == MTYPE_CONF_UP)

    MAC_PRINTF("\n");
    return uplinkResult;
} // ..sNS_uplink_finish()

static const char*
downlinkBC(MYSQL* sc, mote_t* mote, const sql_t* sql, json_object** ansJobj)
{
    time_t now = time(NULL);
    char buf[32];
    s_t* s = mote->s;
    bool Benabled = false;

    if (s->ClassB.ping_period > 0) {
        deviceProfileReq(sc, mote->devEui, mote->devAddr, SupportsClassB, buf, sizeof(buf));
        if (buf[0] == 1)
            Benabled = true;
    }

    s->downlink.resendAt = 0;
    if (s->downlink.md.Confirmed)
        printf(" Conf-Down ");
    else
        printf(" UNconf-Down ");

    if (s->ClassB.ping_period > 0 && Benabled) {
        const char* result;
        sNS_schedule_downlink(mote);
        result = sNS_finish_phy_downlink(mote, sql, 'B', ansJobj);  /* class B? send to fNS now */
        printf("classB-send-result:%s\n", result);
        if (result == Success) {
            if (s->downlink.md.Confirmed) {
                unsigned n;
                deviceProfileReq(sc, mote->devEui, mote->devAddr, ClassBTimeout, buf, sizeof(buf));
                sscanf(buf, "%u", &n);
                if (n > 0)
                    s->downlink.resendAt = now + n;
                return NULL;    // json answer isnt sent until Ack uplink received
            }
            printf("resendAt%lu ", s->downlink.resendAt);
        }
        return result;
    } else {
        const char* result;
        char buf[32];
        deviceProfileReq(sc, mote->devEui, mote->devAddr, SupportsClassC, buf, sizeof(buf));
        printf("SupportsClassC:%c ", buf[0]);
        if (buf[0] == '1') {
            /* LoRaWAN-1.1 will tell us device class, but LoRaWAN-1.0 will not */
            if ((mote->session.OptNeg && s->classModeInd == CLASS_C) || !mote->session.OptNeg) {
                sNS_schedule_downlink(mote);
                result = sNS_finish_phy_downlink(mote, sql, 'C', ansJobj);  /* class C? send to fNS now */
                printf("classC-send-result:%s\n", result);
                if (result == Success) {
                    if (s->downlink.md.Confirmed) {
                        unsigned n;
                        deviceProfileReq(sc, mote->devEui, mote->devAddr, ClassCTimeout, buf, sizeof(buf));
                        sscanf(buf, "%u", &n);
                        printf("ClassCTimeout:%u ", n);
                        if (n > 0)
                            s->downlink.resendAt = now + n;
                        return NULL;    // json answer isnt sent until Ack uplink received
                    }
                    printf("resendAt%lu ", s->downlink.resendAt);
                }
                return result;
            }
        }
    }

    return NULL;
}

const char*
hNS_to_sNS_downlink(MYSQL* sc, mote_t* mote, unsigned long reqTid, const char* requester, const uint8_t* frmPayload, uint8_t frmPayloadLength, const DLMetaData_t* dlmd, json_object** ansJobj)
{
    s_t* s;
    char buf[32];
    sql_t sql;

    printf("hNS_to_sNS_downlink() ");

    sql_motes_query(sc, mote->devEui, mote->devAddr, &sql);

    if (!mote->s) {
        mote->s = calloc(1, sizeof(s_t));
        deviceProfileReq(sc, mote->devEui, mote->devAddr, RFRegion, buf, sizeof(buf));
        network_controller_mote_init(mote->s, getRFRegion(buf), "hNS_to_sNS_downlink");
    }
    s = mote->s;

    if (!sql.OptNeg) {
        unsigned neededFCntDown = mote->session.NFCntDown;
        char buf[32];

        /* lorawan 1.0: when AFCntDown != NFCntDown, then FRMPayload is encrypted with wrong frame count */

        deviceProfileReq(sc, mote->devEui, mote->devAddr, SupportsClassC, buf, sizeof(buf));
        /* for lorawan1v0: if device profile supports ClassC, assume its always in classC */
        if (buf[0] != '1') {
            /* downlink sent in response to uplink */
            if (s->incrNFCntDown) // previous downlink was confirmed
                neededFCntDown++;
        } /* else downlink sent immediately classC */

        if (dlmd->FCntDown != neededFCntDown) {
            printf("\e[33mFCntDown: AS gave %u but NS ", dlmd->FCntDown);
            if (s->incrNFCntDown) { // previous downlink was confirmed
                printf("will have");
            } else  // previous downlink was unconfirmed
                printf("has");
            printf(" %u\e[0m\n", neededFCntDown);

            *ansJobj = json_object_new_object();    // generate answer now
            json_object_object_add(*ansJobj, FCntDown, json_object_new_int(neededFCntDown));
            return Other;
        }
    }

    s = mote->s;

    /* class A: save payload, to be sent later on next rx window */
    s->downlink.FRMPayloadLen = frmPayloadLength;
    memcpy(s->downlink.FRMPayloadBin, frmPayload, frmPayloadLength);
    memcpy(&s->downlink.md, dlmd, sizeof(DLMetaData_t));
    printf("hNS_to_sNS_downlink() \e[5m%p-->frmLen%u\e[0m fport%u tid%lu FCntDown%u ping_period%u\n", s, s->downlink.FRMPayloadLen, s->downlink.md.FPort, s->downlink.reqTid, dlmd->FCntDown, s->ClassB.ping_period);

    if (sql.roamState == roamsPASSIVE || sql.roamState == roamNONE)
        s->downlink.isAnsDestAS = true;
    else
        s->downlink.isAnsDestAS = false;
        
    s->downlink.reqTid = reqTid;
    if (s->downlink.ansDest_)
        s->downlink.ansDest_ = realloc(s->downlink.ansDest_, strlen(requester)+1);
    else
        s->downlink.ansDest_ = malloc(strlen(requester)+1);
    strcpy(s->downlink.ansDest_, requester);

    return downlinkBC(sc, mote, &sql, ansJobj);
} // ..hNS_to_sNS_downlink()

uint8_t
sNS_get_dlphylen(const mote_t* mote)
{
    s_t* s = mote->s;
    return s->downlink.PHYPayloadLen;
}

void
sNSDownlinkSent(mote_t* mote, const char* result, json_object** httpdAnsJobj)
{
    s_t* s = mote->s;
    mhdr_t* tx_mhdr = (mhdr_t*)&s->downlink.PHYPayloadBin[0];

    printf("sNSDownlinkSent(,%s,) ", result);
    print_mtype(tx_mhdr->bits.MType);
    if (result == Success) {
        //printf("sNSDownlinkSent()-success ");
        if (tx_mhdr->bits.MType == MTYPE_UNCONF_DN) {
            printf("sentUnConf NFCntDown%u ", mote->session.NFCntDown);
            if (s->incrNFCntDown && incr_sql_NFCntDown(mote->devAddr) == 0) {
                mote->session.NFCntDown++; // update our RAM copy to same as that in database
                s->incrNFCntDown = false;
            }
            printf("NFCntDown%u ", mote->session.NFCntDown);
        } else
            printf("sNSDownlinkSent(,%s,) mtype %02x=notUnConf ", result, tx_mhdr->bits.MType);

        s->downlink.PHYPayloadLen = 0;
    } else
        printf(" \e[31msNSDownlinkSent(,%s,)\e[0m ", result);

    if (result != NULL && !s->downlink.md.Confirmed && s->downlink.FRMPayloadLen > 0) {
        printElapsed(mote);
        answer_app_downlink(mote, result, httpdAnsJobj);
        s->answer_app_downlink = false;;
    }

    //MAC_PRINTF("\n");
} // ..sNSDownlinkSent()

const char*
sNS_finish_phy_downlink(mote_t* mote, const sql_t* sql, char classMode, json_object** httpdAnsJobj)
{
    mhdr_t ul_mhdr;
    s_t* s = mote->s;
    const char* result = NULL;
    uint8_t rxDelay1;

    printElapsed(mote);
    printf("sNS_finish_phy_downlink() ");
    if (!s) {
        printf("s==NULL\n");
        return Other;
    }
    printf(" down-phylen%u ", s->downlink.PHYPayloadLen);

    ul_mhdr.octet = mote->ULPayloadBin[0];
    printf(" ul-");
    print_mtype(ul_mhdr.bits.MType);

    if (s->downlink.PHYPayloadLen == 0) {
        printf(" zero-dl-phyLen\n");
        goto downlinkDone;
    }

    if (ul_mhdr.bits.MType == MTYPE_JOIN_REQ || ul_mhdr.bits.MType == MTYPE_REJOIN_REQ)
        rxDelay1 = 5;
    else
        rxDelay1 = 1;    // TODO get from DeviceProfile

    if (mote->bestR == NULL) {
        /* send locally */
        MAC_PRINTF("dl-local ");
        result = sNS_phy_downlink_local(mote, &mote->ulmd_local, rxDelay1, classMode);
    } else {
        requester_t* r;
        uint32_t nid;
        /* send remotely? */
        char hostname[64];
        DLMetaData_t dlMetaData = { 0 };

        if (mote->devEui == NONE_DEVEUI) {
            dlMetaData.DevEUI = NONE_DEVEUI;
            dlMetaData.DevAddr = mote->devAddr;
        } else {
            dlMetaData.DevEUI = mote->devEui;
            dlMetaData.DevAddr = NONE_DEVADDR;
        }

        dlMetaData.ClassMode = classMode;
        if (s->ClassB.ping_period > 0 && classMode == 'B') {
            if (getPingConfig(mote->devEui, mote->devAddr, &dlMetaData) < 0)
                return Other;
            dlMetaData.PingPeriod = s->ClassB.ping_period;
            printf( "B-PingPeriod:%u ", dlMetaData.PingPeriod);
        } else {
            dlMetaData.RXDelay1 = rxDelay1;
        }

        if (mote->bestR == NULL) {
            printf("\e[31mno bestR\e[0m\n");
            return Other;
        }
        r = *(mote->bestR);

        json_object *dlmd_obj = generateDLMetaData(&r->ulmd, mote->rxdrOffset1, &dlMetaData, sNS);

        s->DLFreq1 = dlMetaData.DLFreq1;
        s->DLFreq2 = dlMetaData.DLFreq2;
        MAC_PRINTF("dl-remote ");
        if (dlmd_obj) {
            json_object* jo = json_object_new_object(); 
            json_object_object_add(jo, DLMetaData, dlmd_obj);
            sscanf(r->ClientID, "%x", &nid);
            sprintf(hostname, "%06x.%s", nid, netIdDomain);
            //MAC_PRINTF(" sNS_phy_down->");
            result = sendXmitDataReq(mote, "sNS_finish_phy_downlink->sendXmitDataReq", jo, r->ClientID, hostname, PHYPayload, s->downlink.PHYPayloadBin, s->downlink.PHYPayloadLen, sNS_downlink_phy_answer);
            if (result != Success)
                printf("\e[31msNS_finish_phy_downlink %s = sendXmitDataReq\e[0m\n", result);
        } else
            result = Other;

        dlmd_free(&dlMetaData);
    }

    printf(">>have result %s<< ", result);
    if (result != NULL) // when result == NULL, downlink hasnt yet been sent
        sNSDownlinkSent(mote, result, httpdAnsJobj);

downlinkDone:
    if (s->answer_app_downlink) {
        s->answer_app_downlink = false;;
        answer_app_downlink(mote, Success, httpdAnsJobj);
    }

    if (ul_mhdr.bits.MType != MTYPE_UNCONF_UP && ul_mhdr.bits.MType != MTYPE_CONF_UP)
        return result;

    /* the following only runs on MTYPE_UNCONF_UP or MTYPE_CONF_UP */

    /* forward any FRMPayload in uplink */
    fhdr_t *rx_fhdr = (fhdr_t*)&mote->ULPayloadBin[1];
    int rxofs = sizeof(mhdr_t) + sizeof(fhdr_t) + rx_fhdr->FCtrl.ulBits.FOptsLen;
    if ((mote->ULPHYPayloadLen - LORA_FRAMEMICBYTES) > rxofs) {
        const char* ret;
        const uint8_t* rx_fport_ptr = &mote->ULPayloadBin[rxofs];
        const uint8_t* FRMPayloadBin = &mote->ULPayloadBin[rxofs+1];
        uint8_t FRMPayloadLen = (mote->ULPHYPayloadLen - LORA_FRAMEMICBYTES) - (rxofs + 1);
        printf(" up-FPort%u up-FRMPayloadLen%u ", *rx_fport_ptr, FRMPayloadLen);

        if (*rx_fport_ptr > 0) {
            ULMetaData_t* ulmd;
            ServiceProfile_t sp;
            json_object* ulmd_json;
            if (mote->bestR != NULL) {
                requester_t* r = *(mote->bestR);
                ulmd = &r->ulmd;
            } else
                ulmd = &mote->ulmd_local;

            ulmd->FPort = *rx_fport_ptr;
            readServiceProfileFromSql(sqlConn_lora_network, mote->devEui, mote->devAddr, &sp);
            ulmd_json = generateULMetaData(ulmd, sNS, sp.AddGWMetadata);
            if (ulmd_json) {
                /* TODO ULMetaData remove FNSULToken, RFRegion, not to be sent  */
                if (sql->roamState == roamsHANDOVER) {
                    char hostname[128];
                    char destIDstr[12];
                    json_object* jo = json_object_new_object();
                    json_object_object_add(jo, ULMetaData, ulmd_json);
                    sprintf(destIDstr, "%06x", sql->roamingWithNetID);
                    sprintf(hostname, "%06x.%s", sql->roamingWithNetID, netIdDomain);
                    //printf(" sNS_frm_up->");
                    ret = sendXmitDataReq(mote, "sNS_finish_phy_downlink->sendXmitDataReq", jo, destIDstr, hostname, FRMPayload, FRMPayloadBin, FRMPayloadLen, sNS_uplink_frm_answer);
                    if (ret == Success) {
                        ret = NULL; // result is provided by sNS_XmitDataAnsCallback()
                    } else
                        printf("\e[31m%s = sendXmitDataReq()\e[0m\n", ret);
                } else {
                    ULMetaData_t ulmd;
                    printElapsed(mote);
                    if (ParseULMetaData(ulmd_json, &ulmd) == 0) {
                        printf("sNS_finish_phy_downlink->");
                        if (hNS_XmitDataReq_toAS(mote, FRMPayloadBin, FRMPayloadLen, &ulmd) < 0) {
                            printf("\e[31mhNS_XmitDataReq_toAS() failed\[e0m\n");
                            ret = XmitFailed;
                        }
                    } else {
                        printf("\e[31mParseULMetaData_() failed\e[0m\n");
                        ret = MalformedRequest;
                    }
                }
            } else
                printf("\e[31msNS_finish_phy_downlink bad ulmd\e[0m\n");

        }
    } else {
        MAC_PRINTF("no-FPort ");
    }

    return result;
} // ..sNS_finish_phy_downlink()

static void
sNS_xRStopAnsCallback(MYSQL* sc, mote_t* mote, json_object* jobj, const char* rxResult, const char* senderID, const uint8_t* rfBuf, uint8_t rfLen)
{
    if (rxResult == Success) {
        uint32_t otherNetID;
        sscanf(senderID, "%x", &otherNetID);
        printf("sNS_xRStopAnsCallback(%s)\n", rxResult);
        mote_update_database_roam(sc, mote->devEui, mote->devAddr, roamNONE, NULL, &otherNetID, NULL);
    } else
        printf("\e[31msNS_xRStopAnsCallback(%s)\e[0m\n", rxResult);
}

void
sNS_roamStop(mote_t* mote)
{
    s_t* s;
    if (!mote->s)
        return;
    s = mote->s;

    s->RStop = true;
}

void
sNS_service(mote_t* mote, time_t now)
{
    s_t* s;

    if (mote->progress != PROGRESS_OFF || mote->s == NULL)
        return;

    s = mote->s;

    if (s->RStop) {
        CURL* easy;
        char buf[64];
        json_object *jobj;
        char hostname[128];
        char destIDstr[12];
        sql_t sql;
        uint32_t tid;
        const char *mt, *sr = sql_motes_query(sqlConn_lora_network, mote->devEui, mote->devAddr, &sql);
        if (sr != Success) {
            goto stopDone;
        }

        if (sql.roamState == roamsPASSIVE)
            mt = PRStopReq;
        else if (sql.roamState == roamhHANDOVER)
            mt = HRStopReq;
        else {
            printf("\e[31mcant stop roam %s\n", sql.roamState);
            goto stopDone;
        }

        if (next_tid(mote, "sNS_service", sNS_xRStopAnsCallback, &tid) < 0) {
            printf("\e[31mhroamStop-tid-fail\e[0m\n");
            goto stopDone;
        }

        jobj = json_object_new_object();
        /* send Lifetime if this NS doesnt want to receive another roaming start request */

        if (mote->devEui != NONE_DEVEUI) {
            sprintf(buf, "%"PRIx64, mote->devEui);
            json_object_object_add(jobj, DevEUI, json_object_new_string(buf));
        } else {
            sprintf(buf, "%x", mote->devAddr);
            json_object_object_add(jobj, DevAddr, json_object_new_string(buf));
        }

        sprintf(destIDstr, "%06x", sql.roamingWithNetID);
        sprintf(hostname, "%06x.%s", sql.roamingWithNetID, netIdDomain);


        lib_generate_json(jobj, destIDstr, myNetwork_idStr, tid, mt, NULL);

        easy = curl_easy_init();
        if (easy) {
            curl_multi_add_handle(multi_handle, easy);
            if (http_post_hostname(easy, jobj, hostname, true) < 0)
                printf("\e[31mhttp_post_hostname() failed\[e0m\n");
        }

stopDone:
        s->RStop = false;
    } // ..if (s->RStop)

    if (s->downlink.resendAt != 0) {
        const char* result;
        //printf("resend? ");
        if (s->downlink.resendAt <= now) {
            sql_t sql;
            printf("resending %lds\n", s->downlink.resendAt - now);
            sql_motes_query(sqlConn_lora_network, mote->devEui, mote->devAddr, &sql);
            result = downlinkBC(sqlConn_lora_network, mote, &sql, NULL);
            if (result)
                answer_app_downlink(mote, result, NULL);
        } /*else {
            printf("no, in %lds\n", s->downlink.resendAt - now);
        }*/
    }

} // ..sNS_service()

