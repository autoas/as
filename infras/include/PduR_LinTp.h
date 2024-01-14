/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
#ifndef PDUR_LINTP_H
#define PDUR_LINTP_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#ifdef PDUR_DCM_LINTP_ZERO_COST
#include "Dcm.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifdef PDUR_DCM_LINTP_ZERO_COST
#define PduR_LinTpCopyTxData Dcm_CopyTxData
#define PduR_LinTpRxIndication Dcm_TpRxIndication
#define PduR_LinTpTxConfirmation Dcm_TpTxConfirmation
#define PduR_LinTpStartOfReception Dcm_StartOfReception
#define PduR_LinTpCopyRxData Dcm_CopyRxData
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#ifndef PDUR_DCM_LINTP_ZERO_COST
/* 9.2.4 LinTp module transmission of I-PDU */
/* @SWS_PduR_00518 */
BufReq_ReturnType PduR_LinTpCopyTxData(PduIdType id, const PduInfoType *info,
                                       const RetryInfoType *retry, PduLengthType *availableDataPtr);
/* @SWS_PduR_00375 */
void PduR_LinTpRxIndication(PduIdType id, Std_ReturnType result);
/* @SWS_PduR_00381 */
void PduR_LinTpTxConfirmation(PduIdType id, Std_ReturnType result);

/* 9.1.4 LinTp module reception of I-PDU */
/* @SWS_PduR_00507 */
BufReq_ReturnType PduR_LinTpStartOfReception(PduIdType id, const PduInfoType *info,
                                             PduLengthType TpSduLength,
                                             PduLengthType *bufferSizePtr);
/* @SWS_PduR_00512 */
BufReq_ReturnType PduR_LinTpCopyRxData(PduIdType id, const PduInfoType *info,
                                       PduLengthType *bufferSizePtr);
#endif
#endif /* PDUR_LINTP_H */
