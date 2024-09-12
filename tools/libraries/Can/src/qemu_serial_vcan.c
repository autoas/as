/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "osal.h"
#include <sys/queue.h>

#include "canlib.h"
#include "canlib_types.h"
#include "TcpIp.h"
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
struct can_frame {
  uint8_t data[CAN_MTU];
};

struct Can_SerialHandle_s {
  uint32_t busid;
  uint32_t port;
  int online;
  TcpIp_SocketIdType sock;
  can_device_rx_notification_t rx_notification;
  struct can_frame frame;
  int index;
  STAILQ_ENTRY(Can_SerialHandle_s) entry;
};
struct Can_SerialHandleList_s {
  bool initialized;
  OSAL_ThreadType rx_thread;
  volatile boolean terminated;
  STAILQ_HEAD(, Can_SerialHandle_s) head;
  OSAL_MutexType mutex;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
static bool qs_probe(int busid, uint32_t port, uint32_t baudrate,
                     can_device_rx_notification_t rx_notification);
static bool qs_write(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data);
static void qs_close(uint32_t port);
static void rx_daemon(void *);
/* ================================ [ DATAS     ] ============================================== */
const Can_DeviceOpsType can_qs_ops = {
  .name = "qemu",
  .probe = qs_probe,
  .close = qs_close,
  .write = qs_write,
};

static struct Can_SerialHandleList_s serialH = {
  .initialized = false,
  .terminated = false,
  .mutex = NULL,
};
/* ================================ [ LOCALS    ] ============================================== */
static struct Can_SerialHandle_s *getHandle(uint32_t port) {
  struct Can_SerialHandle_s *handle, *h;
  handle = NULL;

  OSAL_MutexLock(serialH.mutex);
  STAILQ_FOREACH(h, &serialH.head, entry) {
    if (h->port == port) {
      handle = h;
      break;
    }
  }
  OSAL_MutexUnlock(serialH.mutex);

  return handle;
}

static bool qs_probe(int busid, uint32_t port, uint32_t baudrate,
                     can_device_rx_notification_t rx_notification) {
  bool rv = true;
  struct Can_SerialHandle_s *handle;
  TcpIp_SocketIdType sock;
  TcpIp_SockAddrType RemoteAddr;
  Std_ReturnType ret;

  if (false == serialH.initialized) {
    STAILQ_INIT(&serialH.head);
    TcpIp_Init(NULL);
    serialH.initialized = true;
    serialH.terminated = true;
    serialH.mutex = OSAL_MutexCreate(NULL);
  }

  handle = getHandle(port);

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
      handle = malloc(sizeof(struct Can_SerialHandle_s));
      assert(handle);
      handle->busid = busid;
      handle->port = port;
      handle->rx_notification = rx_notification;
      handle->sock = sock;
      handle->index = 0;
      OSAL_MutexLock(serialH.mutex);
      STAILQ_INSERT_TAIL(&serialH.head, handle, entry);
      OSAL_MutexUnlock(serialH.mutex);
    } else {
      rv = false;
    }
  }

  OSAL_MutexLock(serialH.mutex);
  if ((true == serialH.terminated) && (false == STAILQ_EMPTY(&serialH.head))) {
    serialH.rx_thread = OSAL_ThreadCreate(rx_daemon, NULL);
    if (NULL != serialH.rx_thread) {
      serialH.terminated = false;
    } else {
      assert(0);
    }
  }
  OSAL_MutexUnlock(serialH.mutex);

  return rv;
}
static bool qs_write(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data) {
  bool rv = true;
  struct can_frame frame;
  Std_ReturnType ret;
  struct Can_SerialHandle_s *handle = getHandle(port);
  if (handle != NULL) {
    mSetCANID(frame, canid);
    mSetCANDLC(frame, dlc);
    assert(dlc <= CAN_MAX_DLEN);
    memcpy(frame.data, data, dlc);
    OSAL_MutexLock(serialH.mutex);
    ret = TcpIp_Send(handle->sock, (const uint8_t *)&frame, CAN_MTU);
    OSAL_MutexUnlock(serialH.mutex);
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

static void qs_close(uint32_t port) {
  struct Can_SerialHandle_s *handle = getHandle(port);

  if (NULL != handle) {
    OSAL_MutexLock(serialH.mutex);
    STAILQ_REMOVE(&serialH.head, handle, Can_SerialHandle_s, entry);
    OSAL_MutexUnlock(serialH.mutex);
    TcpIp_Close(handle->sock, TRUE);
    free(handle);

    if (true == STAILQ_EMPTY(&serialH.head)) {
      serialH.terminated = true;
      OSAL_ThreadJoin(serialH.rx_thread);
      OSAL_ThreadDestory(serialH.rx_thread);
      OSAL_MutexDestory(serialH.mutex);
    }
  }
}

static void rx_notifiy(struct Can_SerialHandle_s *handle) {
  uint32_t len;
  uint8_t *data;
  Std_ReturnType ret;
  do {
    len = sizeof(handle->frame) - handle->index;
    data = &handle->frame.data[handle->index];
    ret = TcpIp_Recv(handle->sock, data, &len);
    if ((E_OK == ret) && (len > 0)) {
      handle->index += len;
    }
    if ((E_OK == ret) && (handle->index == sizeof(handle->frame))) {
      handle->index = 0;
      handle->rx_notification(handle->busid, mCANID(handle->frame), mCANDLC(handle->frame),
                              handle->frame.data);
    } else {
      ret = E_NOT_OK;
    }
  } while (E_OK == ret);
}

static void rx_daemon(void *param) {
  (void)param;
  struct Can_SerialHandle_s *handle;
  while (false == serialH.terminated) {
    OSAL_MutexLock(serialH.mutex);
    STAILQ_FOREACH(handle, &serialH.head, entry) {
      rx_notifiy(handle);
    }
    OSAL_MutexUnlock(serialH.mutex);
    OSAL_SleepUs(1000);
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */