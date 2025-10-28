---
layout: post
title: AUTOSAR NvM
category: AUTOSAR
comments: true
---


# AUTOSAR NvM Configuration Overview

The **Non-Volatile Memory (NvM)** module in AUTOSAR manages persistent storage of critical data (e.g., DTCs, calibration parameters) across power cycles or ECU resets. This document provides configuration guidelines, JSON schema details, and best practices for defining NvM blocks and data elements, with a focus on DTC storage use cases.

---

## 1. Key Configuration Concepts

### 1.1 Core Purpose of NvM
- **Persistent Storage**: Ensures data (e.g., DTC statuses, fault counters) survives ECU resets or power loss.  
- **Block-Based Management**: Organizes data into logical "blocks" with configurable repetition (for scalability).  
- **Target Selection**: Supports two underlying storage targets:  
  - `Fee`: Flash EEPROM Emulation (software-based non-volatile storage).  
  - `Ea`: EEPROM Abstraction.  

---

## 2. JSON Configuration Structure

### Example NvM Configuration (DTC Storage)
```json
{
  "class": "NvM",          // Fixed class identifier for NvM configurations
  "target": "Fee",         // Underlying storage target ("Fee" or "Ea")
  "blocks": [              // List of logical NvM blocks to manage
    {
      "name": "Dem_NvmEventStatusRecord{}",  // Block name (ends with "{}" if repeated)
      "repeat": 8,         // Repeat this block 8 times (creates 8 instances)
      "data": [            // Data elements within the block
        { 
          "name": "status", 
          "type": "uint8", 
          "default": "0x50"  // Default value (C initializer: 0x50)
        },
        { 
          "name": "testFailedCounter", 
          "type": "uint8", 
          "default": 0       // Default value (C initializer: 0)
        }
      ]
    },
    // Additional blocks (e.g., for other DTC-related data)
  ]
}
```

---

## 3. Detailed Attribute Breakdown

### 3.1 Top-Level Attributes
| Attribute | Type       | Description                                                                 |
|-----------|------------|-----------------------------------------------------------------------------|
| `class`   | String     | Fixed value `"NvM"` (identifies the configuration type).                    |
| `target`  | String     | Storage target: `"Fee"` (Flash EEPROM Emulation) or `"Ea"` (EEPROM Abstraction). |
| `blocks`  | Array      | List of NvM blocks to manage (each block defines a logical data group).      |

---

### 3.2 Block-Level Attributes
Each block in `blocks` defines a group of related data elements (e.g., DTC status and counters).  

| Attribute | Type       | Description                                                                 |
|-----------|------------|-----------------------------------------------------------------------------|
| `name`    | String     | Logical name of the block. If `repeat` is used, the name **must end with `{}`** (e.g., `Dem_NvmEventStatusRecord{}`). |
| `repeat`  | Integer    | Optional. Number of times to repeat the block (creates `repeat` instances, e.g., `repeat: 8` generates `Dem_NvmEventStatusRecord0` to `Dem_NvmEventStatusRecord7`). |
| `data`    | Array      | List of data elements within the block (defines the structure of the block). |

---

### 3.3 Data Element Attributes
Each data element within a block defines a specific piece of stored data (e.g., `status` or `testFailedCounter`).  

| Attribute | Type       | Description                                                                 |
|-----------|------------|-----------------------------------------------------------------------------|
| `name`    | String     | Name of the data element. If `repeat` is used, the name **must end with `{}`** (e.g., `status{}`). |
| `repeat`  | Integer    | Optional. Number of times to repeat the data element within the block (creates `repeat` copies, e.g., `repeat: 2` generates `status[2]`). |
| `type`    | String     | Data type. Supported types: <br>- Scalar: `int8`, `int16`, `int32`, `uint8`, `uint16`, `uint32` <br>- Array: `int8_n`, `int16_n`, `int32_n`, `uint8_n`, `uint16_n`, `uint32_n` (requires `size` attribute). |
| `size`    | Integer    | Required for array types (e.g., `uint8_5` defines an array of 5 `uint8` elements). |
| `default` | String     | Default value (evaluated as a Python expression; used to initialize the C variable). |

---

## 4. Generator Tool

The [NvM Generator](../../tools/generator/NvM.py) converts the JSON configuration into C code that initializes NvM blocks and data elements. Key features:  
- Validates JSON syntax and attribute compliance (e.g., ensures `repeat` is used correctly).  
- Generates type-safe C structures (e.g., `Dem_NvmEventStatusRecord0` for repeated blocks).  
- Auto-populates default values (using Python `eval` to resolve expressions like `"0xFF * 2"`).  

---