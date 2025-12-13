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

typedef unsigned long DWORD;
typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef float FLOAT;
typedef char *LPSTR;
typedef uint64_t UINT64;
#include "PCANBasic.h"

using namespace std::literals::chrono_literals;
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
  std::mutex mutex;
  std::thread rx_thread;
  volatile bool terminated;
  can_device_rx_notification_t rx_notification;
  STAILQ_ENTRY(Can_PeakHandle_s) entry;
};

typedef struct {
  void *dll;
  TPCANStatus (*CAN_InitializeFD)(TPCANHandle, TPCANBitrateFD);
  TPCANStatus (*CAN_WriteFD)(TPCANHandle, TPCANMsgFD *);
  TPCANStatus (*CAN_ReadFD)(TPCANHandle, TPCANMsgFD *, TPCANTimestampFD *);
  TPCANStatus (*CAN_Uninitialize)(TPCANHandle);
} Peak_InterfaceType;

struct Can_PeakHandleList_s {
  bool initialized;
  std::mutex mutex;
  Peak_InterfaceType IF;
  STAILQ_HEAD(, Can_PeakHandle_s) head;
};

typedef struct {
  uint32_t baudrate;
  const char *bitrate;
} Can_PeakBaudrate_t;
/* ================================ [ DECLARES  ] ============================================== */
static bool peak_probe(int busid, uint32_t port, uint32_t baudrate,
                       can_device_rx_notification_t rx_notification);
static bool peak_write(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                       uint64_t timestamp);
static void peak_close(uint32_t port);
static void peak_read(uint32_t port);
static void rx_daemon(struct Can_PeakHandle_s *handle);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const Can_DeviceOpsType can_peakfd_ops = {
  .name = "peakfd",
  .probe = peak_probe,
  .close = peak_close,
  .write = peak_write,
  .read = peak_read,
};

static struct Can_PeakHandleList_s peakH = {
  .initialized = false,
};

static uint32_t peak_ports[] = {
  PCAN_USBBUS1, PCAN_USBBUS2, PCAN_USBBUS3, PCAN_USBBUS4,
  PCAN_USBBUS5, PCAN_USBBUS6, PCAN_USBBUS7, PCAN_USBBUS8,
};

/* refer: https://documentation.help/PCAN-Basic/CAN_InitializeFD.html
 * https://docs.peak-system.com/API/PCAN-Basic.Net/html/eebb7d25-f978-60f2-54b8-5b126db9dff5.htm
 * https://docs.peak-system.com/API/PCAN-Basic.Net/html/63345c55-9c4f-4f59-a2c4-b200cf71ed3a.htm */
static Can_PeakBaudrate_t peak_bauds[] = {
  /* Norminal BitRate 1M, Sample Point 75%; Data BitRate 4M, Sample Point 80% */
  {1000 Kbps, "f_clock_mhz=80,nom_brp=10,nom_tseg1=5,nom_tseg2=2,nom_sjw=2,  "
              "data_brp=2,data_tseg1=2,data_tseg2=2,data_sjw=1"},
  /* Norminal BitRate 500K, Sample Point 80%; Data BitRate 2M, Sample Point 80% */
  {500 Kbps, "f_clock_mhz=80,nom_brp=2,nom_tseg1=63,nom_tseg2=16,nom_sjw=16,"
             "data_brp=2,data_tseg1=15,data_tseg2=4,data_sjw=4"},
  /* Norminal BitRate 500K, Sample Point 87.5%; Data BitRate 2M, Sample Point 83.3% */
  {500001, "f_clock_mhz=24,nom_brp=3,nom_tseg1=13,nom_tseg2=2,nom_sjw=2,"
           "data_brp=1,data_tseg1=9,data_tseg2=2,data_sjw=2"},
};

/* https://docs.peak-system.com/API/PCAN-Basic.Net/html/826d5a95-5e1c-9dad-8d5b-2bf4ebb7e455.htm */
static const uint8_t s_CanFdDLCs[] = {8, 12, 16, 20, 24, 32, 48, 64};
/* ================================ [ LOCALS    ] ============================================== */
static uint8_t getFdDLC(uint8_t len, uint8_t &rlen) {
  uint8_t dl = (uint8_t)len;
  size_t i;
  rlen = len;

  if (len > 8) {
    for (i = 0; i < ARRAY_SIZE(s_CanFdDLCs); i++) {
      if (len <= s_CanFdDLCs[i]) {
        dl = (uint8_t)8 + i;
        rlen = s_CanFdDLCs[i];
        break;
      }
    }
  }

  return dl;
}

static struct Can_PeakHandle_s *getHandle(uint32_t port) {
  struct Can_PeakHandle_s *handle, *h;
  handle = NULL;
  std::lock_guard<std::mutex> lck(peakH.mutex);
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
static bool get_peak_param(uint32_t *port, uint32_t baudrate, const char **pBitrate) {
  uint32_t i;
  bool rv = true;

  if (*port < ARRAY_SIZE(peak_ports)) {
    *port = peak_ports[*port];
  } else {
    rv = false;
  }

  for (i = 0; i < ARRAY_SIZE(peak_bauds); i++) {
    if (baudrate == peak_bauds[i].baudrate) {
      *pBitrate = peak_bauds[i].bitrate;
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

  PEAK_LOAD(InitializeFD);
  PEAK_LOAD(WriteFD);
  PEAK_LOAD(ReadFD);
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
  const char *peak_baud = NULL;
  uint32_t peak_port = port;
  TPCANStatus status;

  if (false == peakH.initialized) {
    rv = openDriver();
    if (false == rv) {
      return false;
    }
    STAILQ_INIT(&peakH.head);

    peakH.initialized = true;
  }

  handle = getHandle(port);

  if (handle) {
    ASLOG(WARN, ("CAN PEAK port=%d is already on-line, no need to probe it again!\n", port));
    rv = false;
  } else {
    rv = get_peak_param(&peak_port, baudrate, &peak_baud);
    if (rv) {
      status = PEAK_CALL(InitializeFD)(peak_port, (TPCANBitrateFD)peak_baud);

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
      handle->terminated = false;
      handle->rx_thread = std::thread(rx_daemon, handle);
      std::lock_guard<std::mutex> lck(peakH.mutex);
      STAILQ_INSERT_TAIL(&peakH.head, handle, entry);
    } else {
      rv = false;
    }
  }

  return rv;
}

static bool peak_write(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                       uint64_t timestamp) {
  bool rv = true;
  TPCANStatus status;
  TPCANMsgFD msg;
  uint8_t rlen;
  struct Can_PeakHandle_s *handle = getHandle(port);

  (void)timestamp;

  if (handle != NULL) {
    msg.ID = canid & 0x7FFFFFFFUL;
    msg.DLC = getFdDLC(dlc, rlen);
    if (0 != (canid & CAN_ID_EXTENDED)) {
      msg.MSGTYPE = PCAN_MESSAGE_EXTENDED | PCAN_MESSAGE_FD;
    } else {
      msg.MSGTYPE = PCAN_MESSAGE_STANDARD | PCAN_MESSAGE_FD;
    }

    if (rlen > 8) {
      msg.MSGTYPE |= PCAN_MESSAGE_BRS;
    }

    memcpy(msg.DATA, data, dlc);
    for (size_t i = dlc; i < rlen; i++) {
      msg.DATA[i] = 0;
    }

    status = PEAK_CALL(WriteFD)(handle->channel, &msg);
    if (PCAN_ERROR_OK == status) {
      /* send OK */
    } else {
      rv = false;
      ASLOG(WARN, ("CAN PEAK port=%d send message failed: error=0x%lX! DLC=%d rlen=%d\n", port,
                   status, (int)msg.DLC, (int)rlen));
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
    std::lock_guard<std::mutex> lck(peakH.mutex);
    handle->terminated = true;
    if (handle->rx_thread.joinable()) {
      handle->rx_thread.join();
    }
    (void)PEAK_CALL(Uninitialize)(handle->channel);
    STAILQ_REMOVE(&peakH.head, handle, Can_PeakHandle_s, entry);
    delete handle;
  }
}

static void rx_notifiy(struct Can_PeakHandle_s *handle) {
  TPCANMsgFD msg;
  TPCANTimestampFD canTs = 0;
  TPCANStatus status;
  uint8_t rlen;
  do {
    std::lock_guard<std::mutex> lck(handle->mutex);
    status = PEAK_CALL(ReadFD)(handle->channel, &msg, &canTs);
    if (PCAN_ERROR_OK == status) {
      if (can_is_perf_mode()) {
        canTs = PAL_Timestamp();
      } else {
        if ((0 == handle->localTime) && (0 == handle->devTime)) {
          /* sync time but this is not accurate */
          handle->devTime = canTs;
          handle->localTime = PAL_Timestamp();
        }
        canTs = handle->localTime + (canTs - handle->devTime);
      }
      if (msg.DLC <= 8) {
        rlen = msg.DLC;
      } else if (msg.DLC <= 15) {
        rlen = s_CanFdDLCs[msg.DLC - 8];
      } else {
        rlen = msg.DLC;
      }
      handle->rx_notification(handle->busid, msg.ID, rlen, msg.DATA, canTs);
    } else if (PCAN_ERROR_QRCVEMPTY == status) {

    } else {
      ASLOG(WARN, ("CAN PEAK port=%d read message failed: error=0x%lX!\n", handle->port, status));
    }
  } while (PCAN_ERROR_OK == status);
}

static void peak_read(uint32_t port) {
  struct Can_PeakHandle_s *handle = getHandle(port);

  if (NULL != handle) {
    rx_notifiy(handle);
  }
}

static void rx_daemon(struct Can_PeakHandle_s *handle) {
  while (false == handle->terminated) {
    rx_notifiy(handle);
    if (false == can_is_perf_mode()) {
      std::this_thread::sleep_for(1ms);
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _WIN32 */
