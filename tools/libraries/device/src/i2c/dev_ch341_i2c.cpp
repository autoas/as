/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 *
 * ref: https://blog.csdn.net/WCH_TechGroup/article/details/130932558
 *      https://www.wch.cn/download/CH341PAR_ZIP.html
 */
/* ================================ [ INCLUDES  ] ============================================== */
#ifdef _WIN32
#include <windows.h>
#include "PAL.h"
#include "devlib.h"
#include "devlib_priv.hpp"
#include <stdlib.h>
#include <string.h>
#include "Std_Debug.h"
#include "Std_Timer.h"
#include <assert.h>
#include <errno.h>

#ifdef ERROR
#undef ERROR
#endif
/* ================================ [ MACROS    ] ============================================== */
#define PPARAM(p) ((Dev_CH341I2CParamType *)p)

#define AS_LOG_CH341 0
#define AS_LOG_CH341I 1

#define CH341_LOAD(sym)                                                                            \
  if (0 == r) {                                                                                    \
    *(void **)&CH341_I2C_Interface.CH341##sym = PAL_DlSym(CH341_I2C_Interface.dll, "CH341" #sym); \
    if (nullptr == CH341_I2C_Interface.CH341##sym) {                                                  \
      r = -__LINE__;                                                                               \
      ASLOG(ERROR, ("Can't find symbold " #sym "\n"));                                             \
    }                                                                                              \
  }

#define CH341_CALL(sym) CH341_I2C_Interface.CH341##sym

/* Length of data packets supported by CH341 */
#define mCH341_PACKET_LENGTH 32

/* The command package of the I2C interface, starting from the secondary byte, is the I2C
 * command stream */
#define mCH341A_CMD_I2C_STREAM 0xAA
/* Command flow of I2C interface: generate start bit */
#define mCH341A_CMD_I2C_STM_STA 0x74
/* Command flow of I2C interface: generate stop bit */
#define mCH341A_CMD_I2C_STM_STO 0x75
/* Command flow of I2C interface: output data, bit 5- bit 0 is the length, subsequent bytes are
 * data, and length 0 only sends one byte and returns an answer */
#define mCH341A_CMD_I2C_STM_OUT 0x80
/* I2C interface command flow: input data, bit 5-bit 0 is the length, and 0 length only receives one
 * byte and sends no response */
#define mCH341A_CMD_I2C_STM_IN 0xC0
/* The maximum length of input and output data of a single command in the command stream of I2C
 * interface*/
#define mCH341A_CMD_I2C_STM_MAX ((0x3F < mCH341_PACKET_LENGTH) ? 0x3F : mCH341_PACKET_LENGTH)
/* Command flow of I2C interface: set parameters, bit 2=i/o number of SPI (0= single input single
 * output, 1= double input double output), bit 1 0=i2c speed (00= low speed, 01= standard, 10= fast,
 * 11= high speed) */
#define mCH341A_CMD_I2C_STM_SET 0x60
/* Command flow of I2C interface: delay in microseconds, bit 3- bit 0 as delay value */
#define mCH341A_CMD_I2C_STM_US 0x40
/* Command flow of I2C interface: delay in milliseconds, bit 3-bit 0 is the delay value */
#define mCH341A_CMD_I2C_STM_MS 0x50
/* Maximum value of single command delay of command flow of I2C interface */
#define mCH341A_CMD_I2C_STM_DLY 0x0F
/* Command flow of I2C interface: Command package ends in advance */
#define mCH341A_CMD_I2C_STM_END 0x00

#define mCH341A_CMD_I2C_PACKET_MAX 128

#define E_NO_ACK (0xdeadaccf)
#define E_ACK (0xdeadacc0)
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  int port;
  uint32_t delayUs; /* delay for each byte */
  UCHAR mOutBuffer[mCH341A_CMD_I2C_STM_MAX * mCH341A_CMD_I2C_PACKET_MAX];
  ULONG mOutLength;
  UCHAR mInBuffer[mCH341A_CMD_I2C_STM_MAX * mCH341A_CMD_I2C_PACKET_MAX];
  ULONG mOutPoints[mCH341A_CMD_I2C_PACKET_MAX + 1];
  ULONG mLength;
  ULONG mOutNum;
} Dev_CH341I2CParamType;

typedef struct {
  void *dll;
  HANDLE (*CH341OpenDevice)(ULONG);
  VOID (*CH341CloseDevice)(ULONG);
  BOOL (*CH341SetStream)(ULONG, ULONG);
  BOOL (*CH341WriteData)(ULONG, PVOID, PULONG);
  BOOL (*CH341WriteRead)(ULONG, ULONG, PVOID, ULONG, ULONG, PULONG, PVOID);
  BOOL (*CH341StreamI2C)(ULONG, ULONG, PVOID, ULONG, PVOID);
} CH341_I2C_InterfaceType;
/* ================================ [ DECLARES  ] ============================================== */
static int dev_ch341_i2c_open(const char *device, const char *option, void **param);
static int dev_ch341_i2c_read(void *param, uint8_t *data, size_t size);
static int dev_ch341_i2c_write(void *param, const uint8_t *data, size_t size);
static void dev_ch341_i2c_close(void *param);
static int dev_ch341_i2c_ioctl(void *param, int type, const void *data, size_t size);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const Dev_DeviceOpsType ch341_i2c_dev_ops = {
  .name = "CH341_I2C/",
  .open = dev_ch341_i2c_open,
  .read = dev_ch341_i2c_read,
  .write = dev_ch341_i2c_write,
  .ioctl = dev_ch341_i2c_ioctl,
  .close = dev_ch341_i2c_close,
};

static CH341_I2C_InterfaceType CH341_I2C_Interface = {
  nullptr,
};
/* ================================ [ LOCALS    ] ============================================== */
static int openDriver(void) {
  int r = 0;
  CH341_I2C_Interface.dll = PAL_DlOpen("CH341DLLA64.DLL");
  if (nullptr == CH341_I2C_Interface.dll) {
    r = -__LINE__;
    ASLOG(ERROR, ("Can't open CH341DLLA64.DLL\n"));
  }

  CH341_LOAD(OpenDevice);
  CH341_LOAD(CloseDevice);
  CH341_LOAD(SetStream);
  CH341_LOAD(WriteData);
  CH341_LOAD(WriteRead);
  CH341_LOAD(StreamI2C);

  if (0 != r) {
    if (nullptr != CH341_I2C_Interface.dll) {
      PAL_DlClose(CH341_I2C_Interface.dll);
      CH341_I2C_Interface.dll = nullptr;
    }
  }
  return r;
}

static int dev_ch341_i2c_open(const char *device, const char *option, void **param) {
  int baudrate, port;
  int r = 0;
  HANDLE handle;
  ULONG iMode = 0;
  BOOL ret;

  port = atoi(&device[10]);
  baudrate = atoi(option);

  switch (baudrate) {
  case 20000:
    iMode = 0b00;
    break;
  case 100000:
    iMode = 0b01;
    break;
  case 400000:
    iMode = 0b10;
    break;
  case 750000:
    iMode = 0b11;
    break;
  default:
    ASLOG(ERROR,
          ("invalid baudrate for CH341 IIC, please choose from 20000, 100000, 400000, 750000\n"));
    r = -__LINE__;
    break;
  }

  if (0 == r) {
    if (nullptr == CH341_I2C_Interface.dll) {
      r = openDriver();
    }
  }

  if (0 == r) {
    handle = CH341_CALL(OpenDevice)((ULONG)port);
    if (nullptr == handle) {
      ASLOG(ERROR, ("Can't open CH341 port %d\n", port));
      r = -__LINE__;
    } else {
      ret = CH341_CALL(SetStream)((ULONG)port, iMode);
      if (TRUE != ret) {
        ASLOG(ERROR, ("Can't set CH341 port %d to mode 0x%lx\n", port, iMode));
        r = -__LINE__;
      }
    }
  }

  if (0 == r) {
    *param = malloc(sizeof(Dev_CH341I2CParamType));
    memset(*param, 0, sizeof(Dev_CH341I2CParamType));
    PPARAM(*param)->port = port;
    PPARAM(*param)->delayUs = 0;
    ASLOG(INFO, ("ch341_i2c open(%d, %d) OK\n", port, baudrate));
  }

  return r;
}

static int dev_ch341_i2c_read(void *param, uint8_t *data, size_t size) {
  return -1;
}

static int dev_ch341_i2c_write(void *param, const uint8_t *data, size_t size) {
  return -1;
}

static void dev_ch341_i2c_close(void *param) {
  CH341_CALL(CloseDevice)((ULONG)PPARAM(param)->port);
  free(param);
}

static int i2c_add_streams(Dev_CH341I2CParamType *dev, UCHAR *streams, ULONG num) {
  int r = 0;

  ULONG lastPoint = 0;
  ULONG mLength = dev->mOutLength;

  if (dev->mOutNum > 0) {
    lastPoint = dev->mOutPoints[dev->mOutNum - 1];
  }

  mLength = dev->mOutLength - lastPoint;

  /* check there is enough space to add the streams */
  if ((mLength + num) <= (mCH341A_CMD_I2C_STM_MAX - 1)) {
    /* pass */
  } else { /* no enough space, insert end */
    dev->mOutBuffer[dev->mOutLength++] = mCH341A_CMD_I2C_STM_END;
    dev->mOutPoints[dev->mOutNum++] = dev->mOutLength;
    dev->mOutBuffer[dev->mOutLength++] = mCH341A_CMD_I2C_STREAM;
  }

  if ((dev->mOutLength + num) < (sizeof(dev->mOutBuffer) - 1)) {
    memcpy(&dev->mOutBuffer[dev->mOutLength], streams, num);
    dev->mOutLength += num;
  } else {
    r = -ENOSPC;
  }

  return r;
}

static int i2c_start(Dev_CH341I2CParamType *dev) {
  int r = 0;
  UCHAR mBuffer[1] = {mCH341A_CMD_I2C_STM_STA};

  r = i2c_add_streams(dev, mBuffer, 1);

  ASLOG(CH341, ("i2c_start(%d) = %d\n", dev->port, r));
  return r;
}

static int i2c_delayUs(Dev_CH341I2CParamType *dev, uint32_t delayUs) {
  int r = 0;
  UCHAR mBuffer[1];
  uint32_t delay = delayUs;

  if (delay > 0) {
    if (delay <= mCH341A_CMD_I2C_STM_DLY) {
      mBuffer[0] = mCH341A_CMD_I2C_STM_US | (mCH341A_CMD_I2C_STM_DLY & delay);
    } else {
      delay /= 1000;
      mBuffer[0] = mCH341A_CMD_I2C_STM_MS | (mCH341A_CMD_I2C_STM_DLY & delay);
    }

    r = i2c_add_streams(dev, mBuffer, 1);

    ASLOG(CH341, ("i2c_delay(%d) = %d\n", dev->port, r));
  }

  return r;
}

static int i2c_delay(Dev_CH341I2CParamType *dev) {
  return i2c_delayUs(dev, dev->delayUs);
}

static int i2c_stop(Dev_CH341I2CParamType *dev) {
  int r = 0;
  UCHAR mBuffer[1] = {mCH341A_CMD_I2C_STM_STO};

  r = i2c_add_streams(dev, mBuffer, 1);

  ASLOG(CH341, ("i2c_stop(%d) = %d\n", dev->port, r));
  return r;
}

static int i2c_restart(Dev_CH341I2CParamType *dev) {
  int r;

  r = i2c_stop(dev);
  if (0 == r) {
    /* for some low performance MCU, give the slave some response time */
    r = i2c_delayUs(dev, 1000);
    if (0 == r) {
      r = i2c_start(dev);
    }
  }

  return r;
}

static int i2c_writeb(Dev_CH341I2CParamType *dev, uint8_t byte) {
  int r = 0;
  /* only send one byte and returns an answer */
  UCHAR mBuffer[2] = {mCH341A_CMD_I2C_STM_OUT, byte};

  r = i2c_add_streams(dev, mBuffer, 2);

  if (0 == r) {
    r = i2c_delay(dev);
  }

  ASLOG(CH341, ("i2c_writeb(%d, %02X) = %d\n", dev->port, byte, r));

  return r;
}

static int i2c_readb(Dev_CH341I2CParamType *dev, boolean noAck) {
  int r = 0;
  UCHAR mBuffer[1];

  if (TRUE == noAck) {
    mBuffer[0] = mCH341A_CMD_I2C_STM_IN; /* no ack send */
  } else {
    mBuffer[0] = mCH341A_CMD_I2C_STM_IN | 1; /* send ack */
  }
  r = i2c_add_streams(dev, mBuffer, 1);

  if (0 == r) {
    r = i2c_delay(dev);
  }

  ASLOG(CH341, ("i2c_readb(%d) = %d, ack=%d\n", dev->port, r, noAck ? 1 : 0));

  return r;
}

static int i2c_send_address(Dev_CH341I2CParamType *dev, dev_i2c_msg_t *msg) {
  int r = 0;
  uint8_t addr1;

  /* 7-bit addr */
  addr1 = msg->addr << 1;
  if (msg->flags & DEV_I2C_RD) {
    addr1 |= 1;
  }

  r = i2c_writeb(dev, addr1);

  return r;
}

static int i2c_send_address_check(Dev_CH341I2CParamType *dev, dev_i2c_msg_t *msg) {
  int r = 0;
  uint16_t ignore_nack = msg->flags & DEV_I2C_IGNORE_NACK;
  uint16_t no_write_ack = msg->flags & DEV_I2C_NO_WRITE_ACK;
  uint8_t ack = dev->mInBuffer[dev->mLength++];
  uint8_t addr1;

  /* 7-bit addr */
  addr1 = msg->addr << 1;
  if (msg->flags & DEV_I2C_RD) {
    addr1 |= 1;
  }

  if (0 == (ack & 0x80)) {
    /* has ack */
    if (0 != no_write_ack) {
      ASLOG(ERROR, ("send addr = %X has ack(%X), flags=%X\n", addr1, ack, msg->flags));
      r = -E_ACK; /* treate as NOK as has ACK */
    }
  } else if ((0 != ignore_nack) || (0 != no_write_ack)) {
    r = 0; /* treate as OK as ignore ACK or expect no write ack */
  } else {
    ASLOG(ERROR, ("send addr = %X no ack(%X), flags=%X\n", addr1, ack, msg->flags));
    r = -E_NO_ACK;
  }

  ASLOG(CH341, ("send addr(%d) = %02X, ack=%02X\n", dev->port, addr1, ack));

  return r;
}

static int i2c_recv_bytes(Dev_CH341I2CParamType *dev, dev_i2c_msg_t *msg) {
  int r = 0;
  uint32_t i;

  for (i = 0; (i < msg->len) && (0 == r); i++) {
    r = i2c_readb(dev, (0 != (msg->flags & DEV_I2C_NO_READ_ACK)));
  }

  return r;
}

static int i2c_recv_bytes_check(Dev_CH341I2CParamType *dev, dev_i2c_msg_t *msg) {
  int r = 0;
  uint32_t i;

  for (i = 0; (i < msg->len) && (0 == r); i++) {
    msg->data[i] = dev->mInBuffer[dev->mLength++];
  }

  return r;
}

static int i2c_send_bytes(Dev_CH341I2CParamType *dev, dev_i2c_msg_t *msg) {
  int r = 0;
  uint32_t i;

  for (i = 0; (i < msg->len) && (0 == r); i++) {
    r = i2c_writeb(dev, msg->data[i]);
  }

  return r;
}

static int i2c_send_bytes_check(Dev_CH341I2CParamType *dev, dev_i2c_msg_t *msg) {
  int r = 0;
  uint32_t i;
  uint16_t ignore_nack = msg->flags & DEV_I2C_IGNORE_NACK;
  uint16_t no_write_ack = msg->flags & DEV_I2C_NO_WRITE_ACK;
  uint8_t ack;

  for (i = 0; (i < msg->len) && (0 == r); i++) {
    ack = dev->mInBuffer[dev->mLength++];
    if (0 == (ack & 0x80)) {
      /* has ack */
      if (0 != no_write_ack) {
        ASLOG(ERROR,
              ("send data[%d] = %X has ack(%X), flags=%X\n", i, msg->data[i], ack, msg->flags));
        r = -E_ACK; /* treate as NOK as has ACK */
      }
    } else if ((0 != ignore_nack) || (0 != no_write_ack)) {
      r = 0; /* treate as OK as ignore ACK or expect no write ack */
    } else {
      ASLOG(ERROR, ("send data[%d] = %X no ack(%X), flags=%X\n", i, msg->data[i], ack, msg->flags));
      r = -E_NO_ACK;
    }
    ASLOG(CH341, ("send byte(%d) = %02X, ack=%02X\n", dev->port, msg->data[i], ack));
  }

  return r;
}

static int dev_ch341_i2c_transfer_check(Dev_CH341I2CParamType *dev, dev_i2c_msg_t *msgs,
                                        uint32_t num, ULONG mInLen) {
  int r = 0;
  uint32_t i;
  dev_i2c_msg_t *msg;
  dev->mLength = 0;
  for (i = 0; (i < num) && (0 == r); i++) {
    msg = &msgs[i];
    if (0 == (msg->flags & DEV_I2C_NO_START)) {
      if (0 == r) {
        r = i2c_send_address_check(dev, msg);
      }
    }
    if (0 == r) {
      if (msg->flags & DEV_I2C_RD) {
        r = i2c_recv_bytes_check(dev, msg);
      } else {
        r = i2c_send_bytes_check(dev, msg);
      }
    }
  }

  if (0 != r) {
    ASLOG(ERROR, ("ch341_i2c check failed\n"));
  } else if (dev->mLength != mInLen) {
    ASLOG(ERROR,
          ("ch341_i2c check: not all data consumed, consumed %lu of %lu\n", dev->mLength, mInLen));
  }
  return r;
}

static int i2c_stream_fix(Dev_CH341I2CParamType *dev) {
  int r = 0;
  uint32_t lastStart;
  uint32_t lastLength;
  uint32_t prevStart;
  uint32_t prevLength;
  uint32_t i = 0;
  boolean hasData = FALSE;
  uint32_t prevDataPos;
  uint32_t lastDelay = 0;
  uint8_t cmd;
  if (dev->mOutNum > 1) {
    /* size of mOutPoints is N = (dev->mOutNum-1), 0, { 1, ..., N-2, N-1 }*/
    lastStart = dev->mOutPoints[dev->mOutNum - 2];
    lastLength = dev->mOutPoints[dev->mOutNum - 1] - dev->mOutPoints[dev->mOutNum - 2];
    for (i = 0; (i < lastLength) && (0 == r); /* empty */) {
      cmd = dev->mOutBuffer[lastStart + i];
      if ((mCH341A_CMD_I2C_STREAM == cmd) || (mCH341A_CMD_I2C_STM_STA == cmd) ||
          (mCH341A_CMD_I2C_STM_STO == cmd) || (mCH341A_CMD_I2C_STM_END == cmd)) {
        i++;
      } else if ((mCH341A_CMD_I2C_STM_US == (cmd & 0xF0)) ||
                 (mCH341A_CMD_I2C_STM_MS == (cmd & 0xF0))) {
        lastDelay = lastStart + i;
        i++;
      } else if ((mCH341A_CMD_I2C_STM_OUT == cmd)) {
        i += 2;
        hasData = TRUE;
      } else if ((mCH341A_CMD_I2C_STM_IN == cmd) || ((mCH341A_CMD_I2C_STM_IN | 1) == cmd)) {
        i += 1;
        hasData = TRUE;
      } else {
        ASLOG(ERROR, ("ch341_i2c invalid command %02X\n", cmd));
        r = -EINVAL;
      }
    }

    if (FALSE == hasData) {
      /* move one data from the previous packet */
      prevStart = 0;
      if (dev->mOutNum > 2) {
        prevStart = dev->mOutPoints[dev->mOutNum - 3];
      }
      prevLength = lastStart - prevStart;
      for (i = 0; (i < prevLength) && (0 == r); /* empty */) {
        cmd = dev->mOutBuffer[prevStart + i];
        if ((mCH341A_CMD_I2C_STREAM == cmd) || (mCH341A_CMD_I2C_STM_STA == cmd) ||
            (mCH341A_CMD_I2C_STM_STO == cmd) || (mCH341A_CMD_I2C_STM_END == cmd)) {
          i++;
        } else if ((mCH341A_CMD_I2C_STM_US == (cmd & 0xF0)) ||
                   (mCH341A_CMD_I2C_STM_MS == (cmd & 0xF0))) {
          i++;
        } else if ((mCH341A_CMD_I2C_STM_OUT == cmd)) {
          prevDataPos = prevStart + i;
          i += 2;
          hasData = TRUE;
        } else if ((mCH341A_CMD_I2C_STM_IN == cmd) || ((mCH341A_CMD_I2C_STM_IN | 1) == cmd)) {
          prevDataPos = prevStart + i;
          i += 1;
          hasData = TRUE;
        } else {
          ASLOG(ERROR, ("ch341_i2c invalid command %02X\n", cmd));
          r = -EINVAL;
        }
      }

      if (hasData) {
        assert(mCH341A_CMD_I2C_STM_END == dev->mOutBuffer[lastStart - 1]);
        assert(mCH341A_CMD_I2C_STREAM == dev->mOutBuffer[lastStart]);
        /* do the move */
        memmove(&dev->mOutBuffer[prevDataPos + 2], &dev->mOutBuffer[prevDataPos],
                lastStart - prevDataPos - 1);
        dev->mOutBuffer[prevDataPos] = mCH341A_CMD_I2C_STM_END;
        dev->mOutBuffer[prevDataPos + 1] = mCH341A_CMD_I2C_STREAM;
        dev->mOutPoints[dev->mOutNum - 2] = prevDataPos + 1;
        if (0 != lastDelay) {
          if (mCH341A_CMD_I2C_STM_STO == dev->mOutBuffer[lastDelay + 1]) {
            dev->mOutBuffer[lastDelay] = mCH341A_CMD_I2C_STM_STO;
            dev->mOutBuffer[lastDelay + 1] = mCH341A_CMD_I2C_STM_END;
            dev->mOutPoints[dev->mOutNum - 1] -= 1;
            dev->mOutLength -= 1;
          }
        }
      }
    }
  }
  return r;
}

static int dev_ch341_i2c_transfer(Dev_CH341I2CParamType *dev, dev_i2c_msg_t *msgs, uint32_t num) {
  int r = 0;
  uint32_t i;
  dev_i2c_msg_t *msg;
  ULONG mInLen = 0;
  ULONG mInLenTmp = 0;
  ULONG mLength = 0;
  ULONG lastPoint = 0;
  BOOL ret;
#if AS_LOG_CH341I > 0
  char startTs[64];
  char endTs[64];
  Std_GetDateTime(startTs, sizeof(startTs));
#endif
  dev->mOutLength = 0;
  dev->mOutNum = 0;
  dev->mOutBuffer[dev->mOutLength++] = mCH341A_CMD_I2C_STREAM;
  for (i = 0; (i < num) && (0 == r); i++) {
    msg = &msgs[i];
    if (0 == (msg->flags & DEV_I2C_NO_START)) {
      if (i > 0) {
        r = i2c_restart(dev);
      } else {
        r = i2c_start(dev);
      }
      if (0 == r) {
        r = i2c_send_address(dev, msg);
      }
    }
    if (0 == r) {
      if (msg->flags & DEV_I2C_RD) {
        r = i2c_recv_bytes(dev, msg);
      } else {
        r = i2c_send_bytes(dev, msg);
      }
    }
  }

  if (0 == r) { /* send stop bit */
    if (0 == (msg->flags & DEV_I2C_NO_STOP)) {
      r = i2c_stop(dev);
    }
  }

  if (0 == r) {
    dev->mOutBuffer[dev->mOutLength++] = mCH341A_CMD_I2C_STM_END;
    dev->mOutPoints[dev->mOutNum++] = dev->mOutLength;
    r = i2c_stream_fix(dev);
  }

  if (0 == r) {
#if AS_LOG_CH341I > 0
    PRINTF("ch341_i2c(%d) request: [ ", dev->port);
    for (i = 0; i < dev->mOutLength; i++) {
      PRINTF("%02X ", dev->mOutBuffer[i]);
    }
    PRINTF("] @ %s\n", startTs);
#endif
    for (i = 0; i < dev->mOutNum; i++) {
      mLength = dev->mOutPoints[i] - lastPoint;
#if AS_LOG_CH341 > 0
      PRINTF("ch341_i2c(%d) packet %d: [ ", dev->port, i);
      for (uint32_t j = 0; j < mLength; j++) {
        PRINTF("%02X ", dev->mOutBuffer[lastPoint + j]);
      }
      PRINTF("] @ %s\n", startTs);
#endif
      ret = CH341_CALL(WriteRead)((ULONG)dev->port, mLength, &dev->mOutBuffer[lastPoint],
                                  mCH341A_CMD_I2C_STM_MAX, 1, &mInLenTmp, &dev->mInBuffer[mInLen]);
      if (ret) {
        lastPoint = dev->mOutPoints[i];
        mInLen += mInLenTmp;
      } else {
        r = -__LINE__;
        ASLOG(ERROR, ("ch341_i2c WriteRead(%d) failed for packet %d\n", dev->port, i));
      }
    }

    if (0 == r) {
#if AS_LOG_CH341I > 0
      Std_GetDateTime(endTs, sizeof(endTs));
      PRINTF("ch341_i2c(%d) respose: [ ", dev->port);
      for (i = 0; i < mInLen; i++) {
        PRINTF("%02X ", dev->mInBuffer[i]);
      }
      PRINTF("] @ %s\n", endTs);
#endif
      r = dev_ch341_i2c_transfer_check(dev, msgs, num, mInLen);
    }
  } else {
    ASLOG(ERROR, ("too much stream, r = %d\n", r));
  }

  return r;
}

static int dev_ch341_i2c_ioctl(void *param, int type, const void *data, size_t size) {
  int r = -__LINE__;
  uint32_t num;

  switch (type) {
  case DEV_IOCTL_SET_TIMEOUT:
    r = 0;
    break;
  case DEV_IOCTL_TRANSFER:
    num = size / sizeof(dev_i2c_msg_t);
    if ((data != nullptr) && (size == (num * sizeof(dev_i2c_msg_t)))) {
      r = dev_ch341_i2c_transfer(PPARAM(param), (dev_i2c_msg_t *)data, num);
    } else {
      r = -__LINE__;
    }
    break;
  case DEV_IOCTL_SET_DELAY:
    num = *(uint32_t *)data;
    if (num <= mCH341A_CMD_I2C_STM_DLY) {
      PPARAM(param)->delayUs = num; /* delay in us */
      r = 0;
    } else if ((num >= 1000) && (num <= (mCH341A_CMD_I2C_STM_DLY * 1000))) {
      PPARAM(param)->delayUs = num; /* delay in ms */
      r = 0;
    } else {
      ASLOG(ERROR, ("ch341_i2c(%d) delay %u is invalid\n", PPARAM(param)->port, num));
    }
    break;
  default:
    break;
  }
  return r;
}
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _WIN32 */
