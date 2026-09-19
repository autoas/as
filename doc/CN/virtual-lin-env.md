---
layout: post
title: 虚拟 LIN 环境：没有硬件也能开发与学习
category: AUTOSAR
comments: true
---

# LIN 总线仿真技术

本文假设读者具备一定的 LIN 协议知识。LIN 与 CAN 不同：CAN 是多主广播，而 LIN 是**单主多从、命令-响应（command/response）**网络。通信总是由主节点发起：

1. 主节点发送帧头（Header）：Break + Sync + PID（受保护 ID，6 位 ID 加 2 位奇偶校验）；
2. 被该 ID 点名的从节点（或主节点自己）在响应槽中回送数据（1~8 字节）和校验和；
3. 校验和分经典校验（classic）与增强校验（enhanced，把 PID 也计入），诊断帧 0x3C/0x3D 通常使用增强校验。

因此从软件角度看，一次 LIN 交互就是两件事：**发帧头**和**收/发响应数据**。只要用 socket 把帧头和数据在各节点间转发，就能在一台 PC 上完整仿真 LIN 网络，包括 LinIf、LinTp（ISO 15765 over LIN）以及 LIN bootloader。

本项目提供两代 LIN 仿真器和统一的客户端库 linlib。

## LIN 仿真器 v1（TCP 中心转发）

v1 仿真器是一个 TCP 服务器，应用名为 `LinSimulator`：

* 监听 `127.0.0.1` 的 `100 + 总线号` 端口（例如总线 0 为 100）；
* 每个节点是一个 TCP 客户端，所有帧先发给仿真器，再由仿真器转发给其他节点，模拟总线分发；
* 仿真器理解 LIN 帧格式：帧头（`H`）与响应数据（`D`）必须配对，1 秒内等不到数据会报 `Lin Error` 超时；完整帧（帧头+数据，`F`）直接记录转发；
* 支持用 `-f <Mask>#<Code>` 按 PID 过滤打印日志。

源码：[lin_simulator.c](../../tools/libraries/device/utils/lin_simulator.c)。

## LIN 仿真器 v2（UDP 组播，推荐）

v2 与 CAN 仿真器 v2 一样，采用 **UDP 组播**（组播地址 `224.244.224.245`，端口 `10000 + 总线号`），设备名为 `simulator_v2`：

* **无需中心服务器进程**：各节点直接加入同一组播组互相收发，更接近真实总线；
* 每帧携带发送方 16 字节 UUID，节点据此丢弃自己发出的回声帧；
* `LinSimulatorV2` 程序是**可选的总线监视器**：它只加入组播组、打印总线报文（含相邻帧间隔毫秒数），不做任何转发。

源码：[lin_simulator_v2.c](../../tools/libraries/device/utils/lin_simulator_v2.c)，客户端实现 [lin_simulator_v2.cpp](../../tools/libraries/device/src/Lin/lin_simulator_v2.cpp)。

## linlib 客户端库

无论 v1/v2 仿真器，还是真实硬件桥接，节点都通过统一的 linlib 访问 LIN 总线，头文件为 [linlib.h](../../tools/libraries/Lin/include/linlib.h)：

```c
typedef uint32_t lin_id_t;

int  lin_open(const char *device_name, uint32_t port, uint32_t baudrate);
bool lin_write(int busid, lin_id_t id, uint8_t dlc, const uint8_t *data, bool enhanced);
bool lin_read(int busid, lin_id_t id, uint8_t dlc, uint8_t *data, bool enhanced,
              int timeout /* ms */);
bool lin_close(int busid);
```

使用约定（这是与 canlib 最大的不同）：

* `lin_read()` 用于主节点轮询：它先向总线发送指定 ID 的**帧头**，然后在 `timeout` 毫秒内等待从节点的响应数据，并自动校验 checksum；
* `lin_write()` 用于携带数据的一帧（例如主节点请求帧 MRF）：帧头与数据一次性发出；
* ID 小于等于 `0x3F` 时库自动计算 PID 的两位奇偶校验位；直接传入大于 `0x3F` 的值则视为已带校验的 PID/扩展 ID；
* `enhanced` 选择增强/经典校验和；
* 内部线上帧用类型字符区分：`H` 帧头、`D` 数据、`F` 帧头+数据（扩展 ID 对应小写 `h`/`f`）。

支持的设备：

| device_name | 设备 |
| --- | --- |
| `simulator` | TCP LIN 仿真器 v1（需启动 LinSimulator，端口 100+总线号） |
| `simulator_v2` | UDP 组播 LIN 仿真器（**v2，推荐**，无需服务器，端口 10000+总线号） |
| `lvds` | 通过 RS232 连接 LVDS 调试器访问真实 LIN（用于 lvds-arch 平台刷写） |
| `i2c` | LIN 从节点 I2C 桥接 |
| `spi` | LIN 从节点 SPI 桥接 |

在 AUTOSAR 一侧，simulator 平台在 linlib/DevLib 之上封装了符合 AUTOSAR Lin 驱动接口的 [LinAc.c](../../app/platform/simulator/src/LinAc.c)（`Lin_SendFrame`、`Lin_MainFunction`、`Lin_MainFunction_Read`），LinApp、LinBL 等仿真应用即通过它挂到虚拟总线上。

Python 侧 AsPy 也提供了对应封装：

```python
>>> import AsPy
>>> master = AsPy.lin('simulator', 0)                 # device/port/baudrate/enhanced/timeout 均可按关键字传入
>>> master.write(0x3C, bytes([0x10, 0x01]))           # 发送携带数据的一帧
True
>>> master.read(0x3D, 8)                              # 发 0x3D 帧头并等待从节点响应
[True, 61, b'...']                                    # [成功, 实际 PID, 数据]
```

## 实验：对仿真 LIN 从节点做 UDS 诊断

下面用 LinApp 模拟一个 LIN 诊断从节点（其 [LinIf 配置](../../app/bootloader/config/LinIf/LinIf.json) 为 slave 模式：`0x3C` 主请求 MRF、`0x3D` 从响应 SRF，均为增强校验和），用 IsoTpSend 充当主节点/诊断仪，经 LinTp 发送 UDS 会话切换请求 `10 01`。

```sh
# step 1: app 标签页，编译 LIN 仿真器、LinApp 从节点和 IsoTpSend
D:\repository\as>scons --app=LinSimulator
D:\repository\as>scons --app=LinApp
D:\repository\as>scons --app=IsoTpSend

# sim 标签页：启动 LIN 总线 0 的 v1 仿真器（simulator_v2 无需此步）
D:\repository\as>build\nt\GCC\one\LinSimulator.exe 0
lin(0) socket driver on-line!
```

```sh
# step 2: app 标签页：运行仿真 LIN 从节点
D:\repository\as>build\nt\GCC\LinApp\LinApp.exe
```

```sh
# step 3: boot 标签页：主节点/诊断仪经 LIN 发送 UDS 10 01
D:\repository\as>build\nt\GCC\IsoTpSend\IsoTpSend.exe -d LIN.simulator -p 0 -t 0x3c -r 0x3d -v 1001
TX: 10 01
RX: 50 01 13 88 00 32
```

此时在 sim 标签页可以看到总线上完整的帧头/数据记录（PID、DLC、checksum）。至此，无需任何 LIN 硬件就完成了一次 LIN 诊断会话仿真；把设备名换成 `LIN.simulator_v2` 并去掉 LinSimulator 进程，即切换到推荐的 v2 组播方案。
