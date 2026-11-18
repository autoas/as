/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef SOMEIP_PRIV_H
#define SOMEIP_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "TcpIp.h"
#include "sys/queue.h"
/* ================================ [ MACROS    ] ============================================== */
#define DET_THIS_MODULE_ID MODULE_ID_SOMEIPTP

/* ================================ [ TYPES     ] ============================================== */
/* API for service */
typedef void (*SomeIp_OnAvailabilityFncType)(boolean isAvailable);
typedef void (*SomeIp_OnConnectFncType)(uint16_t condId, boolean isConnected);

/* for events */
typedef void (*SomeIp_OnSubscribeFncType)(boolean isSubscribe);

/* API for method */
typedef Std_ReturnType (*SomeIp_OnRequestFncType)(uint32_t requestId, SomeIp_MessageType *req,
                                                  SomeIp_MessageType *res);
typedef Std_ReturnType (*SomeIp_OnFireForgotFncType)(uint32_t requestId, SomeIp_MessageType *req);
typedef Std_ReturnType (*SomeIp_OnAsyncRequestFncType)(uint32_t requestId, SomeIp_MessageType *res);

/* For the LF, set the msg->Data as beginning of the buffer */
/* msg == NULL to indicate transmission error, abort current transmit */
typedef Std_ReturnType (*SomeIp_OnTpCopyRxDataFncType)(uint32_t requestId,
                                                       SomeIp_TpMessageType *msg);
typedef Std_ReturnType (*SomeIp_OnTpCopyTxDataFncType)(uint32_t requestId,
                                                       SomeIp_TpMessageType *msg);

typedef Std_ReturnType (*SomeIp_OnResponseFncType)(uint32_t requestId, SomeIp_MessageType *res);
typedef Std_ReturnType (*SomeIp_OnErrorFncType)(uint32_t requestId, Std_ReturnType ercd);

/* API for events */
typedef Std_ReturnType (*SomeIp_OnNotificationFncType)(uint32_t requestId, SomeIp_MessageType *evt);

typedef struct {
  uint16_t MethodId;
  uint8_t InterfaceVersion;
  SomeIp_OnRequestFncType OnRequest;
  SomeIp_OnFireForgotFncType OnFireForgot;
  SomeIp_OnAsyncRequestFncType OnAsyncRequest;
  SomeIp_OnTpCopyRxDataFncType OnTpCopyRxData;
  SomeIp_OnTpCopyTxDataFncType OnTpCopyTxData;
  uint32_t ResMaxLen;
} SomeIp_ServerMethodType;

typedef struct {
  uint16_t SdHandleID;
  uint16_t EventId;
  uint8_t InterfaceVersion;
  SomeIp_OnTpCopyTxDataFncType OnTpCopyTxData;
} SomeIp_ServerEventType;

typedef struct {
  uint16_t MethodId;
  uint8_t InterfaceVersion;
  SomeIp_OnResponseFncType OnResponse;
  SomeIp_OnErrorFncType OnError;
  SomeIp_OnTpCopyRxDataFncType OnTpCopyRxData;
  SomeIp_OnTpCopyTxDataFncType OnTpCopyTxData;
} SomeIp_ClientMethodType;

typedef struct {
  uint16_t EventId;
  uint8_t InterfaceVersion;
  SomeIp_OnNotificationFncType OnNotification;
  SomeIp_OnTpCopyRxDataFncType OnTpCopyRxData;
} SomeIp_ClientEventType;

/* @SWS_SomeIpXf_00152 */
typedef struct {
  uint16_t ServiceId;
  uint16_t MethodId;
  uint32_t Length;
  uint16_t ClientId;
  uint16_t SessionId;
  uint8_t InterfaceVersion;
  uint8_t MessageType;
  uint8_t ReturnCode;
  boolean IsTpFlag;
} SomeIp_HeaderType;

typedef struct {
  uint32_t Offset;
  boolean MoreSegmentsFlag;
} SomeIp_TpHeaderType;

typedef struct {
  SomeIp_HeaderType Header;
  SomeIp_TpHeaderType TpHeader;
  TcpIp_SockAddrType RemoteAddr;
  SomeIp_MessageType Req;
} SomeIp_MsgType;

typedef struct SomeIp_AsyncReqMsg_s {
  STAILQ_ENTRY(SomeIp_AsyncReqMsg_s) entry;
  TcpIp_SockAddrType RemoteAddr;
  uint16_t ClientId;
  uint16_t SessionId;
  uint16_t MethodId;
} SomeIp_AsyncReqMsgType;

typedef struct SomeIp_RxTpMsg_s {
  STAILQ_ENTRY(SomeIp_RxTpMsg_s) entry;
  TcpIp_SockAddrType RemoteAddr;
  uint32_t Offset;
  uint16_t MethodId; /* this is the key */
  uint16_t ClientId;
  uint16_t SessionId;
  uint16_t Timer;
} SomeIp_RxTpMsgType;

#define SomeIp_RxTpEvtMsg_s SomeIp_RxTpMsg_s
typedef SomeIp_RxTpMsgType SomeIp_RxTpEvtMsgType;

typedef struct SomeIp_TxTpMsg_s {
  STAILQ_ENTRY(SomeIp_TxTpMsg_s) entry;
  TcpIp_SockAddrType RemoteAddr;
  uint32_t Offset;
  uint32_t Length;
  uint16_t MethodId; /* this is the key */
  uint16_t ClientId;
  uint16_t SessionId;
  uint16_t Timer;
  uint8_t MessageType;
#ifndef DISABLE_SOMEIP_TX_NOK_RETRY
  uint8_t RetryCounter;
#endif
} SomeIp_TxTpMsgType;

typedef struct SomeIp_TxTpEvtMsg_s {
  STAILQ_ENTRY(SomeIp_TxTpEvtMsg_s) entry;
  uint32_t Offset;
  uint32_t Length;
  uint16_t EventId; /* this is the key */
  uint16_t SessionId;
  uint16_t Timer;
} SomeIp_TxTpEvtMsgType;

typedef struct SomeIp_WaitResMsg_s {
  STAILQ_ENTRY(SomeIp_WaitResMsg_s) entry;
  uint16_t MethodId;
  uint16_t SessionId;
  uint16_t Timer;
} SomeIp_WaitResMsgType;

/* For TCP large messages */
typedef struct {
  uint8_t *Data;
  uint32_t Length;
  uint32_t Offset;
  uint8_t Header[16];
  uint8_t LenOfHd;
} SomeIp_TpBufferType;

typedef STAILQ_HEAD(rxTpMsgHead, SomeIp_RxTpMsg_s) SomeIp_RxTpMsgList;
typedef STAILQ_HEAD(txTpMsgHead, SomeIp_TxTpMsg_s) SomeIp_TxTpMsgList;
typedef STAILQ_HEAD(txTpEvtMsgHead, SomeIp_TxTpEvtMsg_s) SomeIp_TxTpEvtMsgList;
typedef STAILQ_HEAD(txWaitResMsgHead, SomeIp_WaitResMsg_s) SomeIp_WaitResMsgList;

typedef struct {
  STAILQ_HEAD(ReqMsgHead, SomeIp_AsyncReqMsg_s) PendingAsyncReqMsgs;
  SomeIp_RxTpMsgList PendingRxTpMsgs;
  SomeIp_TxTpMsgList PendingTxTpMsgs;
  SomeIp_TxTpEvtMsgList PendingTxTpEvtMsgs;
  bool Online;
  bool TakenControled;
} SomeIp_ServerConnectionContextType;

typedef struct {
  SomeIp_RxTpMsgList PendingRxTpEvtMsgs;
  SomeIp_RxTpMsgList PendingRxTpMsgs;
  SomeIp_TxTpMsgList PendingTxTpMsgs;
  SomeIp_WaitResMsgList PendingWaitResMsgs;
  uint16_t SessionId;
  boolean Online;
} SomeIp_ClientServiceContextType;

typedef struct {
  SomeIp_ServerConnectionContextType *Context;
  PduIdType RxPduId; /* SOMEIP RxPduId used by Rx Control */
  PduIdType TxPduId;
  SoAd_SoConIdType SoConId;
} SomeIp_ServerConnectionType;

typedef struct {
  SomeIp_TxTpEvtMsgList PendingTxTpEvtMsgs;
  uint16_t SessionId;
  boolean Online;
} SomeIp_ServerContextType;

typedef struct {
  uint16_t ServiceId;
  uint16_t ClientId;
  uint16_t SdHandleID;
  const SomeIp_ServerMethodType *Methods;
  uint16_t NumOfMethods;
  const SomeIp_ServerEventType *Events;
  uint16_t NumOfEvents;
  const SomeIp_ServerConnectionType *Connections;
  uint8_t NumOfConnections;
  TcpIp_ProtocolType Protocol;
  SomeIp_ServerContextType *Context;
  uint16_t SeparationTime; /* @ECUC_SomeIpTp_00006 */
  SomeIp_OnConnectFncType OnConnect;
} SomeIp_ServerServiceType;

typedef struct {
  uint16_t ServiceId;
  uint16_t ClientId;
  uint16_t SdHandleID;
  const SomeIp_ClientMethodType *Methods;
  uint16_t NumOfMethods;
  const SomeIp_ClientEventType *Events;
  uint16_t NumOfEvents;
  SomeIp_ClientServiceContextType *Context;
  PduIdType TxPduId;
  SomeIp_OnAvailabilityFncType OnAvailability;
  uint16_t SeparationTime; /* @ECUC_SomeIpTp_00006 */
  uint16_t ResponseTimeout;
} SomeIp_ClientServiceType;

typedef struct {
  boolean IsServer;
  SoAd_SoConIdType SoConId;
  const void *Service;
} SomeIp_ServiceType;

struct SomeIp_Config_s {
  /* @ECUC_SomeIpTp_00023 */
  uint16_t TpRxTimeoutTime;
  const SomeIp_ServiceType *Services;
  uint16_t NumOfService;
  const uint16_t *PID2ServiceMap;
  const uint16_t *PID2ServiceConnectionMap;
  uint16_t NumOfPIDs;
  const uint16_t *TxMethod2ServiceMap;
  const uint16_t *TxMethod2PerServiceMap;
  uint16_t NumOfTxMethods;
  const uint16_t *TxEvent2ServiceMap;
  const uint16_t *TxEvent2PerServiceMap;
  uint16_t NumOfTxEvents;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */

#endif /* SOMEIP_PRIV_H */
