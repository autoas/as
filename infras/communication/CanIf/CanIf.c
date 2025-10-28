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
  Can_HwType mailbox;
  PduLengthType SduLength;
  uint8_t data[CANIF_RX_PACKET_DATA_SIZE];
} CanIf_RxPacketType;

typedef struct CanIf_TxPacket_s {
  STAILQ_ENTRY(CanIf_TxPacket_s) entry;
  PduIdType TxPduId;
  PduLengthType SduLength;
  uint8_t data[CANIF_TX_PACKET_DATA_SIZE];
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
static void CanIf_RxDispatch(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr) {
  const CanIf_RxPduType *var;
  const CanIf_RxPduType *rxPdu = NULL;
  const CanIf_CtrlConfigType *config;
  uint16_t l;
  uint16_t h;
  uint16_t m;
  uint32_t canid;

  DET_VALIDATE((NULL != Mailbox) && (NULL != PduInfoPtr) && (NULL != PduInfoPtr->SduDataPtr), 0xFF,
               CANIF_E_PARAM_POINTER, return);
  DET_VALIDATE(Mailbox->ControllerId < CANIF_CONFIG->numOfCtrls, 0xFF, CANIF_E_PARAM_POINTER,
               return);
  ASLOG(CANIF,
        ("RX CAN ID=0x%08X LEN=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X]\n", Mailbox->CanId,
         PduInfoPtr->SduLength, PduInfoPtr->SduDataPtr[0], PduInfoPtr->SduDataPtr[1],
         PduInfoPtr->SduDataPtr[2], PduInfoPtr->SduDataPtr[3], PduInfoPtr->SduDataPtr[4],
         PduInfoPtr->SduDataPtr[5], PduInfoPtr->SduDataPtr[6], PduInfoPtr->SduDataPtr[7]));

  config = &CANIF_CONFIG->CtrlConfigs[Mailbox->ControllerId];
  l = 0;
  h = config->numOfRxPdus - 1u;

  canid = Mailbox->CanId & 0x1FFFFFFFul;

  if (canid < config->rxPdus[0].canid) {
    l = h + 1u; /* avoid the underflow of "m - 1" */
  }
  while ((NULL == rxPdu) && (l <= h)) {
    m = l + ((h - l) >> 1);
    var = &config->rxPdus[m];
    if (var->canid == (canid & var->mask)) {
      rxPdu = var;
    } else if (var->canid > canid) {
      h = m - 1u;
    } else if (var->canid < canid) {
      l = m + 1u;
    } else {
      /* A case that has message with the same IDs: impossible!!! */
      for (h = m + 1u; (NULL == rxPdu) && (h < config->numOfRxPdus); h++) {
        var = &config->rxPdus[h];
        if (var->canid == canid) {
          rxPdu = var;
        }
      }
      for (l = m - 1u; (NULL == rxPdu) && (l < config->numOfRxPdus); l--) {
        /* NOTE: l-- underflow then to be UINT16_MAX */
        var = &config->rxPdus[l];
        if (var->canid == canid) {
          rxPdu = var;
        }
      }
      break;
    }
  }

#if defined(linux) || defined(_WIN32)
  /* For the host PC tools, the CanIf table is not sorted */
  for (l = 0; (l < config->numOfRxPdus) && (NULL == rxPdu); l++) {
    var = &config->rxPdus[l];
    if (var->canid == (canid & var->mask)) {
      rxPdu = var;
    }
  }
#endif

  if (NULL != rxPdu) {
    if (NULL != rxPdu->rxInd) {
      rxPdu->rxInd(rxPdu->rxPduId, PduInfoPtr);
    } else {
      ASLOG(CANIF,
            ("[%d] 0x%" PRIu32 " rx without indication\n", Mailbox->ControllerId, Mailbox->CanId));
    }
  } else {
    ASLOG(CANIF, ("[%d] 0x%" PRIu32 " rx without dest, ignore it\n", Mailbox->ControllerId,
                  Mailbox->CanId));
  }
#ifdef USE_MIRROR
  if (TRUE == CANIF_CONFIG->CtrlContexts[Mailbox->ControllerId].bMirroringActive) {
    Mirror_ReportCanFrame(Mailbox->ControllerId, Mailbox->CanId, PduInfoPtr->SduLength,
                          PduInfoPtr->SduDataPtr);
  }
#endif
}

static Std_ReturnType CanIf_TransmitInternal(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  Can_PduType canPdu;
  const CanIf_TxPduType *txPdu;
  CanIf_CtrlContextType *context;
#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
  const CanIf_CtrlConfigType *ctrlCfg;
#endif
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x49, CANIF_E_UNINIT, return E_NOT_OK);
  /* @SWS_CANIF_00319 */
  DET_VALIDATE(TxPduId < CANIF_CONFIG->numOfTxPdus, 0x49, CANIF_E_INVALID_TXPDUID, return E_NOT_OK);
  /* @SWS_CANIF_00320 */
  DET_VALIDATE((NULL != PduInfoPtr) && (NULL != PduInfoPtr->SduDataPtr), 0x49,
               CANIF_E_PARAM_POINTER, return E_NOT_OK);
  context = &CANIF_CONFIG->CtrlContexts[CANIF_CONFIG->txPdus[TxPduId].ControllerId];
#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
  ctrlCfg = &CANIF_CONFIG->CtrlConfigs[CANIF_CONFIG->txPdus[TxPduId].ControllerId];
#endif
  if (CANIF_ONLINE == context->PduMode) {
    canPdu.swPduHandle = TxPduId;
    canPdu.length = PduInfoPtr->SduLength;
    canPdu.sdu = PduInfoPtr->SduDataPtr;
    txPdu = &CANIF_CONFIG->txPdus[TxPduId];
    if (NULL != txPdu->p_canid) {
      if (NULL != PduInfoPtr->MetaDataPtr) {
        canPdu.id = *(Can_IdType *)PduInfoPtr->MetaDataPtr;
      } else {
        canPdu.id = *txPdu->p_canid;
      }
    } else {
      canPdu.id = txPdu->canid;
    }
#ifdef CANIF_USE_TX_CALLOUT
    ret = CanIf_UserTxCallout(CANIF_CONFIG->txPdus[TxPduId].ControllerId, &canPdu);
    if (E_OK == ret) {
#endif
      ret = Can_Write(txPdu->hoh, &canPdu);
#ifdef CANIF_USE_TX_CALLOUT
    }
#endif
#ifdef USE_MIRROR
    if (E_OK == ret) {
      if (TRUE ==
          CANIF_CONFIG->CtrlContexts[CANIF_CONFIG->txPdus[TxPduId].ControllerId].bMirroringActive) {
        Mirror_ReportCanFrame(CANIF_CONFIG->txPdus[TxPduId].ControllerId, canPdu.id,
                              PduInfoPtr->SduLength, PduInfoPtr->SduDataPtr);
      }
    }
#endif
#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
    if (E_OK == ret) {
      if (0u == context->txTimeoutTimer) {
        context->txTimeoutTimer = ctrlCfg->txTimerout;
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

  for (i = 0; i < CANIF_CONFIG->numOfCtrls; i++) {
    context = &CANIF_CONFIG->CtrlContexts[i];
    bTxTimeout = FALSE;
    EnterCritical();
    if (context->txTimeoutTimer > 0u) {
      context->txTimeoutTimer--;
      if (0u == context->txTimeoutTimer) {
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
void CanIf_Init(const CanIf_ConfigType *ConfigPtr) {
  uint8_t i;
#ifdef CANIF_USE_PB_CONFIG
  if (NULL != ConfigPtr) {
    CANIF_CONFIG = ConfigPtr;
  } else {
    CANIF_CONFIG = &CanIf_Config;
  }
#else
  (void)ConfigPtr;
#endif
  for (i = 0; i < CANIF_CONFIG->numOfCtrls; i++) {
    CANIF_CONFIG->CtrlContexts[i].PduMode = CANIF_OFFLINE;
#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
    CANIF_CONFIG->CtrlContexts[i].txTimeoutTimer = 0;
#endif
#ifdef USE_MIRROR
    CANIF_CONFIG->CtrlContexts[i].bMirroringActive = FALSE;
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

Std_ReturnType CanIf_SetControllerMode(uint8_t ControllerId,
                                       Can_ControllerStateType ControllerMode) {

  DET_VALIDATE(NULL != CANIF_CONFIG, 0x03, CANIF_E_UNINIT, return E_NOT_OK);
  /* @SWS_CANIF_00311 */
  DET_VALIDATE(ControllerId < CANIF_CONFIG->numOfCtrls, 0x03, CANIF_E_PARAM_CONTROLLERID,
               return E_NOT_OK);
  /* @SWS_CANIF_00774 */
  DET_VALIDATE((CAN_CS_STARTED == ControllerMode) || (CAN_CS_STOPPED == ControllerMode) ||
                 (CAN_CS_SLEEP == ControllerMode),
               0x03, CANIF_E_PARAM_CTRLMODE, return E_NOT_OK);

#ifdef USE_MIRROR
  if (TRUE == CANIF_CONFIG->CtrlContexts[ControllerId].bMirroringActive) {
    if (CAN_CS_STARTED == ControllerMode) {
      Mirror_ReportCanState(ControllerId, MIRROR_CAN_NS_BUS_ONLINE);
    }
  }
#endif
  return Can_SetControllerMode(ControllerId, ControllerMode);
}

Std_ReturnType CanIf_GetControllerMode(uint8_t ControllerId,
                                       Can_ControllerStateType *ControllerModePtr) {
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x04, CANIF_E_UNINIT, return E_NOT_OK);
  /* @SWS_CANIF_00313 */
  DET_VALIDATE(ControllerId < CANIF_CONFIG->numOfCtrls, 0x04, CANIF_E_PARAM_CONTROLLERID,
               return E_NOT_OK);
  /* @SWS_CANIF_00656 */
  DET_VALIDATE(NULL != ControllerModePtr, 0x04, CANIF_E_PARAM_POINTER, return E_NOT_OK);

  return Can_GetControllerMode(ControllerId, ControllerModePtr);
}

Std_ReturnType CanIf_SetPduMode(uint8_t ControllerId, CanIf_PduModeType PduModeRequest) {
  Std_ReturnType ret = E_OK;
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x09, CANIF_E_UNINIT, return E_NOT_OK);
  /* @SWS_CANIF_00341 */
  DET_VALIDATE(ControllerId < CANIF_CONFIG->numOfCtrls, 0x09, CANIF_E_PARAM_CONTROLLERID,
               return E_NOT_OK);

  /* @SWS_CANIF_00860 */
  DET_VALIDATE(PduModeRequest <= CANIF_ONLINE, 0x09, CANIF_E_PARAM_PDU_MODE, return E_NOT_OK);

  CANIF_CONFIG->CtrlContexts[ControllerId].PduMode = PduModeRequest;
#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
  if (CANIF_ONLINE != PduModeRequest) {
    CANIF_CONFIG->CtrlContexts[ControllerId].txTimeoutTimer = 0;
  }
#endif

#if CANIF_TX_PACKET_POOL_SIZE > 0
  if (CANIF_ONLINE != PduModeRequest) { /* drop any message that in tx queue */
    EnterCritical();
    STAILQ_INIT(&canIfTxPackets);
    mp_init(&canIfTxPacketPool, (uint8_t *)&canIfTxPacketSlots, sizeof(CanIf_TxPacketType),
            ARRAY_SIZE(canIfTxPacketSlots));
    ExitCritical();
  }
#endif
  return ret;
}

Std_ReturnType CanIf_GetPduMode(uint8_t ControllerId, CanIf_PduModeType *PduModePtr) {
  Std_ReturnType ret = E_OK;
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x0A, CANIF_E_UNINIT, return E_NOT_OK);
  /* @SWS_CANIF_00346 */
  DET_VALIDATE(ControllerId < CANIF_CONFIG->numOfCtrls, 0x0A, CANIF_E_PARAM_CONTROLLERID,
               return E_NOT_OK);

  /* @SWS_CANIF_00657 */
  DET_VALIDATE(NULL == PduModePtr, 0x0A, CANIF_E_PARAM_POINTER, return E_NOT_OK);

  *PduModePtr = CANIF_CONFIG->CtrlContexts[ControllerId].PduMode;

  return ret;
}

Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;

#if CANIF_TX_PACKET_POOL_SIZE > 0
  CanIf_TxPacketType *packet = NULL;
#endif

  ret = CanIf_TransmitInternal(TxPduId, PduInfoPtr);

#if CANIF_TX_PACKET_POOL_SIZE > 0
  if (CAN_BUSY == ret) {
    if (PduInfoPtr->SduLength <= CANIF_TX_PACKET_DATA_SIZE) {
      packet = (CanIf_TxPacketType *)mp_alloc(&canIfTxPacketPool);
      if (NULL != packet) {
        packet->TxPduId = TxPduId;
        packet->SduLength = PduInfoPtr->SduLength;
        (void)memcpy(packet->data, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
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

void CanIf_SetDynamicTxId(PduIdType CanIfTxSduId, Can_IdType CanId) {
  const CanIf_TxPduType *txPdu;
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x0C, CANIF_E_UNINIT, return);
  /* @SWS_CANIF_00352 */
  DET_VALIDATE(CanIfTxSduId < CANIF_CONFIG->numOfTxPdus, 0x0C, CANIF_E_INVALID_TXPDUID, return);

  txPdu = &CANIF_CONFIG->txPdus[CanIfTxSduId];
  if (NULL != txPdu->p_canid) {
    *txPdu->p_canid = CanId;
  } else {
    ASLOG(CANIFE, ("[%d] canid is constant\n", CanIfTxSduId));
  }
}

void CanIf_TxConfirmation(PduIdType CanTxPduId) {
  const CanIf_TxPduType *txPdu;
#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
  CanIf_CtrlContextType *context;
#endif
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x13, CANIF_E_UNINIT, return);
  /* @SWS_CANIF_00410 */
  DET_VALIDATE(CanTxPduId < CANIF_CONFIG->numOfTxPdus, 0x13, CANIF_E_PARAM_LPDU, return);
  txPdu = &CANIF_CONFIG->txPdus[CanTxPduId];
  txPdu->txConfirm(txPdu->txPduId, E_OK);

#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
  context = &CANIF_CONFIG->CtrlContexts[txPdu->ControllerId];
  context->txTimeoutTimer = 0; /* cancel the timer */
#endif
}

void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr) {
#ifdef CANIF_USE_RX_CALLOUT
  Std_ReturnType ret;
#endif
  /* @SWS_CANIF_00419 */
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x14, CANIF_E_UNINIT, return);
  DET_VALIDATE((NULL != Mailbox) && (NULL != PduInfoPtr) && (NULL != PduInfoPtr->SduDataPtr), 0x14,
               CANIF_E_PARAM_POINTER, return);

#ifdef CANIF_USE_RX_CALLOUT
  ret = CanIf_UserRxCallout(Mailbox, PduInfoPtr);
  if (E_OK == ret) {
#endif
#if CANIF_RX_PACKET_POOL_SIZE > 0
    CanIf_RxPacketType *packet = NULL;
    if (PduInfoPtr->SduLength <= CANIF_RX_PACKET_DATA_SIZE) {
      packet = (CanIf_RxPacketType *)mp_alloc(&canIfRxPacketPool);
      if (NULL != packet) {
        packet->mailbox = *Mailbox;
        packet->SduLength = PduInfoPtr->SduLength;
        (void)memcpy(packet->data, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
        EnterCritical();
        STAILQ_INSERT_TAIL(&canIfRxPackets, packet, entry);
        ExitCritical();
      }
    }
    if (NULL == packet) {
      CanIf_RxDispatch(Mailbox, PduInfoPtr);
    }
#else
  CanIf_RxDispatch(Mailbox, PduInfoPtr);
#endif

#ifdef CANIF_USE_RX_CALLOUT
  }
#endif
}

void CanIf_ControllerBusOff(uint8_t ControllerId) {
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x16, CANIF_E_UNINIT, return);
  /* SWS_CANIF_00429 */
  DET_VALIDATE(ControllerId < CANIF_CONFIG->numOfCtrls, 0x16, CANIF_E_PARAM_CONTROLLERID, return);
#ifdef USE_CANSM
  CanSM_ControllerBusOff(ControllerId);
#endif

#ifdef USE_MIRROR
  if (TRUE == CANIF_CONFIG->CtrlContexts[ControllerId].bMirroringActive) {
    Mirror_ReportCanState(ControllerId, MIRROR_CAN_NS_BUS_OFF);
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
    pduInfo.SduDataPtr = rxPacket->data;
    pduInfo.SduLength = rxPacket->SduLength;
    pduInfo.MetaDataPtr = (uint8_t *)&rxPacket->mailbox;
    CanIf_RxDispatch(&rxPacket->mailbox, &pduInfo);
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
    pduInfo.SduDataPtr = txPacket->data;
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
Std_ReturnType CanIf_EnableBusMirroring(uint8_t ControllerId, boolean MirroringActive) {
  Std_ReturnType ret = E_OK;
  DET_VALIDATE(NULL != CANIF_CONFIG, 0x4c, CANIF_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(ControllerId < CANIF_CONFIG->numOfCtrls, 0x4c, CANIF_E_PARAM_CONTROLLERID,
               return E_NOT_OK);

  CANIF_CONFIG->CtrlContexts[ControllerId].bMirroringActive = MirroringActive;
  return ret;
}
#endif
