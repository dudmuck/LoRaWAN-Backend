#include "ns.h"
#include <float.h>  // FLT_MAX

const char RStopReq[] = "roamStop";
const char roamingUpdate[] = "roamingUpdate";


static MYSQL *sql_conn_ns_web;
struct Session *sessions;

enum {
    /* 0 */ OPR_NONE = 0,
    /* 1 */ OPR_CREATE,
    /* 2 */ OPR_DELETE,
    /* 3 */ OPR_EDIT,
    /* 5 */ OPR_ADD_PROFILES,
    /* 6 */ OPR_RSTOP,
};

typedef struct {
    uint8_t op : 4; // 0,1,2,3
    uint8_t addMote : 1;    // 4
    uint8_t addProfiles: 1; // 5
    uint8_t enable: 1;  // 6
    uint8_t ota: 1; // 7
} opFlags_t;

typedef enum {
    FORM_STATE_START,
    FORM_STATE_TABLE_HEADER,
    FORM_STATE_TABLE_ROWS,
    FORM_STATE_FORM,
    FORM_STATE_END
} form_state_e;

struct elements {
    const char* form;
    const char* element;
    const char* notes;
};

typedef struct {
    form_state_e form_state;
    MYSQL_RES *result;
    MYSQL_ROW row;
    unsigned i;

    char DevEUIstr[40];    // hex in, saved as base10
    char devAddrStr[16];   // hex in, saved as base10
    char NwkAddrStr[16];
    opFlags_t opFlags;
    const struct elements* es;

    char SNwkSIntKey[64];
    char FNwkSIntKey[64];
    char NwkSEncKey[64];

    DeviceProfile_t DeviceProfile;
    ServiceProfile_t ServiceProfile;

    struct { 
        char AS_ID[64];
    } RoutingProfile;

    my_ulonglong moteID;
    char target[64];    // devEui + devAddr combo

    struct {
        uint32_t NetID;
        bool fMICup;
        char KEKlabel[129];
        char KEKstr[33];
    } network;

    uint32_t Lifetime;
    char where[128];
    char NetIDstr[12];

    uint64_t gatewayID;
    const char* RFRegion;

    char redir_url[128];
} appInfo_t;

void parse_action(const char* inStr, void *dest, appInfo_t* ai)
{
    if (0 == strcmp(inStr, "create")) {
        ai->opFlags.op = OPR_CREATE;
        ai->opFlags.addMote = 1;
        ai->opFlags.addProfiles = 0;
    } else if (0 == strcmp(inStr, "createHome")) {
        ai->opFlags.op = OPR_CREATE;
        ai->opFlags.addMote = 1;
        ai->opFlags.addProfiles = 1;
    } else
        printf("\e[31munknown \"%s\"\e[0m\n", inStr);

    printf("parse_action(%s,,) op%u\n", inStr, ai->opFlags.op);
}

/* for text entry */
void parse_string_hextodec(const char* inStr, void *dest, appInfo_t* ai)
{
    uint64_t u64;
    if (inStr[0]) {
        //printf("parse_string_hextodec(\"%s\",) ", inStr);
        sscanf(inStr, "%"PRIx64, &u64);
        sprintf(dest, "%"PRIu64, u64);
        //printf("-> \"%s\"\n", (char*)dest);
    }
}

void parse_string(const char* inStr, void *dest, appInfo_t* ai)
{
    strcpy(dest, inStr);    // TODO strncpy
    //printf("parse_string dest \"%s\"\n", (char*)dest);
}

void parse_float(const char* inStr, void *dest, appInfo_t* ai)
{
    sscanf(inStr, "%f", (float*)dest);
}

void parse_signed(const char* inStr, void *dest, appInfo_t* ai)
{
    sscanf(inStr, "%d", (int*)dest);
}

void parse_unsigned(const char* inStr, void *dest, appInfo_t* ai)
{
    sscanf(inStr, "%u", (unsigned*)dest);
}

void parse_bool(const char* inStr, void *dest, appInfo_t* ai)
{
    bool* bp = dest;
    *bp = true;
}

void parse_x32(const char* inStr, void *dest, appInfo_t* ai)
{
    sscanf(inStr, "%x", (unsigned*)dest);
}

void parse_x64(const char* inStr, void *dest, appInfo_t* ai)
{
    sscanf(inStr, "%"PRIx64, (uint64_t*)dest);
}

void parse_u64(const char* inStr, void *dest, appInfo_t* ai)
{
    sscanf(inStr, "%"PRIu64, (uint64_t*)dest);
}

struct parsed_s {
    const char* name;
    void (*cb)(const char* inStr, void *dest, appInfo_t*);
    size_t offset;
};


const struct parsed_s key_actions[] = {
    /* names compared to key */
    { "DevEUI", parse_string_hextodec, offsetof(appInfo_t, DevEUIstr) },
    { "DevAddr", parse_string_hextodec, offsetof(appInfo_t, devAddrStr) },
    { "NwkAddr", parse_string, offsetof(appInfo_t, NwkAddrStr) },
    { "action", parse_action, offsetof(appInfo_t, opFlags) },
    { "moteID", parse_u64, offsetof(appInfo_t, moteID) },
    { "NetID", parse_string, offsetof(appInfo_t, NetIDstr) },


    { Lifetime, parse_unsigned, offsetof(appInfo_t, Lifetime) },

    { SNwkSIntKey, parse_string, offsetof(appInfo_t, SNwkSIntKey) },
    { FNwkSIntKey, parse_string, offsetof(appInfo_t, FNwkSIntKey) },
    { NwkSEncKey, parse_string, offsetof(appInfo_t, NwkSEncKey) },

    { SupportsClassB, parse_bool, offsetof(appInfo_t, DeviceProfile.SupportsClassB) },
    { ClassBTimeout, parse_unsigned, offsetof(appInfo_t, DeviceProfile.ClassBTimeout) },
    { PingSlotPeriod, parse_unsigned, offsetof(appInfo_t, DeviceProfile.PingSlotPeriod) },
    { PingSlotDR, parse_unsigned, offsetof(appInfo_t, DeviceProfile.PingSlotDR) },
    { PingSlotFreq, parse_float, offsetof(appInfo_t, DeviceProfile.PingSlotFreq) },
    { SupportsClassC, parse_bool, offsetof(appInfo_t, DeviceProfile.SupportsClassC) },
    { ClassCTimeout, parse_unsigned, offsetof(appInfo_t, DeviceProfile.ClassCTimeout) },
    { RegParamsRevision, parse_string, offsetof(appInfo_t, DeviceProfile.RegParamsRevision) },
    { SupportsJoin, parse_bool, offsetof(appInfo_t, DeviceProfile.SupportsJoin) },
    { MACVersion, parse_string, offsetof(appInfo_t, DeviceProfile.MACVersion) },
    { RXDelay1, parse_unsigned, offsetof(appInfo_t, DeviceProfile.RXDelay1) },
    { RXDROffset1, parse_unsigned, offsetof(appInfo_t, DeviceProfile.RXDROffset1) },
    { RXDataRate2, parse_unsigned, offsetof(appInfo_t, DeviceProfile.RXDataRate2) },
    { RXFreq2, parse_float, offsetof(appInfo_t, DeviceProfile.RXFreq2) },
    { FactoryPresetFreqs, parse_string, offsetof(appInfo_t, DeviceProfile.FactoryPresetFreqs) },
    { MaxEIRP, parse_signed, offsetof(appInfo_t, DeviceProfile.MaxEIRP) },
    { MaxDutyCycle, parse_float, offsetof(appInfo_t, DeviceProfile.MaxDutyCycle) },
    { RFRegion, parse_string, offsetof(appInfo_t, DeviceProfile.RFRegion) },
    { Supports32bitFCnt, parse_bool, offsetof(appInfo_t, DeviceProfile.Supports32bitFCnt) },


    { ULRate, parse_unsigned, offsetof(appInfo_t, ServiceProfile.ULRate) },
    { ULBucketSize, parse_unsigned, offsetof(appInfo_t, ServiceProfile.ULBucketSize) },
    { ULRatePolicy, parse_string, offsetof(appInfo_t, ServiceProfile.ULRatePolicy) },
    { DLRate, parse_unsigned, offsetof(appInfo_t, ServiceProfile.DLRate) },
    { DLBucketSize, parse_unsigned, offsetof(appInfo_t, ServiceProfile.DLBucketSize) },
    { DLRatePolicy, parse_string, offsetof(appInfo_t, ServiceProfile.DLRatePolicy) },
    { AddGWMetadata, parse_bool, offsetof(appInfo_t, ServiceProfile.AddGWMetadata) },
    { DevStatusReqFreq, parse_unsigned, offsetof(appInfo_t, ServiceProfile.DevStatusReqFreq) },
    { ReportDevStatusBattery, parse_bool, offsetof(appInfo_t, ServiceProfile.ReportDevStatusBattery) },
    { ReportDevStatusMargin, parse_bool, offsetof(appInfo_t, ServiceProfile.ReportDevStatusMargin) },
    { DRMin, parse_unsigned, offsetof(appInfo_t, ServiceProfile.DRMin) },
    { DRMax, parse_unsigned, offsetof(appInfo_t, ServiceProfile.DRMax) },
    { ChannelMask, parse_string, offsetof(appInfo_t, ServiceProfile.ChannelMask) },
    { PRAllowed, parse_bool, offsetof(appInfo_t, ServiceProfile.PRAllowed) },
    { HRAllowed, parse_bool, offsetof(appInfo_t, ServiceProfile.HRAllowed) },
    { RAAllowed, parse_bool, offsetof(appInfo_t, ServiceProfile.RAAllowed) },
    { NwkGeoLoc, parse_bool, offsetof(appInfo_t, ServiceProfile.NwkGeoLoc) },
    { TargetPER, parse_float, offsetof(appInfo_t, ServiceProfile.TargetPER) },
    { MinGWDiversity, parse_unsigned, offsetof(appInfo_t, ServiceProfile.MinGWDiversity) },

    { AS_ID, parse_string, offsetof(appInfo_t, RoutingProfile.AS_ID) },

    { NULL, NULL, 0 },
};

void parse_homeAdd(const char* inStr, void *dest, appInfo_t* ai)
{
    strcpy(dest, inStr);    // TODO strncpy
    ai->opFlags.op = OPR_ADD_PROFILES;
    ai->opFlags.addMote = 0;
    ai->opFlags.addProfiles = 1;
}


void parse_roamingUpdate(const char* inStr, void *dest, appInfo_t* ai)
{
    strcpy(dest, inStr);    // TODO strncpy
    ai->opFlags.op = OPR_NONE;  // update operation from post url
    ai->opFlags.addMote = 0;
    ai->opFlags.addProfiles = 0;
}

void parse_RStopReq(const char* inStr, void *dest, appInfo_t* ai)
{
    strcpy(dest, inStr);    // TODO strncpy
    ai->opFlags.op = OPR_RSTOP;
    ai->opFlags.addMote = 0;
    ai->opFlags.addProfiles = 0;
}

void parse_delete(const char* inStr, void *dest, appInfo_t* ai)
{
    strcpy(dest, inStr);    // TODO strncpy
    ai->opFlags.op = OPR_DELETE;
}


const struct parsed_s data_actions[] = {
    /* names compared to data */
    { "delete", parse_delete, offsetof(appInfo_t, target) },
    { "homeAdd", parse_homeAdd, offsetof(appInfo_t, target) },

    { roamingUpdate, parse_roamingUpdate, offsetof(appInfo_t, target) },
    { RStopReq, parse_RStopReq, offsetof(appInfo_t, target) },

    { NULL, NULL, 0 },
};

const char* fwdToNetIDnone = "NULL";

void
browser_post_init(struct Session *session)
{
    appInfo_t* ai = session->appInfo;

    ai->opFlags.op = OPR_NONE;

    /* unchecked checkbox items are not posted, must clear them here at start */ 

    ai->DeviceProfile.SupportsClassB = 0;
    ai->DeviceProfile.SupportsClassC = 0;
    ai->DeviceProfile.Supports32bitFCnt = 0;

    ai->ServiceProfile.AddGWMetadata = 0;
    ai->ServiceProfile.ReportDevStatusBattery = 0;
    ai->ServiceProfile.ReportDevStatusMargin = 0;
    ai->ServiceProfile.PRAllowed = 0;
    ai->ServiceProfile.HRAllowed = 0;
    ai->ServiceProfile.RAAllowed = 0;
    ai->ServiceProfile.NwkGeoLoc = 0;

    ai->network.fMICup = false;
    ai->network.KEKlabel[0] = 0;
    ai->network.KEKstr[0] = 0;

    ai->DevEUIstr[0] = 0;
    ai->devAddrStr[0] = 0;
    ai->NwkAddrStr[0] = 0;

    ai->RFRegion = NULL;

    HTTP_PRINTF("\e[33mbrowser_post_init()\e[0m\n");
}

const char neditURL[] = "/nedit";
const char peditURL[] = "/pedit";
const char networksURL[] = "/networks";
const char gatewaysURL[] = "/gateways";
const char motesURL[] = "/motes";
const char postFailURL[] = "/postFail";
const char addProfilesToNewURL[] = "/addProfilesToNewURL";
const char addProfilesToExistingURL[] = "/addProfilesToExistingURL";
const char updateProfilesURL[] = "/updateProfilesURL";
const char sKeyEditURL[] = "/sKeyEdit";
const char roamingURL[] = "/roaming";
const char redirectURL[] = "/redir";

static void
changed_region(const char* idStr, const char* rfRegion, appInfo_t* ai)
{
    char query[512];

    sprintf(query, "SELECT RFRegion FROM gateways WHERE eui = %s", idStr);
    if (mysql_query(sql_conn_ns_web, query)) {
        printf("\e[31m%s: %s\e[0m\n", query, mysql_error(sql_conn_ns_web));
        return;
    }
    ai->result = mysql_use_result(sql_conn_ns_web);
    if (ai->result) {
        ai->row = mysql_fetch_row(ai->result);
        if (ai->row) {
            if (strcmp(rfRegion, ai->row[0]) != 0) {
                sscanf(idStr, "%"PRIu64, &ai->gatewayID);
                ai->RFRegion = getRFRegion(rfRegion);
                //printf("changed gateway region %"PRIx64", %s\n", ai->gatewayID, ai->RFRegion);
            }
        }
        mysql_free_result(ai->result);
    }
}

/**
 * Iterator over key-value pairs where the value
 * maybe made available in increments and/or may
 * not be zero-terminated.  Used for processing
 * POST data.
 *
 * @param cls user-specified closure
 * @param kind type of the value
 * @param key 0-terminated key for the value
 * @param filename name of the uploaded file, NULL if not known
 * @param content_type mime-type of the data, NULL if not known
 * @param transfer_encoding encoding of the data, NULL if not known
 * @param data pointer to size bytes of data at the
 *              specified offset
 * @param off offset of data in the overall value
 * @param size number of bytes in data available
 * @return MHD_YES to continue iterating,
 *         MHD_NO to abort the iteration
 */
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
    char* aicp = (char*)session->appInfo;
    appInfo_t* ai = session->appInfo;
    unsigned i;

#if 0//HTTP_DEBUG
    switch (kind) {
        case MHD_RESPONSE_HEADER_KIND: printf("rh "); break;
        case MHD_HEADER_KIND: printf("header "); break;
        case MHD_COOKIE_KIND: printf("cookie "); break;
        case MHD_POSTDATA_KIND: printf("postdata "); break;
        case MHD_GET_ARGUMENT_KIND: printf("getArg "); break;
        case MHD_FOOTER_KIND: printf("footer "); break;
    }
#endif /* HTTP_DEBUG */

    HTTP_PRINTF("post_iterator(%zu) url:\"%s\" key:\"%s\" data:\"%s\"\n", size, request->post_url, key, data);
    if (size == 0)
        return MHD_YES;

    if (request->post_url) {
           if (strcmp(request->post_url, networksURL) == 0) {
            if (strcmp(key, "NetID") == 0)
                sscanf(data, "%x", &ai->network.NetID);
            else if (strcmp(key, PRAllowed) == 0)
                ai->ServiceProfile.PRAllowed = true;
            else if (strcmp(key, HRAllowed) == 0)
                ai->ServiceProfile.HRAllowed = true;
            else if (strcmp(key, RAAllowed) == 0)
                ai->ServiceProfile.RAAllowed = true;
            else if (strcmp(key, "fMICup") == 0)
                ai->network.fMICup = true;
            else if (strcmp(key, "KEKlabel") == 0)
                strncpy(ai->network.KEKlabel, data, sizeof(ai->network.KEKlabel));
            else if (strcmp(key, "KEK") == 0)
                strncpy(ai->network.KEKstr, data, sizeof(ai->network.KEKstr));
            else if (strcmp(key, "add") == 0)
                ai->opFlags.op = OPR_CREATE;
            else if (strcmp(key, "change") == 0)
                ai->opFlags.op = OPR_EDIT;
            else if (strcmp(data, "delete") == 0) {
                printf("delete %s\n", key);
                ai->opFlags.op = OPR_DELETE;
                sscanf(key, "%u", &ai->network.NetID);
            } else
                printf("\e[31mtodo key:%s, data:%s\e[0m\n", key, data);
            return MHD_YES;
        } else if (strcmp(request->post_url, gatewaysURL) == 0) {
            if (strncmp(key, "region_", 7) == 0) {
                changed_region(key+7, data, ai);
                return MHD_YES;
            }
        }
    }

    for (i = 0; key_actions[i].name != NULL; i++) {
        if (0 == strcmp(key_actions[i].name, key)) {
            HTTP_PRINTF("key action \"%s\" cb \"%s\", offset:%zu\n", key, data, key_actions[i].offset);
            key_actions[i].cb(data, aicp + key_actions[i].offset, ai);
            return MHD_YES;
        }
    }

    for (i = 0; data_actions[i].name != NULL; i++) {
        if (0 == strcmp(data_actions[i].name, data)) {
            data_actions[i].cb(key, aicp + data_actions[i].offset, ai);
            return MHD_YES;
        }
    }

    HTTP_PRINTF("\e[31mUnsupported form value `%s'\e[0m\n", key);

    return MHD_YES;
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
fill_v1_form (const void *cls,
	      const char *mime,
	      struct Session *session,
	      struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;
    char body[1024];

    sprintf(body, "<html><head><title>NS %06x</title></head><body><h3>Network Server %06x</h3><a href=\"%s\">motes</a><br><a href=\"%s\">networks</a> roaming policy<br><a href=\"%s\">gateways</a></body></html>", myNetwork_id32, myNetwork_id32, motesURL, networksURL, gatewaysURL);
    response = MHD_create_response_from_buffer (strlen (body), body, MHD_RESPMEM_MUST_COPY);
    if (NULL == response)
        return MHD_NO;

    lib_add_session_cookie (session, response);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_ENCODING, mime);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}

/* dropdown selection of foreign netids */
static void
create_netid_select(char* buf, const char* curNetID)
{
    char query[512];
    MYSQL_RES *result;
    MYSQL_ROW row;

    //printf("create_netid_select(,%s)\n", curNetID);
    sprintf(query, "SELECT NetID FROM roaming");
    if (mysql_query(sql_conn_ns_web, query)) {
        strcat(buf, query);
        strcat(buf, " : ");
        strcat(buf, mysql_error(sql_conn_ns_web));
        return;
    }
    result = mysql_use_result(sql_conn_ns_web);
    if (result) {
        uint32_t curNetID32;
        sscanf(curNetID, "%u", &curNetID32);
        strcat(buf, "<select name=\"NetID\"><option value=\"");
        strcat(buf, fwdToNetIDnone);
        strcat(buf, "\">none</option><option value=\"");
        sprintf(query, "%u", NONE_NETID);
        strcat(buf, query);
        if (curNetID32 == NONE_NETID)
            strcat(buf, "\" selected>");
        else
            strcat(buf, "\">");
        strcat(buf, "HomeNSReq</option>");
        while ((row = mysql_fetch_row(result))) {
            char netidstr[24];
            unsigned n;
            sscanf(row[0], "%u", &n);
            sprintf(netidstr, "%06x", n);
            strcat(buf, "<option value=\"");
            strcat(buf, row[0]);
            //printf("comparing \"%s\" to \"%s\"\n", row[0], curNetID);
            if (strcmp(row[0], curNetID) == 0)   // moteStr has currently selected foreign NetID
                strcat(buf, "\" selected>");
            else
                strcat(buf, "\">");
            strcat(buf, netidstr);
            strcat(buf, "</option>");
        }
        mysql_free_result(result);
        strcat(buf, "</select>");
    } else
        strcat(buf, "no result");
}

const char gw_css[] =
"table.gateways{"
"    font-family: Arial;"
"    font-size: 13pt;"
"    border-width: 1px 1px 1px 1px;"
"    border-style: solid;"
"}    "
""
"table.gateways td {"
"    text-align: left;"
"    border-width: 1px 1px 1px 1px;"
"    border-style: solid;"
"    padding: 5px;"
"}    "
""
"table.gateways td:nth-child(1) {"
"    font-family: monospace;"
"    font-size: 12pt;"
"}";
//"    width: 300;"

const char* const region_strs[] = {
    EU868,
    US902,
    China470,
    China779,
    EU433,
    Australia915,
    AS923,
    KR922,
    IN865,
    RU868,
    NULL
};

static ssize_t
gateways_page_iterator(void *cls,
          uint64_t pos,
          char *buf,
          size_t max)
{
    char query[512];
    struct Session *session = cls;
    appInfo_t* ai = session->appInfo;
    int n, len = 0;
    uint64_t id;

    switch (ai->form_state) {
        case FORM_STATE_START:
            strcpy(buf, "<html><head><title>NS ");
            strcat(buf, myNetwork_idStr);
            strcat(buf, "</title><style>");
            strcat(buf, gw_css);
            strcat(buf, "</style></head><body><a href=\"/\">top</a><h3>Gateways</h3><form method=\"post\" action=\"");
            strcat(buf, gatewaysURL);
            strcat(buf, "\"><table class=\"gateways\" border=\"1\">\n");
            len = strlen(buf);
            ai->form_state = FORM_STATE_TABLE_HEADER;
            break;
        case FORM_STATE_TABLE_HEADER:
            sprintf(query, "SELECT eui, RFRegion, UNIX_TIMESTAMP(time) FROM gateways ORDER BY time DESC");
            if (mysql_query(sql_conn_ns_web, query)) {
                len = sprintf(buf, "<tr>gateways_page_iterator %s</tr>", mysql_error(sql_conn_ns_web));
                ai->result = NULL;
                ai->form_state = FORM_STATE_FORM;
                break;
            }
            ai->form_state = FORM_STATE_TABLE_ROWS;
            ai->result = mysql_use_result(sql_conn_ns_web);
            if (ai->result == NULL) {
                len = sprintf(buf, "<tr>no result</tr>");
                ai->form_state = FORM_STATE_FORM;
                break;
            }
            len = sprintf(buf, "<tr><th>ID</th><th>RFRegion</th><th>last seen</th></tr>\n");
            ai->row = mysql_fetch_row(ai->result);
            if (ai->row)
                ai->form_state = FORM_STATE_TABLE_ROWS;
            else
                ai->form_state = FORM_STATE_FORM;
            break;
        case FORM_STATE_TABLE_ROWS:
            strcpy(buf, "<tr><td>");
            if (!ai->row[0]) {
                strcat(buf, "no id</td></tr>");
                goto next;
            }
            sscanf(ai->row[0], "%"PRIu64, &id);
            sprintf(query, "%"PRIx64, id);
            strcat(buf, query);
            strcat(buf, "</td><td><select onchange=\"this.form.submit()\" name=\"region_");
            strcat(buf, ai->row[0]);
            strcat(buf, "\">");
            for (n = 0; region_strs[n]; n++) {
                strcat(buf, "<option value=\"");
                strcat(buf, region_strs[n]);
                if (ai->row[1] && strcmp(ai->row[1], region_strs[n]) == 0)
                    strcat(buf, "\" selected>");
                else
                    strcat(buf, "\">");
                strcat(buf, region_strs[n]);
                strcat(buf, "</option>");
            }
            strcat(buf, "</select></td><td>");
            if (ai->row[2]) {
                getAgo(ai->row[2], query);
                strcat(buf, query);
            }
            strcat(buf, "</td></tr>\n");
next:
            len = strlen(buf);
            ai->row = mysql_fetch_row(ai->result);
            if (ai->row == NULL)
                ai->form_state = FORM_STATE_FORM;
            break;
        case FORM_STATE_FORM:
            if (ai->result)
                mysql_free_result(ai->result);
            len = sprintf(buf, "</table></form></body></html>");
            ai->form_state = FORM_STATE_END;
            break;
        case FORM_STATE_END:
            return MHD_CONTENT_READER_END_OF_STREAM;
    } // ..switch (ai->form_state)

    return len;
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
    char lora_session[48];
    char name[64];
    char query[2048];
    uint32_t devAddr;
    uint64_t devEui;
    struct Session *session = cls;
    appInfo_t* ai = session->appInfo;
    //static unsigned num_fields;

    switch (ai->form_state) {
        case FORM_STATE_START:
            len = sprintf(buf, "<html><head><title>NS %s</title><style>%s</style></head><body><a href=\"/\">top</a><h3>Network Server %s motes</h3>OTA mode: DevEUI is provided.  If <b>forward to NetID</b> is blank (none), then this NS can be home NS<br>ABP mode: DevEUI is blank<form id=\"form\" method=\"post\" action=\"%s\"><table class=\"motes\" border=\"1\">\n", myNetwork_idStr, mote_css, myNetwork_idStr, motesURL);
            ai->form_state = FORM_STATE_TABLE_HEADER;
            break;
        case FORM_STATE_TABLE_HEADER:
            sprintf(query, "SELECT motes.fwdToNetID, motes.DevEUI, sessions.DevAddr, DeviceProfiles.DeviceProfileID, UNIX_TIMESTAMP(sessions.Until) FROM motes LEFT JOIN sessions ON motes.ID = sessions.ID LEFT JOIN DeviceProfiles ON motes.ID = DeviceProfiles.DeviceProfileID ORDER BY sessions.createdAt DESC");
            if (mysql_query(sql_conn_ns_web, query)) {
                len = sprintf(buf, "<tr><th>%s</th</tr><tr><th>%s</th></tr>", query, mysql_error(sql_conn_ns_web));
                ai->result = NULL;
                ai->form_state = FORM_STATE_FORM;
                break;
            }
            ai->result = mysql_use_result(sql_conn_ns_web);
            if (ai->result == NULL) {
                len = sprintf(buf, "<tr>no result</tr>");
                ai->form_state = FORM_STATE_FORM;
                break;
            }
            len = sprintf(buf, "<tr><th>forward to<br>NetID</th><th>DevEUI</th><th>DevAddr</th><th></th><th>Home<br>Profile</th><th>session<br>expiry</th></tr>\n");
            //num_fields = mysql_num_fields(ai->result);
            ai->row = mysql_fetch_row(ai->result);
            if (ai->row)
                ai->form_state = FORM_STATE_TABLE_ROWS;
            else {
                printf("motes no rows\n");
                ai->form_state = FORM_STATE_FORM;
            }
            break;
        case FORM_STATE_TABLE_ROWS:
            /*for (len = 0; len < num_fields; len++)
                printf("%s, ", ai->row[len]);
            printf("   --- num_fields %u\n", num_fields);*/


            lora_session[0] = 0;
            if (ai->row[1]) {
                sscanf(ai->row[1], "%"PRIu64, &devEui);
                if (ai->row[4]) {
                    getAgo(ai->row[4], lora_session);
                } else
                    lora_session[0] = 0;
            } else
                devEui = 0;

            if (ai->row[2]) {
                if (ai->row[1] == NULL) { // this is ABP mote
                    sprintf(lora_session, "<a href=\"%s/%s\">SKeys</a>", sKeyEditURL, ai->row[2]);
                }
                sscanf(ai->row[2], "%u", &devAddr);
            } else {
                devAddr = 0;
            }

            if (ai->row[1] && ai->row[2]) {   // both deveui and devaddr
                sprintf(name, "%"PRIu64"_%u", devEui, devAddr);
            } else if (ai->row[1]) {    // only deveui
                sprintf(name, "%"PRIu64"_", devEui);
            } else if (ai->row[2]) {    // only devaddr
                sprintf(name, "_%u", devAddr);
            } else {
                name[0] = 0;
            }

            strcpy(buf, "<tr><td>");
            if (ai->row[0]) {
                uint32_t nid;
                sscanf(ai->row[0], "%u", &nid);
                if (nid == NONE_NETID)
                    strcat(buf, HomeNSReq);
                else {
                    sprintf(query, "%06x", nid);
                    strcat(buf, query);
                }
            }
            strcat(buf, "</td><td>");
            if (ai->row[1]) {
                sscanf(ai->row[1], "%"PRIu64, &devEui);
                sprintf(query, "%016"PRIx64, devEui);
                strcat(buf, query);
            }
            strcat(buf, "</td><td>");
            if (ai->row[2]) {
                sscanf(ai->row[2], "%u", &devAddr);
                sprintf(query, "%08x", devAddr);
                strcat(buf, query);
            }
            strcat(buf, "</td><td><input type=\"submit\" name=\"");
            strcat(buf, name);
            strcat(buf, "\" value=\"delete\"></td><td>");

            // if device profile ID is equal to mote ID, then mote has profile
            if (ai->row[3]) {
                strcat(buf, "<a href=\"");
                strcat(buf, peditURL);
                strcat(buf, "/");
                sprintf(query, ai->row[3]);
                strcat(buf, query);
                strcat(buf, "\">edit</a>");
            } else {
                strcat(buf, "<input type=\"submit\" name=\"");
                strcat(buf, name);
                strcat(buf, "\" value=\"homeAdd\"/>");
            }

            strcat(buf, "</td><td>");
            strcat(buf, lora_session);
            strcat(buf, "</td><td><a href=\"");
            strcat(buf, roamingURL);
            strcat(buf, "/");
            strcat(buf, name);
            strcat(buf, "\">roaming</a></td></tr>\n");
            len = strlen(buf);
//nextRow:
            ai->row = mysql_fetch_row(ai->result);
            if (ai->row)
                ai->form_state = FORM_STATE_TABLE_ROWS;
            else
                ai->form_state = FORM_STATE_FORM;
            break;
        case FORM_STATE_FORM:
            if (ai->result)
                mysql_free_result(ai->result);
            strcat(buf, "<tr><td>");
            create_netid_select(buf, "none");
            strcat(buf, "</td>");
            sprintf(query, "<td><input type=\"text\" name=\"DevEUI\"/ size=\"17\" placeholder=\"OTA DevEUI\"></td><td><input type=\"text\" name=\"NwkAddr\" size=\"10\" placeholder=\"ABP NwkAddr 0 to 0x%x\"/></td><td><input type=\"submit\" value=\"create\" name=\"action\"></td><td><input type=\"submit\" value=\"createHome\" name=\"action\"></td></tr></table></form></body></html>", (1 << nwkAddrBits)-1);
            strcat(buf, query);
            len = strlen(buf);
            ai->form_state = FORM_STATE_END;
            break;
        case FORM_STATE_END:
            return MHD_CONTENT_READER_END_OF_STREAM;
    } // ..switch (ai->form_state)

    return len;
}


static int
serve_motes(const void *cls,
	      const char *mime,
	      struct Session *session,
	      struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;
    appInfo_t* ai = session->appInfo;

    ai->form_state = FORM_STATE_START;
    response = MHD_create_response_from_callback (
        MHD_SIZE_UNKNOWN, 1024, &motes_page_iterator, session, NULL
    );
    lib_add_session_cookie (session, response);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}


static ssize_t
networks_page_iterator(void *cls,
          uint64_t pos,
          char *buf,
          size_t max)
{
    struct Session *session = cls;
    appInfo_t* ai = session->appInfo;
    int len = 0;
    uint32_t netid;
    char query[512];

    //printf("form state:%d\n", ai->form_state);
    switch (ai->form_state) {
        case FORM_STATE_START:
            len = sprintf(buf, "<html><head><title>NS %s</title></head><body><a href=\"/\">top</a><h3>Network Server %s roaming policy</h3><form id=\"form\" method=\"post\" action=\"%s\"><table border=\"1\">\n", myNetwork_idStr, myNetwork_idStr, networksURL);
            ai->form_state = FORM_STATE_TABLE_HEADER;
            break;
        case FORM_STATE_TABLE_HEADER:
            sprintf(query, "SELECT NetID, PRAllowed, HRAllowed, RAAllowed, fMICup FROM roaming");
            if (mysql_query(sql_conn_ns_web, query)) {
                len = sprintf(buf, "<tr>mysql_query() %s</tr>", mysql_error(sql_conn_ns_web));
                ai->result = NULL;
                ai->form_state = FORM_STATE_FORM;
                break;
            }
            ai->result = mysql_use_result(sql_conn_ns_web);
            if (ai->result == NULL) {
                len = sprintf(buf, "<tr>no result</tr>");
                ai->form_state = FORM_STATE_FORM;
                break;
            }
            len = sprintf(buf, "<tr><th>NetID</th><th>passive allowed</th><th>handover allowed</th><th>activation allowed</th><th>fMICup</th></tr>\n");
            ai->row = mysql_fetch_row(ai->result);
            if (ai->row)
                ai->form_state = FORM_STATE_TABLE_ROWS;
            else
                ai->form_state = FORM_STATE_FORM;
            break;
        case FORM_STATE_TABLE_ROWS:
            sscanf(ai->row[0], "%u", &netid);
            len = sprintf(buf, "<tr><td align=\"center\">%06x</td><td align=\"center\">%s</td><td align=\"center\">%s</td><td align=\"center\">%s</td><td align=\"center\">%s</td><td><input type=\"submit\" name=\"%u\" value=\"delete\"></td><td><a href=\"%s/%u\">edit</a></td></tr>\n", netid, ai->row[1], ai->row[2], ai->row[3], ai->row[4], netid, neditURL, netid);
            ai->row = mysql_fetch_row(ai->result);
            if (ai->row)
                ai->form_state = FORM_STATE_TABLE_ROWS;
            else
                ai->form_state = FORM_STATE_FORM;
            break;
        case FORM_STATE_FORM:
            len = sprintf(buf, "<tr>"
"<td><input type=\"text\" name=\"NetID\" size=\"8\"></td>"
"<td align=\"center\"><input type=\"checkbox\" name=\"PRAllowed\"></td>"
"<td align=\"center\"><input type=\"checkbox\" name=\"HRAllowed\"></td>"
"<td align=\"center\"><input type=\"checkbox\" name=\"RAAllowed\"></td>"
"<td align=\"center\"><input type=\"checkbox\" name=\"fMICup \" ></td>"
"<td align=\"center\"><input type=\"submit\" value=\"add\" name=\"add\" placeholder=\"hex\"></td>"
"</tr></table></form></body></html>");
            ai->form_state = FORM_STATE_END;
            break;
        case FORM_STATE_END:
            if (ai->result)
                mysql_free_result(ai->result);
            return MHD_CONTENT_READER_END_OF_STREAM;
    } // ..switch (ai->form_state)

    return len;
}

static int
serve_gateways(const void *cls,
	      const char *mime,
	      struct Session *session,
	      struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;
    appInfo_t* ai = session->appInfo;

    ai->form_state = FORM_STATE_START;
    response = MHD_create_response_from_callback (
        MHD_SIZE_UNKNOWN, 1024, &gateways_page_iterator, session, NULL
    );
    lib_add_session_cookie (session, response);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}

static int
serve_networks(const void *cls,
	      const char *mime,
	      struct Session *session,
	      struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;
    appInfo_t* ai = session->appInfo;

    ai->form_state = FORM_STATE_START;
    response = MHD_create_response_from_callback (
        MHD_SIZE_UNKNOWN, 1024, &networks_page_iterator, session, NULL
    );
    lib_add_session_cookie (session, response);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}

/**
 * Invalid URL page.
 */
#define NOT_FOUND_ERROR "<html><head><title>Not found</title></head><body>NS invalid-url</body></html>"
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


#define CB_TEST "<html><head><title>cb test</title></head><body> <form id=\"form\" method=\"post\" action=\"\"> <input type=\"checkbox\" onchange=\"$('#form').submit();\"  name=\"checkbox\" class=\"checkbox\"/> </form> </body></html>"
//#define CB_TEST "<html><head><title>cb test</title></head><body> <form id=\"form\" method=\"post\" action=\"\"> <input type=\"checkbox\" name=\"checkbox\" class=\"checkbox\"/> </form> <script type=\"text/javascript\">  $(function(){ $('.checkbox').on('change',function(){ $('#form').submit(); }); }); </script> </body></html>"
static int
serve_cb(const void *cls,
		const char *mime,
		struct Session *session,
		struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;

    response = MHD_create_response_from_buffer (strlen (CB_TEST), (void *) CB_TEST, MHD_RESPMEM_PERSISTENT);
    if (NULL == response)
        return MHD_NO;
    lib_add_session_cookie (session, response);
    ret = MHD_queue_response (connection, MHD_HTTP_NOT_FOUND, response);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_ENCODING, mime);
    MHD_destroy_response (response);
    return ret;
}

const struct elements esDeviceProfile[] = {
    { "<input type=\"checkbox\" name=\"%s\" value=\"set\"%s/>", SupportsClassB, "End-Device supports Class B" },
    { "<input type=\"text\" name=\"%s\" placeholder=\"seconds\"%s/>", ClassBTimeout, "Maximum delay for the End-Device to answer a MAC request or a confirmed DL frame (mandatory if class B mode supported)" },
    { "<input type=\"text\" name=\"%s\" placeholder=\"number\"%s/>", PingSlotPeriod, "Provided by PingSlotInfoReq" },
    { "<input type=\"text\" name=\"%s\" placeholder=\"number\"%s/>", PingSlotDR, "Mandatory if class B mode supported" },
    { "<input type=\"text\" name=\"%s\" placeholder=\"number\"%s/>", PingSlotFreq, "Mandatory if class B mode supported" },
    { "<input type=\"checkbox\" name=\"%s\" value=\"set\"%s/>", SupportsClassC, "End-Device supports Class C" },
    { "<input type=\"text\" name=\"%s\" placeholder=\"seconds\"%s/>", ClassCTimeout, "Maximum delay for the End-Device to answer a MAC request or a confirmed DL frame (mandatory if class C mode supported)" },
    { "<input type=\"text\" name=\"%s\" placeholder=\"1.1 or 1.0.2 etc\"%s/>", MACVersion, "Version of the LoRaWAN supported by the End-Device" },
    { "<input type=\"text\" name=\"%s\"%s/>", RegParamsRevision, "Revision of the Regional Parameters document supported by the End-Device" },
    { "<input type=\"hidden\" name=\"%s\" value=\"%c\"/>%s", SupportsJoin, "End-Device supports Join (OTAA) or not (ABP)" },
    { "<input type=\"text\" name=\"%s\" placeholder=\"\"%s/>", RXDelay1, "Class A RX1 delay (mandatory for ABP)" },
    { "<input type=\"text\" name=\"%s\" placeholder=\"\"%s/>", RXDROffset1, "RX1 data rate offset (mandatory for ABP)"},
    { "<input type=\"text\" name=\"%s\" placeholder=\"bits-per-second\"%s/>", RXDataRate2, "RX2 data rate (mandatory for ABP)"},
    { "<input type=\"text\" name=\"%s\" placeholder=\"float MHz\"%s/>", RXFreq2, "RX2 channel frequency (mandatory for ABP)"},
    { "<input type=\"text\" name=\"%s\" placeholder=\"\"%s/>", FactoryPresetFreqs, "List of factory-preset frequencies (mandatory for ABP)"},
    { "<input type=\"text\" name=\"%s\" placeholder=\"(dBm)\"%s/>", MaxEIRP, "Maximum EIRP supported by the End-Device"},
    { "<input type=\"text\" name=\"%s\" placeholder=\"(0.10 for 10%%)\"%s/>", MaxDutyCycle, "Maximum duty cycle supported by the End-Device RF region name"},
    { NULL , RFRegion, "RF region name"},
    { "<input type=\"checkbox\" name=\"%s\" value=\"set\"%s/>", Supports32bitFCnt, "End-Device uses 32bit FCnt (mandatory for LoRaWAN 1.0 End-Device)"},
    { NULL, NULL, NULL }
};

const struct elements esServiceProfile[] = {
    { "<input type=\"text\" name=\"%s\" placeholder=\"Number\"%s/>", ULRate, "Token bucket filling rate, including ACKs (packet/h)" },
    { "<input type=\"text\" name=\"%s\" placeholder=\"Number\"%s/>", ULBucketSize, "Token bucket burst size" },
    { " <select name=\"%s\"> <option value=\"Drop\">Drop</option> <option value=\"Mark\">Mark</option>%s</select>", ULRatePolicy, "Drop or mark when exceeding ULRate"},
    { "<input type=\"text\" name=\"%s\" placeholder=\"Number\"%s/>", DLRate, "Token bucket filling rate, including ACKs (packet/h)" },
    { "<input type=\"text\" name=\"%s\" placeholder=\"Number\"%s/>", DLBucketSize, "Token bucket burst size" },
    { " <select name=\"%s\"> <option value=\"Drop\">Drop</option> <option value=\"Mark\">Mark</option>%s</select>", DLRatePolicy, "Drop or mark when exceeding DLRate"},
    { "<input type=\"checkbox\" name=\"%s\" value=\"set\"%s/>", AddGWMetadata, "(RSSI, SNR, GW geoloc., etc.) are added to the packet sent to AS"},
    { "<input type=\"text\" name=\"%s\" placeholder=\"num--per-day\"%s/>", DevStatusReqFreq, "Frequency to initiate an End-Device status request (request/day)" },
    { "<input type=\"checkbox\" name=\"%s\" value=\"set\"%s/>", ReportDevStatusBattery, "Report End-Device battery level to AS"},
    { "<input type=\"checkbox\" name=\"%s\" value=\"set\"%s/>", ReportDevStatusMargin, "Report End-Device margin to AS"},
    { "<input type=\"text\" name=\"%s\" placeholder=\"Number\"%s/>", DRMin, "Minimum allowed data rate. Used for ADR." },
    { "<input type=\"text\" name=\"%s\" placeholder=\"Number\"%s/>", DRMax, "Maximum allowed data rate. Used for ADR." },
    { "<input type=\"text\" name=\"%s\" placeholder=\"hex-string\"%s/>", ChannelMask, "ChannelMask, sNS does not have to obey (i.e., informative)." },
    { "<input type=\"checkbox\" name=\"%s\" value=\"set\"%s/>", PRAllowed, "Passive Roaming allowed"},
    { "<input type=\"checkbox\" name=\"%s\" value=\"set\"%s/>", HRAllowed, "Handover Roaming allowed"},
    { "<input type=\"checkbox\" name=\"%s\" value=\"set\"%s/>", RAAllowed, "Roaming Activation allowed"},
    { "<input type=\"checkbox\" name=\"%s\" value=\"set\"%s/>", NwkGeoLoc, "network geolocation service"},

    { "<input type=\"text\" name=\"%s\" placeholder=\"0.10 is 10%%\"%s/>", TargetPER, "Target Packet Error Rate" },
    { "<input type=\"text\" name=\"%s\" placeholder=\"Number\"%s/>", MinGWDiversity, "Minimum number of receiving GWs (informative)"},

    { NULL, NULL, NULL }
};

const struct elements esRouteProfile[] = {
    { "<input type=\"text\" name=\"%s\" placeholder=\"string\"%s/>", AS_ID, "IP address, or DNS name, etc" },
    { NULL, NULL, NULL }
};


static ssize_t
profile_page_iterator(void *cls,
          uint64_t pos,
          char *buf,
          size_t max)
{
    int len = 0;
    struct Session *session = cls;
    appInfo_t* ai = session->appInfo;
    char str[384];
    char name[48];
    char query[512];

    switch (ai->form_state) {
        case FORM_STATE_START:
            ai->form_state = FORM_STATE_TABLE_HEADER;
            strcpy(buf, "<html><head><title>NS ");
            strcat(buf, myNetwork_idStr);
            strcat(buf, "</title></head><body><a href=\"");
            strcat(buf, motesURL);
            strcat(buf, "\">back</a><h3>");
            if (ai->opFlags.ota) {
                uint64_t eui64;
                sscanf(ai->DevEUIstr, "%"PRIu64, &eui64);
                sprintf(query, "%016"PRIx64, eui64);
                strcat(buf, "DevEUI 0x");
            } else {
                uint32_t devAddr;
                sscanf(ai->devAddrStr, "%u", &devAddr);
                sprintf(query, "%08x", devAddr);
                strcat(buf, "DevAddr 0x");
            }
            strcat(buf, query);
            strcat(buf, " ");
            if (ai->opFlags.op == OPR_CREATE || ai->opFlags.op == OPR_ADD_PROFILES) {
                const char* url;
                printf(" profile form ota:%u\n", ai->opFlags.ota);
                if (ai->opFlags.addMote)
                    url = addProfilesToNewURL;
                else
                    url = addProfilesToExistingURL;

                if (ai->opFlags.ota) {
                    strcat(buf, "OTA add profiles</h3><form method=\"post\" action=\"");
                    strcat(buf, url);
                    strcat(buf, "\"><input type=\"hidden\" name=\"DevEUI\" value=\"");
                    strcat(buf, query);
                } else {
                    strcat(buf, "ABP add profiles</h3><form method=\"post\" action=\"");
                    strcat(buf, url);
                    strcat(buf, "\"><input type=\"hidden\" name=\"DevAddr\" value=\"");
                    strcat(buf, query);
                }
            } else if (ai->opFlags.op == OPR_EDIT) {
                strcat(buf, "Edit Profiles</h3><form method=\"post\" action=\"");
                strcat(buf, updateProfilesURL);
                strcat(buf, "\"><input type=\"hidden\" name=\"moteID\" value=\"");
                strcat(buf, session->urlSubDir);
            }
            strcat(buf, "\"/><table border=\"1\">\n");
            len = strlen(buf);
            break;
        case FORM_STATE_TABLE_HEADER:
            if (ai->es == esDeviceProfile) {
                if (ai->opFlags.op == OPR_EDIT)
                    sprintf(query, "SELECT * FROM DeviceProfiles WHERE DeviceProfileID = %s", session->urlSubDir);
                sprintf(name, "Device");
            } else if (ai->es == esServiceProfile) {
                if (ai->opFlags.op == OPR_EDIT)
                    sprintf(query, "SELECT * FROM ServiceProfiles WHERE ServiceProfileID = %s", session->urlSubDir);
                sprintf(name, "Service");
            } else if (ai->es == esRouteProfile) {
                if (ai->opFlags.op == OPR_EDIT)
                    sprintf(query, "SELECT * FROM RoutingProfiles WHERE RoutingProfileID = %s", session->urlSubDir);
                sprintf(name, "Routing");
            } else
                sprintf(name, "???");

            if (ai->opFlags.op == OPR_EDIT) {
                if (mysql_query(sql_conn_ns_web, query)) {
                    len = sprintf(buf, "</table></form>%s </body></html>", mysql_error(sql_conn_ns_web));
                    ai->form_state = FORM_STATE_END;
                    break;
                }
                ai->result = mysql_use_result(sql_conn_ns_web);
                if (ai->result == NULL) {
                    len = sprintf(buf, "</table></form>no result for ID %s</body></html>", session->urlSubDir);
                    ai->form_state = FORM_STATE_END;
                    break;
                }
                ai->row = mysql_fetch_row(ai->result);
                if (!ai->row) {
                    strcpy(buf, "</table></form>no ");
                    if (ai->es == esDeviceProfile)
                        strcat(buf, "DeviceProfile"); 
                    else if (ai->es == esServiceProfile)
                        strcat(buf, "ServiceProfile"); 
                    else if (ai->es == esRouteProfile)
                        strcat(buf, "RouteProfile"); 
                    strcat(buf, " for this mote</body></html>");
                    len = strlen(buf);
                    //len = sprintf(buf, "</table></form>no row for ID %s</body></html>", session->urlSubDir);
                    mysql_free_result(ai->result);
                    ai->form_state = FORM_STATE_END;
                    break;
                }
            }

            len = sprintf(buf, " <tr> <th>%s Profile</th> <th>element</th> <th>description</th> </tr>", name);
            if (ai->es[0].form == NULL)
                ai->form_state = FORM_STATE_FORM;
            else {
                ai->form_state = FORM_STATE_TABLE_ROWS;
                ai->i = 0;
            }
            break;
        case FORM_STATE_TABLE_ROWS:
            if (ai->opFlags.op == OPR_EDIT && ai->es[ai->i].element != RFRegion) {
                //printf("row[%u]:%s  ", ai->i+1, ai->row[ai->i+1]);  // +1: first column is id
                if (strstr(ai->es[ai->i].form, "checkbox")) {
                    if (ai->row[ai->i+1][0] == '1')
                        sprintf(query, " checked");
                    else
                        query[0] = 0;
                } else if (strstr(ai->es[ai->i].form, "select") && strstr(ai->es[ai->i].form, "option")) {
                    sprintf(query, "<option selected=\"selected\">%s</option>", ai->row[ai->i+1]);
                } else
                    sprintf(query, " value=\"%s\"", ai->row[ai->i+1]);
                //printf(" --> %s\n", query);
            } else
                query[0] = 0;

            if (strcmp(ai->es[ai->i].element, SupportsJoin) == 0) {
                char ch;
                if (ai->opFlags.op == OPR_EDIT) { // SupportsJoin not editable
                    if (ai->row[ai->i+1][0] == '1') {
                        snprintf(query, sizeof(query), "OTA");
                    } else {
                        snprintf(query, sizeof(query), "ABP");
                    }
                    ch = ai->row[ai->i+1][0];
                } else {
                    if (ai->opFlags.ota)
                        ch = '1';
                    else
                        ch = '0';
                }
                /* insert hidden value */
                snprintf(str, sizeof(str), ai->es[ai->i].form, ai->es[ai->i].element, ch, query); 
            } else if (strcmp(ai->es[ai->i].element, RFRegion) == 0) {
                unsigned n;
                strcpy(str, "<select name=\"");
                strcat(str, RFRegion);
                strcat(str, "\">");
                for (n = 0; region_strs[n]; n++) {
                    strcat(str, "<option value=\"");
                    strcat(str, region_strs[n]);
                    if (ai->opFlags.op == OPR_EDIT && strcmp(ai->row[ai->i+1], region_strs[n]) == 0)
                        strcat(str, "\" selected>");
                    else
                        strcat(str, "\">");
                    strcat(str, region_strs[n]);
                    strcat(str, "</option>");
                }
                strcat(str, "</select>");
//" <select name=\"%s\"> <option value=\"EU868\">EU868</option> <option value=\"US902\">US902</option> <option value=\"China779\">China779</option> <option value=\"EU433\">EU433</option> <option value=\"Australia915\">Australia915</option> <option value=\"China470\">China470</option> <option value=\"AS923\">AS923</option>%s</select>"
//  yyy;
            } else {
                // print element into form item name
                snprintf(str, sizeof(str), ai->es[ai->i].form, ai->es[ai->i].element, query); 
            }
            //printf("str: %s\n", str);
            len = sprintf(buf, "<tr><td>%s</td> <td>%s</td> <td>%s</td></tr>\n",
                str, ai->es[ai->i].element, ai->es[ai->i].notes);
            if (ai->es[++(ai->i)].element == NULL)
                ai->form_state = FORM_STATE_FORM;
            break;
        case FORM_STATE_FORM:
            if (ai->opFlags.op == OPR_EDIT)
                mysql_free_result(ai->result);

            if (ai->es == esDeviceProfile) {
                ai->es = esServiceProfile;
                len = sprintf(buf, "<tr><td></td> <td></td> <td></td></tr>\n");
                ai->form_state = FORM_STATE_TABLE_HEADER;
            } else if (ai->es == esServiceProfile) {
                ai->es = esRouteProfile;
                len = sprintf(buf, "<tr><td></td> <td></td> <td></td></tr>\n");
                ai->form_state = FORM_STATE_TABLE_HEADER;
            } else if (ai->es == esRouteProfile) {
                if (ai->opFlags.op == OPR_EDIT)
                    sprintf(str, "Commit");
                else {
                    if (ai->opFlags.addMote)
                        sprintf(str, "AddToNew");
                    else
                        sprintf(str, "AddToExisting");
                }
                len = sprintf(buf, "</table><input type=\"submit\" value=\"%s\"></form></body></html>", str);
                ai->form_state = FORM_STATE_END;
            }
            break;
        case FORM_STATE_END:
            return MHD_CONTENT_READER_END_OF_STREAM;
    } // ..switch (ai->form_state)
    return len;
}


static ssize_t
roaming_page_iterator(void *cls,
          uint64_t pos,
          char *buf,
          size_t max)
{
    int len = 0;
    struct Session *session = cls;
    appInfo_t* ai = session->appInfo;
    char devEuiStr[48];
    char devAddrStr[24];
    char moteStr[48];
    char query[512];

    switch (ai->form_state) {
        case FORM_STATE_START:
            ai->form_state = FORM_STATE_TABLE_HEADER;
            moteStr[0] = 0;
            ai->where[0] = 0;
            if (strlen(session->urlSubDir) > 0) {
                if (getTarget(session->urlSubDir, devEuiStr, sizeof(devEuiStr), devAddrStr, sizeof(devAddrStr)) < 0) {
                    strcpy(buf, "<html><head><title>NS</title></head><body><h3>roaming page getTarget failed</h3></body</html>");
                    ai->form_state = FORM_STATE_END;
                    len = strlen(buf);
                    break;
                }
                if (strlen(devEuiStr) > 0) {
                    uint64_t devEui;
                    sscanf(devEuiStr, "%"PRIu64, &devEui);
                    sprintf(moteStr, "%016"PRIx64, devEui);
                    sprintf(ai->where, "DevEUI = %s", devEuiStr);
                } else if (strlen(devAddrStr) > 0) {
                    const char* res;
                    uint32_t devAddr;
                    sscanf(devAddrStr, "%u", &devAddr);
                    sprintf(moteStr, "%08x", devAddr);
                    sprintf(ai->where, "ID = %llu", getMoteID(sql_conn_ns_web, NONE_DEVEUI, devAddr, &res));
                }
            }

            strcpy(buf, "<html><head><title>NS ");
            strcat(buf, myNetwork_idStr);
            strcat(buf, "</title></head><body><a href=\"");
            strcat(buf, motesURL);
            strcat(buf, "\">back</a><h3>NS ");
            strcat(buf, myNetwork_idStr);
            strcat(buf, " Roaming ");
            strcat(buf, moteStr);
            strcat(buf, "</h3><form method=\"post\" action=\"");
            strcat(buf, roamingURL);
            strcat(buf, "\">");

            ai->result = NULL;
                sprintf(query, "SELECT roamState, UNIX_TIMESTAMP(roamUntil) FROM motes WHERE %s", ai->where);
            if (mysql_query(sql_conn_ns_web, query)) {
                strcat(buf, "<tr>");
                strcat(buf, query);
                strcat(buf, "<br>mysql_query() ");
                strcat(buf, mysql_error(sql_conn_ns_web));
                strcat(buf, "<br></tr>");
                ai->form_state = FORM_STATE_FORM;
                len = strlen(buf);
                break;
            }
            ai->result = mysql_use_result(sql_conn_ns_web);
            if (ai->result) {
                ai->row = mysql_fetch_row(ai->result);
                if (ai->row) {
                    strcat(buf, "Current state: ");
                    strcat(buf, ai->row[0]);
                    if (ai->row[1]) {
                        strcat(buf, ", expiration: ");
                        getAgo(ai->row[1], query);
                        strcat(buf, query);
                    }
                    if (strcmp(roamhHANDOVER, ai->row[0]) == 0 || strcmp(roamsPASSIVE, ai->row[0]) == 0) {
                        strcat(buf, "<input type=\"submit\" name=\"");
                        strcat(buf, session->urlSubDir);
                        strcat(buf, "\" value=\"");
                        strcat(buf, RStopReq);
                        strcat(buf, "\">");
                    }
                }
                mysql_free_result(ai->result);
            }

            strcat(buf, "<br>Lifetime zero for stateless<table border=\"1\">");
            len = strlen(buf);
            break;
        case FORM_STATE_TABLE_HEADER:
            sprintf(query, "SELECT sNSLifetime, fwdToNetID FROM motes WHERE %s", ai->where);
            if (mysql_query(sql_conn_ns_web, query)) {
                len = sprintf(buf, "<tr>mysql_query() %s</tr>", mysql_error(sql_conn_ns_web));
                ai->form_state = FORM_STATE_FORM;
                break;
            }
            ai->result = mysql_use_result(sql_conn_ns_web);
            if (ai->result == NULL) {
                len = sprintf(buf, "<tr>no result</tr>");
                ai->form_state = FORM_STATE_FORM;
                break;
            }
            len = sprintf(buf, "<tr><th>forward to<br>NetID</th><th>sNS<br>Lifetime</th></tr>");
            ai->row = mysql_fetch_row(ai->result);
            if (ai->row)
                ai->form_state = FORM_STATE_TABLE_ROWS;
            else
                ai->form_state = FORM_STATE_FORM;
            break;
        case FORM_STATE_TABLE_ROWS:
            ai->form_state = FORM_STATE_FORM;
            devAddrStr[0] = 0;
            if (ai->row) {
                if (ai->row[0])    // copy Lifetime
                    strcpy(devAddrStr, ai->row[0]);
                if (ai->row[1]) // copy fwdToNetID
                    strcpy(moteStr, ai->row[1]);
            }

            if (ai->result)
                mysql_free_result(ai->result);

            strcat(buf, "<tr><td>");
            create_netid_select(buf, moteStr);
            strcat(buf, "</td>");
            sprintf(query, "<td><input type=\"text\" name=\"Lifetime\" size=\"7\" value=\"%s\"></td></tr>", devAddrStr);
            strcat(buf, query);
            len = strlen(buf);
            break;
        case FORM_STATE_FORM:
            ai->form_state = FORM_STATE_END;
            strcpy(buf, "</table><input type=\"submit\" name=\"");
            strcat(buf, session->urlSubDir);
            strcat(buf, "\" value=\"");
            strcat(buf, roamingUpdate);
            strcat(buf, "\"></form></body></html>");
            len = strlen(buf);
            break;
        case FORM_STATE_END:
            return MHD_CONTENT_READER_END_OF_STREAM;
    } // ..switch (ai->form_state)
    return len;
}

static int
serve_profile_add(const void *cls,
	      const char *mime,
	      struct Session *session,
	      struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;
    appInfo_t* ai = session->appInfo;

    ai->es = esDeviceProfile;

    ai->form_state = FORM_STATE_START;
    response = MHD_create_response_from_callback (
        MHD_SIZE_UNKNOWN, 1024, &profile_page_iterator, session, NULL
    );
    lib_add_session_cookie (session, response);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}

	      
static int
static_page(const char* errmsg,
	      const char *mime,
	      struct Session *session,
	      struct MHD_Connection *connection)
{
    struct MHD_Response *response;
    int ret;
    char body[1536];

    strcpy(body, "<html><head><title>NS fail</title></head><body>");
    strcat(body, errmsg);
    strcat(body, "</body></html>");

    response = MHD_create_response_from_buffer (strlen (body), body, MHD_RESPMEM_MUST_COPY);
    if (NULL == response)
        return MHD_NO;

    lib_add_session_cookie (session, response);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_ENCODING, mime);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}

static int
serve_profile_edit(const void *cls,
	      const char *mime,
	      struct Session *session,
	      struct MHD_Connection *connection)
{
    char query[512];
    int ret;
    struct MHD_Response *response;
    appInfo_t* ai = session->appInfo;
    uint64_t devEui = NONE_DEVEUI;
    char errmsg[1024];

    ai->DevEUIstr[0] = 0;
    ai->devAddrStr[0] = 0;

    sprintf(query, "SELECT DevEUI FROM motes WHERE ID = %s", session->urlSubDir);
    if (mysql_query(sql_conn_ns_web, query)) {
        sprintf(errmsg, "%s: %s", query, mysql_error(sql_conn_ns_web));
        return static_page(errmsg, mime, session, connection);
    }
    ai->result = mysql_use_result(sql_conn_ns_web);
    if (ai->result) {
        ai->row = mysql_fetch_row(ai->result);
        if (ai->row) {
            if (ai->row[0]) {
                sscanf(ai->row[0], "%"PRIu64, &devEui);
                ai->opFlags.ota = 1;
                sprintf(ai->DevEUIstr, "%"PRIu64, devEui);
            }
        } else {
            mysql_free_result(ai->result);
            sprintf(errmsg, "profile_edit %s: no row", query);
            return static_page(errmsg, mime, session, connection);
        }
        mysql_free_result(ai->result);
    } else{
        sprintf(errmsg, "%s: no result", query);
        return static_page(errmsg, mime, session, connection);
    }

    if (ai->DevEUIstr[0] == 0) {
        sprintf(query, "SELECT DevAddr FROM sessions WHERE ID = %s", session->urlSubDir);
        if (mysql_query(sql_conn_ns_web, query)) {
            sprintf(errmsg, "%s: %s", query, mysql_error(sql_conn_ns_web));
            return static_page(errmsg, mime, session, connection);
        }
        ai->result = mysql_use_result(sql_conn_ns_web);
        if (ai->result) {
            ai->row = mysql_fetch_row(ai->result);
            if (ai->row) {
                ai->opFlags.ota = 0;
                strcpy(ai->devAddrStr, ai->row[0]);
            } else {
                mysql_free_result(ai->result);
                sprintf(errmsg, "sessions DevAddr %s: no row", query);
                return static_page(errmsg, mime, session, connection);
            }
            mysql_free_result(ai->result);
        } else {
            sprintf(errmsg, "%s: no result", query);
            return static_page(errmsg, mime, session, connection);
        }
    }
    

    ai->opFlags.op = OPR_EDIT;
    ai->es = esDeviceProfile;

    ai->form_state = FORM_STATE_START;
    response = MHD_create_response_from_callback (
        MHD_SIZE_UNKNOWN, 1024, &profile_page_iterator, session, NULL
    );
    lib_add_session_cookie (session, response);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}


static int
serve_roaming_edit(const void *cls,
	      const char *mime,
	      struct Session *session,
	      struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;
    appInfo_t* ai = session->appInfo;

    ai->form_state = FORM_STATE_START;
    response = MHD_create_response_from_callback (
        MHD_SIZE_UNKNOWN, 1024, &roaming_page_iterator, session, NULL
    );
    lib_add_session_cookie (session, response);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}

static int
serve_network_edit(const void *cls,
	      const char *mime,
	      struct Session *session,
	      struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;
    char query[512];
    char buf[2048];
    uint32_t netID;

    strcpy(buf, "<html><head><title>NS ");
    strcat(buf, myNetwork_idStr);
    strcat(buf, "</title></head><body><h3>edit NetID ");
    sscanf(session->urlSubDir, "%u", &netID);
    sprintf(query, "%06x", netID);
    strcat(buf, query);
    strcat(buf, "</h3><form id=\"form\" method=\"post\" action=\"");
    strcat(buf, networksURL);
    strcat(buf, "\"><input type=\"hidden\" name=\"NetID\" value=\"");
    strcat(buf, query);
    strcat(buf, "\"><table><tr>");

    sprintf(query, "SELECT *, HEX(KEK) FROM roaming WHERE NetID = %s", session->urlSubDir);
    if (mysql_query(sql_conn_ns_web, query) == 0) {
        MYSQL_RES *result = mysql_use_result(sql_conn_ns_web);
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);

            strcat(buf, "<td>PRAllowed:</td><td><input type=\"checkbox\" name=\"PRAllowed\" ");
            strcat(buf, row[1][0] == '0' ? "" : "checked");
            strcat(buf, "></td></tr><tr><td>HRAllowed:</td><td><input type=\"checkbox\" name=\"HRAllowed\" ");
            strcat(buf, row[2][0] == '0' ? "" : "checked");
            strcat(buf, "></td></tr><tr><td>RAAllowed:</td><td><input type=\"checkbox\" name=\"RAAllowed\" ");
            strcat(buf, row[3][0] == '0' ? "" : "checked");
            strcat(buf, "></td></tr><tr><td>fMICup:</td><td><input type=\"checkbox\" name=\"fMICup\" ");
            strcat(buf, row[4][0] == '0' ? "" : "checked");
            strcat(buf, "></td></tr><tr><td>KEK label:</td><td><input type=\"text\" name=\"KEKlabel\" value=\"");
            if (row[5])
                strcat(buf, row[5]);
            strcat(buf, "\"></td></tr><tr><td>KEK:</td><td><input type=\"text\" name=\"KEK\" size=\"32\" value=\"");
            if (row[7])
                strcat(buf, row[7]);
            strcat(buf, "\"></tr><tr><td><input type=\"submit\" value=\"change\" name=\"change\"></td></tr>");
        } else
            strcat(buf, "<td>no result</td>");

        mysql_free_result(result);
    } else {
        strcat(buf, "<td>");
        strcat(buf, mysql_error(sql_conn_ns_web));
        strcat(buf, "</td>");
    }

    strcat(buf, "</table></form></body></html>");
    response = MHD_create_response_from_buffer (strlen (buf), buf, MHD_RESPMEM_MUST_COPY);
    if (NULL == response)
        return MHD_NO;

    lib_add_session_cookie (session, response);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_ENCODING, mime);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}

const char *keyName[] = 
{
    SNwkSIntKey,
    FNwkSIntKey,
    NwkSEncKey
};

static ssize_t
skey_page_iterator(void *cls,
          uint64_t pos,
          char *buf,
          size_t max)
{
    char query[512];
    ssize_t len = 0;
    struct Session *session = cls;
    appInfo_t* ai = session->appInfo;
    char str[128];
    char valStr[64];

    switch (ai->form_state) {
        uint32_t devAddr;
        case FORM_STATE_START:
            sscanf(session->urlSubDir, "%u", &devAddr);
            len = sprintf(buf, "<html><head><title>ABP Session Keys</title></head><body><a href=\"%s\">back</a><h3>ABP %08x<br><br>All 3 same value for v1.0,<br>Each unique for v1.1</h3><form method=\"post\" action=\"%s\"> <input type=\"hidden\" name=\"DevAddr\" value=\"%x\"/><table border=\"1\">", motesURL, devAddr, sKeyEditURL, devAddr);
            ai->form_state = FORM_STATE_TABLE_HEADER;
            break;
        case FORM_STATE_TABLE_HEADER:
            /* TODO also query NFCntDown and FCntUp for display and reset */
            sprintf(query, "SELECT HEX(%s), HEX(%s), HEX(%s) FROM sessions WHERE DevAddr = %s", keyName[0], keyName[1], keyName[2], session->urlSubDir);
            ai->result = NULL;
            if (mysql_query(sql_conn_ns_web, query)) {
                len = sprintf(buf, "<tr>mysql_query() %s</tr>", mysql_error(sql_conn_ns_web));
                ai->form_state = FORM_STATE_FORM;
                break;
            }
            ai->result = mysql_use_result(sql_conn_ns_web);
            if (ai->result == NULL) {
                len = sprintf(buf, "<tr>no result</tr>");
                ai->form_state = FORM_STATE_FORM;
                break;
            }
            ai->row = mysql_fetch_row(ai->result);
            if (ai->row) {
                ai->i = 0;
                len = sprintf(buf, "<tr><th>value</th><th>name</th></tr>");
                ai->form_state = FORM_STATE_TABLE_ROWS;
            } else
                ai->form_state = FORM_STATE_FORM;
            break;
        case FORM_STATE_TABLE_ROWS:
            if (ai->row[ai->i]) {
                printf("row %u: \"%s\"\n", ai->i, ai->row[ai->i]);
                sprintf(valStr, " value=\"%s\"", ai->row[ai->i]);
            } else
                valStr[0] = 0;
            sprintf(str, "<input type=\"text\" name=\"%s\" placeholder=\"32 hex digits\" size=\"33\" %s/>", keyName[ai->i], valStr);
            len = sprintf(buf, "<tr><th>%s</th><th>%s</th></tr>", str, keyName[ai->i]);
            if (++ai->i == 3)
                ai->form_state = FORM_STATE_FORM;
            break;
        case FORM_STATE_FORM:
            if (ai->result)
                mysql_free_result(ai->result);
            len = sprintf(buf, "</table><input type=\"submit\"/>");
            ai->form_state = FORM_STATE_END;
            break;
        case FORM_STATE_END:
            len = sprintf(buf, "</form></body></html>");
            return MHD_CONTENT_READER_END_OF_STREAM;
    } // ..switch (ai->form_state)
    return len;
}

static int
serve_skey_edit(const void *cls,
	      const char *mime,
	      struct Session *session,
	      struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;
    appInfo_t* ai = session->appInfo;

    ai->form_state = FORM_STATE_START;
    response = MHD_create_response_from_callback (
        MHD_SIZE_UNKNOWN, 1024, &skey_page_iterator, session, NULL
    );
    lib_add_session_cookie (session, response);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}

static char postFailPage[256];


static int
serve_redirect(const void *cls,
		const char *mime,
		struct Session *session,
		struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;
    appInfo_t* ai = session->appInfo;
    char body[1024];

    sprintf(body, "<html><head><title>NS %06x</title><meta http-equiv=\"refresh\" content=\"0; url=.%s\" /></head><body></body></html>", myNetwork_id32, ai->redir_url);
    //printf("%s\n", body);
    response = MHD_create_response_from_buffer (strlen (body), body, MHD_RESPMEM_MUST_COPY);
    if (NULL == response)
        return MHD_NO;

    lib_add_session_cookie (session, response);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_ENCODING, mime);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}

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

/**
 * List of all pages served by this HTTP server.
 */
struct Page pages[] =
  {
    { "/", "text/html",  &fill_v1_form, NULL },
    { networksURL, "text/html",  &serve_networks, NULL },
    { gatewaysURL, "text/html",  &serve_gateways, NULL },
    { motesURL, "text/html",  &serve_motes, NULL },
    { postFailURL, "text/html", &serve_post_fail, NULL },
    { addProfilesToExistingURL, "text/html", &serve_profile_add, NULL },
    { addProfilesToNewURL, "text/html", &serve_profile_add, NULL },
    { peditURL, "text/html", &serve_profile_edit, NULL },
    { sKeyEditURL, "text/html", &serve_skey_edit, NULL },
    { neditURL, "text/html", &serve_network_edit, NULL },
    { roamingURL, "text/html", &serve_roaming_edit, NULL },
    { redirectURL, "text/html", &serve_redirect, NULL },


    { "/cb", "text/html", &serve_cb, NULL },
    { NULL, NULL, &not_found_page, NULL } /* 404 */
  };

static const char*
strGetMotesWhere(const char* devEuiStr, const char* devAddrStr, char* out)
{
    const char* ret;
    char query[256];
    MYSQL_RES *result;

    if (strlen(devEuiStr) > 0) {
        strcpy(out, "DevEUI = ");
        strcat(out, devEuiStr);
        return Success;
    }

    out[0] = 0;

    strcpy(query, "SELECT ID FROM sessions WHERE DevAddr = ");
    strcat(query, devAddrStr);
    if (mysql_query(sql_conn_ns_web, query)) {
        printf("\e[31mget mote id: %s\e[0m\n", mysql_error(sql_conn_ns_web));
        return Other;
    }

    result = mysql_use_result(sql_conn_ns_web);
    if (result) {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            unsigned moteID;
            sscanf(row[0], "%u", &moteID);
            sprintf(out, "ID = %u", moteID);
            ret = Success;
        } else {
            printf(" DevAddr \"%s\" not found\n", devAddrStr);
            ret = UnknownDevAddr;
        }
        mysql_free_result(result);
    } else {
        printf("\e[31msNS mote_id_from_devAddr %s, no result\e[0m\n", query);
        ret = Other;
    }

    return ret;
}


/**
 * Invalid method page.
 */
#define METHOD_ERROR "<html><head><title>Illegal request</title></head><body>Go away.</body></html>"

json_object*
jsonGetServiceProfile(uint64_t devEui, uint32_t devAddr)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    char query[512];
    char where[128];
    json_object* ret = NULL;

    if (getMotesWhere(sql_conn_ns_web, devEui, devAddr, where) != Success)
        return NULL;

    strcpy(query, "SELECT ServiceProfiles.* FROM ServiceProfiles INNER JOIN motes ON ServiceProfiles.ServiceProfileID = motes.ID WHERE ");
    strcat(query, where);
    if (mysql_query(sql_conn_ns_web, query)) {
        fprintf(stderr, "\e[31msNS jsonGetServiceProfile: %s\e[0m\n", mysql_error(sql_conn_ns_web));
        return ret;
    }
    result = mysql_use_result(sql_conn_ns_web);
    if (result == NULL) {
        fprintf(stderr, "\e[31msNS jsonGetServiceProfile: no result\e[0m\n");
        return ret;
    }
    row = mysql_fetch_row(result);
    if (row) {
        ret = json_object_new_object();
        int /*i,*/ num_fields = mysql_num_fields(result);
        printf("jsonGetServiceProfile() num_fields:%d\n", num_fields);
        /*for (i = 0; i < num_fields; i++) {
            printf("%d) %s\n", i, row[i]);
        }*/
        json_object_object_add(ret, ServiceProfileID, json_object_new_string(row[0]));
        json_object_object_add(ret, ULRate, json_object_new_int(atoi(row[1])));
        json_object_object_add(ret, ULBucketSize, json_object_new_int(atoi(row[2])));
        json_object_object_add(ret, ULRatePolicy, json_object_new_string(row[3]));
        json_object_object_add(ret, DLRate, json_object_new_int(atoi(row[4])));
        json_object_object_add(ret, DLBucketSize, json_object_new_int(atoi(row[5])));
        json_object_object_add(ret, DLRatePolicy, json_object_new_string(row[6]));
        json_object_object_add(ret, AddGWMetadata, json_object_new_boolean(row[7][0]-'0'));
        json_object_object_add(ret, DevStatusReqFreq, json_object_new_int(atoi(row[8])));
        json_object_object_add(ret, ReportDevStatusBattery, json_object_new_boolean(row[9][0]-'0'));
        json_object_object_add(ret, ReportDevStatusMargin, json_object_new_boolean(row[10][0]-'0'));
        json_object_object_add(ret, DRMin, json_object_new_int(atoi(row[11])));
        json_object_object_add(ret, DRMax, json_object_new_int(atoi(row[12])));
        json_object_object_add(ret, ChannelMask, json_object_new_string(row[13]));
        json_object_object_add(ret, PRAllowed, json_object_new_boolean(row[14][0]-'0'));
        json_object_object_add(ret, HRAllowed, json_object_new_boolean(row[15][0]-'0'));
        json_object_object_add(ret, RAAllowed, json_object_new_boolean(row[16][0]-'0'));
        json_object_object_add(ret, NwkGeoLoc, json_object_new_boolean(row[17][0]-'0'));
        json_object_object_add(ret, TargetPER, json_object_new_double(strtof(row[18], NULL)));
        json_object_object_add(ret, MinGWDiversity, json_object_new_int(atoi(row[19])));
    } else
        fprintf(stderr, "\e[31msNS jsonGetServiceProfile: no row\e[0m\n");

    mysql_free_result(result);

    return ret;
}


const char*
getAppSKey(json_object* inJobj, const char* clientID, json_object** ansJobj)
{
    json_object* obj;
    uint64_t devEui;
    const char* ret = Other;
    json_object *envl;
    uint32_t devAddr;

    if (!*ansJobj)
        *ansJobj = json_object_new_object();    /* sending answer immediately */

    if (!json_object_object_get_ex(inJobj, DevEUI, &obj)) {
        /* only OTA devices. ABP would have permanent AppSKey */
        return MalformedRequest;
    }

    sscanf(json_object_get_string(obj), "%"PRIx64, &devEui);
    envl = getAppSKeyEnvelope(sql_conn_ns_web, devEui, &devAddr, &ret);
    if (envl) {
        char str[32];
        json_object_object_add(*ansJobj, AppSKey, envl);

        sprintf(str, "%x", devAddr);    /* devAddr is used along with AppSKey for encryption */
        json_object_object_add(*ansJobj, DevAddr, json_object_new_string(str));
    } else
        printf("\e[31mgetAppSKeyEnvelope() %s\e[0m\n", ret);

    return ret;
}

const char*
jsonGetDeviceProfile(uint64_t devEui, const char* clientID, json_object** ansJobj)
{
    const char* roamingType = NULL;
    char query[512];
    MYSQL_RES *result;
    MYSQL_ROW row;
    //json_object* obj;
    //unsigned long profileID;
    my_ulonglong m_id;
    uint32_t NetID;
    unsigned PRAllowed, HRAllowed;
    const char* res;

    if (!*ansJobj)
        *ansJobj = json_object_new_object();    /* sending answer immediately */

    sscanf(clientID, "%x", &NetID);
    sprintf(query, "SELECT PRAllowed, HRAllowed FROM roaming WHERE NetID = %u", NetID);
    if (mysql_query(sql_conn_ns_web, query)) {
        printf("mysql_query() %s", mysql_error(sql_conn_ns_web));
        return NoRoamingAgreement;
    }
    result = mysql_use_result(sql_conn_ns_web);
    if (result == NULL) {
        printf("no result: netid %s\n", clientID);
        return NoRoamingAgreement;
    }
    row = mysql_fetch_row(result);
    if (row == NULL) {
        printf("no row: netid %s\n", clientID);
        mysql_free_result(result);
        return NoRoamingAgreement;
    }
    sscanf(row[0], "%u", &PRAllowed);
    sscanf(row[1], "%u", &HRAllowed);
    mysql_free_result(result);
    printf("net PRAllowed:%u HRAllowed:%u\n", PRAllowed, HRAllowed);
    if (!PRAllowed && !HRAllowed)
        return NoRoamingAgreement;

    m_id = getMoteID(sql_conn_ns_web, devEui, NONE_DEVADDR, &res);
    if (m_id == 0) {
        printf("jsonGetDeviceProfile %s = getMoteID(%016"PRIx64")\n", res, devEui);
        return UnknownDevEUI;
    }

    sprintf(query, "SELECT PRAllowed, HRAllowed, RAAllowed FROM ServiceProfiles WHERE ServiceProfileID = %llu", m_id);
    if (mysql_query(sql_conn_ns_web, query)) {
        fprintf(stderr, "NS ServiceProfile %llu Error querying server: %s\n", m_id, mysql_error(sql_conn_ns_web));
        return RoamingActDisallowed;
    }
    result = mysql_use_result(sql_conn_ns_web);
    if (result == NULL) {
        printf("service profile %llu no result\n", m_id);
        return RoamingActDisallowed;
    }
    row = mysql_fetch_row(result);
    if (row == NULL) {
        printf("service profile %llu no row\n", m_id);
        mysql_free_result(result);
        return RoamingActDisallowed;
    }
    sscanf(row[0], "%u", &PRAllowed);
    sscanf(row[1], "%u", &HRAllowed);
    mysql_free_result(result);
    printf("mote PRAllowed:%u HRAllowed:%u\n", PRAllowed, HRAllowed);

    /* allowed to send DeviceProfile */
    if (HRAllowed)
        roamingType = Handover;
    else if (PRAllowed)
        roamingType = Passive;
    else
        return RoamingActDisallowed;

    json_object_object_add(*ansJobj, RoamingActivationType, json_object_new_string(roamingType));

    sprintf(query, "SELECT *, DATE_FORMAT(timestamp, '%%Y-%%m-%%dT%%TZ') FROM DeviceProfiles WHERE DeviceProfileID = %llu", m_id);
    if (mysql_query(sql_conn_ns_web, query)) {
        fprintf(stderr, "\e31mNS DeviceProfiles %llu Error querying server: %s\e[0m\n", m_id, mysql_error(sql_conn_ns_web));
        return Other;
    }
    result = mysql_use_result(sql_conn_ns_web);
    if (result == NULL) {
        printf("\e31mdevice profile %llu no result\e[0m\n", m_id);
        return Other;
    }
    row = mysql_fetch_row(result);
    if (row == NULL) {
        printf("\e31mdevice profile %llu no row\e[0m\n", m_id);
        mysql_free_result(result);
        return Other;
    }


    json_object* dpo = json_object_new_object();

    // row[0] DeviceProfileID
    json_object_object_add(dpo, SupportsClassB, json_object_new_boolean(row[1][0]-'0'));
    json_object_object_add(dpo, ClassBTimeout, json_object_new_int(atoi(row[2])));
    json_object_object_add(dpo, PingSlotPeriod, json_object_new_int(atoi(row[3])));
    json_object_object_add(dpo, PingSlotDR, json_object_new_int(atoi(row[4])));
    json_object_object_add(dpo, PingSlotFreq, json_object_new_double(strtof(row[5], NULL)));
    json_object_object_add(dpo, SupportsClassC, json_object_new_boolean(row[6][0]-'0'));
    json_object_object_add(dpo, ClassCTimeout, json_object_new_int(atoi(row[7])));
    json_object_object_add(dpo, MACVersion, json_object_new_string(row[8]));
    json_object_object_add(dpo, RegParamsRevision, json_object_new_string(row[9]));
    json_object_object_add(dpo, SupportsJoin, json_object_new_boolean(row[10][0]-'0'));
    json_object_object_add(dpo, RXDelay1, json_object_new_int(atoi(row[11])));
    json_object_object_add(dpo, RXDROffset1, json_object_new_int(atoi(row[12])));
    json_object_object_add(dpo, RXDataRate2, json_object_new_int(atoi(row[13])));
    json_object_object_add(dpo, RXFreq2, json_object_new_double(strtof(row[14], NULL)));

    json_object* jarray = json_object_new_array();
    const char* t = strtok(row[15], ",");
    while (t) {
        json_object_array_add(jarray, json_object_new_double(strtof(t, NULL)));
        t = strtok(NULL, ",");
    }
    json_object_object_add(dpo, FactoryPresetFreqs, jarray);

    json_object_object_add(dpo, MaxEIRP, json_object_new_int(atoi(row[16])));
    json_object_object_add(dpo, MaxDutyCycle, json_object_new_double(strtof(row[17], NULL)));
    json_object_object_add(dpo, RFRegion, json_object_new_string(row[18]));
    json_object_object_add(dpo, Supports32bitFCnt, json_object_new_boolean(row[19][0]-'0'));
    /* DeviceProfileTimestamp at toplevel, not into DeviceProfile object */
    json_object_object_add(*ansJobj, DeviceProfileTimestamp, json_object_new_string(row[21]));
    mysql_free_result(result);

    json_object_object_add(*ansJobj, DeviceProfile, dpo);

    return Success;
} // ..jsonGetDeviceProfile()

/* incoming json from httpd or from curl response */
void
ParseJson(MYSQL* sc, const struct sockaddr *client_addr, json_object* inJobj, json_object** ansJobj)
{
    int frm;
    uint8_t payloadLen = 0;
    uint8_t payBuf[256];
    uint8_t* payloadBufPtr = NULL;
    json_object *obj;
    const char* pmt;
    const char* Result;
    char sender_id[64];
    unsigned long trans_id = 0;
    const char* _ansMt = NULL;
    const char* rxResult = NULL;

    printf("NS ParseJson ");
    if (client_addr) {
        unsigned int src_port;
        char ipstr[INET_ADDRSTRLEN];
        void *vp = NULL;
        if (client_addr->sa_family == AF_INET) {
            struct sockaddr_in* sin_ptr = (struct sockaddr_in*)client_addr;
            vp = &sin_ptr->sin_addr;
            src_port = ntohs(sin_ptr->sin_port);
            printf("AF_INET ");
        } else if (client_addr->sa_family == AF_INET6) {
            struct sockaddr_in6* sin_ptr = (struct sockaddr_in6*)client_addr;
            vp = &sin_ptr->sin6_addr;
            src_port = ntohs(sin_ptr->sin6_port);
            printf("AF_INET6 ");
        } else
            printf("AF_?? ");

        if (vp)
            printf("from IP address %s : %u\n", inet_ntop(client_addr->sa_family, vp, ipstr, sizeof ipstr), src_port);
    }

    /*json_object_object_foreach(jobj, key0, val0) {
        int type = json_object_get_type(jobj);
        printf("key:%s ", key0);
        json_print_type(type);
        printf("\n");
    }*/

    Result = lib_parse_json(inJobj, &_ansMt, &pmt, sender_id, myNetwork_idStr, &trans_id, &rxResult);
    printf("NS %s = lib_parse_json()\n", Result);
    if (Result == MalformedRequest) {
        if (ansJobj != NULL)
            *ansJobj = json_object_new_object();
        goto Ans;
    }

    if (_ansMt == NULL && rxResult == NULL) {
        /* answers need to have a result */
        printf("\e[31mParseJson() NULL rxResult\e[0m\n");
        goto noAns;
    }
/* do not create answer object here, might decide to send answer later 
    if (isReq)
        *ansJobj = json_object_new_object();
*/

    if (strcmp(Result, Success) != 0) {
        printf("\e[31mNS ParseJson(): %s = lib_parse_json()\e[0m\n", Result);
        fflush(stdout);
        if (_ansMt != NULL)
            goto Ans;
        else
            goto noAns;
    }
    Result = Other;



    if (json_object_object_get_ex(inJobj, PHYPayload, &obj))
        frm = 0;
    else if (json_object_object_get_ex(inJobj, FRMPayload, &obj))
        frm = 1;
    else
        frm = -1;

    if (frm >= 0) {
        const char* phyStr = json_object_get_string(obj);
        int i, len = strlen(phyStr);
        payloadLen = len / 2;
        if (frm == 0) {
            if (payloadLen < MINIMUM_PHY_SIZE) {
                printf("\e[31mPHYPayload too small %u \"%s\"\e[0m\n", payloadLen, phyStr);
                fflush(stdout);
                Result = FrameSizeError;
                if (_ansMt != NULL && ansJobj != NULL)
                    *ansJobj = json_object_new_object();
                goto Ans;
            }
        } else if (payloadLen > MAXIMUM_FRM_SIZE) {
            printf("\e[31mFRMPayload too large\e[0m\n");
            fflush(stdout);
            Result = FrameSizeError;
            if (_ansMt != NULL && ansJobj != NULL)
                *ansJobj = json_object_new_object();
            goto Ans;
        }
        payloadBufPtr = payBuf;
        for (i = 0; i < len; i += 2) {
            unsigned o;
            sscanf(phyStr, "%02x", &o);
            *payloadBufPtr++ = o;
            phyStr += 2;
        }
        payloadBufPtr = payBuf;  // set back to start, non-null indicates having PHYPayload
    }


    if (_ansMt == NULL) {
        struct _mote_list* my_mote_list;
        bool answered = false;
        for (my_mote_list = mote_list; my_mote_list != NULL; my_mote_list = my_mote_list->next) {
            unsigned n;
            if (my_mote_list->motePtr == NULL)
                continue;

            for (n = 0; n < NT; n++) {
                mote_t* mote = NULL;
                //printf("ans tid %lu ", trans_id);
                if (my_mote_list->motePtr->t[n].sentTID == trans_id) {
                    mote = my_mote_list->motePtr;
                    if (mote->t[n].AnsCallback) {
                        /* must save AnsCallback, because it might generate a request requiring another answer */
                        AnsCallback_t saved = mote->t[n].AnsCallback;
                        //printf("ok\n");
                        mote->t[n].AnsCallback = NULL;
                        saved(sql_conn_ns_web, mote, inJobj, rxResult, sender_id, payloadBufPtr, payloadLen);

                        answered = true;
                    } else
                        printf("\e[31mParseJson() %s, tid %lu no AnsCallback %u\e[0m\n", pmt, trans_id, n);
                    goto noAns; // dont answer an answer
                }
            }
        }
        if (!answered) {
            printf("\e[31msNS ParseJson() Ans tid %lu not found\e[0m\n", trans_id);
            goto noAns; // dont answer an answer
        }
    } else {
        /*********** only requests here ***********/

        /* uplink reqs:
         *  with ULMetadata:                     PRStartReq, HRStartReq, XmitDataReq
         *  with PHYPayload: JoinReq, RejoinReq, PRStartReq, HRStartReq, XmitDataReq
         *                    ->JS  ,    ->JS  */
        if (pmt == ProfileReq) {
            uint32_t nid;
            sscanf(sender_id, "%x", &nid);
            if (isNetID(sql_conn_ns_web, nid, "count(1)") != 1) {
                Result = NoRoamingAgreement;
                goto Ans;
            }
            if (json_object_object_get_ex(inJobj, DevEUI, &obj)) {
                uint64_t devEui;
                sscanf(json_object_get_string(obj), "%"PRIx64, &devEui);
                Result = jsonGetDeviceProfile(devEui, sender_id, ansJobj);
            } else
                Result = MalformedRequest;
            goto Ans;
        } else if (pmt == AppSKeyReq) {
            Result = getAppSKey(inJobj, sender_id, ansJobj);
            goto Ans;
        }

        if (pmt == PRStopReq || pmt == HRStopReq)
            Result = RStop(sql_conn_ns_web, pmt, inJobj, sender_id, ansJobj);
        else {
            if (json_object_object_get_ex(inJobj, ULMetaData, &obj))
                Result = uplinkJson(sql_conn_ns_web, trans_id, sender_id, client_addr, obj, inJobj, pmt, frm, payloadBufPtr, payloadLen, ansJobj);
            else if (json_object_object_get_ex(inJobj, DLMetaData, &obj))
                Result = downlinkJson(sql_conn_ns_web, trans_id, sender_id, client_addr, obj, inJobj, pmt, frm, payloadBufPtr, payloadLen, ansJobj);
            else {
                printf("\e[31mn%s either ULMetaData or DLMetaData\e[0m\n", pmt);
                Result = MalformedRequest;
            }
        }

        if (Result == NULL) {
            printf(" noAns");
            goto noAns; /* not generating answer here: answer will be sent after sNS completes */
        }
    } // ..isReq

Ans:
    /* even if this was a request, answer might not be able to be obtained immedately, instead would be sent later */
    if (ansJobj != NULL) {
        printf("end-ParseJson() %s Ans:%p ", Result, *ansJobj);
        if (ansJobj != NULL && *ansJobj != NULL) {
            lib_generate_json(*ansJobj, sender_id, myNetwork_idStr, trans_id, _ansMt, Result);
        }
    } else
        printf("\e[31mNULL ansJobj\e['0m\n");
noAns:
    printf("\n");
} // ..ParseJson()

int updateNetwork(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    char query[512];
    char str[17];

    strcpy(query, "UPDATE roaming SET PRAllowed = ");
    strcat(query, ai->ServiceProfile.PRAllowed ? "1" : "0");
    strcat(query, ", HRAllowed = ");
    strcat(query, ai->ServiceProfile.HRAllowed ? "1" : "0");
    strcat(query, ", RAAllowed = ");
    strcat(query, ai->ServiceProfile.RAAllowed ? "1" : "0");
    strcat(query, ", fMICup = ");
    strcat(query, ai->network.fMICup ? "1" : "0");

    strcat(query, ", KEKlabel = ");
    if (strlen(ai->network.KEKlabel) > 0) {
        strcat(query, "'");
        strcat(query, ai->network.KEKlabel);
        strcat(query, "'");
    } else
        strcat(query, "NULL");

    strcat(query, ", KEK = ");
    if (strlen(ai->network.KEKstr) > 0) {
        strcat(query, "0x");
        strcat(query, ai->network.KEKstr);
    } else
        strcat(query, "NULL");

    strcat(query, " WHERE NetID = ");
    sprintf(str, "%u", ai->network.NetID);
    strcat(query, str);
    printf("%s\n", query);
    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "%s<br>update roaming: %s", query, mysql_error(sql_conn_ns_web));
        return -1;
    }
    if (mysql_affected_rows(sql_conn_ns_web) < 1) {
        snprintf(failMsg, sizeof_failMsg, "roaming: nothing updated");
        return -1;
    }

    return 0;
}

int deleteNetwork(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    char query[512];
    sprintf(query, "DELETE FROM roaming WHERE NetID = %u", ai->network.NetID);
    printf("%s\n", query);
    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "delete from roaming: %s", mysql_error(sql_conn_ns_web));
        return -1;
    }
    if (mysql_affected_rows(sql_conn_ns_web) < 1) {
        snprintf(failMsg, sizeof_failMsg, "delete network: nothing deleted");
        return -1;
    }

    return 0;
}

int changeGatewayRegion(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    char query[512];

    if (ai->RFRegion == NULL) {
        snprintf(failMsg, sizeof_failMsg, "NULL RFRegion");
        return -1;
    }

    sprintf(query, "UPDATE gateways SET RFRegion = '%s' WHERE eui = %"PRIu64, ai->RFRegion, ai->gatewayID);
    //printf("%s\n", query);
    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "%s: %s\n", query, mysql_error(sql_conn_ns_web));
        return -1;
    }
    if (mysql_affected_rows(sql_conn_ns_web) < 1) {
        snprintf(failMsg, sizeof_failMsg, "no rows affected: %s\n", query);
        return -1;
    }

    gateway_disconnect(ai->gatewayID);

    return 0;
}

int stopRoaming(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    char devEuiStr[64];
    char devAddrStr[32];
    uint64_t devEui;
    uint32_t devAddr;

    if (getTarget(ai->target, devEuiStr, sizeof(devEuiStr), devAddrStr, sizeof(devAddrStr)) < 0) {
        snprintf(failMsg, sizeof_failMsg, "get target \"%s\"", ai->target);
        return -1;
    }

    sscanf(devEuiStr, "%"PRIu64, &devEui);
    sscanf(devAddrStr, "%u", &devAddr);
    printf("stopRoaming() \"%s\", \"%s\" --- %"PRIx64" / %08x\n", devEuiStr, devAddrStr, devEui, devAddr);
    // NONE_DEVADDR
    mote_t* mote = getMote(sql_conn_ns_web, &mote_list, devEui, devAddr);
    if (mote == NULL) {
        snprintf(failMsg, sizeof_failMsg, "mote not found");
        return -1;
    }
    sNS_roamStop(mote);
    return 0;
}

int updateRoaming(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    char devEuiStr[64];
    char devAddrStr[32];
    char query[512];
    char where[128];

    if (getTarget(ai->target, devEuiStr, sizeof(devEuiStr), devAddrStr, sizeof(devAddrStr)) < 0) {
        snprintf(failMsg, sizeof_failMsg, "get target \"%s\"", ai->target);
        return -1;
    }

    if (strGetMotesWhere(devEuiStr, devAddrStr, where) != Success)
        return -1;

    /* sNSLifetime is source of duration of roaming */
    sprintf(query, "UPDATE motes SET sNSLifetime = %u, fwdToNetID = %s WHERE %s", ai->Lifetime, ai->NetIDstr, where);
    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "update motes: %s\n", mysql_error(sql_conn_ns_web));
        return -1;
    }
    if (mysql_affected_rows(sql_conn_ns_web) < 1) {
        snprintf(failMsg, sizeof_failMsg, "update motes: nothing updated: %s\n", query);
        return -1;
    }
    return 0;
}

int addNetwork(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    char query[512];

    if (ai->network.NetID == myNetwork_id32) {
        snprintf(failMsg, sizeof_failMsg, "can only roam with other networks");
        return -1;
    }

    sprintf(query, "INSERT INTO roaming (NetID, PRAllowed, HRAllowed, RAAllowed, fMICup) VALUES (%u, %u, %u, %u, %u)", ai->network.NetID, ai->ServiceProfile.PRAllowed, ai->ServiceProfile.HRAllowed, ai->ServiceProfile.RAAllowed, ai->network.fMICup);
    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "insert into roaming: %s", mysql_error(sql_conn_ns_web));
        return -1;
    }

    return 0;
}

/* updateSKeys ABP only */
static int
updateSKeys(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    char query[512];
    bool OptNeg;
    uint8_t SNwkSIntKeyBin[LORA_CYPHERKEYBYTES];
    uint8_t FNwkSIntKeyBin[LORA_CYPHERKEYBYTES];
    uint8_t NwkSEncKeyBin[LORA_CYPHERKEYBYTES];
    unsigned n;
    char buf[16];
    MYSQL_RES *result;
    MYSQL_ROW row;
    unsigned mote_id;

    if (numHexDigits(ai->SNwkSIntKey) < 32 || ascii_hex_to_buf(ai->SNwkSIntKey, SNwkSIntKeyBin) < 0) {
        snprintf(failMsg, sizeof_failMsg, "%s < 32 hex digits", SNwkSIntKey);
        return -1;
    }

    if (numHexDigits(ai->FNwkSIntKey) < 32 || ascii_hex_to_buf(ai->FNwkSIntKey, FNwkSIntKeyBin) < 0) {
        snprintf(failMsg, sizeof_failMsg, "%s < 32 hex digits", FNwkSIntKey);
        return -1;
    }

    if (numHexDigits(ai->NwkSEncKey) < 32 || ascii_hex_to_buf(ai->NwkSEncKey, NwkSEncKeyBin) < 0) {
        snprintf(failMsg, sizeof_failMsg, "%s < 32 hex digits", NwkSEncKey);
        return -1;
    }


    strcpy(query, "UPDATE sessions SET ");
    strcat(query, SNwkSIntKey);
    strcat(query, " = 0x");
    for (n = 0; n < LORA_CYPHERKEYBYTES; n++) {
        sprintf(buf, "%02x", SNwkSIntKeyBin[n]);
        strcat(query, buf);
    }
    strcat(query, ", ");
    strcat(query, FNwkSIntKey);
    strcat(query, " = 0x");
    for (n = 0; n < LORA_CYPHERKEYBYTES; n++) {
        sprintf(buf, "%02x", FNwkSIntKeyBin[n]);
        strcat(query, buf);
    }
    strcat(query, ", ");
    strcat(query, NwkSEncKey);
    strcat(query, " = 0x");
    for (n = 0; n < LORA_CYPHERKEYBYTES; n++) {
        sprintf(buf, "%02x", NwkSEncKeyBin[n]);
        strcat(query, buf);
    }
    strcat(query, " WHERE DevAddr = ");
    strcat(query, ai->devAddrStr);

    printf("%s\n", query);
    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "update SKeys: %s\n", mysql_error(sql_conn_ns_web));
        return -1;
    }
    if (mysql_affected_rows(sql_conn_ns_web) < 1) {
        snprintf(failMsg, sizeof_failMsg, "update SKeys: nothing updated");
        return -1;
    }

    sprintf(query, "SELECT ID FROM sessions WHERE DevAddr = %s", ai->devAddrStr);
    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "%s: %s\n", query, mysql_error(sql_conn_ns_web));
        return -1;
    }
    result = mysql_use_result(sql_conn_ns_web);
    if (result) {
        row = mysql_fetch_row(result);
        if (row) {
            sscanf(row[0], "%u", &mote_id);
        } else {
            mysql_free_result(result);
            snprintf(failMsg, sizeof_failMsg, "sessions DevAddr %s: no row\n", query);
            return -1;
        }
        mysql_free_result(result);
    } else {
        snprintf(failMsg, sizeof_failMsg, "%s: no result\n", query);
        return -1;
    }

    if (memcmp(FNwkSIntKeyBin, SNwkSIntKeyBin, LORA_CYPHERKEYBYTES) == 0 &&
        memcmp(FNwkSIntKeyBin, NwkSEncKeyBin, LORA_CYPHERKEYBYTES) == 0)
        OptNeg = false;
    else
        OptNeg = true;

    sprintf(query, "UPDATE motes SET OptNeg = %u WHERE ID = %u", OptNeg, mote_id);
    printf("%s\n", query);
    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "update motes: %s\n", mysql_error(sql_conn_ns_web));
        return -1;
    }
    /*if (mysql_affected_rows(sql_conn_ns_web) < 1) {
        snprintf(failMsg, sizeof_failMsg, "update OptNeg: nothing updated: %s\n", query);
        return -1;
    }*/

    return 0;
}

int
insertServiceProfile(const ServiceProfile_t* sp, my_ulonglong id)
{
    char query[2048];

    if (strlen(sp->ULRatePolicy) > 16) {
        printf("\e[31mULRatePolicy too large\e[0m\n");
        return -1;
    }
    if (strlen(sp->DLRatePolicy) > 16) {
        printf("\e[31mDLRatePolicy too large\e[0m\n");
        return -1;
    }
    if (strlen(sp->ChannelMask) > 32) {
        printf("\e[31mChannelMask too large\e[0m\n");
        return -1;
    }
        
        

    sprintf(query, "INSERT INTO ServiceProfiles ("
        "ServiceProfileID,"
        "ULRate,"
        "ULBucketSize,"
        "ULRatePolicy,"
        "DLRate,"
        "DLBucketSize,"
        "DLRatePolicy,"
        "AddGWMetadata,"
        "DevStatusReqFreq,"
        "ReportDevStatusBattery,"
        "ReportDevStatusMargin,"
        "DRMin,"
        "DRMax,"
        "ChannelMask,"
        "PRAllowed,"
        "HRAllowed,"
        "RAAllowed,"
        "NwkGeoLoc,"
        "TargetPER,"
        "MinGWDiversity"
        ") VALUES ("
        "%llu,"/* ServiceProfileID */
        "%u,"/* unsigned ULRate */
        "%u,"/* unsigned ULBucketSize */
        "'%s',"/* char ULRatePolicy[32] */
        "%u,"/* unsigned DLRate */
        "%u,"/* unsigned DLBucketSize */
        "'%s',"/* char DLRatePolicy[32] */
        "%u,"/* bool AddGWMetadata */
        "%u,"/* unsigned DevStatusReqFreq */
        "%u,"/* bool ReportDevStatusBattery */
        "%u,"/* bool ReportDevStatusMargin */
        "%u,"/* unsigned DRMin */
        "%u,"/* unsigned DRMax */
        "'%s',"/* char ChannelMask[32] */
        "%u,"/* bool PRAllowed */
        "%u,"/* bool HRAllowed */
        "%u,"/* bool RAAllowed */
        "%u,"/* bool NwkGeoLoc */
        "'%f',"/* float TargetPER */
        "%u"/* unsigned MinGWDiversity */
        ")",
        id,
        sp->ULRate,
        sp->ULBucketSize,
        sp->ULRatePolicy,
        sp->DLRate,
        sp->DLBucketSize,
        sp->DLRatePolicy,
        sp->AddGWMetadata,
        sp->DevStatusReqFreq,
        sp->ReportDevStatusBattery,
        sp->ReportDevStatusMargin,
        sp->DRMin,
        sp->DRMax,
        sp->ChannelMask,
        sp->PRAllowed,
        sp->HRAllowed,
        sp->RAAllowed,
        sp->NwkGeoLoc,
        sp->TargetPER,
        sp->MinGWDiversity
    );
    printf("%s\n", query);
    if (mysql_query(sql_conn_ns_web, query)) {
        return -1;
    }
    return mysql_affected_rows(sql_conn_ns_web);
}

int
updateServiceProfile(const ServiceProfile_t* sp, my_ulonglong id)
{
    char query[2048];

    sprintf(query, "UPDATE ServiceProfiles SET "
        "%s = %u, " /* ULRate*/
        "%s = %u, " /* ULBucketSize*/
        "%s = '%s', " /* ULRatePolicy*/
        "%s = %u, " /* DLRate*/
        "%s = %u, " /* DLBucketSize*/
        "%s = '%s', " /* DLRatePolicy*/
        "%s = %u, " /* AddGWMetadata*/
        "%s = %u, " /* DevStatusReqFreq*/
        "%s = %u, " /* ReportDevStatusBattery*/
        "%s = %u, " /* ReportDevStatusMargin*/
        "%s = %u, " /* DRMin*/
        "%s = %u, " /* DRMax*/
        "%s = '%s', " /* ChannelMask*/
        "%s = %u, " /* PRAllowed*/
        "%s = %u, " /* HRAllowed*/
        "%s = %u, " /* RAAllowed*/
        "%s = %u, " /* NwkGeoLoc*/
        "%s = '%f', " /* TargetPER*/
        "%s = %u " /* MinGWDiversity*/
        "WHERE ServiceProfileID = %llu", 
        ULRate, sp->ULRate,
        ULBucketSize, sp->ULBucketSize,
        ULRatePolicy, sp->ULRatePolicy,
        DLRate, sp->DLRate,
        DLBucketSize, sp->DLBucketSize,
        DLRatePolicy, sp->DLRatePolicy,
        AddGWMetadata, sp->AddGWMetadata,
        DevStatusReqFreq, sp->DevStatusReqFreq,
        ReportDevStatusBattery, sp->ReportDevStatusBattery,
        ReportDevStatusMargin, sp->ReportDevStatusMargin,
        DRMin, sp->DRMin,
        DRMax, sp->DRMax,
        ChannelMask, sp->ChannelMask,
        PRAllowed, sp->PRAllowed,
        HRAllowed, sp->HRAllowed,
        RAAllowed, sp->RAAllowed,
        NwkGeoLoc, sp->NwkGeoLoc,
        TargetPER, sp->TargetPER,
        MinGWDiversity, sp->MinGWDiversity,
        id
    );
    if (mysql_query(sql_conn_ns_web, query)) {
        return -1;
    }

    return 0;
}

static int
updateProfiles(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    //MYSQL_RES *result;
    //uint64_t devEui = NONE_DEVEUI;
    char query[2048];

    if (mysql_query(sql_conn_ns_web, "BEGIN")) {
        snprintf(failMsg, sizeof_failMsg, "BEGIN %s", mysql_error(sql_conn_ns_web));
        return -1;
    }

    if (updateDeviceProfile(sql_conn_ns_web, &ai->DeviceProfile, ai->moteID) < 0) {
        snprintf(failMsg, sizeof_failMsg, "update DeviceProfiles %s", mysql_error(sql_conn_ns_web));
        return -1;
    }

    if (updateServiceProfile(&ai->ServiceProfile, ai->moteID) < 0) {
        snprintf(failMsg, sizeof_failMsg, "ServiceProfiles %s", mysql_error(sql_conn_ns_web));
        return -1;
    }

    /* routing */
    sprintf(query, "UPDATE RoutingProfiles SET "
        "AS_ID = '%s' " /* AS_ID */
        "WHERE RoutingProfileID = %llu", 
        ai->RoutingProfile.AS_ID,
        ai->moteID
    );
    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "RoutingProfiles %s", mysql_error(sql_conn_ns_web));
        return -1;
    }

    if (mysql_query(sql_conn_ns_web, "COMMIT")) {
        snprintf(failMsg, sizeof_failMsg, "COMMIT %s", mysql_error(sql_conn_ns_web));
        return -1;
    }

#if 0
    sprintf(query, "SELECT DevEUI FROM motes WHERE ID = %llu", ai->moteID);
    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "%s: %s", query, mysql_error(sql_conn_ns_web));
        return -1;
    }
    result = mysql_use_result(sql_conn_ns_web);
    if (result) {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            if (row[0])
                sscanf(row[0], "%"PRIu64, &devEui);
        } else {
            mysql_free_result(result);
            snprintf(failMsg, sizeof_failMsg, "motes DevEUI %s: no row", query);
            return -1;
        }
        mysql_free_result(result);
    } else {
        snprintf(failMsg, sizeof_failMsg, "%s: no result", query);
        return -1;
    }

    if (devEui != NONE_DEVEUI) {
        moteReadProfiles(devEui, NONE_DEVADDR);
        return 0;
    }

    /* ABP get DevAddr */
    sprintf(query, "SELECT DevAddr FROM sessions WHERE ID = %llu", ai->moteID);
    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "%s: %s", query, mysql_error(sql_conn_ns_web));
        return -1;
    }
    result = mysql_use_result(sql_conn_ns_web);
    if (result) {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            uint32_t devAddr;
            sscanf(row[0], "%u", &devAddr);
            moteReadProfiles(NONE_DEVEUI, devAddr);
        } else {
            mysql_free_result(result);
            snprintf(failMsg, sizeof_failMsg, "sessions DevAddr %s: no row", query);
            return -1;
        }
        mysql_free_result(result);
    } else {
        snprintf(failMsg, sizeof_failMsg, "%s: no result", query);
        return -1;
    }
#endif /* if 0 */

    return 0;
} // ..updateProfiles()

my_ulonglong
addMote(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    my_ulonglong ret;
    char query[1048];
    //MYSQL_RES *result;
    //char str[64];
    unsigned i;

    strcpy(query, "INSERT INTO motes (fwdToNetID, ");
    if (strlen(ai->DevEUIstr) > 0)
        strcat(query, "DevEUI, ");
    else if (strlen(ai->devAddrStr) == 0) {
        snprintf(failMsg, sizeof_failMsg, "addMote: nothing to add");
        return 0;
    }

    i = strlen(query);
    query[i-2] = 0;  // back over last ", "
    strcat(query, ") VALUES ("); 

    if (strlen(ai->NetIDstr) == 0)
        strcat(query, fwdToNetIDnone);
    else
        strcat(query, ai->NetIDstr);
    strcat(query, ", ");

    if (strlen(ai->DevEUIstr) > 0) {
        strcat(query, ai->DevEUIstr);
        strcat(query, ", ");
    }

    i = strlen(query);
    query[i-2] = 0;  // back over last ", "
    strcat(query, ")"); 

    //printf("%s\n", query);
    if (mysql_query(sql_conn_ns_web, query)) {
        //printf("%s\n", query);
        snprintf(failMsg, sizeof_failMsg, "addMote %u: %s", mysql_errno(sql_conn_ns_web), mysql_error(sql_conn_ns_web));
        return 0;
    }

    ret = mysql_insert_id(sql_conn_ns_web);

    if (strlen(ai->devAddrStr) > 0) {
#if 0
        unsigned mote_id;
        /* ABP: session must be created now, get just-now created mote ID */
        sprintf(query, "SELECT ID FROM motes ORDER BY ID DESC LIMIT 1");
        if (mysql_query(sql_conn_ns_web, query)) {
            printf("%s\n", query);
            snprintf(failMsg, sizeof_failMsg, "addMote %u: %s", mysql_errno(sql_conn_ns_web), mysql_error(sql_conn_ns_web));
            return 0;
        }
        result = mysql_use_result(sql_conn_ns_web);
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            sscanf(row[0], "%u", &mote_id);
            mysql_free_result(result);
        } else {
            snprintf(failMsg, sizeof_failMsg, "addMote get id: no result");
            return 0;
        }
        printf("new id %u\n", mote_id);
#endif /* if 0 */

        sprintf(query, "INSERT INTO sessions (ID, DevAddr) VALUES (%llu, %s)", ret, ai->devAddrStr);
        if (mysql_query(sql_conn_ns_web, query)) {
            snprintf(failMsg, sizeof_failMsg, "addMote: %s", mysql_error(sql_conn_ns_web));
            return 0;
        }
            //snprintf(failMsg, sizeof_failMsg, "addMote: mote_id %u", mote_id);
    }

    return ret;
} // ..addMote()

int
updateDeviceProfile(MYSQL* sc, const DeviceProfile_t* dp, my_ulonglong id)
{
    int ret = -1;
    char query[2048];

    printf("updateDeviceProfile(,%llu) ", id);
    sprintf(query, "UPDATE DeviceProfiles SET "
        "%s = %u, " /* SupportsClassB */
        "%s = %u, " /* ClassBTimeout */
        "%s = %u, " /* PingSlotPeriod */
        "%s = %u, " /* PingSlotDR */
        "%s = '%f', " /* PingSlotFreq */
        "%s = %u, " /* SupportsClassC */
        "%s = %u, " /* ClassCTimeout */
        "%s = '%s', " /* RegParamsRevision */
        "%s = %u, " /* SupportsJoin */
        "%s = '%s', " /* MACVersion */
        "%s = %u, " /* RXDelay1 */
        "%s = %u, " /* RXDROffset1 */
        "%s = %u, " /* RXDataRate2 */
        "%s = '%f', " /* RXFreq2 */
        "%s = '%s', " /* FactoryPresetFreqs */
        "%s = %d, " /* MaxEIRP */
        "%s = '%f', " /* MaxDutyCycle */
        "%s = '%s', " /* RFRegion */
        "%s = %u " /* Supports32bitFCnt */
        "WHERE DeviceProfileID = %llu", 
        SupportsClassB,     dp->SupportsClassB,
        ClassBTimeout,      dp->ClassBTimeout,
        PingSlotPeriod,     dp->PingSlotPeriod,
        PingSlotDR,         dp->PingSlotDR,
        PingSlotFreq,       dp->PingSlotFreq,
        SupportsClassC,     dp->SupportsClassC,
        ClassCTimeout,      dp->ClassCTimeout,
        RegParamsRevision,  dp->RegParamsRevision,
        SupportsJoin,       dp->SupportsJoin,
        MACVersion,         dp->MACVersion,
        RXDelay1,           dp->RXDelay1,
        RXDROffset1,        dp->RXDROffset1,
        RXDataRate2,        dp->RXDataRate2,
        RXFreq2,            dp->RXFreq2,
        FactoryPresetFreqs, dp->FactoryPresetFreqs,
        MaxEIRP,            dp->MaxEIRP,
        MaxDutyCycle,       dp->MaxDutyCycle,
        RFRegion,           dp->RFRegion,
        Supports32bitFCnt,  dp->Supports32bitFCnt,
        id
    );
    printf("%s\r\n", query);
    if (sc == NULL) {
        ret = mq_send(mqd, query, strlen(query)+1, 0);
        if (ret < 0) {
            perror("updateDeviceProfile mq_send");
            printf("query len %zu\n", strlen(query)+1);
            return ret;
        }
    } else {
        if (mysql_query(sc, query)) {
            printf("\e[31mupdateDeviceProfile() %s\e[0m\n", mysql_error(sc));
            return -1;
        }
        printf("\e33maffected-rows:%llu\e[0m\n", mysql_affected_rows(sc));
        if (mysql_affected_rows(sc) == 0) {
            /* nothing changed, just update time stamp */
            strcpy(query, "UPDATE DeviceProfiles SET timestamp = CURRENT_TIMESTAMP()");
            if (mysql_query(sc, query)) {
                printf("\e[31mupdateDeviceProfile() %s\e[0m\n", mysql_error(sc));
                return -1;
            }
        }
        ret = 0;
    }

    return 0;
} // ..updateDeviceProfile()

int 
insertDeviceProfile(MYSQL* sc, DeviceProfile_t* dp, my_ulonglong id)
{
    struct _region_list* rl;
    const char* rfRegion = getRFRegion(dp->RFRegion);
    char query[2048];

    printf("insertDeviceProfile() ");
    for (rl = region_list; rl != NULL; rl = rl->next) {
        if (rfRegion == rl->region.RFRegion) {
            const regional_t* rp = &(rl->region.regional);
            dp->RXDROffset1 = rp->defaults.Rx1DrOffset;
            dp->RXFreq2 = rp->defaults.Rx2Channel.FrequencyHz / 1000000.0;
            dp->RXDataRate2 = rp->defaults.Rx2Channel.Datarate;
            if (rp->defaults.ping_freq_hz != 0)
                dp->PingSlotFreq = rp->defaults.ping_freq_hz;
            dp->PingSlotDR = rp->defaults.ping_dr;
            // TODO ServiceProfile --- rl->region.regional->defaults.uplink_dr_max
            printf("setting defaults rx2:%.2f,dr%u\n", dp->RXFreq2, dp->RXDataRate2);
            break;
        }
    }

    /* enforce regional PHY values into DeviceProfile */
    /*if (rfRegion == US902) {
        dp->RXFreq2 = 923.3;
        dp->RXDataRate2 = 8;
    } else
        printf("\e[31mTODO regiona defaults %s\e[0m\n", dp->RFRegion);*/

    sprintf(query, "INSERT INTO DeviceProfiles ("
        "DeviceProfileID,"
        "SupportsClassB,"
        "ClassBTimeout,"
        "PingSlotPeriod,"
        "PingSlotDR,"
        "PingSlotFreq,"
        "SupportsClassC,"
        "ClassCTimeout,"
        "RegParamsRevision,"
        "SupportsJoin,"
        "MACVersion,"
        "RXDelay1,"
        "RXDROffset1,"
        "RXDataRate2,"
        "RXFreq2,"
        "FactoryPresetFreqs,"
        "MaxEIRP,"
        "MaxDutyCycle,"
        "RFRegion,"
        "Supports32bitFCnt"
        ") VALUES ("
        "%llu,"  /* 1 DeviceProfileID */
        "%u,"    /* 2 SupportsClassB*/
        "%u,"    /* 3 ClassBTimeout*/
        "%u,"    /* 4 PingSlotPeriod*/
        "%u,"    /* 5 PingSlotDR*/
        "'%f',"  /* 6 PingSlotFreq */
        "%u,"    /* 7 SupportsClassC */
        "%u,"    /* 8 ClassCTimeout*/
        "'%s',"  /* 9 RegParamsRevision*/
        "%u,"   /* 10 SupportsJoin*/
        "'%s'," /* 11 MACVersion*/
        "%u,"   /* 12 RXDelay1*/
        "%u,"   /* 13 RXDROffset1*/
        "%u,"   /* 14 RXDataRate2*/
        "'%f'," /* 15 RXFreq2*/
        "'%s'," /* 16 FactoryPresetFreqs*/
        "%d,"   /* 17 MaxEIRP*/
        "'%f'," /* 18 MaxDutyCycle*/
        "'%s'," /* 19 RFRegion*/
        "%u"    /* 20 Supports32bitFCnt*/
        ")",
        id,
        dp->SupportsClassB,
        dp->ClassBTimeout,
        dp->PingSlotPeriod,
        dp->PingSlotDR,
        dp->PingSlotFreq,
        dp->SupportsClassC,
        dp->ClassCTimeout,
        dp->RegParamsRevision,
        dp->SupportsJoin,
        dp->MACVersion,
        dp->RXDelay1,
        dp->RXDROffset1,
        dp->RXDataRate2,
        dp->RXFreq2,
        dp->FactoryPresetFreqs,
        dp->MaxEIRP,
        dp->MaxDutyCycle,
        dp->RFRegion,
        dp->Supports32bitFCnt
    );
    printf("%s\n", query);
    if (sc == NULL) {
        int ret = mq_send(mqd, query, strlen(query)+1, 0);
        if (ret < 0) {
            perror("insertDeviceProfile mq_send");
            printf("query len %zu\n", strlen(query)+1);
        }
        return ret;
    } else {
        if (mysql_query(sc, query)) {
            printf("\e[31minsertDeviceProfile() %s\e[0m\n", mysql_error(sc));
            return -1;
        }
        return mysql_affected_rows(sc);
    }
} // ..insertDeviceProfile();

int
newProfiles(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg, bool newMote)
{
    int ret;
    char query[2048];
    my_ulonglong m_id;

    /* if target is null, then creating new mote with profiles */
    /* if target is given, then adding profiles to existing mote */
    printf("newProfiles() newMote:%u\n", newMote);

    if (newMote) {
        m_id = addMote(ai, failMsg, sizeof_failMsg);

        if (mysql_query(sql_conn_ns_web, "BEGIN")) {
            snprintf(failMsg, sizeof_failMsg, "newProfiles BEGIN %s", mysql_error(sql_conn_ns_web));
            return -1;
        }
        printf("BEGIN new\n");
    } else {
        MYSQL_RES *result;
        char where[128];
        if (strGetMotesWhere(ai->DevEUIstr, ai->devAddrStr, where) != Success)
            return -1;

        sprintf(query, "SELECT ID FROM motes WHERE %s", where);
        if (mysql_query(sql_conn_ns_web, query)) {
            snprintf(failMsg, sizeof_failMsg, "%s: %s\n", query, mysql_error(sql_conn_ns_web));
            return -1;
        }
        result = mysql_use_result(sql_conn_ns_web);
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row) {
                sscanf(row[0], "%llu", &m_id);
            } else {
                mysql_free_result(result);
                snprintf(failMsg, sizeof_failMsg, "motes ID %s: no row\n", query);
                return -1;
            }
            mysql_free_result(result);
        } else {
            snprintf(failMsg, sizeof_failMsg, "%s: no result\n", query);
            return -1;
        }

        if (mysql_query(sql_conn_ns_web, "BEGIN")) {
            snprintf(failMsg, sizeof_failMsg, "newProfiles BEGIN %s", mysql_error(sql_conn_ns_web));
            return -1;
        }
        printf("BEGIN add\n");
    }

    if (insertDeviceProfile(sql_conn_ns_web, &ai->DeviceProfile, m_id) != 1) {
        snprintf(failMsg, sizeof_failMsg, "newProfiles insertDeviceProfile(): %s", mysql_error(sql_conn_ns_web));
        return -1;
    }

    ret = insertServiceProfile(&ai->ServiceProfile, m_id);
    if (ret < 0) {
        snprintf(failMsg, sizeof_failMsg, "newProfiles ServiceProfiles %s", mysql_error(sql_conn_ns_web));
        return -1;
    } else if (ret != 1) {
        snprintf(failMsg, sizeof_failMsg, "newProfiles ServiceProfiles %d inserted rows", ret);
        return -1;
    }

    sprintf(query, "INSERT INTO RoutingProfiles (RoutingProfileID, AS_ID) VALUES (%llu, '%s')", m_id, ai->RoutingProfile.AS_ID);
    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "newProfiles RoutingProfiles %s", mysql_error(sql_conn_ns_web));
        return -1;
    }
    ret = mysql_affected_rows(sql_conn_ns_web);
    if (ret != 1) {
        snprintf(failMsg, sizeof_failMsg, "newProfiles ServiceProfiles %d inserted rows", ret);
        return -1;
    }

    if (mysql_query(sql_conn_ns_web, "COMMIT")) {
        snprintf(failMsg, sizeof_failMsg, "newProfiles COMMIT %s", mysql_error(sql_conn_ns_web));
        return -1;
    }

    return 0;
} // ...newProfiles()

void checkProfiles(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    if (ai->DeviceProfile.SupportsClassB) {
        /*if (ai->DeviceProfile.PingSlotPeriod == 0) {  // Provided by end-device via PingSlotInfoReq
            snprintf(failMsg, sizeof_failMsg, "%s required for classB", PingSlotPeriod);
            return;
        }*/
        //printf("checkProfiles PingSlotFreq:%f, PingSlotDR:%u\n", ai->DeviceProfile.PingSlotFreq, ai->DeviceProfile.PingSlotDR);
        if (ai->DeviceProfile.PingSlotFreq == FLT_MAX) {
            snprintf(failMsg, sizeof_failMsg, "%s required for classB", PingSlotFreq);
            return;
        }
        if (ai->DeviceProfile.PingSlotDR == UINT_MAX) {
            snprintf(failMsg, sizeof_failMsg, "%s required for classB", PingSlotDR);
            return;
        }
    } else {
        if (ai->DeviceProfile.PingSlotFreq == FLT_MAX)
            ai->DeviceProfile.PingSlotFreq = 0; // truncate large value
        if (ai->DeviceProfile.PingSlotDR == UINT_MAX)
            ai->DeviceProfile.PingSlotDR = 0; // truncate large value
    }

    if (strlen(ai->DeviceProfile.MACVersion) < 1) {
        snprintf(failMsg, sizeof_failMsg, "%s required", MACVersion);
        return;
    }

    if (ai->DeviceProfile.MaxEIRP == INT_MAX) {
        snprintf(failMsg, sizeof_failMsg, "%s required", MaxEIRP);
        return;
    }

    if (ai->DeviceProfile.SupportsClassC) {
        if (ai->DeviceProfile.ClassCTimeout == UINT_MAX) {
            snprintf(failMsg, sizeof_failMsg, "%s required", MaxEIRP);
            return;
        }
    }

    if (!ai->DeviceProfile.SupportsJoin) {
        if (ai->DeviceProfile.RXDelay1 == UINT_MAX) {
            snprintf(failMsg, sizeof_failMsg, "ABP %s required", RXDelay1);
            return;
        }
        if (ai->DeviceProfile.RXDROffset1 == UINT_MAX) {
            snprintf(failMsg, sizeof_failMsg, "ABP %s required", RXDROffset1);
            return;
        }
        if (ai->DeviceProfile.RXDataRate2 == UINT_MAX) {
            snprintf(failMsg, sizeof_failMsg, "ABP %s required", RXDataRate2);
            return;
        }
        if (ai->DeviceProfile.RXFreq2 == FLT_MAX) {
            snprintf(failMsg, sizeof_failMsg, "ABP %s required", RXFreq2);
            return;
        }
    } else {
        /* OTA */
        // shorten the reset value
        if (ai->DeviceProfile.RXDelay1 == UINT_MAX)
            ai->DeviceProfile.RXDelay1 = 1;
        if (ai->DeviceProfile.RXDROffset1 == UINT_MAX)
            ai->DeviceProfile.RXDROffset1 = 0;
        if (ai->DeviceProfile.RXFreq2 == FLT_MAX)
            ai->DeviceProfile.RXFreq2 = 0;
        if (ai->DeviceProfile.RXDataRate2 == UINT_MAX)
            ai->DeviceProfile.RXDataRate2 = 0;
    }
}

static int
deleteMote(char* failMsg, size_t sizeof_failMsg, const char* target)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    char query[512];
    char devEuiStr[64];
    char devAddrStr[32];
    my_ulonglong mote_id;

    if (getTarget(target, devEuiStr, sizeof(devEuiStr), devAddrStr, sizeof(devAddrStr)) < 0) {
        snprintf(failMsg, sizeof_failMsg, "deleteMote getTarget \"%s\"", target);
        return -1;
    }

    /* devEui and devAddr are provided as ascii decimal */
    if (strlen(devEuiStr) > 0) {
        /* OTA: get profile ID from DevEUI */
        sprintf(query, "SELECT ID FROM motes WHERE DevEUI = %s", devEuiStr);
    } else if (strlen(devAddrStr) > 0) {
        /* ABP: get mote ID from session, then profile ID from mote ID */
        sprintf(query, "SELECT ID FROM sessions WHERE DevAddr = %s", devAddrStr);
    }

    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "%s: %s\n", query, mysql_error(sql_conn_ns_web));
        return -1;
    }
    result = mysql_use_result(sql_conn_ns_web);
    if (result) {
        row = mysql_fetch_row(result);
        if (row) {
            sscanf(row[0], "%llu", &mote_id);
        } else {
            mysql_free_result(result);
            snprintf(failMsg, sizeof_failMsg, "%s: no row\n", query);
            return -1;
        }
        mysql_free_result(result);
    } else {
        snprintf(failMsg, sizeof_failMsg, "%s: no result\n", query);
        return -1;
    }

    if (mysql_query(sql_conn_ns_web, "BEGIN")) {
        snprintf(failMsg, sizeof_failMsg, "BEGIN %s", mysql_error(sql_conn_ns_web));
        return -1;
    }

    sprintf(query, "DELETE FROM DeviceProfiles WHERE DeviceProfileID = %llu", mote_id);
    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "delete device profile: %s\n", mysql_error(sql_conn_ns_web));
        return -1;
    }
    sprintf(query, "DELETE FROM ServiceProfiles WHERE ServiceProfileID = %llu", mote_id);
    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "delete service profile: %s\n", mysql_error(sql_conn_ns_web));
        return -1;
    }
    sprintf(query, "DELETE FROM RoutingProfiles WHERE RoutingProfileID = %llu", mote_id);
    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "delete routing profile: %s\n", mysql_error(sql_conn_ns_web));
        return -1;
    }

    sprintf(query, "DELETE FROM motes WHERE ID = %llu", mote_id);
    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "%s: %s\n", query, mysql_error(sql_conn_ns_web));
        return -1;
    }

    sprintf(query, "DELETE FROM sessions WHERE ID = %llu", mote_id);
    if (mysql_query(sql_conn_ns_web, query)) {
        snprintf(failMsg, sizeof_failMsg, "%s: %s\n", query, mysql_error(sql_conn_ns_web));
        return -1;
    }

    if (mysql_query(sql_conn_ns_web, "COMMIT")) {
        snprintf(failMsg, sizeof_failMsg, "COMMIT %s", mysql_error(sql_conn_ns_web));
        return -1;
    }

    return 0;
} // ..deleteMote()

int
NwkAddrStr_to_devAddrStr(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    uint32_t nwkAddr, devAddr;
    if (sscanf(ai->NwkAddrStr, "%x", &nwkAddr) < 1) {
        snprintf(failMsg, sizeof_failMsg, "nwkAddr invalid");
        return -1;
    }
    if (nwkAddr > (1 << nwkAddrBits)+1) {
        snprintf(failMsg, sizeof_failMsg, "nwkAddr too large");
        return -1;
    }

    // NetID and TypePrefix already added to devAddrBase at startup
    devAddr = devAddrBase | nwkAddr;

    sprintf(ai->devAddrStr, "%u", devAddr);

    return 0;
} // ..NwkAddrStr_to_devAddrStr()

void
browser_post_submitted(const char* url, struct Request* request)
{
    char failMsg[1024];
    struct Session *session = request->session;
    appInfo_t* ai = session->appInfo;

    failMsg[0] = 0;
    HTTP_PRINTF("NS browser_post_submitted(%s, ) ai=%p\n", url, ai);
    if (strcmp(url, motesURL) == 0) {
        HTTP_PRINTF("\e[33mmotesURL op%u\e[0m\n", ai->opFlags.op);
        if (ai->opFlags.op == OPR_CREATE) {
            HTTP_PRINTF("\e[33mmotesURL add mote DevEUIstr:\"%s\", NwkAddrStr:\"%s\" NetIDstr:\"%s\"\e[0m\n", ai->DevEUIstr, ai->NwkAddrStr, ai->NetIDstr);
            if ((strlen(ai->DevEUIstr) == 0 && strlen(ai->NwkAddrStr) == 0) || (strlen(ai->DevEUIstr) != 0 && strlen(ai->NwkAddrStr) != 0))
                snprintf(failMsg, sizeof(failMsg) ,"enter either DevEUI(OTA) or NwkAddr(ABP)");
            else {
                if (strlen(ai->NwkAddrStr) > 0 && NwkAddrStr_to_devAddrStr(ai, failMsg, sizeof(failMsg)) < 0)
                    goto done;

                if (strlen(ai->DevEUIstr) > 0) {
                    ai->opFlags.ota = 1;
                } else {
                    ai->opFlags.ota = 0;
                }

                if (ai->opFlags.addProfiles) {
                    /* make this NS home NS */
                    if (strcmp(ai->NetIDstr, fwdToNetIDnone) != 0) {
                        snprintf(failMsg, sizeof(failMsg) ,"adding profiles requires foreign NetID to be none");
                        goto done;
                    }
                    if (ai->opFlags.addMote)
                        strcpy(ai->redir_url, addProfilesToNewURL);
                    else
                        strcpy(ai->redir_url, addProfilesToExistingURL);
                } else {
                    /* add mote without profiles (just for fNS or sNS) */
                    HTTP_PRINTF("notHome add\n");
                    if (addMote(ai, failMsg, sizeof(failMsg)) > 0)
                        strcpy(ai->redir_url, motesURL);
                }
            }
        } else if (ai->opFlags.op == OPR_DELETE) {
            if (deleteMote(failMsg, sizeof(failMsg), ai->target) == 0)
                strcpy(ai->redir_url, motesURL);
        } else if (ai->opFlags.op == OPR_EDIT) {
            /* shouldnt get here */
            snprintf(failMsg, sizeof(failMsg), "OPR_EDIT");
        } else if (ai->opFlags.op == OPR_ADD_PROFILES) {
            char devEuiStr[48];
            char devAddrStr[24];
            if (getTarget(ai->target, devEuiStr, sizeof(devEuiStr), devAddrStr, sizeof(devAddrStr)) < 0) {
                snprintf(failMsg, sizeof(failMsg), "OPR_ADD_PROFILES getTarget \"%s\"", ai->target);
                goto done;
            }
            HTTP_PRINTF("OPR_ADD_PROFILES target:%s ", ai->target);
            if (strlen(devEuiStr) > 0) {
                ai->opFlags.ota = 1;
                strcpy(ai->DevEUIstr, devEuiStr);
            } else {
                ai->opFlags.ota = 0;
                strcpy(ai->devAddrStr, devAddrStr);
            }
            strcpy(ai->redir_url, addProfilesToExistingURL);
        } else {
            snprintf(failMsg, sizeof(failMsg), "browser_post_submitted %s unknown op %d, at %p", url, ai->opFlags.op, &ai->opFlags);
        }
    } else if (strcmp(url, addProfilesToExistingURL) == 0) {
        HTTP_PRINTF("addProfilesToExistingURL DevEUI:%s, devAddr:%s\n", ai->DevEUIstr, ai->devAddrStr);
        checkProfiles(ai, failMsg, sizeof(failMsg));
        printf("addProfilesToExistingURL DevEUIstr:%s, devAddrStr:%s, target:%s\n", ai->DevEUIstr, ai->devAddrStr, ai->target);
        if (failMsg[0] == 0) {
            if (newProfiles(ai, failMsg, sizeof(failMsg), false) == 0)
                strcpy(ai->redir_url, motesURL);
        }
    } else if (strcmp(url, addProfilesToNewURL) == 0) {
        HTTP_PRINTF("addProfilesToNewURL DevEUI \"%s\", addr \"%s\"\n", ai->DevEUIstr, ai->devAddrStr);
        checkProfiles(ai, failMsg, sizeof(failMsg));
        if (failMsg[0] == 0) {
            if (newProfiles(ai, failMsg, sizeof(failMsg), true) == 0)
                strcpy(ai->redir_url, motesURL);
        }
    } else if (strcmp(url, updateProfilesURL) == 0) {
        checkProfiles(ai, failMsg, sizeof(failMsg));
        if (failMsg[0] == 0) {
            if (updateProfiles(ai, failMsg, sizeof(failMsg)) == 0)
                strcpy(ai->redir_url, motesURL);
        }
    } else if (strcmp(url, sKeyEditURL) == 0) {
        if (updateSKeys(ai, failMsg, sizeof(failMsg)) == 0)
            strcpy(ai->redir_url, motesURL);
    } else if (strcmp(url, networksURL) == 0) {
        if (ai->opFlags.op == OPR_DELETE) {
            if (deleteNetwork(ai, failMsg, sizeof(failMsg)) == 0)
                strcpy(ai->redir_url, networksURL);
        } else if (ai->opFlags.op == OPR_EDIT) {
            if (updateNetwork(ai, failMsg, sizeof(failMsg)) == 0)
                strcpy(ai->redir_url, networksURL);
        } else if (ai->opFlags.op == OPR_CREATE) {
            if (addNetwork(ai, failMsg, sizeof(failMsg)) == 0)
                strcpy(ai->redir_url, networksURL);
        }
    } else if (strcmp(url, roamingURL) == 0) {
        if (ai->opFlags.op == OPR_RSTOP ) {
            if (stopRoaming(ai, failMsg, sizeof(failMsg)) == 0)
                strcpy(ai->redir_url, motesURL);
        } else {
            if (updateRoaming(ai, failMsg, sizeof(failMsg)) == 0)
                strcpy(ai->redir_url, motesURL);
        }
    } else if (strcmp(url, gatewaysURL) == 0) {
        if (changeGatewayRegion(ai, failMsg, sizeof(failMsg)) == 0)
            strcpy(ai->redir_url, gatewaysURL);
    } else
        snprintf(failMsg, sizeof(failMsg), "unknown url %s", url);


done:
    if (strlen(failMsg) > 0) {
        printf("\e[31m%s\e[0m\n", failMsg);
        snprintf(postFailPage, sizeof(postFailPage), "<html><head><title>Failed</title></head><body>%s</body></html>", failMsg);
        strcpy(ai->redir_url, postFailURL);
    }

    HTTP_PRINTF("redir_url = \"%s\"\n", ai->redir_url);
    request->post_url = redirectURL;
} // ..browser_post_submitted()

void* create_appInfo()
{
    appInfo_t* ret = calloc(1, sizeof(appInfo_t));

    HTTP_PRINTF("\e[33mcreate_appInfo() %p\e[0m\n", ret);
    /* these values must be assigned by user */
    ret->DeviceProfile.MaxEIRP = INT_MAX;
    ret->DeviceProfile.SupportsClassC = UINT_MAX;
    ret->DeviceProfile.PingSlotDR = UINT_MAX;
    ret->DeviceProfile.RXDelay1 = UINT_MAX;
    ret->DeviceProfile.RXDROffset1 = UINT_MAX;
    ret->DeviceProfile.RXDataRate2 = UINT_MAX;
    ret->DeviceProfile.RXFreq2 = FLT_MAX;
    ret->DeviceProfile.PingSlotFreq = FLT_MAX;

    return ret;
}

int
web_init(const char* dbhostname, const char* dbuser, const char* dbpass, uint16_t dbport, const char *dbName)
{
    //return database_open(dbhostname, dbuser, dbpass, dbport, dbName, &sql_conn_ns_web, NS_VERSION)
    sql_conn_ns_web = mysql_init(NULL);
    if (sql_conn_ns_web == NULL) {
        fprintf(stderr, "Failed to initialize: %s\n", mysql_error(sql_conn_ns_web));
        return -1;
    }

    /* enable re-connect */
    my_bool reconnect = 1;
    if (mysql_options(sql_conn_ns_web, MYSQL_OPT_RECONNECT, &reconnect) != 0) {
        fprintf(stderr, "mysql_options() failed\n");
        return -1;
    }

    printf("database connect %s\n", dbName);
    /* Connect to the server */
    if (!mysql_real_connect(sql_conn_ns_web, dbhostname, dbuser, dbpass, dbName, dbport, NULL, 0))
    {
        fprintf(stderr, "Failed to connect to server: %s\n", mysql_error(sql_conn_ns_web));
        return -1;
    }
    return 0;
}

