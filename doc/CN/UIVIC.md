---
layout: post
title: UIVIC 虚拟仪表
category: Tools
comments: true
---

# UIVIC 虚拟仪表

UIVIC 是一个基于 Qt GUI 的**虚拟仪表（Virtual Instrument Cluster）**，用于模拟整车指示灯（telltale）和仪表（gauge）。

默认情况下，UIVIC 作为 CanIC 示例应用的后端 GUI。这只是一个仿真：CanIC 与UIVIC 之间的数据交换基于 SOME/IP。

## 1. 构建与运行

按以下命令操作。建议运行前禁用所有与 VirtualBox 相关的网络适配器。

```sh
# 构建
scons --app=CanIC --os=OSAL
scons --lib=AsOne
scons --app=asone --prebuilt
scons --lib=UIVIC --prebuilt --os=OSAL

# 运行
build\nt\GCC\CanIC\CanIC.exe

cd build\nt\GCC\one
asone.exe

cd tools\asone
python main.py
# 在 CAN 面板中打开 CAN0，然后切换到 COM，按 'x' 键请求
# CanIC 进入 Network 模式，可以看到 TxMsgTime 每秒变化。
# 进入 RxMsgAbsInfo，可以用它控制 UIVIC 的仪表指针。
```

![CanIC 与 UIVIC 演示](../images/uivic-canic-demo.png)

## 2. 源代码

更多细节请阅读相关源码：

- [Swc_Telltale.c](../../app/app/config/SWC/Telltale/Swc_Telltale.c) - 指示灯软件组件
- [Swc_Gauge.c](../../app/app/config/SWC/Gauge/Swc_Gauge.c) - 仪表软件组件
- [CanIC 侧的 SOME/IP 服务端](../../tools/asone/src/ui/vic/server.cpp)
- [UIVIC 侧的 SOME/IP 客户端](../../tools/asone/src/ui/vic/client.cpp)
- [UIVIC 主窗口](../../tools/asone/src/ui/UIVIC.cpp)
