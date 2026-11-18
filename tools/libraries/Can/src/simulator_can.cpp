/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <mutex>
#include <thread>
#include <sys/queue.h>

#include "canlib.h"
#include "canlib_types.hpp"
#include "TcpIp.h"

using namespace std::literals::chrono_literals;
namespace simulator_can {
/* ================================ [ MACROS    ] ============================================== */
#define CAN_SOCKET_IP TCPIP_IPV4_ADDR(127, 0, 0, 1)

#define CAN_MAX_DLEN 64 /* 64 for CANFD */
#define CAN_MTU sizeof(struct CanL_WireFrameType)
#define CAN_PORT_MIN 8000

#define mCANID(frame)                                                                              \
  (((uint32_t)frame.data[CAN_MAX_DLEN + 0] << 24) +                                                \
   ((uint32_t)frame.data[CAN_MAX_DLEN + 1] << 16) +                                                \
   ((uint32_t)frame.data[CAN_MAX_DLEN + 2] << 8) + ((uint32_t)frame.data[CAN_MAX_DLEN + 3]))

#define mSetCANID(frame, canid)                                                                    \
  do {                                                                                             \
    frame.data[CAN_MAX_DLEN + 0] = (uint8_t)(canid >> 24);                                         \
    frame.data[CAN_MAX_DLEN + 1] = (uint8_t)(canid >> 16);                                         \
    frame.data[CAN_MAX_DLEN + 2] = (uint8_t)(canid >> 8);                                          \
    frame.data[CAN_MAX_DLEN + 3] = (uint8_t)(canid);                                               \
  } while (0)

#define mCANDLC(frame) ((uint8_t)frame.data[CAN_MAX_DLEN + 4])
#define mSetCANDLC(frame, dlc)                                                                     \
  do {                                                                                             \
    frame.data[CAN_MAX_DLEN + 4] = dlc;                                                            \
  } while (0)
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
  uint8_t data[CAN_MAX_DLEN + 5];
};
struct CanL_SocketHandleType {
  uint32_t BusId;
  uint32_t Port;
  uint32_t Baudrate;
  std::mutex mutex;
  std::thread RxThread;
  volatile bool Terminated;
  CanL_DeviceRxNotificationType RxNotification;
  TcpIp_SocketIdType s; /* can raw socket */
  STAILQ_ENTRY(CanL_SocketHandleType) entry;
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
static void CanL_SocketClose(uint32_t port);
static void CanL_RxDaemon(struct CanL_SocketHandleType *handle);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const CanL_DeviceOpsType CanL_SimulatorOps = {
  .Name = "simulator",
  .Probe = CanL_SocketProbe,
  .Close = CanL_SocketClose,
  .Write = CanL_SocketWrite,
};
static struct CanL_SocketHandleListType socketH = {
  .Initialized = FALSE,
};
/* ================================ [ LOCALS    ] ============================================== */
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
  bool rv = TRUE;
  struct CanL_SocketHandleType *handle;
  TcpIp_SocketIdType s;
  TcpIp_SockAddrType RemoteAddr;
  Std_ReturnType ercd;

  if (FALSE == socketH.Initialized) {
    STAILQ_INIT(&socketH.head);
    TcpIp_Init(NULL);
    socketH.Initialized = TRUE;
  }

  handle = CanL_GetHandle(port);

  if (handle) {
    ASLOG(WARN, ("CAN socket port=%d is already on-line, no need to probe it again!\n", port));
    rv = FALSE;
  } else {
    s = TcpIp_Create(TCPIP_IPPROTO_TCP);
    if (s >= 0) {
      (void)TcpIp_SetupAddrFrom(&RemoteAddr, CAN_SOCKET_IP, CAN_PORT_MIN + port);
      ercd = TcpIp_TcpConnect(s, &RemoteAddr);
      if (E_OK != ercd) {
        TcpIp_Close(s, TRUE);
        rv = FALSE;
      }
    } else {
      rv = FALSE;
    }

    if (rv) { /* open port OK */
      handle = new struct CanL_SocketHandleType;
      assert(handle);
      handle->BusId = busId;
      handle->Port = port;
      handle->Baudrate = baudrate;
      handle->RxNotification = rxNotification;
      handle->s = s;
      handle->Terminated = false;
      handle->RxThread = std::thread(CanL_RxDaemon, handle);
      std::lock_guard<std::mutex> lck(socketH.mutex);
      STAILQ_INSERT_TAIL(&socketH.head, handle, entry);
    } else {
      rv = FALSE;
    }
  }

  return rv;
}
static bool CanL_SocketWrite(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                             uint64_t timestamp) {
  bool rv = TRUE;
  struct CanL_SocketHandleType *handle = CanL_GetHandle(port);
  (void)timestamp;
  if (handle != NULL) {
    struct CanL_WireFrameType frame;
    mSetCANID(frame, canid);
    mSetCANDLC(frame, dlc);
    assert(dlc <= CAN_MAX_DLEN);
    memcpy(frame.data, data, dlc);
    if (TcpIp_Send(handle->s, (uint8_t *)&frame, CAN_MTU) != E_OK) {
      ASLOG(WARN, ("CAN Socket port=%d send message failed!\n", port));
      rv = FALSE;
    }
  } else {
    rv = FALSE;
    ASLOG(WARN, ("CAN Socket port=%d is not on-line, not able to send message!\n", port));
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
    TcpIp_Close(handle->s, TRUE);
    STAILQ_REMOVE(&socketH.head, handle, CanL_SocketHandleType, entry);
    delete handle;
  }
}

static void CanL_RxIndication(struct CanL_SocketHandleType *handle) {
  struct CanL_WireFrameType frame;
  uint32_t Length;
  Std_ReturnType ercd;
  do {
    std::lock_guard<std::mutex> lck(handle->mutex);
    Length = sizeof(frame);
    ercd = TcpIp_Recv(handle->s, (uint8_t *)&frame, &Length);
    if ((E_OK == ercd) && (Length == sizeof(frame))) {
      handle->RxNotification(handle->BusId, mCANID(frame), mCANDLC(frame), frame.data, 0);
    }

    if (E_OK != ercd) {
      ASLOG(WARN, ("CAN Socket port=%d read message failed with error %d!\n", handle->Port, ercd));
    }
  } while (sizeof(frame) == Length);
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
} // namespace simulator_can
