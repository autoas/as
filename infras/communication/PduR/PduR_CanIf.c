/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "PduR.h"
#include "PduR_Priv.h"
#include "PduR_CanIf.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void PduR_CanIfRxIndication(PduIdType rxPduId, const PduInfoType *pduInfoPtr) {
  PduR_RxIndication(rxPduId, pduInfoPtr);
}

void PduR_CanIfTxConfirmation(PduIdType txPduId, Std_ReturnType result) {
  PduR_TxConfirmation(txPduId, result);
}
