/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
#ifndef PDUR_COM_H
#define PDUR_COM_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_PduR_00406 */
Std_ReturnType PduR_ComTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_PduR_00362 */
void PduR_ComRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_PduR_00365 */
void PduR_ComTxConfirmation(PduIdType TxPduId, Std_ReturnType result);

/* @SWS_PduR_00369 */
Std_ReturnType PduR_ComTriggerTransmit(PduIdType TxPduId, PduInfoType *PduInfoPtr);
#endif /* PDUR_COM_H */
