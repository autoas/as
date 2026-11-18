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
Std_ReturnType PduR_SecOCTransmit(PduIdType txPduId, const PduInfoType *pduInfoPtr) {
  return PduR_Transmit(txPduId, pduInfoPtr);
}

void PduR_SecOCRxIndication(PduIdType rxPduId, const PduInfoType *pduInfoPtr) {
  PduR_RxIndication(rxPduId, pduInfoPtr);
}

void PduR_SecOCTxConfirmation(PduIdType txPduId, Std_ReturnType result) {
  PduR_TxConfirmation(txPduId, result);
}
