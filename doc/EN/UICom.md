## UICom

### UICom Lua script for everything

example "com.lua"

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
### UICom Lua script for each TX or RX message

example "RxMsgAbsInfo.lua"

```lua
require("com")

period = 100

VehicleSpeed = 100

-- the signals is a key-value table that represent the value of each signal of the TX message
-- the init function can modify the value of each and return a key-value table to let the UICom
-- to use it as default
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
  -- update other message's singal
  com.set("CAN0.TxMsgTime.year", year)
  -- the first return is the signals of this message that need to be updated
  return signals, period
end

-- this is for RX message only that a message is received
function on_rx(signals)
  print(signals.second)
end

-- this is for TX message only when a message is successfully transmited
function on_tx()
  print("on_tx")
end
```