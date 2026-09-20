---
layout: post
title: AUTOSAR Bootloader（BL）配置说明
category: AUTOSAR
comments: true
---

# AUTOSAR Bootloader（BL）模块配置说明

## 目录

1. [配置示例](#1-配置示例)
2. [通用配置（`general` 段）](#2-通用配置general-段)
   - [通用配置参数](#通用配置参数)
   - [取值要求说明](#取值要求说明)
3. [内存布局（`memory` 段）](#3-内存布局memory-段)
   - [内存区属性](#内存区属性)
   - [已定义的内存区](#已定义的内存区)
4. [代码生成](#4-代码生成)
5. [签名格式（V1/V2/V3）](#5-签名格式v1v2v3)
   - [格式对比](#51-格式对比)
   - [V1 格式（`crc16`、`crc32`）](#52-v1-格式crc16crc32)
   - [V2 格式（`crc16-v2`、`crc32-v2`）](#53-v2-格式crc16-v2crc32-v2)
   - [V3 格式（`crc16-v3`、`crc32-v3`）](#54-v3-格式crc16-v3crc32-v3)
   - [与 BL 的集成](#55-与-bl-的集成)
   - [`BL_USE_APP_INFO` 与 `BL_USE_APP_INFO_V2`](#56-bl_use_app_info-与-bl_use_app_info_v2)
   - [签名类型映射](#57-签名类型映射)
6. [CanBL 的 Windows 仿真测试](#6-canbl-的-windows-仿真测试)
   - [前置条件](#61-前置条件)
   - [编译所需组件](#62-编译所需组件)
   - [生成哑 Flash Driver（1052 字节）](#63-生成哑-flash-driver1052-字节)
   - [生成哑 Application（多段）](#64-生成哑-application多段)
   - [用 Loader 签名](#65-用-loader-签名)
   - [测试应用刷写](#66-测试应用刷写)

Bootloader（BL）模块负责 AUTOSAR ECU 中应用的加载、完整性校验以及 A/B 分区切换。本文介绍代码生成时用到的关键配置参数、内存布局规则和功能开关。

## 1. 配置示例

完整示例见：\
[Bootloader BL.json](../../app/bootloader/config/BL/BL.json)

该 JSON 文件同时定义了通用配置与物理内存布局，由 `BL.py` 生成器读取并生成 `BL_Cfg.h` 与 `BL_Cfg.c`。

## 2. 通用配置（`general` 段）

`general` 段控制编译期功能开关与通信参数，所有条目都会直接映射为 `BL_Cfg.h` 中的 C 预处理宏。

### 通用配置参数

| 宏 | 类型 | 是否必需 | 说明 |
| --- | --- | --- | --- |
| `BL_USE_AB` | `boolean` | 可选 | 启用 A/B 双应用分区。 |
| `BL_USE_AB_UPDATE_ACTIVE` | `boolean` | 条件（`BL_USE_AB`） | 允许更新当前正在运行的活动分区（谨慎使用）。 |
| `BL_USE_META` | `boolean` | 可选 | 启用元数据（如版本号、滚动计数器）用于应用校验。 |
| `BL_USE_CRC_16` | `boolean` | 可选 | 刷写或校验时使用 CRC16 做完整性检查。 |
| `BL_USE_CRC_32` | `boolean` | 可选 | 刷写或校验时使用 CRC32 做完整性检查。**注意**：`BL_USE_CRC_16` 与 `BL_USE_CRC_32` 不能同时启用，二选一。 |
| `BL_USE_AB_ACTIVE_BASED_ON_META_ROLLING_COUNTER` | `boolean` | 条件（`BL_USE_AB` 且 `BL_USE_META`） | 根据元数据滚动计数器选择活动分区（值越大版本越新）。 |
| `BL_USE_APP_INFO` | `boolean` | 可选 | 包含基础应用信息区（如构建 ID、时间戳）。 |
| `BL_USE_APP_INFO_V2` | `boolean` | 条件（`BL_USE_APP_INFO`） | 使用扩展的应用信息格式（v2）。 |
| `CAN_DIAG_P2P_RX` | `hex string` | 可选 | 点对点诊断请求 CAN ID（如 `"0x731"`）。 |
| `CAN_DIAG_P2P_TX` | `hex string` | 可选 | 点对点诊断响应 CAN ID。 |
| `CAN_DIAG_P2A_RX` | `hex string` | 可选 | 功能寻址（点对所有）诊断请求 CAN ID。 |
| `DCM_DISABLE_PROGRAM_SESSION_PROTECTION` | `boolean` | 可选 | 绕过编程会话中的安全访问要求（仅限开发调试）。 |
| `BL_USE_BUILTIN_FLS_READ` | `boolean` | 可选 | 使用内建的 flash 读取实现，而非外部服务。 |
| `BL_USE_FLS_READ` | `boolean` | 可选 | 启用 flash 读取服务支持（替代直接内存访问）。 |
| `FINGER_PRINT_SIZE` | `string` | 条件（`BL_USE_META`） | 指纹区大小（如 `"8*1024"`），构建期求值。 |
| `META_SIZE` | `string` | 条件（`BL_USE_META`） | 元数据区大小（如 `"8*1024"`）。 |
| `APP_SECINON_INFO_SIZE` | `string` | 条件（`BL_USE_APP_INFO`） | 应用段信息区大小（注意：名称中的拼写错误为历史遗留，保持兼容）。 |
| `APP_VALID_FLAG_SIZE` | `string` | 条件（`BL_USE_META`） | 应用有效标志区大小（如 `"8*1024"`）。 |
| `ROD_OFFSET` | `hex string` | 可选 | RoD（Run-on-Demand）配置相对应用基地址的偏移。默认 `0x800`。 |
| `FL_USE_WRITE_WINDOW_BUFFER` | `boolean` | 可选 | 启用带缓冲的 flash 写窗口以优化性能。 |
| `FLASH_ERASE_SIZE` | `integer` | 可选 | Flash 扇区擦除大小（字节，如 `2048`），用于擦除对齐。默认 `512`。 |
| `FLASH_WRITE_SIZE` | `integer` | 可选 | 最小 flash 写对齐（字节，如 `32`）。默认 `8`。 |
| `FLASH_READ_SIZE` | `integer` | 可选 | 最小 flash 读对齐（字节，如 `4`）。默认 `1`。 |
| `FL_ERASE_PER_CYCLE` | `integer` | 可选 | 每个 main function 周期最多擦除的扇区数（用于限制执行时间）。缺省时每周期擦除一个扇区。 |
| `FL_WRITE_PER_CYCLE` | `integer` | 可选 | 每个 main function 周期最多执行的 flash 写操作次数（每次大小为 `FLASH_WRITE_SIZE`），用于约束编程耗时。默认 `4096/FLASH_WRITE_SIZE`。 |
| `FL_READ_PER_CYCLE` | `integer` | 可选 | 每个周期处理的 flash 读单元数上限。默认 `4096/FLASH_READ_SIZE`。 |
| `FL_ERASE_RCRRP_CYCLE` | `integer` | 可选 | 返回 DCM_E_FORCE_RCRRP 的擦除周期数上限。默认 0，表示禁用。 |
| `FL_WRITE_RCRRP_CYCLE` | `integer` | 可选 | 返回 DCM_E_FORCE_RCRRP 的写周期数上限。默认 0，表示禁用。 |
| `FL_WRITE_WINDOW_SIZE` | `integer` | 条件（`FL_USE_WRITE_WINDOW_BUFFER`） | 用于聚合写操作的内部 RAM 缓冲大小（字节），必须是 `FLASH_WRITE_SIZE` 的整数倍。默认 `8 * FLASH_WRITE_SIZE`。 |
| `BL_APP_VALID_SAMPLE_SIZE` | `integer` | 可选 | 应用完整性校验时每个采样点读取的字节数。默认按 32 字节对齐。 |
| `BL_APP_VALID_SAMPLE_STRIDE` | `integer` | 可选 | CRC 校验时相邻采样点之间的地址增量（字节）。默认 `1024`。 |
| `BL_FLS_READ_SIZE` | `integer` | 可选 | 单次传输（如 DCM 服务中）从 flash 读取的最大字节数。默认 `256`。 |

> 所有布尔值在为 `true` 时生成 `#define MACRO`；数值与字符串原样输出（如 `#define FLASH_ERASE_SIZE 2048`）。

***

### 取值要求说明

- **必需（Required）**：应在 BL.json 中给出；即使缺失生成器也会采用默认值，但最佳实践是显式定义。
- **可选（Optional）**：可省略，使用合理的默认值。
- **条件（Conditional）**：仅在所引用的功能开关启用时才需要。

## 3. 内存布局（`memory` 段）

`memory` 段定义 bootloader 各组件的物理地址范围与属性。每个 flash bank 用三个关键属性描述：`address`、`size`、`sectorSize`。

### 内存区属性

| 属性 | 类型 | 说明 |
| --- | --- | --- |
| `address` | hex string | 内存区起始地址（如 `"0xA0040000"`），必须与 flash 页/扇区边界对齐。 |
| `size` | string | 区域大小（字节）。可以是十进制数、十六进制字面量（如 `"0x10000"`）或算术表达式（如 `"0xA0300000-0xA0040000"`），由生成器在构建期求值。 |
| `sectorSize` | integer | Flash 设备的擦除粒度（字节，如 `2048`），内部用于擦/写对齐与校验。 |

### 已定义的内存区

| 区域 | 是否必需 | 说明 |
| --- | --- | --- |
| `FlashDriver` | 是 | 为 Flash 驱动保留的内存（通常拷贝到 RAM 执行），包含底层 flash 操作例程。 |
| `FlashA` | 是 | 主应用分区，始终使用。 |
| `FlashB` | 条件 | 备用应用分区，仅在启用 `BL_USE_AB` 时使用。 |
| `Fee` | 可选 | Flash EEPROM 仿真（FEE）存储区。 |

> **元数据放置**：\
> 启用 `BL_USE_META` 时，元数据结构（fingerprint、meta、info、有效标志、备份）放在每个应用区（`FlashA`/`FlashB`）的**最后 2 个扇区**（通常情况下）。可用应用空间截止到 `(区域结束地址 - 2 个扇区)`，为这些结构预留。
>
> **扇区对齐**：\
> 所有 flash 操作都遵循 `sectorSize`。生成器假定 `address` 与 `size` 都是 `sectorSize` 的整数倍。

## 4. 代码生成

`BL.py` 脚本读取 BL.json 并生成：

- `GEN/BL_Cfg.h`：包含 `general` 段的全部宏；
- `GEN/BL_Cfg.c`：定义内存布局数组（`blMemoryListA`、`blMemoryListB`）以及全局符号地址（如 `blAppMetaAddrA`）。

***

## 5. 签名格式（V1/V2/V3）

Bootloader 支持三种应用完整性签名格式，实现在 `tools/libraries/srec/srec.c`，由 Loader 工具使用。

### 5.1 格式对比

| 格式 | 签名类型 | CRC 类型 | 说明 | 适用场景 |
| --- | --- | --- | --- | --- |
| **V1** | `crc16`、`crc32` | CRC16/CRC32 | 定长填充，CRC 置于末尾 | 旧版、简单校验 |
| **V2** | `crc16-v2`、`crc32-v2` | CRC16/CRC32 | 链式 CRC + 块元数据 | 增强完整性校验 |
| **V3** | `crc16-v3`、`crc32-v3` | CRC16/CRC32 | 链式 CRC + 块列表 + magic | BL 推荐 |

### 5.2 V1 格式（`crc16`、`crc32`）

**算法：**

1. 用 `0xFF` 把二进制填充到固定总长度；
2. 对整个填充区域计算 CRC（不含 CRC 本身所占字节）；
3. 把 CRC 追加到末尾（CRC16 占 2 字节，CRC32 占 4 字节）。

**内存布局：**

```text
[Application Data][0xFF padding][CRC (2/4 bytes)]
^                              ^
startAddr                      startAddr + totalSize - crcLen
```

**Loader 命令：**

```bash
Loader.exe -f app.s19 -s <total_size> -S crc32
```

### 5.3 V2 格式（`crc16-v2`、`crc32-v2`）

**算法：**

1. 对所有 S-record 数据块增量（链式）计算 CRC；
2. 在指定签名地址追加元数据，包含：
   - 块数量（4 字节）
   - CRC 值（2 或 4 字节）
   - 每块信息：地址（4 字节）+ 长度（4 字节）

**内存布局：**

```text
[Application Data]...[Signature Area]
                      ^
                 signAddr
                      [numOfBlks (4B)][CRC (2/4B)][block0_addr+len (8B)][block1_addr+len (8B)]...
```

**Loader 命令：**

```bash
Loader.exe -f app.s19 -s <sign_address> -S crc32-v2
```

### 5.4 V3 格式（`crc16-v3`、`crc32-v3`）

**算法：**

1. 对所有 S-record 数据块增量（链式）计算 CRC；
2. 在最后一个数据块之后追加元数据，包含：
   - 每块信息：地址（4 字节）+ 长度（4 字节）
   - 块数量（4 字节）
   - CRC 值（2 或 4 字节）
   - 用于格式识别的 magic 字符串 `"$BYASV3#"`（8 字节）

**内存布局：**

```text
[Application Data]...[Block List][Footer]
                                   ^
                              signAddr (end of data)
                                   [block0_addr+len (8B)][block1_addr+len (8B)]...[numOfBlks (4B)][CRC (2/4B)][$BYASV3# (8B)]
```

**Loader 命令：**

```bash
Loader.exe -f app.s19 -s <sign_address> -S crc32-v3
```

### 5.5 与 BL 的集成

Bootloader 的 `bl_core.c` 通过以下函数实现签名校验：

| 函数 | 用途 |
| --- | --- |
| `BL_CheckAppIntegrity()` | 启动时校验应用完整性 |
| `BL_CheckIntegrity()` | 通过 UDS 请求触发应用校验 |
| `getAppNSampledCrc()` | 计算采样 CRC 以实现快速校验 |

**V3 是推荐格式**，因为它：

- 支持多内存块（非连续区域）；
- 带 magic 字符串用于格式检测；
- 采用链式 CRC，完整性更好；
- 可启用 `BL_USE_APP_INFO_V2` 功能。

### 5.6 BL_USE_APP_INFO 与 BL_USE_APP_INFO_V2

Bootloader 提供两个配置开关，决定应用元数据的处理方式。

#### BL_USE_APP_INFO（支持 V2 签名格式）

启用 `BL_USE_APP_INFO` 后：

- 使用应用信息区（`blAppInfoAddr`）存放段元数据；
- 元数据格式：`[numOfSections (4B)][CRC (4B)][section0_addr+len (8B)][section1_addr+len (8B)]...`
- 对应 **V2 签名格式**（`crc32-v2`）；
- `getAppNSampledCrc()` 读取段信息并增量校验 CRC。

#### BL_USE_APP_INFO_V2（支持 V3 签名格式）

启用 `BL_USE_APP_INFO_V2` 后：

- **必须先启用 `BL_USE_APP_INFO`**；
- 检测 V3 签名格式并转换为与 V1 兼容的格式；
- 通过 `BL_CopyAppInfoV2ForV1()` 完成：
  1. 根据 magic 字符串 `"$BYASV3#"` 定位 V3 尾部；
  2. 把块列表与尾部拷贝到应用信息地址；
  3. 从而与 V2 校验逻辑向后兼容；
- 对应 **V3 签名格式**（`crc32-v3`）。

#### 关键差异

| 特性 | `BL_USE_APP_INFO` | `BL_USE_APP_INFO_V2` |
| --- | --- | --- |
| 签名格式 | V2（`crc32-v2`） | V3（`crc32-v3`） |
| Magic 字符串 | 无 | `"$BYASV3#"` |
| 元数据位置 | 固定的应用信息地址 | 应用数据末尾 |
| 是否需要转换 | 否 | 是（V3 转 V1 格式） |
| 块列表顺序 | 头部 + CRC + 各块 | 各块 + 头部/CRC + Magic |

#### 格式转换（`BL_CopyAppInfoV2ForV1`）

`BL_CopyAppInfoV2ForV1()` 的处理过程：

```text
V3 格式（应用末尾）:
[Application Data]...[Block List][numOfBlks (4B)][CRC (4B)][$BYASV3# (8B)]

      <--> BL_CopyAppInfoV2ForV1()

V1 格式（blAppInfoAddr 处）:
[numOfBlks (4B)][CRC (4B)][Block List]
```

该转换使 bootloader 可以用同一套校验逻辑同时支持 V2 和 V3 格式。

#### 推荐配置

| 适用场景 | 推荐开关 | 签名格式 |
| --- | --- | --- |
| 简单单块应用 | 无 | `crc32`（V1） |
| 多块应用 | `BL_USE_APP_INFO` | `crc32-v2`（V2） |
| 高级多块 + 格式检测 | `BL_USE_APP_INFO` + `BL_USE_APP_INFO_V2` | `crc32-v3`（V3） |

### 5.7 签名类型映射

Loader 工具使用 `-S` 参数选择签名算法：

| 参数 | 签名类型 | CRC 初始值 |
| --- | --- | --- |
| `crc16` | SREC_SIGN_CRC16 | 0xFFFF |
| `crc32` | SREC_SIGN_CRC32 | 0xFFFFFFFF |
| `crc16-v2` | SREC_SIGN_CRC16_V2 | 0xFFFF |
| `crc32-v2` | SREC_SIGN_CRC32_V2 | 0xFFFFFFFF |
| `crc16-v3` | SREC_SIGN_CRC16_V3 | 0xFFFF |
| `crc32-v3` | SREC_SIGN_CRC32_V3 | 0xFFFFFFFF |

> **注意：** V2 与 V3 使用**链式 CRC 计算**（把上一段 CRC 作为下一段初始值），而 V1 是对整个填充区域做一次 CRC 计算。

***

## 6. CanBL 的 Windows 仿真测试

在 Windows 上通过仿真测试 CanBL 时，需要先生成哑的 flash driver 与应用文件，用 Loader 签名后再测试。

### 6.1 前置条件

确保以下工具在 PATH 中可用：

- `python`（用于运行生成脚本 `tools/utils/gensims19.py`）
- `Loader.exe`（通过 `--app=Loader` 编译）

### 6.2 编译所需组件

首先编译 loader 及相关库：

```bash
scons --lib=AsOne
scons --lib=LoaderFBL
scons --app=IsoTpSend
scons --app=Loader
scons --app=CanBL
scons --app=CanApp
```

### 6.3 生成哑 Flash Driver（1052 字节）

使用生成脚本 [tools/utils/gensims19.py](../../tools/utils/gensims19.py) 生成一个哑 flash driver 文件，内容为顺序值（0, 1, 2, ..., 255, 0, 1, ...），地址为 0，输出到 CanBL 构建目录：

```bash
python tools/utils/gensims19.py -n 1 -s 1052 -g 0 -b 0 -o build/FlashDriverDummy.s19
```

### 6.4 生成哑 Application（多段）

生成脚本 [tools/utils/gensims19.py](../../tools/utils/gensims19.py) 用于生成多段 S19 文件。

不带参数运行（使用默认值）：

```bash
python tools/utils/gensims19.py
```

分别为 A、B 分区生成文件：

```bash
# 分区 A
python tools/utils/gensims19.py -n 8 -s 8192 -g 2048 -b 0x1000 -o build/AppDummy.s19.A

# 分区 B
python tools/utils/gensims19.py -n 8 -s 8192 -g 2048 -b 0x100000 -o build/AppDummy.s19.B
```

### 6.5 用 Loader 签名

使用 Loader 工具对应用签名：

```bash
# 分区 A
build\nt\GCC\Loader\Loader.exe -f build/AppDummy.s19.A -s 0xfff00 -S crc32-v3

# 分区 B
build\nt\GCC\Loader\Loader.exe -f build/AppDummy.s19.B -s 0x1fff00 -S crc32-v3

# FlashDriver
build\nt\GCC\Loader\Loader.exe -f build/FlashDriverDummy.s19 -s 2048 -S crc32
```

### 6.6 测试应用刷写

用 Loader 把签名后的应用刷入 CanBL：

```bash
# 启动 CAN bootloader
build\nt\GCC\CanBL\CanBL.exe

build\nt\GCC\Loader\Loader.exe -l 64 -c FBL -S crc32 -f build/FlashDriverDummy.s19.sign -a build/AppDummy.s19.A.sign

build\nt\GCC\Loader\Loader.exe -l 64 -c FBL -S crc32 -f build/FlashDriverDummy.s19.sign -a build/AppDummy.s19.B.sign
```
