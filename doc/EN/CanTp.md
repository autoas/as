---
layout: post
title: AUTOSAR CanTp Configuration
category: AUTOSAR
comments: true
---

# Configuration Notes for CanTp

## Example Configuration

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

## Channel Configuration

CanTp uses a channel-based design where each configured channel automatically includes:
- 1 TxPdu
- 1 RxPdu

Refer to the generated [`CanTp_Cfg.c`](../../app/app/config/CanTp/GEN/CanTp_Cfg.c):

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
  /* @ECUC_CanTp_00252: Minimum time between transmissions of two CF N-PDUs */
  uint8_t STmin;
  uint8_t BS;
  uint8_t N_TA; /* Only required for CANTP_EXTENDED addressing */
  uint8_t CanTpRxWftMax; /* Cannot be 0xFF */
  uint8_t LL_DL; /* 8 for CAN, 64 for CAN FD */
  uint8_t padding;
  uint8_t *data; /* Data buffer for TP frame transmission */
} CanTp_ChannelConfigType;
```

### Configuration Parameters

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

## Important Notes

1. This configuration provides basic functionality. For advanced parameters, modify the generated `CanTp_Cfg.c` directly.

2. Each CanTp channel requires corresponding PDU configurations in `CanIf.json` with specific naming:
   - RxPdu: `${name}_RX`
   - TxPdu: `${name}_TX`

Example:
```json
"RxPdus": [
  { "name": "P2P_RX", "id": "0x731", "hoh": 0, "up": "CanTp" },
  { "name": "P2A_RX", "id": "0x7DF", "hoh": 0, "up": "CanTp" }
],
"TxPdus": [
  { "name": "P2P_TX", "id": "0x732", "hoh": 0, "up": "CanTp" },
  { "name": "P2A_TX", "id": "0x732", "hoh": 0, "up": "CanTp" }
]
```

## Generator

Configuration is processed by: [CanTp.py](../../tools/generator/CanTp.py)