/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Module XCP AUTOSAR CP R23-11
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Xcp.h"
#include "Xcp_Cfg.h"
#include "Xcp_Priv.h"
#include "Std_Debug.h"
#include "Std_Critical.h"
#include "CanIf.h"
#include "mempool.h"
#include <string.h>

#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_XCP 0
#define AS_LOG_XCPI 0
#define AS_LOG_XCPW 2
#define AS_LOG_XCPE 3

/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern CONSTANT(Xcp_ConfigType, XCP_CONST) Xcp_Config;
/* ================================ [ DATAS     ] ============================================== */
static Xcp_PacketType xcpPacketSlots[XCP_PACKET_POOL_SIZE];
static mempool_t xcpPacketPool;

static Xcp_ContextType Xcp_Context;
/* ================================ [ LOCALS    ] ============================================== */
static P2CONST(Xcp_ServiceType, AUTOMATIC, XCP_CONST) Xcp_FindService(uint8_t Pid) {
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  P2CONST(Xcp_ServiceType, AUTOMATIC, XCP_CONST) service = NULL;
  uint16_t l;
  uint16_t h;
  uint16_t m;

  l = 0;
  h = config->numOfServices - 1u;
  if (Pid < config->services[0].Pid) {
    l = h + 1u; /* avoid the underflow of "m - 1" */
  }

  while (l <= h) {
    m = l + ((h - l) >> 1);
    if (config->services[m].Pid > Pid) {
      h = m - 1u;
    } else if (config->services[m].Pid < Pid) {
      l = m + 1u;
    } else {
      service = &config->services[m];
      break;
    }
  }

  return service;
}

static void Xcp_TxError(uint8_t channel, Xcp_NegativeResponseCodeType nrc) {
  uint8_t data[2] = {XCP_PID_ERR, 0};
  PduInfoType PduInfo = {data, NULL, sizeof(data)};
  Xcp_ContextType *context = Xcp_GetContext();
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Std_ReturnType ret = E_NOT_OK;
  Xcp_PacketType *packet = NULL;

  data[1] = nrc;

  if (XCP_ON_CAN_CHL == channel) {
    ret = CanIf_Transmit(config->CanIfTxPduId, &PduInfo);
    if (E_OK != ret) {
      packet = Xcp_AllocPacket();
      if (NULL != packet) {
        (void)memcpy(packet->payload, PduInfo.SduDataPtr, PduInfo.SduLength);
        packet->length = (Xcp_MsgLenType)PduInfo.SduLength;
        ASLOG(XCP, ("[%d] alloc Tx Err packet %p\n", channel, packet));
        EnterCritical();
        STAILQ_INSERT_TAIL(&context->txPackets, packet, entry);
        ExitCritical();
      } else {
        ASLOG(XCPE, ("[%d] Tx Error Packet fail as no free packet\n", channel));
        ret = E_NOT_OK;
      }
    }
  } else {
    ASLOG(XCPE, ("[%d] only XCP on CAN supported for now\n", channel));
  }
}

static Std_ReturnType Xcp_TxResponse(uint8_t channel, uint8_t *data, Xcp_MsgLenType len) {
  PduInfoType PduInfo = {data, NULL, (PduLengthType)0};
  Xcp_ContextType *context = Xcp_GetContext();
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Std_ReturnType ret = E_NOT_OK;
  Xcp_PacketType *packet = NULL;

  data[0] = XCP_PID_RES;
  PduInfo.SduLength = len;
  if (XCP_ON_CAN_CHL == channel) {
    ret = CanIf_Transmit(config->CanIfTxPduId, &PduInfo);
    if (E_OK != ret) {
      packet = Xcp_AllocPacket();
      if (NULL != packet) {
        (void)memcpy(packet->payload, PduInfo.SduDataPtr, PduInfo.SduLength);
        packet->length = (Xcp_MsgLenType)PduInfo.SduLength;
        ASLOG(XCP, ("[%d] alloc Tx Res packet %p\n", channel, packet));
        EnterCritical();
        STAILQ_INSERT_TAIL(&context->txPackets, packet, entry);
        ExitCritical();
      } else {
        ASLOG(XCPE, ("[%d] Tx Res Packet fail as no free packet\n", channel));
        ret = E_NOT_OK;
      }
    }
  } else {
    ASLOG(XCPE, ("[%d] only XCP on CAN supported for now\n", channel));
  }

  return ret;
}

static void Xcp_MainFunction_Request(void) {
  Xcp_ContextType *context = Xcp_GetContext();
  Xcp_PacketType *packet = NULL;
  P2CONST(Xcp_ServiceType, AUTOMATIC, XCP_CONST) service = NULL;
  Std_ReturnType ret = E_OK;
  Xcp_NegativeResponseCodeType nrc = XCP_E_GENERIC;
  uint8_t resData[XCP_PATCKET_MAX_SIZE];
  uint8_t channel = context->activeChannel;

  packet = STAILQ_FIRST(&context->rxPackets);

  if (NULL != packet) {
    service = Xcp_FindService(packet->payload[0]);
    if (NULL == service) {
      nrc = XCP_E_CMD_UNKNOWN;
      ret = E_NOT_OK;
      ASLOG(XCPE, ("[%d] command %02X unknown\n", channel, packet->payload[0]));
    } else if (0u != (service->resource & context->lockedResource)) {
      nrc = XCP_E_ACCESS_LOCKED;
      ret = E_NOT_OK;
      ASLOG(XCPE, ("[%d] command %02X access locked\n", channel, packet->payload[0]));
    } else {
      if (NULL == context->curService) {
        ASLOG(XCPI, ("[%d] command %02X len %d\n", channel, packet->payload[0], packet->length));
        context->curService = service;
        context->msgContext.opStatus = XCP_INITIAL;
      } else if (service == context->curService) {
        context->msgContext.opStatus = XCP_PENDING;
      } else {
        ASLOG(XCPE, ("[%d] Fatal memory error, current service %02X, pending service %02X, abort\n",
                     channel, context->curService->Pid, service->Pid));
        context->curService = service;
        context->msgContext.opStatus = XCP_INITIAL;
      }
      context->msgContext.reqData = &packet->payload[1];
      context->msgContext.reqDataLen = packet->length - 1u;
      context->msgContext.resData = &resData[1];
      context->msgContext.resDataLen = 0;
      context->msgContext.resMaxDataLen = XCP_PATCKET_MAX_SIZE - 1u;
      ret = service->dspServiceFnc(&context->msgContext, &nrc);

      if (E_OK == ret) {
        ASLOG(XCP, ("[%d] command %02X DONE\n", channel, packet->payload[0]));
        (void)Xcp_TxResponse(channel, resData, context->msgContext.resDataLen + 1u);
        context->curService = NULL;
      } else if (XCP_E_PENDING == ret) {
        /* OK, schedule it next time */
        context->msgContext.opStatus = XCP_PENDING;
        if (0u != context->msgContext.resDataLen) { /* pending but with response */
          ret = Xcp_TxResponse(channel, resData, context->msgContext.resDataLen + 1u);
          if (E_OK == ret) {
            ret = XCP_E_PENDING;
          } else {
            ASLOG(XCPE, ("[%d] Abort pending service %02X\n", channel, packet->payload[0]));
          }
        }
      } else {
        /* Error sending nrc, or OK no RES */
        ASLOG(XCP, ("[%d] command %02X ret = %02X, nrc = %02X\n", channel, packet->payload[0], ret,
                    nrc));
        context->curService = NULL;
      }
    }
  } else {
    /* no request packet */
  }

  if (XCP_E_PENDING != ret) {
    if (NULL != packet) {
      ASLOG(XCP, ("[%d] free packet %p for service %02X\n", channel, packet, packet->payload[0]));
      EnterCritical();
      STAILQ_REMOVE_HEAD(&context->rxPackets, entry);
      ExitCritical();
      Xcp_FreePacket(packet);
    }

    if (XCP_E_OK_NO_RES == ret) {
      /* OK with no RES */
    } else if (E_OK != ret) {
      Xcp_TxError(channel, nrc);
    } else {
      /* OK case */
    }
  }
}

static void Xcp_MainFunction_Response(void) {
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Xcp_ContextType *context = Xcp_GetContext();
  Xcp_PacketType *packet = STAILQ_FIRST(&context->txPackets);
  PduInfoType PduInfo;
  Std_ReturnType ret;
  if (NULL != packet) {
    PduInfo.MetaDataPtr = NULL;
    PduInfo.SduDataPtr = packet->payload;
    PduInfo.SduLength = (PduLengthType)packet->length;
    /* Now only support XCP on CAN */
    ret = CanIf_Transmit(config->CanIfTxPduId, &PduInfo);
    if (E_OK == ret) {
      EnterCritical();
      STAILQ_REMOVE_HEAD(&context->txPackets, entry);
      ExitCritical();
      ASLOG(XCP, ("[%d] free Tx packet %p\n", 0, packet));
      Xcp_FreePacket(packet);
    }
  }
}

static void Xcp_MainFunction_Event(void) {
  Xcp_ContextType *context = Xcp_GetContext();
  (void)context;
#ifdef XCP_USE_SERVICE_PGM_PROGRAM_RESET
  if (context->timer2Reset > 0u) {
    context->timer2Reset--;
    if (0u == context->timer2Reset) {
      ASLOG(XCPI, ("Xcp Reset\n"));
      Xcp_PerformReset();
    }
  }
#endif
}
/* ================================ [ FUNCTIONS ] ============================================== */
uint32_t Xcp_GetU32(const uint8_t *data) {
  uint32_t u32 = 0xdeadbeefu;

  if (0xde == (*(uint8_t *)&u32)) { /* big endian */
    u32 = ((uint32_t)data[0] << 24) + ((uint32_t)data[1] << 16) + ((uint32_t)data[2] << 8) +
          ((uint32_t)data[3]);
  } else {
    u32 = ((uint32_t)data[3] << 24) + ((uint32_t)data[2] << 16) + ((uint32_t)data[1] << 8) +
          ((uint32_t)data[0]);
  }

  return u32;
}

void Xcp_SetU32(uint8_t *data, uint32_t u32) {
  uint32_t endianMask = 0xdeadbeefu;

  if (0xde == (*(uint8_t *)&endianMask)) { /* big endian */
    data[0] = (u32 >> 24) & 0xFFu;
    data[1] = (u32 >> 16) & 0xFFu;
    data[2] = (u32 >> 8) & 0xFFu;
    data[3] = u32 & 0xFFu;
  } else {
    data[3] = (u32 >> 24) & 0xFFu;
    data[2] = (u32 >> 16) & 0xFFu;
    data[1] = (u32 >> 8) & 0xFFu;
    data[0] = u32 & 0xFFu;
  }
}

uint16_t Xcp_GetU16(const uint8_t *data) {
  uint16_t u16 = 0xdeadu;

  if (0xde == (*(uint8_t *)&u16)) { /* big endian */
    u16 = ((uint16_t)data[0] << 8) + ((uint16_t)data[1]);
  } else {
    u16 = ((uint16_t)data[1] << 8) + ((uint16_t)data[0]);
  }

  return u16;
}

void Xcp_SetU16(uint8_t *data, uint16_t u16) {
  uint16_t endianMask = 0xdeadu;

  if (0xde == (*(uint8_t *)&endianMask)) { /* big endian */
    data[0] = (u16 >> 8) & 0xFFu;
    data[1] = u16 & 0xFFu;
  } else {
    data[1] = (u16 >> 8) & 0xFFu;
    data[0] = u16 & 0xFFu;
  }
}

Xcp_ContextType *Xcp_GetContext(void) {
  return (&Xcp_Context);
}

P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) Xcp_GetConfig(void) {
  return (&Xcp_Config);
}

P2CONST(void, AUTOMATIC, XCP_CONST) Xcp_GetCurServiceConfig(void) {
  Xcp_ContextType *context = Xcp_GetContext();
  P2CONST(void, AUTOMATIC, XCP_CONST) config = NULL;

  if (NULL != context->curService) {
    config = context->curService->config;
  }

  return config;
}

boolean Xcp_HasFreePacket(void) {
  return (NULL != SLIST_FIRST(&xcpPacketPool.head));
}

Xcp_PacketType *Xcp_AllocPacket(void) {
  return (Xcp_PacketType *)mp_alloc(&xcpPacketPool);
}

void Xcp_FreePacket(Xcp_PacketType *packet) {
  mp_free(&xcpPacketPool, (uint8_t *)packet);
}

void Xcp_Init(const Xcp_ConfigType *ConfigPtr) {
  Xcp_ContextType *context = Xcp_GetContext();
  (void)ConfigPtr;
  (void)memset(context, 0, sizeof(Xcp_ContextType));
  context->activeChannel = XCP_INVALID_CHL;
  STAILQ_INIT(&context->rxPackets);
  STAILQ_INIT(&context->txPackets);
  mp_init(&xcpPacketPool, (uint8_t *)&xcpPacketSlots, sizeof(Xcp_PacketType),
          ARRAY_SIZE(xcpPacketSlots));
  context->lockedResource = XCP_RES_MASK; /* default all locked */

  Xcp_DspDaqInit();
}

void Xcp_RxIndication(uint8_t channel, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  Xcp_PacketType *packet = NULL;
  Xcp_NegativeResponseCodeType nrc = XCP_E_GENERIC;

  DET_VALIDATE((NULL != PduInfoPtr) && (NULL != PduInfoPtr->SduDataPtr), 0x42, XCP_E_PARAM_POINTER,
               return);
  DET_VALIDATE(PduInfoPtr->SduLength > 0, 0x42, XCP_E_PARAM_POINTER, return);

  if (XCP_INVALID_CHL == context->activeChannel) {
    if (XCP_PID_CMD_STD_CONNECT != PduInfoPtr->SduDataPtr[0]) {
      ASLOG(XCPE, ("[%d] Xcp no connection established for command %02X\n", channel,
                   PduInfoPtr->SduDataPtr[0]));
      ret = E_NOT_OK;
    } else {
      /* mark this channel as active channel for now */
      context->activeChannel = channel;
    }
  } else if (channel != context->activeChannel) {
    ASLOG(XCPE, ("[%d] Xcp already active on channel %d\n", channel, context->activeChannel));
    nrc = XCP_E_CMD_BUSY;
    ret = E_NOT_OK;
  } else {
    /* OK */
  }

  if (E_OK == ret) {
    if (PduInfoPtr->SduLength > XCP_PATCKET_MAX_SIZE) {
      ASLOG(XCPE, ("[%d] packet too big\n", channel));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    packet = Xcp_AllocPacket();
    if (NULL != packet) {
      (void)memcpy(packet->payload, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
      packet->length = (Xcp_MsgLenType)PduInfoPtr->SduLength;
      ASLOG(XCP, ("[%d] alloc packet %p for service %02X\n", channel, packet, packet->payload[0]));
      EnterCritical();
      STAILQ_INSERT_TAIL(&context->rxPackets, packet, entry);
      ExitCritical();
    } else {
      ASLOG(XCPE, ("[%d] no free packet\n", channel));
      nrc = XCP_E_RESOURCE_TEMPORARY_NOT_ACCESSIBLE;
      ret = E_NOT_OK;
    }
  }

  if (E_OK != ret) {
    Xcp_TxError(channel, nrc);
  }
}

void Xcp_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  (void)RxPduId;
  /* @SWS_Xcp_00847 */
  DET_VALIDATE(XCP_ON_CAN_CHL == RxPduId, 0x42, XCP_E_INVALID_PDUID, return);
  Xcp_RxIndication(XCP_ON_CAN_CHL, PduInfoPtr);
}

void Xcp_CanIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result) {
  (void)TxPduId;
  (void)result;
}

void Xcp_MainFunction(void) {
  Xcp_MainFunction_Request();
  Xcp_MainFunction_Response();
  Xcp_MainFunction_Event();
  Xcp_MainFunction_Daq();
}

void Xcp_MainFunction_Write(void) {
  Xcp_MainFunction_DaqWrite();
}
