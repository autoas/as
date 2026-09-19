---
layout: post
title: CAN OSEK NM 简介
category: AUTOSAR
comments: true
---

# CAN OSEK NM 简介

CAN 网络管理（Network Management，NM）比以太网网络管理简单得多，它要解决的核心问题只有一个：让总线上的各个 CAN 节点**同醒同睡**——需要通信时一起唤醒，无人通信时一起进入睡眠。

CAN NM 有很多变种，常用的有 [AUTOSAR CanNm](../../infras/include/CanNm.h) 和 [OSEK NM](../../infras/include/OsekNm.h)，此外还有各大厂商的自定义机制，但目标一致。

OSEK NM 自身又分为两种机制：

* **直接网络管理**：使用专用 NM CAN 报文传递网络状态，节点间组成逻辑环（logical ring），按顺序传递令牌；
* **间接网络管理**：没有专用 NM 报文，借用应用报文间接同步网络状态。

按这个划分，AUTOSAR CanNm 也属于直接网络管理。间接网络管理在实际项目中用得较少，且各车企借用的应用报文各不相同、难以标准化，因此本文只介绍 OSEK **直接**网络管理。规范细节可参考 OSEK/VDX NM 253 文档，本文重点介绍本实现如何配置与使用。

## 目录

- [第一，如何配置](#第一如何配置)
- [第二，如何集成使用](#第二如何集成使用)
  - [状态控制 API](#状态控制-api)
  - [错误处理](#错误处理)
  - [状态机](#状态机)
- [主机仿真实验](#主机仿真实验)

## 第一，如何配置

一个 CAN 通道的配置示例见 [OsekNm.json](../../app/app/config/Com/OsekNm.json)：

```json
{
  "class": "OsekNm",
  "networks": [
    {
      "name": "OSEKNM0",
      "tTx": 20,
      "tTyp": 1000,
      "tMax": 1500,
      "tError": 1000,
      "tWbs": 5000,
      "NodeMask": "0xFF",
      "NodeId": 0,
      "rx_limit": 4,
      "tx_limit": 8
    }
  ]
}
```

各参数含义（结构体定义见 [OsekNm_Priv.h](../../infras/communication/OsekNm/OsekNm_Priv.h)）：

| 参数 | 含义 |
| --- | --- |
| `name` | 网络名，用于生成对应的 txPduId（如 `CANIF_OSEKNM0_TX`） |
| `NodeId` | 本节点在逻辑环上的编号，同时决定 NM 报文 CAN ID：**`0x400 + NodeId`** |
| `tTyp` | 正常情况下相邻两帧 ring 报文之间的典型周期（ms） |
| `tMax` | 相邻两帧 ring 报文之间允许的最大间隔（ms），超时认为 ring 中断 |
| `tError` | 进入 LimpHome 后，两帧 NMLimpHome 报文之间的间隔（ms） |
| `tWbs` | 收到睡眠指示后，等待进入 NMBusSleep（总线睡眠）状态的时间（ms） |
| `tTx` | NM 报文发送请求被底层拒绝后，重发请求的延迟（ms） |
| `tx_limit` | 连续发送失败/异常计数超过该值后进入 LimpHome |
| `rx_limit` | 连续接收异常计数超过该值后进入 LimpHome |
| `NodeMask` | 从 CAN ID 中提取 NodeId 用的掩码（收到报文时 `CanId & NodeMask` 得到源节点号） |

时间参数都以毫秒填写，生成器按 `OSEKNM_MAIN_FUNCTION_PERIOD`（默认 10 ms，可在 JSON 顶层用 `MainFunctionPeriod` 覆盖）换算成 `OsekNm_MainFunction` 的调用周期数。

NM 报文为 8 字节，结构（见 [OsekNm.h](../../infras/include/OsekNm.h) 的 `OsekNm_PduType`）：源节点 ID、目的节点 ID、OpCode（Ring/Aloof/Sleep 指示等）加 6 字节 RingData。

## 第二，如何集成使用

本 OSEK NM 实现基于 AUTOSAR 分层架构，与 CanIf 的集成胶水代码由生成器 [OsekNm.py](../../tools/generator/OsekNm.py) 自动生成到 `GEN/OsekNm_Cfg.c`，包括：

* 默认的 `OsekNm_D_Offline` / `OsekNm_D_Online`（内部回调 Nm 的 `Nm_PrepareBusSleepMode` / `Nm_NetworkMode`）；
* 不使用 CanIf（`USE_CANIF` 未定义）时的 `CanIf_RxIndication` / `CanIf_TxConfirmation` / `CanIf_Transmit` 桩，接收侧按 `(CanId & 0xFFFFFF00) == 0x400` 过滤 NM 报文，发送 ID 为 `0x400 + NodeId`。

集成方需要自己实现的只有总线动作回调 `OsekNm_D_Init`（生成器给了空实现，按需覆写）：

```c
void OsekNm_D_Init(NetworkHandleType NetId, OsekNm_RoutineRefType Routine);
```

`Routine` 取值告知需要执行的总线动作：

| Routine | 动作 |
| --- | --- |
| `OSEKNM_ROUTINE_BUS_INIT` | 初始化总线 |
| `OSEKNM_ROUTINE_BUS_SHUTDOWN` | 关闭总线 |
| `OSEKNM_ROUTINE_BUS_RESTART` | 重启总线 |
| `OSEKNM_ROUTINE_BUS_SLEEP` | 总线进入睡眠（允许唤醒） |
| `OSEKNM_ROUTINE_BUS_AWAKE` | 总线唤醒 |

使用 CanIf 时（仿真平台的典型做法），在 PDU 路由表中为 NM 通道配置好 RxPdu，并在接收/发送确认路径上调用：

```c
/* CanIf 收到报文后按 ID 路由给 OsekNm */
OsekNm_RxIndication(NetId, PduInfoPtr);

/* 发送完成确认 */
OsekNm_TxConfirmation(NetId, E_OK);
```

初始化并启动网络管理：

```c
OsekNm_Init(&OsekNm_Config);
OsekNm_Talk(0);     /* 以参与 ring 的方式入网（Silent 为只听不发） */
OsekNm_Start(0);
```

状态机必须由周期性任务驱动（周期与 `OSEKNM_MAIN_FUNCTION_PERIOD` 一致）：

```c
void MainFunction_10ms(void) {
  OsekNm_MainFunction();
}
```

### 状态控制 API

请求网络模式（睡眠/唤醒）：

```c
OsekNm_GotoMode(0, OSEKNM_BUS_SLEEP);  /* 请求同睡 */
OsekNm_GotoMode(0, OSEKNM_AWAKE);      /* 请求唤醒 */
```

参与方式：

```c
OsekNm_Talk(0);    /* 正常参与逻辑环，收发 NM 报文 */
OsekNm_Silent(0);  /* 静默：在线监听但不发送 NM 报文 */
```

网络请求/释放（与上层 ComM/Nm 对接）：

```c
OsekNm_NetworkRequest(0);  /* 本地需要通信，请求网络保持唤醒 */
OsekNm_NetworkRelease(0);  /* 本地不再需要通信，允许网络睡眠 */
```

其他：

```c
OsekNm_Stop(0);            /* 停止网络管理 */

Nm_ModeType mode;
OsekNm_GetState(0, &mode); /* NM_MODE_BUS_SLEEP / NM_MODE_PREPARE_BUS_SLEEP /
                              NM_MODE_SYNCHRONIZE / NM_MODE_NETWORK */
```

### 错误处理

总线 bus-off 与本地唤醒事件需上报给 OsekNm：

```c
/* CAN 控制器 bus-off 恢复后通知 NM（触发 BUS_RESTART，计数超限则进 LimpHome） */
OsekNm_BusErrorIndication(NetId);

/* 总线唤醒中断触发时通知 NM */
OsekNm_WakeupIndication(NetId);
```

### 状态机

内部状态（定义见 [OsekNm_Priv.h](../../infras/communication/OsekNm/OsekNm_Priv.h)）：

| 状态 | 含义 |
| --- | --- |
| `OSEKNM_STATE_OFF` | 关闭，网络管理未启动 |
| `OSEKNM_STATE_ON` | 已启动后的内部过渡态 |
| `OSEKNM_STATE_NORMAL` | Normal Operation，正常参与逻辑环 |
| `OSEKNM_STATE_NORMAL_PREPARE_SLEEP` | 正常模式下收到睡眠指示，准备睡眠 |
| `OSEKNM_STATE_WAIT_BUS_SLEEP_NORMAL` | 正常模式下等待总线睡眠计时（tWbs） |
| `OSEKNM_STATE_BUS_SLEEP` | 总线睡眠，停止 NM 报文、等待唤醒 |
| `OSEKNM_STATE_LIMPHOME` | 网络故障（超时/计数超限），周期性发送 NMLimpHome 报文 |
| `OSEKNM_STATE_LIMPHOME_PREPARE_SLEEP` | LimpHome 下准备睡眠 |
| `OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME` | LimpHome 下等待总线睡眠计时 |

## 主机仿真实验

在 Windows 主机上可以用 v2 虚拟 CAN 总线（见[虚拟 CAN 环境](./virtual-can-env.md)）同时跑多个 NM 节点，无需任何硬件。每个主机进程是环上的一个节点，节点号通过环境变量 `OSEKNM_NODE_ID` 在进程启动时注入（生成的 `OsekNm_Cfg.c` 中带 constructor 自动读取，并调用 `CanIf_SetDynamicTxId` 把发送 ID 设为 `0x400 + NodeId`）：

```sh
# sim 页：v2 组播总线无需启动中心服务器；可选开一个抓包监视器
D:\repository\as>build\nt\GCC\CanDump\CanDump.exe -d simulator_v2
```

```sh
# 分别开 3 个终端，模拟节点 1、2、3 入网
D:\repository\as>set OSEKNM_NODE_ID=1
D:\repository\as>build\nt\GCC\CanApp\CanApp.exe

D:\repository\as>set OSEKNM_NODE_ID=2
D:\repository\as>build\nt\GCC\CanApp\CanApp.exe

D:\repository\as>set OSEKNM_NODE_ID=3
D:\repository\as>build\nt\GCC\CanApp\CanApp.exe
```

三个节点启动后，CanDump 中可以看到 ID 为 `0x401 / 0x402 / 0x403` 的 NM 报文按 tTyp 周期依次轮转——这就是逻辑环在传递令牌；任一节点调用 `OsekNm_NetworkRequest` 网络保持唤醒，所有节点都调用 `OsekNm_NetworkRelease` 并经 tWbs 后，全网一起进入 BusSleep，总线上不再有 NM 报文；任一节点唤醒，全网再次被唤醒。
