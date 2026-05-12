layout: post
title: AUTOSAR Bootloader (BL) Configuration
category: AUTOSAR
comments: true

# Configuration Notes for AUTOSAR Bootloader (BL) Module

## Table of Contents

1. [Example Configuration](#1-example-configuration)
2. [General Configuration (`general` Section)](#2-general-configuration-general-section)
   - [General Configuration Parameters](#general-configuration)
   - [Notes on Required Values](#notes-on-required-values)
3. [Memory Layout (`memory` Section)](#3-memory-layout-memory-section)
   - [Memory Region Attributes](#memory-region-attributes)
   - [Defined Regions](#defined-regions)
4. [Code Generation](#4-code-generation)
5. [Signature Formats (V1/V2/V3)](#5-signature-formats-v1v2v3)
   - [Format Comparison](#51-format-comparison)
   - [V1 Format (`crc16`, `crc32`)](#52-v1-format-crc16-crc32)
   - [V2 Format (`crc16-v2`, `crc32-v2`)](#53-v2-format-crc16-v2-crc32-v2)
   - [V3 Format (`crc16-v3`, `crc32-v3`)](#54-v3-format-crc16-v3-crc32-v3)
   - [BL Integration](#55-bl-integration)
   - [`BL_USE_APP_INFO` vs `BL_USE_APP_INFO_V2`](#56-bl_use_app_info-vs-bl_use_app_info_v2)
   - [Signature Type Mapping](#57-signature-type-mapping)
6. [Windows Simulation Testing for CanBL](#6-windows-simulation-testing-for-canbl)
   - [Prerequisites](#61-prerequisites)
   - [Build Required Components](#62-build-required-components)
   - [Generate Dummy Flash Driver](#63-generate-dummy-flash-driver-1052-bytes)
   - [Generate Dummy Application](#64-generate-dummy-application-32-kb)
   - [Sign Application with Loader](#65-sign-application-with-loader)
   - [Test Application Programming](#66-test-application-programming)

The Bootloader (BL) module manages application loading, validation, and A/B partition switching in AUTOSAR-compliant ECUs. This document outlines key configuration parameters, memory layout rules, and feature flags used during code generation.

## 1. Example Configuration

For a complete example, see:\
[Bootloader BL.json](../../app/bootloader/config/BL/BL.json)

This JSON file defines both general configurations and physical memory layout, which are consumed by the `BL.py` generator to produce `BL_Cfg.h` and `BL_Cfg.c`.

## 2. General Configuration (`general` Section)

The `general` section controls compile-time features and communication settings. All entries map directly to C preprocessor macros in `BL_Cfg.h`.

### General Configuration

| Macro                                            | Type         | Required?                                 | Description                                                                                                                                                                       |
| ------------------------------------------------ | ------------ | ----------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `BL_USE_AB`                                      | `boolean`    | Optional                                  | Enable A/B dual-application partitioning.                                                                                                                                         |
| `BL_USE_AB_UPDATE_ACTIVE`                        | `boolean`    | Conditional(`BL_USE_AB`)                  | Allow updating the currently active partition (use with caution).                                                                                                                 |
| `BL_USE_META`                                    | `boolean`    | Optional                                  | Enable metadata (e.g., version, rolling counter) for application validation.                                                                                                      |
| `BL_USE_CRC_16`                                  | `boolean`    | Optional                                  | Use CRC16 for integrity checks during flashing or validation.                                                                                                                     |
| `BL_USE_CRC_32`                                  | `boolean`    | Optional                                  | Use CRC32 for integrity checks during flashing or validation. **Note**: `BL_USE_CRC_16` and `BL_USE_CRC_32` cannot be enabled simultaneously, choose one.                         |
| `BL_USE_AB_ACTIVE_BASED_ON_META_ROLLING_COUNTER` | `boolean`    | Conditional(`BL_USE_AB` and`BL_USE_META`) | Select the active partition based on the metadata rolling counter (higher value = newer version).                                                                                 |
| `BL_USE_APP_INFO`                                | `boolean`    | Optional                                  | Include basic application information section (e.g., build ID, timestamp).                                                                                                        |
| `BL_USE_APP_INFO_V2`                             | `boolean`    | Conditional(`BL_USE_APP_INFO`)            | Use extended application info format (v2).                                                                                                                                        |
| `CAN_DIAG_P2P_RX`                                | `hex string` | Optional                                  | CAN ID for point-to-point diagnostic request (e.g., `"0x731"`).                                                                                                                   |
| `CAN_DIAG_P2P_TX`                                | `hex string` | Optional                                  | CAN ID for point-to-point diagnostic response.                                                                                                                                    |
| `CAN_DIAG_P2A_RX`                                | `hex string` | Optional                                  | CAN ID for functional (point-to-all) diagnostic request.                                                                                                                          |
| `DCM_DISABLE_PROGRAM_SESSION_PROTECTION`         | `boolean`    | Optional                                  | Bypass security access requirement in programming session (for development only).                                                                                                 |
| `BL_USE_BUILTIN_FLS_READ`                        | `boolean`    | Optional                                  | Use built-in flash read implementation instead of external service.                                                                                                               |
| `BL_USE_FLS_READ`                                | `boolean`    | Optional                                  | Enable flash read service support (instead of direct memory access).                                                                                                              |
| `FINGER_PRINT_SIZE`                              | `string`     | Conditional(`BL_USE_META`)                | Size of the fingerprint region (e.g., `"8*1024"`). Evaluated at build time.                                                                                                       |
| `META_SIZE`                                      | `string`     | Conditional(`BL_USE_META`)                | Size of the metadata region (e.g., `"8*1024"`).                                                                                                                                   |
| `APP_SECINON_INFO_SIZE`                          | `string`     | Conditional(`BL_USE_APP_INFO`)            | Size of the application section info region (note: typo in name; kept for compatibility).                                                                                         |
| `APP_VALID_FLAG_SIZE`                            | `string`     | Conditional(`BL_USE_META`)                | Size of the application validity flag region (e.g., `"8*1024"`).                                                                                                                  |
| `ROD_OFFSET`                                     | `hex string` | Optional                                  | Offset from application base address where RoD (Run-on-Demand) config is located.default: `0x800`                                                                                 |
| `FL_USE_WRITE_WINDOW_BUFFER`                     | `boolean`    | Optional                                  | Enable buffered flash write window for performance optimization.                                                                                                                  |
| `FLASH_ERASE_SIZE`                               | `integer`    | Optional                                  | Flash sector erase size in bytes (e.g., `2048`). Used for erase alignment. default: `512`                                                                                         |
| `FLASH_WRITE_SIZE`                               | `integer`    | Optional                                  | Minimum flash write alignment in bytes (e.g., `32`). default: `8`                                                                                                                 |
| `FLASH_READ_SIZE`                                | `integer`    | Optional                                  | Minimum flash read alignment in bytes (e.g., `4`). default: `1`                                                                                                                   |
| `FL_ERASE_PER_CYCLE`                             | `integer`    | Optional                                  | Maximum number of flash sectors erased per main function cycle (to limit execution time). Default behavior if omitted: erase one sector per cycle.                                |
| `FL_WRITE_PER_CYCLE`                             | `integer`    | Optional                                  | Maximum number of flash write operations (of size `FLASH_WRITE_SIZE`) performed per main function cycle. Helps bound flash programming latency. default: `4096/FLASH_WRITE_SIZE`. |
| `FL_READ_PER_CYCLE`                              | `integer`    | Optional                                  | Maximum number of flash read units processed per cycle.  default: `4096/FLASH_READ_SIZE`.                                                                                         |
| `FL_ERASE_RCRRP_CYCLE`                           | `integer`    | Optional                                  | Maximum number of erase main function cycles to return DCM\_E\_FORCE\_RCRRP.Default: 0 - diabled.                                                                                 |
| `FL_WRITE_RCRRP_CYCLE`                           | `integer`    | Optional                                  | Maximum number of write main function cycles to return DCM\_E\_FORCE\_RCRRP.Default: 0 - diabled.                                                                                 |
| `FL_WRITE_WINDOW_SIZE`                           | `integer`    | Conditional(`FL_USE_WRITE_WINDOW_BUFFER`) | Size of the internal RAM buffer used for flash write aggregation, in bytes. Must be a multiple of `FLASH_WRITE_SIZE`.  default: `8 * FLASH_WRITE_SIZE`.                           |
| `BL_APP_VALID_SAMPLE_SIZE`                       | `integer`    | Optional                                  | Number of bytes read at each sampling point during application integrity validation. Default: aligned to 32 bytes.                                                                |
| `BL_APP_VALID_SAMPLE_STRIDE`                     | `integer`    | Optional                                  | Address increment (in bytes) between consecutive sampling points during CRC validation. Default: `1024`.                                                                          |
| `BL_FLS_READ_SIZE`                               | `integer`    | Optional                                  | Maximum number of bytes read from flash in a single transfer (e.g., during DCM services). Default: `256`.                                                                         |

> ? All boolean values generate `#define MACRO` when `true`. Numeric and string values are emitted as-is (e.g., `#define FLASH_ERASE_SIZE 2048`).

***

### Notes on `Required?` Values:

- **Required**: Must be present in `BL.json`; the generator assumes a default if missing, but best practice is to define explicitly.
- **Optional**: Can be omitted; sensible defaults are used.
- **Conditional**: Only needed when the referenced feature(s) are enabled.

## 3. Memory Layout (`memory` Section)

The `memory` section defines physical address ranges and attributes for bootloader components. Each flash bank is described with three key properties: `address`, `size`, and `sectorSize`.

### Memory Region Attributes

| Attribute    | Type       | Description                                                                                                                                                                                           |
| ------------ | ---------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `address`    | hex string | Start address of the memory region (e.g., `"0xA0040000"`). Must be aligned to flash page/sector boundaries.                                                                                           |
| `size`       | string     | Size of the region in bytes. May be a decimal number, hex literal (e.g., `"0x10000"`), or arithmetic expression (e.g., `"0xA0300000-0xA0040000"`). The generator evaluates expressions at build time. |
| `sectorSize` | integer    | Erase granularity of the flash device in bytes (e.g., `2048`). Used internally for erase/write alignment and validation.                                                                              |

### Defined Regions

| Region        | Required?   | Description                                                                                                      |
| ------------- | ----------- | ---------------------------------------------------------------------------------------------------------------- |
| `FlashDriver` | Yes         | Memory reserved for the flash driver (typically copied to RAM for execution). Contains low-level flash routines. |
| `FlashA`      | Yes         | Primary application partition. Always used.                                                                      |
| `FlashB`      | Conditional | Secondary application partition. Only used if `BL_USE_AB` is enabled.                                            |
| `Fee`         | Optional    | Flash EEPROM Emulation (FEE) storage area.                                                                       |

> ? **Metadata Placement**:\
> When `BL_USE_META` is enabled, metadata structures (fingerprint, meta, info, valid flag, backup) are placed in the **last 2 sectors** (generally) of each application region (`FlashA`/`FlashB`). The usable application space ends at `(region_end - 2 sectors)` to reserve space for these structures.
>
> ? **Sector Alignment**:\
> All flash operations respect `sectorSize`. The generator assumes that `address` and `size` are multiples of `sectorSize`.

## 4. Code Generation

The `BL.py` script reads `BL.json` and generates:

- `GEN/BL_Cfg.h`: Contains all macros from the `general` section.
- `GEN/BL_Cfg.c`: Defines memory layout arrays (`blMemoryListA`, `blMemoryListB`) and global symbol addresses (e.g., `blAppMetaAddrA`).

***

## 5. Signature Formats (V1/V2/V3)

The bootloader supports three signature formats for application integrity verification. These formats are implemented in `tools/libraries/srec/srec.c` and used by the Loader tool.

### 5.1 Format Comparison

| Format | Sign Type | CRC Type | Description | Use Case |
|--------|-----------|----------|-------------|----------|
| **V1** | `crc16`, `crc32` | CRC16/CRC32 | Fixed-size padding with CRC at end | Legacy, simple validation |
| **V2** | `crc16-v2`, `crc32-v2` | CRC16/CRC32 | Chained CRC with block metadata | Enhanced integrity |
| **V3** | `crc16-v3`, `crc32-v3` | CRC16/CRC32 | Chained CRC with block list + magic | Recommended for BL |

### 5.2 V1 Format (`crc16`, `crc32`)

**Algorithm:**
1. Pad the binary to a fixed total size with `0xFF`
2. Calculate CRC over the entire padded area (excluding CRC bytes)
3. Append CRC value at the end (last 2 bytes for CRC16, 4 bytes for CRC32)

**Memory Layout:**
```
[Application Data][0xFF padding][CRC (2/4 bytes)]
^                              ^
startAddr                      startAddr + totalSize - crcLen
```

**Loader Command:**
```bash
Loader.exe -f app.s19 -s <total_size> -S crc32
```

### 5.3 V2 Format (`crc16-v2`, `crc32-v2`)

**Algorithm:**
1. Calculate CRC incrementally over all S-record blocks (chained)
2. Append metadata at the specified signature address containing:
   - Number of blocks (4 bytes)
   - CRC value (2 or 4 bytes)
   - Per-block info: address (4 bytes) + length (4 bytes) for each block

**Memory Layout:**
```
[Application Data]...[Signature Area]
                      ^
                 signAddr
                      [numOfBlks (4B)][CRC (2/4B)][block0_addr+len (8B)][block1_addr+len (8B)]...
```

**Loader Command:**
```bash
Loader.exe -f app.s19 -s <sign_address> -S crc32-v2
```

### 5.4 V3 Format (`crc16-v3`, `crc32-v3`)

**Algorithm:**
1. Calculate CRC incrementally over all S-record blocks (chained)
2. Append metadata after the last data block containing:
   - Per-block info: address (4 bytes) + length (4 bytes) for each block
   - Number of blocks (4 bytes)
   - CRC value (2 or 4 bytes)
   - Magic string `"$BYASV3#"` (8 bytes) for format identification

**Memory Layout:**
```
[Application Data]...[Block List][Footer]
                                   ^
                              signAddr (end of data)
                                   [block0_addr+len (8B)][block1_addr+len (8B)]...[numOfBlks (4B)][CRC (2/4B)][$BYASV3# (8B)]
```

**Loader Command:**
```bash
Loader.exe -f app.s19 -s <sign_address> -S crc32-v3
```

### 5.5 BL Integration

The bootloader's `bl_core.c` implements signature verification through the following functions:

| Function | Purpose |
|----------|---------|
| `BL_CheckAppIntegrity()` | Validates application integrity on boot |
| `BL_CheckIntegrity()` | Validates application via UDS request |
| `getAppNSampledCrc()` | Computes sampled CRC for fast validation |

**V3 is the recommended format** for the bootloader as it:
- Supports multiple memory blocks (discontinuous regions)
- Includes a magic string for format detection
- Uses chained CRC calculation for better integrity
- Enables the `BL_USE_APP_INFO_V2` feature

### 5.7 BL_USE_APP_INFO vs BL_USE_APP_INFO_V2

The bootloader supports two configuration flags that determine how application metadata is handled:

#### BL_USE_APP_INFO (V2 Signature Format Support)

When `BL_USE_APP_INFO` is enabled:

- Uses the application info section (`blAppInfoAddr`) to store section metadata
- Metadata format: `[numOfSections (4B)][CRC (4B)][section0_addr+len (8B)][section1_addr+len (8B)]...`
- Corresponds to the **V2 signature format** (`crc32-v2`)
- The `getAppNSampledCrc()` function reads section info and validates CRC incrementally

#### BL_USE_APP_INFO_V2 (V3 Signature Format Support)

When `BL_USE_APP_INFO_V2` is enabled:

- **Requires `BL_USE_APP_INFO`** to be enabled
- Detects and converts V3 signature format to V1-compatible format
- Uses the `BL_CopyAppInfoV2ForV1()` function to:
  1. Locate the V3 footer with magic string `"$BYASV3#"`
  2. Copy the block list and footer to the app info address
  3. Enable backward compatibility with V2 validation logic
- Corresponds to the **V3 signature format** (`crc32-v3`)

#### Key Differences

| Feature | `BL_USE_APP_INFO` | `BL_USE_APP_INFO_V2` |
|---------|-------------------|----------------------|
| Signature Format | V2 (`crc32-v2`) | V3 (`crc32-v3`) |
| Magic String | None | `"$BYASV3#"` |
| Metadata Location | Fixed app info address | End of application data |
| Conversion Required | No | Yes (V3 to V1 format) |
| Block List Order | Header then CRC then Blocks | Blocks then Header/CRC then Magic |

#### Format Conversion (`BL_CopyAppInfoV2ForV1`)

The `BL_CopyAppInfoV2ForV1()` function performs the following steps:

```
V3 Format (end of application):
[Application Data]...[Block List][numOfBlks (4B)][CRC (4B)][$BYASV3# (8B)]

      <--> BL_CopyAppInfoV2ForV1()

V1 Format (at blAppInfoAddr):
[numOfBlks (4B)][CRC (4B)][Block List]
```

This conversion allows the bootloader to use the same validation logic for both V2 and V3 formats.

#### Recommended Configuration

| Use Case | Recommended Flags | Signature Format |
|----------|-------------------|------------------|
| Simple single-block apps | None | `crc32` (V1) |
| Multi-block apps | `BL_USE_APP_INFO` | `crc32-v2` (V2) |
| Advanced multi-block + format detection | `BL_USE_APP_INFO` + `BL_USE_APP_INFO_V2` | `crc32-v3` (V3) |

### 5.7 Signature Type Mapping

The Loader tool uses the `-S` parameter to select the signature algorithm:

| Parameter | Sign Type | CRC Initial Value |
|-----------|-----------|-------------------|
| `crc16` | SREC_SIGN_CRC16 | 0xFFFF |
| `crc32` | SREC_SIGN_CRC32 | 0xFFFFFFFF |
| `crc16-v2` | SREC_SIGN_CRC16_V2 | 0xFFFF |
| `crc32-v2` | SREC_SIGN_CRC32_V2 | 0xFFFFFFFF |
| `crc16-v3` | SREC_SIGN_CRC16_V3 | 0xFFFF |
| `crc32-v3` | SREC_SIGN_CRC32_V3 | 0xFFFFFFFF |

> **Note:** V2 and V3 use **chained CRC calculation** (passing previous CRC as initial value), while V1 uses a single CRC calculation over the entire padded region.

***

## 6. Windows Simulation Testing for CanBL

When testing CanBL on Windows using simulation, you need to generate dummy flash driver and application files, then sign them with the Loader tool before testing.

### 6.1 Prerequisites

Ensure the following tools are available in your PATH:
- `objcopy` (from MSYS2 binutils package)
- `Loader.exe` (built from `--app=Loader`)

### 6.2 Build Required Components

First, build the loader and related libraries:

```bash
scons --lib=AsOne
scons --lib=LoaderFBL
scons --app=IsoTpSend
scons --app=Loader
scons --app=CanBL
scons --app=CanApp
```

### 6.3 Generate Dummy Flash Driver (1052 bytes)

Use MSYS `objcopy` to create a dummy flash driver file with sequential values (0, 1, 2, ..., 255, 0, 1, ...) at address 0, placed in the CanBL build folder:

```bash
# Create a file with 1052 bytes of sequential values (0, 1, 2, ..., 255, 0, 1, ...) using Python
python -c "with open('build/FlashDriverDummy.bin', 'wb') as f: f.write(bytes([i % 256 for i in range(1052)]))"

# Convert to S19 format using MSYS objcopy with address at 0
objcopy -I binary -O srec --adjust-vma 0 build/FlashDriverDummy.bin build/FlashDriverDummy.s19

```

### 6.4 Generate Dummy Application (32 KB)

Create a dummy 32 KB application file using MSYS tools:

```bash
# Create a file with 32768 bytes (32KB) of sequential values (0, 1, 2, ..., 255, 0, 1, ...) using Python
python -c "with open('build/AppDummy.bin', 'wb') as f: f.write(bytes([i % 256 for i in range(32768)]))"

# Convert to S19 format using MSYS objcopy
objcopy -I binary -O srec --adjust-vma 0x1000 build/AppDummy.bin build/AppDummy.s19.A

objcopy -I binary -O srec --adjust-vma 0x100000 build/AppDummy.bin build/AppDummy.s19.B

```

### 6.5 Sign Application with Loader

Sign the application using the Loader tool:

```bash
# Sign for partition A
build\nt\GCC\Loader\Loader.exe -f build/AppDummy.s19.A -s 0xfff00 -S crc32-v3

# Sign for partition B
build\nt\GCC\Loader\Loader.exe -f build/AppDummy.s19.B -s 0x1fff00 -S crc32-v3

# Sign for FlashDriver
build\nt\GCC\Loader\Loader.exe -f build/FlashDriverDummy.s19 -s 2048 -S crc32

```

### 6.6 Test Application Programming

Use the Loader tool to program the signed application to CanBL:

```bash
# start the CAN bootloader
build\nt\GCC\CanBL\CanBL.exe

build\nt\GCC\Loader\Loader.exe -l 64 -c FBL -S crc32 -f build/FlashDriverDummy.s19.sign -a build/AppDummy.s19.A.sign

build\nt\GCC\Loader\Loader.exe -l 64 -c FBL -S crc32 -f build/FlashDriverDummy.s19.sign -a build/AppDummy.s19.B.sign
```
