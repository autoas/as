/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Ref: Specification of SOME/IP Transformer AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "SomeIp.h"
#include "SomeIp_Cfg.h"
#include "SomeIp_Priv.h"
#include "Std_Debug.h"
#include "Std_Critical.h"
#include "Sd.h"
#include "mempool.h"
#include <string.h>
#ifdef USE_PCAP
#include "pcap.h"
#endif

#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_SOMEIP 0
#define AS_LOG_SOMEIPI 0
#define AS_LOG_SOMEIPW 0
#define AS_LOG_SOMEIPE 4

#define SD_FLG_EVENT_GROUP_MULTICAST 0x10u

/* @SWS_SomeIpXf_00031 */
#define SOMEIP_MSG_REQUEST 0x00u
#define SOMEIP_MSG_REQUEST_NO_RETURN 0x01u
#define SOMEIP_MSG_NOTIFICATION 0x02u
#define SOMEIP_MSG_RESPONSE 0x80u
#define SOMEIP_MSG_ERROR 0x81u

#define SOMEIP_MSG_REQUEST_ACK 0x40u
#define SOMEIP_MSG_REQUEST_NO_RETURN_ACK 0x41u
#define SOMEIP_MSG_NOTIFICATION_ACK 0x42u

#define SOMEIP_MSG_RESPONSE_ACK 0xC0u
#define SOMEIP_MSG_ERROR_ACK 0xC1u

/* @PRS_SOMEIP_00367 */
#define SOMEIP_TP_FLAG 0x20u

#define SOMEIP_SF_MAX 1396u
#define SOMEIP_TP_MAX 1392u

#define SOMEIP_TP_FRAME_MAX (SOMEIP_TP_MAX + 20u)

#define SOMEIP_CONFIG (someIpConfigPtr)

#ifndef SOMEIP_TX_NOK_RETRY_MAX
#define SOMEIP_TX_NOK_RETRY_MAX 3u
#endif

#ifdef USE_PCAP
#define PCAP_TRACE PCap_SomeIp
#else
#define PCAP_TRACE(serviceId, methodId, interfaceVersion, messageType, returnCode, payload,        \
                   payloadLength, clientId, sessionId, RemoteAddr, isTp, offset, more, isRx)
#endif

#define IS_TP_ENABLED(obj) (NULL != (obj)->OnTpCopyTxData)

/* SQP: SOMEIP Queue and Pool */
#define DEF_SQP(T, size)                                                                           \
  static SomeIp_##T##Type someIp##T##Slots[size];                                                  \
  static mempool_t someIp##T##Pool;

#define DEC_SQP(T)                                                                                 \
  SomeIp_##T##Type *var;                                                                           \
  SomeIp_##T##Type *next

#define SQP_INIT(T)                                                                                \
  do {                                                                                             \
    mp_init(&someIp##T##Pool, (uint8_t *)&someIp##T##Slots, sizeof(SomeIp_##T##Type),              \
            ARRAY_SIZE(someIp##T##Slots));                                                         \
  } while (0u)

#define SQP_FIRST(T)                                                                               \
  do {                                                                                             \
    var = STAILQ_FIRST(&context->Pending##T##s);                                                   \
  } while (0u)

#define SQP_NEXT()                                                                                 \
  do {                                                                                             \
    EnterCritical();                                                                               \
    next = STAILQ_NEXT(var, entry);                                                                \
    ExitCritical();                                                                                \
  } while (0u)

#define SQP_WHILE(T)                                                                               \
  SQP_FIRST(T);                                                                                    \
  while (NULL != var) {                                                                            \
    SQP_NEXT();

#define SQP_WHILE_END()                                                                            \
  var = next;                                                                                      \
  }

/* CRM: context RM */
#define SQP_CRM_AND_FREE(T)                                                                        \
  do {                                                                                             \
    EnterCritical();                                                                               \
    STAILQ_REMOVE(&context->Pending##T##s, var, SomeIp_##T##_s, entry);                            \
    ExitCritical();                                                                                \
    mp_free(&someIp##T##Pool, (uint8_t *)var);                                                     \
  } while (0u)

/* LRM: list RM */
#define SQP_LRM_AND_FREE(T)                                                                        \
  do {                                                                                             \
    EnterCritical();                                                                               \
    STAILQ_REMOVE(pending##T##s, var, SomeIp_##T##_s, entry);                                      \
    ExitCritical();                                                                                \
    mp_free(&someIp##T##Pool, (uint8_t *)var);                                                     \
  } while (0u)

/* Context Append */
#define SQP_CAPPEND(T)                                                                             \
  do {                                                                                             \
    EnterCritical();                                                                               \
    STAILQ_INSERT_TAIL(&context->Pending##T##s, var, entry);                                       \
    ExitCritical();                                                                                \
  } while (0u)

/* List Append */
#define SQP_LAPPEND(T)                                                                             \
  do {                                                                                             \
    EnterCritical();                                                                               \
    STAILQ_INSERT_TAIL(pending##T##s, var, entry);                                                 \
    ExitCritical();                                                                                \
  } while (0u)

#define SQP_ALLOC(T)                                                                               \
  do {                                                                                             \
    var = (SomeIp_##T##Type *)mp_alloc(&someIp##T##Pool);                                          \
  } while (0u)

#define SQP_FREE(T)                                                                                \
  do {                                                                                             \
    mp_free(&someIp##T##Pool, (uint8_t *)var);                                                     \
  } while (0u)

#define SQP_CLEAR(T)                                                                               \
  do {                                                                                             \
    SomeIp_##T##Type *var;                                                                         \
    EnterCritical();                                                                               \
    var = STAILQ_FIRST(&context->Pending##T##s);                                                   \
    while (NULL != var) {                                                                          \
      STAILQ_REMOVE_HEAD(&context->Pending##T##s, entry);                                          \
      mp_free(&someIp##T##Pool, (uint8_t *)var);                                                   \
      var = STAILQ_FIRST(&context->Pending##T##s);                                                 \
    }                                                                                              \
    ExitCritical();                                                                                \
  } while (0u)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const SomeIp_ConfigType SomeIp_Config;
/* ================================ [ DATAS     ] ============================================== */
DEF_SQP(AsyncReqMsg, SOMEIP_ASYNC_REQUEST_MESSAGE_POOL_SIZE)
DEF_SQP(RxTpMsg, SOMEIP_RX_TP_MESSAGE_POOL_SIZE)
DEF_SQP(TxTpMsg, SOMEIP_TX_TP_MESSAGE_POOL_SIZE)
#define someIpRxTpEvtMsgPool someIpRxTpMsgPool
DEF_SQP(TxTpEvtMsg, SOMEIP_TX_TP_EVENT_MESSAGE_POOL_SIZE)
DEF_SQP(WaitResMsg, SOMEIP_WAIT_RESPOSE_MESSAGE_POOL_SIZE)
static const SomeIp_ConfigType *someIpConfigPtr = NULL;
/* ================================ [ LOCALS    ] ============================================== */
static Std_ReturnType SomeIp_DecodeHeader(const uint8_t *data, uint32_t length,
                                          SomeIp_HeaderType *header) {
  Std_ReturnType ret = E_OK;
  if (length >= 16u) {
    if (data[12] != 1u) { /* @SWS_SomeIpXf_00029 */
      ASLOG(SOMEIP, ("invlaid protocol version\n"));
      ret = SOMEIP_E_WRONG_PROTOCOL_VERSION;
    } else {
      header->Length =
        ((uint32_t)data[4] << 24) + ((uint32_t)data[5] << 16) + ((uint32_t)data[6] << 8u) + data[7];
      header->ServiceId = ((uint16_t)data[0] << 8u) + data[1];
      header->MethodId = ((uint16_t)data[2] << 8u) + data[3];
      header->ClientId = ((uint16_t)data[8] << 8u) + data[9];
      header->SessionId = ((uint16_t)data[10] << 8u) + data[11];
      header->InterfaceVersion = data[13];
      header->MessageType = data[14];
      header->ReturnCode = data[15];
      if (0u != (header->MessageType & SOMEIP_TP_FLAG)) {
        header->MessageType &= ~SOMEIP_TP_FLAG;
        header->IsTpFlag = TRUE;
        if ((header->Length > (8u + 4u + SOMEIP_TP_MAX)) || (header->Length <= (8u + 4u))) {
          ASLOG(SOMEIPE, ("TP length not valid\n"));
          ret = SOMEIP_E_MALFORMED_MESSAGE;
        }
      } else {
        header->IsTpFlag = FALSE;
      }
      if (header->Length < 8u) {
        ret = E_NOT_OK;
        ASLOG(SOMEIPE, ("invalid length\n"));
      } else if ((header->Length + 8u) > length) {
        ret = SOMEIP_E_MSG_TOO_SHORT;
      } else if ((header->Length + 8u) < length) {
        ret = SOMEIP_E_MSG_TOO_LARGE;
      } else {
      }
    }
  } else {
    ASLOG(SOMEIPE, ("message too short\n"));
    ret = SOMEIP_E_MALFORMED_MESSAGE;
  }
  return ret;
}

static Std_ReturnType SomeIp_DecodeMsg(const PduInfoType *pduInfoPtr, SomeIp_MsgType *msg) {
  Std_ReturnType ret;
  uint32_t offset;

  ret = SomeIp_DecodeHeader(pduInfoPtr->SduDataPtr, pduInfoPtr->SduLength, &msg->Header);
  if (E_OK == ret) {
    msg->RemoteAddr = *(const TcpIp_SockAddrType *)pduInfoPtr->MetaDataPtr;
    if (TRUE == msg->Header.IsTpFlag) {
      offset = ((uint32_t)pduInfoPtr->SduDataPtr[16] << 24) +
               ((uint32_t)pduInfoPtr->SduDataPtr[17] << 16) +
               ((uint32_t)pduInfoPtr->SduDataPtr[18] << 8u) + pduInfoPtr->SduDataPtr[19];
      msg->TpHeader.Offset = offset & 0xFFFFFFF0UL;
      msg->TpHeader.MoreSegmentsFlag = offset & 0x01u;
      msg->Req.Data = &pduInfoPtr->SduDataPtr[20];
      msg->Req.Length = pduInfoPtr->SduLength - 20u;
    } else {
      msg->Req.Data = &pduInfoPtr->SduDataPtr[16];
      msg->Req.Length = pduInfoPtr->SduLength - 16u;
    }
  }

  return ret;
}

static void SomeIp_BuildHeader(uint8_t *header, uint16_t serviceId, uint16_t methodId,
                               uint16_t clientId, uint16_t sessionId, uint8_t interfaceVersion,
                               uint8_t messageType, uint8_t returnCode, uint32_t payloadLength) {
  uint32_t length = payloadLength + 8u;
  header[0] = (serviceId >> 8u) & 0xFFu;
  header[1] = serviceId & 0xFFu;
  header[2] = (methodId >> 8u) & 0xFFu;
  header[3] = methodId & 0xFFu;
  header[4] = (length >> 24) & 0xFFu;
  header[5] = (length >> 16) & 0xFFu;
  header[6] = (length >> 8u) & 0xFFu;
  header[7] = length & 0xFFu;
  header[8] = (clientId >> 8u) & 0xFFu;
  header[9] = clientId & 0xFFu;
  header[10] = (sessionId >> 8u) & 0xFFu;
  header[11] = sessionId & 0xFFu;
  header[12] = 1;
  header[13] = interfaceVersion;
  header[14] = messageType;
  header[15] = returnCode;

  ASLOG(SOMEIP,
        ("build: service 0x%x:0x%x:%d message type 0x%x return code %d payload %d bytes from "
         "client 0x%x:%d\n",
         serviceId, methodId, interfaceVersion, messageType, returnCode, payloadLength, clientId,
         sessionId));
}

static Std_ReturnType SomeIp_Transmit(PduIdType txPduId, TcpIp_SockAddrType *remoteAddr,
                                      uint8_t *data, uint16_t serviceId, uint16_t methodId,
                                      uint16_t clientId, uint16_t sessionId,
                                      uint8_t interfaceVersion, uint8_t messageType,
                                      uint8_t returnCode, uint32_t payloadLength) {
  Std_ReturnType ret;
  PduInfoType PduInfo;

  PduInfo.MetaDataPtr = (uint8_t *)remoteAddr;
  PduInfo.SduDataPtr = data;
  PduInfo.SduLength = payloadLength + 16u;
  SomeIp_BuildHeader(data, serviceId, methodId, clientId, sessionId, interfaceVersion, messageType,
                     returnCode, payloadLength);
  ret = SoAd_IfTransmit(txPduId, &PduInfo);
  if (E_OK != ret) {
    ASLOG(SOMEIPE, ("Fail to send msg\n"));
  } else {
    PCAP_TRACE(serviceId, methodId, interfaceVersion, messageType, returnCode, &data[16],
               payloadLength, clientId, sessionId, remoteAddr, FALSE, 0u, FALSE, FALSE);
  }
  return ret;
}

static Std_ReturnType SomeIp_TransError(PduIdType txPduId, TcpIp_SockAddrType *remoteAddr,
                                        uint16_t serviceId, uint16_t methodId, uint16_t clientId,
                                        uint16_t sessionId, uint8_t interfaceVersion,
                                        uint8_t messageType, uint8_t returnCode) {
  Std_ReturnType ret;
  PduInfoType PduInfo;
  uint8_t errMsg[16];

  PduInfo.MetaDataPtr = (uint8_t *)remoteAddr;
  PduInfo.SduDataPtr = errMsg;
  PduInfo.SduLength = 16u;
  SomeIp_BuildHeader(errMsg, serviceId, methodId, clientId, sessionId, interfaceVersion,
                     messageType, returnCode, 0u);
  ret = SoAd_IfTransmit(txPduId, &PduInfo);
  if (E_OK != ret) {
    ASLOG(SOMEIPE, ("Fail to send error\n"));
  } else {
    PCAP_TRACE(serviceId, methodId, interfaceVersion, messageType, returnCode, NULL, 0u, clientId,
               sessionId, remoteAddr, FALSE, 0u, FALSE, FALSE);
  }
  return ret;
}

static Std_ReturnType SomeIp_ReplyError(PduIdType txPduId, SomeIp_MsgType *msg, uint8_t errorCode) {
  Std_ReturnType ret = SomeIp_TransError(
    txPduId, &msg->RemoteAddr, msg->Header.ServiceId, msg->Header.MethodId, msg->Header.ClientId,
    msg->Header.SessionId, msg->Header.InterfaceVersion, SOMEIP_MSG_ERROR, errorCode);
  return ret;
}

static void SomeIp_InitServer(const SomeIp_ServerServiceType *config) {
  uint16_t i;
  for (i = 0u; i < config->NumOfConnections; i++) {
    (void)memset(config->Connections[i].Context, 0u, sizeof(SomeIp_ServerConnectionContextType));
    STAILQ_INIT(&(config->Connections[i].Context->PendingAsyncReqMsgs));
    STAILQ_INIT(&(config->Connections[i].Context->PendingRxTpMsgs));
    STAILQ_INIT(&(config->Connections[i].Context->PendingTxTpMsgs));
  }
  (void)memset(config->Context, 0u, sizeof(SomeIp_ServerContextType));
  STAILQ_INIT(&(config->Context->PendingTxTpEvtMsgs));
  config->Context->SessionId = 1u;
}

static void SomeIp_InitClient(const SomeIp_ClientServiceType *config) {
  SomeIp_ClientServiceContextType *context = config->Context;
  (void)memset(context, 0u, sizeof(SomeIp_ClientServiceContextType));
  STAILQ_INIT(&(context->PendingRxTpEvtMsgs));
  STAILQ_INIT(&(context->PendingRxTpMsgs));
  STAILQ_INIT(&(context->PendingTxTpMsgs));
  STAILQ_INIT(&(context->PendingWaitResMsgs));
  context->SessionId = 1u;
}

static SomeIp_RxTpMsgType *SomeIp_RxTpMsgFind(SomeIp_RxTpMsgList *pendingRxTpMsgs,
                                              uint16_t methodId, SomeIp_MsgType *msg) {
  SomeIp_RxTpMsgType *rxTpMsg = NULL;
  SomeIp_RxTpMsgType *var;
  EnterCritical();
  STAILQ_FOREACH(var, pendingRxTpMsgs, entry) {
    if ((var->MethodId == methodId) &&
        (0u == memcmp(&var->RemoteAddr, &msg->RemoteAddr, sizeof(TcpIp_SockAddrType)))) {
      rxTpMsg = var;
      break;
    }
  }
  ExitCritical();
  return rxTpMsg;
}

/* Start a new Rx TP session upon a First Frame: allocate a pending entry and
 * append it to the pending list. Return E_OK if the FF is accepted, the new
 * pending entry is passed out via pVar. */
static Std_ReturnType SomeIp_RxTpMsgStartFF(SomeIp_RxTpMsgList *pendingRxTpMsgs, uint16_t methodId,
                                            SomeIp_MsgType *msg, SomeIp_RxTpMsgType **pVar) {
  Std_ReturnType ret = E_OK;
  SomeIp_RxTpMsgType *var;
  ASLOG(SOMEIP, ("%x:%x:%x:%d FF length = %u\n", msg->Header.ServiceId, msg->Header.MethodId,
                 msg->Header.ClientId, msg->Header.SessionId, msg->Req.Length));
  SQP_ALLOC(RxTpMsg);
  if (NULL == var) {
    ret = SOMEIP_E_NOMEM;
    ASLOG(SOMEIPE, ("%x:%x:%x:%d OoM for Tp Rx\n", msg->Header.ServiceId, msg->Header.MethodId,
                    msg->Header.ClientId, msg->Header.SessionId));
  } else {
    var->Offset = 0u;
    var->ClientId = msg->Header.ClientId;
    var->SessionId = msg->Header.SessionId;
    var->RemoteAddr = msg->RemoteAddr;
    var->MethodId = methodId;
    var->Timer = SOMEIP_CONFIG->TpRxTimeoutTime;
    SQP_LAPPEND(RxTpMsg);
  }
  *pVar = var;
  return ret;
}

static Std_ReturnType
SomeIp_ProcessRxTpMsg(uint16_t conId, SomeIp_RxTpMsgList *pendingRxTpMsgs, uint16_t methodId,
                      SomeIp_OnTpCopyRxDataFncType onTpCopyRxData, SomeIp_MsgType *msg)

{
  Std_ReturnType ret = E_OK;
  SomeIp_TpMessageType tpMsg;
  SomeIp_RxTpMsgType *var = NULL;
  uint32_t requestId;
  int result;
  (void)conId;
  if (NULL != onTpCopyRxData) {
    var = SomeIp_RxTpMsgFind(pendingRxTpMsgs, methodId, msg);
    if (NULL == var) {
      if ((0u == msg->TpHeader.Offset) && (msg->TpHeader.MoreSegmentsFlag)) {
        ret = SomeIp_RxTpMsgStartFF(pendingRxTpMsgs, methodId, msg, &var);
      } else {
        ret = SOMEIP_E_MALFORMED_MESSAGE;
        ASLOG(SOMEIPE, ("%x:%x:%x:%d Tp message malformed or loss\n", msg->Header.ServiceId,
                        msg->Header.MethodId, msg->Header.ClientId, msg->Header.SessionId));
      }
    } else {
      result = memcmp(&var->RemoteAddr, &msg->RemoteAddr, sizeof(TcpIp_SockAddrType));
      if ((var->ClientId == msg->Header.ClientId) && (var->SessionId == msg->Header.SessionId) &&
          (var->Offset == msg->TpHeader.Offset) && (0 == result)) {
        var->Timer = SOMEIP_CONFIG->TpRxTimeoutTime;
        ASLOG(SOMEIP, ("%x:%x:%x:%d %s length = %u, offset = %d\n", msg->Header.ServiceId,
                       msg->Header.MethodId, msg->Header.ClientId, msg->Header.SessionId,
                       msg->TpHeader.MoreSegmentsFlag ? "CF" : "LF", msg->Req.Length, var->Offset));
      } else {
        /* Pending TP msg does not match the incoming one. Typically the tail of
         * the previous message (var->SessionId) was lost on the wire and the
         * peer has moved on to a new message (msg->Header.SessionId). Abort the
         * old pending message first. */
        requestId = ((uint32_t)var->ClientId << 16) + var->SessionId;
        (void)onTpCopyRxData(requestId, NULL);
        SQP_LRM_AND_FREE(RxTpMsg);
        var = NULL;
        if ((0u == msg->TpHeader.Offset) && (msg->TpHeader.MoreSegmentsFlag)) {
          /* New First Frame of a fresh session: start receiving it instead. */
          ret = SomeIp_RxTpMsgStartFF(pendingRxTpMsgs, methodId, msg, &var);
        } else {
          ret = SOMEIP_E_MALFORMED_MESSAGE;
          ASLOG(SOMEIPE,
                ("%x:%x:%x:%d Tp message not as expected, loss maybe\n", msg->Header.ServiceId,
                 msg->Header.MethodId, msg->Header.ClientId, msg->Header.SessionId));
        }
      }
    }
  } else {
    ret = SOMEIP_E_MALFORMED_MESSAGE;
  }

  if (E_OK == ret) {
    tpMsg.Data = msg->Req.Data;
    tpMsg.Length = msg->Req.Length;
    tpMsg.Offset = msg->TpHeader.Offset;
    tpMsg.MoreSegmentsFlag = msg->TpHeader.MoreSegmentsFlag;
    requestId = ((uint32_t)msg->Header.ClientId << 16) + msg->Header.SessionId;
    ret = onTpCopyRxData(requestId, &tpMsg);
    if (E_OK == ret) {
      var->Offset += msg->Req.Length;
      if (TRUE == msg->TpHeader.MoreSegmentsFlag) {
        ret = SOMEIP_E_OK_SILENT;
      } else {
        msg->Req.Data = tpMsg.Data;
        msg->Req.Length = var->Offset;
        SQP_LRM_AND_FREE(RxTpMsg);
      }
    }
  }

  return ret;
}

static Std_ReturnType SomeIp_SendNextTxTpMsg(PduIdType txPduId, uint16_t serviceId,
                                             uint16_t methodId, uint8_t interfaceVersion,
                                             uint8_t messageType,
                                             SomeIp_OnTpCopyTxDataFncType onTpCopyTxData,
                                             SomeIp_TxTpMsgType *var) {
  Std_ReturnType ret = E_OK;
  SomeIp_TpMessageType tpMsg;
  uint32_t len = var->Length - var->Offset;
  uint32_t offset = var->Offset;
  uint32_t requestId;
  uint8_t *data;

  if (len > SOMEIP_TP_MAX) {
    len = SOMEIP_TP_MAX;
    offset |= 0x01u; /* setup more flag */
    tpMsg.MoreSegmentsFlag = TRUE;
  } else {
    tpMsg.MoreSegmentsFlag = FALSE;
  }

  tpMsg.Data = NULL;
  tpMsg.Length = len;
  tpMsg.Offset = var->Offset;
  requestId = ((uint32_t)var->ClientId << 16) + var->SessionId;
  ret = onTpCopyTxData(requestId, &tpMsg);
  if ((E_OK == ret) && (NULL != tpMsg.Data)) {
    data = tpMsg.Data - 20;
    data[16] = (offset >> 24) & 0xFFu;
    data[17] = (offset >> 16) & 0xFFu;
    data[18] = (offset >> 8u) & 0xFFu;
    data[19] = offset & 0xFFu;
    ret = SomeIp_Transmit(txPduId, &var->RemoteAddr, data, serviceId, methodId, var->ClientId,
                          var->SessionId, interfaceVersion, messageType | SOMEIP_TP_FLAG, E_OK,
                          len + 4u);
    if (E_OK == ret) {
      var->Offset += len;
#ifndef DISABLE_SOMEIP_TX_NOK_RETRY
    } else if ((E_NOT_OK == ret) && (var->RetryCounter < SOMEIP_TX_NOK_RETRY_MAX)) {
      var->RetryCounter++;
      ASLOG(SOMEIPW, ("Tx TP NOK, try %d\n", var->RetryCounter));
      ret = E_OK;
#endif
    } else {
      ASLOG(SOMEIPE, ("Tx TP NOK\n"));
      onTpCopyTxData(requestId, NULL); /* indicate Tx error */
    }
  }
  return ret;
}

static Std_ReturnType SomeIp_TransmitEvtMsg(const SomeIp_ServerServiceType *config,
                                            const SomeIp_ServerEventType *event, uint8_t *data,
                                            uint32_t payloadLength, boolean isTp,
                                            Sd_EventHandlerSubscriberListType *list,
                                            uint16_t sessionId) {
  Std_ReturnType ret = E_NOT_OK;
  Std_ReturnType ret2;
  Sd_EventHandlerSubscriberType *sub;
  uint8_t messageType = SOMEIP_MSG_NOTIFICATION;
  TcpIp_SockAddrType *RemoteAddr = NULL;
  boolean bMulticast = FALSE;

  if (TRUE == isTp) {
    messageType |= SOMEIP_TP_FLAG;
  }

  sub = STAILQ_FIRST(list);
  while (NULL != sub) {
    if (0u != sub->Flags) {
      RemoteAddr = &sub->RemoteAddr;
      if (0u != (SD_FLG_EVENT_GROUP_MULTICAST & sub->Flags)) {
        bMulticast = TRUE;
        RemoteAddr = NULL;
      }
      ret2 = SomeIp_Transmit(sub->TxPduId, RemoteAddr, data, config->ServiceId, event->EventId,
                             config->ClientId, sessionId, event->InterfaceVersion, messageType, 0u,
                             payloadLength);
      if (E_OK == ret2) {
        ret = E_OK;
      } else {
        ASLOG(SOMEIPE, ("Failed to notify event %x:%x to %d.%d.%d.%d:%d\n", config->ServiceId,
                        event->EventId, sub->RemoteAddr.Addr[0], sub->RemoteAddr.Addr[1],
                        sub->RemoteAddr.Addr[2], sub->RemoteAddr.Addr[3], sub->RemoteAddr.Port));
      }
    }
    /* NOTE: if any one is unsubscribed during this, TX is broken, but Sd Main and SomeIp Main
     * scheduled in one Task, not possible */
    sub = STAILQ_NEXT(sub, entry);
    if (TRUE == bMulticast) {
      break;
    }
  }

  return ret;
}

static Std_ReturnType SomeIp_SendNextTxTpEvtMsg(const SomeIp_ServerServiceType *config,
                                                const SomeIp_ServerEventType *event,
                                                SomeIp_TxTpEvtMsgType *var,
                                                Sd_EventHandlerSubscriberListType *list) {
  Std_ReturnType ret = E_OK;
  SomeIp_TpMessageType tpMsg;
  uint32_t len = var->Length - var->Offset;
  uint32_t offset = var->Offset;
  uint8_t *data;
  uint32_t requestId;

  if (len > SOMEIP_TP_MAX) {
    len = SOMEIP_TP_MAX;
    offset |= 0x01u; /* setup more flag */
    tpMsg.MoreSegmentsFlag = TRUE;
  } else {
    tpMsg.MoreSegmentsFlag = FALSE;
  }

  tpMsg.Data = NULL;
  tpMsg.Length = len;
  tpMsg.Offset = var->Offset;
  requestId = ((uint32_t)var->EventId << 16) + var->SessionId;
  ret = event->OnTpCopyTxData(requestId, &tpMsg);
  if ((E_OK == ret) && (NULL != tpMsg.Data)) {
    data = tpMsg.Data - 20;
    data[16] = (offset >> 24) & 0xFFu;
    data[17] = (offset >> 16) & 0xFFu;
    data[18] = (offset >> 8u) & 0xFFu;
    data[19] = offset & 0xFFu;
    ret = SomeIp_TransmitEvtMsg(config, event, data, len + 4, TRUE, list, var->SessionId);
    if (E_OK == ret) {
      var->Offset += len;
    } else {
      ASLOG(SOMEIPE, ("Tx TP EVT NOK\n"));
    }
  }
  return ret;
}

static Std_ReturnType SomeIp_ReplyRequest(const SomeIp_ServerServiceType *config, uint16_t conId,
                                          uint16_t methodId, uint16_t clientId, uint16_t sessionId,
                                          TcpIp_SockAddrType *remoteAddr, SomeIp_MessageType *res) {
  Std_ReturnType ret = E_OK;
  const SomeIp_ServerConnectionType *connection = &config->Connections[conId];
  SomeIp_ServerConnectionContextType *context = connection->Context;
  const SomeIp_ServerMethodType *method = &config->Methods[methodId];
  SomeIp_TxTpMsgType *var;

  if (IS_TP_ENABLED(method) && (res->Length > SOMEIP_SF_MAX)) {
    SQP_ALLOC(TxTpMsg);
    if (NULL != var) {
      var->ClientId = clientId;
      var->SessionId = sessionId;
      var->MethodId = methodId;
      var->RemoteAddr = *remoteAddr;
      var->Offset = 0u;
      var->Length = res->Length;
#ifndef DISABLE_SOMEIP_TX_NOK_RETRY
      var->RetryCounter = 0u;
#endif
      var->Timer = config->SeparationTime;
      ret = SomeIp_SendNextTxTpMsg(connection->TxPduId, config->ServiceId, method->MethodId,
                                   method->InterfaceVersion, SOMEIP_MSG_RESPONSE,
                                   method->OnTpCopyTxData, var);
      if (E_OK == ret) {
        SQP_CAPPEND(TxTpMsg);
      } else {
        SQP_FREE(TxTpMsg);
      }
    } else {
      ret = SOMEIP_E_NOMEM;
      ASLOG(SOMEIPE, ("OoM for cache reply Tp Tx\n"));
    }
  } else {
    ret = SomeIp_Transmit(connection->TxPduId, remoteAddr, res->Data, config->ServiceId,
                          method->MethodId, clientId, sessionId, method->InterfaceVersion,
                          SOMEIP_MSG_RESPONSE, E_OK, res->Length);
  }

  return ret;
}

static Std_ReturnType SomeIp_WaitResponse(const SomeIp_ClientServiceType *config, uint16_t methodId,
                                          uint16_t sessionId) {
  Std_ReturnType ret = E_OK;
  SomeIp_ClientServiceContextType *context = config->Context;
  SomeIp_WaitResMsgType *var;

  SQP_ALLOC(WaitResMsg);
  if (NULL != var) {
    var->MethodId = methodId;
    var->SessionId = sessionId;
    var->Timer = config->ResponseTimeout;
    SQP_CAPPEND(WaitResMsg);
  } else {
    ASLOG(SOMEIPE, ("OoM for wait res msg\n"));
    ret = E_NOT_OK;
  }

  return ret;
}

static Std_ReturnType SomeIp_SendRequest(const SomeIp_ClientServiceType *config, uint16_t methodId,
                                         uint16_t clientId, uint16_t sessionId,
                                         TcpIp_SockAddrType *remoteAddr, SomeIp_MessageType *req,
                                         uint8_t messageType) {
  Std_ReturnType ret = E_OK;
  SomeIp_ClientServiceContextType *context = config->Context;
  const SomeIp_ClientMethodType *method = &config->Methods[methodId];
  SomeIp_TxTpMsgType *var;
  uint8_t *data;
  uint8_t localBuf[16];
  if (IS_TP_ENABLED(method) && (req->Length > SOMEIP_SF_MAX)) {
    SQP_ALLOC(TxTpMsg);
    if (NULL != var) {
      var->ClientId = clientId;
      var->SessionId = sessionId;
      var->MethodId = methodId;
      var->RemoteAddr = *remoteAddr;
      var->Offset = 0u;
      var->Length = req->Length;
      var->MessageType = messageType;
#ifndef DISABLE_SOMEIP_TX_NOK_RETRY
      var->RetryCounter = 0u;
#endif
      var->Timer = config->SeparationTime;
      ret =
        SomeIp_SendNextTxTpMsg(config->TxPduId, config->ServiceId, method->MethodId,
                               method->InterfaceVersion, messageType, method->OnTpCopyTxData, var);
      if (E_OK == ret) {
        SQP_CAPPEND(TxTpMsg);
      } else {
        SQP_FREE(TxTpMsg);
      }
    } else {
      ret = SOMEIP_E_NOMEM;
      ASLOG(SOMEIPE, ("OoM for cache request Tp Tx\n"));
    }
  } else {
    if ((NULL == req->Data) || (0 == req->Length)) {
      data = localBuf;
    } else {
      data = req->Data - 16;
    }
    ret = SomeIp_Transmit(config->TxPduId, remoteAddr, data, config->ServiceId, method->MethodId,
                          clientId, sessionId, method->InterfaceVersion, messageType, E_OK,
                          req->Length);
    if (E_OK == ret) {
      ret = SomeIp_WaitResponse(config, methodId, sessionId);
    }
  }

  return ret;
}

/* Check if a long TP message can be transmitted for the given method.
 * @SWS_SomeIpTp_00016 @PRS_SOMEIP_00750: interleaving of long TP messages
 * of the same method is not supported, only one ongoing TP TX per method
 * is allowed, otherwise reject the new one with SOMEIP_E_BUSY */
static Std_ReturnType SomeIp_CheckTxTpBusy(const SomeIp_ClientServiceType *config,
                                           const SomeIp_ClientServiceContextType *context,
                                           uint16_t methodIndex, uint32_t length) {
  Std_ReturnType ret = E_OK;
  const SomeIp_ClientMethodType *method = &config->Methods[methodIndex];
  const SomeIp_TxTpMsgList *PendingTxTpMsgs = &context->PendingTxTpMsgs;
  SomeIp_TxTpMsgType *txTpMsg;

  if (IS_TP_ENABLED(method) && (length > SOMEIP_SF_MAX)) {
    EnterCritical();
    STAILQ_FOREACH(txTpMsg, PendingTxTpMsgs, entry) {
      if (txTpMsg->MethodId == methodIndex) {
        ret = SOMEIP_E_BUSY;
        break;
      }
    }
    ExitCritical();
    if (SOMEIP_E_BUSY == ret) {
      ASLOG(SOMEIPE, ("%x:%x busy for long TP TX\n", config->ServiceId, method->MethodId));
    }
  }

  return ret;
}

static Std_ReturnType SomeIp_RequestOrFire(uint32_t requestId, uint8_t *data, uint32_t length,
                                           uint8_t messageType) {
  Std_ReturnType ret = E_OK;
  uint16_t TxMethodId = (requestId >> 16) & 0xFFFFu;
  uint16_t sessionId = requestId & 0xFFFFu;
  const SomeIp_ClientServiceType *config;
  SomeIp_ClientServiceContextType *context;
  uint16_t index;
  TcpIp_SockAddrType RemoteAddr;
  SomeIp_MessageType msg;

  if (TxMethodId < SOMEIP_CONFIG->NumOfTxMethods) {
    index = SOMEIP_CONFIG->TxMethod2ServiceMap[TxMethodId];
    config = (const SomeIp_ClientServiceType *)SOMEIP_CONFIG->Services[index].Service;
    context = config->Context;
    if (FALSE == context->Online) {
      ret = E_NOT_OK;
    }
  } else {
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    index = SOMEIP_CONFIG->TxMethod2PerServiceMap[TxMethodId];
    ret = Sd_GetProviderAddr(config->SdHandleID, &RemoteAddr);
    if (E_OK == ret) {
      msg.Data = data;
      msg.Length = length;
    }
  }

  if (E_OK == ret) {
    ret = SomeIp_CheckTxTpBusy(config, context, index, length);
  }

  if (E_OK == ret) {
    if (0 == sessionId) {
      EnterCritical();
      sessionId = context->SessionId;
      context->SessionId++;
      if (0 == context->SessionId) {
        context->SessionId = 1;
      }
      ExitCritical();
    }
    ret = SomeIp_SendRequest(config, index, config->ClientId, sessionId, &RemoteAddr, &msg,
                             messageType);
  }

  return ret;
}

static Std_ReturnType SomeIp_SendNotification(const SomeIp_ServerServiceType *config,
                                              uint16_t eventId, uint16_t sessionId,
                                              SomeIp_MessageType *req,
                                              Sd_EventHandlerSubscriberListType *list) {
  Std_ReturnType ret = E_OK;
  SomeIp_ServerContextType *context = config->Context;
  const SomeIp_ServerEventType *event =
    &config->Events[SOMEIP_CONFIG->TxEvent2PerServiceMap[eventId]];
  SomeIp_TxTpEvtMsgType *var;
  uint8_t *data;
  uint8_t localBuf[16];
  if (IS_TP_ENABLED(event) && (req->Length > SOMEIP_SF_MAX)) {
    SQP_ALLOC(TxTpEvtMsg);
    if (NULL != var) {
      var->EventId = eventId;
      var->SessionId = sessionId;
      var->Offset = 0u;
      var->Length = req->Length;
      var->Timer = config->SeparationTime;
      ret = SomeIp_SendNextTxTpEvtMsg(config, event, var, list);
      if (E_OK == ret) {
        SQP_CAPPEND(TxTpEvtMsg);
      } else {
        SQP_FREE(TxTpEvtMsg);
      }
    } else {
      ret = SOMEIP_E_NOMEM;
      ASLOG(SOMEIPE, ("OoM for cache notification Tp Tx\n"));
    }
  } else {
    if ((NULL == req->Data) || (0 == req->Length)) {
      data = localBuf;
    } else {
      data = req->Data - 16;
    }
    ret = SomeIp_TransmitEvtMsg(config, event, data, req->Length, FALSE, list, sessionId);
  }

  return ret;
}

/* Check if a long TP event message can be transmitted for the given event.
 * @SWS_SomeIpTp_00016 @PRS_SOMEIP_00750: interleaving of long TP messages
 * of the same event is not supported, only one ongoing TP TX per event
 * is allowed, otherwise reject the new one with SOMEIP_E_BUSY */
static Std_ReturnType SomeIp_CheckTxTpEvtBusy(const SomeIp_ServerServiceType *config,
                                              uint16_t eventIndex, uint16_t eventId,
                                              uint32_t length) {
  Std_ReturnType ret = E_OK;
  const SomeIp_ServerContextType *context = config->Context;
  const SomeIp_ServerEventType *event = &config->Events[eventIndex];
  const SomeIp_TxTpEvtMsgList *PendingTxTpEvtMsgs = &context->PendingTxTpEvtMsgs;
  SomeIp_TxTpEvtMsgType *txTpEvtMsg;

  if (IS_TP_ENABLED(event) && (length > SOMEIP_SF_MAX)) {
    EnterCritical();
    STAILQ_FOREACH(txTpEvtMsg, PendingTxTpEvtMsgs, entry) {
      if (txTpEvtMsg->EventId == eventId) {
        ret = SOMEIP_E_BUSY;
        break;
      }
    }
    ExitCritical();
    if (SOMEIP_E_BUSY == ret) {
      ASLOG(SOMEIPE, ("%x:%x busy for long TP EVT TX\n", config->ServiceId, event->EventId));
    }
  }

  return ret;
}

static Std_ReturnType SomeIp_ProcessRequest(const SomeIp_ServerServiceType *config, uint16_t conId,
                                            uint16_t methodId, SomeIp_MsgType *msg) {
  Std_ReturnType ret = E_OK;
  const SomeIp_ServerConnectionType *connection = &config->Connections[conId];
  SomeIp_ServerConnectionContextType *context = connection->Context;
  const SomeIp_ServerMethodType *method = &config->Methods[methodId];
  SomeIp_AsyncReqMsgType *var;
  SomeIp_MessageType res;
  uint32_t requestId = ((uint32_t)msg->Header.ClientId << 16) + msg->Header.SessionId;
  uint8_t localBuf[16];

  res.Data = NULL;
  res.Length = 0;
  ASLOG(SOMEIP, ("%x:%x:%x:%d request length = %u\n", msg->Header.ServiceId, msg->Header.MethodId,
                 msg->Header.ClientId, msg->Header.SessionId, msg->Req.Length));
  ret = method->OnRequest(requestId, &msg->Req, &res);
  ASLOG(SOMEIP, ("%x:%x:%x:%d reply length = %u\n", msg->Header.ServiceId, msg->Header.MethodId,
                 msg->Header.ClientId, msg->Header.SessionId, res.Length));
  if (E_OK == ret) {
    if (0 == res.Length) {
      res.Data = localBuf;
    } else if (NULL != res.Data) {
      res.Data -= 16;
    } else {
      ASLOG(SOMEIPE, ("%x:%x:%x:%d reply buffer is null\n", msg->Header.ServiceId,
                      msg->Header.MethodId, msg->Header.ClientId, msg->Header.SessionId));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    ret = SomeIp_ReplyRequest(config, conId, methodId, msg->Header.ClientId, msg->Header.SessionId,
                              &msg->RemoteAddr, &res);
    if (E_OK != ret) {
      /* with NULL to tell app that failed to transmit response */
      (void)method->OnAsyncRequest(conId, NULL);
    }
  } else if (SOMEIP_E_PENDING == ret) {
    /* response pending */
    SQP_ALLOC(AsyncReqMsg);
    if (NULL != var) {
      var->ClientId = msg->Header.ClientId;
      var->SessionId = msg->Header.SessionId;
      var->MethodId = methodId;
      var->RemoteAddr = msg->RemoteAddr;
      SQP_CAPPEND(AsyncReqMsg);
    } else {
      ret = SOMEIP_E_NOMEM;
      ASLOG(SOMEIPE, ("OoM for cache request\n"));
    }
  } else {
    /* do nothing */
  }
  return ret;
}

static Std_ReturnType SomeIp_HandleServerMessage_Request(const SomeIp_ServerServiceType *config,
                                                         uint16_t conId, SomeIp_MsgType *msg) {
  Std_ReturnType ret = SOMEIP_E_UNKNOWN_METHOD;
  uint16_t methodId;
  const SomeIp_ServerConnectionType *connection = &config->Connections[conId];
  SomeIp_ServerConnectionContextType *context = connection->Context;
  const SomeIp_ServerMethodType *method = NULL;
  uint32_t requestId;

  for (methodId = 0u; methodId < config->NumOfMethods; methodId++) {
    if (config->Methods[methodId].MethodId == msg->Header.MethodId) {
      if (((0xFFu == config->Methods[methodId].InterfaceVersion) ||
           (config->Methods[methodId].InterfaceVersion == msg->Header.InterfaceVersion))) {
        method = &config->Methods[methodId];
        ret = E_OK;
        break;
      } else {
        ret = SOMEIP_E_WRONG_INTERFACE_VERSION;
      }
    }
  }

  if (E_OK == ret) {
    if (TRUE == msg->Header.IsTpFlag) {
      ret = SomeIp_ProcessRxTpMsg(conId, &context->PendingRxTpMsgs, methodId,
                                  method->OnTpCopyRxData, msg);
    }
  }

  if (E_OK == ret) {
    requestId = ((uint32_t)msg->Header.ClientId << 16) + msg->Header.SessionId;
    if (SOMEIP_MSG_REQUEST == msg->Header.MessageType) {
      ret = SomeIp_ProcessRequest(config, conId, methodId, msg);
    } else {
      ret = method->OnFireForgot(requestId, &msg->Req);
      if (E_OK == ret) {
        ret = SOMEIP_E_OK_SILENT;
      }
    }
  }

  return ret;
}

static Std_ReturnType SomeIp_IsResponseExpected(const SomeIp_ClientServiceType *config,
                                                uint16_t methodId, SomeIp_MsgType *msg) {
  Std_ReturnType ret = E_NOT_OK;
  SomeIp_ClientServiceContextType *context = config->Context;
  SomeIp_WaitResMsgType *var;

  EnterCritical();
  if ((FALSE == msg->Header.IsTpFlag) || (0u == msg->TpHeader.Offset)) {
    STAILQ_FOREACH(var, &context->PendingWaitResMsgs, entry) {
      if ((var->MethodId == methodId) && (var->SessionId == msg->Header.SessionId)) {
        ret = E_OK;
        SQP_CRM_AND_FREE(WaitResMsg);
        break;
      }
    }
  } else { /* For TP msg with offset not 0u, no check */
    ret = E_OK;
  }
  ExitCritical();

  return ret;
}

static Std_ReturnType SomeIp_HandleClientMessage_Respose(const SomeIp_ClientServiceType *config,
                                                         SomeIp_MsgType *msg) {
  Std_ReturnType ret = SOMEIP_E_UNKNOWN_METHOD;
  const SomeIp_ClientMethodType *method = NULL;
  SomeIp_ClientServiceContextType *context = config->Context;
  uint16_t methodId;
  uint32_t requestId;

  for (methodId = 0u; methodId < config->NumOfMethods; methodId++) {
    if (config->Methods[methodId].MethodId == msg->Header.MethodId) {
      if (((0xFFu == config->Methods[methodId].InterfaceVersion) ||
           (config->Methods[methodId].InterfaceVersion == msg->Header.InterfaceVersion))) {
        method = &config->Methods[methodId];
        ret = E_OK;
        break;
      } else {
        ret = SOMEIP_E_WRONG_INTERFACE_VERSION;
      }
    }
  }

  if (E_OK == ret) {
    ret = SomeIp_IsResponseExpected(config, methodId, msg);
    if (E_OK != ret) {
      ASLOG(SOMEIPE, ("response %x:%x:%d not as expected\n", config->ServiceId, method->MethodId,
                      msg->Header.SessionId));
    }
  }

  if (E_OK == ret) {
    if (TRUE == msg->Header.IsTpFlag) {
      ret =
        SomeIp_ProcessRxTpMsg(0u, &context->PendingRxTpMsgs, methodId, method->OnTpCopyRxData, msg);
    }
  }

  if (E_OK == ret) {
    requestId = ((uint32_t)msg->Header.ClientId << 16) + msg->Header.SessionId;
    if (E_OK == msg->Header.ReturnCode) {
      ret = method->OnResponse(requestId, &msg->Req);
    } else {
      ret = method->OnError(requestId, msg->Header.ReturnCode);
      ASLOG(SOMEIPE, ("response %x:%x:%d with error %u\n", config->ServiceId, method->MethodId,
                      msg->Header.SessionId, msg->Header.ReturnCode));
    }
  }

  return ret;
}

static Std_ReturnType SomeIp_HandleServerMessage(const SomeIp_ServerServiceType *config,
                                                 uint16_t conId, SomeIp_MsgType *msg) {
  Std_ReturnType ret = E_OK;

  if (conId < config->NumOfConnections) {
    if (config->ServiceId != msg->Header.ServiceId) {
      ASLOG(SOMEIPE, ("unknown service\n"));
      ret = SOMEIP_E_UNKNOWN_SERVICE;
    }
  } else {
    ASLOG(SOMEIPE, ("invalid connection ID\n"));
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    switch (msg->Header.MessageType) {
    case SOMEIP_MSG_REQUEST:
    case SOMEIP_MSG_REQUEST_NO_RETURN:
      ret = SomeIp_HandleServerMessage_Request(config, conId, msg);
      break;
    case SOMEIP_MSG_NOTIFICATION:
      /* slient as event multicast */
      break;
    default:
      ASLOG(SOMEIPE, ("server: unsupported message type 0x%x\n", msg->Header.MessageType));
      ret = SOMEIP_E_WRONG_MESSAGE_TYPE;
      break;
    }
  }

  if (SOMEIP_E_PENDING == ret) {
    /* ok silent */
  } else if (SOMEIP_E_WRONG_MESSAGE_TYPE == ret) {
    /* ignore it */
  } else if (E_OK != ret) {
    (void)SomeIp_ReplyError(config->Connections[conId].TxPduId, msg, ret);
  } else {
    /* ok, pass */
  }

  return ret;
}

static Std_ReturnType
SomeIp_HandleClientMessage_Notification(const SomeIp_ClientServiceType *config,
                                        SomeIp_MsgType *msg) {
  Std_ReturnType ret = E_NOT_OK;
  SomeIp_ClientServiceContextType *context = config->Context;
  const SomeIp_ClientEventType *event = NULL;
  uint16_t eventId;
  uint32_t requestId;

  for (eventId = 0u; eventId < config->NumOfEvents; eventId++) {
    if (config->Events[eventId].EventId == msg->Header.MethodId) {
      if (((0xFFu == config->Events[eventId].InterfaceVersion) ||
           (config->Events[eventId].InterfaceVersion == msg->Header.InterfaceVersion))) {
        event = &config->Events[eventId];
        ret = E_OK;
        break;
      } else {
        ret = SOMEIP_E_WRONG_INTERFACE_VERSION;
      }
    }
  }

  if (E_OK == ret) {
    if (TRUE == msg->Header.IsTpFlag) {
      ret = SomeIp_ProcessRxTpMsg(0u, &context->PendingRxTpEvtMsgs, eventId, event->OnTpCopyRxData,
                                  msg);
    }
  }

  if (E_OK == ret) {
    requestId = ((uint32_t)msg->Header.ClientId << 16) + msg->Header.SessionId;
    ret = event->OnNotification(requestId, &msg->Req);
  } else {
    ret = SOMEIP_E_UNKNOWN_METHOD;
  }

  return ret;
}

static Std_ReturnType SomeIp_HandleClientMessage(const SomeIp_ClientServiceType *config,
                                                 SomeIp_MsgType *msg) {
  Std_ReturnType ret = E_OK;

  if (config->ServiceId != msg->Header.ServiceId) {
    ASLOG(SOMEIPE, ("unknown service\n"));
    ret = SOMEIP_E_UNKNOWN_SERVICE;
  }

  if (E_OK == ret) {
    switch (msg->Header.MessageType) {
    case SOMEIP_MSG_NOTIFICATION:
      ret = SomeIp_HandleClientMessage_Notification(config, msg);
      break;
    case SOMEIP_MSG_RESPONSE:
      ret = SomeIp_HandleClientMessage_Respose(config, msg);
      break;
    case SOMEIP_MSG_ERROR:
      ret = SomeIp_HandleClientMessage_Respose(config, msg);
      break;
    default:
      ASLOG(SOMEIPE, ("client: unsupported message type 0x%x\n", msg->Header.MessageType));
      ret = SOMEIP_E_WRONG_MESSAGE_TYPE;
      break;
    }
  }

  return ret;
}

static void SomeIp_MainServerAsyncRequestOnClose(const SomeIp_ServerServiceType *config,
                                                 uint16_t conId) {
  const SomeIp_ServerConnectionType *connection = &config->Connections[conId];
  SomeIp_ServerConnectionContextType *context = connection->Context;
  DEC_SQP(AsyncReqMsg);
  const SomeIp_ServerMethodType *method = NULL;
  SQP_WHILE(AsyncReqMsg) {
    method = &config->Methods[var->MethodId];
    (void)method->OnAsyncRequest(conId, NULL);
    SQP_CRM_AND_FREE(AsyncReqMsg);
  }
  SQP_WHILE_END()
}

static void SomeIp_MainServerAsyncRequest(const SomeIp_ServerServiceType *config, uint16_t conId) {
  const SomeIp_ServerConnectionType *connection = &config->Connections[conId];
  SomeIp_ServerConnectionContextType *context = connection->Context;
  DEC_SQP(AsyncReqMsg);
  const SomeIp_ServerMethodType *method = NULL;
  SomeIp_MessageType res;
  Std_ReturnType ret;
  uint8_t localBuf[16];

  SQP_WHILE(AsyncReqMsg) {
    method = &config->Methods[var->MethodId];

    res.Data = NULL;
    res.Length = 0;

    ret = method->OnAsyncRequest(conId, &res);
    if (E_OK == ret) {
      if (0 == res.Length) {
        res.Data = localBuf;
      } else if (NULL != res.Data) {
        res.Data -= 16;
      } else {
        ASLOG(SOMEIPE, ("%x:%x:%x:%d reply buffer is null\n", config->ServiceId, method->MethodId,
                        var->ClientId, var->SessionId));
        ret = E_NOT_OK;
      }
    }
    if (E_OK == ret) {
      ret = SomeIp_ReplyRequest(config, conId, var->MethodId, var->ClientId, var->SessionId,
                                &var->RemoteAddr, &res);
      if (E_OK != ret) {
        (void)SomeIp_TransError(connection->TxPduId, &var->RemoteAddr, config->ServiceId,
                                method->MethodId, var->ClientId, var->SessionId,
                                method->InterfaceVersion, SOMEIP_MSG_ERROR, ret);
        if (E_OK != ret) {
          /* with NULL to tell app that failed to transmit response */
          (void)method->OnAsyncRequest(conId, NULL);
        }
      }

      SQP_CRM_AND_FREE(AsyncReqMsg);
    } else if (SOMEIP_E_PENDING == ret) {
      /* do nothing */
    } else {
      (void)SomeIp_TransError(connection->TxPduId, &var->RemoteAddr, config->ServiceId,
                              method->MethodId, var->ClientId, var->SessionId,
                              method->InterfaceVersion, SOMEIP_MSG_RESPONSE, ret);

      SQP_CRM_AND_FREE(AsyncReqMsg);
    }
  }
  SQP_WHILE_END()
}

static void SomeIp_MainServerRxTpMsg(const SomeIp_ServerServiceType *config, uint16_t conId) {
  const SomeIp_ServerConnectionType *connection = &config->Connections[conId];
  SomeIp_ServerConnectionContextType *context = connection->Context;
  DEC_SQP(RxTpMsg);
  const SomeIp_ServerMethodType *method = NULL;
  Std_ReturnType ret;

  SQP_WHILE(RxTpMsg) {
    method = &config->Methods[var->MethodId];

    if (var->Timer > 0u) {
      var->Timer--;
      if (0 == var->Timer) {
        ASLOG(SOMEIPE,
              ("server method %x:%x:%x:%d Rx Tp msg timeout, offset %d\n", config->ServiceId,
               method->MethodId, var->ClientId, var->SessionId, var->Offset));
        ret = SomeIp_TransError(connection->TxPduId, &var->RemoteAddr, config->ServiceId,
                                method->MethodId, var->ClientId, var->SessionId,
                                method->InterfaceVersion, SOMEIP_MSG_ERROR, SOMEIP_E_TIMEOUT);
        if (E_NOT_OK == ret) {
          var->Timer = 1; /* retry next time */
        } else {
          method->OnTpCopyRxData(((uint32_t)var->ClientId << 16) + var->SessionId, NULL);
          SQP_CRM_AND_FREE(RxTpMsg);
        }
      }
    }
  }
  SQP_WHILE_END()
}

static void SomeIp_MainServerRxTpMsgOnClose(const SomeIp_ServerServiceType *config,
                                            uint16_t conId) {
  const SomeIp_ServerConnectionType *connection = &config->Connections[conId];
  SomeIp_ServerConnectionContextType *context = connection->Context;
  DEC_SQP(RxTpMsg);
  const SomeIp_ServerMethodType *method = NULL;

  SQP_WHILE(RxTpMsg) {
    method = &config->Methods[var->MethodId];
    method->OnTpCopyRxData(((uint32_t)var->ClientId << 16) + var->SessionId, NULL);
    SQP_CRM_AND_FREE(RxTpMsg);
  }
  SQP_WHILE_END()
}

static void SomeIp_MainServerTxTpMsg(const SomeIp_ServerServiceType *config, uint16_t conId) {
  const SomeIp_ServerConnectionType *connection = &config->Connections[conId];
  SomeIp_ServerConnectionContextType *context = connection->Context;
  DEC_SQP(TxTpMsg);
  const SomeIp_ServerMethodType *method = NULL;
  Std_ReturnType ret;

  SQP_WHILE(TxTpMsg) {
    method = &config->Methods[var->MethodId];
    if (var->Timer > 0u) {
      var->Timer--;
    }
    if (0 == var->Timer) {
      ret = SomeIp_SendNextTxTpMsg(connection->TxPduId, config->ServiceId, method->MethodId,
                                   method->InterfaceVersion, SOMEIP_MSG_RESPONSE,
                                   method->OnTpCopyTxData, var);
      if (E_OK == ret) {
        if (var->Offset >= var->Length) {
          SQP_CRM_AND_FREE(TxTpMsg);
        } else {
          var->Timer = config->SeparationTime;
        }
      } else { /* abort this tx */
        (void)method->OnTpCopyTxData(((uint32_t)var->ClientId << 16) + var->SessionId, NULL);
        SQP_CRM_AND_FREE(TxTpMsg);
      }
    }
  }
  SQP_WHILE_END()
}

static void SomeIp_MainServerTxTpMsgOnClose(const SomeIp_ServerServiceType *config,
                                            uint16_t conId) {
  const SomeIp_ServerConnectionType *connection = &config->Connections[conId];
  SomeIp_ServerConnectionContextType *context = connection->Context;
  DEC_SQP(TxTpMsg);
  const SomeIp_ServerMethodType *method = NULL;

  SQP_WHILE(TxTpMsg) {
    method = &config->Methods[var->MethodId];
    (void)method->OnTpCopyTxData(((uint32_t)var->ClientId << 16) + var->SessionId, NULL);
    SQP_CRM_AND_FREE(TxTpMsg);
  }
  SQP_WHILE_END()
}

static void SomeIp_MainServerTxTpEvtMsg(const SomeIp_ServerServiceType *config) {
  SomeIp_ServerContextType *context = config->Context;
  DEC_SQP(TxTpEvtMsg);
  const SomeIp_ServerEventType *event = NULL;
  Sd_EventHandlerSubscriberListType *list;
  Std_ReturnType ret;

  SQP_WHILE(TxTpEvtMsg) {
    event = &config->Events[SOMEIP_CONFIG->TxEvent2PerServiceMap[var->EventId]];
    if (var->Timer > 0u) {
      var->Timer--;
    }
    if (0 == var->Timer) {
      /* NOTE: may result partial data send to later online subscribers */
      ret = Sd_GetSubscribers(event->SdHandleID, &list);
      if (E_OK == ret) {
        ret = SomeIp_SendNextTxTpEvtMsg(config, event, var, list);
        if (E_OK == ret) {
          if (var->Offset >= var->Length) {
            SQP_CRM_AND_FREE(TxTpEvtMsg);
          } else {
            var->Timer = config->SeparationTime;
          }
        } else { /* abort this tx */
          (void)event->OnTpCopyTxData(((uint32_t)var->EventId << 16) + var->SessionId, NULL);
          SQP_CRM_AND_FREE(TxTpEvtMsg);
        }
      } else { /* No Subscribers now */
        (void)event->OnTpCopyTxData(((uint32_t)var->EventId << 16) + var->SessionId, NULL);
        SQP_CRM_AND_FREE(TxTpEvtMsg);
      }
    }
  }
  SQP_WHILE_END()
}

static void SomeIp_MainServerTxTpEvtMsgOnClose(const SomeIp_ServerServiceType *config) {
  SomeIp_ServerContextType *context = config->Context;
  DEC_SQP(TxTpEvtMsg);
  const SomeIp_ServerEventType *event = NULL;

  SQP_WHILE(TxTpEvtMsg) {
    event = &config->Events[SOMEIP_CONFIG->TxEvent2PerServiceMap[var->EventId]];
    (void)event->OnTpCopyTxData(((uint32_t)var->EventId << 16) + var->SessionId, NULL);
    SQP_CRM_AND_FREE(TxTpEvtMsg);
  }
  SQP_WHILE_END()
}

static void SomeIp_MainServer(const SomeIp_ServerServiceType *config) {
  uint16_t conId;

  for (conId = 0u; conId < config->NumOfConnections; conId++) {
    SomeIp_MainServerAsyncRequest(config, conId);
    SomeIp_MainServerTxTpMsg(config, conId);
    SomeIp_MainServerRxTpMsg(config, conId);
  }

  SomeIp_MainServerTxTpEvtMsg(config);
}

static void SomeIp_MainServerOnClose(const SomeIp_ServerServiceType *config, uint16_t conId) {
  SomeIp_MainServerAsyncRequestOnClose(config, conId);
  SomeIp_MainServerTxTpMsgOnClose(config, conId);
  SomeIp_MainServerRxTpMsgOnClose(config, conId);
  SomeIp_MainServerTxTpEvtMsgOnClose(config);
}

static void SomeIp_MainClientRxTpMsg(const SomeIp_ClientServiceType *config) {
  SomeIp_ClientServiceContextType *context = config->Context;
  DEC_SQP(RxTpMsg);
  const SomeIp_ClientMethodType *method = NULL;
  uint32_t requestId;

  SQP_WHILE(RxTpMsg) {
    method = &config->Methods[var->MethodId];

    if (var->Timer > 0u) {
      var->Timer--;
      if (0 == var->Timer) {
        requestId = ((uint32_t)var->ClientId << 16) + var->SessionId;
        ASLOG(SOMEIPE,
              ("client method %x:%x:%x:%d Rx Tp msg timeout, offset %d\n", config->ServiceId,
               method->MethodId, var->ClientId, var->SessionId, var->Offset));
        method->OnError(requestId, SOMEIP_E_TIMEOUT);
        SQP_CRM_AND_FREE(RxTpMsg);
      }
    }
  }
  SQP_WHILE_END()
}

static void SomeIp_MainClientRxTpMsgOnClose(const SomeIp_ClientServiceType *config) {
  SomeIp_ClientServiceContextType *context = config->Context;
  DEC_SQP(RxTpMsg);
  const SomeIp_ClientMethodType *method = NULL;
  uint32_t requestId;

  SQP_WHILE(RxTpMsg) {
    method = &config->Methods[var->MethodId];
    requestId = ((uint32_t)var->ClientId << 16) + var->SessionId;
    method->OnTpCopyRxData(requestId, NULL);
    method->OnError(requestId, SOMEIP_E_NOT_REACHABLE);
    SQP_CRM_AND_FREE(RxTpMsg);
  }
  SQP_WHILE_END()
}

static void SomeIp_MainClientRxTpEvtMsg(const SomeIp_ClientServiceType *config) {
  SomeIp_ClientServiceContextType *context = config->Context;
  const SomeIp_ClientEventType *event;
  uint32_t requestId;
  DEC_SQP(RxTpEvtMsg);

  SQP_WHILE(RxTpEvtMsg) {
    if (var->Timer > 0u) {
      var->Timer--;
      if (0 == var->Timer) {
        event = &config->Events[var->MethodId];
        ASLOG(SOMEIPE,
              ("client event %x:%x:%x:%d Rx Tp msg timeout, offset %d\n", config->ServiceId,
               event->EventId, var->ClientId, var->SessionId, var->Offset));
        requestId = ((uint32_t)var->ClientId << 16) + var->SessionId;
        event->OnTpCopyRxData(requestId, NULL);
        SQP_CRM_AND_FREE(RxTpEvtMsg);
      }
    }
  }
  SQP_WHILE_END()
}

static void SomeIp_MainClientRxTpEvtMsgOnClose(const SomeIp_ClientServiceType *config) {
  SomeIp_ClientServiceContextType *context = config->Context;
  const SomeIp_ClientEventType *event;
  uint32_t requestId;
  DEC_SQP(RxTpEvtMsg);

  SQP_WHILE(RxTpEvtMsg) {
    event = &config->Events[var->MethodId];
    requestId = ((uint32_t)var->ClientId << 16) + var->SessionId;
    event->OnTpCopyRxData(requestId, NULL);
    SQP_CRM_AND_FREE(RxTpEvtMsg);
  }
  SQP_WHILE_END()
}

static void SomeIp_MainClientTxTpMsg(const SomeIp_ClientServiceType *config) {
  SomeIp_ClientServiceContextType *context = config->Context;
  DEC_SQP(TxTpMsg);
  const SomeIp_ClientMethodType *method = NULL;
  Std_ReturnType ret;

  SQP_WHILE(TxTpMsg) {
    method = &config->Methods[var->MethodId];
    if (var->Timer > 0u) {
      var->Timer--;
    }
    if (0u == var->Timer) {
      ret = SomeIp_SendNextTxTpMsg(config->TxPduId, config->ServiceId, method->MethodId,
                                   method->InterfaceVersion, SOMEIP_MSG_REQUEST,
                                   method->OnTpCopyTxData, var);
      if (E_OK == ret) {
        if (var->Offset >= var->Length) {
          SQP_CRM_AND_FREE(TxTpMsg);
          (void)SomeIp_WaitResponse(config, var->MethodId, var->SessionId);
        }
      } else { /* abort this tx */
        SQP_CRM_AND_FREE(TxTpMsg);
      }
    }
  }
  SQP_WHILE_END()
}

static void SomeIp_MainClientTxTpMsgOnClose(const SomeIp_ClientServiceType *config) {
  SomeIp_ClientServiceContextType *context = config->Context;
  DEC_SQP(TxTpMsg);
  const SomeIp_ClientMethodType *method = NULL;
  uint32_t requestId;

  SQP_WHILE(TxTpMsg) {
    method = &config->Methods[var->MethodId];
    requestId = ((uint32_t)var->ClientId << 16) + var->SessionId;
    method->OnTpCopyTxData(requestId, NULL);
    method->OnError(requestId, SOMEIP_E_NOT_REACHABLE);
    SQP_CRM_AND_FREE(TxTpMsg);
  }
  SQP_WHILE_END()
}

static void SomeIp_MainClientWaitResMsg(const SomeIp_ClientServiceType *config) {
  SomeIp_ClientServiceContextType *context = config->Context;
  DEC_SQP(WaitResMsg);
  const SomeIp_ClientMethodType *method = NULL;
  uint32_t requestId;

  SQP_WHILE(WaitResMsg) {
    method = &config->Methods[var->MethodId];
    if (var->Timer > 0u) {
      var->Timer--;
    }
    if (0u == var->Timer) {
      requestId = ((uint32_t)var->MethodId << 16) + var->SessionId;
      ASLOG(SOMEIPE, ("client method %x:%x:%u wait response timeout\n", config->ServiceId,
                      method->MethodId, var->SessionId));
      method->OnError(requestId, SOMEIP_E_TIMEOUT);
      SQP_CRM_AND_FREE(WaitResMsg);
    }
  }
  SQP_WHILE_END()
}

static void SomeIp_MainClientWaitResMsgOnClose(const SomeIp_ClientServiceType *config) {
  SomeIp_ClientServiceContextType *context = config->Context;
  DEC_SQP(WaitResMsg);
  const SomeIp_ClientMethodType *method = NULL;
  uint32_t requestId;

  SQP_WHILE(WaitResMsg) {
    method = &config->Methods[var->MethodId];
    requestId = ((uint32_t)var->MethodId << 16) + var->SessionId;
    method->OnError(requestId, SOMEIP_E_NOT_REACHABLE);
    SQP_CRM_AND_FREE(WaitResMsg);
  }
  SQP_WHILE_END()
}

static void SomeIp_MainClient(const SomeIp_ClientServiceType *config) {
  SomeIp_MainClientTxTpMsg(config);
  SomeIp_MainClientRxTpMsg(config);
  SomeIp_MainClientRxTpEvtMsg(config);
  SomeIp_MainClientWaitResMsg(config);
}

static void SomeIp_MainClientOnClose(const SomeIp_ClientServiceType *config) {
  SomeIp_MainClientTxTpMsgOnClose(config);
  SomeIp_MainClientRxTpMsgOnClose(config);
  SomeIp_MainClientRxTpEvtMsgOnClose(config);
  SomeIp_MainClientWaitResMsgOnClose(config);
}

static void SomeIp_ServerServiceModeChg(const SomeIp_ServerServiceType *service, uint16_t conId,
                                        SoAd_SoConModeType mode) {
  SomeIp_ServerConnectionContextType *context = service->Connections[conId].Context;
  uint16_t i;
  if (SOAD_SOCON_OFFLINE == mode) {
    SomeIp_MainServerOnClose(service, conId);
    context->Online = FALSE;
    service->OnConnect(conId, FALSE);
    for (i = 0u; i < service->NumOfEvents; i++) {
      Sd_RemoveSubscriber(service->Events[i].SdHandleID, service->Connections[conId].TxPduId);
    }
    Sd_NotifyServiceOffline(service->SdHandleID);
  } else {
    context->Online = TRUE;
    service->OnConnect(conId, TRUE);
  }
}

static void SomeIp_ClientServiceModeChg(const SomeIp_ClientServiceType *service,
                                        SoAd_SoConModeType mode) {
  SomeIp_ClientServiceContextType *context = service->Context;

  if (SOAD_SOCON_OFFLINE == mode) {
    SomeIp_MainClientOnClose(service);
    context->Online = FALSE;
    service->OnAvailability(FALSE);
  } else {
    context->Online = TRUE;
    service->OnAvailability(TRUE);
  }
}
static Std_ReturnType SomeIp_HandleRxMsg(PduIdType rxPduId, SomeIp_MsgType *msg) {
  Std_ReturnType ret;
  uint16_t index;
  ASLOG(SOMEIP, ("[%d] service 0x%x:0x%x:%d message type 0x%x return code %d payload %d bytes "
                 "from client 0x%x:%d %d.%d.%d.%d:%d\n",
                 rxPduId, msg->Header.ServiceId, msg->Header.MethodId, msg->Header.InterfaceVersion,
                 msg->Header.MessageType, msg->Header.ReturnCode, msg->Header.Length - 8,
                 msg->Header.ClientId, msg->Header.SessionId, msg->RemoteAddr.Addr[0],
                 msg->RemoteAddr.Addr[1], msg->RemoteAddr.Addr[2], msg->RemoteAddr.Addr[3],
                 msg->RemoteAddr.Port));
  PCAP_TRACE(msg->Header.ServiceId, msg->Header.MethodId, msg->Header.InterfaceVersion,
             msg->Header.MessageType, msg->Header.ReturnCode, msg->Req.Data, msg->Req.Length,
             msg->Header.ClientId, msg->Header.SessionId, &msg->RemoteAddr, msg->Header.IsTpFlag,
             msg->TpHeader.Offset, msg->TpHeader.MoreSegmentsFlag, TRUE);
  if (rxPduId < SOMEIP_CONFIG->NumOfPIDs) {
    index = SOMEIP_CONFIG->PID2ServiceMap[rxPduId];
    if (index < SOMEIP_CONFIG->NumOfService) {
      if (TRUE == SOMEIP_CONFIG->Services[index].IsServer) {
        ret = SomeIp_HandleServerMessage(
          (const SomeIp_ServerServiceType *)SOMEIP_CONFIG->Services[index].Service,
          SOMEIP_CONFIG->PID2ServiceConnectionMap[rxPduId], msg);
      } else {
        ret = SomeIp_HandleClientMessage(
          (const SomeIp_ClientServiceType *)SOMEIP_CONFIG->Services[index].Service, msg);
      }
    } else {
      ret = E_NOT_OK;
      ASLOG(SOMEIPE, ("invalid configuration for PID2ServiceMap\n"));
    }
  } else {
    ret = SOMEIP_E_NOT_REACHABLE;
  }

  return ret;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void SomeIp_RxIndication(PduIdType rxPduId, const PduInfoType *pduInfoPtr) {
  SomeIp_MsgType msg;
  Std_ReturnType ret;

  ret = SomeIp_DecodeMsg(pduInfoPtr, &msg);
  if (E_OK == ret) {
    (void)SomeIp_HandleRxMsg(rxPduId, &msg);
  } else {
    ASLOG(SOMEIPE, ("IF message malformed\n"));
  }
}

void SomeIp_SoConModeChg(SoAd_SoConIdType soConId, SoAd_SoConModeType mode) {
  Std_ReturnType ret = E_NOT_OK;
  const SomeIp_ServiceType *service = NULL;
  const SomeIp_ServerServiceType *ss = NULL;
  SomeIp_ServerContextType *context;
  uint16_t i;
  uint16_t j;
  uint16_t conId = 0u;

  ASLOG(SOMEIP, ("SoConId %d mode %d\n", soConId, mode));
  for (i = 0u; (i < SOMEIP_CONFIG->NumOfService) && (E_NOT_OK == ret); i++) {
    service = &SOMEIP_CONFIG->Services[i];
    if (service->SoConId == soConId) {
      ret = E_OK;
    } else if (TRUE == service->IsServer) {
      ss = (const SomeIp_ServerServiceType *)service->Service;
      for (j = 0u; (j < ss->NumOfConnections) && (E_NOT_OK == ret); j++) {
        if (ss->Connections[j].SoConId == soConId) {
          conId = j;
          ret = E_OK;
        }
      }
    } else {
      /* not match */
    }
  }

  if (E_OK == ret) {
    if (TRUE == service->IsServer) {
      if (service->SoConId == soConId) {
        ss = (const SomeIp_ServerServiceType *)service->Service;
        context = ss->Context;
        if (SOAD_SOCON_OFFLINE == mode) {
          context->Online = FALSE;
        } else {
          context->Online = TRUE;
        }
        if (TCPIP_IPPROTO_TCP == ss->Protocol) {
          if (SOAD_SOCON_OFFLINE == mode) {
            for (j = 0u; j < ss->NumOfConnections; j++) {
              (void)SoAd_CloseSoCon(ss->Connections[j].SoConId, TRUE);
              SomeIp_ServerServiceModeChg(ss, j, SOAD_SOCON_OFFLINE);
            }
          }
        } else {
          SomeIp_ServerServiceModeChg(ss, 0u, mode);
        }
      } else {
        SomeIp_ServerServiceModeChg((const SomeIp_ServerServiceType *)service->Service, conId,
                                    mode);
      }
    } else {
      SomeIp_ClientServiceModeChg((const SomeIp_ClientServiceType *)service->Service, mode);
    }
  } else {
    ASLOG(SOMEIPE, ("SoConId %d unknown\n", soConId));
  }
}

Std_ReturnType SomeIp_Request(uint32_t requestId, uint8_t *data, uint32_t length) {
  return SomeIp_RequestOrFire(requestId, data, length, SOMEIP_MSG_REQUEST);
}

Std_ReturnType SomeIp_FireForgot(uint32_t requestId, uint8_t *data, uint32_t length) {
  return SomeIp_RequestOrFire(requestId, data, length, SOMEIP_MSG_REQUEST_NO_RETURN);
}

Std_ReturnType SomeIp_Notification(uint32_t requestId, uint8_t *data, uint32_t length) {
  Std_ReturnType ret = E_OK;
  uint16_t TxEventId = (requestId >> 16) & 0xFFFFu;
  uint16_t sessionId = requestId & 0xFFFFu;
  const SomeIp_ServerServiceType *config;
  const SomeIp_ServerEventType *event;
  uint16_t index;
  Sd_EventHandlerSubscriberListType *list;
  SomeIp_MessageType msg;

  if (TxEventId < SOMEIP_CONFIG->NumOfTxEvents) {
    index = SOMEIP_CONFIG->TxEvent2ServiceMap[TxEventId];
    config = (const SomeIp_ServerServiceType *)SOMEIP_CONFIG->Services[index].Service;
    if (FALSE == config->Context->Online) {
      ret = E_NOT_OK;
    }
  } else {
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    index = SOMEIP_CONFIG->TxEvent2PerServiceMap[TxEventId];
    event = &config->Events[index];
    ret = Sd_GetSubscribers(event->SdHandleID, &list);
  }

  if (E_OK == ret) {
    msg.Data = data;
    msg.Length = length;
  }

  if (E_OK == ret) {
    ret = SomeIp_CheckTxTpEvtBusy(config, index, TxEventId, length);
  }

  if (E_OK == ret) {
    if (0 == sessionId) {
      EnterCritical();
      sessionId = config->Context->SessionId;
      config->Context->SessionId++;
      if (0 == config->Context->SessionId) {
        config->Context->SessionId = 1u;
      }
      ExitCritical();
    }
    ret = SomeIp_SendNotification(config, TxEventId, sessionId, &msg, list);
  }

  return ret;
}

Std_ReturnType SomeIp_ResolveSubscriber(uint16_t serviceId, Sd_EventHandlerSubscriberType *sub) {
  Std_ReturnType ret = E_NOT_OK;
  Std_ReturnType ret2;
  const SomeIp_ServerServiceType *service = NULL;
  const SomeIp_ServerConnectionType *connection = NULL;
  TcpIp_SockAddrType RemoteAddr;
  uint16_t i;
  int result;

  if (serviceId < SOMEIP_CONFIG->NumOfService) {
    if (TRUE == SOMEIP_CONFIG->Services[serviceId].IsServer) {
      service = (const SomeIp_ServerServiceType *)SOMEIP_CONFIG->Services[serviceId].Service;
      if (TRUE == service->Context->Online) {
        if (TCPIP_IPPROTO_UDP == service->Protocol) {
          connection = &service->Connections[0];
          sub->TxPduId = connection->TxPduId;
          ret = E_OK;
        } else {
          for (i = 0u; i < service->NumOfConnections; i++) {
            connection = &service->Connections[i];
            if (TRUE == connection->Context->Online) {
              ret2 = SoAd_GetRemoteAddr(connection->SoConId, &RemoteAddr);
              if (E_OK == ret2) {
                result = memcmp(&sub->RemoteAddr, &RemoteAddr, sizeof(RemoteAddr));
                if (0 == result) {
                  sub->TxPduId = connection->TxPduId;
                  ret = E_OK;
                  break;
                }
              }
            }
          }
        }
      } else {
        ASLOG(SOMEIPE, ("service[%d] not online\n", serviceId));
      }
    }
  }

  return ret;
}

void SomeIp_Init(const SomeIp_ConfigType *configPtr) {
  uint16_t i;
  if (NULL != configPtr) {
    someIpConfigPtr = configPtr;
  } else {
    someIpConfigPtr = &SomeIp_Config;
  }
  SQP_INIT(AsyncReqMsg);
  SQP_INIT(TxTpMsg);
  SQP_INIT(RxTpMsg);
  SQP_INIT(TxTpEvtMsg);
  SQP_INIT(WaitResMsg);
  for (i = 0u; i < SOMEIP_CONFIG->NumOfService; i++) {
    if (TRUE == SOMEIP_CONFIG->Services[i].IsServer) {
      SomeIp_InitServer((const SomeIp_ServerServiceType *)SOMEIP_CONFIG->Services[i].Service);
    } else {
      SomeIp_InitClient((const SomeIp_ClientServiceType *)SOMEIP_CONFIG->Services[i].Service);
    }
  }
}

void SomeIp_MainFunction(void) {
  uint16_t i;
  boolean bLinkedUp = TcpIp_IsLinkedUp();
  for (i = 0u; (TRUE == bLinkedUp) && (i < SOMEIP_CONFIG->NumOfService); i++) {
    if (TRUE == SOMEIP_CONFIG->Services[i].IsServer) {
      SomeIp_MainServer((const SomeIp_ServerServiceType *)SOMEIP_CONFIG->Services[i].Service);
    } else {
      SomeIp_MainClient((const SomeIp_ClientServiceType *)SOMEIP_CONFIG->Services[i].Service);
    }
  }
}

Std_ReturnType SomeIp_ConnectionTakeControl(uint16_t serviceId, uint16_t conId) {
  Std_ReturnType ret = E_NOT_OK;
  const SomeIp_ServerServiceType *config;
  const SomeIp_ServerConnectionType *connection;
  SomeIp_ServerConnectionContextType *context;

  if (serviceId < SOMEIP_CONFIG->NumOfService) {
    if (TRUE == SOMEIP_CONFIG->Services[serviceId].IsServer) {
      config = SOMEIP_CONFIG->Services[serviceId].Service;
      if (conId < config->NumOfConnections) {
        connection = &config->Connections[conId];
        context = connection->Context;
        ret = SoAd_TakeControl(connection->SoConId);
        if (E_OK == ret) {
          ret = SoAd_SetTimeout(connection->SoConId, 10u);
          context->TakenControled = TRUE;
        }
      }
    }
  }

  return ret;
}

Std_ReturnType SomeIp_ConnectionRxControl(uint16_t serviceId, uint16_t conId,
                                          SomeIp_CtrlRxBufferType *buffer) {
  Std_ReturnType ret = E_NOT_OK;
  const SomeIp_ServerServiceType *config;
  const SomeIp_ServerConnectionType *connection;
  SomeIp_ServerConnectionContextType *context;
  PduInfoType PduInfo;
  TcpIp_SockAddrType RemoteAddr;
  SomeIp_MsgType msg;

  if (serviceId < SOMEIP_CONFIG->NumOfService) {
    if (TRUE == SOMEIP_CONFIG->Services[serviceId].IsServer) {
      config = SOMEIP_CONFIG->Services[serviceId].Service;
      if (conId < config->NumOfConnections) {
        connection = &config->Connections[conId];
        context = connection->Context;
        if (TRUE == context->TakenControled) {
          ret = E_OK;
        }
      }
    }
  }
  if (E_OK == ret) {
    if (TCPIP_IPPROTO_UDP == config->Protocol) {
      buffer->Length = buffer->Size;
      ret = SoAd_ControlRecv(connection->SoConId, buffer->Data, &buffer->Length);
      if ((E_OK == ret) && (buffer->Length > 0u)) {
        buffer->Offset = buffer->Length;
      }
    } else {
      if (buffer->Offset < 16) {
        buffer->Length = 16 - buffer->Offset;
        ret = SoAd_ControlRecv(connection->SoConId, &buffer->Data[buffer->Offset], &buffer->Length);
        if ((E_OK == ret) && (buffer->Length > 0u)) {
          buffer->Offset += buffer->Length;
          if (buffer->Offset >= 16) {
            (void)SoAd_GetRemoteAddr(connection->SoConId, &RemoteAddr);
            PduInfo.SduDataPtr = buffer->Data;
            PduInfo.SduLength = buffer->Offset;
            PduInfo.MetaDataPtr = (uint8_t *)&RemoteAddr;
            ret = SomeIp_HeaderIndication(connection->RxPduId, &PduInfo, &buffer->Length);
            if (E_OK == ret) {
              if (buffer->Length > buffer->Size) {
                ASLOG(SOMEIPE, ("SOMEIP message payload length %d exceeds buffer size %d\n",
                                buffer->Length, buffer->Size));
                while (buffer->Offset < buffer->Length) {
                  PduInfo.SduLength = buffer->Length - buffer->Offset;
                  if (PduInfo.SduLength > buffer->Size) {
                    PduInfo.SduLength = buffer->Size;
                    /* drop the rest */
                    (void)SoAd_ControlRecv(connection->SoConId, buffer->Data, &PduInfo.SduLength);
                    buffer->Offset += PduInfo.SduLength;
                  }
                }
                buffer->Offset = 0u;
                ret = E_NOT_OK;
              }
            } else {
              buffer->Offset = 0u;
            }
          }
        }
      }
      if ((buffer->Offset >= 16) && (buffer->Offset < buffer->Length)) {
        PduInfo.SduLength = buffer->Length - buffer->Offset;
        ret =
          SoAd_ControlRecv(connection->SoConId, &buffer->Data[buffer->Offset], &PduInfo.SduLength);
        if ((E_OK == ret) && (PduInfo.SduLength > 0u)) {
          buffer->Offset += PduInfo.SduLength;
        }
      } else {
        ASLOG(SOMEIPE, ("SOMEIP message offset %u exceeds payload size %u\n", buffer->Offset,
                        buffer->Length));
        buffer->Offset = 0u;
      }
    }
  }

  if (E_OK == ret) {
    if ((buffer->Offset >= buffer->Length) && (buffer->Offset >= 16)) {
      (void)SoAd_GetRemoteAddr(connection->SoConId, &RemoteAddr);
      PduInfo.SduDataPtr = buffer->Data;
      PduInfo.SduLength = buffer->Offset;
      PduInfo.MetaDataPtr = (uint8_t *)&RemoteAddr;
      ret = SomeIp_DecodeMsg(&PduInfo, &msg);
      if (E_OK == ret) {
        ret = SomeIp_HandleRxMsg(connection->RxPduId, &msg);
      } else {
        ASLOG(SOMEIPE, ("IF message malformed\n"));
      }
      buffer->Offset = 0u;
    } else {
      ret = SOMEIP_E_PENDING;
    }
  }
  return ret;
}

Std_ReturnType SomeIp_HeaderIndication(PduIdType rxPduId, const PduInfoType *info,
                                       uint32_t *payloadLength) {
  Std_ReturnType ret = E_OK;
  uint8_t *data = info->SduDataPtr;
  (void)rxPduId;

  if (info->SduLength < 8u) {
    ASLOG(SOMEIPE, ("malformed SOMEIP message\n"));
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    *payloadLength =
      ((uint32_t)data[4] << 24) + ((uint32_t)data[5] << 16) + ((uint32_t)data[6] << 8u) + data[7];
  }

  return ret;
}

void SomeIp_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x01, SOMEIP_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_SOMEIPTP;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 0;
}
