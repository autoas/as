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
#include <stdlib.h>
#include <unistd.h>
/* ================================ [ MACROS    ] ============================================== */
#if defined(_WIN32)
#define AS_BUILD_DIR "build/nt/GCC"
#define AS_APP_SUFFIX ".exe"
#else
#define AS_BUILD_DIR "build/posix/GCC"
#define AS_APP_SUFFIX ""
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
#ifdef USE_FLS
void FlsAc_Stop(void);
#endif
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
  boolean rv = FALSE;
  FILE *flagFile = fopen("build/bl_update_request.flag", "r");
  if (flagFile) {
    fclose(flagFile);
    rv = TRUE;
  }
  return rv;
}

void BL_JumpToApp(void) {
  char *appPath;
  char defaultPath[] = AS_BUILD_DIR "/CanApp/CanApp" AS_APP_SUFFIX;
  char *args[2] = {NULL};

  appPath = getenv("BL_APP_PATH");
  if (NULL == appPath) {
    appPath = defaultPath;
  }

  ASLOG(INFO, ("BL: jumping to %s\n", appPath));

  args[0] = appPath;

#ifdef USE_FLS
  FlsAc_Stop();
#endif
  execv(appPath, args);

  ASLOG(INFO, ("BL: failed to jump to %s, error: %d\n", appPath, errno));
}

void BL_AliveIndicate(void) {
}

void App_EnterProgramSession(void) {
  char *blPath;
  char defaultPath[] = AS_BUILD_DIR "/CanBL/CanBL" AS_APP_SUFFIX;
  char *args[2] = {NULL};
  FILE *flagFile;

  blPath = getenv("BL_PATH");
  if (NULL == blPath) {
    blPath = defaultPath;
  }

  flagFile = fopen("build/bl_update_request.flag", "w");
  if (flagFile) {
    fclose(flagFile);
  }

  ASLOG(INFO, ("App: requesting jump to BL: %s\n", blPath));

  args[0] = blPath;

#ifdef USE_FLS
  FlsAc_Stop();
#endif
  execv(blPath, args);

  ASLOG(INFO, ("App: failed to jump to %s, error: %d\n", blPath, errno));
}

Std_ReturnType BL_GetProgramCondition(Dcm_ProgConditionsType **cond) {
  Std_ReturnType r = E_NOT_OK;
  FILE *flagFile;

  static Dcm_ProgConditionsType s_cond;
  s_cond.ConnectionId = 0;
  s_cond.TesterAddress = 0;
  s_cond.Sid = 0x10;
  s_cond.SubFncId = 0x02;
  s_cond.Reprograming = TRUE;
  s_cond.ApplUpdated = TRUE;
  s_cond.ResponseRequired = TRUE;

  flagFile = fopen("build/bl_update_request.flag", "r");
  if (flagFile) {
    fclose(flagFile);
    remove("build/bl_update_request.flag");
    r = E_OK;
    *cond = &s_cond;
  }

  return r;
}
