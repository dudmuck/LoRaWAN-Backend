/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "libserver.h"

const char EU868[] = "EU868";
const char US902[] = "US902";
const char US902A[] = "US902A";
const char US902B[] = "US902B";
const char US902C[] = "US902C";
const char US902D[] = "US902D";
const char US902E[] = "US902E";
const char US902F[] = "US902F";
const char US902G[] = "US902G";
const char US902H[] = "US902H";
const char China470[] = "China470";
const char China779[] = "China779";
const char EU433[] = "EU433";
const char Australia915[] = "Australia915";
const char AS923[] = "AS923";
const char KR922[] = "KR922";
const char IN865[] = "IN865";
const char RU868[] = "RU868";

const char MalformedRequest[] = "MalformedRequest";
const char InvalidProtocolVersion[] = "InvalidProtocolVersion";
const char UnknownReceiverID[] = "UnknownReceiverID";

static const char Result[] = "Result";
static const char ProtocolVersion[] = "ProtocolVersion";
static const char SenderID[] = "SenderID";
static const char ReceiverID[] = "ReceiverID";
static const char TransactionID[] = "TransactionID";
static const char MessageType[] = "MessageType";
const char MACVersion[] = "MACVersion";
const char PHYPayload[] = "PHYPayload";
const char FRMPayload[] = "FRMPayload";
const char DevEUI[] = "DevEUI";

const char JoinReq[] = "JoinReq";
const char JoinAns[] = "JoinAns";
const char RejoinReq[] = "RejoinReq";
const char RejoinAns[] = "RejoinAns";
const char XmitDataReq[] = "XmitDataReq";
const char XmitDataAns[] = "XmitDataAns";
const char PRStartReq[] = "PRStartReq";
const char PRStartAns[] = "PRStartAns";
const char PRStopReq[] = "PRStopReq";
const char PRStopAns[] = "PRStopAns";
const char HRStartReq[] = "HRStartReq";
const char HRStartAns[] = "HRStartAns";
const char HRStopReq[] = "HRStopReq";
const char HRStopAns[] = "HRStopAns";
const char HomeNSReq[] = "HomeNSReq";
const char HomeNSAns[] = "HomeNSAns";
const char ProfileReq[] = "ProfileReq";
const char ProfileAns[] = "ProfileAns";
const char AppSKeyReq[] = "AppSKeyReq";
const char AppSKeyAns[] = "AppSKeyAns";

const char FrameSizeError[] = "FrameSizeError";
const char UnknownDevEUI[] = "UnknownDevEUI";
const char UnknownDevAddr[] = "UnknownDevAddr";
const char MICFailed[] = "MICFailed";
const char Success[] = "Success";
const char ActivationDisallowed[] = "ActivationDisallowed";
const char JoinReqFailed[] = "JoinReqFailed";
const char NoRoamingAgreement[] = "NoRoamingAgreement";
const char RoamingActDisallowed[] = "RoamingActDisallowed";
const char Deferred[] = "Deferred";
const char StaleDeviceProfile[] = "StaleDeviceProfile";
const char XmitFailed[] = "XmitFailed";
const char InvalidFPort[] = "InvalidFPort";

const char DevAddr[] = "DevAddr";
const char DLSettings[] = "DLSettings";
const char RxDelay[] = "RxDelay";
const char CFList[] = "CFList";
const char Lifetime[] = "Lifetime";
const char SNwkSIntKey[] = "SNwkSIntKey";
const char AESKey[] = "AESKey";
const char Other[] = "Other";
const char KEKLabel[] = "KEKLabel";
const char FNwkSIntKey[] = "FNwkSIntKey";
const char NwkSEncKey[] = "NwkSEncKey";
const char NwkSKey[] = "NwkSKey";
const char AppSKey[] = "AppSKey";
const char SupportsJoin[] = "SupportsJoin";

const char DeviceProfile[] = "DeviceProfile";
const char SupportsClassB[] = "SupportsClassB";
const char ClassBTimeout[] = "ClassBTimeout";
const char MaxEIRP[] = "MaxEIRP";
const char PingSlotPeriod[] = "PingSlotPeriod";
const char PingSlotDR[] = "PingSlotDR";
const char PingSlotFreq[] = "PingSlotFreq";
const char SupportsClassC[] = "SupportsClassC";
const char ClassCTimeout[] = "ClassCTimeout";
const char RegParamsRevision[] = "RegParamsRevision";
const char RXDelay1[] = "RXDelay1";
const char RXDROffset1[] = "RXDROffset1";
const char RXDataRate2[] = "RXDataRate2";
const char RXFreq2[] = "RXFreq2";
const char FactoryPresetFreqs[] = "FactoryPresetFreqs";
const char MaxDutyCycle[] = "MaxDutyCycle";
const char RFRegion[] = "RFRegion";
const char Supports32bitFCnt[] = "Supports32bitFCnt";
const char DeviceProfileTimestamp[] = "DeviceProfileTimestamp";

const char ServiceProfileID[] = "ServiceProfileID";
const char ServiceProfile[] = "ServiceProfile";
const char ULRate[] = "ULRate";
const char ULBucketSize[] = "ULBucketSize";
const char ULRatePolicy[] = "ULRatePolicy";
const char DLRate[] = "DLRate";
const char DLBucketSize[] = "DLBucketSize";
const char DLRatePolicy[] = "DLRatePolicy";
const char AddGWMetadata[] = "AddGWMetadata";
const char DevStatusReqFreq[] = "DevStatusReqFreq";
const char ReportDevStatusBattery[] = "ReportDevStatusBattery";
const char ReportDevStatusMargin[] = "ReportDevStatusMargin";
const char DRMin[] = "DRMin";
const char DRMax[] = "DRMax";
const char ChannelMask[] = "ChannelMask";
const char PRAllowed[] = "PRAllowed";
const char HRAllowed[] = "HRAllowed";
const char RAAllowed[] = "RAAllowed";
const char NwkGeoLoc[] = "NwkGeoLoc";
const char TargetPER[] = "TargetPER";
const char MinGWDiversity[] = "MinGWDiversity";
const char AS_ID[] = "AS-ID";
const char ULMetaData[] = "ULMetaData";
const char FPort[] = "FPort";
const char Margin[] = "Margin";
const char Battery[] = "Battery";
const char FCntDown[] = "FCntDown";
const char FCntUp[] = "FCntUp";
const char ULFreq[] = "ULFreq";
const char DataRate[] = "DataRate";
const char RecvTime[] = "RecvTime";
const char GWInfo[] = "GWInfo";
const char Confirmed[] = "Confirmed";
const char GWCnt[] = "GWCnt";

//const char GWInfoElement[] = "GWInfoElement";
const char ID[] = "ID";
const char RSSI[] = "RSSI";
const char SNR[] = "SNR";
const char Lat[] = "Lat";
const char Lon[] = "Lon";
const char ULToken[] = "ULToken";
const char DLAllowed[] = "DLAllowed";

const char RoamingActivationType[] = "RoamingActivationType";
const char Passive[] = "Passive";
const char Handover[] = "Handover";
const char HNetID[] = "HNetID";


const char VSExtension[] = "VSExtension";
const char VendorID[] = "VendorID";
const char Object[] = "Object";

const char seconds_at_beacon[] = "seconds_at_beacon";
const char tstamp_at_beacon[] = "tstamp_at_beacon";
const char lgw_trigcnt_at_next_beacon[] = "lgw_trigcnt_at_next_beacon";
const char beacon_ch[] = "beacon_ch";

const char DLFreq1[] = "DLFreq1";
const char DLFreq2[] = "DLFreq2";
const char ClassMode[] = "ClassMode";
const char DataRate1[] = "DataRate1";
const char DataRate2[] = "DataRate2";
const char FNSULToken[] = "FNSULToken";
const char HiPriorityFlag[] = "HiPriorityFlag";
const char PingPeriod[] = "PingPeriod";
const char DLMetaData[] = "DLMetaData";


/** \brief add 32bit integer to buffer, little endian
 * @return pointer to end of buffer
 */
uint8_t* Write4ByteValue(uint8_t output[], uint32_t input)
{
    uint8_t* ptr = output;

    *(ptr++) = (uint8_t)input,
    input >>= 8;
    *(ptr++) = (uint8_t)input,
    input >>= 8;
    *(ptr++) = (uint8_t)input;
    input >>= 8;
    *(ptr++) = (uint8_t)input;

    return ptr;
}

/** \brief add 24bit integer to buffer, little endian
 * @return pointer to end of buffer
 */
uint8_t* Write3ByteValue(uint8_t output[], uint32_t input)
{
    uint8_t* ptr = output;

    *(ptr++) = (uint8_t)input,
    input >>= 8;
    *(ptr++) = (uint8_t)input,
    input >>= 8;
    *(ptr++) = (uint8_t)input;

    return ptr;
}

/** \brief add 16bit integer to buffer, little endian
 * @return pointer to end of buffer
 */
uint8_t* Write2ByteValue(uint8_t output[], uint32_t input)
{
    uint8_t* ptr = output;

    *(ptr++) = (uint8_t)input,
    input >>= 8;
    *(ptr++) = (uint8_t)input;

    return ptr;
}

/** \brief add 8bit integer to buffer
 * @return pointer to end of buffer
 */
uint8_t* Write1ByteValue(uint8_t output[], uint32_t input)
{
    uint8_t* ptr = output;

    *(ptr++) = (uint8_t)input;

    return ptr;
}

// ans_mt NULL when parsing answer, ans_mt set to ans message type when parsing req
/* parse always-mandatory json items
 * return: Result of parsing */
const char*
lib_parse_json(json_object *jobj, const char** ans_mt, const char** pmt, char* senderID, const char* myID, unsigned long* tid, const char** receivedResult)
{
    json_object *obj;
    const char* ret = Success;

    *ans_mt = NULL;

    if (json_object_object_get_ex(jobj, MessageType, &obj)) {
        const char* str = json_object_get_string(obj);
        if (strcmp(str, JoinReq) == 0) {
            *ans_mt = JoinAns;
            *pmt = JoinReq;
        } else if (strcmp(str, XmitDataReq) == 0) {
            *ans_mt = XmitDataAns;
            *pmt = XmitDataReq;
        } else if (strcmp(str, PRStartReq) == 0) {
            *ans_mt = PRStartAns;
            *pmt = PRStartReq;
        } else if (strcmp(str, JoinAns) == 0)
            *pmt = JoinAns;
        else if (strcmp(str, RejoinReq) == 0) {
            *ans_mt = RejoinAns;
            *pmt = RejoinReq;
        } else if (strcmp(str, RejoinAns) == 0)
            *pmt = RejoinAns;
        else if (strcmp(str, HRStartReq) == 0) {
            *ans_mt = HRStartAns;
            *pmt = HRStartReq;
        } else if (strcmp(str, HRStartAns) == 0)
            *pmt = HRStartAns;
        else if (strcmp(str, XmitDataAns) == 0)
            *pmt = XmitDataAns;
        else if (strcmp(str, PRStartAns) == 0)
            *pmt = PRStartAns;
        else if (strcmp(str, HomeNSReq) == 0) {
            *ans_mt = HomeNSAns;
            *pmt = HomeNSReq;
        } else if (strcmp(str, HomeNSAns) == 0)
            *pmt = HomeNSAns;
        else if (strcmp(str, ProfileReq) == 0) {
            *ans_mt = ProfileAns;
            *pmt = ProfileReq;
        } else if (strcmp(str, ProfileAns) == 0)
            *pmt = ProfileAns;
        else if (strcmp(str, AppSKeyReq) == 0) {
            *ans_mt = AppSKeyAns;
            *pmt = AppSKeyReq;
        } else if (strcmp(str, AppSKeyAns) == 0)
            *pmt = AppSKeyAns;
        else if (strcmp(str, PRStopReq) == 0) {
            *ans_mt = PRStopAns;
            *pmt = PRStopReq;
        } else if (strcmp(str, PRStopAns) == 0)
            *pmt = PRStopAns;
        else if (strcmp(str, HRStopReq) == 0) {
            *ans_mt = HRStopAns;
            *pmt = HRStopReq;
        } else if (strcmp(str, HRStopAns) == 0)
            *pmt = HRStopAns;
        else {
            printf("lib_parse_json() unknown %s %s\n", MessageType, str);
            ret = MalformedRequest;
            *pmt = Other;
        }
    } else  {
        printf("lib_parse_json() missing %s\n", MessageType);
        ret = MalformedRequest;
    }

    if (json_object_object_get_ex(jobj, ProtocolVersion, &obj)) {
        //printf("got %s %s\n", ProtocolVersion, json_object_get_string(obj));
        if (strcmp(json_object_get_string(obj), "1.0") != 0) {
            printf("bad %s %s\n", ProtocolVersion, json_object_get_string(obj));
            ret = InvalidProtocolVersion;
        }
    } else {
        printf("lib_parse_json() missing %s\n", ProtocolVersion);
        ret = MalformedRequest;
    }

    if (json_object_object_get_ex(jobj, SenderID, &obj)) {
        strcpy(senderID, json_object_get_string(obj));
    } else {
        printf("lib_parse_json() missing %s\n", SenderID);
        ret = MalformedRequest;
    }

    if (json_object_object_get_ex(jobj, ReceiverID, &obj)) {
        uint8_t out[64];
        const char* rxID = json_object_get_string(obj);
        //printf("%s ", ReceiverID);
        if (ascii_hex_to_buf(myID, out) == 0) {
            /* integer comparision */
            //printf("hex ");
            if (strlen(myID) > 8) {
                /* 64bit integer comparision */
                uint64_t my64, rx64;
                sscanf(myID, "%"PRIx64, &my64);
                sscanf(rxID, "%"PRIx64, &rx64);
                //printf("64 ");
                if (my64 != rx64)
                    ret = UnknownReceiverID;
            } else {
                /* 32bit integer comparision */
                uint32_t my32, rx32;
                sscanf(myID, "%x", &my32);
                sscanf(rxID, "%x", &rx32);
                //printf("32 ");
                if (my32 != rx32)
                    ret = UnknownReceiverID;
            }
        } else {
            //printf("ascii \"%s\" \"%s\"", rxID, myID);
            /* ascii comparision */
            if (strcasecmp(rxID, myID) != 0)
                ret = UnknownReceiverID;
        }
        //printf("\n");
    } else {
        printf("lib_parse_json() missing %s\n", ReceiverID);
        ret = MalformedRequest;
    }

    if (json_object_object_get_ex(jobj, TransactionID, &obj)) {
#if 0
        const char* str = json_object_get_string(obj);
        *tid = strtol(str, NULL, 0);
        //printf("got %s %d -> %ld\n", TransactionID, json_object_get_int(obj), trans_id);
#endif /* if 0 */
        *tid = json_object_get_int(obj);
    } else {
        printf("lib_parse_json() missing %s\n", TransactionID);
        ret = MalformedRequest;
    }

    /* Result should only be seen if this is Ans MessageType */
    if (receivedResult != NULL) {
        if (json_object_object_get_ex(jobj, Result, &obj)) {
            const char* str = json_object_get_string(obj);

            if (strcmp(str, FrameSizeError) == 0)
                *receivedResult = FrameSizeError;
            else if (strcmp(str, StaleDeviceProfile) == 0)
                *receivedResult = StaleDeviceProfile;
            else if (strcmp(str, MalformedRequest) == 0)
                *receivedResult = MalformedRequest;
            else if (strcmp(str, UnknownDevEUI) == 0)
                *receivedResult = UnknownDevEUI;
            else if (strcmp(str, UnknownDevAddr) == 0)
                *receivedResult = UnknownDevAddr;
            else if (strcmp(str, MICFailed) == 0)
                *receivedResult = MICFailed;
            else if (strcmp(str, Success) == 0)
                *receivedResult = Success;
            else if (strcmp(str, ActivationDisallowed) == 0)
                *receivedResult = ActivationDisallowed;
            else if (strcmp(str, JoinReqFailed) == 0)
                *receivedResult = JoinReqFailed;
            else if (strcmp(str, NoRoamingAgreement) == 0)
                *receivedResult = NoRoamingAgreement;
            else if (strcmp(str, RoamingActDisallowed) == 0)
                *receivedResult = RoamingActDisallowed;
            else if (strcmp(str, Deferred) == 0)
                *receivedResult = Deferred;
            else if (strcmp(str, InvalidFPort) == 0)
                *receivedResult = InvalidFPort;
            else if (strcmp(str, XmitFailed) == 0)
                *receivedResult = XmitFailed;
            else if (strcmp(str, Other) == 0)
                *receivedResult = Other;
            else {
                /* if this was answer, we need (recognized) result */
                if (*ans_mt == NULL)
                    printf("\e[31mlib_parse_json unknown result \"%s\"\n", str);
                *receivedResult = NULL;
            }

        } else {
            /* if this was answer, we need result */
            if (*ans_mt == NULL)
                printf("\e[31mlib_parse_json no result\e[0m\n");
            *receivedResult = NULL;
        }
    } // ..if (receivedResult != NULL)

    return ret;
} /* ...lib_parse_json() */

/* generate always-mandatory json items 
 * paramresult: NULL if mt is a Req */
void lib_generate_json(json_object *jobj, const char* destID, const char* srcID, unsigned long tid, const char* mt, const char* result)
{
    if (mt == NULL) {
        printf("\e[31mlib_generate_json() NULL MessageType\e[0m\n");
        return;
    }

    json_object_object_add(jobj, ProtocolVersion, json_object_new_string("1.0"));
    json_object_object_add(jobj, ReceiverID, json_object_new_string(destID));
    json_object_object_add(jobj, SenderID, json_object_new_string(srcID));
    json_object_object_add(jobj, TransactionID, json_object_new_int(tid));
    json_object_object_add(jobj, MessageType, json_object_new_string(mt));

    if (result)
        json_object_object_add(jobj, Result, json_object_new_string(result));

}

void
json_print_type(json_type t)
{
    switch (t) {
        case json_type_null: printf("null"); break;
        case json_type_boolean: printf("bool"); break;
        case json_type_double: printf("double"); break;
        case json_type_int: printf("int"); break;
        case json_type_object: printf("object"); break;
        case json_type_array: printf("array"); break;
        case json_type_string: printf("string"); break;
    }
}

int get_int(char c)
{
    if (c >= '0' && c <= '9') return      c - '0';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    return -1;
}

uint64_t
eui_buf_to_uint64(const uint8_t* eui)
{
    uint64_t ret = 0;
    int i;

    /* most significant last over air */
    for (i = LORA_EUI_LENGTH - 1; i >= 0; i--) {
        ret <<= 8;
        ret |= eui[i];
    }
    return ret;
}

void
uint64_to_eui_buf(uint64_t in, uint8_t* out)
{
    unsigned i;

    /* least significant first over air */
    for (i = 0; i < LORA_EUI_LENGTH; i++) {
        *out = in & 0xff;
        in >>= 8;
        out++;
    }
}

int ascii_hex_to_buf(const char* in_str, uint8_t* output)
{
    size_t i;
    size_t size = strlen(in_str) / 2;

    for (i = 0; i < size; ++i) {
        int hi = get_int(in_str[2*i]);
        int lo = get_int(in_str[2*i+1]);
        if (hi < 0 || lo < 0)
            return -1;
        //output[i] = get_int(in_str[2*i]) * 16 + get_int(in_str[2*i+1]);
        output[i] = hi * 16 + lo;
    }

    /*for (i = 0; i < size; ++i)
        printf("%x ", output[i]);
    printf("\n");*/
    return 0;
}

static int
set_config(MYSQL** db_ptr, const char* const field_name, const char* const value_buf)
{
    char query[512];

    sprintf(query, "REPLACE INTO configuration SET name = '%s', value = '%s'", field_name, value_buf);
    SQL_PRINTF("(c) %s\n", query);
    if (mysql_query(*db_ptr, query)) {
        fprintf(stderr, "Error set jsonUdpSocket: %s\n", mysql_error(*db_ptr));
        return -1;
    }
    return 0;
}

static int
query_config(MYSQL** db_ptr, const char* const field_name, char* value_buf, ssize_t sizeof_value_buf)
{
    char query[512];
    MYSQL_RES *result;
    MYSQL_ROW row;

    sprintf(query, "SELECT value FROM configuration WHERE name = '%s'", field_name);
    if (mysql_query(*db_ptr, query)) {
        fprintf(stderr, "(query_config) Error querying server: %s\n", mysql_error(*db_ptr));
        return -1;
    }

    result = mysql_use_result(*db_ptr);
    if (result == NULL) {
        printf("No result.\n");
        return -1;
    }

    //num_fields = mysql_num_fields(result);
    while ((row = mysql_fetch_row(result))) {
        unsigned long *field_lengths = mysql_fetch_lengths(result);
        if (field_lengths == NULL) {
            fprintf(stderr, "Failed to get field lengths: %s\n", mysql_error(*db_ptr));
            return -1;
        }
        /*for (int i = 0; i < num_fields; i++) {
            printf("[%d:%lu:%s] ", i, field_lengths[i], row[i]);
        }*/
        //sscanf(row[0], "%u", &json_udp_port);
        strncpy(value_buf, row[0], sizeof_value_buf);
    }
    mysql_free_result(result);

    return 0;
}

/** @brief open MYSQL database and open UDP socket
 * @param dbname name of database to open
 * @param db_ptr pointer to MYSQL connection
 * @param default_udp_port default UDP port number to use if none exists in database
 * @param buildVersion version string written to configuration
 * @return -1 for failure, or UDP socket file descriptor
 */
int
database_open(const char* dbhostname, const char* dbuser, const char* dbpass, uint16_t dbport, const char* const dbname, MYSQL** db_ptr, const char* const buildVersion)
{
    time_t t = time(NULL);
    struct tm* info = localtime(&t);
    char time_buf[32];
    char date_buf[32];
    char str[32];

    *db_ptr = mysql_init(NULL);
    if (*db_ptr == NULL) {
        fprintf(stderr, "Failed to initialize: %s\n", mysql_error(*db_ptr));
        return -1;
    }

    /* enable re-connect */
    my_bool reconnect = 1;
    if (mysql_options(*db_ptr , MYSQL_OPT_RECONNECT, &reconnect) != 0) {
        fprintf(stderr, "mysql_options() failed\n");
        return -1;
    }

    printf("database connect %s\n", dbname);
    /* Connect to the server */
    if (!mysql_real_connect(*db_ptr, dbhostname, dbuser, dbpass, dbname, dbport, NULL, 0))
    {
        fprintf(stderr, "Failed to connect to server: %s\n", mysql_error(*db_ptr));
        return -1;
    }

    if (query_config(db_ptr, "buildDate", str, sizeof(str)) < 0)
        return -1;

    if (strcmp(str, __DATE__) != 0)
        set_config(db_ptr, "buildDate", __DATE__);

    if (query_config(db_ptr, "buildTime", str, sizeof(str)) < 0)
        return -1;

    if (strcmp(str, __TIME__) != 0)
        set_config(db_ptr, "buildTime", __TIME__);


    if (query_config(db_ptr, "buildVersion", str, sizeof(str)) < 0)
        return -1;

    if (strcmp(str, buildVersion) != 0)
        set_config(db_ptr, "buildVersion", buildVersion);


    if (query_config(db_ptr, "startDate", str, sizeof(str)) < 0)
        return -1;

    strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", info);
    if (strcmp(str, date_buf) != 0)
        set_config(db_ptr, "startDate", date_buf);

    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", info);
    set_config(db_ptr, "startTime", time_buf);

    return 0;
}

const char*
parse_key_envelope(json_object* obj, const key_envelope_t* ke, uint8_t* keyOut)
{
    int i, algo = GCRY_CIPHER_AES128;  // could also be AES192 or AES256
    json_object* o;
    const char* rxLabel;
    uint8_t encryptedBin[LORA_KEYENVELOPE_LEN];
    uint8_t encryptedBinLen;
    const char* inStr;

    if (!json_object_object_get_ex(obj, AESKey, &o)) {
        printf("\e[31mmissing %s\e[0m\n", AESKey);
        return MalformedRequest;
    }
    inStr = json_object_get_string(o);
    encryptedBinLen = strlen(inStr) / 2;
    if (encryptedBinLen > sizeof(encryptedBin)) {
        printf("\e[31mencryptedBinLen size %u > %zu\e[0m\n", encryptedBinLen, sizeof(encryptedBin));
        return Other;
    }
    for (i = 0; i < encryptedBinLen; i++) {
        unsigned o;
        sscanf(inStr, "%02x", &o);
        encryptedBin[i] = o;
        inStr += 2;
    }

    if (json_object_object_get_ex(obj, KEKLabel, &o)) {
        gcry_error_t err;
        gcry_cipher_hd_t hd;
        if (ke->kek_label == NULL) {
            printf("\e[31mreceived envelope has %s, but not us\n", KEKLabel);
            return Other;
        }
        rxLabel = json_object_get_string(o);
        if (strcmp(rxLabel, ke->kek_label) != 0) {
            printf("\e[31m%s mismatch: %s vs %s\n", KEKLabel, rxLabel, ke->kek_label);
            return Other;
        }

        err = gcry_cipher_open(&hd, algo, GCRY_CIPHER_MODE_AESWRAP, 0);
        if (err) {
            printf("gcryp_cipher_open() failed: %s\n", gpg_strerror(err));
            return Other;
        }

        err = gcry_cipher_setkey(hd, ke->key_bin, ke->key_len);
        if (err) {
            printf("gcry_cipher_setkey() failed: %s, ken_len%u\n", gpg_strerror(err), ke->key_len);
            return Other;
        }

        err = gcry_cipher_decrypt(hd, keyOut, LORA_CYPHERKEYBYTES, encryptedBin, encryptedBinLen);
        if (err) {
            printf("gcry_cipher_decrypt() failed: %s\n", gpg_strerror(err));
            return Other;
        }
        gcry_cipher_close(hd);
        /*printf("keyOut:");
        for (i = 0; i < LORA_CYPHERKEYBYTES; i++)
            printf("%02x ", keyOut[i]);
        printf("\n");*/
    } // ..if received KEKLabel
    else
        memcpy(keyOut, encryptedBin, LORA_CYPHERKEYBYTES);

    return Success;
}

json_object*
create_KeyEnvelope(const uint8_t* key_bin, const key_envelope_t* ke)
{
    char str[512];
    char* strPtr = str;
    json_object* obj;
    gcry_error_t err;
    gcry_cipher_hd_t hd;
    uint8_t encrypted[LORA_KEYENVELOPE_LEN];
    const uint8_t* key_out;
    uint8_t key_out_len;
    int i, algo = GCRY_CIPHER_AES128;  // could also be AES192 or AES256

    obj = json_object_new_object();

    if (ke->kek_label != NULL) {
        err = gcry_cipher_open(&hd, algo, GCRY_CIPHER_MODE_AESWRAP, 0);
        if (err) {
            printf("gcryp_cipher_open() failed: %s\n", gpg_strerror(err));
            return NULL;
        }

        err = gcry_cipher_setkey(hd, ke->key_bin, ke->key_len);
        if (err) {
            printf("gcry_cipher_setkey() failed: %s, key_len%u\n", gpg_strerror(err), ke->key_len);
            return NULL;
        }

        err = gcry_cipher_encrypt(hd, encrypted, sizeof(encrypted), key_bin, LORA_CYPHERKEYBYTES);
        if (err) {
            printf("gcry_cipher_setkey() failed: %s\n", gpg_strerror(err));
            return NULL;
        }
        gcry_cipher_close(hd);

        json_object_object_add(obj, KEKLabel, json_object_new_string(ke->kek_label));

        key_out = encrypted;
        key_out_len = sizeof(encrypted);
    } else {
        key_out_len = LORA_CYPHERKEYBYTES;
        key_out = key_bin;
    }

    strPtr = str;
    for (i = 0; i < key_out_len; i++) {
        sprintf(strPtr, "%02x", key_out[i]);
        strPtr += 2;
    }
    json_object_object_add(obj, AESKey, json_object_new_string(str));

    return obj;
}

int
parse_json_KeyEnvelope(const char* name, json_object* jobjSrv, key_envelope_t* k)
{
    json_object *obj;

    if (json_object_object_get_ex(jobjSrv, name, &obj)) {
        int i;
        json_object* ko;
        if (!json_object_object_get_ex(obj, KEKLabel, &ko)) {
            printf("json conf KeyEnvelope has no %s\n", KEKLabel);
            return -1;
        }
        k->kek_label = malloc(strlen(json_object_get_string(ko))+1);
        strcpy(k->kek_label, json_object_get_string(ko));
        if (!json_object_object_get_ex(obj, AESKey, &ko)) {
            printf("json conf KeyEnvelope has no %s\n", AESKey);
            return -1;
        }
        const char* keyStr = json_object_get_string(ko);
        k->key_len = strlen(keyStr) / 2;
        if (k->key_len != LORA_CYPHERKEYBYTES) {
            printf("%s keylen must be %u\n", k->kek_label, LORA_CYPHERKEYBYTES);
            return -1;
        }
        k->key_bin = malloc(k->key_len);
        printf("kek_label:%s -> ", k->kek_label);
        for (i = 0; i < k->key_len; i++) {
            unsigned o;
            sscanf(keyStr, "%02x", &o);
            k->key_bin[i] = o;
            printf("%02x ", k->key_bin[i]);
            keyStr += 2;
        }
        printf("\n");
    }

    return 0;
}

struct _host_list* host_list = NULL; /**< list of hosts */

int
parse_server_config(const char* conf_file, int (*conf_callback)(json_object*, conf_t*), conf_t* out)
{
    bool fail = false;
    json_object *jobj, *jobjSrv, *obj;
    int len;
    int ret = -1;
    enum json_tokener_error jerr;
    struct json_tokener *tok = json_tokener_new();
    FILE *file = fopen(conf_file, "r");
    if (file == NULL) {
        perror(conf_file);
        return -1;
    }

    do {
        char line[96];
        if (fgets(line, sizeof(line), file) == NULL) {
            fprintf(stderr, "NULL == fgets()\n");
            goto pEnd;
        }
        len = strlen(line);
        jobj = json_tokener_parse_ex(tok, line, len);
        //printf("jobj:%p, len%u\n", jobj, len);
    } while ((jerr = json_tokener_get_error(tok)) == json_tokener_continue);

    if (jerr != json_tokener_success) {
        printf("parse_server_config() json error: %s\n", json_tokener_error_desc(jerr));
        goto pEnd;
    }
    if (tok->char_offset < len) {
        printf("json extra chars\n");
    }
    /***********************/


    if (!json_object_object_get_ex(jobj, "server", &jobjSrv)) {
        printf("json conf no server\n");
        goto pEnd;
    }
    /************ have server object ***********/


    if (json_object_object_get_ex(jobjSrv, "sql_username", &obj)) {
        //json_print_type(json_object_get_type(obj));
        strncpy(out->sql_username, json_object_get_string(obj), sizeof(out->sql_username));
    } else {
        fail = true;
        printf("no sql_username\n");
    }

    if (json_object_object_get_ex(jobjSrv, "sql_password", &obj)) {
        strncpy(out->sql_password, json_object_get_string(obj), sizeof(out->sql_password));
    } else {
        fail = true;
        printf("no sql_password\n");
    }

    if (json_object_object_get_ex(jobjSrv, "sql_hostname", &obj)) {
        strncpy(out->sql_hostname, json_object_get_string(obj), sizeof(out->sql_hostname));
    } else {
        fail = true;
        printf("no sql_hostname\n");
    }

    if (json_object_object_get_ex(jobjSrv, "sql_port", &obj)) {
        out->sql_port = json_object_get_int(obj);
    } else {
        fail = true;
        printf("no sql_port\n");
    }

    if (json_object_object_get_ex(jobjSrv, "httpd_port", &obj)) {
        out->httpd_port = json_object_get_int(obj);
    } else {
        fail = true;
        printf("no httpd_port \n");
    }

    if (json_object_object_get_ex(jobjSrv, "joinDomain", &obj)) {
        strncpy(out->joinDomain, json_object_get_string(obj), sizeof(out->joinDomain));
    } else {
        fail = true;
        printf("no joinDomain\n");
    }

    if (json_object_object_get_ex(jobjSrv, "netidDomain", &obj)) {
        strncpy(out->netIdDomain, json_object_get_string(obj), sizeof(out->netIdDomain));
    } else {
        fail = true;
        printf("no netidDomain\n");
    }

    /* override DNS lookups with hosts listed in json configuration */
    if (json_object_object_get_ex(jobjSrv, "hosts", &obj)) {
        struct _host_list* my_host_list = NULL;
        int len, i;
        len = json_object_array_length(obj);
        for (i = 0; i < len; i++) {
            char hostname[256];
            json_object *o, *ajo = json_object_array_get_idx(obj, i);
            if (host_list == NULL) {    // first time
                host_list = calloc(1, sizeof(struct _host_list));
                my_host_list = host_list;
            } else {
                my_host_list->next = calloc(1, sizeof(struct _host_list));
                my_host_list = my_host_list->next;
            }
            if (json_object_object_get_ex(ajo, "postTo", &o)) {
                my_host_list->postTo = malloc(strlen(json_object_get_string(o))+1);
                strcpy(my_host_list->postTo, json_object_get_string(o));
            }
            if (json_object_object_get_ex(ajo, "join", &o)) {
                strcpy(hostname, json_object_get_string(o));
                strcat(hostname, ".");
                strcat(hostname, out->joinDomain);
                printf("JOIN HOSTNAME %s (length %u)\n", hostname, strlen(hostname)+1);
                my_host_list->name = malloc(strlen(hostname)+1);
                strcpy(my_host_list->name, hostname);
            } else if (json_object_object_get_ex(ajo, "network", &o)) {
                strcpy(hostname, json_object_get_string(o));
                strcat(hostname, ".");
                strcat(hostname, out->netIdDomain);
                printf("NETWORK HOSTNAME %s\n", hostname);
                my_host_list->name = malloc(strlen(hostname)+1);
                strcpy(my_host_list->name, hostname);
            }
            if (json_object_object_get_ex(ajo, "port", &o)) {
                my_host_list->port = json_object_get_int(o);
            }
        } // ..array interator

    } // ..if have hosts array

    ret = 0;
    if (!fail && conf_callback)
        ret = conf_callback(jobjSrv, out);
    else
        ret = -1;


pEnd:
    if (tok)
        json_tokener_free(tok);
    fclose(file);
    return ret;
} // ..parse_server_config()

void getJsHostName(uint64_t eui, char* urlOut, const char* domain)
{
    int i;
    char euiStr[20];
    char digit[3];
    sprintf(euiStr, "%016"PRIx64, eui);
    urlOut[0] = 0;
    digit[1] = '.';
    digit[2] = 0;
    for (i = 15; i >= 0; i--) {
        digit[0] = euiStr[i];
        strcat(urlOut, digit);
    }
    strcat(urlOut, domain);
}

void init_string(struct string *s)
{
    s->len = 0;
    s->ptr = malloc(s->len+1);
    if (s->ptr == NULL) {
        fprintf(stderr, "malloc() failed\n");
        exit(EXIT_FAILURE);
    }
    s->ptr[0] = '\0';
}

static size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s)
{
    size_t new_len = s->len + size*nmemb;
    s->ptr = realloc(s->ptr, new_len+1);
    if (s->ptr == NULL) {
        fprintf(stderr, "realloc() failed\n");
        exit(EXIT_FAILURE);
    }
    memcpy(s->ptr+s->len, ptr, size*nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;

    return size*nmemb;
}

#define N_TIDS      16
volatile unsigned tids[N_TIDS];
volatile unsigned tids_in;

void check_post(json_object* postJson)
{
    json_object* obj;
    /* check for duplicate transaction ID in outgoing answer */
    if (json_object_object_get_ex(postJson, Result, &obj)) {
        if (json_object_object_get_ex(postJson, TransactionID, &obj)) {
            unsigned myIdx, n, tid = json_object_get_int(obj);
            myIdx = tids_in;    // start at oldest
            for (n = 0; n < N_TIDS; n++) {
                if (tid == tids[myIdx]) {
                    printf("already sent tid %u\n", tid);
                    exit(EXIT_FAILURE);
                }
                if (++myIdx== N_TIDS)
                    myIdx = 0;
            }
            tids[tids_in] = tid;
            if (++tids_in == N_TIDS)
                tids_in = 0;
        } else {
            printf("no tid in answer\n");
            exit(EXIT_FAILURE);
        }
    }
}

/* resolver not used, URL passed directly to curl */
int
http_post_url(CURL* curl, json_object* postJson, const char* url, bool ansNeeded)
{
    curlPrivate_t* cp = malloc(sizeof(curlPrivate_t));
    struct curl_slist *headers = NULL;

    cp->pj = postJson;
    check_post(postJson);

    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "charsets: utf-8");

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_object_to_json_string(postJson));

    if (ansNeeded) {
        cp->response = malloc(sizeof(struct string));
        init_string(cp->response);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, cp->response);
    } else
        cp->response = NULL;

    curl_easy_setopt(curl, CURLOPT_PRIVATE, cp);
    curl_easy_setopt(curl, CURLOPT_URL, url);

    return 0;
}

/* requires resolver to post */
int
http_post_hostname(CURL* curl, json_object* postJson, const char* hostname, bool ansNeeded)
{
    curlPrivate_t* cp = malloc(sizeof(curlPrivate_t));
    int ret;
    struct curl_slist *headers = NULL;

    cp->pj = postJson;
    check_post(postJson);

    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "charsets: utf-8");

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_object_to_json_string(postJson));

    ret = resolve_post(curl, hostname, false);
    if (ret == 0 && ansNeeded) {
        cp->response = malloc(sizeof(struct string));
        init_string(cp->response);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, cp->response);
    } else
        cp->response = NULL;

    curl_easy_setopt(curl, CURLOPT_PRIVATE, cp);

    return ret;
}

static void
get_post_ans(MYSQL* sc, struct string* response)
{
    struct json_tokener *tok = json_tokener_new();
    json_object *jo = json_tokener_parse_ex(tok, response->ptr, response->len);
    enum json_tokener_error jerr = json_tokener_get_error(tok);
    if (jerr == json_tokener_success) {
        if (tok->char_offset < response->len)
            printf("\e[31mjson extra chars\e[0m\n");

        printf(" get_post_ans()->ParseJson() ");
        printf(" ######## %s ########\n", json_object_to_json_string(jo));
        ParseJson(sc, NULL, jo, NULL);
    } else
        printf("\e[31mjerr %s\e[0m\n", json_tokener_error_desc(jerr));

    json_object_put(jo);
    json_tokener_free(tok);
}

void curl_service(MYSQL* sc, CURLM* cm, unsigned ms)
{
    int numfds;
    struct CURLMsg* m;
    int still_running = 0;
    CURLMcode mc = curl_multi_wait(cm, NULL, 0, ms, &numfds);
    if (mc != CURLM_OK) {
        printf("\e[31m%s = curl_multi_wait()\e[0m\n", curl_multi_strerror(mc));
    }

    mc = curl_multi_perform(cm, &still_running);

    do {
        int msgs_left = 0;
        m = curl_multi_info_read(cm, &msgs_left);
        if (m && (m->msg == CURLMSG_DONE)) {
            curlPrivate_t *cp;
            CURL* e = m->easy_handle;
            printf("CURLMSG_DONE ");
            if (curl_easy_getinfo(e, CURLINFO_PRIVATE, &cp) != CURLE_OK)
                printf(" \e[31mcurl_easy_getinfo-fail\e[0m ");
            if (m->data.result != CURLE_OK)
                printf("\e[31mcurl_service %s\e[0m\n", curl_easy_strerror(m->data.result));
            else
                printf("OK ");
            
            json_object_put(cp->pj);
            if (cp->response != NULL) {
                printf("ressponse->len%u\n", cp->response->len);
                if (cp->response->len > 0) {
                    get_post_ans(sc, cp->response);
                    free(cp->response->ptr);
                }
                free(cp->response);
            }
            free(cp);
            curl_multi_remove_handle(cm, e);
            curl_easy_cleanup(e);
        }
    } while (m);
} // ..curl_service()

void print_buf(const uint8_t* buf, uint8_t len, const char* txt)
{
    uint8_t i;
    printf("%s: ", txt);
    for (i = 0; i < len; i++)
        printf("%02x ", buf[i]);

#if 0
    // print ascii text
    printf("    ");
    for (i = 0; i < len; i++) {
        if (buf[i] >= ' ' && buf[i] < 0x7f)
            putchar(buf[i]);
        else
            putchar('.');
    }
#endif /* if 0 */

    printf("\n");
}

/**
 * Linked list of all active sessions.  Yes, O(n) but a
 * hash table would be overkill for a simple example...
 */
//from app: static struct Session *sessions;

#define COOKIE_NAME "session"
/**
 * Return the session handle for this connection, or
 * create one if this is a new user.
 */
static struct Session *
get_session (struct MHD_Connection *connection, bool json, const char* jsonSender)
{
    struct Session *ret;

    if (!json) {
        const char *cookie = MHD_lookup_connection_value (connection, MHD_COOKIE_KIND, COOKIE_NAME);
        if (cookie != NULL) {
            /* find existing session */
            ret = sessions;
            while (NULL != ret) {
                if (0 == strcmp (cookie, ret->sid)) {
                    break;
                }
                ret = ret->next;
            }
            if (NULL != ret) {
                ret->rc++;
                return ret;
            }
        }
    } else {
        ret = sessions;
        while (NULL != ret) {
            if (0 == strcmp(jsonSender, ret->sid)) {
                    break;
            }
            ret = ret->next;
        }
        if (NULL != ret) {
            ret->rc++;
            return ret;
        }
    }

    /* create fresh session */
    ret = calloc (1, sizeof (struct Session));
    if (NULL == ret) {
        printf("\e[31mcalloc error: %s\e[0m\n", strerror (errno));
        return NULL;
    }

    if (sessionCreate(ret) < 0) {   // from user of this library
        printf("\e[31msessionCreate fail\e[0m\n");
        free(ret);
        return NULL;
    }

    if (json) {
        ret->appInfo = NULL;
        strncpy(ret->sid, jsonSender, sizeof(ret->sid));
    } else {
        ret->appInfo = create_appInfo();    // from user of this library

        /* not a super-secure way to generate a random session ID,
        but should do for a simple example... */
        snprintf (ret->sid,
            sizeof (ret->sid),
            "%X%X%X%X",
            (unsigned int) rand (),
            (unsigned int) rand (),
            (unsigned int) rand (),
            (unsigned int) rand ()
        );
    }

    ret->rc++;
    ret->start = time (NULL);
    ret->next = sessions;
    sessions = ret;
    return ret;
}

/**
 * Add header to response to set a session cookie.
 *
 * @param session session to use
 * @param response response to modify
 */
void
lib_add_session_cookie (struct Session *session, struct MHD_Response *response)
{
    char cstr[256];

    snprintf (cstr, sizeof (cstr), "%s=%s", COOKIE_NAME, session->sid);
    //printf("add_session_cookie:%s\n", cstr);

    if (MHD_NO == MHD_add_response_header (response, MHD_HTTP_HEADER_SET_COOKIE, cstr))
    {
        fprintf (stderr, "Failed to set session cookie header!\n");
    }
}

/**
 * Handler that adds the 'v1' value to the given HTML code.
 *
 * @param cls unused
 * @param mime mime type to use
 * @param session session handle
 * @param connection connection to use
 */
static int
page_handler_json(const void *cls,
	      const char *mime,
	      struct Session *session,
	      struct MHD_Connection *connection,
          json_object *jobjAns)
{
    int ret;
    struct MHD_Response *response;
    char *reply;

    if (jobjAns) {
        const char *jstr = json_object_to_json_string(jobjAns);
        //printf("page_handler_json sending ---- %s\n", jstr);
        reply = malloc (strlen (jstr)+1);
        if (NULL == reply) {
            printf("NULL = malloc()\n");
            return MHD_NO;
        }
        if (jobjAns != NULL)
            strcpy(reply, jstr);
    } else {
        reply = malloc (1);
        reply[0] = 0;
    }

    //JSON_PRINTF("reply:\"%s\"\n", reply);
    response = MHD_create_response_from_buffer (strlen (reply), (void *) reply, MHD_RESPMEM_MUST_FREE);
    if (NULL == response) {
        printf("NULL = MHD_create_response_from_buffer()\n");
        return MHD_NO;
    }

    //only for browser -- lib_add_session_cookie (session, response);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_ENCODING, mime);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
} // ..page_handler_json()

/**
 * Invalid method page.
 */
#define METHOD_ERROR "<html><head><title>Illegal request</title></head><body>method-error</body></html>"

/**
 * Main MHD callback for handling requests.
 *
 * @param cls argument given together with the function
 *        pointer when the handler was registered with MHD
 * @param connection handle identifying the incoming connection
 * @param url the requested url
 * @param method the HTTP method used ("GET", "PUT", etc.)
 * @param version the HTTP version string (i.e. "HTTP/1.1")
 * @param upload_data the data being uploaded (excluding HEADERS,
 *        for a POST that fits into memory and that is encoded
 *        with a supported encoding, the POST data will NOT be
 *        given in upload_data and is instead available as
 *        part of MHD_get_connection_values; very large POST
 *        data *will* be made available incrementally in
 *        upload_data)
 * @param upload_data_size set initially to the size of the
 *        upload_data provided; the method must update this
 *        value to the number of bytes NOT processed;
 * @param ptr pointer that the callback can set to some
 *        address and that will be preserved by MHD for future
 *        calls for this request; since the access handler may
 *        be called many times (i.e., for a PUT/POST operation
 *        with plenty of upload data) this allows the application
 *        to easily associate some request-specific state.
 *        If necessary, this state can be cleaned up in the
 *        global "MHD_RequestCompleted" callback (which
 *        can be set with the MHD_OPTION_NOTIFY_COMPLETED).
 *        Initially, <tt>*con_cls</tt> will be NULL.
 * @return MHS_YES if the connection was handled successfully,
 *         MHS_NO if the socket must be closed due to a serios
 *         error while handling the request
 */
int
lib_create_response (void *cls,
		 struct MHD_Connection *connection,
		 const char *url,
		 const char *method,
		 const char *version,
		 const char *upload_data,
		 size_t *upload_data_size,
		 void **ptr)
{
    struct MHD_Response *response;
    struct Request *request;
    struct Session *session = NULL;
    int ret;
    unsigned int i;

    request = *ptr;
    //printf("lib_create_response(,%p) %s url='%s' uds%zu ", *ptr, method, url, *upload_data_size);
    if (NULL == request)
    {
        const char* header;
        request = calloc (1, sizeof (struct Request));
        if (NULL == request)
        {
            fprintf (stderr, "calloc error: %s\n", strerror (errno));
            return MHD_NO;
        }
        header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Accept");
        //printf(" header:'%s' ", header);
        if (strstr(header, "json") != NULL) {
            request->json = true;
        }
        *ptr = request;
        if (0 == strcmp (method, MHD_HTTP_METHOD_POST)) {
            if (request->json) {
                request->ans_jobj = NULL;
            } else {
                request->post_url = url;
                //printf("MHD_create_post_processor ");
                request->pp = MHD_create_post_processor (connection, 1024, &post_iterator, request);
                if (NULL == request->pp) {
                    fprintf (stderr, "Failed to setup post processor for `%s'\n", url);
                    return MHD_NO; // internal error 
                }
            }
        }
        //HTTP_PRINTF("YES\n");
        return MHD_YES;
    }

    if (!request->json) {
        //printf(" request->session:%p UDS%u ", request->session, *upload_data_size);
        fflush(stdout);
        if (NULL == request->session)
        {
            HTTP_PRINTF("get_session() ");
            request->session = get_session (connection, request->json, NULL);
            if (NULL == request->session) {
                printf ("\e[31mFailed to setup session for `%s'\e[0m\n", url);
                return MHD_NO; /* internal error */
            }
        }
        session = request->session;
        session->start = time (NULL);
    }
    
    if (0 == strcmp (method, MHD_HTTP_METHOD_POST))
    {
        /* evaluate POST data */
        if (0 != *upload_data_size)
        {
            if (request->json) {
                enum json_tokener_error jerr;
                json_object *jobj;
                struct json_tokener *tok;
                //printf("JSON --- %s ---- ", upload_data);
                //fflush(stdout);
                tok = json_tokener_new();
                jobj = json_tokener_parse_ex(tok, upload_data, *upload_data_size);
                jerr = json_tokener_get_error(tok);
                if (jerr == json_tokener_success) {
                    const union MHD_ConnectionInfo* info = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
                    if (tok->char_offset < *upload_data_size)
                        printf("\e[31mjson extra chars %zu\e[0m\n", *upload_data_size - tok->char_offset);

                    if (NULL == request->session) {
                        char senderID[64];
                        json_object* obj;
                        if (json_object_object_get_ex(jobj, SenderID, &obj))
                            strcpy(senderID, json_object_get_string(obj));
                        else
                            strcpy(senderID, "unknownSender");

                        request->session = get_session (connection, request->json, senderID);
                        if (NULL == request->session) {
                            printf ("\e[31mFailed to setup session for `%s', '%s'\e[0m\n", url, senderID);
                            return MHD_NO; /* internal error */
                        }
                    }
                    session = request->session;
                    session->start = time (NULL);

                    ParseJson(session->sqlConn, info->client_addr, jobj, &request->ans_jobj);
                } else
                    printf("\e[31mlib-jerr %s ------ %s\e[0m\n", json_tokener_error_desc(jerr), json_object_to_json_string(jobj));

                json_object_put(jobj);
                json_tokener_free(tok);
            } else {
                /* call application to prepare for browser post */
                browser_post_init(request->session);
                MHD_post_process (request->pp, upload_data, *upload_data_size);
                browser_post_submitted(url, request);
            }
            *upload_data_size = 0;
            return MHD_YES;
        } /*else
            HTTP_PRINTF("upload_data_size==0 ");*/

        if (!request->json) {
            /* done with POST data, serve response */
            MHD_destroy_post_processor (request->pp);
            request->pp = NULL;
        }
        method = MHD_HTTP_METHOD_GET; /* fake 'GET' */
        if (NULL != request->post_url)
            url = request->post_url;
    }


    if ( (0 == strcmp (method, MHD_HTTP_METHOD_GET)) || (0 == strcmp (method, MHD_HTTP_METHOD_HEAD)) )
    {
        /*HTTP_PRINTF("GET \"%s\" ", url);*/
        /* find out which page to serve */
        if (request->json) {
            ret = page_handler_json(NULL, "application/json", session, connection, request->ans_jobj);
        } else {
            i=0;
            char root[32];
            const char* urlPtr;
            const char* sep = strchr(url+1, '/');
            if (sep != NULL) {
                strncpy(root, url, sep-url);
                root[sep-url] = 0;
                strncpy(session->urlSubDir, sep+1, sizeof(session->urlSubDir)-(sep-url));
                urlPtr = root;
                /*HTTP_PRINTF("SubDir '%s'\n", session->urlSubDir);*/
            } else
                urlPtr = url;

            while ( (pages[i].url != NULL) && (0 != strcmp (pages[i].url, urlPtr)) )
                i++;
            //HTTP_PRINTF("pages[%d] %s\n", i, urlPtr);
            ret = pages[i].handler (pages[i].handler_cls, pages[i].mime, session, connection);
        }
        if (ret != MHD_YES)
            fprintf (stderr, "Failed to create page for `%s'\n", url);
        return ret;
    }
    /* unsupported HTTP method */
    response = MHD_create_response_from_buffer(
            strlen (METHOD_ERROR),
            (void *) METHOD_ERROR,
            MHD_RESPMEM_PERSISTENT
    );
    ret = MHD_queue_response (connection, MHD_HTTP_NOT_ACCEPTABLE, response);
    MHD_destroy_response (response);
    /*HTTP_PRINTF("MHD_%d\n", ret);*/
    return ret;
} // ..lib_create_response()

/**
 * Clean up handles of sessions that have been idle for
 * too long.
 */
void
lib_expire_sessions ()
{
    struct Session *pos;
    struct Session *prev;
    struct Session *next;
    time_t now;

    now = time (NULL);
    prev = NULL;
    pos = sessions;
    while (NULL != pos)
    {
        next = pos->next;
        if (now - pos->start > 60 * 60)
        {
            /* expire sessions after 1h */
            if (NULL == prev)
                sessions = pos->next;
            else
                prev->next = next;

            if (pos->appInfo)
                free(pos->appInfo);

            sessionEnd(pos);   // from user of this library
            free (pos);
        }
        else
        prev = pos;
        pos = next;
    }
}


/**
 * Callback called upon completion of a request.
 * Decrements session reference counter.
 *
 * @param cls not used
 * @param connection connection that completed
 * @param con_cls session handle
 * @param toe status code
 */
void
lib_request_completed_callback (void *cls,
			    struct MHD_Connection *connection,
			    void **con_cls,
			    enum MHD_RequestTerminationCode toe)
{
    struct Request *request = *con_cls;

    if (NULL == request)
        return;

    if (NULL != request->session)
        request->session->rc--;

    if (NULL != request->pp)
        MHD_destroy_post_processor (request->pp);

    if (request->ans_jobj)
        json_object_put(request->ans_jobj);   // free reply


    free (request);
    printf("\n");
}

/** @brief xor buffer */
static void BlockExOr(uint8_t const l[], uint8_t const r[], uint8_t out[], uint16_t bytes)
{
    uint8_t const* lptr = l;
    uint8_t const* rptr = r;
    uint8_t* optr = out;
    uint8_t const* const end = out + bytes;

    for (;optr < end; lptr++, rptr++, optr++)
        *optr = *lptr ^ *rptr;
}

static uint16_t FindBlockOverhang(uint16_t inputDataLength)
{
    return inputDataLength & (LORA_ENCRYPTIONBLOCKBYTES - 1);
}

static uint16_t CountBlocks(uint16_t inputDataLength)
{
    uint16_t blockSizeMinus1 = LORA_ENCRYPTIONBLOCKBYTES - 1;
    uint16_t inRoundDown = inputDataLength & ~blockSizeMinus1;
    uint16_t roundUp = (FindBlockOverhang(inputDataLength) > 0) ? 1 : 0;
    uint16_t result = inRoundDown / LORA_ENCRYPTIONBLOCKBYTES + roundUp;

    return result;
}

/** @brief encrypt or decrypt payload buffer
 * @param key [in] AES key
 * @param in [in] input buffer
 * @param inputDataLength [in] input buffer length
 * @param address [in] mote device address
 * @param up [in] true for uplink, false for downlink
 * @param sequenceNumber [in] frame counter of packet
 * @param out [out] result buffer
 */
void LoRa_Encrypt(uint8_t i, const uint8_t key[], uint8_t const in[], uint16_t inputDataLength, uint32_t address, bool up, uint32_t sequenceNumber, uint8_t* out)
{
    if (inputDataLength == 0)
        return;

    uint8_t A[LORA_ENCRYPTIONBLOCKBYTES];

    memset(A, 0, LORA_ENCRYPTIONBLOCKBYTES);

    A[ 0] = 0x01; //encryption flags
    A[ 5] = up ? 0 : 1;

    Write4ByteValue(&A[6], address);
    Write4ByteValue(&A[10], sequenceNumber);

    uint16_t const blocks = CountBlocks(inputDataLength);
    uint16_t const overHangBytes = FindBlockOverhang(inputDataLength);

    uint8_t const* blockInput = in;
    uint8_t* blockOutput = out;
    for ( ; i <= blocks; i++, blockInput += LORA_ENCRYPTIONBLOCKBYTES, blockOutput += LORA_ENCRYPTIONBLOCKBYTES)
    {
        A[15] = (uint8_t)i;

        aes_context aesContext;
        aes_set_key(key, LORA_CYPHERKEYBYTES, &aesContext);

        uint8_t S[LORA_CYPHERKEYBYTES];
        aes_encrypt(A, S, &aesContext);

        uint16_t bytesToExOr;
        if ((i < blocks) || (overHangBytes == 0))
            bytesToExOr = LORA_CYPHERKEYBYTES;
        else
            bytesToExOr = overHangBytes;

        BlockExOr(S, blockInput, blockOutput, bytesToExOr);
    }
}

unsigned
numHexDigits(const char* hexStr)
{
    unsigned ret = 0;

    while (*hexStr) {
        if (
            (*hexStr >= '0' && *hexStr <= '9') ||
            (*hexStr >= 'A' && *hexStr <= 'F') ||
            (*hexStr >= 'a' && *hexStr <= 'f') )
        {
            ret++;
            hexStr++;
        } else
            return ret;
    }

    return ret;
}

int getTarget(const char* target, char* devEuiStr, size_t s1, char* devAddrStr, size_t s2)
{
    char* sep;
    if (strlen(target) < 1)
        return -1;
    sep = strchr(target, '_');
    fflush(stdout);
    if (sep-target > s1)
        printf("\e[31mgetTarget devEuiStr too small\e[0m");
    strncpy(devEuiStr, target, s1);
    devEuiStr[sep-target] = 0;
    strncpy(devAddrStr, sep+1, s2);

    return 0;
}

const char* 
getKey(json_object *jobj, const char* objName, const key_envelope_t* envelope, uint8_t* keyOut)
{
    json_object *obj;

    if (json_object_object_get_ex(jobj, objName, &obj)) {
        return parse_key_envelope(obj, envelope, keyOut);
    } else
        return NULL;
}

int
parseDevAddr(uint32_t devAddr, uint32_t* NetID, uint32_t* NwkID, uint32_t* NwkAddr)
{
    //printf("parseDevAddr() ");
    if ((devAddr & 0xff000000) == 0xfe000000) { // NetID type 7
        //printf("t7 ");
        *NwkID = (devAddr >> 7) & 0x1ffff; // 17bits
        *NetID = 0xe00000 | *NwkID; // 21lsb id
        *NwkAddr = devAddr & 0x7f;
    } else if ((devAddr & 0xfe000000) == 0xfc000000) { // NetID type 6
        //printf("t6 ");
        *NwkID = (devAddr >> 10) & 0x7fff; // 15bits
        *NetID = 0xc00000 | *NwkID; // 21lsb id
        *NwkAddr = devAddr & 0x3ff;
    } else if ((devAddr & 0xfc000000) == 0xf8000000) { // NetID type 5
        //printf("t5 ");
        *NwkID = (devAddr >> 13) & 0x1fff; // 13bits
        *NetID = 0xa00000 | *NwkID; // 21lsb id
        *NwkAddr = devAddr & 0x1fff;
    } else if ((devAddr& 0xf8000000) == 0xf0000000) { // NetID type 4
        //printf("t4 ");
        *NwkID = (devAddr >> 16) & 0x7ff; // 11bits
        *NetID = 0x800000 | *NwkID; // 21lsb id
        *NwkAddr = devAddr & 0xffff;
    } else if ((devAddr& 0xf0000000) == 0xe0000000) { // NetID type 3
        //printf("t3 ");
        *NwkID = (devAddr >> 18) & 0x3ff; // 10bits
        *NetID = 0x600000 | *NwkID; // 21lsb id
        *NwkAddr = devAddr & 0x3ffff;
    } else if ((devAddr& 0xe0000000) == 0xc0000000) { // NetID type 2
        //printf("t2 ");
        *NwkID = (devAddr >> 20) & 0x1ff; // 9bits
        *NetID = 0x400000 | *NwkID; // 9lsb id
        *NwkAddr = devAddr & 0xfffff;
    } else if ((devAddr& 0xc0000000) == 0x80000000) { // NetID type 1
        //printf("t1 ");
        *NwkID = (devAddr >> 24) & 0x3f; // 6bits
        *NetID = 0x200000 | *NwkID; // 6lsb id
        *NwkAddr = devAddr & 0xffffff;
    } else if ((devAddr& 0x80000000) == 0x00000000) { // NetID type 0
        //printf("01 ");
        *NwkID = (devAddr >> 25) & 0x3f; // 6bits
        *NetID = 0x000000 | *NwkID; // 6lsb id
        *NwkAddr = devAddr & 0x1ffffff;
    } else {
        //printf("\n");
        return UINT_MAX;
    }

    //printf(" %06x\n", *NetID);

    return 0;
}


const char*
getRFRegion(const char* str)
{
    if (strcmp(str, EU868) == 0)
        return EU868;
    else if (strcmp(str, US902) == 0)
        return US902;
    else if (strcmp(str, US902A) == 0)
        return US902A;
    else if (strcmp(str, US902B) == 0)
        return US902B;
    else if (strcmp(str, US902C) == 0)
        return US902C;
    else if (strcmp(str, US902D) == 0)
        return US902D;
    else if (strcmp(str, US902E) == 0)
        return US902E;
    else if (strcmp(str, US902F) == 0)
        return US902F;
    else if (strcmp(str, US902G) == 0)
        return US902G;
    else if (strcmp(str, US902H) == 0)
        return US902H;
    else if (strcmp(str, China779) == 0)
        return China779;
    else if (strcmp(str, China470) == 0)
        return China470;
    else if (strcmp(str, EU433) == 0)
        return EU433;
    else if (strcmp(str, Australia915) == 0)
        return Australia915;
    else if (strcmp(str, AS923) == 0)
        return AS923;

    return NULL;
}

static void
parseGWInfoElement(json_object* obj, GWInfo_t* out)
{
    json_object* o;

    if (json_object_object_get_ex(obj, DLAllowed, &o))
        out->DLAllowed = json_object_get_boolean(o);
    else {
        // not sent to hNS
    }

    if (json_object_object_get_ex(obj, SNR, &o))
        out->SNR = json_object_get_int(o);
    else {
        out->SNR = CHAR_MIN;
        // optional to hNS
    }

    if (json_object_object_get_ex(obj, RSSI, &o))
        out->RSSI = json_object_get_int(o);
    else {
        out->RSSI = CHAR_MIN;
        // optional to hNS
    }

    if (json_object_object_get_ex(obj, ID, &o)) {
        const char* str = json_object_get_string(o);
        sscanf(str, "%"PRIx64, &out->id);
    }

    if (json_object_object_get_ex(obj, RFRegion, &o))
        out->RFRegion = getRFRegion(json_object_get_string(o));

    if (json_object_object_get_ex(obj, Lat, &o))
        out->Lat = json_object_get_double(o);
    if (json_object_object_get_ex(obj, Lon, &o))
        out->Lon = json_object_get_double(o);

    if (json_object_object_get_ex(obj, ULToken, &o)) {
        const char* str = json_object_get_string(o);
        out->ULToken = malloc(strlen(str)+1);
        strcpy(out->ULToken, str);
    } else {
        out->ULToken = NULL;
    }

} // ..parseGWInfoElement()

int
ParseULMetaData(json_object* j, ULMetaData_t* ulMetaData)
{
    json_object *ULobj;

    ulMetaData->DevEUI = NONE_DEVEUI;
    ulMetaData->DevAddr = NONE_DEVADDR;
    ulMetaData->RecvTime[0] = 0;
    ulMetaData->RFRegion = NULL;
    ulMetaData->FNSULToken = NULL;
    ulMetaData->gwList = NULL;
    ulMetaData->GWCnt = 0;
    ulMetaData->DataRate = UCHAR_MAX;
    ulMetaData->ULFreq = 0;

    // DevEUI would be absent for ABP
    if (json_object_object_get_ex(j, DevEUI, &ULobj))
        sscanf(json_object_get_string(ULobj), "%"PRIx64, &ulMetaData->DevEUI);
    else
        ulMetaData->DevEUI = NONE_DEVEUI;

    if (json_object_object_get_ex(j, DevAddr, &ULobj))
        sscanf(json_object_get_string(ULobj), "%x", &ulMetaData->DevAddr);
    else
        ulMetaData->DevAddr = NONE_DEVADDR;

    if (json_object_object_get_ex(j, FPort, &ULobj))
        ulMetaData->FPort = json_object_get_int(ULobj);
    else
        ulMetaData->FPort = -1; // if from fNS, then fPort is unknown


    if (json_object_object_get_ex(j, FCntDown, &ULobj))
        ulMetaData->FCntDown = json_object_get_int(ULobj);
    else
        ulMetaData->FCntDown = 0;   // TODO value for not present?


    if (json_object_object_get_ex(j, FCntUp, &ULobj))
        ulMetaData->FCntUp = json_object_get_int(ULobj);
    else
        ulMetaData->FCntUp = 0;   // TODO value for not present?


    if (json_object_object_get_ex(j, Confirmed, &ULobj)) {
        /* carried only sNS-hNS interface */
        ulMetaData->Confirmed = json_object_get_boolean(ULobj);
    }

    if (json_object_object_get_ex(j, DataRate, &ULobj)) {
        ulMetaData->DataRate = json_object_get_int(ULobj);
    } else {
        /* DataRate only mandatory over fNS-sNS interface */
    }

    if (json_object_object_get_ex(j, ULFreq, &ULobj)) {
        ulMetaData->ULFreq = json_object_get_double(ULobj);
    } else {
        /* ULFreq only mandatory over fNS-sNS interface */
    }

    if (json_object_object_get_ex(j, Margin, &ULobj)) {
        // if allowed by Service Profile
        ulMetaData->Margin = json_object_get_int(ULobj);
    } 

    if (json_object_object_get_ex(j, Battery, &ULobj)) {
        // if allowed by Service Profile
        ulMetaData->Battery = json_object_get_int(ULobj);
    } 

    if (json_object_object_get_ex(j, FNSULToken, &ULobj)) {
        const char* str = json_object_get_string(ULobj);
        ulMetaData->FNSULToken = malloc(strlen(str)+1);
        strcpy(ulMetaData->FNSULToken, str);
    }

    if (json_object_object_get_ex(j, RecvTime, &ULobj))
        strncpy(ulMetaData->RecvTime , json_object_get_string(ULobj), sizeof(ulMetaData->RecvTime ));
    else {
        /* RecvTime mandatory over bother interfaces */
        printf("\e[31m%s: %s required\e[0m\n", ULMetaData, RecvTime);
        return -1;
    }

    if (json_object_object_get_ex(j, RFRegion, &ULobj))
        ulMetaData->RFRegion = getRFRegion(json_object_get_string(ULobj));
    else {
        /* RFRegion only on fNS-sNS, but not on sNS-hNS */
    }

    if (json_object_object_get_ex(j, GWCnt, &ULobj))
        ulMetaData->GWCnt = json_object_get_int(ULobj);

    if (json_object_object_get_ex(j, GWInfo, &ULobj)) {
        struct _gwList* mygwl;
        int i, alen = json_object_array_length(ULobj);
        if (alen != ulMetaData->GWCnt) {
            printf("\e[31m%s: %s=%u mismatch array length %u\e[0m\n", ULMetaData, GWCnt, ulMetaData->GWCnt, alen);
            return -1;
        }

        ulMetaData->gwList = calloc(1, sizeof(struct _gwList));
        mygwl = ulMetaData->gwList;

        for (i = 0; i < ulMetaData->GWCnt; ) {
            json_object *ajo = json_object_array_get_idx(ULobj, i);
            mygwl->GWInfo = calloc(1, sizeof(GWInfo_t));
            parseGWInfoElement(ajo, mygwl->GWInfo);

            if (++i < ulMetaData->GWCnt) {
                mygwl->next = calloc(1, sizeof(struct _gwList));
                mygwl = mygwl->next;
            } else
                mygwl->next = NULL;
        } // ..for (i = 0; i < ulMetaData->GWCnt; i++)
    } else if (ulMetaData->GWCnt > 0) {
        printf("\e[31mParseULMetaData: GWCnt=%u but no GWInfo\e[0m\n", ulMetaData->GWCnt);
        return -1;
    }

    return 0;
} // ..ParseULMetaData()

/** @brief return seconds between to timespec structs */
double difftimespec(struct timespec end, struct timespec beginning) {
    double x;

    x = 1E-9 * (double)(end.tv_nsec - beginning.tv_nsec);
    x += (double)(end.tv_sec - beginning.tv_sec);

    return x;
}

void ulmd_free(ULMetaData_t* ulmd)
{
    if (ulmd == NULL)
        return;

    while (ulmd->gwList != NULL) {
        struct _gwList* next = ulmd->gwList->next;
        GWInfo_t* g = ulmd->gwList->GWInfo;
        if (g != NULL) {
            if (g->ULToken)
                free(g->ULToken);
            free(g);
        }
        if (ulmd->FNSULToken) {
            free(ulmd->FNSULToken);
            ulmd->FNSULToken = NULL;
        }
        free(ulmd->gwList);
        ulmd->gwList = next;
    }

    ulmd->GWCnt = 0;
}

void
getAgo(const char* in, char* out)
{
    time_t t, now = time(NULL);
    sscanf(in, "%lu", &t);
    if (t > now) {
        t = t - now;
        if (t > 86400) {
            float days = t / (float)86400;
            sprintf(out, "in %.1f days", days);
        } else if (t > 3600) {
            float hours = t / (float)3600;
            sprintf(out, "in %.1f hours", hours);
        } else if (t > 60) {
            float mins = t / (float)60;
            sprintf(out, "in %.1f minutes", mins);
        } else
            sprintf(out, "in %lusec", t);
    } else {
        t = now - t;
        if (t > 86400) {
            float daysAgo = t / (float)86400;
            sprintf(out, "%.1f days ago", daysAgo);
        } else if (t > 3600) {
            float hoursAgo = t / (float)3600;
            sprintf(out, "%.1f hours ago", hoursAgo);
        } else if (t > 60) {
            float minsAgo = t / (float)60;
            sprintf(out, "%.1f minutes ago", minsAgo);
        } else
            sprintf(out, "%lu seconds ago", t);
    }
}

void
print_mtype(mtype_e mt)
{
    printf("MTYPE_");
    switch (mt) {
        case MTYPE_JOIN_REQ: printf("JOIN_REQ "); break;
        case MTYPE_JOIN_ACC: printf("JOIN_ACC "); break;
        case MTYPE_UNCONF_UP: printf("UNCONF_UP"); break;
        case MTYPE_UNCONF_DN: printf("UNCONF_DN"); break;
        case MTYPE_CONF_UP: printf("CONF_UP"); break;
        case MTYPE_CONF_DN: printf("CONF_DN"); break;
        case MTYPE_REJOIN_REQ: printf("REJOIN_REQ"); break;
        case MTYPE_P: printf("P"); break;
    }
    printf(" ");
}

/** @brief message integrity calculation for (re)join frames (OTA)
 * @param key [in] application key
 * @param input [in] received payload buffer
 * @param dataLength [in] received payload buffer length
 * @param output [out] 4-byte integrity value
 */
void LoRa_GenerateJoinFrameIntegrityCode(bool verbose, const uint8_t key[], uint8_t const input[], uint16_t dataLength, uint8_t* output)
{
    AES_CMAC_CTX cmacctx;
    AES_CMAC_Init(&cmacctx);
    AES_CMAC_SetKey(&cmacctx, key);

    if (verbose) {
        print_buf(key, LORA_CYPHERKEYBYTES, "\nmic-key");
        print_buf(input, dataLength, "mic-in");
    }

    AES_CMAC_Update(&cmacctx, input, dataLength);
    uint8_t temp[LORA_AUTHENTICATIONBLOCKBYTES];
    AES_CMAC_Final(temp, &cmacctx);
    memcpy(output, temp, LORA_FRAMEMICBYTES);
}

