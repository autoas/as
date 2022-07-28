/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Interface AUTOSAR CP Release 4.4.0
 */
#ifndef CANIF_CAN_H
#define CANIF_CAN_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Can_GeneralTypes.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_CANIF_00006 */
void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr);
/* @SWS_CANIF_00007 */
void CanIf_TxConfirmation(PduIdType CanTxPduId);
#ifdef __cplusplus
}
#endif
#endif /* CANIF_CAN_H */