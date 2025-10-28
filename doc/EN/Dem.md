---
layout: post
title: AUTOSAR Dem
category: AUTOSAR
comments: true
---

# Configuration Notes for AUTOSAR Dem

This document provides guidance for configuring the Diagnostic Event Manager (Dem) in AUTOSAR systems, including memory setup, DTC definitions, and extended data handling. A [simple configuration example](../../app/app/config/Dcm/Dem.json) is included to illustrate key concepts.

---

## 1. Memories Configuration
The `Memories` section defines storage locations for DTC status, snapshots, and extended data. At minimum, a `Primary` memory must be specified (mandatory), while `Mirror` memory is optional.

### Example JSON Snippet:
```json
"Memories": [
  {
    "name": "Primary",
    "origin": "0x0001"  // Mandatory primary storage
  },
  {
    "name": "Mirror",
    "origin": "0x0002"  // Optional mirrored storage
  }
]
```

---

## 2. General Configuration
The `general` section specifies default attributes for DTCs if they are not explicitly defined in individual DTC entries. Refer to the [Generator Dem.py](../../tools/generator/Dem.py) API `GetProp` for details.

### Example JSON Snippet:
```json
"general": {
  "AgingCycleCounterThreshold": 5,
  "ConfirmationThreshold": 2,
  "OccurrenceCounterProcessing": "TF",
  "FreezeFrameRecordTrigger": "TEST_FAILED",
  "ExtendedDataRecordTrigger": "TEST_FAILED",
  "DebounceCounterDecrementStepSize": 2,
  "DebounceCounterFailedThreshold": 10,
  "DebounceCounterIncrementStepSize": 1,
  "DebounceCounterJumpDown": false,
  "DebounceCounterJumpDownValue": 0,
  "DebounceCounterJumpUp": true,
  "DebounceCounterJumpUpValue": 0,
  "DebounceCounterPassedThreshold": -10
}
```

---

## 3. DTC Definitions
The `DTCs` section lists all supported Diagnostic Trouble Codes (DTCs) with their specific properties.

### Example JSON Snippet:
```json
"DTCs": [
  {
    "name": "DTC0",
    "number": "0x112200",
    "conditions": ["BatteryNormal"],  // List of condition names (max 32)
    "destination": ["Primary", "Mirror"],  // Storage locations for the DTC
    "priority": 0,  // Lower value = higher priority (higher priority DTCs may displace lower ones)
    "OperationCycleRef": "IGNITION"  // Operation cycle this DTC belongs to
  },
  // Additional DTC entries...
]
```

### Key DTC Attributes:
| Attribute             | Description                                                                 |
|-----------------------|-----------------------------------------------------------------------------|
| `name`                | Human-readable name for the DTC (e.g., "DTC0").                            |
| `number`              | Unique DTC identifier (e.g., "0x112200").                                  |
| `conditions`          | List of condition names (Dem generator auto-assigns IDs; max 32 conditions).|
| `destination`         | List of memory names (from `Memories`) where the DTC is stored.            |
| `priority`            | Priority level (lower values = higher priority; higher-priority DTCs may displace lower ones). |
| `OperationCycleRef`   | Operation cycle (e.g., "IGNITION") to which the DTC belongs.               |
| `events` (Optional)   | Group of events combined into a single DTC (used when NvM storage is limited).|

---

## 4. Environment Variables for Snapshots
The `Environments` section defines environment variables captured in snapshots (e.g., sensor readings).

### Example JSON Snippet:
```json
"Environments": [
  {
    "name": "Battery",
    "id": "0x1001",
    "type": "uint16",
    "unit": "v"  // Units like "V" (volts) or "°„C" (degrees Celsius)
  },
  // Additional environment variables...
]
```

---

## 5. NvM Storage Requirements
Each DTC and its associated events require a dedicated NvM (Non-Volatile Memory) block slot to store status, snapshots, and extended data. 

### Important Notes:
- If NvM storage is sufficient, **dedicate one slot per DTC** to avoid displacement of older DTCs.
- The current Dem generator supports **2 snapshot records per DTC** (dynamically assigned as `1` and `2`). If this is insufficient, manually update the generated C code.  
  Example generated C code snippet:  
  ```c
  static CONSTANT(uint8_t, DEM_CONST) Dem_FreezeFrameRecNumsForDTC0[] = {1, 2};
  static CONSTANT(uint8_t, DEM_CONST) Dem_FreezeFrameRecNumsForDTC1[] = {1, 2};
  static CONSTANT(uint8_t, DEM_CONST) Dem_FreezeFrameRecNumsForDTC2[] = {1, 2};
  // ... additional DTC entries
  ```

---

## 6. Extended Data Configuration
The `ExtendedDatas` section defines custom data to be stored with DTCs. Predefined types (e.g., `FaultOccurrenceCounter`, `AgingCounter`) are supported. For custom types, implement a callback function in the customer code.

### Example JSON Snippet:
```json
"ExtendedDatas": [
  {
    "name": "FaultOccurrenceCounter",
    "type": "uint8"  // Data type (e.g., uint8, uint16)
  },
  // Additional extended data entries...
]
```

---

## 7. Generator Tool
For details on the Dem configuration generator, refer to the [Generator Dem.py](../../tools/generator/Dem.py) script.
