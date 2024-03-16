/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef _VRING_DDS_NAMED_SEMAPHORE_HPP_
#define _VRING_DDS_NAMED_SEMAPHORE_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include <semaphore.h>
#include <string>
#include <stdint.h>

namespace as {
namespace vdds {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class NamedSemaphore {
public:
  NamedSemaphore(std::string name, int value);
  NamedSemaphore(std::string name);
  ~NamedSemaphore();

  int create();
  int wait(uint32_t timeoutMs);
  int post();

private:
  sem_t *m_Sem = nullptr;
  std::string m_Name;
  int m_Value = -1;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace vdds
} // namespace as
#endif /* _VRING_DDS_NAMED_SEMAPHORE_HPP_ */
