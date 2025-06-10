/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN State Manager AUTOSAR CP R23-11
 */
#ifndef CAN_SM_CANIF_H
#define CAN_SM_CANIF_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_CanSM_00064 */
void CanSM_ControllerBusOff(uint8_t ControllerId);

/* @SWS_CanSM_00396 */
void CanSM_ControllerModeIndication(uint8_t ControllerId, Can_ControllerStateType ControllerMode);

/* @SWS_CanSM_00399 */
void CanSM_TransceiverModeIndication(uint8_t TransceiverId, CanTrcv_TrcvModeType TransceiverMode);

/* @SWS_CanSM_00410 */
void CanSM_TxTimeoutException(NetworkHandleType Channel);

/* @SWS_CanSM_00413 */
void CanSM_ClearTrcvWufFlagIndication(uint8_t Transceiver);

/* @SWS_CanSM_00416 */
void CanSM_CheckTransceiverWakeFlagIndication(uint8_t Transceiver);

/* @SWS_CanSM_00419 */
void CanSM_ConfirmPnAvailability(uint8_t TransceiverId);

/* @SWS_CanSM_91004 */
void CanSM_ConfirmCtrlPnAvailability(uint8_t ControllerId);
#endif /* CAN_SM_CANIF_H */
