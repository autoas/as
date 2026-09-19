---
layout: post
title: 开发环境搭建
category: AUTOSAR
comments: true
---

# 开发环境搭建指南

## 构建系统简介

本项目（以下简称 AS）使用基于 Python 的 [SCons](https://scons.org/) 作为构建系统，通过 Python 脚本组织海量模块的编译，具有很高的灵活性与可控性。本文介绍如何在 Windows 上从零搭建 AS 的仿真开发环境，无需任何硬件开发板。

## 一、安装必备软件

首先下载并安装以下三个软件，建议使用默认安装路径：

| 软件 | 下载地址 | 默认安装路径 |
| --- | --- | --- |
| MSYS2 | [msys2.org](https://www.msys2.org/) | `C:/msys64` |
| Anaconda3 | [anaconda.com](https://www.anaconda.com/) | `C:/Anaconda3` |
| 7-Zip | [sparanoid.com/lab/7z](https://sparanoid.com/lab/7z/) | `C:/Program Files/7-Zip/7z.exe` |

* **MSYS2**：提供 pacman 包管理器，用于安装 gcc/g++、qemu 等工具链；
* **Anaconda3**：提供 Python 环境及 pip，用于安装 scons 等 Python 依赖（默认已自带 pyQt 等大量常用库）；
* **7-Zip**：开发终端 ConEmu 以 7z 压缩包方式分发，需要用它解压。

## 二、启动开发终端

双击仓库根目录下的 [Console.bat](../../Console.bat)。首次运行时，脚本会自动下载安装 [ConEmu](https://conemu.github.io/)（Windows 下一款好用的命令行终端）并解压到 `AS/download/ConEmu`。

如果网络访问 GitHub 不稳定导致自动下载失败，可以手动下载 [ConEmu Portable](https://www.fosshub.com/ConEmu.html)，解压到上述目录后再次双击 Console.bat：

![ConEmu 安装目录](../images/conemu-install.png)

启动成功后，ConEmu 会同时打开 **sim、app、boot、tools** 四个终端标签页，分别用于仿真节点、应用程序、bootloader 和工具的构建与运行：

![ConEmu 终端](../images/conemu-terminal.png)

> 提示：如需把 ConEmu 及构建过程中下载的第三方包放到仓库之外，可以设置环境变量 `AS_DOWNLOAD_DIR` 指向自定义目录。

## 三、安装依赖工具链

在任一 ConEmu 标签页中逐行执行以下命令，通过 pacman 安装工具链：

```sh
pacman -Syu
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
pacman -S unzip wget git make cmake patch automake-wrapper libtool
pacman -S mingw-w64-x86_64-gcc mingw32/mingw-w64-i686-gcc mingw-w64-x86_64-binutils
pacman -S mingw-w64-x86_64-diffutils mingw-w64-x86_64-pkg-config mingw-w64-x86_64-dlfcn
pacman -S mingw-w64-x86_64-glib2 mingw-w64-x86_64-gtk3 mingw-w64-x86_64-protobuf
pacman -S ncurses-devel gperf curl unrar msys2-runtime-devel mingw-w64-x86_64-qemu
```

然后激活 Anaconda 环境并安装 Python 依赖（建议在每个标签页执行 scons 前都先激活一次）：

```sh
c:\anaconda3\Scripts\activate
pip install scons==4.5.2 pyserial pybind11 pillow ply pyqt5 bitarray
```

> 注意：部分 Anaconda 安装需要把 `C:\Anaconda3\Lib\site.py` 中的 `ENABLE_USER_SITE` 从 `None` 改为 `False`，并确保当前用户对 `C:\Anaconda3` 目录拥有完整访问权限。

以上命令未必涵盖所有依赖（写作本文时作者的环境早已就绪）。如果编译时报缺少某个头文件或命令，按报错信息用 pacman/pip 补装即可。

## 四、验证构建

切换到 **app** 标签页，依次编译两个示例程序：

```sh
# app 标签页
D:\repository\as>scons --app=IsoTpSend
scons: Reading SConscript files ...
scons: done reading SConscript files.
scons: Building targets ...
scons: building associated VariantDir targets: build\nt\GCC\IsoTpSend
CC app\platform\simulator\src\Can.c
......
CC tools\libraries\isotp\utils\isotp_send.c
LINK build\nt\GCC\IsoTpSend\IsoTpSend.exe
scons: done building targets.

D:\repository\as>scons --app=CanApp
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

## 五、运行诊断仿真测试

在 **app** 标签页运行 CanApp，它相当于一个仿真的 CAN 节点：

```sh
# app 标签页
D:\repository\as>build\nt\GCC\CanApp\CanApp.exe
INFO    :application build @ Dec  3 2021 21:57:05
......
DCM     :physical service 10, len=2
INFO    :App_GetSessionChangePermission(1 --> 1)
INFO    :DCM s3server timeout!
```

再切换到 **boot** 标签页，运行 IsoTpSend 模拟诊断仪，向 CanApp 发送 UDS 会话切换请求（`10 01`）：

```sh
# boot 标签页
D:\repository\as>build\nt\GCC\IsoTpSend\IsoTpSend.exe -v 1001
TX: 10 01
RX: 50 01 13 88 00 32
```

至此，我们就在没有任何硬件的情况下完成了一次简单的诊断会话仿真：CanApp 扮演 CAN 节点，IsoTpSend 扮演诊断仪。仿真原理将在后续文章中介绍。

开发环境搭建完毕，Enjoy！

## 附：GDB 调试问题

默认的 `C:/msys64/usr/bin/gdb` 在调试 MinGW 程序时可能存在问题，建议改用 UCRT64 版本 `C:/msys64/ucrt64/bin/gdb.exe`，该工具链已在第三节通过以下命令安装：

```sh
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
```
