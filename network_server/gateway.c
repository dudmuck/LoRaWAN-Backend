/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

#include "gw_server.h"
#include "fNS.h"

//#define BEACON_DEBUG

#ifdef BEACON_DEBUG
    #define BEACON_PRINTF(...)     printf(__VA_ARGS__)
#else
    #define BEACON_PRINTF(...)
#endif

bool fNS_enable = true;

CURLM *multi_handle;

struct _mote_list* mote_list = NULL;
MYSQL *sqlConn_lora_network;

uint32_t myNetwork_id32;
char myNetwork_idStr[24];
char mq_name[24];
uint32_t myVendorID;
uint8_t dl_rxwin;
unsigned leap_seconds;
key_envelope_t key_envelope_ns_js;

unsigned int tcp_listen_port = 5555;   /**< TODO:json-conf::: gateway connections listen port */
#define MAXMSG  512 /**< gateway TCP read buffer size */

#define BEACON_PERIOD_US        128000000   /**< microseconds between beacon start */

char regionPath[128];
struct _region_list* region_list = NULL; /**< list of possible gateway regions */

char joinDomain[64];
char netIdDomain[64];


struct _gateway_list* gateway_list = NULL; /**< list of active gateways */

/** @brief create TCP listening socket for incoming gateway connections
 * @return -1 for failure or socket file descriptor
 */
int
make_socket (uint16_t port)
{
    int yes = 1;
    int sock;
    struct sockaddr_in name;

    /* Create the socket. */
    sock = socket (PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror ("\e[31msocket\e[0m");
        return -1;
    }

    if ( setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 )
    {
        perror("\e[31msetsockopt\e[0m");
    }

    /* Give the socket a name. */
    name.sin_family = AF_INET;
    name.sin_port = htons (port);
    name.sin_addr.s_addr = htonl (INADDR_ANY);
    do {
        if (bind (sock, (struct sockaddr *) &name, sizeof (name)) == 0)
        {
            break;  /* ok */
        } else if (errno != EADDRINUSE) {
            fprintf(stderr, "bind port %u errno:%d\n", port, errno);
            perror ("\e[31mbind\e[0m");
            return -1;
        } else {
            printf("EADDRINUSE retry\n");
            sleep(1);
        }
    } while (errno == EADDRINUSE);
/*    if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
    {
        fprintf(stderr, "bind port %u errno:%d\n", port, errno);
        perror ("bind");
        return -1;
    }*/

    return sock;
}

/** @brief load region configuration of gateway from JSON file
 * @param rp pointer to region of gateway
 * @param file_buf JSON config buffer to be parsed
 */
void parse_region_config(regional_t* rp, const char * file_buf)
{
    enum json_tokener_error jerr;
    json_object *jobj, *jobjSrv, *obj;
    struct json_tokener *tok = json_tokener_new();
    int i, len;

    do {
        jobj = json_tokener_parse_ex(tok, file_buf, strlen(file_buf));
    } while ((jerr = json_tokener_get_error(tok)) == json_tokener_continue);

    if (jerr != json_tokener_success) {
        printf("parse_region_config json error: %s\n", json_tokener_error_desc(jerr));
        goto prEnd;
    }
    if (tok->char_offset < strlen(file_buf)) {
        printf("json region extra chars\n");
    }


    /************** parse "lora_server" section ************/
    if (!json_object_object_get_ex(jobj, "lora_server", &jobjSrv)) {
        printf("json conf no lora_server\n");
        goto prEnd;
    }

    if (json_object_object_get_ex(jobjSrv, "rx2_freq_hz", &obj)) {
        rp->defaults.Rx2Channel.FrequencyHz = json_object_get_int(obj);
        printf("Rx2Channel.FrequencyHz%u\n", rp->defaults.Rx2Channel.FrequencyHz);
    }

    if (json_object_object_get_ex(jobjSrv, "rx2_dr", &obj)) {
        rp->defaults.Rx2Channel.Datarate = json_object_get_int(obj);
        printf("Rx2Channel.Datarate%u\n", rp->defaults.Rx2Channel.Datarate);
    }

    if (json_object_object_get_ex(jobjSrv, "TxParamSetup", &obj)) {
        json_object* jo;
        rp->enableTxParamSetup = true;
        if (json_object_object_get_ex(obj, "DownlinkDwellTime", &jo))
            rp->TxParamSetup.bits.DownlinkDwellTime = json_object_get_int(jo);
        if (json_object_object_get_ex(obj, "UplinkDwellTime", &jo))
            rp->TxParamSetup.bits.UplinkDwellTime = json_object_get_int(jo);
        if (json_object_object_get_ex(obj, "MaxEIRP", &jo))
            rp->TxParamSetup.bits.MaxEIRP = json_object_get_int(jo);
    } else
        rp->enableTxParamSetup = false;

    if (json_object_object_get_ex(jobjSrv, "data_rates", &obj)) {
        rp->num_datarates = json_object_array_length(obj);
        for (i = 0; i < rp->num_datarates; i++) {
            int sf = 0, fdev_khz = 0;
            json_object *o, *ajo = json_object_array_get_idx(obj, i);
            if (json_object_object_get_ex(ajo, "sf", &o)) {
                sf = json_object_get_int(o);
                switch (sf) {
                    case 7: rp->datarates[i].lgw_sf_bps = DR_LORA_SF7; break;
                    case 8: rp->datarates[i].lgw_sf_bps = DR_LORA_SF8; break;
                    case 9: rp->datarates[i].lgw_sf_bps = DR_LORA_SF9     ; break;
                    case 10: rp->datarates[i].lgw_sf_bps = DR_LORA_SF10    ; break;
                    case 11: rp->datarates[i].lgw_sf_bps = DR_LORA_SF11    ; break;
                    case 12: rp->datarates[i].lgw_sf_bps = DR_LORA_SF12    ; break;
                    default: rp->datarates[i].lgw_sf_bps = DR_LORA_MULTI   ; break;
                }
                printf("sf%u ", sf);
            }
            if (json_object_object_get_ex(ajo, "fdev_khz", &o)) {
                fdev_khz = json_object_get_int(o);
            }
            if (fdev_khz == 0) {
                if (json_object_object_get_ex(ajo, "bw", &o)) {
                    int bw = json_object_get_int(o);
                    switch (bw) {
                        case 7: case 8:   rp->datarates[i].lgw_bw = BW_7K8HZ; break;
                        case 15: case 16: rp->datarates[i].lgw_bw = BW_15K6HZ; break;
                        case 31:          rp->datarates[i].lgw_bw = BW_31K2HZ; break;
                        case 62: case 63: rp->datarates[i].lgw_bw = BW_62K5HZ; break;
                        case 125:         rp->datarates[i].lgw_bw = BW_125KHZ; break;
                        case 250:         rp->datarates[i].lgw_bw = BW_250KHZ; break;
                        case 500:         rp->datarates[i].lgw_bw = BW_500KHZ; break;
                        default:          rp->datarates[i].lgw_bw = BW_UNDEFINED; break;
                    }
                    printf("bw%u ", bw);
                }
            } else {
                rp->datarates[i].lgw_sf_bps = sf;
                rp->datarates[i].fdev_khz = fdev_khz;
                printf("fdev:%uKHz %ubps\n", rp->datarates[i].fdev_khz, rp->datarates[i].lgw_sf_bps);
            }
            printf("\n");
        } // ..for()
    } // ..data_rates

    if (json_object_object_get_ex(jobjSrv, "uplink_dr_max", &obj)) {
        rp->defaults.uplink_dr_max = json_object_get_int(obj);
        printf("uplink_dr_max %u\n", rp->defaults.uplink_dr_max );
    }

    if (json_object_object_get_ex(jobjSrv, "dl_tx_dbm", &obj)) {
        rp->dl_tx_dbm = json_object_get_int(obj);
        printf("dl_tx_dbm %u\n", rp->dl_tx_dbm );
    }

    if (json_object_object_get_ex(jobjSrv, "ping_freq_hz", &obj)) {
        rp->defaults.ping_freq_hz = json_object_get_int(obj);
        printf("ping_freq_hz %u\n", rp->defaults.ping_freq_hz );
    }

    if (json_object_object_get_ex(jobjSrv, "ping_dr", &obj)) {
        rp->defaults.ping_dr = json_object_get_int(obj);
        printf("ping_dr %u\n", rp->defaults.ping_dr );
    }

    if (json_object_object_get_ex(jobjSrv, "default_txpwr_idx", &obj)) {
        rp->default_txpwr_idx = json_object_get_int(obj);
        printf("default_txpwr_idx %u\n", rp->default_txpwr_idx );
    }

    if (json_object_object_get_ex(jobjSrv, "max_txpwr_idx", &obj)) {
        rp->max_txpwr_idx = json_object_get_int(obj);
        printf("max_txpwr_idx %u\n", rp->max_txpwr_idx );
    }

    if (json_object_object_get_ex(jobjSrv, "Rx1DrOffset", &obj)) {
        rp->defaults.Rx1DrOffset = json_object_get_int(obj);
        printf("Rx1DrOffset %u\n", rp->defaults.Rx1DrOffset);
    }

    if (json_object_object_get_ex(jobjSrv, "init_chmasks", &obj)) {
        len = json_object_array_length(obj);
        for (i = 0; i < len; i++) {
            unsigned n;
            json_object *ajo = json_object_array_get_idx(obj, i);
            sscanf(json_object_get_string(ajo), "%x", &n);
            rp->init_ChMask[i] = n;
            printf("ChMask[%u]:%04x\n", i, rp->init_ChMask[i]);
        }
    }

    /************** parse "lorawan" section ************/
    if (!json_object_object_get_ex(jobj, "lorawan", &jobjSrv)) {
        printf("json conf no lora_server\n");
        goto prEnd;
    }

    if (json_object_object_get_ex(jobjSrv, "beacon_hz", &obj)) {
        rp->beacon_hz = json_object_get_int(obj);
        printf("beacon_hz %u\n", rp->beacon_hz );
    }


prEnd:
    if (tok)
        json_tokener_free(tok);
}


/** @brief how much time elapsed (our host time) since uplink received from gateway */
float
get_time_since_gw_read(gateway_t* gw)
{
    struct timespec now;

    if (clock_gettime (CLOCK_REALTIME, &now) == -1)
        perror ("clock_gettime");

    return difftimespec(now, gw->read_host_time);
}

/** @brief convert BW_* to Hz
 */
int32_t lgw_bw_getval(int x) {
    switch (x) {
        case BW_500KHZ: return 500000;
        case BW_250KHZ: return 250000;
        case BW_125KHZ: return 125000;
        case BW_62K5HZ: return 62500;
        case BW_31K2HZ: return 31200;
        case BW_15K6HZ: return 15600;
        case BW_7K8HZ : return 7800;
        default: return -1;
    }
}

/** @brief convert DR_LORA_* to spreading factor 7 to 12
 */
int32_t lgw_sf_getval(int x) {
    switch (x) {
        case DR_LORA_SF7: return 7;
        case DR_LORA_SF8: return 8;
        case DR_LORA_SF9: return 9;
        case DR_LORA_SF10: return 10;
        case DR_LORA_SF11: return 11;
        case DR_LORA_SF12: return 12;
        default: return -1;
    }
}

uint8_t fsk_sync_word_size = 3; /**< length of FSK SFD in octets */
/** @brief get duration of packet
 * @param rx type of vp struct
 * @param vp lgw_pkt_rx_s struct or lgw_pkt_tx_s struct
 */
uint32_t lgw_time_on_air_us(bool rx, const void* vp)
{
    int32_t val;
    uint8_t SF, H, DE;
    float BW;
    uint32_t payloadSymbNb, Tpacket;
    double Tsym, Tpreamble, Tpayload, Tfsk;
    const struct lgw_pkt_rx_s* rx_pkt = NULL;
    const struct lgw_pkt_tx_s* tx_pkt = NULL;
    uint8_t     modulation;     /*!> modulation used by the packet */
    uint8_t     bandwidth;      /*!> modulation bandwidth (LoRa only) */
    uint32_t    datarate;       /*!> RX datarate of the packet (SF for LoRa) */
    uint16_t    preamble;       /*!> set the preamble length, 0 for default */
    bool        no_header;      /*!> if true, enable implicit header mode (LoRa), fixed length (FSK) */
    bool        no_crc;         /*!> if true, do not send a CRC in the packet */
    uint16_t    size;           /*!> payload size in bytes */
    uint8_t     coderate;       /*!> error-correcting code of the packet (LoRa only) */

    if (vp == NULL) {
        fprintf(stderr, "ERROR: Failed to compute time on air, wrong parameter\n");
        return 0;
    }

    if (rx) {
        rx_pkt = vp;
        modulation = rx_pkt->modulation;
        bandwidth = rx_pkt->bandwidth;
        datarate = rx_pkt->datarate;
        preamble = STD_LORA_PREAMB;
        no_header = false;
        no_crc = false;
        size = rx_pkt->size;
        coderate = rx_pkt->coderate;
    } else {
        tx_pkt = vp;
        modulation = tx_pkt->modulation;
        bandwidth = tx_pkt->bandwidth;
        datarate = tx_pkt->datarate;
        preamble = tx_pkt->preamble;
        no_header = tx_pkt->no_header;
        no_crc = tx_pkt->no_crc;
        size = tx_pkt->size;
        coderate = tx_pkt->coderate;
    }

    if (modulation == MOD_LORA) {
        /* Get bandwidth */
        val = lgw_bw_getval(bandwidth);
        if (val != -1) {
            BW = val / (float)1e6;
        } else {
            fprintf(stderr, "ERROR: Cannot compute time on air for this packet, unsupported bandwidth (0x%02X)\n", bandwidth);
            return 0;
        }

        /* Get datarate */
        val = lgw_sf_getval(datarate);
        if (val != -1) {
            SF = (uint8_t)val;
        } else {
            fprintf(stderr, "ERROR: Cannot compute time on air for this packet, unsupported datarate (0x%02X)\n", datarate);
            return 0;
        }

        /* Duration of 1 symbol */
        Tsym = pow(2, SF) / BW;

        /* Duration of preamble */
        //Tpreamble = (8 + 4.25) * Tsym; /* 8 programmed symbols in preamble */
        Tpreamble = (preamble + 4.25) * Tsym; /* 8 programmed symbols in preamble */

        /* Duration of payload */
        //H = (packet->no_header==false) ? 0 : 1; /* header is always enabled, except for beacons */
        H = no_header ? 1 : 0; /* header is always enabled, except for beacons */
        DE = (SF >= 11) ? 1 : 0; /* Low datarate optimization enabled for SF11 and SF12 */
        uint8_t crcOn = no_crc ? 0: 1;

        payloadSymbNb = 8 + (ceil((double)(8*size - 4*SF + 28 + 16*crcOn - 20*H) / (double)(4*(SF - 2*DE))) * (coderate + 4)); /* Explicitely cast to double to keep precision of the division */

        Tpayload = payloadSymbNb * Tsym;

        /* Duration of packet */
        Tpacket = Tpreamble + Tpayload;
    } else if (modulation == MOD_FSK) {
        /* PREAMBLE + SYNC_WORD + PKT_LEN + PKT_PAYLOAD + CRC
                PREAMBLE: default 5 bytes
                SYNC_WORD: default 3 bytes
                PKT_LEN: 1 byte (variable length mode)
                PKT_PAYLOAD: x bytes
                CRC: 0 or 2 bytes
        */
        Tfsk = (8 * (double)(preamble + fsk_sync_word_size + 1 + size + ((no_crc == true) ? 0 : 2)) / (double)datarate);

        /* Duration of packet */
        Tpacket = (uint32_t)Tfsk + 1; /* add margin for rounding */
    } else {
        Tpacket = 0;
        fprintf(stderr, "ERROR: Cannot compute time on air for this packet, unsupported modulation (0x%02X)\n", modulation);
    }

    return Tpacket;
}


/** @brief send downlink packet for gateway to transmit
 * @param gw gateway to send to
 * @param tx_pkt packet to transmit
 * @return 0 for ok, 1 for failure
 */
int gateway_write_downlink(gateway_t* gw, const struct lgw_pkt_tx_s* tx_pkt)
{
    int wlen;
    uint32_t* u32_ptr;
    uint16_t* u16_ptr;
    unsigned int buf_idx = 0;
    uint8_t user_buf[sizeof(struct lgw_pkt_tx_s)];
#if 0
    uint32_t    freq_hz;        /*!> center frequency of TX */
    uint8_t     tx_mode;        /*!> select on what event/time the TX is triggered */
    uint32_t    count_us;       /*!> timestamp or delay in microseconds for TX trigger */
    uint8_t     rf_chain;       /*!> through which RF chain will the packet be sent */
    int8_t      rf_power;       /*!> TX power, in dBm */
    uint8_t     modulation;     /*!> modulation to use for the packet */
    uint8_t     bandwidth;      /*!> modulation bandwidth (LoRa only) */
    uint32_t    datarate;       /*!> TX datarate (baudrate for FSK, SF for LoRa) */
    uint8_t     coderate;       /*!> error-correcting code of the packet (LoRa only) */
    bool        invert_pol;     /*!> invert signal polarity, for orthogonal downlinks (LoRa only) */
    uint8_t     f_dev;          /*!> frequency deviation, in kHz (FSK only) */
    uint16_t    preamble;       /*!> set the preamble length, 0 for default */
    bool        no_crc;         /*!> if true, do not send a CRC in the packet */
    bool        no_header;      /*!> if true, enable implicit header mode (LoRa), fixed length (FSK) */
    uint16_t    size;           /*!> payload size in bytes */
    uint8_t     payload[256];   /*!> buffer containing the payload */
#endif /* #if 0 */
    user_buf[buf_idx++] = DOWNLINK_TX;
    user_buf[buf_idx++] = 0;    // for length-lo
    user_buf[buf_idx++] = 0;    // for length-hi

    u32_ptr = (uint32_t*)&user_buf[buf_idx];
    *u32_ptr = tx_pkt->freq_hz;
    RF_PRINTF("\e[32mgateway_write_downlink() size%u %uhz to fd %d, modem%u, since uplink:%f ", tx_pkt->size, *u32_ptr, gw->fd, tx_pkt->modem_idx, get_time_since_gw_read(gw));
    buf_idx += sizeof(tx_pkt->freq_hz);

    user_buf[buf_idx++] = tx_pkt->tx_mode;

    u32_ptr = (uint32_t*)&user_buf[buf_idx];
    *u32_ptr = tx_pkt->count_us;
    buf_idx += sizeof(tx_pkt->count_us);

    user_buf[buf_idx++] = 0xff; /* written by gateway tx_pkt->rf_chain  */
    user_buf[buf_idx++] = tx_pkt->rf_power;
    user_buf[buf_idx++] = tx_pkt->modulation;
    user_buf[buf_idx++] = tx_pkt->bandwidth;

    u32_ptr = (uint32_t*)&user_buf[buf_idx];
    *u32_ptr = tx_pkt->datarate;
    buf_idx += sizeof(tx_pkt->datarate);

    user_buf[buf_idx++] = tx_pkt->coderate;
    user_buf[buf_idx++] = tx_pkt->invert_pol;
    user_buf[buf_idx++] = tx_pkt->f_dev;

    u16_ptr = (uint16_t*)&user_buf[buf_idx];
    *u16_ptr = tx_pkt->preamble;
    buf_idx += sizeof(tx_pkt->preamble);

    user_buf[buf_idx++] = tx_pkt->no_crc;
    user_buf[buf_idx++] = tx_pkt->no_header;

    u16_ptr = (uint16_t*)&user_buf[buf_idx];
    *u16_ptr = tx_pkt->size;
    buf_idx += sizeof(tx_pkt->size);

    if (tx_pkt->size > sizeof(tx_pkt->payload)) {
        printf("\e[31mtx_pkt size too large %u\e[0m\n", tx_pkt->size);
        return 1;   //1:fail
    }

    memcpy(&user_buf[buf_idx], tx_pkt->payload, tx_pkt->size);
    buf_idx += tx_pkt->size;

    user_buf[buf_idx++] = tx_pkt->modem_idx;

    // end
    user_buf[1] = buf_idx & 0xff;
    user_buf[2] = buf_idx >> 8;

    wlen = write(gw->fd, user_buf, buf_idx);
    if (wlen < 0) {
        printf("\e[31merrno:%d fd:%d\e[0m\n", errno, gw->fd);
        perror("\e[31mgw-write\e[0m");
        return 1;   // 1:fail
    } else if (wlen != buf_idx) {
        printf("\e[31mgw write len %d vs %d\e[0m\n", wlen, buf_idx);
        return 1;   // 1:fail
    } else
        printf("%d = write(,,%d)\e[0m\n", wlen, buf_idx);

    gw->count_us_tx_start = tx_pkt->count_us;
    gw->count_us_tx_end = tx_pkt->count_us + lgw_time_on_air_us(false, tx_pkt);

    //network_server_log_pkt(gw, tx_pkt, true);

    return 0;   // 0:ok
}

int gateway_id_write_downlink(uint64_t eui, const struct lgw_pkt_tx_s* tx_pkt)
{
    struct _gateway_list* gl;

    for (gl = gateway_list; gl != NULL; gl = gl->next) {
        if (gl->gateway.eui == eui) {
            return gateway_write_downlink(&gl->gateway, tx_pkt);
        }
    }

    return -1;
}

/** @brief send JSON configuration to gateway
 * @param gateway pointer to gateway
 */
int send_gateway_config(gateway_t* gateway)
{
    int wlen, len, fd;
    char fileNamePath[256];
    char buf[8192];
    struct _region_list* rl;
    region_t* region = NULL;
    const char *fn = NULL;

    for (rl = region_list; rl != NULL; rl = rl->next) {
        region = &rl->region;
        if (region->RFRegion == gateway->RFRegion) {
            if (gateway->num_modems == 1)
                fn = region->conf_file_8ch;
            else if (gateway->num_modems == 2)
                fn = region->conf_file_16ch;
            else {
                fprintf(stderr, "region %s: unsupported num_modems %u\n", region->RFRegion, gateway->num_modems);
                return -1;
            }
            break;
        }
    }

    if (!fn) {
        printf("no file for region %s\n", gateway->RFRegion);
        return -1;
    }

    sprintf(fileNamePath, "%s/%s", regionPath, fn);
    fd = open(fileNamePath, O_RDONLY, 0);
    if (fd < 0) {
        perror(fileNamePath);
        return -1;
    }
    printf("opened file %s\n", fileNamePath);

    len = read(fd, buf, sizeof(buf));
    if (len < 0) {
        perror("\e[31mfile-read\e[0m");
        return -1;
    }
    if (sizeof(buf) == len) {
        printf("buffer to small: read len=%d, buf size=%zu\n", len, sizeof(buf));
        close(fd);
        return -1;
    }

    /* send length */
    wlen = write(gateway->fd, &len, sizeof(len));
    if (wlen < 0) {
        perror("\e[31mgw-write\e[0m");
    } else
        printf("gw write len %d\n", wlen);

    /* send file */
    wlen = write(gateway->fd, buf, len);
    if (wlen < 0) {
        perror("\e[31mgw-write\e[0m");
    } else {
        printf("gw write returned:%d vs %d\n", wlen, len);
    }

    /* get server-side band-specific configuration for this gateway */
    //parse_region_config(&gateway->regional, buf);
    gateway->rp = &region->regional;
    printf("gateway %"PRIx64" config'd\n", gateway->eui);

    close(fd);

    return 0;
}

/** @brief add new gateway onto list of gateways
 * @param gl_ptr pointer to gateway list
 * @param eui EUI of new gateway
 * @param fd file descriptor of connection to gateway
 */
void create_gateway(struct _gateway_list** gl_ptr, uint64_t eui, int fd)
{
    struct _gateway_list* gl;

    *gl_ptr = malloc(sizeof(struct _gateway_list));
    memset(*gl_ptr, 0, sizeof(struct _gateway_list));

    gl = *gl_ptr;
    gl->gateway.eui = eui;
    gl->gateway.fd = fd;
    sprintf(gl->gateway.eui_hex_str, "%" PRIx64, eui);
    printf("gateway eui hex:%s\n", gl->gateway.eui_hex_str);
}

/** @brief time captured at first beacon indication, zero for indication complete */
struct timespec beacon_indication_start_time = { 0 };

/** @brief process beacon indication from gateway
 * @param gateway pointer to gateway
 * @param tstamp sx1301 counter captured at beacon TX start
 * @param secs time value sent over the air
 * @param host_time our host time at beacon indication
 */
void
beacon_indication(gateway_t* gateway, uint32_t tstamp, uint32_t secs, struct timespec* host_time)
{
    BEACON_PRINTF("beacon_indication(%" PRIx64 ", %u, %u)\n", gateway->eui, tstamp, secs);

    gateway->new_indication = true;

    if (secs == 0xffffffff) {
        BEACON_PRINTF("%"PRIx64" lost GPS fix\n", gateway->eui);
        gateway->have_gps_fix = false;
        gateway->tstamp_at_beacon = 0;
        gateway->seconds_at_beacon = 0;
        return;
    }

    if (gateway->tstamp_at_beacon != 0 && gateway->seconds_at_beacon != 0) {
        uint32_t measured_us = tstamp - gateway->tstamp_at_beacon;
        BEACON_PRINTF("measured_us: %u = %u - %u\n", measured_us, tstamp, gateway->tstamp_at_beacon);
        int trigcnt_error_over_beacon_period = BEACON_PERIOD_US - measured_us;
        BEACON_PRINTF("trigcnt_error_over_beacon_period: %d = %u - %u\n", trigcnt_error_over_beacon_period, BEACON_PERIOD_US, measured_us);
        if (abs(trigcnt_error_over_beacon_period) > (BEACON_PERIOD_US / 2)) {
            /* missing beacon notifications */
            float missed = trigcnt_error_over_beacon_period / (float)BEACON_PERIOD_US;
            int modulus_error = trigcnt_error_over_beacon_period % BEACON_PERIOD_US;
            printf("\e[31mmissed beacon notifications: %f, mod_err:%d\e[0m\n", missed, modulus_error);
            printf("\e[31m %d = %d %% %u \e[0m\n", modulus_error, trigcnt_error_over_beacon_period, BEACON_PERIOD_US);
            trigcnt_error_over_beacon_period = (int)(modulus_error / missed);
            /* TODO: possible to get crystal error over missed beacons? */
            gateway->sx1301_ppm_err = 0;
        } else {
            gateway->sx1301_ppm_err = trigcnt_error_over_beacon_period  / (float)(BEACON_PERIOD_US / 1000000);
        }
        BEACON_PRINTF("err:%d, sx1301_ppm_err:%f ", trigcnt_error_over_beacon_period, gateway->sx1301_ppm_err);
        uint32_t reserved_trigcnt_offset = 2120000 + (gateway->sx1301_ppm_err * 2.12);
        gateway->trigcnt_pingslot_zero = tstamp + reserved_trigcnt_offset;

        gateway->lgw_trigcnt_at_next_beacon = tstamp + (BEACON_PERIOD_US - trigcnt_error_over_beacon_period);
    } else {
        uint32_t reserved_trigcnt_offset = 2120000;
        gateway->trigcnt_pingslot_zero = tstamp + reserved_trigcnt_offset;
        gateway->lgw_trigcnt_at_next_beacon = tstamp + BEACON_PERIOD_US;
        BEACON_PRINTF("uncorrected ");
    }
    BEACON_PRINTF("lgw_trigcnt_at_next_beacon:%u ", gateway->lgw_trigcnt_at_next_beacon);

    if (beacon_indication_start_time.tv_sec == 0) {
        /* for later triggering of servicing data collected here,
         * save time of first beacon indication seen at this period */
        beacon_indication_start_time.tv_sec = host_time->tv_sec;
        beacon_indication_start_time.tv_nsec = host_time->tv_nsec;
    }

    gateway->host_time_beacon_indication.tv_sec = host_time->tv_sec;
    gateway->host_time_beacon_indication.tv_nsec = host_time->tv_nsec;
    gateway->have_gps_fix = true;

    gateway->tstamp_at_beacon = tstamp;
    gateway->seconds_at_beacon = secs;

    gateway->count_us_tx_start = tstamp;
    // dont care about exact value, inside beacon_reserved
    gateway->count_us_tx_end = tstamp + 1000000;

    BEACON_PRINTF("\n");
}

/** @brief print lora spreading factor to stdout
 */
void print_hal_sf(uint8_t datarate)
{
    printf("sf");
    switch (datarate) {
        case DR_LORA_SF7: printf("7 "); break;
        case DR_LORA_SF8: printf("8 "); break;
        case DR_LORA_SF9: printf("9 "); break;
        case DR_LORA_SF10: printf("10 "); break;
        case DR_LORA_SF11: printf("11 "); break;
        case DR_LORA_SF12: printf("12 "); break;
    }
}

/** @brief print lora bandwidth KHz to stdout
 */
void print_hal_bw(uint8_t bandwidth)
{
    RF_PRINTF("bw");
    switch (bandwidth) {
        case BW_125KHZ: printf("125 "); break;
        case BW_250KHZ: printf("250 "); break;
        case BW_500KHZ: printf("500 "); break;
        default: printf("? "); break;
    }
}

/** @brief process packet received by gateway
 * @param gateway pointer to receiving gateway
 * @param user_buf uplink buffer sent by gateway
 * @param length length of buffer sent by gateway
 */
void
parse_uplink(gateway_t* gateway, const uint8_t* user_buf, uint16_t length)
{
    mhdr_t* mhdr;
    uint16_t* u16_ptr;
    struct lgw_pkt_rx_s rx_pkt;
    unsigned int idx = 0;
    uint32_t* u32_ptr;
    RF_PRINTF("\e[32mfNS parse_uplink(,,%u) ", length);

    u32_ptr = (uint32_t*)&user_buf[idx];
    rx_pkt.freq_hz = *u32_ptr;
    idx += sizeof(rx_pkt.freq_hz);
    RF_PRINTF("%uhz ", rx_pkt.freq_hz);

    rx_pkt.if_chain = user_buf[idx++];
    rx_pkt.status = user_buf[idx++];

    u32_ptr = (uint32_t*)&user_buf[idx];
    rx_pkt.count_us = *u32_ptr;
    idx += sizeof(rx_pkt.count_us);

    rx_pkt.rf_chain = user_buf[idx++];
    rx_pkt.modulation = user_buf[idx++];
    rx_pkt.bandwidth = user_buf[idx++];

    u32_ptr = (uint32_t*)&user_buf[idx];
    rx_pkt.datarate = *u32_ptr;
    idx += sizeof(rx_pkt.datarate);

    rx_pkt.coderate = user_buf[idx++];

    memcpy(&rx_pkt.rssi , &user_buf[idx], sizeof(rx_pkt.rssi));
    idx += sizeof(rx_pkt.rssi);

    memcpy(&rx_pkt.snr , &user_buf[idx], sizeof(rx_pkt.snr));
    idx += sizeof(rx_pkt.snr);
    RF_PRINTF("snr:%.1f rssi:%.0f ", rx_pkt.snr, rx_pkt.rssi);

    memcpy(&rx_pkt.snr_min , &user_buf[idx], sizeof(rx_pkt.snr_min));
    idx += sizeof(rx_pkt.snr_min);

    memcpy(&rx_pkt.snr_max , &user_buf[idx], sizeof(rx_pkt.snr_max));
    idx += sizeof(rx_pkt.snr_max);

    u16_ptr = (uint16_t*)&user_buf[idx];
    rx_pkt.crc = *u16_ptr;
    idx += sizeof(rx_pkt.crc);

    u16_ptr = (uint16_t*)&user_buf[idx];
    rx_pkt.size = *u16_ptr;
    idx += sizeof(rx_pkt.size);

    if (rx_pkt.size > sizeof(rx_pkt.payload)) {
        RF_PRINTF("\e31msize:%u\e[0m\n", rx_pkt.size);
        return;
    }
    RF_PRINTF("size:%u ", rx_pkt.size);

    if (rx_pkt.modulation == MOD_LORA) {
        RF_PRINTF("LORA ");
        print_hal_sf(rx_pkt.datarate);
        print_hal_bw(rx_pkt.bandwidth);
    } else if (rx_pkt.modulation == MOD_FSK)
        RF_PRINTF("FSK %dbps ", rx_pkt.datarate);


    memcpy(rx_pkt.payload, user_buf+idx, rx_pkt.size);
    idx += rx_pkt.size;

    rx_pkt.modem_idx = user_buf[idx++];

    DEBUG_RF_BUF(rx_pkt.payload, rx_pkt.size, "rfrx");
    RF_PRINTF(" modem%u ", rx_pkt.modem_idx);

    if (rx_pkt.size <= (sizeof(fhdr_t) + LORA_FRAMEMICBYTES)) {
        printf("\e[31mrf-too-small\e[0m\n");
        return;
    }

    mhdr = (mhdr_t*)&rx_pkt.payload[0];
    if (mhdr->bits.major != 0) {
        printf("\e[31munsupported major:%u\e[0m\n", mhdr->bits.major);
        return;
    }

    RF_PRINTF(" ..up\e[0m\n");
    fNS_uplink_direct(gateway, &rx_pkt);


    //TODO network_server_log_pkt(gateway, &rx_pkt, false);
}

/** @brief find matching gateway EUI in list of gateways
 * \return pointer to gateway, or NULL if not found */
gateway_t*
gateway_find(uint64_t eui)
{
    struct _gateway_list* gl;
    for (gl = gateway_list; gl != NULL; gl = gl->next) {
        gateway_t* gw = &gl->gateway;
        if (gw->eui == eui) {
            return gw;
        }
    }

    return NULL;
}

/** @brief disconnect TCP connection from gateway
 */
void
gateway_disconnect(uint64_t eui)
{
    gateway_t* gw = gateway_find(eui);
    if (gw != NULL) {
        printf("closing %d\n", gw->fd);
        /* TODO: send reconfig command to gateway */
        shutdown(gw->fd, 0);
    }
}


/** @brief process all messages from gateway
 * @param buffer read buffer from TCP socket
 * @param nbytes size of read buffer
 * @param gw_fd gateway TCP file descriptor
 */
int
parse_gateway(const uint8_t* _buf, uint8_t nbytes, int gw_fd)
{
    const uint8_t* _bufStart = _buf;
    struct timespec read_host_time;
    uint64_t eui = 0;
    int i;
    const uint8_t* cmd;
    const uint16_t* msg_len;
    gateway_t* this_gateway = NULL;
    uint8_t alignedBuf[1024] __attribute__ ((aligned(4)));
    uint8_t* abuf = alignedBuf;
    const uint8_t* aBufStart;

    if (nbytes < 9) {
        printf("\e[31mparse_gateway() nbytes < header (%d < %d)\e[0m\n", nbytes, 9);
        return -1;
    }

    cmd = _buf++;
    msg_len = (uint16_t*)_buf;
    _buf += 2;

    if (clock_gettime (CLOCK_REALTIME, &read_host_time) == -1)
        perror ("clock_gettime");

    /* buffer[0] command
    *  buffer[1:2] uint16_t length
    *  buffer[3:8] mac addr
    */

    for (i = 0; i < 6; i++) {
        eui <<= 8;
        eui |= *_buf++;
    }

    //printf("gateway-eui:%" PRIx64 ", msg_len:%u ", eui, *msg_len);

    if ((nbytes - (_buf-_bufStart)) > sizeof(alignedBuf)) {
        printf("abuf too small (%u > %zu)\n", (unsigned)(nbytes - (_buf-_bufStart)), sizeof(alignedBuf));
        printf("nbytes:%d, _buf-_bufStart:%d\n", nbytes, (int)(_buf-_bufStart));
        return -1;
    }
    memcpy(abuf, _buf, nbytes - (_buf-_bufStart));
    aBufStart = abuf;

    /******** gateway linked-list... ****************/
    if (gateway_list == NULL) { // first time
        create_gateway(&gateway_list, eui, gw_fd);
        this_gateway = &gateway_list->gateway;
        printf("\e[32mcreate first gateway fd:%d\e[0m\n", gw_fd);
    } else {
        struct _gateway_list* gateway_list_ptr = gateway_list;
        while (gateway_list_ptr != NULL) {
            if (eui == gateway_list_ptr->gateway.eui) {
                this_gateway = &gateway_list_ptr->gateway;
                //printf("found gateway %p ", this_gateway);
                if (this_gateway->fd == -1) {
                    printf("gateway reconnect\n");
                } if (gw_fd != this_gateway->fd) {
                    printf("\e[31mgw_fd mismatch: %d vs %d\e[0m\n", gw_fd, this_gateway->fd);
                    close(this_gateway->fd);
                }
                this_gateway->fd = gw_fd;
                break;
            } else if (gateway_list_ptr->next == NULL) {
                create_gateway(&gateway_list_ptr->next, eui, gw_fd);
                this_gateway = &gateway_list_ptr->next->gateway;
                printf("create next gateway fd:%d\n", gw_fd);
                break;
            }
            gateway_list_ptr = gateway_list_ptr->next;
        } // ..while (gateway_list_ptr != NULL)
    }
    if (this_gateway == NULL) {
        fprintf(stderr, "this_gateway == NULL\n");
        return -1;
    }
    //printf("this_gateway %p\n", this_gateway);
    /******** ...gateway linked-list ****************/

    this_gateway->read_host_time.tv_sec = read_host_time.tv_sec;
    this_gateway->read_host_time.tv_nsec = read_host_time.tv_nsec;

    switch (*cmd) {
        int i;
        uint32_t* u32_ptr;
        double* dptr;
        short* sptr;
        case REQ_CONF:
            u32_ptr = (uint32_t*)abuf;
            abuf += sizeof(uint32_t);
            printf("REQ_CONF prot_ver%d:", *u32_ptr);
            if (*u32_ptr != PROT_VER) {
                printf("PROT_VER mismatch (%u vs %u)\r\n", PROT_VER, *u32_ptr);
                close(gw_fd);
                return -1;
            }
            i = fNS_find_gateway(this_gateway);
            this_gateway->num_modems = *abuf++;
            printf("%d = find_gateway() num_modems:%u\n", i, this_gateway->num_modems);
            if (i == 0) {
                /* first time this gateway connecting here */
                fNS_add_gateway(this_gateway);
                if (fNS_find_gateway(this_gateway) > 0) {
                    if (send_gateway_config(this_gateway) < 0) {
                        printf("closing gateway %" PRIx64 "\n", this_gateway->eui);
                        close(gw_fd);
                        return -1;
                    }
                }
            } else if (i > 0) {
                printf("found gateway, region %s\n", this_gateway->RFRegion);
                if (send_gateway_config(this_gateway) < 0) {
                    printf("closing gateway %" PRIx64 "\n", this_gateway->eui);
                    close(gw_fd);
                    return -1;
                }
            }
            break;
        case BEACON_INIT:
            u32_ptr = (uint32_t*)abuf;
            this_gateway->lgw_trigcnt_at_next_beacon = *u32_ptr;
            abuf += sizeof(uint32_t);

            u32_ptr = (uint32_t*)abuf;
            // when beacon would have been sent if gateway was already running
            this_gateway->seconds_at_beacon = *u32_ptr;
            abuf += sizeof(uint32_t);

            dptr = (double*)abuf;
            this_gateway->coord.lat = *dptr;
            abuf += sizeof(double);

            dptr = (double*)abuf;
            this_gateway->coord.lon = *dptr;
            abuf += sizeof(double);

            sptr = (short*)abuf;
            this_gateway->coord.alt = *sptr;
            abuf += sizeof(short);

            this_gateway->beacon_ch = *abuf++;
            this_gateway->beacon_modem_idx = *abuf++;

            printf("%" PRIx64 ": BEACON_INIT %u at lat:%f lon:%f alt:%d ch%d, modem%u, last beacon at %u\n",
                this_gateway->eui,
                this_gateway->lgw_trigcnt_at_next_beacon,
                this_gateway->coord.lat, this_gateway->coord.lon, this_gateway->coord.alt,
                this_gateway->beacon_ch,
                this_gateway->beacon_modem_idx,
                this_gateway->seconds_at_beacon
            );

            fNS_update_gateway(this_gateway, &read_host_time);

            this_gateway->tstamp_at_beacon = this_gateway->lgw_trigcnt_at_next_beacon - BEACON_PERIOD_US;
            this_gateway->have_gps_fix = true;
            break;
        case BEACON_INDICATION:
            {
                uint32_t tstamp, secs;
                BEACON_PRINTF("BEACON_INDICATION:");
                u32_ptr = (uint32_t*)abuf;
                tstamp = *u32_ptr;
                abuf += sizeof(uint32_t);
                BEACON_PRINTF("tstamp_at_beacon_tx:%u (diff %d) ", *u32_ptr, *u32_ptr - this_gateway->lgw_trigcnt_at_next_beacon);
                u32_ptr = (uint32_t*)abuf;
                secs = *u32_ptr;
                abuf += sizeof(uint32_t);

                dptr = (double*)abuf;
                this_gateway->coord.lat = *dptr;
                abuf += sizeof(double);

                dptr = (double*)abuf;
                this_gateway->coord.lon = *dptr;
                abuf += sizeof(double);

                sptr = (short*)abuf;
                this_gateway->coord.alt = *sptr;
                abuf += sizeof(short);

                this_gateway->beacon_ch = *abuf++;

                BEACON_PRINTF("beacon_sent_seconds:%u lat:%f lon:%f alt:%d ch%d\n", *u32_ptr,
                    this_gateway->coord.lat, this_gateway->coord.lon, this_gateway->coord.alt,
                    this_gateway->beacon_ch
                );

                beacon_indication(this_gateway, tstamp, secs, &read_host_time);
            }
            break;
        case UPLINK:
            this_gateway->uppacketsreceived++;
            i = (*msg_len) - (abuf-aBufStart);
            parse_uplink(this_gateway, abuf, i);
            abuf += i;
            break;
        default:
            printf("\e[31mparse_gateway() unknown req:%u\n", *cmd);
            for (i = 0; i < nbytes; i++) {
                printf("%02x ", abuf[i]);
            }
            printf("\e[0m\n");
            break;
    }

    return (abuf - aBufStart) + (_buf - _bufStart);
} // ..parse_gateway()

/** @brief read gateway TCP socket
 * @param filedes file descriptor of TCP socket
 * @return -1 for failure, 0 for ok
 */
int
read_from_client (int filedes)
{
    uint8_t buffer[MAXMSG];
    int nbytes;

    nbytes = read (filedes, buffer, MAXMSG);
    if (nbytes < 0)
    {
        /* Read error. */
        perror ("\e[32msock-read\e[0m");
        return -1;
    }
    else if (nbytes == 0) {
        /* End-of-file. */
        printf("read_from_client(%d) EOF\n", filedes);
        struct _gateway_list* gateway_list_ptr = gateway_list;
        while (gateway_list_ptr != NULL) {
            if (filedes == gateway_list_ptr->gateway.fd) {
                /* indicate gateway as disconnected */
                gateway_list_ptr->gateway.fd = -1;
                gateway_list_ptr->gateway.updated = false;
                gateway_list_ptr->gateway.have_gps_fix = false;
                break;
            }
            gateway_list_ptr = gateway_list_ptr->next;
        }

        return -1;
    } else
    {
        /* Data read. */
        while (nbytes > 0) {
            int r;
            r = parse_gateway(buffer, nbytes, filedes);
            if (r< 0)
                return r;
            nbytes -= r;
        }
        return 0;
    }
} // ..read_from_client()


#if 0
void
discard_mote(mote_t** mpptr)
{
    //printf("discarding %"PRIx64"\n", (*mpptr)->dev_eui64);
    free(*mpptr);
    *mpptr = NULL;
}
#endif

#define GATEWAY_WAITING_SECONDS     0.2 /**< permitted latency variation between gateways */
/** @brief run deferred tasks
 * complete uplink processing and service gateway beacon indications */
static void
gateway_service()
{
    struct _mote_list* my_mote_list;
    struct timespec now;

    if (clock_gettime (CLOCK_REALTIME, &now) == -1)
        perror ("clock_gettime");

    /* beacon indication service: */
    if (beacon_indication_start_time.tv_sec != 0) {
        double secs = difftimespec(now, beacon_indication_start_time);
        if (secs > GATEWAY_WAITING_SECONDS) {
            beacon_indication_start_time.tv_sec = 0;

            for (my_mote_list = mote_list; my_mote_list != NULL; my_mote_list = my_mote_list->next) {
                mote_t* mote = my_mote_list->motePtr;
                if (!mote)
                    continue;

                fNS_beacon_service(mote);
            }

            /* update gateways */
            struct _gateway_list* gl;
            for (gl = gateway_list; gl != NULL; gl = gl->next) {
                gateway_t* gw = &gl->gateway;
                //printf("%"PRIx64" gw->new_indication:%u\n", gw->eui, gw->new_indication);
                if (gw->new_indication) {
                    fNS_update_gateway(gw, &gw->host_time_beacon_indication);
                    gw->new_indication = false;
                    gw->updated = true;
                } else
                    gw->updated = false;

                //network_server_log_beacon(gw);
            } // ..for (gl = gateway_list; gl != NULL; gl = gl->next)

        } // ..if (secs > GATEWAY_WAITING_SECONDS)
    }

} // ..fNS_uplink_service()

pthread_t thread0;

void *my_entry_function(void *d)
{
    struct timeval tv;
    struct timeval *tvp;
    MHD_UNSIGNED_LONG_LONG mhd_timeout = 20;    // milliseconds
    MHD_socket max;
    fd_set ws;
    fd_set es;
    fd_set rs;

    for (;;) {
        /*************** microhttpd... *************************/
        lib_expire_sessions ();
        max = 0;
        FD_ZERO (&rs);
        FD_ZERO (&ws);
        FD_ZERO (&es);
        if (MHD_YES != MHD_get_fdset (d, &rs, &ws, &es, &max))
            break; /* fatal internal error */
        if (MHD_get_timeout (d, &mhd_timeout) == MHD_YES)
        {
            tv.tv_sec = mhd_timeout / 1000;
            tv.tv_usec = (mhd_timeout - (tv.tv_sec * 1000)) * 1000;
            tvp = &tv;
        }
        else
            tvp = NULL;
        if (-1 == select (max + 1, &rs, &ws, &es, tvp))
        {
            if (EINTR != errno)
                abort ();
        }
        MHD_run (d);
        /*************** ...microhttpd *************************/
    }

    return NULL;
}

void
json_print_type(json_type t)
{
    switch (t) {
        case json_type_null: printf("null"); break;
        case json_type_boolean: printf("bool"); break;
        case json_type_double: printf("double"); break;
        case json_type_int: printf("int"); break;
        case json_type_object:
            printf("object");
            break;
        case json_type_array: printf("array"); break;
        case json_type_string: printf("string"); break;
    }
}

int ns_conf_json(json_object *jobjSrv, conf_t* c)
{
    int len;
    json_object *obj;
    struct _region_list* rl;

    if (json_object_object_get_ex(jobjSrv, "network_id", &obj)) {
        sscanf(json_object_get_string(obj), "%i", &myNetwork_id32);
        printf("network_id:%x\n", myNetwork_id32);
        sprintf(myNetwork_idStr, "%06x", myNetwork_id32);
    } else {
        printf("no network_id\n");
        return -1;
    }

    if (json_object_object_get_ex(jobjSrv, "leap_seconds", &obj)) {
        leap_seconds = json_object_get_int(obj);
    } else {
        printf("no leap_seconds\n");
        return -1;
    }

    if (json_object_object_get_ex(jobjSrv, "dl_rxwin", &obj)) {
        dl_rxwin = json_object_get_int(obj);
    } else {
        printf("no dl_rxwin\n");
        return -1;
    }

    if (json_object_object_get_ex(jobjSrv, "regionPath", &obj)) {
        strncpy(regionPath, json_object_get_string(obj), sizeof(regionPath));
    } else {
        printf("no regionPath\n");
        return -1;
    }

    if (json_object_object_get_ex(jobjSrv, "regions", &obj)) {
        struct _region_list* my_region_list = NULL;
        int i;
        len = json_object_array_length(obj);
        for (i = 0; i < len; i++) {
            json_object *o, *ajo = json_object_array_get_idx(obj, i);

            if (region_list == NULL) {  // first time
                region_list = calloc(1, sizeof(struct _region_list));
                my_region_list = region_list;
            } else {
                my_region_list->next = calloc(1, sizeof(struct _region_list));
                my_region_list = my_region_list->next;
            }

            if (json_object_object_get_ex(ajo, "name", &o)) {
                const char* name = json_object_get_string(o);
                if (strcmp(name, EU868) == 0)
                    my_region_list->region.RFRegion = EU868;
                else if (strcmp(name, US902) == 0)
                    my_region_list->region.RFRegion = US902;
                else if (strcmp(name, China470) == 0)
                    my_region_list->region.RFRegion = China470;
                else if (strcmp(name, China779) == 0)
                    my_region_list->region.RFRegion = China779;
                else if (strcmp(name, EU433) == 0)
                    my_region_list->region.RFRegion = EU433;
                else if (strcmp(name, Australia915) == 0)
                    my_region_list->region.RFRegion = Australia915;
                else if (strcmp(name, AS923) == 0)
                    my_region_list->region.RFRegion = AS923;
                else {
                    printf("conf unknown region name \"%s\"\n", name);
                    return -1;
                }
                printf(" have region \"%s\"\n", my_region_list->region.RFRegion);
            }
            /*if (json_object_object_get_ex(ajo, "index", &o)) {
                my_region_list->region.index = json_object_get_int(o);
                printf("index %u\n", my_region_list->region.index);
            }*/
            if (json_object_object_get_ex(ajo, "conf_file_8ch", &o)) {
                my_region_list->region.conf_file_8ch = malloc(json_object_get_string_len(o)+1);
                strcpy(my_region_list->region.conf_file_8ch, json_object_get_string(o));
                printf("8ch:%s\n", my_region_list->region.conf_file_8ch);
            }
            if (json_object_object_get_ex(ajo, "conf_file_16ch", &o)) {
                my_region_list->region.conf_file_16ch = malloc(json_object_get_string_len(o)+1);
                strcpy(my_region_list->region.conf_file_16ch, json_object_get_string(o));
                printf("16ch:%s\n", my_region_list->region.conf_file_16ch);
            }
        }
    } // ..regions

    for (rl = region_list; rl != NULL; rl = rl->next) {
        char buf[8192];
        region_t* region = &rl->region;
        int fd;
        char fileNamePath[256];
        if (region == NULL || region->conf_file_8ch == NULL)
            continue;
        sprintf(fileNamePath, "%s/%s", regionPath, region->conf_file_8ch);
        fd = open(fileNamePath, O_RDONLY, 0);
        if (fd < 0) {
            perror(fileNamePath);
            continue;
        }
        len = read(fd, buf, sizeof(buf));
        if (len <= 0) {
            perror("\e[31mfile-read\e[0m");
            close(fd);
            continue;
        }
        parse_region_config(&region->regional, buf);
        close(fd);

        region->regional.rx1_band_conv = NULL;
        region->regional.parse_start_mac_cmd = NULL;
        region->regional.init_session = NULL;
        region->regional.get_cflist = NULL;

        if (region->RFRegion == EU868) {
            region->regional.rx1_band_conv = rx1_band_conv_eu868;
        } else if (region->RFRegion == US902) {
            region->regional.rx1_band_conv = rx1_band_conv_us902;
            region->regional.get_ch = us902_get_ch;
        } else if (region->RFRegion == China779) {
            region->regional.rx1_band_conv = rx1_band_conv_cn779;
        } else if (region->RFRegion == EU433) {
            region->regional.rx1_band_conv = rx1_band_conv_eu433;
        } else if (region->RFRegion == Australia915) {
            region->regional.rx1_band_conv = rx1_band_conv_au915;
            region->regional.get_ch = au915_get_ch;
        } else if (region->RFRegion == AS923) {
            region->regional.rx1_band_conv = rx1_band_conv_as923;
            region->regional.parse_start_mac_cmd = arib_parse_start_mac_cmd;
            region->regional.get_ch = arib_get_ch;
            region->regional.init_session = arib_init_session;
            region->regional.get_cflist = arib_get_cflist;
        }


    } // ..for (rl = region_list; rl != NULL; rl = rl->next)

    if (parse_json_KeyEnvelope("KeyEnvelopeNSJS", jobjSrv, &key_envelope_ns_js) < 0)
        return -1;

    if (json_object_object_get_ex(jobjSrv, VendorID, &obj))
        sscanf(json_object_get_string(obj), "%x", &myVendorID);

    return 0;
}


mqd_t mqd;
void
intHandler(int dummy)
{
    mq_close(mqd);
    printf("mq_close\n");
    fflush(stdout);
    mq_unlink(mq_name);
}

int sessionCreate(struct Session* s) { return 0; }
void sessionEnd(struct Session* s) { }

pid_t pid;

struct MHD_Daemon *
init(const char* conf_filename, int argHttpPort, int argNetID)
{
    struct mq_attr attr;
    conf_t c;
    struct MHD_Daemon *ret;
    char dbName[64];
    //char url[128];

    if (parse_server_config(conf_filename, ns_conf_json, &c) < 0) {
        printf("parse_server_config(%s) failed\n", conf_filename);
        return NULL;
    }
    strcpy(joinDomain, c.joinDomain);
    strcpy(netIdDomain, c.netIdDomain);

    if (argNetID == -1)
        strcpy(dbName, "lora_network");
    else {   // permit multiple network servers on same host
        sprintf(dbName, "lora_network%06x", argNetID);
        sprintf(myNetwork_idStr, "%06x", argNetID);
        myNetwork_id32 = argNetID;
    }

    if (fNS_init(c.sql_hostname, c.sql_username, c.sql_password, c.sql_port, dbName) < 0) {
        printf("failed fNS_init()\n");
        return NULL;
    }

    if (web_init(c.sql_hostname, c.sql_username, c.sql_password, c.sql_port, dbName) < 0) {
        printf("failed fNS_init()\n");
        return NULL;
    }

    // attr.mq_flags; 0 or O_NONBLOCK
    attr.mq_maxmsg = 7;    // max # of message on queue
    attr.mq_msgsize = MQ_MSGSIZE; // max message size in bytes
    // attr.mq_curmsgs; # of messages sitting in queue
    sprintf(mq_name, "/ns%06x", myNetwork_id32);
    mqd = mq_open(mq_name, O_WRONLY | O_CREAT, 0666, &attr);
    if (mqd == (mqd_t)-1) {
        perror("mq_open");
        return NULL;
    }

    {
        pid = fork();
        if (pid < 0) {
            perror("fork");
            return NULL;
        } else if (pid == 0) {
            // child process
            printf("child start %u\n", pid);
            exit(child(mq_name, c.sql_hostname, c.sql_username, c.sql_password, c.sql_port, dbName));
        } else {
            // parent process
            printf("parent mqd %d\n", mqd);
            signal(SIGINT, intHandler);
        }
    }

    if (argHttpPort != -1)
        c.httpd_port = argHttpPort;     // override conf.json port

    ret = MHD_start_daemon (MHD_USE_ERROR_LOG,
        c.httpd_port,
        NULL, NULL,
        &lib_create_response, NULL,
        MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 15,
        MHD_OPTION_NOTIFY_COMPLETED, &lib_request_completed_callback, NULL,
        MHD_OPTION_END
    );
    if (ret != NULL)
        printf("httpd port %u\n", c.httpd_port);


    return ret;
}

int
main (int argc, char **argv)
{
    char conf_filename[96]; /**< file name of server JSON configuration */
    fd_set active_fd_set, read_fd_set;
    int status, opt, i, sock;
    struct MHD_Daemon *d;
    int argNetID = -1;
    int argHttpPort = -1;

    strcpy(conf_filename, "../network_server/conf.json");  // default conf file

    while ((opt = getopt(argc, argv, "n:tp:fg:")) != -1) {
        switch (opt) {
            case 'c':
                strncpy(conf_filename, optarg, sizeof(conf_filename));
                break;
            case 'n':
                argNetID = strtol(optarg, NULL, 16);
                break;
            case 'p':
                argHttpPort = atoi(optarg);
                break;
            case 'f':
                fNS_enable = false;
                break;
            case 'g':
                tcp_listen_port = atoi(optarg);
                break;
            /*case 't':
                gw_snr_test();
                return 0;*/
            default: /* '?' */
                printf("-g <portNum>     gateway listen port (default %u)\n", tcp_listen_port);
                printf("-c <confFile>     use different conf.json\n");
                printf("-p <httpdPort>      run httpd on other port\n");
                printf("-n <nexNetID>      run as different NetID, uses sql table lora_networkXXXXXX\n");
                printf("-f                 disable gateway interface\n");
                return -1;
        }
    }
    printf("gateway listen port %u\n", tcp_listen_port);

    d = init(conf_filename, argHttpPort, argNetID);
    if (!d) {
        printf("init() failed\n");
        return -1;
    }

    pthread_create(&thread0, NULL, my_entry_function, d);

    curl_global_init(CURL_GLOBAL_ALL);

/*    curl = curl_easy_init();
    if (!curl)
        return -1;*/
    multi_handle = curl_multi_init();

    if (fNS_enable) {
        /* Create the socket and set it up to accept connections. */
        sock = make_socket (tcp_listen_port);
        if (sock < 0) {
            printf("failed make_socket\n");
            return -1;
        }

        if (listen (sock, 1) < 0)
        {
            perror ("\e[31mlisten\e[0m");
            return -1;
        }

        FD_ZERO (&active_fd_set);

        /* Initialize the set of active sockets. */
        FD_SET (sock, &active_fd_set);
    } else
        sock = -1;

    while (1)
    {
        if (fNS_enable) {
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;

            /* Block until input arrives on one or more active sockets. */
            read_fd_set = active_fd_set;
            if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, &timeout) < 0)
            {
                perror ("\e[31mselect\e[0m");
                goto gEnd;
            }

            /* Service all the sockets with input pending. */
            for (i = 0; i < FD_SETSIZE; ++i) {
                if (FD_ISSET (i, &read_fd_set))
                {
                    if (i == sock)
                    {
                        socklen_t size;
                        struct sockaddr_in clientname;
                        /* Connection request on original socket. */
                        int new;
                        size = sizeof (clientname);
                        new = accept (sock, (struct sockaddr *) &clientname, &size);
                        if (new < 0)
                        {
                            perror ("\e[31maccept\e[0m");
                            goto gEnd;
                        }
                        //fprintf (stderr, "Server: connect from host %s, port %hd.\n", inet_ntoa (clientname.sin_addr), ntohs (clientname.sin_port));
                        fprintf (stderr, "Server: connect from host %s, port %u, %d = accept()\n", inet_ntoa (clientname.sin_addr), ntohs (clientname.sin_port), new);
                        FD_SET (new, &active_fd_set);
                    }
                    else
                    {
                        /* Data arriving on an already-connected socket. */
                        if (read_from_client (i) < 0)
                        {
                            close (i);
                            FD_CLR (i, &active_fd_set);
                        }
                    }
                } // ..if (FD_ISSET (i, &read_fd_set))
            } // ..for (i = 0; i < FD_SETSIZE; ++i)

            gateway_service();

        } /*else
            usleep(100000);*/

        common_service();

        curl_service(sqlConn_lora_network, multi_handle, 100);
    } // ..while (1)

    MHD_stop_daemon (d);
    pthread_join(thread0, NULL);

gEnd:
    printf("waiting on child pid:%u\n", pid);
    waitpid(pid, &status, 0);
    printf("waitpid done\n");

    curl_global_cleanup();
}
