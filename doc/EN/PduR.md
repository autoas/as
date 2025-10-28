---
layout: post
title: AUTOSAR PduR
category: AUTOSAR
comments: true
---

# AUTOSAR PduR Configuration Overview

The **PDU Router (PduR)** in AUTOSAR manages communication between software modules (e.g., `CanTp`, `Dcm`, `LinTp`) by routing Protocol Data Units (PDUs) across the system. This document explains how to configure PduR networks and routines, leveraging JSON definitions and automated code generation.

---

## 1. Core Concepts

### 1.1 What is PduR?  
PduR acts as a software router that:  
- Routes PDUs between upper-layer modules (e.g., `Dcm` ? `CanTp`).  
- Supports gateway scenarios (e.g., routing a CAN0 PDU to a CAN1 PDU via `CanIf`).  
- Relies on `Com` and `CanIf` for physical bus communication (PduR itself does not handle hardware).  

### 1.2 Key Components  
- **Networks**: Define logical communication channels between modules (e.g., `CAN0` for CAN-based PDUs).  
- **Routines**: Map PDU names to their source/destination modules (e.g., route `CAN0_MSG0` from `CanIf` to `Dcm`).  

---

## 2. PduR JSON Configuration Example

A typical PduR configuration includes `networks` (bus definitions) and `routines` (PDU routing rules).  

### Example: [application/PduR.json](../../app/app/config/Com/PduR.json)
```json
{
  "class": "PduR",       // Fixed class identifier for PduR configurations
  "routines": [          // List of PDU routing rules
    {
      "name": "P2P_RX",  // Routine name (P2P = Peer-to-Peer)
      "from": "CanTp",   // Source module (e.g., CanTp forward received PDU to Dcm)
      "to": "Dcm"        // Destination module
    },
    {
      "name": "P2P_TX",  // Routine name
      "from": "Dcm",     // Source module (Dcm sends PDU to CanTp)
      "to": "CanTp"
    },
    {
      "name": "P2A_RX",  // Routine name (P2A = Peer-to-Application)
      "from": "CanTp",   // Source: CanTp
      "to": "Dcm"        // Destination: Dcm
    },
    {
      "name": "P2A_TX",  // Routine name
      "from": "Dcm",     // Source: Dcm
      "to": "CanTp"      // Destination: CanTp
    }
  ],
  "networks": [          // List of logical networks (bus interfaces)
    {
      "name": "CAN0",    // Logical network name (matches DBC file)
      "network": "CAN",  // Physical network type (e.g., "CAN", "LIN")
      "me": "AS",        // Module entity (e.g., "AS" = Application Software)
      "dbc": "CAN0.dbc"  // Path to DBC file defining CAN messages
    }
  ]
}
```

---

## 3. Networks Configuration

### 3.1 Purpose of Networks  
Networks in PduR define **logical communication channels** (e.g., CAN0, LIN0) and link them to physical bus parameters (via DBC files). Unlike `CanIf` (which handles physical bus configuration), PduR networks focus on **inter-module routing**.  

### 3.2 Key Attributes  
| Attribute | Type       | Description                                                                 |
|-----------|------------|-----------------------------------------------------------------------------|
| `name`    | String     | Logical name of the network (e.g., `CAN0`).     |
| `network` | String     | Physical network type (e.g., `"CAN"` for Controller Area Network).          |
| `me`      | String     | Module entity (e.g., `"AS"` = Application Software; maps to ECU architecture).|
| `dbc`     | String     | Path to the DBC file defining messages, IDs, and signals for this network.   |

---

## 4. Routines Configuration

### 4.1 Purpose of Routines  
Routines define **how PDUs are routed between modules**. Each routine maps a PDU name to its source and destination modules.  

### 4.2 Routine Structure

```json
    {
      "name": "SOURCE_PDU_NAME",
      "from": "SOURCE_MODULE",
      "to": "DEST_MODULE",
      "dest": "DEST_PDU_NAME"
    },
```

A routine is defined by:  
- `name`: Unique identifier for the routing rule (often derived from the PDU name).  
- `from`: Source module (e.g., `CanTp`, `Dcm`).  
- `to`: Destination module (e.g., `Dcm`, `CanTp`).  
- `dest` (Optional): Renamed PDU at the destination (defaults to `name` if omitted).  

### 4.3 Example Scenarios  

#### Basic Peer-to-Peer Routing  
Route a PDU from `CanTp` to `Dcm` (common for diagnostic requests):  
```json
{
  "name": "P2P_RX",
  "from": "CanTp",
  "to": "Dcm"
}
```

#### Gateway Routing (CAN0 ¡ú CAN1)  
Route a CAN0 PDU to a CAN1 PDU (e.g., for cross-bus communication):  
```json
{
  "name": "CAN0_MSG0",
  "from": "CanIf",
  "to": "CanIf",
  "dest": "CAN1_MSG0"  // Rename PDU to match CAN1's DBC definition
}
```

### 4.4 Limitations  
- **No LinIf-CANIF Gateway**: PduR currently does not support routing between `LinIf` (LIN) and `CanIf` (CAN).  
- **No Physical Bus Handling**: PduR relies on `CanIf`/`LinIf` for physical bus communication (e.g., bit timing, arbitration).  

---

## 5. Code Generation with `PduR.py`  

The [Generator PduR.py](../../tools/generator/PduR.py) script automates PduR configuration by:  
1. **Parsing DBC Files**: Converts message definitions from `CAN0.dbc` (or other DBCs) into PduR routines.  
2. **Generating GEN/PduR.json**: Produces a system-ready configuration file that maps DBC messages to PduR routines.  
3. **Validating Syntax**: Ensures JSON compliance and correct module references.  