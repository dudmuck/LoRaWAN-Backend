/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "libserver.h"

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#define MAX_DNS_STRING 255
#define MAX_DNS_NAME 256
struct naptr_rdata {
    unsigned short order;
    unsigned short pref;
    unsigned int flags_len;
    char flags[MAX_DNS_STRING];
    unsigned int services_len;
    char services[MAX_DNS_STRING];
    unsigned int regexp_len;
    char regexp[MAX_DNS_STRING];
    unsigned int repl_len; /* not currently used */
    char repl[MAX_DNS_NAME];
};

struct _result_list {
    unsigned short class;
    unsigned int ttl;
    void* vp;
    struct _result_list* next;
};

struct srv_rdata {
    unsigned short priority;
    unsigned short weight;
    unsigned short running_sum;
    unsigned short port;
    unsigned int name_len;
    char name[MAX_DNS_NAME];
};

static void*
get_srv(struct _result_list* rl)
{
    struct _result_list* myrl;
    unsigned lowestPri = UINT_MAX;
    unsigned lowestWeight = UINT_MAX;
    unsigned nlowestWeight = 0;
    unsigned i, use;

    for (myrl = rl; myrl; myrl = myrl->next) {
        struct srv_rdata* s;
        if (!myrl->vp)
            continue;
        s = myrl->vp;
        if (s->priority < lowestPri)
            lowestPri = s->priority;
    }
    if (lowestPri == UINT_MAX)
        return NULL;

    for (myrl = rl; myrl; myrl = myrl->next) {
        struct srv_rdata* s;
        if (!myrl->vp)
            continue;
        s = myrl->vp;
        if (s->priority > lowestPri) {
            continue;
        }
        if (s->weight <= lowestWeight) {
            lowestWeight = s->weight;
            nlowestWeight++;
        }
    }
    if (lowestWeight == UINT_MAX)
        return NULL;

    use = rand() % nlowestWeight;

    i = 0;
    for (myrl = rl; myrl; myrl = myrl->next) {
        struct srv_rdata* s;
        if (!myrl->vp)
            continue;
        s = myrl->vp;
        if (s->priority > lowestPri || s->weight > lowestWeight)
            continue;
        if (i == use) {
            return &myrl->vp;
        }
        i++;
    }

    return NULL;
}

static void*
get_naptr(struct _result_list* rl)
{
    struct _result_list* myrl;
    unsigned lowestOrder = UINT_MAX;
    unsigned lowestPref = UINT_MAX;
    unsigned nlowestPref = 0;
    unsigned i, use;

    for (myrl = rl; myrl; myrl = myrl->next) {
        struct naptr_rdata* n;
        if (!myrl->vp)
            continue;
        n = myrl->vp;
        if (n->order < lowestOrder)
            lowestOrder = n->order;
    }
    if (lowestOrder == UINT_MAX)
        return NULL;

    for (myrl = rl; myrl; myrl = myrl->next) {
        struct naptr_rdata* n;
        if (!myrl->vp)
            continue;
        n = myrl->vp;
        if (n->order > lowestOrder) {
            continue;
        }
        if (n->pref <= lowestPref) {
            lowestPref = n->pref;
            nlowestPref++;
        }
    }
    if (lowestPref == UINT_MAX)
        return NULL;

    use = rand() % nlowestPref;

    i = 0;
    for (myrl = rl; myrl; myrl = myrl->next) {
        struct naptr_rdata* n;
        if (!myrl->vp)
            continue;
        n = myrl->vp;
        if (n->order > lowestOrder || n->pref > lowestPref)
            continue;
        if (i == use) {
            return &myrl->vp;
        }
        i++;
    }

    return NULL;
}

static int
dns_srv_parser( unsigned char* msg, unsigned char* end,
								  unsigned char* rdata, struct srv_rdata* out)
{
	int len;

	if ((rdata+6) >= end)
        return -1;

	memcpy(&out->priority, rdata, 2);
	out->priority = ntohs(out->priority);

	memcpy(&out->weight,   rdata+2, 2);
	out->weight = ntohs(out->weight);

	memcpy(&out->port,     rdata+4, 2);
	out->port = ntohs(out->port);

	rdata += 6;

	if ((len = dn_expand(msg, end, rdata, out->name, MAX_DNS_NAME-1)) == -1) {
        return -1;
    }
	/* add terminating 0 ? (warning: len=compressed name len) */
	out->name_len = strlen(out->name);

    return 0;
}

static int dns_naptr_parser( unsigned char* msg, unsigned char* end,
								  unsigned char* rdata, struct naptr_rdata* out)
{
	if ((rdata + 7) >= end)
        return -1;

	memcpy(&out->order, rdata, 2);
	out->order = ntohs(out->order);

	memcpy(&out->pref, rdata + 2, 2);
	out->pref = ntohs(out->pref);

	out->flags_len = (int)rdata[4];
	if ((rdata + 7 +  out->flags_len) >= end)
        return -1;
	memcpy(&out->flags, rdata + 5, out->flags_len);
    out->flags[out->flags_len] = 0; // null terminate

	out->services_len = (int)rdata[5 + out->flags_len];
	if ((rdata + 7 + out->flags_len + out->services_len) >= end)
        return -1;
	memcpy(&out->services, rdata + 6 + out->flags_len, out->services_len);
    out->services[out->services_len] = 0;   // null terminate

	out->regexp_len = (int)rdata[6 + out->flags_len + out->services_len];
	if ((rdata + 7 + out->flags_len + out->services_len + out->regexp_len) >= end)
        return -1;

	memcpy(&out->regexp, rdata + 7 + out->flags_len + out->services_len, out->regexp_len);
    out->regexp[out->regexp_len] = 0;   // null terminate

	rdata = rdata + 7 + out->flags_len + out->services_len + out->regexp_len;
	out->repl_len = dn_expand(msg, end, rdata, out->repl, MAX_DNS_NAME-1);

	if (out->repl_len == (unsigned int)-1)
        return -1;

	/* add terminating 0 ? (warning: len=compressed name len) */

	return 0;
}

static unsigned char* skipDnsName(unsigned char* p, unsigned char* end)
{
    while (p < end) {
        /* check if \0 (root label length) */
        if (*p == 0) {
            p++;
            break;
        }
        /* check if we found a pointer */
        if ((*p & 0xc0) == 0xc0 ) {
            /* if pointer skip over it (2 bytes) & we found the end */
            p += 2;
            break;
        }
        /* normal label */
        p += *p + 1; 
    }
    return (p >= end) ? 0 : p;
}

#define MAX_QUERY_SIZE      8192
union dns_query {
    HEADER hdr;
    unsigned char buff[MAX_QUERY_SIZE];
};

typedef struct {
    uint16_t rtype;
    uint16_t class;
    uint32_t ttl;
    uint16_t rdlength;
} __attribute__((packed)) answer_md_t;

#define DNS_HDR_SIZE        12
static void*
getRecord(const char* name, int type)
{
    int ansCnt, r, qdCnt, size;
    union dns_query dq;
    unsigned char *p, *end;
    struct _result_list* rlStart = NULL;
    struct _result_list* rl = NULL;

    size = res_search(name, C_IN, type, dq.buff, sizeof(dq));
    if (size < 0) {
        return NULL;
    }

    p = dq.buff + DNS_HDR_SIZE;
    end = dq.buff + size;
    if (p >= end)
        return NULL;
    qdCnt = ntohs((unsigned short)dq.hdr.qdcount);

    for (r = 0; r < qdCnt; r++){
        /* skip the name of the question */
        if ((p = skipDnsName(p, end)) == 0) {
            printf("skipname == 0\n");
            return NULL;
        }
        p += 2+2; /* skip QCODE & QCLASS */
        if (p >= end) {
            printf("p >= end\n");
            return NULL;
        }
    };
    ansCnt = ntohs((unsigned short)dq.hdr.ancount);

    for (r=0; (r<ansCnt) && (p<end); r++) {
        answer_md_t a, *srcPtr;
	    struct naptr_rdata naptr_rd;
        struct srv_rdata srv_rd;

        if ((p = skipDnsName(p, end)) == 0) {
            printf("skipDnsName = 0 (B)\n");
            return NULL;
        }
        if (p+sizeof(answer_md_t) >= end)
            return NULL;
        srcPtr = (answer_md_t*)p;

        a.rtype = ntohs(srcPtr->rtype);
        a.class = ntohs(srcPtr->class);
        a.ttl = ntohl(srcPtr->ttl);
        a.rdlength = ntohs(srcPtr->rdlength);

        p += sizeof(answer_md_t);

        if (rlStart) {
            rl->next = calloc(1, sizeof(struct _result_list));
            rl = rl->next;
        } else {
            /* first */
            rlStart = calloc(1, sizeof(struct _result_list));
            rl = rlStart;
        }
        rl->ttl = a.ttl;
        rl->class = a.class;

        switch (a.rtype) {
            case T_SRV:
                if (dns_srv_parser(dq.buff, end, p, &srv_rd) < 0)
                    break;
                rl->vp = malloc(sizeof(struct srv_rdata));
                memcpy(rl->vp, &srv_rd, sizeof(struct srv_rdata));
                break;
			case T_NAPTR:
				if (dns_naptr_parser(dq.buff, end, p, &naptr_rd) < 0)
                    break;
                rl->vp = malloc(sizeof(struct naptr_rdata));
                memcpy(rl->vp, &naptr_rd, sizeof(struct naptr_rdata));
                break;
            default:
                printf("unknown type %d\n", a.rtype);
                break;
        }

		p += a.rdlength;

    } // ..for()

    return rlStart;
}

/* return: 0 for not found locally, 1 for found and posted */
static int
local_lookup_post(CURL* curl, const char* hostname)
{
    struct _host_list* hl;

    for (hl = host_list; hl != NULL; hl = hl->next) {
        char url[296];
        printf("local_lookup_post %s ::: %s port %u\n", hostname, hl->name, hl->port);
        if (strcmp(hostname, hl->name) == 0) {
            sprintf(url, "http://%s:%u", hl->postTo, hl->port);
            printf("local found, posting to %s\n", url);
            curl_easy_setopt(curl, CURLOPT_URL, url);
            return 1;
        }
    }

    return 0;
}

static volatile bool initialized = false;

int resolve_post(CURL* curl, const char* hostname, bool verbose)
{
    int ret = -1;
    char url[296];
    struct _result_list* myrl;
    struct _result_list* nl = NULL;
    void* vp;

    if (!initialized) {
        if (res_init()) {
            printf("\e[31mres_init()\e[0m\n");
            return ret;
        }
        initialized = true;
    }

    if (local_lookup_post(curl, hostname)) {
        return 0;
    }

    nl = getRecord(hostname, T_NAPTR);
    if (!nl) {
        printf("\e[31mresolve_post() NULL = getRecord(%s)\e[0m\n", hostname);
        return ret;
    }

    do {
        vp = get_naptr(nl);
        if (vp) {
            struct naptr_rdata** np = vp;
            struct naptr_rdata* n = *np;
            if (verbose)
                printf("NAPTR %u %u '%s' '%s' '%s'\n", n->order, n->pref, n->flags, n->services, n->repl);
            if (n->flags[0] == 'S') {
                struct _result_list* sl = getRecord(n->repl, T_SRV);
                void* svp;
                if (sl) {
                    do {
                        svp = get_srv(sl);
                        if (svp) {
                            struct srv_rdata** sp = svp;
                            struct srv_rdata* s = *sp;
                            if (verbose)
                                printf("SRV %u %u %u %s\n", s->priority, s->weight, s->port, s->name);
                            if (curl) {
                                sprintf(url, "http://%s:%u", s->name, s->port);
                                if (verbose)
                                    printf(" >>>>>>>POST url %s\n", url);
                                curl_easy_setopt(curl, CURLOPT_URL, url);
                                ret = 0;

                                free(*sp);  // only trying this one once
                                *sp = NULL;
                            } else {
                                printf("resolve-no-curl\n ");
                                break;
                            }
                        }
                    } while (svp != NULL);

                    for (myrl = sl; myrl; ) {
                        struct _result_list* next = myrl->next;
                        if (myrl->vp) {
                            free(myrl->vp);
                        }
                        free(myrl);
                        myrl = next;
                    }
                    if (ret == 0)
                        break;
                }
            } else if (n->flags[0] == 'U') {
                /* n->repl is URI */
                if (curl) {
                    curl_easy_setopt(curl, CURLOPT_URL, n->repl);
                    if (verbose)
                        printf("use as URI %s\n", n->repl);
                    ret = 0;
                }
            } else if (n->flags[0] == 'A') {
                // TODO where is port number?
                printf("\e[31mTODO A record\e[0m\n");
            } else if (n->flags[0] == 'P') {
                if (resolve_post(curl, n->repl, verbose) == 0)
                    break;
            } else
                printf("TODO service %s\n", n->flags);

            free(*np);  // only trying this one once
            *np = NULL;
        } /*else
            printf("NULL = get_naptr()\n");*/
    } while (vp != NULL);


    for (myrl = nl; myrl; ) {
        struct _result_list* next = myrl->next;
        if (myrl->vp) {
            free(myrl->vp);
        }
        free(myrl);
        myrl = next;
    }

    return ret;
}

