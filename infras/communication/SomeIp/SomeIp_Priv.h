/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef _SOMEIP_PRIV_H
#define _SOMEIP_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "TcpIp.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef Std_ReturnType (*SomeIp_RequestFncType)(const uint8_t *reqData, uint32_t reqLen,
                                                uint8_t *resData, uint32_t *resLen);

typedef Std_ReturnType (*SomeIp_ResponseFncType)(const uint8_t *resData, uint32_t resLen);

typedef Std_ReturnType (*SomeIp_NotificationFncType)(const uint8_t *evtData, uint32_t evtLen);

typedef struct {
  uint16_t methodId;
  uint8_t interfaceVersion;
  SomeIp_RequestFncType requestFnc;
} SomeIp_ServerMethodType;

typedef struct {
  uint16_t sdHandleID;
  uint16_t eventId;
  uint8_t interfaceVersion;
} SomeIp_ServerEventType;

typedef struct {
  uint16_t methodId;
  uint8_t interfaceVersion;
  SomeIp_ResponseFncType responseFnc;
} SomeIp_ClientMethodType;

typedef struct {
  uint16_t eventId;
  uint8_t interfaceVersion;
  SomeIp_NotificationFncType notifyFnc;
} SomeIp_ClientEventType;

typedef struct {
  uint16_t sessionId;
  uint32_t txLen;
} SomeIp_ServerServiceContextType;

typedef struct {
  uint16_t sessionId;
} SomeIp_ClientServiceContextType;

typedef struct {
  SomeIp_ServerServiceContextType *context;
  uint8_t *buffer;
  uint16_t bufferLen;
  PduIdType TxPduId;
} SomeIp_ServerConnectionType;

typedef struct {
  uint16_t serviceId;
  uint16_t clientId;
  const SomeIp_ServerMethodType *methods;
  uint16_t numOfMethods;
  const SomeIp_ServerEventType *events;
  uint16_t numOfEvents;
  const SomeIp_ServerConnectionType *connections;
  uint8_t numOfConnections;
} SomeIp_ServerServiceType;

typedef struct {
  uint16_t serviceId;
  uint16_t clientId;
  uint16_t sdHandleID;
  const SomeIp_ClientMethodType *methods;
  uint16_t numOfMethods;
  const SomeIp_ClientEventType *events;
  uint16_t numOfEvents;
  SomeIp_ClientServiceContextType *context;
  uint8_t *buffer;
  uint16_t bufferLen;
  PduIdType TxPduId;
  TcpIp_ProtocolType protocol;
} SomeIp_ClientServiceType;

typedef struct {
  boolean isServer;
  const void *service;
} SomeIp_ServiceType;

struct SomeIp_Config_s {
  const SomeIp_ServiceType *services;
  uint16_t numOfService;
  const uint16_t* PID2ServiceMap;
  const uint16_t* PID2ServiceConnectionMap;
  uint16_t numOfPIDs;
  const uint16_t *TxMethod2ServiceMap;
  const uint16_t *TxMethod2PerServiceMap;
  uint16_t numOfTxMethods;
  const uint16_t *TxEvent2ServiceMap;
  const uint16_t *TxEvent2PerServiceMap;
  uint16_t numOfTxEvents;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */

#endif /* _SOMEIP_PRIV_H */
