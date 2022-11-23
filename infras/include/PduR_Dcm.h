/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
#ifndef PDUR_DCM_H
#define PDUR_DCM_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#ifdef PDUR_DCM_CANTP_ZERO_COST
#include "CanTp.h"
#endif
#ifdef PDUR_DCM_LINTP_ZERO_COST
#include "LinTp.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifdef PDUR_DCM_CANTP_ZERO_COST
#define PduR_DcmTransmit CanTp_Transmit
#endif
#ifdef PDUR_DCM_LINTP_ZERO_COST
#define PduR_DcmTransmit LinTp_Transmit
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#if !defined(PDUR_DCM_CANTP_ZERO_COST) && !defined(PDUR_DCM_LINTP_ZERO_COST)
/* 9.2.4 CanTp module transmission of I-PDU */
/* @SWS_PduR_00406 */
Std_ReturnType PduR_DcmTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);
/* @SWS_PduR_00769 */
Std_ReturnType PduR_DcmCancelTransmit(PduIdType TxPduId);
/* @SWS_PduR_00767 */
Std_ReturnType PduR_DcmCancelReceive(PduIdType RxPduId);
#endif
#endif /* PDUR_DCM_H */
