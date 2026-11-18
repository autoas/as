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
  TPCANStatus (*CAN_InitializeFD)(TPCANHandle, TPCANBitrateFD);
  TPCANStatus (*CAN_WriteFD)(TPCANHandle, TPCANMsgFD *);
  TPCANStatus (*CAN_ReadFD)(TPCANHandle, TPCANMsgFD *, TPCANTimestampFD *);
  TPCANStatus (*CAN_Uninitialize)(TPCANHandle);
} Peak_InterfaceType;

struct CanL_PeakHandleListType {
  bool Initialized;
  std::mutex mutex;
  Peak_InterfaceType IF;
  STAILQ_HEAD(, CanL_PeakHandleType) head;
};

typedef struct {
  uint32_t Baudrate;
  const char *bitrate;
} Can_PeakBaudrate_t;
/* ================================ [ DECLARES  ] ============================================== */
static bool CanL_PeakProbe(int busId, uint32_t port, uint32_t baudrate,
                           CanL_DeviceRxNotificationType rxNotification);
static bool CanL_PeakWrite(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                           uint64_t timestamp);
static void CanL_PeakClose(uint32_t port);
static void CanL_PeakRead(uint32_t port);
static void CanL_RxDaemon(struct CanL_PeakHandleType *handle);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const CanL_DeviceOpsType CanL_PeakFdOps = {
  .Name = "peakfd",
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

/* refer: https://documentation.help/PCAN-Basic/CAN_InitializeFD.html
 * https://docs.peak-system.com/API/PCAN-Basic.Net/html/eebb7d25-f978-60f2-54b8-5b126db9dff5.htm
 * https://docs.peak-system.com/API/PCAN-Basic.Net/html/63345c55-9c4f-4f59-a2c4-b200cf71ed3a.htm */
static Can_PeakBaudrate_t peak_bauds[] = {
  /* Norminal BitRate 1M, Sample Point 75%; data BitRate 4M, Sample Point 80% */
  {1000 Kbps, "f_clock_mhz=80,nom_brp=10,nom_tseg1=5,nom_tseg2=2,nom_sjw=2,  "
              "data_brp=2,data_tseg1=2,data_tseg2=2,data_sjw=1"},
  /* Norminal BitRate 500K, Sample Point 80%; data BitRate 2M, Sample Point 80% */
  {500 Kbps, "f_clock_mhz=80,nom_brp=2,nom_tseg1=63,nom_tseg2=16,nom_sjw=16,"
             "data_brp=2,data_tseg1=15,data_tseg2=4,data_sjw=4"},
  /* Norminal BitRate 500K, Sample Point 87.5%; data BitRate 2M, Sample Point 83.3% */
  {500001, "f_clock_mhz=24,nom_brp=3,nom_tseg1=13,nom_tseg2=2,nom_sjw=2,"
           "data_brp=1,data_tseg1=9,data_tseg2=2,data_sjw=2"},
};

/* https://docs.peak-system.com/API/PCAN-Basic.Net/html/826d5a95-5e1c-9dad-8d5b-2bf4ebb7e455.htm */
static const uint8_t s_CanFdDLCs[] = {8, 12, 16, 20, 24, 32, 48, 64};
/* ================================ [ LOCALS    ] ============================================== */
static uint8_t CanL_GetFdDlc(uint8_t len, uint8_t &rlen) {
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
static bool CanL_GetPeakParam(uint32_t *port, uint32_t baudrate, const char **pBitrate) {
  uint32_t i;
  bool rv = true;

  if (*port < ARRAY_SIZE(peak_ports)) {
    *port = peak_ports[*port];
  } else {
    rv = false;
  }

  for (i = 0; i < ARRAY_SIZE(peak_bauds); i++) {
    if (baudrate == peak_bauds[i].Baudrate) {
      *pBitrate = peak_bauds[i].bitrate;
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

static bool CanL_PeakProbe(int busId, uint32_t port, uint32_t baudrate,
                           CanL_DeviceRxNotificationType rxNotification) {
  bool rv = true;
  struct CanL_PeakHandleType *handle;
  const char *peak_baud = NULL;
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
    rv = CanL_GetPeakParam(&peak_port, baudrate, &peak_baud);
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
  TPCANMsgFD msg;
  uint8_t rlen;
  struct CanL_PeakHandleType *handle = CanL_GetHandle(port);

  (void)timestamp;

  if (handle != NULL) {
    msg.ID = canid & 0x7FFFFFFFUL;
    msg.DLC = CanL_GetFdDlc(dlc, rlen);
    if (0 != (canid & CANL_ID_EXTENDED)) {
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

    status = PEAK_CALL(WriteFD)(handle->Channel, &msg);
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
  TPCANMsgFD msg;
  TPCANTimestampFD canTs = 0;
  TPCANStatus status;
  uint8_t rlen;
  do {
    std::lock_guard<std::mutex> lck(handle->mutex);
    status = PEAK_CALL(ReadFD)(handle->Channel, &msg, &canTs);
    if (PCAN_ERROR_OK == status) {
      if (CanL_IsPerfMode()) {
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
      handle->RxNotification(handle->BusId, msg.ID, rlen, msg.DATA, canTs);
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
