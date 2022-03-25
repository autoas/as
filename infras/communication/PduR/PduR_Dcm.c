/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "PduR.h"
#include "PduR_Priv.h"
#include "PduR_Cfg.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef PDUR_DCM_TX_BASE_ID
#warning PDUR_DCM_TX_BASE_ID not defined, default set it to 0
#define PDUR_DCM_TX_BASE_ID 0
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType PduR_DcmTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  return PduR_TpTransmit(TxPduId + PDUR_DCM_TX_BASE_ID, PduInfoPtr);
}