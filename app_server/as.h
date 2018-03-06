/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "libserver.h"
#include <mqueue.h>

typedef enum {
    MAIN_OP_NONE = 0,
    MAIN_OP_GETKEY,
} mainop_e;

typedef struct {
    uint64_t devEui;
    uint32_t devAddr;
    uint8_t fport;
    bool conf;

    uint8_t* payload;
    uint8_t payLen;
} msg_t;

struct _tlist {
    msg_t msg;
    uint32_t tid;
    struct _tlist* next;
};

extern struct _tlist* tlist;

extern const char Cayenne[];
extern char myAS_ID[];
extern key_envelope_t key_envelope_app;
extern uint32_t next_req_tid;
extern char netIdDomain[];
extern volatile mainop_e mainOp;

int send_downlink(MYSQL* sc, char* failMsg, size_t sizeof_failMsg, msg_t* msg);
const char* app_uplink(MYSQL* sc, const char* frmStr, const ULMetaData_t* ulmd, const uint8_t* AppSKeyBin);

int writeDownlinkStatus(MYSQL* sc, uint64_t devEui, uint32_t devAddr, const char* text);
void tlistAdd(const msg_t* msg, uint32_t reqSentTid);

#define MSGSIZE     768
extern mqd_t mqwd;
int child(const char* mqName, const char* dbhostname, const char* dbuser, const char* dbpass, uint16_t dbport, const char *dbName);

extern CURLM *multi_handle;

/* from base64.c: */
unsigned char * base64_encode(const unsigned char *src, size_t len, size_t *out_len);

