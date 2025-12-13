layout: post
title: AUTOSAR Bootloader (BL) Configuration
category: AUTOSAR
comments: true

# Configuration Notes for AUTOSAR Bootloader (BL) Module

The Bootloader (BL) module manages application loading, validation, and A/B partition switching in AUTOSAR-compliant ECUs. This document outlines key configuration parameters, memory layout rules, and feature flags used during code generation.

## 1. Example Configuration

For a complete example, see:  
[Bootloader BL.json](../../app/bootloader/config/BL/BL.json)

This JSON file defines both general configurations and physical memory layout, which are consumed by the `BL.py` generator to produce `BL_Cfg.h` and `BL_Cfg.c`.

## 2. General Configuration (`general` Section)

The `general` section controls compile-time features and communication settings. All entries map directly to C preprocessor macros in `BL_Cfg.h`.

### General Configuration

| Macro | Type | Required? | Description |
|------|------|-----------|-------------|
| `BL_USE_AB` | `boolean` | Optional | Enable A/B dual-application partitioning. |
| `BL_USE_AB_UPDATE_ACTIVE` | `boolean` | Conditional<br>(`BL_USE_AB`) | Allow updating the currently active partition (use with caution). |
| `BL_USE_META` | `boolean` | Optional | Enable metadata (e.g., version, rolling counter) for application validation. |
| `BL_USE_CRC_16` | `boolean` | Optional | Use CRC16 for integrity checks during flashing or validation. |
| `BL_USE_CRC_32` | `boolean` | Optional | Use CRC32 for integrity checks during flashing or validation. **Note**: `BL_USE_CRC_16` and `BL_USE_CRC_32` cannot be enabled simultaneously, choose one. |
| `BL_USE_AB_ACTIVE_BASED_`<br>`ON_META_ROLLING_COUNTER` | `boolean` | Conditional<br>(`BL_USE_AB` and<br>`BL_USE_META`) | Select the active partition based on the metadata rolling counter (higher value = newer version). |
| `BL_USE_APP_INFO` | `boolean` | Optional | Include basic application information section (e.g., build ID, timestamp). |
| `BL_USE_APP_INFO_V2` | `boolean` | Conditional<br>(`BL_USE_APP_INFO`) | Use extended application info format (v2). |
| `CAN_DIAG_P2P_RX` | `hex string` | Optional | CAN ID for point-to-point diagnostic request (e.g., `"0x731"`). |
| `CAN_DIAG_P2P_TX` | `hex string` | Optional | CAN ID for point-to-point diagnostic response. |
| `CAN_DIAG_P2A_RX` | `hex string` | Optional | CAN ID for functional (point-to-all) diagnostic request. |
| `DCM_DISABLE_PROGRAM_`<br>`SESSION_PROTECTION` | `boolean` | Optional | Bypass security access requirement in programming session (for development only). |
| `BL_USE_BUILTIN_FLS_READ` | `boolean` | Optional | Use built-in flash read implementation instead of external service. |
| `BL_USE_FLS_READ` | `boolean` | Optional | Enable flash read service support (instead of direct memory access). |
| `FINGER_PRINT_SIZE` | `string` | Conditional<br>(`BL_USE_META`) | Size of the fingerprint region (e.g., `"8*1024"`). Evaluated at build time. |
| `META_SIZE` | `string` | Conditional<br>(`BL_USE_META`) | Size of the metadata region (e.g., `"8*1024"`). |
| `APP_SECINON_INFO_SIZE` | `string` | Conditional<br>(`BL_USE_APP_INFO`) | Size of the application section info region (note: typo in name; kept for compatibility). |
| `APP_VALID_FLAG_SIZE` | `string` | Conditional<br>(`BL_USE_META`) | Size of the application validity flag region (e.g., `"8*1024"`). |
| `ROD_OFFSET` | `hex string` | Optional | Offset from application base address where RoD (Run-on-Demand) config is located.<br>default: `0x800`|
| `FL_USE_WRITE_WINDOW_BUFFER` | `boolean` | Optional | Enable buffered flash write window for performance optimization. |
| `FLASH_ERASE_SIZE` | `integer` | Optional | Flash sector erase size in bytes (e.g., `2048`). Used for erase alignment.<br> default: `512` |
| `FLASH_WRITE_SIZE` | `integer` | Optional | Minimum flash write alignment in bytes (e.g., `32`).<br> default: `8` |
| `FLASH_READ_SIZE` | `integer` | Optional | Minimum flash read alignment in bytes (e.g., `4`).<br> default: `1` |
| `FL_ERASE_PER_CYCLE` | `integer` | Optional | Maximum number of flash sectors erased per main function cycle (to limit execution time). Default behavior if omitted: erase one sector per cycle. |
| `FL_WRITE_PER_CYCLE` | `integer` | Optional | Maximum number of flash write operations (of size `FLASH_WRITE_SIZE`) performed per main function cycle. Helps bound flash programming latency.<br> default: `4096/FLASH_WRITE_SIZE`. |
| `FL_READ_PER_CYCLE` | `integer` | Optional | Maximum number of flash read units processed per cycle. <br> default: `4096/FLASH_READ_SIZE`. |
| `FL_ERASE_RCRRP_CYCLE` | `integer` | Optional | Maximum number of erase main function cycles to return DCM_E_FORCE_RCRRP.<br>Default: 0 - diabled. |
| `FL_WRITE_RCRRP_CYCLE` | `integer` | Optional | Maximum number of write main function cycles to return DCM_E_FORCE_RCRRP.<br>Default: 0 - diabled. |
| `FL_WRITE_WINDOW_SIZE` | `integer` | Conditional<br>(`FL_USE_WRITE_`<br>`WINDOW_BUFFER`) | Size of the internal RAM buffer used for flash write aggregation, in bytes. Must be a multiple of `FLASH_WRITE_SIZE`. <br> default: `8 * FLASH_WRITE_SIZE`. |
| `BL_APP_VALID_SAMPLE_SIZE` | `integer` | Optional | Number of bytes read at each sampling point during application integrity validation. Default: aligned to 32 bytes. |
| `BL_APP_VALID_SAMPLE_STRIDE` | `integer` | Optional | Address increment (in bytes) between consecutive sampling points during CRC validation. Default: `1024`. |
| `BL_FLS_READ_SIZE` | `integer` | Optional | Maximum number of bytes read from flash in a single transfer (e.g., during DCM services). Default: `256`. |

> ? All boolean values generate `#define MACRO` when `true`. Numeric and string values are emitted as-is (e.g., `#define FLASH_ERASE_SIZE 2048`).

---

### Notes on `Required?` Values:
- **Required**: Must be present in `BL.json`; the generator assumes a default if missing, but best practice is to define explicitly.
- **Optional**: Can be omitted; sensible defaults are used.
- **Conditional**: Only needed when the referenced feature(s) are enabled.


## 3. Memory Layout (`memory` Section)

The `memory` section defines physical address ranges and attributes for bootloader components. Each flash bank is described with three key properties: `address`, `size`, and `sectorSize`.

### Memory Region Attributes

| Attribute | Type | Description |
|----------|------|-------------|
| `address` | hex string | Start address of the memory region (e.g., `"0xA0040000"`). Must be aligned to flash page/sector boundaries. |
| `size` | string | Size of the region in bytes. May be a decimal number, hex literal (e.g., `"0x10000"`), or arithmetic expression (e.g., `"0xA0300000-0xA0040000"`). The generator evaluates expressions at build time. |
| `sectorSize` | integer | Erase granularity of the flash device in bytes (e.g., `2048`). Used internally for erase/write alignment and validation. |

### Defined Regions

| Region | Required? | Description |
|-------|-----------|-------------|
| `FlashDriver` | Yes | Memory reserved for the flash driver (typically copied to RAM for execution). Contains low-level flash routines. |
| `FlashA` | Yes | Primary application partition. Always used. |
| `FlashB` | Conditional | Secondary application partition. Only used if `BL_USE_AB` is enabled. |
| `Fee` | Optional | Flash EEPROM Emulation (FEE) storage area. |

> ? **Metadata Placement**:  
> When `BL_USE_META` is enabled, metadata structures (fingerprint, meta, info, valid flag, backup) are placed in the **last 2 sectors** (generally) of each application region (`FlashA`/`FlashB`). The usable application space ends at `(region_end - 2 sectors)` to reserve space for these structures.
>
> ? **Sector Alignment**:  
> All flash operations respect `sectorSize`. The generator assumes that `address` and `size` are multiples of `sectorSize`.

## 4. Code Generation

The `BL.py` script reads `BL.json` and generates:
- `GEN/BL_Cfg.h`: Contains all macros from the `general` section.
- `GEN/BL_Cfg.c`: Defines memory layout arrays (`blMemoryListA`, `blMemoryListB`) and global symbol addresses (e.g., `blAppMetaAddrA`).