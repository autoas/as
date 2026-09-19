---
layout: post
title: 虚拟 CAN 环境：没有硬件也能开发与学习
category: AUTOSAR
comments: true
---

# CAN 总线仿真技术

本文假设读者具备一定的 CAN 协议知识；即使不了解也没有关系，CAN 协议本身非常简单。协议文档虽然篇幅很大，但其中很多是硬件特性的介绍。从软件角度看，CAN 通信无外乎两件事：发送报文与接收报文。

从软件定义的角度，AUTOSAR 与 Linux 的 CAN 报文结构分别如下：

```c
// AUTOSAR
typedef struct {
  uint8_t length;
  Can_IdType id;
  uint8_t *sdu;
} Can_PduType;

// Linux
struct can_frame {
    canid_t can_id;  /* 32 bit CAN_ID + EFF/RTR/ERR flags */
    __u8    can_dlc; /* frame payload length in byte (0 .. 8) */
    __u8    __pad;   /* padding */
    __u8    __res0;  /* reserved / padding */
    __u8    __res1;  /* reserved / padding */
    __u8    data[8] __attribute__((aligned(8)));
};
```

CAN 网络一个突出的特性是广播：同一总线上某个节点发送的报文，可以被其他所有节点同时接收到。基于这一特性，我们可以在 Windows 或 Linux 操作系统上利用 socket 模拟总线行为，实现 CAN 网络通信的仿真。

Linux 内核本身就提供了 [virtual CAN](https://www.pragmaticlinux.com/2021/10/how-to-create-a-virtual-can-interface-on-linux/)（vcan）接口，有兴趣的读者可以深入研究；而 Windows 上目前没有类似的原生机制，这正是本项目自研基于 socket 的 CAN 总线仿真方案的原因。

![canbus-sim](../images/canbus-sim.png)

如上图所示，CAN Sim 是一个 TCP Server，N0、N1、N2 是 TCP Client，各自与 CAN Sim 建立 TCP 连接。当 N0 发送 CAN 报文时，报文先发送给 CAN Sim，再由 CAN Sim 转发给 N1 和 N2，从而实现虚拟广播。这就是 v1 版 TCP 仿真器，其源码为 [can_simulator.c](../../tools/libraries/Can/utils/can_simulator.c)，实现相当简单，代码不足 500 行，有兴趣的读者可以阅读理解。

### CAN 仿真器 v2（推荐）

v2 版仿真器是目前推荐使用的版本。它不再需要中心 TCP 服务器，而是直接采用 **UDP 组播**（组播地址 `224.244.224.245`，UDP 端口为 `8000 + 总线号`）：

* 无需启动任何仿真器进程——每个节点直接加入组播组即可相互收发报文，更接近 CAN 总线真实的广播行为；
* 每帧携带 8 字节时间戳和发送方的 16 字节 UUID，节点通过 UUID 丢弃自己发出的回声帧；
* 数据载荷最大支持 64 字节，即支持 CAN FD。

v2 客户端实现位于 [simulator_can_v2.cpp](../../tools/libraries/Can/src/simulator_can_v2.cpp)，对应的设备名为 `simulator_v2`。v1 的 TCP 方案则保留兼容，多年来它稳定支撑了 CanTp、Dcm、Com、CanNm、OsekNm 等各通信栈基础模块的开发与验证，也辅助验证了完整的 bootloader 解决方案。

Client 端同样开源，头文件 [canlib.h](../../tools/libraries/Can/include/canlib.h) 提供的 API 如下：

```c
/* 扩展帧：将 CAN_ID_EXTENDED 与 ID 按位或 */
#define CAN_ID_EXTENDED 0x80000000U
#define CAN_ID_ANY     ((uint32_t)-1) /* read/wait 过滤：任意 ID          */
#define CAN_ID_MONITOR ((uint32_t)-2) /* read/wait 收发混合 trace 队列     */

typedef struct {
  uint32_t canid;            /* 11/29 位 ID，扩展帧需 OR CAN_ID_EXTENDED */
  uint8_t  dlc;              /* 0 .. 64（CAN FD） */
  uint8_t  data[64];
  uint64_t timestamp;        /* 微秒 */
} can_frame_t;

int  can_open(const char *device_name, uint32_t port, uint32_t baudrate);
bool can_write(int busid, uint32_t canid, uint8_t dlc, const uint8_t *data);
bool can_read(int busid, uint32_t *canid /* InOut */, uint8_t *dlc /* InOut */, uint8_t *data);
bool can_write_v2(int busid, can_frame_t *can_frame); /* 可自带时间戳，支持 CAN FD */
bool can_read_v2(int busid, can_frame_t *can_frame);  /* 同时返回时间戳 */
bool can_close(int busid);
bool can_reset(int busid);
bool can_wait(int busid, uint32_t canid, uint32_t timeoutMs);
bool can_wait_v2(int busid, uint32_t canid, uint32_t timeoutMs); /* 低延迟，供 ISO-TP v2 刷写使用 */
```

目前 canlib 支持的设备如下：

| device_name | 设备 |
| --- | --- |
| `simulator` | TCP CAN 总线仿真器（v1，需启动 CanSimulator） |
| `simulator_v2` | UDP 组播 CAN 总线仿真器（**v2，推荐**，无需服务器进程） |
| `qemu` | QEMU 串口虚拟 CAN |
| `vxl` | [Vector XL Driver Library](https://www.vector.com/int/en/products/products-a-z/libraries-drivers/xl-driver-library/)（如 CANcaseXL） |
| `peak` | [PEAK CAN](https://www.peak-system.com/)（经典 CAN） |
| `peakfd` | PEAK CAN（CAN FD） |
| `zlg` | [周立功 ZLG CAN](https://www.zlg.cn/can/can/index.html) |

API 数量很少。为了与 AUTOSAR 兼容，项目在 canlib 之上又封装了一层符合 AUTOSAR CAN 驱动规范的接口，代码位于 [simulator/Can.cpp](../../app/platform/simulator/src/Can.cpp)，API 如下：

```c
void Can_Init(const Can_ConfigType *Config);
void Can_DeInit(void);
Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo);
Std_ReturnType Can_SetControllerMode(uint8_t Controller, Can_ControllerStateType Transition);
void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr);
```

canlib 及其 AUTOSAR CAN 驱动的具体使用方法将在后续文章中介绍。

## 实验：用 Python 收发报文

下面的演示使用 v1 版 TCP 仿真器（需要先运行 CanSimulator）。如果使用推荐的 v2 版仿真器，无需编译和启动 CanSimulator，只要把 `AsPy.can()` 的设备名从 `'simulator'` 改为 `'simulator_v2'` 即可，不需要任何中心进程。

```sh
# step 1: 切换到 app 标签页，编译仿真器和 AsPy 库
D:\repository\as>scons --app=CanSimulator
D:\repository\as>scons --lib=AsPy
D:\repository\as>cp build\nt\GCC\AsPy\AsPy.dll AsPy.pyd

# sim 标签页：启动 CAN 总线 0 的仿真器（simulator_v2 无需此步）
D:\repository\as>build\nt\GCC\CanSimulator\CanSimulator.exe 0
```

```sh
# step 2: 切换到 app 标签页，启动 Python 交互环境，模拟 CAN 节点 N0
D:\repository\as>python
>>> import os
>>> os.add_dll_directory('C:/msys64/mingw64/bin')
>>> import AsPy
>>> n0 = AsPy.can('simulator', 0)

# step 3: 切换到 boot 标签页，再启动一个 Python 交互环境，模拟 CAN 节点 N1
D:\repository\as>python
>>> import os
>>> os.add_dll_directory('C:/msys64/mingw64/bin')
>>> import AsPy
>>> n1 = AsPy.can('simulator', 0)
```

```text
# step 4: 切回 sim 标签页，可以看到 CAN Sim 检测到两个节点上线
can socket 104 on-line!
can socket 108 on-line!
```

```python
# step 5: 在 app 标签页，N0 发送一条 CAN 报文
>>> n0.write(0x731, bytes(range(8)))
True
```

```text
# step 6: sim 标签页可以看到 N0 的报文已发送
canid=00000731,dlc=08,data=[00,01,02,03,04,05,06,07,] [........] @ 1011.104675 s rel 625132.69 ms
```

```python
# step 7: 在 boot 标签页，N1 接收 N0 的报文，再回复一条
>>> n1.read(0x731)
[True, 1841, b'\x00\x01\x02\x03\x04\x05\x06\x07']
# N0 发出的 ID 为 0x731 的报文已被 N1 收到
>>> n1.write(0x732, bytes(range(8)))
True
```

```text
# step 8: sim 标签页可以看到 N1 的报文
canid=00000732,dlc=08,data=[00,01,02,03,04,05,06,07,] [........] @ 1098.185181 s rel 87080.51 ms
```

```python
# step 9: 回到 app 标签页，N0 接收 N1 的报文
>>> n0.read(0x732)
[True, 1842, b'\x00\x01\x02\x03\x04\x05\x06\x07']
# N1 发出的 ID 为 0x732 的报文同样被 N0 收到
```

可以看到，借助 canlib，只用 Python 就能轻松访问 CAN 设备、操纵报文收发，可以快速编写 CAN 相关测试用例和上位机工具。
