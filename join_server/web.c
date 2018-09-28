/*
Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <stdbool.h>
#include "web.h"
#include "js.h"

struct Session *sessions;

static const char action_setHomeNetID[] = "setHomeNetID";  // key=action
static const char action_addRoamNetID[] = "addRoamNetID";  // key=action
static const char action_updateLifetime[] = "updateLifetime";  // key=action

static const char deleteNetID[] = "deleteNetID";    // data, netid value in key



typedef enum {
    OP_NONE,
    OP_ADD,
    OP_DEL,
    OP_SET_HOME_NETID,
    OP_ADD_ROAM_NETID,
    OP_DEL_ROAM_NETID,
    OP_LIFETIME
} op_e;

typedef enum {
    FORM_STATE_START,
    FORM_STATE_TABLE_HEADER,
    FORM_STATE_TABLE_ROWS,
    FORM_STATE_FORM,
    FORM_STATE_END
} form_state_e;

typedef struct {
    uint64_t DevEUI;
    char NwkKeyStr[LORA_CYPHERKEY_STRLEN];
    char AppKeyStr[LORA_CYPHERKEY_STRLEN];
    op_e op;
    uint32_t roamNetID, homeNetID;
    unsigned Lifetime;

    form_state_e form_state;
    MYSQL_RES *result;
    MYSQL_ROW row;

    char redir_url[128];
} appInfo_t;

const char*
getHomeNS(json_object* inJobj, json_object** ansJobj, const char* sender_id)
{
    char query[512];
    const char* ret = Other;
    uint64_t devEui;
    json_object* obj;
    MYSQL_RES *result;
    unsigned long moteID = ULONG_MAX;
    uint32_t ans_netid;
    uint32_t askingNetID;

    sscanf(sender_id, "%x", &askingNetID);

    *ansJobj = json_object_new_object();

    if (json_object_object_get_ex(inJobj, DevEUI, &obj)) {
        sscanf(json_object_get_string(obj), "%"PRIx64, &devEui);
    } else
        return MalformedRequest;

    sprintf(query, "SELECT homeNetID, ID FROM joinmotes WHERE eui = %"PRIu64, devEui);
    if (mysql_query(sql_conn_lora_join, query)) {
        sprintf("\e[31mgetHomeNS %s\e[0m\n", mysql_error(sql_conn_lora_join));
        return ret;
    }
    result = mysql_use_result(sql_conn_lora_join);
    if (result) {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            if (row[0]) {
                sscanf(row[0], "%u", &ans_netid);
                sscanf(row[1], "%lu", &moteID);
                ret = Success;
            } else {
                printf("\e[31mno home netid %"PRIu64" / %"PRIx64"\e[0m\n", devEui, devEui);
                ret = Other;
            }
        } else {
            //printf("\e[31mgetHomeNS no row %"PRIu64" / %"PRIx64"\e[0m\n", devEui, devEui);
            ret = UnknownDevEUI;
            printf("getHomeNS %s %016"PRIx64"\n", UnknownDevEUI, devEui);
        }
    } else {
        printf("\e[31mgetHomeNS no result %"PRIu64" / %"PRIx64"\e[0m\n", devEui, devEui);
        ret = Other;
    }
    mysql_free_result(result);

    if (ret != Success) {
        return ret;
    }

    /* have ID of this mote, check if we're authorized to give HNetID to asking NetID */
    ret = NoRoamingAgreement;   // default not authorized
    sprintf(query, "SELECT NetID FROM roamingNetIDs WHERE moteID = %lu", moteID);
    if (mysql_query(sql_conn_lora_join, query)) {
        printf("\e[31mSELECT roamingNetIDs %s\e[0m\n", mysql_error(sql_conn_lora_join));
        return ret;
    }
    result = mysql_use_result(sql_conn_lora_join);
    if (result) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result))) {
            uint32_t netid;
            sscanf(row[0], "%u", &netid);
            printf("allowed net id %06x vs asking %06x\n", netid, askingNetID);
            if (askingNetID == netid) {
                ret = Success;
                break;
            }
        }
        mysql_free_result(result);
    }

    if (ret == Success) {
        sprintf(query, "%06x", ans_netid);
        json_object_object_add(*ansJobj, HNetID, json_object_new_string(query));
    }

    return ret;
}

void
ParseJson(MYSQL* sc, const struct sockaddr *client_addr, json_object* inJobj, json_object** ansJobj)
{
    const char* pmt;
    char sender_id[64];
    unsigned long trans_id = 0;
    const char* ansMt = NULL;
    const char* rxResult;
    const char* Result = lib_parse_json(inJobj, &ansMt, &pmt, sender_id, myJoinEuiStr, &trans_id, &rxResult);

    if (strcmp(Result, Success) != 0) {
        printf("\e[31mJS ParseJson(): %s = lib_parse_json()\e[0m\n", Result);
        if (ansMt != NULL)
            goto Ans;
        else
            return;
    }
    Result = Other;

    if (JoinReq == pmt || RejoinReq == pmt) {
        uint32_t sender_id32;
        sscanf(sender_id, "%x", &sender_id32);
        Result = parse_rf_join_req(pmt, inJobj, sender_id32, ansJobj);
    } else if (HomeNSReq == pmt)
        Result = getHomeNS(inJobj, ansJobj, sender_id);
    else
        printf("\e[31mJS unknown mt %s\e[0m\n", pmt);

Ans:
    printf("JS ansMt '%s'\n", ansMt);
    if (*ansJobj) {
        lib_generate_json(*ansJobj, sender_id, myJoinEuiStr, trans_id, ansMt, Result);
    }
}

/**
 * Invalid URL page.
 */
#define NOT_FOUND_ERROR "<html><head><title>Not found</title></head><body>JS invalid-url</body></html>"

/**
 * Front page. (/)
 */
#define MAIN_PAGE "<html><head><title>Join Server</title></head><body><h3>Join Server %s</h3><a href=\"otaMotes\">OTA motes</a></body></html>"

/**
 * Second page. (/2)
 */
#define SECOND_PAGE "<html><head><title>Tell me more</title></head><body><a href=\"/\">previous</a> <form action=\"/S\" method=\"post\">%s, what is your job? <input type=\"text\" name=\"v2\" value=\"%s\" /><input type=\"submit\" value=\"Next\" /></body></html>"

/**
 * Second page (/S)
 */
#define SUBMIT_PAGE "<html><head><title>Ready to submit?</title></head><body><form action=\"/F\" method=\"post\"><a href=\"/2\">previous </a> <input type=\"hidden\" name=\"DONE\" value=\"yes\" /><input type=\"submit\" value=\"Submit\" /></body></html>"

/**
 * Last page.
 */
#define LAST_PAGE "<html><head><title>Thank you</title></head><body>Thank you.</body></html>"


/**
 * Handler that adds the 'v1' value to the given HTML code.
 *
 * @param cls unused
 * @param mime mime type to use
 * @param session session handle
 * @param connection connection to use
 */
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

    len = strlen (MAIN_PAGE) + strlen(myJoinEuiStr) + 1;
    reply = malloc (len);
    if (NULL == reply)
        return MHD_NO;
    snprintf (reply, len, MAIN_PAGE, myJoinEuiStr);
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

/**
 * Handler that returns a simple static HTTP page that
 * is passed in via 'cls'.
 *
 * @param cls a 'const char *' with the HTML webpage to return
 * @param mime mime type to use
 * @param session session handle
 * @param connection connection to use
 */
static int
serve_simple_form (const void *cls,
		   const char *mime,
		   struct Session *session,
		   struct MHD_Connection *connection)
{
    int ret;
    const char *form = cls;
    struct MHD_Response *response;

    /* return static form */
    response = MHD_create_response_from_buffer (strlen (form), (void *) form, MHD_RESPMEM_PERSISTENT);
    if (NULL == response)
        return MHD_NO;

    lib_add_session_cookie (session, response);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_ENCODING, mime);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}


const char otaMotesURL[] = "/otaMotes";
const char editURL[] = "/edit";
const char redirectURL[] = "/redir";
const char postFailURL[] = "/postFail";

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
ota_page_iterator (void *cls,
          uint64_t pos,
          char *buf,
          size_t max)
{
    uint32_t netid;
    int len = 0;
    char query[512];
    char netidStr[16];
    uint64_t eui64;
    struct Session *session = cls;
    appInfo_t* ai = session->appInfo;

    switch (ai->form_state) {
        case FORM_STATE_START:
            len = sprintf(buf, "<html><head><title>OTA Motes</title><style>%s</style></head><body><h3>Join Server %s</h3><form method=\"post\" action=\"%s\"><table class=\"motes\" border=\"1\">", mote_css, myJoinEuiStr, otaMotesURL);
            ai->form_state = FORM_STATE_TABLE_HEADER;
            break;
        case FORM_STATE_TABLE_HEADER:
            sprintf(query, "SELECT eui, HEX(NwkKey), HEX(AppKey), homeNetID FROM joinmotes");
            if (mysql_query(sql_conn_lora_join, query)) {
                len = sprintf(buf, "<tr>mysql_query() %s</tr>", mysql_error(sql_conn_lora_join));
                ai->form_state = FORM_STATE_FORM;
                break;
            }
            ai->result = mysql_use_result(sql_conn_lora_join);
            if (ai->result == NULL) {
                len = sprintf(buf, "<tr>no result</tr>");
                ai->form_state = FORM_STATE_FORM;
                break;
            }
            len = sprintf(buf, "<tr><th>DevEUI</th><th>1.0 and 1.1<br>NwkKey</th><th>1.1 only<br>AppKey</th><th>NetID for<br>HomeNSReq</th></tr>");
            ai->row = mysql_fetch_row(ai->result);
            if (ai->row)
                ai->form_state = FORM_STATE_TABLE_ROWS;
            else
                ai->form_state = FORM_STATE_FORM;
            break;
        case FORM_STATE_TABLE_ROWS:
            sscanf(ai->row[0], "%"PRIu64, &eui64);
            if (ai->row[3]) {
                sscanf(ai->row[3], "%u", &netid);
                sprintf(netidStr, "%06x", netid);
            } else
                netidStr[0] = 0;
            len = sprintf(buf, "<tr><td>%016"PRIx64"</td><td>%s</td><td>%s</td><td>%s</td><td><input type=\"submit\" name=\"del-%"PRIx64"\" value=\"delete\"></td><td><a href=\"%s/%"PRIx64"\">edit</a></td></tr>", eui64, ai->row[1], ai->row[2], netidStr, eui64, editURL, eui64);
            ai->row = mysql_fetch_row(ai->result);
            if (ai->row)
                ai->form_state = FORM_STATE_TABLE_ROWS;
            else
                ai->form_state = FORM_STATE_FORM;
            break;
        case FORM_STATE_FORM:
            if (ai->result)
                mysql_free_result(ai->result);
            len = sprintf(buf, "<tr><td><input type=\"text\" name=\"DevEUI\"/ size=\"16\"></td><td><input type=\"text\" size=\"32\" name=\"NwkKey\"/></td><td><input type=\"text\" size=\"32\" name=\"AppKey\"></td><td><input type=\"text\" size=\"6\" name=\"NetID\"/></td><td><INPUT TYPE=\"SUBMIT\" name=\"addbut\" VALUE=\"Add\" NAME=\"B1\"></td></tr></TABLE></FORM></body></html>");
            ai->form_state = FORM_STATE_END;
            break;
        case FORM_STATE_END:
            return MHD_CONTENT_READER_END_OF_STREAM;
    }

    return len;
}

static int
serve_ota_motes (const void *cls,
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
                                                &ota_page_iterator,
                                                session,
                                                NULL);
    lib_add_session_cookie (session, response);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}


static int
serve_edit(const void *cls,
	      const char *mime,
	      struct Session *session,
	      struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;
    char statusStr[512];
    char body[2048];
    char query[256];
    uint64_t eui64;
    unsigned long moteID;
    unsigned lifetime;

    sscanf(session->urlSubDir, "%"PRIx64, &eui64);

    strcpy(body, "<html><head><title>JS ");
    strcat(body, myJoinEuiStr);
    strcat(body, "</title></head><body><a href=\"");
    strcat(body, otaMotesURL);
    strcat(body, "\">back</a><h3>Join Server ");
    strcat(body, myJoinEuiStr);
    strcat(body, "<br>Edit mote ");
    strcat(body, session->urlSubDir);
    strcat(body, "</h3><form method=\"post\" action=\"");
    strcat(body, editURL);
    strcat(body, "\"><input type=\"hidden\" name=\"DevEUI\" value=\"");
    strcat(body, session->urlSubDir);
    strcat(body, "\">HomeNSAns value:<table border=\"1\">");

    strcpy(statusStr, "<br>");
    //sprintf(query, "SELECT homeNetID, ID, Lifetime FROM joinmotes WHERE eui = %"PRIu64, eui64);
    sprintf(query, "SELECT * FROM joinmotes WHERE eui = %"PRIu64, eui64);
    if (mysql_query(sql_conn_lora_join, query)) {
        strcat(body, "<tr>mysql_query() ");
        strcat(body, mysql_error(sql_conn_lora_join));
        strcat(body, "</tr>");
    } else {
        MYSQL_RES *result = mysql_use_result(sql_conn_lora_join);
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row) {
                strcat(body, "<tr><th>homeNetID:</th><td><input type=\"text\" name=\"NetID\" value=\"");
                if (row[7]) {
                    char str[16];
                    uint32_t netid;
                    sscanf(row[7], "%u", &netid);
                    sprintf(str, "%06x", netid);
                    strcat(body, str);
                }
                if (row[8])
                    sscanf(row[8], "%lu", &moteID);
                if (row[9])
                    sscanf(row[9], "%u", &lifetime);
                strcat(body, "\"></td></tr>");

                if (row[3]) {
                    strcat(statusStr, "last RJCount1: ");
                    strcat(statusStr, row[3]);
                    strcat(statusStr, "<br>");
                }
                if (row[4]) {
                    strcat(statusStr, "last DevNonce: ");
                    strcat(statusStr, row[4]);
                    strcat(statusStr, "<br>");
                }

            }
            strcat(statusStr, "</tr>");
            mysql_free_result(result);
        }
    }

    strcat(body, "</table><input type=\"submit\" value=\"");
    strcat(body, action_setHomeNetID);
    strcat(body, "\" name=\"action\"><br><br>");
    strcat(body, "Authorized networks to give HomeNSAns:<table border=\"1\">");
    strcat(body, "<tr><th>NetIDs to roam with</th></tr>");

    sprintf(query, "SELECT NetID FROM roamingNetIDs WHERE moteID = %lu", moteID);
    if (mysql_query(sql_conn_lora_join, query)) {
        strcat(body, "roamingNetIDs: ");
        strcat(body, mysql_error(sql_conn_lora_join));
    } else {
        MYSQL_RES *result = mysql_use_result(sql_conn_lora_join);
        if (result) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {
                uint32_t netid;
                char str[16];
                sscanf(row[0], "%u", &netid);
                sprintf(str, "%06x", netid);
                strcat(body, "<tr><td>");
                strcat(body, str);
                strcat(body, "</td><td><input type=\"submit\" name=\"");
                strcat(body, str);
                strcat(body, "\" value=\"");
                strcat(body, deleteNetID);
                strcat(body, "\"></td></tr>");
            }
        } else
            strcat(body, "roamingNetIDs: no result");
        mysql_free_result(result);
    }

    strcat(body, "<tr><td><input type=\"text\" name=\"roamNetID\"></td><td><input type=\"submit\" name=\"action\" value=\"");
    strcat(body, action_addRoamNetID);
    strcat(body, "\"</td></tr></table><br><table><tr><th>Lifetime:</th><td><input type=\"text\" name=\"Lifetime\" value=\"");
    sprintf(query, "%u", lifetime);
    strcat(body, query);
    strcat(body, "\"></td><td><input type=\"submit\" value=\"");
    strcat(body, action_updateLifetime);
    strcat(body, "\" name=\"action\"></td></tr>");
    strcat(body, "</table></form>");
    strcat(body, statusStr);
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
serve_redirect(const void *cls,
		const char *mime,
		struct Session *session,
		struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;
    appInfo_t* ai = session->appInfo;
    char body[1024];

    sprintf(body, "<html><head><title>JS %s</title><meta http-equiv=\"refresh\" content=\"0; url=.%s\" /></head><body></body></html>", myJoinEuiStr, ai->redir_url);
    response = MHD_create_response_from_buffer (strlen (body), body, MHD_RESPMEM_MUST_COPY);
    if (NULL == response)
        return MHD_NO;

    lib_add_session_cookie (session, response);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_ENCODING, mime);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}

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

/**
 * List of all pages served by this HTTP server.
 */
struct Page pages[] =
  { /* url, mime, handler, handler_cls */
    { "/", "text/html",  &main_page, NULL },
    { "/S", "text/html", &serve_simple_form, SUBMIT_PAGE },
    { "/F", "text/html", &serve_simple_form, LAST_PAGE },
    { otaMotesURL, "text/html", &serve_ota_motes, NULL},
    { editURL, "text/html", &serve_edit, NULL},
    { redirectURL, "text/html", &serve_redirect, NULL },
    { postFailURL, "text/html", &serve_post_fail, NULL },

    { NULL, NULL, &not_found_page, NULL } /* 404 */
  };


void* create_appInfo()
{
    return calloc(1, sizeof(appInfo_t));
}

void
browser_post_init(struct Session *session)
{
    //appInfo_t* ai = session->appInfo;
    /* unchecked checkbox items are not posted, must clear them here at start */ 
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
    appInfo_t* ai = session->appInfo;

    //printf("post_iterator() key %s ofs%"PRIu64"\n", key, off);
    if (0 == strcmp ("DevEUI", key))
    {
        sscanf(data, "%"PRIx64, &ai->DevEUI);
        return MHD_YES;
    }
    else if (0 == strcmp ("NwkKey", key))
    {
        strncpy(ai->NwkKeyStr, data, sizeof(ai->NwkKeyStr));
        return MHD_YES;
    }
    else if (0 == strcmp ("AppKey", key))
    {
        strncpy(ai->AppKeyStr, data, sizeof(ai->AppKeyStr));
        return MHD_YES;
    } else if (0 == strcmp ("action", key)) {
        if (strcmp(data, action_setHomeNetID) == 0)
            ai->op = OP_SET_HOME_NETID;
        else if (strcmp(data, action_addRoamNetID) == 0)
            ai->op = OP_ADD_ROAM_NETID;
        else if (strcmp(data, action_updateLifetime) == 0)
            ai->op = OP_LIFETIME;
        else
            printf("\e[31munknown action \"%s\"\e[0m\n", data);
        return MHD_YES;
    } else if (0 == strcmp ("Lifetime", key)) {
        sscanf(data, "%u", &ai->Lifetime);
        return MHD_YES;
    } else if (0 == strcmp ("addbut", key)) {
        ai->op = OP_ADD;
        return MHD_YES;
    } else if (0 == strncmp ("del-", key, 4)) {
        sscanf(key+4, "%"PRIx64, &ai->DevEUI);
        ai->op = OP_DEL;
        return MHD_YES;
    } else if (0 == strcmp ("NetID", key)) {
        if (strlen(data) == 0)
            ai->homeNetID = NONE_NETID;
        else
            sscanf(data, "%x", &ai->homeNetID);
        printf("post_iterator home net id %06x, %s\n", ai->homeNetID, data);
    } else if (0 == strcmp("roamNetID", key)) {
        if (strlen(data) == 0)
            ai->roamNetID = NONE_NETID;
        else
            sscanf(data, "%x", &ai->roamNetID);
        printf("post_iterator roam net id %06x, %s\n", ai->roamNetID, data);
        return MHD_YES;
    } else if (0 == strcmp(deleteNetID, data)) {
        sscanf(key, "%x", &ai->roamNetID);
        ai->op = OP_DEL_ROAM_NETID;
        return MHD_YES;
    }
    fprintf (stderr, "\e[31mUnsupported form value `%s'\e[0m\n", key);
    return MHD_YES;
}

#define DEFAULT_LIFETIME        3600

static void
mote_ota_add(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    unsigned i;
    char buf[32];
    char query[512];
    uint8_t NwkKey_bin[LORA_CYPHERKEYBYTES];
    char* strPtr = ai->NwkKeyStr;  // pointer to start of NwkKey string

    for (i = 0; i < LORA_CYPHERKEYBYTES; i++) {
        unsigned octet;
        sscanf(strPtr, "%02x", &octet);
        NwkKey_bin[i] = octet;
        strPtr += 2;
    }

    if (strlen(ai->AppKeyStr) == 0) {
        strcpy(query, "REPLACE INTO joinmotes (Lifetime, eui, NwkKey) VALUES (");
        sprintf(buf, "%u", DEFAULT_LIFETIME);
        strcat(query, buf);
        strcat(query, ", ");
        sprintf(buf, "%"PRIu64, ai->DevEUI);
        strcat(query, buf);
        strcat(query, ", 0x");
        for (i = 0; i < LORA_CYPHERKEYBYTES; i++) {
            sprintf(buf, "%02x", NwkKey_bin[i]);
            strcat(query, buf);
        }
        strcat(query, ")");
    } else {
        uint8_t* airDevEui = (uint8_t*)&ai->DevEUI;
        uint8_t JSIntKey_bin[LORA_CYPHERKEYBYTES];
        uint8_t JSEncKey_bin[LORA_CYPHERKEYBYTES];
        uint8_t AppKey_bin[LORA_CYPHERKEYBYTES];

        strPtr = ai->AppKeyStr;
        for (i = 0; i < LORA_CYPHERKEYBYTES; i++) {
            unsigned octet;
            sscanf(strPtr, "%02x", &octet);
            AppKey_bin[i] = octet;
            strPtr += 2;
        }

        GenerateJoinKey(0x05, NwkKey_bin, airDevEui, JSEncKey_bin);
        GenerateJoinKey(0x06, NwkKey_bin, airDevEui, JSIntKey_bin);

        strcpy(query, "REPLACE INTO joinmotes (Lifetime, eui, NwkKey, AppKey, JSIntKey, JSEncKey) VALUES (");
        sprintf(buf, "%u", DEFAULT_LIFETIME);
        strcat(query, buf);
        strcat(query, ", ");
        sprintf(buf, "%"PRIu64, ai->DevEUI);
        strcat(query, buf);
        strcat(query, ", 0x");
        for (i = 0; i < LORA_CYPHERKEYBYTES; i++) {
            sprintf(buf, "%02x", NwkKey_bin[i]);
            strcat(query, buf);
        }
        strcat(query, ", 0x");
        for (i = 0; i < LORA_CYPHERKEYBYTES; i++) {
            sprintf(buf, "%02x", AppKey_bin[i]);
            strcat(query, buf);
        }
        strcat(query, ", 0x");
        for (i = 0; i < LORA_CYPHERKEYBYTES; i++) {
            sprintf(buf, "%02x", JSIntKey_bin[i]);
            strcat(query, buf);
        }
        strcat(query, ", 0x");
        for (i = 0; i < LORA_CYPHERKEYBYTES; i++) {
            sprintf(buf, "%02x", JSEncKey_bin[i]);
            strcat(query, buf);
        }
        strcat(query, ")");
    }

    printf("%s\n", query);
    if (mysql_query(sql_conn_lora_join, query)) {
        sprintf(failMsg, "\e[31mJS (ota add) Error querying server: %s\e[0m\n", mysql_error(sql_conn_lora_join));
        return;
    }
    printf("js joinmotes add mysql_affected_rows:%u\n", (unsigned int)mysql_affected_rows(sql_conn_lora_join));

}

static void
delMote(uint64_t DevEUI, char* failMsg)
{
    char query[512];

    sprintf(query, "DELETE FROM joinmotes WHERE eui = %"PRIu64, DevEUI);
    SQL_PRINTF("js %s\n", query);
    if (mysql_query(sql_conn_lora_join, query)) {
        sprintf(failMsg, "joinmotes mysql_query() %s", mysql_error(sql_conn_lora_join));
        return;
    }
    printf("js delete from joinmotes mysql_affected_rows:%u\n", (unsigned int)mysql_affected_rows(sql_conn_lora_join));

    sprintf(query, "DELETE FROM nonces WHERE mote = %"PRIu64, DevEUI);
    SQL_PRINTF("js %s\n", query);
    if (mysql_query(sql_conn_lora_join, query)) {
        sprintf(failMsg, "nonces mysql_query() %s", mysql_error(sql_conn_lora_join));
        return;
    }
    printf("js delete from nonces mysql_affected_rows:%u\n", (unsigned int)mysql_affected_rows(sql_conn_lora_join));
}

static int 
otaMotesSubmitted(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    int i;

    printf("otaMotesSubmitted() DevEui:%"PRIx64", NwkKey:%s, AppKey:%s ", ai->DevEUI, ai->NwkKeyStr, ai->AppKeyStr);
    if (ai->op == OP_DEL) {
        printf("OP_DEL\n");
        delMote(ai->DevEUI, failMsg);
    } else if (ai->op == OP_ADD) {
        printf("OP_ADD\n");

        if (strlen(ai->NwkKeyStr) < 32) {
            sprintf(failMsg, "NwkKey less than 32 chars");
        } else {
            for (i = 0; i < 32; i++) {
                char ch = ai->NwkKeyStr[i];
                if (ch >= 'a' && ch <= 'f')
                    continue;
                if (ch >= 'A' && ch <= 'F')
                    continue;
                if (ch >= '0' && ch <= '9')
                    continue;
                snprintf(failMsg, sizeof_failMsg, "NwkKey: bad hex char at %d", i);
                break;
            }
        }

        if (strlen(ai->AppKeyStr) > 0) {
            if (strlen(ai->AppKeyStr) < 32) {
                snprintf(failMsg, sizeof_failMsg, "AppKey less than 32 chars");
            }
        }

        if (failMsg[0] == 0)
            mote_ota_add(ai, failMsg, sizeof_failMsg);
    }

    if (failMsg[0] == 0)
        return 0;
    else
        return -1;
} // ..otaMotesSubmitted()

unsigned long
getMoteID(uint64_t devEui)
{
    char query[512];
    unsigned long ret = ULONG_MAX;

    sprintf(query, "SELECT ID FROM joinmotes WHERE eui = %"PRIu64, devEui);
    if (mysql_query(sql_conn_lora_join, query)) {
        printf("\e[31mjoinmotes mysql_query() %s\e[0m\n", mysql_error(sql_conn_lora_join));
        return ret;
    }

    MYSQL_RES *result = mysql_use_result(sql_conn_lora_join);
    if (!result) {
        printf("\e[31mjoinmotes SELECT ID no result\e[0m\n");
        return ret;
    }
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row)
        sscanf(row[0], "%lu", &ret);
    else
        printf("\e[31mjoinmotes SELECT ID no row\e[0m\n");

    mysql_free_result(result);

    return ret;
}

int
updateLifetime(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    char query[512];
    sprintf(query, "UPDATE joinmotes SET Lifetime = %u WHERE eui = %"PRIu64, ai->Lifetime, ai->DevEUI);
    if (mysql_query(sql_conn_lora_join, query)) {
        snprintf(failMsg, sizeof_failMsg, "joinmotes mysql_query() %s", mysql_error(sql_conn_lora_join));
        return -1;
    }
    if (mysql_affected_rows(sql_conn_lora_join) < 1) {
        snprintf(failMsg, sizeof_failMsg, "set lifetime: nothing updated");
        return -1;
    }
    return 0;
}

int
delRoamableNetID(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    char query[512];
    unsigned long moteID = getMoteID(ai->DevEUI);

    if (moteID == ULONG_MAX) {
        snprintf(failMsg, sizeof_failMsg, "getMoteID(%"PRIx64") failed", ai->DevEUI);
        return -1;
    }

    sprintf(query, "DELETE FROM roamingNetIDs WHERE moteID = %lu AND NetID = %u", moteID, ai->roamNetID);
    if (mysql_query(sql_conn_lora_join, query)) {
        snprintf(failMsg, sizeof_failMsg, "DELETE roamingNetIDs %s", mysql_error(sql_conn_lora_join));
        return -1;
    }
    moteID = mysql_affected_rows(sql_conn_lora_join);
    if (moteID != 1) {
        snprintf(failMsg, sizeof_failMsg, "DELETE roamingNetIDs affected rows %lu", moteID);
        return -1;
    }

    return 0;
}

int
addRoamableNetID(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    char query[512];
    unsigned long moteID = getMoteID(ai->DevEUI);

    if (moteID == ULONG_MAX) {
        snprintf(failMsg, sizeof_failMsg, "getMoteID(%"PRIx64") failed", ai->DevEUI);
        return -1;
    }

    sprintf(query, "INSERT INTO roamingNetIDs (moteID, NetID) VALUE (%lu, %u)", moteID, ai->roamNetID);
    if (mysql_query(sql_conn_lora_join, query)) {
        snprintf(failMsg, sizeof_failMsg, "INSERT roamingNetIDs %s", mysql_error(sql_conn_lora_join));
        return -1;
    }
    moteID = mysql_affected_rows(sql_conn_lora_join);
    if (moteID != 1) {
        snprintf(failMsg, sizeof_failMsg, "INSERT roamingNetIDs affected rows %lu", moteID);
        return -1;
    }

    return 0;
}

int
updateHomeNetID(appInfo_t* ai, char* failMsg, size_t sizeof_failMsg)
{
    char query[512];
    char valStr[16];

    printf("updateHomeNetID() ai->homeNetID:%06x\n", ai->homeNetID);
    if (ai->homeNetID == NONE_NETID)
        strcpy(valStr, "NULL");
    else
        sprintf(valStr, "%u", ai->homeNetID);

    sprintf(query, "UPDATE joinmotes SET homeNetID = %s WHERE eui = %"PRIu64, valStr, ai->DevEUI);
    if (mysql_query(sql_conn_lora_join, query)) {
        snprintf(failMsg, sizeof_failMsg, "joinmotes mysql_query() %s", mysql_error(sql_conn_lora_join));
        return -1;
    }
    if (mysql_affected_rows(sql_conn_lora_join) < 1) {
        snprintf(failMsg, sizeof_failMsg, "set netid: nothing updated<br>%s", query);
        return -1;
    }
    return 0;
}

void
browser_post_submitted(const char* url, struct Request* request)
{
    char failMsg[1024];
    struct Session *session = request->session;
    appInfo_t* ai = session->appInfo;

    failMsg[0] = 0;
    printf("browser_post_submitted(\"%s\",) ", url);

    if (strcmp(url, otaMotesURL) == 0) {
        if (otaMotesSubmitted(ai, failMsg, sizeof(failMsg)) == 0)
            strcpy(ai->redir_url, otaMotesURL);
    } else if (strcmp(url, editURL) == 0) {
        if (ai->op == OP_SET_HOME_NETID) {
            if (updateHomeNetID(ai, failMsg, sizeof(failMsg)) == 0)
                strcpy(ai->redir_url, otaMotesURL);
        } else if (ai->op == OP_ADD_ROAM_NETID) {
            if (addRoamableNetID(ai, failMsg, sizeof(failMsg)) == 0)
                strcpy(ai->redir_url, editURL);
        } else if (ai->op == OP_DEL_ROAM_NETID) {
            if (delRoamableNetID(ai, failMsg, sizeof(failMsg)) == 0)
                strcpy(ai->redir_url, editURL);
        } else if (ai->op == OP_LIFETIME) {
            if (updateLifetime(ai, failMsg, sizeof(failMsg)) == 0)
                strcpy(ai->redir_url, editURL);
        } else
            snprintf(failMsg, sizeof(failMsg), "%s unknown op %u", url, ai->op);
    } else
        snprintf(failMsg, sizeof(failMsg), "browser_post_submitted unknown url %s", url);

    if (strlen(failMsg) > 0) {
        printf("\e[31m%s\e[0m\n", failMsg);
        snprintf(postFailPage, sizeof(postFailPage), "<html><head><title>Failed</title></head><body>%s</body></html>", failMsg);
        strcpy(ai->redir_url, postFailURL);
    }

    request->post_url = redirectURL;
    printf("\n");
}


