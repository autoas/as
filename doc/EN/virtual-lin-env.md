---
layout: post
title: Virtual LIN Environment - Develop and Learn Without Hardware
category: AUTOSAR
comments: true
---

# LIN Bus Simulation

This article assumes the reader has some basic knowledge of LIN. Unlike CAN, which is multi-master and broadcast, LIN is a **single-master, multi-slave, command/response** network. Every transfer is initiated by the master:

1. The master sends a header: Break + Sync + PID (protected ID: the 6-bit ID plus 2 parity bits);
2. The slave addressed by that ID (or the master itself) puts the response (1-8 data bytes) and the checksum into the response slot;
3. The checksum is either classic or enhanced (the enhanced one also covers the PID). The diagnostic frames 0x3C/0x3D normally use the enhanced checksum.

So from a software point of view, a LIN exchange consists of just two operations: **sending a header** and **receiving/sending the response data**. By forwarding headers and data between nodes over sockets, an entire LIN network can be emulated on one PC - including LinIf, LinTp (ISO 15765 over LIN) and the LIN bootloader.

The project provides two generations of LIN simulator and one common client library, linlib.

## LIN Simulator v1 (central TCP forwarder)

The v1 simulator is a TCP server; its application name is `LinSimulator`:

* It listens on `127.0.0.1`, TCP port `100 + bus id` (100 for bus 0);
* Every node is a TCP client; each frame goes to the simulator first and the simulator then forwards it to the other nodes, emulating bus distribution;
* The simulator understands the LIN framing: a header (`H`) must be paired with response data (`D`) within 1 second, otherwise a `Lin Error` timeout is reported; a complete frame (header plus data, `F`) is logged and forwarded directly;
* `-f <Mask>#<Code>` filters the printed log by PID.

Source: [lin_simulator.c](../../tools/libraries/device/utils/lin_simulator.c).

## LIN Simulator v2 (UDP multicast, recommended)

Like the CAN simulator v2, the v2 LIN simulator uses **UDP multicast** (group `224.244.224.245`, UDP port `10000 + bus id`), with device name `simulator_v2`:

* **No central server process is required** - every node simply joins the multicast group and exchanges frames directly, which is much closer to the real bus;
* each frame carries the sender's 16-byte UUID; a node uses the UUID to discard its own echoed frames;
* the `LinSimulatorV2` program is an **optional bus monitor**: it only joins the multicast group and prints bus traffic (including the inter-frame gap in milliseconds); it does not forward anything.

The simulator source is [lin_simulator_v2.c](../../tools/libraries/device/utils/lin_simulator_v2.c), and the client implementation is [lin_simulator_v2.cpp](../../tools/libraries/device/src/Lin/lin_simulator_v2.cpp).

## The linlib Client Library

Regardless of whether the v1 or v2 simulator, or a real hardware bridge is used, every node accesses the LIN bus through the common linlib; the header is [linlib.h](../../tools/libraries/Lin/include/linlib.h):

```c
typedef uint32_t lin_id_t;

int  lin_open(const char *device_name, uint32_t port, uint32_t baudrate);
bool lin_write(int busid, lin_id_t id, uint8_t dlc, const uint8_t *data, bool enhanced);
bool lin_read(int busid, lin_id_t id, uint8_t dlc, uint8_t *data, bool enhanced,
              int timeout /* ms */);
bool lin_close(int busid);
```

Usage conventions (this is the biggest difference from canlib):

* `lin_read()` is the master polling call: it first sends the **header** for the given ID onto the bus, then waits up to `timeout` milliseconds for the slave response and verifies the checksum automatically;
* `lin_write()` is used for a frame that carries data (e.g. the master request frame MRF): header and data are sent together;
* for an ID no larger than `0x3F`, the two PID parity bits are computed by the library; passing a value larger than `0x3F` is treated as a PID that already carries parity / an extended ID;
* `enhanced` selects the enhanced vs. classic checksum;
* internally, wire frames are distinguished by type characters: `H` header, `D` data, `F` header plus data (extended IDs use the lowercase `h`/`f`).

Supported devices:

| device_name | Device |
| --- | --- |
| `simulator` | TCP LIN simulator v1 (requires LinSimulator, port 100+bus) |
| `simulator_v2` | UDP multicast LIN simulator (**v2, recommended**, serverless, port 10000+bus) |
| `lvds` | Real LIN over an RS232-connected LVDS debugger (used to flash the lvds-arch platform) |
| `i2c` | LIN slave I2C bridge |
| `spi` | LIN slave SPI bridge |

On the AUTOSAR side, the simulator platform wraps linlib/DevLib with an AUTOSAR Lin driver, [LinAc.c](../../app/platform/simulator/src/LinAc.c) (`Lin_SendFrame`, `Lin_MainFunction`, `Lin_MainFunction_Read`); simulated applications such as LinApp and LinBL are attached to the virtual bus through it.

AsPy provides the matching Python wrapper:

```python
>>> import AsPy
>>> master = AsPy.lin('simulator', 0)                 # device/port/baudrate/enhanced/timeout as keyword args
>>> master.write(0x3C, bytes([0x10, 0x01]))           # send a frame that carries data
True
>>> master.read(0x3D, 8)                              # send the 0x3D header and wait for the slave response
[True, 61, b'...']                                    # [success, actual PID, data]
```

## Lab: UDS Diagnostics Against a Simulated LIN Slave

In the following walk-through, LinApp emulates a LIN diagnostic slave (its [LinIf configuration](../../app/bootloader/config/LinIf/LinIf.json) is in slave mode: `0x3C` master request MRF and `0x3D` slave response SRF, both with the enhanced checksum), while IsoTpSend acts as the master/tester and sends the UDS session-control request `10 01` over LinTp.

```sh
# step 1: in the app tab, build the LIN simulator, the LinApp slave and IsoTpSend
D:\repository\as>scons --app=LinSimulator
D:\repository\as>scons --app=LinApp
D:\repository\as>scons --app=IsoTpSend

# sim tab: start the v1 simulator for LIN bus 0 (not needed for simulator_v2)
D:\repository\as>build\nt\GCC\one\LinSimulator.exe 0
lin(0) socket driver on-line!
```

```sh
# step 2: in the app tab, run the simulated LIN slave node
D:\repository\as>build\nt\GCC\LinApp\LinApp.exe
```

```sh
# step 3: in the boot tab, the master/tester sends UDS 10 01 over LIN
D:\repository\as>build\nt\GCC\IsoTpSend\IsoTpSend.exe -d LIN.simulator -p 0 -t 0x3c -r 0x3d -v 1001
TX: 10 01
RX: 50 01 13 88 00 32
```

In the sim tab you can now see the complete header/data traffic on the bus (PID, DLC, checksum). A full LIN diagnostic session has been emulated without any LIN hardware; switching the device name to `LIN.simulator_v2` and dropping the LinSimulator process moves the setup to the recommended v2 multicast scheme.
