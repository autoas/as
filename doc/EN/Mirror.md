---
layout: post
title: AUTOSAR Bus Mirror
category: AUTOSAR
comments: true
---

## Menu

- [Configuration notes for Bus Mirror](#configuration-notes-for-bus-mirror)
  - [`SourceNetworkCan` Configuration](#sourcenetworkcan-configuration)
  - [`SourceNetworkLin` Configuration](#sourcenetworklin-configuration)
  - [`DestNetworkIp` Configuration](#destnetworkip-configuration)
- [Integration notes for Bus Mirror](#integration-notes-for-bus-mirror)
  - [LinIf integration notes](#linif-integration-notes)
  - [CanIf integration notes](#canif-integration-notes)
  - [SoAd integration notes](#soad-integration-notes)
  - [Mirror timestamp integration notes](#mirror-timestamp-integration-notes)

# Configuration notes for Bus Mirror

* [application/Mirror.json](../../app/app/config/Mirror/Mirror.json)

## `SourceNetworkCan` Configuration

Defines the settings for a **source CAN network**, including filters and controller identification.

### **JSON Example:**
```json
"SourceNetworkCan": [
    {
      "name": "CAN0",
      "StaticFilters": [
         { "type": "range", "lower": 0, "upper": "0x100" },
         { "type": "mask", "code": "0x700", "mask": "0x700" }
      ],
      "MaxDynamicFilters": 128,
      "ControllerId": 0
    }
]
```

---

### **Parameter Definitions:**

| Parameter            | Required? | Description                                                                 |
|----------------------|-----------|-----------------------------------------------------------------------------|
| **`name`**           | Yes       | Name of the CAN network (e.g., `"CAN0"`).                                   |
| **`StaticFilters`**  | No        | List of static filters. Supports two types (see below). Default: `[]`.      |
| **`MaxDynamicFilters`** | Yes    | Maximum dynamic filters allowed (range: `1~255`). Total filters (static + dynamic) must **not exceed 255**. |
| **`ControllerId`**   | Yes       | Numeric ID of the CAN controller (e.g., `0`).                              |

---

#### **Static Filter Types:**
1. **`range`**  
   - Filters CAN IDs within a range.  
   - **Fields:**  
     - `lower`: Start of range (e.g., `0`).  
     - `upper`: End of range (e.g., `"0x100"`).  

2. **`mask`**  
   - Filters IDs using a bitmask.  
   - **Fields:**  
     - `code`: Pattern to match (e.g., `"0x700"`).  
     - `mask`: Bitmask applied (e.g., `"0x700"`).  

---

## `SourceNetworkLin` Configuration

Defines the settings for a **source LIN network**, including filters and controller identification.

### **JSON Example:**
```json
"SourceNetworkLin": [
    {
      "name": "LIN0",
      "StaticFilters": [
         { "type": "range", "lower":0, "upper": "0x10" },
         { "type": "mask", "code":"0x20", "mask": "0x70" }
      ],
      "MaxDynamicFilters": 128,
      "ControllerId": 0
    }
]
```

---

### **Parameter Definitions:**

| Parameter            | Required? | Description                                                                 |
|----------------------|-----------|-----------------------------------------------------------------------------|
| **`name`**           | Yes       | Name of the LIN network (e.g., `"LIN0"`).                                   |
| **`StaticFilters`**  | No        | List of static filters. Supports two types (see below). Default: `[]`.      |
| **`MaxDynamicFilters`** | Yes    | Maximum dynamic filters allowed (range: `1~255`). Total filters (static + dynamic) must **not exceed 255**. |
| **`ControllerId`**   | Yes       | Numeric ID of the LIN controller (e.g., `0`).                              |

---

#### **Static Filter Types:**
1. **`range`**  
   - Filters LIN IDs within a range.  
   - **Fields:**  
     - `lower`: Start of range (e.g., `0`).  
     - `upper`: End of range (e.g., `"0x10"`).  

2. **`mask`**  
   - Filters IDs using a bitmask.  
   - **Fields:**  
     - `code`: Pattern to match (e.g., `"0x20"`).  
     - `mask`: Bitmask applied (e.g., `"0x70"`).  

---

## `DestNetworkIp` Configuration

Defines the settings for a **destination IP network**, including queue/buffer sizes and SoAd socket association.  

### **JSON Example:**  
```json
"DestNetworkIp": [
    {
        "name": "AS",
        "DestQueueSize": 2,
        "DestBufferSize": 1400,
        "SoAd": "MIRROR_CLIENT_0"
    }
]
```

---

### **Parameter Definitions:**  

| Parameter             | Required? | Description                                                                 |
|-----------------------|-----------|-----------------------------------------------------------------------------|
| **`name`**            | Yes       | Logical name of the IP network (e.g., `"AS"`).                              |
| **`DestQueueSize`**   | Yes       | Maximum number of frames buffered in the output queue. <br> **Note:** Impacts latency and memory usage. <br> **Note:** The value must be power of 2. |
| **`DestBufferSize`**  | Yes       | Size (in bytes) of the frame buffer for outgoing data. <br> **Must align with MTU/packet size constraints.** |
| **`SoAd`**            | Yes       | Corresponding SoAd socket connection name (e.g., `"MIRROR_CLIENT_0"`). <br> Ensures proper routing to the AUTOSAR Socket Adapter layer. |

---

# Integration notes for Bus Mirror

## LinIf integration notes

```c
void Mirror_ReportLinFrame(NetworkHandleType network, Lin_FramePidType pid, const PduInfoType *pdu, Lin_StatusType status);

// this API was optional
Std_ReturnType LinIf_EnableBusMirroring(NetworkHandleType Channel, boolean MirroringActive);

// suggest implementation for LinIf_EnableBusMirroring
static boolean bLinMirroringActive[4];
Std_ReturnType LinIf_EnableBusMirroring(NetworkHandleType Channel, boolean MirroringActive) {
    bLinMirroringActive[Channel] = MirroringActive;
    return E_OK;
}

// corresponding place in LinIf call Mirror_ReportLinFrame

if (TRUE == bLinMirroringActive[Channel]) {
    // status: LIN_RX_OK, LIN_TX_OK or other error status
    Mirror_ReportLinFrame(Channel, pid, &PduInfo, status);
}
```

## CanIf integration notes

```c
void Mirror_ReportCanFrame(uint8_t controllerId, Can_IdType canId, uint8_t length, const uint8_t *payload);
void Mirror_ReportCanState(uint8_t controllerId, Mirror_CanNetworkStateType NetworkState);

// this API was optional
Std_ReturnType CanIf_EnableBusMirroring(uint8_t ControllerId, boolean MirroringActive);

// suggest implementation for CanIf_EnableBusMirroring
static boolean bCanMirroringActive[4];
Std_ReturnType CanIf_EnableBusMirroring(uint8_t ControllerId, boolean MirroringActive) {
    bCanMirroringActive[ControllerId] = MirroringActive;
    return E_OK;
}

// corresponding place in CanIf call Mirror_ReportCanFrame
if (TRUE == bCanMirroringActive[ControllerId]) {
    Mirror_ReportCanFrame(ControllerId, canId, &length, payload);
}

// corresponding place in CanIf or Can status ISR call Mirror_ReportCanState
if (TRUE == bCanMirroringActive[ControllerId]) {
    Mirror_ReportCanState(ControllerId, MIRROR_CAN_NS_BUS_ONLINE);
    // or
    Mirror_ReportCanState(ControllerId, MIRROR_CAN_NS_BUS_OFF);
    // or
    Mirror_ReportCanState(ControllerId, MIRROR_CAN_NS_ERROR_PASSIVE | ((TxErrorCounter/8)&MIRROR_CAN_NS_TX_ERROR_COUNTER_MASK));
}
```

## SoAd integration notes

### AS SoAd configuration example

```json
"sockets": [
    {
        "name": "MIRROR_CLIENT_0",
        "client": "224.244.224.245:30511",
        "protocol": "UDP",
        "multicast": true,
        "up": "Mirror",
        "RxPduId": "0"
    }
]
```

This configuration is almost the same for other AUTOSAR SoAd implementations, where a SoAd socket is configured for Bus Mirror.

The generator [SoAd.py](../../tools/generator/SoAd.py) may need to be updated to use the correct macros in the generated SoAd_Cfg.h, as the SoConId and TxPduId prefixes might differ from the AS implementation.


```python
            C.write("    SOAD_SOCKID_%s, /* SoConId */\n" % (network["SoAd"]))
            C.write("    SOAD_TX_PID_%s, /* TxPduId */\n" % (network["SoAd"]))
```

```c
// in AS Mirror_Cfg.c
static const Mirror_DestNetworkIpType Mirror_DestNetworkIps[] = {  {
    &Mirror_DestNetworkIpContexts[0],
    Mirror_DestBuffersAS,
    MIRROR_CONVERT_MS_TO_MAIN_CYCLES(655u), /* MirrorDestTransmissionDeadline */
    SOAD_SOCKID_MIRROR_CLIENT_0, /* SoConId */
    SOAD_TX_PID_MIRROR_CLIENT_0, /* TxPduId */
    2u, /* NumDestBuffers */
  }
};
```

In any case, either:  
- Update the generator, or  
- Manually modify the generated `Mirror_Cfg.c`  

to ensure `SoConId` and `TxPduId` use the correct naming convention. 


## Mirror timestamp integration notes

### A Standard `StbM_GetCurrentTime` API Must Be Provided

Even though **AS** does not include the **StbM** module, a minimal implementation exists in [`std_timer.c`](../../infras/system/timer/std_timer.c) for demonstration purposes.  

For **Mirror**, the implementation only needs to provide:
- `secondsHi`  
- `seconds`  
- `nanoseconds`  

#### **Example Implementation**  
```c
Std_ReturnType StbM_GetCurrentTime(StbM_SynchronizedTimeBaseType timeBaseId,
                                   StbM_TimeTupleType *timeTuple, 
                                   StbM_UserDataType *userData) {
  Std_ReturnType ret = E_OK;
  std_time_t tm;
  
  if (0 == timeBaseId) {
    tm = Std_GetTime();
    timeTuple->globalTime.secondsHi = (tm / 1000000) >> 32;
    timeTuple->globalTime.seconds = (tm / 1000000) & 0xFFFFFFFFul;
    timeTuple->globalTime.nanoseconds = (tm % 1000000) * 1000;
  } else {
    ret = E_NOT_OK;
  }
  return ret;
}
```

Additionally, the `std_time_t Std_GetTime(void)` function must be implemented to return the current time in **microseconds**.
