/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Mcu.h"
#include <assert.h>
#include "Std_Types.h"
#include "Std_Debug.h"
#include "Dcm.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Mcu_Init(const Mcu_ConfigType *ConfigPtr) {
  (void)ConfigPtr;
}

void Dcm_PerformReset(uint8_t resetType) {
  ASLOG(INFO, ("dummy perform reset %d\n", resetType));
}

boolean BL_IsUpdateRequested(void) {
  return FALSE;
}

void BL_JumpToApp(void) {
}

void BL_AliveIndicate(void) {
}

void App_AliveIndicate(void) {
}

void App_EnterProgramSession(void) {
}

Std_ReturnType BL_GetProgramCondition(Dcm_ProgConditionsType **cond) {
  return E_NOT_OK;
}

void User_Init(void) {
}

void User_MainTask10ms(void) {
}
