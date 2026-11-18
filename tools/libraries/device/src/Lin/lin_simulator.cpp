/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include "Std_Types.h"
#include "linlib.hpp"
#include "Std_Debug.h"
#include "Std_Timer.h"
#include "TcpIp.h"
/* ================================ [ MACROS    ] ============================================== */
#define LIN_PORT_MIN 100
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint32_t BusId;
  TcpIp_SocketIdType Socket;
} Dev_LinSocketBusType;
/* ================================ [ DECLARES  ] ============================================== */
static int Dev_SocketOpen(Dev_LinDeviceType *dev, const char *option);
static int Dev_SocketWrite(Dev_LinDeviceType *dev, Dev_LinFrameType *frame);
static int Dev_SocketRead(Dev_LinDeviceType *dev, Dev_LinFrameType *frame);
static void Dev_SocketClose(Dev_LinDeviceType *dev);
static int Dev_SocketIoctl(Dev_LinDeviceType *dev, int type, const void *data, size_t size);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const Dev_LinDeviceOpsType Dev_LinSimulatorOps = {
  .Name = "simulator/",
  .Open = Dev_SocketOpen,
  .Close = Dev_SocketClose,
  .Write = Dev_SocketWrite,
  .Read = Dev_SocketRead,
  .IoCtl = Dev_SocketIoctl,
};
/* ================================ [ LOCALS    ] ============================================== */
static int Dev_SocketOpen(Dev_LinDeviceType *dev, const char *option) {
  int r = 0;
  Std_ReturnType ercd;
  uint32_t busid = (uint32_t)atoi(&dev->Name.c_str()[14]);
  TcpIp_SocketIdType s;
  TcpIp_SockAddrType sockAddr;
  Dev_LinSocketBusType *bus;

  TcpIp_Init(nullptr);
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
    bus = new Dev_LinSocketBusType;
    if (bus != nullptr) {
      bus->BusId = busid;
      bus->Socket = s;
      dev->Param = (void *)bus;
    } else {
      r = -3;
    }
  }

  return r;
}

static int Dev_SocketWrite(Dev_LinDeviceType *dev, Dev_LinFrameType *frame) {
  int r = DEVLIN_MTU;
  Std_ReturnType ercd;
  Dev_LinSocketBusType *bus = (Dev_LinSocketBusType *)dev->Param;

  ercd = TcpIp_Send(bus->Socket, (uint8_t *)frame, DEVLIN_MTU);
  if (ercd != E_OK) {
    ASLOG(ERROR, ("LIN Socket %s send message failed!\n", dev->Name.c_str()));
    r = -1;
  }

  return r;
}

static int Dev_SocketRead(Dev_LinDeviceType *dev, Dev_LinFrameType *frame) {
  int r = 0;
  Std_ReturnType ercd;
  Dev_LinSocketBusType *bus = (Dev_LinSocketBusType *)dev->Param;
  uint32_t len = DEVLIN_MTU;

  ercd = TcpIp_Recv(bus->Socket, (uint8_t *)frame, &len);
  if ((len == DEVLIN_MTU) && (E_OK == ercd)) {
    r = DEVLIN_MTU;
  } else if (E_OK == ercd) {
    /* pass */
  } else {
    r = -2;
  }

  if (r < 0) {
    ASLOG(ERROR, ("LIN Socket %s read message failed!\n", dev->Name.c_str()));
  }

  return r;
}

static void Dev_SocketClose(Dev_LinDeviceType *dev) {
  Dev_LinSocketBusType *bus = (Dev_LinSocketBusType *)dev->Param;
  TcpIp_Close(bus->Socket, TRUE);
  delete bus;
}

static int Dev_SocketIoctl(Dev_LinDeviceType *dev, int type, const void *data, size_t size) {
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
