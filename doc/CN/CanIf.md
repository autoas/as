---
layout: post
title: AUTOSAR CanIf 配置指南
category: AUTOSAR
comments: true
---

# AUTOSAR CanIf 配置指南

CAN 接口层（CanIf）位于 CAN 驱动与上层模块（CanTp、OsekNm、CanNm、CanTSyn、Xcp、SecOC、PduR 以及用户模块）之间，负责 CAN ID 过滤、Rx/Tx PDU 与硬件对象（HOH）的映射，并根据 `up` 字段把接收指示和发送确认分发给对应上层。本文介绍由[CanIf.py](../../tools/generator/CanIf.py) 消费的 JSON 配置。

完整的真实示例见[app/app/config/Com/CanIf.json](../../app/app/config/Com/CanIf.json)。

## 1. 顶层结构

```json
{
  "class": "CanIf",
  "RxPacketPoolSize": 8,
  "RxPacketDataSize": 64,
  "TxPacketPoolSize": 8,
  "TxPacketDataSize": 64,
  "networks": [
    {
      "name": "CAN0",
      "me": "AS",
      "dbc": "CAN0.dbc",
      "RxPdus": [ ... ],
      "TxPdus": [ ... ]
    }
  ]
}
```

| 字段 | 说明 |
| --- | --- |
| `RxPacketPoolSize` / `TxPacketPoolSize` | 动态接收/发送报文池的预分配缓冲区数量（对应宏 `CANIF_RX_PACKET_POOL_SIZE` / `CANIF_TX_PACKET_POOL_SIZE`，0 表示禁用报文池） |
| `RxPacketDataSize` / `TxPacketDataSize` | 报池中每个缓冲区的负载字节数（CAN FD 用 64，经典 CAN 用 8） |
| `networks` | 每个 CAN 控制器对应一个条目；CanIf 通道号等于网络下标，即 CAN 控制器 ID |

## 2. 网络配置

每个 network 条目配置一个 CAN 控制器：

* `name` - 网络名，例如 `CAN0`；
* `me` - 本 ECU 在 DBC 文件中的节点名；
* `dbc` - 用于自动导入 Com 报文的 DBC 数据库。DBC 中应只包含 Com 信号报文；诊断（CanTp）、NM、XCP、SecOC 帧在 `RxPdus`/`TxPdus` 中显式列出，应从 DBC 中剔除；
* `ignore` - 可选，生成器需要跳过的 DBC 报文名列表；
* `E2E` - 可选，需要 E2E 保护/校验回调的 DBC 报文名列表（仅在定义 `USE_E2E` 时生成）。

所有 RxPdu、TxPdu 名称在整个配置中必须唯一。

## 3. RxPdu / TxPdu 条目

```json
"RxPdus": [
  { "name": "P2P_RX", "id": "0x731", "hoh": 0, "up": "CanTp" },
  { "name": "OSEKNM0_RX", "id": "0x400", "mask": "0x700", "hoh": 0, "up": "OsekNm" }
],
"TxPdus": [
  { "name": "P2P_TX", "id": "0x732", "hoh": 0, "up": "CanTp" },
  { "name": "OSEKNM0_TX", "id": "0x400", "dynamic": true, "hoh": 0, "up": "OsekNm" }
]
```

| 字段 | 说明 |
| --- | --- |
| `name` | 唯一的 PDU 名称。CanTp PDU 必须遵循 `${channel}_RX` / `${channel}_TX` 命名（见 CanTp 文档） |
| `id` | 十六进制 CAN ID；扩展帧使用 29 位 IDE 标识 |
| `mask` | 可选的接收过滤掩码；硬件匹配条件为 `(接收ID & mask) == (id & mask)`，默认 `0x1FFFFFFF`（精确匹配） |
| `hoh` | 硬件对象句柄：控制器本地的 HRH/HTH 索引，多个 PDU 可共用一个 HOH |
| `up` | 上层模块名：`CanTp`、`OsekNm`、`CanNm`、`CanTSyn`、`Xcp`、`SecOC`、`PduR` 或用户回调（`User...`，见下文） |
| `dynamic` | 可选发送标志：该 PDU 运行时从发送报文池动态分配，而非使用静态缓冲区 |

## 4. 硬件对象句柄（HOH）

`hoh` 按控制器对硬件邮箱编号。以两个 CAN 控制器、各 4 个邮箱为例，全局枚举为：

```c
enum {
  CAN0_HTH0 = 0,
  CAN0_HTH1 = 1,
  CAN0_HTH2 = 2,
  CAN0_HTH3 = 3,
  CAN1_HTH0 = 4,
  CAN1_HTH1 = 5,
  CAN1_HTH2 = 6,
  CAN1_HTH3 = 7,
};
```

## 5. 用户自定义上层

`up` 值以 `User` 开头时注册的是用户回调对，而非协议栈模块。建议使用不同后缀，如 `User0Rx`、`User0Tx`：

```c
/* 配置为 "up": "User0Rx" 的 RxPdu 收到报文时调用 */
void User0Rx_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/* 配置为 "up": "User0Tx" 的 TxPdu 发送完成后调用 */
void User0Tx_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);
```

用户在已配置的 TxPdu 上发送：

```c
ret = CanIf_Transmit(CANIF_USER0_TX, &PduInfo);
```

## 6. 生成器输出

[CanIf.py](../../tools/generator/CanIf.py) 把显式 Rx/Tx PDU 列表与从 DBC 导入的全部报文合并，在配置目录（例如[app/app/config/Com/GEN](../../app/app/config/Com/GEN/)）下生成`GEN/CanIf_Cfg.c`、`GEN/CanIf_Cfg.h` 以及中间产物 `GEN/CanIf.json`。
