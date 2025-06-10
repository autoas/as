---
layout: post
title: AUTOSAR Dem
category: AUTOSAR
comments: true
---

# Configuration notes for Dem

[Dem example](../../app/app/config/Dcm/Dem.json) is simple configuration example to let you know how to configure Dem for DTC.

Firstly with `Memories` to specify the memory that used to store the DTC status/snapshot and its extended datas. And generally, the `Primary` memory must be specified, the `Mirror` is optional.

```json
  "Memories": [
    {
      "name": "Primary",
      "origin": "0x0001"
    },
    {
      "name": "Mirror",
      "origin": "0x0002"
    }
  ],
```

And with `general` to specify some common attributes for DTC if the DTC itself has no such attribute. Check  [Genetator Dem.py](../../tools/generator/Dem.py) API `GetProp`.

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
  },
```

And with `DTCs` to list all the DTCs that supported.

```json
  "DTCs": [
    {
      "name": "DTC0",
      "number": "0x112200",
      "conditions": [
        "BatteryNormal"
      ],
      "destination": [
        "Primary",
        "Mirror"
      ],
      "priority": 0,
      "OperationCycleRef": "IGNITION"
    },
    ...
  ]
```


| attr | comments |
|------|----------|
| name | the DTC name |
| number | the DTC number |
| conditions | a list of conditions' name, Dem generator will automatically assign a condition ID to the condition identified by the name, only support up to 32 conditions |
| destination | the memory destinition to store the DTC |
| priority |  the priority of DTC, lower value means higher priprity. higher priority DTC will result displacement the DTC with lower priority |
|OperationCycleRef | operation cycle this DTC belongs to |
| events | optional, if present, means a group of events comnined DTC, generally used for case the NvM is short of usage |

And for this Dem implementation, that each DTC and its associated event must has a dedicated slot of NvM block to record its status.

And if the NvM is big enough, it was strongly suggested that each DTC has a dedicated slot of NvM block to store the snapshot and its extended data to avoid the displacement.

With `Environments` to specify the environment variables of the snapshot.

```json
  "Environments": [
    {
      "name": "Battery",
      "id": "0x1001",
      "type": "uint16",
      "unit": "v"
    },
    ...
  ]
```

 And generally, for a DTC, it maybe for example to capture the snapshot when the DTC first time happens and then record the snapshot that the DTC happens last time, thus generally for a DTC snapshot, it needs 2 record numbers. Now it was the Dem generator only support 2 record number and the record number can't be configured, it was dynamic given a record number value `2*idx_of_dtc+1, 2*idx_of_dtc+2`. If this was not right, please manually update the generated C code. And in fact, the Dem implementation can support 1 or 2 more record number.

 ```c
static CONSTANT(uint8_t, DEM_CONST) Dem_FreezeFrameRecNumsForDTC0[] = {1, 2};
static CONSTANT(uint8_t, DEM_CONST) Dem_FreezeFrameRecNumsForDTC1[] = {3, 4};
static CONSTANT(uint8_t, DEM_CONST) Dem_FreezeFrameRecNumsForDTC2[] = {5, 6};
static CONSTANT(uint8_t, DEM_CONST) Dem_FreezeFrameRecNumsForDTC3[] = {7, 8};
static CONSTANT(uint8_t, DEM_CONST) Dem_FreezeFrameRecNumsForDTC4[] = {9, 10};
static CONSTANT(uint8_t, DEM_CONST) Dem_FreezeFrameRecNumsForDTC_COMB0[] = {11, 12};
```


With `ExtendedDatas` to specify the extended data that need to be supported. Now the Dem already support some common extened datas, such as `FaultOccuranceCounter`/`AgingCounter`/`AgedCounter`, etc. For the others, a callback function to get the configured extended data need to be implemented by customer.

```json
  "ExtendedDatas": [
    {
      "name": "FaultOccuranceCounter",
      "type": "uint8"
    },
    ...
  ]
```

## Genetator

* [Genetator Dem.py](../../tools/generator/Dem.py)
