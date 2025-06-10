/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef _VRING_BASE_HPP_
#define _VRING_BASE_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include <array>
#include <chrono>
#include <cstring>
#include <errno.h>
#include <map>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <string>
#include <thread>
#include <vector>

#include "dma_memory.hpp"
#include "named_semaphore.hpp"
#include "shared_memory.hpp"

namespace as {
namespace vdds {
namespace vring {
/* ================================ [ MACROS    ] ============================================== */
#ifndef VRING_ALIGNMENT
#define VRING_ALIGNMENT 64
#endif

#ifndef VRING_MAX_READERS
#define VRING_MAX_READERS 8
#endif

#define VRING_ALIGN(sz) (((sz) + (VRING_ALIGNMENT)-1) & (~((VRING_ALIGNMENT)-1)))

#define VRING_SIZE_OF_META() VRING_ALIGN(sizeof(VRing_MetaType))

#define VRING_SIZE_OF_DESC(numDesc) VRING_ALIGN((sizeof(VRing_DescType)) * numDesc)

#define VRING_SIZE_OF_AVAIL(numDesc)                                                               \
  VRING_ALIGN((sizeof(VRing_AvailType) + sizeof(uint32_t) * numDesc) * numDesc)

#define VRING_SIZE_OF_USED(numDesc)                                                                \
  VRING_ALIGN((sizeof(VRing_UsedType) + sizeof(VRing_UsedElemType) * numDesc) * numDesc)

#define VRING_SIZE_OF_ALL_USED(numDesc)                                                            \
  (VRING_ALIGN((sizeof(VRing_UsedType) + sizeof(VRing_UsedElemType) * numDesc) * numDesc) *        \
   VRING_MAX_READERS)

#define VRING_USED_STATE_FREE 0
#define VRING_USED_STATE_INIT 1
#define VRING_USED_STATE_READY 2
#define VRING_USED_STATE_KILLED 3
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint32_t msgSize;
  uint32_t numDesc;
} VRing_MetaType;

typedef struct {
  uint32_t id;  /* Index of start of used descriptor chain. */
  uint32_t len; /* Total length of the descriptor chain which was used (written to) */
} VRing_UsedElemType;

typedef struct {
  int32_t state;  /* atomic used state: 0 : free, 1: init, 2: ready, 3: killed */
  uint32_t heart; /* atomic heart beat counter */
  uint32_t lastHeart;
  uint32_t lastIdx;
  uint32_t idx;
  VRing_UsedElemType ring[];
} VRing_UsedType;

class Base {
public:
  Base(std::string name, uint32_t numDesc = 8);
  ~Base();

  uint64_t timestamp();

protected:
  std::string replace(std::string resource_str, std::string sub_str, std::string new_str);
  std::string toAsName(std::string name);

  int spinLock(int32_t *pLock);
  void spinUnlock(int32_t *pLock);

protected:
  std::string m_Name;
  uint32_t m_NumDesc = 8;

  VRing_MetaType *m_Meta = nullptr;
  VRing_UsedType *m_Used = nullptr;

  std::shared_ptr<NamedSemaphore> m_SemAvail = nullptr;

  std::shared_ptr<SharedMemory> m_SharedMemory;
  std::vector<std::shared_ptr<DmaMemory>> m_DmaMems;

  bool m_Stop = false;
  std::thread m_Thread;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace vring
} // namespace vdds
} // namespace as
#endif /* _VRING_BASE_HPP_ */
