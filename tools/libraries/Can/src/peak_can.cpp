#ifdef _WIN32
/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include <sys/queue.h>
#include <mutex>
#include <thread>
#include <chrono>
#include "PAL.h"
#include <assert.h>
#include "Std_Debug.h"
#include "canlib.h"
#include "canlib_types.hpp"

using namespace std::literals::chrono_literals;

typedef unsigned long DWORD;
typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef float FLOAT;
typedef char *LPSTR;
typedef uint64_t UINT64;
#include "PCANBasic.h"

/* ================================ [ MACROS    ] ============================================== */
#define Kbps *1000

#ifdef USE_PEAK_DLL
#define PEAK_LOAD(sym)                                                                             \
  if (r) {                                                                                         \
    *(void **)&peakH.IF.CAN_##sym = PAL_DlSym(peakH.IF.dll, "CAN_" #sym);                          \
    if (NULL == peakH.IF.CAN_##sym) {                                                              \
      r = false;                                                                                   \
      ASLOG(ERROR, ("Can't find symbold " #sym "\n"));                                             \
    }                                                                                              \
  }
#define PEAK_CALL(sym) peakH.IF.CAN_##sym
#else
#define PEAK_LOAD(sym)
#define PEAK_CALL(sym) CAN_##sym
#endif
/* ================================ [ TYPES     ] ============================================== */
struct Can_PeakHandle_s {
  int busid;
  uint32_t port;
  uint32_t channel;
  uint32_t baudrate;
  uint64_t localTime;
  uint64_t devTime;
  can_device_rx_notification_t rx_notification;
  STAILQ_ENTRY(Can_PeakHandle_s) entry;
};

typedef struct {
  void *dll;
  TPCANStatus (*CAN_Initialize)(TPCANHandle, TPCANBaudrate, TPCANType, DWORD, WORD);
  TPCANStatus (*CAN_Write)(TPCANHandle, TPCANMsg *);
  TPCANStatus (*CAN_Read)(TPCANHandle, TPCANMsg *, TPCANTimestamp *);
  TPCANStatus (*CAN_Uninitialize)(TPCANHandle);
} Peak_InterfaceType;

struct Can_PeakHandleList_s {
  bool initialized;
  std::thread rx_thread;
  volatile bool terminated;
  std::mutex mutex;
  Peak_InterfaceType IF;
  STAILQ_HEAD(, Can_PeakHandle_s) head;
};
/* ================================ [ DECLARES  ] ============================================== */
static bool peak_probe(int busid, uint32_t port, uint32_t baudrate,
                       can_device_rx_notification_t rx_notification);
static bool peak_write(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                       uint64_t timestamp);
static void peak_close(uint32_t port);
static void rx_daemon(void *);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const Can_DeviceOpsType can_peak_ops = {
  .name = "peak",
  .probe = peak_probe,
  .close = peak_close,
  .write = peak_write,
};

static struct Can_PeakHandleList_s peakH = {
  .initialized = false,
  .terminated = false,
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
  std::lock_guard<std::mutex>(peakH.mutex);
  STAILQ_FOREACH(h, &peakH.head, entry) {
    if (h->port == port) {
      handle = h;
      break;
    }
  }
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

static bool openDriver(void) {
  bool r = true;
#ifdef USE_PEAK_DLL
  peakH.IF.dll = PAL_DlOpen("PCANBasic.dll");
  if (NULL == peakH.IF.dll) {
    r = false;
    ASLOG(ERROR, ("Can't open PCANBasic.dll\n"));
  }

  PEAK_LOAD(Initialize);
  PEAK_LOAD(Write);
  PEAK_LOAD(Read);
  PEAK_LOAD(Uninitialize);

  if (false == r) {
    if (NULL != peakH.IF.dll) {
      PAL_DlClose(peakH.IF.dll);
      peakH.IF.dll = NULL;
    }
  }
#endif
  return r;
}

static bool peak_probe(int busid, uint32_t port, uint32_t baudrate,
                       can_device_rx_notification_t rx_notification) {
  bool rv = true;
  struct Can_PeakHandle_s *handle;
  uint32_t peak_baud = baudrate;
  uint32_t peak_port = port;
  TPCANStatus status;

  if (false == peakH.initialized) {
    rv = openDriver();
    if (false == rv) {
      return false;
    }
    STAILQ_INIT(&peakH.head);

    peakH.initialized = true;
    peakH.terminated = true;
  }

  handle = getHandle(port);

  if (handle) {
    ASLOG(WARN, ("CAN PEAK port=%d is already on-line, no need to probe it again!\n", port));
    rv = false;
  } else {
    rv = get_peak_param(&peak_port, &peak_baud);
    if (rv) {
      status = PEAK_CALL(Initialize)(peak_port, peak_baud, 0, 0, 0);

      if (PCAN_ERROR_OK == status) {
        rv = true;
      } else {
        ASLOG(WARN, ("CAN PEAK port=%d is not able to be opened, error=0x%lX!\n", port, status));
        rv = false;
      }
    }
    if (rv) { /* open port OK */
      handle = new struct Can_PeakHandle_s;
      assert(handle);
      handle->busid = busid;
      handle->port = port;
      handle->channel = peak_port;
      handle->baudrate = baudrate;
      handle->rx_notification = rx_notification;
      handle->localTime = 0;
      handle->devTime = 0;
      std::lock_guard<std::mutex>(peakH.mutex);
      STAILQ_INSERT_TAIL(&peakH.head, handle, entry);
    } else {
      rv = false;
    }
  }

  if ((true == peakH.terminated) && (false == STAILQ_EMPTY(&peakH.head))) {
    peakH.terminated = false;
    peakH.rx_thread = std::thread(rx_daemon, nullptr);
  }

  return rv;
}

static bool peak_write(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                       uint64_t timestamp) {
  bool rv = true;
  TPCANStatus status;
  TPCANMsg msg;
  struct Can_PeakHandle_s *handle = getHandle(port);

  (void)timestamp;

  if (handle != NULL) {
    if (sizeof(msg.DATA) < dlc) {
      ASLOG(WARN, ("CAN PEAK port=%d request transmit message with dlc %u!\n", port, dlc));
      dlc = sizeof(msg.DATA);
    }
    msg.ID = canid & 0x7FFFFFFFUL;
    msg.LEN = dlc;
    if (0 != (canid & CAN_ID_EXTENDED)) {
      msg.MSGTYPE = PCAN_MESSAGE_EXTENDED;
    } else {
      msg.MSGTYPE = PCAN_MESSAGE_STANDARD;
    }

    memcpy(msg.DATA, data, dlc);

    status = PEAK_CALL(Write)(handle->channel, &msg);
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
    std::lock_guard<std::mutex>(peakH.mutex);
    STAILQ_REMOVE(&peakH.head, handle, Can_PeakHandle_s, entry);

    (void)PEAK_CALL(Uninitialize)(handle->channel);

    delete handle;

    if (true == STAILQ_EMPTY(&peakH.head)) {
      peakH.terminated = true;
      if (peakH.rx_thread.joinable()) {
        peakH.rx_thread.join();
      }
    }
  }
}

static void rx_notifiy(struct Can_PeakHandle_s *handle) {
  TPCANMsg msg;
  TPCANTimestamp canTs = {0};
  uint64_t timestamp;
  TPCANStatus status;
  do {
    status = PEAK_CALL(Read)(handle->channel, &msg, &canTs);
    if (PCAN_ERROR_OK == status) {
      if (can_is_perf_mode()) {
        timestamp = PAL_Timestamp();
      } else {
        timestamp = canTs.micros + (1000ULL * canTs.millis) +
                    (0x100000000ULL * 1000ULL * canTs.millis_overflow);
        if ((0 == handle->localTime) && (0 == handle->devTime)) {
          /* sync time but this is not accurate */
          handle->devTime = timestamp;
          handle->localTime = PAL_Timestamp();
        }
        timestamp = handle->localTime + (timestamp - handle->devTime);
      }
      handle->rx_notification(handle->busid, msg.ID, msg.LEN, msg.DATA, timestamp);
    } else if (PCAN_ERROR_QRCVEMPTY == status) {

    } else {
      ASLOG(WARN, ("CAN PEAK port=%d read message failed: error=0x%lX!\n", handle->port, status));
    }
  } while (PCAN_ERROR_OK == status);
}

static void rx_daemon(void *param) {
  (void)param;
  struct Can_PeakHandle_s *handle;
  while (false == peakH.terminated) {
    std::lock_guard<std::mutex>(peakH.mutex);
    STAILQ_FOREACH(handle, &peakH.head, entry) {
      rx_notifiy(handle);
    }
    if (false == can_is_perf_mode()) {
      std::this_thread::sleep_for(1ms);
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _WIN32 */
