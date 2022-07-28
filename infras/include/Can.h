/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Driver AUTOSAR CP Release 4.4.0
 */
#ifndef CAN_H
#define CAN_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Can_GeneralTypes.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef struct Can_Config_s Can_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_Can_00223 */
void Can_Init(const Can_ConfigType *Config);
/* @SWS_Can_91002 */
void Can_DeInit(void);

/* @SWS_Can_00233 */
Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo);

/* @SWS_Can_00230 */
Std_ReturnType Can_SetControllerMode(uint8_t Controller, Can_ControllerStateType Transition);

/* @SWS_Can_00225 */
void Can_MainFunction_Write(void);
/* @SWS_Can_00226 */
void Can_MainFunction_Read(void);

void Can_MainFunction_WriteChannel(uint8_t Channel);
void Can_MainFunction_ReadChannel(uint8_t Channel);
/* @SWS_Can_00227 */
void Can_MainFunction_BusOff(void);
/* @SWS_Can_00228 */
void Can_MainFunction_WakeUp(void);
/* @SWS_Can_00368 */
void Can_MainFunction_Mode(void);
#ifdef __cplusplus
}
#endif
#endif /* CAN_H */