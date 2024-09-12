/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2023 Parai Wang <parai@foxmail.com>
 */
#ifdef _WIN32
/* ================================ [ INCLUDES  ] ============================================== */
#include <sys/queue.h>
#include "osal.h"
#include <assert.h>
#include "osal.h"
#include "Std_Types.h"
#include "Std_Debug.h"
#include "canlib.h"
#include "canlib_types.h"

#include "ControlCAN.h"
/* ================================ [ MACROS    ] ============================================== */
#define Kbps *1000

#define AS_LOG_ZLG 1

#ifdef USE_ZLG_DLL
#define ZLG_LOAD(sym)                                                                              \
  if (r) {                                                                                         \
    *(void **)&zlgH.IF.VCI_##sym = OSAL_DlSym(zlgH.IF.dll, "VCI_" #sym);                           \
    if (NULL == zlgH.IF.VCI_##sym) {                                                               \
      r = false;                                                                                   \
      ASLOG(ERROR, ("Can't find symbold " #sym "\n"));                                             \
    }                                                                                              \
  }
#define ZLG_CALL(sym) zlgH.IF.VCI_##sym
#else
#define ZLG_LOAD(sym)
#define ZLG_CALL(sym) VCI_##sym
#endif
/* ================================ [ TYPES     ] ============================================== */
struct Can_ZLGHandle_s {
  uint32_t busid;
  uint32_t port;
  uint32_t DeviceType;
  uint32_t CANInd;
  uint32_t baudrate;
  can_device_rx_notification_t rx_notification;
  STAILQ_ENTRY(Can_ZLGHandle_s) entry;
};

typedef struct {
  void *dll;
  DWORD (*VCI_OpenDevice)(DWORD, DWORD, DWORD);
  DWORD (*VCI_CloseDevice)(DWORD, DWORD);
  DWORD (*VCI_InitCAN)(DWORD, DWORD, DWORD, PVCI_INIT_CONFIG);
  DWORD (*VCI_StartCAN)(DWORD, DWORD, DWORD);
  ULONG (*VCI_Transmit)(DWORD, DWORD, DWORD, PVCI_CAN_OBJ, ULONG);
  ULONG (*VCI_Receive)(DWORD, DWORD, DWORD, PVCI_CAN_OBJ, ULONG, INT);
  ULONG (*VCI_GetReceiveNum)(DWORD, DWORD, DWORD);
  DWORD (*VCI_ReadBoardInfo)(DWORD, DWORD, PVCI_BOARD_INFO);
  DWORD (*VCI_ReadErrInfo)(DWORD, DWORD, DWORD, PVCI_ERR_INFO);
} Zlg_InterfaceType;

struct Can_ZLGHandleList_s {
  bool initialized;
  OSAL_ThreadType rx_thread;
  volatile bool terminated;
  OSAL_MutexType mutex;
  Zlg_InterfaceType IF;
  STAILQ_HEAD(, Can_ZLGHandle_s) head;
};
/* ================================ [ DECLARES  ] ============================================== */
static bool zlg_probe(int busid, uint32_t port, uint32_t baudrate,
                      can_device_rx_notification_t rx_notification);
static bool zlg_write(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data);
static void zlg_close(uint32_t port);
static void rx_daemon(void *);
/* ================================ [ DATAS     ] ============================================== */
const Can_DeviceOpsType can_zlg_ops = {
  .name = "zlg",
  .probe = zlg_probe,
  .close = zlg_close,
  .write = zlg_write,
};

static uint32_t zlg_bauds[][2] = {
  {1000 Kbps, 0x0014}, {800 Kbps, 0x0016}, {666 Kbps, 0x80BA}, {500 Kbps, 0x001C},
  {400 Kbps, 0x80FA},  {250 Kbps, 0x011C}, {200 Kbps, 0x81FA}, {125 Kbps, 0x031C},
  {100 Kbps, 0x041C},  {80 Kbps, 0x83FF},  {50 Kbps, 0x091C},  {40 Kbps, 0x87FF},
  {20 Kbps, 0x181C},   {10 Kbps, 0x311C},  {5 Kbps, 0xBFFF},
};

static struct Can_ZLGHandleList_s zlgH = {
  .initialized = false,
  .terminated = false,
  .mutex = NULL,
};
/* ================================ [ LOCALS    ] ============================================== */
static bool openDriver(void) {
  bool r = true;
#ifdef USE_ZLG_DLL
  zlgH.IF.dll = OSAL_DlOpen("ControlCAN.dll");
  if (NULL == zlgH.IF.dll) {
    r = false;
    ASLOG(ERROR, ("Can't open ControlCAN.dll\n"));
  }

  ZLG_LOAD(OpenDevice);
  ZLG_LOAD(CloseDevice);
  ZLG_LOAD(InitCAN);
  ZLG_LOAD(StartCAN);
  ZLG_LOAD(Transmit);
  ZLG_LOAD(Receive);
  ZLG_LOAD(GetReceiveNum);
  ZLG_LOAD(ReadBoardInfo);
  ZLG_LOAD(ReadErrInfo);

  if (false == r) {
    if (NULL != zlgH.IF.dll) {
      OSAL_DlClose(zlgH.IF.dll);
      zlgH.IF.dll = NULL;
    }
  }
#endif
  return r;
}

static struct Can_ZLGHandle_s *getHandle(uint32_t port) {
  struct Can_ZLGHandle_s *handle, *h;
  handle = NULL;

  OSAL_MutexLock(zlgH.mutex);
  STAILQ_FOREACH(h, &zlgH.head, entry) {
    if (h->port == port) {
      handle = h;
      break;
    }
  }
  OSAL_MutexUnlock(zlgH.mutex);

  return handle;
}

static bool get_zlg_param(uint32_t port, uint32_t *DeviceType, uint32_t *CANInd,
                          uint32_t *baudrate) {
  uint32_t i;
  bool rv = true;

  char *pVCI_USBCAN = getenv("VCI_USBCAN");

  if (NULL != pVCI_USBCAN) {
    *DeviceType = strtoul(pVCI_USBCAN, NULL, 10);
    *CANInd = port;
    ASLOG(ZLG, ("open VCI_USBCAN %d port %d\n", *DeviceType, *CANInd));
  } else {
    ASLOG(
      WARN,
      ("please set env VCI_USBCAN according to the device type value specified in ConttrolCAN.h!\n"
       "  set VCI_USBCAN=4 REM for VCI_USBCAN2 (default)\n"
       "  set VCI_USBCAN=21 REM for VCI_USBCAN_2E_U\n"));
    *DeviceType = 4; /* default for VCI_USBCAN2 */
    *CANInd = port;
  }

  for (i = 0; i < ARRAY_SIZE(zlg_bauds); i++) {
    if (*baudrate == zlg_bauds[i][0]) {
      *baudrate = zlg_bauds[i][1];
      break;
    }
  }

  if (i >= ARRAY_SIZE(zlg_bauds)) {
    rv = false;
  }

  return rv;
}

static bool zlg_probe(int busid, uint32_t port, uint32_t baudrate,
                      can_device_rx_notification_t rx_notification) {
  bool rv = true;
  struct Can_ZLGHandle_s *handle;

  if (false == zlgH.initialized) {
    rv = openDriver();
    if (false == rv) {
      return false;
    }
    STAILQ_INIT(&zlgH.head);
    zlgH.initialized = true;
    zlgH.terminated = true;
    zlgH.mutex = OSAL_MutexCreate(NULL);
  }

  handle = getHandle(port);

  if (handle) {
    ASLOG(WARN, ("CAN ZLG port=%d is already on-line, no need to probe it again!\n", port));
    rv = false;
  } else {
    uint32_t DeviceType;
    uint32_t CANInd;
    uint32_t baud = baudrate;
    VCI_INIT_CONFIG init_config;
    uint32_t status;

    rv = get_zlg_param(port, &DeviceType, &CANInd, &baud);

    if (rv) {
      status = ZLG_CALL(OpenDevice)(DeviceType, 0, 0);

      if (STATUS_OK != status) {
        ASLOG(WARN, ("CAN ZLG port=%d is not able to be opened,error=%X!\n", port, status));
        ASLOG(WARN, ("maybe you forgot about the ControlCAN\\64\\kerneldlls to be copied to local "
                     "directory!\n"));
        rv = false;
      }
    }

    if (rv) {
      init_config.AccCode = 0x00000000;
      init_config.AccMask = 0xFFFFFFFF;
      init_config.Filter = 0;
      init_config.Mode = 0; /* normal mode */
      init_config.Timing0 = (UCHAR)(baud >> 8) & 0xFF;
      init_config.Timing1 = (UCHAR)baud & 0xFF;

      status = ZLG_CALL(InitCAN)(DeviceType, 0, CANInd, &init_config);
      if (STATUS_OK != status) {
        ASLOG(WARN, ("CAN ZLG port=%d is not able to be initialized,error=%X!\n", port, status));
        rv = false;
      }
    }

    if (rv) {
      VCI_BOARD_INFO info;
      status = ZLG_CALL(ReadBoardInfo)(DeviceType, 0, &info);
      if (STATUS_OK == status) {
        ASLOG(ZLG, ("%s %s version %d.%d.%d.%d.%d.%d\n", info.str_hw_Type, info.str_Serial_Num,
                    info.hw_Version, info.fw_Version, info.dr_Version, info.in_Version,
                    info.irq_Num, info.can_Num));
      }
    }

    if (rv) {
      status = ZLG_CALL(StartCAN)(DeviceType, 0, CANInd);
      if (STATUS_OK != status) {
        ASLOG(WARN, ("CAN ZLG port=%d is not able to be started,error=%X!\n", port, status));
        rv = false;
      }
    }

    if (rv) { /* open port OK */
      handle = malloc(sizeof(struct Can_ZLGHandle_s));
      assert(handle);
      handle->busid = busid;
      handle->port = port;
      handle->DeviceType = DeviceType;
      handle->CANInd = CANInd;
      handle->baudrate = baudrate;
      handle->rx_notification = rx_notification;
      OSAL_MutexLock(zlgH.mutex);
      STAILQ_INSERT_TAIL(&zlgH.head, handle, entry);
      OSAL_MutexUnlock(zlgH.mutex);
    } else {
      rv = false;
    }
  }

  if ((true == zlgH.terminated) && (false == STAILQ_EMPTY(&zlgH.head))) {
    zlgH.rx_thread = OSAL_ThreadCreate(rx_daemon, NULL);
    if (NULL != zlgH.rx_thread) {
      zlgH.terminated = false;
    } else {
      assert(0);
    }
  }

  return rv;
}
static bool zlg_write(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data) {
  bool rv = true;
  uint32_t status;
  struct Can_ZLGHandle_s *handle = getHandle(port);

  if (handle != NULL) {
    VCI_CAN_OBJ msg;
    msg.SendType = 0;
    msg.ID = canid & 0x7FFFFFFFUL;
    msg.DataLen = dlc;
    if (0 != (canid & CAN_ID_EXTENDED)) {
      msg.ExternFlag = 1;
    } else {
      msg.ExternFlag = 0;
    }
    msg.RemoteFlag = 0;

    memcpy(msg.Data, data, dlc);

    status = ZLG_CALL(Transmit)(handle->DeviceType, 0, handle->CANInd, &msg, 1);
    if (STATUS_OK == status) {
      /* send OK */
    } else {
      rv = false;
      ASLOG(WARN, ("CAN ZLG port=%d send message failed: error = %X!\n", port, status));
    }
  } else {
    rv = false;
    ASLOG(WARN, ("CAN ZLG port=%d is not on-line, not able to send message!\n", port));
  }

  return rv;
}
static void zlg_close(uint32_t port) {
  struct Can_ZLGHandle_s *handle = getHandle(port);

  if (NULL != handle) {
    OSAL_MutexLock(zlgH.mutex);
    STAILQ_REMOVE(&zlgH.head, handle, Can_ZLGHandle_s, entry);
    OSAL_MutexUnlock(zlgH.mutex);
    if (true == STAILQ_EMPTY(&zlgH.head)) {
      ZLG_CALL(CloseDevice)(handle->DeviceType, 0);
      zlgH.terminated = true;
      OSAL_ThreadJoin(zlgH.rx_thread);
      OSAL_ThreadDestory(zlgH.rx_thread);
      OSAL_MutexDestory(zlgH.mutex);
    }
    free(handle);
  }
}

static void rx_notifiy(struct Can_ZLGHandle_s *handle) {
  VCI_CAN_OBJ msg;
  VCI_ERR_INFO info;
  uint32_t dwRel;
  uint32_t status;

  do {
    dwRel = ZLG_CALL(GetReceiveNum)(handle->DeviceType, 0, handle->CANInd);

    if (dwRel > 0) {
      status = ZLG_CALL(Receive)(handle->DeviceType, 0, handle->CANInd, &msg, 1, 0);

      if (STATUS_OK == status) {
        handle->rx_notification(handle->busid, msg.ID, msg.DataLen, msg.Data);
      } else {
        ASLOG(WARN, ("CAN ZLG port=%d read message failed: error = %X!\n", handle->port, status));
      }
    }
  } while (dwRel > 0);

  status = ZLG_CALL(ReadErrInfo)(handle->DeviceType, 0, handle->CANInd, &info);
  if ((STATUS_OK == status) && (0 != info.ErrCode)) {
    ASLOG(ZLG, ("ZLG error for %d: 0x%X\n", handle->CANInd, info.ErrCode));
  }
}
static void rx_daemon(void *param) {
  (void)param;
  struct Can_ZLGHandle_s *handle;
  while (false == zlgH.terminated) {
    OSAL_MutexLock(zlgH.mutex);
    STAILQ_FOREACH(handle, &zlgH.head, entry) {
      rx_notifiy(handle);
    }
    OSAL_MutexUnlock(zlgH.mutex);
    OSAL_SleepUs(1000);
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _WIN32 */
