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
static P2CONST(Xcp_ServiceType, AUTOMATIC, XCP_CONST) Xcp_FindService(uint8_t pid) {
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  P2CONST(Xcp_ServiceType, AUTOMATIC, XCP_CONST) service = NULL;
  uint16_t l;
  uint16_t h;
  uint16_t m;

  l = 0;
  h = config->NumOfServices - 1u;
  if (pid < config->Services[0].Pid) {
    l = h + 1u; /* avoid the underflow of "m - 1" */
  }

  while (l <= h) {
    m = l + ((h - l) >> 1);
    if (config->Services[m].Pid > pid) {
      h = m - 1u;
    } else if (config->Services[m].Pid < pid) {
      l = m + 1u;
    } else {
      service = &config->Services[m];
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
        (void)memcpy(packet->Payload, PduInfo.SduDataPtr, PduInfo.SduLength);
        packet->Length = (Xcp_MsgLenType)PduInfo.SduLength;
        ASLOG(XCP, ("[%d] alloc Tx Err packet %p\n", channel, packet));
        EnterCritical();
        STAILQ_INSERT_TAIL(&context->TxPackets, packet, entry);
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
        (void)memcpy(packet->Payload, PduInfo.SduDataPtr, PduInfo.SduLength);
        packet->Length = (Xcp_MsgLenType)PduInfo.SduLength;
        ASLOG(XCP, ("[%d] alloc Tx Res packet %p\n", channel, packet));
        EnterCritical();
        STAILQ_INSERT_TAIL(&context->TxPackets, packet, entry);
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
  uint8_t channel = context->ActiveChannel;

  packet = STAILQ_FIRST(&context->RxPackets);

  if (NULL != packet) {
    service = Xcp_FindService(packet->Payload[0]);
    if (NULL == service) {
      nrc = XCP_E_CMD_UNKNOWN;
      ret = E_NOT_OK;
      ASLOG(XCPE, ("[%d] command %02X unknown\n", channel, packet->Payload[0]));
    } else if (0u != (service->Resource & context->LockedResource)) {
      nrc = XCP_E_ACCESS_LOCKED;
      ret = E_NOT_OK;
      ASLOG(XCPE, ("[%d] command %02X access locked\n", channel, packet->Payload[0]));
    } else {
      if (NULL == context->CurService) {
        ASLOG(XCPI, ("[%d] command %02X len %d\n", channel, packet->Payload[0], packet->Length));
        context->CurService = service;
        context->MsgContext.OpStatus = XCP_INITIAL;
      } else if (service == context->CurService) {
        context->MsgContext.OpStatus = XCP_PENDING;
      } else {
        ASLOG(XCPE, ("[%d] Fatal memory error, current service %02X, pending service %02X, abort\n",
                     channel, context->CurService->Pid, service->Pid));
        context->CurService = service;
        context->MsgContext.OpStatus = XCP_INITIAL;
      }
      context->MsgContext.ReqData = &packet->Payload[1];
      context->MsgContext.ReqDataLen = packet->Length - 1u;
      context->MsgContext.ResData = &resData[1];
      context->MsgContext.ResDataLen = 0;
      context->MsgContext.ResMaxDataLen = XCP_PATCKET_MAX_SIZE - 1u;
      ret = service->DspServiceFnc(&context->MsgContext, &nrc);

      if (E_OK == ret) {
        ASLOG(XCP, ("[%d] command %02X DONE\n", channel, packet->Payload[0]));
        (void)Xcp_TxResponse(channel, resData, context->MsgContext.ResDataLen + 1u);
        context->CurService = NULL;
      } else if (XCP_E_PENDING == ret) {
        /* OK, schedule it next time */
        context->MsgContext.OpStatus = XCP_PENDING;
        if (0u != context->MsgContext.ResDataLen) { /* pending but with response */
          ret = Xcp_TxResponse(channel, resData, context->MsgContext.ResDataLen + 1u);
          if (E_OK == ret) {
            ret = XCP_E_PENDING;
          } else {
            ASLOG(XCPE, ("[%d] Abort pending service %02X\n", channel, packet->Payload[0]));
          }
        }
      } else {
        /* Error sending nrc, or OK no RES */
        ASLOG(XCP, ("[%d] command %02X ret = %02X, nrc = %02X\n", channel, packet->Payload[0], ret,
                    nrc));
        context->CurService = NULL;
      }
    }
  } else {
    /* no request packet */
  }

  if (XCP_E_PENDING != ret) {
    if (NULL != packet) {
      ASLOG(XCP, ("[%d] free packet %p for service %02X\n", channel, packet, packet->Payload[0]));
      EnterCritical();
      STAILQ_REMOVE_HEAD(&context->RxPackets, entry);
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
  Xcp_PacketType *packet = STAILQ_FIRST(&context->TxPackets);
  PduInfoType PduInfo;
  Std_ReturnType ret;
  if (NULL != packet) {
    PduInfo.MetaDataPtr = NULL;
    PduInfo.SduDataPtr = packet->Payload;
    PduInfo.SduLength = (PduLengthType)packet->Length;
    /* Now only support XCP on CAN */
    ret = CanIf_Transmit(config->CanIfTxPduId, &PduInfo);
    if (E_OK == ret) {
      EnterCritical();
      STAILQ_REMOVE_HEAD(&context->TxPackets, entry);
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
  if (context->Timer2Reset > 0u) {
    context->Timer2Reset--;
    if (0u == context->Timer2Reset) {
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

  if (NULL != context->CurService) {
    config = context->CurService->Config;
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

void Xcp_Init(const Xcp_ConfigType *configPtr) {
  Xcp_ContextType *context = Xcp_GetContext();
  (void)configPtr;
  (void)memset(context, 0, sizeof(Xcp_ContextType));
  context->ActiveChannel = XCP_INVALID_CHL;
  STAILQ_INIT(&context->RxPackets);
  STAILQ_INIT(&context->TxPackets);
  mp_init(&xcpPacketPool, (uint8_t *)&xcpPacketSlots, sizeof(Xcp_PacketType),
          ARRAY_SIZE(xcpPacketSlots));
  context->LockedResource = XCP_RES_MASK; /* default all locked */

  Xcp_DspDaqInit();
}

void Xcp_RxIndication(uint8_t channel, const PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  Xcp_PacketType *packet = NULL;
  Xcp_NegativeResponseCodeType nrc = XCP_E_GENERIC;

  DET_VALIDATE((NULL != pduInfoPtr) && (NULL != pduInfoPtr->SduDataPtr), 0x42, XCP_E_PARAM_POINTER,
               return);
  DET_VALIDATE(pduInfoPtr->SduLength > 0, 0x42, XCP_E_PARAM_POINTER, return);

  if (XCP_INVALID_CHL == context->ActiveChannel) {
    if (XCP_PID_CMD_STD_CONNECT != pduInfoPtr->SduDataPtr[0]) {
      ASLOG(XCPE, ("[%d] Xcp no connection established for command %02X\n", channel,
                   pduInfoPtr->SduDataPtr[0]));
      ret = E_NOT_OK;
    } else {
      /* mark this channel as active channel for now */
      context->ActiveChannel = channel;
    }
  } else if (channel != context->ActiveChannel) {
    ASLOG(XCPE, ("[%d] Xcp already active on channel %d\n", channel, context->ActiveChannel));
    nrc = XCP_E_CMD_BUSY;
    ret = E_NOT_OK;
  } else {
    /* OK */
  }

  if (E_OK == ret) {
    if (pduInfoPtr->SduLength > XCP_PATCKET_MAX_SIZE) {
      ASLOG(XCPE, ("[%d] packet too big\n", channel));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    packet = Xcp_AllocPacket();
    if (NULL != packet) {
      (void)memcpy(packet->Payload, pduInfoPtr->SduDataPtr, pduInfoPtr->SduLength);
      packet->Length = (Xcp_MsgLenType)pduInfoPtr->SduLength;
      ASLOG(XCP, ("[%d] alloc packet %p for service %02X\n", channel, packet, packet->Payload[0]));
      EnterCritical();
      STAILQ_INSERT_TAIL(&context->RxPackets, packet, entry);
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

void Xcp_CanIfRxIndication(PduIdType rxPduId, const PduInfoType *pduInfoPtr) {
  /* @SWS_Xcp_00847 */
  /* CanIf only routes the XCP CTO PDUs to this callback. rxPduId is the CanIf
   * PDU id of the configured XCP Rx PDU (CANIF_XCP_ON_CAN_RX), not an XCP
   * channel, so it must not be validated against XCP_ON_CAN_CHL: doing so
   * rejected every frame whenever the XCP Rx PDU was not PDU 0, leaving the
   * XCP slave silent on CONNECT. */
  (void)rxPduId;
  Xcp_RxIndication(XCP_ON_CAN_CHL, pduInfoPtr);
}

void Xcp_CanIfTxConfirmation(PduIdType txPduId, Std_ReturnType result) {
  (void)txPduId;
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

void Xcp_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x01, XCP_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_XCP;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 0;
}
