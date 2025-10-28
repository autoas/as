---
layout: post
title: 开发环境搭建
category: AUTOSAR
comments: true
---

# 工欲善其事必先利其器

现如今，其实技术都已相当完备，集成开发环境IDE满天飞，如微软的Visual Studio，谷歌的Android Studio，还有第三方开源的eclipse等等，所以，一般而言从大学出来的学生鲜有人是不用IDE的，没有IDE，可能会导致没法干活。但通常来讲好用的IDE通常都不是免费的，另外IDE在多项目工程管理时，也不是那么的灵活，尤其是在嵌入式领域，做过平台软件开发的同学或许都知道yocto、android make等等，其管理着海量软件包，每个包其实都是用Makefile或者CMake去控制构建的。但Makefile和CMake有都有着很高的学习成本，并且也不是十分的灵活。但众所周知，现在没有啥是python干不了的。

所以，我的库是使用python scons控制构建的，其具有相当高的灵活性和可控性。这里不做过多的介绍了，这里将介绍如果你想在Windows上评估或者学习[autoas/ssas-public ](https://github.com/autoas/ssas-public)这个库（后面将简称ssas），如何搭建起仿真开发环境。

本开发环境高度依赖于如下几个软件工具，各位须首先下载并安装：


| Package                             | 安装目录 |
| ---------------------------------------- | ----------- |
| [msys2]([MSYS2](https://www.msys2.org/)) | C:/msys64 |
| [anaconda3](https://www.anaconda.com/)   | C:/Anaconda3 |
|  [7z](https://sparanoid.com/lab/7z/)  | C:/Program Files/7-Zip/7z.exe |

对于msys2，其可以使用pacman工具安装后续ssas库所需要用到的工具链， 比如gcc，g++等。

对于anaconda3，其也可以使用pip工具后续安装ssas库所依赖的软件包，比如scons等，但默认，anaconda3已经继承了非常多的python库，比如pyQT等。

当你安装好如下软件后，你就可以双击ssas工程目录下的[Console.bat](../../Console.bat), 其会自动下载安装[ConEmu](https://conemu.github.io/)(一个windows下非常好用的命令行终端工具)。当然由于国内经常访问github会出错的问题，你也可以手动下载[ConEmu Portable](https://www.fosshub.com/ConEmu.html)版本，并安装到ssas-public/download/ConEmu目录下，如下图所示，之后在重新双击[Console.bat](../../Console.bat), 启动ssas开发终端。

![ConEum Install](../images/conemu-install.png)

最终，你将看到如下界面，至此我们可以开始安装ssas开发所依赖的工具链和各种库。

![conemu-terminal](../images/conemu-terminal.png)

在ConEmu终端，一行一行运行如下命令进行开发依赖库的安装。

```sh
pacman -Syuu
pacman -S unzip wget git mingw-w64-x86_64-gcc mingw-w64-x86_64-glib2 mingw-w64-x86_64-gtk3
pacman -S mingw32/mingw-w64-i686-gcc mingw-w64-x86_64-diffutils
pacman -S ncurses-devel gperf curl make cmake automake-wrapper libtool
pacman -S unrar mingw-w64-x86_64-pkg-config mingw-w64-x86_64-binutils
pacman -S msys2-runtime-devel mingw-w64-x86_64-qemu mingw-w64-x86_64-dlfcn
pacman -S mingw-w64-x86_64-protobuf
# For some anaconda installations, need to modify C:\Anaconda3\Lib\site.py to change the
# value of "ENABLE_USER_SITE" from "None" to "False", and need to ensure the user has full
# access rights to "C:\Anaconda3"
pip install scons==4.5.2 pyserial pybind11 pillow ply pyqt5 bitarray
```

* 如上命令不一定全乎，可能漏了啥也说不定，因为在我写本文档之时，我的开发环境早已搭好。但没关系，当我们开始编译时，编译错误会告诉我们缺了某个头文件啦，或者缺了某个命令啥的，各位自行谷歌百度下，解决这种问题还是很容易的。

接着，我们可以尝试编译如下几个APP测试下, 首先点击ConEmu的app，切换到app那一页， 如下命令编译：

```sh
# app 页
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

之后，在app页，运行CanApp，在切换到boot页，使用IsoTpSend模拟诊断节点，发送诊断报文：

```sh
# app 页
D:\repository\ssas-public> build\nt\GCC\CanApp\CanApp.exe
INFO    :application build @ Dec  3 2021 21:57:05
......
DCM     :physical service 10, len=2
INFO    :App_GetSessionChangePermission(1 --> 1)
INFO    :DCM s3server timeout!
```
```sh
# boot 页
D:\repository\ssas-public>build\nt\GCC\IsoTpSend\IsoTpSend.exe -v 1001
TX: 10 01
RX: 50 01 13 88 00 32
```

如上，我们其实就是模拟了一个简单的诊断会话测试，CanApp就相当于一个CAN节点，IsoTpSend相当于诊断节点。至于，仿真原理，请看后续文章。

但至此，基本上，开发环境搭建完毕。Enjoy！

# GDB 调试问题解决

我发现默认路径 `C:/msys64/usr/bin/gdb` 存在问题，建议改用 `C:/msys64/ucrt64/bin/gdb.exe`。

```sh
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
```