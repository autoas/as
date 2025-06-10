---
layout: post
title: AUTOSAR CanTp
category: AUTOSAR
comments: true
---

# Configuration notes for CanTp

Below is 1 examples:

* [application/CanTp.json](../../app/app/config/CanTp/CanTp.json)

```json
{
  "class": "CanTp",
  "channels": [
    {
      "name": "P2P"
    },
    {
      "name": "P2A"
    }
  ]
}
```

## Channels

Please note that CanTp is a channel based design, it means that for each configured channel, it will automatically has 1 TxPdu and 1 RxPdu, check the generated [CanTp_Cfg.c](../../app/app/config/CanTp/GEN/CanTp_Cfg.c)

```c
typedef struct {
  CanTp_AddressingFormatType AddressingFormat;
  PduIdType CanIfTxPduId;
  PduIdType PduR_RxPduId;
  PduIdType PduR_TxPduId;
  /* Time for transmission of the CAN frame (any N-PDU) on the sender side */
  uint16_t N_As;
  /* Time until reception of the next flow control N-PDU (see ISO 15765-2) */
  uint16_t N_Bs;
  /* Time until reception of the next consecutive frame N-PDU (see ISO 15765-2) */
  uint16_t N_Cr;
  /* @ECUC_CanTp_00252: Sets the duration of the minimum time the CanTp sender shall wait
   * between the transmissions of two CF N-PDUs.*/
  uint8_t STmin;
  uint8_t BS;
  uint8_t N_TA; /* if addressing format is CANTP_EXTENDED */
  /* @ECUC_CanTp_00251 */
  uint8_t CanTpRxWftMax; /* can't not be 0xFF */
  uint8_t LL_DL;         /* for CAN it was 8, for CANFD it was 64 */
  uint8_t padding;
  uint8_t *data; /* data buffer to TP frame transmission */
} CanTp_ChannelConfigType;

  {
    /* P2P */
    CANTP_STANDARD,
    CANIF_P2P_TX,
    PDUR_P2P_RX /* PduR_RxPduId */,
    PDUR_P2P_TX /* PduR_TxPduId */,
    CANTP_CONVERT_MS_TO_MAIN_CYCLES(CANTP_CFG_N_As),
    CANTP_CONVERT_MS_TO_MAIN_CYCLES(CANTP_CFG_N_Bs),
    CANTP_CONVERT_MS_TO_MAIN_CYCLES(CANTP_CFG_N_Cr),
    CANTP_CFG_STMIN,
    CANTP_CFG_BS,
    0 /* N_TA */,
    CANTP_CFG_RX_WFT_MAX,
    CANTP_LL_DL,
    CANTP_CFG_PADDING,
    u8P2PData,
  },
```

```json
    {
      "name": "Channel Name",
      "N_TA": "optional, only need if EXTENDED CanTp",
      "N_As": "Time for transmission of the CAN frame (any N-PDU) on the sender side",
      "N_Bs": "Time until reception of the next flow control N-PDU (see ISO 15765-2)",
      "N_Cr": "Time until reception of the next consecutive frame N-PDU (see ISO 15765-2)",
      "STmin": "Sets the duration of the minimum time the CanTp sender shall wait between the transmissions of two CF N-PDUs",
      "BS": "Block Size",
      "WftMax": "how many Flow Control wait N-PDUs can be consecutively transmitted by the receiver",
      "LL_DL": "Low level frame maximum data length, for CAN, it was 8, for CANFD, it was 64",
      "padding": "padding value"
    }
```

For now, this CanTp configuration is very simple, for any other parameters, please update the generated CanTp_Cfg.c file directly.

And please note that for each CanTp channel, you need configure its RxPdu and TxPdu in CanIf.json. the name must be `${name}_RX` and `${name}_TX` in CanIf.

```json
...
      "RxPdus": [
        { "name": "P2P_RX", "id": "0x731", "hoh": 0, "up": "CanTp" },
        { "name": "P2A_RX", "id": "0x7DF", "hoh": 0, "up": "CanTp" },
        ...
      ],
      "TxPdus": [
        { "name": "P2P_TX", "id": "0x732", "hoh": 0, "up": "CanTp" },
        { "name": "P2A_TX", "id": "0x732", "hoh": 0, "up": "CanTp" },
        ...
      ]
...
```


## Genetator

* [Genetator CanTp.py](../../tools/generator/CanTp.py) 
