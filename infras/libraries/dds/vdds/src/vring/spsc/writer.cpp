/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "vring/spsc/writer.hpp"
#include "Std_Debug.h"
#include <assert.h>
#include <cinttypes>
#include <fcntl.h>
#include <unistd.h>

namespace as {
namespace vdds {
namespace vring {
namespace spsc {
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_VRSPSC 0
#define AS_LOG_VRSPSCI 1
#define AS_LOG_VRSPSCW 2
#define AS_LOG_VRSPSCE 3
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Writer::Writer(std::string name, uint32_t msgSize, uint32_t numDesc)
  : Base(name, numDesc), m_MsgSize(msgSize) {
}

int Writer::init() {
  int ret = 0;

  auto sharedMemory = std::make_shared<SharedMemory>(m_Name, size());
  if (nullptr == sharedMemory) {
    ret = ENOMEM;
  } else {
    ret = sharedMemory->create();
  }

  if (0 == ret) {
    m_SharedMemory = sharedMemory;
    m_Meta = (VRing_MetaType *)m_SharedMemory->getVA();
    m_Desc = (VRing_DescType *)(((uintptr_t)m_Meta) + VRING_SIZE_OF_META());
    m_Avail = (VRing_AvailType *)(((uintptr_t)m_Desc) + VRING_SIZE_OF_DESC(m_NumDesc));
    m_Used = (VRing_UsedType *)(((uintptr_t)m_Avail) + VRING_SIZE_OF_AVAIL(m_NumDesc));
    ret = setup();
  } else {
    ASLOG(VRSPSCE, ("vring writer can't open shm %s\n", m_Name.c_str()));
  }

  if (0 == ret) {
    m_SemAvail = std::make_shared<NamedSemaphore>(m_Name, m_NumDesc);
    if (nullptr == m_SemAvail) {
      ret = ENOMEM;
    } else {
      ret = m_SemAvail->create();
    }
  }

  if (0 == ret) {

    std::string semName = m_Name + "_used";
    auto sem = std::make_shared<NamedSemaphore>(semName, 0);
    if (nullptr == sem) {
      ret = ENOMEM;
    } else {
      ret = sem->create();
      if (0 == ret) {
        m_SemUsed = sem;
      }
    }
  }

  if (0 == ret) {
    ASLOG(VRSPSC, ("vring writer %s online: msgSize = %u,  numDesc = %u\n", m_Name.c_str(),
                   m_MsgSize, m_NumDesc));
    m_Thread = std::thread(&Writer::threadMain, this);
  }

  return ret;
}

Writer::~Writer() {
  m_Stop = true;
  if (m_Thread.joinable()) {
    m_Thread.join();
  }

  m_SemAvail = nullptr;
  m_SemUsed = nullptr;
  m_SharedMemory = nullptr;
  m_DmaMems.clear();
}

int Writer::setup() {
  uint32_t i;
  int ret = 0;

  memset(m_SharedMemory->getVA(), 0, size());
  m_Meta->msgSize = m_MsgSize;
  m_Meta->numDesc = m_NumDesc;
  for (i = 0; (i < m_NumDesc) && (0 == ret); i++) {
    std::string shmFile = m_Name + "_" + std::to_string(i) + "_" + std::to_string(m_MsgSize);
    auto dmaMemory = std::make_shared<DmaMemory>(shmFile, m_MsgSize);
    if (nullptr != dmaMemory) {
      ret = dmaMemory->create();
      if (0 == ret) {
#ifdef USE_DMA_BUF
        m_Desc[i].handle = dmaMemory->getHandle();
#else
        m_Desc[i].handle = i;
#endif
        m_Desc[i].len = m_MsgSize;
        m_Avail->ring[i] = i;
        m_Avail->idx++;
        m_DmaMems.push_back(dmaMemory);
      }
    } else {
      ret = ENOMEM;
    }
  }

  return ret;
}

void Writer::resetIfNeed() {
  if (m_Reset) {
    for (uint32_t i = 0; i < m_NumDesc; i++) {
      m_Avail->ring[i] = i;
    }
    m_Avail->lastIdx = 0;
    m_Avail->idx = m_NumDesc;
    m_Reset = false;
  }
}

int Writer::get(void *&buf, uint32_t &idx, uint32_t &len, uint32_t timeoutMs) {
  int ret = 0;

  resetIfNeed();
  (void)m_SemAvail->wait(timeoutMs);

  if (m_Avail->lastIdx == m_Avail->idx) {
    /* no buffers */
    ret = ENODATA;
  } else {
    idx = m_Avail->ring[m_Avail->lastIdx % m_NumDesc];
    buf = m_DmaMems[idx]->getVA();
    len = m_Desc[idx].len;
    m_Avail->lastIdx++;
    ASLOG(VRSPSC, ("vring writer %s: get DESC[%u], len = %u; AVAIL: lastIdx = %u, idx = %u\n",
                   m_Name.c_str(), idx, len, m_Avail->lastIdx, m_Avail->idx));
  }

  return ret;
}

int Writer::put(uint32_t idx, uint32_t len) {
  VRing_UsedElemType *usedElem;
  int ret = 0;

  if (idx > m_NumDesc) {
    ret = EINVAL;
  } else {
    /* state == 1, the used ring is in good status */
    if (VRING_USED_STATE_READY == __atomic_load_n(&m_Used->state, __ATOMIC_RELAXED)) {
      usedElem = &m_Used->ring[m_Used->idx % m_NumDesc];
      usedElem->id = idx;
      usedElem->len = len;
      ASLOG(VRSPSC, ("vring writer %s: put DESC[%u], len = %u; used: lastIdx = %u, idx = %u\n",
                     m_Name.c_str(), idx, len, m_Used->lastIdx, m_Used->idx));
      m_Used->idx++;
      ret = m_SemUsed->post();
    } else {
      /* OK, put it back */
      (void)drop(idx);
      ret = ENOLINK;
    }
  }

  return ret;
}

int Writer::drop(uint32_t idx) {
  int ret = 0;

  if (idx > m_NumDesc) {
    ret = EINVAL;
  } else {
    m_Avail->ring[m_Avail->idx % m_NumDesc] = idx;
    m_Avail->idx++;
    ASLOG(VRSPSC, ("vring writer %s: drop DESC[%u]; AVAIL: lastIdx = %u, idx = %u\n",
                   m_Name.c_str(), idx, m_Avail->lastIdx, m_Avail->idx));
    ret = m_SemAvail->post();
  }

  return ret;
}

void Writer::readerHeartCheck() {
  uint32_t curHeart;
  uint32_t ref;

  if (VRING_USED_STATE_READY == __atomic_load_n(&m_Used->state, __ATOMIC_RELAXED)) {
    curHeart = __atomic_load_n(&m_Used->heart, __ATOMIC_RELAXED);
    if (curHeart == m_Used->lastHeart) { /* the reader is dead or stuck */
      ASLOG(VRSPSCE, ("vring reader %s is dead\n", m_Name.c_str()));
      m_Reset = true;
      ref = __atomic_sub_fetch(&m_Used->state, VRING_USED_STATE_READY, __ATOMIC_RELAXED);
      assert(VRING_USED_STATE_FREE == ref);
    } else {
      m_Used->lastHeart = curHeart;
    }
  }
}

void Writer::threadMain() {
  while (false == m_Stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    readerHeartCheck();
  }
}

} // namespace spsc
} // namespace vring
} // namespace vdds
} // namespace as