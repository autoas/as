---
layout: post
title: AUTOSAR Dcm Configuration
category: AUTOSAR
comments: true
---

# Configuration Notes for the AUTOSAR Dcm Module

The Diagnostic Communication Manager (Dcm) handles diagnostic requests, session management, security access and data exchange in AUTOSAR systems. This document describes the key configuration items: sessions, security levels, the service map, DIDs, memory access rules, routines, I/O control, timing and buffers.

> **Note**: A configuration can be authored as either:
>
> * a JSON file (for example `Dcm.json`) processed by [Dcm.py](../../tools/generator/Dcm.py), or
> * an Excel sheet (for example `Dcm.xlsx`) for specification review.
>
> Both are transformed into C code (`Dcm_Cfg.h`, `Dcm_Cfg.c`) that implements the runtime behavior.

Practical configurations:

* [Bootloader Dcm.json](../../app/bootloader/config/Dcm/Dcm.json)
* [Application Dcm.json](../../app/app/config/Dcm/Dcm.json)

## 1. Session configuration

Sessions have unique IDs and control which services and security levels are reachable:

```json
"sessions": [
  { "name": "Default",  "id": "0x01" },
  { "name": "Program",  "id": "0x02" },
  { "name": "Extended", "id": "0x03" },
  { "name": "Factory",  "id": "0x50" }
]
```

In generated code the sessions map to `Dcm_SesCtrls[]` (`DCM_DEFAULT_SESSION`, ...) with bit masks `DCM_DEFAULT_MASK = 0x1`, `DCM_PROGRAM_MASK = 0x2` and so on. Whether a session switch is allowed is delegated to the application callback:

```c
Std_ReturnType App_GetSessionChangePermission(
    Dcm_SesCtrlType active, Dcm_SesCtrlType target,
    Dcm_NegativeResponseCodeType *nrc);
```

## 2. Security level configuration

Seed/key authentication is configured per session:

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

This generates `Dcm_SecLevelConfigs[]` with constants `DCM_SEC_LEVEL1`, `DCM_SEC_LEVEL2`, ..., session masks scoping each level, and lockout protection: after 3 failed attempts (`NumAtt = 3`) the level is locked for `DelayTime = 3000` ms.

## 3. Service configuration

A `ServiceMap` maps UDS service IDs to their implementation:

| Attribute | Required? | Description |
| --- | --- | --- |
| `name` | No | Documentation name, for example `"ReadDID"` |
| `id` | Yes | UDS SID, for example `0x22` |
| `sessions` | No | Allowed sessions (default: all) |
| `securities` | No | Required security levels (empty means none) |
| `access` | No | `"physical"`, `"functional"` or both |
| `API` | Conditional | Service callbacks, for example DID read/write handlers |

> Services such as `Authentication (0x29)` are disabled by default and are only generated when `USE_CRYPTO` is defined.

## 4. Data identifiers (DIDs)

### Static DIDs (0x22 / 0x2E)

Static DIDs live in the `"DIDs"` section. Each entry specifies:

* `ID` in hex;
* `length`;
* `attribute`: `"r"`, `"w"` or `"rw"`;
* optional `sessions`, `securities` and `access`.

### Periodic DIDs (0x2A)

```json
"DIDs": [
  { "name": "P01", "ID": "0x01", "sourceDID": "0xF201" }
]
```

The client requests periodic transmission of source DID `0xF201` using the periodic identifier `0x01`.

### Dynamic DIDs (0x2C)

Supported automatically once service `0x2C` is configured. Dynamic DID definitions are stored at the tail of the TX buffer.

## 5. Memory access rules

The `"memories"` section restricts the address ranges reachable by the upload/download services (0x34/0x36/0x37):

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

* AddressAndLengthFormatIdentifier is fixed to `0x44` (4-byte address plus 4-byte length);
* every request is validated at runtime by `Dcm_DspIsMemoryValid()`.

## 6. Routines and I/O control

### Routines (0x31)

```json
"Routines": [
  {
    "ID": "0xFEEF",
    "actions": ["Start", "Result"],
    "API": {
      "start":  "App_NvmTest_FEEF_Start",
      "result": "App_NvmTest_FEEF_Result"
    }
  }
]
```

### I/O control (0x2F)

```json
"IOCTLs": [
  {
    "ID": "0xFC01",
    "actions": [
      { "id": 0, "API": "App_IOCtl_IOCTL1_FC01_ReturnControlToEcu" },
      { "id": 3, "API": "App_IOCtl_IOCTL1_FC01_ShortTermAdjustment" }
    ]
  }
]
```

Both obey the session and security constraints of the JSON configuration.

## 7. Timing and buffering

The timing parameters define the UDS server behavior: session timeout `S3` and the response deadlines `P2` and `P2*`. Values are given in milliseconds and converted to `Dcm_MainFunction()` cycles at compile time:

```c
#define DCM_CONVERT_MS_TO_MAIN_CYCLES(x) \
  ((x + DCM_MAIN_FUNCTION_PERIOD - 1u) / DCM_MAIN_FUNCTION_PERIOD)
```

All internal timers therefore tick in sync with the periodic `Dcm_MainFunction()` call (typically every 10 ms).

### JSON example

```json
"timings": {
  "S3Server": 5000,
  "P2ServerMax": 50,
  "P2StarServerMax": 150,
  "P2ServerAdjust": 20,
  "P2StarServerAdjust": 20
}
```

### Generated C code

[Dcm.py](../../tools/generator/Dcm.py) emits the row initializer in this order:

```c
const Dcm_DslProtocolTimingRowType Dcm_DslProtocolTimingRow = {
  DCM_CONVERT_MS_TO_MAIN_CYCLES(5000u),   /* S3Server */
  DCM_CONVERT_MS_TO_MAIN_CYCLES(20u),     /* P2ServerAdjust */
  DCM_CONVERT_MS_TO_MAIN_CYCLES(20u),     /* P2StarServerAdjust */
  DCM_CONVERT_MS_TO_MAIN_CYCLES(50u),     /* P2ServerMax */
  DCM_CONVERT_MS_TO_MAIN_CYCLES(150u)     /* P2StarServerMax */
};
```

### Parameter roles

| Parameter | Default (ms) | Purpose |
| --- | --- | --- |
| `S3Server` | 5000 | Time the ECU stays in a non-default session after the last request, then falls back to the default session |
| `P2ServerMax` | 50 | Maximum response time for a standard request (time spent answering NRC 0x78 is excluded) |
| `P2StarServerMax` | 150 | Maximum response time once the server has sent NRC 0x78 (ResponsePending) |
| `P2ServerAdjust` | 20 | Safety margin added to the P2 timer for scheduling jitter |
| `P2StarServerAdjust` | 20 | Safety margin for the P2* timer, used during flash programming and long routines |

> **Important**:
>
> * `P2StarServerMax` must be greater than or equal to the longest handler execution time (for example flash erase/write);
> * if a service cannot finish within `P2ServerMax`, it must first send NRC 0x78 and then complete within `P2StarServerMax`;
> * all values are resolved statically at build time; there is no runtime configuration.

### Buffer sizes

The `"buffer"` section sets the receive/transmit buffer sizes:

```json
"buffer": {
  "rx": 576,
  "tx": 576
}
```

The buffers must hold:

* the largest expected UDS request (DID lists, memory addresses and so on);
* the largest possible positive or negative response;
* dynamic DID definitions, which temporarily use the tail of the TX buffer.

Typical sizing ranges from 512 to 1024 bytes for complex ECUs; 256 bytes is the recommended minimum. The buffers are declared as:

```c
static uint8_t rxBuffer[576];
static uint8_t txBuffer[576];
```

and referenced from `Dcm_Config`.
