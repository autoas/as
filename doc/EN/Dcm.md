---
layout: post
title: AUTOSAR DCM
category: AUTOSAR
comments: true
---

# Configuration Notes for AUTOSAR DCM Module

The Diagnostic Communication Manager (DCM) handles diagnostic requests, session management, security access, and data exchange in AUTOSAR systems. This document outlines key configuration parameters, including session definitions, security levels, service mappings, and memory access rules.

---

## 1. Example Configurations
For practical implementations, see:  
- [Bootloader Dcm.json](../../app/bootloader/config/Dcm.json) (Bootloader-specific DCM setup)  
- [Application Dcm.json](../../app/app/config/Dcm/Dcm.json) (Application-layer DCM configuration)  

---

## 2. Session Configuration
Define supported diagnostic sessions with unique IDs. Sessions control access to services and security levels.

### Example Session Definitions:
```json
"sessions": [
  { "name": "Default", "id": "0x01" },    // Default session (e.g., normal operation)
  { "name": "Program", "id": "0x02" },   // Programming session (e.g., ECU flashing)
  { "name": "Extended", "id": "0x03" },  // Extended diagnostic session
  { "name": "Factory", "id": "0x50" }    // Factory calibration session
]
```

---

## 3. Security Level Configuration
Configure security access requirements for specific sessions, including seed/key APIs for authentication.

### Example Security Definitions:
```json
"securities": [
  { 
    "name": "Extended", 
    "level": 1,                // Security level (0¨C255; 0 = no security)
    "size": 4,                 // Size of seed/key (bytes)
    "sessions": ["Extended"],  // Sessions requiring this security level
    "API": { 
      "seed": "App_GetExtendedSessionSeed",  // Function to generate seed
      "key": "App_CompareExtendedSessionKey" // Function to validate key
    } 
  },
  { 
    "name": "Program", 
    "level": 2, 
    "size": 4, 
    "sessions": ["Program"],   // Only "Program" session requires this security
    "API": { 
      "seed": "App_GetProgramSessionSeed", 
      "key": "App_CompareProgramSessionKey"
    } 
  }
]
```

---

## 4. Service Configuration
The DCM generator uses a `ServiceMap` to map service IDs to their implementation details. Services define how diagnostic requests (e.g., read data, write memory) are processed.

### Common Service Attributes:
| Attribute | Required? | Description                                                                 |
|-----------|-----------|-----------------------------------------------------------------------------|
| `name`    | No        | Human-readable name for documentation (e.g., "read_did").                   |
| `id`      | Yes       | Unique service ID (e.g., `0x22` for Read Data by Identifier).               |
| `sessions`| No        | Allowed sessions (empty = all sessions).                                    |
| `securities`| No       | Required security levels (empty = no security).                             |
| `access`  | No        | Access type: `"physical"`, `"functional"`, or both (default: both if empty). |
| `API`     | Conditional| Service-specific callback definitions (e.g., seed/key functions for security). |

### Example: Read Data by Identifier (0x22)
This service reads data by Data Identifier (DID) and requires mapping DIDs to their storage locations/APIs.  
```json
{
  "name": "read_did",
  "id": "0x22",
  "DIDs": [  // List of supported DIDs for this service
    { 
      "name": "FingerPrint", 
      "id": "0xF15B", 
      "size": 10, 
      "API": "App_ReadFingerPrint"  // Callback to read FingerPrint DID
    },
    { 
      "name": "TestDID1", 
      "id": "0xAB01", 
      "size": 10, 
      "API": "App_ReadAB01"  // Callback to read TestDID1
    }
  ]
}
```

### DID Configuration:
Individual DIDs can also be defined with granular access controls:  
```json
{ 
  "name": "DID_name",       // e.g., "EngineHours"
  "id": "DID_ID",          // e.g., "0xF123"
  "size": "data_length",   // Data length (bytes, e.g., "4")
  "API": "callback_function",  // Function to read/write DID data
  "sessions": [...],       // Allowed sessions (empty = all)
  "securities": [...],     // Required security levels (empty = none)
  "access": [...]          // "physical", "functional", or both
}
```

---

## 5. Memory Access Configuration
Configure memory regions and services for reading/writing raw memory addresses (e.g., `0x23` for Read Memory by Address, `0x3D` for Write Memory by Address).

### Example Memory Configuration:
```json
"memories": [  // Define memory regions accessible via DCM services
  { 
    "name": "Memory1", 
    "low": 0,              // Start address (decimal or hex)
    "high": "0x100000",    // End address (hex recommended)
    "attr": "rw"           // Access flags: "r" (read-only), "w" (write-only), "rw" (read-write)
  },
  { 
    "name": "Memory2", 
    "low": "0x300000",     // Hex start address
    "high": "0x400000",    // Hex end address
    "attr": "r"            // Read-only memory
  }
],
"memory.format": ["0x44"],  // Optional: Memory data format (e.g., hex encoding)
"services": [               // List of enabled memory services
  { "name": "read_memory_by_address", "id": "0x23" },
  { "name": "write_memory_by_address", "id": "0x3D" }
]
```

### Memory Region Attributes:
| Attribute | Description                                                                 |
|-----------|-----------------------------------------------------------------------------|
| `name`    | Human-readable name for the memory region (e.g., "CalibrationData").         |
| `low`     | Start address of the memory region (decimal or hex).                        |
| `high`    | End address of the memory region (decimal or hex).                          |
| `attr`    | Access permissions: `"r"` (read-only), `"w"` (write-only), or `"rw"` (both). |
| `sessions`| Allowed sessions (empty = all sessions).                                    |
| `securities`| Required security levels (empty = no security).                             |
| `access`  | Access type: `"physical"` (direct memory access) or `"functional"` (logical access). |

---

## 6. Generator Tool
Configuration is processed by the [Dcm.py](../../tools/generator/Dcm.py) script, which converts JSON definitions into C code (e.g., service handlers, session management logic).