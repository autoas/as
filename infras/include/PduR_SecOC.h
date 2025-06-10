/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
#ifndef PDUR_SECOC_H
#define PDUR_SECOC_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_PduR_00406 */
Std_ReturnType PduR_SecOCTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_PduR_00362 */
void PduR_SecOCRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_PduR_00365 */
void PduR_SecOCTxConfirmation(PduIdType TxPduId, Std_ReturnType result);

/* @SWS_PduR_00369 */
Std_ReturnType PduR_SecOCTriggerTransmit(PduIdType TxPduId, PduInfoType *PduInfoPtr);
#endif /* PDUR_SECOC_H */
