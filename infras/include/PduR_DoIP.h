/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
#ifndef PDUR_DOIP_H
#define PDUR_DOIP_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */

/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_PduR_00518 */
BufReq_ReturnType PduR_DoIPCopyTxData(PduIdType id, const PduInfoType *info,
                                       const RetryInfoType *retry, PduLengthType *availableDataPtr);
/* @SWS_PduR_00375 */
void PduR_DoIPRxIndication(PduIdType id, Std_ReturnType result);
/* @SWS_PduR_00381 */
void PduR_DoIPTxConfirmation(PduIdType id, Std_ReturnType result);

/* @SWS_PduR_00507 */
BufReq_ReturnType PduR_DoIPStartOfReception(PduIdType id, const PduInfoType *info,
                                             PduLengthType TpSduLength,
                                             PduLengthType *bufferSizePtr);
/* @SWS_PduR_00512 */
BufReq_ReturnType PduR_DoIPCopyRxData(PduIdType id, const PduInfoType *info,
                                       PduLengthType *bufferSizePtr);
#endif /* PDUR_DOIP_H */
