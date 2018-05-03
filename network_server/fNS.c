/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "fNS.h"
#include <math.h>

#define NS_VERSION      "0.1"   /**< version */

const char*
fNS_downlink(const DLMetaData_t* DLMetaData, unsigned long reqTid, const char* clientID, const uint8_t* txBuf, uint8_t txLen, const char* txt)
{
    struct _gateway_list* gl;
    struct _region_list* rl;
    struct lgw_pkt_tx_s tx_pkt;
    region_t* region = NULL;
    int i;
    ULToken_t bestULToken;
    const char* inptr;
    gateway_t *gw = NULL;
    struct timespec host_time_now;
    double seconds_since_uplink;

    if (txBuf == NULL) {
        printf("\e[31mfNS_downlink() txBuf == NULL\e[0m\n");
        return XmitFailed;
    }

    if (txLen == 0) {
        printf("\e[31mfNS_downlink() txLen == 0\e[0m\n");
        return XmitFailed;
    }


    if (clock_gettime (CLOCK_REALTIME, &host_time_now) == -1)
        perror ("clock_gettime");

    //printf("fNS_downlink(%s) FNSULToken:%s", txt, DLMetaData->FNSULToken);
    if (DLMetaData->FNSULToken == NULL) {
        printf("\e[31mfNS_downlink() no FNSULToken\e[0m\n");
        return XmitFailed;
    }

    inptr = DLMetaData->FNSULToken;
    for (i = 0; i < sizeof(bestULToken.octets); i++) {
        unsigned oct;
        sscanf(inptr, "%02x", &oct);
        bestULToken.octets[i] = oct;
        inptr += 2;
    }
    /* TODO: if best gateway is busy, use another receving gateway */

    for (gl = gateway_list; gl != NULL; gl = gl->next) {
        if (gl->gateway.fd == bestULToken.gateway.fd)
            gw = &gl->gateway;
    }
    if (!gw)
        return XmitFailed;

    seconds_since_uplink = difftimespec(host_time_now, gw->read_host_time);
    /* 0.01: give a bit of time for gateway to load packet */
    if (DLMetaData->ClassMode == 'A' && (seconds_since_uplink + 0.01) > DLMetaData->RXDelay1) {
        float late_by = seconds_since_uplink - DLMetaData->RXDelay1;
        printf("\e[31mlate by %f seconds\e[0m\n", late_by);
        return XmitFailed;
    }

    for (rl = region_list; rl != NULL; rl = rl->next) {
        if (rl->region.RFRegion == gw->RFRegion)
            region = &rl->region;
    }
    if (!region)
        return XmitFailed;

    // ON_GPS, IMMEDIATE
    tx_pkt.tx_mode = TIMESTAMPED;
    // done by gateway: tx_pkt.rf_chain
    tx_pkt.rf_power = region->regional.dl_tx_dbm;
    tx_pkt.invert_pol = true;
    tx_pkt.preamble = STD_LORA_PREAMB;
    tx_pkt.no_crc = true;
    tx_pkt.no_header = false;

    tx_pkt.size = txLen;
    memcpy(tx_pkt.payload, txBuf, txLen);

    DEBUG_MAC_BUF(tx_pkt.payload, tx_pkt.size, "tx_pkt.payload");

    tx_pkt.modem_idx = 0;   // TODO which modem. rx_pkt->modem_idx;
    tx_pkt.coderate = CR_LORA_4_5;
    tx_pkt.modulation = MOD_LORA;

    if (DLMetaData->ClassMode == 'B') {
        uint16_t ping_offset;   /**< time offset in beacon period */
        double seconds_since_beacon;
        int16_t now_pingslot;
        uint16_t next_occurring_ping;
        fhdr_t* tx_fhdr = (fhdr_t*)&txBuf[1];

        printf(" PING TX %fMHz dr%u ", DLMetaData->DLFreq1, DLMetaData->DataRate1);
        tx_pkt.freq_hz = DLMetaData->DLFreq1 * 1000000;
        if (region->regional.datarates[DLMetaData->DataRate1].fdev_khz == 0) {
            tx_pkt.datarate = region->regional.datarates[DLMetaData->DataRate1].lgw_sf_bps;
            tx_pkt.bandwidth = region->regional.datarates[DLMetaData->DataRate1].lgw_bw;
            MAC_PRINTF("lora ");
            print_hal_sf(tx_pkt.datarate);
            print_hal_bw(tx_pkt.bandwidth);
        } else {
            tx_pkt.modulation = MOD_FSK;
            tx_pkt.f_dev = region->regional.datarates[DLMetaData->DataRate1].fdev_khz;
            tx_pkt.datarate = region->regional.datarates[DLMetaData->DataRate1].lgw_sf_bps;
            MAC_PRINTF("FSK fdev:%d, %dbps ", tx_pkt.f_dev, tx_pkt.datarate);
        }

        printf("PingPeriod:%u seconds at beacon:%u / %08x\n", DLMetaData->PingPeriod, gw->seconds_at_beacon, gw->seconds_at_beacon);
        if (DLMetaData->PingPeriod == 0) {
            printf("\e[31mzero-PingPeriod\e[0m\n");
            return XmitFailed;
        }
        seconds_since_beacon = difftimespec(host_time_now, gw->host_time_beacon_indication);
        printf("seconds_since_beacon :%f\n", seconds_since_beacon);
        now_pingslot = (int16_t)ceil((seconds_since_beacon - 2.12) / 0.03);
        LoRaMacBeaconComputePingOffset(gw->seconds_at_beacon, tx_fhdr->DevAddr, DLMetaData->PingPeriod, &ping_offset);
        printf("now_pingslot:%u ping_offset:%u ", now_pingslot, ping_offset);
        uint16_t _use_pingslot = ping_offset;
        while (_use_pingslot < (now_pingslot+5))    // +5 for network latency
            _use_pingslot += DLMetaData->PingPeriod;
        printf("tx in %d pingslots, use_pingslot:%u\n", _use_pingslot - now_pingslot, _use_pingslot);
        next_occurring_ping = _use_pingslot;
        uint32_t ping_offset_us = next_occurring_ping * 30000;
        float ping_offset_s = next_occurring_ping * 0.03;
        tx_pkt.count_us = gw->trigcnt_pingslot_zero;
        tx_pkt.count_us += ping_offset_us + (gw->sx1301_ppm_err * ping_offset_s);
        printf("use_pingslot:%u, ping_offset_us:%u, ping_offset_s:%f\n", next_occurring_ping, ping_offset_us, ping_offset_s);
        if (next_occurring_ping >= BEACON_WINDOW_SLOTS) {
            mote_t* mote;
            f_t* f;
            mote = getMote(sqlConn_lora_network, &mote_list, NONE_DEVEUI, tx_fhdr->DevAddr);
            if (mote == NULL)
                return XmitFailed;
            if (mote->f == NULL)
                mote->f = calloc(1, sizeof(f_t));
            f = mote->f;
            /* cant be sent in this beacon period */
            printf("send in next beacon window\n");
            f->beaconDeferred = true;
            f->PingPeriod = DLMetaData->PingPeriod;
            f->gw = gw;
            memcpy(&f->tx_pkt, &tx_pkt, sizeof(struct lgw_pkt_tx_s));
            if (clientID) {
                f->clientID = malloc(strlen(clientID)+1);
                strcpy(f->clientID, clientID);
                f->reqTid = reqTid;
            }
            return NULL;    // result later after beacon
        }

        if (gateway_id_write_downlink(gw->eui, &tx_pkt) < 0)
            return XmitFailed;
        else
            return Success;
    } // ..if (DLMetaData->ClassMode == 'B')
    else if (DLMetaData->ClassMode == 'C') {
        printf("ClassC ");
    } else if (DLMetaData->ClassMode != 'A') {
        printf("\e[31m###### TODO class %c ######\e[0m ", DLMetaData->ClassMode);
        return XmitFailed;
    }
 

    if (DLMetaData->DLFreq1 > 1.0) {
        MAC_PRINTF(" TX1 dr%u ", DLMetaData->DataRate1);
        if (DLMetaData->DataRate1 >= region->regional.num_datarates) {
            MAC_PRINTF("\e[31mdr%u >= %u\n", DLMetaData->DataRate1, region->regional.num_datarates);
            return XmitFailed;
        }
        if (region->regional.datarates[DLMetaData->DataRate1].fdev_khz == 0) {
            tx_pkt.datarate = region->regional.datarates[DLMetaData->DataRate1].lgw_sf_bps;
            tx_pkt.bandwidth = region->regional.datarates[DLMetaData->DataRate1].lgw_bw;
            MAC_PRINTF("lora ");
            print_hal_sf(tx_pkt.datarate);
            print_hal_bw(tx_pkt.bandwidth);
        } else {
            tx_pkt.modulation = MOD_FSK;
            tx_pkt.f_dev = region->regional.datarates[DLMetaData->DataRate1].fdev_khz;
            tx_pkt.datarate = region->regional.datarates[DLMetaData->DataRate1].lgw_sf_bps;
            MAC_PRINTF("FSK fdev:%d, %dbps ", tx_pkt.f_dev, tx_pkt.datarate);
        }

        tx_pkt.freq_hz = DLMetaData->DLFreq1 * 1000000;
        tx_pkt.count_us = bestULToken.gateway.count_us + DLMetaData->RXDelay1 * 1000000;;

        MAC_PRINTF("delay%u %uHz\e[0m\n", DLMetaData->RXDelay1, tx_pkt.freq_hz);
        if (gateway_id_write_downlink(gw->eui, &tx_pkt) < 0)
            return XmitFailed;
    }

    if (DLMetaData->DLFreq2 > 1.0) {
        MAC_PRINTF(" TX2 dr%u ", DLMetaData->DataRate2);
        if (DLMetaData->DataRate2 >= region->regional.num_datarates) {
            MAC_PRINTF("\e[31mdr%u >= %u\n", DLMetaData->DataRate2, region->regional.num_datarates);
            return XmitFailed;
        }
        if (region->regional.datarates[DLMetaData->DataRate2].fdev_khz == 0) {
            tx_pkt.datarate = region->regional.datarates[DLMetaData->DataRate2].lgw_sf_bps;
            tx_pkt.bandwidth = region->regional.datarates[DLMetaData->DataRate2].lgw_bw;
            MAC_PRINTF("lora ");
            print_hal_sf(tx_pkt.datarate);
            print_hal_bw(tx_pkt.bandwidth);
        } else {
            tx_pkt.modulation = MOD_FSK;
            tx_pkt.f_dev = region->regional.datarates[DLMetaData->DataRate2].fdev_khz;
            tx_pkt.datarate = region->regional.datarates[DLMetaData->DataRate2].lgw_sf_bps;
            MAC_PRINTF("FSK fdev:%d, %dbps", tx_pkt.f_dev, tx_pkt.datarate );
        }

        if (DLMetaData->ClassMode == 'C') {
            tx_pkt.tx_mode = IMMEDIATE;
        } else
            tx_pkt.count_us = bestULToken.gateway.count_us + (DLMetaData->RXDelay1+1) * 1000000;

        tx_pkt.freq_hz = DLMetaData->DLFreq2 * 1000000;

        MAC_PRINTF("delay%u %uHz\e[0m\n", DLMetaData->RXDelay1+1, tx_pkt.freq_hz);
        if (gateway_id_write_downlink(gw->eui, &tx_pkt) < 0)
            return XmitFailed;
    }

    return Success;
} // ..fNS_downlink()

/** @brief update gateways table in network server database
 * typically done when beacon indication received
 * @param gw to update
 * @param time host time (now)
 */
void fNS_update_gateway(gateway_t* gw, struct timespec* time)
{
    char query[512];
    //struct tm* info;
    //char time_buffer[80];

    //info = localtime(&time->tv_sec);
    //strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d - %H:%M:%S", info);
    //sprintf(query, "UPDATE gateways SET time = '%s' WHERE hex(DevEUI) = '%s'", time_buffer, gw->eui_hex_str);
    sprintf(query, "UPDATE gateways SET time = FROM_UNIXTIME(%lu) WHERE hex(eui) = '%s'", time->tv_sec, gw->eui_hex_str);
    SQL_PRINTF("ns %s\n", query);
    if (mysql_query(sqlConn_lora_network, query)) {
        printf("\e[31mupdate_gateway %s ----- %s\e[0m\n", query, mysql_error(sqlConn_lora_network));
        return;
    }

    sprintf(query, "UPDATE gateways SET downpacketsreceived = %u, gooduppacketsreceived = %u, packetstransmitted = %u, uppacketsforwarded = %u, uppacketsreceived = %u WHERE HEX(eui) = '%s'", gw->downpacketsreceived, gw->gooduppacketsreceived, gw->packetstransmitted, gw->uppacketsforwarded, gw->uppacketsreceived, gw->eui_hex_str);
    SQL_PRINTF("ns %s\n", query);
    if (mysql_query(sqlConn_lora_network, query)) {
        fprintf(stderr, "NS (updategw) Error querying server: %s\n", mysql_error(sqlConn_lora_network));
        return;
    }

    sprintf(query, "UPDATE gateways SET latitude = %f, longitude = %f, altitude = %d WHERE hex(eui) = '%s'",
            gw->coord.lat, gw->coord.lon, gw->coord.alt, gw->eui_hex_str
    );
    SQL_PRINTF("ns %s\n", query);
    if (mysql_query(sqlConn_lora_network, query)) {
        fprintf(stderr, "NS (updategw) Error querying server: %s\n", mysql_error(sqlConn_lora_network));
        return;
    }

}

/** @brief retrieve gateway from database
 * used when gateway has connected and requested configuration
 * @param gw pointer to gateway
 * @return count of gateways found, or -1 for failure, or 0 if not found (new gateway)
 */
int fNS_find_gateway(gateway_t* gw)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    unsigned int num_fields;
    char query[512];
    int num_results = 0;

    sprintf(query, "SELECT RFRegion, downpacketsreceived, gooduppacketsreceived, packetstransmitted, uppacketsforwarded, uppacketsreceived FROM gateways WHERE eui = %" PRIu64, gw->eui);
    SQL_PRINTF("ns %s\n", query);

    if (mysql_query(sqlConn_lora_network, query)) {
        fprintf(stderr, "NS (find gw) Error querying server: %s\n", mysql_error(sqlConn_lora_network));
        return -1;
    }
    result = mysql_use_result(sqlConn_lora_network);
    if (result == NULL) {
        printf("No result.\n");
        return 0;
    }

    num_fields = mysql_num_fields(result);
    printf("NS gateways num_fields:%u\n", num_fields);
    while ((row = mysql_fetch_row(result))) {
        unsigned long *field_lengths = mysql_fetch_lengths(result);
        if (field_lengths == NULL) {
            fprintf(stderr, "NS Failed to get field lengths: %s\n", mysql_error(sqlConn_lora_network));
            return -1;
        }
        /*for (int i = 0; i < num_fields; i++) {
            if (row[i] == NULL) {
                printf("row[%d]==NULL\n", i);
            } else {
                printf("row[%d]:%s\n", i, row[i]);
            }
        }*/

        gw->RFRegion = getRFRegion(row[0]);
        sscanf(row[1], "%u", &gw->downpacketsreceived);
        sscanf(row[2], "%u", &gw->gooduppacketsreceived);
        sscanf(row[3], "%u", &gw->packetstransmitted);
        sscanf(row[4], "%u", &gw->uppacketsforwarded);
        sscanf(row[5], "%u", &gw->uppacketsreceived);

        num_results++;
    }

    mysql_free_result(result);

    printf("NS found gw, counts: %u, %u, %u, %u, %u\n",
        gw->downpacketsreceived,
        gw->gooduppacketsreceived,
        gw->packetstransmitted,
        gw->uppacketsforwarded,
        gw->uppacketsreceived
    );

    return num_results;
}

#define DEFAULT_GW_REGION       AS923 /**< TODO:put-into-conf. initial region assignment for new gateway */
/** @brief create new gateway in network server database
 * Used when new gateway EUI is seen, first time connecting
 * @param gw pointer to gateway
 * @return 1 if gateway added, or -1 for failure
 */
int fNS_add_gateway(gateway_t* gw)
{
    char query[128];

    printf("network_server_add_gateway()\n");
    gw->RFRegion = DEFAULT_GW_REGION;  // TODO: get from database defaultGatewayRegion
    sprintf(query, "INSERT INTO gateways (eui, RFRegion, allowGpsToSetPosition) VALUES (%" PRIu64  ", '%s', TRUE)", gw->eui, gw->RFRegion);
    SQL_PRINTF("ns %s\n", query);
    if (mysql_query(sqlConn_lora_network, query)) {
        printf("\e[31mNS (add gw) Error querying server: %s\e[0m\n", mysql_error(sqlConn_lora_network));
        return -1;
    }

    printf("mysql_affected_rows:%u\n", (unsigned int)mysql_affected_rows(sqlConn_lora_network));
    return mysql_affected_rows(sqlConn_lora_network);
}

int
fNS_init(const char* dbhostname, const char* dbuser, const char* dbpass, uint16_t dbport, const char *dbName)
{
    if (database_open(dbhostname, dbuser, dbpass, dbport, dbName, &sqlConn_lora_network, NS_VERSION) < 0)
        return -1;

    unsigned ID;
    unsigned NetID_Type = myNetwork_id32 >> 21;

    if (NetID_Type < 2)
        ID = myNetwork_id32 & 0x3f; // 6LSB
    else if (NetID_Type == 2)
        ID = myNetwork_id32 & 0x1ff;    // 9LSB
    else
        ID = myNetwork_id32 & 0x1fffff; // 21LSB

    // type prefix length = NetID_Type + 1
    switch (NetID_Type) {
        case 0:
            devAddrBase = 0x00000000;
            nwkAddrBits = 25;
            break;
        case 1:
            devAddrBase = 0x80000000;
            nwkAddrBits = 24;
            break;
        case 2:
            devAddrBase = 0xc0000000;
            nwkAddrBits = 20;
            break;
        case 3:
            devAddrBase = 0xe0000000;
            nwkAddrBits = 18;
            break;
        case 4:
            devAddrBase = 0xf0000000;
            nwkAddrBits = 16;
            break;
        case 5:
            devAddrBase = 0xf8000000;
            nwkAddrBits = 13;
            break;
        case 6:
            devAddrBase = 0xfc000000;
            nwkAddrBits = 10;
            break;
        case 7:
            devAddrBase = 0xfe000000;
            nwkAddrBits = 7;
            break;
        default:
            return -1;
    }
    devAddrBase |= ID << nwkAddrBits;
    printf("network_id:%06x, devAddrBase:%08x, nwkAddrBits:%u\n", myNetwork_id32, devAddrBase, nwkAddrBits);


    return 0;
} // ..fNS_init()

/** @brief convert lora spreading factor + bandwidth to datarate
 */
static uint8_t
get_dr(regional_t* rp, uint32_t lgw_datarate, uint8_t lgw_bw)
{
    uint8_t dr;

    for (dr = 0; dr < MAX_DATARATES; dr++) {
        /* TODO: MOD_FSK, when fdev_khz == 0 */
        if (lgw_datarate == rp->datarates[dr].lgw_sf_bps && lgw_bw == rp->datarates[dr].lgw_bw) {
            return dr;
        }
    } // ..for (i = 0; i < MAX_DATARATES; i++)

    return MAX_DATARATES;   // not found
}

void fNS_uplink_direct(const gateway_t* gateway, const struct lgw_pkt_rx_s* rx_pkt)
{
    int sq, i;
    time_t rx_seconds;
    uint32_t s_since_beacon;
    mote_t* mote;
    bool joinReq = false;
    uint8_t rx2s;
    ULToken_t ult;
    char ultstr[ULTOKENSTRSIZE];
    char *ultstrptr;
    struct _gwList* mygwl;
    uint8_t dr = get_dr(gateway->rp, rx_pkt->datarate, rx_pkt->bandwidth);

    if (MAX_DATARATES == dr) {
        printf("\e[31m%s ", gateway->RFRegion);
        print_hal_sf(rx_pkt->datarate);
        print_hal_bw(rx_pkt->bandwidth);
        printf(" uplink datarate not found\e[0m\n");
        return;
    }

    mote = GetMoteMtype(sqlConn_lora_network, rx_pkt->payload, &joinReq);
    if (!mote) {
        printf(" NULL = GetMoteMtype()\n");
        return;
    }

    ult.gateway.fd = gateway->fd;
    ult.gateway.count_us = rx_pkt->count_us;
    ult.gateway.seconds_at_beacon = gateway->seconds_at_beacon;
    ult.gateway.tstamp_at_beacon = gateway->tstamp_at_beacon;
    ult.gateway.lgw_trigcnt_at_next_beacon = gateway->lgw_trigcnt_at_next_beacon;
    ult.gateway.beacon_ch = gateway->beacon_ch;

    if (dl_rxwin == 1)
        rx2s = joinReq ? 5 : 1;  /* block out mote until rx1 window */
    else
        rx2s = joinReq ? 6 : 2;  /* block out mote until rx2 window */

    //printf(" fNS_uplink_direct() ");
    if (mote->progress == PROGRESS_OFF) {
        struct tm* timeinfo;
        save_uplink_start(mote, rx2s, PROGRESS_LOCAL);
        //printf(" OFF->LOCAL ");

        s_since_beacon = (rx_pkt->count_us - gateway->tstamp_at_beacon) / 1000000;
        rx_seconds = gateway->seconds_at_beacon + s_since_beacon;
        rx_seconds += 315964800;// TODO convert 1970 epoch to 1980 without leaps
        rx_seconds -= leap_seconds;
        timeinfo = localtime(&rx_seconds);  
        strftime(mote->ulmd_local.RecvTime, sizeof(mote->ulmd_local.RecvTime), "%FT%T%Z", timeinfo);
        mote->ulmd_local.GWCnt = 0;

        mote->read_host_time.tv_sec = gateway->read_host_time.tv_sec;
        mote->read_host_time.tv_nsec = gateway->read_host_time.tv_nsec;
    }
    //printf("\n");

    ultstrptr = ultstr;
    for (i = 0; i < sizeof(ult.octets); i++) {
        sprintf(ultstrptr, "%02x", ult.octets[i]);
        ultstrptr += 2;
    }
    //MAC_PRINTF("new ULToken \"%s\" ", ultstr);

    GWInfo_t* gi;
    if (mote->ulmd_local.gwList == NULL) {
        mote->ulmd_local.gwList = calloc(1, sizeof(struct _gwList));
        mygwl = mote->ulmd_local.gwList;
    } else {
        mygwl = mote->ulmd_local.gwList;
        while (mygwl->next != NULL)
            mygwl = mygwl->next;
        mygwl->next = calloc(1, sizeof(struct _gwList));
        mygwl = mygwl->next;
    }
    mygwl->GWInfo = malloc(sizeof(GWInfo_t));
    gi = mygwl->GWInfo;

    mote->ulmd_local.GWCnt++;

    /*uint64_t id;*/    gi->id = gateway->eui;
    /*const char* RFRegion;*/ gi->RFRegion = gateway->RFRegion;
    /*int8_t RSSI;*/ gi->RSSI = rx_pkt->rssi;
    /*int8_t SNR;*/ gi->SNR = rx_pkt->snr;
    /*float Lat;*/ gi->Lat = gateway->coord.lat;
    /*float Lon;*/ gi->Lon = gateway->coord.lon;
    /*char *ULToken;*/ gi->ULToken = malloc(strlen(ultstr)+1);
    strcpy(gi->ULToken, ultstr);
    /*bool DLAllowed;*/ gi->DLAllowed = true;

    sq = (rx_pkt->snr * SNR_WEIGHT) + rx_pkt->rssi;
    if (sq > mote->best_sq) {
        ssize_t ulen = strlen(ultstr);
        //MAC_PRINTF("save snr as best %f ", rx_pkt->snr);
        mote->best_sq = sq;
        printf("local-sq%d ", mote->best_sq);
        mote->rx_snr = rx_pkt->snr;

        if (!mote->bestULTokenStr)
            mote->bestULTokenStr = malloc(ulen+1);

        strcpy(mote->bestULTokenStr, ultstr);

        memcpy(mote->ULPayloadBin, rx_pkt->payload, rx_pkt->size);
        mote->ULPHYPayloadLen = rx_pkt->size;

        mote->ulmd_local.RFRegion = gateway->RFRegion;
        mote->ulmd_local.DataRate = dr;
        mote->ulmd_local.DevEUI = mote->devEui;
        mote->ulmd_local.DevAddr = mote->devAddr;
        mote->ulmd_local.ULFreq = rx_pkt->freq_hz / 1000000.0;
    } /*else
        MAC_PRINTF("\n");*/

} // ..fNS_uplink_direct()

static void
fNS_XmitDataAnsCallback(MYSQL* sc, mote_t* mote, json_object* jobj, const char* rxResult, const char* senderID, const uint8_t* rfBuf, uint8_t rfLen)
{

    printf("fNS_XmitDataAnsCallback() %s from %s rfLen%u ", rxResult, senderID, rfLen);

    if (rfLen > 0) {
        /* normally, downlinks would be sent down as XmitDataReq, but join answer is sent in Ans */
        json_object* obj;
        const mhdr_t *mhdr = (mhdr_t*)mote->ULPayloadBin;
        //printf("\e[31m");
        print_mtype(mhdr->bits.MType);
        //printf("\e[0m\n");

        if (json_object_object_get_ex(jobj, DLMetaData, &obj)) {
            DLMetaData_t dlMetaData = { 0 };
            role_e role = parseDLMetaData(obj, &dlMetaData);
            if (role == fNS) {
                const char* ret;

                printf(" common->");
                ret = fNS_downlink(&dlMetaData, 0, NULL, rfBuf, rfLen, "fNS_XmitDataAnsCallback");
                if (ret != Success)
                    printf("\e[31mfNS_XmitDataAnsCallback %s = fNS_downlink()\e[0m\n", ret);
            } else
                printf("\e[31mparseDLMetaData role %u\e[0m\n", role);

            dlmd_free(&dlMetaData);
        } else
            printf("\e[31mXRStart-missing DLMetaData\e[0m\n");
    }


    printf("\n");
} // ..fNS_XmitDataAnsCallback()

static bool
check_cmacF(const uint8_t* phyPayload, uint8_t phyLen, uint32_t fCntUp, const uint8_t* fNwkSIntKey)
{
    uint16_t cmacF;
    AES_CMAC_CTX cmacctx;
    //uint8_t b[LORA_AUTHENTICATIONBLOCKBYTES];
    block_t block;
    uint8_t temp[LORA_AUTHENTICATIONBLOCKBYTES];
    const fhdr_t* rx_fhdr = (fhdr_t*)&phyPayload[1];
    uint16_t rx_mic;

    if (!fNwkSIntKey) {
        printf("\e[31mcheck_cmacF() no FNwkSIntKey\e[0m\n");
        return false;
    }

    /*printf(" (%02x %02x) ",
        phyPayload[phyLen-1],
        phyPayload[phyLen-2]
    );*/
    rx_mic = phyPayload[phyLen-1] << 8;
    rx_mic += phyPayload[phyLen-2];

    phyLen -= LORA_FRAMEMICBYTES;

    /* do block b0 */
    //memset(b, 0 , LORA_AUTHENTICATIONBLOCKBYTES);
    block.b.header = 0x49; //b[0] = 0x49; //authentication flags
    block.b.confFCnt = 0;
    block.b.dr = 0;
    block.b.ch = 0;
    block.b.dir = DIR_UP;
    block.b.DevAddr = rx_fhdr->DevAddr;
    block.b.FCnt = fCntUp;
    block.b.zero8 = 0;
    block.b.lenMsg = phyLen;

    //b[5] = 0; // up
    //Write4ByteValue(&b[6], rx_fhdr->DevAddr);
    //Write4ByteValue(&b[10], fCntUp);

    //b[15] = phyLen;
    DEBUG_MIC_BUF_UP(block.octets, LORA_AUTHENTICATIONBLOCKBYTES, "up-b0");

    AES_CMAC_Init(&cmacctx);
    AES_CMAC_SetKey(&cmacctx, fNwkSIntKey);
    DEBUG_MIC_BUF_UP(fNwkSIntKey, 16, "b0-key");

    AES_CMAC_Update(&cmacctx, block.octets, LORA_AUTHENTICATIONBLOCKBYTES);
    AES_CMAC_Update(&cmacctx, phyPayload, phyLen);

    AES_CMAC_Final(temp, &cmacctx);

    memcpy(&cmacF, temp, 2);

    DEBUG_MIC_BUF_UP(phyPayload, phyLen, "phyPayload");
    DEBUG_MIC_UP("up-mic-1v1: FCntUp%u phyLen%u, cmacF:%04x ", fCntUp, phyLen, cmacF);
    DEBUG_MIC_UP("cmacF rx_mic:%04x vs %04x\n", rx_mic, cmacF);
    /*if (cmacF != rx_mic) {
        print_buf(b, LORA_AUTHENTICATIONBLOCKBYTES, "up-b0");
        print_buf(fNwkSIntKey, 16, "b0-key");
        printf("FCntUp%u cmacF rx_mic:%04x vs %04x\n", fCntUp, rx_mic, cmacF);
    }*/
    return cmacF == rx_mic;
}

static const char*
fNS_XmitDataReq_uplink(mote_t* mote, const sql_t* sql)
{
    char hostname[128];
    char buf[512];
    const ULMetaData_t* u = &mote->ulmd_local;
    const char* result;
    json_object *jo, *uj;

    const mhdr_t *mhdr = (mhdr_t*)mote->ULPayloadBin;
    if ((mhdr->bits.MType == MTYPE_UNCONF_UP || mhdr->bits.MType == MTYPE_CONF_UP) && sql->enable_fNS_MIC) {
        const fhdr_t *fhdr = (fhdr_t*)&mote->ULPayloadBin[1];
        int A, B;
        uint32_t rxFCnt32;
        rxFCnt32 = (mote->session.FCntUp & 0xffff0000) | fhdr->FCnt;
        printf(" enable_fNS_MIC ");
        A = rxFCnt32 - mote->session.FCntUp;
        B = (rxFCnt32 + 0x10000) - mote->session.FCntUp;
        if (A < 0 && B > MAX_FCNT_GAP) {
            printf("\e[31mreplay\e[0m\n");
            return Other;
        }
        if (A > MAX_FCNT_GAP && B > MAX_FCNT_GAP) {
            printf("\e[31mreplay\e[0m\n");
            return Other;
        }

        if (!sql->OptNeg) {
            block_t block;
            uint32_t calculated_mic, rx_mic;
            block.b.header = 0x49;
            block.b.confFCnt = 0;
            block.b.dr = 0;
            block.b.ch = 0;
            block.b.dir = DIR_UP;
            block.b.DevAddr = fhdr->DevAddr;
            block.b.FCnt = rxFCnt32;
            block.b.zero8 = 0;
            block.b.lenMsg = mote->ULPHYPayloadLen - LORA_FRAMEMICBYTES;
            calculated_mic = LoRa_GenerateDataFrameIntegrityCode(&block, mote->session.SNwkSIntKeyBin, mote->ULPayloadBin);
            rx_mic = mote->ULPayloadBin[mote->ULPHYPayloadLen-1] << 24;
            rx_mic += mote->ULPayloadBin[mote->ULPHYPayloadLen-2] << 16;
            rx_mic += mote->ULPayloadBin[mote->ULPHYPayloadLen-3] << 8;
            rx_mic += mote->ULPayloadBin[mote->ULPHYPayloadLen-4];
            if (calculated_mic != rx_mic)
                return MICFailed;
        }   // else 1v1 cmacF checked by caller

        printf("MICok ");
    } // if (unConf)

    uj = generateULMetaData(u, fNS, true);
    if (!uj) {
        printf("\e[31mfNS_XmitDataReq_uplink ulmd_local bad\e[0m\n");
        return Other;
    }
    jo = json_object_new_object();
    json_object_object_add(jo, ULMetaData, uj);

    sprintf(buf, "%06x", sql->roamingWithNetID);
    sprintf(hostname, "%06x.%s", sql->roamingWithNetID, netIdDomain);

    printElapsed(mote);
    //printf(" fNS->");
    result = sendXmitDataReq(mote, "fNS_XmitDataReq_uplink->sendXmitDataReq", jo, buf, hostname, PHYPayload, mote->ULPayloadBin, mote->ULPHYPayloadLen, fNS_XmitDataAnsCallback);
    if (result != Success) {
        printf("\e[31mfNS %s = sendXmitDataReq()\e[0m\n", result);
    } 

    return result;
} // ..fNS_XmitDataReq_uplink()


static void
PRStartAnsCallback(MYSQL* sc, mote_t* mote, json_object* jobj, const char* rxResult, const char* senderID, const uint8_t* rfBuf, uint8_t rfLen)
{
    json_object* obj;
    unsigned lifetime = 0;
    bool fMICup = false;
    time_t until;
    time_t* untilPtr = NULL;

    printElapsed(mote);
    printf("PRStartAnsCallback(%s) ", rxResult);

    if (json_object_object_get_ex(jobj, Lifetime, &obj)) {
        lifetime = json_object_get_int(obj);
        printf("lifetime%u ", lifetime);
        until = time(NULL) + lifetime;
        untilPtr = &until;
    } else
        printf("no-lifetime ");

    if (json_object_object_get_ex(jobj, DevAddr, &obj)) {
        /* sNS generated a DevAddr for an OTA join, we must save it to recognize future uplinks */
        sscanf(json_object_get_string(obj), "%x", &mote->devAddr);
        printf("->%08x ", mote->devAddr);
    } else if (mote->devAddr == NONE_DEVADDR)
        printf("\e[31mmissing DevAddr\e[0m ");

    if (json_object_object_get_ex(jobj, DevEUI, &obj)) {
        /* this is probably answer from (un)conf uplink, and this ED is OTA, but never previously seen a (re)join */
        sscanf(json_object_get_string(obj), "%"PRIx64, &mote->devEui);
        printf("->%016"PRIx64" ", mote->devEui);
    } else if (mote->devEui == NONE_DEVEUI)
        printf("\e[31mmissing DevEUI\e[0m ");


    if (lifetime > 0) {
        if (rxResult == Success) {
            json_object* obj;
            uint8_t keyBin[LORA_CYPHERKEYBYTES];

            fMICup = false;
            if (json_object_object_get_ex(jobj, FCntUp, &obj)) {
                const char* res;
                uint32_t FCntUp = json_object_get_int(obj);

                printf("1v1 ");
                res = get_nsns_key(sc, jobj, FNwkSIntKey, senderID, keyBin);
                if (res == Success) {
                    /* 1v1 */
                    add_database_session(sc, mote->devEui, untilPtr, NULL, keyBin, NULL, &FCntUp, NULL, &mote->devAddr, true);
                    fMICup = true;
                } else if (res == NULL) {
                    printf("\e[31mno %s\e[0m ", FNwkSIntKey);
                    add_database_session(sc, mote->devEui, untilPtr, NULL, NULL, NULL, &FCntUp, NULL, &mote->devAddr, true);
                    fMICup = false;
                } else
                    printf("\e[31m%s %s\e[0m ", res, FNwkSIntKey);
            } else {
                const char* res;
                printf("1v0? ");
                res = get_nsns_key(sc, jobj, NwkSKey, senderID, keyBin);
                if (res == Success) {
                    /* 1v0 */
                    print_buf(keyBin, 16, NwkSKey);
                    add_database_session(sc, mote->devEui, untilPtr, keyBin, keyBin, keyBin, NULL, NULL, &mote->devAddr, false);
                    fMICup = true;
                } /* else not checking uplink MIC */
                else if (res == NULL) {
                    printf("no %s ", NwkSKey);
                    add_database_session(sc, mote->devEui, untilPtr, NULL, NULL, NULL, NULL, NULL, &mote->devAddr, false);
                    fMICup = false;
                } else
                    printf("\e[31m%s %s\e[0m ", res, NwkSKey);
            }
        } else if (rxResult == Deferred) {
            if (mote->devAddr == NONE_DEVADDR)
                printf("\e[31mno DevAddr to defer\e[0m\n");
            else
                add_database_session(sc, mote->devEui, untilPtr, NULL, NULL, NULL, NULL, NULL, &mote->devAddr, false);
        }

        /* even if new session wasnt added, old sessions are always defunct when lifetime > 0 */
        deleteOldSessions(sc, mote, false);
    } // ..if (lifetime > 0)

    xRStartAnsCallback(sc, false, mote, jobj, rxResult, senderID, rfBuf, rfLen, fMICup, lifetime);
}

static int
sendPRStartReq(mote_t* mote, uint32_t NetID, const uint8_t* PHYPayloadBin, uint8_t PHYPayloadLen)
{
    uint32_t tid;
    int n, ret;
    char destStr[64];
    char buf[512];
    char* strPtr;
    json_object *jobj, *uj;
    char hostname[64];
    CURL* easy;
    int nxfers;

    if (PHYPayloadLen < MINIMUM_PHY_SIZE) {
        printf("\e[31msendPRStartReq PHYPayloadLen %u\e[0m\n", PHYPayloadLen);
        return -1;
    }

    if (next_tid(mote, "sendPRStartReq", PRStartAnsCallback, &tid) < 0) {
        printf("\e[31mprstart-tid-fail\e[0m\n");
        return -1;
    }

    printf("fNS sendPRStartReq() tid %u ", tid);
    uj = generateULMetaData(&mote->ulmd_local, fNS, true);
    if (!uj) {
        printf("\e[31mlocal_ulmd bad\e[0m\n");
        return -1;
    }
    jobj = json_object_new_object();
    json_object_object_add(jobj, ULMetaData, uj);

    sprintf(destStr, "%x", NetID);
    lib_generate_json(jobj, destStr, myNetwork_idStr, tid, PRStartReq, NULL);

    /* PHYPayload */
    buf[0] = 0;
    strPtr = buf;
    for (n = 0; n < PHYPayloadLen; n++) {
        sprintf(strPtr, "%02x", PHYPayloadBin[n]);
        strPtr += 2;
    }
    json_object_object_add(jobj, PHYPayload, json_object_new_string(buf)); // sendPRStartReq

    sprintf(hostname, "%06x.%s", NetID, netIdDomain);

    easy = curl_easy_init();
    if (!easy)
        return CURLE_FAILED_INIT;   // TODO appropriate return
    curl_multi_add_handle(multi_handle, easy);

    ret = http_post_hostname(easy, jobj, hostname, true);
    CURLMcode mc = curl_multi_perform(multi_handle, &nxfers);
    if (mc != CURLM_OK)
        printf(" %s = curl_multi_perform(),%d ", curl_multi_strerror(mc), nxfers);

    return ret;
} // ..sendPRStartReq()

static void
ProfileAnsCallback(MYSQL* sc, mote_t* mote, json_object* jobj, const char* rxResult, const char* senderID, const uint8_t* rfBuf, uint8_t rfLen)
{
    json_object* obj;
    const char* roamingType = NULL;
    time_t until;
    time_t* untilPtr = NULL;
    uint32_t otherNetID;
    uint32_t rxNetID;

    sscanf(senderID, "%x", &otherNetID);

    if (json_object_object_get_ex(jobj, Lifetime, &obj)) {
        until = time(NULL) + json_object_get_int(obj);
        untilPtr = &until;
    }

    printf("ProfileAnsCallback %s ", rxResult);
    if (rxResult != Success) {
        /* failures could be NoRoamingAgreement, UnknownDevEUI, RoamingActDisallowed */
        /* TODO inhibit further ProfileReq until lifetime expires */
        if (untilPtr != NULL) {
            printf("############## roam write deferred #################\n");
            mote_update_database_roam(sc, mote->devEui, mote->devAddr, roamDEFERRED, untilPtr, &otherNetID, NULL);
        }
        return;
    }

    if (json_object_object_get_ex(jobj, RoamingActivationType, &obj)) {
        if (strcmp(json_object_get_string(obj), Passive) == 0)
            roamingType = Passive;
        else if (strcmp(json_object_get_string(obj), Handover) == 0)
            roamingType = Handover;
    } else {
        printf("missing %s\n", RoamingActivationType);
        return;
    }
        

    if (json_object_object_get_ex(jobj, DeviceProfile, &obj)) {
        json_object* ob;
        if (json_object_object_get_ex(jobj, DeviceProfileTimestamp, &ob)) {
            if (saveDeviceProfile(sc, mote, obj, json_object_get_string(ob)) < 0) {
                printf("\e[31mProfileAnsCallback saveDeviceProfile\e[0m\n");
                return;
            }
        }
    }

    sscanf(senderID, "%x", &rxNetID);
    printf("%s ProfileAns -> ", roamingType);
    if (roamingType == Passive) {
        /* send PRStartReq */
        printf("PRStartReq\n");
        if (sendPRStartReq(mote, rxNetID, mote->ULPayloadBin, mote->ULPHYPayloadLen) < 0) {
            printf("\e[31mProfileAnsCallback sendPRStartReq\e[0m\n");
        }
    } else if (roamingType == Handover) {
        printf("HRStartReq\n");
        sNS_sendHRStartReq(sc, mote, rxNetID);
        printf("\e[31mProfileAnsCallback roamingType %s\e[0m\n", roamingType);
    } else
        return;

} // ..ProfileAnsCallback()

static int
sendProfileReq(mote_t* mote, uint32_t NetID)
{
    uint32_t tid;
    int ret;
    char hostname[128];
    char destStr[64];
    json_object *jobj;
    CURL* easy;
    int nxfers;

    printf("sendProfileReq ");
    if (next_tid(mote, "sendProfileReq", ProfileAnsCallback, &tid) < 0) {
        printf("\e[31mprofilesans-tid-fail\e[0m\n");
        return -1;
    }

    jobj = json_object_new_object();

    sprintf(destStr, "%"PRIx64, mote->devEui);
    json_object_object_add(jobj, DevEUI, json_object_new_string(destStr));

    sprintf(destStr, "%x", NetID);
    lib_generate_json(jobj, destStr, myNetwork_idStr, tid, ProfileReq, NULL);

    sprintf(hostname, "%06x.%s", NetID, netIdDomain);

    easy = curl_easy_init();
    if (!easy)
        return CURLE_FAILED_INIT;   // TODO appropriate return
    curl_multi_add_handle(multi_handle, easy);

    ret = http_post_hostname(easy, jobj, hostname, true);
    CURLMcode mc = curl_multi_perform(multi_handle, &nxfers);
    if (mc != CURLM_OK)
        printf(" %s = curl_multi_perform(),%d ", curl_multi_strerror(mc), nxfers);

    return ret;
} // ..sendProfileReq()

static void
HomeNSAnsCallback(MYSQL* sc, mote_t* mote, json_object* jobj, const char* rxResult, const char* senderID, const uint8_t* rfBuf, uint8_t rfLen)
{

    printf("HomeNSAnsCallback ULPHYPayloadLen%u ", mote->ULPHYPayloadLen);

    if (rxResult == Success)
        printf("%s ", rxResult);
    else
        printf("\e[31m%s\e[0m ", rxResult);

    if (rxResult == Success) {
        json_object* obj;
        if (json_object_object_get_ex(jobj, HNetID, &obj)) {
            uint32_t netid;
            sscanf(json_object_get_string(obj), "%x", &netid);

            if ((isNetID(sc, netid, PRAllowed) == 1)) {
                if (sendPRStartReq(mote, netid, mote->ULPayloadBin, mote->ULPHYPayloadLen) < 0) {
                    printf("\e[31mHomeNSAnsCallback sendPRStartReq\e[0m\n");
                }
            } else if ((isNetID(sc, netid, HRAllowed) == 1)) {
                if (deviceProfileReq(sc, mote->devEui, NONE_DEVADDR, "DeviceProfileID", NULL, 0) == 0)
                    sNS_sendHRStartReq(sc, mote, netid);
                else
                    sendProfileReq(mote, netid);
            } else
                printf("neither PRAllowed or HRAllowed ");
        } // ..have HNetID
    } // ..Success

} // ..HomeNSAnsCallback()

static void
sendHomeNSReq(mote_t* mote, uint64_t rxDevEui, uint64_t rxJoinEui)
{
    CURL* easy;
    uint32_t tid;
    char hostname[64];
    char destStr[64];
    json_object *jobj = json_object_new_object();

    printf("sendHomeNSReq ULPHYPayloadLen%u\n", mote->ULPHYPayloadLen);

    if (next_tid(mote, "sendHomeNSReq", HomeNSAnsCallback, &tid) < 0) {
        printf("\e[31mhomensreq-tid-fail\e[0m\n");
        return;
    }

    sprintf(destStr, "%"PRIx64, rxDevEui);
    json_object_object_add(jobj, DevEUI, json_object_new_string(destStr));

    sprintf(destStr, "%"PRIx64, rxJoinEui);
    lib_generate_json(jobj, destStr, myNetwork_idStr, tid, HomeNSReq, NULL);

    printf("toJS: %s ", json_object_to_json_string(jobj));

    easy = curl_easy_init();
    if (!easy)
        return;   // TODO appropriate return
    curl_multi_add_handle(multi_handle, easy);

    getJsHostName(rxJoinEui, hostname, joinDomain);
    http_post_hostname(easy, jobj, hostname, true);

} // ..sendHomeNSReq()


static bool
activate(mote_t* mote, uint64_t rxDevEui, uint64_t rxJoinEui)
{
    uint32_t fwdto_netId;
    const char* result;

    mote->devEui = rxDevEui;
    MAC_PRINTF(" %016"PRIx64" ", rxDevEui);

    /* fwdToNetID is the netID this mote belongs to (NULL for this NS) */
    result = sql_motes_query_item(sqlConn_lora_network, mote->devEui, NONE_DEVADDR, "fwdToNetID", &fwdto_netId);
    if (result != Success) {
        printf("\e[31mJ%s = sql_motes_query_item()\e[0m\n", result);
        return true;    // fail, skip jsonFinish
    }
    printf(" fwdToNetID %06x ", fwdto_netId);
    if (fwdto_netId == NONE_NETID) {
        /* this mote provisioned on other network, need to get its NetID */
        /* do HomeNSReq, then ProfileReq */
        sendHomeNSReq(mote, mote->devEui, rxJoinEui);
        // not until roaming start request is sent -- mote->ULPHYPayloadLen = 0;
        return true;    // sent out, skip jsonFinish
    } else if (fwdto_netId != myNetwork_id32) {
        /* this mote provisioned on other network, already have its NetID */

        /* if both PRAllowed and HRAllowed on fwdto_netId,
         * we need to get DeviceProfile to tell us which one to use */

        if ((isNetID(sqlConn_lora_network, fwdto_netId, PRAllowed) == 1)) {
            if (sendPRStartReq(mote, fwdto_netId, mote->ULPayloadBin, mote->ULPHYPayloadLen) < 0) {
                printf("\e[31mactivate sendPRStartReq\e[0m\n");
            }
            mote->ULPHYPayloadLen = 0;
        } else if ((isNetID(sqlConn_lora_network, fwdto_netId, HRAllowed) == 1)) {
            if (deviceProfileReq(sqlConn_lora_network, mote->devEui, NONE_DEVADDR, "DeviceProfileID", NULL, 0) == 0)
                sNS_sendHRStartReq(sqlConn_lora_network, mote, fwdto_netId);
            else
                sendProfileReq(mote, fwdto_netId);

            mote->ULPHYPayloadLen = 0;
        } else
            MAC_PRINTF("neither PRAllowed or HRAllowed ");

        return true;  // ED on foreign net, wont be getting any json for this ED
    }

    return false;
}


/* return true to skip, finish now (dont run jsonFinish) */
bool fNS_uplink_finish(mote_t* mote, sql_t* sql, bool* discard)
{
    mhdr_t* mhdr;

    /* can run in NONE, fPASSIVE, sHANDOVER
       cant run in DEFERRED, sPASSIVE, hHANDOVER */

    if (sql->roamState != roamNONE && sql->roamState != roamsHANDOVER && sql->roamState != roamfPASSIVE) {
        /* state is DEFERRED or sPASSIVE or hHANDOVER: not operating as fNS */
        printf("\e[31mfNS_uplink_finish in %s\e[0m\n", sql->roamState);
        return false;
    }

    /* here: this NS is sNS for this ED (if provisioned) */
    
    mhdr = (mhdr_t*)mote->ULPayloadBin;

    printf(" fNS-");
    print_mtype(mhdr->bits.MType);
    if (mhdr->bits.MType == MTYPE_REJOIN_REQ) {
        // any rejoin request received during passive fNS should be taken as restart of roaming 
        rejoin02_req_t* r2 = (rejoin02_req_t*)mote->ULPayloadBin;
        printf(" type%u ", r2->type);
        if (r2->type == 2 || r2->type == 0) {
            /* type0 page 31 */
            uint32_t netid;
            netid = r2->NetID[0];
            netid |= r2->NetID[1] << 8;
            netid |= r2->NetID[2] << 16;

            if ((r2->type == 2 || r2->type == 0)) {
                if (netid != myNetwork_id32) {
                    printf(" %016"PRIx64" netid:%06x ", mote->devEui, netid);
                    /* any rejoin request received during passive fNS should be taken as restart of roaming  */
                    if ((isNetID(sqlConn_lora_network, netid, PRAllowed) == 1)) {
                        if (sendPRStartReq(mote, netid, mote->ULPayloadBin, mote->ULPHYPayloadLen) < 0) {
                            printf("\e[31mfNS_uplink_finish sendPRStartReq\e[0m\n");
                        }
                    } else if (isNetID(sqlConn_lora_network, netid, HRAllowed) == 1) {
                        if (deviceProfileReq(sqlConn_lora_network, mote->devEui, NONE_DEVADDR, "DeviceProfileID", NULL, 0) == 0)
                            sNS_sendHRStartReq(sqlConn_lora_network, mote, netid);
                        else
                            sendProfileReq(mote, netid);
                    } else {
                        printf("%s\n", NoRoamingAgreement);
                        *discard = true;
                    }
                    mote->ULPHYPayloadLen = 0;
                    return true;/* dont take this in our sNS */
                } else {
                    printf("\e[33mfNS-type%u on this NS\e[0m\n", r2->type);
                }
            }
        } else if (r2->type == 1) {
            /* restore connectivity */
            /* If the NS is not acting as the sNS for the End-Device,
             * then the NS SHALL treat the incoming Rejoin-request Type 1
             * exactly same way as it would process a Join-request. */
            /* Acting as sNS during roamNONE and roamsHANDOVER */
            if (sql->roamState == roamNONE || sql->roamState != roamsHANDOVER) {
                rejoin1_req_t *rj1 = (rejoin1_req_t*)mote->ULPayloadBin;
                /* this NS is sNS for this ED */
                //printf("\e[31mfNS-rejoin1-TODO\e[0m\n");
                bool ret = activate(mote, eui_buf_to_uint64(rj1->DevEUI), eui_buf_to_uint64(rj1->JoinEUI));
                if (ret) {
                    printf("rejoin1-skipping-jsonFinish ");
                    return true;
                }
                printf("rejoin1->jsonFinish ");
            }
        }
    } else if (mhdr->bits.MType == MTYPE_JOIN_REQ) {
        join_req_t* rx_jreq_ptr = (join_req_t*)mote->ULPayloadBin;

        bool ret = activate(mote, eui_buf_to_uint64(rx_jreq_ptr->DevEUI), eui_buf_to_uint64(rx_jreq_ptr->JoinEUI));
        if (ret)
            return true;

        /* JoinReq: this ED is home on this NS */
        MAC_PRINTF("fNS-my-net ");

        /* this mote provisioned on this network, completed in sNS at json-finish */
    } else if (mhdr->bits.MType == MTYPE_UNCONF_UP || mhdr->bits.MType == MTYPE_CONF_UP) {
        const fhdr_t *rx_fhdr = (fhdr_t*)&mote->ULPayloadBin[1];

        if (sql->roamState == roamNONE) {
            uint32_t NwkID, rxNetID, NwkAddr;
            MAC_PRINTF("roamingOFF ");
            if (parseDevAddr(rx_fhdr->DevAddr, &rxNetID, &NwkID, &NwkAddr) < 0) {
                printf("\e[31mbad DevAddr %08x\e[0m\n", rx_fhdr->DevAddr);
                return true;   // Other
            }
            MAC_PRINTF("fNS %08x: NwkID=%x, rxNetID=%06x NwkAddr:%x\n", rx_fhdr->DevAddr, NwkID, rxNetID, NwkAddr);
            if (rxNetID != myNetwork_id32) {
                /* mote from some other network */
                if (isNetID(sqlConn_lora_network, rxNetID, PRAllowed) == 1) {
                    if (sendPRStartReq(mote, rxNetID, mote->ULPayloadBin, mote->ULPHYPayloadLen) < 0) {
                        printf("\e[31mfNS_uplink_finish sendPRStartReq\e[0m\n");
                    }
                } else if (isNetID(sqlConn_lora_network, rxNetID, HRAllowed) == 1) {
                    if (deviceProfileReq(sqlConn_lora_network, mote->devEui, mote->devAddr, "DeviceProfileID", NULL, 0) == 0)
                        sNS_sendHRStartReq(sqlConn_lora_network, mote, rxNetID);
                    else
                        sendProfileReq(mote, rxNetID);
                } else {
                    printf("%s\n", NoRoamingAgreement);
                    *discard = true;
                }
                //TODO this mote: DevRoamingDisallowed;
                mote->ULPHYPayloadLen = 0;
                return true;/* dont take this in our sNS */
            }
        }

        if (sql->OptNeg && !(sql->roamState == roamfPASSIVE && !sql->enable_fNS_MIC)) {
            uint32_t fCntUp;
            bool ok;
ftry:
            fCntUp = (mote->session.FCntUp & 0xffff0000) | rx_fhdr->FCnt;
            ok = check_cmacF(mote->ULPayloadBin, mote->ULPHYPayloadLen, fCntUp, mote->session.FNwkSIntKeyBin);
            if (!ok) {
                printf("\e[31mcmacF-bad nth%u ", mote->nth);
                print_buf(mote->session.FNwkSIntKeyBin, 16, "FNwkSIntKey");
                if (mote->session.next) {
                    if (getSession(sqlConn_lora_network, NONE_DEVEUI, mote->devAddr, ++mote->nth, &mote->session) < 0) {
                        return true;    // skip jsonFinish
                    }
                    printf("next\e[0m ");
                    goto ftry;
                }
                printf("last\e[0m\n");
                return true;    // skip jsonFinish
            } else if (mote->nth > 0)
                printf("\e[31mcmacF-ok on %u\e[0m\n", mote->nth);
        }

        if (sql->roamState == roamsHANDOVER) {
            /* this NS has MAC-layer control over this ED */
            const char* uplinkResult = sNS_uplink(mote, sql, &mote->ulmd_local, discard);
            /* sNS_uplink() null return when result expected later from json answer */
            if (uplinkResult && uplinkResult != Success)
                printf("\e[31mfNS %s = sNS_uplink()\e[0m\n", uplinkResult);
            return false;   // any FRMPayload downlinks will be sent at jsonFinish
        } else if (sql->roamState == roamfPASSIVE) {
            const char* result = fNS_XmitDataReq_uplink(mote, sql);
            if (result != Success)
                printf("\e[31m");
            printf(" %s = fNS_XmitDataReq_uplink()\e[0m\n", result);
            mote->ULPHYPayloadLen = 0;
            return true;    // sent out, skip jsonFinish
        }

    } // ..else if (mhdr->bits.MType == MTYPE_UNCONF_UP || mhdr->bits.MType == MTYPE_CONF_UP)

    return false;
} // ..fNS_uplink_finish()

void
fNS_beacon_service(mote_t* mote)
{
    f_t* f = mote->f;

    if (!f)
        return;

    if (f->beaconDeferred) {
        sql_t sql;
        const char *result, *sqlResult;
        uint16_t ping_offset;   /**< time offset in beacon period */
        f->beaconDeferred = false;

        LoRaMacBeaconComputePingOffset(f->gw->seconds_at_beacon, mote->devAddr, f->PingPeriod, &ping_offset);
        printf("beacon deferred, ping_offset:%u\n", ping_offset);

        uint32_t ping_offset_us = ping_offset * 30000;
        float ping_offset_s = ping_offset * 0.03;
        f->tx_pkt.count_us = f->gw->trigcnt_pingslot_zero;
        f->tx_pkt.count_us += ping_offset_us + (f->gw->sx1301_ppm_err * ping_offset_s);

        if (gateway_id_write_downlink(f->gw->eui, &f->tx_pkt) < 0)
            result = XmitFailed;
        else
            result = Success;

        sqlResult = sql_motes_query(sqlConn_lora_network, mote->devEui, mote->devAddr, &sql);
        if (sqlResult == Success) {
            printf(" roamState:%s\n", sql.roamState);
            if (sql.roamState == roamfPASSIVE) {
                json_object* aj = json_object_new_object();
                sendXmitDataAns(false, aj, f->clientID, f->reqTid, result);
                free(f->clientID);
                //printf("\e[31mTODO send %s for deferred class-B\e[0m\n", result);
            } else
                answer_app_downlink(mote, result, NULL);
        }
    }
} // ..fNS_beacon_service()

