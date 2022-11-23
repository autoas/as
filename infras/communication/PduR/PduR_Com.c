/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "PduR.h"
#include "PduR_Priv.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType PduR_ComTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  return PduR_Transmit(TxPduId, PduInfoPtr);
}

void PduR_ComRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  PduR_RxIndication(RxPduId, PduInfoPtr);
}

void PduR_ComTxConfirmation(PduIdType TxPduId, Std_ReturnType result) {
  PduR_TxConfirmation(TxPduId, result);
}