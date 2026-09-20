# AS - Automotive Software

**AS** is an open-source implementation of automotive software based on **AUTOSAR 4.4**, covering BSW (Basic Software) modules, PC tools, and supporting libraries, developed independently by one author. It is free for evaluation and study purposes; commercial use requires a commercial license (see [License](#license)).

## Architecture

![architecture](doc/images/architecture.png)

## Supported Platforms

MCU/ECU platforms currently supported by AS:

| MCU (Vendor / Core) | Bootloader | Application |
| --- | :-: | :--- |
| AutoChips AC7840x (Cortex-M4F) | CAN | Com Stack, Diagnostic Stack, Memory Stack |
| TI AWR294x radar SoC (R4F) | CAN | Com Stack, Diagnostic Stack, Memory Stack |
| TI CC27xx (Cortex-M33, BLE) | CAN | Com Stack, Diagnostic Stack |
| ChipON KF32A136 (KungFu32) | CAN | Com Stack, Diagnostic Stack, Memory Stack |
| Microchip SAMD21/SAMDA1 (Cortex-M0+) | LIN | Diagnostic Stack, Memory Stack |
| NXP MC9S12XEP100 (S12X) | CAN+LIN | Com Stack, Diagnostic Stack, Memory Stack |
| NXP MPC5634M (PowerPC e200z3) | CAN | Com Stack, Diagnostic Stack |
| QEMU virtual: virt / versatilepb / x86 / STM32-P107 | CAN | Com Stack, Diagnostic Stack, Memory Stack |
| Renesas RH850 | CAN | Com Stack, Diagnostic Stack, Memory Stack |
| NXP S32K1xx (Cortex-M4F) | CAN | Com Stack, Diagnostic Stack, Memory Stack |
| Infineon AURIX TC3x7 (TriCore) | CAN, DoIP (in app) | Com Stack, Diagnostic Stack, DoIP, SOME/IP-SD |
| ST STM32F107VC (Cortex-M3, USB2CAN device) | CAN | Com Stack, Diagnostic Stack |
| YTM32B1xx (Cortex-M4F) | CAN | Com Stack, Diagnostic Stack, Memory Stack |

## Tools & Libraries

### Bus Simulation & Hardware Access

- **CanBusSimulator**: CAN bus simulator over IP socket.
- **LinBusSimulator**: LIN bus simulator over IP socket.
- **CanLib**: CAN library to access CAN hardware (Vector CanCaseXL, PeakCan, ZLGCAN, etc.).
- **LinLib**: LIN library to access LIN hardware (COM, USB2I2C, USB2SPI, etc.).
- **DevLib**: abstract device libraries to access any other kind of automotive devices.

### Protocol & Middleware

- **IsoTp**: LIN or CAN (CAN FD) transport layer (ISO 15765).
- **Loader**: library used for bootloader.
- **DoIPClient**: DoIP client library to access a DoIP server.
- [**VDDS**](infras/libraries/dds/vdds/): [Virtio ring buffer & shared memory based DDS](doc/EN/VirtioDDS.md).

### PC Tools

- **AsPy**: Python interface providing APIs for CAN, LIN and IsoTp, easy to be used to implement test cases.
- **asone**: Qt based GUI tool for Com/Dcm/FlashLoader, provided in both a Python version and a pure C++ version (the C++ version is recommended). It embeds a Lua engine for UICom and UIDcm, so Lua scripts can be used to control Com/UDS communication.
- **JSON Editor**: JSON schema & PyQt5 based configuration GUI tool for AS.

![JSON Editor](doc/images/json-editor-ssas.gif)

## Successful Solutions for Customers

- CAN/LIN protocol based flashloader/bootloader for MCU.
- CAN/LIN based UDS/COM stack for MCU.
- **asone UICom**: PC tool for CAN/LIN based signal communication, scriptable with Lua.
- **asone UIDcm**: PC tool for CAN/LIN based UDS; Lua scripts process request/response data and display it.
- **asone UIFBL**: PC tool for CAN/LIN based flashloader/bootloader.

## Documents

| Topic | CN | EN |
| --- | --- | --- |
| Background | [CN](doc/CN/background.md) | [EN](doc/EN/background.md) |
| Build environment setup | [CN](doc/CN/build-env-setup.md) | [EN](doc/EN/build-env-setup.md) |
| Virtual CAN environment | [CN](doc/CN/virtual-can-env.md) | [EN](doc/EN/virtual-can-env.md) |
| Virtual LIN environment | [CN](doc/CN/virtual-lin-env.md) | [EN](doc/EN/virtual-lin-env.md) |
| CAN bootloader | [CN: boot over QEMU](doc/CN/can-bootloader.md)<br>[CN: BL config & host sim](doc/CN/BL.md) | [EN: boot over QEMU](doc/EN/can-bootloader.md)<br>[EN: BL config & host sim](doc/EN/BL.md) |
| BL over DoIP demo | [CN](doc/CN/BL-DoIP.md) | [EN](doc/EN/BL-DoIP.md) |
| CAN OSEK NM | [CN](doc/CN/can-oseknm.md) | [EN](doc/EN/can-oseknm.md) |
| NvM | [CN](doc/CN/nvm.md) | [EN](doc/EN/NvM.md) |
| CAN Interface (CanIf) | [CN](doc/CN/CanIf.md) | [EN](doc/EN/CanIf.md) |
| CAN Transport Layer (CanTp) | [CN](doc/CN/CanTp.md) | [EN](doc/EN/CanTp.md) |
| LIN Interface (LinIf) | [CN](doc/CN/LinIf.md) | [EN](doc/EN/LinIf.md) |
| Communication (Com) | [CN](doc/CN/Com.md) | [EN](doc/EN/Com.md) |
| PDU Router (PduR) | [CN](doc/CN/PduR.md) | [EN](doc/EN/PduR.md) |
| Diagnostic Communication Manager (Dcm) | [CN](doc/CN/Dcm.md) | [EN](doc/EN/Dcm.md) |
| Diagnostic Event Manager (Dem) | [CN](doc/CN/Dem.md) | [EN](doc/EN/Dem.md) |
| Bus Mirroring (Mirror) | [CN](doc/CN/Mirror.md) | [EN](doc/EN/Mirror.md) |
| SOME/IP-SD | [CN](doc/CN/SOMEIP-SD.md) | [EN](doc/EN/SOMEIP-SD.md) |
| DoIP | [CN](doc/CN/DoIP.md) | [EN](doc/EN/DoIP.md) |
| TLS (mbedTLS) | [CN](doc/CN/TLS.md) | [EN](doc/EN/TLS.md) |
| VDDS | [CN](doc/CN/VirtioDDS.md) | [EN](doc/EN/VirtioDDS.md) |

### Tools & Guides

| Topic | CN | EN |
| --- | --- | --- |
| JSON Editor | [CN](doc/CN/JsonEditor.md) | [EN](doc/EN/JsonEditor.md) |
| How to create CA (X.509) | [CN](doc/CN/HowToCreateCA.md) | [EN](doc/EN/HowToCreateCA.md) |
| UICom Lua scripting | [CN](doc/CN/UICom.md) | [EN](doc/EN/UICom.md) |
| UIVIC virtual instrument cluster | [CN](doc/CN/UIVIC.md) | [EN](doc/EN/UIVIC.md) |

## License

Dual-licensed under GPLv3 and a commercial license (see [LICENSE](LICENSE)). For commercial use, contact: parai@foxmail.com
