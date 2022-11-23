#ifdef _WIN32
/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <windows.h>
#ifdef SLIST_ENTRY
#undef SLIST_ENTRY
#endif
#include <sys/queue.h>
#include <pthread.h>
#include <assert.h>

#include "PCANBasic.h"

#include "Std_Types.h"
#include "Std_Debug.h"
#include "canlib.h"
#include "canlib_types.h"
/* ================================ [ MACROS    ] ============================================== */
#define Kbps *1000
/* ================================ [ TYPES     ] ============================================== */
struct Can_PeakHandle_s {
  int busid;
  uint32_t port;
  uint32_t channel;
  uint32_t baudrate;
  can_device_rx_notification_t rx_notification;
  STAILQ_ENTRY(Can_PeakHandle_s) entry;
};
struct Can_PeakHandleList_s {
  bool initialized;
  pthread_t rx_thread;
  volatile bool terminated;
  pthread_mutex_t mutex;
  STAILQ_HEAD(, Can_PeakHandle_s) head;
};
/* ================================ [ DECLARES  ] ============================================== */
static bool peak_probe(int busid, uint32_t port, uint32_t baudrate,
                       can_device_rx_notification_t rx_notification);
static bool peak_write(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data);
static void peak_close(uint32_t port);
static void *rx_daemon(void *);
/* ================================ [ DATAS     ] ============================================== */
const Can_DeviceOpsType can_peak_ops = {
  .name = "peak",
  .probe = peak_probe,
  .close = peak_close,
  .write = peak_write,
};

static struct Can_PeakHandleList_s peakH = {
  .initialized = false,
  .terminated = false,
  .mutex = PTHREAD_MUTEX_INITIALIZER,
};

static uint32_t peak_ports[] = {
  PCAN_USBBUS1, PCAN_USBBUS2, PCAN_USBBUS3, PCAN_USBBUS4,
  PCAN_USBBUS5, PCAN_USBBUS6, PCAN_USBBUS7, PCAN_USBBUS8,
};

static uint32_t peak_bauds[][2] = {
  {1000 Kbps, PCAN_BAUD_1M},  {800 Kbps, PCAN_BAUD_800K}, {500 Kbps, PCAN_BAUD_500K},
  {250 Kbps, PCAN_BAUD_250K}, {100 Kbps, PCAN_BAUD_100K}, {95 Kbps, PCAN_BAUD_95K},
  {83 Kbps, PCAN_BAUD_83K},   {50 Kbps, PCAN_BAUD_50K},   {47 Kbps, PCAN_BAUD_47K},
  {33 Kbps, PCAN_BAUD_33K},   {20 Kbps, PCAN_BAUD_20K},   {10 Kbps, PCAN_BAUD_10K},
  {5 Kbps, PCAN_BAUD_5K}};
/* ================================ [ LOCALS    ] ============================================== */
static struct Can_PeakHandle_s *getHandle(uint32_t port) {
  struct Can_PeakHandle_s *handle, *h;
  handle = NULL;
  pthread_mutex_lock(&peakH.mutex);
  STAILQ_FOREACH(h, &peakH.head, entry) {
    if (h->port == port) {
      handle = h;
      break;
    }
  }
  pthread_mutex_unlock(&peakH.mutex);
  return handle;
}
/*
 * InOut: port, baudrate
 */
static bool get_peak_param(uint32_t *port, uint32_t *baudrate) {
  uint32_t i;
  bool rv = true;

  if (*port < ARRAY_SIZE(peak_ports)) {
    *port = peak_ports[*port];
  } else {
    rv = false;
  }

  for (i = 0; i < ARRAY_SIZE(peak_bauds); i++) {
    if (*baudrate == peak_bauds[i][0]) {
      *baudrate = peak_bauds[i][1];
      break;
    }
  }

  if (i >= ARRAY_SIZE(peak_bauds)) {
    rv = false;
  }

  return rv;
}

static bool peak_probe(int busid, uint32_t port, uint32_t baudrate,
                       can_device_rx_notification_t rx_notification) {
  bool rv = true;
  struct Can_PeakHandle_s *handle;
  uint32_t peak_baud = baudrate;
  uint32_t peak_port = port;
  TPCANStatus status;

  pthread_mutex_lock(&peakH.mutex);
  if (false == peakH.initialized) {
    STAILQ_INIT(&peakH.head);

    peakH.initialized = true;
    peakH.terminated = true;
  }
  pthread_mutex_unlock(&peakH.mutex);

  handle = getHandle(port);

  if (handle) {
    ASLOG(WARN, ("CAN PEAK port=%d is already on-line, no need to probe it again!\n", port));
    rv = false;
  } else {
    rv = get_peak_param(&peak_port, &peak_baud);
    if (rv) {
      status = CAN_Initialize(peak_port, peak_baud, 0, 0, 0);

      if (PCAN_ERROR_OK == status) {
        rv = true;
      } else {
        ASLOG(WARN, ("CAN PEAK port=%d is not able to be opened, error=0x%lX!\n", port, status));
        rv = false;
      }
    }
    if (rv) { /* open port OK */
      handle = malloc(sizeof(struct Can_PeakHandle_s));
      assert(handle);
      handle->busid = busid;
      handle->port = port;
      handle->channel = peak_port;
      handle->baudrate = baudrate;
      handle->rx_notification = rx_notification;
      pthread_mutex_lock(&peakH.mutex);
      STAILQ_INSERT_TAIL(&peakH.head, handle, entry);
      pthread_mutex_unlock(&peakH.mutex);
    } else {
      rv = false;
    }
  }

  if ((true == peakH.terminated) && (false == STAILQ_EMPTY(&peakH.head))) {
    if (0 == pthread_create(&(peakH.rx_thread), NULL, rx_daemon, NULL)) {
      peakH.terminated = false;
    } else {
      assert(0);
    }
  }

  return rv;
}

static bool peak_write(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data) {
  bool rv = true;
  TPCANStatus status;
  TPCANMsg msg;
  struct Can_PeakHandle_s *handle = getHandle(port);

  if (handle != NULL) {
    msg.ID = canid;
    msg.LEN = dlc;
    if (0 != (canid & 0x80000000)) {
      msg.MSGTYPE = PCAN_MESSAGE_STANDARD;
    } else {
      msg.MSGTYPE = PCAN_MESSAGE_EXTENDED;
    }

    memcpy(msg.DATA, data, dlc);

    status = CAN_Write(handle->channel, &msg);
    if (PCAN_ERROR_OK == status) {
      /* send OK */
    } else {
      rv = false;
      ASLOG(WARN, ("CAN PEAK port=%d send message failed: error=0x%lX!\n", port, status));
    }
  } else {
    rv = false;
    ASLOG(WARN, ("CAN Peak port=%d is not on-line, not able to send message!\n", port));
  }

  return rv;
}

static void peak_close(uint32_t port) {
  struct Can_PeakHandle_s *handle = getHandle(port);

  if (NULL != handle) {
    pthread_mutex_lock(&peakH.mutex);
    STAILQ_REMOVE(&peakH.head, handle, Can_PeakHandle_s, entry);
    pthread_mutex_unlock(&peakH.mutex);

    free(handle);

    if (true == STAILQ_EMPTY(&peakH.head)) {
      peakH.terminated = true;
      pthread_join(peakH.rx_thread, NULL);
    }
  }
}

static void rx_notifiy(struct Can_PeakHandle_s *handle) {
  TPCANMsg msg;
  TPCANStatus status;
  do {
    status = CAN_Read(handle->channel, &msg, NULL);

    if (PCAN_ERROR_OK == status) {
      handle->rx_notification(handle->busid, msg.ID, msg.LEN, msg.DATA);
    } else if (PCAN_ERROR_QRCVEMPTY == status) {

    } else {
      ASLOG(WARN, ("CAN PEAK port=%d read message failed: error=0x%lX!\n", handle->port, status));
    }
  } while (PCAN_ERROR_OK == status);
}

static void *rx_daemon(void *param) {
  (void)param;
  struct Can_PeakHandle_s *handle;
  while (false == peakH.terminated) {
    pthread_mutex_lock(&peakH.mutex);
    STAILQ_FOREACH(handle, &peakH.head, entry) {
      rx_notifiy(handle);
    }
    pthread_mutex_unlock(&peakH.mutex);
    usleep(1000);
  }

  return NULL;
}
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _WIN32 */
