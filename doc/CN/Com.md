---
layout: post
title: AUTOSAR Com 配置
category: AUTOSAR
comments: true
---

# Com 模块配置说明

AUTOSAR Com 模块负责基于信号的通信：发送时把信号打包进 I-PDU，接收后把 I-PDU解包为信号。配置由 [Com.py](../../tools/generator/Com.py) 消费，它从 DBC（CAN）或 LDF（LIN）数据库导入报文/信号定义，生成 `GEN/Com_Cfg.c` / `GEN/Com_Cfg.h`。本文示例为 [app/app/config/Com/Com.json](../../app/app/config/Com/Com.json)，生成产物位于 [app/app/config/Com/GEN](../../app/app/config/Com/GEN/)。

## 1. CAN 网络示例

```json
{
  "class": "Com",
  "E2E": "../E2E/E2E.json",
  "networks": [
    {
      "name": "CAN0",
      "network": "CAN",
      "device": "simulator_v2",
      "port": 0,
      "baudrate": 500000,
      "me": "AS",
      "groups": [
        { "SystemTime": ["year", "month", "day", "hour", "minute", "second"] }
      ],
      "use_dbc": true,
      "dbc": "CAN0.dbc",
      "trigger": ["CanNmUserData"],
      "E2E": [ { "name": "TxMsgTime", "profile": "P11" } ]
    }
  ]
}
```

## 2. LIN 网络示例

LIN 网络用 `ldf` 替代 `dbc`。生成器先把 LDF 转换为内部 DBC 表示，后续处理完全相同：

```json
{
  "class": "Com",
  "networks": [
    {
      "name": "LIN0",
      "network": "LIN",
      "me": "AS",
      "ldf": "LIN0.ldf"
    }
  ]
}
```

网络名必须与 LinIf 使用的名称一致，以保证信号路由和 LDF 解释一致（见 LinIf文档）。

## 3. 网络字段

| 字段 | CAN | LIN | 说明 |
| --- | --- | --- | --- |
| `name` | 需要 | 需要 | 网络标识，须与 CanIf/LinIf 一致 |
| `network` | `"CAN"` | `"LIN"` | 物理网络类型 |
| `me` | 需要 | 需要 | 本 ECU 在 DBC/LDF 中的节点名 |
| `device` | 需要 | 不需要 | CAN 设备名，例如 `simulator_v2` |
| `port` | 需要 | 不需要 | 控制器端口号 |
| `baudrate` | 需要 | 不需要 | CAN 波特率（bit/s） |
| `dbc` / `ldf` | `dbc` | `ldf` | 包含报文和信号的数据库文件 |
| `use_dbc` | 可选 | - | 为 true 时自动导入 DBC 中节点 `me` 的全部报文 |
| `groups` | 可选 | 可选 | 信号组；每组把一个组名映射到其成员信号列表 |
| `trigger` | 可选 | - | 使用触发发送（trigger-transmit）的报文名列表（LIN 下所有报文均为触发发送） |
| `messages` | 可选 | 可选 | 手写的报文/信号定义，用于补充或覆盖数据库导入（例如 DBC 中没有的 SecOC PDU） |
| `E2E` | 可选 | 可选 | 每个报文的 E2E 配置列表，如 `{ "name": "TxMsgTime", "profile": "P11" }`；仅在定义 `USE_E2E` 时生成 |
| `enable_message_tx_callout` / `enable_message_rx_callout` | 可选 | 可选 | 为 true 时生成全局用户回调，每发送/接收一条报文都会调用 |

## 4. 信号与信号组

* 信号带有 `start`、`size`、`endian`（`big`/`little`）、`sign`、`factor`、`offset`、`min`、`max` 以及可选的 `node` 列表；生成器据此生成打包/解包代码和信号初始值。
* `groups` 把若干信号收集为一个 AUTOSAR 信号组，作为一个整体被一致地接收和读取（例如 `SystemTime` 组）。
* 列入 `trigger` 的报文通过 `Com_TriggerIPDUSend()` 按需发送，而不是由周期定时器发送。

## 5. 生成器

[Com.py](../../tools/generator/Com.py) 读取 JSON 和数据库文件，对 LIN 网络执行可选的 LDF 转 DBC，再完成信号组、trigger、E2E 和回调处理，最终在配置目录生成[`GEN/Com.json`](../../app/app/config/Com/GEN/Com.json) 以及`Com_Cfg.c`/`Com_Cfg.h`。
