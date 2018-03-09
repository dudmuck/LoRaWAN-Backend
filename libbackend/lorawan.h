/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <stdint.h>

#define LORA_MACHEADERLENGTH            1
#define LORA_FRAMEMICBYTES              4

#define LORA_CYPHERKEYBYTES             16
#define LORA_CYPHERKEY_STRLEN           ((LORA_CYPHERKEYBYTES * 2) + 1)
#define LORA_KEYENVELOPE_LEN            (LORA_CYPHERKEYBYTES + 8)
#define LORA_AUTHENTICATIONBLOCKBYTES   16
#define LORA_ENCRYPTIONBLOCKBYTES       16

#define LORA_EUI_LENGTH                 8

#define BEACON_WINDOW_SLOTS                         4096

#define MAX_CH_MASKS        6       /**< size of channel mask */

/** @brief LoRaWAN mac header
 */
typedef union {
    struct {
        uint8_t major   : 2;    // 0 1
        uint8_t rfu     : 3;    // 2 3 4
        uint8_t MType   : 3;    // 5 6 7
    } bits; /**< mac header bits */
    uint8_t octet;  /**< mac header byte */
} mhdr_t;

typedef enum {
    MTYPE_JOIN_REQ = 0,
    MTYPE_JOIN_ACC,//1
    MTYPE_UNCONF_UP,//2
    MTYPE_UNCONF_DN,//3
    MTYPE_CONF_UP,//4
    MTYPE_CONF_DN,//5
    MTYPE_REJOIN_REQ,//6
    MTYPE_P,//7
} mtype_e;

/** @brief join request uplink from mote */
typedef struct {
    mhdr_t mhdr;    /**< message header */
    uint8_t JoinEUI[LORA_EUI_LENGTH];    /**< applicaton EUI unencrypted */
    uint8_t DevEUI[LORA_EUI_LENGTH];    /**< device EUI unencrypted */
    uint16_t DevNonce;  /**< nonce from end-node */
} __attribute__((packed)) join_req_t;

typedef struct {
    mhdr_t mhdr;    /**< message header */
    uint8_t type;
    uint8_t NetID[3];
    uint8_t DevEUI[LORA_EUI_LENGTH];    /**< device EUI unencrypted */
    uint16_t RJcount0;
} __attribute__((packed)) rejoin02_req_t;

typedef struct {
    mhdr_t mhdr;    /**< message header */
    uint8_t type;
    uint8_t JoinEUI[LORA_EUI_LENGTH];    /**< applicaton EUI unencrypted */
    uint8_t DevEUI[LORA_EUI_LENGTH];    /**< device EUI unencrypted */
    uint16_t RJcount1;
} __attribute__((packed)) rejoin1_req_t;

/** @brief frame control byte of LoRaWAN header */
typedef union {
    struct {
        uint8_t FOptsLen        : 4;    // 0 1 2 3
        uint8_t FPending        : 1;    // 4
        uint8_t ACK             : 1;    // 5
        uint8_t ADRACKReq       : 1;    // 6
        uint8_t ADR             : 1;    // 7
    } dlBits;   /**< downlink, gateway transmit */
    struct {
        uint8_t FOptsLen        : 4;    // 0 1 2 3
        uint8_t classB          : 1;    // 4    unused in classA
        uint8_t ACK             : 1;    // 5
        uint8_t ADRACKReq       : 1;    // 6
        uint8_t ADR             : 1;    // 7
    } ulBits;   /**< uplink, gateway receive */
    uint8_t octet;  /**< entire byte */
} FCtrl_t;

/** @brief LoRaWAN frame header */
typedef struct {
    uint32_t DevAddr;   /**< device address */
    FCtrl_t FCtrl;  /**< frame control */
    uint16_t FCnt;  /**< sequence number */
} __attribute__((packed)) fhdr_t;

#define MINIMUM_PHY_SIZE    (LORA_MACHEADERLENGTH + sizeof(fhdr_t) + LORA_FRAMEMICBYTES)
#define MAXIMUM_FRM_SIZE    (255 - MINIMUM_PHY_SIZE)

typedef enum eLoRaMacMoteCmd
{
    MOTE_MAC_RESET_IND               = 0x01,
    /*!
     * LinkCheckReq
     */
    MOTE_MAC_LINK_CHECK_REQ          = 0x02,
    /*!
     * LinkADRAns
     */
    MOTE_MAC_LINK_ADR_ANS            = 0x03,
    /*!
     * DutyCycleAns
     */
    MOTE_MAC_DUTY_CYCLE_ANS          = 0x04,
    /*!
     * RXParamSetupAns
     */
    MOTE_MAC_RX_PARAM_SETUP_ANS      = 0x05,
    /*!
     * DevStatusAns
     */
    MOTE_MAC_DEV_STATUS_ANS          = 0x06,
    /*!
     * NewChannelAns
     */
    MOTE_MAC_NEW_CHANNEL_ANS         = 0x07,
    /*!
     * RXTimingSetupAns
     */
    MOTE_MAC_RX_TIMING_SETUP_ANS     = 0x08,
    MOTE_MAC_TX_PARAM_SETUP_ANS      = 0x09,

    MOTE_MAC_REKEY_IND               = 0x0b,
    MOTE_MAC_DEVICE_TIME_REQ         = 0x0d,
    MOTE_MAC_REJOIN_PARAM_ANS        = 0x0f,
    /*!
     * PingSlotInfoReq
     */
    MOTE_MAC_PING_SLOT_INFO_REQ      = 0x10,
    /*!
     * PingSlotFreqAns
     */
    MOTE_MAC_PING_SLOT_FREQ_ANS      = 0x11,
    /*!
     * BeaconTimingReq
     */
    MOTE_MAC_BEACON_TIMING_REQ       = 0x12,
    /*!
     * BeaconFreqAns
     */
    MOTE_MAC_BEACON_FREQ_ANS         = 0x13,
    MOTE_MAC_DEVICE_MODE_IND         = 0x20
}LoRaMacMoteCmd_t;

typedef enum eLoRaMacSrvCmd
{
    SRV_MAC_RESET_CONF               = 0x01,
    /*!
     * LinkCheckAns
     */
    SRV_MAC_LINK_CHECK_ANS           = 0x02,
    /*!
     * LinkADRReq
     */
    SRV_MAC_LINK_ADR_REQ             = 0x03,
    /*!
     * DutyCycleReq
     */
    SRV_MAC_DUTY_CYCLE_REQ           = 0x04,
    /*!
     * RXParamSetupReq
     */
    SRV_MAC_RX_PARAM_SETUP_REQ       = 0x05,
    /*!
     * DevStatusReq
     */
    SRV_MAC_DEV_STATUS_REQ           = 0x06,
    /*!
     * NewChannelReq
     */
    SRV_MAC_NEW_CHANNEL_REQ          = 0x07,
    /*!
     * RXTimingSetupReq
     */
    SRV_MAC_RX_TIMING_SETUP_REQ      = 0x08,
    SRV_MAC_TX_PARAM_SETUP_REQ       = 0x09,

    SRV_MAC_REKEY_CONF               = 0x0b,
    SRV_MAC_DEVICE_TIME_ANS          = 0x0d,
    SRV_MAC_FORCE_REJOIN_REQ         = 0x0e,
    SRV_MAC_REJOIN_PARAM_REQ         = 0x0f,
    /*!
     * PingSlotInfoAns
     */
    SRV_MAC_PING_SLOT_INFO_ANS       = 0x10,
    /*!
     * PingSlotChannelReq
     */
    SRV_MAC_PING_SLOT_CHANNEL_REQ    = 0x11,
    /*!
     * BeaconTimingAns
     */
    SRV_MAC_BEACON_TIMING_ANS        = 0x12,
    /*!
     * BeaconFreqReq
     */
    SRV_MAC_BEACON_FREQ_REQ          = 0x13,
    SRV_MAC_DEVICE_MODE_CONF         = 0x20
} LoRaMacSrvCmd_t;

/**
 * LoRaMAC receive window 2 channel parameters
 */
typedef struct sRx2ChannelParams
{
    /**
     * Frequency in Hz
     */
    uint32_t FrequencyHz;
    /**
     * Data rate
     *
     * EU868 - [DR_0, DR_1, DR_2, DR_3, DR_4, DR_5, DR_6, DR_7]
     *
     * US915 - [DR_8, DR_9, DR_10, DR_11, DR_12, DR_13]
     */
    uint8_t  Datarate;
} Rx2ChannelParams_t;

#define DR_0        0
#define DR_1        1
#define DR_2        2
#define DR_3        3
#define DR_4        4
#define DR_5        5
#define DR_6        6
#define DR_7        7
#define DR_8        8
#define DR_9        9
#define DR_10       10
#define DR_11       11
#define DR_12       12
#define DR_13       13
#define DR_14       14
#define DR_15       15

#define CLASS_A     0
#define CLASS_C     2
/** \endcond */
