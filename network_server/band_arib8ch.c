/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "sNS.h"
#include <math.h>

const float channelsMHz[] = {
    923.2, // ch0
    923.4, // ch1
    922.0, // ch2
    921.8, // ch3
    921.6, // ch4
    921.4, // ch5
};

int arib_get_ch(float MHz, uint8_t DataRate)
{
    unsigned ch;

    MAC_PRINTF(" arib_get_ch(%f, %u) ", MHz, DataRate);
    for (ch = 0; ch < 6; ch++) {
        MAC_PRINTF("ch%u ", ch);
        if (fabsf(MHz - channelsMHz[ch]) < 0.03)
            return ch;
    }

    return -1;
}

uint8_t*
arib_get_cflist(uint8_t* ptr)
{
    unsigned ch;
    uint32_t freq;
    /*
     * LoRaMacChannelAdd(3) 921800000hz dr:0x53 add ch3 mask:000b
     * LoRaMacChannelAdd(4) 921600000hz dr:0x53 add ch4 mask:001b
     * LoRaMacChannelAdd(5) 921400000hz dr:0x53 add ch5 mask:003b*/

    ch = 3;
    freq = (channelsMHz[ch++]*1000000) / 100; //
    ptr = Write3ByteValue(ptr, freq);

    freq = (channelsMHz[ch++]*1000000) / 100; //
    ptr = Write3ByteValue(ptr, freq);

    freq = (channelsMHz[ch++]*1000000) / 100; //
    ptr = Write3ByteValue(ptr, freq);

    freq = 0;   //
    ptr = Write3ByteValue(ptr, freq);

    freq = 0;   //
    ptr = Write3ByteValue(ptr, freq);

    ptr = Write1ByteValue(ptr, 0);  // CFListType


    return ptr;
}


/* see table 4, page 50 */
void
arib_init_session(mote_t* mote, const regional_t* rp)
{
    unsigned dr;
    float MHz;
    uint8_t cmd_buf[MAC_CMD_SIZE];
    uint32_t freq;
    char str[32];
    s_t* s = mote->s;

    /* enable channels in cflist */
    s->ChMask[0] |= 0x0038;
    printf("\e[36mChMask[0]:%04x\e[0m\n", s->ChMask[0]);

    MAC_PRINTF("arib_init_session()\n");
    if (deviceProfileReq(sqlConn_lora_network, mote->devEui, mote->devAddr, SupportsClassB, str, sizeof(str)) < 0) {
        printf("arib_init_session deviceProfileReq\n");
        return;
    }

    if (str[0] == '1') {
        if (deviceProfileReq(sqlConn_lora_network, mote->devEui, mote->devAddr, PingSlotDR, str, sizeof(str)) < 0) {
            printf("arib_init_session deviceProfileReq\n");
            return;
        }
        sscanf(str, "%u", &dr);
        if (deviceProfileReq(sqlConn_lora_network, mote->devEui, mote->devAddr, PingSlotFreq, str, sizeof(str)) < 0) {
            printf("arib_init_session deviceProfileReq\n");
            return;
        }
        sscanf(str, "%f", &MHz);
        //freq = rp->ping_freq_hz / 100;
        freq = MHz * 10000;
        cmd_buf[0] = SRV_MAC_PING_SLOT_CHANNEL_REQ;
        cmd_buf[1] = freq & 0xff;
        cmd_buf[2] = (freq >> 8) & 0xff;
        cmd_buf[3] = (freq >> 16) & 0xff;
        cmd_buf[4] = dr;//rp->ping_dr;
        put_queue_mac_cmds(s, 5, cmd_buf, true);

        freq = rp->beacon_hz / 100;
        cmd_buf[0] = SRV_MAC_BEACON_FREQ_REQ;
        cmd_buf[1] = freq & 0xff;
        cmd_buf[2] = (freq >> 8) & 0xff;
        cmd_buf[3] = (freq >> 16) & 0xff;
        put_queue_mac_cmds(s, 4, cmd_buf, true);
    }

    if (deviceProfileReq(sqlConn_lora_network, mote->devEui, mote->devAddr, RXFreq2, str, sizeof(str)) < 0) {
        printf("arib_init_session deviceProfileReq\n");
        return;
    }
    sscanf(str, "%f", &MHz);

    if (deviceProfileReq(sqlConn_lora_network, mote->devEui, mote->devAddr, RXDataRate2, str, sizeof(str)) < 0) {
        printf("arib_init_session deviceProfileReq\n");
        return;
    }
    sscanf(str, "%u", &dr);

    //freq = rp->Rx2Channel.FrequencyHz / 100;
    freq = MHz * 10000;
    cmd_buf[0] = SRV_MAC_RX_PARAM_SETUP_REQ;   // Rx2 window config
   // cmd_buf[1] = 0 | rp->Rx2Channel.Datarate; // drOffset:hi-nibble, datarate:lo-nibble
    cmd_buf[1] = 0 | dr; // drOffset:hi-nibble, datarate:lo-nibble
    cmd_buf[2] = freq & 0xff;
    cmd_buf[3] = (freq >> 8) & 0xff;
    cmd_buf[4] = (freq >> 16) & 0xff;
    put_queue_mac_cmds(s, 5, cmd_buf, true);
}

void
arib_parse_start_mac_cmd(const uint8_t* buf, uint8_t buf_len, mote_t* mote)
{
    uint8_t cmd_buf[MAC_CMD_SIZE];
    uint32_t freq;
    uint8_t idx;
    unsigned ch;
    s_t* s = mote->s;

    //printf("arib_parse_start_mac_cmd() ");
    for (idx = 0; idx < buf_len; ) {
        switch (buf[idx]) {
            case MOTE_MAC_RX_PARAM_SETUP_ANS:
                idx++; // cmd
                idx++; // status
                ch = 2;
                freq = (channelsMHz[ch] * 1000000) / 100;

                cmd_buf[0] = SRV_MAC_NEW_CHANNEL_REQ;
                cmd_buf[1] = ch; // channel index
                cmd_buf[2] = freq & 0xff;
                cmd_buf[3] = (freq >> 8) & 0xff;
                cmd_buf[4] = (freq >> 16) & 0xff;
                cmd_buf[5] = (DR_5 << 4) | DR_3;  // DrRange
                put_queue_mac_cmds(s, 6, cmd_buf, true);

                s->ChMask[0] |= 0x0004;  // ch2 enable
                s->flags.force_adr = true;  // ensure ch_mask is sent
                printf("put NEW_CHANNEL_REQ [36mChMask[0]:%04x[0m", s->ChMask[0]);
                break;
            case MOTE_MAC_NEW_CHANNEL_ANS:
                idx++; // cmd
                idx++; // status
                s->ChMask[0] &= ~0x0003;  // ch0,1 disable
                s->flags.force_adr = true;  // ensure ch_mask is sent
                s->flags.send_start_mac_cmds = false;   // done
                printf(" session_start = false, [36mChMask[0]:%04x[0m", s->ChMask[0]);
                break;
            default:
                /* ignore unrecognized */
                idx++;
                break;
        } // ...switch (cmd)
    } // ..for (idx = 0; idx < buf_len; )

    //printf("\n");
}

void rx1_band_conv_as923(float ULFreq, uint8_t ULDataRate, uint8_t rxdrOffset1, DLMetaData_t* out)
{
    out->DLFreq2 = 0;
    out->DLFreq1 = ULFreq;
    out->DataRate1 = ULDataRate - rxdrOffset1;
}

