/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "sNS.h"

//#define ADR_DEBUG

#ifdef ADR_DEBUG
    #define ADR_PRINTF(...)     printf(__VA_ARGS__)
#else
    #define ADR_PRINTF(...)
#endif

#define NULL_SNR        -30     /**< initial (blank) SNR value */

void network_controller_uplink(s_t* s, int8_t snr)
{
    s->snr_history[s->snr_history_idx] = snr;
    if (++s->snr_history_idx == SNR_HISTORY_SIZE)
        s->snr_history_idx = 0;
}

void network_controller_mote_init(s_t* s, const char* RFRegion, const char* txt)
{
    int i;
    region_t* region = NULL;
    struct _region_list* rl;

    printf("\e[7mnetwork_controller_mote_init() RFRegion \"%s\" from %s\e[0m\n", RFRegion, txt);

    for (rl = region_list; rl != NULL; rl = rl->next) {
        if (rl->region.RFRegion == RFRegion)
            region = &rl->region;
    }
    if (!region)
        return;

    /* reset snr history */
    for (i = 0; i < SNR_HISTORY_SIZE; i++)
        s->snr_history[i] = NULL_SNR;

    for (i = 0; i < MAX_CH_MASKS; i++) {
        s->ChMask[i] = region->regional.init_ChMask[i];
        printf("\e[36minit chmask%u:%04x\e[0m\n", i, s->ChMask[i]);
    }

    s->txpwr_idx = region->regional.default_txpwr_idx;
    ADR_PRINTF("txpwr%u\e[0m\n", s->txpwr_idx);
}

void network_controller_adr(s_t* s, uint8_t DataRate, const char* RFRegion)
{
    uint8_t orig_txpwr_idx = s->txpwr_idx;
    uint8_t orig_ul_dr = DataRate;
    uint8_t hist_idx = s->snr_history_idx;
    unsigned int i;
    float max_snr = -40, min_snr = 40, snr = 0;
    region_t* region = NULL;
    struct _region_list* rl;

    for (rl = region_list; rl != NULL; rl = rl->next) {
        if (rl->region.RFRegion == RFRegion)
            region = &rl->region;
    }
    if (!region)
        return;

    for (i = 0; i < SNR_HISTORY_SIZE; i++) {
        if (s->snr_history[hist_idx] > max_snr)
            max_snr = s->snr_history[hist_idx];
        if (s->snr_history[hist_idx] < min_snr)
            min_snr = s->snr_history[hist_idx];

        snr += s->snr_history[hist_idx];

        if (hist_idx == 0)
            hist_idx = SNR_HISTORY_SIZE - 1;
        else
            hist_idx--;
    }
    snr /= SNR_HISTORY_SIZE;

    //printf("ADR avg snr:%.1f  (min:%.1f max:%.1f)\n", snr, min_snr, max_snr);
    if ((max_snr - min_snr) < 4.0 && DataRate != MAX_DATARATES) {
        /* stable snr */

        /* spreading-factor reduction, if S/N ratio permits */
        ADR_PRINTF("ADR txpwr%d %.1fdB DataRate%d ", s->txpwr_idx, snr, DataRate);
        /* TODO get DRMax from ServideProfile */
        while (DataRate < region->regional.defaults.uplink_dr_max && snr > 0.0) {
            DataRate++;
            snr -= 2.5;
            ADR_PRINTF("(%.1fdB DataRate%u) ", snr, DataRate);
        }

        /* end-node tx power reduction, if S/N ratio permits */
        while (snr > 3.0) {
            if (s->txpwr_idx < region->regional.max_txpwr_idx) {
                s->txpwr_idx++;  // higher power idx is lower dBm
                snr -= 3.0;
                ADR_PRINTF("(%.1fdB txpwr%d) ", snr, s->txpwr_idx);
            } else
                break;
        }
    } // ..if stable snr


    if (s->flags.force_adr || orig_ul_dr != DataRate || orig_txpwr_idx != s->txpwr_idx) {
        uint8_t cmd_buf[MAC_CMD_SIZE];
        ADR_PRINTF("ADR mote \e[36mChMask[0]:%04x\e[0m txpwr%u ", s->ChMask[0], s->txpwr_idx);
        ADR_PRINTF("=dr%d\n", DataRate);

        cmd_buf[0] = SRV_MAC_LINK_ADR_REQ;
        cmd_buf[1] = (DataRate << 4) | s->txpwr_idx;
        /* TODO: us915 band */
        cmd_buf[2] = s->ChMask[0] & 0xff;
        cmd_buf[3] = s->ChMask[0] >> 8;
        cmd_buf[4] = 0x00; /* redundancy: RFU + ChMaskCntl:hi-nibble NbTrans:lo-nibble */
        put_queue_mac_cmds(s, 5, cmd_buf, true);
    }
}

