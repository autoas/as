/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Transceiver Driver AUTOSAR CP R23-11
 */
#ifndef CAN_TRCV_H
#define CAN_TRCV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Can_GeneralTypes.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef struct CanTrcv_Config_s CanTrcv_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_CanTrcv_00001 */
void CanTrcv_Init(const CanTrcv_ConfigType *ConfigPtr);

/* @SWS_CanTrcv_00002 */
Std_ReturnType CanTrcv_SetOpMode(uint8_t Transceiver, CanTrcv_TrcvModeType OpMode);

/* @SWS_CanTrcv_00005 */
Std_ReturnType CanTrcv_GetOpMode(uint8_t Transceiver, CanTrcv_TrcvModeType *OpMode);

/* @SWS_CanTrcv_91001 */
void CanTrcv_DeInit(void);
#endif /* CAN_TRCV_H */
