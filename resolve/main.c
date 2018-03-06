/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "libserver.h"

struct Page pages[] = {
    { NULL, NULL, NULL, NULL } /* 404 */
};

struct Session *sessions;

int sessionCreate(struct Session* s) { return 0; }
void sessionEnd(struct Session* s) { }

void
browser_post_init(struct Session *session)
{
}

void
browser_post_submitted(const char* url, struct Request* request)
{
}

void
ParseJson(MYSQL* sc, const struct sockaddr *client_addr, json_object* inJobj, json_object** ansJobj)
{
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
    return MHD_NO;
}

void* create_appInfo()
{
    return NULL;
}

int
main (int argc, char **argv)
{
    char hostname[64];
    uint64_t rxJoinEui = 0x647fda800000012e;

    srand(time(NULL));

    resolve_post(NULL, "00050a.netids.example.com", true);

    getJsHostName(rxJoinEui, hostname, "joineuis.example.com");
    printf("hostname: %s\n", hostname);
    resolve_post(NULL, hostname, true);
}

