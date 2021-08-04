/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef BL_H
#define BL_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dcm.h"
#include "Std_Debug.h"
#include "Flash.h"
#include "Crc.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_BL 0
#define AS_LOG_BLI 1
#define AS_LOG_BLE 2
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern boolean BL_IsUpdateRequested(void);
extern void BL_JumpToApp(void);
extern Std_ReturnType BL_GetProgramCondition(Dcm_ProgConditionsType **cond);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void BL_Init(void);
void BL_SessionReset(void);
Std_ReturnType BL_CheckAppIntegrity(void);
void BL_CheckAndJump(void);
#endif /* BL_H */
