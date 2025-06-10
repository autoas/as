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
/* @SWS_CANIF_91006 */
#define CANIF_E_PARAM_CANID 10
#define CANIF_E_PARAM_HOH 12
#define CANIF_E_PARAM_LPDU 13
#define CANIF_E_PARAM_CONTROLLERID 15
#define CANIF_E_PARAM_WAKEUPSOURCE 16
#define CANIF_E_PARAM_TRCV 17
#define CANIF_E_PARAM_TRCVMODE 18
#define CANIF_E_PARAM_TRCVWAKEUPMODE 19
#define CANIF_E_PARAM_POINTER 20
#define CANIF_E_PARAM_CTRLMODE 21
#define CANIF_E_PARAM_PDU_MODE 22
#define CANIF_E_PARAM_CAN_ERROR 23
#define CANIF_E_UNINIT 30
#define CANIF_E_INVALID_TXPDUID 50
#define CANIF_E_INVALID_RXPDUID 60
#define CANIF_E_INIT_FAILED 80

#define CANIF_OFFLINE ((CanIf_PduModeType)0)
#define CANIF_TX_OFFLINE ((CanIf_PduModeType)1)
#define CANIF_TX_OFFLINE_ACTIVE ((CanIf_PduModeType)2)
#define CANIF_ONLINE ((CanIf_PduModeType)3)
/* ================================ [ TYPES     ] ============================================== */
typedef struct CanIf_Config_s CanIf_ConfigType;

/* @SWS_CANIF_00137 */
typedef uint8_t CanIf_PduModeType;

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_CANIF_00001 */
void CanIf_Init(const CanIf_ConfigType *ConfigPtr);

/* @SWS_CANIF_00003 */
Std_ReturnType CanIf_SetControllerMode(uint8_t ControllerId,
                                       Can_ControllerStateType ControllerMode);

/* @SWS_CANIF_00229 */
Std_ReturnType CanIf_GetControllerMode(uint8_t ControllerId,
                                       Can_ControllerStateType *ControllerModePtr);

/* @SWS_CANIF_00005 */
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_CANIF_00008 */
Std_ReturnType CanIf_SetPduMode(uint8_t ControllerId, CanIf_PduModeType PduModeRequest);

/* @SWS_CANIF_00009 */
Std_ReturnType CanIf_GetPduMode(uint8_t ControllerId, CanIf_PduModeType *PduModePtr);

/* @SWS_CANIF_00189 */
void CanIf_SetDynamicTxId(PduIdType CanIfTxSduId, Can_IdType CanId);

/* @SWS_CANIF_00287 */
Std_ReturnType CanIf_SetTrcvMode(uint8_t TransceiverId, CanTrcv_TrcvModeType TransceiverMode);

/* @SWS_CANIF_00288 */
Std_ReturnType CanIf_GetTrcvMode(uint8_t TransceiverId, CanTrcv_TrcvModeType *TransceiverModePtr);

void CanIf_MainFunction(void);

void CanIf_MainFunction_Fast(void);
#ifdef __cplusplus
}
#endif
#endif /* CANIF_H */
