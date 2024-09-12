/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifdef _WIN32
/* ================================ [ INCLUDES  ] ============================================== */
#include "osal.h"
#include <string.h>
#include <assert.h>
#include <sys/queue.h>
#include "Std_Debug.h"
#include "Std_Types.h"
#include "canlib.h"
#include "canlib_types.h"

#define POINTER_32
typedef void *HANDLE;

#include "vxlapi.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
struct Can_VxlHandle_s {
  XLportHandle xlHandle;
  XLaccess xlAccess;
  XLdriverConfig xlDrvConfig;
  uint32_t busid;
  uint32_t port;
  uint32_t baudrate;
  can_device_rx_notification_t rx_notification;
  STAILQ_ENTRY(Can_VxlHandle_s) entry;
};

struct Can_VxlHandleList_s {
  bool initialized;
  OSAL_ThreadType rx_thread;
  volatile bool terminated;
  OSAL_MutexType mutex;
  STAILQ_HEAD(, Can_VxlHandle_s) head;
};
/* ================================ [ DECLARES  ] ============================================== */
static bool vxl_probe(int busid, uint32_t port, uint32_t baudrate,
                      can_device_rx_notification_t rx_notification);
static bool vxl_write(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data);
static void vxl_close(uint32_t port);
static void rx_daemon(void *);
/* ================================ [ DATAS     ] ============================================== */
const Can_DeviceOpsType can_vxl_ops = {
  .name = "vxl",
  .probe = vxl_probe,
  .close = vxl_close,
  .write = vxl_write,
};

static struct Can_VxlHandleList_s vxlH = {
  .initialized = false,
  .terminated = false,
  .mutex = NULL,
};
/* ================================ [ LOCALS    ] ============================================== */

static struct Can_VxlHandle_s *getHandle(uint32_t port) {
  struct Can_VxlHandle_s *handle, *h;
  handle = NULL;
  OSAL_MutexLock(vxlH.mutex);
  STAILQ_FOREACH(h, &vxlH.head, entry) {
    if (h->port == port) {
      handle = h;
      break;
    }
  }
  OSAL_MutexUnlock(vxlH.mutex);
  return handle;
}

static void vxlPrintConfig(XLdriverConfig *xlDrvConfig) {

  unsigned int i;
  char str[100];

  printf("----------------------------------------------------------\n");
  printf("- %02d channels       Hardware Configuration               -\n",
         xlDrvConfig->channelCount);
  printf("----------------------------------------------------------\n");

  for (i = 0; i < xlDrvConfig->channelCount; i++) {

    printf("- Ch:%02d, CM:0x%03llx,", xlDrvConfig->channel[i].channelIndex,
           xlDrvConfig->channel[i].channelMask);

    strncpy_s(str, 100, xlDrvConfig->channel[i].name, 23);
    printf(" %23s,", str);

    memset(str, 0, sizeof(str));

    if (xlDrvConfig->channel[i].transceiverType != XL_TRANSCEIVER_TYPE_NONE) {
      strncpy_s(str, 100, xlDrvConfig->channel[i].transceiverName, 13);
      printf("%13s -\n", str);
    } else {
      printf("    no Cab!   -\n");
    }
  }

  printf("----------------------------------------------------------\n\n");
}

static bool open_vxl(struct Can_VxlHandle_s *handle) {
  char userName[32];
  XLstatus status;
  XLaccess accessMask;

  status = xlGetDriverConfig(&handle->xlDrvConfig);

  if (XL_SUCCESS == status) {
    vxlPrintConfig(&handle->xlDrvConfig);
  } else {
    ASLOG(WARN, ("CAN VXL get driver config error<%d>: %s\n", status, xlGetErrorString(status)));
    return false;
  }

  sprintf(userName, "port%d", (int)handle->port);
  accessMask = 1 << handle->port;
  handle->xlAccess = accessMask;
  status = xlOpenPort(&handle->xlHandle, userName, accessMask, &handle->xlAccess, 512,
                      XL_INTERFACE_VERSION, XL_BUS_TYPE_CAN);
  if (XL_SUCCESS != status) {
    ASLOG(WARN, ("CAN VXL open port error<%d>: %s\n", status, xlGetErrorString(status)));
    return false;
  }

  status = xlCanSetChannelBitrate(handle->xlHandle, handle->xlAccess, handle->baudrate);
  if (XL_SUCCESS != status) {
    ASLOG(WARN, ("CAN VXL open error<%d>: %s\n", status, xlGetErrorString(status)));
    return false;
  }

  status =
    xlActivateChannel(handle->xlHandle, handle->xlAccess, XL_BUS_TYPE_CAN, XL_ACTIVATE_RESET_CLOCK);
  if (XL_SUCCESS != status) {
    ASLOG(WARN, ("CAN VXL open error<%d>: %s\n", status, xlGetErrorString(status)));
    return false;
  }

  return true;
}
static bool vxl_probe(int busid, uint32_t port, uint32_t baudrate,
                      can_device_rx_notification_t rx_notification) {
  bool rv = true;
  struct Can_VxlHandle_s *handle;

  if (false == vxlH.initialized) {
    XLstatus status = xlOpenDriver();

    if (XL_SUCCESS != status) {
      if (xlGetErrorString != NULL) {
        ASLOG(WARN, ("CAN VXL open error<%d>: %s\n", status, xlGetErrorString(status)));
      }
      return false;
    }

    STAILQ_INIT(&vxlH.head);

    vxlH.initialized = true;
    vxlH.terminated = true;
    vxlH.mutex = OSAL_MutexCreate(NULL);
  }

  handle = getHandle(port);

  if (handle) {
    ASLOG(WARN, ("CAN VXL port=%d is already on-line, no need to probe it again!\n", port));
    rv = false;
  } else {
    handle = malloc(sizeof(struct Can_VxlHandle_s));
    assert(handle);
    handle->busid = busid;
    handle->port = port;
    handle->baudrate = baudrate;
    handle->rx_notification = rx_notification;
    if (open_vxl(handle)) { /* open port OK */
      OSAL_MutexLock(vxlH.mutex);
      STAILQ_INSERT_TAIL(&vxlH.head, handle, entry);
      OSAL_MutexUnlock(vxlH.mutex);
    } else {
      free(handle);
      ASLOG(WARN, ("CAN VXL port=%d is is not able to be opened!\n", port));
      rv = false;
    }
  }

  if ((true == vxlH.terminated) && (false == STAILQ_EMPTY(&vxlH.head))) {
    vxlH.rx_thread = OSAL_ThreadCreate(rx_daemon, NULL);
    if (NULL != vxlH.rx_thread) {
      vxlH.terminated = false;
    } else {
      assert(0);
    }
  }

  return rv;
}

static bool vxl_write(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data) {
  bool rv = true;
  struct Can_VxlHandle_s *handle = getHandle(port);

  if (handle != NULL) {
    XLstatus status;
    unsigned int EventCount = 1;
    if (dlc > 8) {
      XLcanTxEvent Event;
      Event.tag = XL_TRANSMIT_MSG;
      Event.tagData.canMsg.canId = canid;
      Event.tagData.canMsg.msgFlags = 0;
      memcpy(Event.tagData.canMsg.data, data, dlc);
      Event.tagData.canMsg.dlc = dlc;
      status = xlCanTransmitEx(handle->xlHandle, handle->xlAccess, EventCount, &EventCount, &Event);
    } else {
      XLevent Event;
      Event.tag = XL_TRANSMIT_MSG;
      Event.tagData.msg.id = canid;
      Event.tagData.msg.flags = 0;
      memcpy(Event.tagData.msg.data, data, dlc);
      Event.tagData.msg.dlc = dlc;
      status = xlCanTransmit(handle->xlHandle, handle->xlAccess, &EventCount, &Event);
    }
    if (XL_SUCCESS != status) {
      rv = false;
      ASLOG(WARN, ("CAN VXL port=%d send message failed: %s!\n", port, xlGetErrorString(status)));
    }
  } else {
    rv = false;
    ASLOG(WARN, ("CAN Vxl port=%d is not on-line, not able to send message!\n", port));
  }

  return rv;
}

static void vxl_close(uint32_t port) {
  struct Can_VxlHandle_s *handle = getHandle(port);

  if (NULL != handle) {
    OSAL_MutexLock(vxlH.mutex);
    STAILQ_REMOVE(&vxlH.head, handle, Can_VxlHandle_s, entry);
    OSAL_MutexUnlock(vxlH.mutex);

    free(handle);

    if (true == STAILQ_EMPTY(&vxlH.head)) {
      vxlH.terminated = true;
      OSAL_ThreadJoin(vxlH.rx_thread);
      OSAL_ThreadDestory(vxlH.rx_thread);
      OSAL_MutexDestory(vxlH.mutex);
    }
  }
}

static void rx_notifiy(struct Can_VxlHandle_s *handle) {
  char *string;
  XLstatus status;
  unsigned int EventCount = 1;
  uint32_t i;
  XLevent Event;
  uint32_t port, time, canid, dlc, tid, u32V;
  uint8_t data[8];
  char sdata[32];
  do {
    status = xlReceive(handle->xlHandle, &EventCount, &Event);
    if (XL_ERR_QUEUE_IS_EMPTY == status) {
      return;
    }

    if (XL_SUCCESS != status) {
      ASLOG(WARN, ("CAN VXL port=%d receive message failed: %s!\n", handle->port,
                   xlGetErrorString(status)));
      break;
    }
    string = xlGetEventString(&Event);
    if (NULL != strstr(string, "ERROR_FRAME")) {
      ASLOG(WARN, ("%s!\n", string));
    } else if (NULL != strstr(string, "RX_MSG")) {
      /* RX_MSG c=0, t=222, id=0510 l=8, 0000000000000000 tid=00 */
      sscanf(string, "RX_MSG c=%d, t=%d, id=%X l=%d, %s tid=%d", &port, &time, &canid, &dlc, sdata,
             &tid);
      assert((dlc * 2) <= strlen(sdata));
      for (i = 0; i < dlc; i++) {
        sscanf(&sdata[2 * i], "%2x", &u32V);
        data[i] = (uint8_t)u32V;
      }
      handle->rx_notification(handle->busid, canid, dlc, data);
    } else {
      ASLOG(WARN, ("CAN VXL port=%d receive unknown message: '%s'!\n", handle->port, string));
    }
  } while (XL_SUCCESS == status);
}
static void rx_daemon(void *param) {
  (void)param;
  struct Can_VxlHandle_s *handle;

  while (false == vxlH.terminated) {
    OSAL_MutexLock(vxlH.mutex);
    STAILQ_FOREACH(handle, &vxlH.head, entry) {
      rx_notifiy(handle);
    }
    OSAL_MutexUnlock(vxlH.mutex);
    OSAL_SleepUs(1000);
  }

  xlCloseDriver();
}

/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _WIN32 */
