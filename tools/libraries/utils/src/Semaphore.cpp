/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Semaphore.hpp"

#if defined(_MSC_VER)
#include <windows.h>
#include <shlwapi.h>
#else
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#endif

#include <assert.h>
namespace as {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Semaphore::Semaphore(int value) {
#if defined(_MSC_VER)
  m_Sem = (void *)CreateSemaphoreA(NULL, value, LONG_MAX, NULL);
#else
  m_Sem = new sem_t;
  int r = sem_init((sem_t *)m_Sem, 0, value);
  assert(0 == r);
#endif

  assert(nullptr != m_Sem);
}

Semaphore::~Semaphore() {
  if (nullptr != m_Sem) {
#if defined(_MSC_VER)
    (void)CloseHandle((HANDLE)m_Sem);
#else
    delete (sem_t *)m_Sem;
#endif
  }
}

int Semaphore::wait(uint32_t timeoutMs) {
  int ret = 0;
#if defined(_MSC_VER)
  if (UINT32_MAX == timeoutMs) { /* wait forever */
    ret = WaitForSingleObject((HANDLE)m_Sem, INFINITE);
  } else {
    ret = WaitForSingleObject((HANDLE)m_Sem, timeoutMs);
  }
#else
  if (UINT32_MAX == timeoutMs) { /* wait forever */
    ret = sem_wait((sem_t *)m_Sem);
  } else if (0 == timeoutMs) {
    ret = sem_trywait((sem_t *)m_Sem);
  } else {
    struct timespec ts;
    (void)clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeoutMs / 1000;
    ts.tv_nsec += (timeoutMs % 1000) * 1000000;

    ret = sem_timedwait((sem_t *)m_Sem, &ts);
  }
#endif
  return ret;
}

int Semaphore::post() {
  int ret = 0;
#if defined(_MSC_VER)
  ret = ReleaseSemaphore((HANDLE)m_Sem, 1, NULL);
#else
  ret = sem_post((sem_t *)m_Sem);
#endif

  return ret;
}
} // namespace as
