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
#define CAN_MTU sizeof(struct can_frame)

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
 * struct can_frame - basic CAN frame structure
 * @can_id:  CAN ID of the frame and CAN_*_FLAG flags, see canid_t definition
 * @can_dlc: frame payload length in byte (0 .. 8) aka data length code
 *           N.B. the DLC field from ISO 11898-1 Chapter 8.4.2.3 has a 1:1
 *           mapping of the 'data length code' to the real payload length
 * @data:    CAN frame payload (up to 8 byte)
 */
struct can_frame {
  uint8_t canid[4];
  uint8_t dlc;
  uint8_t data[CAN_MAX_DLEN];
  uint8_t timestamp[8];
  uint8_t uuid[CAN_UUID_LENGTH];
};
struct Can_SocketHandle_s {
  uint32_t busid;
  uint32_t port;
  std::mutex mutex;
  std::thread rx_thread;
  volatile bool terminated;
  can_device_rx_notification_t rx_notification;
  TcpIp_SocketIdType sockRd;
  TcpIp_SocketIdType sockWt;
  STAILQ_ENTRY(Can_SocketHandle_s) entry;
  uint8_t uuid[CAN_UUID_LENGTH];
};
struct Can_socketHandleList_s {
  bool initialized;
  std::mutex mutex;
  STAILQ_HEAD(, Can_SocketHandle_s) head;
};
/* ================================ [ DECLARES  ] ============================================== */
static bool socket_probe(int busid, uint32_t port, uint32_t baudrate,
                         can_device_rx_notification_t rx_notification);
static bool socket_write(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                         uint64_t timestamp);
static void socket_read(uint32_t port);
static void socket_close(uint32_t port);
static void rx_daemon(struct Can_SocketHandle_s *handle);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const Can_DeviceOpsType can_simulator_v2_ops = {
  .name = "simulator_v2",
  .probe = socket_probe,
  .close = socket_close,
  .write = socket_write,
  .read = socket_read,
};
static struct Can_socketHandleList_s socketH = {
  .initialized = false,
};
/* ================================ [ LOCALS    ] ============================================== */
#ifdef _WIN32
static void get_uuid(uint8_t *uuid, size_t length) {
  GUID guid;
  CoCreateGuid(&guid);
  memcpy(uuid, &guid, length);
}
#else
static void get_uuid(uint8_t *uuid, size_t length) {
  uuid_t uu;
  uuid_generate(uu);
  memcpy(uuid, uu, length);
}
#endif

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
  bool rv = true;
  struct Can_SocketHandle_s *handle;
  TcpIp_SocketIdType sockRd, sockWt;
  Std_ReturnType ret;
  TcpIp_SockAddrType ipv4Addr;
  uint16_t Port;

  if (false == socketH.initialized) {
    STAILQ_INIT(&socketH.head);
    TcpIp_Init(NULL);
    socketH.initialized = true;
  }

  handle = getHandle(port);

  if (handle) {
    ASLOG(WARN, ("CAN socket port=%d is already on-line, no need to probe it again!\n", port));
    rv = false;
  } else {
    sockRd = TcpIp_Create(TCPIP_IPPROTO_UDP);
    if (sockRd >= 0) {
      Port = CAN_PORT_MIN + port;
      ret = TcpIp_SetNonBlock(sockRd, FALSE);
      if (E_OK == ret) {
        ret = TcpIp_SetTimeout(sockRd, 10);
      }
      if (E_OK == ret) {
        ret = TcpIp_Bind(sockRd, TCPIP_LOCALADDRID_ANY, &Port);
      }
      if (E_OK == ret) {
        TcpIp_SetupAddrFrom(&ipv4Addr, CAN_CAST_IP, Port);
        ret = TcpIp_AddToMulticast(sockRd, &ipv4Addr);
      }
      if (E_OK != ret) {
        TcpIp_Close(sockRd, TRUE);
        rv = FALSE;
        ASLOG(ERROR, ("CAN socket bind to %x:%d failed\n", CAN_CAST_IP, CAN_PORT_MIN + port));
      } else {
        (void)TcpIp_SetMulticastIF(sockRd, TCPIP_LOCALADDRID_ANY);
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
        (void)TcpIp_SetMulticastIF(sockWt, TCPIP_LOCALADDRID_ANY);
      }
    }
    if (rv) { /* open port OK */
      handle = new struct Can_SocketHandle_s;
      std::lock_guard<std::mutex> lck2(handle->mutex);
      assert(handle);
      handle->busid = busid;
      handle->port = port;
      handle->rx_notification = rx_notification;
      handle->sockRd = sockRd;
      handle->sockWt = sockWt;
      get_uuid(handle->uuid, sizeof(handle->uuid));
      ASHEXDUMP(DEBUG, ("uuid[%u]:", port), handle->uuid, sizeof(handle->uuid));
      handle->terminated = false;

      handle->rx_thread = std::thread(rx_daemon, handle);
      std::lock_guard<std::mutex> lck(socketH.mutex);
      STAILQ_INSERT_TAIL(&socketH.head, handle, entry);
    } else {
      rv = false;
    }
  }

  return rv;
}
static bool socket_write(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                         uint64_t timestamp) {
  bool rv = true;
  struct can_frame frame;
  TcpIp_SockAddrType RemoteAddr;
  Std_ReturnType ret;
  struct Can_SocketHandle_s *handle = getHandle(port);
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
    TcpIp_SetupAddrFrom(&RemoteAddr, CAN_CAST_IP, CAN_PORT_MIN + handle->port);
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
static void socket_close(uint32_t port) {
  struct Can_SocketHandle_s *handle = getHandle(port);

  if (NULL != handle) {
    std::lock_guard<std::mutex> lck(socketH.mutex);
    handle->terminated = true;
    if (handle->rx_thread.joinable()) {
      handle->rx_thread.join();
    }
    TcpIp_Close(handle->sockRd, TRUE);
    TcpIp_Close(handle->sockWt, TRUE);
    STAILQ_REMOVE(&socketH.head, handle, Can_SocketHandle_s, entry);
    delete handle;
  }
}

static void rx_notifiy(struct Can_SocketHandle_s *handle) {
  struct can_frame frame;
  TcpIp_SockAddrType RemoteAddr;
  uint32_t len = sizeof(frame);
  Std_ReturnType ret;
  do {
    std::lock_guard<std::mutex> lck(handle->mutex);
    ret = TcpIp_RecvFrom(handle->sockRd, &RemoteAddr, (uint8_t *)&frame, &len);
    if ((E_OK == ret) && (len == sizeof(frame))) {
      if (0 != memcmp(frame.uuid, handle->uuid, sizeof(frame.uuid))) {
        handle->rx_notification(handle->busid, mCANID(frame), frame.dlc, frame.data,
                                mTimeStamp(frame));
      }
    } else {
      ret = E_NOT_OK;
    }
  } while (E_OK == ret);
}

static void socket_read(uint32_t port) {
  struct Can_SocketHandle_s *handle = getHandle(port);

  if (NULL != handle) {
    rx_notifiy(handle);
  }
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
} // namespace simulator_can_v2
