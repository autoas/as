---
layout: post
title: AUTOSAR Diagnostic Over IP
category: AUTOSAR
comments: true
---

## Sockets for one DoIP server

### 1. UDP discovery socket

This socket is by default configured as broadcast for sending vehicle announcement message.

### 2. TCP DoIP server socket

This socket is created as serever for waiting client connections.

## An example to play with

* NetApp: An app with SOMEIP/SD, DoIP and CAN stack together
* DoIPSend: An app that play as a DOIP tester

Build NetApp accoriding to [SOMEIP/SD](./SOMEIP-SD.md)

```sh
scons --app=DoIPSend

# run NetApp
build\nt\GCC\NetApp\NetApp.exe

# run CanApp as edge node
build\nt\GCC\CanApp\CanApp.exe

set AS_LOCAL_IP=172.18.0.1
build\nt\GCC\DoIPSend\DoIPSend.exe  -v1001

build\nt\GCC\DoIPSend\DoIPSend.exe  -v1001 -t caaa

```


