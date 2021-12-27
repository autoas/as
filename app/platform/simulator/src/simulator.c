/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Mcu.h"
#include "Std_Timer.h"
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include "Std_Types.h"
#include "Std_Debug.h"
#include "Std_Critical.h"
#include <pthread.h>
#include "Dcm.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static pthread_mutex_t cirtical_mutex;
/* ================================ [ LOCALS    ] ============================================== */
static void __attribute__((constructor)) __CriticalInit(void) {
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&cirtical_mutex, &attr);
}
/* ================================ [ FUNCTIONS ] ============================================== */
std_time_t Std_GetTime(void) {
  struct timeval now;
  std_time_t tm;

  (void)gettimeofday(&now, NULL);
  tm = (std_time_t)(now.tv_sec * 1000000 + now.tv_usec);

  return tm;
}

void Mcu_Init(const Mcu_ConfigType *ConfigPtr) {
  (void)ConfigPtr;
}

void Dcm_PerformReset(uint8_t resetType) {
  ASLOG(INFO, ("dummy perform reset %d\n", resetType));
}

boolean BL_IsUpdateRequested(void) {
  return FALSE;
}

void BL_JumpToApp(void) {
}

void BL_AliveIndicate(void) {
}

void App_AliveIndicate(void) {
}

void App_EnterProgramSession(void) {
}

imask_t Std_EnterCritical(void) {
  pthread_mutex_lock(&cirtical_mutex);
  return 0;
}

void Std_ExitCritical(imask_t mask) {
  pthread_mutex_unlock(&cirtical_mutex);
}

Std_ReturnType BL_GetProgramCondition(Dcm_ProgConditionsType **cond) {
  return E_NOT_OK;
}

void User_Init(void) {
}

void User_MainTask10ms(void) {
}
