---
layout: post
title: AUTOSAR Com
category: AUTOSAR
comments: true
---

# Configuration Notes for Com Module

## Example Configuration

```json
{
  "class": "Com",
  "networks": [
    {
      "name": "CAN0",
      "network": "CAN",
      "device": "simulator_v2",
      "port": 0,
      "baudrate": 500000,
      "me": "AS",
      "groups": [
        {
          "SystemTime": ["year", "month", "day", "hour", "minute", "second"]
        }
      ],
      "dbc": "CAN0.dbc"
    }
  ]
}
```

## Network Configuration

### CAN Networks
- The `dbc` field specifies the CAN database file containing messages/signals
- The `groups` field defines AUTOSAR Com group signals (optional)
- All messages/signals from the DBC will be automatically imported

### Generated Configuration
The build system generates [`GEN/Com.json`](../../app/app/config/Com/GEN/Com.json) containing:
- All converted DBC messages/signals
- Group signal definitions
- Network parameters

### LIN Networks
For LIN networks, use `ldf` instead of `dbc`:

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

## Key Fields

| Field | Description |
|-------|-------------|
| `name` | Network identifier |
| `network` | Network type (CAN/LIN) |
| `device` | Hardware device name |
| `port` | Physical port number |
| `baudrate` | CAN baudrate (bits/sec) |
| `me` | ECU identifier |
| `groups` | Signal grouping definitions |
| `dbc`/`ldf` | Database file path |

## Generator

Configuration is processed by: [Com.py](../../tools/generator/Com.py)