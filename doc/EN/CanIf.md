---
layout: post
title: AUTOSAR CanIf
category: AUTOSAR
comments: true
---

# AUTOSAR CanIf Configuration Guide

## Configuration Example

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

## Network Configuration

### Key Concepts
- Each network entry represents a CAN controller configuration
- The CanIf channel ID equals the network index and corresponds to the CAN controller ID
- All RxPdu and TxPdu names must be unique across the configuration

### DBC File Usage
- The "dbc" field specifies the database file for message definitions
- **Important**: The DBC should only contain messages for the COM module
- Remove diagnostic, NM, TP, or XCP-related frames from the DBC before use

### Node Identification
- The "me" field specifies the ECU node name representing the current module

### Generated Configuration
- The generator creates `GEN/CanIf.json` in the parent directory
- This file contains all DBC messages converted to Rx/Tx PDUs

## Hardware Object Handles (HOH)

### Definition
- **TxPdu**: Represents CAN hardware transfer object handles
- **RxPdu**: Represents CAN hardware receiving object handles

### Multiple Network Example
For systems with multiple CAN networks (each with 4 message boxes):

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

## User-Defined CanIf Configuration

### Naming Convention
- The "up" field must begin with "User"
- Recommended to use unique suffixes (e.g., "User0Rx", "User0Tx")
- Identical suffixes are permitted but not recommended

### Callback API

#### Receive Callback
```c
void ${up}_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);
```

#### Transmission Callback
```c
void ${up}_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);
```

#### Transmission Request
```c
ret = CanIf_Transmit(CANIF_USER0_TX, ...);
```

## Generator Implementation

The configuration is processed by: [CanIf.py](../../tools/generator/CanIf.py)
