---
layout: post
title: Development Environment Setup
category: AUTOSAR
comments: true
---

# Development Environment Setup Guide

## Build System Overview

The repository uses Python SCons as its build system, providing flexibility and simplicity for AUTOSAR development. This guide covers Windows environment setup for evaluating [autoas/ssas-public](https://github.com/autoas/ssas-public).

## Required Software Installation

| Package | Download Link | Default Install Path |
|---------|---------------|----------------------|
| MSYS2 | [msys2.org](https://www.msys2.org/) | `C:/msys64` |
| Anaconda3 | [anaconda.com](https://www.anaconda.com/) | `C:/Anaconda3` |
| 7-Zip | [sparanoid.com/lab/7z](https://sparanoid.com/lab/7z/) | `C:/Program Files/7-Zip/7z.exe` |

## Environment Configuration

Use pacman to install essential toolchain components:
```sh
pacman -Syu  # Update package database
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
```

### Anaconda Configuration
Install Python dependencies via pip:
```sh
pip install scons pyserial pybind11 pillow ply pyqt5 bitarray
```

**Note:** For some Anaconda installations:
1. Modify `C:\Anaconda3\Lib\site.py`:
   ```python
   ENABLE_USER_SITE = False  # Change from None
   ```
2. Ensure full user access rights to `C:\Anaconda3`

## Development Console Setup

1. Launch the environment using [Console.bat](../../Console.bat)
2. First run will automatically install [ConEmu](https://conemu.github.io/)
   - **Manual Installation Option**:
     - Download [ConEmu Portable](https://www.fosshub.com/ConEmu.html)
     - Install to `ssas-public/download/ConEmu`
     - ![ConEmu Installation](../images/conemu-install.png)

Successful setup will show:  
![ConEmu Terminal](../images/conemu-terminal.png)
## Dependency Installation

Run these commands in ConEmu:

```sh
# Core toolchain
pacman -S unzip wget git mingw-w64-x86_64-gcc mingw-w64-x86_64-glib2 
pacman -S mingw-w64-x86_64-gtk3 mingw-w64-x86_64-diffutils
pacman -S ncurses-devel gperf curl make cmake automake-wrapper libtool

# Additional utilities
pacman -S unrar mingw-w64-x86_64-pkg-config mingw-w64-x86_64-binutils
pacman -S msys2-runtime-devel mingw-w64-x86_64-qemu mingw-w64-x86_64-dlfcn
pacman -S mingw-w64-x86_64-protobuf patch autotools

# Python packages
pip install scons pyserial pybind11 pillow ply pyqt5 bitarray
```

**Note:** Missing dependencies will be revealed during compilation and can be installed as needed.

## Verification Build

### Building Test Applications

```sh
# app panel
# better activate the python env before run scons, the same for other panels
D:\repository\ssas-public>c:\anaconda3\Scripts\activate
D:\repository\ssas-public>scons --app=IsoTpSend
scons: Reading SConscript files ...
scons: done reading SConscript files.
scons: Building targets ...
scons: building associated VariantDir targets: build\nt\GCC\IsoTpSend
CC app\platform\simulator\src\Can.c
......
CC tools\libraries\isotp\utils\isotp_send.c
LINK build\nt\GCC\IsoTpSend\IsoTpSend.exe
scons: done building targets.

D:\repository\ssas-public>scons --app=CanApp
scons: Reading SConscript files ...
scons: done reading SConscript files.
scons: Building targets ...
scons: building associated VariantDir targets: build\nt\GCC\CanApp
CC app\app\config\CanNm_Cfg.c
......
CC app\platform\simulator\src\simulator.c
LINK build\nt\GCC\CanApp\CanApp.exe
scons: done building targets.
```

### Diagnostic Simulation Test

```sh
# app panel
D:\repository\ssas-public> build\nt\GCC\CanApp\CanApp.exe
INFO    :application build @ Dec  3 2021 21:57:05
......
DCM     :physical service 10, len=2
INFO    :App_GetSessionChangePermission(1 --> 1)
INFO    :DCM s3server timeout!
```
```sh
# boot panel
D:\repository\ssas-public>build\nt\GCC\IsoTpSend\IsoTpSend.exe -v 1001
TX: 10 01
RX: 50 01 13 88 00 32
```

As shown above, we have actually simulated a simple diagnostic session test. The CanApp acts as a CAN node, while IsoTpSend simulates another diagnostic node. For details about the simulation principle, please see the follow-up article.

Now that the development environment setup is complete. Enjoy!


## Debugging Configuration

For optimal GDB performance:
```sh
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
```
Use `C:/msys64/ucrt64/bin/gdb.exe` instead of the default MSYS2 GDB.
