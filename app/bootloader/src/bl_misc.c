/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "bl.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */

Std_ReturnType BL_ComCtrlEnableRxAndTx(uint8_t comType, Dcm_NegativeResponseCodeType *ErrorCode) {
  return E_OK;
}

Std_ReturnType BL_ComCtrlDisableRxAndTx(uint8_t comType, Dcm_NegativeResponseCodeType *ErrorCode) {
  return E_OK;
}

Std_ReturnType Dem_EnableDTCSetting(uint8_t ClientId) {
  return E_OK;
}

Std_ReturnType Dem_DisableDTCSetting(uint8_t ClientId) {
  return E_OK;
}