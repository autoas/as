/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include "PAL.h"

#include "Std_Types.h"
#include "linlib.hpp"
#include "Std_Debug.h"
#include "Std_Timer.h"
#include "devlib.h"
/* ================================ [ MACROS    ] ============================================== */
#define LinBit(v, pos) (((v) >> (pos)) & 0x01)

#define AS_LOG_IIC 1
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  int Fd;
  std::mutex Mutex;
  uint32_t Pid;
  uint8_t RLen;
  uint8_t Data[DEVLIN_MAX_DATA_SIZE];
} Dev_LinI2cBusType;
/* ================================ [ DECLARES  ] ============================================== */
static int Dev_I2cOpen(Dev_LinDeviceType *dev, const char *option);
static int Dev_I2cWrite(Dev_LinDeviceType *dev, Dev_LinFrameType *frame);
static int Dev_I2cRead(Dev_LinDeviceType *dev, Dev_LinFrameType *frame);
static void Dev_I2cClose(Dev_LinDeviceType *dev);
static int Dev_I2cIoctl(Dev_LinDeviceType *dev, int type, const void *data, size_t size);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const Dev_LinDeviceOpsType Dev_LinI2cOps = {
  .Name = "i2c/",
  .Open = Dev_I2cOpen,
  .Close = Dev_I2cClose,
  .Write = Dev_I2cWrite,
  .Read = Dev_I2cRead,
  .IoCtl = Dev_I2cIoctl,
};
/* ================================ [ LOCALS    ] ============================================== */
static int Dev_I2cOpen(Dev_LinDeviceType *dev, const char *option) {
  int r = 0;
  Dev_LinI2cBusType *bus;
  const char *device = &dev->Name[8];
  int fd;

  fd = Dev_Open(device, option);

  if (fd >= 0) {
    bus = new Dev_LinI2cBusType;
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

static int Dev_I2cWrite(Dev_LinDeviceType *dev, Dev_LinFrameType *frame) {
  int r = DEVLIN_MTU;
  Dev_LinI2cBusType *bus = (Dev_LinI2cBusType *)dev->Param;
  Dev_I2cMsgType msgs[3];
  uint8_t addr;
  uint8_t frameId;
#if AS_LOG_IIC > 0
  int i;
  char startTs[64];
  Std_GetDateTime(startTs, sizeof(startTs));
#endif

  addr = (frame->Pid >> 8) & 0xFF;
  frameId = frame->Pid & 0xFF;

  switch (frame->Type) {
  case DEVLIN_TYPE_EXT_HEADER: /* when write header, this is a RX frame */
                            /* I2C TX: start addr W ACK frameId ACK stop */
    msgs[0].Addr = addr;
    msgs[0].Data = (uint8_t *)&frameId;
    msgs[0].Len = 1;
    msgs[0].Flags = DEV_I2C_WR;
    /* the last byte expect ack = 1 */
    if (1 == frame->Dlc) {
      /* I2C TX: start addr R ACK byte NACK stop */
      msgs[1].Addr = addr;
      msgs[1].Data = (uint8_t *)bus->Data;
      msgs[1].Len = frame->Dlc;
      msgs[1].Flags = DEV_I2C_RD | DEV_I2C_NO_READ_ACK;
      r = Dev_IoCtl(bus->Fd, DEV_IOCTL_TRANSFER, msgs, sizeof(Dev_I2cMsgType) * 2);
    } else {
      /* I2C TX: start addr R ACK byte0 ACK byte1 ACK ... byteN NACK stop */
      msgs[1].Addr = addr;
      msgs[1].Data = (uint8_t *)bus->Data;
      msgs[1].Len = frame->Dlc - 1;
      msgs[1].Flags = DEV_I2C_RD;
      msgs[2].Addr = addr;
      msgs[2].Data = (uint8_t *)&bus->Data[frame->Dlc - 1];
      msgs[2].Len = 1;
      msgs[2].Flags = DEV_I2C_RD | DEV_I2C_NO_START | DEV_I2C_NO_READ_ACK;
      r = Dev_IoCtl(bus->Fd, DEV_IOCTL_TRANSFER, msgs, sizeof(Dev_I2cMsgType) * 3);
    }
    if (0 == r) {
#if AS_LOG_IIC > 0
      PRINTF("lin_i2c(%d) read from slave 0x%02X frameId=0x%02X data=[", bus->Fd, addr, frameId);
      for (i = 0; i < frame->Dlc; i++) {
        PRINTF("%02X ", bus->Data[i]);
      }
      PRINTF("] @ %s\n", startTs);
#endif
      std::lock_guard<std::mutex> lg(bus->Mutex);
      if (bus->RLen > 0) {
        ASLOG(ERROR, ("LIN IIC Fatal error as rx frame for frameId 0x%02X to slave 0x%02X not "
                      "read out by upper layer\n",
                      frameId, addr));
      }
      bus->Pid = frame->Pid;
      bus->RLen = frame->Dlc;
    } else {
      ASLOG(ERROR,
            ("LIN IIC failed to send header for frameId 0x%02X to slave 0x%02X\n", frameId, addr));
    }

    break;
  case DEVLIN_TYPE_EXT_HEADER_AND_DATA:
#if AS_LOG_IIC > 0
    PRINTF("lin_i2c(%d) write to  slave 0x%02X frameId=0x%02X data=[", bus->Fd, addr, frameId);
    for (i = 0; i < frame->Dlc; i++) {
      PRINTF("%02X ", frame->Data[i]);
    }
    PRINTF("] @ %s\n", startTs);
#endif
    msgs[0].Addr = addr;
    msgs[0].Data = (uint8_t *)&frameId;
    msgs[0].Len = 1;
    msgs[0].Flags = DEV_I2C_WR;
    if (1 == frame->Dlc) {
      /* I2C TX: start addr W ACK frameId ACK byte NACK stop */
      msgs[1].Addr = addr;
      msgs[1].Data = (uint8_t *)frame->Data;
      msgs[1].Len = frame->Dlc;
      msgs[1].Flags = DEV_I2C_WR | DEV_I2C_NO_START | DEV_I2C_IGNORE_NACK;
      r = Dev_IoCtl(bus->Fd, DEV_IOCTL_TRANSFER, msgs, sizeof(Dev_I2cMsgType) * 2);
    } else {
      /* I2C TX: start addr W ACK frameId ACK byte0 ACK byte1 ACK ... byteN NACK stop */
      msgs[1].Addr = addr;
      msgs[1].Data = (uint8_t *)frame->Data;
      msgs[1].Len = frame->Dlc - 1;
      msgs[1].Flags = DEV_I2C_WR | DEV_I2C_NO_START;
      msgs[2].Addr = addr;
      msgs[2].Data = (uint8_t *)&frame->Data[frame->Dlc - 1];
      msgs[2].Len = 1;
      msgs[2].Flags = DEV_I2C_WR | DEV_I2C_NO_START | DEV_I2C_IGNORE_NACK;
      r = Dev_IoCtl(bus->Fd, DEV_IOCTL_TRANSFER, msgs, sizeof(Dev_I2cMsgType) * 3);
    }
    if (0 != r) {
      ASLOG(ERROR,
            ("LIN IIC failed to send frame for frameId 0x%02X to slave 0x%02X\n", frameId, addr));
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

static int Dev_I2cRead(Dev_LinDeviceType *dev, Dev_LinFrameType *frame) {
  int r = 0;
  Dev_LinI2cBusType *bus = (Dev_LinI2cBusType *)dev->Param;

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

static void Dev_I2cClose(Dev_LinDeviceType *dev) {
  Dev_LinI2cBusType *bus = (Dev_LinI2cBusType *)dev->Param;

  Dev_Close(bus->Fd);
  delete bus;
}

static int Dev_I2cIoctl(Dev_LinDeviceType *dev, int type, const void *data, size_t size) {
  int r = -EINVAL;
  Dev_LinI2cBusType *bus = (Dev_LinI2cBusType *)dev->Param;
  r = Dev_IoCtl(bus->Fd, type, data, size);
  return r;
}
/* ================================ [ FUNCTIONS ] ============================================== */
