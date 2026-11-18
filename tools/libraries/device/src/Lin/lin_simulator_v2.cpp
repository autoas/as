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

#if defined(_WIN32)
#include <objbase.h>
#else
#include <uuid/uuid.h>
#endif

#ifdef ERROR
#undef ERROR
#endif
/* ================================ [ MACROS    ] ============================================== */
#define LIN_CAST_IP TCPIP_IPV4_ADDR(224, 244, 224, 245)
#define LIN_PORT_MIN 10000

#if defined(_WIN32)
#define LIN_UUID_LENGTH sizeof(GUID)
#else
#define LIN_UUID_LENGTH 16
#endif
/* ================================ [ TYPES     ] ============================================== */
struct lin_frame {
  Dev_LinFrameType frame;
  uint8_t uuid[LIN_UUID_LENGTH];
};

typedef struct {
  uint32_t BusId;
  uint32_t Port;
  TcpIp_SocketIdType SockRd;
  TcpIp_SocketIdType SockWt;
  uint8_t Uuid[LIN_UUID_LENGTH];
} Dev_LinSocketBusType;
/* ================================ [ DECLARES  ] ============================================== */
static int Dev_SocketOpen(Dev_LinDeviceType *dev, const char *option);
static int Dev_SocketWrite(Dev_LinDeviceType *dev, Dev_LinFrameType *frame);
static int Dev_SocketRead(Dev_LinDeviceType *dev, Dev_LinFrameType *frame);
static void Dev_SocketClose(Dev_LinDeviceType *dev);
static int Dev_SocketIoctl(Dev_LinDeviceType *dev, int type, const void *data, size_t size);
/* ================================ [ DATAS     ] ============================================== */
extern "C" const Dev_LinDeviceOpsType Dev_LinSimulatorV2Ops = {
  .Name = "simulator_v2/",
  .Open = Dev_SocketOpen,
  .Close = Dev_SocketClose,
  .Write = Dev_SocketWrite,
  .Read = Dev_SocketRead,
  .IoCtl = Dev_SocketIoctl,
};
/* ================================ [ LOCALS    ] ============================================== */
#ifdef _WIN32
static void Dev_GetUuid(uint8_t *uuid, size_t length) {
  GUID guid;
  CoCreateGuid(&guid);
  memcpy(uuid, &guid, length);
}
#else
static void Dev_GetUuid(uint8_t *uuid, size_t length) {
  uuid_t uu;
  uuid_generate(uu);
  memcpy(uuid, uu, length);
}
#endif

static int Dev_SocketOpen(Dev_LinDeviceType *dev, const char *option) {
  Std_ReturnType ret = E_NOT_OK;
  uint32_t busid = (uint32_t)atoi(&dev->Name.c_str()[17]);
  TcpIp_SocketIdType sockRd, sockWt;
  TcpIp_SockAddrType ipv4Addr;
  uint16_t Port;
  Dev_LinSocketBusType *bus;

  TcpIp_Init(nullptr);
  sockRd = TcpIp_Create(TCPIP_IPPROTO_UDP);
  if (sockRd >= 0) {
    Port = LIN_PORT_MIN + busid;
    ret = TcpIp_SetNonBlock(sockRd, FALSE);
    if (E_OK == ret) {
      ret = TcpIp_SetTimeout(sockRd, 10);
    }
    if (E_OK == ret) {
      ret = TcpIp_Bind(sockRd, TCPIP_LOCALADDRID_ANY, &Port);
    }
    if (E_OK == ret) {
      TcpIp_SetupAddrFrom(&ipv4Addr, LIN_CAST_IP, Port);
      ret = TcpIp_AddToMulticast(sockRd, &ipv4Addr);
    }
    if (E_OK != ret) {
      TcpIp_Close(sockRd, TRUE);
      ASLOG(ERROR, ("LIN socket bind to %x:%d failed\n", LIN_CAST_IP, Port));
    }
  } else {
    ret = E_NOT_OK;
    ASLOG(ERROR, ("LIN socket create read sock failed\n"));
  }
  if (E_OK == ret) {
    sockWt = TcpIp_Create(TCPIP_IPPROTO_UDP);
    if (sockWt < 0) {
      TcpIp_Close(sockRd, TRUE);
      TcpIp_Close(sockWt, TRUE);
      ret = E_NOT_OK;
      ASLOG(ERROR, ("LIN socket create write sock failed\n"));
    }
  }

  if (E_OK == ret) {
    bus = new Dev_LinSocketBusType;
    if (bus != nullptr) {
      bus->BusId = busid;
      bus->Port = Port;
      dev->Param = (void *)bus;
      bus->SockRd = sockRd;
      bus->SockWt = sockWt;
      Dev_GetUuid(bus->Uuid, sizeof(bus->Uuid));
    } else {
      ret = E_NOT_OK;
    }
  }

  return (int)ret;
}

static int Dev_SocketWrite(Dev_LinDeviceType *dev, Dev_LinFrameType *frame) {
  int r = DEVLIN_MTU;
  Std_ReturnType ercd;
  Dev_LinSocketBusType *bus = (Dev_LinSocketBusType *)dev->Param;
  struct lin_frame lframe;
  TcpIp_SockAddrType RemoteAddr;

  lframe.frame = *frame;
  memcpy(lframe.uuid, bus->Uuid, sizeof(lframe.uuid));
  TcpIp_SetupAddrFrom(&RemoteAddr, LIN_CAST_IP, bus->Port);
  ercd = TcpIp_SendTo(bus->SockWt, &RemoteAddr, (const uint8_t *)&lframe, sizeof(lframe));
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
  struct lin_frame lframe;
  TcpIp_SockAddrType RemoteAddr;
  uint32_t len = sizeof(lframe);

  ercd = TcpIp_RecvFrom(bus->SockRd, &RemoteAddr, (uint8_t *)&lframe, &len);
  if ((len == sizeof(lframe)) && (E_OK == ercd)) {
    if (0 != memcmp(lframe.uuid, bus->Uuid, sizeof(lframe.uuid))) {
      *frame = lframe.frame;
      r = DEVLIN_MTU;
    } else {
      /* pass, send by me */
    }
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
  TcpIp_Close(bus->SockRd, TRUE);
  TcpIp_Close(bus->SockWt, TRUE);
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
