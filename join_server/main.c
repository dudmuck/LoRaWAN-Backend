/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "web.h"
#include "js.h"

#define JS_VERSION  "0.1"   /**< version */

#define LORA_MAXFRAMELENGTH             235 /**< maximum user payload length for LoRaWAN packet */
#define LORA_MINDATAHEADERLENGTH        7   /**< size of LoRaWAN header */
#define LORA_PORTLENGTH                 1   /**< size of port byte */
#define LORA_MAXDATABYTES    (LORA_MAXFRAMELENGTH - (LORA_MACHEADERLENGTH + LORA_MINDATAHEADERLENGTH + LORA_PORTLENGTH + LORA_FRAMEMICBYTES)) /**< encrypted buffer maximum size */

typedef enum {
    keyType_FNwkSIntKey,
    keyType_SNwkSIntKey,
    keyType_NwkSEncKey,
    keyType_AppSKey
} keyType_e;


key_envelope_t key_envelope_app;
key_envelope_t key_envelope_nwk;
MYSQL *sql_conn_lora_join;

uint64_t myJoinEui64;
char myJoinEuiStr[64];

char netIdDomain[64];

void GenerateJoinKey(uint8_t token, const uint8_t* root_key, const uint8_t* dev_eui, uint8_t* output)
{
    aes_context aesContext;
    uint8_t input[LORA_ENCRYPTIONBLOCKBYTES];
    uint8_t* ptr = input;

    ptr = Write1ByteValue(ptr, token);

    /* dev_eui is taken directly from over-the-air reception, so order is correct */
    memcpy(ptr, dev_eui, LORA_EUI_LENGTH);
    ptr += LORA_EUI_LENGTH;

    memset(ptr, 0, LORA_ENCRYPTIONBLOCKBYTES - (ptr - input));

    DEBUG_CRYPT_BUF(root_key, LORA_CYPHERKEYBYTES, "generate-join-key-root_key");
    aes_set_key(root_key, LORA_CYPHERKEYBYTES, &aesContext);

    DEBUG_CRYPT_BUF(input, LORA_CYPHERKEYBYTES, "generate-join-key-input");
    aes_encrypt(input, output, &aesContext);
}

static int
get_sql_joinmote_key_(const char* key_name, uint64_t devEui64, uint8_t* out)
{
    char query[512];
    MYSQL_RES *result;
    int ret = -1;

    sprintf(query, "SELECT %s FROM joinmotes WHERE eui = %"PRIu64, key_name, devEui64);
    printf("js %s\n", query);

    if (mysql_query(sql_conn_lora_join, query)) {
        printf("\e[31mJS (get key): %s\e[0m\n", mysql_error(sql_conn_lora_join));
        return ret;
    }

    result = mysql_use_result(sql_conn_lora_join);
    if (result != NULL) {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            if (row[0]) {
                memcpy(out, row[0], LORA_CYPHERKEYBYTES);
                ret = 1;
            } else
                ret = 0;
        } else
            printf("\e[31mjoinmotes %s No row\e[0m\n", key_name);

        mysql_free_result(result);
    } else 
        printf("\e[31mjoinmotes %s No result\e[0m\n", key_name);


    return ret;
} // ..get_sql_joinmote_key_()

/* return 1=nonce found, 0=nonce unique */
static int
check_nonce(uint64_t dev_eui64, uint16_t DevNonce)
{
    unsigned int num_fields, n_nonces = 0;
    char query[512];
    MYSQL_RES *result;
    MYSQL_ROW row;

    printf("%"PRIx64" join-DevNonce:%04x ", dev_eui64, DevNonce);
    sprintf(query, "SELECT mote FROM nonces WHERE HEX(mote) = '%"PRIx64"' AND HEX(nonce) = '%x'", dev_eui64, DevNonce);
    SQL_PRINTF("js %s\n", query);
    if (mysql_query(sql_conn_lora_join, query)) {
        fprintf(stderr, "\e[31mJS (check nonce) Error querying server: %s\e[0m\n", mysql_error(sql_conn_lora_join));
        return -1;
    }
    result = mysql_use_result(sql_conn_lora_join);
    if (result == NULL) {
        printf("nonces No result.\n");
        return -1;
    }

    num_fields = mysql_num_fields(result);
    while ((row = mysql_fetch_row(result))) {
        unsigned long *field_lengths = mysql_fetch_lengths(result);
        for (int i = 0; i < num_fields; i++) {
            printf("[%d:%lu:%s] ", i, field_lengths[i], row[i]);
        }
        printf("\n");
        n_nonces++;
    }

    mysql_free_result(result);

    return n_nonces;
}

/** @brief generates session keys for OTA motes, run at join-accept
 * @param generateNetworkKey [in] true for network key, false for application key
 * @param applicationKey [in] OTA key
 * @param networkId [in] global network ID
 * @param applicationNonce [in] random number generated at join-accept
 * @param deviceNonce [in] random number generated by end-node
 * @param output [out] generated key
*/
static void GenerateSessionKey(bool OptNeg, keyType_e key_type, const uint8_t* root_key, uint32_t joinNonce, const uint8_t* rxJoinEUI, uint16_t rxDevNonce, uint32_t network_id, uint8_t* output)
{
    aes_context aesContext;
    uint8_t input[LORA_ENCRYPTIONBLOCKBYTES];
    uint8_t* ptr = &input[1];

    switch (key_type) {
        case keyType_FNwkSIntKey: input[0] = 0x01; break;
        case keyType_SNwkSIntKey: input[0] = 0x03; break;
        case keyType_NwkSEncKey: input[0] = 0x04; break;
        case keyType_AppSKey: input[0] = 0x02; break;
    }

    ptr = Write3ByteValue(ptr, joinNonce);
    if (OptNeg) {
        memcpy(ptr, rxJoinEUI, LORA_EUI_LENGTH);
        ptr += LORA_EUI_LENGTH;
    } else {
        ptr = Write3ByteValue(ptr, network_id);
    }
    ptr = Write2ByteValue(ptr, rxDevNonce);
    memset(ptr, 0, LORA_ENCRYPTIONBLOCKBYTES - (ptr - input));

    DEBUG_CRYPT_BUF(root_key, LORA_CYPHERKEYBYTES, "genkey-root");
    aes_set_key(root_key, LORA_CYPHERKEYBYTES, &aesContext);

    DEBUG_CRYPT("OptNeg%u ", OptNeg);
    print_buf(input, LORA_CYPHERKEYBYTES, "genkey-in");
    aes_encrypt(input, output, &aesContext);
}

/** @brief encrypt join-accept message for transmit to end-node
 * @param key [in] OTA AES key
 * @param input [in] unencrypted buffer
 * @param length [in] length of unencrypted buffer
 * @param output [out] encrypted to be transmitted
 */
void CryptJoinServer(uint8_t const* key, uint8_t const* input, uint16_t length, uint8_t* output)
{
    aes_context aesContext;
    memset(aesContext.ksch, '\0', 240);
    aes_set_key(key, LORA_CYPHERKEYBYTES, &aesContext);

    aes_decrypt(input, output, &aesContext);
    if (length >= 16)
        aes_decrypt(input + 16, output + 16, &aesContext);
}

typedef union {
    struct {
        uint8_t mhdr;
        unsigned int _joinNonce : 24;
        unsigned int Home_NetID : 24;
        uint32_t DevAddr;
        uint8_t DLSettings;
        uint8_t RxDelay;
    } __attribute__((packed)) fields;
    uint8_t octets[13];
} joinAccept_t;

/* called from incoming json, return Result string */
const char*
parse_rf_join_req(const char* mt, json_object* inJobj, uint32_t network_id, json_object** ansJobj)
{
    int i;
    uint8_t rxLen, JoinReqType;
    uint8_t rxBuf[256], txBuf[256];
    uint8_t* rxBufPtr = rxBuf;
    uint8_t uncyphered[LORA_MAXDATABYTES];
    joinAccept_t* ja;
    char query[512];
    const char* ret = Success;
    uint8_t MicKey[LORA_CYPHERKEYBYTES];
    int haveMicKey = 0;
    uint8_t NwkKey[LORA_CYPHERKEYBYTES];
    uint8_t _AppKey[LORA_CYPHERKEYBYTES];
    bool OptNeg;
    uint64_t devEui64;
    MYSQL_RES *result;
    unsigned lifetime;
    mhdr_t* mhdr;
    uint16_t rxNonce;
    const uint8_t* rfJoinEUI;
    uint8_t rfEUI[LORA_EUI_LENGTH];
    json_object* obj;

    printf("parse_rf_join_req(%s,,%06x,) ", mt, network_id);

    *ansJobj = json_object_new_object();

    if (json_object_object_get_ex(inJobj, PHYPayload, &obj)) {
        const char* phyStr = json_object_get_string(obj);
        int phyStrLen = strlen(phyStr);
        if (phyStrLen > sizeof(rxBuf)) {
            printf("\e[31mphyStrLen%u\n", phyStrLen);
            return FrameSizeError;
        }

        rxLen = phyStrLen / 2;
        for (i = 0; i < phyStrLen; i += 2) {
            unsigned o;
            sscanf(phyStr, "%02x", &o);
            *rxBufPtr++ = o;
            phyStr += 2;
        }
    } else
        return MalformedRequest;

    if (json_object_object_get_ex(inJobj, DevEUI, &obj)) {
        const char* str = json_object_get_string(obj);
        sscanf(str, "%"PRIx64, &devEui64);
        printf(" DevEUI:%016"PRIx64" ", devEui64);
    } else
        return MalformedRequest;



    mhdr = (mhdr_t*)rxBuf;
    print_mtype(mhdr->bits.MType);
    if (mhdr->bits.MType == MTYPE_REJOIN_REQ) {
        rejoin1_req_t *rj1 = (rejoin1_req_t*)rxBuf;
        if (rj1->type == 1) {
            if (devEui64 != eui_buf_to_uint64(rj1->DevEUI)) {
                printf("\e[31mwrong DevEUI %016"PRIx64" vs packet %016"PRIx64"\e[0m\n", devEui64, eui_buf_to_uint64(rj1->DevEUI));
                return Other;
            }
            rfJoinEUI = rj1->JoinEUI;
            rxNonce = rj1->RJcount1;
            // TODO cmac = aes128_cmac(JSIntKey, MHDR | RejoinType | JoinEUI| DevEUI | RJcount1)
            haveMicKey = get_sql_joinmote_key_("JSIntKey", devEui64, MicKey);
            if (haveMicKey != 1) {
                printf("\e[31mjoin-mote not found devEui:%"PRIx64"\e[0m\n", devEui64);
                ret = UnknownDevEUI;
                goto jend;
            }
        } if (rj1->type == 0 || rj1->type == 2) {
            rejoin02_req_t* rj2 = (rejoin02_req_t*)rxBuf;
            rxNonce = rj2->RJcount0;
            uint64_to_eui_buf(myJoinEui64, rfEUI);
            rfJoinEUI = rfEUI;
            /* MIC done by NS because it uses SNwkSIntKey */
            haveMicKey = 0;
        } else {
            printf("unknown rejoin type%u\n", rj1->type);
            return Other;
        }

        JoinReqType = rj1->type; 
    } else if (mhdr->bits.MType == MTYPE_JOIN_REQ) {
        join_req_t* jreq_ptr = (join_req_t*)rxBuf;
        if (devEui64 != eui_buf_to_uint64(jreq_ptr->DevEUI)) {
            printf("\e[31mwrong DevEUI %016"PRIx64" vs packet %016"PRIx64"\e[0m\n", devEui64, eui_buf_to_uint64(jreq_ptr->DevEUI));
            return Other;
        }
        rfJoinEUI = jreq_ptr->JoinEUI;
        rxNonce = jreq_ptr->DevNonce;
        // cmac = aes128_cmac(NwkKey, MHDR | JoinEUI | DevEUI | DevNonce)
        haveMicKey = get_sql_joinmote_key_("NwkKey", devEui64, MicKey);
        if (haveMicKey != 1) {
            printf("\e[31mjoin-mote not found devEui:%"PRIx64"\e[0m\n", devEui64);
            ret = UnknownDevEUI;
            goto jend;
        }
        JoinReqType = 0xff; 
    } else
        return Other;

/*    printf("devEui:");
    for (i = 0; i < 8; i++)
        printf("%02x ", jreq_ptr->DevEUI[i]);
    printf("\n");
    printf("joinEui:");
    for (i = 0; i < 8; i++)
        printf("%02x ", jreq_ptr->JoinEUI[i]);
    printf("\n");*/

    if (eui_buf_to_uint64(rfJoinEUI) != myJoinEui64) {
        printf("\e[31mwrong rx joinEui %"PRIx64" vs %"PRIx64"\e[0m\n", eui_buf_to_uint64(rfJoinEUI), myJoinEui64);
        ret = Other;
        goto jend;
    }

    /* no more return after this */
    if (haveMicKey) {
        uint32_t calculated_mic, rx_mic;
        print_buf(MicKey, LORA_CYPHERKEYBYTES, "\nMicKey");

        LoRa_GenerateJoinFrameIntegrityCode(false, MicKey, rxBuf, rxLen-LORA_FRAMEMICBYTES, (uint8_t*)&calculated_mic);

        rx_mic = rxBuf[rxLen-1] << 24;
        rx_mic += rxBuf[rxLen-2] << 16;
        rx_mic += rxBuf[rxLen-3] << 8;
        rx_mic += rxBuf[rxLen-4];
        if (rx_mic != calculated_mic) {
            printf("\e[31mmic fail %08x != %08x (rx_mic != calculated_mic)\e[0m\n", rx_mic, calculated_mic);
            ret = MICFailed;
            goto jend;
        }
    } else 
        printf("\e[33mnot checking MIC\e[0m\n");

    /****************************** send_join_complete()... **************/
    ja = (joinAccept_t *)uncyphered;

    i = get_sql_joinmote_key_("AppKey", devEui64, _AppKey);
    if (i < 0) {
        ret = JoinReqFailed;
        goto jend;
    }
    if (i == 1) {
        const char* nonceColumn = NULL;
        if (mhdr->bits.MType == MTYPE_REJOIN_REQ) {
            nonceColumn = "RJcount1_last";
        } else if (mhdr->bits.MType == MTYPE_JOIN_REQ) {
            nonceColumn = "DevNonce_last";
        }
        /* lorawan1v1 */
        MYSQL_ROW row;
        sprintf(query, "SELECT %s, joinNonce FROM joinmotes WHERE eui = %"PRIu64, nonceColumn, devEui64);
        SQL_PRINTF("js %s\n", query);
        if (mysql_query(sql_conn_lora_join, query)) {
            printf("\e[31mJS %s: %s\e[0m\n", query, mysql_error(sql_conn_lora_join));
            ret = JoinReqFailed;
            goto jend;
        }
        result = mysql_store_result(sql_conn_lora_join);
        //printf("eui %"PRIx64"\n", mote->dev_eui64);
        row = mysql_fetch_row(result);
        if (row) {
            unsigned n, nonce_last;
            if (row[0])
                sscanf(row[0], "%u", &nonce_last);
            else
                nonce_last = 0;
            sscanf(row[1], "%u", &n);
            printf("nonce_last:%u, joinNonce:%u\n", nonce_last, n);
            mysql_free_result(result);
            if (rxNonce <= nonce_last) {
                printf("\e[31mignoring joinReq due to nonce: %u <= %u\e[0m\n", rxNonce, nonce_last);
                ret = ActivationDisallowed;
                goto jend;
            }
            ja->fields._joinNonce = n;  // 24bit joinNonce
        } else {
            printf("\e[31mJS %016"PRIx64" no row\e[0m\n", devEui64);
            ret = JoinReqFailed;
            goto jend;
        }

        sprintf(query, "UPDATE joinmotes SET %s = %s + 1, joinNonce = joinNonce + 1 WHERE eui = %"PRIu64, nonceColumn, nonceColumn, devEui64);
        printf("js %s\n", query);

        if (mysql_query(sql_conn_lora_join, query)) {
            printf("\e[31m(JS update) %s: %s\e[0m\n", query, mysql_error(sql_conn_lora_join));
            ret = JoinReqFailed;
            goto jend;
        }
        if (mysql_affected_rows(sql_conn_lora_join) < 1) {
            printf("\e[31m%s\e[0m\n", query);
        }
        printf(" 1v1 ");
        OptNeg = true;
    } else {
        /* lorawan1v0 */
        if (check_nonce(devEui64, rxNonce)) {
            printf("\e[31mignoring join req due to devNonce\e[0m\n");
            ret = ActivationDisallowed;
            goto jend;
        }

        /* save devNonce from join request */
        sprintf(query, "INSERT INTO nonces (mote, nonce) VALUE (%"PRIu64", %d)", devEui64, rxNonce);
        SQL_PRINTF("js %s\n", query);
        if (mysql_query(sql_conn_lora_join, query)) {
            fprintf(stderr, "\e[31mJS (send_join) Error querying server: %s\e[0m\n", mysql_error(sql_conn_lora_join));
        }
        SQL_PRINTF("js nonces mysql_affected_rows:%u\n", (unsigned int)mysql_affected_rows(sql_conn_lora_join));
        printf(" 1v0 ");

        ja->fields._joinNonce = rand() & 0xffffff;  // 24bit appNonce

        OptNeg = false;
    }

    if (get_sql_joinmote_key_("NwkKey", devEui64, NwkKey) < 0) {
        ret = JoinReqFailed;
        goto jend;
    }

    /****************************** SendJoinComplete()... **************/
    ja->fields.mhdr = MTYPE_JOIN_ACC << 5;

    // join request is always sent from home NS
    ja->fields.Home_NetID = network_id;

    if (json_object_object_get_ex(inJobj, DevAddr, &obj)) {
        uint32_t devAddr;
        const char* str = json_object_get_string(obj);
        sscanf(str, "%x", &devAddr);
        printf(" DevAddr:%08x ", devAddr);
        ja->fields.DevAddr = devAddr;
    } else
        return MalformedRequest;

    if (json_object_object_get_ex(inJobj, DLSettings, &obj)) {
        uint32_t DLsettings;
        const char* str = json_object_get_string(obj);
        printf("got %s %s ", DLSettings, str);
        sscanf(str, "%x", &DLsettings);
        if (OptNeg)
            DLsettings |= 0x80; // 1v1
        else
            DLsettings &= ~0x80;    // 1v0

        ja->fields.DLSettings = DLsettings;
        printf("DLsettings:%02x ", DLsettings);
    } else
        return MalformedRequest;

    if (json_object_object_get_ex(inJobj, RxDelay, &obj)) {
        uint32_t rx_delay;
        const char* str = json_object_get_string(obj);
        printf("got %s %s ", RxDelay, str);
        sscanf(str, "%x", &rx_delay);
        ja->fields.RxDelay = rx_delay;
    } else
        return MalformedRequest;

    uint8_t* unptr = &uncyphered[sizeof(joinAccept_t)];
    if (json_object_object_get_ex(inJobj, CFList, &obj)) {
        const char* cflStr = json_object_get_string(obj);
        int cflLen = strlen(cflStr);
        printf("got %s: %s ", CFList, cflStr);
        if (cflLen > 32) {
            printf("\e[31mcflLen%u\e[0m\n", cflLen);
            ret = FrameSizeError;
            goto jend;
        }
        for (i = 0; i < cflLen; i += 2) {
            unsigned o;
            sscanf(cflStr, "%02x", &o);
            *unptr++ = o;
            cflStr += 2;
        }
    }


    if (OptNeg) {
        uint8_t JSIntKey[LORA_CYPHERKEYBYTES];
        uint8_t buf[LORA_MAXDATABYTES];
        uint8_t* ptr = buf;
        uint16_t authenticatedBytes = unptr - uncyphered;
        printf("1v1-mic ");
        /* MIC for join-accept answer and Rejoin-Request type 1 */
        //1v1: cmac = es128_cmac(JSIntKey, JoinReqType | JoinEUI | DevNonce | MHDR | JoinNonce | NetID | DevAddr | DLSettings | RxDelay | CFList | CFListType)
        if (get_sql_joinmote_key_("JSIntKey", devEui64, JSIntKey) < 0) {
            printf("no JSIntKey\n");
            ret = JoinReqFailed;
            goto jend;
        }
        print_buf(JSIntKey, 16, "JSIntKey");
        *ptr++ = JoinReqType;  // JoinReqType for join request (others are rejoinReq)
        printf("JoinReqType:%02x rfJoinEUI:%"PRIx64" rxNonce:%04x\n", JoinReqType, eui_buf_to_uint64(rfJoinEUI), rxNonce);
        memcpy(ptr, rfJoinEUI, LORA_EUI_LENGTH);
        ptr += LORA_EUI_LENGTH;
        ptr = Write2ByteValue(ptr, rxNonce);
        memcpy(ptr, uncyphered, authenticatedBytes);
        ptr += authenticatedBytes;
        print_buf(buf, ptr - buf, "jaMic-in");
        LoRa_GenerateJoinFrameIntegrityCode(false, JSIntKey, buf, ptr - &buf[0], unptr);
    } else {
        printf("1v0-mic ");
        //1v0: cmac = aes128_cmac(NwkKey,                                     MHDR | JoinNonce | NetID | DevAddr | DLSettings | RxDelay | CFList | CFListType)
        uint16_t authenticatedBytes = unptr - uncyphered;
        LoRa_GenerateJoinFrameIntegrityCode(false, NwkKey, uncyphered, authenticatedBytes, unptr);
        //print_buf(current, 4, "1v0 ja mic");
    }
    unptr += LORA_FRAMEMICBYTES;

    txBuf[0] = MTYPE_JOIN_ACC << 5; // MHDR
    printf("JoinReqType:%02x ", JoinReqType);
    print_buf(uncyphered, unptr - uncyphered, "uncyph");
    //encrypt
    uint16_t cypherBytes = (unptr - uncyphered) - LORA_MACHEADERLENGTH;
    if (OptNeg) {
        //1v1: aes128_decrypt(NwkKey or JSEncKey, JoinNonce | NetID | DevAddr | DLSettings | RxDelay | CFList | CFListType | MIC).
        //use JSEncKey when responding to Rejoin-Request
        //use NwkKey when responding to JoinRequest
        printf("1v1-cyph ");
        if (JoinReqType == 0xff) {
            CryptJoinServer(NwkKey, &uncyphered[LORA_MACHEADERLENGTH], cypherBytes, &txBuf[LORA_MACHEADERLENGTH]);
            print_buf(NwkKey, 16, "NwkKey");
        } else {
            uint8_t JSEncKey[LORA_CYPHERKEYBYTES];
            if (get_sql_joinmote_key_("JSEncKey", devEui64, JSEncKey) < 0) {
                printf("\e[31mno JSEncKey\e[0m\n");
                ret = JoinReqFailed;
                goto jend;
            }
            print_buf(JSEncKey, 16, "JSEncKey");
            CryptJoinServer(JSEncKey, &uncyphered[LORA_MACHEADERLENGTH], cypherBytes, &txBuf[LORA_MACHEADERLENGTH]);
        }
    } else {
        printf("1v0-cyph ");
        //1v0: aes128_decrypt(NwkKey            , JoinNonce | NetID | DevAddr | DLSettings | RxDelay | CFList | MIC)
        CryptJoinServer(NwkKey, &uncyphered[LORA_MACHEADERLENGTH], cypherBytes, &txBuf[LORA_MACHEADERLENGTH]);
        print_buf(NwkKey, 16, "NwkKey-1v0");
    }
    print_buf(txBuf, unptr - uncyphered, " txbuf");

    sprintf(query, "SELECT Lifetime FROM joinmotes WHERE eui = '%"PRIu64"'", devEui64);
    if (mysql_query(sql_conn_lora_join, query)) {
        fprintf(stderr, "\e[31mJS %s: %s\e[0m\n", query, mysql_error(sql_conn_lora_join));
        ret = JoinReqFailed;
        goto jend;
    }
    result = mysql_store_result(sql_conn_lora_join);
    if (result) {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row)
            sscanf(row[0], "%u", &lifetime);

        mysql_free_result(result);
    }

    /****** generate json answer ********/
    {
        char str[512];
        char* strPtr = str;
        json_object* obj;
        uint8_t FNwkSIntKey_bin[LORA_CYPHERKEYBYTES];
        uint8_t AppSKey_bin[LORA_CYPHERKEYBYTES];

        printf("joinNonce:%u\n", ja->fields._joinNonce);
        printf("rxNonce:%u\n", rxNonce);
        GenerateSessionKey(OptNeg, keyType_FNwkSIntKey, NwkKey, ja->fields._joinNonce, rfJoinEUI, rxNonce, network_id, FNwkSIntKey_bin);
        print_buf(FNwkSIntKey_bin, LORA_CYPHERKEYBYTES, "FNwkSIntKey");

        if (OptNeg) {
            /* LoRaWan 1.1 */
            uint8_t SNwkSIntKey_bin[LORA_CYPHERKEYBYTES];
            uint8_t NwkSEncKey_bin[LORA_CYPHERKEYBYTES];
            GenerateSessionKey(true, keyType_AppSKey, _AppKey, ja->fields._joinNonce, rfJoinEUI, rxNonce, network_id, AppSKey_bin);
            DEBUG_CRYPT_BUF(AppSKey_bin, LORA_CYPHERKEYBYTES, "AppSKey");
            GenerateSessionKey(true, keyType_SNwkSIntKey, NwkKey, ja->fields._joinNonce, rfJoinEUI, rxNonce, network_id, SNwkSIntKey_bin);
            DEBUG_CRYPT_BUF(SNwkSIntKey_bin, LORA_CYPHERKEYBYTES, "SNwkSIntKey");
            GenerateSessionKey(true, keyType_NwkSEncKey, NwkKey, ja->fields._joinNonce, rfJoinEUI, rxNonce, network_id, NwkSEncKey_bin);
            DEBUG_CRYPT_BUF(NwkSEncKey_bin, LORA_CYPHERKEYBYTES, "NwkSEncKey");

            obj = create_KeyEnvelope(SNwkSIntKey_bin, &key_envelope_nwk);
            if (obj == NULL) {
                ret = JoinReqFailed;
                goto jend;
            }
            json_object_object_add(*ansJobj, SNwkSIntKey, obj);

            obj = create_KeyEnvelope(FNwkSIntKey_bin, &key_envelope_nwk);
            if (obj == NULL) {
                ret = JoinReqFailed;
                goto jend;
            }
            json_object_object_add(*ansJobj, FNwkSIntKey, obj);

            obj = create_KeyEnvelope(NwkSEncKey_bin, &key_envelope_nwk);
            if (obj == NULL) {
                ret = JoinReqFailed;
                goto jend;
            }
            json_object_object_add(*ansJobj, NwkSEncKey, obj);
        } else {
            /* LoRaWan 1.0.x */
            GenerateSessionKey(false, keyType_AppSKey, NwkKey, ja->fields._joinNonce, rfJoinEUI, rxNonce, network_id, AppSKey_bin);
            obj = create_KeyEnvelope(FNwkSIntKey_bin, &key_envelope_nwk);
            if (obj == NULL) {
                ret = JoinReqFailed;
                goto jend;
            }
            json_object_object_add(*ansJobj, NwkSKey, obj);
        }

        obj = create_KeyEnvelope(AppSKey_bin, &key_envelope_app);
        if (obj == NULL) {
            ret = JoinReqFailed;
            goto jend;
        }
        json_object_object_add(*ansJobj, AppSKey, obj);

        for (i = 0; i < (unptr - uncyphered); i++) {
            sprintf(strPtr, "%02x", txBuf[i]);
            strPtr += 2;
        }
        json_object_object_add(*ansJobj, PHYPayload, json_object_new_string(str));

        json_object_object_add(*ansJobj, Lifetime, json_object_new_int(lifetime));

        //json_object_object_add(*ansJobj, SessionKeyID, json_object_new_string("TODO"));
    }

jend:
    return ret;
} // ..parse_rf_join_req()

int js_conf_json(json_object *jobjSrv, conf_t* c)
{
    json_object *obj;

    if (json_object_object_get_ex(jobjSrv, "myJoinEui", &obj)) {
        sscanf(json_object_get_string(obj), "%"PRIx64, &myJoinEui64);
        sprintf(myJoinEuiStr, "%016"PRIx64, myJoinEui64);
    } else {
        printf("no myJoinEui\n");
        return -1;
    }

    if (parse_json_KeyEnvelope("KeyEnvelopeApp", jobjSrv, &key_envelope_app) < 0)
        return -1;
    if (parse_json_KeyEnvelope("KeyEnvelopeNwk", jobjSrv, &key_envelope_nwk) < 0)
        return -1;

    return 0;
}

int sessionCreate(struct Session* s) { return 0; }
void sessionEnd(struct Session* s) { }

struct MHD_Daemon *
init(const char* conf_filename)
{
    struct MHD_Daemon *ret;
    conf_t c;
    const char js_db_name[] = "lora_join";

    if (parse_server_config(conf_filename, js_conf_json, &c) < 0) {
        return NULL;
    }
    strcpy(netIdDomain, c.netIdDomain);

    if (database_open(c.sql_hostname, c.sql_username, c.sql_password, c.sql_port, js_db_name, &sql_conn_lora_join, JS_VERSION) < 0)
        return NULL;

    ret = MHD_start_daemon (MHD_USE_ERROR_LOG,
        c.httpd_port, /* unsigned short port*/
        NULL, NULL, /* MHD_AcceptPolicyCallback apc, void *apc_cls */
        &lib_create_response, NULL, /* MHD_AccessHandlerCallback dh, void *dh_cls */
        MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 15,
        MHD_OPTION_NOTIFY_COMPLETED, &lib_request_completed_callback, NULL,
        MHD_OPTION_END
    );
    if (ret != NULL)
        printf("httpd port %u\n", c.httpd_port);

    srand(time(NULL));

    return ret;
}

// TODO old nonces                join_server_once_daily();
int
main (int argc, char **argv)
{
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

    strcpy(conf_filename, "../join_server/conf.json");  // default conf file

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

    while (1) {
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
    }
    MHD_stop_daemon (d);

    return 0;
}

