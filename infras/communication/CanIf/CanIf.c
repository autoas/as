/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 - 2022 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Interface AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "CanIf.h"
#include "CanIf_Priv.h"
#include "Can.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_CANIF 0
#define AS_LOG_CANIFI 2
#define AS_LOG_CANIFE 3

#define CANIF_CONFIG (&CanIf_Config)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const CanIf_ConfigType CanIf_Config;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void CanIf_Init(const CanIf_ConfigType *ConfigPtr) {
}

Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  const CanIf_ConfigType *config = CANIF_CONFIG;
  Can_PduType canPdu;
  const CanIf_TxPduType *txPdu;

  if (TxPduId < config->numOfTxPdus) {
    canPdu.swPduHandle = TxPduId;
    canPdu.length = PduInfoPtr->SduLength;
    canPdu.sdu = PduInfoPtr->SduDataPtr;
    txPdu = &config->txPdus[TxPduId];
    if (NULL != txPdu->p_canid) {
      canPdu.id = *txPdu->p_canid;
    } else {
      canPdu.id = txPdu->canid;
    }

    ret = Can_Write(txPdu->hoh, &canPdu);
  } else {
    ASLOG(CANIFE, ("transmist with invalid TxPduId\n"));
  }

  return ret;
}

void CanIf_SetDynamicTxId(PduIdType CanIfTxSduId, Can_IdType CanId) {
  const CanIf_ConfigType *config = CANIF_CONFIG;
  const CanIf_TxPduType *txPdu;

  if (CanIfTxSduId < config->numOfTxPdus) {
    txPdu = &config->txPdus[CanIfTxSduId];
    if (NULL != txPdu->p_canid) {
      *txPdu->p_canid = CanId;
    } else {
      ASLOG(CANIFE, ("[%d] canid is constant\n", CanIfTxSduId));
    }
  } else {
    ASLOG(CANIFE, ("set canid with invalid TxPduId\n"));
  }
}

void CanIf_TxConfirmation(PduIdType CanTxPduId) {
  const CanIf_ConfigType *config = CANIF_CONFIG;
  const CanIf_TxPduType *txPdu;

  if (CanTxPduId < config->numOfTxPdus) {
    txPdu = &config->txPdus[CanTxPduId];
    txPdu->txConfirm(txPdu->txPduId, E_OK);
  } else {
    ASLOG(CANIFE, ("tx confirm with invalid TxPduId\n"));
  }
}

void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr) {
  const CanIf_ConfigType *config = CANIF_CONFIG;
  const CanIf_RxPduType *var;
  const CanIf_RxPduType *rxPdu = NULL;
  uint16_t i;

  /* TODO: this is slow, better to build fast searching map */
  for (i = 0; i < config->numOfRxPdus; i++) {
    var = &config->rxPdus[i];
    if (var->hoh == Mailbox->Hoh) {
      if (var->canid == (Mailbox->CanId & var->mask)) {
        rxPdu = var;
        break;
      }
    }
  }

  if (NULL != rxPdu) {
    if (NULL != rxPdu->rxInd) {
      rxPdu->rxInd(rxPdu->rxPduId, PduInfoPtr);
    } else {
      ASLOG(CANIF, ("[%d] 0x%x rx without indication\n", Mailbox->ControllerId, Mailbox->CanId));
    }
  } else {
    ASLOG(CANIF, ("[%d] 0x%x rx without dest, ignore it\n", Mailbox->ControllerId, Mailbox->CanId));
  }
}
