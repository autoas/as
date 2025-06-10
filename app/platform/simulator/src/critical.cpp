/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "Std_Critical.h"
#include "Std_Compiler.h"
#include <mutex>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static std::recursive_mutex cirtical_mutex;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
imask_t Std_EnterCritical(void) {
  cirtical_mutex.lock();
  return 0;
}

void Std_ExitCritical(imask_t mask) {
  cirtical_mutex.unlock();
}
