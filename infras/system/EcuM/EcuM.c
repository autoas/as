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
  EcuM_Contxt.state = ECUM_STATE_STARTUP_ONE;
}

void EcuM_StartupTwo(void) {
  EcuM_AL_DriverInitTwo();
  EcuM_Contxt.state = ECUM_STATE_STARTUP_TWO;
}

Std_ReturnType EcuM_RequestRUN(EcuM_UserType user) {
  Std_ReturnType ret = E_OK;
  (void)user;

  EcuM_Contxt.bRequestRun = TRUE;

  return ret;
}

Std_ReturnType EcuM_ReleaseRUN(EcuM_UserType user) {
  Std_ReturnType ret = E_OK;
  (void)user;

  EcuM_Contxt.bRequestRun = FALSE;

  return ret;
}

Std_ReturnType EcuM_RequestPOST_RUN(EcuM_UserType user) {
  Std_ReturnType ret = E_OK;
  (void)user;

  EcuM_Contxt.bRequestPostRun = TRUE;

  return ret;
}

Std_ReturnType EcuM_ReleasePOST_RUN(EcuM_UserType user) {
  Std_ReturnType ret = E_OK;
  (void)user;

  EcuM_Contxt.bRequestPostRun = FALSE;

  return ret;
}

void EcuM_MainFunction(void) {
  if (TRUE == EcuM_Contxt.bRequestRun) {
    if (ECUM_STATE_RUN != EcuM_Contxt.state) {
      ASLOG(ECUM, ("Enter Run\n"));
      EcuM_AL_EnterRUN();
      EcuM_Contxt.state = ECUM_STATE_RUN;
    }
  } else if (TRUE == EcuM_Contxt.bRequestPostRun) {
    if (ECUM_STATE_POST_RUN != EcuM_Contxt.state) {
      ASLOG(ECUM, ("Enter Post Run\n"));
      EcuM_AL_EnterPOST_RUN();
      EcuM_Contxt.state = ECUM_STATE_POST_RUN;
    }
  } else {
    if (EcuM_Contxt.Twakeup > 0) {
      EcuM_Contxt.Twakeup--;
    } else if (ECUM_STATE_SLEEP != EcuM_Contxt.state) {
      ASLOG(ECUM, ("Enter Sleep\n"));
      EcuM_Contxt.state = ECUM_STATE_SLEEP;
      EcuM_AL_EnterSLEEP();
      ASLOG(ECUM, ("Enter Wakeup\n"));
      EcuM_Contxt.Twakeup = EcuM_Config.TMinWakup;
      EcuM_Contxt.state = ECUM_STATE_WAKEUP;
    } else {
      /* do nothing */
    }
  }
}
