/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "ns.h"
/* shared betweed gateway.c and fNS.c */

/** @brief gateway information
 */
typedef struct {
    int fd; /**< file descriptor for network connection to gateway */
    uint8_t num_modems; /**< 1=8ch gateway, 2=16ch gateway, ... */

    uint64_t eui;   /**< gateway EUI as 64bit integer */
    char eui_hex_str[32];   /**< gateway EUI as hex string */

    struct coord_s coord;   /**< physical location of gateway */
    uint32_t tstamp_at_beacon;  /**< sx1301 counter captured at last beacon TX */
    uint32_t seconds_at_beacon; /**< time value transmitted in beacon payload */
    float sx1301_ppm_err;   /**< measured sx1301 counter error over beacon period */
    uint32_t trigcnt_pingslot_zero; /**< sx1301 counter at ping slot zero start */
    struct timespec host_time_beacon_indication;    /**< our host time at last beacon indication */
    uint32_t lgw_trigcnt_at_next_beacon; /**< predicted sx1301 counter at next beacon TX start */
    bool new_indication;    /**< new beacon indication received */
    bool updated;   /**< new_indication was taken, trigcnt_pingslot_zero is valid */
    bool have_gps_fix;  /**< gateway has GPS fix, PPS is working */
    uint8_t beacon_ch;  /**< radio channel of last beacon transmitted */
    uint8_t beacon_modem_idx;  /**< which modem used for beacons and pings */

    uint32_t count_us_tx_start; /**< sx1301 counter at last TX start */
    uint32_t count_us_tx_end;   /**< sx1301 counter at last TX end */
    unsigned int logging_periods; /**< count of beacon periods to enable packet logging */

    const char* RFRegion; /**<  */
    uint32_t downpacketsreceived; /**< count of network_server_send_downlink() calls */
    uint32_t gooduppacketsreceived; /**< count of uplink packets received passed MIC test */
    uint32_t packetstransmitted; /**< count of downlink packets sent to this gateway */
    uint32_t uppacketsforwarded; /**< count of LoRaWAN uplink packets received */
    uint32_t uppacketsreceived; /**< count of uplink packets received */

    regional_t* rp;

    struct timespec read_host_time; /**< (TODO remove) our time captured at last read from gateway */
} gateway_t;

typedef struct {
    bool beaconDeferred;
    unsigned PingPeriod;
    gateway_t *gw;
    struct lgw_pkt_tx_s tx_pkt;
    char* clientID;
    unsigned long reqTid;
} f_t;

/** @brief list of active gateways */
struct _gateway_list {
    gateway_t gateway;  /**< this gateway */
    struct _gateway_list* next; /**< next gateway */
};

extern struct _gateway_list* gateway_list; /**< list of active gateways */

/* from fNS.c: */
int fNS_find_gateway(gateway_t* gw);
int fNS_add_gateway(gateway_t* gw);
void fNS_update_gateway(gateway_t* gw, struct timespec* time);
int fNS_init(const char* dbhostname, const char* dbuser, const char* dbpass, uint16_t dbport, const char*);
void fNS_uplink_direct(const gateway_t* gateway, const struct lgw_pkt_rx_s* rx_pkt);
void fNS_beacon_service(mote_t* mote);

