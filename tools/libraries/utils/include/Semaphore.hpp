/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef SEMAPHORE_HPP
#define SEMAPHORE_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>

namespace as {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class Semaphore {
public:
  Semaphore(int value = 0);
  ~Semaphore();

  int Wait(uint32_t timeoutMs = UINT32_MAX);
  int Post(void);

private:
  void *m_Sem = nullptr;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace as
#endif /* SEMAPHORE_HPP */
