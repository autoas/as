/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
#ifndef PDUR_CANTP_H
#define PDUR_CANTP_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#ifdef PDUR_DCM_CANTP_ZERO_COST
#include "Dcm.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifdef PDUR_DCM_CANTP_ZERO_COST
#define PduR_CanTpCopyTxData Dcm_CopyTxData
#define PduR_CanTpRxIndication Dcm_TpRxIndication
#define PduR_CanTpTxConfirmation Dcm_TpTxConfirmation
#define PduR_CanTpStartOfReception Dcm_StartOfReception
#define PduR_CanTpCopyRxData Dcm_CopyRxData
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#ifndef PDUR_DCM_CANTP_ZERO_COST
/* 9.2.4 CanTp module transmission of I-PDU */
/* @SWS_PduR_00518 */
BufReq_ReturnType PduR_CanTpCopyTxData(PduIdType id, const PduInfoType *info,
                                       const RetryInfoType *retry, PduLengthType *availableDataPtr);
/* @SWS_PduR_00375 */
void PduR_CanTpRxIndication(PduIdType id, Std_ReturnType result);
/* @SWS_PduR_00381 */
void PduR_CanTpTxConfirmation(PduIdType id, Std_ReturnType result);

/* 9.1.4 CanTp module reception of I-PDU */
/* @SWS_PduR_00507 */
BufReq_ReturnType PduR_CanTpStartOfReception(PduIdType id, const PduInfoType *info,
                                             PduLengthType TpSduLength,
                                             PduLengthType *bufferSizePtr);
/* @SWS_PduR_00512 */
BufReq_ReturnType PduR_CanTpCopyRxData(PduIdType id, const PduInfoType *info,
                                       PduLengthType *bufferSizePtr);
#endif
#endif /* PDUR_CANTP_H */
