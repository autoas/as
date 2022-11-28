---
layout: post
title: CAN OSEK NM 简介
category: AUTOSAR
comments: true
---

# CAN OSEK NM 简介

CAN NM（Network Management）网络管理非以太网网络管理，CAN NM比起以太网网络管理要简单太多，其无非进行网络上CAN节点的一个同醒同睡问题。

CAN NM也有很多变种，常用的如[AUTOSAR CAN NM](https://github.com/autoas/ssas-public/blob/master/infras/include/CanNm.h)和[OSEK NM](https://github.com/autoas/ssas-public/blob/master/infras/include/OsekNm.h)，当然还有很多各大厂商自定义的网络管理机制，但所有的CAN NM变种，其都为了实现同一个目的，同睡同醒。

OSEK NM其自身又分为2种机制，直接网络管理和间接网络管理，如何理解呢，其实也很简单，有专有CAN报文来传递网络状态信息的即为直接网络管理；没有专有CAN报文的，使用应用报文来简介同步网络状态信息的即为间接网络管理。按此来划分，其实AUTOSAR CAN NM属于直接网络管理。

本文将简单介绍OSEK NM直接网络管理，对于间接网络管理，现实中还是应用的比较少的，并且间接网络管理不同车型车企，其对应的应用报文也各不一样，不统一，很难标准化。

关于OSEK NM的原理，可以参考文档OSEK NM 253的文档，并且网上也有很多介绍行的文档，所以本文主要介绍我实现的OSEK NM如何使用的问题。

## 第一， 如何配置

关于OSEK配置信息的描述都位于[OsekNm_Priv.h](https://github.com/autoas/ssas-public/blob/master/infras/communication/OsekNm/OsekNm_Priv.h)头文件中，一个CAN通道的配置可以参考文件[OsekNm_Cfg.c](https://github.com/autoas/ssas-public/blob/master/app/app/config/OsekNm_Cfg.c)，该配置信息还是比较简单直白的，无非配置些节点的各OSEK NM的参数：

| NM Parameter  | Definition                                                   | Valid Area                   |
| :------------ | ------------------------------------------------------------ | ---------------------------- |
| NodeId        | Relative identification of the node-specific NM messages     | local for each node specific |
| TTyp          | Typical time interval between two ring messages              | global for all nodes         |
| TMax          | Maximum time interval between two ring messages              | global for all nodes         |
| TError        | Time interval between two ring messages with NMLimpHome identification | global all nodes             |
| TWaitBusSleep | Time the NM waits before transmission in NMBusSleep          | global all nodes             |
| TTx           | Delay to repeat the transmission request of a NM message if the request was rejected by the DLL | local for each node specific |
## 第二，如何集成使用

本OSEK NM实现可与任何系统集成，可单独使用。在集成时，须实现如下几个API：

```c
void D_Init(NetIdType NetId, RoutineRefType Routine);
// disable application communication by D_Offline
void D_Offline(NetIdType NetId);
// enable application communication by D_Online
void D_Online(NetIdType NetId);
// request to transmit NM frame
StatusType D_WindowDataReq(NetIdType NetId, NMPduType *NMPDU, uint8_t DataLengthTx);
```

在ssas-public项目中，你可以注意到实际上前三个API为空实现，其实这是不对的，需要按照OSEK NM文档的要求去实现。

如下为D_WindowDataReq使用AUTOSAR CAN驱动的一个简单的实现，该实现以CAN ID从0x500到0x5FF为NM网段。

```c
StatusType D_WindowDataReq(NetIdType NetId, NMPduType *NMPDU, uint8_t DataLengthTx) {
  StatusType ercd;
  Can_PduType canPdu;

  canPdu.swPduHandle = 2;
  canPdu.id = 0x500 + NMPDU->Source;
  canPdu.length = DataLengthTx;
  canPdu.sdu = &NMPDU->Destination;

  ercd = Can_Write(NetId, &canPdu);
  // 注意，NM报文发送成功时，在发送成功中断里，需要调用OsekNm_TxConformation

  return ercd;
}
```

OSEK NM其他节点报文接受处理，如下代码所示：

```c
void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr) {
  ...
  if ((Mailbox->CanId >= 0x500) && ((Mailbox->CanId <= 0x5FF))) {
    NMPduType NMPDU;
    NMPDU.Source = Mailbox->CanId - 0x500;
    memcpy(&NMPDU.Destination, PduInfoPtr->SduDataPtr, 8);
    OsekNm_RxIndication(Mailbox->ControllerId, &NMPDU);
  }
  ...
}
```

如上，实现如上所有，OSEK NM就可以开始工作了，只需要调用其初始化函数即可！

```c
  OsekNm_Init(NULL);
  TalkNM(0);
  StartNM(0);
```

另外，需要周期性调用函数OsekNm_MainFunction来驱动OSEK NM工作。

API GotoMode可用来控制网络的状态：

```c
GotoMode(0, NM_BusSleep); // 请求睡眠，释放网络
GotoMode(0, NM_Awake);    // 请求唤醒，激活网络
```

另外，还有如下两个API来控制是否真正参与网络管理，可用来实现UDS的网络管理通讯控制服务。

```c
StatusType SilentNM(NetIdType);
StatusType TalkNM(NetIdType);
```
另外，需要特别强调一点， OSEK NM具有CAN总线 bus off错误的管理机制，即在发生bus off 时，调用OsekNm_BusErrorIndication即可。
另，CAN总线由睡眠到唤醒时，调用OsekNm_WakeupIndication 即可。

## 第三，一个例子

参考 [OsekNm](https://github.com/autoas/ssas-public/blob/master/examples/OsekNm.md)。
