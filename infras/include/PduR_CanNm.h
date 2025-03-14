/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
#ifndef PDUR_CANNM_H
#define PDUR_CANNM_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#ifdef PDUR_CANNM_COM_ZERO_COST
#include "Com.h"
#endif

#ifdef PDUR_CANNM_COM_ZERO_COST
#define PduR_CanNmTriggerTransmit Com_TriggerTransmit
#define PduR_CanNmTxConfirmation Com_TxConfirmation
#define PduR_CanNmRxIndication Com_RxIndication
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#ifndef PDUR_CANNM_COM_ZERO_COST
Std_ReturnType PduR_CanNmTriggerTransmit(PduIdType TxPduId, PduInfoType *PduInfoPtr);

void PduR_CanNmTxConfirmation(PduIdType TxPduId, Std_ReturnType result);

void PduR_CanNmRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
#endif
#endif /* PDUR_CANNM_H */
