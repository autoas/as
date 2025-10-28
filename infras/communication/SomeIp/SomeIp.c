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
#include "NetMem.h"
#include <string.h>
#ifdef USE_PCAP
#include "pcap.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_SOMEIP 0
#define AS_LOG_SOMEIPI 2
#define AS_LOG_SOMEIPW 3
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

#ifndef SOMEIP_ASYNC_REQUEST_MESSAGE_POOL_SIZE
#define SOMEIP_ASYNC_REQUEST_MESSAGE_POOL_SIZE 8u
#endif

#ifndef SOMEIP_RX_TP_MESSAGE_POOL_SIZE
#define SOMEIP_RX_TP_MESSAGE_POOL_SIZE 8u
#endif

#ifndef SOMEIP_TX_TP_MESSAGE_POOL_SIZE
#define SOMEIP_TX_TP_MESSAGE_POOL_SIZE 8u
#endif

#ifndef SOMEIP_TX_TP_EVENT_MESSAGE_POOL_SIZE
#define SOMEIP_TX_TP_EVENT_MESSAGE_POOL_SIZE 8u
#endif

#ifndef SOMEIP_TCP_BUFFER_POOL_SIZE
#define SOMEIP_TCP_BUFFER_POOL_SIZE 8u
#endif

#ifndef SOMEIP_WAIT_RESPOSE_MESSAGE_POOL_SIZE
#define SOMEIP_WAIT_RESPOSE_MESSAGE_POOL_SIZE 8u
#endif

#ifndef SOMEIP_TX_NOK_RETRY_MAX
#define SOMEIP_TX_NOK_RETRY_MAX 3u
#endif

#ifdef USE_PCAP
#define PCAP_TRACE PCap_SomeIp
#else
#define PCAP_TRACE(serviceId, methodId, interfaceVersion, messageType, returnCode, payload,        \
                   payloadLength, clientId, sessionId, RemoteAddr, isTp, offset, more, isRx)
#endif

#define IS_TP_ENABLED(obj) (NULL != (obj)->onTpCopyTxData)

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
    var = STAILQ_FIRST(&context->pending##T##s);                                                   \
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
    STAILQ_REMOVE(&context->pending##T##s, var, SomeIp_##T##_s, entry);                            \
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
    STAILQ_INSERT_TAIL(&context->pending##T##s, var, entry);                                       \
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
    var = STAILQ_FIRST(&context->pending##T##s);                                                   \
    while (NULL != var) {                                                                          \
      STAILQ_REMOVE_HEAD(&context->pending##T##s, entry);                                          \
      mp_free(&someIp##T##Pool, (uint8_t *)var);                                                   \
      var = STAILQ_FIRST(&context->pending##T##s);                                                 \
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
      header->length =
        ((uint32_t)data[4] << 24) + ((uint32_t)data[5] << 16) + ((uint32_t)data[6] << 8u) + data[7];
      header->serviceId = ((uint16_t)data[0] << 8u) + data[1];
      header->methodId = ((uint16_t)data[2] << 8u) + data[3];
      header->clientId = ((uint16_t)data[8] << 8u) + data[9];
      header->sessionId = ((uint16_t)data[10] << 8u) + data[11];
      header->interfaceVersion = data[13];
      header->messageType = data[14];
      header->returnCode = data[15];
      if (0u != (header->messageType & SOMEIP_TP_FLAG)) {
        header->messageType &= ~SOMEIP_TP_FLAG;
        header->isTpFlag = TRUE;
        if ((header->length > (8u + 4u + SOMEIP_TP_MAX)) || (header->length <= (8u + 4u))) {
          ASLOG(SOMEIPE, ("TP length not valid\n"));
          ret = SOMEIP_E_MALFORMED_MESSAGE;
        }
      } else {
        header->isTpFlag = FALSE;
      }
      if (header->length < 8u) {
        ret = E_NOT_OK;
        ASLOG(SOMEIPE, ("invalid length\n"));
      } else if ((header->length + 8u) > length) {
        ret = SOMEIP_E_MSG_TOO_SHORT;
      } else if ((header->length + 8u) < length) {
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

static Std_ReturnType SomeIp_DecodeMsg(const PduInfoType *PduInfoPtr, SomeIp_MsgType *msg) {
  Std_ReturnType ret;
  uint32_t offset;

  ret = SomeIp_DecodeHeader(PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength, &msg->header);
  if (E_OK == ret) {
    msg->RemoteAddr = *(const TcpIp_SockAddrType *)PduInfoPtr->MetaDataPtr;
    if (TRUE == msg->header.isTpFlag) {
      offset = ((uint32_t)PduInfoPtr->SduDataPtr[16] << 24) +
               ((uint32_t)PduInfoPtr->SduDataPtr[17] << 16) +
               ((uint32_t)PduInfoPtr->SduDataPtr[18] << 8u) + PduInfoPtr->SduDataPtr[19];
      msg->tpHeader.offset = offset & 0xFFFFFFF0UL;
      msg->tpHeader.moreSegmentsFlag = offset & 0x01u;
      msg->req.data = &PduInfoPtr->SduDataPtr[20];
      msg->req.length = PduInfoPtr->SduLength - 20u;
    } else {
      msg->req.data = &PduInfoPtr->SduDataPtr[16];
      msg->req.length = PduInfoPtr->SduLength - 16u;
    }
  }

  return ret;
}

#ifdef SOMEIP_USE_TP_BUF
static Std_ReturnType SomeIp_DecodeMsgTp(uint8_t *header, uint8_t *data, uint32_t length,
                                         const TcpIp_SockAddrType *RemoteAddr,
                                         SomeIp_MsgType *msg) {
  Std_ReturnType ret;
  uint32_t offset;

  ret = SomeIp_DecodeHeader(header, length + 16u, &msg->header);
  if (E_OK == ret) {
    msg->RemoteAddr = *RemoteAddr;
    if (TRUE == msg->header.isTpFlag) {
      offset =
        ((uint32_t)data[0] << 24) + ((uint32_t)data[1] << 16) + ((uint32_t)data[2] << 8u) + data[3];
      msg->tpHeader.offset = offset & 0xFFFFFFF0UL;
      msg->tpHeader.moreSegmentsFlag = offset & 0x01u;
      msg->req.data = &data[4];
      msg->req.length = length - 4u;
    } else {
      msg->req.data = data;
      msg->req.length = length;
    }
  }

  return ret;
}

static Std_ReturnType SomeIp_HandleTpHeader(SomeIp_TpBufferType *tpBuf, PduInfoType *PduInfoPtr,
                                            SomeIp_MsgType *msg) {
  Std_ReturnType ret;
  uint32_t leftLen;
  leftLen = sizeof(tpBuf->header) - tpBuf->lenOfHd;
  if (PduInfoPtr->SduLength < leftLen) {
    leftLen = PduInfoPtr->SduLength;
  }
  if (leftLen > 0u) {
    (void)memcpy(&tpBuf->header[tpBuf->lenOfHd], PduInfoPtr->SduDataPtr, leftLen);
    PduInfoPtr->SduDataPtr = &PduInfoPtr->SduDataPtr[leftLen];
    PduInfoPtr->SduLength -= leftLen;
    tpBuf->lenOfHd += leftLen;
  }

  ASLOG(SOMEIP, ("header %d(+%d), data %d\n", tpBuf->lenOfHd, leftLen, PduInfoPtr->SduLength));

  if (sizeof(tpBuf->header) == tpBuf->lenOfHd) {
    ret = SomeIp_DecodeMsgTp(tpBuf->header, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength,
                             (const TcpIp_SockAddrType *)PduInfoPtr->MetaDataPtr, msg);
  } else {
    ret = SOMEIP_E_PENDING;
  }

  return ret;
}
#endif /* SOMEIP_USE_TP_BUF */

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

static Std_ReturnType SomeIp_Transmit(PduIdType TxPduId, TcpIp_SockAddrType *RemoteAddr,
                                      uint8_t *data, uint16_t serviceId, uint16_t methodId,
                                      uint16_t clientId, uint16_t sessionId,
                                      uint8_t interfaceVersion, uint8_t messageType,
                                      uint8_t returnCode, uint32_t payloadLength) {
  Std_ReturnType ret;
  PduInfoType PduInfo;

  PduInfo.MetaDataPtr = (uint8_t *)RemoteAddr;
  PduInfo.SduDataPtr = data;
  PduInfo.SduLength = payloadLength + 16u;
  SomeIp_BuildHeader(data, serviceId, methodId, clientId, sessionId, interfaceVersion, messageType,
                     returnCode, payloadLength);
  ret = SoAd_IfTransmit(TxPduId, &PduInfo);
  if (E_OK != ret) {
    ASLOG(SOMEIPE, ("Fail to send msg\n"));
  } else {
    PCAP_TRACE(serviceId, methodId, interfaceVersion, messageType, returnCode, &data[16],
               payloadLength, clientId, sessionId, RemoteAddr, FALSE, 0u, FALSE, FALSE);
  }
  return ret;
}

static Std_ReturnType SomeIp_TransError(PduIdType TxPduId, TcpIp_SockAddrType *RemoteAddr,
                                        uint16_t serviceId, uint16_t methodId, uint16_t clientId,
                                        uint16_t sessionId, uint8_t interfaceVersion,
                                        uint8_t messageType, uint8_t returnCode) {
  Std_ReturnType ret;
  PduInfoType PduInfo;
  uint8_t errMsg[16];

  PduInfo.MetaDataPtr = (uint8_t *)RemoteAddr;
  PduInfo.SduDataPtr = errMsg;
  PduInfo.SduLength = 16u;
  SomeIp_BuildHeader(errMsg, serviceId, methodId, clientId, sessionId, interfaceVersion,
                     messageType, returnCode, 0u);
  ret = SoAd_IfTransmit(TxPduId, &PduInfo);
  if (E_OK != ret) {
    ASLOG(SOMEIPE, ("Fail to send error\n"));
  } else {
    PCAP_TRACE(serviceId, methodId, interfaceVersion, messageType, returnCode, NULL, 0u, clientId,
               sessionId, RemoteAddr, FALSE, 0u, FALSE, FALSE);
  }
  return ret;
}

static Std_ReturnType SomeIp_ReplyError(PduIdType TxPduId, SomeIp_MsgType *msg, uint8_t errorCode) {
  Std_ReturnType ret = SomeIp_TransError(
    TxPduId, &msg->RemoteAddr, msg->header.serviceId, msg->header.methodId, msg->header.clientId,
    msg->header.sessionId, msg->header.interfaceVersion, SOMEIP_MSG_ERROR, errorCode);
  return ret;
}

static void SomeIp_InitServer(const SomeIp_ServerServiceType *config) {
  uint16_t i;
  for (i = 0u; i < config->numOfConnections; i++) {
    (void)memset(config->connections[i].context, 0u, sizeof(SomeIp_ServerConnectionContextType));
    STAILQ_INIT(&(config->connections[i].context->pendingAsyncReqMsgs));
    STAILQ_INIT(&(config->connections[i].context->pendingRxTpMsgs));
    STAILQ_INIT(&(config->connections[i].context->pendingTxTpMsgs));
#ifdef SOMEIP_USE_TP_BUF
    if (NULL != config->connections[i].tpBuf) {
      (void)memset(config->connections[i].tpBuf, 0u, sizeof(SomeIp_TpBufferType));
    }
#endif
  }
  (void)memset(config->context, 0u, sizeof(SomeIp_ServerContextType));
  STAILQ_INIT(&(config->context->pendingTxTpEvtMsgs));
}

static void SomeIp_InitClient(const SomeIp_ClientServiceType *config) {
  SomeIp_ClientServiceContextType *context = config->context;
  (void)memset(context, 0u, sizeof(SomeIp_ClientServiceContextType));
  STAILQ_INIT(&(context->pendingRxTpEvtMsgs));
  STAILQ_INIT(&(context->pendingRxTpMsgs));
  STAILQ_INIT(&(context->pendingTxTpMsgs));
  STAILQ_INIT(&(context->pendingWaitResMsgs));
#ifdef SOMEIP_USE_TP_BUF
  if (NULL != config->tpBuf) {
    (void)memset(config->tpBuf, 0u, sizeof(SomeIp_TpBufferType));
  }
#endif
}

static SomeIp_RxTpMsgType *SomeIp_RxTpMsgFind(SomeIp_RxTpMsgList *pendingRxTpMsgs,
                                              uint16_t methodId) {
  SomeIp_RxTpMsgType *rxTpMsg = NULL;
  SomeIp_RxTpMsgType *var;
  EnterCritical();
  STAILQ_FOREACH(var, pendingRxTpMsgs, entry) {
    if (var->methodId == methodId) {
      rxTpMsg = var;
      break;
    }
  }
  ExitCritical();
  return rxTpMsg;
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
    var = SomeIp_RxTpMsgFind(pendingRxTpMsgs, methodId);
    if (NULL == var) {
      if ((0u == msg->tpHeader.offset) && (msg->tpHeader.moreSegmentsFlag)) {
        ASLOG(SOMEIP, ("%x:%x:%x:%d FF length = %u\n", msg->header.serviceId, msg->header.methodId,
                       msg->header.clientId, msg->header.sessionId, msg->req.length));
        SQP_ALLOC(RxTpMsg);
        if (NULL == var) {
          ret = SOMEIP_E_NOMEM;
          ASLOG(SOMEIPE, ("%x:%x:%x:%d OoM for Tp Rx\n", msg->header.serviceId,
                          msg->header.methodId, msg->header.clientId, msg->header.sessionId));
        } else {
          var->offset = 0u;
          var->clientId = msg->header.clientId;
          var->sessionId = msg->header.sessionId;
          var->RemoteAddr = msg->RemoteAddr;
          var->methodId = methodId;
          var->timer = SOMEIP_CONFIG->TpRxTimeoutTime;
          SQP_LAPPEND(RxTpMsg);
        }
      } else {
        ret = SOMEIP_E_MALFORMED_MESSAGE;
        ASLOG(SOMEIPE, ("%x:%x:%x:%d Tp message malformed or loss\n", msg->header.serviceId,
                        msg->header.methodId, msg->header.clientId, msg->header.sessionId));
      }
    } else {
      result = memcmp(&var->RemoteAddr, &msg->RemoteAddr, sizeof(TcpIp_SockAddrType));
      if ((var->clientId == msg->header.clientId) && (var->sessionId == msg->header.sessionId) &&
          (var->offset == msg->tpHeader.offset) && (0 == result)) {
        var->timer = SOMEIP_CONFIG->TpRxTimeoutTime;
        ASLOG(SOMEIP, ("%x:%x:%x:%d %s length = %u, offset = %d\n", msg->header.serviceId,
                       msg->header.methodId, msg->header.clientId, msg->header.sessionId,
                       msg->tpHeader.moreSegmentsFlag ? "CF" : "LF", msg->req.length, var->offset));
      } else {
        SQP_LRM_AND_FREE(RxTpMsg);
        ret = SOMEIP_E_MALFORMED_MESSAGE;
        ASLOG(SOMEIPE,
              ("%x:%x:%x:%d Tp message not as expected, loss maybe\n", msg->header.serviceId,
               msg->header.methodId, msg->header.clientId, msg->header.sessionId));
      }
    }
  } else {
    ret = SOMEIP_E_MALFORMED_MESSAGE;
  }

  if (E_OK == ret) {
    tpMsg.data = msg->req.data;
    tpMsg.length = msg->req.length;
    tpMsg.offset = msg->tpHeader.offset;
    tpMsg.moreSegmentsFlag = msg->tpHeader.moreSegmentsFlag;
    requestId = ((uint32_t)msg->header.clientId << 16) + msg->header.sessionId;
    ret = onTpCopyRxData(requestId, &tpMsg);
    if (E_OK == ret) {
      var->offset += msg->req.length;
      if (TRUE == msg->tpHeader.moreSegmentsFlag) {
        ret = SOMEIP_E_OK_SILENT;
      } else {
        msg->req.data = tpMsg.data;
        msg->req.length = var->offset;
        SQP_LRM_AND_FREE(RxTpMsg);
      }
    }
  }

  return ret;
}

static Std_ReturnType SomeIp_SendNextTxTpMsg(PduIdType TxPduId, uint16_t serviceId,
                                             uint16_t methodId, uint8_t interfaceVersion,
                                             uint8_t messageType,
                                             SomeIp_OnTpCopyTxDataFncType onTpCopyTxData,
                                             SomeIp_TxTpMsgType *var) {
  Std_ReturnType ret = E_OK;
  SomeIp_TpMessageType tpMsg;
  uint32_t len = var->length - var->offset;
  uint32_t offset = var->offset;
  uint32_t requestId;
  uint8_t *data;

  if (len > SOMEIP_TP_MAX) {
    len = SOMEIP_TP_MAX;
    offset |= 0x01u; /* setup more flag */
    tpMsg.moreSegmentsFlag = TRUE;
  } else {
    tpMsg.moreSegmentsFlag = FALSE;
  }

  data = (uint8_t *)Net_MemAlloc(len + 20u);
  if (NULL != data) {
    tpMsg.data = &data[20];
    tpMsg.length = len;
    tpMsg.offset = var->offset;
    requestId = ((uint32_t)var->clientId << 16) + var->sessionId;
    ret = onTpCopyTxData(requestId, &tpMsg);
    if (E_OK == ret) {
      data[16] = (offset >> 24) & 0xFFu;
      data[17] = (offset >> 16) & 0xFFu;
      data[18] = (offset >> 8u) & 0xFFu;
      data[19] = offset & 0xFFu;
      ret = SomeIp_Transmit(TxPduId, &var->RemoteAddr, data, serviceId, methodId, var->clientId,
                            var->sessionId, interfaceVersion, messageType | SOMEIP_TP_FLAG, E_OK,
                            len + 4u);
      if (E_OK == ret) {
        var->offset += len;
#ifndef DISABLE_SOMEIP_TX_NOK_RETRY
      } else if ((E_NOT_OK == ret) && (var->retryCounter < SOMEIP_TX_NOK_RETRY_MAX)) {
        var->retryCounter++;
        ASLOG(SOMEIPW, ("Tx TP NOK, try %d\n", var->retryCounter));
        ret = E_OK;
#endif
      } else {
        ASLOG(SOMEIPE, ("Tx TP NOK\n"));
      }
    }
  } else {
    ASLOG(SOMEIPW, ("OoM, schedule Tx TP msg next time\n"));
    ret = E_OK;
  }

  if (NULL != data) {
    Net_MemFree(data);
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
    if (0u != sub->flags) {
      if (0u == (SD_FLG_EVENT_GROUP_MULTICAST & sub->flags)) {
        RemoteAddr = &sub->RemoteAddr;
        if (FALSE == bMulticast) {
          bMulticast = TRUE;
        }
      }
      ret2 = SomeIp_Transmit(sub->TxPduId, RemoteAddr, data, config->serviceId, event->eventId,
                             config->clientId, sessionId, event->interfaceVersion, messageType, 0u,
                             payloadLength);
      if (E_OK == ret2) {
        ret = E_OK;
      } else {
        ASLOG(SOMEIPE, ("Failed to notify event %x:%x to %d.%d.%d.%d:%d\n", config->serviceId,
                        event->eventId, sub->RemoteAddr.addr[0], sub->RemoteAddr.addr[1],
                        sub->RemoteAddr.addr[2], sub->RemoteAddr.addr[3], sub->RemoteAddr.port));
      }
    }
    /* NOTE: if any one is unsubscribed during this, TX is broken */
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
  uint32_t len = var->length - var->offset;
  uint32_t offset = var->offset;
  uint8_t *data;
  uint32_t requestId;

  if (len > SOMEIP_TP_MAX) {
    len = SOMEIP_TP_MAX;
    offset |= 0x01u; /* setup more flag */
    tpMsg.moreSegmentsFlag = TRUE;
  } else {
    tpMsg.moreSegmentsFlag = FALSE;
  }

  data = (uint8_t *)Net_MemAlloc(len + 20u);
  if (NULL != data) {
    tpMsg.data = &data[20];
    tpMsg.length = len;
    tpMsg.offset = var->offset;
    requestId = ((uint32_t)var->eventId << 16) + var->sessionId;
    ret = event->onTpCopyTxData(requestId, &tpMsg);
    if (E_OK == ret) {
      data[16] = (offset >> 24) & 0xFFu;
      data[17] = (offset >> 16) & 0xFFu;
      data[18] = (offset >> 8u) & 0xFFu;
      data[19] = offset & 0xFFu;
      ret = SomeIp_TransmitEvtMsg(config, event, data, len + 4, TRUE, list, var->sessionId);
      if (E_OK == ret) {
        var->offset += len;
      } else {
        ASLOG(SOMEIPE, ("Tx TP EVT NOK\n"));
      }
    }
  } else {
    ASLOG(SOMEIPW, ("OoM, schedule Tx TP EVT msg next time\n"));
    ret = E_OK;
  }

  if (NULL != data) {
    Net_MemFree(data);
  }

  return ret;
}

static Std_ReturnType SomeIp_ReplyRequest(const SomeIp_ServerServiceType *config, uint16_t conId,
                                          uint16_t methodId, uint16_t clientId, uint16_t sessionId,
                                          TcpIp_SockAddrType *RemoteAddr, SomeIp_MessageType *res) {
  Std_ReturnType ret = E_OK;
  const SomeIp_ServerConnectionType *connection = &config->connections[conId];
  SomeIp_ServerConnectionContextType *context = connection->context;
  const SomeIp_ServerMethodType *method = &config->methods[methodId];
  SomeIp_TxTpMsgType *var;

  if (IS_TP_ENABLED(method) && (res->length > SOMEIP_SF_MAX)) {
    SQP_ALLOC(TxTpMsg);
    if (NULL != var) {
      var->clientId = clientId;
      var->sessionId = sessionId;
      var->methodId = methodId;
      var->RemoteAddr = *RemoteAddr;
      var->offset = 0u;
      var->length = res->length;
#ifndef DISABLE_SOMEIP_TX_NOK_RETRY
      var->retryCounter = 0u;
#endif
      var->timer = config->SeparationTime;
      ret = SomeIp_SendNextTxTpMsg(connection->TxPduId, config->serviceId, method->methodId,
                                   method->interfaceVersion, SOMEIP_MSG_RESPONSE,
                                   method->onTpCopyTxData, var);
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
    ret = SomeIp_Transmit(connection->TxPduId, RemoteAddr, res->data, config->serviceId,
                          method->methodId, clientId, sessionId, method->interfaceVersion,
                          SOMEIP_MSG_RESPONSE, E_OK, res->length);
  }

  return ret;
}

static Std_ReturnType SomeIp_WaitResponse(const SomeIp_ClientServiceType *config, uint16_t methodId,
                                          uint16_t sessionId) {
  Std_ReturnType ret = E_OK;
  SomeIp_ClientServiceContextType *context = config->context;
  SomeIp_WaitResMsgType *var;

  SQP_ALLOC(WaitResMsg);
  if (NULL != var) {
    var->methodId = methodId;
    var->sessionId = sessionId;
    var->timer = config->ResponseTimeout;
    SQP_CAPPEND(WaitResMsg);
  } else {
    ASLOG(SOMEIPE, ("OoM for wait res msg\n"));
    ret = E_NOT_OK;
  }

  return ret;
}

static Std_ReturnType SomeIp_SendRequest(const SomeIp_ClientServiceType *config, uint16_t methodId,
                                         uint16_t clientId, uint16_t sessionId,
                                         TcpIp_SockAddrType *RemoteAddr, SomeIp_MessageType *req,
                                         uint8_t messageType) {
  Std_ReturnType ret = E_OK;
  SomeIp_ClientServiceContextType *context = config->context;
  const SomeIp_ClientMethodType *method = &config->methods[methodId];
  SomeIp_TxTpMsgType *var;
  uint8_t *data;

  if (IS_TP_ENABLED(method) && (req->length > SOMEIP_SF_MAX)) {
    SQP_ALLOC(TxTpMsg);
    if (NULL != var) {
      var->clientId = clientId;
      var->sessionId = sessionId;
      var->methodId = methodId;
      var->RemoteAddr = *RemoteAddr;
      var->offset = 0u;
      var->length = req->length;
      var->messageType = messageType;
#ifndef DISABLE_SOMEIP_TX_NOK_RETRY
      var->retryCounter = 0u;
#endif
      var->timer = config->SeparationTime;
      ret =
        SomeIp_SendNextTxTpMsg(config->TxPduId, config->serviceId, method->methodId,
                               method->interfaceVersion, messageType, method->onTpCopyTxData, var);
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
    data = (uint8_t *)Net_MemAlloc(req->length + 16u);
    if (NULL != data) {
      (void)memcpy(&data[16], req->data, req->length);
      ret = SomeIp_Transmit(config->TxPduId, RemoteAddr, data, config->serviceId, method->methodId,
                            clientId, sessionId, method->interfaceVersion, messageType, E_OK,
                            req->length);
      Net_MemFree(data);
      if (E_OK == ret) {
        ret = SomeIp_WaitResponse(config, methodId, sessionId);
      }
    } else {
      ASLOG(SOMEIPE, ("OoM for request SF Tx\n"));
      ret = E_NOT_OK;
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

  if (TxMethodId < SOMEIP_CONFIG->numOfTxMethods) {
    index = SOMEIP_CONFIG->TxMethod2ServiceMap[TxMethodId];
    config = (const SomeIp_ClientServiceType *)SOMEIP_CONFIG->services[index].service;
    context = config->context;
    if (FALSE == context->online) {
      ret = E_NOT_OK;
    }
  } else {
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    index = SOMEIP_CONFIG->TxMethod2PerServiceMap[TxMethodId];
    ret = Sd_GetProviderAddr(config->sdHandleID, &RemoteAddr);
    if (E_OK == ret) {
      msg.data = data;
      msg.length = length;
    }
  }

  if (E_OK == ret) {
    ret = SomeIp_SendRequest(config, index, config->clientId, sessionId, &RemoteAddr, &msg,
                             messageType);
  }

  return ret;
}

static Std_ReturnType SomeIp_SendNotification(const SomeIp_ServerServiceType *config,
                                              uint16_t eventId, uint16_t sessionId,
                                              SomeIp_MessageType *req,
                                              Sd_EventHandlerSubscriberListType *list) {
  Std_ReturnType ret = E_OK;
  SomeIp_ServerContextType *context = config->context;
  const SomeIp_ServerEventType *event =
    &config->events[SOMEIP_CONFIG->TxEvent2PerServiceMap[eventId]];
  SomeIp_TxTpEvtMsgType *var;
  uint8_t *data;

  if (IS_TP_ENABLED(event) && (req->length > SOMEIP_SF_MAX)) {
    SQP_ALLOC(TxTpEvtMsg);
    if (NULL != var) {
      var->eventId = eventId;
      var->sessionId = sessionId;
      var->offset = 0u;
      var->length = req->length;
      var->timer = config->SeparationTime;
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
    data = (uint8_t *)Net_MemAlloc(req->length + 16u);
    if (NULL != data) {
      (void)memcpy(&data[16], req->data, req->length);
      ret = SomeIp_TransmitEvtMsg(config, event, data, req->length, FALSE, list, sessionId);
      Net_MemFree(data);
    } else {
      ASLOG(SOMEIPE, ("OoM for notification SF Tx\n"));
      ret = E_NOT_OK;
    }
  }

  return ret;
}

static Std_ReturnType SomeIp_ProcessRequest(const SomeIp_ServerServiceType *config, uint16_t conId,
                                            uint16_t methodId, SomeIp_MsgType *msg) {
  Std_ReturnType ret = E_OK;
  uint8_t *resData = NULL;
  const SomeIp_ServerConnectionType *connection = &config->connections[conId];
  SomeIp_ServerConnectionContextType *context = connection->context;
  const SomeIp_ServerMethodType *method = &config->methods[methodId];
  SomeIp_AsyncReqMsgType *var;
  SomeIp_MessageType res;
  uint32_t requestId = ((uint32_t)msg->header.clientId << 16) + msg->header.sessionId;

  resData = (uint8_t *)Net_MemAlloc(method->resMaxLen + 16u);
  if (NULL == resData) {
    ASLOG(SOMEIPE, ("OoM for client request\n"));
    ret = SOMEIP_E_NOMEM;
  } else {
    res.data = &resData[16];
    res.length = method->resMaxLen;
    ASLOG(SOMEIP, ("%x:%x:%x:%d request length = %u\n", msg->header.serviceId, msg->header.methodId,
                   msg->header.clientId, msg->header.sessionId, msg->req.length));
    ret = method->onRequest(requestId, &msg->req, &res);
    ASLOG(SOMEIP, ("%x:%x:%x:%d reply length = %u\n", msg->header.serviceId, msg->header.methodId,
                   msg->header.clientId, msg->header.sessionId, res.length));
    if (res.data != &resData[16]) {
      if (IS_TP_ENABLED(method) && (res.length > SOMEIP_SF_MAX)) {
        /* OK for TP case */
      } else {
        ASLOG(SOMEIPE, ("For short message, fill in response in res.data\n"));
        ret = E_NOT_OK;
      }
    }
    res.data = resData;
  }

  if (E_OK == ret) {
    if (IS_TP_ENABLED(method) && (res.length > SOMEIP_SF_MAX)) {
      Net_MemFree(resData);
      resData = NULL;
    }
  }

  if (E_OK == ret) {
    ret = SomeIp_ReplyRequest(config, conId, methodId, msg->header.clientId, msg->header.sessionId,
                              &msg->RemoteAddr, &res);
  } else if (SOMEIP_E_PENDING == ret) {
    /* response pending */
    SQP_ALLOC(AsyncReqMsg);
    if (NULL != var) {
      var->clientId = msg->header.clientId;
      var->sessionId = msg->header.sessionId;
      var->methodId = methodId;
      var->RemoteAddr = msg->RemoteAddr;
      SQP_CAPPEND(AsyncReqMsg);
    } else {
      ret = SOMEIP_E_NOMEM;
      ASLOG(SOMEIPE, ("OoM for cache request\n"));
    }
  } else {
    /* do nothing */
  }

  if (NULL != resData) {
    Net_MemFree(resData);
  }

  return ret;
}

static Std_ReturnType SomeIp_HandleServerMessage_Request(const SomeIp_ServerServiceType *config,
                                                         uint16_t conId, SomeIp_MsgType *msg) {
  Std_ReturnType ret = SOMEIP_E_UNKNOWN_METHOD;
  uint16_t methodId;
  const SomeIp_ServerConnectionType *connection = &config->connections[conId];
  SomeIp_ServerConnectionContextType *context = connection->context;
  const SomeIp_ServerMethodType *method = NULL;
  uint32_t requestId;

  for (methodId = 0u; methodId < config->numOfMethods; methodId++) {
    if (config->methods[methodId].methodId == msg->header.methodId) {
      if (((0xFFu == config->methods[methodId].interfaceVersion) ||
           (config->methods[methodId].interfaceVersion == msg->header.interfaceVersion))) {
        method = &config->methods[methodId];
        ret = E_OK;
        break;
      } else {
        ret = SOMEIP_E_WRONG_INTERFACE_VERSION;
      }
    }
  }

  if (E_OK == ret) {
    if (TRUE == msg->header.isTpFlag) {
      ret = SomeIp_ProcessRxTpMsg(conId, &context->pendingRxTpMsgs, methodId,
                                  method->onTpCopyRxData, msg);
    }
  }

  if (E_OK == ret) {
    requestId = ((uint32_t)msg->header.clientId << 16) + msg->header.sessionId;
    if (SOMEIP_MSG_REQUEST == msg->header.messageType) {
      ret = SomeIp_ProcessRequest(config, conId, methodId, msg);
    } else {
      ret = method->onFireForgot(requestId, &msg->req);
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
  SomeIp_ClientServiceContextType *context = config->context;
  SomeIp_WaitResMsgType *var;

  EnterCritical();
  if ((FALSE == msg->header.isTpFlag) || (0u == msg->tpHeader.offset)) {
    STAILQ_FOREACH(var, &context->pendingWaitResMsgs, entry) {
      if ((var->methodId == methodId) && (var->sessionId == msg->header.sessionId)) {
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
  SomeIp_ClientServiceContextType *context = config->context;
  uint16_t methodId;
  uint32_t requestId;

  for (methodId = 0u; methodId < config->numOfMethods; methodId++) {
    if (config->methods[methodId].methodId == msg->header.methodId) {
      if (((0xFFu == config->methods[methodId].interfaceVersion) ||
           (config->methods[methodId].interfaceVersion == msg->header.interfaceVersion))) {
        method = &config->methods[methodId];
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
      ASLOG(SOMEIPE, ("response %x:%x:%d not as expected\n", config->serviceId, method->methodId,
                      msg->header.sessionId));
    }
  }

  if (E_OK == ret) {
    if (TRUE == msg->header.isTpFlag) {
      ret =
        SomeIp_ProcessRxTpMsg(0u, &context->pendingRxTpMsgs, methodId, method->onTpCopyRxData, msg);
    }
  }

  if (E_OK == ret) {
    requestId = ((uint32_t)msg->header.clientId << 16) + msg->header.sessionId;
    if (E_OK == msg->header.returnCode) {
      ret = method->onResponse(requestId, &msg->req);
    } else {
      ret = method->onError(requestId, msg->header.returnCode);
      ASLOG(SOMEIPE, ("response %x:%x:%d with error %u\n", config->serviceId, method->methodId,
                      msg->header.sessionId, msg->header.returnCode));
    }
  }

  return ret;
}

static Std_ReturnType SomeIp_HandleServerMessage(const SomeIp_ServerServiceType *config,
                                                 uint16_t conId, SomeIp_MsgType *msg) {
  Std_ReturnType ret = E_OK;

  if (conId < config->numOfConnections) {
    if (config->serviceId != msg->header.serviceId) {
      ASLOG(SOMEIPE, ("unknown service\n"));
      ret = SOMEIP_E_UNKNOWN_SERVICE;
    }
  } else {
    ASLOG(SOMEIPE, ("invalid connection ID\n"));
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    switch (msg->header.messageType) {
    case SOMEIP_MSG_REQUEST:
    case SOMEIP_MSG_REQUEST_NO_RETURN:
      ret = SomeIp_HandleServerMessage_Request(config, conId, msg);
      break;
    case SOMEIP_MSG_NOTIFICATION:
      /* slient as event multicast */
      break;
    default:
      ASLOG(SOMEIPE, ("server: unsupported message type 0x%x\n", msg->header.messageType));
      ret = SOMEIP_E_WRONG_MESSAGE_TYPE;
      break;
    }
  }

  if (SOMEIP_E_PENDING == ret) {
    /* ok silent */
  } else if (SOMEIP_E_WRONG_MESSAGE_TYPE == ret) {
    /* ignore it */
  } else if (E_OK != ret) {
    (void)SomeIp_ReplyError(config->connections[conId].TxPduId, msg, ret);
  } else {
    /* ok, pass */
  }

  return ret;
}

static Std_ReturnType
SomeIp_HandleClientMessage_Notification(const SomeIp_ClientServiceType *config,
                                        SomeIp_MsgType *msg) {
  Std_ReturnType ret = E_NOT_OK;
  SomeIp_ClientServiceContextType *context = config->context;
  const SomeIp_ClientEventType *event = NULL;
  uint16_t eventId;
  uint32_t requestId;

  for (eventId = 0u; eventId < config->numOfEvents; eventId++) {
    if (config->events[eventId].eventId == msg->header.methodId) {
      if (((0xFFu == config->events[eventId].interfaceVersion) ||
           (config->events[eventId].interfaceVersion == msg->header.interfaceVersion))) {
        event = &config->events[eventId];
        ret = E_OK;
        break;
      } else {
        ret = SOMEIP_E_WRONG_INTERFACE_VERSION;
      }
    }
  }

  if (E_OK == ret) {
    if (TRUE == msg->header.isTpFlag) {
      ret = SomeIp_ProcessRxTpMsg(0u, &context->pendingRxTpEvtMsgs, eventId, event->onTpCopyRxData,
                                  msg);
    }
  }

  if (E_OK == ret) {
    requestId = ((uint32_t)msg->header.clientId << 16) + msg->header.sessionId;
    ret = event->onNotification(requestId, &msg->req);
  } else {
    ret = SOMEIP_E_UNKNOWN_METHOD;
  }

  return ret;
}

static Std_ReturnType SomeIp_HandleClientMessage(const SomeIp_ClientServiceType *config,
                                                 SomeIp_MsgType *msg) {
  Std_ReturnType ret = E_OK;

  if (config->serviceId != msg->header.serviceId) {
    ASLOG(SOMEIPE, ("unknown service\n"));
    ret = SOMEIP_E_UNKNOWN_SERVICE;
  }

  if (E_OK == ret) {
    switch (msg->header.messageType) {
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
      ASLOG(SOMEIPE, ("client: unsupported message type 0x%x\n", msg->header.messageType));
      ret = SOMEIP_E_WRONG_MESSAGE_TYPE;
      break;
    }
  }

  return ret;
}

static void SomeIp_MainServerAsyncRequest(const SomeIp_ServerServiceType *config, uint16_t conId) {
  const SomeIp_ServerConnectionType *connection = &config->connections[conId];
  SomeIp_ServerConnectionContextType *context = connection->context;
  DEC_SQP(AsyncReqMsg);
  const SomeIp_ServerMethodType *method = NULL;
  uint8_t *resData;
  SomeIp_MessageType res;
  Std_ReturnType ret;

  SQP_WHILE(AsyncReqMsg) {
    method = &config->methods[var->methodId];
    resData = (uint8_t *)Net_MemAlloc(method->resMaxLen + 16u);
    if (NULL != resData) {
      res.data = &resData[16];
      res.length = method->resMaxLen;
      ret = method->onAsyncRequest(conId, &res);
      if (res.data != &resData[16]) {
        if (IS_TP_ENABLED(method) && (res.length > SOMEIP_SF_MAX)) {
          /* OK for TP case */
        } else {
          ASLOG(SOMEIPE, ("For async short message, fill in response in res.data\n"));
          ret = E_NOT_OK;
        }
      }
      res.data = resData;
      if (E_OK == ret) {
        ret = SomeIp_ReplyRequest(config, conId, var->methodId, var->clientId, var->sessionId,
                                  &var->RemoteAddr, &res);
        if (E_OK != ret) {
          (void)SomeIp_TransError(connection->TxPduId, &var->RemoteAddr, config->serviceId,
                                  method->methodId, var->clientId, var->sessionId,
                                  method->interfaceVersion, SOMEIP_MSG_ERROR, ret);
        }

        SQP_CRM_AND_FREE(AsyncReqMsg);
      } else if (SOMEIP_E_PENDING == ret) {
        /* do nothing */
      } else {
        (void)SomeIp_TransError(connection->TxPduId, &var->RemoteAddr, config->serviceId,
                                method->methodId, var->clientId, var->sessionId,
                                method->interfaceVersion, SOMEIP_MSG_RESPONSE, ret);

        SQP_CRM_AND_FREE(AsyncReqMsg);
      }
      Net_MemFree(resData);
    } else {
      ASLOG(SOMEIPE, ("OoM for async request\n"));
    }
  }
  SQP_WHILE_END()
}

static void SomeIp_MainServerRxTpMsg(const SomeIp_ServerServiceType *config, uint16_t conId) {
  const SomeIp_ServerConnectionType *connection = &config->connections[conId];
  SomeIp_ServerConnectionContextType *context = connection->context;
  DEC_SQP(RxTpMsg);
  const SomeIp_ServerMethodType *method = NULL;
  Std_ReturnType ret;

  SQP_WHILE(RxTpMsg) {
    method = &config->methods[var->methodId];

    if (var->timer > 0u) {
      var->timer--;
      if (0 == var->timer) {
        ASLOG(SOMEIPE,
              ("server method %x:%x:%x:%d Rx Tp msg timeout, offset %d\n", config->serviceId,
               method->methodId, var->clientId, var->sessionId, var->offset));
        ret = SomeIp_TransError(connection->TxPduId, &var->RemoteAddr, config->serviceId,
                                method->methodId, var->clientId, var->sessionId,
                                method->interfaceVersion, SOMEIP_MSG_ERROR, SOMEIP_E_TIMEOUT);
        if (E_NOT_OK == ret) {
          var->timer = 1; /* retry next time */
        } else {
          method->onTpCopyRxData(((uint32_t)var->clientId << 16) + var->sessionId, NULL);
          SQP_CRM_AND_FREE(RxTpMsg);
        }
      }
    }
  }
  SQP_WHILE_END()
}

static void SomeIp_MainServerTxTpMsg(const SomeIp_ServerServiceType *config, uint16_t conId) {
  const SomeIp_ServerConnectionType *connection = &config->connections[conId];
  SomeIp_ServerConnectionContextType *context = connection->context;
  DEC_SQP(TxTpMsg);
  const SomeIp_ServerMethodType *method = NULL;
  Std_ReturnType ret;

  SQP_WHILE(TxTpMsg) {
    method = &config->methods[var->methodId];
    if (var->timer > 0u) {
      var->timer--;
    }
    if (0 == var->timer) {
      ret = SomeIp_SendNextTxTpMsg(connection->TxPduId, config->serviceId, method->methodId,
                                   method->interfaceVersion, SOMEIP_MSG_RESPONSE,
                                   method->onTpCopyTxData, var);
      if (E_OK == ret) {
        if (var->offset >= var->length) {
          SQP_CRM_AND_FREE(TxTpMsg);
        } else {
          var->timer = config->SeparationTime;
        }
      } else { /* abort this tx */
        (void)method->onTpCopyTxData(((uint32_t)var->clientId << 16) + var->sessionId, NULL);
        SQP_CRM_AND_FREE(TxTpMsg);
      }
    }
  }
  SQP_WHILE_END()
}

static void SomeIp_MainServerTxTpEvtMsg(const SomeIp_ServerServiceType *config) {
  SomeIp_ServerContextType *context = config->context;
  DEC_SQP(TxTpEvtMsg);
  const SomeIp_ServerEventType *event = NULL;
  Sd_EventHandlerSubscriberListType *list;
  Std_ReturnType ret;

  SQP_WHILE(TxTpEvtMsg) {
    event = &config->events[SOMEIP_CONFIG->TxEvent2PerServiceMap[var->eventId]];
    if (var->timer > 0u) {
      var->timer--;
    }
    if (0 == var->timer) {
      /* NOTE: may result partial data send to later online subscribers */
      ret = Sd_GetSubscribers(event->sdHandleID, &list);
      if (E_OK == ret) {
        ret = SomeIp_SendNextTxTpEvtMsg(config, event, var, list);
        if (E_OK == ret) {
          if (var->offset >= var->length) {
            SQP_CRM_AND_FREE(TxTpEvtMsg);
          } else {
            var->timer = config->SeparationTime;
          }
        } else { /* abort this tx */
          (void)event->onTpCopyTxData(((uint32_t)var->eventId << 16) + var->sessionId, NULL);
          SQP_CRM_AND_FREE(TxTpEvtMsg);
        }
      }
    }
  }
  SQP_WHILE_END()
}

static void SomeIp_MainServer(const SomeIp_ServerServiceType *config) {
  uint16_t conId;

  for (conId = 0u; conId < config->numOfConnections; conId++) {
    SomeIp_MainServerAsyncRequest(config, conId);
    SomeIp_MainServerTxTpMsg(config, conId);
    SomeIp_MainServerRxTpMsg(config, conId);
  }

  SomeIp_MainServerTxTpEvtMsg(config);
}

static void SomeIp_MainClientRxTpMsg(const SomeIp_ClientServiceType *config) {
  SomeIp_ClientServiceContextType *context = config->context;
  DEC_SQP(RxTpMsg);
  const SomeIp_ClientMethodType *method = NULL;
  uint32_t requestId;

  SQP_WHILE(RxTpMsg) {
    method = &config->methods[var->methodId];

    if (var->timer > 0u) {
      var->timer--;
      if (0 == var->timer) {
        requestId = ((uint32_t)var->clientId << 16) + var->sessionId;
        ASLOG(SOMEIPE,
              ("client method %x:%x:%x:%d Rx Tp msg timeout, offset %d\n", config->serviceId,
               method->methodId, var->clientId, var->sessionId, var->offset));
        method->onError(requestId, SOMEIP_E_TIMEOUT);
        SQP_CRM_AND_FREE(RxTpMsg);
      }
    }
  }
  SQP_WHILE_END()
}

static void SomeIp_MainClientRxTpEvtMsg(const SomeIp_ClientServiceType *config) {
  SomeIp_ClientServiceContextType *context = config->context;
  DEC_SQP(RxTpEvtMsg);

  SQP_WHILE(RxTpEvtMsg) {
    if (var->timer > 0u) {
      var->timer--;
      if (0 == var->timer) {
        ASLOG(SOMEIPE,
              ("client event %x:%x:%x:%d Rx Tp msg timeout, offset %d\n", config->serviceId,
               config->events[var->methodId].eventId, var->clientId, var->sessionId, var->offset));
        SQP_CRM_AND_FREE(RxTpEvtMsg);
      }
    }
  }
  SQP_WHILE_END()
}

static void SomeIp_MainClientTxTpMsg(const SomeIp_ClientServiceType *config) {
  SomeIp_ClientServiceContextType *context = config->context;
  DEC_SQP(TxTpMsg);
  const SomeIp_ClientMethodType *method = NULL;
  Std_ReturnType ret;

  SQP_WHILE(TxTpMsg) {
    method = &config->methods[var->methodId];
    if (var->timer > 0u) {
      var->timer--;
    }
    if (0u == var->timer) {
      ret = SomeIp_SendNextTxTpMsg(config->TxPduId, config->serviceId, method->methodId,
                                   method->interfaceVersion, SOMEIP_MSG_REQUEST,
                                   method->onTpCopyTxData, var);
      if (E_OK == ret) {
        if (var->offset >= var->length) {
          SQP_CRM_AND_FREE(TxTpMsg);
          (void)SomeIp_WaitResponse(config, var->methodId, var->sessionId);
        }
      } else { /* abort this tx */
        SQP_CRM_AND_FREE(TxTpMsg);
      }
    }
  }
  SQP_WHILE_END()
}

static void SomeIp_MainClientWaitResMsg(const SomeIp_ClientServiceType *config) {
  SomeIp_ClientServiceContextType *context = config->context;
  DEC_SQP(WaitResMsg);
  const SomeIp_ClientMethodType *method = NULL;
  uint32_t requestId;

  SQP_WHILE(WaitResMsg) {
    method = &config->methods[var->methodId];
    if (var->timer > 0u) {
      var->timer--;
    }
    if (0u == var->timer) {
      requestId = ((uint32_t)var->methodId << 16) + var->sessionId;
      ASLOG(SOMEIPE, ("client method %x:%x:%u wait response timeout\n", config->serviceId,
                      method->methodId, var->sessionId));
      method->onError(requestId, SOMEIP_E_TIMEOUT);
      SQP_CRM_AND_FREE(WaitResMsg);
    }
  }
  SQP_WHILE_END()
}

static void SomeIp_MainClient(const SomeIp_ClientServiceType *config) {
  SomeIp_MainClientTxTpMsg(config);
  SomeIp_MainClientRxTpMsg(config);
  SomeIp_MainClientRxTpEvtMsg(config);
  SomeIp_MainClientWaitResMsg(config);
}

static void SomeIp_ServerServiceModeChg(const SomeIp_ServerServiceType *service, uint16_t conId,
                                        SoAd_SoConModeType Mode) {
  SomeIp_ServerConnectionContextType *context = service->connections[conId].context;
  uint16_t i;
  if (SOAD_SOCON_OFFLINE == Mode) {
    SQP_CLEAR(AsyncReqMsg);
    SQP_CLEAR(RxTpMsg);
    SQP_CLEAR(TxTpMsg);
    SQP_CLEAR(TxTpEvtMsg);
    context->online = FALSE;
    service->onConnect(conId, FALSE);
    for (i = 0u; i < service->numOfEvents; i++) {
      Sd_RemoveSubscriber(service->events[i].sdHandleID, service->connections[conId].TxPduId);
    }
  } else {
    context->online = TRUE;
    service->onConnect(conId, TRUE);
  }
}

static void SomeIp_ClientServiceModeChg(const SomeIp_ClientServiceType *service,
                                        SoAd_SoConModeType Mode) {
  SomeIp_ClientServiceContextType *context = service->context;

  if (SOAD_SOCON_OFFLINE == Mode) {
    SQP_CLEAR(RxTpEvtMsg);
    SQP_CLEAR(RxTpMsg);
    SQP_CLEAR(TxTpMsg);
    context->online = FALSE;
    service->onAvailability(FALSE);
  } else {
    context->online = TRUE;
    service->onAvailability(TRUE);
  }
}
static Std_ReturnType SomeIp_HandleRxMsg(PduIdType RxPduId, SomeIp_MsgType *msg) {
  Std_ReturnType ret;
  uint16_t index;
  ASLOG(SOMEIP, ("[%d] service 0x%x:0x%x:%d message type 0x%x return code %d payload %d bytes "
                 "from client 0x%x:%d %d.%d.%d.%d:%d\n",
                 RxPduId, msg->header.serviceId, msg->header.methodId, msg->header.interfaceVersion,
                 msg->header.messageType, msg->header.returnCode, msg->header.length - 8,
                 msg->header.clientId, msg->header.sessionId, msg->RemoteAddr.addr[0],
                 msg->RemoteAddr.addr[1], msg->RemoteAddr.addr[2], msg->RemoteAddr.addr[3],
                 msg->RemoteAddr.port));
  PCAP_TRACE(msg->header.serviceId, msg->header.methodId, msg->header.interfaceVersion,
             msg->header.messageType, msg->header.returnCode, msg->req.data, msg->req.length,
             msg->header.clientId, msg->header.sessionId, &msg->RemoteAddr, msg->header.isTpFlag,
             msg->tpHeader.offset, msg->tpHeader.moreSegmentsFlag, TRUE);
  if (RxPduId < SOMEIP_CONFIG->numOfPIDs) {
    index = SOMEIP_CONFIG->PID2ServiceMap[RxPduId];
    if (index < SOMEIP_CONFIG->numOfService) {
      if (TRUE == SOMEIP_CONFIG->services[index].isServer) {
        ret = SomeIp_HandleServerMessage(
          (const SomeIp_ServerServiceType *)SOMEIP_CONFIG->services[index].service,
          SOMEIP_CONFIG->PID2ServiceConnectionMap[RxPduId], msg);
      } else {
        ret = SomeIp_HandleClientMessage(
          (const SomeIp_ClientServiceType *)SOMEIP_CONFIG->services[index].service, msg);
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
void SomeIp_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  SomeIp_MsgType msg;
  Std_ReturnType ret;

  ret = SomeIp_DecodeMsg(PduInfoPtr, &msg);
  if (E_OK == ret) {
    (void)SomeIp_HandleRxMsg(RxPduId, &msg);
  } else {
    ASLOG(SOMEIPE, ("IF message malformed\n"));
  }
}

#ifdef SOMEIP_USE_TP_BUF
BufReq_ReturnType SomeIp_SoAdTpCopyRxData(PduIdType RxPduId, const PduInfoType *PduInfoPtr,
                                          PduLengthType *bufferSizePtr) {
  BufReq_ReturnType bret = BUFREQ_E_NOT_OK;
  Std_ReturnType ret = E_NOT_OK;
  SomeIp_MsgType msg;
  const SomeIp_ClientServiceType *cs;
  const SomeIp_ServerServiceType *ss;
  SomeIp_TpBufferType *tpBuf = NULL;
  PduInfoType PduInfo;
  uint16_t index;
  uint32_t leftLen;

  if (RxPduId < SOMEIP_CONFIG->numOfPIDs) {
    index = SOMEIP_CONFIG->PID2ServiceMap[RxPduId];
    if (index < SOMEIP_CONFIG->numOfService) {
      if (TRUE == SOMEIP_CONFIG->services[index].isServer) {
        ss = (const SomeIp_ServerServiceType *)SOMEIP_CONFIG->services[index].service;
        index = SOMEIP_CONFIG->PID2ServiceConnectionMap[RxPduId];
        tpBuf = ss->connections[index].tpBuf;
      } else {
        cs = (const SomeIp_ClientServiceType *)SOMEIP_CONFIG->services[index].service;
        tpBuf = cs->tpBuf;
      }
    }
  }

  *bufferSizePtr = 0u;
  if (NULL != tpBuf) {
    PduInfo = *PduInfoPtr;
    ASLOG(SOMEIP, ("Tp input(%d)\n", PduInfo.SduLength));
    while (PduInfo.SduLength > 0u) {
      if (NULL == tpBuf->data) {
        ret = SomeIp_HandleTpHeader(tpBuf, &PduInfo, &msg);
        if (E_OK == ret) {
          ASLOG(SOMEIP, ("consume all\n"));
          (void)SomeIp_HandleRxMsg(RxPduId, &msg);
          *bufferSizePtr += msg.header.length + 8u;
          (void)memset(tpBuf, 0u, sizeof(SomeIp_TpBufferType));
          PduInfo.SduLength = 0u;
          bret = BUFREQ_OK;
        } else if (SOMEIP_E_PENDING == ret) {
          /* do nothing as header not full */
          ASLOG(SOMEIP, ("not enough header, wait\n"));
        } else if (SOMEIP_E_MSG_TOO_SHORT == ret) {
          tpBuf->data = (uint8_t *)Net_MemAlloc(msg.header.length - 8u);
          if (NULL != tpBuf->data) {
            tpBuf->length = msg.header.length - 8u;
            tpBuf->offset = PduInfo.SduLength;
            (void)memcpy(tpBuf->data, PduInfo.SduDataPtr, PduInfo.SduLength);
            *bufferSizePtr += msg.header.length + 8u;
            PduInfo.SduLength = 0u;
            bret = BUFREQ_OK;
            ASLOG(SOMEIP, ("too short, offset %d, length %d\n", tpBuf->offset, tpBuf->length));
          } else {
            (void)memset(tpBuf, 0u, sizeof(SomeIp_TpBufferType));
            ASLOG(SOMEIPE, ("OoM for tp buffer, abort\n"));
          }
        } else if (SOMEIP_E_MSG_TOO_LARGE == ret) {
          leftLen = PduInfo.SduLength - (msg.header.length - 8u);
          PduInfo.SduLength = msg.header.length - 8u;
          ASLOG(SOMEIP, ("too large, consume %d, left %d\n", PduInfo.SduLength, leftLen));
          ret = SomeIp_DecodeMsgTp(tpBuf->header, PduInfo.SduDataPtr, PduInfo.SduLength,
                                   (const TcpIp_SockAddrType *)PduInfo.MetaDataPtr, &msg);
          if (E_OK == ret) {
            (void)SomeIp_HandleRxMsg(RxPduId, &msg);
          } else {
            ASLOG(SOMEIPE, ("Fatal Protol error\n"));
          }
          (void)memset(tpBuf, 0u, sizeof(SomeIp_TpBufferType));
          PduInfo.SduDataPtr = &PduInfo.SduDataPtr[PduInfo.SduLength];
          PduInfo.SduLength = leftLen;
        } else {
          (void)memset(tpBuf, 0u, sizeof(SomeIp_TpBufferType));
          ASLOG(SOMEIPE, ("TP message malformed, abort\n"));
        }
      } else {
        leftLen = tpBuf->length - tpBuf->offset;
        if (leftLen >= PduInfo.SduLength) {
          leftLen = PduInfo.SduLength;
        }
        (void)memcpy(&tpBuf->data[tpBuf->offset], PduInfo.SduDataPtr, leftLen);
        PduInfo.SduDataPtr = &PduInfo.SduDataPtr[leftLen];
        PduInfo.SduLength -= leftLen;
        tpBuf->offset += leftLen;
        if (tpBuf->offset >= tpBuf->length) {
          ret = SomeIp_DecodeMsgTp(tpBuf->header, tpBuf->data, tpBuf->length,
                                   (const TcpIp_SockAddrType *)PduInfo.MetaDataPtr, &msg);
          if (E_OK == ret) {
            (void)SomeIp_HandleRxMsg(RxPduId, &msg);
          } else {
            ASLOG(SOMEIPE, ("Fatal Protol error\n"));
          }
          Net_MemFree(tpBuf->data);
          (void)memset(tpBuf, 0u, sizeof(SomeIp_TpBufferType));
          *bufferSizePtr += tpBuf->length + 16u;
          bret = BUFREQ_OK;
        }
      }
    }
  } else {
    ASLOG(SOMEIPE, ("Invalid TP RxPduId = %d\n", RxPduId));
  }

  return bret;
}
#endif

void SomeIp_SoConModeChg(SoAd_SoConIdType SoConId, SoAd_SoConModeType Mode) {
  Std_ReturnType ret = E_NOT_OK;
  const SomeIp_ServiceType *service = NULL;
  const SomeIp_ServerServiceType *ss = NULL;
  SomeIp_ServerContextType *context;
  uint16_t i;
  uint16_t j;
  uint16_t conId = 0u;

  ASLOG(SOMEIP, ("SoConId %d Mode %d\n", SoConId, Mode));
  for (i = 0u; (i < SOMEIP_CONFIG->numOfService) && (E_NOT_OK == ret); i++) {
    service = &SOMEIP_CONFIG->services[i];
    if (service->SoConId == SoConId) {
      ret = E_OK;
    } else if (TRUE == service->isServer) {
      ss = (const SomeIp_ServerServiceType *)service->service;
      for (j = 0u; (j < ss->numOfConnections) && (E_NOT_OK == ret); j++) {
        if (ss->connections[j].SoConId == SoConId) {
          conId = j;
          ret = E_OK;
        }
      }
    } else {
      /* not match */
    }
  }

  if (E_OK == ret) {
    if (TRUE == service->isServer) {
      if (service->SoConId == SoConId) {
        ss = (const SomeIp_ServerServiceType *)service->service;
        context = ss->context;
        if (SOAD_SOCON_OFFLINE == Mode) {
          SQP_CLEAR(TxTpEvtMsg);
          context->online = FALSE;
        } else {
          context->online = TRUE;
        }
        if (TCPIP_IPPROTO_TCP == ss->protocol) {
          if (SOAD_SOCON_OFFLINE == Mode) {
            for (j = 0u; j < ss->numOfConnections; j++) {
              (void)SoAd_CloseSoCon(ss->connections[j].SoConId, TRUE);
            }
          }
        } else {
          SomeIp_ServerServiceModeChg(ss, 0u, Mode);
        }
      } else {
        SomeIp_ServerServiceModeChg((const SomeIp_ServerServiceType *)service->service, conId,
                                    Mode);
      }
    } else {
      SomeIp_ClientServiceModeChg((const SomeIp_ClientServiceType *)service->service, Mode);
    }
  } else {
    ASLOG(SOMEIPE, ("SoConId %d unknown\n", SoConId));
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

  if (TxEventId < SOMEIP_CONFIG->numOfTxEvents) {
    index = SOMEIP_CONFIG->TxEvent2ServiceMap[TxEventId];
    config = (const SomeIp_ServerServiceType *)SOMEIP_CONFIG->services[index].service;
    if (FALSE == config->context->online) {
      ret = E_NOT_OK;
    }
  } else {
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    index = SOMEIP_CONFIG->TxEvent2PerServiceMap[TxEventId];
    event = &config->events[index];
    ret = Sd_GetSubscribers(event->sdHandleID, &list);
  }

  if (E_OK == ret) {
    msg.data = data;
    msg.length = length;
  }

  if (E_OK == ret) {
    ret = SomeIp_SendNotification(config, TxEventId, sessionId, &msg, list);
  }

  return ret;
}

Std_ReturnType SomeIp_ResolveSubscriber(uint16_t ServiceId, Sd_EventHandlerSubscriberType *sub) {
  Std_ReturnType ret = E_NOT_OK;
  Std_ReturnType ret2;
  const SomeIp_ServerServiceType *service = NULL;
  const SomeIp_ServerConnectionType *connection = NULL;
  TcpIp_SockAddrType RemoteAddr;
  uint16_t i;
  int result;

  if (ServiceId < SOMEIP_CONFIG->numOfService) {
    if (TRUE == SOMEIP_CONFIG->services[ServiceId].isServer) {
      service = (const SomeIp_ServerServiceType *)SOMEIP_CONFIG->services[ServiceId].service;
      if (service->context->online) {
        if (TCPIP_IPPROTO_UDP == service->protocol) {
          connection = &service->connections[0];
          sub->TxPduId = connection->TxPduId;
          ret = E_OK;
        } else {
          for (i = 0u; i < service->numOfConnections; i++) {
            connection = &service->connections[i];
            if (connection->context->online) {
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
      }
    }
  }

  return ret;
}

void SomeIp_Init(const SomeIp_ConfigType *ConfigPtr) {
  uint16_t i;
  if (NULL != ConfigPtr) {
    someIpConfigPtr = ConfigPtr;
  } else {
    someIpConfigPtr = &SomeIp_Config;
  }
  SQP_INIT(AsyncReqMsg);
  SQP_INIT(TxTpMsg);
  SQP_INIT(RxTpMsg);
  SQP_INIT(TxTpEvtMsg);
  SQP_INIT(WaitResMsg);
  for (i = 0u; i < SOMEIP_CONFIG->numOfService; i++) {
    if (TRUE == SOMEIP_CONFIG->services[i].isServer) {
      SomeIp_InitServer((const SomeIp_ServerServiceType *)SOMEIP_CONFIG->services[i].service);
    } else {
      SomeIp_InitClient((const SomeIp_ClientServiceType *)SOMEIP_CONFIG->services[i].service);
    }
  }
}

void SomeIp_MainFunction(void) {
  uint16_t i;
  for (i = 0u; i < SOMEIP_CONFIG->numOfService; i++) {
    if (TRUE == SOMEIP_CONFIG->services[i].isServer) {
      SomeIp_MainServer((const SomeIp_ServerServiceType *)SOMEIP_CONFIG->services[i].service);
    } else {
      SomeIp_MainClient((const SomeIp_ClientServiceType *)SOMEIP_CONFIG->services[i].service);
    }
  }
}

#ifdef SOMEIP_USE_TP_BUF
Std_ReturnType SomeIp_ConnectionTakeControl(uint16_t serviceId, uint16_t conId) {
  Std_ReturnType ret = E_NOT_OK;
  const SomeIp_ServerServiceType *config;
  const SomeIp_ServerConnectionType *connection;
  SomeIp_ServerConnectionContextType *context;

  if (serviceId < SOMEIP_CONFIG->numOfService) {
    if (TRUE == SOMEIP_CONFIG->services[ServiceId].isServer) {
      config = SOMEIP_CONFIG->services[serviceId].service;
      connection = &config->connections[conId];
      context = connection->context;
      ret = SoAd_TakeControl(connection->SoConId);
      if (E_OK == ret) {
        ret = SoAd_SetTimeout(connection->SoConId, 10u);
        context->takenControled = TRUE;
      }
    }
  }

  return ret;
}

Std_ReturnType SomeIp_ConnectionRxControl(uint16_t serviceId, uint16_t conId, uint8_t *data,
                                          uint32_t *length) {
  Std_ReturnType ret = E_NOT_OK;
  const SomeIp_ServerServiceType *config;
  const SomeIp_ServerConnectionType *connection;
  SomeIp_ServerConnectionContextType *context;
  PduInfoType PduInfo;
  PduLengthType bufferSize;
  TcpIp_SockAddrType RemoteAddr;

  if (serviceId < SOMEIP_CONFIG->numOfService) {
    if (TRUE == SOMEIP_CONFIG->services[ServiceId].isServer) {
      config = SOMEIP_CONFIG->services[serviceId].service;
      connection = &config->connections[conId];
      context = connection->context;
      if (TRUE == context->takenControled) {
        ret = SoAd_ControlRecv(connection->SoConId, data, length);
        if ((E_OK == ret) && (*length > 0u)) {
          (void)SoAd_GetRemoteAddr(connection->SoConId, &RemoteAddr);
          PduInfo.SduDataPtr = data;
          PduInfo.SduLength = *length;
          PduInfo.MetaDataPtr = (uint8_t *)&RemoteAddr;
          if (TCPIP_IPPROTO_UDP == config->protocol) {
            SomeIp_RxIndication(connection->RxPduId, &PduInfo);
          } else {
            (void)SomeIp_SoAdTpCopyRxData(connection->RxPduId, &PduInfo, &bufferSize);
          }
        }
      }
    }
  }

  return ret;
}
#endif /* SOMEIP_USE_TP_BUF */

Std_ReturnType SomeIp_HeaderIndication(PduIdType RxPduId, const PduInfoType *info,
                                       uint32_t *payloadLength) {
  Std_ReturnType ret = E_OK;
  uint8_t *data = info->SduDataPtr;
  (void)RxPduId;

  if (info->SduLength < 8u) {
    ASLOG(SDE, ("malformed SOMEIP message\n"));
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    *payloadLength =
      ((uint32_t)data[4] << 24) + ((uint32_t)data[5] << 16) + ((uint32_t)data[6] << 8u) + data[7];
  }

  return ret;
}
