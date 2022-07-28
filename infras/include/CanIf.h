/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Interface AUTOSAR CP Release 4.4.0
 */
#ifndef CANIF_H
#define CANIF_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "Can.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef struct CanIf_Config_s CanIf_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_CANIF_00001 */
void CanIf_Init(const CanIf_ConfigType *ConfigPtr);
/* @SWS_CANIF_00005 */
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_CANIF_00006 */
void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr);

/* @SWS_CANIF_00007 */
void CanIf_TxConfirmation(PduIdType CanTxPduId);

/* @SWS_CANIF_00189 */
void CanIf_SetDynamicTxId(PduIdType CanIfTxSduId, Can_IdType CanId);
#ifdef __cplusplus
}
#endif
#endif /* CANIF_H */