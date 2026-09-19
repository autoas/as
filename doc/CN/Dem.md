---
layout: post
title: AUTOSAR Dem 配置
category: AUTOSAR
comments: true
---

# AUTOSAR Dem 配置说明

诊断事件管理模块（Dem）保存 DTC 状态字节、冻结帧快照和扩展数据。本文介绍存储内存、通用去抖/老化参数、DTC、快照环境变量和扩展数据记录的配置方法。完整示例见[app/app/config/Dcm/Dem.json](../../app/app/config/Dcm/Dem.json)。

## 1. 存储器（Memories）

`Memories` 节定义 DTC 状态、快照和扩展数据的存储位置。`Primary` 存储器为必需，`Mirror` 存储器可选：

```json
"Memories": [
  { "name": "Primary", "origin": "0x0001" },
  { "name": "Mirror",  "origin": "0x0002" }
]
```

## 2. 通用参数（general）

`general` 节提供默认值，当 DTC 条目未单独指定时使用。支持的键与[Dem.py](../../tools/generator/Dem.py) 的 `GetProp` 逻辑对应：

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

这些参数涵盖老化/确认计数器、快照和扩展数据捕获触发条件，以及基于计数器的去抖算法（步进值、失败/通过阈值和可选的跳升/跳降行为）。

## 3. DTC 定义

`DTCs` 节列出所有受支持的 DTC：

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

| 属性 | 说明 |
| --- | --- |
| `name` | 便于阅读的 DTC 名称，例如 `DTC0` |
| `number` | 唯一的 DTC 编号（UDS 按 DTC 查状态的掩码格式），例如 `0x112200` |
| `conditions` | 使能该 DTC 的条件名列表（生成器自动分配 ID，最多 32 个条件） |
| `destination` | 该 DTC 存储到的 `Memories` 存储器名列表 |
| `priority` | 数值越小优先级越高；高优先级 DTC 可能挤掉低优先级 DTC |
| `OperationCycleRef` | 该 DTC 所属的运行周期，例如 `IGNITION` |
| `is_group` / `events` | 可选：多个事件共享一个 DTC（NvM 空间受限时使用） |

真实配置中的事件分组示例：

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

## 4. 快照环境变量（Environments）

`Environments` 节列出冻结帧快照中捕获的数据元素。标量条目使用基本 `type` 和自由格式的 `unit` 字符串；复合条目使用 `type: "struct"` 并给出命名字段：

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

## 5. NvM 存储需求

每个 DTC（以及每个分组事件）都需要一个专用 NvM 块槽位，用于保存状态字节、快照和扩展数据：

* NvM 空间充足时建议每个 DTC 预留一个槽位，避免旧 DTC 被挤掉；
* 生成器目前为每个 DTC 生成两条冻结帧记录，编号为 `1` 和 `2`。若不够用，需要手动修改生成的 C 代码：

```c
static CONSTANT(uint8_t, DEM_CONST) Dem_FreezeFrameRecNumsForDTC0[] = {1, 2};
static CONSTANT(uint8_t, DEM_CONST) Dem_FreezeFrameRecNumsForDTC1[] = {1, 2};
```

## 6. 扩展数据（ExtendedDatas）

`ExtendedDatas` 节声明每个 DTC 附带的扩展数据记录。预定义记录`FaultOccuranceCounter`、`AgingCounter`、`AgedCounter` 由协议栈处理；自定义记录需要应用层提供回调函数：

```json
"ExtendedDatas": [
  { "name": "FaultOccuranceCounter", "type": "uint8" },
  { "name": "AgingCounter",          "type": "uint8" },
  { "name": "AgedCounter",           "type": "uint8" }
]
```

## 7. 生成器

配置由 [Dem.py](../../tools/generator/Dem.py) 处理，在 JSON 同目录生成`Dem_Cfg.c` 和 `Dem_Cfg.h`。
