---
layout: post
title: AUTOSAR CanTp 配置
category: AUTOSAR
comments: true
---

# CanTp 配置说明

CanTp 实现 ISO 15765-2 定义的 CAN 分段传输，向下连接 CanIf，向上连接 PduR。配置由 [CanTp.py](../../tools/generator/CanTp.py) 消费；本文示例为[app/app/config/CanTp/CanTp.json](../../app/app/config/CanTp/CanTp.json)，生成代码见 [CanTp_Cfg.c](../../app/app/config/CanTp/GEN/CanTp_Cfg.c)。

## 1. 基于通道的设计

`channels` 中每个条目创建一条逻辑 CanTp 通道。一条通道固定包含 1 个 RxPdu 和1 个 TxPdu，生成器按通道名派生其 CanIf 名称：`${name}_RX` 和 `${name}_TX`。

```json
{
  "class": "CanTp",
  "channels": [
    { "name": "P2P" },
    { "name": "P2A", "ComType": "FUNCTIONAL" },
    { "name": "P2A_FW", "ComType": "FUNCTIONAL" }
  ],
  "backup-channels": [
    { "name": "FW_CAN0_SECOC_MSG0" }
  ]
}
```

* `channels` - 物理寻址和功能寻址诊断通道。`ComType` 为 `PHYSICAL`（默认）或`FUNCTIONAL`；
* `backup-channels` - 用作 SecOC 兜底路径的通道。备份通道会被实例化，但在 SecOC切换过去之前不参与路由。

对应的 CanIf PDU 必须在 CanIf.json 中以相同名称存在，且 `up` 设为 `CanTp`：

```json
"RxPdus": [
  { "name": "P2P_RX", "id": "0x731", "hoh": 0, "up": "CanTp" },
  { "name": "P2A_RX", "id": "0x7DF", "hoh": 0, "up": "CanTp" }
],
"TxPdus": [
  { "name": "P2P_TX", "id": "0x732", "hoh": 0, "up": "CanTp" },
  { "name": "P2A_TX", "id": "0x732", "hoh": 0, "up": "CanTp" }
]
```

## 2. 通道参数

除 `name` 外所有字段均可选，缺省时使用编译默认值：

| 字段 | 默认值 | 说明 |
| --- | --- | --- |
| `name` | - | 通道名；据此派生 CanIf `${name}_RX` / `${name}_TX` PDU 名以及 PduR 路由 ID |
| `ComType` | `PHYSICAL` | `PHYSICAL` 一对一物理寻址；`FUNCTIONAL` 功能寻址请求（CAN ID 0x7DF） |
| `N_As` | 0 | 发送方发送一帧 CAN 报文（任意 N-PDU）的时间 |
| `N_Bs` | 0 | 等待下一个流控制 N-PDU 的超时（ISO 15765-2） |
| `N_Cr` | 0 | 等待下一个连续帧 N-PDU 的超时（ISO 15765-2） |
| `STmin` | 0 | 两个连续帧之间的最小发送间隔 |
| `BS` | 0 | 发送流控制帧时使用的块大小（Block Size） |
| `WftMax` | 8 | 允许连续发送的等待流控帧（FC-WAIT）次数；不能为 0xFF |
| `LL_DL` | `CANTP_LL_DL`（8） | 底层单帧数据长度：经典 CAN 为 8，CAN FD 为 64 |
| `padding` | `0x55` | 启用填充时使用的填充字节 |
| `N_TA` | 0 | 网络目标地址，仅扩展寻址时需要 |

顶层可选 `STMinAdjust`，让生成代码对协商得到的 STmin 叠加一个固定修正量。在主机仿真环境中，进程启动时可用环境变量 `LL_DL` 覆盖所有通道的 `LL_DL`，这样同一份构建无需重新生成即可在经典 CAN 与 CAN FD 之间切换。

生成的通道结构体：

```c
typedef struct {
  CanTp_AddressingFormatType AddressingFormat;
  PduIdType CanIfTxPduId;
  PduIdType PduR_RxPduId;
  PduIdType PduR_TxPduId;
  uint16_t N_As;
  uint16_t N_Bs;
  uint16_t N_Cr;
  uint8_t STmin;
  uint8_t BS;
  uint8_t N_TA;              /* 仅 CANTP_EXTENDED 寻址使用 */
  uint8_t CanTpRxWftMax;     /* 不能为 0xFF */
  uint8_t LL_DL;             /* CAN 为 8，CAN FD 为 64 */
  uint8_t padding;
  uint8_t *data;             /* 分段帧发送缓冲区 */
} CanTp_ChannelConfigType;
```

## 3. 注意事项

1. JSON 只暴露常用参数；高级调优（寻址格式、自定义缓冲区大小）可直接修改生成的`CanTp_Cfg.c`。
2. CanTp 报文不出现在 DBC 中，始终在 CanIf.json 里显式声明。
3. CanTp 与上层（Dcm、DoIP、LinTp，或网关场景下的另一个 CanTp）之间的路由由PduR 生成器生成，见 PduR 文档。

## 4. 生成器

配置由 [CanTp.py](../../tools/generator/CanTp.py) 处理，在 JSON 同目录生成`GEN/CanTp_Cfg.c` 和 `GEN/CanTp_Cfg.h`。
