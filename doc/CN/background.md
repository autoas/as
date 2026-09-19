---
layout: post
title: 开篇
category: AUTOSAR
comments: true
---

# 我在做什么

受 Linux 开源精神的影响，从毕业至今，我一直坚持每天写一点代码并开源，其中绝大多数与 AUTOSAR 及其工具链相关。早期受个人能力和精力所限，开发主要基于 ArcCore 的 AUTOSAR 开源版本进行，所有产物都堆放在 [autoas/as-deprecated](https://github.com/autoas/as-deprecated) 中。如今，这个库已经被我放弃，原因有如下几点：

* ArcCore 自开源 AUTOSAR 3.1 版本后，再未有更新；
* ArcCore 采用 GPLv2 协议，无法直接用于商业产品；
* [autoas/as-deprecated](https://github.com/autoas/as-deprecated) 更像是我个人的一个学习库，里面堆放了很多与 AUTOSAR 无关的东西；
* 库的结构越来越复杂，初学者很难玩转；
* 鲜有人愿意参与 AUTOSAR 开源。

# 我想做什么

所以我想做出一点更简单的东西。我不想做一个大而全的完整 AUTOSAR，而是想开发一系列小的模块——可以单独使用、并且使用频率很高的 AUTOSAR 模块。或许有人会说：这样不就不是一个完整的 AUTOSAR 了吗？但这真的不重要。AUTOSAR 本身也有实现符合等级（ICC, Implementation Conformance Class）的概念，在某些应用场景下，这些小而美的模块反而更有优越性。

典型的例子是 MCU 的 bootloader：受 MCU Flash 大小限制，bootloader 的代码体积越小越好，此时可以单独使用的 CanTp、LinTp、Dcm 就非常有优势——只拿需要的模块，不背多余的包袱。

同时，我也想做一个可仿真的平台，利用软件仿真技术来开发和验证这些模块。这样即使用户没有硬件开发板，只要有一台电脑，也可以学习、评估这些模块。目前项目同时提供两条仿真路径：

* 基于 QEMU 的 MCU 全系统仿真，可以直接运行和调试 bootloader 与应用程序；
* 基于 IP socket 的 CAN/LIN 总线仿真器（CanBusSimulator / LinBusSimulator），配合宿主机构成完整的虚拟总线环境。

在本文最初写就之时，我的新库 [autoas/as](https://github.com/autoas/as) 才初具雏形；而现在，它已经包含：

* 基于 CAN、LIN 的通信与诊断（UDS）协议栈；
* 基于以太网 socket 的 DoIP、SOME/IP-SD 协议栈；
* 基于 EEP/FLS 的 NvM 存储栈；
* 完整的基于 CAN、LIN 的 bootloader 上下位机解决方案；
* asone、AsPy、JSON Editor 等 PC 端工具；
* 十余个 MCU/ECU 平台的移植支持（详见 README 的平台列表）。

这些模块或许还谈不上百分之百完美，比如完全符合 MISRA C 规范、满足功能安全要求等等，但它们完全可以胜任大量实际应用需求。

接下来，我会持续对这个库做介绍：如何搭建仿真与开发环境、每个模块有哪些 API、每个模块如何使用与集成，等等。

# 关于商业

开源最初以学习和技术积累为目的，这个过程也确实带来了很多收获。随着各模块逐渐成熟，希望它们除了用于学习评估之外，也能在实际项目中发挥作用，并通过持续投入与反馈形成良性循环。

与此同时，对已有研究成果进行系统化整理、持续开发与反复验证这些模块，并撰写配套文档，本身也是一项有价值的工作。因此，业余时间的重点会放在模块的完善、验证和文档建设上，也希望这些工作能够获得相应的回报。

本项目采用 GPLv3 与商业授权双许可模式：

* 评估、学习与开源用途可直接使用 GPLv3 版本；
* 若贵公司需要在闭源商业产品中使用本项目的任何模块，或需要 bootloader 完整解决方案，欢迎邮件联系，一切可商量。

联系我，Email：parai@foxmail.com
