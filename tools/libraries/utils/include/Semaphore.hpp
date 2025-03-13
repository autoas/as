/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef _SEMAPHORE_HPP_
#define _SEMAPHORE_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>

namespace as {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class Semaphore {
public:
  Semaphore(int value = 0);
  ~Semaphore();

  int wait(uint32_t timeoutMs = UINT32_MAX);
  int post();

private:
  void *m_Sem = nullptr;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace as
#endif /* _SEMAPHORE_HPP_ */
