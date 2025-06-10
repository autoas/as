/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
#ifndef PDUR_J1939TP_H
#define PDUR_J1939TP_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#ifdef PDUR_DCM_J1939TP_ZERO_COST
#include "Dcm.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifdef PDUR_DCM_J1939TP_ZERO_COST
#define PduR_J1939TpCopyTxData Dcm_CopyTxData
#define PduR_J1939TpRxIndication Dcm_TpRxIndication
#define PduR_J1939TpTxConfirmation Dcm_TpTxConfirmation
#define PduR_J1939TpStartOfReception Dcm_StartOfReception
#define PduR_J1939TpCopyRxData Dcm_CopyRxData
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_J1939Tp_00116 */
#ifndef PDUR_DCM_J1939TP_ZERO_COST
/* 9.2.4 J1939Tp module transmission of I-PDU */
/* @SWS_PduR_00518 */
BufReq_ReturnType PduR_J1939TpCopyTxData(PduIdType id, const PduInfoType *info,
                                         const RetryInfoType *retry,
                                         PduLengthType *availableDataPtr);
/* @SWS_PduR_00375 */
void PduR_J1939TpRxIndication(PduIdType id, Std_ReturnType result);
/* @SWS_PduR_00381 */
void PduR_J1939TpTxConfirmation(PduIdType id, Std_ReturnType result);

/* 9.1.4 J1939Tp module reception of I-PDU */
/* @SWS_PduR_00507 */
BufReq_ReturnType PduR_J1939TpStartOfReception(PduIdType id, const PduInfoType *info,
                                               PduLengthType TpSduLength,
                                               PduLengthType *bufferSizePtr);
/* @SWS_PduR_00512 */
BufReq_ReturnType PduR_J1939TpCopyRxData(PduIdType id, const PduInfoType *info,
                                         PduLengthType *bufferSizePtr);
#endif

Std_ReturnType PduR_J1939TpTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);
#ifdef __cplusplus
}
#endif
#endif /* PDUR_J1939TP_H */
