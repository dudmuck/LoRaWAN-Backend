/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "ns.h"

#define MAC_CMD_SIZE            8   /**< maximum length of single mac command */
#define MAC_CMD_QUEUE_SIZE      6   /**< maximum mac commands to store into mote */
#define SNR_HISTORY_SIZE        3   /**< number of SNRs to store */

#define MAX_CH_MASKS        6


typedef struct {
    bool checkOldSessions;

    uint16_t ChMask[MAX_CH_MASKS];  /**< channel mask enable bits of this mote */

    bool send_start_mac_cmds; /**< network server processes regional-specific mac commands on mote initialization */
    bool session_start_at_uplink;   /**< server sends initialization mac commands, set from join-accept */
    bool force_adr; /**< used to send channel mask to end-node */
    struct {
        uint8_t len;
        bool needs_answer;
        uint8_t buf[MAC_CMD_SIZE];
    } mac_cmd_queue[MAC_CMD_QUEUE_SIZE];    /**< mac commands to be sent to this mote */
    uint8_t mac_cmd_queue_in_idx;
    uint8_t mac_cmd_queue_out_idx;

    float snr_history[SNR_HISTORY_SIZE];    /**< most recent uplink signal qualities */
    uint8_t snr_history_idx;    /**< circular SNR history buffer index */
    uint8_t txpwr_idx;  /**< mote transmit power: higher number is lower power */

    uint16_t ConfFCntDown;
    uint32_t AFCntDown;    /**< for next ULMetaData */
    uint8_t confDLPHYPayloadBin[256];
    uint8_t confDLPHYPayloadLen;
    bool incrNFCntDown;

    struct {
        union {
            struct {
                uint8_t periodicity: 3;    // 0 to 2
                uint8_t rfu: 5;    // 3 to 7
            } bits;
            uint8_t octet;
        } ping_slot_info;   /**< saved PingSlotInfoRequest */
        uint16_t ping_period;   /**< ping_period * 30ms = interval in milliseconds */
        uint16_t ping_offset;   /**< time offset in beacon period */
    } ClassB;

    struct {
        DLMetaData_t md;
        unsigned long reqTid;
        char* ansDest_;
        bool isAnsDestAS;

        uint8_t PHYPayloadBin[256];
        uint8_t PHYPayloadLen;

        uint8_t FRMPayloadBin[244];
        uint8_t FRMPayloadLen;
        bool FRMSent;
    } downlink;

    float DLFreq1, DLFreq2;

    bool RStop;
    uint8_t type2_rejoin_count;
    bool answer_app_downlink;
} s_t;

void put_queue_mac_cmds(s_t*, uint8_t cmd_len, uint8_t* cmd_buf, bool needs_answer);

/* adr.c: */
void network_controller_mote_init(s_t* s, const char* RFRegion, const char* txt);
void network_controller_uplink(s_t* s, int8_t snr);
void network_controller_adr(s_t* s, uint8_t DataRate, const char* RFRegion);
