---
layout: post
title: AUTOSAR NvM
category: AUTOSAR
comments: true
---

# Configuration notes for NvM

[NvM example](../../app/app/config/NvM/NvM.json) is mainly a configuration example for how to configure the NvM for Dem/DTC storage.

```json
{
    "class": "NvM",
    "target": "Fee",
    "blocks": [
        {
            "name": "Dem_NvmEventStatusRecord{}",
            "repeat": 8,
            "data": [
                { "name": "status", "type": "uint8", "default": "0x50" },
                { "name": "testFailedCounter", "type": "uint8", "default": 0 }
            ]
        },
        ...
    ]
}
```

The `target` must be one of `Fee` or `Ea`.

And below table is the attribute for a block

| attr | coments |
|------|--------|
| name | name must be end with `{}` if attr `repeat` is present |
| repeat | optional, repeat the definition of this NvM block N times |
| data | the list of data elements in this block |

For the above example, the `repeat` of `Dem_NvmEventStatusRecord{}` is 8, it equals to:

```json
  { "name": "Dem_NvmEventStatusRecord0", "data": [....] },
  { "name": "Dem_NvmEventStatusRecord1", "data": [....] },
  { "name": "Dem_NvmEventStatusRecord2", "data": [....] },
  { "name": "Dem_NvmEventStatusRecord3", "data": [....] },
  { "name": "Dem_NvmEventStatusRecord4", "data": [....] },
  { "name": "Dem_NvmEventStatusRecord5", "data": [....] },
  { "name": "Dem_NvmEventStatusRecord6", "data": [....] },
  { "name": "Dem_NvmEventStatusRecord7", "data": [....] },
```

Also this `repeat` also be applied to the data elements.

| attr | coments |
|------|--------|
| name | data name must be end with `{}` if attr `repeat` is present |
| repeat | optional, repeat the definition of this data N times |
| type | one of int8, int16, int32, uint8, uint16, uint32, or array type int8_n, int16_n, int32_n, uint8_n, uint16_n, uint32_n, if is array, `size` must be specified |
| size | the size of the array |
| default | default values, it can be a python expression, python `eval` API will be used to generate the C default initializer |


## Genetator

* [Genetator NvM.py](../../tools/generator/NvM.py)
