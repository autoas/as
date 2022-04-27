/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 - 2022 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Interface AUTOSAR CP Release 4.4.0
 */
#ifndef _CANIF_PRIV_H
#define _CANIF_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef void (*CanIf_RxIndicationFncType)(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
typedef void (*CanIf_TxConfirmationFncType)(PduIdType TxPduId, Std_ReturnType result);

typedef struct {
  CanIf_RxIndicationFncType rxInd;
  PduIdType rxPduId;
  Can_IdType canid;
  Can_IdType mask; /* if RxCanId&mask == canid, then process */
  Can_HwHandleType hoh;
} CanIf_RxPduType;

typedef struct {
  CanIf_TxConfirmationFncType txConfirm;
  PduIdType txPduId;
  Can_IdType canid;
  Can_IdType *p_canid;
  Can_HwHandleType hoh;
} CanIf_TxPduType;

struct CanIf_Config_s {
  const CanIf_RxPduType *rxPdus;
  const CanIf_TxPduType *txPdus;
  uint16_t numOfRxPdus;
  uint16_t numOfTxPdus;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _CANIF_PRIV_H */
