/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef _VRING_SPMC_BASE_HPP_
#define _VRING_SPMC_BASE_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include "vring/base.hpp"

namespace as {
namespace vdds {
namespace vring {
namespace spmc {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint64_t timestamp; /* timestamp in microseconds when publish this DESC */
  uint64_t handle;    /* the virtual shared large memory handle */
  uint32_t len;
  int32_t spin; /* The spinlock to protect the ref and timestamp */
  int32_t ref;  /* The reference counter */
} VRing_DescType;

typedef struct {
  uint32_t lastIdx;
  int32_t spin; /* The spinlock to ensure the idx and ring content updated atomic */
  uint32_t idx;
  uint32_t ring[];
} VRing_AvailType;

class Base : public vring::Base {
public:
  Base(std::string name, uint32_t numDesc = 8);
  ~Base();

protected:
  uint32_t size();

protected:
  VRing_DescType *m_Desc = nullptr;
  VRing_AvailType *m_Avail = nullptr;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace spmc
} // namespace vring
} // namespace vdds
} // namespace as
#endif /* _VRING_SPMC_BASE_HPP_ */
