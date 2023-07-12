/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#ifdef USE_DCM
#include "Dcm.h"
#include "Dem.h"
#include "Std_Timer.h"
#include "Std_Debug.h"
#include <string.h>
#if defined(_WIN32)
#include <time.h>
#endif
/* ================================ [ MACROS    ] ============================================== */
#define App_SECURITY_LEVEL_EXTDS DCM_SEC_LEVEL1

#define TO_BCD(v) (((v) / 10) * 16 + ((v) % 10))
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void App_EnterProgramSession(void);
/* ================================ [ DATAS     ] ============================================== */
static uint32_t app_prgs_seed = 0xdeadbeef;
static uint32_t app_extds_seed = 0xbeafdada;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType App_GetSessionChangePermission(Dcm_SesCtrlType sesCtrlTypeActive,
                                              Dcm_SesCtrlType sesCtrlTypeNew,
                                              Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType ercd = E_OK;
  ASLOG(INFO, ("App_GetSessionChangePermission(%d --> %d)\n", (int)sesCtrlTypeActive,
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

    if (App_SECURITY_LEVEL_EXTDS != level) {
      *nrc = DCM_E_SECUTITY_ACCESS_DENIED;
      ercd = E_NOT_OK;
    }
  }

  if (E_OK == ercd) {
    /* do nothing */
  }

  return ercd;
}

Std_ReturnType App_GetProgramSessionSeed(uint8_t *seed, Dcm_NegativeResponseCodeType *errorCode) {
  uint32_t u32Seed; /* intentional not initialized to use the stack random value */
  uint32_t u32Time = Std_GetTime();

  app_prgs_seed = app_prgs_seed ^ u32Seed ^ u32Time ^ 0xfeedbeef;

  ASLOG(INFO, ("App_GetProgramSessionSeed(seed = %X)\n", app_prgs_seed));

  seed[0] = (uint8_t)(app_prgs_seed >> 24);
  seed[1] = (uint8_t)(app_prgs_seed >> 16);
  seed[2] = (uint8_t)(app_prgs_seed >> 8);
  seed[3] = (uint8_t)(app_prgs_seed);
  return E_OK;
}

Std_ReturnType App_CompareProgramSessionKey(uint8_t *key, Dcm_NegativeResponseCodeType *errorCode) {
  Std_ReturnType ercd;
  uint32_t u32Key = ((uint32_t)key[0] << 24) + ((uint32_t)key[1] << 16) + ((uint32_t)key[2] << 8) +
                    ((uint32_t)key[3]);
  uint32_t u32KeyExpected = app_prgs_seed ^ 0x94586792;

  ASLOG(INFO, ("App_CompareProgramSessionKey(key = %X(%X))\n", u32Key, u32KeyExpected));

  if (u32KeyExpected == u32Key) {
    ercd = E_OK;
  } else {
    *errorCode = DCM_E_INVALID_KEY;
    ercd = E_NOT_OK;
  }
  return ercd;
}

Std_ReturnType App_GetExtendedSessionSeed(uint8_t *seed, Dcm_NegativeResponseCodeType *errorCode) {
  uint32_t u32Seed; /* intentional not initialized to use the stack random value */
  uint32_t u32Time = Std_GetTime();

  app_extds_seed = app_extds_seed ^ u32Seed ^ u32Time ^ 0x95774321;

  ASLOG(INFO, ("App_GetExtendedSessionSeed(seed = %X)\n", app_extds_seed));

  seed[0] = (uint8_t)(app_extds_seed >> 24);
  seed[1] = (uint8_t)(app_extds_seed >> 16);
  seed[2] = (uint8_t)(app_extds_seed >> 8);
  seed[3] = (uint8_t)(app_extds_seed);

  return E_OK;
}

Std_ReturnType App_CompareExtendedSessionKey(uint8_t *key,
                                             Dcm_NegativeResponseCodeType *errorCode) {
  Std_ReturnType ercd;
  uint32_t u32Key = ((uint32_t)key[0] << 24) + ((uint32_t)key[1] << 16) + ((uint32_t)key[2] << 8) +
                    ((uint32_t)key[3]);
  uint32_t u32KeyExpected = app_extds_seed ^ 0x78934673;

  ASLOG(INFO, ("App_CompareExtendedSessionKey(key = %X(%X))\n", u32Key, u32KeyExpected));

  if (u32KeyExpected == u32Key) {
    ercd = E_OK;
  } else {
    *errorCode = DCM_E_INVALID_KEY;
    ercd = E_NOT_OK;
  }
  return ercd;
}
#ifdef USE_DEM
Std_ReturnType Dem_FFD_GetBattery(Dem_EventIdType EventId, uint8_t *data,
                                  Dem_DTCOriginType DTCOrigin) {
  return E_OK;
}

Std_ReturnType Dem_FFD_GetVehileSpeed(Dem_EventIdType EventId, uint8_t *data,
                                      Dem_DTCOriginType DTCOrigin) {
  return E_OK;
}

Std_ReturnType Dem_FFD_GetEngineSpeed(Dem_EventIdType EventId, uint8_t *data,
                                      Dem_DTCOriginType DTCOrigin) {
  return E_OK;
}

Std_ReturnType Dem_FFD_GetTime(Dem_EventIdType EventId, uint8_t *data,
                               Dem_DTCOriginType DTCOrigin) {
#if defined(_WIN32)
  time_t t = time(0);
  struct tm *lt = localtime(&t);
  data[0] = TO_BCD((1900 + lt->tm_year) - 2000);
  data[1] = TO_BCD(lt->tm_mon + 1);
  data[2] = TO_BCD(lt->tm_mday);
  data[3] = TO_BCD(lt->tm_hour);
  data[4] = TO_BCD(lt->tm_min);
  data[5] = TO_BCD(lt->tm_sec);
#endif
  return E_OK;
}
#endif
void Dcm_SessionChangeIndication(Dcm_SesCtrlType sesCtrlTypeActive, Dcm_SesCtrlType sesCtrlTypeNew,
                                 boolean timeout) {
  if (DCM_PROGRAMMING_SESSION == sesCtrlTypeNew) {
    App_EnterProgramSession();
  }
}

Std_ReturnType App_ReadFingerPrint(Dcm_OpStatusType opStatus, uint8_t *data, uint16_t length,
                                   Dcm_NegativeResponseCodeType *errorCode) {
  memset(data, 0xA5, length);
  return E_OK;
}

Std_ReturnType App_ReadAB01(Dcm_OpStatusType opStatus, uint8_t *data, uint16_t length,
                            Dcm_NegativeResponseCodeType *errorCode) {
  int i;
  for (i = 0; i < length; i++) {
    data[i] = 0x10 + i;
  }
  return E_OK;
}

Std_ReturnType App_ReadAB02(Dcm_OpStatusType opStatus, uint8_t *data, uint16_t length,
                            Dcm_NegativeResponseCodeType *errorCode) {
  int i;
  for (i = 0; i < length; i++) {
    data[i] = 0x20 + i;
  }
  return E_OK;
}

Std_ReturnType App_GetPeriodicDID01(Dcm_OpStatusType opStatus, uint8_t *data, uint16_t length,
                                    Dcm_NegativeResponseCodeType *errorCode) {
  int i;
  for (i = 0; i < length; i++) {
    data[i] = 0xA0 + i;
  }
  return E_OK;
}

Std_ReturnType App_GetPeriodicDID02(Dcm_OpStatusType opStatus, uint8_t *data, uint16_t length,
                                    Dcm_NegativeResponseCodeType *errorCode) {
  int i;
  for (i = 0; i < length; i++) {
    data[i] = 0xB0 + i;
  }
  return E_OK;
}

Std_ReturnType App_GetEcuResetPermission(Dcm_OpStatusType OpStatus,
                                         Dcm_NegativeResponseCodeType *ErrorCode) {
  return E_OK;
}

Dcm_ReturnReadMemoryType Dcm_ReadMemory(Dcm_OpStatusType OpStatus, uint8_t MemoryIdentifier,
                                        uint32_t MemoryAddress, uint32_t MemorySize,
                                        uint8_t *MemoryData,
                                        Dcm_NegativeResponseCodeType *ErrorCode) {
  return E_OK;
}

Dcm_ReturnWriteMemoryType Dcm_WriteMemory(Dcm_OpStatusType OpStatus, uint8_t MemoryIdentifier,
                                          uint32_t MemoryAddress, uint32_t MemorySize,
                                          const uint8_t *MemoryData,
                                          Dcm_NegativeResponseCodeType *ErrorCode) {
  return E_OK;
}

Std_ReturnType App_IOCtl_FC01_ShortTermAdjustment(uint8_t *ControlRecord, uint16_t length,
                                                  uint8_t *resData, uint16_t *resDataLen,
                                                  uint8_t *nrc) {
  *resDataLen = 3;
  memset(resData, 0xA5, 3);
  return E_OK;
}

Std_ReturnType App_IOCtl_FC01_ReturnControlToEcuFnc(uint8_t *ControlRecord, uint16_t length,
                                                    uint8_t *resData, uint16_t *resDataLen,
                                                    uint8_t *nrc) {
  *resDataLen = 8;
  memset(resData, 0x88, 8);
  return E_OK;
}
#endif /* USE_DCM */
