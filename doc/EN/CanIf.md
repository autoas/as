---
layout: post
title: AUTOSAR CanIf Configuration
category: AUTOSAR
comments: true
---

# AUTOSAR CanIf Configuration Guide

The CAN Interface (CanIf) sits between the CAN driver and the upper layers (CanTp, OsekNm, CanNm, CanTSyn, Xcp, SecOC, PduR and user modules). It performs CAN ID filtering, maps Rx/Tx PDUs to hardware objects (HOH) and dispatches indications and confirmations to the upper layer configured in the `up` field. This document describes the JSON configuration consumed by [CanIf.py](../../tools/generator/CanIf.py).

A complete real-world example is available at [app/app/config/Com/CanIf.json](../../app/app/config/Com/CanIf.json).

## 1. Top-level structure

```json
{
  "class": "CanIf",
  "RxPacketPoolSize": 8,
  "RxPacketDataSize": 64,
  "TxPacketPoolSize": 8,
  "TxPacketDataSize": 64,
  "networks": [
    {
      "name": "CAN0",
      "me": "AS",
      "dbc": "CAN0.dbc",
      "RxPdus": [ ... ],
      "TxPdus": [ ... ]
    }
  ]
}
```

| Field | Description |
| --- | --- |
| `RxPacketPoolSize` / `TxPacketPoolSize` | Number of pre-allocated packet buffers for the dynamic Rx/Tx packet pools (`CANIF_RX_PACKET_POOL_SIZE` / `CANIF_TX_PACKET_POOL_SIZE`, 0 disables the pool) |
| `RxPacketDataSize` / `TxPacketDataSize` | Payload size in bytes of each pooled packet (use 64 for CAN FD, 8 for classic CAN) |
| `networks` | One entry per CAN controller; the CanIf channel ID equals the network index and therefore the CAN controller ID |

## 2. Network configuration

Each network entry configures one CAN controller:

* `name` - network name, for example `CAN0`;
* `me` - node name of the local ECU in the DBC file;
* `dbc` - DBC database used to import the Com messages automatically. The DBC should contain only Com signals. Diagnostic (CanTp), NM, XCP and SecOC frames are listed explicitly in `RxPdus`/`TxPdus` and should be removed from the DBC;
* `ignore` - optional list of DBC message names that the generator must skip;
* `E2E` - optional list of DBC message names that require an E2E protect/check callout (emitted only when `USE_E2E` is defined).

All RxPdu and TxPdu names must be unique across the whole configuration.

## 3. RxPdu / TxPdu entries

```json
"RxPdus": [
  { "name": "P2P_RX", "id": "0x731", "hoh": 0, "up": "CanTp" },
  { "name": "OSEKNM0_RX", "id": "0x400", "mask": "0x700", "hoh": 0, "up": "OsekNm" }
],
"TxPdus": [
  { "name": "P2P_TX", "id": "0x732", "hoh": 0, "up": "CanTp" },
  { "name": "OSEKNM0_TX", "id": "0x400", "dynamic": true, "hoh": 0, "up": "OsekNm" }
]
```

| Field | Description |
| --- | --- |
| `name` | Unique PDU name. CanTp PDUs must follow `${channel}_RX` / `${channel}_TX` (see the CanTp document) |
| `id` | CAN ID in hex. Extended IDs set the 29-bit IDE flag |
| `mask` | Optional Rx acceptance mask; the hardware matches when `(received & mask) == (id & mask)`. Defaults to `0x1FFFFFFF` (exact match) |
| `hoh` | Hardware object handle: the controller-local HRH/HTH index. Multiple PDUs may share one HOH |
| `up` | Upper-layer module name: `CanTp`, `OsekNm`, `CanNm`, `CanTSyn`, `Xcp`, `SecOC`, `PduR` or a user callback (`User...`, see below) |
| `dynamic` | Optional Tx flag: the PDU is allocated from the Tx packet pool at runtime instead of using a static buffer |

## 4. Hardware object handles (HOH)

`hoh` numbers the hardware message boxes per controller. With two CAN controllers and four boxes each, the global enumeration looks like:

```c
enum {
  CAN0_HTH0 = 0,
  CAN0_HTH1 = 1,
  CAN0_HTH2 = 2,
  CAN0_HTH3 = 3,
  CAN1_HTH0 = 4,
  CAN1_HTH1 = 5,
  CAN1_HTH2 = 6,
  CAN1_HTH3 = 7,
};
```

## 5. User-defined upper layers

An `up` value starting with `User` registers a user callback pair instead of a stack module. Distinct suffixes such as `User0Rx` and `User0Tx` are recommended:

```c
/* called when the RxPdu configured with "up": "User0Rx" is received */
void User0Rx_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/* called after the TxPdu configured with "up": "User0Tx" is transmitted */
void User0Tx_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);
```

The user transmits on a configured TxPdu with:

```c
ret = CanIf_Transmit(CANIF_USER0_TX, &PduInfo);
```

## 6. Generator output

[CanIf.py](../../tools/generator/CanIf.py) merges the explicit Rx/Tx PDU lists with all messages imported from the DBC and generates `GEN/CanIf_Cfg.c` and `GEN/CanIf_Cfg.h` (plus the intermediate `GEN/CanIf.json`) in the configuration directory, for example under [app/app/config/Com/GEN](../../app/app/config/Com/GEN/).
