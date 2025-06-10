/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN State Manager AUTOSAR CP R23-11
 */
#ifndef CAN_SM_H
#define CAN_SM_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "Rte_ComM_Type.h"
/* ================================ [ MACROS    ] ============================================== */
#define CANSM_BOR_IDLE ((CanSM_BusOffRecoveryStateType)0)
#define CANSM_S_BUS_OFF_CHECK ((CanSM_BusOffRecoveryStateType)1)
#define CANSM_S_NO_BUS_OFF ((CanSM_BusOffRecoveryStateType)2)
#define CANSM_S_BUS_OFF_RECOVERY_L1 ((CanSM_BusOffRecoveryStateType)3)
#define CANSM_S_RESTART_CC ((CanSM_BusOffRecoveryStateType)4)
#define CANSM_S_RESTART_CC_WAIT ((CanSM_BusOffRecoveryStateType)5)
#define CANSM_S_BUS_OFF_RECOVERY_L2 ((CanSM_BusOffRecoveryStateType)6)

/* @SWS_CanSM_00654 */
#define CANSM_E_UNINIT 0x01
#define CANSM_E_PARAM_POINTER 0x02
#define CANSM_E_INVALID_NETWORK_HANDLE 0x03
#define CANSM_E_PARAM_CONTROLLER 0x04
#define CANSM_E_PARAM_TRANSCEIVER 0x05
#define CANSM_E_NOT_IN_NO_COM 0x0B
/* ================================ [ TYPES     ] ============================================== */
typedef struct CanSM_Config_s CanSM_ConfigType;

/* @SWS_CanSM_00598 */
typedef enum {
  CANSM_BSWM_NO_COMMUNICATION,
  CANSM_BSWM_SILENT_COMMUNICATION,
  CANSM_BSWM_FULL_COMMUNICATION,
  CANSM_BSWM_BUS_OFF,
  CANSM_BSWM_CHANGE_BAUDRATE
} CanSM_BswMCurrentStateType;

typedef uint8_t CanSM_BusOffRecoveryStateType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_CanSM_00023 */
void CanSM_Init(const CanSM_ConfigType *ConfigPtr);

/* @SWS_CanSM_91001 */
void CanSM_DeInit(void);

/* @SWS_CanSM_00062 */
Std_ReturnType CanSM_RequestComMode(NetworkHandleType network, ComM_ModeType ComM_Mode);

/* @SWS_CanSM_00063 */
Std_ReturnType CanSM_GetCurrentComMode(NetworkHandleType network, ComM_ModeType *ComM_ModePtr);

/* @SWS_CanSM_00609 */
Std_ReturnType CanSM_StartWakeupSource(NetworkHandleType network);

/* @SWS_CanSM_00610 */
Std_ReturnType CanSM_StopWakeupSource(NetworkHandleType network);

/* @SWS_CanSM_00644 */
Std_ReturnType CanSM_SetEcuPassive(boolean CanSM_Passive);

/* SWS_CanSM_00065 */
void CanSM_MainFunction(void);

Std_ReturnType CanSM_GetBusoff_Substate(NetworkHandleType network,
                                        CanSM_BusOffRecoveryStateType *BORStatePtr);
#endif /* CAN_SM_H */
