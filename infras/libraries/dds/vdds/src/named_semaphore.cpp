/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "named_semaphore.hpp"
#include <fcntl.h>
#include "Std_Debug.h"
#include <time.h>
#include <assert.h>

namespace as {
namespace vdds {
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_NSEMW 2
#define AS_LOG_NSEME 3
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
NamedSemaphore::NamedSemaphore(std::string name, int value) : m_Name(name), m_Value(value) {
#if defined(_WIN32)
  m_Name = "Local/" + m_Name;
#endif
  assert(value >= 0);
}

NamedSemaphore::NamedSemaphore(std::string name) : m_Name(name) {
#if defined(_WIN32)
  m_Name = "Local/" + m_Name;
#endif
}

NamedSemaphore::~NamedSemaphore() {
  if (nullptr != m_Sem) {
    sem_close(m_Sem);
    if (m_Value >= 0) {
      sem_unlink(m_Name.c_str());
    }
  }
}

int NamedSemaphore::create() {
  int ret = 0;
  int oflag = O_RDWR;

  if (m_Value >= 0) {
    oflag |= O_CREAT | O_EXCL;
  }

  m_Sem = sem_open(m_Name.c_str(), oflag, 0600, m_Value);
  if (nullptr == m_Sem) {
    ASLOG(NSEME, ("can't create sem %s\n", m_Name.c_str()));
    ret = EPIPE;
  }

  return ret;
}

int NamedSemaphore::wait(uint32_t timeoutMs) {
  int ret = 0;
  struct timespec ts;

  if (timeoutMs != 0) {
    ret = clock_gettime(CLOCK_REALTIME, &ts);
    if (0 == ret) {
      ts.tv_sec += timeoutMs / 1000;
      ts.tv_nsec += (timeoutMs % 1000) * 1000000;
      ret = sem_timedwait(m_Sem, &ts);
    }

    if (0 != ret) {
      ASLOG(NSEME, ("sem %s wait error %d\n", m_Name.c_str(), ret));
    }
  }

  return ret;
}

int NamedSemaphore::post() {
  int ret = 0;

  ret = sem_post(m_Sem);
  if (0 != ret) {
    ASLOG(NSEME, ("sem %s: sem post failed: %d\n", m_Name.c_str(), ret));
  }

  return ret;
}
} // namespace vdds
} // namespace as
