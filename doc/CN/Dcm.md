---
layout: post
title: AUTOSAR Dcm 配置
category: AUTOSAR
comments: true
---

# AUTOSAR Dcm 模块配置说明

诊断通信管理模块（Dcm）负责 AUTOSAR 系统中的诊断请求处理、会话管理、安全访问和数据交换。本文介绍关键配置项：会话、安全等级、服务映射表、DID、内存访问规则、例程、I/O 控制、定时参数和缓冲区。

> **注意**：配置可以采用以下任一形式编写：
>
> * JSON 文件（例如 `Dcm.json`），由 [Dcm.py](../../tools/generator/Dcm.py) 处理；
> * Excel 表格（例如 `Dcm.xlsx`），用于规范评审。
>
> 两者最终都会转换为实现运行时行为的 C 代码（`Dcm_Cfg.h`、`Dcm_Cfg.c`）。

实际配置参考：

* [Bootloader Dcm.json](../../app/bootloader/config/Dcm/Dcm.json)
* [应用 Dcm.json](../../app/app/config/Dcm/Dcm.json)

## 1. 会话配置

会话具有唯一 ID，控制可访问的服务和安全等级：

```json
"sessions": [
  { "name": "Default",  "id": "0x01" },
  { "name": "Program",  "id": "0x02" },
  { "name": "Extended", "id": "0x03" },
  { "name": "Factory",  "id": "0x50" }
]
```

生成代码中会话映射到 `Dcm_SesCtrls[]`（`DCM_DEFAULT_SESSION` 等），对应位掩码`DCM_DEFAULT_MASK = 0x1`、`DCM_PROGRAM_MASK = 0x2`，依此类推。会话切换是否允许由应用回调决定：

```c
Std_ReturnType App_GetSessionChangePermission(
    Dcm_SesCtrlType active, Dcm_SesCtrlType target,
    Dcm_NegativeResponseCodeType *nrc);
```

## 2. 安全等级配置

按会话配置 seed/key 认证：

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

生成 `Dcm_SecLevelConfigs[]`，包含常量 `DCM_SEC_LEVEL1`、`DCM_SEC_LEVEL2`等、限定作用域的会话掩码，以及锁定保护：连续 3 次失败（`NumAtt = 3`）后锁定`DelayTime = 3000` ms。

## 3. 服务配置

`ServiceMap` 把 UDS 服务 ID 映射到具体实现：

| 属性 | 是否必需 | 说明 |
| --- | --- | --- |
| `name` | 否 | 文档用途的名称，例如 `"ReadDID"` |
| `id` | 是 | UDS SID，例如 `0x22` |
| `sessions` | 否 | 允许的会话列表（默认全部允许） |
| `securities` | 否 | 所需安全等级（空表示不需要） |
| `access` | 否 | `"physical"`、`"functional"` 或两者 |
| `API` | 条件需要 | 服务回调，例如 DID 读/写处理函数 |

> 诸如 `Authentication (0x29)` 的服务默认禁用，只有定义 `USE_CRYPTO` 时才会生成。

## 4. 数据标识符（DID）

### 静态 DID（0x22 / 0x2E）

静态 DID 位于 `"DIDs"` 节，每个条目必须指定：

* 十六进制 `ID`；
* `length`；
* `attribute`：`"r"`、`"w"` 或 `"rw"`；
* 可选的 `sessions`、`securities` 和 `access`。

### 周期 DID（0x2A）

```json
"DIDs": [
  { "name": "P01", "ID": "0x01", "sourceDID": "0xF201" }
]
```

客户端使用周期标识符 `0x01` 请求源 DID `0xF201` 的周期传输。

### 动态 DID（0x2C）

配置了服务 `0x2C` 后自动支持。动态 DID 定义存放在 TX 缓冲区的尾部。

## 5. 内存访问规则

`"memories"` 节限制上传/下载服务（0x34/0x36/0x37）可访问的地址范围：

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

* 地址长度格式标识符固定为 `0x44`（4 字节地址 + 4 字节长度）；
* 每个请求在运行时由 `Dcm_DspIsMemoryValid()` 校验。

## 6. 例程与 I/O 控制

### 例程（0x31）

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

### I/O 控制（0x2F）

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

两者都遵循 JSON 中定义的会话和安全约束。

## 7. 定时参数与缓冲区

定时参数定义 UDS 服务端行为：会话超时 `S3` 以及响应期限 `P2`、`P2*`。数值以毫秒给出，在编译期转换为 `Dcm_MainFunction()` 的调度周期数：

```c
#define DCM_CONVERT_MS_TO_MAIN_CYCLES(x) \
  ((x + DCM_MAIN_FUNCTION_PERIOD - 1u) / DCM_MAIN_FUNCTION_PERIOD)
```

因此所有内部定时器都与周期性的 `Dcm_MainFunction()` 调用（通常每 10 ms 一次）保持同步。

### JSON 示例

```json
"timings": {
  "S3Server": 5000,
  "P2ServerMax": 50,
  "P2StarServerMax": 150,
  "P2ServerAdjust": 20,
  "P2StarServerAdjust": 20
}
```

### 生成的 C 代码

[Dcm.py](../../tools/generator/Dcm.py) 按以下顺序生成该结构体初始化值：

```c
const Dcm_DslProtocolTimingRowType Dcm_DslProtocolTimingRow = {
  DCM_CONVERT_MS_TO_MAIN_CYCLES(5000u),   /* S3Server */
  DCM_CONVERT_MS_TO_MAIN_CYCLES(20u),     /* P2ServerAdjust */
  DCM_CONVERT_MS_TO_MAIN_CYCLES(20u),     /* P2StarServerAdjust */
  DCM_CONVERT_MS_TO_MAIN_CYCLES(50u),     /* P2ServerMax */
  DCM_CONVERT_MS_TO_MAIN_CYCLES(150u)     /* P2StarServerMax */
};
```

### 参数含义

| 参数 | 默认值（ms） | 用途 |
| --- | --- | --- |
| `S3Server` | 5000 | 最后一次请求后 ECU 保持在非默认会话的时间，超时后回到默认会话 |
| `P2ServerMax` | 50 | 标准请求的最大响应时间（回复 NRC 0x78 所花时间不计入） |
| `P2StarServerMax` | 150 | 服务端已发送 NRC 0x78（ResponsePending）后的最大响应时间 |
| `P2ServerAdjust` | 20 | 为调度抖动给 P2 定时器增加的安全裕量 |
| `P2StarServerAdjust` | 20 | P2* 定时器的安全裕量，用于刷写和长耗时例程 |

> **重要**：
>
> * `P2StarServerMax` 必须大于等于最长的处理函数执行时间（例如 flash 擦除/写入）；
> * 若服务无法在 `P2ServerMax` 内完成，必须先发送 NRC 0x78，然后在`P2StarServerMax` 内完成；
> * 所有数值在构建期静态确定，不支持运行时配置。

### 缓冲区大小

`"buffer"` 节设置接收/发送缓冲区大小：

```json
"buffer": {
  "rx": 576,
  "tx": 576
}
```

缓冲区必须容纳：

* 最大的预期 UDS 请求（DID 列表、内存地址等）；
* 最大的肯定或否定响应；
* 动态 DID 定义（临时使用 TX 缓冲区尾部）。

复杂 ECU 的典型取值为 512 至 1024 字节；建议最小不低于 256 字节。缓冲区声明为：

```c
static uint8_t rxBuffer[576];
static uint8_t txBuffer[576];
```

并在 `Dcm_Config` 中引用。
