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
#define CAN_MTU sizeof(struct can_frame)
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
 * struct can_frame - basic CAN frame structure
 * @can_id:  CAN ID of the frame and CAN_*_FLAG flags, see canid_t definition
 * @can_dlc: frame payload length in byte (0 .. 8) aka data length code
 *           N.B. the DLC field from ISO 11898-1 Chapter 8.4.2.3 has a 1:1
 *           mapping of the 'data length code' to the real payload length
 * @data:    CAN frame payload (up to 8 byte)
 */
struct can_frame {
  uint8_t data[CAN_MAX_DLEN + 5];
};
struct Can_SocketHandle_s {
  uint32_t busid;
  uint32_t port;
  uint32_t baudrate;
  std::mutex mutex;
  std::thread rx_thread;
  volatile bool terminated;
  can_device_rx_notification_t rx_notification;
  TcpIp_SocketIdType s; /* can raw socket */
  STAILQ_ENTRY(Can_SocketHandle_s) entry;
};
struct Can_SocketHandleList_s {
  bool initialized;
  std::mutex mutex;
  STAILQ_HEAD(, Can_SocketHandle_s) head;
};
/* ================================ [ DECLARES  ] ============================================== */
static bool socket_probe(int busid, uint32_t port, uint32_t baudrate,
                         can_device_rx_notification_t rx_notification);
static bool socket_write(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                         uint64_t timestamp);
static void socket_close(uint32_t port);
static void rx_daemon(struct Can_SocketHandle_s *handle);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const Can_DeviceOpsType can_simulator_ops = {
  .name = "simulator",
  .probe = socket_probe,
  .close = socket_close,
  .write = socket_write,
};
static struct Can_SocketHandleList_s socketH = {
  .initialized = FALSE,
};
/* ================================ [ LOCALS    ] ============================================== */
static struct Can_SocketHandle_s *getHandle(uint32_t port) {
  struct Can_SocketHandle_s *handle, *h;
  handle = NULL;

  std::lock_guard<std::mutex> lck(socketH.mutex);
  STAILQ_FOREACH(h, &socketH.head, entry) {
    if (h->port == port) {
      handle = h;
      break;
    }
  }

  return handle;
}

static bool socket_probe(int busid, uint32_t port, uint32_t baudrate,
                         can_device_rx_notification_t rx_notification) {
  bool rv = TRUE;
  struct Can_SocketHandle_s *handle;
  TcpIp_SocketIdType s;
  TcpIp_SockAddrType RemoteAddr;
  Std_ReturnType ercd;

  if (FALSE == socketH.initialized) {
    STAILQ_INIT(&socketH.head);
    TcpIp_Init(NULL);
    socketH.initialized = TRUE;
  }

  handle = getHandle(port);

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
      handle = new struct Can_SocketHandle_s;
      assert(handle);
      handle->busid = busid;
      handle->port = port;
      handle->baudrate = baudrate;
      handle->rx_notification = rx_notification;
      handle->s = s;
      handle->terminated = false;
      handle->rx_thread = std::thread(rx_daemon, handle);
      std::lock_guard<std::mutex> lck(socketH.mutex);
      STAILQ_INSERT_TAIL(&socketH.head, handle, entry);
    } else {
      rv = FALSE;
    }
  }

  return rv;
}
static bool socket_write(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                         uint64_t timestamp) {
  bool rv = TRUE;
  struct Can_SocketHandle_s *handle = getHandle(port);
  (void)timestamp;
  if (handle != NULL) {
    struct can_frame frame;
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
static void socket_close(uint32_t port) {
  struct Can_SocketHandle_s *handle = getHandle(port);

  if (NULL != handle) {
    std::lock_guard<std::mutex> lck(socketH.mutex);
    handle->terminated = true;
    if (handle->rx_thread.joinable()) {
      handle->rx_thread.join();
    }
    TcpIp_Close(handle->s, TRUE);
    STAILQ_REMOVE(&socketH.head, handle, Can_SocketHandle_s, entry);
    delete handle;
  }
}

static void rx_notifiy(struct Can_SocketHandle_s *handle) {
  struct can_frame frame;
  uint32_t Length;
  Std_ReturnType ercd;
  do {
    std::lock_guard<std::mutex> lck(handle->mutex);
    Length = sizeof(frame);
    ercd = TcpIp_Recv(handle->s, (uint8_t *)&frame, &Length);
    if ((E_OK == ercd) && (Length == sizeof(frame))) {
      handle->rx_notification(handle->busid, mCANID(frame), mCANDLC(frame), frame.data, 0);
    }

    if (E_OK != ercd) {
      ASLOG(WARN, ("CAN Socket port=%d read message failed with error %d!\n", handle->port, ercd));
    }
  } while (sizeof(frame) == Length);
}

static void rx_daemon(struct Can_SocketHandle_s *handle) {
  while (false == handle->terminated) {
    rx_notifiy(handle);
    if (false == can_is_perf_mode()) {
      std::this_thread::sleep_for(1ms);
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace simulator_can
