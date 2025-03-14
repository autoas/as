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

/* @SWS_CANIF_00218 */
void CanIf_ControllerBusOff(uint8_t ControllerId);

/* @SWS_CANIF_00699 */
void CanIf_ControllerModeIndication(uint8_t ControllerId, Can_ControllerStateType ControllerMode);

/* @SWS_CANIF_00764 */
void CanIf_TrcvModeIndication(uint8_t TransceiverId, CanTrcv_TrcvModeType TransceiverMode);

/* @SWS_CANIF_91008 */
void CanIf_ControllerErrorStatePassive(uint8_t ControllerId, uint16_t RxErrorCounter,
                                       uint16_t TxErrorCounter);

/* @SWS_CANIF_91009 */
void CanIf_ErrorNotification(uint8_t ControllerId, Can_ErrorType Can_ErrorType);
#ifdef __cplusplus
}
#endif
#endif /* CANIF_CAN_H */
