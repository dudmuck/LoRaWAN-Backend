/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "ns.h"
#include <math.h>

/* fNS latency would be added to sNS latency */
#define REQUEST_WAITING_SECONDS_LOCAL     0.2
#define REQUEST_WAITING_SECONDS_JSON      0.4

typedef struct {
    uint8_t discard : 1;
    uint8_t skip_next : 1;
} flags_t;

uint8_t nwkAddrBits;
uint32_t devAddrBase;

/* sql enum roamState */
const char roamNONE[] = "NONE";
const char roamfPASSIVE[] = "fPASSIVE";
const char roamDEFERRED[] = "DEFERRED";
const char roamsPASSIVE[] = "sPASSIVE";
const char roamsHANDOVER[] = "sHANDOVER";
const char roamhHANDOVER[] = "hHANDOVER";

void
printElapsed(const mote_t* mote)
{
    double s;
    struct timespec now;

    if (clock_gettime (CLOCK_REALTIME, &now) == -1)
        perror ("clock_gettime");

    s = difftimespec(now, mote->read_host_time);
    printf("\n\e[0;40;33m%016"PRIx64" / %08x  %.3f\e[0m ", mote->devEui, mote->devAddr, s);
}

uint32_t
LoRa_GenerateDataFrameIntegrityCode(const block_t* block, const uint8_t* sNwkSIntKey, const uint8_t* pktPayload)
{
    uint8_t temp[LORA_AUTHENTICATIONBLOCKBYTES];
    uint32_t mic;
    AES_CMAC_CTX cmacctx;

    AES_CMAC_Init(&cmacctx);
    AES_CMAC_SetKey(&cmacctx, sNwkSIntKey);
    if (block->b.dir == DIR_UP) {
        DEBUG_MIC_BUF_UP(sNwkSIntKey, 16, "b0-up-MIC-key");
        DEBUG_MIC_BUF_UP(block->octets, 16, "b0-MIC");
    } else if (block->b.dir == DIR_DOWN) {
        DEBUG_MIC_BUF_DOWN(sNwkSIntKey, 16, "b0-dwn-MIC-key");
        DEBUG_MIC_BUF_DOWN(block->octets, 16, "b0-MIC");
    }

    AES_CMAC_Update(&cmacctx, block->octets, LORA_AUTHENTICATIONBLOCKBYTES);
    AES_CMAC_Update(&cmacctx, pktPayload, block->b.lenMsg);
    AES_CMAC_Final(temp, &cmacctx);
    memcpy(&mic, temp, LORA_FRAMEMICBYTES);
    return mic;
}

static int
create_mote(MYSQL* sc, mote_t **m_pptr, uint64_t devEui, uint32_t devAddr)
{
    mote_t* mote;

#ifdef RF_DEBUG
    printf("\e[35;1m create mote %016"PRIx64" / %08x ", devEui, devAddr);
#endif

    *m_pptr = malloc(sizeof(mote_t));
    if (!*m_pptr)
        return -1;
    memset(*m_pptr, 0, sizeof(mote_t));
    mote = *m_pptr;

    mote->new = true;
    mote->best_sq = INT_MIN;

    mote->devEui = devEui;

    if (devEui == NONE_DEVEUI) {
        char query[512];
        const char* res;
        my_ulonglong id = getMoteID(sc, NONE_DEVEUI, devAddr, &res);
#ifdef RF_DEBUG
        printf(" search-id%llu-on-%08x(%u) ", id, devAddr, devAddr);
#endif
        sprintf(query, "SELECT DevEUI FROM motes WHERE ID = %llu", id);
        if (mysql_query(sc, query)) {
            printf("\n%s\n", query);
            printf("\e[31mcreate %s --- %s\e[0m\n", query, mysql_error(sc));
        } else {
            MYSQL_RES *result;
            result = mysql_use_result(sc);
            if (result) {
                MYSQL_ROW row;
                row = mysql_fetch_row(result);
                if (row && row[0]) {
                    sscanf(row[0], "%"PRIu64, &mote->devEui);
                    printf("create_mote->%016"PRIx64, mote->devEui);
                }
                mysql_free_result(result);
            }
        }
    }

    mote->devAddr = devAddr;

#ifdef RF_DEBUG
    printf("\e[0m\n");
#endif
    return 0;
}

static int 
create_mote_on_list(MYSQL* sc, struct _mote_list** ml_ptr, uint64_t devEui, uint32_t devAddr)
{
    struct _mote_list* ml;

    *ml_ptr = malloc(sizeof(struct _mote_list));
    memset(*ml_ptr, 0, sizeof(struct _mote_list));

    ml = *ml_ptr;

    return create_mote(sc, &ml->motePtr, devEui, devAddr);
}

mote_t*
getMote(MYSQL* sc, struct _mote_list **moteList, uint64_t devEui, uint32_t devAddr)
{
    struct _mote_list* My_mote_list = NULL;

    if (devEui == NONE_DEVEUI) {
        /* see if there is a session for this devAddr, could be associated with a DevEUI */
        char query[512];
        const char* res;
        my_ulonglong id = getMoteID(sc, NONE_DEVEUI, devAddr, &res);

        if (id > 0) {
            sprintf(query, "SELECT DevEUI FROM motes WHERE ID = %llu", id);
            if (mysql_query(sc, query)) {
                printf("\n%s\n", query);
                printf("\e[31mgetMote() %s --- (%u) %s\e[0m\n", query, mysql_errno(sc), mysql_error(sc));
            } else {
                MYSQL_RES *result;
                result = mysql_use_result(sc);
                if (result) {
                    MYSQL_ROW row;
                    row = mysql_fetch_row(result);
                    if (row && row[0]) {
                        sscanf(row[0], "%"PRIu64, &devEui);
                        //printf("getMote->%016"PRIx64, devEui);
                    }
                    mysql_free_result(result);
                }
            }
        } // else no session for this mote (perhaps passive-fNS), search mote list
    }

    if (*moteList == NULL) {
        /* first time */
        if (create_mote_on_list(sc, moteList, devEui, devAddr) < 0)
            return NULL;
        My_mote_list = *moteList;
    } else {
        /* find this mote in our mote list */
        struct _mote_list* ml_empty = NULL;
        My_mote_list = *moteList;
        while (1) {
            if (My_mote_list->motePtr == NULL) {
                if (ml_empty == NULL)
                    ml_empty = My_mote_list;
            } else {
#if 0
                DevAddr changes when session changes
                if (devEui != NONE_DEVEUI && devAddr != NONE_DEVADDR) {
                    if (devEui == My_mote_list->motePtr->devEui && devAddr == My_mote_list->motePtr->devAddr) {
                        printf("\e[35;1m found mote %"PRIx64" / %08x\e[0m ", devEui, devAddr);
                        goto done;
                    }
                }else
#endif /* if 0 */
                if (devEui != NONE_DEVEUI) {
                    if (devEui == My_mote_list->motePtr->devEui) {
                        //printf("\e[35;1m found from DevEUI %"PRIx64" (%08x", devEui, My_mote_list->motePtr->devAddr);
                        if (devAddr != NONE_DEVADDR) {
                            My_mote_list->motePtr->devAddr = devAddr;
                            //printf("->%08x", My_mote_list->motePtr->devAddr);
                        }
                        //printf(")\e[0m");
                        goto done;
                    }
                } else if (devAddr!= NONE_DEVADDR) {
                    if (devAddr == My_mote_list->motePtr->devAddr) {
                        //printf("\e[35;1m found from DevAddr %08x (%"PRIx64")\e[0m ", devAddr, My_mote_list->motePtr->devEui);
                        goto done;
                    }
                }

            }
            if (My_mote_list->next != NULL)
                My_mote_list = My_mote_list->next;
            else
                break;  // new to be created
        } // ..while (1)

        if (ml_empty != NULL) {
            if (create_mote(sc, &ml_empty->motePtr, devEui, devAddr) < 0)
                return NULL;
            My_mote_list = ml_empty;
            //printf("on first empty: ");
        } else {
            if (create_mote_on_list(sc, &My_mote_list->next, devEui, devAddr) < 0)
                return NULL;
            //printf("at last on list: ");
            My_mote_list = My_mote_list->next;
        }

    } // ...if (mote_list != NULL)

done:
    return My_mote_list->motePtr;
} // ..getMote()


static json_object*
generateGWInfoElement(const GWInfo_t* in, role_e from, bool up)
{
    char gwidstr[64];
    json_object* element = json_object_new_object();

    sprintf(gwidstr, "%"PRIx64, in->id);
    printf("(gw %s %s %ddB rssi:%d) ", gwidstr, in->RFRegion, in->SNR, in->RSSI);
    json_object_object_add(element, ID, json_object_new_string(gwidstr));

    json_object_object_add(element, RFRegion, json_object_new_string(in->RFRegion));

    json_object_object_add(element, RSSI, json_object_new_int(in->RSSI));
    json_object_object_add(element, SNR, json_object_new_int(in->SNR));
    json_object_object_add(element, Lat, json_object_new_double(in->Lat));
    json_object_object_add(element, Lon, json_object_new_double(in->Lon));

    if ((up && from == fNS) || (!up && from == sNS)) {
        /* not sent by sNS up to hNS */
        if (in->ULToken != NULL) {
            json_object_object_add(element, ULToken, json_object_new_string(in->ULToken));
        }
        json_object_object_add(element, DLAllowed, json_object_new_boolean(in->DLAllowed));
    }

    return element;
}

json_object*
generateULMetaData(const ULMetaData_t* md, role_e from, bool addGWMetadata)
{
    char str[128];
    json_object* ulmdObj;

    if (addGWMetadata && md->GWCnt > 0) {
        unsigned n;
        struct _gwList* mygwl;
        json_object* jarray;

        if (md->gwList == NULL) {
            printf("\e[31mULMetaData gwList=null but GwCnt=%u\e[0\n", md->GWCnt);
            return NULL;
        }

        mygwl = md->gwList;
        jarray = json_object_new_array();

        for (n = 0; n < md->GWCnt; n++) {
            json_object_array_add(jarray, generateGWInfoElement(mygwl->GWInfo, from, true));
            mygwl = mygwl->next;
        }

        ulmdObj = json_object_new_object();
        json_object_object_add(ulmdObj, GWInfo, jarray);

        json_object_object_add(ulmdObj, GWCnt, json_object_new_int(md->GWCnt));
    } else
        ulmdObj = json_object_new_object();

    if (md->DevEUI != NONE_DEVEUI) {
        sprintf(str, "%"PRIx64, md->DevEUI);
        json_object_object_add(ulmdObj, DevEUI, json_object_new_string(str));
    }

    if (md->DevAddr != NONE_DEVADDR) {
        sprintf(str, "%x", md->DevAddr);
        json_object_object_add(ulmdObj, DevAddr, json_object_new_string(str));
    }

    if (from == sNS) {
        json_object_object_add(ulmdObj, FPort, json_object_new_int(md->FPort));
        /* if no FPort then also FCnts are absent */
        json_object_object_add(ulmdObj, FCntDown, json_object_new_int(md->FCntDown));
        json_object_object_add(ulmdObj, FCntUp, json_object_new_int(md->FCntUp));
        json_object_object_add(ulmdObj, Confirmed, json_object_new_boolean(md->Confirmed));
    }
    json_object_object_add(ulmdObj, DataRate, json_object_new_int(md->DataRate));
    json_object_object_add(ulmdObj, ULFreq, json_object_new_double(md->ULFreq));

/* TODO
Margin
Battery
 * */

    if (from == fNS) {
        json_object_object_add(ulmdObj, RFRegion, json_object_new_string(md->RFRegion));
        if (md->FNSULToken)
            json_object_object_add(ulmdObj, FNSULToken, json_object_new_string(md->FNSULToken));
    }

    json_object_object_add(ulmdObj, RecvTime, json_object_new_string(md->RecvTime));
    json_object_object_add(ulmdObj, ULFreq, json_object_new_double(md->ULFreq));

    /* TODO: according to service profile
    if (json_object_object_get_ex(j, ULFreq, &ULobj))
    if (json_object_object_get_ex(j, DataRate, &ULobj))
    if (json_object_object_get_ex(j, Margin, &ULobj))
    if (json_object_object_get_ex(j, Battery, &ULobj))
    if (json_object_object_get_ex(j, GWCnt, &ULobj))
    json_object_object_get_ex(j, GWInfo, &ULobj))
 */
    return ulmdObj;
} // ..generateULMetaData()

int
isNetID(MYSQL* sc, uint32_t NetID, const char* colName)
{
    char query[512];
    MYSQL_RES *result;
    MYSQL_ROW row;
    int ret = -1;

    sprintf(query, "SELECT %s FROM roaming WHERE NetID = %u", colName, NetID);
    if (mysql_query(sc, query)) {
        printf("\e[31misNetID mysql_query() %s %06x (%u) %s\e[0m\n", colName, NetID, mysql_errno(sc), mysql_error(sc));
        return ret;
    }
    result = mysql_use_result(sc);
    if (result != NULL) {
        row = mysql_fetch_row(result);
        if (row) {
            if (row[0][0] == '0')
                ret = 0;
            else
                ret = 1;
        }

        mysql_free_result(result);
    } else
        printf("\e[31m%06x %s no result\e[0m\n", NetID, colName);

    //printf(" ret=%d\n", ret);
    return ret;
}

void timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }

    return;
}

void
save_uplink_start(mote_t* mote, uint8_t rx2_seconds, progress_e toState)
{
    struct timespec read_host_time;
    struct _requesterList* myList;

    if (clock_gettime (CLOCK_REALTIME, &read_host_time) == -1)
        perror ("clock_gettime");

    ulmd_free(&mote->ulmd_local);

    if (toState == PROGRESS_JSON) {
        struct timespec t, result;
        t.tv_sec = 0;
        t.tv_nsec = REQUEST_WAITING_SECONDS_LOCAL * 1000000000;
        timespec_diff(&t, &read_host_time, &result);
        read_host_time.tv_sec = result.tv_sec;
        read_host_time.tv_nsec = result.tv_nsec;
    }

    /* new uplink, free previous requester list */
    myList = mote->requesterList;
    while (myList) {
        struct _requesterList *myListNext = myList->next;
        if (myList->R) {
            if (myList->R->needsAnswer)
                printf("\e[31m%p unanswered req from (at %p) %s tid:%lu\e[0m\n", myList->R, &(myList->R), myList->R->ClientID, myList->R->inTid);

            ulmd_free(&myList->R->ulmd);
            free(myList->R);
        }
        free(myList);
        myList = myListNext;
    }
    mote->requesterList = NULL;
    mote->bestR = NULL;
    mote->best_sq = INT_MIN;

    mote->first_uplink_host_time.tv_sec = read_host_time.tv_sec;
    mote->first_uplink_host_time.tv_nsec = read_host_time.tv_nsec;

    mote->release_uplink_host_time.tv_sec = read_host_time.tv_sec + rx2_seconds;
    mote->release_uplink_host_time.tv_nsec = read_host_time.tv_nsec;

    mote->progress = toState;
    mote->ULPHYPayloadLen = 0;
    mote->session.until = 0;    // ensure session copy is current
} // ..save_uplink_start()

mote_t*
GetMoteMtype(MYSQL* sc, const uint8_t* PHYPayloadBin, bool *jReq/*, const char* RFRegion*/)
{
    mote_t* mote = NULL;
    mhdr_t* mhdr = (mhdr_t*)PHYPayloadBin;

    *jReq = false;

    //printf("GetMoteType ");
#ifdef RF_DEBUG
    print_mtype(mhdr->bits.MType);
#endif
    if (mhdr->bits.MType == MTYPE_REJOIN_REQ) {
        rejoin02_req_t* r2 = (rejoin02_req_t*)PHYPayloadBin;
        //printf(" type%u ", r2->type);
        if (r2->type == 1) {
            rejoin1_req_t* r1 = (rejoin1_req_t*)PHYPayloadBin;
            uint64_t rxDevEui64 = eui_buf_to_uint64(r1->DevEUI);
            mote = getMote(sc, &mote_list, rxDevEui64, NONE_DEVADDR);
            if (!mote) {
                printf("NULL = getMote()\n");
                return NULL;
            }
            //printf(" (devEui %016"PRIx64" : %016"PRIx64") ", rxDevEui64, mote->devEui);
        } else if (r2->type == 2 || r2->type == 0) {
            uint64_t rxDevEui64 = eui_buf_to_uint64(r2->DevEUI);
            mote = getMote(sc, &mote_list, rxDevEui64, NONE_DEVADDR);
            if (!mote) {
                printf("NULL = getMote()\n");
                return NULL;
            }
        } else {
            printf("rejoin-type%u?\n", r2->type);
            return NULL;
        }
    } else if (mhdr->bits.MType == MTYPE_JOIN_REQ) {
        join_req_t* rx_jreq_ptr = (join_req_t*)PHYPayloadBin;
        uint64_t rxDevEui64 = eui_buf_to_uint64(rx_jreq_ptr->DevEUI);
        mote = getMote(sc, &mote_list, rxDevEui64, NONE_DEVADDR);
        if (!mote) {
            printf("NULL = getMote()\n");
            return NULL;
        }
        *jReq = true;
    } else if (mhdr->bits.MType == MTYPE_UNCONF_UP || mhdr->bits.MType == MTYPE_CONF_UP) {
        fhdr_t *rx_fhdr = (fhdr_t*)&PHYPayloadBin[1];
        mote = getMote(sc, &mote_list, NONE_DEVEUI, rx_fhdr->DevAddr);
        if (!mote)
            return NULL;
    } else {
        printf("GetMoteType unknown mtype %02x\n", mhdr->bits.MType);
        return NULL;
    }

    return mote;
} // ..GetMoteMtype()

int
mote_update_database_roam(MYSQL* sc, uint64_t devEui, uint32_t devAddr, const char* state, const time_t* until, const uint32_t* roamingWithNetID, const bool* enable_fNS_MIC)
{
    char query[512];
    char where[128];
    int i;
    char str[20];

    printf("mote_update_database_roam(%016"PRIx64" / %08x) ", devEui, devAddr);
    if (getMotesWhere(sc, devEui, devAddr, where) != Success)
        return -1;

    strcpy(query, "UPDATE motes SET roamState = '");
    strcat(query, state);
    strcat(query, "', ");

    if (until != NULL) {
        sprintf(str, "%lu", *until);
        strcat(query, "roamUntil = FROM_UNIXTIME(");
        strcat(query, str);
        strcat(query, "), ");
    }

    if (roamingWithNetID != NULL) {
        sprintf(str, "%u", *roamingWithNetID);
        strcat(query, "roamingWithNetID = '");
        strcat(query, str);
        strcat(query, "', ");
    }

    if (enable_fNS_MIC != NULL) {
        sprintf(str, "%u", *roamingWithNetID);
        strcat(query, "enable_fNS_MIC = ");
        if (*enable_fNS_MIC)
            strcat(query, "1");
        else
            strcat(query, "0");
        strcat(query, ", ");
    }

    i = strlen(query);
    query[i-2] = 0;  // back over last ", "

    strcat(query, " WHERE ");
    strcat(query, where);

    i = mq_send(mqd, query, strlen(query)+1, 0);
    if (i < 0)
        perror("mote_update_database_roam mq_send");

    return i;
} // ..mote_update_database_roam()

static my_ulonglong
getMoteIDfromDevEUI(MYSQL* sc, uint64_t devEui)
{
    my_ulonglong ret = 0;
    MYSQL_RES *result;
    MYSQL_ROW row;
    char query[128];

    sprintf(query, "SELECT ID FROM motes WHERE DevEUI = %"PRIu64, devEui);
    if (mysql_query(sc, query)) {
        printf("\n%s\n", query);
        printf("\e[31mgetMoteIDfromDevEUI(%016"PRIx64"): (%d) %s\e[0m\n", devEui, mysql_errno(sc), mysql_error(sc));
        return ret;
    }
    result = mysql_use_result(sc);
    if (!result) {
        printf("\e[31mgetMoteIDfromDevEUI(%016"PRIx64"): no result\e[0m\n", devEui);
        return ret;
    }
    row = mysql_fetch_row(result);
    if (!row) {
        printf("\e[31mgetMoteIDfromDevEUI(%016"PRIx64"): no row\e[0m\n", devEui);
        mysql_free_result(result);
        return ret;
    }
    sscanf(row[0], "%llu", &ret);
    mysql_free_result(result);

    return ret;
} // ..getMoteIDfromDevEUI();

static void free_mote(mote_t* mote)
{
    if (mote->f)
        free(mote->f);
    if (mote->s)    /* TODO: free sNS */
        free(mote->s);
    if (mote->h)
        free(mote->h);
    if (mote->bestULTokenStr)
        free(mote->bestULTokenStr);

    free(mote);
}

int
deleteNeverUsedSessions(MYSQL* sc, uint64_t devEui)
{
    struct _mote_list* my_mote_list;
    MYSQL_RES *result;
    MYSQL_ROW row;
    int ret = -1;
    char query[128];
    my_ulonglong id = getMoteIDfromDevEUI(sc, devEui);

    printf("deleteNeverUsedSessions() id%llu ", id);

    sprintf(query, "SELECT DevAddr FROM sessions WHERE ID = %llu AND FCntUp IS NULL", id);
    if (mysql_query(sc, query)) {
        printf("\e[31m%s ---- %s\e[0m\n", query, mysql_error(sc));
        return ret;
    }
    result = mysql_use_result(sc);
    if (result == NULL) {
        printf("No result.\n");
        return ret;
    }
    while ((row = mysql_fetch_row(result))) {
        uint32_t devAddr;
        sscanf(row[0], "%u", &devAddr);
        printf(" devAddr:%08x ", devAddr);
        for (my_mote_list = mote_list; my_mote_list != NULL; my_mote_list = my_mote_list->next) {
            mote_t* mote = my_mote_list->motePtr;
            if (!mote)
                continue;
            printf("==%08x? ", mote->devAddr == devAddr);
            if (mote->devAddr == devAddr) {
                printf("free ");
                free_mote(my_mote_list->motePtr);
                my_mote_list->motePtr = NULL;
            }
        }
    }
    mysql_free_result(result);

    sprintf(query, "DELETE FROM sessions WHERE ID = %llu AND FCntUp IS NULL", id);
    fflush(stdout);
    ret = mq_send(mqd, query, strlen(query)+1, 0);
    if (ret < 0)
        perror("deleteOldSessions mq_send");

    printf("\n");
    return ret;
}

/* when OTA end-device has been verified to be using new session, old sessions are removed */
int
deleteOldSessions(MYSQL* sc, uint64_t devEui, bool all)
{   /* for OTA */
    char query[128];
    char str[128];
    int ret = -1;
    char newestDatetime[32];
    my_ulonglong id = getMoteIDfromDevEUI(sc, devEui);

    printf("deleteOldSessions() id%llu ", id);
    newestDatetime[0] = 0;
    if (!all) {
        MYSQL_RES *result;
        sprintf(query, "SELECT createdAt FROM sessions WHERE ID = %llu AND createdAt IN (SELECT max(createdAt) FROM sessions)", id);
        if (mysql_query(sc, query)) {
            printf("\e[31m%s ---- %s\e[0m\n", query, mysql_error(sc));
            return ret;
        }
        result = mysql_use_result(sc);
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row) {
                if (row[0]) {
                    strncpy(newestDatetime, row[0], sizeof(newestDatetime));
                    printf("saving newestDatetime %s\n", newestDatetime);
                    ret = 0;
                }
            }
            mysql_free_result(result);
        } else {
            unsigned err = mysql_errno(sc);
            printf("\e[31mget createdAt no result %d\e[0m\n", err);
        }
        if (ret < 0)
            return ret;
    }

    strcpy(query, "DELETE FROM sessions WHERE ID = ");
    sprintf(str, "%llu", id);
    strcat(query, str);
    if (!all) {
        strcat(query, " AND createdAt < '");
        strcat(query, newestDatetime);
        strcat(query, "'");
    } else
        printf("\e[31mdelete all sessions %016"PRIx64"\e[0m\n", devEui);

    printf("deleting from sessions...");
    fflush(stdout);
    ret = mq_send(mqd, query, strlen(query)+1, 0);
    if (ret < 0)
        perror("deleteOldSessions mq_send");

    return ret;
} // ..deleteOldSessions()

int
add_database_session(   /* for OTA */
    MYSQL* sc,
    uint64_t devEui64,
    const time_t* until,
    const uint8_t* SNwkSIntKey_bin,
    const uint8_t* FNwkSIntKey_bin,
    const uint8_t* NwkSEncKey_bin,
    const uint32_t* FCntUp,
    const uint32_t* NFCntDown,
    const uint32_t* newDevAddr,
    bool OptNeg)
{
    unsigned id;
    int i;
    char query[768];
    char str[LORA_CYPHERKEY_STRLEN];

    if (devEui64 == NONE_DEVEUI) {
        printf("\e[31m not adding session for blank devEui\e[0m\n");
        return -1;
    }

    id = getMoteIDfromDevEUI(sc, devEui64);
    //printf("inserting session using id %u\n", id);

    strcpy(query, "INSERT INTO sessions (ID, ");
    if (until)
        strcat(query, "Until, ");
    if (newDevAddr)
        strcat(query, "DevAddr, ");
    if (NFCntDown)
        strcat(query, "NFCntDown, ");
    if (FCntUp)
        strcat(query, "FCntUp, ");
    if (FNwkSIntKey_bin)
        strcat(query, "FNwkSIntKey, ");
    if (SNwkSIntKey_bin)
        strcat(query, "SNwkSIntKey, ");
    if (NwkSEncKey_bin)
        strcat(query, "NwkSEncKey, ");

    i = strlen(query);
    query[i-2] = 0;  // back over last ", "
    strcat(query, ") VALUES ("); 

    sprintf(str, "%u", id);
    strcat(query, str);
    strcat(query, ", ");

    printf("adding session ");
    if (until) {
        sprintf(str, "FROM_UNIXTIME(%lu)", *until);
        printf(" for %lus ", *until - time(NULL));
        strcat(query, str);
        strcat(query, ", ");
    }
    if (newDevAddr) {
        sprintf(str, "%u", *newDevAddr);
        printf("%08x ", *newDevAddr);
        strcat(query, str);
        strcat(query, ", ");
    }
    if (NFCntDown) {
        printf("NFCntDown%u ", *NFCntDown);
        sprintf(str, "%u", *NFCntDown);
        strcat(query, str);
        strcat(query, ", ");
    }
    if (FCntUp) {
        printf("FCntUp%u ", *FCntUp);
        sprintf(str, "%u", *FCntUp);
        strcat(query, str);
        strcat(query, ", ");
    }
    if (FNwkSIntKey_bin) {
        printf("FNwkSIntKey ");
        //print_buf(FNwkSIntKey_bin, LORA_CYPHERKEYBYTES, "set-FNwkSIntKey");
        strcat(query, "0x");
        for (i = 0; i < LORA_CYPHERKEYBYTES; i++) {
            uint8_t ofs = i * 2;
            sprintf(str+ofs, "%02x", FNwkSIntKey_bin[i]);
        }
        //printf("str:%s\n", str);
        strcat(query, str);
        strcat(query, ", ");
    }
    if (SNwkSIntKey_bin) {
        //printf("SNwkSIntKey ");
        print_buf(SNwkSIntKey_bin, LORA_CYPHERKEYBYTES, "set-SNwkSIntKey");
        strcat(query, "0x");
        for (i = 0; i < LORA_CYPHERKEYBYTES; i++) {
            uint8_t ofs = i * 2;
            sprintf(str+ofs, "%02x", SNwkSIntKey_bin[i]);
        }
        //printf("str:%s\n", str);
        strcat(query, str);
        strcat(query, ", ");
    }
    if (NwkSEncKey_bin) {
        printf("NwkSEncKey ");
        //print_buf(NwkSEncKey_bin, LORA_CYPHERKEYBYTES, "set-NwkSEncKey");
        strcat(query, "0x");
        for (i = 0; i < LORA_CYPHERKEYBYTES; i++) {
            uint8_t ofs = i * 2;
            sprintf(str+ofs, "%02x", NwkSEncKey_bin[i]);
        }
        //printf("str:%s\n", str);
        strcat(query, str);
        strcat(query, ", ");
    }

    i = strlen(query);
    query[i-2] = 0;  // back over last ", "
    strcat(query, ")");

    printf("query len %zu\n", strlen(query));
    i = mq_send(mqd, query, strlen(query)+1, 0);
    if (i < 0) {
        perror("add_database_session mq_send");
        return i;
    }

    sprintf(query, "UPDATE motes SET OptNeg = %u WHERE DevEUI = %"PRIu64, OptNeg, devEui64);
    printf("query len %zu\n", strlen(query));
    i = mq_send(mqd, query, strlen(query)+1, 0);
    if (i < 0) {
        perror("add_database_session mq_send");
        return i;
    }

    printf(" 0 = add_database() ");
    return 0;
} // ..add_database_session()

/* role is receiver of DLMetaData */
role_e 
parseDLMetaData(json_object* obj, DLMetaData_t* out)
{
    role_e rret;
    json_object* o;

    out->FNSULToken = NULL;
    out->ultList = NULL;
    out->DLFreq1 = 0;
    out->DLFreq2 = 0;
    out->Confirmed = false;
    out->HiPriorityFlag = false;
    out->DevEUI = NONE_DEVEUI;  /* absent to statless fNS */
    out->DevAddr = NONE_DEVADDR;  /* absent to statless fNS */

    if (json_object_object_get_ex(obj, DLFreq1, &o)) {
        out->DLFreq1 = json_object_get_double(o);
        if (json_object_object_get_ex(obj, DataRate1, &o))
            out->DataRate1 = json_object_get_int(o);
    }

    if (json_object_object_get_ex(obj, DLFreq2, &o)) {
        out->DLFreq2 = json_object_get_double(o);
        if (json_object_object_get_ex(obj, DataRate2, &o))
            out->DataRate2 = json_object_get_int(o);
    }

    if (out->DLFreq1 != 0 || out->DLFreq2 != 0) {
        rret = fNS;

        if (json_object_object_get_ex(obj, RXDelay1, &o))
            out->RXDelay1 = json_object_get_int(o);
        else {
            printf("\e[31mparseDLMetaData missing %s\e[0m\n", RXDelay1);
            return noneNS;
        }

        if (json_object_object_get_ex(obj, ClassMode, &o)) {
            const char* str = json_object_get_string(o);
            out->ClassMode = str[0];
        } else {
            printf("\e[31mparseDLMetaData missing %s\e[0m\n", ClassMode);
            return noneNS;
        }

        if (out->ClassMode == 'B') {
            if (json_object_object_get_ex(obj, PingPeriod, &o))
                out->PingPeriod = json_object_get_int(o);
            else {
                printf("\e[31mparseDLMetaData missing %s for %s %c\e[0m\n", PingPeriod, ClassMode, out->ClassMode);
                return noneNS;
            }
        }

        if (json_object_object_get_ex(obj, DevEUI, &o)) {
            const char* str = json_object_get_string(o);
            sscanf(str, "%"PRIx64, &out->DevEUI);
        }

        if (json_object_object_get_ex(obj, DevAddr, &o)) {
            const char* str = json_object_get_string(o);
            sscanf(str, "%x", &out->DevAddr);
        }

        if (json_object_object_get_ex(obj, FNSULToken, &o)) {
            const char* str = json_object_get_string(o);
            out->FNSULToken = malloc(strlen(str)+1);
            strcpy(out->FNSULToken, str);
        }

        if (json_object_object_get_ex(obj, HiPriorityFlag, &o)) {
            out->HiPriorityFlag = json_object_get_boolean(o);
        }

    } else {
        rret = sNS;

        /* sent to sNS is required DevEUI for OTA or DevAddr for ABP */
        if (json_object_object_get_ex(obj, DevEUI, &o)) {
            const char* str = json_object_get_string(o);
            sscanf(str, "%"PRIx64, &out->DevEUI);
            out->DevAddr = NONE_DEVADDR; 
        } else if (json_object_object_get_ex(obj, DevAddr, &o)) {
            const char* str = json_object_get_string(o);
            sscanf(str, "%x", &out->DevAddr);
            out->DevEUI = NONE_DEVEUI;
        } else
            return noneNS;

        if (json_object_object_get_ex(obj, FPort, &o))
            out->FPort = json_object_get_int(o);
        else
            return noneNS;

        if (json_object_object_get_ex(obj, FCntDown, &o))
            out->FCntDown = json_object_get_int(o);
        else
            return noneNS;

        if (json_object_object_get_ex(obj, Confirmed, &o))
            out->Confirmed = json_object_get_boolean(o);
    }

    if (rret == fNS) {
        if (json_object_object_get_ex(obj, GWInfo, &o)) {
            struct _ultList* mylist;
            int i, alen = json_object_array_length(o);
            out->ultList = calloc(1, sizeof(struct _ultList));
            mylist = out->ultList;
            for (i = 0; i < alen; ) {
                const char* str = json_object_get_string(json_object_array_get_idx(o, i));
                mylist->ULToken = malloc(strlen(str)+1);
                strcpy(mylist->ULToken, str);

                if (++i < alen)
                    mylist->next = calloc(1, sizeof(struct _ultList));
                else
                    mylist->next = NULL;
            }
        }
    }

    return rret;
} // ..parseDLMetaData()

json_object*
generateDLMetaData(const ULMetaData_t *ulmd, uint8_t rxdrOffset1, DLMetaData_t* dlmd, role_e from)
{
    json_object* ret = json_object_new_object();
    char str[64];

    if (ulmd) { /* items only generated by sNS (not hNS) */
        struct _gwList* mygwl;

        if (ulmd->FNSULToken)
            json_object_object_add(ret, FNSULToken, json_object_new_string(ulmd->FNSULToken));

        json_object_object_add(ret, DataRate, json_object_new_int(ulmd->DataRate));

        json_object_object_add(ret, RFRegion, json_object_new_string(ulmd->RFRegion));

        if (dlmd->ClassMode == 'A')
            sNS_band_conv(dl_rxwin, dlmd->DevEUI, dlmd->DevAddr, ulmd->ULFreq, ulmd->DataRate, rxdrOffset1, ulmd->RFRegion, dlmd);
        else
            printf("\e[31mgenerateDLMetaData class '%c'\e[0m ", dlmd->ClassMode);

        json_object_object_add(ret, GWCnt, json_object_new_int(ulmd->GWCnt));

        json_object* out_jarray = json_object_new_array();
        for (mygwl = ulmd->gwList; mygwl; mygwl = mygwl->next) {
            GWInfo_t* gi = mygwl->GWInfo;
            json_object* ulo = json_object_new_object();
            json_object_object_add(ulo, ULToken, json_object_new_string(gi->ULToken));
            json_object_array_add(out_jarray, ulo);
        }

    } else if (from == sNS)
        printf("\e[31mgdlm no ulmd\e[0m ");

    if (dlmd->DevEUI != NONE_DEVEUI) {
        sprintf(str, "%"PRIx64, dlmd->DevEUI);
        json_object_object_add(ret, DevEUI, json_object_new_string(str));
    }

    if (dlmd->DevAddr != NONE_DEVADDR) {
        sprintf(str, "%x", dlmd->DevAddr);
        json_object_object_add(ret, DevAddr, json_object_new_string(str));
    }

    if (from == sNS) {
        if (dlmd->DLFreq1 > 0) {
            json_object_object_add(ret, DLFreq1, json_object_new_double(dlmd->DLFreq1));
            json_object_object_add(ret, DataRate1, json_object_new_int(dlmd->DataRate1));
        }

        if (dlmd->DLFreq2 > 0) {
            json_object_object_add(ret, DLFreq2, json_object_new_double(dlmd->DLFreq2));
            json_object_object_add(ret, DataRate2, json_object_new_int(dlmd->DataRate2));
        }

        json_object_object_add(ret, RXDelay1 , json_object_new_int(dlmd->RXDelay1));

        str[0] = dlmd->ClassMode;
        str[1] = 0;
        json_object_object_add(ret, ClassMode, json_object_new_string(str));

        if (dlmd->ClassMode == 'B') {
            json_object_object_add(ret, PingPeriod, json_object_new_int(dlmd->PingPeriod));
            printf("generateDLMetaData PingPeriod:%u ", dlmd->PingPeriod);
        }

        json_object_object_add(ret, HiPriorityFlag, json_object_new_boolean(dlmd->HiPriorityFlag));
    } else {
        /* hNS generated */
        json_object_object_add(ret, FPort, json_object_new_int(dlmd->FPort));
        json_object_object_add(ret, FCntDown, json_object_new_int(dlmd->FCntDown));
        json_object_object_add(ret, Confirmed, json_object_new_boolean(dlmd->Confirmed));
    }

    return ret;
} // ..generateDLMetaData()

my_ulonglong
getMoteID(MYSQL* sc, uint64_t devEui, uint32_t devAddr, const char** res)
{
    my_ulonglong ret = 0;
    char query[256];
    char str[96];
    MYSQL_RES *result;

    strcpy(query, "SELECT ID FROM ");

    if (devEui != NONE_DEVEUI) {
        *res = UnknownDevEUI;
        strcat(query, "motes WHERE DevEUI = ");
        sprintf(str, "%"PRIu64, devEui);
        strcat(query, str);
    } else {
        *res = UnknownDevAddr;
        strcat(query, "sessions WHERE DevAddr = ");
        sprintf(str, "%u", devAddr);
        strcat(query, str);
        /* retrieve only most recent session using this DevAddr,
         * otherwise might get old session from another OTA end device  */
        //gives null answer if matching devAddr is older -- strcat(query, " AND createdAt IN (SELECT max(createdAt) FROM sessions)");
        strcat(query, " ORDER BY createdAt DESC");  // want the lastest session which matches DevAddr
    }

    if (mysql_query(sc, query)) {
        printf("\e[31mgetMoteID(): (%u) %s\e[0m\n", mysql_errno(sc), mysql_error(sc));
        return 0;
    }

    result = mysql_use_result(sc);
    if (result) {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            sscanf(row[0], "%llu", &ret);
            *res = Success;
        } /* probably no session if acting as passive-fNS ---- else
            printf("\e[31mgetMoteID no row \"%s\"\e[0m\n", query);*/
        mysql_free_result(result);
    } else {
        printf("\e[31mgetMoteID, no result --- %s\e[0m\n", query);
    }

    return ret;
} // ..getMoteID()

const char*
getMotesWhere(MYSQL* sc, uint64_t devEui, uint32_t devAddr, char* out)
{
    const char* ret;
    char query[256];
    MYSQL_RES *result;

    if (devEui != NONE_DEVEUI) {
        sprintf(out, "DevEUI = %"PRIu64, devEui);
        return Success;
    }

    out[0] = 0;

    sprintf(query, "SELECT ID FROM sessions WHERE DevAddr = %u", devAddr);
    if (mysql_query(sc, query)) {
        printf("\e[31mget mote id: %s\e[0m\n", mysql_error(sc));
        return Other;
    }

    result = mysql_use_result(sc);
    if (result) {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            unsigned moteID;
            sscanf(row[0], "%u", &moteID);
            sprintf(out, "ID = %u", moteID);
            ret = Success;
        } else {
#ifdef RF_DEBUG
            printf(" DevAddr %08x not found\n", devAddr);
#endif
            ret = UnknownDevAddr;
        }
        mysql_free_result(result);
    } else {
        printf("\e[31msNS mote_id_from_devAddr %s, no result\e[0m\n", query);
        ret = Other;
    }

    return ret;
} // ..getMotesWhere()

const char*
sql_motes_query(MYSQL* sc, uint64_t devEui, uint32_t devAddr, sql_t* out)
{
    MYSQL_ROW row;
    MYSQL_RES *result;
    char query[512];
    char where[128];
    int n;
    const char* ret;
    my_ulonglong id = 0;

    memset(out, 0, sizeof(sql_t));

    ret = getMotesWhere(sc, devEui, devAddr, where);
    if (ret != Success) {
        out->roamState = roamNONE;
        return ret;
    }

    strcpy(query, "SELECT *, UNIX_TIMESTAMP(roamUntil) FROM motes WHERE ");
    strcat(query, where);
    SQL_PRINTF("ns %s\n", query);
    if (mysql_query(sc, query)) {
        printf("\n%s\n", query);
        printf("\e[31mNS sql_motes_query: (%d) %s\e[0m\n", mysql_errno(sc), mysql_error(sc));
        return Other;
    }

    result = mysql_use_result(sc);
    if (result == NULL) {
        printf("\e[31mNS sql_motes_query %s, no result\e[0m\n", query);
        return Other;
    }

    row = mysql_fetch_row(result);
    if (row == NULL) {
        RF_PRINTF("%016"PRIx64" / %08x not in motes\n", devEui, devAddr);
        mysql_free_result(result);
        if (devEui == NONE_DEVEUI)
            return UnknownDevAddr;
        else
            return UnknownDevEUI;
    }
    /*int num_fields = mysql_num_fields(result);
    for (n = 0; n < num_fields; n++) {
        printf("motes row[%u]: \"%s\"\n", n, row[n]);
    }*/

    sscanf(row[0], "%llu", &id);

    if (row[2] != NULL) {
        sscanf(row[2], "%u", &n);
        out->OptNeg = n;
    }

    out->is_sNS = false;
    if (strcmp(row[4], roamNONE) == 0) {
        out->roamState = roamNONE;
        out->is_sNS = true;
    } else if (strcmp(row[4], roamfPASSIVE) == 0)
        out->roamState = roamfPASSIVE;
    else if (strcmp(row[4], roamDEFERRED) == 0)
        out->roamState = roamDEFERRED;
    else if (strcmp(row[4], roamsPASSIVE) == 0) {
        out->roamState = roamsPASSIVE;
        out->is_sNS = true;
    } else if (strcmp(row[4], roamsHANDOVER) == 0) {
        out->roamState = roamsHANDOVER;
        out->is_sNS = true;
    } else if (strcmp(row[4], roamhHANDOVER) == 0)
        out->roamState = roamhHANDOVER;
    else {
        printf("\e[31mroamState unknown %s\e[0m\n", row[4]);
        ret = MalformedRequest;
    }

    if (row[7])
        out->enable_fNS_MIC = row[7][0] == '1';

    if (row[6])
        sscanf(row[6], "%u", &out->roamingWithNetID);

    if (row[12] != NULL) {
        sscanf(row[12], "%lu", &out->roamUntil);
        if (out->roamUntil < time(NULL))
            out->roamExpired = true;
        else
            out->roamExpired = false;
    } else
        out->roamUntil = 0; // null roam expiration: expire now

    ret = Success;

    mysql_free_result(result);

    printf(" id%llu ", id);

    return ret;
} // ..sql_motes_query()

int
getSession(MYSQL* sc, uint64_t devEui, uint32_t devAddr, uint8_t nth, session_t* out)
{
    char query[512];
    char str[96];
    MYSQL_RES *result;
    int ret = -1;

    out->until = 0; // zero indicates no session

    if (devEui != NONE_DEVEUI) {
        my_ulonglong id;
        sprintf(query, "SELECT ID FROM motes WHERE DevEui = %"PRIu64, devEui);
        if (mysql_query(sc, query)) {
            printf("\e[31mget mote ID: %s\e[0m\n", mysql_error(sc));
            return -1;
        }
        result = mysql_use_result(sc);
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row)
                sscanf(row[0], "%llu", &id);
            else {
                printf("\e[31mget mote ID: no result\e[0m\n");
                mysql_free_result(result);
                return -1;
            }
            mysql_free_result(result);
        } else {
            printf("\e[31mget mote ID: no result\e[0m\n");
            return -1;
        }
        strcpy(query, "SELECT *, UNIX_TIMESTAMP(Until) FROM sessions WHERE ID = ");
        sprintf(str, "%llu", id);
    } else {
        strcpy(query, "SELECT *, UNIX_TIMESTAMP(Until) FROM sessions WHERE DevAddr = ");
        sprintf(str, "%u", devAddr);
    }
    strcat(query, str);
    strcat(query, " ORDER BY createdAt DESC LIMIT ");
    sprintf(str, "%u", nth);
    strcat(query, str);
    strcat(query, ",1");

    //printf("%s\n", query);
    if (mysql_query(sc, query)) {
        printf("\e[31mget session: (%d) %s\e[0m\n", mysql_errno(sc), mysql_error(sc));
        return -1;
    }
    result = mysql_use_result(sc);
    if (result == NULL) {
        printf("\e[31mgetSession, no result: %s\e[0m\n", query);
        return -1;
    }

    //int num_fields = mysql_num_fields(result);
    MYSQL_ROW row = mysql_fetch_row(result);
    ret = 0;
    if (row != NULL) {
        if (row[3] == NULL) {
            printf("NULL NFCntDown\n");
            out->NFCntDown = 0;
            out->nullNFCntDown = true;
        } else {
            out->nullNFCntDown = false;
            sscanf(row[3], "%u", &out->NFCntDown);
        }

        if (row[4] == NULL) {
            printf("NULL FCntUp (first uplink)\n");
            out->FCntUp = 0;
        } else
            sscanf(row[4], "%u", &out->FCntUp);

        if (row[6])
            memcpy(out->SNwkSIntKeyBin, row[6], LORA_CYPHERKEYBYTES);
        if (row[5])
            memcpy(out->FNwkSIntKeyBin, row[5], LORA_CYPHERKEYBYTES);
        if (row[7])
            memcpy(out->NwkSEncKeyBin, row[7], LORA_CYPHERKEYBYTES);

        //print_buf(out->SNwkSIntKeyBin, LORA_CYPHERKEYBYTES, "get-SNwkSIntKey");
        /* if all network session keys are different then lorawan-1v1, if all same then 1v0 */
        out->OptNeg = memcmp(out->FNwkSIntKeyBin, out->SNwkSIntKeyBin, LORA_CYPHERKEYBYTES) != 0 &&
                      memcmp(out->FNwkSIntKeyBin, out->NwkSEncKeyBin, LORA_CYPHERKEYBYTES) != 0;

        if (row[1] != NULL) {   // row[1] is datetime, row[11] is datetime as time_t
            sscanf(row[11], "%lu", &out->until);
            if (out->until < time(NULL))
                out->expired = true;
            else
                out->expired = false;
        } else {
            /* ABP never expires */
            out->until = ULONG_MAX;
            out->expired = false;
        }

        row = mysql_fetch_row(result);
        if (row)
            out->next = true;
        else
            out->next = false;

    } else {    /* no row means mote exists, but no session at this time */
#ifdef RF_DEBUG
        printf(" session-no-row ");
#endif
        out->expired = true;
    }

#ifdef RF_DEBUG
    printf("session-free-result  ");
    fflush(stdout);
#endif
    mysql_free_result(result);

    return ret;
} // ..getSession()

static int
saveServiceProfile(MYSQL* sc, mote_t* mote, json_object* obj)
{
    MYSQL_RES *result;
    char where[128];
    char query[512];
    ServiceProfile_t sp;
    json_object* ob;
    int ret = -1;
    MYSQL_ROW row;

    //printf("\nsaveServiceProfile() %s\n", json_object_to_json_string(obj));
    if (getMotesWhere(sc, mote->devEui, mote->devAddr, where) != Success) {
        printf("\e[31msaveServiceProfile getMotesWhere fail %016"PRIx64" / %08x\e[0m\n", mote->devEui, mote->devAddr);
        return ret;
    }

    if (json_object_object_get_ex(obj, ULRate, &ob))
        sp.ULRate = json_object_get_int(ob);
    if (json_object_object_get_ex(obj, ULBucketSize, &ob))
        sp.ULBucketSize = json_object_get_int(ob);
    if (json_object_object_get_ex(obj, ULRatePolicy, &ob))
        strncpy(sp.ULRatePolicy, json_object_get_string(ob), sizeof(sp.ULRatePolicy));
    else
        sp.ULRatePolicy[0] = 0;
    if (json_object_object_get_ex(obj, DLRate, &ob))
        sp.DLRate = json_object_get_int(ob);
    if (json_object_object_get_ex(obj, DLBucketSize, &ob))
        sp.DLBucketSize = json_object_get_int(ob);
    if (json_object_object_get_ex(obj, DLRatePolicy, &ob))
        strncpy(sp.DLRatePolicy, json_object_get_string(ob), sizeof(sp.DLRatePolicy));
    else
        sp.DLRatePolicy[0] = 0;
    if (json_object_object_get_ex(obj, AddGWMetadata, &ob))
        sp.AddGWMetadata = json_object_get_boolean(ob);
    if (json_object_object_get_ex(obj, DevStatusReqFreq, &ob))
        sp.DevStatusReqFreq = json_object_get_int(ob);
    if (json_object_object_get_ex(obj, ReportDevStatusBattery, &ob))
        sp.ReportDevStatusBattery = json_object_get_boolean(ob);
    if (json_object_object_get_ex(obj, ReportDevStatusMargin, &ob))
        sp.ReportDevStatusMargin = json_object_get_boolean(ob);
    if (json_object_object_get_ex(obj, DRMin, &ob))
        sp.DRMin = json_object_get_int(ob);
    if (json_object_object_get_ex(obj, DRMax, &ob))
        sp.DRMax = json_object_get_int(ob);
    if (json_object_object_get_ex(obj, ChannelMask, &ob)) {
        const char* str = json_object_get_string(ob);
        strncpy(sp.ChannelMask, str, sizeof(sp.ChannelMask));
    } else
        sp.ChannelMask[0] = 0;
    if (json_object_object_get_ex(obj, PRAllowed, &ob))
        sp.PRAllowed = json_object_get_boolean(ob);
    if (json_object_object_get_ex(obj, HRAllowed, &ob))
        sp.HRAllowed = json_object_get_boolean(ob);
    if (json_object_object_get_ex(obj, RAAllowed, &ob))
        sp.RAAllowed = json_object_get_boolean(ob);
    if (json_object_object_get_ex(obj, NwkGeoLoc, &ob))
        sp.NwkGeoLoc = json_object_get_boolean(ob);
    if (json_object_object_get_ex(obj, TargetPER, &ob))
        sp.TargetPER = json_object_get_double(ob);
    if (json_object_object_get_ex(obj, MinGWDiversity, &ob))
        sp.MinGWDiversity = json_object_get_int(ob);


    sprintf(query, "SELECT ServiceProfileID FROM ServiceProfiles INNER JOIN motes ON ServiceProfiles.ServiceProfileID = motes.ID WHERE %s", where);
    if (mysql_query(sc, query)) {
        printf("\e[31m%s: %s\e[0m\n", query, mysql_error(sc));
        return ret;
    }
    result = mysql_use_result(sc);
    if (result == NULL) {
        fprintf(stderr, "\e[31m%s: no result\e[0m\n", query);
        return ret;
    }
    row = mysql_fetch_row(result);
    if (row) {
        my_ulonglong id;
        sscanf(row[0], "%llu", &id);
        mysql_free_result(result);
        /* update existing profile */
        printf("update-existing-%s ", ServiceProfile);

        ret = updateServiceProfile(&sp, id);
    } else {
        const char* res;
        my_ulonglong id;
        mysql_free_result(result);
        id = getMoteID(sc, mote->devEui, mote->devAddr, &res);
        if (id > 0) {
            /* add new profile */
            printf("add-new-%s ", ServiceProfile);
            ret = insertServiceProfile(&sp, id);    // returns affected rows
            if (ret == 1)
                ret = 0;
            else if (ret < 0)
                printf("\e[31minsertServiceProfile: %s\e[0m\n", mysql_error(sc));
            else
                printf("\e[31m%d = insertServiceProfile\e[0m\n", ret);
        } else
            printf("\e[31msaveServiceProfile %s\e[0m\n", res);
    }

    return ret;
} // ..saveServiceProfile()

/* output iso-8601 */
int
getDeviceProfileTimestamp(MYSQL* sc, const mote_t* mote, char* out, size_t sizeof_out, time_t* tout)
{
    char query[512];
    char where[128];
    MYSQL_RES *result;
    MYSQL_ROW row;
    int ret = -1;

    if (out)
        out[0] = 0;

    if (getMotesWhere(sc, mote->devEui, mote->devAddr, where) != Success) {
        printf("\e[31mgetDeviceProfileTimestamp getMotesWhere failed\e[0m\n");
        return ret;
    }

    sprintf(query, "SELECT UNIX_TIMESTAMP(DeviceProfiles.timestamp) FROM DeviceProfiles INNER JOIN motes ON DeviceProfiles.DeviceProfileID = motes.ID WHERE %s", where);
    if (mysql_query(sc, query)) {
        fprintf(stderr, "\e[31mgetDeviceProfileTimestamp: %s\e[0m\n", mysql_error(sc));
        return ret;
    }
    result = mysql_use_result(sc);
    if (result == NULL) {
        fprintf(stderr, "\e[31mgetDeviceProfileTimestamp: no result\e[0m\n");
        return ret;
    }
    row = mysql_fetch_row(result);
    if (row) {
        time_t t;
        struct tm* timeinfo;
        printf("getDeviceProfileTimestamp: %s -> ", row[0]);
        sscanf(row[0], "%lu", &t);
        if (tout)
            *tout = t;
        timeinfo = localtime(&t);  

        printf(" ours-struct-tm:%d,%d,%d,%d,%d,%d,%d,%d,%d ", timeinfo->tm_isdst, timeinfo->tm_yday, timeinfo->tm_wday, timeinfo->tm_year, timeinfo->tm_mon, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        if (out) {
            strftime(out, sizeof_out, "%FT%T%Z", timeinfo);
            printf(" ours:%s ", out);
        }
        ret = 0;
    } else
        fprintf(stderr, "\e[31mgetDeviceProfileTimestamp %s, no row\e[0m\n", where);

    mysql_free_result(result);
    return ret;
} // ..getDeviceProfileTimestamp()

int
saveDeviceProfile(MYSQL* sc, mote_t* mote, json_object* obj, const char* timeStamp)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    char query[512];
    char where[128];
    DeviceProfile_t dp;
    json_object* ob;
    int ret = -1, i;

    printf("\nsaveDeviceProfile() %s  %s --> %s\n", timeStamp, DeviceProfile, json_object_to_json_string(obj));

    if (json_object_object_get_ex(obj, SupportsClassB, &ob))
        dp.SupportsClassB = json_object_get_boolean(ob);
    if (json_object_object_get_ex(obj, ClassBTimeout, &ob))
        dp.ClassBTimeout = json_object_get_int(ob);
    if (json_object_object_get_ex(obj, PingSlotPeriod, &ob))
        dp.PingSlotPeriod = json_object_get_int(ob);
    if (json_object_object_get_ex(obj, PingSlotDR, &ob))
        dp.PingSlotDR = json_object_get_int(ob);
    if (json_object_object_get_ex(obj, PingSlotFreq, &ob))
        dp.PingSlotFreq = json_object_get_double(ob);
    if (json_object_object_get_ex(obj, SupportsClassC, &ob))
        dp.SupportsClassC = json_object_get_boolean(ob);
    if (json_object_object_get_ex(obj, ClassCTimeout, &ob))
        dp.ClassCTimeout = json_object_get_int(ob);
    if (json_object_object_get_ex(obj, MACVersion, &ob))
        strncpy(dp.MACVersion, json_object_get_string(ob), sizeof(dp.MACVersion));
    if (json_object_object_get_ex(obj, RegParamsRevision, &ob))
        strncpy(dp.RegParamsRevision, json_object_get_string(ob), sizeof(dp.RegParamsRevision));
    if (json_object_object_get_ex(obj, SupportsJoin, &ob))
        dp.SupportsJoin = json_object_get_boolean(ob);
    if (json_object_object_get_ex(obj, RXDelay1, &ob))
        dp.RXDelay1 = json_object_get_int(ob);
    if (json_object_object_get_ex(obj, RXDROffset1, &ob))
        dp.RXDROffset1 = json_object_get_int(ob);
    if (json_object_object_get_ex(obj, RXDataRate2, &ob))
        dp.RXDataRate2 = json_object_get_int(ob);
    if (json_object_object_get_ex(obj, RXFreq2, &ob))
        dp.RXFreq2 = json_object_get_double(ob);
    if (json_object_object_get_ex(obj, FactoryPresetFreqs, &ob)) {
        int alen = json_object_array_length(ob);
        dp.FactoryPresetFreqs[0] = 0;
        for (i = 0; i < alen; ) {
            json_object *ajo = json_object_array_get_idx(ob, i);
            char str[8];
            sprintf(str, "%.2f", json_object_get_double(ajo));
            strcat(dp.FactoryPresetFreqs, str);
            if (++i < alen)
                strcat(dp.FactoryPresetFreqs, ", ");
        }
    }

    if (json_object_object_get_ex(obj, MaxEIRP, &ob))
        dp.MaxEIRP = json_object_get_int(ob);
    if (json_object_object_get_ex(obj, MaxDutyCycle, &ob))
        dp.MaxDutyCycle = json_object_get_double(ob);
    if (json_object_object_get_ex(obj, RFRegion, &ob))
        strncpy(dp.RFRegion, json_object_get_string(ob), sizeof(dp.RFRegion));
    if (json_object_object_get_ex(obj, Supports32bitFCnt, &ob))
        dp.Supports32bitFCnt = json_object_get_boolean(ob);

    if (getMotesWhere(sc, mote->devEui, mote->devAddr, where) != Success) {
        printf("\e[31msaveDeviceProfile getMotesWhere failed\e[0m\n");
        return ret;
    }
    sprintf(query, "SELECT DeviceProfileID FROM DeviceProfiles INNER JOIN motes ON DeviceProfiles.DeviceProfileID = motes.ID WHERE %s", where);
    if (mysql_query(sc, query)) {
        printf("\e[31m%s: %s\e[0m\n", query, mysql_error(sc));
        return ret;
    }
    result = mysql_use_result(sc);
    if (result == NULL) {
        fprintf(stderr, "\e[31m%s: no result\e[0m\n", query);
        return ret;
    }
    row = mysql_fetch_row(result);
    if (row) {
        my_ulonglong id;
        sscanf(row[0], "%llu", &id);
        mysql_free_result(result);
        /* update existing profile */

        ret = updateDeviceProfile(NULL, &dp, id);
    } else {
        const char* res;
        my_ulonglong id;
        mysql_free_result(result);
        id = getMoteID(sc, mote->devEui, mote->devAddr, &res);
        if (id > 0) {
            /* add new profile */
            ret = insertDeviceProfile(NULL, &dp, id);
        } else
            printf("\e[31msaveDeviceProfile %s\e[0m\n", res);
    }

    if (ret == 0) {
        time_t t;
        struct tm* timeinfo;
        /* since database write takes forever, save write time immediately for HRStartReq */
        time(&t);
        timeinfo = localtime(&t);  
        strftime(mote->writtenDeviceProfileTimestamp, sizeof(mote->writtenDeviceProfileTimestamp), "%FT%T%Z", timeinfo);
    }

    return ret;
} // ..saveDeviceProfile()

/* sNSLifetime is source of duration of roaming */
int
getLifetime(MYSQL* sc, uint64_t devEui, uint32_t devAddr)
{
    char query[512];
    char where[128];
    MYSQL_RES *result;
    MYSQL_ROW row;
    unsigned ret = -1;   // roaming master default to zero lifetime (ie stateless passive fNS)

    if (getMotesWhere(sc, devEui, devAddr, where) != Success) {
        printf("\e[31mgetLifetime getMotesWhere failed\e[0m\n");
        return ret;
    }

    sprintf(query, "SELECT sNSLifetime FROM motes WHERE %s", where);
    if (mysql_query(sc, query)) {
        printf("\e[31mgetLifetime %s: %s\e[0m\n", query, mysql_error(sc));
        return false;
    }
    result = mysql_use_result(sc);
    if (result) {
        row = mysql_fetch_row(result);
        if (row) {
            if (row[0])
                sscanf(row[0], "%u", &ret);
        } else {
            printf("\e[33m%s sNSLifetime no row\e[0m\n", where);
            ret = 0;    /* take NULL Lifetime as zero Lifetime */
        }
        mysql_free_result(result);
    } else
        printf("\e[31m%s sNSLifetime no result\e[0m\n", where);

    return ret;
}


void dlmd_free(DLMetaData_t* dlmd)
{
    if (dlmd == NULL)
        return;

    while (dlmd->ultList) {
        struct _ultList* next = dlmd->ultList->next;
        if (dlmd->ultList->ULToken)
            free(dlmd->ultList->ULToken);
        if (dlmd->FNSULToken)
            free(dlmd->FNSULToken);
        free(dlmd->ultList);
        dlmd->ultList = next;
    }
}

int 
r_post_ans_to_ns(requester_t** rp, const char* result, json_object *jobj)
{
    char hostname[128];
    uint32_t cli_netID;
    const char* ansMt;
    requester_t* r = *rp;
    CURL* easy;
    int ret = -1, nxfers;

    if (!r->needsAnswer) {
        printf("\e[31mr_post_ans_to_ns() doesnt need answer\e[0m\n");
        return -1;
    }

    if (jobj == NULL) {
        printf("\e[31mr_post_ans_to_ns() NULL json_object\e[0m\n");
        return -1;
    }

    easy = curl_easy_init();
    if (!easy)
        return -1;
    curl_multi_add_handle(multi_handle, easy);

    sscanf(r->ClientID, "%x", &cli_netID);
    sprintf(hostname, "%06x.%s", cli_netID, netIdDomain);
    printf("\nclient hostname %s, jobj:%p ", hostname, jobj);
    fflush(stdout);

    if (r->MessageType == PRStartReq)
        ansMt = PRStartAns;
    else if (r->MessageType == HRStartReq)
        ansMt = HRStartAns;
    else if (r->MessageType == XmitDataReq)
        ansMt = XmitDataAns; // r_post_ans_to_ns
    else {
        printf("\e31mr_post_to_ns unhandled mt %s\e[0m\n", r->MessageType);
        ansMt = NULL;
    }

    if (ansMt == NULL) {
        char unknownMt[64];
        unknownMt[0] = '?';
        strcat(unknownMt, r->MessageType);
        strcat(unknownMt, "?");
        lib_generate_json(jobj, r->ClientID, myNetwork_idStr, r->inTid, unknownMt, Other);
    } else
        lib_generate_json(jobj, r->ClientID, myNetwork_idStr, r->inTid, ansMt, result);

    printf("ans-to-ns %s %s ", r->ClientID, json_object_to_json_string(jobj));
    ret = http_post_hostname(easy, jobj, hostname, false);
    CURLMcode mc = curl_multi_perform(multi_handle, &nxfers);
    if (mc != CURLM_OK)
        printf(" r_post_ans_to_ns %s = curl_multi_perform(),%d ", curl_multi_strerror(mc), nxfers);
    else {
        r->MessageType = NULL;
        r->ClientID[0] = 0;
        ulmd_free(&r->ulmd);
        free(*rp);
        *rp = NULL;
    }

    return ret;
} // ..r_post_ans_to_ns()

const char*
sql_motes_query_item(MYSQL* sc, uint64_t devEui, uint32_t devAddr, const char* colName, void* out)
{
    char query[512];
    char where[128];
    MYSQL_RES *result;
    MYSQL_ROW row;
    const char* ret = Other;

    if (getMotesWhere(sc, devEui, devAddr, where) != Success) {
        if (devEui == NONE_DEVEUI)
            ret = UnknownDevAddr;
        else
            ret = UnknownDevEUI;
    }

    sprintf(query, "SELECT %s FROM motes WHERE %s", colName, where);
    SQL_PRINTF("ns %s\n", query);
    if (mysql_query(sc, query)) {
        printf("\e[31msNS sql_motes_query_item %s Error querying server: %s\e[0m\n", colName, mysql_error(sc));
        return Other;
    }
    result = mysql_use_result(sc);
    if (result != NULL) {
        row = mysql_fetch_row(result);
        if (row) {
            if (strcmp(colName, "fwdToNetID") == 0) {
                //printf("fwdToNetID row[0]:%s\n", row[0]);
                if (row[0]) { 
                    sscanf(row[0], "%u", (unsigned int*)out);
                } else {
                     /* if null, then this mote is home on this network server */
                    (*(unsigned int *)out) = myNetwork_id32;
                }
                ret = Success;
            } else
                return ret;
        } else
            printf("sql_motes_query_item %s %s, no row\n", colName, where);

        mysql_free_result(result);
    } else
        printf("\e[31msNS sql_motes_query_item %s %s, no result\e[0m\n", colName, where);

    return ret;
} // ..sql_motes_query_item()


static const char*
isMotesAllowed(MYSQL* sc, uint64_t devEui, uint32_t devAddr, bool* Pout, bool* Hout, bool* Aout)
{
    char query[512];
    MYSQL_RES *result;
    MYSQL_ROW row;
    const char* res;
    my_ulonglong m_id = getMoteID(sc, devEui, devAddr, &res);

    if (m_id == 0) {
        return res;
    }

    sprintf(query, "SELECT PRAllowed, HRAllowed, RAAllowed FROM ServiceProfiles WHERE ServiceProfileID = %llu", m_id);
    if (mysql_query(sc, query)) {
        fprintf(stderr, "\e[31mNS ServiceProfile %llu Error querying server: %s\e[0m\n", m_id, mysql_error(sc));
        return Other;
    }
    result = mysql_use_result(sc);
    if (result == NULL) {
        printf("\e[31mServiceProfileID %llu: no result\e[0m\n", m_id);
        return Other;
    }
    row = mysql_fetch_row(result);

    if (row) {
        if (Pout) {
            if (row[0][0] == '0')
                *Pout = false;
            else
                *Pout = true;
        }

        if (Hout) {
            if (row[1][0] == '0')
                *Hout = false;
            else
                *Hout = true;
        }

        if (Aout) {
            if (row[2][0] == '0')
                *Aout = false;
            else
                *Aout = true;
        }
    } else
        printf("\e[31m%s: no row (ID %llu)\e[0m\n", query, m_id);

    mysql_free_result(result);
    return Success;
}

const char InvalidResult[] = "invalid";

static flags_t
finish(mote_t* mote, bool jsonFinish)
{
    sql_t sql;
    struct _requesterList *myList;
    const char *sqlResult, *uplinkResult = NULL;
    flags_t ret;
    mhdr_t* rx_mhdr = (mhdr_t*)mote->ULPayloadBin;

    ret.discard = 0;
    ret.skip_next = 0;

    if (mote->session.until == 0) {
        /* havent yet retrieved session, get it now */

        if (rx_mhdr->bits.MType == MTYPE_REJOIN_REQ) {
            mote->nth = 0;
            if (getSession(sqlConn_lora_network, mote->devEui, NONE_DEVADDR, mote->nth, &mote->session) < 0) {
                ret.discard = 1;
                ret.skip_next = 1;
                return ret;
            }
        } else if (rx_mhdr->bits.MType == MTYPE_UNCONF_UP || rx_mhdr->bits.MType == MTYPE_CONF_UP) {
            mote->nth = 0;
            if (getSession(sqlConn_lora_network, NONE_DEVEUI, mote->devAddr, mote->nth, &mote->session) < 0) {
                ret.discard = 1;
                ret.skip_next = 1;
                return ret;
            }
        }
    }

    if (!jsonFinish) {
        mote->ulmd_local.FNSULToken = malloc(strlen(mote->bestULTokenStr)+1);
        strcpy(mote->ulmd_local.FNSULToken, mote->bestULTokenStr);
    } // ..if (!jsonFinish)

#ifdef RF_DEBUG
    printElapsed(mote);
    printf("\nfinish(,%u) %016"PRIx64" / %08x ", jsonFinish, mote->devEui, mote->devAddr);
#endif
    sqlResult = sql_motes_query(sqlConn_lora_network, mote->devEui, mote->devAddr, &sql);
    if (sqlResult != Success) {
        if (jsonFinish) {
            if (mote->bestR)
                goto jsonHandler;   /* send failure reply to NS which sent this uplink */
        } else {
            /* let unknown devAddr thru, might belong to net this NS has roaming agreement with */
            if (mote->devEui != NONE_DEVEUI) {
                RF_PRINTF("%s sql-setting-discard ", sqlResult);
                ret.discard = 1;    /* remove this now */
                ret.skip_next = 1;
                return ret; // dont have this mote in our database, cant do anything with it 
            }
        }
    } // ..if (sqlResult != Success)

#ifdef RF_DEBUG
    printf(" roam%s ", sql.roamState);
#endif
    if (sql.roamState != roamNONE) {
        if (sql.roamExpired) {
            printf(" end-roaming-expired ");
            /* as sNS in handover roaming: keep roaming state until ED gets forceRejoinReq */
            /* as hNS in handover roaming: keep roaming state and permit rejoin request during expired roaming */
            if (sql.roamState != roamsHANDOVER && sql.roamState != roamhHANDOVER) {
                mote_update_database_roam(sqlConn_lora_network, mote->devEui, mote->devAddr, roamNONE, NULL, NULL, NULL);
                sql.roamState = roamNONE;
            }
        } else {
            printf("for %lu more seconds ", sql.roamUntil - time(NULL));
            if (sql.roamState == roamDEFERRED) {
                printf("\n");
                fflush(stdout);
                ret.skip_next = 1;
                return ret;
            }
        }
    }

    /* do local uplinks when jsonFinish==false, so fNS can send them out first */
    if (jsonFinish) {
        bool discard = false;

        if (sql.roamState == roamsHANDOVER) {
             // sNS_uplink() was called from fNS, only need to send downlink
             goto fDone;
        }
        /* compare local uplinks signal quality against any received from json */
        //printf(" best_r:%p ", mote->best_r);
        if (mote->bestR == NULL) {
            /* no json incoming, or local is best */
            printf("localBest ");
        } else {
            printf("remoteBest ");
        }

        if (mote->bestR != NULL && (sql.roamState == roamsPASSIVE || sql.roamState == roamhHANDOVER)) {
            uint32_t best_cli_id;
            requester_t* r = *mote->bestR;
            sscanf(r->ClientID, "%x", &best_cli_id);
            printf("\e[1;33;46m roaming-with-%06x, best-%06x\e[0m ", sql.roamingWithNetID, best_cli_id);
            /* TODO: has best roaming NS changed? */
        }

        printf(" finish->sNS_uplink_finish() ");
        uplinkResult = sNS_uplink_finish(mote, jsonFinish, &sql, &discard);
        if (uplinkResult != NULL && uplinkResult != Success)
            printf("\e[31mfinish-%s = sNS_uplink_finish()\e[0m ", uplinkResult );
        ret.discard = discard;
    } else if (sql.roamState == roamNONE || sql.roamState == roamfPASSIVE || sql.roamState == roamsHANDOVER) {
        bool discard = false;
        /* take local uplinks so our fNS can send out json before cutoff on receiving NS */
        bool skip = fNS_uplink_finish(mote, &sql, &discard);
        ret.discard = discard;
        ret.skip_next = skip;
        // for roamsHANDOVER: we own the mote, must send FRMPayload now, done in fNS_uplink_finish() -> _sNS_uplink()
        goto fDone; // any required downlink has already been sent
    }

    hNS_uplink_finish(mote, &sql);


jsonHandler:
    for (myList = mote->requesterList; myList != NULL; myList = myList->next) {
        const char* result = InvalidResult;
        char hostname[128];
        uint32_t cli_netID;
        json_object *jobj;
        requester_t *r = myList->R;

        if (r == NULL)
            continue;
        if (!r->needsAnswer)
            continue;
        if (rx_mhdr->bits.MType == MTYPE_REJOIN_REQ || rx_mhdr->bits.MType == MTYPE_JOIN_REQ) {
            /* let sNS_JoinAnsJson() answer this, here is only for (un)conf */
            printf("finish-letting-sNS_JoinAnsJson ");
            continue;
        }
/* OTA: must restart roaming here when devEui != NONE_DEVEUI
        if ((r->MessageType == PRStartReq || r->MessageType == HRStartReq) && mote->devEui != NONE_DEVEUI)
            continue;   // here only handling roaming start for ABP end devices
*/


        jobj = json_object_new_object();
        printf("client r->id:%s r->mt:%s ", r->ClientID, r->MessageType);

        sscanf(r->ClientID, "%x", &cli_netID);
        sprintf(hostname, "%06x.%s", cli_netID, netIdDomain);
        HTTP_PRINTF("client-hostname-\"%s\" ", hostname);

        if (sqlResult != Success) {
            printf("\e[31msql result %s\e[0m ", sqlResult);
            result = sqlResult;
            goto sendAns;
        }

        if (r->MessageType == PRStartReq || r->MessageType == HRStartReq) {
            int roamLifetime = getLifetime(sqlConn_lora_network, mote->devEui, NONE_DEVADDR);   // our lifetime
            if (roamLifetime < 1) {
                printf("\e[31mfinish lifetime fail\e[1m\n");
                result = Other;
                goto sendAns;
            }
            if (roamLifetime > 0)
                json_object_object_add(jobj, Lifetime, json_object_new_int(roamLifetime));

            /* roaming start on (un)conf uplink: roaming partner might not know DevEUI (if this is OTA end device) */
            if (mote->devEui != NONE_DEVEUI) {
                char str[32];
                sprintf(str, "%"PRIx64, mote->devEui);
                json_object_object_add(jobj, DevEUI, json_object_new_string(str));
            }

            if (r == *(mote->bestR)) {
                result = sNS_answer_RStart_Success(mote, jobj);
                // immediate update RAM copy of roamState for downlink code
                if (r->MessageType == PRStartReq)
                    sql.roamState = roamsPASSIVE;
                else if (r->MessageType == HRStartReq)
                    sql.roamState = roamhHANDOVER;
            } else
                result = Deferred;

        } else if (r->MessageType == XmitDataReq) {
            // in sPASSIVE, answer was already sent via r_post_ans_to_ns()
            result = NULL;  // send no answer, unless hNS_XmitDataReq_toAS() fails
            if (mote->ULFRMPayloadLen > 0) {
                const char* res = hNS_XmitDataReq_toAS(mote, mote->ULPayloadBin, mote->ULFRMPayloadLen, &r->ulmd);
                if (res != Success)
                    result = res;
                // else answer is sent by hNS_XmitDataAnsCallback_uplink()
                mote->ULFRMPayloadLen = 0;
            }
        }

sendAns:
        if (result == InvalidResult)
            printf("\e[31mresult-set-set\e[0m ");

        if (result) {
            printElapsed(mote);
            printf("  ((%s %s)) finish-", r->MessageType, result);
            int ret = r_post_ans_to_ns(&(myList->R), result, jobj);
            if (ret == 0) {
                json_object* jo;
                if (json_object_object_get_ex(jobj, PHYPayload, &jo)) {
                    sNSDownlinkSent(mote, Success, NULL);
                }
                if (result == Success) {
                    if (r->MessageType == PRStartReq || r->MessageType == HRStartReq) {
                        sNS_answer_RStart_Success_save(mote);
                    }
                }
            } else
                printf("\e[31m%d = r_post_ans_to_ns()\e[0m\n", ret);
        } else
            printf(" finish-null-result ");

    } // ..for (myList = mote->requesterList; myList != NULL; myList = myList->next)

    /****************************************************/

fDone:
    if (jsonFinish) {
        if (uplinkResult == Success)
            sNS_finish_phy_downlink(mote, &sql, 'A', NULL); // in response to uplink is class-A
        else
            printf("skipping sNS_finish_phy_downlink due to %s uplinkResult\n", uplinkResult);

        /* done with uplink: clear for future classB/C downlinks */
        mote->ULPHYPayloadLen = 0;
    }

    /* requester list and ulmd-gateway list is freed at start of next uplink */

    //printf(" ..finish skip_next%u discard%u\n", ret.skip_next, ret.discard);
    //fflush(stdout);

    return ret;
} // ..finish()

int
sendXmitDataAns(bool toAS, json_object* ansJobj, const char* destIDstr, unsigned long reqTid, const char* result)
{
    int ret = -1;
    char hostname[128];
    CURL* easy;

    lib_generate_json(ansJobj, destIDstr, myNetwork_idStr, reqTid, XmitDataAns, result);
    printf("to %s: %s\n", destIDstr, json_object_to_json_string(ansJobj));

    easy = curl_easy_init();
    if (!easy)
        return CURLE_FAILED_INIT;   // TODO appropriate return
    curl_multi_add_handle(multi_handle, easy);

    if (toAS) {
        strcpy(hostname, "http://");
        strcat(hostname, destIDstr);
        /* resolver not used, URL passed directly to curl */
        ret = http_post_url(easy, ansJobj, hostname, NULL);
    } else {
        /* requires resolver */
        uint32_t cli_netID;
        sscanf(destIDstr, "%x", &cli_netID);
        sprintf(hostname, "%06x.%s", cli_netID, netIdDomain);
        ret = http_post_hostname(easy, ansJobj, hostname, NULL);
    }
    if (ret == 0) {
        int nxfers;
        CURLMcode mc = curl_multi_perform(multi_handle, &nxfers);
        if (mc != CURLM_OK)
            printf(" sendXmitDataAns %s = curl_multi_perform(),%d\n ", curl_multi_strerror(mc), nxfers);
    } else
        printf("\e[31m_sendXmitDataAns post %d\e[0m\n", ret);

    return ret;
} // ..sendXmitDataAns()

const char*
sendXmitDataReq(mote_t* mote, const char* txt, json_object* jo, const char* destIDstr, const char* hostname, const char* payloadObjName, const uint8_t* PayloadBin, uint8_t PayloadLen, AnsCallback_t acb)
{
    CURL* easy;
    uint32_t tid;
    int pret, n;
    char buf[512];
    char* strPtr;
    const char* ret = XmitFailed;

    //printf("sendXmitDataReq() %s to %s host %s PayloadLen%u ", payloadObjName, destIDstr, hostname, PayloadLen);
    if (PayloadLen == 0) {
        printf("\e[31mno-Payload\e[0m ");
        return XmitFailed;
    }

    if (next_tid(mote, txt, acb, &tid) < 0) {
        printf("\e[31msendXmitDataReq-tid-fail\e[0m\n");
        return ret;
    }

    strPtr = buf;
    for (n = 0; n < PayloadLen; n++) {
        sprintf(strPtr, "%02x", PayloadBin[n]);
        strPtr += 2;
    }
    json_object_object_add(jo, payloadObjName, json_object_new_string(buf));

    // payload in answer would be sent on downlink
    lib_generate_json(jo, destIDstr, myNetwork_idStr, tid, XmitDataReq, NULL);

    //JSON_PRINTF("to %s %s ", hostname, json_object_to_json_string(jo));
    //printf(" ---- %s\n", json_object_to_json_string(jo));

    easy = curl_easy_init();
    if (!easy)
        return XmitFailed;
    curl_multi_add_handle(multi_handle, easy);

    if (strncmp(hostname, "http", 4)) {
        /* not url, requires resolver */
        pret = http_post_hostname(easy, jo, hostname, true);
    } else {
        /* resolver not used, URL passed directly to curl */
        pret = http_post_url(easy, jo, hostname, true);
    }

    if (pret == 0) {
        int nxfers;
        CURLMcode mc = curl_multi_perform(multi_handle, &nxfers);
        if (mc != CURLM_OK) {
            printf(" sendXmitDataReq %s = curl_multi_perform(),%d\n ", curl_multi_strerror(mc), nxfers);
            ret = XmitFailed;
        } else
            ret = Success;
        //printf("\e[32mfrom %s\e[0m ", hostname);
    } else {
        printf("\e[31m%s = sendXmitDataReq() %s\e[0m\n", ret, hostname);
    }

    return ret;
} // ..sendXmitDataReq()

const char*
downlinkJson(MYSQL* sc, unsigned long reqTid, const char* clientID, const struct sockaddr *client, json_object* dlmdobj, json_object* inJobj, const char* messageType, int frm, const uint8_t* payBuf, uint8_t payLen, json_object** ansJobj)
{
    mote_t* mote;
    const char *sr, *ret = NULL;
    json_object* o;
    DLMetaData_t dlmd = { 0 };
    sql_t sql;
    role_e role;

    printf(" downlinkJson(%s) payLen%u ---- dlmetadata %s ----- ", messageType, payLen, json_object_to_json_string(dlmdobj));
    role = parseDLMetaData(dlmdobj, &dlmd);

    if (role == fNS) {
        ret = fNS_downlink(&dlmd, reqTid, clientID, payBuf, payLen, "downlinkJson");
        goto djdone;
    } else if (role == noneNS) {
        ret = MalformedRequest;
        goto djdone;
    }

    if (dlmd.DevEUI == NONE_DEVEUI && dlmd.DevAddr == NONE_DEVADDR) {
        ret = MalformedRequest;
        goto djdone;
    }

    if (dlmd.DevEUI != NONE_DEVEUI)
        mote = getMote(sc, &mote_list, dlmd.DevEUI, NONE_DEVADDR);
    else
        mote = getMote(sc, &mote_list, dlmd.DevEUI, dlmd.DevAddr);

    if (!mote) {
        printf("notFound ");
        if (dlmd.DevEUI != NONE_DEVEUI)
            ret = UnknownDevEUI;
        else if (dlmd.DevAddr != NONE_DEVADDR)
            ret = UnknownDevAddr;
        else 
            ret = MalformedRequest;
        goto djdone;
    }
    printElapsed(mote);

    if (json_object_object_get_ex(dlmdobj, FPort, &o)) {
        if (json_object_get_int(o) == 0) {
            printf("fport-zero ");
            ret = InvalidFPort;
            goto djdone;
        }
    } else {
        printf("no-fport ");
        ret = InvalidFPort;
        goto djdone;
    }

    sr = sql_motes_query(sc, mote->devEui, mote->devAddr, &sql);
    if (sr != Success) {
        ret = sr;
        goto djdone;
    }
    printf(" downlinkJson %s\n", sql.roamState);

    if (sql.roamState == roamhHANDOVER) {
        /* this NS is hNS, but sNS is remote */
        ret = hNS_XmitDataReq_down(mote, reqTid, clientID, payBuf, payLen, &dlmd, sql.roamingWithNetID);
        if (ret)
            printf("\e[31m%s = hNS_XmitDataReq_down\e[0m\n", ret);
    } else {
        /* this NS is sNS */
        ret = hNS_to_sNS_downlink(sc, mote, reqTid, clientID, payBuf, payLen, &dlmd, ansJobj);
        /* null return on success */
    }

djdone:
    if (ret != NULL && *ansJobj == NULL)
        *ansJobj = json_object_new_object();    // generate answer now

    dlmd_free(&dlmd);

    return ret;
} // ..downlinkJson()

const char*
uplinkJson(MYSQL* sc, unsigned long reqTid, const char* clientID, const struct sockaddr *client, json_object* ulj, json_object* inJobj, const char* messageType, int frm, const uint8_t* payBuf, uint8_t payLen, json_object** ansJobj)
{
    bool joinReq = false;
    mote_t* mote = NULL;
    requester_t** R;
    uint8_t rx2s;

    printf("%u uplinkJson(%s) %s ", now_ms(), messageType, frm == 0 ? "PHY" : "FRM");

    printf(" ---- %s ---- ", json_object_to_json_string(ulj));
    if (payLen == 0) {
        /* TODO PRStopReq, HRStopReq has no payload. (ProfileReq doesnt get here) */
        printf("\e[31m%s: no payload\e[0m\n", messageType);
        *ansJobj = json_object_new_object();    // generate answer now
        return MalformedRequest;
    }

    if (frm == 0) {
         /* is payBuf is PHYPayload */
        mote = GetMoteMtype(sc, payBuf, &joinReq);
    } else if (frm == 1) {
        json_object* o;
        uint64_t DevEui64 = NONE_DEVEUI;
        uint32_t DevAddr32 = NONE_DEVADDR;

        if (json_object_object_get_ex(ulj, DevEUI, &o))
            sscanf(json_object_get_string(o), "%"PRIx64, &DevEui64);
        if (json_object_object_get_ex(ulj, DevAddr, &o))
            sscanf(json_object_get_string(o), "%x", &DevAddr32);

        /* uplink from another NS */
        if (DevEui64 != NONE_DEVEUI)
            mote = getMote(sc, &mote_list, DevEui64, NONE_DEVADDR);
        else
            mote = getMote(sc, &mote_list, NONE_DEVEUI, DevAddr32);

        if (!mote) {
            printf("\e[31m%s: mote not found %"PRIx64" / %08x\e[0m\n", messageType, DevEui64, DevAddr32);
            return Other;
        }
    } else {
        printf("\e[31m%s: payload type fail %d\e[0m\n", messageType, frm);
    }

    if (mote == NULL) {
        printf("\e[31mno mote\e[0m\n");
        return Other;
    }

    if (dl_rxwin == 1)
        rx2s = joinReq ? 5 : 1;  /* block out mote until rx1 window */
    else
        rx2s = joinReq ? 6 : 2;  /* block out mote until rx2 window */

    if (mote->progress == PROGRESS_OFF) {
        printf("no-local-uplinks OFF->JSON ");
        save_uplink_start(mote, rx2s, PROGRESS_JSON);
    }

    if (messageType == HRStartReq) {
        const char* res;
        bool Hallowed, Aallowed;
        uint32_t cli_netID;
        json_object* ob;

        printf("HRStartReq ");

        sscanf(clientID, "%x", &cli_netID);
        if (isNetID(sc, cli_netID, "HRAllowed") != 1) {
            printf("sNS %06x not handover with net %06x\n", myNetwork_id32, cli_netID);
            *ansJobj = json_object_new_object();    // generate answer now
            return NoRoamingAgreement;
        }

        res = isMotesAllowed(sc, mote->devEui, mote->devAddr, NULL, &Hallowed, &Aallowed);
        if (res != Success) {
            printf("\e[31mHRStartReq %s = isMotesAllowed()\e[0m ", res);
            *ansJobj = json_object_new_object();    // generate answer now
            if (mote->devEui == NONE_DEVEUI)
                return UnknownDevAddr;
            else
                return UnknownDevEUI;
        }
        if (!Hallowed) {
            printf(" !Hallowed ");
            *ansJobj = json_object_new_object();    // generate answer now
            return RoamingActDisallowed;
        }

        if (json_object_object_get_ex(inJobj, DeviceProfileTimestamp, &ob)) {
            char ours[64];
            time_t t, tdb;
            struct tm timeinfo;
            const char* inStr = json_object_get_string(ob);
            strptime(inStr, "%FT%T%Z", &timeinfo);
            timeinfo.tm_isdst = 0;
            printf(" theirs-struct-tm:%d,%d,%d,%d,%d,%d,%d,%d,%d ", timeinfo.tm_isdst, timeinfo.tm_yday, timeinfo.tm_wday, timeinfo.tm_year, timeinfo.tm_mon, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            t = mktime(&timeinfo);
            printf("theirs:\"%s\", %lu\n", inStr, t);
            if (getDeviceProfileTimestamp(sc, mote, ours, sizeof(ours), &tdb) < 0) {
                *ansJobj = json_object_new_object();    // generate answer now
                return Other;
            }
            printf("  ours:\"%s\", %lu\n", ours, tdb);
            printf("HRStartReq  %lu < %lu ? has ", t, tdb);
            if (t < tdb) {
                unsigned diff = tdb - t;
                printf("old DeviceProfile age:%u\n", diff);
                const char* sqlResult;
                *ansJobj = json_object_new_object();    // generate answer now
                sqlResult = jsonGetDeviceProfile(mote->devEui, clientID, ansJobj);
                printf("%s = jsonGetDeviceProfile()\n", sqlResult);
                if (Success == sqlResult)
                    return StaleDeviceProfile;
                else
                    return Other;
            } else {
                printf("current DeviceProfile\n");
                /* not answering here, JoinReq must be sent, answered from JoinAns */
            }
        } else {
            printf("\e[31m%s: missing %s\e[0m\n", messageType, DeviceProfileTimestamp);
            *ansJobj = json_object_new_object();    // generate answer now
            return MalformedRequest;
        }
    } // ..if (MessageType == HRStartReq)
    else if (messageType == PRStartReq) {
        const char* res;
        bool Pallowed, Aallowed;
        uint32_t cli_netID;

        printf("PRStartReq ");
        sscanf(clientID, "%x", &cli_netID);
        if (isNetID(sc, cli_netID, "PRAllowed") != 1) {
            printf("sNS %06x not passive with net %06x\n", myNetwork_id32, cli_netID);
            *ansJobj = json_object_new_object();    // generate answer now
            return NoRoamingAgreement;
        }
        res = isMotesAllowed(sc, mote->devEui, mote->devAddr, &Pallowed, NULL, &Aallowed);
        if (res != Success) {
            printf("\e[31mPRStartReq %s = isMotesAllowed()\e[0m ", res);
            *ansJobj = json_object_new_object();    // generate answer now
            if (mote->devEui == NONE_DEVEUI)
                return UnknownDevAddr;
            else
                return UnknownDevEUI;
        }
        if (!Pallowed) {
            printf(" !Pallowed ");
            *ansJobj = json_object_new_object();    // generate answer now
            return RoamingActDisallowed;
        }
    }


    /* from json */
    R = NULL;
    printf("clientID:%s-", clientID);
    if (mote->requesterList == NULL) {
        printf("first ");
        mote->requesterList = calloc(1, sizeof(struct _requesterList));
        mote->requesterList->R = calloc(1, sizeof(requester_t));
        R = &(mote->requesterList->R);
    } else {
        struct _requesterList* myList = mote->requesterList;
        printf("next ");
        for (;;) {
            if (myList->next == NULL) {
                myList->next = calloc(1, sizeof(struct _requesterList));
                myList->next->R = calloc(1, sizeof(requester_t));
                R = &(myList->next->R);
                break;
            } else
                myList = myList->next;
        }
    }

    requester_t* r = *R;
    r->needsAnswer = true;
    r->MessageType = messageType;
    r->inTid = reqTid;
    strncpy(r->ClientID, clientID, sizeof(r->ClientID));
    if (frm == 0) {  /* if have PHYPayload */
        if (ParseULMetaData(ulj, &r->ulmd) == 0) {
            if (r->ulmd.DevEUI == NONE_DEVEUI && mote->devEui != NONE_DEVEUI) {
                /* this NS knows DevEUI but requesting NS doesnt */
                r->ulmd.DevEUI = mote->devEui; // this ULMetaData could be sent to AS
                printf("\e[35;1mULMetaData %016"PRIx64" / %08x\e[0m ", r->ulmd.DevEUI, r->ulmd.DevAddr);
            }
            if (r->ulmd.gwList) {
                struct _gwList* mygwl;
                int best_sq = INT_MIN;
                int8_t rx_snr = CHAR_MIN;
                const char* ulToken = NULL;
                printf("remote-ParseULMetaData-OK ");
                for (mygwl = r->ulmd.gwList; mygwl; mygwl = mygwl->next) {
                    GWInfo_t* gi = mygwl->GWInfo;
                    int sq = (gi->SNR * SNR_WEIGHT) + gi->RSSI;
                    printf( "{ %d %d } ", gi->SNR, gi->RSSI);
                    if (sq > best_sq) {
                        best_sq = sq;
                        rx_snr = gi->SNR;
                        ulToken = gi->ULToken;
                    }
                }
                printf("( sq %d > %d? ) ", best_sq, mote->best_sq);
                if (best_sq > mote->best_sq) {
                    fhdr_t *rx_fhdr = (fhdr_t*)&mote->ULPayloadBin[1];
                    memcpy(mote->ULPayloadBin, payBuf, payLen);
                    mote->ULPHYPayloadLen = payLen;
                    printf(" Y payLen%u ", payLen);
                    printf(" {[{hdr rx %08x fctrl:%02x }} ", rx_fhdr->DevAddr, rx_fhdr->FCtrl.octet);
                    mote->best_sq = best_sq;
                    mote->rx_snr = rx_snr;
                    mote->bestR = R;
                    if (ulToken) {
                        ssize_t len = strlen(ulToken);
                        if (mote->bestULTokenStr)
                            mote->bestULTokenStr = realloc(mote->bestULTokenStr, len+1);
                        else
                            mote->bestULTokenStr = malloc(len+1);

                        strcpy(mote->bestULTokenStr, ulToken);
                    }
                } else
                    printf("no ");
            } else {
                printf("\e[31mParseULMetaData_() ok, no gwList\e[0m\n");
                *ansJobj = json_object_new_object();    // generate answer now
                r->needsAnswer = false;
                return MalformedRequest;
            }
        } else {
            printf("\e[31mParseULMetaData_() failed\e[0m\n");
            *ansJobj = json_object_new_object();    // generate answer now
            r->needsAnswer = false;
            return MalformedRequest;
        }

    } // ..if (have PHYPayload and ulmdObj)
    else if (frm == 1 && mote) {
        /* should be FRMPayload uplink in hHANDOVER roam state */
        if (ParseULMetaData(ulj, &r->ulmd) < 0) {
            printf("\e[31mParseULMetaData_() failed\e[0m\n");
            *ansJobj = json_object_new_object();    // generate answer now
            r->needsAnswer = false;
            return MalformedRequest;
        }
        mote->ULFRMPayloadLen = payLen;
        memcpy(mote->ULPayloadBin, payBuf, payLen);
        r->ans_ans = true;  // must be set here because might be answered immediately
    }

    printf("\n");
    return NULL;
} // ..uplinkJson()

uint32_t now_ms()
{
    uint32_t ret;
    struct timespec now;

    if (clock_gettime (CLOCK_REALTIME, &now) == -1)
        perror ("clock_gettime");

    ret = now.tv_sec * 1000;
    ret += now.tv_nsec / 1000000;
    return ret;
}

void common_service()
{
    struct _mote_list* my_mote_list;
    struct timespec now;

    if (clock_gettime (CLOCK_REALTIME, &now) == -1)
        perror ("clock_gettime");

    /* find motes with requests waiting */
    for (my_mote_list = mote_list; my_mote_list != NULL; my_mote_list = my_mote_list->next) {
        flags_t flags;
        double seconds_since_request;
        mote_t* mote = my_mote_list->motePtr;
        if (!mote)
            continue;

        sNS_service(mote, now.tv_sec);
        hNS_service(mote, now.tv_sec);

        if (mote->progress == PROGRESS_OFF)
            continue;

        seconds_since_request = difftimespec(now, mote->first_uplink_host_time);

        switch (mote->progress) {
            case PROGRESS_OFF:
                continue;
            case PROGRESS_LOCAL:
                if (seconds_since_request > REQUEST_WAITING_SECONDS_LOCAL) {
                    flags = finish(mote, false);
                    if (flags.skip_next)
                        mote->progress = PROGRESS_OFF;
                    else
                        mote->progress = PROGRESS_JSON;
                }
                break;
            case PROGRESS_JSON:
                if (seconds_since_request > REQUEST_WAITING_SECONDS_JSON) {
                    flags = finish(mote, true);
                    mote->progress = PROGRESS_OFF;
                }
                break;
        }

        if (mote->progress == PROGRESS_OFF) {
            mote->new = false;
            if (flags.discard) {
                free_mote(mote);
                my_mote_list->motePtr = NULL;
            } 
        }

    } // ..for (my_mote_list = mote_list; my_mote_list != NULL; my_mote_list = my_mote_list->next)

} // ..common_service()

int
deviceProfileReq(MYSQL* sc, uint64_t devEui, uint32_t devAddr, const char* elementName, char* out, size_t outSize)
{
    char query[512];
    MYSQL_RES *result;
    MYSQL_ROW row;
    int ret = -1;
    const char* res;
    my_ulonglong m_id = getMoteID(sc, devEui, devAddr, &res);

    if (m_id == 0) {
        printf("\e[31mdeviceProfileReq %s\e[0m\n", res);
        return ret;
    }

    sprintf(query, "SELECT %s FROM DeviceProfiles WHERE DeviceProfileID = %llu", elementName, m_id);
    if (mysql_query(sc, query)) {
        printf("\e[31m%s: %s\e[0m\n", query, mysql_error(sc));
        return ret;
    }
    result = mysql_use_result(sc);
    if (result) {
        row = mysql_fetch_row(result);
        if (row && row[0]) {
            if (out)
                strncpy(out, row[0], outSize);
            ret = 0;
        } 
        mysql_free_result(result);
    }

    return ret;
}

const char*
RStop(MYSQL* sc, const char* pmt, json_object* inJobj, const char* senderID, json_object** ansJobj)
{
    uint32_t otherNetID;
    const char *sr, *ret;
    json_object* obj;
    sql_t sql;
    unsigned lifetime = 0;
    time_t until;
    time_t* untilPtr = NULL;
    uint32_t devAddr = NONE_DEVADDR;
    uint64_t devEui = NONE_DEVEUI;

    *ansJobj = json_object_new_object();

    printf("RStop(%s) ----- %s\n", pmt, json_object_to_json_string(inJobj));
    fflush(stdout);

    if (json_object_object_get_ex(inJobj, DevAddr, &obj)) {
        ret = UnknownDevAddr;
        sscanf(json_object_get_string(obj), "%x", &devAddr);
        sr = sql_motes_query(sc, NONE_DEVEUI, devAddr, &sql);
    } else if (json_object_object_get_ex(inJobj, DevEUI, &obj)) {
        ret = UnknownDevEUI;
        printf("devEui:%s\n", json_object_get_string(obj));
        fflush(stdout);
        sscanf(json_object_get_string(obj), "%"PRIx64, &devEui);
        sr = sql_motes_query(sc, devEui, NONE_DEVADDR, &sql);
    } else
        return MalformedRequest;

    if (sr != Success)
        return ret;

    if (json_object_object_get_ex(inJobj, Lifetime, &obj)) {
        lifetime = json_object_get_int(obj);
        printf("lifetime%u ", lifetime);
        until = time(NULL) + lifetime;
        untilPtr = &until;
    }

    sscanf(senderID, "%x", &otherNetID);

    if (sql.roamState == roamfPASSIVE) {
        if (pmt == PRStopReq) {
            if (untilPtr)
                mote_update_database_roam(sc, devEui, devAddr, roamDEFERRED, untilPtr, &otherNetID, NULL);
            else
                mote_update_database_roam(sc, devEui, devAddr, roamNONE, NULL, &otherNetID, NULL);

            ret = Success;
        }
    } else if (sql.roamState == roamsHANDOVER) {
        if (pmt == HRStopReq) {
            if (untilPtr)
                mote_update_database_roam(sc, devEui, devAddr, roamDEFERRED, untilPtr, &otherNetID, NULL);
            else
                mote_update_database_roam(sc, devEui, devAddr, roamNONE, NULL, &otherNetID, NULL);

            ret = Success;
        }
    }

    return ret; // not in a roaming state that can be stopped
} // ..RStop()

void
xRStartAnsCallback(MYSQL* sc, bool handover, mote_t* mote, json_object* jobj, const char* rxResult, const char* senderID, const uint8_t* rfBuf, uint8_t rfLen, bool fMICup, unsigned lifetime)
{
    time_t until;
    time_t* untilPtr = NULL;
    const char* newState = roamNONE;
    json_object* obj;
    uint32_t otherNetID;

    sscanf(senderID, "%x", &otherNetID);

    if ((rxResult != Success && rxResult != Deferred) || rfLen == 0)
        printf("\e[31m");
    printf("xRStartAnsCallback %s rfLen%u: %016"PRIx64" / %08x\e[0m ", rxResult, rfLen, mote->devEui, mote->devAddr);

    if (json_object_object_get_ex(jobj, DevEUI, &obj)) {
        uint64_t eui;
        sscanf(json_object_get_string(obj), "%"PRIx64, &eui);
        if (mote->devEui != eui) {
            mote->devEui = eui;
            printf("\e[31m->%016"PRIx64"\e[0m ", mote->devEui);
        }
    }

    if (rfLen > 0) {
        if (mote->bestR == NULL) {
            /* send locally */
            if (rfBuf && json_object_object_get_ex(jobj, DLMetaData, &obj)) {
                DLMetaData_t dlMetaData = { 0 };
                role_e role = parseDLMetaData(obj, &dlMetaData);
                if (role == fNS) {
                    const char* ret;
                    printf(" common->");
                    ret = fNS_downlink(&dlMetaData, 0, NULL, rfBuf, rfLen, "xRStartAnsCallback");
                    if (ret != Success)
                        printf("\e[31mxRStartAnsCallback %s = fNS_downlink()\e[0m\n", ret);
                } else
                    printf("\e[31mparseDLMetaData role %u\e[0m\n", role);

                dlmd_free(&dlMetaData);
            } else
                printf("\e[31mxRStart-missing DLMetaData or no rfBuf\e[0m\n");
        } else {
            /* send json */
            printf("\e[31msend-phy-json\e[0m");
        }
    }

    if (lifetime == 0) {
        printf("zero lifetime\n");
        return;
    }
    until = time(NULL) + lifetime;
    untilPtr = &until;

    if (rxResult == Success) {
        printf("rxResult==Success ");
        if (json_object_object_get_ex(jobj, ServiceProfile, &obj)) {
            if (saveServiceProfile(sc, mote, obj) < 0) {
                printf("\e[31mnot roaming: couldnt save ServiceProfile\e[0m\n");
                return;
            }

            if (handover)
                newState = roamsHANDOVER;
            else
                newState = roamfPASSIVE;
        } else
            printf("no %s ", ServiceProfile);

    } else if (strcmp(rxResult, Deferred) == 0) {
        newState = roamDEFERRED;
    }

    if (mote_update_database_roam(sc, mote->devEui, mote->devAddr, newState, untilPtr, &otherNetID, &fMICup) < 0) {
        printf("\e[31mmote_update_database_roam failed\e[0m\n");
    }

} // ..xRStartAnsCallback()

int
next_tid(mote_t* mote, const char* txt, AnsCallback_t cb, uint32_t* out)
{
    static bool inited = false;
    static uint32_t _next_req_tid;
    unsigned n;

    if (!inited) {
        srand(time(NULL));
        _next_req_tid = rand();
        inited = true;
    }

    for (n = 0; n < NT; n++) {
        if (mote->t[n].AnsCallback == NULL) {
            mote->t[n].sentTID = ++_next_req_tid;
            mote->t[n].AnsCallback = cb;
            *out = mote->t[n].sentTID;
            //printf("\e[1;36;45mTID-%u-%s\e[0m ", mote->t[n].sentTID, txt);
            return 0;
        }
    }

    /* transactions full */
    {
        unsigned on = 0;
        uint32_t oldestTID = mote->t[on].sentTID;
        for (n = 1; n < NT; n++) {
            if (mote->t[n].sentTID < oldestTID) {
                oldestTID = mote->t[n].sentTID;
                on = n;
            }
        }
        mote->t[on].sentTID = ++_next_req_tid;
        mote->t[on].AnsCallback = cb;
        //printf("\e[33moverwrite TID[%d] %u, %p\e[0m\n", n, mote->t[on].sentTID, mote->t[on].AnsCallback);
        *out = mote->t[on].sentTID;
    }
    return 0;
}

const char* 
get_nsns_key(MYSQL* sc, json_object *jobj, const char* objName, const char* netIDstr, uint8_t* keyOut)
{
    char query[256];
    MYSQL_RES *result;
    MYSQL_ROW row;
    json_object *obj, *o;
    uint32_t netid;
    const char* AESKeyStr;
    key_envelope_t key_envelope;
    const char* ret = Other;

    if (!json_object_object_get_ex(jobj, objName, &obj))
        return NULL;

    if (!json_object_object_get_ex(obj, AESKey, &o)) {
        printf("\e[31m%s no AESKey\e[0m\n", objName);
        return ret;
    }
    AESKeyStr = json_object_get_string(o);

    if (!json_object_object_get_ex(obj, KEKLabel, &o)) {
        // key in the clear
        for (unsigned n = 0; n < LORA_CYPHERKEYBYTES; n++) {
            unsigned octet;
            sscanf(AESKeyStr, "%02x", &octet);
            AESKeyStr += 2;
            keyOut[n] = octet;
        }
        return Success;
    }

    sscanf(netIDstr, "%x", &netid);
    sprintf(query, "SELECT KEK FROM roaming WHERE NetID = %u AND KEKlabel = '%s'", netid, json_object_get_string(o));
    if (mysql_query(sc, query)) {
        printf("\e[31mget KEK %s\e[0m\n", mysql_error(sc));
        return Other;
    }
    result = mysql_use_result(sc);
    if (result == NULL) {
        printf("\e[31mget KEK no result\e[0m\n");
        return Other;
    }
    row = mysql_fetch_row(result);
    if (row) {
        key_envelope.kek_label = malloc(strlen(json_object_get_string(o))+1);
        strcpy(key_envelope.kek_label, json_object_get_string(o));
        key_envelope.key_len = LORA_CYPHERKEYBYTES;
        key_envelope.key_bin = malloc(key_envelope.key_len);
        memcpy(key_envelope.key_bin, row[0], LORA_CYPHERKEYBYTES);
        ret = parse_key_envelope(obj, &key_envelope, keyOut);
        print_buf(keyOut, LORA_CYPHERKEYBYTES, objName);
        free(key_envelope.key_bin);
        free(key_envelope.kek_label);
    }
    mysql_free_result(result);

    return ret;
}

