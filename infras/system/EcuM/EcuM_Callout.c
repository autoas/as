/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "EcuM.h"
#include "EcuM_Priv.h"
#include "EcuM_Externals.h"
#include "EcuM_Cfg.h"

#ifdef ENABLE_NVM_TEST
#undef USE_DEM
#endif

#ifdef USE_TRACE_APP
#include "TraceApp_Cfg.h"
#else
#define STD_TRACE_APP(ev)
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const EcuM_ConfigType EcuM_Config;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void EcuM_MemoryService(void) {
  uint16_t i;
  EcuM_DriverMainFncType pDriverMainFnc;

  for (i = 0; i < EcuM_Config.numOfMainMem; i++) {
    pDriverMainFnc = EcuM_Config.DriverMemMainList[i];
    pDriverMainFnc();
  }
}

void EcuM_AL_SetProgrammableInterrupts(void) {
}

void EcuM_AL_DriverInitZero(void) {
  uint16_t i;
  const EcuM_DriverInitItemType *pInitItem;

  for (i = 0; i < EcuM_Config.numOfZero; i++) {
    pInitItem = &EcuM_Config.DriverInitListZero[i];
    pInitItem->InitFnc(pInitItem->config);
  }

#ifdef USE_DEM
  Dem_PreInit();
#endif
}

void EcuM_AL_DriverInitOne(void) {
  uint16_t i;
  const EcuM_DriverInitItemType *pInitItem;

  for (i = 0; i < EcuM_Config.numOfOne; i++) {
    pInitItem = &EcuM_Config.DriverInitListOne[i];
    pInitItem->InitFnc(pInitItem->config);
  }
}

void EcuM_AL_DriverInitTwo(void) {
  uint16_t i;
  const EcuM_DriverInitItemType *pInitItem;
#if defined(USE_TRACE_APP) && defined(USE_CANIF)
  CanIf_SetControllerMode(0, CAN_CS_STARTED);
#endif
  STD_TRACE_APP(MEMORY_TASK_B);
#ifdef USE_NVM
  while (MEMIF_IDLE != NvM_GetStatus()) {
    EcuM_MemoryService();
  }
  NvM_ReadAll();
  while (MEMIF_IDLE != NvM_GetStatus()) {
    EcuM_MemoryService();
  }
#endif
  STD_TRACE_APP(MEMORY_TASK_E);

  for (i = 0; i < EcuM_Config.numOfTwo; i++) {
    pInitItem = &EcuM_Config.DriverInitListTwo[i];
    pInitItem->InitFnc(pInitItem->config);
  }
}

void EcuM_BswService(void) {
  uint16_t i;
  EcuM_DriverMainFncType pDriverMainFnc;

  for (i = 0; i < EcuM_Config.numOfMain; i++) {
    pDriverMainFnc = EcuM_Config.DriverMainList[i];
    pDriverMainFnc();
  }
}

void EcuM_BswServiceFast(void) {
  uint16_t i;
  EcuM_DriverMainFncType pDriverMainFnc;

  for (i = 0; i < EcuM_Config.numOfMainFast; i++) {
    pDriverMainFnc = EcuM_Config.DriverMainFastList[i];
    pDriverMainFnc();
  }

  STD_TRACE_APP(MEMORY_TASK_B);
  EcuM_MemoryService();
  STD_TRACE_APP(MEMORY_TASK_E);
}
