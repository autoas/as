---
layout: post
title: UICom Lua Scripting
category: Tools
comments: true
---

# UICom Lua Scripting

UICom is the COM panel of the [AsOne](../../tools/asone) PC tool. Its behavior can be extended with [Lua](https://www.lua.org/) scripts: a script may send messages periodically, modify signal values, react to received messages and draw real-time figures. This page describes the two scripting modes and their callback functions.

The script engine is implemented in [UICom.cpp](../../tools/asone/src/ui/UICom.cpp) and the figure API in [figure.cpp](../../tools/asone/src/ui/figure/figure.cpp).

## 1. Global Script ("com.lua")

The global script is loaded once for the whole COM panel. Its `init()` and `main()` functions run periodically, and it can define message-specific callbacks named `on_rx_<Network>_<Message>()` and `on_tx_<Network>_<Message>()`.

Example `com.lua`:

```lua
require("com")
require("figure")
x = 0
y = 0
function init()
  fig = { name="figure0", titleX="x", titleY="y", minX=0, maxX=100, minY=0, maxY=100,
  lines = { { name="line0", type="line"}, { name="line1", type="line"} } }
  figure.create(fig)
  return 100
end

function main()
  x = x + 0.1
  y0 = x
  y1 = 2*x
  figure.add_point("figure0", "line0", x, y0)
  figure.add_point("figure0", "line1", x, y1)
  return 100
end

function on_rx_CAN0_TxMsgTime()
  print("on_rx_CAN0_TxMsgTime")
end

function on_tx_CAN0_RxMsgAbsInfo()
  print("on_tx_CAN0_RxMsgAbsInfo")
end
```

The return value of `init()` and `main()` is the call period in milliseconds (100 ms in the example).

### 1.1 Figure API

| Call | Description |
| --- | --- |
| `figure.create(fig)` | Creates a figure window from a descriptor table (`name`, axis titles/ranges and `lines`) |
| `figure.add_point(figName, lineName, x, y)` | Appends one point to the named line |

## 2. Per-Message Script

Every TX or RX message can have its own script file (by default named `<MessageName>.lua`). The UI passes a key-value table holding the current value of every signal of the message.

Example `RxMsgAbsInfo.lua`:

```lua
require("com")

period = 100

VehicleSpeed = 100

-- signals is a key-value table with the current value of each signal of the
-- message. init() may adjust the defaults and returns them together with the
-- call period in milliseconds.
function init(signals)
  return signals, period
end

function main(signals)
  VehicleSpeed = VehicleSpeed + 100
  if VehicleSpeed > 24000 then
    VehicleSpeed = 0
  end
  signals.VehicleSpeed = VehicleSpeed
  year = com.get("CAN0.TxMsgTime.year")
  year = year + 1
  -- update a signal of another message
  com.set("CAN0.TxMsgTime.year", year)
  -- the first return value is the signal table of this message to be updated
  return signals, period
end

-- RX messages only: called whenever the message is received
function on_rx(signals)
  print(signals.second)
end

-- TX messages only: called after the message was transmitted successfully
function on_tx()
  print("on_tx")
end
```

### 2.1 Callback Reference

| Callback | Availability | Description |
| --- | --- | --- |
| `init(signals)` | once | Receives the default signal table; returns `signals, period` |
| `main(signals)` | periodic | Updates signal values; returns `signals, period` |
| `on_rx(signals)` | RX messages | Called on message reception |
| `on_tx()` | TX messages | Called after successful transmission |

### 2.2 Cross-Message Signal Access

| Call | Description |
| --- | --- |
| `com.get("<Network>.<Message>.<Signal>")` | Reads a signal value of any message (for example `com.get("CAN0.TxMsgTime.year")`) |
| `com.set("<Network>.<Message>.<Signal>", value)` | Writes a signal value of another message |

## 3. Loading a Script

Use the script input field / browse button on the COM panel. The global script defaults to `com.lua`; a per-message script defaults to `<MessageName>.lua`. More Lua examples (diagnostic tester, XCP) can be found in [tools/asone/examples](../../tools/asone/examples).
