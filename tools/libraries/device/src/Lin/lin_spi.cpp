/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <assert.h>

#include "Std_Types.h"
#include "linlib.hpp"
#include "Std_Debug.h"
#include "Std_Timer.h"
#include "devlib.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_SPI 0
/* for the slave that response has one shift byte*/
#define SPI_RESPONSE_SHIFT
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  int Fd;
  std::mutex Mutex;
  uint32_t Pid;
  uint8_t RLen;
  uint8_t Data[3 + DEVLIN_MAX_DATA_SIZE + 1 + 1];
} Dev_LinSpiBusType;
/* ================================ [ DECLARES  ] ============================================== */
static int Dev_SpiOpen(Dev_LinDeviceType *dev, const char *option);
static int Dev_SpiWrite(Dev_LinDeviceType *dev, Dev_LinFrameType *frame);
static int Dev_SpiRead(Dev_LinDeviceType *dev, Dev_LinFrameType *frame);
static void Dev_SpiClose(Dev_LinDeviceType *dev);
static int Dev_SpiIoctl(Dev_LinDeviceType *dev, int type, const void *data, size_t size);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const Dev_LinDeviceOpsType Dev_LinSpiOps = {
  .Name = "spi/",
  .Open = Dev_SpiOpen,
  .Close = Dev_SpiClose,
  .Write = Dev_SpiWrite,
  .Read = Dev_SpiRead,
  .IoCtl = Dev_SpiIoctl,
};
/* ================================ [ LOCALS    ] ============================================== */
static int Dev_SpiOpen(Dev_LinDeviceType *dev, const char *option) {
  int r = 0;
  Dev_LinSpiBusType *bus;
  const char *device = &dev->Name[8];
  int fd;

  fd = Dev_Open(device, option);

  if (fd >= 0) {
    bus = new Dev_LinSpiBusType;
    if (bus != NULL) {
      bus->Fd = fd;
      bus->RLen = 0;
    } else {
      r = -__LINE__;
    }

    if (0 == r) {
      dev->Param = (void *)bus;
    } else {
      delete bus;
    }
  } else {
    r = -__LINE__;
  }

  return r;
}

static uint8_t Dev_SpiChecksum(uint8_t *data, size_t len) {
  size_t i;
  uint8_t checksum = 0;

  for (i = 0; i < len; i++) {
    checksum += data[i];
  }

  return ~checksum;
}

static int Dev_SpiWrite(Dev_LinDeviceType *dev, Dev_LinFrameType *frame) {
  int r = DEVLIN_MTU;
  Dev_LinSpiBusType *bus = (Dev_LinSpiBusType *)dev->Param;
  Dev_SpiMsgType msgs[2];
  uint8_t *data = bus->Data;
  uint8_t checksum;
  uint16_t id = (uint16_t)frame->Pid;
#if AS_LOG_SPI > 0
  int i;
  char startTs[64];
  Std_GetDateTime(startTs, sizeof(startTs));
#endif

  if ((DEVLIN_TYPE_HEADER == frame->Type) || (DEVLIN_TYPE_HEADER_AND_DATA == frame->Type)) {
    id &= 0x3F;
  }

  data[0] = (id >> 8) & 0xFF;
  data[1] = id & 0xFF;
  data[2] = frame->Dlc;

  assert(frame->Dlc <= DEVLIN_MAX_DATA_SIZE);

  switch (frame->Type) {
  case DEVLIN_TYPE_HEADER:
    /* intention no break */
  case DEVLIN_TYPE_EXT_HEADER: /* when write header, this is a RX frame */
    msgs[0].Src = data;
    msgs[0].Dst = NULL;
    msgs[0].Len = 3;
    msgs[1].Src = NULL;
    msgs[1].Dst = (uint8_t *)&data[3];
    msgs[1].Len = frame->Dlc + 1;
#ifdef SPI_RESPONSE_SHIFT
    data[3 + frame->Dlc + 1] = 0xFF;
    msgs[1].Len += 1;
#endif
    r = Dev_IoCtl(bus->Fd, DEV_IOCTL_TRANSFER, msgs, sizeof(Dev_SpiMsgType) * 2);
    if (0 == r) {
#ifdef SPI_RESPONSE_SHIFT
      memmove(&data[3], &data[4], frame->Dlc + 1);
#endif
#if AS_LOG_SPI > 0
      PRINTF("lin_spi(%d) read frameId=0x%X data=[", bus->Fd, id);
      for (i = 0; i < frame->Dlc; i++) {
        PRINTF("%02X ", data[3 + i]);
      }
      PRINTF("] checksum=%02X @ %s\n", data[3 + frame->Dlc], startTs);
#endif

      checksum = Dev_SpiChecksum(data, 3 + frame->Dlc);
      if (checksum != data[3 + frame->Dlc]) {
        ASLOG(ERROR, ("LIN SPI get frame data with invalid checksum for frameId 0x%X\n", id));
        r = -__LINE__;
      } else {
        memmove(data, &data[3], frame->Dlc);
      }
      std::lock_guard<std::mutex> lg(bus->Mutex);
      if (bus->RLen > 0) {
        ASLOG(ERROR, ("LIN SPI Fatal error as rx frame for frameId 0x%X "
                      "not read out by upper layer\n",
                      id));
      }
      bus->Pid = frame->Pid;
      bus->RLen = frame->Dlc;
    } else {
      ASLOG(ERROR, ("LIN SPI failed to send header for frameId 0x%X\n", id));
    }

    break;
  case DEVLIN_TYPE_HEADER_AND_DATA:
    /* intention no break */
  case DEVLIN_TYPE_EXT_HEADER_AND_DATA:
    memcpy(&data[3], frame->Data, frame->Dlc);
    data[3 + frame->Dlc] = Dev_SpiChecksum(data, 3 + frame->Dlc);
    msgs[0].Src = data;
    msgs[0].Dst = NULL;
    msgs[0].Len = 3;
    msgs[1].Src = &data[3];
    msgs[1].Dst = NULL;
    msgs[1].Len = frame->Dlc + 1;
#if AS_LOG_SPI > 0
    PRINTF("lin_spi(%d) write frameId=0x%X data=[", bus->Fd, id);
    for (i = 0; i < frame->Dlc; i++) {
      PRINTF("%02X ", frame->Data[i]);
    }
    PRINTF("] checksum=%02X @ %s\n", data[3 + frame->Dlc], startTs);
#endif
    r = Dev_IoCtl(bus->Fd, DEV_IOCTL_TRANSFER, msgs, sizeof(Dev_SpiMsgType) * 2);
    if (0 != r) {
      ASLOG(ERROR, ("LIN SPI failed to send frame for frameId 0x%X\n", id));
    }
    break;
  default:
    r = -__LINE__;
    break;
  }

  if (0 == r) {
    r = DEVLIN_MTU;
  }

  return r;
}

static void Lin_Checksum(Dev_LinFrameType *frame) {
  uint8_t checksum = 0;
  int i;

  /* default use enhanced checksum, so LinL_Read must with enhanced = true */
  checksum += (frame->Pid >> 8) & 0xFF;
  checksum += frame->Pid & 0xFF;

  for (i = 0; i < frame->Dlc; i++) {
    checksum += frame->Data[i];
  }
  checksum = ~checksum;

  frame->Checksum = checksum;
}

static int Dev_SpiRead(Dev_LinDeviceType *dev, Dev_LinFrameType *frame) {
  int r = 0;
  Dev_LinSpiBusType *bus = (Dev_LinSpiBusType *)dev->Param;

  std::lock_guard<std::mutex> lg(bus->Mutex);
  if (bus->RLen > 0) {
    frame->Type = DEVLIN_TYPE_DATA;
    frame->Dlc = bus->RLen;
    frame->Pid = bus->Pid;
    memcpy(frame->Data, bus->Data, bus->RLen);
    bus->RLen = 0;
    Lin_Checksum(frame);
    r = DEVLIN_MTU;
  } else {
    r = -__LINE__;
  }

  return r;
}

static void Dev_SpiClose(Dev_LinDeviceType *dev) {
  Dev_LinSpiBusType *bus = (Dev_LinSpiBusType *)dev->Param;

  Dev_Close(bus->Fd);
  delete bus;
}

static int Dev_SpiIoctl(Dev_LinDeviceType *dev, int type, const void *data, size_t size) {
  int r = -EINVAL;
  Dev_LinSpiBusType *bus = (Dev_LinSpiBusType *)dev->Param;
  r = Dev_IoCtl(bus->Fd, type, data, size);
  return r;
}
/* ================================ [ FUNCTIONS ] ============================================== */
