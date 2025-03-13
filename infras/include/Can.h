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
#ifndef STDIO_TX_CANID
#define STDIO_TX_CANID 0x7FF
#endif

#ifndef STDIO_RX_CANID
#define STDIO_RX_CANID 0x7FE
#endif

#ifndef STDIO_TX_CAN_HANDLE
#define STDIO_TX_CAN_HANDLE 0xFFFE
#endif

#ifndef STDIO_TX_CAN_HTH
#define STDIO_TX_CAN_HTH 0
#endif

#ifndef TRACE_TX_CANID
#define TRACE_TX_CANID 0x7FD
#endif

#ifndef TRACE_TX_CAN_HANDLE
#define TRACE_TX_CAN_HANDLE STDIO_TX_CAN_HANDLE
#endif
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
