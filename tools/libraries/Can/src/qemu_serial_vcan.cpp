/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <mutex>
#include <thread>
#include <sys/queue.h>

#include "canlib.h"
#include "canlib_types.hpp"
#include "TcpIp.h"

using namespace std::literals::chrono_literals;
/* ================================ [ MACROS    ] ============================================== */
#define CAN_PORT_MIN 9000

#define CAN_MAX_DLEN 64 /* 64 for CANFD */
#define CAN_MTU (CAN_MAX_DLEN + 5)

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
struct CanL_WireFrameType {
  uint8_t data[CAN_MTU];
};

struct CanL_SerialHandleType {
  uint32_t BusId;
  uint32_t Port;
  int online;
  TcpIp_SocketIdType sock;
  std::mutex mutex;
  std::thread RxThread;
  volatile bool Terminated;
  CanL_DeviceRxNotificationType RxNotification;
  struct CanL_WireFrameType frame;
  int index;
  STAILQ_ENTRY(CanL_SerialHandleType) entry;
};
struct CanL_SerialHandleListType {
  bool Initialized;
  STAILQ_HEAD(, CanL_SerialHandleType) head;
  std::mutex mutex;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
static bool CanL_QemuProbe(int busId, uint32_t port, uint32_t baudrate,
                           CanL_DeviceRxNotificationType rxNotification);
static bool CanL_QemuWrite(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                           uint64_t timestamp);
static void CanL_QemuClose(uint32_t port);
static void CanL_RxDaemon(struct CanL_SerialHandleType *handle);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const CanL_DeviceOpsType CanL_QemuOps = {
  .Name = "qemu",
  .Probe = CanL_QemuProbe,
  .Close = CanL_QemuClose,
  .Write = CanL_QemuWrite,
};

static struct CanL_SerialHandleListType serialH = {
  .Initialized = false,
};
/* ================================ [ LOCALS    ] ============================================== */
static struct CanL_SerialHandleType *CanL_GetHandle(uint32_t port) {
  struct CanL_SerialHandleType *handle, *h;
  handle = NULL;

  std::lock_guard<std::mutex> lck(serialH.mutex);
  STAILQ_FOREACH(h, &serialH.head, entry) {
    if (h->Port == port) {
      handle = h;
      break;
    }
  }

  return handle;
}

static bool CanL_QemuProbe(int busId, uint32_t port, uint32_t baudrate,
                           CanL_DeviceRxNotificationType rxNotification) {
  bool rv = true;
  struct CanL_SerialHandleType *handle;
  TcpIp_SocketIdType sock;
  TcpIp_SockAddrType RemoteAddr;
  Std_ReturnType ret;

  if (false == serialH.Initialized) {
    STAILQ_INIT(&serialH.head);
    TcpIp_Init(NULL);
    serialH.Initialized = true;
  }

  handle = CanL_GetHandle(port);

  if (handle) {
    ASLOG(WARN, ("CAN qemu port=%d is already on-line, no need to probe it again!\n", port));
    rv = false;
  } else {
    sock = TcpIp_Create(TCPIP_IPPROTO_TCP);
    if (sock >= 0) {
      TcpIp_SetupAddrFrom(&RemoteAddr, TCPIP_IPV4_ADDR(127, 0, 0, 1), CAN_PORT_MIN + port);
      ret = TcpIp_TcpConnect(sock, &RemoteAddr);
      if (E_OK != ret) {
        TcpIp_Close(sock, TRUE);
        rv = FALSE;
        ASLOG(ERROR, ("CAN qemu connect to 127.0.0.1:%d failed\n", CAN_PORT_MIN + port));
      }
    } else {
      rv = FALSE;
      ASLOG(ERROR, ("CAN qemu create read sock failed\n"));
    }

    if (rv) { /* open port OK */
      handle = new struct CanL_SerialHandleType;
      assert(handle);
      handle->BusId = busId;
      handle->Port = port;
      handle->RxNotification = rxNotification;
      handle->sock = sock;
      handle->index = 0;
      handle->Terminated = false;
      handle->RxThread = std::thread(CanL_RxDaemon, handle);
      std::lock_guard<std::mutex> lck(serialH.mutex);
      STAILQ_INSERT_TAIL(&serialH.head, handle, entry);
    } else {
      rv = false;
    }
  }

  return rv;
}
static bool CanL_QemuWrite(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                           uint64_t timestamp) {
  bool rv = true;
  struct CanL_WireFrameType frame;
  Std_ReturnType ret;
  struct CanL_SerialHandleType *handle = CanL_GetHandle(port);
  (void)timestamp;
  if (handle != NULL) {
    mSetCANID(frame, canid);
    mSetCANDLC(frame, dlc);
    assert(dlc <= CAN_MAX_DLEN);
    memcpy(frame.data, data, dlc);
    std::lock_guard<std::mutex> lck(serialH.mutex);
    ret = TcpIp_Send(handle->sock, (const uint8_t *)&frame, CAN_MTU);
    if (E_OK != ret) {
      ASLOG(WARN, ("CAN qemu port=%d send message failed!\n", port));
      rv = false;
    }
  } else {
    rv = false;
    ASLOG(WARN, ("CAN qemu port=%d is not on-line, not able to send message!\n", port));
  }

  return rv;
}

static void CanL_QemuClose(uint32_t port) {
  struct CanL_SerialHandleType *handle = CanL_GetHandle(port);

  if (NULL != handle) {
    std::lock_guard<std::mutex> lck(serialH.mutex);
    handle->Terminated = true;
    if (handle->RxThread.joinable()) {
      handle->RxThread.join();
    }

    TcpIp_Close(handle->sock, TRUE);
    STAILQ_REMOVE(&serialH.head, handle, CanL_SerialHandleType, entry);
    delete handle;
  }
}

static void CanL_RxIndication(struct CanL_SerialHandleType *handle) {
  uint32_t len;
  uint8_t *data;
  Std_ReturnType ret;
  do {
    std::lock_guard<std::mutex> lck(handle->mutex);
    len = sizeof(handle->frame) - handle->index;
    data = &handle->frame.data[handle->index];
    ret = TcpIp_Recv(handle->sock, data, &len);
    if ((E_OK == ret) && (len > 0)) {
      handle->index += len;
    }
    if ((E_OK == ret) && (handle->index == sizeof(handle->frame))) {
      handle->index = 0;
      handle->RxNotification(handle->BusId, mCANID(handle->frame), mCANDLC(handle->frame),
                             handle->frame.data, 0);
    } else {
      ret = E_NOT_OK;
    }
  } while (E_OK == ret);
}

static void CanL_RxDaemon(struct CanL_SerialHandleType *handle) {
  while (false == handle->Terminated) {
    CanL_RxIndication(handle);
    if (false == CanL_IsPerfMode()) {
      std::this_thread::sleep_for(1ms);
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
