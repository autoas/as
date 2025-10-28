---
layout: post
title: TLS
category: AUTOSAR
comments: true
---

# TLS Overview

```
  +------------+
  |  User App  |
  +------------+
    |       ^
    |send   |recv
    V       |
  +------------+
  |   TLS      |
  +------------+
    |       ^
    |send   |recv
    V       |
  +------------+
  |   SoAd     |
  +------------+
    |       ^
    |send   |recv
    V       |
  +------------+
  |   TcpIp    |
  +------------+
```