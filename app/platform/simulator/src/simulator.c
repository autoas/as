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
static boolean s_reset = FALSE;
/* ================================ [ FUNCTIONS ] ============================================== */
void Mcu_Init(const Mcu_ConfigType *ConfigPtr) {
  (void)ConfigPtr;
}

void Dcm_PerformReset(uint8_t resetType) {
  s_reset = TRUE;
}

void Xcp_PerformReset(void) {
  s_reset = TRUE;
}

boolean Mcu_IsResetRequested(void) {
  boolean rv = s_reset;
  s_reset = FALSE;
  return rv;
}

boolean BL_IsUpdateRequested(void) {
  return FALSE;
}

void BL_JumpToApp(void) {
}

void BL_AliveIndicate(void) {
}

void App_EnterProgramSession(void) {
}

Std_ReturnType BL_GetProgramCondition(Dcm_ProgConditionsType **cond) {
  return E_NOT_OK;
}
