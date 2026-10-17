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
  int fd;
  std::mutex mutex;
  uint32_t pid;
  uint8_t rLen;
  uint8_t data[3 + LIN_MAX_DATA_SIZE + 1 + 1];
} Lin_SPIBusType;
/* ================================ [ DECLARES  ] ============================================== */
static int spi_open(Lin_DeviceType *dev, const char *option);
static int spi_write(Lin_DeviceType *dev, Lin_FrameType *frame);
static int spi_read(Lin_DeviceType *dev, Lin_FrameType *frame);
static void spi_close(Lin_DeviceType *dev);
static int spi_ioctl(Lin_DeviceType *dev, int type, const void *data, size_t size);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const Lin_DeviceOpsType lin_spi_ops = {
  .name = "spi/",
  .open = spi_open,
  .close = spi_close,
  .write = spi_write,
  .read = spi_read,
  .ioctl = spi_ioctl,
};
/* ================================ [ LOCALS    ] ============================================== */
static int spi_open(Lin_DeviceType *dev, const char *option) {
  int r = 0;
  Lin_SPIBusType *bus;
  const char *device = &dev->name[8];
  int fd;

  fd = dev_open(device, option);

  if (fd >= 0) {
    bus = new Lin_SPIBusType;
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

static uint8_t spi_checksum(uint8_t *data, size_t len) {
  size_t i;
  uint8_t checksum = 0;

  for (i = 0; i < len; i++) {
    checksum += data[i];
  }

  return ~checksum;
}

static int spi_write(Lin_DeviceType *dev, Lin_FrameType *frame) {
  int r = LIN_MTU;
  Lin_SPIBusType *bus = (Lin_SPIBusType *)dev->param;
  dev_spi_msg_t msgs[2];
  uint8_t *data = bus->data;
  uint8_t checksum;
  uint16_t id = (uint16_t)frame->pid;
#if AS_LOG_SPI > 0
  int i;
  char startTs[64];
  Std_GetDateTime(startTs, sizeof(startTs));
#endif

  if ((LIN_TYPE_HEADER == frame->type) || (LIN_TYPE_HEADER_AND_DATA == frame->type)) {
    id &= 0x3F;
  }

  data[0] = (id >> 8) & 0xFF;
  data[1] = id & 0xFF;
  data[2] = frame->dlc;

  assert(frame->dlc <= LIN_MAX_DATA_SIZE);

  switch (frame->type) {
  case LIN_TYPE_HEADER:
    /* intention no break */
  case LIN_TYPE_EXT_HEADER: /* when write header, this is a RX frame */
    msgs[0].src = data;
    msgs[0].dst = NULL;
    msgs[0].len = 3;
    msgs[1].src = NULL;
    msgs[1].dst = (uint8_t *)&data[3];
    msgs[1].len = frame->dlc + 1;
#ifdef SPI_RESPONSE_SHIFT
    data[3 + frame->dlc + 1] = 0xFF;
    msgs[1].len += 1;
#endif
    r = dev_ioctl(bus->fd, DEV_IOCTL_TRANSFER, msgs, sizeof(dev_spi_msg_t) * 2);
    if (0 == r) {
#ifdef SPI_RESPONSE_SHIFT
      memmove(&data[3], &data[4], frame->dlc + 1);
#endif
#if AS_LOG_SPI > 0
      PRINTF("lin_spi(%d) read frameId=0x%X data=[", bus->fd, id);
      for (i = 0; i < frame->dlc; i++) {
        PRINTF("%02X ", data[3 + i]);
      }
      PRINTF("] checksum=%02X @ %s\n", data[3 + frame->dlc], startTs);
#endif

      checksum = spi_checksum(data, 3 + frame->dlc);
      if (checksum != data[3 + frame->dlc]) {
        ASLOG(ERROR, ("LIN SPI get frame data with invalid checksum for frameId 0x%X\n", id));
        r = -__LINE__;
      } else {
        memmove(data, &data[3], frame->dlc);
      }
      std::lock_guard<std::mutex> lg(bus->mutex);
      if (bus->rLen > 0) {
        ASLOG(ERROR, ("LIN SPI Fatal error as rx frame for frameId 0x%X "
                      "not read out by upper layer\n",
                      id));
      }
      bus->pid = frame->pid;
      bus->rLen = frame->dlc;
    } else {
      ASLOG(ERROR, ("LIN SPI failed to send header for frameId 0x%X\n", id));
    }

    break;
  case LIN_TYPE_HEADER_AND_DATA:
    /* intention no break */
  case LIN_TYPE_EXT_HEADER_AND_DATA:
    memcpy(&data[3], frame->data, frame->dlc);
    data[3 + frame->dlc] = spi_checksum(data, 3 + frame->dlc);
    msgs[0].src = data;
    msgs[0].dst = NULL;
    msgs[0].len = 3;
    msgs[1].src = &data[3];
    msgs[1].dst = NULL;
    msgs[1].len = frame->dlc + 1;
#if AS_LOG_SPI > 0
    PRINTF("lin_spi(%d) write frameId=0x%X data=[", bus->fd, id);
    for (i = 0; i < frame->dlc; i++) {
      PRINTF("%02X ", frame->data[i]);
    }
    PRINTF("] checksum=%02X @ %s\n", data[3 + frame->dlc], startTs);
#endif
    r = dev_ioctl(bus->fd, DEV_IOCTL_TRANSFER, msgs, sizeof(dev_spi_msg_t) * 2);
    if (0 != r) {
      ASLOG(ERROR, ("LIN SPI failed to send frame for frameId 0x%X\n", id));
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

static int spi_read(Lin_DeviceType *dev, Lin_FrameType *frame) {
  int r = 0;
  Lin_SPIBusType *bus = (Lin_SPIBusType *)dev->param;

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

static void spi_close(Lin_DeviceType *dev) {
  Lin_SPIBusType *bus = (Lin_SPIBusType *)dev->param;

  dev_close(bus->fd);
  delete bus;
}

static int spi_ioctl(Lin_DeviceType *dev, int type, const void *data, size_t size) {
  int r = -EINVAL;
  Lin_SPIBusType *bus = (Lin_SPIBusType *)dev->param;
  r = dev_ioctl(bus->fd, type, data, size);
  return r;
}
/* ================================ [ FUNCTIONS ] ============================================== */
