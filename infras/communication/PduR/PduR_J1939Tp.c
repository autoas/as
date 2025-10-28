/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "PduR.h"
#include "PduR_Priv.h"
#include "PduR_J1939Tp.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
BufReq_ReturnType PduR_J1939TpCopyTxData(PduIdType id, const PduInfoType *info,
                                         const RetryInfoType *retry,
                                         PduLengthType *availableDataPtr) {
  return PduR_CopyTxData(id, info, retry, availableDataPtr);
}

void PduR_J1939TpRxIndication(PduIdType id, Std_ReturnType result) {
  PduR_TpRxIndication(id, result);
}

void PduR_J1939TpTxConfirmation(PduIdType id, Std_ReturnType result) {
  PduR_TxConfirmation(id, result);
}

BufReq_ReturnType PduR_J1939TpStartOfReception(PduIdType id, const PduInfoType *info,
                                               PduLengthType TpSduLength,
                                               PduLengthType *bufferSizePtr) {
  return PduR_StartOfReception(id, info, TpSduLength, bufferSizePtr);
}

BufReq_ReturnType PduR_J1939TpCopyRxData(PduIdType id, const PduInfoType *info,
                                         PduLengthType *bufferSizePtr) {
  return PduR_CopyRxData(id, info, bufferSizePtr);
}

Std_ReturnType PduR_J1939TpTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  return PduR_TpTransmit(TxPduId, PduInfoPtr);
}
