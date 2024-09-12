#ifdef _WIN32
/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include <sys/queue.h>
#include "osal.h"
#include <assert.h>
#include "Std_Debug.h"
#include "canlib.h"
#include "canlib_types.h"

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
    *(void **)&peakH.IF.CAN_##sym = OSAL_DlSym(peakH.IF.dll, "CAN_" #sym);                         \
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
  OSAL_ThreadType rx_thread;
  volatile bool terminated;
  OSAL_MutexType mutex;
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
static bool peak_write(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data);
static void peak_close(uint32_t port);
static void rx_daemon(void *);
/* ================================ [ DATAS     ] ============================================== */
const Can_DeviceOpsType can_peakfd_ops = {
  .name = "peakfd",
  .probe = peak_probe,
  .close = peak_close,
  .write = peak_write,
};

static struct Can_PeakHandleList_s peakH = {
  .initialized = false,
  .terminated = false,
  .mutex = NULL,
};

static uint32_t peak_ports[] = {
  PCAN_USBBUS1, PCAN_USBBUS2, PCAN_USBBUS3, PCAN_USBBUS4,
  PCAN_USBBUS5, PCAN_USBBUS6, PCAN_USBBUS7, PCAN_USBBUS8,
};

/* refer: https://documentation.help/PCAN-Basic/CAN_InitializeFD.html
 * https://docs.peak-system.com/API/PCAN-Basic.Net/html/eebb7d25-f978-60f2-54b8-5b126db9dff5.htm
 * https://docs.peak-system.com/API/PCAN-Basic.Net/html/63345c55-9c4f-4f59-a2c4-b200cf71ed3a.htm */
static Can_PeakBaudrate_t peak_bauds[] = {
  /* Defines a FD Bit rate string with nominal and data Bit rate set to 1 MB */
  {1000 Kbps, "f_clock_mhz=24, nom_brp=1, nom_tseg1=17, nom_tseg2=6, nom_sjw=1, data_brp=1, "
              "data_tseg1=16, data_tseg2=7, data_sjw=1"},
  /* Defines a FD Bit rate string with nominal bit rate of 500 kBit/s and data bit rate
   * of 2 MB (SAE J2284-4) */
  {500 Kbps, "f_clock=80000000,nom_brp=2,nom_tseg1=63,nom_tseg2=16,nom_sjw=16,data_brp=2,data_"
             "tseg1=15,data_tseg2=4,data_sjw=4"}};
/* ================================ [ LOCALS    ] ============================================== */
static struct Can_PeakHandle_s *getHandle(uint32_t port) {
  struct Can_PeakHandle_s *handle, *h;
  handle = NULL;
  OSAL_MutexLock(peakH.mutex);
  STAILQ_FOREACH(h, &peakH.head, entry) {
    if (h->port == port) {
      handle = h;
      break;
    }
  }
  OSAL_MutexUnlock(peakH.mutex);
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
  peakH.IF.dll = OSAL_DlOpen("PCANBasic.dll");
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
      OSAL_DlClose(peakH.IF.dll);
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
    peakH.terminated = true;
    peakH.mutex = OSAL_MutexCreate(NULL);
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
      handle = malloc(sizeof(struct Can_PeakHandle_s));
      assert(handle);
      handle->busid = busid;
      handle->port = port;
      handle->channel = peak_port;
      handle->baudrate = baudrate;
      handle->rx_notification = rx_notification;
      OSAL_MutexLock(peakH.mutex);
      STAILQ_INSERT_TAIL(&peakH.head, handle, entry);
      OSAL_MutexUnlock(peakH.mutex);
    } else {
      rv = false;
    }
  }

  if ((true == peakH.terminated) && (false == STAILQ_EMPTY(&peakH.head))) {
    peakH.rx_thread = OSAL_ThreadCreate(rx_daemon, NULL);
    if (NULL != peakH.rx_thread) {
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
  TPCANMsgFD msg;
  struct Can_PeakHandle_s *handle = getHandle(port);

  if (handle != NULL) {
    msg.ID = canid;
    msg.DLC = dlc;
    if (0 != (canid & CAN_ID_EXTENDED)) {
      msg.MSGTYPE = PCAN_MESSAGE_EXTENDED;
    } else {
      msg.MSGTYPE = PCAN_MESSAGE_STANDARD;
    }

    memcpy(msg.DATA, data, dlc);

    status = PEAK_CALL(WriteFD)(handle->channel, &msg);
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
    OSAL_MutexLock(peakH.mutex);
    STAILQ_REMOVE(&peakH.head, handle, Can_PeakHandle_s, entry);
    OSAL_MutexUnlock(peakH.mutex);

    (void)PEAK_CALL(Uninitialize)(handle->channel);

    free(handle);

    if (true == STAILQ_EMPTY(&peakH.head)) {
      peakH.terminated = true;
      OSAL_ThreadJoin(peakH.rx_thread);
      OSAL_ThreadDestory(peakH.rx_thread);
      OSAL_MutexDestory(peakH.mutex);
    }
  }
}

static void rx_notifiy(struct Can_PeakHandle_s *handle) {
  TPCANMsgFD msg;
  TPCANStatus status;
  do {
    status = PEAK_CALL(ReadFD)(handle->channel, &msg, NULL);

    if (PCAN_ERROR_OK == status) {
      handle->rx_notification(handle->busid, msg.ID, msg.DLC, msg.DATA);
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
    OSAL_MutexLock(peakH.mutex);
    STAILQ_FOREACH(handle, &peakH.head, entry) {
      rx_notifiy(handle);
    }
    OSAL_MutexUnlock(peakH.mutex);
    OSAL_SleepUs(1000);
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _WIN32 */
