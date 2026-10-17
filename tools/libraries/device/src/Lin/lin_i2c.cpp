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
#define LIN_BIT(v, pos) (((v) >> (pos)) & 0x01)

#define AS_LOG_IIC 1
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  int fd;
  std::mutex mutex;
  uint32_t pid;
  uint8_t rLen;
  uint8_t data[LIN_MAX_DATA_SIZE];
} Lin_IICBusType;
/* ================================ [ DECLARES  ] ============================================== */
static int i2c_open(Lin_DeviceType *dev, const char *option);
static int i2c_write(Lin_DeviceType *dev, Lin_FrameType *frame);
static int i2c_read(Lin_DeviceType *dev, Lin_FrameType *frame);
static void i2c_close(Lin_DeviceType *dev);
static int i2c_ioctl(Lin_DeviceType *dev, int type, const void *data, size_t size);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const Lin_DeviceOpsType lin_i2c_ops = {
  .name = "i2c/",
  .open = i2c_open,
  .close = i2c_close,
  .write = i2c_write,
  .read = i2c_read,
  .ioctl = i2c_ioctl,
};
/* ================================ [ LOCALS    ] ============================================== */
static int i2c_open(Lin_DeviceType *dev, const char *option) {
  int r = 0;
  Lin_IICBusType *bus;
  const char *device = &dev->name[8];
  int fd;

  fd = dev_open(device, option);

  if (fd >= 0) {
    bus = new Lin_IICBusType;
    if (bus != NULL) {
      bus->fd = fd;
      bus->rLen = 0;
    } else {
      r = -__LINE__;
    }

    if (0 == r) {
      dev->param = (void *)bus;
    } else {
      delete bus;
    }
  } else {
    r = -__LINE__;
  }

  return r;
}

static int i2c_write(Lin_DeviceType *dev, Lin_FrameType *frame) {
  int r = LIN_MTU;
  Lin_IICBusType *bus = (Lin_IICBusType *)dev->param;
  dev_i2c_msg_t msgs[3];
  uint8_t addr;
  uint8_t frameId;
#if AS_LOG_IIC > 0
  int i;
  char startTs[64];
  Std_GetDateTime(startTs, sizeof(startTs));
#endif

  addr = (frame->pid >> 8) & 0xFF;
  frameId = frame->pid & 0xFF;

  switch (frame->type) {
  case LIN_TYPE_EXT_HEADER: /* when write header, this is a RX frame */
                            /* I2C TX: start addr W ACK frameId ACK stop */
    msgs[0].addr = addr;
    msgs[0].data = (uint8_t *)&frameId;
    msgs[0].len = 1;
    msgs[0].flags = DEV_I2C_WR;
    /* the last byte expect ack = 1 */
    if (1 == frame->dlc) {
      /* I2C TX: start addr R ACK byte NACK stop */
      msgs[1].addr = addr;
      msgs[1].data = (uint8_t *)bus->data;
      msgs[1].len = frame->dlc;
      msgs[1].flags = DEV_I2C_RD | DEV_I2C_NO_READ_ACK;
      r = dev_ioctl(bus->fd, DEV_IOCTL_TRANSFER, msgs, sizeof(dev_i2c_msg_t) * 2);
    } else {
      /* I2C TX: start addr R ACK byte0 ACK byte1 ACK ... byteN NACK stop */
      msgs[1].addr = addr;
      msgs[1].data = (uint8_t *)bus->data;
      msgs[1].len = frame->dlc - 1;
      msgs[1].flags = DEV_I2C_RD;
      msgs[2].addr = addr;
      msgs[2].data = (uint8_t *)&bus->data[frame->dlc - 1];
      msgs[2].len = 1;
      msgs[2].flags = DEV_I2C_RD | DEV_I2C_NO_START | DEV_I2C_NO_READ_ACK;
      r = dev_ioctl(bus->fd, DEV_IOCTL_TRANSFER, msgs, sizeof(dev_i2c_msg_t) * 3);
    }
    if (0 == r) {
#if AS_LOG_IIC > 0
      PRINTF("lin_i2c(%d) read from slave 0x%02X frameId=0x%02X data=[", bus->fd, addr, frameId);
      for (i = 0; i < frame->dlc; i++) {
        PRINTF("%02X ", bus->data[i]);
      }
      PRINTF("] @ %s\n", startTs);
#endif
      std::lock_guard<std::mutex> lg(bus->mutex);
      if (bus->rLen > 0) {
        ASLOG(ERROR, ("LIN IIC Fatal error as rx frame for frameId 0x%02X to slave 0x%02X not "
                      "read out by upper layer\n",
                      frameId, addr));
      }
      bus->pid = frame->pid;
      bus->rLen = frame->dlc;
    } else {
      ASLOG(ERROR,
            ("LIN IIC failed to send header for frameId 0x%02X to slave 0x%02X\n", frameId, addr));
    }

    break;
  case LIN_TYPE_EXT_HEADER_AND_DATA:
#if AS_LOG_IIC > 0
    PRINTF("lin_i2c(%d) write to  slave 0x%02X frameId=0x%02X data=[", bus->fd, addr, frameId);
    for (i = 0; i < frame->dlc; i++) {
      PRINTF("%02X ", frame->data[i]);
    }
    PRINTF("] @ %s\n", startTs);
#endif
    msgs[0].addr = addr;
    msgs[0].data = (uint8_t *)&frameId;
    msgs[0].len = 1;
    msgs[0].flags = DEV_I2C_WR;
    if (1 == frame->dlc) {
      /* I2C TX: start addr W ACK frameId ACK byte NACK stop */
      msgs[1].addr = addr;
      msgs[1].data = (uint8_t *)frame->data;
      msgs[1].len = frame->dlc;
      msgs[1].flags = DEV_I2C_WR | DEV_I2C_NO_START | DEV_I2C_IGNORE_NACK;
      r = dev_ioctl(bus->fd, DEV_IOCTL_TRANSFER, msgs, sizeof(dev_i2c_msg_t) * 2);
    } else {
      /* I2C TX: start addr W ACK frameId ACK byte0 ACK byte1 ACK ... byteN NACK stop */
      msgs[1].addr = addr;
      msgs[1].data = (uint8_t *)frame->data;
      msgs[1].len = frame->dlc - 1;
      msgs[1].flags = DEV_I2C_WR | DEV_I2C_NO_START;
      msgs[2].addr = addr;
      msgs[2].data = (uint8_t *)&frame->data[frame->dlc - 1];
      msgs[2].len = 1;
      msgs[2].flags = DEV_I2C_WR | DEV_I2C_NO_START | DEV_I2C_IGNORE_NACK;
      r = dev_ioctl(bus->fd, DEV_IOCTL_TRANSFER, msgs, sizeof(dev_i2c_msg_t) * 3);
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
    r = LIN_MTU;
  }

  return r;
}

static void Lin_Checksum(Lin_FrameType *frame) {
  uint8_t checksum = 0;
  int i;

  /* default use enhanced checksum, so lin_read must with enhanced = true */
  checksum += (frame->pid >> 8) & 0xFF;
  checksum += frame->pid & 0xFF;

  for (i = 0; i < frame->dlc; i++) {
    checksum += frame->data[i];
  }
  checksum = ~checksum;

  frame->checksum = checksum;
}

static int i2c_read(Lin_DeviceType *dev, Lin_FrameType *frame) {
  int r = 0;
  Lin_IICBusType *bus = (Lin_IICBusType *)dev->param;

  std::lock_guard<std::mutex> lg(bus->mutex);
  if (bus->rLen > 0) {
    frame->type = LIN_TYPE_DATA;
    frame->dlc = bus->rLen;
    frame->pid = bus->pid;
    memcpy(frame->data, bus->data, bus->rLen);
    bus->rLen = 0;
    Lin_Checksum(frame);
    r = LIN_MTU;
  } else {
    r = -__LINE__;
  }

  return r;
}

static void i2c_close(Lin_DeviceType *dev) {
  Lin_IICBusType *bus = (Lin_IICBusType *)dev->param;

  dev_close(bus->fd);
  delete bus;
}

static int i2c_ioctl(Lin_DeviceType *dev, int type, const void *data, size_t size) {
  int r = -EINVAL;
  Lin_IICBusType *bus = (Lin_IICBusType *)dev->param;
  r = dev_ioctl(bus->fd, type, data, size);
  return r;
}
/* ================================ [ FUNCTIONS ] ============================================== */
