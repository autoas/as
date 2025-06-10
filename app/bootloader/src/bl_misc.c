/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "bl.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const uint32_t blFingerPrintAddr;
extern const uint32_t blAppValidFlagAddr;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
#if defined(BL_USE_META) || defined(BL_USE_APP_INFO_V2)
static boolean bMiscMetaInfoInitDone = FALSE;
#endif
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

void BL_MiscInit(void) {
#if defined(BL_USE_META) || defined(BL_USE_APP_INFO_V2)
  bMiscMetaInfoInitDone = FALSE;
#endif
}

Std_ReturnType BL_MiscMetaInfoInitOnce(void) {
  Std_ReturnType ret = E_OK;
#if defined(BL_USE_META) || defined(BL_USE_APP_INFO_V2)
  if (FALSE == bMiscMetaInfoInitDone) {
#ifdef BL_USE_META
    ret = BL_MetaBackup();
#endif
#ifdef BL_USE_APP_INFO_V2
    if (E_OK == ret) {
      BL_FLS_INIT();
      BL_FLS_ERASE(blFingerPrintAddr,
                   FLASH_ALIGNED_ERASE_SIZE(blAppValidFlagAddr - blFingerPrintAddr));
      if (kFlashOk != blFlashParam.errorcode) {
        ret = E_NOT_OK;
      }
    }
#endif
    if (E_OK == ret) {
      bMiscMetaInfoInitDone = TRUE;
    }
  }
#endif
  return ret;
}
