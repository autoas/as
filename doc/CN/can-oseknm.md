---
layout: post
title: CAN OSEK NM 简介
category: AUTOSAR
comments: true
---

# CAN OSEK NM 简介

CAN NM（Network Management）网络管理非以太网网络管理，CAN NM比起以太网网络管理要简单太多，其无非进行网络上CAN节点的一个同醒同睡问题。

CAN NM也有很多变种，常用的如[AUTOSAR CAN NM](../../infras/include/CanNm.h)和[OSEK NM](../../infras/include/OsekNm.h)，当然还有很多各大厂商自定义的网络管理机制，但所有的CAN NM变种，其都为了实现同一个目的，同睡同醒。

OSEK NM其自身又分为2种机制，直接网络管理和间接网络管理，如何理解呢，其实也很简单，有专有CAN报文来传递网络状态信息的即为直接网络管理；没有专有CAN报文的，使用应用报文来简介同步网络状态信息的即为间接网络管理。按此来划分，其实AUTOSAR CAN NM属于直接网络管理。

本文将简单介绍OSEK NM直接网络管理，对于间接网络管理，现实中还是应用的比较少的，并且间接网络管理不同车型车企，其对应的应用报文也各不一样，不统一，很难标准化。

关于OSEK NM的原理，可以参考文档OSEK NM 253的文档，并且网上也有很多介绍行的文档，所以本文主要介绍我实现的OSEK NM如何使用的问题。

## 目录

- [如何配置](#第一-如何配置)
- [如何集成使用](#第二-如何集成使用)
  - [状态控制API](#状态控制api)
  - [错误处理](#错误处理)
  - [状态机](#状态机)

## 第一， 如何配置

关于OSEK配置信息的描述都位于[OsekNm_Priv.h](../../infras/communication/OsekNm/OsekNm_Priv.h)头文件中，一个CAN通道的配置可以参考文件[OsekNm.json](../../app/app/config/Com/OsekNm.json)，该配置信息还是比较简单直白的，无非配置些节点的各OSEK NM的参数：

| NM Parameter  | Definition                                                   |
| :------------ | ------------------------------------------------------------ |
| name          | Network name, used to generate txPduId                       |
| NodeId        | Relative identification of the node-specific NM messages     |
| tTyp          | Typical time interval between two ring messages (ms)          |
| tMax          | Maximum time interval between two ring messages (ms)          |
| tError        | Time interval between two ring messages with NMLimpHome identification (ms) |
| tWbs          | Time the NM waits before entering NMBusSleep state (ms)       |
| tTx           | Delay to repeat the transmission request of a NM message if the request was rejected by the DLL (ms) |
| tx_limit      | Maximum number of consecutive transmissions before entering Limphome |
| rx_limit      | Maximum number of consecutive receptions before entering Limphome |
| NodeMask      | Mask to extract NodeId from CAN ID                          |

## 第二，如何集成使用

本OSEK NM实现基于AUTOSAR架构，已内置了与CanIf的集成。在集成时，须实现如下几个回调API：

```c
void OsekNm_D_Init(NetworkHandleType NetId, OsekNm_RoutineRefType Routine);
void OsekNm_D_Offline(NetworkHandleType NetId);
void OsekNm_D_Online(NetworkHandleType NetId);
```

`OsekNm_D_Init` 用于执行总线初始化/唤醒/睡眠/重启等操作，`OsekNm_D_Online` 和 `OsekNm_D_Offline` 分别用于启用和禁用应用层通信。

OSEK NM内部已实现 `OsekNm_D_WindowDataReq`，通过 `CanIf_Transmit` 发送NM报文。接收报文处理如下：

```c
void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr) {
  ...
  if ((Mailbox->CanId >= 0x500) && ((Mailbox->CanId <= 0x5FF))) {
    PduInfoType pduInfo;
    pduInfo.SduDataPtr = PduInfoPtr->SduDataPtr;
    pduInfo.SduLength = PduInfoPtr->SduLength;
    pduInfo.MetaDataPtr = (void *)Mailbox;
    OsekNm_RxIndication(NetId, &pduInfo);
  }
  ...
}
```

发送确认回调需调用 `OsekNm_TxConfirmation`：

```c
void CanIf_TxConfirmation(PduIdType TxPduId, Std_ReturnType result) {
  ...
  OsekNm_TxConfirmation(NetId, result);
  ...
}
```

初始化并启动OSEK NM：

```c
OsekNm_Init(NULL);
OsekNm_Talk(0);
OsekNm_Start(0);
```

需要周期性调用 `OsekNm_MainFunction` 来驱动状态机工作：

```c
void MainFunction(void) {
  OsekNm_MainFunction();
}
```

### 状态控制API

`OsekNm_GotoMode` 用于控制网络状态：

```c
OsekNm_GotoMode(0, OSEKNM_BUS_SLEEP); // 请求睡眠
OsekNm_GotoMode(0, OSEKNM_AWAKE);    // 请求唤醒
```

`OsekNm_Silent` 和 `OsekNm_Talk` 控制是否参与网络管理：

```c
Std_ReturnType OsekNm_Silent(NetworkHandleType);  // 静默模式，不发送NM报文
Std_ReturnType OsekNm_Talk(NetworkHandleType);   // 正常参与网络管理
```

`OsekNm_NetworkRequest` 和 `OsekNm_NetworkRelease` 用于请求和释放网络：

```c
Std_ReturnType OsekNm_NetworkRequest(NetworkHandleType);   // 请求网络唤醒
Std_ReturnType OsekNm_NetworkRelease(NetworkHandleType);   // 请求网络睡眠
```

`OsekNm_Stop` 用于停止网络管理：

```c
Std_ReturnType OsekNm_Stop(NetworkHandleType);
```

`OsekNm_GetState` 用于获取当前网络状态：

```c
Nm_ModeType mode;
OsekNm_GetState(NetId, &mode);
```

### 错误处理

CAN总线bus off时调用 `OsekNm_BusErrorIndication`：

```c
void CanIf_BusErrorIndication(NetworkHandleType NetId) {
  OsekNm_BusErrorIndication(NetId);
}
```

CAN总线唤醒时调用 `OsekNm_WakeupIndication`：

```c
void CanIf_WakeupIndication(NetworkHandleType NetId) {
  OsekNm_WakeupIndication(NetId);
}
```

### 状态机

OSEK NM实现了完整的状态机，包括：
- `OSEKNM_STATE_OFF` - 关闭状态
- `OSEKNM_STATE_BUS_SLEEP` - 总线睡眠状态
- `OSEKNM_STATE_NORMAL` - 正常工作状态
- `OSEKNM_STATE_NORMAL_PREPARE_SLEEP` - 正常模式准备睡眠
- `OSEKNM_STATE_WAIT_BUS_SLEEP_NORMAL` - 正常模式等待总线睡眠
- `OSEKNM_STATE_LIMPHOME` - Limphome状态
- `OSEKNM_STATE_LIMPHOME_PREPARE_SLEEP` - Limphome准备睡眠
- `OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME` - Limphome等待总线睡眠

