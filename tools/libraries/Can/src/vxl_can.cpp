/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifdef _WIN32
/* ================================ [ INCLUDES  ] ============================================== */
#include <mutex>
#include <thread>
#include <string.h>
#include <assert.h>
#include <sys/queue.h>
#include "Std_Debug.h"
#include "Std_Types.h"
#include "canlib.h"
#include "canlib_types.hpp"

#define POINTER_32
typedef void *HANDLE;

#include "vxlapi.h"

using namespace std::literals::chrono_literals;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
struct CanL_VxlHandleType {
  XLportHandle xlHandle;
  XLaccess xlAccess;
  XLdriverConfig xlDrvConfig;
  uint32_t BusId;
  uint32_t Port;
  uint32_t Baudrate;
  std::mutex mutex;
  std::thread RxThread;
  volatile bool Terminated;
  CanL_DeviceRxNotificationType RxNotification;
  STAILQ_ENTRY(CanL_VxlHandleType) entry;
};

struct CanL_VxlHandleListType {
  bool Initialized;
  std::mutex mutex;
  STAILQ_HEAD(, CanL_VxlHandleType) head;
};
/* ================================ [ DECLARES  ] ============================================== */
static bool CanL_VxlProbe(int busId, uint32_t port, uint32_t baudrate,
                          CanL_DeviceRxNotificationType rxNotification);
static bool CanL_VxlWrite(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                          uint64_t timestamp);
static void CanL_VxlClose(uint32_t port);
static void CanL_VxlRead(uint32_t port);
static void CanL_RxDaemon(struct CanL_VxlHandleType *handle);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const CanL_DeviceOpsType CanL_VxlOps = {
  .Name = "vxl",
  .Probe = CanL_VxlProbe,
  .Close = CanL_VxlClose,
  .Write = CanL_VxlWrite,
  .Read = CanL_VxlRead,
};

static struct CanL_VxlHandleListType vxlH = {
  .Initialized = false,
};
/* ================================ [ LOCALS    ] ============================================== */

static struct CanL_VxlHandleType *CanL_GetHandle(uint32_t port) {
  struct CanL_VxlHandleType *handle, *h;
  handle = NULL;
  std::lock_guard<std::mutex> lck(vxlH.mutex);
  STAILQ_FOREACH(h, &vxlH.head, entry) {
    if (h->Port == port) {
      handle = h;
      break;
    }
  }
  return handle;
}

static void CanL_VxlPrintConfig(XLdriverConfig *xlDrvConfig) {

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

static bool CanL_OpenVxl(struct CanL_VxlHandleType *handle) {
  char userName[32];
  XLstatus status;
  XLaccess accessMask;

  status = xlGetDriverConfig(&handle->xlDrvConfig);

  if (XL_SUCCESS == status) {
    CanL_VxlPrintConfig(&handle->xlDrvConfig);
  } else {
    ASLOG(WARN, ("CAN VXL get driver config error<%d>: %s\n", status, xlGetErrorString(status)));
    return false;
  }

  sprintf(userName, "port%d", (int)handle->Port);
  accessMask = 1 << handle->Port;
  handle->xlAccess = accessMask;
  status = xlOpenPort(&handle->xlHandle, userName, accessMask, &handle->xlAccess, 512,
                      XL_INTERFACE_VERSION, XL_BUS_TYPE_CAN);
  if (XL_SUCCESS != status) {
    ASLOG(WARN, ("CAN VXL open port error<%d>: %s\n", status, xlGetErrorString(status)));
    return false;
  }

  status = xlCanSetChannelBitrate(handle->xlHandle, handle->xlAccess, handle->Baudrate);
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
static bool CanL_VxlProbe(int busId, uint32_t port, uint32_t baudrate,
                          CanL_DeviceRxNotificationType rxNotification) {
  bool rv = true;
  struct CanL_VxlHandleType *handle;

  if (false == vxlH.Initialized) {
    XLstatus status = xlOpenDriver();

    if (XL_SUCCESS != status) {
      if (xlGetErrorString != NULL) {
        ASLOG(WARN, ("CAN VXL open error<%d>: %s\n", status, xlGetErrorString(status)));
      }
      return false;
    }

    STAILQ_INIT(&vxlH.head);

    vxlH.Initialized = true;
  }

  handle = CanL_GetHandle(port);

  if (handle) {
    ASLOG(WARN, ("CAN VXL port=%d is already on-line, no need to probe it again!\n", port));
    rv = false;
  } else {
    handle = new struct CanL_VxlHandleType;
    assert(handle);
    handle->BusId = busId;
    handle->Port = port;
    handle->Baudrate = baudrate;
    handle->RxNotification = rxNotification;
    if (CanL_OpenVxl(handle)) { /* open port OK */
      handle->Terminated = false;
      handle->RxThread = std::thread(CanL_RxDaemon, handle);
      std::lock_guard<std::mutex> lck(vxlH.mutex);
      STAILQ_INSERT_TAIL(&vxlH.head, handle, entry);
    } else {
      delete handle;
      ASLOG(WARN, ("CAN VXL port=%d is is not able to be opened!\n", port));
      rv = false;
    }
  }

  return rv;
}

static bool CanL_VxlWrite(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data,
                          uint64_t timestamp) {
  bool rv = true;
  struct CanL_VxlHandleType *handle = CanL_GetHandle(port);
  (void)timestamp;

  if (handle != NULL) {
    XLstatus status;
    unsigned int EventCount = 1;
    if (dlc > 8) {
      XLcanTxEvent Event;
      if (sizeof(Event.tagData.canMsg.data) < dlc) {
        ASLOG(WARN, ("CAN VXL port=%d request transmit message with dlc %u!\n", port, dlc));
        dlc = sizeof(Event.tagData.canMsg.data);
      }
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

static void CanL_VxlClose(uint32_t port) {
  struct CanL_VxlHandleType *handle = CanL_GetHandle(port);

  if (NULL != handle) {
    std::lock_guard<std::mutex> lck(vxlH.mutex);
    handle->Terminated = true;
    if (handle->RxThread.joinable()) {
      handle->RxThread.join();
    }
    STAILQ_REMOVE(&vxlH.head, handle, CanL_VxlHandleType, entry);

    delete handle;

    if (true == STAILQ_EMPTY(&vxlH.head)) {
      xlCloseDriver();
    }
  }
}

static void CanL_RxIndication(struct CanL_VxlHandleType *handle) {
  char *string;
  XLstatus status;
  unsigned int EventCount = 1;
  uint32_t i;
  XLevent Event;
  uint32_t port, time, canid, dlc, tid, u32V;
  uint8_t data[8];
  char sdata[32];
  do {
    std::lock_guard<std::mutex> lck(handle->mutex);
    status = xlReceive(handle->xlHandle, &EventCount, &Event);
    if (XL_ERR_QUEUE_IS_EMPTY == status) {
      return;
    }

    if (XL_SUCCESS != status) {
      ASLOG(WARN, ("CAN VXL port=%d receive message failed: %s!\n", handle->Port,
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
      handle->RxNotification(handle->BusId, canid, dlc, data, 0);
    } else {
      ASLOG(WARN, ("CAN VXL port=%d receive unknown message: '%s'!\n", handle->Port, string));
    }
  } while (XL_SUCCESS == status);
}

static void CanL_VxlRead(uint32_t port) {
  struct CanL_VxlHandleType *handle = CanL_GetHandle(port);

  if (NULL != handle) {
    CanL_RxIndication(handle);
  }
}

static void CanL_RxDaemon(struct CanL_VxlHandleType *handle) {
  while (false == handle->Terminated) {
    CanL_RxIndication(handle);
    if (false == CanL_IsPerfMode()) {
      std::this_thread::sleep_for(1ms);
    }
  }
}

/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _WIN32 */
