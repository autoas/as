/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "osal.h"
#include "Std_Types.h"
#include <stdlib.h>
#include <thread>
#include <assert.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
OSAL_ThreadType OSAL_ThreadCreate(OSAL_ThreadEntryType entry, void *args) {
  OSAL_ThreadType thread = NULL;

  thread = (OSAL_ThreadType) new std::thread(entry, args);

  return thread;
}

int OSAL_ThreadJoin(OSAL_ThreadType thread) {
  std::thread *th = (std::thread *)thread;

  if (th->joinable()) {
    th->join();
  }

  return 0;
}

int OSAL_ThreadDestory(OSAL_ThreadType thread) {
  std::thread *th = (std::thread *)thread;

  if (th->joinable()) {
    th->join();
  }

  delete th;
  return 0;
}