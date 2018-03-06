/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <unistd.h>
#define __USE_XOPEN     /* for strptime */
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <mysql.h>
#include <string.h>
#include <json-c/json.h>
#include <errno.h>
#include <curl/curl.h>
#include <microhttpd.h>
#include <gcrypt.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "lorawan.h"
#include "cmac.h"

#define MIC_DEBUG_DOWN
//#define MIC_DEBUG_UP
//#define CRYPT_DEBUG
//#define HTTP_DEBUG
//#define JSON_DEBUG

#ifdef HTTP_DEBUG
    #define HTTP_PRINTF(...)     printf(__VA_ARGS__)
#else
    #define HTTP_PRINTF(...)
#endif

#ifdef SQL_DEBUG
    #define SQL_PRINTF(...)     printf(__VA_ARGS__)
#else
    #define SQL_PRINTF(...)
#endif

#ifdef JSON_DEBUG
    #define JSON_PRINTF(...)     printf(__VA_ARGS__)
#else
    #define JSON_PRINTF(...)
#endif

#ifdef CRYPT_DEBUG
    #define DEBUG_CRYPT_BUF(x,y,z)  print_buf(x,y,z)
    #define DEBUG_CRYPT(...)      printf(__VA_ARGS__)
#else
    #define DEBUG_CRYPT_BUF(x,y,z)  
    #define DEBUG_CRYPT(...)      
#endif

#ifdef MIC_DEBUG_UP
    #define DEBUG_MIC_BUF_UP(x,y,z)    print_buf(x,y,z)
    #define DEBUG_MIC_UP(...)        printf(__VA_ARGS__)
#else
    #define DEBUG_MIC_BUF_UP(x,y,z)    
    #define DEBUG_MIC_UP(...)        
#endif

#ifdef MIC_DEBUG_DOWN
    #define DEBUG_MIC_BUF_DOWN(x,y,z)    print_buf(x,y,z)
    #define DEBUG_MIC_DOWN(...)        printf(__VA_ARGS__)
#else
    #define DEBUG_MIC_BUF_DOWN(x,y,z)    
    #define DEBUG_MIC_DOWN(...)        
#endif

#define URL_LENGTH      128

typedef struct {
    unsigned int sql_port;
    char sql_hostname[64];
    char sql_username[64];
    char sql_password[64];
    unsigned int httpd_port;

    char joinDomain[64];
    char netIdDomain[64];
} conf_t;

/**
 * State we keep for each user/session/browser.
 */
struct Session
{
    /**
     * We keep all sessions in a linked list.
     */
    struct Session *next;

    /**
     * Unique ID for this session.
     */
    char sid[33];

    /**
     * Reference counter giving the number of connections
     * currently using this session.
     */
    unsigned int rc;

    /**
     * Time when this session was last active.
     */
    time_t start;

    char urlSubDir[64];    // if url has '-', then this is everything after it

    void *appInfo;
    MYSQL *sqlConn;
};

/**
 * Type of handler that generates a reply.
 *
 * @param cls content for the page (handler-specific)
 * @param mime mime type to use
 * @param session session information
 * @param connection connection to process
 * @param MHD_YES on success, MHD_NO on failure
 */
typedef int (*PageHandler)(const void *cls,
			   const char *mime,
			   struct Session *session,
			   struct MHD_Connection *connection);

/**
 * Entry we generate for each page served.
 */
struct Page
{
  /**
   * Acceptable URL for this page.
   */
  const char *url;

  /**
   * Mime type to set for the page.
   */
  const char *mime;

  /**
   * Handler to call to generate response.
   */
  PageHandler handler;

  /**
   * Extra argument to handler.
   */
  const void *handler_cls;
};


/**
 * Data kept per request.
 */
struct Request
{

  /**
   * Associated session.
   */
  struct Session *session;

  /**
   * Post processor handling form data (IF this is
   * a POST request).
   */
  struct MHD_PostProcessor *pp;

  /**
   * URL to serve in response to this POST (if this request
   * was a 'POST')
   */
  const char *post_url;

    bool json;
    json_object *ans_jobj;   // reply json
};

extern const char EU868[];
extern const char US902[];
extern const char China779[];
extern const char China470[];
extern const char EU433[];
extern const char Australia915[];
extern const char AS923[];
extern const char KR922[];
extern const char IN865[];
extern const char RU868[];

extern const char JoinReq[];
extern const char JoinAns[];
extern const char RejoinReq[];
extern const char RejoinAns[];
extern const char XmitDataReq[];
extern const char XmitDataAns[];
extern const char PRStartReq[];
extern const char PRStartAns[];
extern const char PRStopReq[];
extern const char PRStopAns[];
extern const char HRStartReq[];
extern const char HRStartAns[];
extern const char HRStopReq[];
extern const char HRStopAns[];
extern const char HomeNSReq[];
extern const char HomeNSAns[];
extern const char ProfileReq[];
extern const char ProfileAns[];
extern const char AppSKeyReq[];
extern const char AppSKeyAns[];



extern const char Success[];
extern const char MalformedRequest[];
extern const char InvalidProtocolVersion[];
extern const char FrameSizeError[];
extern const char UnknownReceiverID[];
extern const char UnknownDevEUI[];
extern const char UnknownDevAddr[];
extern const char MICFailed[];
extern const char ActivationDisallowed[];
extern const char JoinReqFailed[];
extern const char NoRoamingAgreement[];
extern const char RoamingActDisallowed[];
extern const char Deferred[];
extern const char StaleDeviceProfile[];
extern const char XmitFailed[];
extern const char InvalidFPort[];


extern const char PHYPayload[];
extern const char FRMPayload[];
extern const char DevEUI[];
extern const char DevAddr[];
extern const char DLSettings[];
extern const char RxDelay[];
extern const char CFList[];
extern const char Lifetime[];
extern const char SNwkSIntKey[];
extern const char AESKey[];
extern const char Other[];
extern const char KEKLabel[];
extern const char FNwkSIntKey[];
extern const char NwkSEncKey[];
extern const char NwkSKey[];
extern const char AppSKey[];
extern const char SupportsJoin[];

extern const char DeviceProfile[];
extern const char SupportsClassB[];
extern const char ClassBTimeout[];
extern const char PingSlotPeriod[];
extern const char PingSlotDR[];
extern const char PingSlotFreq[];
extern const char SupportsClassC[];
extern const char ClassCTimeout[];
extern const char RegParamsRevision[];
extern const char MACVersion[];
extern const char RXDelay1[];
extern const char RXDROffset1[];
extern const char RXDataRate2[];
extern const char RXFreq2[];
extern const char FactoryPresetFreqs[];
extern const char MaxEIRP[];
extern const char MaxDutyCycle[];
extern const char RFRegion[];
extern const char Supports32bitFCnt[];
extern const char DeviceProfileTimestamp[];


extern const char ServiceProfileID[];
extern const char ServiceProfile[];
extern const char ULRate[];
extern const char ULBucketSize[];
extern const char ULRatePolicy[];
extern const char DLRate[];
extern const char DLBucketSize[];
extern const char DLRatePolicy[];
extern const char AddGWMetadata[];
extern const char DevStatusReqFreq[];
extern const char ReportDevStatusBattery[];
extern const char ReportDevStatusMargin[];
extern const char DRMin[];
extern const char DRMax[];
extern const char ChannelMask[];
extern const char PRAllowed[];
extern const char HRAllowed[];
extern const char RAAllowed[];
extern const char NwkGeoLoc[];
extern const char TargetPER[];
extern const char MinGWDiversity[];
extern const char AS_ID[];
extern const char ULMetaData[];
extern const char FPort[];
extern const char FCntDown[];
extern const char FCntUp[];
extern const char Confirmed[];
extern const char DataRate[];
extern const char ULFreq[];
extern const char Margin[];
extern const char Battery[];
extern const char GWCnt[];
extern const char RecvTime[];
extern const char GWInfo[];

//extern const char GWInfoElement[];
extern const char ID[];
extern const char RSSI[];
extern const char SNR[];
extern const char Lat[];
extern const char Lon[];
extern const char ULToken[];
extern const char DLAllowed[];


extern const char RoamingActivationType[];
extern const char Passive[];
extern const char Handover[];
extern const char HNetID[];


extern const char VSExtension[];
extern const char VendorID[];
extern const char Object[];

extern const char seconds_at_beacon[];
extern const char tstamp_at_beacon[];
extern const char lgw_trigcnt_at_next_beacon[];
extern const char beacon_ch[];

extern const char DLFreq1[];
extern const char DLFreq2[];
extern const char ClassMode[];
extern const char DataRate1[];
extern const char DataRate2[];
extern const char FNSULToken[];
extern const char HiPriorityFlag[];
extern const char PingPeriod[];
extern const char DLMetaData[];


typedef struct {
    char* kek_label;
    uint8_t* key_bin;
    uint8_t key_len;
} key_envelope_t;

int parse_server_config(const char* conf_file, int (*conf_callback)(json_object*, conf_t*), conf_t* conf);
int parse_json_KeyEnvelope(const char*, json_object*, key_envelope_t*);
int database_open(const char*, const char*, const char*, uint16_t, const char* const dbname, MYSQL** db_ptr, const char* const buildVersion);
int ascii_hex_to_buf(const char* in_str, uint8_t* output);
uint64_t eui_buf_to_uint64(const uint8_t* eui);
void uint64_to_eui_buf(uint64_t in, uint8_t* out);
void json_print_type(json_type t);
uint8_t* Write4ByteValue(uint8_t output[], uint32_t input);
uint8_t* Write3ByteValue(uint8_t output[], uint32_t input);
uint8_t* Write2ByteValue(uint8_t output[], uint32_t input);
uint8_t* Write1ByteValue(uint8_t output[], uint32_t input);
json_object* create_KeyEnvelope(const uint8_t* key_bin, const key_envelope_t*);
const char* parse_key_envelope(json_object*, const key_envelope_t*, uint8_t* keyOut);
unsigned numHexDigits(const char* hexStr);
int getTarget(const char* target, char* devEuiStr, size_t s1, char* devAddrStr, size_t s2);
// getKey(): return NULL for key not in json, or result of getting key
const char* getKey(json_object *jobj, const char* objName, const key_envelope_t* envelope, uint8_t* keyOut);

struct string {
    char *ptr;
    size_t len;
};
typedef struct {
    json_object* pj;
    struct string* response;
} curlPrivate_t;

void init_string(struct string *s);
int http_post_url(CURL*, json_object* postJson, const char* url, bool ansNeeded);
int http_post_hostname(CURL*, json_object* postJson, const char* hostname, bool ansNeeded);
void curl_service(MYSQL* sc, CURLM *cm, unsigned ms);

// ans_mt NULL when parsing answer, ans_mt set to ans message type when parsing req
const char* lib_parse_json(json_object *jobj, const char** ans_mt, const char** pmt, char* senderID, const char* myID, unsigned long* tid, const char** receivedResult);
void lib_generate_json(json_object *jobj, const char* destID, const char* srcID, unsigned long tid, const char* mt, const char* result);
void print_buf(const uint8_t* buf, uint8_t len, const char* txt);


int sessionCreate(struct Session*);
void sessionEnd(struct Session*); 
void* create_appInfo(void);
int lib_create_response (void *cls,
		 struct MHD_Connection *connection,
		 const char *url,
		 const char *method,
		 const char *version,
		 const char *upload_data,
		 size_t *upload_data_size,
		 void **ptr
);
void lib_request_completed_callback (void *cls,
			    struct MHD_Connection *connection,
			    void **con_cls,
			    enum MHD_RequestTerminationCode toe
);

void lib_expire_sessions (void);
void lib_add_session_cookie (struct Session *session, struct MHD_Response *response);

/* callbacks to application, provided by application... */
int
post_iterator (void *cls,
	       enum MHD_ValueKind kind,
	       const char *key,
	       const char *filename,
	       const char *content_type,
	       const char *transfer_encoding,
	       const char *data, uint64_t off, size_t size
);
void browser_post_init(struct Session *session); /**< called just prior to first post_iterator call */
void ParseJson(MYSQL* sc, const struct sockaddr *, json_object* inObj, json_object** ansJobj);
void browser_post_submitted(const char* url, struct Request* request);
extern struct Page pages[];
extern struct Session *sessions;
/* ...provided by application */

void LoRa_Encrypt(uint8_t i, const uint8_t key[], uint8_t const in[], uint16_t inputDataLength, uint32_t address, bool up, uint32_t sequenceNumber, uint8_t* out);


typedef struct {
    uint64_t id;
    const char* RFRegion;
    int8_t RSSI;
    int8_t SNR;
    float Lat;
    float Lon;
    char *ULToken;
    bool DLAllowed;
} GWInfo_t;

struct _gwList {
    GWInfo_t* GWInfo;   // must be freed if not null
    struct _gwList* next;
};

#define NONE_NETID          0xffffff
#define NONE_DEVADDR        0xffffffff
#define NONE_DEVEUI         0xffffffffffffffff
typedef struct {
    uint64_t DevEUI;
    uint32_t DevAddr;
    int FPort;  // -1 for no fport present
    uint32_t FCntDown;
    uint32_t FCntUp;
    json_bool Confirmed;
    uint8_t DataRate;
    char* FNSULToken;    // only between fNS-sNS
    float ULFreq;   // MHz
    int Margin;
    int Battery;
    char RecvTime[32];
    const char* RFRegion;
    uint8_t GWCnt;
    struct _gwList* gwList;
} ULMetaData_t;

void getJsHostName(uint64_t eui, char* urlOut, const char* domain);
int parseDevAddr(uint32_t devAddr, uint32_t* NetID, uint32_t* NwkID, uint32_t* NwkAddr);

int ParseULMetaData(json_object* j, ULMetaData_t* ulMetaData);
double difftimespec(struct timespec end, struct timespec beginning);

void ulmd_free(ULMetaData_t* ulmd);

void getAgo(const char* in, char* out);
void print_mtype(mtype_e mt);
void LoRa_GenerateJoinFrameIntegrityCode(bool verbose, const uint8_t key[], uint8_t const input[], uint16_t dataLength, uint8_t* output);

/* from resolve.c: */
int resolve_post(CURL*, const char* hostname, bool verbose);
const char* getRFRegion(const char* str);
