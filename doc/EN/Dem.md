---
layout: post
title: AUTOSAR Dem Configuration
category: AUTOSAR
comments: true
---

# Configuration Notes for AUTOSAR Dem

The Diagnostic Event Manager (Dem) stores DTC status bytes, freeze-frame snapshots and extended data. This document describes how to configure the storage memories, general debounce/aging parameters, DTCs, snapshot environment variables and extended data records. A complete example is available at [app/app/config/Dcm/Dem.json](../../app/app/config/Dcm/Dem.json).

## 1. Memories

The `Memories` section defines where DTC status, snapshots and extended data are stored. A `Primary` memory is mandatory; a `Mirror` memory is optional:

```json
"Memories": [
  { "name": "Primary", "origin": "0x0001" },
  { "name": "Mirror",  "origin": "0x0002" }
]
```

## 2. General parameters

The `general` section provides defaults applied to a DTC when the DTC entry does not override them. The supported keys follow the [Dem.py](../../tools/generator/Dem.py) `GetProp` logic:

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

These cover aging/confirmation counters, the snapshot and extended-data capture triggers, and the counter-based debounce algorithm (step sizes, failed/passed thresholds and optional jump-up/jump-down behavior).

## 3. DTC definitions

The `DTCs` section lists every supported DTC:

```json
"DTCs": [
  {
    "name": "DTC0",
    "number": "0x112200",
    "conditions": ["BatteryNormal"],
    "destination": ["Primary", "Mirror"],
    "priority": 0,
    "OperationCycleRef": "IGNITION"
  }
]
```

| Attribute | Description |
| --- | --- |
| `name` | Human-readable DTC name, for example `DTC0` |
| `number` | Unique DTC number (UDS status-by-DTC mask format), for example `0x112200` |
| `conditions` | Condition names enabling the DTC (the generator assigns the IDs; at most 32 conditions) |
| `destination` | Memory names from `Memories` where the DTC is stored |
| `priority` | Lower numbers mean higher priority; higher-priority DTCs may displace lower-priority ones |
| `OperationCycleRef` | Operation cycle the DTC belongs to, for example `IGNITION` |
| `is_group` / `events` | Optional: several events can share one DTC (used when NvM space is limited) |

The grouped-event example from the real configuration:

```json
{
  "name": "DTC_COMB0",
  "number": "0x112205",
  "is_group": true,
  "events": [
    { "name": "DTC_COMB0_EVENT0" },
    { "name": "DTC_COMB0_EVENT1" },
    { "name": "DTC_COMB0_EVENT2" }
  ]
}
```

## 4. Snapshot environments

The `Environments` section lists the data elements captured in a freeze-frame snapshot. Scalar entries carry a primitive `type` and a free-form `unit` string; composite entries use `type: "struct"` with named member fields:

```json
"Environments": [
  { "name": "Battery",    "id": "0x1001", "type": "uint16", "unit": "v" },
  { "name": "VehileSpeed","id": "0x1002", "type": "uint16", "unit": "km/h" },
  {
    "name": "Time",
    "id": "0x1004",
    "type": "struct",
    "data": [
      { "name": "year",   "type": "uint8" },
      { "name": "month",  "type": "uint8" },
      { "name": "day",    "type": "uint8" },
      { "name": "hour",   "type": "uint8" },
      { "name": "minute", "type": "uint8" },
      { "name": "second", "type": "uint8" }
    ],
    "unit": "YY-MM-DD-HH"
  }
]
```

## 5. NvM storage requirements

Each DTC (and each grouped event) needs a dedicated NvM block slot holding its status byte, snapshots and extended data:

* when NvM space is sufficient, reserve one slot per DTC so older DTCs are never displaced;
* the generator currently emits two freeze-frame records per DTC, numbered `1` and `2`. If more records are required, edit the generated C code:

```c
static CONSTANT(uint8_t, DEM_CONST) Dem_FreezeFrameRecNumsForDTC0[] = {1, 2};
static CONSTANT(uint8_t, DEM_CONST) Dem_FreezeFrameRecNumsForDTC1[] = {1, 2};
```

## 6. Extended data

The `ExtendedDatas` section declares per-DTC extended data records. The predefined records `FaultOccuranceCounter`, `AgingCounter` and `AgedCounter` are handled by the stack; any custom record requires an application callback:

```json
"ExtendedDatas": [
  { "name": "FaultOccuranceCounter", "type": "uint8" },
  { "name": "AgingCounter",          "type": "uint8" },
  { "name": "AgedCounter",           "type": "uint8" }
]
```

## 7. Generator

The configuration is processed by [Dem.py](../../tools/generator/Dem.py), which emits `Dem_Cfg.c` and `Dem_Cfg.h` next to the JSON file.
