/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "devlib.h"
#include "devlib_priv.hpp"
#include "linlib.hpp"
#ifdef USE_STD_PRINTF
#undef USE_STD_PRINTF
#endif
#include "Std_Debug.h"
#include "Std_Types.h"
#include "Std_Timer.h"
#include <sys/queue.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#include <chrono>

using namespace std::literals::chrono_literals;
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_LIN 0
#define AS_LOG_LINE 2
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
static int Dev_LinLOpen(const char *device, const char *option, void **param);
static int Dev_LinLRead(void *param, uint8_t *data, size_t size);
static int Dev_LinLWrite(void *param, const uint8_t *data, size_t size);
static void Dev_LinLClose(void *param);
static int Dev_LinLIoCtl(void *param, int type, const void *data, size_t size);
static void Dev_LinLRxDaemon(void *param);

extern "C" const Dev_LinDeviceOpsType Dev_LinSimulatorOps;
extern "C" const Dev_LinDeviceOpsType Dev_LinSimulatorV2Ops;
extern "C" const Dev_LinDeviceOpsType Dev_LinI2cOps;
extern "C" const Dev_LinDeviceOpsType Dev_LinSpiOps;
/* ================================ [ DATAS     ] ============================================== */
extern "C" const Dev_DeviceOpsType Dev_LinOps = {
  .Name = "lin/",
  .Open = Dev_LinLOpen,
  .Read = Dev_LinLRead,
  .Write = Dev_LinLWrite,
  .IoCtl = Dev_LinLIoCtl,
  .Close = Dev_LinLClose,
};

static const Dev_LinDeviceOpsType *linOps[] = {
  &Dev_LinSimulatorOps, &Dev_LinSimulatorV2Ops, &Dev_LinI2cOps, &Dev_LinSpiOps, nullptr,
};
/* ================================ [ LOCALS    ] ============================================== */
static const Dev_LinDeviceOpsType *Dev_SearchOps(const char *name) {
  const Dev_LinDeviceOpsType *ops, **o;
  o = linOps;
  ops = nullptr;
  while (*o != nullptr) {
    if (name == strstr(name, (*o)->Name.c_str())) {
      ops = *o;
      break;
    }
    o++;
  }

  return ops;
}

static int Dev_LinLOpen(const char *device, const char *option, void **param) {
  int r = 0;
  Dev_LinDeviceType *dev = nullptr;
  const Dev_LinDeviceOpsType *ops = nullptr;

  ops = Dev_SearchOps(&device[4]);
  if (nullptr == ops) {
    ASLOG(ERROR, ("LIN device(%s) is not found\n", device));
    r = -1;
  }

  if (0 == r) {
    dev = new Dev_LinDeviceType;
    if (nullptr == dev) {
      r = -2;
    }
  }

  if (0 == r) {
    dev->Name = device;
    dev->Ops = ops;
    dev->Param = nullptr;
    dev->Size = 0;
    STAILQ_INIT(&dev->Head);
    dev->IsKilled = FALSE;

    r = ops->Open(dev, option);

    if (0 == r) {
      dev->RxThread = std::thread(Dev_LinLRxDaemon, (void *)dev);
    }

    if (0 == r) {
      *param = (void *)dev;
    } else {
      delete dev;
    }
  }

  return r;
}

static int Dev_LinLRead(void *param, uint8_t *data, size_t size) {
  int len = 0;
  Dev_LinDeviceType *dev = (Dev_LinDeviceType *)param;
  Dev_LinFrameBufType *frame;
  std::lock_guard<std::mutex> lg(dev->QLock);
  if (FALSE == STAILQ_EMPTY(&dev->Head)) {
    frame = STAILQ_FIRST(&dev->Head);
    if (size >= (size_t)frame->Size) {
      STAILQ_REMOVE_HEAD(&dev->Head, Entry);
      memcpy(data, frame->Data, frame->Size);
      len = frame->Size;
    } else {
      ASLOG(LINE, ("lin read buffer size %d < %d\n", (int)size, (int)frame->Size));
    }
  }

  return len;
}

static int Dev_LinLWrite(void *param, const uint8_t *data, size_t size) {
  int len = DEVLIN_MTU;
  Dev_LinDeviceType *dev = (Dev_LinDeviceType *)param;
  Dev_LinFrameType frame;

  memset(&frame, 0, DEVLIN_MTU);
  if (((uint8_t)data[0] == DEVLIN_TYPE_HEADER) && (size >= 2)) {
    frame.Type = DEVLIN_TYPE_HEADER;
    frame.Pid = (LinL_IdType)data[1];
    if (size > 2) {
      frame.Dlc = data[2];
    } else {
      frame.Dlc = 8;
    }
    ASLOG(LIN, ("TX Pid=%02X, DLC=%d @%u\n", frame.Pid, (int)frame.Dlc, (uint32_t)Std_GetTime()));
  } else if (((uint8_t)data[0] == DEVLIN_TYPE_DATA) && (size > 2)) {
    frame.Type = DEVLIN_TYPE_DATA;
    frame.Dlc = size - 2;
    memcpy(&frame.Data, &data[1], frame.Dlc);
    frame.Checksum = data[size - 1];
    ASLOG(LIN, ("TX DLC=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X] checksum=%02X @%u\n",
                (int)frame.Dlc, frame.Data[0], frame.Data[1], frame.Data[2], frame.Data[3],
                frame.Data[4], frame.Data[5], frame.Data[6], frame.Data[7], frame.Checksum,
                (uint32_t)Std_GetTime()));
  } else if (((uint8_t)data[0] == DEVLIN_TYPE_HEADER_AND_DATA) && (size > 3)) {
    frame.Type = DEVLIN_TYPE_HEADER_AND_DATA;
    frame.Pid = (LinL_IdType)data[1];
    frame.Dlc = size - 3;
    memcpy(&frame.Data, &data[2], frame.Dlc);
    frame.Checksum = data[size - 1];
    ASLOG(LIN,
          ("TX Pid=%02X, DLC=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X] checksum=%02X @%u\n",
           frame.Pid, (int)frame.Dlc, frame.Data[0], frame.Data[1], frame.Data[2], frame.Data[3],
           frame.Data[4], frame.Data[5], frame.Data[6], frame.Data[7], frame.Checksum,
           (uint32_t)Std_GetTime()));
  } else if (((uint8_t)data[0] == DEVLIN_TYPE_EXT_HEADER) && (size >= 5)) {
    frame.Type = DEVLIN_TYPE_EXT_HEADER;
    frame.Pid =
      ((uint32_t)data[1] << 24) + ((uint32_t)data[2] << 16) + ((uint32_t)data[3] << 8) + data[4];
    if (size > 5) {
      frame.Dlc = data[5];
    } else {
      frame.Dlc = 8;
    }
    ASLOG(LIN, ("TX Pid=%02X, DLC=%d @%u\n", frame.Pid, (int)frame.Dlc, (uint32_t)Std_GetTime()));
  } else if (((uint8_t)data[0] == DEVLIN_TYPE_EXT_HEADER_AND_DATA) && (size > 6)) {
    frame.Type = DEVLIN_TYPE_EXT_HEADER_AND_DATA;
    frame.Pid =
      ((uint32_t)data[1] << 24) + ((uint32_t)data[2] << 16) + ((uint32_t)data[3] << 8) + data[4];
    frame.Dlc = size - 6;
    memcpy(&frame.Data, &data[5], frame.Dlc);
    frame.Checksum = data[size - 1];
    ASLOG(LIN,
          ("TX Pid=%02X, DLC=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X] checksum=%02X @%u\n",
           frame.Pid, (int)frame.Dlc, frame.Data[0], frame.Data[1], frame.Data[2], frame.Data[3],
           frame.Data[4], frame.Data[5], frame.Data[6], frame.Data[7], frame.Checksum,
           (uint32_t)Std_GetTime()));
  } else {
    ASLOG(ERROR, ("Invalid data format for %s\n", dev->Name.c_str()));
    len = -EINVAL;
  }

  if (DEVLIN_MTU == len) {
    len = dev->Ops->Write(dev, &frame);
  }

  if (DEVLIN_MTU == len) {
    len = size;
  }

  return len;
}

static void Dev_LinLClose(void *param) {
  Dev_LinDeviceType *dev = (Dev_LinDeviceType *)param;
  dev->IsKilled = TRUE;
  if (dev->RxThread.joinable()) {
    dev->RxThread.join();
  }
  dev->Ops->Close(dev);
  delete dev;
}

static int Dev_LinLIoCtl(void *param, int type, const void *data, size_t size) {
  int r = -ENOTSUP;
  Dev_LinDeviceType *dev = (Dev_LinDeviceType *)param;

  if (dev->Ops->IoCtl) {
    r = dev->Ops->IoCtl(dev, type, data, size);
  }

  return r;
}

static void Dev_LinLRxDaemon(void *param) {
  Dev_LinDeviceType *dev = (Dev_LinDeviceType *)param;
  Dev_LinFrameType frame;
  Dev_LinFrameBufType *pframe;
  int r;

  while (FALSE == dev->IsKilled) {
    r = dev->Ops->Read(dev, &frame);
    if (DEVLIN_MTU == r) {
      pframe = (Dev_LinFrameBufType *)malloc(sizeof(Dev_LinFrameBufType));
      if (nullptr != pframe) {
        if (frame.Type == DEVLIN_TYPE_HEADER) {
          pframe->Data[0] = DEVLIN_TYPE_HEADER;
          pframe->Data[1] = frame.Pid;
          pframe->Size = 2;
          ASLOG(LIN, ("RX Pid=%02X @%u\n", frame.Pid, (uint32_t)Std_GetTime()));
        } else if (frame.Type == DEVLIN_TYPE_DATA) {
          pframe->Data[0] = DEVLIN_TYPE_DATA;
          memcpy(&pframe->Data[1], frame.Data, frame.Dlc);
          pframe->Data[1 + frame.Dlc] = frame.Checksum;
          pframe->Size = 2 + frame.Dlc;
          ASLOG(LIN, ("RX DLC=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X] "
                      "checksum=%02X @%u\n",
                      (int)frame.Dlc, frame.Data[0], frame.Data[1], frame.Data[2], frame.Data[3],
                      frame.Data[4], frame.Data[5], frame.Data[6], frame.Data[7], frame.Checksum,
                      (uint32_t)Std_GetTime()));
        } else if (frame.Type == DEVLIN_TYPE_HEADER_AND_DATA) {
          pframe->Data[0] = DEVLIN_TYPE_HEADER_AND_DATA;
          pframe->Data[1] = frame.Pid;
          memcpy(&pframe->Data[2], frame.Data, frame.Dlc);
          pframe->Data[2 + frame.Dlc] = frame.Checksum;
          pframe->Size = 3 + frame.Dlc;
          ASLOG(LIN, ("RX Pid=%02X, DLC=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X] "
                      "checksum=%02X @%u\n",
                      frame.Pid, (int)frame.Dlc, frame.Data[0], frame.Data[1], frame.Data[2],
                      frame.Data[3], frame.Data[4], frame.Data[5], frame.Data[6], frame.Data[7],
                      frame.Checksum, (uint32_t)Std_GetTime()));
        } else if (frame.Type == DEVLIN_TYPE_EXT_HEADER) {
          pframe->Data[0] = DEVLIN_TYPE_EXT_HEADER;
          pframe->Data[1] = (frame.Pid >> 24) & 0xFF;
          pframe->Data[2] = (frame.Pid >> 16) & 0xFF;
          pframe->Data[3] = (frame.Pid >> 8) & 0xFF;
          pframe->Data[4] = frame.Pid & 0xFF;
          pframe->Size = 5;
          ASLOG(LIN, ("RX Pid=%02X @%u\n", frame.Pid, (uint32_t)Std_GetTime()));
        } else if (frame.Type == DEVLIN_TYPE_EXT_HEADER_AND_DATA) {
          pframe->Data[0] = DEVLIN_TYPE_EXT_HEADER_AND_DATA;
          pframe->Data[1] = (frame.Pid >> 24) & 0xFF;
          pframe->Data[2] = (frame.Pid >> 16) & 0xFF;
          pframe->Data[3] = (frame.Pid >> 8) & 0xFF;
          pframe->Data[4] = frame.Pid & 0xFF;
          memcpy(&pframe->Data[5], frame.Data, frame.Dlc);
          pframe->Data[5 + frame.Dlc] = frame.Checksum;
          pframe->Size = 5 + frame.Dlc;
          ASLOG(LIN, ("RX Pid=%02X, DLC=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X] "
                      "checksum=%02X @%u\n",
                      frame.Pid, (int)frame.Dlc, frame.Data[0], frame.Data[1], frame.Data[2],
                      frame.Data[3], frame.Data[4], frame.Data[5], frame.Data[6], frame.Data[7],
                      frame.Checksum, (uint32_t)Std_GetTime()));
        } else {
          ASLOG(ERROR, ("invalid frame from %s\n", dev->Name.c_str()));
          continue;
        }
        std::lock_guard<std::mutex> lg(dev->QLock);
        STAILQ_INSERT_TAIL(&dev->Head, pframe, Entry);
      }
    }
    std::this_thread::sleep_for(1ms);
  }
}

/* ================================ [ FUNCTIONS ] ============================================== */
