/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/queue.h>
#include <pthread.h>

#include "Std_Types.h"
#include "linlib.h"
#include "Std_Debug.h"
#include "Std_Timer.h"
#include "TcpIp.h"
/* ================================ [ MACROS    ] ============================================== */
#define LIN_PORT_MIN 100
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint32_t busid;
  TcpIp_SocketIdType s;
} Lin_SocketBusType;
/* ================================ [ DECLARES  ] ============================================== */
static int socket_open(Lin_DeviceType *dev, const char *option);
static int socket_write(Lin_DeviceType *dev, Lin_FrameType *frame);
static int socket_read(Lin_DeviceType *dev, Lin_FrameType *frame);
static void socket_close(Lin_DeviceType *dev);
static int socket_ioctl(Lin_DeviceType *dev, int type, const void *data, size_t size);
/* ================================ [ DATAS     ] ============================================== */
const Lin_DeviceOpsType lin_simulator_ops = {
  .name = "simulator/",
  .open = socket_open,
  .close = socket_close,
  .write = socket_write,
  .read = socket_read,
  .ioctl = socket_ioctl,
};
/* ================================ [ LOCALS    ] ============================================== */
static int socket_open(Lin_DeviceType *dev, const char *option) {
  int r = 0;
  Std_ReturnType ercd;
  uint32_t busid = (uint32_t)atoi(&dev->name[14]);
  TcpIp_SocketIdType s;
  TcpIp_SockAddrType sockAddr;
  Lin_SocketBusType *bus;

  TcpIp_Init(NULL);
  TcpIp_SetupAddrFrom(&sockAddr, TCPIP_IPV4_ADDR(127, 0, 0, 1), LIN_PORT_MIN + busid);
  s = TcpIp_Create(TCPIP_IPPROTO_TCP);
  if (s < 0) {
    ASLOG(ERROR, ("LIN Socket busid=%d open failed!\n", busid));
    r = -1;
  }

  if (0 == r) {
    ercd = TcpIp_TcpConnect(s, &sockAddr);
    if (ercd != E_OK) {
      ASLOG(ERROR, ("connect function failed\n"));
      TcpIp_Close(s, TRUE);
      r = -2;
    }
  }

  if (0 == r) {
    bus = malloc(sizeof(Lin_SocketBusType));
    if (bus != NULL) {
      bus->busid = busid;
      bus->s = s;
      dev->param = (void *)bus;
    } else {
      r = -3;
    }
  }

  return r;
}

static int socket_write(Lin_DeviceType *dev, Lin_FrameType *frame) {
  int r = LIN_MTU;
  Std_ReturnType ercd;
  Lin_SocketBusType *bus = dev->param;

  ercd = TcpIp_Send(bus->s, (uint8_t *)frame, LIN_MTU);
  if (ercd != E_OK) {
    ASLOG(ERROR, ("LIN Socket %s send message failed!\n", dev->name));
    r = -1;
  }

  return r;
}

static int socket_read(Lin_DeviceType *dev, Lin_FrameType *frame) {
  int r = 0;
  Std_ReturnType ercd;
  Lin_SocketBusType *bus = dev->param;
  uint32_t len = LIN_MTU;

  ercd = TcpIp_Recv(bus->s, (uint8_t *)frame, &len);
  if ((len == LIN_MTU) && (E_OK == ercd)) {
    r = LIN_MTU;
  } else if (E_OK == ercd) {
    /* pass */
  } else {
    r = -2;
  }

  if (r < 0) {
    ASLOG(ERROR, ("LIN Socket %s read message failed!\n", dev->name));
  }

  return r;
}

static void socket_close(Lin_DeviceType *dev) {
  Lin_SocketBusType *bus = dev->param;
  TcpIp_Close(bus->s, TRUE);
  free(bus);
}

static int socket_ioctl(Lin_DeviceType *dev, int type, const void *data, size_t size) {
  int r = -__LINE__;

  switch (type) {
  case DEV_IOCTL_SET_TIMEOUT:
    r = 0;
    break;
  default:
    break;
  }
  return r;
}
/* ================================ [ FUNCTIONS ] ============================================== */
