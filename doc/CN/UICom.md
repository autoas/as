---
layout: post
title: UICom Lua 脚本
category: Tools
comments: true
---

# UICom Lua 脚本

UICom 是 [AsOne](../../tools/asone) PC 工具的 COM 面板，可用[Lua](https://www.lua.org/) 脚本扩展其行为：脚本可以周期性发送报文、修改信号值、响应接收到的报文以及绘制实时图表。本文介绍两种脚本模式及其回调函数。

脚本引擎实现在 [UICom.cpp](../../tools/asone/src/ui/UICom.cpp)，图表 API 在[figure.cpp](../../tools/asone/src/ui/figure/figure.cpp)。

## 1. 全局脚本（"com.lua"）

全局脚本对整个 COM 面板只加载一次。它的 `init()` 和 `main()` 函数会周期性执行，还可以定义按报文命名的回调 `on_rx_<网络>_<报文>()` 和`on_tx_<网络>_<报文>()`。

示例 `com.lua`：

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

`init()` 和 `main()` 的返回值是调用周期，单位为毫秒（上例为 100 ms）。

### 1.1 图表 API

| 调用 | 说明 |
| --- | --- |
| `figure.create(fig)` | 根据描述表创建图表窗口（`name`、坐标轴标题/范围、`lines`） |
| `figure.add_point(figName, lineName, x, y)` | 向指定曲线追加一个点 |

## 2. 单报文脚本

每条 TX 或 RX 报文都可以有自己的脚本文件（默认名为 `<报文名>.lua`）。UI 会把一个包含该报文所有信号当前值的键值表传给脚本。

示例 `RxMsgAbsInfo.lua`：

```lua
require("com")

period = 100

VehicleSpeed = 100

-- signals 是包含该报文每个信号当前值的键值表。
-- init() 可以修改默认值，并返回信号表和调用周期（毫秒）。
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
  -- 更新其他报文的信号
  com.set("CAN0.TxMsgTime.year", year)
  -- 第一个返回值是本报文需要更新的信号表
  return signals, period
end

-- 仅 RX 报文：每收到一帧报文时调用
function on_rx(signals)
  print(signals.second)
end

-- 仅 TX 报文：报文成功发送后调用
function on_tx()
  print("on_tx")
end
```

### 2.1 回调参考

| 回调 | 适用范围 | 说明 |
| --- | --- | --- |
| `init(signals)` | 一次 | 接收默认信号表；返回 `signals, period` |
| `main(signals)` | 周期执行 | 更新信号值；返回 `signals, period` |
| `on_rx(signals)` | RX 报文 | 收到报文时调用 |
| `on_tx()` | TX 报文 | 成功发送后调用 |

### 2.2 跨报文信号访问

| 调用 | 说明 |
| --- | --- |
| `com.get("<网络>.<报文>.<信号>")` | 读取任意报文的信号值（例如 `com.get("CAN0.TxMsgTime.year")`） |
| `com.set("<网络>.<报文>.<信号>", value)` | 写入其他报文的信号值 |

## 3. 加载脚本

在 COM 面板上使用脚本输入框/浏览按钮加载脚本。全局脚本默认为 `com.lua`，单报文脚本默认为 `<报文名>.lua`。更多 Lua 示例（诊断测试仪、XCP）见[tools/asone/examples](../../tools/asone/examples)。
