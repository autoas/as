/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022-2023 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "PduR.h"
#include "PduR_Priv.h"
#include "PduR_LinTp.h"
#ifdef USE_LINTP
#include "LinTp_Cfg.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifndef LINTP_GW_USER_HOOK_RX_IND
#define LINTP_GW_USER_HOOK_RX_IND(id, result)
#endif

#ifndef LINTP_GW_USER_HOOK_TX_CONFIRM
#define LINTP_GW_USER_HOOK_TX_CONFIRM(id, result)
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#if defined(PDUR_USE_MEMPOOL)
BufReq_ReturnType PduR_LinTpGwCopyTxData(PduIdType id, const PduInfoType *info,
                                         const RetryInfoType *retry,
                                         PduLengthType *availableDataPtr) {
  return PduR_GwCopyTxData(id, info, retry, availableDataPtr);
}

void PduR_LinTpGwTxConfirmation(PduIdType id, Std_ReturnType result) {
  PduR_GwTxConfirmation(id, result);
}

BufReq_ReturnType PduR_LinTpGwStartOfReception(PduIdType id, const PduInfoType *info,
                                               PduLengthType TpSduLength,
                                               PduLengthType *bufferSizePtr) {
  return PduR_GwStartOfReception(id, info, TpSduLength, bufferSizePtr);
}

BufReq_ReturnType PduR_LinTpGwCopyRxData(PduIdType id, const PduInfoType *info,
                                         PduLengthType *bufferSizePtr) {
  return PduR_GwCopyRxData(id, info, bufferSizePtr);
}

void PduR_LinTpGwRxIndication(PduIdType id, Std_ReturnType result) {
  LINTP_GW_USER_HOOK_RX_IND(id, result);
  PduR_GwRxIndication(id, result);
}
#endif
BufReq_ReturnType PduR_LinTpCopyTxData(PduIdType id, const PduInfoType *info,
                                       const RetryInfoType *retry,
                                       PduLengthType *availableDataPtr) {
  return PduR_CopyTxData(id + PDUR_CONFIG->lintpTxBaseID, info, retry, availableDataPtr);
}

void PduR_LinTpRxIndication(PduIdType id, Std_ReturnType result) {
  PduR_TpRxIndication(id + PDUR_CONFIG->lintpRxBaseID, result);
}

void PduR_LinTpTxConfirmation(PduIdType id, Std_ReturnType result) {
  LINTP_GW_USER_HOOK_TX_CONFIRM(id + PDUR_CONFIG->lintpTxBaseID, result);
  PduR_TxConfirmation(id + PDUR_CONFIG->lintpTxBaseID, result);
}

BufReq_ReturnType PduR_LinTpStartOfReception(PduIdType id, const PduInfoType *info,
                                             PduLengthType TpSduLength,
                                             PduLengthType *bufferSizePtr) {
  return PduR_StartOfReception(id + PDUR_CONFIG->lintpRxBaseID, info, TpSduLength,
                               bufferSizePtr);
}

BufReq_ReturnType PduR_LinTpCopyRxData(PduIdType id, const PduInfoType *info,
                                       PduLengthType *bufferSizePtr) {
  return PduR_CopyRxData(id + PDUR_CONFIG->lintpRxBaseID, info, bufferSizePtr);
}
