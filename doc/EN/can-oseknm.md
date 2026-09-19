---
layout: post
title: CAN OSEK NM Introduction
category: AUTOSAR
comments: true
---

# CAN OSEK NM Introduction

CAN Network Management (NM) is much simpler than Ethernet network management. It solves one core problem: making all CAN nodes on the bus **wake up and go to sleep together** - everyone is active when communication is needed, and everyone sleeps when nobody has anything to send.

There are several CAN NM variants; the common ones are [AUTOSAR CanNm](../../infras/include/CanNm.h) and [OSEK NM](../../infras/include/OsekNm.h), plus many OEM-specific mechanisms, all pursuing the same goal.

OSEK NM itself comes in two flavors:

* **Direct network management**: dedicated NM CAN frames carry network state; the nodes form a logical ring and pass a token in order;
* **Indirect network management**: no dedicated NM frames exist; application frames are reused to synchronize network state indirectly.

Under this classification AUTOSAR CanNm is also a form of direct network management. Indirect management is rarely used in real projects, and the application frames each OEM borrows differ and are hard to standardize, so this article only covers OSEK **direct** network management. For the normative details see the OSEK/VDX NM 253 specification; the focus here is how this implementation is configured and used.

## Table of Contents

- [1. Configuration](#1-configuration)
- [2. Integration](#2-integration)
  - [Mode Control APIs](#mode-control-apis)
  - [Error Handling](#error-handling)
  - [State Machine](#state-machine)
- [Host Simulation Lab](#host-simulation-lab)

## 1. Configuration

A one-channel configuration example can be found in [OsekNm.json](../../app/app/config/Com/OsekNm.json):

```json
{
  "class": "OsekNm",
  "networks": [
    {
      "name": "OSEKNM0",
      "tTx": 20,
      "tTyp": 1000,
      "tMax": 1500,
      "tError": 1000,
      "tWbs": 5000,
      "NodeMask": "0xFF",
      "NodeId": 0,
      "rx_limit": 4,
      "tx_limit": 8
    }
  ]
}
```

Parameter meanings (the struct is defined in [OsekNm_Priv.h](../../infras/communication/OsekNm/OsekNm_Priv.h)):

| Parameter | Meaning |
| --- | --- |
| `name` | Network name; used to derive the txPduId (e.g. `CANIF_OSEKNM0_TX`) |
| `NodeId` | This node's id on the logical ring; it also determines the NM CAN ID: **`0x400 + NodeId`** |
| `tTyp` | Typical period between two ring messages in normal operation (ms) |
| `tMax` | Maximum allowed interval between two ring messages (ms); a timeout means the ring is broken |
| `tError` | Interval between two NMLimpHome messages after entering LimpHome (ms) |
| `tWbs` | Time to wait after a sleep indication before entering NMBusSleep (ms) |
| `tTx` | Delay before retrying a transmit request that the lower layer rejected (ms) |
| `tx_limit` | Consecutive transmit failure/error count that forces LimpHome when exceeded |
| `rx_limit` | Consecutive receive error count that forces LimpHome when exceeded |
| `NodeMask` | Mask used to extract the NodeId from a received CAN ID (`CanId & NodeMask`) |

All timing parameters are given in milliseconds; the generator converts them into numbers of `OsekNm_MainFunction` cycles according to `OSEKNM_MAIN_FUNCTION_PERIOD` (default 10 ms, overridable with a top-level `MainFunctionPeriod` in the JSON).

An NM frame is 8 bytes long (`OsekNm_PduType` in [OsekNm.h](../../infras/include/OsekNm.h)): source node id, destination node id, OpCode (Ring / Aloof / Sleep indication, etc.) plus 6 bytes of RingData.

## 2. Integration

This OSEK NM implementation follows the AUTOSAR layered architecture. The CanIf integration glue is generated automatically into `GEN/OsekNm_Cfg.c` by [OsekNm.py](../../tools/generator/OsekNm.py):

* default `OsekNm_D_Offline` / `OsekNm_D_Online` implementations (which call Nm's `Nm_PrepareBusSleepMode` / `Nm_NetworkMode` internally);
* when CanIf is not used (`USE_CANIF` undefined), stub implementations of `CanIf_RxIndication` / `CanIf_TxConfirmation` / `CanIf_Transmit`; the receive path filters NM frames with `(CanId & 0xFFFFFF00) == 0x400`, and frames are sent with ID `0x400 + NodeId`.

The only bus-action callback the integrator must provide is `OsekNm_D_Init` (the generator emits an empty one, override it as needed):

```c
void OsekNm_D_Init(NetworkHandleType NetId, OsekNm_RoutineRefType Routine);
```

`Routine` identifies the bus action to perform:

| Routine | Action |
| --- | --- |
| `OSEKNM_ROUTINE_BUS_INIT` | Initialize the bus |
| `OSEKNM_ROUTINE_BUS_SHUTDOWN` | Shut down the bus |
| `OSEKNM_ROUTINE_BUS_RESTART` | Restart the bus |
| `OSEKNM_ROUTINE_BUS_SLEEP` | Put the bus to sleep (wake-up capable) |
| `OSEKNM_ROUTINE_BUS_AWAKE` | Wake the bus up |

When CanIf is used (the typical setup on simulated platforms), configure the RxPdu for the NM channel in the PDU routing table and call the NM hooks on the receive / tx-confirmation paths:

```c
/* CanIf routes a received frame to OsekNm according to its ID */
OsekNm_RxIndication(NetId, PduInfoPtr);

/* Transmission completed */
OsekNm_TxConfirmation(NetId, E_OK);
```

Initialize and start network management:

```c
OsekNm_Init(&OsekNm_Config);
OsekNm_Talk(0);     /* join the ring (Silent = listen without sending) */
OsekNm_Start(0);
```

The state machine must be driven by a periodic task (period matching `OSEKNM_MAIN_FUNCTION_PERIOD`):

```c
void MainFunction_10ms(void) {
  OsekNm_MainFunction();
}
```

### Mode Control APIs

Request a network mode (sleep/wake):

```c
OsekNm_GotoMode(0, OSEKNM_BUS_SLEEP);  /* request coordinated sleep */
OsekNm_GotoMode(0, OSEKNM_AWAKE);      /* request wake-up */
```

Participation mode:

```c
OsekNm_Talk(0);    /* take part in the logical ring, send and receive NM frames */
OsekNm_Silent(0);  /* silent: stay online and listen, but never send NM frames */
```

Network request/release (the interface toward upper-layer ComM/Nm):

```c
OsekNm_NetworkRequest(0);  /* local communication needed; keep the network awake */
OsekNm_NetworkRelease(0);  /* local communication finished; the network may sleep */
```

Miscellaneous:

```c
OsekNm_Stop(0);            /* stop network management */

Nm_ModeType mode;
OsekNm_GetState(0, &mode); /* NM_MODE_BUS_SLEEP / NM_MODE_PREPARE_BUS_SLEEP /
                              NM_MODE_SYNCHRONIZE / NM_MODE_NETWORK */
```

### Error Handling

Bus-off and local wake-up events must be reported to OsekNm:

```c
/* notify NM after a CAN bus-off recovery (triggers BUS_RESTART;
   repeated errors drive the node into LimpHome) */
OsekNm_BusErrorIndication(NetId);

/* notify NM when a bus wake-up interrupt fires */
OsekNm_WakeupIndication(NetId);
```

### State Machine

Internal states (defined in [OsekNm_Priv.h](../../infras/communication/OsekNm/OsekNm_Priv.h)):

| State | Meaning |
| --- | --- |
| `OSEKNM_STATE_OFF` | Off; network management not started |
| `OSEKNM_STATE_ON` | Internal transition state after start |
| `OSEKNM_STATE_NORMAL` | Normal Operation; actively taking part in the logical ring |
| `OSEKNM_STATE_NORMAL_PREPARE_SLEEP` | Sleep indication received in normal mode; preparing to sleep |
| `OSEKNM_STATE_WAIT_BUS_SLEEP_NORMAL` | Waiting out the bus-sleep timer (tWbs) in normal mode |
| `OSEKNM_STATE_BUS_SLEEP` | Bus sleep; NM frames stopped, waiting for wake-up |
| `OSEKNM_STATE_LIMPHOME` | Network fault (timeout / counter limit); periodic NMLimpHome frames |
| `OSEKNM_STATE_LIMPHOME_PREPARE_SLEEP` | Preparing to sleep while in LimpHome |
| `OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME` | Waiting out the bus-sleep timer while in LimpHome |

## Host Simulation Lab

On a Windows host you can run several NM nodes at once over the v2 virtual CAN bus (see [Virtual CAN environment](./virtual-can-env.md)) without any hardware. Each host process is one node on the ring; the node id is injected at process start through the `OSEKNM_NODE_ID` environment variable (a constructor in the generated `OsekNm_Cfg.c` reads it and calls `CanIf_SetDynamicTxId` so frames are sent with ID `0x400 + NodeId`):

```sh
# sim tab: the v2 multicast bus needs no central server; optionally start a sniffer
D:\repository\as>build\nt\GCC\CanDump\CanDump.exe -d simulator_v2
```

```sh
# open 3 terminals and simulate nodes 1, 2, 3 joining the network
D:\repository\as>set OSEKNM_NODE_ID=1
D:\repository\as>build\nt\GCC\CanApp\CanApp.exe

D:\repository\as>set OSEKNM_NODE_ID=2
D:\repository\as>build\nt\GCC\CanApp\CanApp.exe

D:\repository\as>set OSEKNM_NODE_ID=3
D:\repository\as>build\nt\GCC\CanApp\CanApp.exe
```

After all three nodes are up, CanDump shows NM frames with IDs `0x401 / 0x402 / 0x403` rotating at the tTyp period - that is the logical ring passing the token. While any node calls `OsekNm_NetworkRequest` the network stays awake; once every node has called `OsekNm_NetworkRelease` and tWbs elapses, the whole network enters BusSleep and NM frames disappear from the bus; a wake-up on any node wakes the whole network again.
