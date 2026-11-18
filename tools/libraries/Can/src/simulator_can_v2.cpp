/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <mutex>
#include <thread>
#include <sys/queue.h>

#include "canlib.h"
#include "canlib_types.hpp"
#include "TcpIp.h"

#if defined(_WIN32)
#include <objbase.h>
#else
#include <uuid/uuid.h>
#endif

#ifdef ERROR
#undef ERROR
#endif

using namespace std::literals::chrono_literals;
namespace simulator_can_v2 {
/* ================================ [ MACROS    ] ============================================== */
#define CAN_MAX_DLEN 64 /* 64 for CANFD */
#define CAN_MTU sizeof(struct CanL_WireFrameType)

#define CAN_CAST_IP TCPIP_IPV4_ADDR(224, 244, 224, 245)
#define CAN_PORT_MIN 8000

#define mCANID(frame)                                                                              \
  (((uint32_t)frame.canid[0] << 24) + ((uint32_t)frame.canid[1] << 16) +                           \
   ((uint32_t)frame.canid[2] << 8) + ((uint32_t)frame.canid[3]))

#define mSetCANID(frame, canid)                                                                    \
  do {                                                                                             \
    frame.canid[0] = (uint8_t)(canid >> 24);                                                       \
    frame.canid[1] = (uint8_t)(canid >> 16);                                                       \
    frame.canid[2] = (uint8_t)(canid >> 8);                                                        \
    frame.canid[3] = (uint8_t)(canid);                                                             \
  } while (0)

#define mTimeStamp(frame)                                                                          \
  (((uint64_t)frame.timestamp[0] << 56) + ((uint64_t)frame.timestamp[1] << 48) +                   \
   ((uint64_t)frame.timestamp[2] << 40) + ((uint64_t)frame.timestamp[3] << 32) +                   \
   ((uint64_t)frame.timestamp[4] << 24) + ((uint64_t)frame.timestamp[5] << 16) +                   \
   ((uint64_t)frame.timestamp[6] << 8) + ((uint64_t)frame.timestamp[7]))

#define mSetTimeStamp(frame, timestamp)                                                            \
  do {                                                                                             \
    frame.timestamp[0] = (uint8_t)(timestamp >> 56);                                               \
    frame.timestamp[1] = (uint8_t)(timestamp >> 48);                                               \
    frame.timestamp[2] = (uint8_t)(timestamp >> 40);                                               \
    frame.timestamp[3] = (uint8_t)(timestamp >> 32);                                               \
    frame.timestamp[4] = (uint8_t)(timestamp >> 24);                                               \
    frame.timestamp[5] = (uint8_t)(timestamp >> 16);                                               \
    frame.timestamp[6] = (uint8_t)(timestamp >> 8);                                                \
    frame.timestamp[7] = (uint8_t)(timestamp);                                                     \
  } while (0)

#if defined(_WIN32)
#define CAN_UUID_LENGTH sizeof(GUID)
#else
#define CAN_UUID_LENGTH 16
#endif
/* ================================ [ TYPES     ] ============================================== */
/**
 * struct CanL_WireFrameType - basic CAN frame structure
 * @can_id:  CAN ID of the frame and CAN_*_FLAG flags, see canid_t definition
 * @can_dlc: frame payload length in byte (0 .. 8) aka data length code
 *           N.B. the DLC field from ISO 11898-1 Chapter 8.4.2.3 has a 1:1
 *           mapping of the 'data length code' to the real payload length
 * @data:    CAN frame payload (up to 8 byte)
 */
struct CanL_WireFrameType {
  uint8_t canid[4];
  uint8_t dlc;
  uint8_t data[CAN_MAX_DLEN];
  uint8_t timestamp[8];
  uint8_t uuid[CAN_UUID_LENGTH];
};
struct CanL_SocketHandleType {
  uint32_t BusId;
  uint32_t Port;
  std::mutex mutex;
  std::thread RxThread;
  volatile bool Terminated;
  CanL_DeviceRxNotificationType RxNotification;
  TcpIp_SocketIdType sockRd;
  TcpIp_SocketIdType sockWt;
  STAILQ_ENTRY(CanL_SocketHandleType) entry;
  uint8_t uuid[CAN_UUID_LENGTH];
};
struct CanL_SocketHandleListType {
  bool Initialized;
  std::mutex mutex;
  STAILQ_HEAD(, CanL_SocketHandleType) head;
};
/* ================================ [ DECLARES  ] ============================================== */
static bool CanL_SocketProbe(int busId, uint32_t port, uint32_t baudrate,
                             CanL_DeviceRxNotificationType rxNotification);
static bool CanL_SocketWrite(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                             uint64_t timestamp);
static void CanL_SocketRead(uint32_t port);
static void CanL_SocketClose(uint32_t port);
static void CanL_RxDaemon(struct CanL_SocketHandleType *handle);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const CanL_DeviceOpsType CanL_SimulatorV2Ops = {
  .Name = "simulator_v2",
  .Probe = CanL_SocketProbe,
  .Close = CanL_SocketClose,
  .Write = CanL_SocketWrite,
  .Read = CanL_SocketRead,
};
static struct CanL_SocketHandleListType socketH = {
  .Initialized = false,
};
/* ================================ [ LOCALS    ] ============================================== */
#ifdef _WIN32
static void CanL_GetUuid(uint8_t *uuid, size_t length) {
  GUID guid;
  CoCreateGuid(&guid);
  memcpy(uuid, &guid, length);
}
#else
static void CanL_GetUuid(uint8_t *uuid, size_t length) {
  uuid_t uu;
  uuid_generate(uu);
  memcpy(uuid, uu, length);
}
#endif

static struct CanL_SocketHandleType *CanL_GetHandle(uint32_t port) {
  struct CanL_SocketHandleType *handle, *h;
  handle = NULL;

  std::lock_guard<std::mutex> lck(socketH.mutex);
  STAILQ_FOREACH(h, &socketH.head, entry) {
    if (h->Port == port) {
      handle = h;
      break;
    }
  }

  return handle;
}

static bool CanL_SocketProbe(int busId, uint32_t port, uint32_t baudrate,
                             CanL_DeviceRxNotificationType rxNotification) {
  bool rv = true;
  struct CanL_SocketHandleType *handle;
  TcpIp_SocketIdType sockRd, sockWt;
  Std_ReturnType ret;
  TcpIp_SockAddrType ipv4Addr;
  uint16_t PortNum;

  if (false == socketH.Initialized) {
    STAILQ_INIT(&socketH.head);
    TcpIp_Init(NULL);
    socketH.Initialized = true;
  }

  handle = CanL_GetHandle(port);

  if (handle) {
    ASLOG(WARN, ("CAN socket port=%d is already on-line, no need to probe it again!\n", port));
    rv = false;
  } else {
    sockRd = TcpIp_Create(TCPIP_IPPROTO_UDP);
    if (sockRd >= 0) {
      PortNum = CAN_PORT_MIN + port;
      ret = TcpIp_SetNonBlock(sockRd, FALSE);
      if (E_OK == ret) {
        ret = TcpIp_SetTimeout(sockRd, 10);
      }
      if (E_OK == ret) {
        ret = TcpIp_Bind(sockRd, 0, &PortNum);
      }
      if (E_OK == ret) {
        TcpIp_SetupAddrFrom(&ipv4Addr, CAN_CAST_IP, port);
        ret = TcpIp_AddToMulticast(sockRd, &ipv4Addr);
      }
      if (E_OK != ret) {
        TcpIp_Close(sockRd, TRUE);
        rv = FALSE;
        ASLOG(ERROR, ("CAN socket bind to %x:%d failed\n", CAN_CAST_IP, CAN_PORT_MIN + port));
      } else {
        (void)TcpIp_SetMulticastIF(sockRd, 0);
      }
    } else {
      rv = FALSE;
      ASLOG(ERROR, ("CAN socket create read sock failed\n"));
    }
    if (rv) {
      sockWt = TcpIp_Create(TCPIP_IPPROTO_UDP);
      if (sockWt < 0) {
        TcpIp_Close(sockRd, TRUE);
        TcpIp_Close(sockWt, TRUE);
        rv = FALSE;
        ASLOG(ERROR, ("CAN socket create write sock failed\n"));
      } else {
        uint16_t u16PortAny = TCPIP_PORT_ANY;
        TcpIp_Bind(sockWt, 0, &u16PortAny);
        (void)TcpIp_SetMulticastIF(sockWt, 0);
      }
    }
    if (rv) { /* open port OK */
      handle = new struct CanL_SocketHandleType;
      std::lock_guard<std::mutex> lck2(handle->mutex);
      assert(handle);
      handle->BusId = busId;
      handle->Port = port;
      handle->RxNotification = rxNotification;
      handle->sockRd = sockRd;
      handle->sockWt = sockWt;
      CanL_GetUuid(handle->uuid, sizeof(handle->uuid));
      ASHEXDUMP(DEBUG, ("uuid[%u]:", port), handle->uuid, sizeof(handle->uuid));
      handle->Terminated = false;

      handle->RxThread = std::thread(CanL_RxDaemon, handle);
      std::lock_guard<std::mutex> lck(socketH.mutex);
      STAILQ_INSERT_TAIL(&socketH.head, handle, entry);
    } else {
      rv = false;
    }
  }

  return rv;
}
static bool CanL_SocketWrite(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                             uint64_t timestamp) {
  bool rv = true;
  struct CanL_WireFrameType frame;
  TcpIp_SockAddrType RemoteAddr;
  Std_ReturnType ret;
  struct CanL_SocketHandleType *handle = CanL_GetHandle(port);
  if (handle != NULL) {
    frame.dlc = dlc;
    mSetTimeStamp(frame, timestamp);
    mSetCANID(frame, canid);
    assert(dlc <= CAN_MAX_DLEN);
    memcpy(frame.data, data, dlc);
    if (dlc < CAN_MAX_DLEN) {
      memset(&frame.data[dlc], 0x55, CAN_MAX_DLEN - dlc);
    }
    memcpy(frame.uuid, handle->uuid, sizeof(frame.uuid));
    TcpIp_SetupAddrFrom(&RemoteAddr, CAN_CAST_IP, CAN_PORT_MIN + handle->Port);
    ret = TcpIp_SendTo(handle->sockWt, &RemoteAddr, (const uint8_t *)&frame, CAN_MTU);
    if (E_OK != ret) {
      ASLOG(WARN, ("CAN socket port=%d send message failed!\n", port));
      rv = false;
    }
  } else {
    rv = false;
    ASLOG(WARN, ("CAN socket port=%d is not on-line, not able to send message!\n", port));
  }

  return rv;
}
static void CanL_SocketClose(uint32_t port) {
  struct CanL_SocketHandleType *handle = CanL_GetHandle(port);

  if (NULL != handle) {
    std::lock_guard<std::mutex> lck(socketH.mutex);
    handle->Terminated = true;
    if (handle->RxThread.joinable()) {
      handle->RxThread.join();
    }
    TcpIp_Close(handle->sockRd, TRUE);
    TcpIp_Close(handle->sockWt, TRUE);
    STAILQ_REMOVE(&socketH.head, handle, CanL_SocketHandleType, entry);
    delete handle;
  }
}

static void CanL_RxIndication(struct CanL_SocketHandleType *handle) {
  struct CanL_WireFrameType frame;
  TcpIp_SockAddrType RemoteAddr;
  uint32_t len = sizeof(frame);
  Std_ReturnType ret;
  do {
    std::lock_guard<std::mutex> lck(handle->mutex);
    ret = TcpIp_RecvFrom(handle->sockRd, &RemoteAddr, (uint8_t *)&frame, &len);
    if ((E_OK == ret) && (len == sizeof(frame))) {
      if (0 != memcmp(frame.uuid, handle->uuid, sizeof(frame.uuid))) {
        handle->RxNotification(handle->BusId, mCANID(frame), frame.dlc, frame.data,
                               mTimeStamp(frame));
      }
    } else {
      ret = E_NOT_OK;
    }
  } while (E_OK == ret);
}

static void CanL_SocketRead(uint32_t port) {
  struct CanL_SocketHandleType *handle = CanL_GetHandle(port);

  if (NULL != handle) {
    CanL_RxIndication(handle);
  }
}

static void CanL_RxDaemon(struct CanL_SocketHandleType *handle) {
  while (false == handle->Terminated) {
    CanL_RxIndication(handle);
    if (false == CanL_IsPerfMode()) {
      std::this_thread::sleep_for(1ms);
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace simulator_can_v2
