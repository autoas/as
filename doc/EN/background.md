---
layout: post
title: Background
category: AUTOSAR
comments: true
---

# What I Have Been Doing

Inspired by the open-source spirit of Linux, I have kept writing and open-sourcing code almost every day since I graduated, most of it related to AUTOSAR and its toolchain. In the early years, limited by both ability and spare time, my work was based on the open-source AUTOSAR release from ArcCore, and everything ended up in [autoas/as-deprecated](https://github.com/autoas/as-deprecated). That repository has now been abandoned, for the following reasons:

* ArcCore never released anything newer than its AUTOSAR 3.1 version;
* ArcCore is licensed under GPLv2, which makes it impossible to use in commercial products;
* [autoas/as-deprecated](https://github.com/autoas/as-deprecated) was really my personal playground, piled high with things unrelated to AUTOSAR;
* The repository grew more and more complex, making it hard for beginners to get started;
* Very few people were willing to contribute to open-source AUTOSAR.

# What I Want to Do

So I decided to build something simpler. Instead of a huge, all-in-one AUTOSAR implementation, I want to develop a set of small modules - AUTOSAR modules that can be used standalone and are needed very often. One might object that this would no longer be a "complete" AUTOSAR, but that really does not matter. AUTOSAR itself defines the concept of Implementation Conformance Classes (ICC); in many scenarios these small, focused modules are actually the better fit.

A typical example is an MCU bootloader: constrained by the MCU flash size, the smaller the bootloader code the better. In that case, standalone CanTp, LinTp and Dcm are exactly what you want - take only the modules you need and carry no extra baggage.

I also wanted a simulation platform so that these modules can be developed and verified with software simulation. This way anyone can learn and evaluate them with nothing more than a computer, even without a hardware board. The project now offers two simulation paths:

* QEMU-based full-system MCU simulation, where the bootloader and applications can be run and debugged directly;
* IP-socket based CAN/LIN bus simulators (CanBusSimulator / LinBusSimulator), which together with host applications form a complete virtual bus environment.

When these words were first written, my new repository [autoas/as](https://github.com/autoas/as) had only just taken shape. Today it already contains:

* CAN- and LIN-based communication and diagnostic (UDS) stacks;
* Ethernet-socket based DoIP and SOME/IP-SD stacks;
* An NvM memory stack on top of EEP/FLS;
* A complete CAN/LIN bootloader solution for both the PC side and the ECU side;
* PC tools including asone, AsPy and the JSON Editor;
* Ports to more than ten MCU/ECU platforms (see the platform list in the README).

These modules may not yet be perfect - for example, they are not 100% MISRA C compliant and do not fully address functional safety requirements - but they are fully capable of serving many real-world applications.

Going forward, I will keep documenting the repository: how to set up the simulation and development environment, what APIs each module offers, and how to use and integrate them.

# About Commercial Use

Open source started as a way of learning and accumulating technical experience, and the journey has indeed been rewarding in many ways. As the modules mature, the hope is that they can serve not only study and evaluation purposes, but also real-world projects, with continued investment and feedback forming a virtuous cycle.

At the same time, systematically organizing existing research, continuously developing and repeatedly verifying these modules, and writing the accompanying documentation is valuable work in itself. My spare time will therefore focus on improving and validating the modules and building up the documentation, and I hope this work can receive corresponding support in return.

This project is dual-licensed under GPLv3 and a commercial license:

* The GPLv3 version can be used directly for evaluation, study and open-source use;
* If your company needs any module of this project in a closed-source commercial product, or needs a complete bootloader solution, please email me - everything is negotiable.

Contact: parai@foxmail.com
