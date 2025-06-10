/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "PduR.h"
#include "PduR_Priv.h"
#include "PduR_SecOC.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType PduR_SecOCTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  return PduR_Transmit(TxPduId, PduInfoPtr);
}

void PduR_SecOCRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  PduR_RxIndication(RxPduId, PduInfoPtr);
}

void PduR_SecOCTxConfirmation(PduIdType TxPduId, Std_ReturnType result) {
  PduR_TxConfirmation(TxPduId, result);
}
