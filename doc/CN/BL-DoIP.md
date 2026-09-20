---
layout: post
title: 主机模拟器上的 BL over DoIP 示例
category: AUTOSAR
comments: true

# 主机模拟器上的 BL over DoIP 示例

## 目录

1. [简介](#1-简介)
2. [架构](#2-架构)
3. [配置](#3-配置)
4. [构建](#4-构建)
5. [运行 DoIPBL](#5-运行-doipbl)
6. [测试通过 DoIP 烧写](#6-测试通过-doip-烧写)
   - [生成虚拟 Flash 驱动](#61-生成虚拟-flash-驱动1052-字节)
   - [生成虚拟应用](#62-生成虚拟应用a-与-b-分区)
   - [为 Flash 驱动与应用签名](#63-为-flash-驱动与应用签名)
   - [通过 DoIP 烧写](#64-通过-doip-烧写)

## 1. 简介

`DoIPBL` 是一个运行在主机模拟器（Windows 或 Linux）上的 Bootloader 应用，演示通过 DoIP（ISO 13400）诊断服务烧写 Bootloader，替代 CAN 方式。

它基于与 `CanBL` 相同的 BL 软件栈（参见 [BL 配置与主机模拟](BL.md)），将 CAN 相关模块（CanTp）替换为以太网诊断栈：DoIP 及其底层的 SoAd 与 TcpIp。BL、Dcm 和 PduR 的配置与行为保持一致。

说明：

- 主机模拟器上的 DoIPBL 构建（`USE_BL` + `USE_DOIP`）会保持 Bootloader 持续运行：既不跳转到应用，也不执行暖复位，因此是一个纯粹的 DoIP 烧写目标，便于复现演示。（CAN BL 仍会像真实硬件一样跳转到应用并复位。）
- 本示例的测试端（诊断仪）是 `Loader` PC 工具，它支持 DoIP 传输（例如 `-d DOIP.224.244.224.245`）。

## 2. 架构

```mermaid
graph TB
    subgraph PC["Loader（PC 工具）"]
        LDR["Loader.exe -d DOIP.224.244.224.245"]
    end
    subgraph HOST["DoIPBL（主机模拟器进程）"]
        TCPIP["TcpIp"] --> SOAD["SoAd"]
        SOAD --> DOIP["DoIP"]
        DOIP --> PDUR["PduR"]
        PDUR --> DCM["Dcm"]
        DCM --> BL["BL"]
        SIM["Simulator（Mcu/复位/跳转胶水）"] --- BL
    end
    LDR -- "UDP 13400 多播发现（224.244.224.245）" --> TCPIP
    LDR -- "TCP 13400 路由激活 + UDS" --> TCPIP
```

诊断请求路径：`Loader -> TCP 13400 -> TcpIp -> SoAd -> DoIP -> PduR(P2P_RX) -> Dcm -> BL`。
响应路径：`BL -> Dcm -> PduR(P2P_TX) -> DoIP -> SoAd -> TcpIp -> TCP -> Loader`。

DoIP 会自动创建两个 SoAd socket：

- 一个 UDP socket，加入车辆识别发现多播组 `224.244.224.245:13400`。
- 一个 TCP 服务端 socket，监听 `13400` 端口（最多 `max_connections` 个连接）。

TCP 连接建立后，诊断仪以源地址 `0xbeef` 发送路由激活请求；激活成功后，UDS 请求以目标地址 `0xdead` 路由到 Dcm。

## 3. 配置

DoIP 相关的所有配置位于 `app/bootloader/config/Net`：

| 文件 | 内容 |
| --- | --- |
| `Network.json` | DoIP 模块：发现地址、`max_connections`、名为 `P2P` 的目标（逻辑地址 `0xdead`）、`default` 例程与地址为 `0xbeef` 的 `default` 诊断仪 |
| `Dcm.json` | DoIPBL 专用的 Dcm 配置，仅 `P2P` 物理通道用于路由（`P2A` 通道仅为满足生成器要求而保留） |
| `PduR.json` | PduR 路由：`P2P_RX` 从 DoIP 到 Dcm，`P2P_TX` 从 Dcm 到 DoIP |
| `Mempool.json` | SoAd/DoIP 使用的内存池（名为 `Net` 的 `MemCluster`） |

DoIP 目标命名为 `P2P`，PduR 例程命名为 `P2P_RX`/`P2P_TX`，从而映射到 `Dcm.json` 生成的 Dcm `P2P` 通道（索引 0）。

应用侧胶水代码位于 `app/bootloader/src/doip.c`（由 `USE_DOIP` 保护），提供：

- `DoIP_default_RoutingActivationAuthenticationCallback` 与 `DoIP_default_RoutingActivationConfirmationCallback`：不做鉴权，直接接受路由激活。
- `Dcm_GetVin`：提供 DoIP 车辆公告所用的 VIN。
- `DoIP_UserGetEID`/`DoIP_UserGetGID`/`DoIP_UserGetPowerModeStatus`/`DoIP_UserGetRoutingActivationResponseOem`：DoIP 用户回调。

在 `app/bootloader/main.c` 中，当定义了 `USE_TCPIP`、`USE_SOAD` 与 `USE_DOIP` 时，初始化会调用 `TcpIp_Init`、`SoAd_Init`、`DoIP_Init` 与 `DoIP_ActivationLineSwitchActive`，并调度 `TcpIp_MainFunction`、`SoAd_MainFunction` 与 `DoIP_MainFunction`。

## 4. 构建

工具链请参考 [构建环境搭建](build-env-setup.md)。然后与 CanBL 模拟测试相同，逐个构建 `AsOne`、`LoaderFBL` 库、`Loader` PC 工具与 `DoIPBL` Bootloader：

```bash
scons --lib=AsOne
scons --lib=LoaderFBL
scons --app=Loader
scons --app=DoIPBL
```

Windows 下的构建产物为：

- `build/nt/GCC/DoIPBL/DoIPBL.exe`，基于 DoIP 的 Bootloader。
- `build/nt/GCC/Loader/Loader.exe`，PC 烧写工具。

构建 `DoIPBL` 时还会根据 `app/bootloader/config/Net/*.json` 在构建目录中生成 Dcm/PduR/SoAd/DoIP 配置代码。

## 5. 运行 DoIPBL

启动 Bootloader（Windows 下请确保 MSYS2 `mingw64/bin` 中的运行时 DLL（如 `libstdc++-6.dll`）在 `PATH` 中）：

```bash
build\nt\GCC\DoIPBL\DoIPBL.exe -v 3
```

DoIPBL 主机构建既不会跳转到应用，也不会复位，因此它会持续运行在默认会话中，随时可接受烧写。

预期行为：

- 打印 `bootloader build @ ...` 后持续运行。
- TcpIp/SoAd 报告链路 up，DoIP 在 UDP/TCP `13400` 端口监听。
- `-v <级别>` 设置日志详细程度，数值越大日志越多。
- 可用 `netstat -ano | findstr 13400` 验证监听 socket，应能看到 `TCP 0.0.0.0:13400 LISTENING` 条目以及 13400 端口上的 UDP 发现 socket。
- 按 `Ctrl+C` 退出。

## 6. 测试通过 DoIP 烧写

本节给出在主机上端到端验证烧写的完整命令列表，所有命令均在仓库根目录下执行。前提：`python` 已在 `PATH` 中，且已按第 4 节构建出 `Loader.exe` 与 `DoIPBL.exe`。

### 6.1 生成虚拟 Flash 驱动（1052 字节）

与 6.2 节一样使用生成脚本，在地址 0 处生成单段 1052 字节、内容为顺序值（0, 1, 2, ..., 255, 0, 1, ...）的 S19：

```bash
python tools/utils/gensims19.py -n 1 -s 1052 -g 0 -b 0 -o build/FlashDriverDummy.s19
```

### 6.2 生成虚拟应用（A 与 B 分区）

生成脚本保存在 [tools/utils/gensims19.py](../../tools/utils/gensims19.py)，用于生成多段 S19 镜像；A 分区起始地址为 `0x1000`，B 分区为 `0x100000`（DoIPBL 共用 `config/BL/BL.json`，因此内存布局与 CanBL 相同）：

```bash
# A 分区
python tools/utils/gensims19.py -n 8 -s 8192 -g 2048 -b 0x1000 -o build/AppDummy.s19.A

# B 分区
python tools/utils/gensims19.py -n 8 -s 8192 -g 2048 -b 0x100000 -o build/AppDummy.s19.B
```

### 6.3 为 Flash 驱动与应用签名

使用 Loader 对镜像签名（签名区与 BL 的 A/B 分区布局对应）：

```bash
# 为 A 分区应用签名
build\nt\GCC\Loader\Loader.exe -f build/AppDummy.s19.A -s 0xfff00 -S crc32-v3

# 为 B 分区应用签名
build\nt\GCC\Loader\Loader.exe -f build/AppDummy.s19.B -s 0x1fff00 -S crc32-v3

# 为 Flash 驱动签名
build\nt\GCC\Loader\Loader.exe -f build/FlashDriverDummy.s19 -s 2048 -S crc32
```

### 6.4 通过 DoIP 烧写

在另一个终端中运行 Loader（Windows 下请确保 `C:\msys64\mingw64\bin` 与 `build\nt\GCC\one` 在 `PATH` 中，以提供 Loader 运行时 DLL）。启动 DoIPBL 并保持其运行；由于 DoIPBL 主机构建既不会跳转到应用也不会复位，同一个 DoIPBL 实例可连续烧写 A、B 两个分区。

```bash
# 启动 DoIPBL 并保持运行
build\nt\GCC\DoIPBL\DoIPBL.exe -v 3

# 烧写 A 分区
build\nt\GCC\Loader\Loader.exe -d DOIP.224.244.224.245 -c FBL -S crc32 -f build/FlashDriverDummy.s19.sign -a build/AppDummy.s19.A.sign

# 烧写 B 分区（无需重启 DoIPBL）
build\nt\GCC\Loader\Loader.exe -d DOIP.224.244.224.245 -c FBL -S crc32 -f build/FlashDriverDummy.s19.sign -a build/AppDummy.s19.B.sign
```

烧写成功时每一步都会打印 `okay`，最后出现 `progress 100.00%` 以及加载速度统计（主机环回约 4 kbps）。烧写流程依次经过 DoIP 路由激活、扩展会话、关闭通信、安全访问（扩展级与编程级）、擦除、下载、校验、完整性检查、ECU 复位、使能通信与开启 DTC 设置，每一步会打印 `PASS` 或 `FAIL`，出现 `FAIL` 即中止烧写。

说明：

- `-d DOIP.224.244.224.245` 选择 DoIP 传输并指定发现多播地址。
- DoIP 默认端口为 `13400`，诊断仪源地址为 `0xbeef`，ECU 目标地址为 `0xdead`（Loader DoIP 客户端的默认值，与 `Network.json` 一致）。
- 此处不需要 `-l 64`，它仅用于 CAN TP。
- `-c FBL` 选择 FBL 加载器，`-S crc32` 必须与 BL 配置一致，详见 [BL 配置](BL.md)。
- 排错：未带 Flash 驱动（缺少 `-f`）时擦除会以 NRC `0x24`（请求顺序错误）失败，请务必先烧写 Flash 驱动。
