/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "EcuM.h"
#include "EcuM_Externals.h"
#include "EcuM_Priv.h"
#include "Std_Debug.h"
#include <string.h>

#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#if defined(_WIN32) || defined(linux)
#define AS_LOG_ECUM 1
#else
#define AS_LOG_ECUM 0
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const EcuM_ConfigType EcuM_Config;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static EcuM_ContextType EcuM_Contxt;
/* ================================ [ FUNCTIONS ] ============================================== */
void EcuM_Init(void) {
  memset(&EcuM_Contxt, 0, sizeof(EcuM_ContextType));
  /* @SWS_EcuM_02411 */
  EcuM_AL_SetProgrammableInterrupts();
  EcuM_AL_DriverInitZero();
  EcuM_AL_DriverInitOne();
  /* No OS support in EcuM */
  EcuM_Contxt.State = ECUM_STATE_STARTUP_ONE;
}

void EcuM_StartupTwo(void) {
  EcuM_AL_DriverInitTwo();
  EcuM_Contxt.State = ECUM_STATE_STARTUP_TWO;
}

Std_ReturnType EcuM_RequestRUN(EcuM_UserType user) {
  Std_ReturnType ret = E_OK;
  (void)user;

  EcuM_Contxt.RequestRun = TRUE;

  return ret;
}

Std_ReturnType EcuM_ReleaseRUN(EcuM_UserType user) {
  Std_ReturnType ret = E_OK;
  (void)user;

  EcuM_Contxt.RequestRun = FALSE;

  return ret;
}

Std_ReturnType EcuM_RequestPOST_RUN(EcuM_UserType user) {
  Std_ReturnType ret = E_OK;
  (void)user;

  EcuM_Contxt.RequestPostRun = TRUE;

  return ret;
}

Std_ReturnType EcuM_ReleasePOST_RUN(EcuM_UserType user) {
  Std_ReturnType ret = E_OK;
  (void)user;

  EcuM_Contxt.RequestPostRun = FALSE;

  return ret;
}

void EcuM_MainFunction(void) {
  if (TRUE == EcuM_Contxt.RequestRun) {
    if (ECUM_STATE_RUN != EcuM_Contxt.State) {
      ASLOG(ECUM, ("Enter Run\n"));
      EcuM_AL_EnterRUN();
      EcuM_Contxt.State = ECUM_STATE_RUN;
    }
  } else if (TRUE == EcuM_Contxt.RequestPostRun) {
    if (ECUM_STATE_POST_RUN != EcuM_Contxt.State) {
      ASLOG(ECUM, ("Enter Post Run\n"));
      EcuM_AL_EnterPOST_RUN();
      EcuM_Contxt.State = ECUM_STATE_POST_RUN;
    }
  } else {
    if (EcuM_Contxt.Twakeup > 0) {
      EcuM_Contxt.Twakeup--;
    } else if (ECUM_STATE_SLEEP != EcuM_Contxt.State) {
      ASLOG(ECUM, ("Enter Sleep\n"));
      EcuM_Contxt.State = ECUM_STATE_SLEEP;
      EcuM_AL_EnterSLEEP();
      ASLOG(ECUM, ("Enter Wakeup\n"));
      EcuM_Contxt.Twakeup = EcuM_Config.TMinWakup;
      EcuM_Contxt.State = ECUM_STATE_WAKEUP;
    } else {
      /* do nothing */
    }
  }
}

void EcuM_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x00, ECUM_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_ECUM;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 0;
}
