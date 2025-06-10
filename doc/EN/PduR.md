---
layout: post
title: AUTOSAR PduR
category: AUTOSAR
comments: true
---

# Configuration notes for PduR

Below is 1 examples:

* [application/PduR.json](../../app/app/config//Com/PduR.json)

```json
{
  "class": "PduR",
  "routines" : [
    {
      "name": "P2P_RX",
      "from": "CanTp",
      "to": "Dcm"
    },
    {
      "name": "P2P_TX",
      "from": "Dcm",
      "to": "CanTp"
    },
    {
      "name": "P2A_RX",
      "from": "CanTp",
      "to": "Dcm"
    },
    {
      "name": "P2A_TX",
      "from": "Dcm",
      "to": "CanTp"
    }
  ],
  "networks": [
    {
      "name": "CAN0",
      "network": "CAN",
      "me": "AS",
      "dbc": "CAN0.dbc"
    }
  ]
}
```


## networks

For PduR, networks are used to specify those Pdus from/to the Com module, almost the same as [CanIf networks configuration](./CanIf.md#L37) but without RxPdu and TxPdu list.

So the routines, are use to specify those message from/to CanTp/Dcm/Nm/LinTp, etc.


And please note that, under the parent directory of PduR.json, file GEN/PduR.json will be generated, in which you can see that all message in dbc converted into routines.

* [GEN/PduR.json](../../app/app/config/Com/GEN/PduR.json)


## routines

```json
    {
      "name": "SOURCE_PDU_NAME",
      "from": "SOURCE_MODULE",
      "to": "DEST_MODULE",
      "dest": "DEST_PDU_NAME"
    },
```

The "dest" is optional, as generally, the "dest" is the same as "name".

But for some PduR gateway case, such as a message from CAN0 routine to CAN1, below is a configuration example:


```json
    {
      "name": "CAN0_MSG0",
      "from": "CanIf",
      "to": "CanIf",
      "dest": "CAN1_MSG0"
    },
```

Please note that for now, PduR doesn't support gateway between LinIf and CanIf.


## Genetator

* [Genetator PduR.py](../../tools/generator/PduR.py)


### Design notes for PduR

```
  The PduR Base Ids looks very weird and it's really not a good design, and it can cover only some simple routines.
 And the routines must be specially configured with correct sorted orders.
 But the reason for this is that LinTp/CanTp and Dcm are channel based design to make it simple, and this issue can
 be resolved that each module hold the right routine table Id when call PduR API, thus can configure all the base id as 0.
                                              `  To Make things simple, the PduR routines are grouped according to "from"/"to"
   +-------+                                  `   [x]: x is the routine path id.
   |  Dcm  |                                  `   [0] = { from: DoIp  0, to: Dcm   0 }  <- DOIP_RX_BASE_ID
   +---0---+                                  `   [1] = { from: DoIp  1, to: CanTp 0 }  <- CANTP_TX_BASE_ID
      ^ |                                     `   [2] = { from: DoIp  2, to: CanTp 1 }
      | v                                     `   [3] = { from: Dcm   0, to: DoIp  0 }  <- DOIP_TX_BASE_ID / DCM_TX_BASE_ID
  +---|-|----------------------------------+  `   [4] = { from: CanTp 0, to: DoIp  1 }  <- CANTP_RX_BASE_ID
  |   | |          PduR                    |  `   [5] = { from: CanTp 1, to: DoIp  2 }
  |   | |  +--------------------------+    |  `
  |   | |  | +----------------------+ |    |  `  The DoIP 1 get a request, thus forward it to CanTp 1, CanTp 1 will call PduR Tx
  |   | |  | |   +---------------+  | |    |  ` Related API with PduId 1, and the PduR will add the PduId with CANTP_TX_BASE_ID,
  |   | |  | |   | +-----------+ |  | |    |  ` thus in this case, get the correct routine path id 2.
  +---|-|--|-|---|-|-----------|-|--|-|----+  `  Thus, when CanTp 1 has a response, it calls PduR Rx related API with PduId 1,
      ^ |  ^ |   ^ |           ^ |  ^ |       ` and the PduR will add the PduId with CANTP_RX_BASE_ID, thus in this case, get
      | V  | V   | V           | V  | V       ` the correct routine path id 5.
  +----0----1-----2----+    +---0----1---+    `
  |       DoIP         |    |   CanTp    |    `
  +--------------------+    +------------+    `

 This design works for gateway cross TP, it will not works for routines in the same TP as below case.

                                              `
   +-------+                                  `   [x]: x is the routine path id.
   |  Dcm  |                                  `   [0] = { from: CanTp 0, to: Dcm   0 }  <- CANTP_RX_BASE_ID
   +---0---+                                  `   [1] = { from: CanTp 1, to: CanTp 4 }  <- CANTP_TX_BASE_ID
      ^ |                                     `   [2] = { from: CanTp 2, to: CanTp 3 }
      | v                                     `   [3] = { from: CanTp 3, to: CanTp 2 }
  +---|-|----------------------------------+  `   [4] = { from: CanTp 4, to: CanTp 1 }
  |   | |          PduR                    |  `   [5] = { from: Dcm   0, to: CanTp 0 }  <- DCM_TX_BASE_ID
  |   | |  +--------------------------+    |  `
  |   | |  | +----------------------+ |    |  `  e.g: CanTp 3 call PduR Tx Api with PduId 3, thus routine path id is 3+1 which is 4,
  |   | |  | |   +---------------+  | |    |  ` it's totally wrong, the correct routine path id is 1 in this case.
  |   | |  | |   | +-----------+ |  | |    |  `
  +---|-|--|-|---|-|-----------|-|--|-|----+  `  So in thus case, all the PduR based id must be configured with 0, and each module
      ^ |  ^ |   ^ |           ^ |  ^ |       ` must call PduR API with the right routhine path id.
      | V  | V   | V           | V  | V       `
  +----0----1-----2---------- --3----4----+   `  And for the underlying Tp, for example for the TP pair 2&3, the TP configuration:
  |               CanTp                   |   `    TP Channel 2: { Rx: PDUR_ID_2_RX=[2], Tx: PDUR_ID_3_RX=[3] }
  +---------------------------------------+   `    TP Channel 3: { Rx: PDUR_ID_3_RX=[3], Tx: PDUR_ID_2_RX=[2] }
                                              `  Please note that the TP Channel Tx referece to the peer.
```