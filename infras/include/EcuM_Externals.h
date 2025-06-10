/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: AUTOSAR_SWS_ECUStateManager.pdf
 */
#ifndef ECUM_EXTERNALS_H
#define ECUM_EXTERNALS_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "EcuM.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_EcuM_04085 */
void EcuM_AL_SetProgrammableInterrupts(void);

/* @SWS_EcuM_02905 */
void EcuM_AL_DriverInitZero(void);

/* @SWS_EcuM_02907 */
void EcuM_AL_DriverInitOne(void);

void EcuM_AL_DriverInitTwo(void);

void EcuM_AL_EnterRUN(void);
void EcuM_AL_EnterPOST_RUN(void);
void EcuM_AL_EnterSLEEP(void);

/* @SWS_EcuM_04137 */
void EcuM_LoopDetection(void);

/* @SWS_EcuM_02916 */
void EcuM_OnGoOffOne(void);

/* @SWS_EcuM_02917 */
void EcuM_OnGoOffTwo(void);

/* @SWS_EcuM_02920 */
void EcuM_AL_SwitchOff(void);

/* @SWS_EcuM_04065 */
void EcuM_AL_Reset(EcuM_ResetType reset);

/* @SWS_EcuM_02918 */
void EcuM_EnableWakeupSources(EcuM_WakeupSourceType wakeupSource);

/* @SWS_EcuM_02929 */
void EcuM_CheckWakeup(EcuM_WakeupSourceType wakeupSource);

/* @SWS_EcuM_02927 */
void EcuM_EndCheckWakeup(EcuM_WakeupSourceType WakeupSource);
#endif /* ECUM_EXTERNALS_H */
