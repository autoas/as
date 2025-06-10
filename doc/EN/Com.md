---
layout: post
title: AUTOSAR Com
category: AUTOSAR
comments: true
---

# Configuration notes for Com

Below is 1 examples:

* [application/Com.json](../../app/app/config/Com/Com.json)

```json
{
  "class": "Com",
  "networks": [
    {
      "name": "CAN0",
      "network": "CAN",
      "device": "simulator_v2",
      "port": 0,
      "baudrate": 500000,
      "me": "AS",
      "groups": [
        {
          "SystemTime": ["year", "month", "day", "hour", "minute", "second"]
        }
      ],
      "dbc": "CAN0.dbc"
    }
  ]
}
```

## networks

For Com, networks are used to specify those messages and signals from/to the Com module, but for CAN network, as dbc are generally provided, so we can simplely specify the "dbc" only. And the "groups" are used to tell if there is some singals want to grouped according to the AUTOSAR Com group signal definition.


And please note that, under the parent directory of Com.json, file GEN/Com.json will be generated, in which you can see that all messages/signsls in dbc converted into messages/signsls.

* [GEN/Com.json](../../app/app/config/Com/GEN/Com.json)

And for LIN network, using "ldf" instead of "dbc" to import message and signals, for example:

```json
{
  "class": "Com",
  "networks": [
    {
      "name": "LIN0",
      "network": "LIN",
      "me": "AS",
      "ldf": "LIN0.ldf"
    }
  ]
}
```

## Genetator

* [Genetator Com.py](../../tools/generator/Com.py)