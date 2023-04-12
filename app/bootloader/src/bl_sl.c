/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "bl.h"
/* ================================ [ MACROS    ] ============================================== */
#define BL_SECURITY_LEVEL_EXTDS DCM_SEC_LEVEL1
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static uint32_t bl_prgs_seed = 0xdeadbeef;
static uint32_t bl_extds_seed = 0xbeafdada;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType BL_GetSessionChangePermission(Dcm_SesCtrlType sesCtrlTypeActive,
                                             Dcm_SesCtrlType sesCtrlTypeNew,
                                             Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType ercd = E_OK;
  ASLOG(BL, ("BL_GetSessionChangePermission(%d --> %d)\n", (int)sesCtrlTypeActive,
             (int)sesCtrlTypeNew));

  /* program session can only be entered through EXTDS session */
  if ((DCM_PROGRAMMING_SESSION == sesCtrlTypeNew) &&
      (DCM_EXTENDED_DIAGNOSTIC_SESSION != sesCtrlTypeActive)) {
    *nrc = DCM_E_REQUEST_SEQUENCE_ERROR;
    ercd = E_NOT_OK;
  }
  if ((DCM_PROGRAMMING_SESSION == sesCtrlTypeNew) &&
      (DCM_EXTENDED_DIAGNOSTIC_SESSION == sesCtrlTypeActive)) {
    Dcm_SecLevelType level = DCM_SEC_LEV_LOCKED;
    (void)Dcm_GetSecurityLevel(&level);

    if (BL_SECURITY_LEVEL_EXTDS != level) {
      *nrc = DCM_E_SECUTITY_ACCESS_DENIED;
      ercd = E_NOT_OK;
    }
  }

  return ercd;
}

Std_ReturnType BL_GetProgramSessionSeed(uint8_t *seed, Dcm_NegativeResponseCodeType *errorCode) {
  uint32_t u32Seed; /* intentional not initialized to use the stack random value */
  uint32_t u32Time = Std_GetTime();

  bl_prgs_seed = bl_prgs_seed ^ u32Seed ^ u32Time ^ 0xfeedbeef;

  ASLOG(BL, ("BL_GetProgramSessionSeed(seed = %X)\n", bl_prgs_seed));

  seed[0] = (uint8_t)(bl_prgs_seed >> 24);
  seed[1] = (uint8_t)(bl_prgs_seed >> 16);
  seed[2] = (uint8_t)(bl_prgs_seed >> 8);
  seed[3] = (uint8_t)(bl_prgs_seed);
  return E_OK;
}

Std_ReturnType BL_CompareProgramSessionKey(uint8_t *key, Dcm_NegativeResponseCodeType *errorCode) {
  Std_ReturnType ercd;
  uint32_t u32Key = ((uint32_t)key[0] << 24) + ((uint32_t)key[1] << 16) + ((uint32_t)key[2] << 8) +
                    ((uint32_t)key[3]);
  uint32_t u32KeyExpected = bl_prgs_seed ^ 0x94586792;

  ASLOG(BL, ("BL_CompareProgramSessionKey(key = %X(%X))\n", u32Key, u32KeyExpected));

  if (u32KeyExpected == u32Key) {
    ercd = E_OK;
  } else {
    *errorCode = DCM_E_INVALID_KEY;
    ercd = E_NOT_OK;
  }
  return ercd;
}

Std_ReturnType BL_GetExtendedSessionSeed(uint8_t *seed, Dcm_NegativeResponseCodeType *errorCode) {
  uint32_t u32Seed; /* intentional not initialized to use the stack random value */
  uint32_t u32Time = Std_GetTime();

  bl_extds_seed = bl_extds_seed ^ u32Seed ^ u32Time ^ 0x95774321;

  ASLOG(BL, ("BL_GetExtendedSessionSeed(seed = %X)\n", bl_extds_seed));

  seed[0] = (uint8_t)(bl_extds_seed >> 24);
  seed[1] = (uint8_t)(bl_extds_seed >> 16);
  seed[2] = (uint8_t)(bl_extds_seed >> 8);
  seed[3] = (uint8_t)(bl_extds_seed);

  return E_OK;
}

Std_ReturnType BL_CompareExtendedSessionKey(uint8_t *key, Dcm_NegativeResponseCodeType *errorCode) {
  Std_ReturnType ercd;
  uint32_t u32Key = ((uint32_t)key[0] << 24) + ((uint32_t)key[1] << 16) + ((uint32_t)key[2] << 8) +
                    ((uint32_t)key[3]);
  uint32_t u32KeyExpected = bl_extds_seed ^ 0x78934673;

  ASLOG(BL, ("BL_CompareExtendedSessionKey(key = %X(%X))\n", u32Key, u32KeyExpected));

  if (u32KeyExpected == u32Key) {
    ercd = E_OK;
  } else {
    *errorCode = DCM_E_INVALID_KEY;
    ercd = E_NOT_OK;
  }
  return ercd;
}

void Dcm_SessionChangeIndication(Dcm_SesCtrlType sesCtrlTypeActive, Dcm_SesCtrlType sesCtrlTypeNew,
                                 boolean timeout) {
  BL_SessionReset();
  if (timeout) {
    if ((DCM_EXTENDED_DIAGNOSTIC_SESSION == sesCtrlTypeActive) ||
        (DCM_PROGRAMMING_SESSION == sesCtrlTypeActive)) {
      Dcm_PerformReset(DCM_WARM_START);
    }
  } else if ((DCM_PROGRAMMING_SESSION == sesCtrlTypeActive) &&
             (DCM_DEFAULT_SESSION == sesCtrlTypeNew)) {
    Dcm_PerformReset(DCM_WARM_START);
  } else {
  }
}

Std_ReturnType BL_GetEcuResetPermission(Dcm_OpStatusType OpStatus,
                                        Dcm_NegativeResponseCodeType *ErrorCode) {
  return E_OK;
}
