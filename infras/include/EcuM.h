/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of ECU State Manager AUTOSAR CP R23-11
 */
#ifndef ECUM_H
#define ECUM_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#define ECUM_MODE_OFF ((EcuM_ModeType)0)
#define ECUM_MODE_STARTUP ((EcuM_ModeType)1)
#define ECUM_MODE_RUN ((EcuM_ModeType)2)
#define ECUM_MODE_POST_RUN ((EcuM_ModeType)3)
#define ECUM_MODE_SHUTDOWN ((EcuM_ModeType)4)
#define ECUM_MODE_SLEEP ((EcuM_ModeType)5)
#define ECUM_MODE_WAKE_SLEEP ((EcuM_ModeType)6)

#define ECUM_SHUTDOWN_TARGET_SLEEP ((EcuM_ShutdownTargetType)0)
#define ECUM_SHUTDOWN_TARGET_RESET ((EcuM_ShutdownTargetType)1)
#define ECUM_SHUTDOWN_TARGET_OFF ((EcuM_ShutdownTargetType)2)

#define ECUM_WKSOURCE_POWER ((EcuM_WakeupSourceType)0x01)
#define ECUM_WKSOURCE_RESET ((EcuM_WakeupSourceType)0x02)
#define ECUM_WKSOURCE_INTERNAL_RESET ((EcuM_WakeupSourceType)0x04)
#define ECUM_WKSOURCE_INTERNAL_WDG ((EcuM_WakeupSourceType)0x08)
#define ECUM_WKSOURCE_EXTERNAL_WDG ((EcuM_WakeupSourceType)0x10)

#define ECUM_RESET_MCU ((EcuM_ResetType)0)
#define ECUM_RESET_WDG ((EcuM_ResetType)1)
#define ECUM_RESET_IO ((EcuM_ResetType)2)

#define ECUM_SUBSTATE_MASK ((EcuM_StateType)0x0f)
#define ECUM_STATE_STARTUP ((EcuM_StateType)0x10)
#define ECUM_STATE_RUN ((EcuM_StateType)0x20)
#define ECUM_STATE_POST_RUN ((EcuM_StateType)0x30)
#define ECUM_STATE_POST_SHUTDOWN ((EcuM_StateType)0x40)
#define ECUM_STATE_SLEEP ((EcuM_StateType)0x50)
#define ECUM_STATE_WAKEUP ((EcuM_StateType)0x60)

#define ECUM_STATE_STARTUP_ONE (ECUM_STATE_STARTUP | 1)
#define ECUM_STATE_STARTUP_TWO (ECUM_STATE_STARTUP | 2)

/* @SWS_EcuM_04032 */
#define ECUM_E_MULTIPLE_RUN_REQUESTS 0x01
#define ECUM_E_SERVICE_DISABLED 0x02
#define ECUM_E_UNINIT 0x03
#define ECUM_E_UNKNOWN_WAKEUP_SOURCE 0x04
#define ECUM_E_INIT_FAILED 0x05
#define ECUM_E_STATE_PAR_OUT_OF_RANGE 0x06
#define ECUM_E_INVALID_PAR 0x07
#define ECUM_E_PARAM_POINTER 0x08
#define ECUM_E_MISMATCHED_RUN_RELEASE 0x09
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_EcuM_04107 */
typedef uint8_t EcuM_ModeType;

/* @SWS_EcuM_91005 */
typedef uint8_t EcuM_StateType;

typedef uint8_t EcuM_UserType;

/* @SWS_EcuM_04044 */
typedef uint8_t EcuM_ResetType;

/* @SWS_EcuM_04136 */
typedef uint8_t EcuM_ShutdownTargetType;

/* @SWS_EcuM_04040 */
typedef uint32_t EcuM_WakeupSourceType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_EcuM_02811 */
void EcuM_Init(void);

/* @SWS_EcuM_02838 */
void EcuM_StartupTwo(void);

/* @SWS_EcuM_02812 */
void EcuM_Shutdown(void);

/* @SWS_EcuM_04122 */
void EcuM_SetState(EcuM_ShutdownTargetType state);

/* @SWS_EcuM_04124 */
Std_ReturnType EcuM_RequestRUN(EcuM_UserType user);

/* @SWS_EcuM_04127 */
Std_ReturnType EcuM_ReleaseRUN(EcuM_UserType user);

/* @SWS_EcuM_04128 */
Std_ReturnType EcuM_RequestPOST_RUN(EcuM_UserType user);

/* @SWS_EcuM_04129 */
Std_ReturnType EcuM_ReleasePOST_RUN(EcuM_UserType user);

/* @SWS_EcuM_02837 */
void EcuM_MainFunction(void);

/* @SWS_EcuM_04108 */
EcuM_ModeType EcuM_GetCurrentMode(void);

void EcuM_BswService(void);

void EcuM_BswServiceFast(void);

/* @SWS_EcuM_02813 */
void EcuM_GetVersionInfo(Std_VersionInfoType *versionInfo);
#endif /* ECUM_H */
