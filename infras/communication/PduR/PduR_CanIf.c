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
void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  PduR_RxIndication(RxPduId, PduInfoPtr);
}

void PduR_CanIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result) {
  PduR_TxConfirmation(TxPduId, result);
}