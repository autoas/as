---
layout: post
title: AUTOSAR Com Configuration
category: AUTOSAR
comments: true
---

# Configuration Notes for the Com Module

The AUTOSAR Com module manages signal-based communication: it packs signals into I-PDUs for transmission and unpacks received I-PDUs into signals. The configuration is consumed by [Com.py](../../tools/generator/Com.py), which imports message/signal definitions from a DBC (CAN) or LDF (LIN) database and generates `GEN/Com_Cfg.c` / `GEN/Com_Cfg.h`. The example in this document is [app/app/config/Com/Com.json](../../app/app/config/Com/Com.json) and the generated output is in [app/app/config/Com/GEN](../../app/app/config/Com/GEN/).

## 1. CAN network example

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

## 2. LIN network example

For LIN, `ldf` replaces `dbc`. The generator converts the LDF into an internal DBC representation first, so all downstream processing is identical:

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

The network name must match the name used by LinIf so that signal routing and the LDF interpretation stay consistent (see the LinIf document).

## 3. Network fields

| Field | CAN | LIN | Description |
| --- | --- | --- | --- |
| `name` | yes | yes | Network identifier, must match CanIf/LinIf |
| `network` | `"CAN"` | `"LIN"` | Physical network type |
| `me` | yes | yes | Local ECU node name inside the DBC/LDF |
| `device` | yes | no | CAN device name, for example `simulator_v2` |
| `port` | yes | no | Controller port index |
| `baudrate` | yes | no | CAN baud rate in bit/s |
| `dbc` / `ldf` | `dbc` | `ldf` | Database file with messages and signals |
| `use_dbc` | optional | - | When true, all messages of node `me` in the DBC are imported |
| `groups` | optional | optional | Signal groups; each group maps one group name to its member signals |
| `trigger` | optional | - | Message names that use trigger-transmit (on LIN every message is trigger-transmitted) |
| `messages` | optional | optional | Hand-written message/signal definitions added to or overriding the database import (for example SecOC PDUs not present in the DBC) |
| `E2E` | optional | optional | Per-message E2E profile list, for example `{ "name": "TxMsgTime", "profile": "P11" }`; emitted only when `USE_E2E` is defined |
| `enable_message_tx_callout` / `enable_message_rx_callout` | optional | optional | When true, generate a global user callout invoked for every transmitted/received message |

## 4. Signals and groups

* Signals carry `start`, `size`, `endian` (`big`/`little`), `sign`, `factor`, `offset`, `min`, `max` and an optional `node` list; the generator emits the pack/unpack code and the signal initial values.
* A `groups` entry collects several signals into one AUTOSAR signal group that is received and read consistently as a unit (for example the `SystemTime` group).
* Messages listed in `trigger` are sent on demand through `Com_TriggerIPDUSend()` instead of a periodic timer.

## 5. Generator

[Com.py](../../tools/generator/Com.py) reads the JSON and the database files, runs the optional LDF-to-DBC conversion for LIN networks, applies group, trigger, E2E and callout processing and emits [`GEN/Com.json`](../../app/app/config/Com/GEN/Com.json) plus `Com_Cfg.c`/`Com_Cfg.h` in the configuration directory.
