/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <stdbool.h>

/** \cond X */
#define MIN_LORA_PREAMB 6 /* minimum Lora preamble length for this application */
#define STD_LORA_PREAMB 8

/* values available for the 'modulation' parameters */
/* NOTE: arbitrary values */
#define MOD_UNDEFINED   0
#define MOD_LORA        0x10
#define MOD_FSK         0x20

/* values available for the 'bandwidth' parameters (LoRa & FSK) */
/* NOTE: directly encode FSK RX bandwidth, do not change */
#define BW_UNDEFINED    0
#define BW_500KHZ       0x01
#define BW_250KHZ       0x02
#define BW_125KHZ       0x03
#define BW_62K5HZ       0x04
#define BW_31K2HZ       0x05
#define BW_15K6HZ       0x06
#define BW_7K8HZ        0x07

/* values available for the 'datarate' parameters */
/* NOTE: LoRa values used directly to code SF bitmask in 'multi' modem, do not change */
#define DR_UNDEFINED    0
#define DR_LORA_SF7     0x02
#define DR_LORA_SF8     0x04
#define DR_LORA_SF9     0x08
#define DR_LORA_SF10    0x10
#define DR_LORA_SF11    0x20
#define DR_LORA_SF12    0x40
#define DR_LORA_MULTI   0x7E
/* NOTE: for FSK directly use baudrate between 500 bauds and 250 kbauds */
#define DR_FSK_MIN      500
#define DR_FSK_MAX      250000

/* values available for the 'coderate' parameters (LoRa only) */
/* NOTE: arbitrary values */
#define CR_UNDEFINED    0
#define CR_LORA_4_5     0x01
#define CR_LORA_4_6     0x02
#define CR_LORA_4_7     0x03
#define CR_LORA_4_8     0x04

/* values available for the 'status' parameter */
/* NOTE: values according to hardware specification */
#define STAT_UNDEFINED  0x00
#define STAT_NO_CRC     0x01
#define STAT_CRC_BAD    0x11
#define STAT_CRC_OK     0x10
/** \endcond */

/* values available for the 'tx_mode' parameter */
#define IMMEDIATE       0   /**< send now */
#define TIMESTAMPED     1   /**< send when counter reached */
#define ON_GPS          2   /**< send at PPS edge */
//#define ON_EVENT      3
//#define GPS_DELAYED   4
//#define EVENT_DELAYED 5

/**
 * @struct lgw_pkt_rx_s
 * @brief Structure containing the metadata of a packet that was received and a pointer to the payload
 */
struct lgw_pkt_rx_s {
    uint32_t    freq_hz;        /*!< central frequency of the IF chain */
    uint8_t     if_chain;       /*!< by which IF chain was packet received */
    uint8_t     status;         /*!< status of the received packet */
    uint32_t    count_us;       /*!< internal concentrator counter for timestamping, 1 microsecond resolution */
    uint8_t     rf_chain;       /*!< through which RF chain the packet was received */
    uint8_t     modulation;     /*!< modulation used by the packet */
    uint8_t     bandwidth;      /*!< modulation bandwidth (LoRa only) */
    uint32_t    datarate;       /*!< RX datarate of the packet (SF for LoRa) */
    uint8_t     coderate;       /*!< error-correcting code of the packet (LoRa only) */
    float       rssi;           /*!< average packet RSSI in dB */
    float       snr;            /*!< average packet SNR, in dB (LoRa only) */
    float       snr_min;        /*!< minimum packet SNR, in dB (LoRa only) */
    float       snr_max;        /*!< maximum packet SNR, in dB (LoRa only) */
    uint16_t    crc;            /*!< CRC that was received in the payload */
    uint16_t    size;           /*!< payload size in bytes */
    uint8_t     payload[256];   /*!< buffer containing the payload */
    uint8_t     modem_idx;      /*!< which modem received this packet */
};

/**
@struct lgw_pkt_tx_s
@brief Structure containing the configuration of a packet to send and a pointer to the payload
*/
struct lgw_pkt_tx_s {
    uint32_t    freq_hz;        /*!< center frequency of TX */
    uint8_t     tx_mode;        /*!< select on what event/time the TX is triggered */
    uint32_t    count_us;       /*!< timestamp or delay in microseconds for TX trigger */
    uint8_t     rf_chain;       /*!< through which RF chain will the packet be sent */
    int8_t      rf_power;       /*!< TX power, in dBm */
    uint8_t     modulation;     /*!< modulation to use for the packet */
    uint8_t     bandwidth;      /*!< modulation bandwidth (LoRa only) */
    uint32_t    datarate;       /*!< TX datarate (baudrate for FSK, SF for LoRa) */
    uint8_t     coderate;       /*!< error-correcting code of the packet (LoRa only) */
    bool        invert_pol;     /*!< invert signal polarity, for orthogonal downlinks (LoRa only) */
    uint8_t     f_dev;          /*!< frequency deviation, in kHz (FSK only) */
    uint16_t    preamble;       /*!< set the preamble length, 0 for default */
    bool        no_crc;         /*!< if true, do not send a CRC in the packet */
    bool        no_header;      /*!< if true, enable implicit header mode (LoRa), fixed length (FSK) */
    uint16_t    size;           /*!< payload size in bytes */
    uint8_t     payload[256];   /*!< buffer containing the payload */
    uint8_t     modem_idx;      /*!< which modem must send this packet */
};

/**
@struct coord_s
@brief Geodesic coordinates
*/
struct coord_s {
    double  lat;    /*!< latitude [-90,90] (North +, South -) */
    double  lon;    /*!< longitude [-180,180] (East +, West -)*/
    short   alt;    /*!< altitude in meters (WGS 84 geoid ref.) */
};

