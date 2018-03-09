/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "libserver.h"
#include "loragw_hal.h"
#include <mqueue.h>

#define MAC_DEBUG
#define RF_DEBUG

#ifdef SNR_DEBUG
    #define SNR_PRINTF(...)      printf(__VA_ARGS__)
#else
    #define SNR_PRINTF(fmt, args...)
#endif

#ifdef MAC_DEBUG
    #define DEBUG_MAC_BUF(x,y,z)  print_buf(x,y,z)
    #define MAC_PRINTF(...)     printf(__VA_ARGS__)
#else
    #define DEBUG_MAC_BUF(x,y,z)
    #define MAC_PRINTF(...)
#endif

#ifdef RF_DEBUG
    #define DEBUG_RF_BUF(x,y,z)  print_buf(x,y,z)
    #define RF_PRINTF(...)     printf(__VA_ARGS__)
#else
    #define DEBUG_RF_BUF(x,y,z)
    #define RF_PRINTF(...)
#endif

#define SNR_WEIGHT      12

typedef enum {
    ANSWER_ALL,
    ANSWER_NOT_BEST,
    ANSWER_ONLY_BEST
} answer_e;

typedef enum {
    noneNS = 0,     // fail
    fNS,
    sNS,
    hNS
} role_e;

typedef struct {
    bool OptNeg;
    const char* roamState;
    time_t roamUntil;
    uint32_t roamingWithNetID;
    bool enable_fNS_MIC;
    bool is_sNS;
    bool roamExpired;
} sql_t;


/** @brief datarate, combined bandwidth and spreading-factor */
typedef struct {
    uint8_t lgw_bw; /**< BW_xxKHZ */
    uint8_t lgw_sf_bps; /**< DR_LORA_SFx, or BPS for fsk */
    uint8_t fdev_khz;   /**< set to zero for LoRa */
} dr_t;

typedef union {
    struct {
        int fd; /**< file descriptor for network connection to gateway */
        uint32_t count_us;  /**< rx timestamp */
        uint32_t seconds_at_beacon; /**< time value transmitted in last beacon payload (for ping offset calc, device time req) */
        uint32_t tstamp_at_beacon;  /**< sx1301 counter captured at last beacon TX (for device time req) */
        uint32_t lgw_trigcnt_at_next_beacon; /**< predicted sx1301 counter at next beacon TX start (for beacon timing ans) */
        uint8_t beacon_ch;  /**< radio channel of last beacon transmitted (for beacon timing ans) */
    } gateway; 
    uint8_t octets[21];
} __attribute__((packed)) ULToken_t;

#define ULTOKENSTRSIZE      ((sizeof(ULToken_t)*2)+1)

struct _ultList {
    char *ULToken;
    struct _ultList* next;
};

typedef struct {
    uint64_t DevEUI;    // OTA downlinks. Absent to stateless fNS
    uint32_t DevAddr;   // support ABP downlinks
    uint8_t FPort;
    uint32_t FCntDown;
    bool Confirmed;
    float DLFreq1, DLFreq2; // in MHz
    uint8_t RXDelay1;   // assuming units: seconds
    uint8_t DataRate1, DataRate2;
    char ClassMode;
    char* FNSULToken;
    struct _ultList* ultList;
    bool HiPriorityFlag;
    unsigned PingPeriod; // when ClassMode == 'B'
} DLMetaData_t;

typedef struct mote_t mote_t;

typedef struct regional_t regional_t;   /**< region of gateway */
#define MAX_DATARATES       24      /**< maximum number of datarates in region */
struct regional_t {
    union {
        struct {
            uint8_t MaxEIRP           : 4; // 0, 1, 2, 3
            uint8_t UplinkDwellTime   : 1; // 4
            uint8_t DownlinkDwellTime : 1; // 5
            uint8_t rfu               : 1; // 6, 7
        } bits;
        uint8_t octet;
    } TxParamSetup;
    bool enableTxParamSetup;

    struct {    /* ServiceProfile initialized with default values from regional-PHY specification */
        Rx2ChannelParams_t Rx2Channel;  /**< Rx2 RF channel and datarate */
        uint32_t ping_freq_hz;  /**< RF channel of ping downlink */
        uint8_t ping_dr;    /**< datarate of ping dowlink */
        uint8_t uplink_dr_max;  /**< fastest uplink datarate DRMax */
        uint8_t Rx1DrOffset;    /**< datarate offset between uplink and Rx1 downlink */
    } defaults;
    int8_t dl_tx_dbm;   /**< gateway transmit power */
    dr_t datarates[MAX_DATARATES];  /**< table of datarates */
    uint8_t num_datarates;
    uint32_t beacon_hz; /**< RF channel of beacon */
    uint8_t default_txpwr_idx;  /**< end-node txpwer_idx: 0=highest power */
    uint8_t max_txpwr_idx;      /**< end-node idx for lowest possible tx power */
    uint16_t init_ChMask[MAX_CH_MASKS]; /**< initial channel mask value */

    int (*get_ch)(float MHz, uint8_t bw); /**< convert hz/bw to channel number */
    uint8_t* (*get_cflist)(uint8_t* ptr); /**< channel list sent on join-accept */
    void (*init_session)(mote_t*, const regional_t*); /**< OTA session startup */
    void (*parse_start_mac_cmd)(const uint8_t* buf, uint8_t buf_len, mote_t*); /**< callback for session initialization mac commands from mote */
    void (*rx1_band_conv)(float ULFreq, uint8_t ULDataRate, uint8_t rxdrOffset1, DLMetaData_t* out);
};

extern MYSQL *sqlConn_lora_network;

/* web.c: */
int web_init(const char* dbhostname, const char* dbuser, const char* dbpass, uint16_t dbport, const char*);
int
create_response (void *cls,
		 struct MHD_Connection *connection,
		 const char *url,
		 const char *method,
		 const char *version,
		 const char *upload_data,
		 size_t *upload_data_size,
		 void **ptr);


/** @brief available region values for gateway
 */
typedef struct {
    char* conf_file_8ch;    /**< file name of json region 8ch configuration */
    char* conf_file_16ch;    /**< file name of json region 16ch configuration */
    const char* RFRegion;
    regional_t regional;    /**< */
} region_t;

/** @brief list of possible gateway regions */
struct _region_list {
    region_t region;    /**< this region */
    struct _region_list* next;  /**< next region */
};

extern struct _region_list* region_list; /**< list of possible gateway regions */

/* regional: */
int us902_get_ch(float, uint8_t bw);
int au915_get_ch(float, uint8_t bw);
int arib_get_ch(float, uint8_t bw);
void arib_init_session(mote_t*, const regional_t*);
uint8_t* arib_get_cflist(uint8_t* ptr);
void arib_parse_start_mac_cmd(const uint8_t* buf, uint8_t buf_len, mote_t*);
void rx1_band_conv_us902(float ULFreq, uint8_t ULDataRate, uint8_t rxdrOffset1, DLMetaData_t* out);
void rx1_band_conv_au915(float ULFreq, uint8_t ULDataRate, uint8_t rxdrOffset1, DLMetaData_t* out);
void rx1_band_conv_as923(float ULFreq, uint8_t ULDataRate, uint8_t rxdrOffset1, DLMetaData_t* out);
void rx1_band_conv_eu868(float ULFreq, uint8_t ULDataRate, uint8_t rxdrOffset1, DLMetaData_t* out);
void rx1_band_conv_eu433(float ULFreq, uint8_t ULDataRate, uint8_t rxdrOffset1, DLMetaData_t* out);
void rx1_band_conv_cn779(float ULFreq, uint8_t ULDataRate, uint8_t rxdrOffset1, DLMetaData_t* out);


/* fNS.c: */
const char* fNS_JoinReq(mote_t* mote);
bool fNS_uplink_finish(mote_t* mote, sql_t* sql, bool* discard); // return true to skip jsonFinish

const char* fNS_downlink(const DLMetaData_t* DLMetaData, unsigned long reqTid, const char* clientID, const uint8_t* txBuf, uint8_t txLen, const char* txt);
extern uint8_t nwkAddrBits; /**< written once at startup (from network_id in conf.json) */
extern uint32_t devAddrBase;    /**< written once at startup (from network_id in conf.json) */



typedef struct {
    bool needsAnswer;
    const char* MessageType;
    char ClientID[64];
    unsigned long inTid;
    ULMetaData_t ulmd;
    bool ans_ans;   // this incoming request caused this ns to send out request, requiring answer to be sent out upon receiving answer
} requester_t;

struct _requesterList {
    requester_t *R;    /**< this */
    struct _requesterList* next;    /**< next */
};

typedef void (*AnsCallback_t)(MYSQL*, mote_t*, json_object*, const char* result, const char* senderID, const uint8_t* rfBuf, uint8_t rfLen);

typedef enum {
    PROGRESS_OFF,
    PROGRESS_LOCAL,
    PROGRESS_JSON
} progress_e;

typedef struct {
    uint32_t NFCntDown;
    uint32_t FCntUp;
    uint8_t SNwkSIntKeyBin[LORA_CYPHERKEYBYTES];
    uint8_t FNwkSIntKeyBin[LORA_CYPHERKEYBYTES];
    uint8_t NwkSEncKeyBin[LORA_CYPHERKEYBYTES];
    bool expired;
    time_t until; // zero indicates no session
    bool next;
    bool OptNeg;
} session_t;

#define NT      2

struct mote_t {
    struct timespec read_host_time;

    struct {
        uint32_t sentTID; /**< transactionID which was sent out in a request */
        AnsCallback_t AnsCallback;
    } t[NT];

    uint32_t devAddr;
    uint64_t devEui;
    session_t session;
    uint8_t nth;    // nth session

    progress_e progress;
    struct timespec first_uplink_host_time;
    struct timespec release_uplink_host_time;

    int best_sq;
    int8_t rx_snr;

    char* bestULTokenStr;

    ULMetaData_t ulmd_local;

    uint8_t ULFRMPayloadLen;
    /* uplink payload over-written upon better signal */
    uint8_t ULPayloadBin[256];
    uint8_t ULPHYPayloadLen;

    void* f;
    void* s;
    void* h;

    uint8_t rxdrOffset1;

    requester_t** bestR;    /**< null when best is local */
    struct _requesterList* requesterList;   /**< from json */

    bool new;
    char writtenDeviceProfileTimestamp[32]; // support slow profile write on roaming start
};

struct _mote_list {
    mote_t* motePtr;    /**< this mote */
    struct _mote_list* next;    /**< next mote */
};

/* from gateway.c: */
#define MQ_MSGSIZE     512
extern mqd_t mqd;
void gateway_disconnect(uint64_t eui);
int gateway_id_write_downlink(uint64_t eui, const struct lgw_pkt_tx_s* tx_pkt);
void print_hal_sf(uint8_t datarate);
void print_hal_bw(uint8_t bandwidth);
extern key_envelope_t key_envelope_ns_js;
extern uint32_t myNetwork_id32;
extern uint32_t myVendorID;
extern char myNetwork_idStr[];
extern uint8_t dl_rxwin;
extern unsigned leap_seconds;
extern CURLM *multi_handle;
extern char joinDomain[];
extern char netIdDomain[];
extern struct _mote_list* mote_list;


int child(const char* mqName, const char* dbhostname, const char* dbuser, const char* dbpass, uint16_t dbport, const char *dbName);

/* common.c: */
const char* get_nsns_key(MYSQL* sc, json_object *jobj, const char* objName, const char* netIDstr, uint8_t* keyOut);
const char* RStop(MYSQL* sc, const char* pmt, json_object* inJobj, const char* senderID, json_object** ansJobj);
void printElapsed(const mote_t* mote);
mote_t* getMote(MYSQL* sc, struct _mote_list **moteList, uint64_t devEui, uint32_t devAddr);
int getSession(MYSQL* sc, uint64_t devEui, uint32_t devAddr, uint8_t nth, session_t* out);
my_ulonglong getMoteID(MYSQL* sc, uint64_t devEui, uint32_t devAddr, const char** result);
int deleteOldSessions(MYSQL* sc, mote_t* mote, bool all);
const char* getMotesWhere(MYSQL* sc, uint64_t devEui, uint32_t devAddr, char* out);
void getWhere(uint64_t devEui, uint32_t devAddr, char* out);
int sendXmitDataAns(bool toAS, json_object* ansJobj, const char* destIDstr, unsigned long reqTid, const char* result);
const char* XmitDataReq_downlink(mote_t* mote, const json_object* dlobj);
int next_tid(mote_t* mote, const char* txt, AnsCallback_t, uint32_t*);
void print_mtype(mtype_e);
const char* sendXmitDataReq(mote_t* mote, const char* txt, json_object* jo, const char* destIDstr, const char* hostname, const char* payloadObjName, const uint8_t* PayloadBin, uint8_t PayloadLen, AnsCallback_t acb);
int r_post_ans_to_ns(requester_t** rp, const char* result, json_object*);
role_e parseDLMetaData(json_object* obj, DLMetaData_t* out);
uint32_t now_ms(void);
void xRStartAnsCallback(MYSQL* sc, bool handover, mote_t* mote, json_object* jobj, const char* rxResult, const char* senderID, const uint8_t* rfBuf, uint8_t rfLen, bool fMICup, unsigned lifetime);
int saveDeviceProfile(MYSQL* sc, mote_t* mote, json_object* obj, const char* timeStamp);
const char* sql_motes_query_item(MYSQL* sc, uint64_t devEui, uint32_t devAddr, const char* colName, void* out);
#define MAX_FCNT_GAP        0x4000
const char* sql_motes_query(MYSQL* sc, uint64_t devEui, uint32_t devAddr, sql_t* out);
int mote_update_database_roam(MYSQL* sc, uint64_t devEui64, uint32_t devAddr, const char* state, const time_t* until, const uint32_t* roamingWithNetID, const bool* enable_fNS_MIC);
extern const char roamNONE[];   // is sNS
extern const char roamfPASSIVE[];
extern const char roamDEFERRED[];
extern const char roamsPASSIVE[];   // is sNS
extern const char roamsHANDOVER[];  // is sNS
extern const char roamhHANDOVER[];
json_object* generateDLMetaData(const ULMetaData_t *, uint8_t rx1dro, DLMetaData_t* dlmd, role_e from);
void dlmd_free(DLMetaData_t* dlmd);
int isNetID(MYSQL* sc, uint32_t NetID, const char* colName);
int getLifetime(MYSQL* sc, uint64_t devEui, uint32_t devAddr);

int add_database_session(
    MYSQL* sc,
    uint64_t devEui64,
    const time_t* until,
    const uint8_t* SNwkSIntKey_bin,
    const uint8_t* FNwkSIntKey_bin,
    const uint8_t* NwkSEncKey_bin,
    const uint32_t* FCntUp,
    const uint32_t* NFCntDown,
    const uint32_t* newDevAddr,
    bool OptNeg
);

json_object* generateULMetaData(const ULMetaData_t* md, role_e from, bool addGWMetadata);
int getDeviceProfileTimestamp(MYSQL* sc, const mote_t* mote, char* out, size_t sizeof_out, time_t* tout);
void answerJsonRequest(mote_t* mote, DLMetaData_t* dlMetaData, const uint8_t* rfBuf, uint8_t rfLen, bool OptNeg, const char* result, const uint8_t* keyBin, const uint8_t* SNwkSIntKey_bin, const uint8_t* FNwkSIntKey_bin, const uint8_t* NwkSEncKey_bin);
void save_uplink_start(mote_t* mote, uint8_t rx2_seconds, progress_e);
bool uplink_finish(mote_t* mote);
void save_uplink_from_direct(mote_t* mote, bool (*finish)(mote_t*), uint8_t rx2_seconds, /*const ULMetaData_t* ulmd_,*/ const GWInfo_t* gwInfo, const uint8_t* PHYPayloadBin, uint8_t PHYPayloadLen);
int deviceProfileReq(MYSQL* sc, uint64_t devEui, uint32_t devAddr, const char* elementName, char* out, size_t outSize);
void common_service(void);
bool isNetIDpassive(uint32_t NetID);


// uplinkJson() returns NULL for answer to be sent later
const char* uplinkJson(MYSQL* sc, unsigned long reqTid, const char* clientID, const struct sockaddr *, json_object* ulj, json_object* inJobj, const char* messageType, int frm, const uint8_t* payBuf, uint8_t payLen, json_object** ansJobj);
const char* downlinkJson(MYSQL* sc, unsigned long reqTid, const char* clientID, const struct sockaddr *, json_object* dlmdobj, json_object* inJobj, const char* messageType, int frm, const uint8_t* payBuf, uint8_t payLen, json_object** ansJobj);

mote_t* GetMoteMtype(MYSQL* sc, const uint8_t* PHYPayloadBin, bool *jReq);

typedef enum {
    DIR_UP = 0,
    DIR_DOWN = 1
} dir_e;

typedef union {
    struct {
        uint8_t header;
        uint16_t confFCnt;
        uint8_t dr;
        uint8_t ch;
        uint8_t dir;
        uint32_t DevAddr;
        uint32_t FCnt;
        uint8_t zero8;
        uint8_t lenMsg;
    } __attribute__((packed)) b;
    uint8_t octets[16];
} block_t;
uint32_t LoRa_GenerateDataFrameIntegrityCode(const block_t* block, const uint8_t* sNwkSIntKey, const uint8_t* pktPayload);

/* hNS.c: */
json_object * getAppSKeyEnvelope(MYSQL* sc, uint64_t devEui, uint32_t* outDevAddr, const char** res);
void hNS_service(mote_t* mote);
const char* hNS_XmitDataReq_down(mote_t* mote, unsigned long reqTid, const char* requester, const uint8_t* frmPayload, uint8_t frmPayloadLength, DLMetaData_t* dlmd, uint32_t dst);
//const char* hNS_XmitDataReq_toAS(mote_t* mote, const uint8_t* FRMPayloadBin, uint8_t FRMPayloadLen, json_object*);
const char* hNS_XmitDataReq_toAS(mote_t* mote, const uint8_t* FRMPayloadBin, uint8_t FRMPayloadLen, const ULMetaData_t*);
int hNS_toJS(mote_t* mote, const char* CFListStr);
void hNS_uplink_finish(mote_t* mote, sql_t* sql);
const char* hNS_to_AS(const mote_t* mote, const char* frmPayload);

/* sNS.c: */
void sNS_roamStop(mote_t* mote);
void sNSDownlinkSent(mote_t* mote, const char* result, json_object** httpdAnsJobj); // exported because PHYPayload is in [HP]RstartAns
void answer_app_downlink(mote_t* mote, const char* result, json_object** httpdAnsJobj); // exported for fNS_beacon_service
void LoRaMacBeaconComputePingOffset( uint64_t beaconTime, uint32_t address, uint16_t pingPeriod, uint16_t *pingOffset );
const char* sNS_answer_RStart_Success(mote_t* mote, json_object* ans_jobj);
void sNS_answer_RStart_Success_save(mote_t* mote);
uint8_t sNS_get_dlphylen(const mote_t* mote);
int sNS_force_rejoin(mote_t* mote, uint8_t type);
void sNS_service(mote_t* mote);
const char* sNS_finish_phy_downlink(mote_t* mote, const sql_t* sql, char classMode, json_object** httpdAnsJobj);

const char* hNS_to_sNS_downlink(MYSQL* sc, mote_t* mote, unsigned long reqtid, const char* requester, const uint8_t *pay, uint8_t paylen, const DLMetaData_t* dlmd, json_object** ansJobj);
int XmitDataReq_toAS(mote_t* mote, const uint8_t* FRMPayloadBin, uint8_t FRMPayloadLen);
int sNS_sendHRStartReq(MYSQL* sc, mote_t* mote, uint32_t homeNetID);
int sNS_band_conv(uint8_t, uint64_t devEui, uint32_t devAddr, float ULFreq, uint8_t ULDataRate, uint8_t rxdrOffset1, const char* ULRFRegion, DLMetaData_t* DLMetaData);
const char* sNS_uplink(mote_t* mote, const sql_t* sql, ULMetaData_t* ulmd, bool* discard);
const char* sNS_uplink_finish(mote_t* mote, bool, sql_t* sql, bool* discard);
int sNS_downlink(mote_t* mote, DLMetaData_t* dlMetaData, const uint8_t* rfBuf, uint8_t rfLen, const uint8_t* SNwkSIntKey_bin, const uint8_t* FNwkSIntKey_bin, const uint8_t* NwkSEncKey_bin);
int hNs_to_sNS_JoinAns(MYSQL* sc, mote_t* mote, json_object* jobj, const char* rxResult, const char* senderID, const uint8_t* rfBuf, uint8_t rfLen);


typedef struct {
    bool SupportsClassB;
    unsigned ClassBTimeout;
    unsigned PingSlotPeriod;
    unsigned PingSlotDR;
    float PingSlotFreq;
    bool SupportsClassC;
    unsigned ClassCTimeout;
    char RegParamsRevision[16];
    bool SupportsJoin;
    char MACVersion[32];
    unsigned RXDelay1;
    unsigned RXDROffset1;
    unsigned RXDataRate2;
    float RXFreq2;
    char FactoryPresetFreqs[96];
    int MaxEIRP;
    float MaxDutyCycle;
    char RFRegion[32];
    bool Supports32bitFCnt;
} DeviceProfile_t;

typedef struct {
    unsigned ULRate;
    unsigned ULBucketSize;
    char ULRatePolicy[16];
    unsigned DLRate;
    unsigned DLBucketSize;
    char DLRatePolicy[16];
    bool AddGWMetadata;
    unsigned DevStatusReqFreq;
    bool ReportDevStatusBattery;
    bool ReportDevStatusMargin;
    unsigned DRMin;
    unsigned DRMax;
    char ChannelMask[32];
    bool PRAllowed;
    bool HRAllowed;
    bool RAAllowed;
    bool NwkGeoLoc;
    float TargetPER;
    unsigned MinGWDiversity;
} ServiceProfile_t;

/* web.c: */
int insertDeviceProfile(MYSQL* sc, DeviceProfile_t* dp, my_ulonglong );
int updateDeviceProfile(MYSQL* sc, const DeviceProfile_t* dp, my_ulonglong id);
const char* jsonGetDeviceProfile(uint64_t devEui, const char* clientID, json_object** ansJobj);
json_object* jsonGetServiceProfile(uint64_t devEui, uint32_t devAddr);
int updateServiceProfile(const ServiceProfile_t* sp, my_ulonglong id);
int insertServiceProfile(const ServiceProfile_t* sp, my_ulonglong id);


