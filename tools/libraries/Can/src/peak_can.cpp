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
struct CanL_PeakHandleType {
  int BusId;
  uint32_t Port;
  uint32_t Channel;
  uint32_t Baudrate;
  uint64_t localTime;
  uint64_t devTime;
  std::mutex mutex;
  std::thread RxThread;
  volatile bool Terminated;
  CanL_DeviceRxNotificationType RxNotification;
  STAILQ_ENTRY(CanL_PeakHandleType) entry;
};

typedef struct {
  void *dll;
  TPCANStatus (*CAN_Initialize)(TPCANHandle, TPCANBaudrate, TPCANType, DWORD, WORD);
  TPCANStatus (*CAN_Write)(TPCANHandle, TPCANMsg *);
  TPCANStatus (*CAN_Read)(TPCANHandle, TPCANMsg *, TPCANTimestamp *);
  TPCANStatus (*CAN_Uninitialize)(TPCANHandle);
} Peak_InterfaceType;

struct CanL_PeakHandleListType {
  bool Initialized;
  std::mutex mutex;
  Peak_InterfaceType IF;
  STAILQ_HEAD(, CanL_PeakHandleType) head;
};
/* ================================ [ DECLARES  ] ============================================== */
static bool CanL_PeakProbe(int busId, uint32_t port, uint32_t baudrate,
                           CanL_DeviceRxNotificationType rxNotification);
static bool CanL_PeakWrite(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                           uint64_t timestamp);
static void CanL_PeakClose(uint32_t port);
static void CanL_PeakRead(uint32_t port);
static void CanL_RxDaemon(struct CanL_PeakHandleType *handle);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const CanL_DeviceOpsType CanL_PeakOps = {
  .Name = "peak",
  .Probe = CanL_PeakProbe,
  .Close = CanL_PeakClose,
  .Write = CanL_PeakWrite,
  .Read = CanL_PeakRead,
};

static struct CanL_PeakHandleListType peakH = {
  .Initialized = false,
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
static struct CanL_PeakHandleType *CanL_GetHandle(uint32_t port) {
  struct CanL_PeakHandleType *handle, *h;
  handle = NULL;
  std::lock_guard<std::mutex> lck(peakH.mutex);
  STAILQ_FOREACH(h, &peakH.head, entry) {
    if (h->Port == port) {
      handle = h;
      break;
    }
  }
  return handle;
}
/*
 * InOut: port, baudrate
 */
static bool CanL_GetPeakParam(uint32_t *port, uint32_t *baudrate) {
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

static bool CanL_OpenDriver(void) {
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

static bool CanL_PeakProbe(int busId, uint32_t port, uint32_t baudrate,
                           CanL_DeviceRxNotificationType rxNotification) {
  bool rv = true;
  struct CanL_PeakHandleType *handle;
  uint32_t peak_baud = baudrate;
  uint32_t peak_port = port;
  TPCANStatus status;

  if (false == peakH.Initialized) {
    rv = CanL_OpenDriver();
    if (false == rv) {
      return false;
    }
    STAILQ_INIT(&peakH.head);

    peakH.Initialized = true;
  }

  handle = CanL_GetHandle(port);

  if (handle) {
    ASLOG(WARN, ("CAN PEAK port=%d is already on-line, no need to probe it again!\n", port));
    rv = false;
  } else {
    rv = CanL_GetPeakParam(&peak_port, &peak_baud);
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
      handle = new struct CanL_PeakHandleType;
      assert(handle);
      handle->BusId = busId;
      handle->Port = port;
      handle->Channel = peak_port;
      handle->Baudrate = baudrate;
      handle->RxNotification = rxNotification;
      handle->localTime = 0;
      handle->devTime = 0;
      handle->Terminated = false;
      handle->RxThread = std::thread(CanL_RxDaemon, handle);
      std::lock_guard<std::mutex> lck(peakH.mutex);
      STAILQ_INSERT_TAIL(&peakH.head, handle, entry);
    } else {
      rv = false;
    }
  }

  return rv;
}

static bool CanL_PeakWrite(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                           uint64_t timestamp) {
  bool rv = true;
  TPCANStatus status;
  TPCANMsg msg;
  struct CanL_PeakHandleType *handle = CanL_GetHandle(port);

  (void)timestamp;

  if (handle != NULL) {
    if (sizeof(msg.DATA) < dlc) {
      ASLOG(WARN, ("CAN PEAK port=%d request transmit message with dlc %u!\n", port, dlc));
      dlc = sizeof(msg.DATA);
    }
    msg.ID = canid & 0x7FFFFFFFUL;
    msg.LEN = dlc;
    if (0 != (canid & CANL_ID_EXTENDED)) {
      msg.MSGTYPE = PCAN_MESSAGE_EXTENDED;
    } else {
      msg.MSGTYPE = PCAN_MESSAGE_STANDARD;
    }

    memcpy(msg.DATA, data, dlc);

    status = PEAK_CALL(Write)(handle->Channel, &msg);
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

static void CanL_PeakClose(uint32_t port) {
  struct CanL_PeakHandleType *handle = CanL_GetHandle(port);

  if (NULL != handle) {
    std::lock_guard<std::mutex> lck(peakH.mutex);
    handle->Terminated = true;
    if (handle->RxThread.joinable()) {
      handle->RxThread.join();
    }
    (void)PEAK_CALL(Uninitialize)(handle->Channel);
    STAILQ_REMOVE(&peakH.head, handle, CanL_PeakHandleType, entry);
    delete handle;
  }
}

static void CanL_RxIndication(struct CanL_PeakHandleType *handle) {
  TPCANMsg msg;
  TPCANTimestamp canTs = {0};
  uint64_t timestamp;
  TPCANStatus status;
  do {
    std::lock_guard<std::mutex> lck(handle->mutex);
    status = PEAK_CALL(Read)(handle->Channel, &msg, &canTs);
    if (PCAN_ERROR_OK == status) {
      if (CanL_IsPerfMode()) {
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
      handle->RxNotification(handle->BusId, msg.ID, msg.LEN, msg.DATA, timestamp);
    } else if (PCAN_ERROR_QRCVEMPTY == status) {

    } else {
      ASLOG(WARN, ("CAN PEAK port=%d read message failed: error=0x%lX!\n", handle->Port, status));
    }
  } while (PCAN_ERROR_OK == status);
}

static void CanL_PeakRead(uint32_t port) {
  struct CanL_PeakHandleType *handle = CanL_GetHandle(port);

  if (NULL != handle) {
    CanL_RxIndication(handle);
  }
}

static void CanL_RxDaemon(struct CanL_PeakHandleType *handle) {
  while (false == handle->Terminated) {
    CanL_RxIndication(handle);
    if (false == CanL_IsPerfMode()) {
      std::this_thread::sleep_for(1ms);
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _WIN32 */
