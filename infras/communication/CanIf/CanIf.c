/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 - 2022 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Interface AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "CanIf.h"
#include "CanIf_Cfg.h"
#include "CanIf_Priv.h"
#include "CanIf_Can.h"
#include "Can.h"
#include "Std_Debug.h"
#ifdef USE_CANSM
#include "CanSM_CanIf.h"
#endif
#ifdef USE_MIRROR
#include "Mirror.h"
#endif
#include "mempool.h"
#include "Std_Critical.h"
#include <string.h>
#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_CANIF 0
#define AS_LOG_CANIFI 2
#define AS_LOG_CANIFE 3

#ifdef CANIF_USE_PB_CONFIG
#define CANIF_CONFIG canifConfig
#else
#define CANIF_CONFIG (&CanIf_Config)
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef struct CanIf_RxPacket_s {
  STAILQ_ENTRY(CanIf_RxPacket_s) entry;
  Can_HwType Mailbox;
  PduLengthType SduLength;
  uint8_t Data[CANIF_RX_PACKET_DATA_SIZE];
} CanIf_RxPacketType;

typedef struct CanIf_TxPacket_s {
  STAILQ_ENTRY(CanIf_TxPacket_s) entry;
  PduIdType TxPduId;
  PduLengthType SduLength;
  uint8_t Data[CANIF_TX_PACKET_DATA_SIZE];
} CanIf_TxPacketType;

typedef STAILQ_HEAD(CanIf_RxPacketListHead_s, CanIf_RxPacket_s) CanIf_RxPacketListType;

typedef STAILQ_HEAD(CanIf_TxPacketListHead_s, CanIf_TxPacket_s) CanIf_TxPacketListType;
/* ================================ [ DECLARES  ] ============================================== */
extern const CanIf_ConfigType CanIf_Config;
/* ================================ [ DATAS     ] ============================================== */
#if CANIF_RX_PACKET_POOL_SIZE > 0
static CanIf_RxPacketListType canIfRxPackets;
static CanIf_RxPacketType canIfRxPacketSlots[CANIF_RX_PACKET_POOL_SIZE];
static mempool_t canIfRxPacketPool;
#endif

#if CANIF_TX_PACKET_POOL_SIZE > 0
static CanIf_TxPacketListType canIfTxPackets;
static CanIf_TxPacketType canIfTxPacketSlots[CANIF_TX_PACKET_POOL_SIZE];
static mempool_t canIfTxPacketPool;
#endif

#ifdef CANIF_USE_PB_CONFIG
static const CanIf_ConfigType *canifConfig = NULL;
#endif
/* ================================ [ LOCALS    ] ============================================== */
static void CanIf_RxDispatch(const Can_HwType *mailbox, const PduInfoType *pduInfoPtr) {
  const CanIf_RxPduType *var;
  const CanIf_RxPduType *rxPdu = NULL;
  const CanIf_CtrlConfigType *config;
  uint16_t l;
  uint16_t h;
  uint16_t m;
  uint32_t canid;

  DET_VALIDATE((NULL != mailbox) && (NULL != pduInfoPtr) && (NULL != pduInfoPtr->SduDataPtr), 0xFF,
               CANIF_E_PARAM_POINTER, return);
  DET_VALIDATE(mailbox->ControllerId < CANIF_CONFIG->NumOfCtrls, 0xFF, CANIF_E_PARAM_POINTER,
               return);
  ASLOG(CANIF,
        ("RX CAN ID=0x%08X LEN=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X]\n", mailbox->CanId,
         pduInfoPtr->SduLength, pduInfoPtr->SduDataPtr[0], pduInfoPtr->SduDataPtr[1],
         pduInfoPtr->SduDataPtr[2], pduInfoPtr->SduDataPtr[3], pduInfoPtr->SduDataPtr[4],
         pduInfoPtr->SduDataPtr[5], pduInfoPtr->SduDataPtr[6], pduInfoPtr->SduDataPtr[7]));

  config = &CANIF_CONFIG->CtrlConfigs[mailbox->ControllerId];
  l = 0;
  h = config->NumOfRxPdus - 1u;

  canid = mailbox->CanId & CAN_CANID_MASK;

  if (0 == config->NumOfRxPdus) {
    h = 0;
    l = h + 1u;
  } else if (canid < config->RxPdus[0].CanId) {
    l = h + 1u; /* avoid the underflow of "m - 1" */
  } else {
    /* do nothing */
  }
  while ((NULL == rxPdu) && (l <= h)) {
    m = l + ((h - l) >> 1);
    var = &config->RxPdus[m];
    if (var->CanId == (canid & var->Mask)) {
      rxPdu = var;
    } else if (var->CanId > canid) {
      h = m - 1u;
    } else if (var->CanId < canid) {
      l = m + 1u;
    } else {
      /* A case that has message with the same IDs: impossible!!! */
      for (h = m + 1u; (NULL == rxPdu) && (h < config->NumOfRxPdus); h++) {
        var = &config->RxPdus[h];
        if (var->CanId == canid) {
          rxPdu = var;
        }
      }
      for (l = m - 1u; (NULL == rxPdu) && (l < config->NumOfRxPdus); l--) {
        /* NOTE: l-- underflow then to be UINT16_MAX */
        var = &config->RxPdus[l];
        if (var->CanId == canid) {
          rxPdu = var;
        }
      }
      break;
    }
  }

#if defined(linux) || defined(_WIN32)
  /* For the host PC tools, the CanIf table is not sorted */
  for (l = 0; (l < config->NumOfRxPdus) && (NULL == rxPdu); l++) {
    var = &config->RxPdus[l];
    if (var->CanId == (canid & var->Mask)) {
      rxPdu = var;
    }
  }
#endif

  if (NULL != rxPdu) {
    if (NULL != rxPdu->RxInd) {
      rxPdu->RxInd(rxPdu->RxPduId, pduInfoPtr);
    } else {
      ASLOG(CANIF,
            ("[%d] 0x%" PRIu32 " rx without indication\n", mailbox->ControllerId, mailbox->CanId));
    }
  } else {
    ASLOG(CANIF, ("[%d] 0x%" PRIu32 " rx without dest, ignore it\n", mailbox->ControllerId,
                  mailbox->CanId));
  }
#ifdef USE_MIRROR
  if (TRUE == CANIF_CONFIG->CtrlContexts[mailbox->ControllerId].MirroringActive) {
    Mirror_ReportCanFrame(mailbox->ControllerId, mailbox->CanId, pduInfoPtr->SduLength,
                          pduInfoPtr->SduDataPtr);
  }
#endif
}

static Std_ReturnType CanIf_TransmitInternal(PduIdType txPduId, const PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  Can_PduType canPdu;
  const CanIf_TxPduType *txPdu;
  CanIf_CtrlContextType *context;
#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
  const CanIf_CtrlConfigType *ctrlCfg;
#endif
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x49, CANIF_E_UNINIT, return E_NOT_OK);
  /* @SWS_CANIF_00319 */
  DET_VALIDATE(txPduId < CANIF_CONFIG->NumOfTxPdus, 0x49, CANIF_E_INVALID_TXPDUID, return E_NOT_OK);
  /* @SWS_CANIF_00320 */
  DET_VALIDATE((NULL != pduInfoPtr) && (NULL != pduInfoPtr->SduDataPtr), 0x49,
               CANIF_E_PARAM_POINTER, return E_NOT_OK);
  context = &CANIF_CONFIG->CtrlContexts[CANIF_CONFIG->TxPdus[txPduId].ControllerId];
#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
  ctrlCfg = &CANIF_CONFIG->CtrlConfigs[CANIF_CONFIG->TxPdus[txPduId].ControllerId];
#endif
  if (CANIF_ONLINE == context->PduMode) {
    canPdu.swPduHandle = txPduId;
    canPdu.length = pduInfoPtr->SduLength;
    canPdu.sdu = pduInfoPtr->SduDataPtr;
    txPdu = &CANIF_CONFIG->TxPdus[txPduId];
    if (NULL != txPdu->CanIdPtr) {
      if (NULL != pduInfoPtr->MetaDataPtr) {
        canPdu.id = *(Can_IdType *)pduInfoPtr->MetaDataPtr;
      } else {
        canPdu.id = *txPdu->CanIdPtr;
      }
    } else {
      canPdu.id = txPdu->CanId;
    }
#ifdef CANIF_USE_TX_CALLOUT
    ret = CanIf_UserTxCallout(CANIF_CONFIG->TxPdus[txPduId].ControllerId, &canPdu);
    if (E_OK == ret) {
#endif
      ret = Can_Write(txPdu->Hoh, &canPdu);
#ifdef CANIF_USE_TX_CALLOUT
    }
#endif
#ifdef USE_MIRROR
    if (E_OK == ret) {
      if (TRUE ==
          CANIF_CONFIG->CtrlContexts[CANIF_CONFIG->TxPdus[txPduId].ControllerId].MirroringActive) {
        Mirror_ReportCanFrame(CANIF_CONFIG->TxPdus[txPduId].ControllerId, canPdu.id,
                              pduInfoPtr->SduLength, pduInfoPtr->SduDataPtr);
      }
    }
#endif
#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
    if (E_OK == ret) {
      if (0u == context->TxTimeoutTimer) {
        context->TxTimeoutTimer = ctrlCfg->TxTimeout;
      }
    }
#endif
  }

  return ret;
}

#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
static void CanIf_MainFunction_TxTimeout(void) {
  uint8_t i;
  CanIf_CtrlContextType *context;
  boolean bTxTimeout;

  for (i = 0; i < CANIF_CONFIG->NumOfCtrls; i++) {
    context = &CANIF_CONFIG->CtrlContexts[i];
    bTxTimeout = FALSE;
    EnterCritical();
    if (context->TxTimeoutTimer > 0u) {
      context->TxTimeoutTimer--;
      if (0u == context->TxTimeoutTimer) {
        bTxTimeout = TRUE;
      }
    }
    ExitCritical();
    if (TRUE == bTxTimeout) {
      CanSM_TxTimeoutException(i);
    }
  }
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
void CanIf_Init(const CanIf_ConfigType *configPtr) {
  uint8_t i;
#ifdef CANIF_USE_PB_CONFIG
  if (NULL != configPtr) {
    CANIF_CONFIG = configPtr;
  } else {
    CANIF_CONFIG = &CanIf_Config;
  }
#else
  (void)configPtr;
#endif
  for (i = 0; i < CANIF_CONFIG->NumOfCtrls; i++) {
    CANIF_CONFIG->CtrlContexts[i].PduMode = CANIF_OFFLINE;
#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
    CANIF_CONFIG->CtrlContexts[i].TxTimeoutTimer = 0;
#endif
#ifdef USE_MIRROR
    CANIF_CONFIG->CtrlContexts[i].MirroringActive = FALSE;
#endif
  }
#if CANIF_RX_PACKET_POOL_SIZE > 0
  STAILQ_INIT(&canIfRxPackets);
  mp_init(&canIfRxPacketPool, (uint8_t *)&canIfRxPacketSlots, sizeof(CanIf_RxPacketType),
          ARRAY_SIZE(canIfRxPacketSlots));
#endif
#if CANIF_TX_PACKET_POOL_SIZE > 0
  STAILQ_INIT(&canIfTxPackets);
  mp_init(&canIfTxPacketPool, (uint8_t *)&canIfTxPacketSlots, sizeof(CanIf_TxPacketType),
          ARRAY_SIZE(canIfTxPacketSlots));
#endif
}

Std_ReturnType CanIf_SetControllerMode(uint8_t controllerId,
                                       Can_ControllerStateType controllerMode) {

  DET_VALIDATE(NULL != CANIF_CONFIG, 0x03, CANIF_E_UNINIT, return E_NOT_OK);
  /* @SWS_CANIF_00311 */
  DET_VALIDATE(controllerId < CANIF_CONFIG->NumOfCtrls, 0x03, CANIF_E_PARAM_CONTROLLERID,
               return E_NOT_OK);
  /* @SWS_CANIF_00774 */
  DET_VALIDATE((CAN_CS_STARTED == controllerMode) || (CAN_CS_STOPPED == controllerMode) ||
                 (CAN_CS_SLEEP == controllerMode),
               0x03, CANIF_E_PARAM_CTRLMODE, return E_NOT_OK);

#ifdef USE_MIRROR
  if (TRUE == CANIF_CONFIG->CtrlContexts[controllerId].MirroringActive) {
    if (CAN_CS_STARTED == controllerMode) {
      Mirror_ReportCanState(controllerId, MIRROR_CAN_NS_BUS_ONLINE);
    }
  }
#endif
  return Can_SetControllerMode(controllerId, controllerMode);
}

Std_ReturnType CanIf_GetControllerMode(uint8_t controllerId,
                                       Can_ControllerStateType *controllerModePtr) {
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x04, CANIF_E_UNINIT, return E_NOT_OK);
  /* @SWS_CANIF_00313 */
  DET_VALIDATE(controllerId < CANIF_CONFIG->NumOfCtrls, 0x04, CANIF_E_PARAM_CONTROLLERID,
               return E_NOT_OK);
  /* @SWS_CANIF_00656 */
  DET_VALIDATE(NULL != controllerModePtr, 0x04, CANIF_E_PARAM_POINTER, return E_NOT_OK);

  return Can_GetControllerMode(controllerId, controllerModePtr);
}

Std_ReturnType CanIf_SetPduMode(uint8_t controllerId, CanIf_PduModeType pduModeRequest) {
  Std_ReturnType ret = E_OK;
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x09, CANIF_E_UNINIT, return E_NOT_OK);
  /* @SWS_CANIF_00341 */
  DET_VALIDATE(controllerId < CANIF_CONFIG->NumOfCtrls, 0x09, CANIF_E_PARAM_CONTROLLERID,
               return E_NOT_OK);

  /* @SWS_CANIF_00860 */
  DET_VALIDATE(pduModeRequest <= CANIF_ONLINE, 0x09, CANIF_E_PARAM_PDU_MODE, return E_NOT_OK);

  CANIF_CONFIG->CtrlContexts[controllerId].PduMode = pduModeRequest;
#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
  if (CANIF_ONLINE != pduModeRequest) {
    CANIF_CONFIG->CtrlContexts[controllerId].TxTimeoutTimer = 0;
  }
#endif

#if CANIF_TX_PACKET_POOL_SIZE > 0
  if (CANIF_ONLINE != pduModeRequest) { /* drop any message that in tx queue */
    EnterCritical();
    STAILQ_INIT(&canIfTxPackets);
    mp_init(&canIfTxPacketPool, (uint8_t *)&canIfTxPacketSlots, sizeof(CanIf_TxPacketType),
            ARRAY_SIZE(canIfTxPacketSlots));
    ExitCritical();
  }
#endif
  return ret;
}

Std_ReturnType CanIf_GetPduMode(uint8_t controllerId, CanIf_PduModeType *pduModePtr) {
  Std_ReturnType ret = E_OK;
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x0A, CANIF_E_UNINIT, return E_NOT_OK);
  /* @SWS_CANIF_00346 */
  DET_VALIDATE(controllerId < CANIF_CONFIG->NumOfCtrls, 0x0A, CANIF_E_PARAM_CONTROLLERID,
               return E_NOT_OK);

  /* @SWS_CANIF_00657 */
  DET_VALIDATE(NULL == pduModePtr, 0x0A, CANIF_E_PARAM_POINTER, return E_NOT_OK);

  *pduModePtr = CANIF_CONFIG->CtrlContexts[controllerId].PduMode;

  return ret;
}

Std_ReturnType CanIf_Transmit(PduIdType txPduId, const PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;

#if CANIF_TX_PACKET_POOL_SIZE > 0
  CanIf_TxPacketType *packet = NULL;
#endif

  ret = CanIf_TransmitInternal(txPduId, pduInfoPtr);

#if CANIF_TX_PACKET_POOL_SIZE > 0
  if (CAN_BUSY == ret) {
    if (pduInfoPtr->SduLength <= CANIF_TX_PACKET_DATA_SIZE) {
      packet = (CanIf_TxPacketType *)mp_alloc(&canIfTxPacketPool);
      if (NULL != packet) {
        packet->TxPduId = txPduId;
        packet->SduLength = pduInfoPtr->SduLength;
        (void)memcpy(packet->Data, pduInfoPtr->SduDataPtr, pduInfoPtr->SduLength);
        EnterCritical();
        STAILQ_INSERT_TAIL(&canIfTxPackets, packet, entry);
        ExitCritical();
        ret = E_OK;
      }
    }
  }
#endif

  return ret;
}

void CanIf_SetDynamicTxId(PduIdType canIfTxSduId, Can_IdType canId) {
  const CanIf_TxPduType *txPdu;
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x0C, CANIF_E_UNINIT, return);
  /* @SWS_CANIF_00352 */
  DET_VALIDATE(canIfTxSduId < CANIF_CONFIG->NumOfTxPdus, 0x0C, CANIF_E_INVALID_TXPDUID, return);

  txPdu = &CANIF_CONFIG->TxPdus[canIfTxSduId];
  if (NULL != txPdu->CanIdPtr) {
    *txPdu->CanIdPtr = canId;
  } else {
    ASLOG(CANIFE, ("[%d] canid is constant\n", canIfTxSduId));
  }
}

void CanIf_TxConfirmation(PduIdType canTxPduId) {
  const CanIf_TxPduType *txPdu;
#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
  CanIf_CtrlContextType *context;
#endif
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x13, CANIF_E_UNINIT, return);
  /* @SWS_CANIF_00410 */
  DET_VALIDATE(canTxPduId < CANIF_CONFIG->NumOfTxPdus, 0x13, CANIF_E_PARAM_LPDU, return);
  txPdu = &CANIF_CONFIG->TxPdus[canTxPduId];
  txPdu->TxConfirm(txPdu->TxPduId, E_OK);

#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
  context = &CANIF_CONFIG->CtrlContexts[txPdu->ControllerId];
  context->TxTimeoutTimer = 0; /* cancel the timer */
#endif
}

void CanIf_RxIndication(const Can_HwType *mailbox, const PduInfoType *pduInfoPtr) {
#ifdef CANIF_USE_RX_CALLOUT
  Std_ReturnType ret;
#endif
#if CANIF_RX_PACKET_POOL_SIZE > 0
  CanIf_RxPacketType *packet = NULL;
#endif
  /* @SWS_CANIF_00419 */
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x14, CANIF_E_UNINIT, return);
  DET_VALIDATE((NULL != mailbox) && (NULL != pduInfoPtr) && (NULL != pduInfoPtr->SduDataPtr), 0x14,
               CANIF_E_PARAM_POINTER, return);

#ifdef CANIF_USE_RX_CALLOUT
  ret = CanIf_UserRxCallout(mailbox, pduInfoPtr);
  if (E_OK == ret) {
#endif
#if CANIF_RX_PACKET_POOL_SIZE > 0
    if (pduInfoPtr->SduLength <= CANIF_RX_PACKET_DATA_SIZE) {
      packet = (CanIf_RxPacketType *)mp_alloc(&canIfRxPacketPool);
      if (NULL != packet) {
        packet->Mailbox = *mailbox;
        packet->SduLength = pduInfoPtr->SduLength;
        (void)memcpy(packet->Data, pduInfoPtr->SduDataPtr, pduInfoPtr->SduLength);
        EnterCritical();
        STAILQ_INSERT_TAIL(&canIfRxPackets, packet, entry);
        ExitCritical();
      }
    }
    if (NULL == packet) {
      CanIf_RxDispatch(mailbox, pduInfoPtr);
    }
#else
  CanIf_RxDispatch(mailbox, pduInfoPtr);
#endif

#ifdef CANIF_USE_RX_CALLOUT
  }
#endif
}

void CanIf_ControllerBusOff(uint8_t controllerId) {
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x16, CANIF_E_UNINIT, return);
  /* SWS_CANIF_00429 */
  DET_VALIDATE(controllerId < CANIF_CONFIG->NumOfCtrls, 0x16, CANIF_E_PARAM_CONTROLLERID, return);
#ifdef USE_CANSM
  CanSM_ControllerBusOff(controllerId);
#endif

#ifdef USE_MIRROR
  if (TRUE == CANIF_CONFIG->CtrlContexts[controllerId].MirroringActive) {
    Mirror_ReportCanState(controllerId, MIRROR_CAN_NS_BUS_OFF);
  }
#endif
}

void CanIf_MainFunction_Fast(void) {
#if CANIF_RX_PACKET_POOL_SIZE > 0
  CanIf_RxPacketType *rxPacket = NULL;
#endif
#if CANIF_TX_PACKET_POOL_SIZE > 0
  Std_ReturnType ret;
  CanIf_TxPacketType *txPacket = NULL;
#endif

#if (CANIF_TX_PACKET_POOL_SIZE > 0) || (CANIF_RX_PACKET_POOL_SIZE > 0)
  PduInfoType pduInfo;
#endif

  DET_VALIDATE(NULL != CANIF_CONFIG, 0xF1, CANIF_E_UNINIT, return);

#if CANIF_RX_PACKET_POOL_SIZE > 0
  EnterCritical();
  rxPacket = STAILQ_FIRST(&canIfRxPackets);
  while (NULL != rxPacket) {
    STAILQ_REMOVE_HEAD(&canIfRxPackets, entry);
    InterLeaveCritical();
    pduInfo.SduDataPtr = rxPacket->Data;
    pduInfo.SduLength = rxPacket->SduLength;
    pduInfo.MetaDataPtr = (uint8_t *)&rxPacket->Mailbox;
    CanIf_RxDispatch(&rxPacket->Mailbox, &pduInfo);
    mp_free(&canIfRxPacketPool, (uint8_t *)rxPacket);
    InterEnterCritical();
    rxPacket = STAILQ_FIRST(&canIfRxPackets);
  }
  ExitCritical();
#endif

#if CANIF_RX_PACKET_POOL_SIZE > 0
  EnterCritical();
  txPacket = STAILQ_FIRST(&canIfTxPackets);
  while (NULL != txPacket) {
    STAILQ_REMOVE_HEAD(&canIfTxPackets, entry);
    InterLeaveCritical();
    pduInfo.SduDataPtr = txPacket->Data;
    pduInfo.SduLength = txPacket->SduLength;
    pduInfo.MetaDataPtr = NULL;
    ret = CanIf_TransmitInternal(txPacket->TxPduId, &pduInfo);
    if (E_OK == ret) {
      mp_free(&canIfTxPacketPool, (uint8_t *)txPacket);
    }
    InterEnterCritical();
    if (E_OK == ret) {
      txPacket = STAILQ_FIRST(&canIfTxPackets);
    } else {
      break;
    }
  }
  ExitCritical();
#endif

#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM) && (1 == CANIF_MAIN_FUNCTION_PERIOD)
  CanIf_MainFunction_TxTimeout();
#endif
}

void CanIf_MainFunction(void) {
  DET_VALIDATE(NULL != CANIF_CONFIG, 0xF2, CANIF_E_UNINIT, return);
#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM) && (1 != CANIF_MAIN_FUNCTION_PERIOD)
  CanIf_MainFunction_TxTimeout();
#endif
}

#ifdef USE_MIRROR
Std_ReturnType CanIf_EnableBusMirroring(uint8_t controllerId, boolean mirroringActive) {
  Std_ReturnType ret = E_OK;
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x4c, CANIF_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(controllerId < CANIF_CONFIG->NumOfCtrls, 0x4c, CANIF_E_PARAM_CONTROLLERID,
               return E_NOT_OK);

  CANIF_CONFIG->CtrlContexts[controllerId].MirroringActive = mirroringActive;
  return ret;
}
#endif

void CanIf_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x0b, CANIF_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_CANIF;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 0;
}
