---
layout: post
title: AUTOSAR DCM
category: AUTOSAR
comments: true
---

# Configuration Notes for AUTOSAR DCM Module

The Diagnostic Communication Manager (DCM) handles diagnostic requests, session management, security access, and data exchange in AUTOSAR systems. This document outlines key configuration parameters, including session definitions, security levels, service mappings, and memory access rules.

> ?? **Note**: The configurations described here can be authored in either:
> - A **JSON file** (e.g., `Dcm.json`) processed by a generator like [`Dcm.py`](../../tools/generator/Dcm.py), or  
> - An **Excel sheet** (e.g., `Dcm.xlsx`) for specification review.  
> Both are transformed into C code (`Dcm_Cfg.h`, `Dcm_Cfg.c`) that implements the runtime DCM behavior.

---

## 1. Example Configurations

For practical implementations, see:
- [Bootloader Dcm.json](../../app/bootloader/config/Dcm.json) (Bootloader-specific DCM setup)
- [Application Dcm.json](../../app/app/config/Dcm/Dcm.json) (Application-layer DCM configuration)

---

## 2. Session Configuration

Define supported diagnostic sessions with unique IDs. Sessions control access to services and security levels.

### Example Session Definitions (from JSON):
```json
"sessions": [
  { "name": "Default", "id": "0x01" },
  { "name": "Program", "id": "0x02" },
  { "name": "Extended", "id": "0x03" },
  { "name": "Factory", "id": "0x50" }
]
```

In generated code:
- Mapped to `Dcm_SesCtrls[] = { DCM_DEFAULT_SESSION, ... }`
- Masks: `DCM_DEFAULT_MASK = 0x1`, `DCM_PROGRAM_MASK = 0x2`, etc.

Session change permission is delegated to:
```c
Std_ReturnType App_GetSessionChangePermission(
    Dcm_SesCtrlType active, Dcm_SesCtrlType target,
    Dcm_NegativeResponseCodeType *nrc);
```

---

## 3. Security Level Configuration

Configure seed/key authentication per session.

### Example Security Definitions (from JSON):
```json
"securities": [
  {
    "name": "Extended",
    "level": 1,
    "size": 4,
    "sessions": ["Extended"],
    "API": {
      "seed": "App_GetExtendedLevelSeed",
      "key": "App_CompareExtendedLevelKey"
    }
  },
  {
    "name": "Program",
    "level": 2,
    "size": 4,
    "sessions": ["Program"],
    "API": {
      "seed": "App_GetProgramLevelSeed",
      "key": "App_CompareProgramLevelKey"
    }
  }
]
```

Generated as `Dcm_SecLevelConfigs[]` with:
- Security level constants: `DCM_SEC_LEVEL1`, `DCM_SEC_LEVEL2`, ...
- Session masks restrict usage scope
- Lockout after 3 failed attempts (`DelayTime = 3000 ms`)

---

## 4. Service Configuration

The DCM generator uses a `ServiceMap` to map service IDs to implementation logic.

| Attribute   | Required? | Description |
|-------------|-----------|-------------|
| `name`      | No        | For documentation (e.g., `"ReadDID"`) |
| `id`        | Yes       | UDS SID (e.g., `0x22`) |
| `sessions`  | No        | List of allowed sessions (default: all) |
| `securities`| No        | Required security levels (empty = none) |
| `access`    | No        | `"physical"`, `"functional"`, or both |
| `API`       | Conditional | Callbacks (e.g., for DID read/write) |

> ?? Services like `Authentication (0x29)` are **disabled by default** unless `USE_CRYPTO` is defined.

---

## 5. Data Identifiers (DIDs)

### Static DIDs (`0x22` / `0x2E`)
Defined in `"DIDs"` section of JSON. Each must specify:
- `ID` (hex),
- `length`,
- `attribute`: `"r"`, `"w"`, or `"rw"`,
- optional `sessions`, `securities`, and `access`.

### Periodic DIDs (`0x2A`)

```json
"DIDs": [
  { "name": "P01", "ID": "0x01", "sourceDID": "0xF201" }
]
```
? Client requests periodic transmission of DID `0xF201` using identifier `0x01`.

### Dynamic DIDs (`0x2C`)
Supported automatically if service `0x2C` is configured. Uses TX buffer tail for storage.

---

## 6. Memory Access Rules

Configured under `"memories"`:
```json
"memories": [
  {
    "low": "0x0",
    "high": "0x100000",
    "attribute": "rw",
    "sessions": ["Program"]
  }
]
```

- Address/length format: fixed to `0x44` (4-byte address + 4-byte length)
- Validated at runtime in `Dcm_DspIsMemoryValid()`

---

## 7. Routines & I/O Control

### Routines (`0x31`)
```json
"Routines": [
  {
    "ID": "0xFEEF",
    "actions": ["Start", "Result"],
    "API": {
      "start": "App_NvmTest_FEEF_Start",
      "result": "App_NvmTest_FEEF_Result"
    }
  }
]
```

### I/O Control (`0x2F`)
```json
"IOCTLs": [
  {
    "ID": "0xFC01",
    "actions": [
      { "id": 0, "API": "App_IOCtl_IOCTL1_FC01_ReturnControlToEcu" },
      { "id": 3, "API": "App_IOCtl_IOCTL1_FC01_ShortTermAdjustment" }
    ],
  }
]
```

Both respect session/security constraints defined in JSON.

---

## 8. Timing & Buffering

Timing parameters define critical UDS server behavior, including session timeout (`S3`), and response deadlines for normal (`P2`) and long-running (`P2*`) operations. These values are configured in milliseconds but **converted to main function cycles** at compile time using the macro:

```c
#define DCM_CONVERT_MS_TO_MAIN_CYCLES(x) \
  ((x + DCM_MAIN_FUNCTION_PERIOD - 1u) / DCM_MAIN_FUNCTION_PERIOD)
```

This ensures all internal timers operate in sync with the DCM’s periodic `Dcm_MainFunction()` call (typically every 10 ms).

### JSON Configuration Example
```json
"timings": {
  "S3Server": 5000,
  "P2ServerMax": 50,
  "P2StarServerMax": 150,
  "P2ServerAdjust": 20,
  "P2StarServerAdjust": 20
}
```

### Generated C Code Mapping

The generator script (`Dcm.py`) emits the following structure initialization:

```c
const Dcm_DslProtocolTimingRowType Dcm_DslProtocolTimingRow = {
  DCM_CONVERT_MS_TO_MAIN_CYCLES(5000u),   /* S3Server */
  DCM_CONVERT_MS_TO_MAIN_CYCLES(20u),     /* P2ServerAdjust */
  DCM_CONVERT_MS_TO_MAIN_CYCLES(100u),    /* P2StarServerAdjust */
  DCM_CONVERT_MS_TO_MAIN_CYCLES(20u),     /* P2ServerMax */
  DCM_CONVERT_MS_TO_MAIN_CYCLES(150u)    /* P2StarServerMax */
};
```

### Parameter Roles

| Parameter | Default (ms) | Purpose |
|---------|--------------|--------|
| `S3Server` | `5000` | Time the ECU remains in a non-default session after the last diagnostic request. After this, it reverts to `DefaultSession`. |
| `P2ServerMax` | `50` | Maximum time (excluding NRC 0x78) the server may take to respond to a standard diagnostic request. |
| `P2StarServerMax` | `150` | Maximum time allowed for services that return **NRC 0x78 (ResponsePending)** before final response. |
| `P2ServerAdjust` | `20` | Safety margin added to computed P2 timer to account for scheduling jitter. Rarely used in practice; often set to small value. |
| `P2StarServerAdjust` | `20` | Similar safety margin for P2* timer. Helps avoid premature timeout during flash programming or complex routines. |

> ?? **Important**:  
> - `P2StarServerMax` **must be ? actual longest handler execution time** (e.g., flash erase/write).  
> - If a service exceeds `P2ServerMax`, it **must** first send `NRC 0x78` and then complete within `P2StarServerMax`.  
> - All values are **statically resolved at build time**—no runtime configuration.

### Buffer Sizes

Also defined in JSON under `"buffer"`:
```json
"buffer": {
  "rx": 576,
  "tx": 576
}
```

These sizes must accommodate:
- Largest expected UDS request (including DID lists, memory addresses, etc.)
- Largest possible positive/negative response
- Dynamic DID definitions (which use TX buffer tail as temporary storage)

Typical sizing:
- `512–1024 bytes` for complex ECUs
- Minimum recommended: `256 bytes`

Buffers are declared as:
```c
static uint8_t rxBuffer[576];
static uint8_t txBuffer[576];
```

and referenced in `Dcm_Config`.

---

