/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "bl.h"
#include "RoD.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef Std_ReturnType (*BL_CheckProgramDependencyFncType)(uint8_t, uint8_t, uint8_t);

typedef struct {
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
} RoD_BLVersionMinType;

typedef struct {
  BL_CheckProgramDependencyFncType callout;
  RoD_BLVersionMinType BLVersionMin;
} RoD_ProgramDependencyType;
/* ================================ [ DECLARES  ] ============================================== */
extern const uint32_t blFingerPrintAddr;
extern const uint32_t blAppValidFlagAddr;
extern const RoD_ConfigType *const RoD_AppConfig;
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

Std_ReturnType BL_UserCheckProgrammingDependencies(void) {
  Std_ReturnType ret = E_OK;
#ifdef ROD_NUMBER_PROGRAM_DEPENDENCY
  uint16_t size = 0;
  RoD_ProgramDependencyType *pProgDep = NULL;
  ret = Rod_ReadData(RoD_AppConfig, ROD_NUMBER_PROGRAM_DEPENDENCY, (const void **)&pProgDep, &size);
  if ((E_OK == ret) && (NULL != pProgDep) && (size == sizeof(RoD_ProgramDependencyType))) {
    if (BL_VERSION_MAJOR < pProgDep->BLVersionMin.major) {
      ret = E_NOT_OK;
    } else if ((BL_VERSION_MAJOR == pProgDep->BLVersionMin.major) &&
               (BL_VERSION_MINOR < pProgDep->BLVersionMin.minor)) {
      ret = E_NOT_OK;
    } else if ((BL_VERSION_MAJOR == pProgDep->BLVersionMin.major) &&
               (BL_VERSION_MINOR == pProgDep->BLVersionMin.minor) &&
               (BL_VERSION_PATCH < pProgDep->BLVersionMin.patch)) {
      ret = E_NOT_OK;
    } else if (NULL != pProgDep->callout) {
      ret = pProgDep->callout(BL_VERSION_MAJOR, BL_VERSION_MINOR, BL_VERSION_PATCH);
    } else {
      /* version is valid */
    }
  }
#endif
  return ret;
}
