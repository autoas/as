/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 *
 * ref: https://blog.csdn.net/WCH_TechGroup/article/details/130928825
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

#ifdef ERROR
#undef ERROR
#endif
/* ================================ [ MACROS    ] ============================================== */
#define PPARAM(p) ((Dev_CH341SPIParamType *)p)

#define AS_LOG_CH341 0
#define AS_LOG_CH341I 1

#define CH341_LOAD(sym)                                                                            \
  if (0 == r) {                                                                                    \
    *(void **)&CH341_SPI_Interface.CH341##sym = PAL_DlSym(CH341_SPI_Interface.dll, "CH341" #sym);  \
    if (NULL == CH341_SPI_Interface.CH341##sym) {                                                  \
      r = -__LINE__;                                                                               \
      ASLOG(ERROR, ("Can't find symbold " #sym "\n"));                                             \
    }                                                                                              \
  }

#define CH341_CALL(sym) CH341_SPI_Interface.CH341##sym

#ifndef CH341_SPI_MAX_SIZE
#define CH341_SPI_MAX_SIZE 4096
#endif

/* don't define CH341_SPI_CS_BY_PIN as I see issues */
#ifdef CH341_SPI_CS_BY_PIN
#define CH341_SPI_CS 0
#else
#define CH341_SPI_CS (0x80 | dev->cs)
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  int Port;
  int cs;
  uint8_t buffer[CH341_SPI_MAX_SIZE];
} Dev_CH341SPIParamType;

typedef struct {
  void *dll;
  HANDLE (*CH341OpenDevice)(ULONG);
  VOID (*CH341CloseDevice)(ULONG);
  BOOL (*CH341SetStream)(ULONG, ULONG);
  BOOL (*CH341StreamSPI4)(ULONG, ULONG, ULONG, PVOID);
  BOOL (*CH341Set_D5_D0)(ULONG, ULONG, ULONG);
} CH341_SPI_InterfaceType;
/* ================================ [ DECLARES  ] ============================================== */
static int Dev_Ch341SpiOpen(const char *device, const char *option, void **param);
static int Dev_Ch341SpiRead(void *param, uint8_t *data, size_t size);
static int Dev_Ch341SpiWrite(void *param, const uint8_t *data, size_t size);
static void Dev_Ch341SpiClose(void *param);
static int Dev_Ch341SpiIoCtl(void *param, int type, const void *data, size_t size);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const Dev_DeviceOpsType Dev_Ch341SpiOps = {
  .Name = "CH341_SPI/",
  .Open = Dev_Ch341SpiOpen,
  .Read = Dev_Ch341SpiRead,
  .Write = Dev_Ch341SpiWrite,
  .IoCtl = Dev_Ch341SpiIoCtl,
  .Close = Dev_Ch341SpiClose,
};

static CH341_SPI_InterfaceType CH341_SPI_Interface = {
  NULL,
};
/* ================================ [ LOCALS    ] ============================================== */
static int openDriver(void) {
  int r = 0;
  CH341_SPI_Interface.dll = PAL_DlOpen("CH341DLLA64.DLL");
  if (NULL == CH341_SPI_Interface.dll) {
    r = -__LINE__;
    ASLOG(ERROR, ("Can't open CH341DLLA64.DLL\n"));
  }

  CH341_LOAD(OpenDevice);
  CH341_LOAD(CloseDevice);
  CH341_LOAD(SetStream);
  CH341_LOAD(StreamSPI4);
  CH341_LOAD(Set_D5_D0);

  if (0 != r) {
    if (NULL != CH341_SPI_Interface.dll) {
      PAL_DlClose(CH341_SPI_Interface.dll);
      CH341_SPI_Interface.dll = NULL;
    }
  }
  return r;
}

static int Dev_Ch341SpiOpen(const char *device, const char *option, void **param) {
  int baudrate, port = 0, cs = 0, bitOrder = 1;
  int r = 0;
  int len = strlen(device);
  HANDLE handle;
  ULONG iMode = 0;
  BOOL ret;

  /* device in format: CH341_SPI/[cs,bitorder/]port, e.g. "CH341_SPI/0,1/0" */
  if ((len >= 11) && (device[10] >= '0') && (device[10] <= '9')) {
    if (15 == len) {
      port = device[14] - '0';
      if ((',' == device[11]) && ('/' == device[13])) {
        if ((device[10] >= '0') && (device[10] <= '9')) {
          cs = device[10] - '0';
        } else {
          r = -__LINE__;
        }
        if ((device[12] >= '0') && (device[12] <= '9')) {
          bitOrder = device[12] - '0';
        } else {
          r = -__LINE__;
        }
      } else {
        r = -__LINE__;
      }
    } else if (11 == len) {
      port = device[10] - '0';
      cs = 0;
      bitOrder = 1;
    } else {
      r = -__LINE__;
    }
  } else {
    r = -__LINE__;
  }

  if (0 != r) {
    ASLOG(ERROR,
          ("invalid device format '%s' for CH341 SPI, expect in format: "
           "CH341_SPI/[cs,bitorder/]port, e.g. 'CH341_SPI/0,1/0' for cs=0, bit order=1, port=0\n",
           device));
  }

  /* option format "Baudrate", e.g. "544000" */
  baudrate = atoi(option);

  switch (baudrate) {
  case 544000: /* speed is about 68K bytes but measured about 1.5Mbps */
    iMode = 0;
    break;
  default:
    ASLOG(ERROR, ("invalid baudrate for CH341 SPI, please choose from 544000\n"));
    r = -__LINE__;
    break;
  }

  if ((cs >= 0) && (cs <= 2)) {
    /* use D0 D1 D2 as cs */
    /* D3 is SCK, D5 is MOSI, D7 is MISO */
  } else {
    ASLOG(ERROR, ("invalid cs for CH341 SPI, please choose from 0,1,2\n"));
    r = -__LINE__;
  }

  /* Bit 7: Bit order in SPI bytes, 0= low first, 1= high first */
  if (0 == bitOrder) {
    /* pass */
  } else if (1 == bitOrder) {
    iMode |= 0x80;
  } else {
    ASLOG(ERROR, ("invalid byte order for CH341 SPI, please choose from 0,1, 0 mean low first, 1 "
                  "high first\n"));
    r = -__LINE__;
  }

  if (0 == r) {
    if (NULL == CH341_SPI_Interface.dll) {
      r = openDriver();
    }
  }

  if (0 == r) {
    handle = CH341_CALL(OpenDevice)((ULONG)port);
    if (NULL == handle) {
      ASLOG(ERROR, ("Can't open CH341 port %d\n", port));
      r = -__LINE__;
    } else {
      /* Bit 2: SPI I/O number /IO pins
       *        0= single in/single out (D3 clock /D5 out /D7 in)(default),
       *        1= double in/double out (D3 clock /D5 out D4 out /D7 in D6 in) */
      ret = CH341_CALL(SetStream)((ULONG)port, iMode);
      if (TRUE != ret) {
        ASLOG(ERROR, ("Can't set CH341 port %d to mode 0x%lx\n", port, iMode));
        r = -__LINE__;
      }
    }
  }

  if (0 == r) {
    *param = malloc(sizeof(Dev_CH341SPIParamType));
    memset(*param, 0, sizeof(Dev_CH341SPIParamType));
    PPARAM(*param)->Port = port;
    PPARAM(*param)->cs = cs;
    ASLOG(INFO, ("ch341_spi open(%d, baudrate=%d, cs=%d, %sSB) OK\n", port, baudrate, cs,
                 bitOrder ? "M" : "L"));
  }

  /* For this device:
   *   DCK/D3 pin is clock output, default low level --> CPOL = 0
   *   DOUT/D5 pin is output during low level before clock rising edge --> CPHA = 1 */

  return r;
}

static int Dev_Ch341SpiRead(void *param, uint8_t *data, size_t size) {
  int len = -1;
  BOOL ret;
  Dev_CH341SPIParamType *dev = PPARAM(param);

  if (size <= CH341_SPI_MAX_SIZE) {
    memset(dev->buffer, 0xFF, size);
    ret = CH341_CALL(StreamSPI4)((ULONG)dev->Port, 0x80 | dev->cs, (ULONG)size, dev->buffer);
    if (TRUE == ret) {
      memcpy(data, dev->buffer, size);
      len = (int)size;
    } else {
      ASLOG(ERROR, ("CH341 port %d read failed\n", dev->Port));
    }
  } else {
    ASLOG(ERROR, ("CH341 port %d read failed as buffer too small\n", dev->Port));
  }

  return len;
}

static int Dev_Ch341SpiWrite(void *param, const uint8_t *data, size_t size) {
  int len = -1;
  BOOL ret;
  Dev_CH341SPIParamType *dev = PPARAM(param);

  if (size <= CH341_SPI_MAX_SIZE) {
    memcpy(dev->buffer, data, size);
    ret = CH341_CALL(StreamSPI4)((ULONG)dev->Port, 0x80 | dev->cs, (ULONG)size, dev->buffer);
    if (TRUE == ret) {
      len = (int)size;
    } else {
      ASLOG(ERROR, ("CH341 port %d write failed\n", dev->Port));
    }
  } else {
    ASLOG(ERROR, ("CH341 port %d write failed as buffer too small\n", dev->Port));
  }

  return len;
}

static void Dev_Ch341SpiClose(void *param) {
  CH341_CALL(CloseDevice)((ULONG)PPARAM(param)->Port);
  free(param);
}

static int dev_ch341_spi_transfer(Dev_CH341SPIParamType *dev, Dev_SpiMsgType *msgs, uint32_t num) {
  int r = 0;
  BOOL ret;
  uint32_t i;
  Dev_SpiMsgType *msg;
#ifdef CH341_SPI_CS_BY_PIN
  ret = CH341_CALL(Set_D5_D0)((ULONG)dev->Port, 0xFF, (~(1 << dev->cs)) & 0xFF);
  if (TRUE != ret) {
    ASLOG(ERROR, ("Can't do chip select %d for CH341 port %d\n", dev->cs, dev->Port));
    r = -1;
  }
#endif
  for (i = 0; (i < num) && (0 == r); i++) {
    msg = &msgs[i];
    if (msg->Src) {
      if (msg->Len <= CH341_SPI_MAX_SIZE) {
        memcpy(dev->buffer, msg->Src, msg->Len);
        ret = CH341_CALL(StreamSPI4)((ULONG)dev->Port, CH341_SPI_CS, (ULONG)msg->Len, dev->buffer);
        if (TRUE == ret) {
          if (msg->Dst) {
            memcpy(msg->Dst, dev->buffer, msg->Len);
          }
        } else {
          ASLOG(ERROR, ("CH341 port %d transfer with source failed\n", dev->Port));
          r = -1;
        }
      } else {
        r = -1;
        ASLOG(ERROR, ("CH341 port %d transfer failed as buffer too small\n", dev->Port));
      }
    } else {
      if (msg->Len <= CH341_SPI_MAX_SIZE) {
        memset(dev->buffer, 0xFF, msg->Len);
        ret = CH341_CALL(StreamSPI4)((ULONG)dev->Port, CH341_SPI_CS, (ULONG)msg->Len, dev->buffer);
        if (TRUE != ret) {
          ASLOG(ERROR, ("CH341 port %d transfer without source failed\n", dev->Port));
          r = -1;
        } else {
          memcpy(msg->Dst, dev->buffer, msg->Len);
        }
      } else {
        r = -1;
        ASLOG(ERROR, ("CH341 port %d transfer failed as buffer too small\n", dev->Port));
      }
    }
  }

#ifdef CH341_SPI_CS_BY_PIN
  CH341_CALL(Set_D5_D0)((ULONG)dev->Port, 0xFF, 0xFF);
#endif

  return r;
}

static int Dev_Ch341SpiIoCtl(void *param, int type, const void *data, size_t size) {
  int r = -__LINE__;
  uint32_t num;

  switch (type) {
  case DEV_IOCTL_SET_TIMEOUT:
    r = 0;
    break;
  case DEV_IOCTL_TRANSFER:
    num = size / sizeof(Dev_SpiMsgType);
    if ((data != NULL) && (size == (num * sizeof(Dev_SpiMsgType)))) {
      r = dev_ch341_spi_transfer(PPARAM(param), (Dev_SpiMsgType *)data, num);
    } else {
      r = -__LINE__;
    }
    break;
  default:
    break;
  }
  return r;
}
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _WIN32 */
