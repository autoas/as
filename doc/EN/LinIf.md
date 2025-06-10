---
layout: post
title: AUTOSAR LinIf
category: AUTOSAR
comments: true
---

# Configuration notes for LinIf

Below is 1 examples:

```json
{
  "class": "LinIf",
  "networks": [
    {
      "name": "LIN0",
      "me": "AS",
      "timeout": 100,
      "ldf": "LIN0.ldf"
    }
  ]
}
```

Currently, for the LIN commutation, \<Mod\>_TriggerTransmit type of API are used and for the purpose to make things simple, the LinIf will ensure the routing to the right upper module.

And this configuration is very simple, just provide the LIN ldf file, that's all, enjoy!

And then need to configure the related Com and LinTp.

For the Com, it's also simple, below is a Com.json example, just configure the ldf as below:

```json
{
  "class": "Com",
  "networks": [
    ...
    {
      "name": "LIN0",
      "network": "LIN",
      "me": "AS",
      "ldf": "LIN0.ldf"
    }
  ]
}
```

For the LinTp master, refer [master LinTp_Cfg.c](../../app/platform/ac7840x/config/LinTp/LinTp_Cfg.c), which is a Lin master CanTp to LinTp demo.

For the LinTp slave, refer [slave LinTp_Cfg.c](../../app/bootloader/config/LinTp_Cfg.c), which is a Lin salve that LinTp to Dcm demo.


A general routing for LIN slave node.
```
            +--------------+      +--------------+
            |     Com      |      |     Dcm      |
            +--------------+      +--------------+
                 | ^                    | ^
                 V |                    V |
            +--------------+      +--------------+
            |    LinIf     | <--> |    LinTp     |
            +--------------+      +--------------+
                 | ^
                 V |
            +--------------+
            |      Lin     |
            +--------------+

```

A general routing for LIN master node.
And for master node, it generally that has CanTp to LinTp Routing in PduR for the purpose to do UDS with those LIN slave nodes through CAN to LIN gateway.

```
            +--------------+         +--------------+
            |     Com      |         |     PduR     |
            +--------------+         +--------------+
                 | ^                   | ^      | ^
                 V |                   V |      V |
            +--------------+      +--------+  +--------+
            |    LinIf     | <--> |  LinTp |  | CanTp  |
            +--------------+      +--------+  +--------+
                 | ^                             | ^
                 V |                             V |
            +--------------+                  +--------+
            |      Lin     |                  |  Can   |
            +--------------+                  +--------+

```
