---
layout: post
title: AUTOSAR CanIf
category: AUTOSAR
comments: true
---

# Configuration notes for CanIf

Below is 1 examples:

* [application/CanIf.json](../../app/app/config/Com/CanIf.json)

```json
{
  "class": "CanIf",
  "networks": [
    {
      "name": "CAN0",
      "me": "AS",
      "dbc": "CAN0.dbc",
      "RxPdus": [
        { "name": "P2P_RX", "id": "0x731", "hoh": 0, "up": "CanTp" },
        ...
        { "name": "XCP_ON_CAN_RX", "id": "0x7A0", "hoh": 0, "up": "Xcp" },
        { "name": "USER0_RX", "id": "0x123", "hoh": 0, "up": "User0Rx" }
      ],
      "TxPdus": [
        { "name": "P2P_TX", "id": "0x732", "hoh": 0, "up": "CanTp" },
        ...
        { "name": "XCP_ON_CAN_TX", "id": "0x7B0", "hoh": 0, "up": "Xcp" },
        { "name": "USER0_TX", "id": "0x234", "hoh": 0, "up": "User0Tx" }
      ]
    }
  ]
}
```

## networks

For CanIf, networks is a list to specify the RxPdus and TxPdus for a CAN controller, the CanIf ID is the index of the network, and the CanIf ID and CAN controller ID are equal.

For the CanIf, each RxPdu and TxPdu must has a unique name, but as for the purpose to make things simple, the each network can be configure by specifiy the "dbc", but please note that all the message in the dbc must be the message for Com moudlue.

So if you have a dbc has Diagnotic/Nm/Tp or Xcp related Frame, please remove those frames from the dbc.

And the "me" is use to specifiy the Ecu node name that represent myself.

And please note that, under the parent directory of CanIf.json, file GEN/CanIf.json will be generated, in which you can see that all message in dbc converted into Rx/Tx Pdus.

* [GEN/CanIf.json](../../app/app/config/Com/GEN/CanIf.json)

For the "hoh", it represent the underlying CAN hardware object handle.

For TxPdu, "hoh" is the CAN hardware transfer object handle.
For RxPdu, "hoh" is the CAN hardware receiving object handle.

And for mulitiple CAN networks. For example, both CAN0 has 4 transfer message boxs, so the hoh for transfer ID is as below:

```c
enum {
  CAN0_HTH0 = 0,
  CAN0_HTP1 = 1,
  CAN0_HTH2 = 2,
  CAN0_HTP3 = 3,
  CAN1_HTH0 = 4,
  CAN2_HTP1 = 5,
  CAN3_HTH2 = 6,
  CAN4_HTP3 = 7,
}
```

### User defined CanIf

The key "up" is a string must be started with "User", and always suggest that each should have a unique suffix, such as the above json example, but OK if they have the same suffix.

The callback API will be:

```c
// For Rx Pdu, the RxPduId will the CANIF symbol ID
void ${up}_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

// For Tx Pdu, the TxPduId will the CANIF symbol ID
void ${up}_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);

// and using the generated TX Pdu symbol in CanIf_Cfg.h to reqeust transmit
ret = CanIf_Transmit(CANIF_USER0_TX, ...);
```



## Genetator

* [Genetator CanIf.py](../../tools/generator/CanIf.py)