/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "vring/spmc/writer.hpp"
#include "Std_Debug.h"
#include <assert.h>
#include <cinttypes>
#include <fcntl.h>
#include <unistd.h>

namespace as {
namespace vdds {
namespace vring {
namespace spmc {
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_VRING 0
#define AS_LOG_VRINGI 1
#define AS_LOG_VRINGW 2
#define AS_LOG_VRINGE 3

#ifndef VRING_DESC_TIMEOUT
#define VRING_DESC_TIMEOUT (2000000)
#endif

#ifndef VRING_SPIN_MAX_COUNTER
#define VRING_SPIN_MAX_COUNTER (1000000)
#endif

/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Writer::Writer(std::string name, uint32_t msgSize, uint32_t numDesc)
  : Base(name, numDesc), m_MsgSize(msgSize) {
  m_SemUseds.reserve(numDesc);
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
    ASLOG(VRINGE, ("vring writer can't open shm %s\n", m_Name.c_str()));
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
    for (uint32_t i = 0; i < VRING_MAX_READERS; i++) {
      std::string semName = m_Name + "_used" + std::to_string(i);
      auto sem = std::make_shared<NamedSemaphore>(semName, 0);
      if (nullptr == sem) {
        ret = ENOMEM;
      } else {
        ret = sem->create();
        if (0 == ret) {
          m_SemUseds.push_back(sem);
        }
      }
    }
  }

  if (0 == ret) {
    ASLOG(VRING, ("vring writer %s online: msgSize = %u,  numDesc = %u\n", m_Name.c_str(),
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
  m_SemUseds.clear();
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

int Writer::get(void *&buf, uint32_t &idx, uint32_t &len, uint32_t timeoutMs) {
  int ret = 0;
  int32_t ref;

  (void)m_SemAvail->wait(timeoutMs);

  ret = spinLock(&m_Avail->spin);
  if (0 == ret) {
    if (m_Avail->lastIdx == m_Avail->idx) {
      /* no buffers */
      ret = ENODATA;
    } else {
      idx = m_Avail->ring[m_Avail->lastIdx % m_NumDesc];
      ref = __atomic_load_n(&m_Desc[idx].ref, __ATOMIC_RELAXED);
      if (0 == ref) {
        buf = m_DmaMems[idx]->getVA();
        len = m_Desc[idx].len;
        m_Avail->lastIdx++;
        ASLOG(VRING, ("vring writer %s: get DESC[%u], len = %u; AVAIL: lastIdx = %u, idx = %u\n",
                      m_Name.c_str(), idx, len, m_Avail->lastIdx, m_Avail->idx));
      } else {
        ASLOG(VRINGE, ("vring writer %s: get DESC[%u] with ref = %d\n", m_Name.c_str(), idx, ref));
        ret = EBADF;
      }
    }
    spinUnlock(&m_Avail->spin);
  } else {
    ASLOG(VRINGE, ("vring writer %s: get lock AVAIL spin timeout\n", m_Name.c_str()));
  }

  return ret;
}

int Writer::put(uint32_t idx, uint32_t len) {
  VRing_UsedType *used;
  VRing_UsedElemType *usedElem;
  uint32_t i;
  bool isUsed = false;
  int ret = 0;
  std::vector<uint32_t> readerIdxs;
  readerIdxs.reserve(VRING_MAX_READERS);

  if (idx > m_NumDesc) {
    ret = EINVAL;
  } else {
    ret = spinLock(&m_Desc[idx].spin);
    if (0 == ret) {
      m_Desc[idx].timestamp = timestamp();
      for (i = 0; i < VRING_MAX_READERS; i++) {
        used = (VRing_UsedType *)(((uintptr_t)m_Used) + VRING_SIZE_OF_USED(m_NumDesc) * i);
        /* state == 1, the used ring is in good status */
        if (VRING_USED_STATE_READY == __atomic_load_n(&used->state, __ATOMIC_RELAXED)) {
          usedElem = &used->ring[used->idx % m_NumDesc];
          usedElem->id = idx;
          usedElem->len = len;
          __atomic_fetch_add(&m_Desc[idx].ref, 1, __ATOMIC_RELAXED);
          isUsed = true;
          ASLOG(VRING,
                ("vring writer %s@%u: put DESC[%u], len = %u ref = %d; used: lastIdx = "
                 "%u, idx = %u\n",
                 m_Name.c_str(), i, idx, len, __atomic_load_n(&m_Desc[idx].ref, __ATOMIC_RELAXED),
                 used->lastIdx, used->idx));
          used->idx++;
          readerIdxs.push_back(i);
        }
      }
      spinUnlock(&m_Desc[idx].spin);
    } else {
      ASLOG(VRINGE, ("vring writer %s: put lock DESC[%u] spin timeout\n", m_Name.c_str(), idx));
    }

    if (false == isUsed) {
      /* OK, put it back */
      (void)drop(idx);
      ret = ENOLINK;
    } else {
      for (auto i : readerIdxs) {
        ret |= m_SemUseds[i]->post();
      }
    }
  }

  return ret;
}

int Writer::drop(uint32_t idx) {
  int ret = 0;

  if (idx > m_NumDesc) {
    ret = EINVAL;
  } else {
    ret = spinLock(&m_Avail->spin);
    if (0 == ret) {
      m_Avail->ring[m_Avail->idx % m_NumDesc] = idx;
      m_Avail->idx++;
      spinUnlock(&m_Avail->spin);
      ASLOG(VRING, ("vring writer %s: drop DESC[%u]; AVAIL: lastIdx = %u, idx = %u\n",
                    m_Name.c_str(), idx, m_Avail->lastIdx, m_Avail->idx));
      ret = m_SemAvail->post();
    } else {
      ASLOG(VRINGE, ("vring writer %s: drop lock AVAIL spin timeout\n", m_Name.c_str()));
    }
  }

  return ret;
}

void Writer::releaseDesc(uint32_t idx) {
  int32_t ref;
  int ret = 0;
  ref = __atomic_sub_fetch(&m_Desc[idx].ref, 1, __ATOMIC_RELAXED);
  if (0 < ref) {
    /* still used by others */
  } else if (0 == ref) {
    ret = spinLock(&m_Avail->spin);
    if (0 == ret) {
      m_Avail->ring[m_Avail->idx % m_NumDesc] = idx;
      m_Avail->idx++;
      spinUnlock(&m_Avail->spin);
      ASLOG(VRINGE, ("vring writer %s: release DESC[%u]\n", m_Name.c_str(), idx));
      (void)m_SemAvail->post();
    } else {
      ASLOG(VRINGE, ("vring writer %s: release lock AVAIL spin timeout\n", m_Name.c_str()));
    }
  } else {
    ASLOG(VRING, ("vring writer %s: release DESC[%u] ref = %d failed\n", m_Name.c_str(), idx, ref));
    assert(0);
  }
}

void Writer::removeAbnormalReader(VRing_UsedType *used, uint32_t readerIdx) {
  VRing_UsedElemType *usedElem;
  uint32_t ref;
  uint32_t idx;
  int ret = 0;

  ASLOG(VRINGE, ("vring reader %s@%u is dead\n", m_Name.c_str(), readerIdx));
  /* set ref > 1, mark as dead to stop the writer to put data on this used ring */
  /* step 1: release the DESC in the reader used ring */
  ref = __atomic_add_fetch(&used->state, 1, __ATOMIC_RELAXED);
  assert(VRING_USED_STATE_KILLED == ref);
  while (used->lastIdx != used->idx) {
    usedElem = &used->ring[used->lastIdx % m_NumDesc];
    idx = usedElem->id;
    ret = spinLock(&m_Desc[idx].spin);
    if (0 == ret) {
      releaseDesc(idx);
      spinUnlock(&m_Desc[idx].spin);
    } else {
      ASLOG(VRINGE,
            ("vring writer %s: rm reader lock DESC[%u] spin timeout\n", m_Name.c_str(), idx));
    }
    used->lastIdx++;
  }

  ref = __atomic_sub_fetch(&used->state, VRING_USED_STATE_KILLED, __ATOMIC_RELAXED);
  assert(VRING_USED_STATE_FREE == ref);
}

void Writer::readerHeartCheck() {
  VRing_UsedType *used;
  uint32_t i;
  uint32_t curHeart;
  for (i = 0; i < VRING_MAX_READERS; i++) {
    used = (VRing_UsedType *)(((uintptr_t)m_Used) + VRING_SIZE_OF_USED(m_NumDesc) * i);
    if (VRING_USED_STATE_READY == __atomic_load_n(&used->state, __ATOMIC_RELAXED)) {
      curHeart = __atomic_load_n(&used->heart, __ATOMIC_RELAXED);
      if (curHeart == used->lastHeart) { /* the reader is dead or stuck */
        removeAbnormalReader(used, i);
      } else {
        used->lastHeart = curHeart;
      }
    }
  }
}

void Writer::checkDescLife() {
  uint64_t elapsed;
  uint32_t idx;
  int32_t ref;
  int ret = 0;

  for (idx = 0; idx < m_NumDesc; idx++) {
    ret = spinLock(&m_Desc[idx].spin);
    if (0 == ret) {
      ref = __atomic_load_n(&m_Desc[idx].ref, __ATOMIC_RELAXED);
      if (ref > 0) {
        elapsed = timestamp() - m_Desc[idx].timestamp;
        if (elapsed > VRING_DESC_TIMEOUT) {
          ASLOG(VRINGE, ("vring writer %s: DESC %u ref = %d timeout\n", m_Name.c_str(), idx, ref));
          /* TODO: this is not right to do the release, it's FATAL APP's bug */
          releaseDesc(idx);
        }
      }
      spinUnlock(&m_Desc[idx].spin);
    } else {
      ASLOG(VRINGE,
            ("vring writer %s: check DESC life lock DESC[%u] spin timeout\n", m_Name.c_str(), idx));
    }
  }
}

void Writer::threadMain() {
  while (false == m_Stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    readerHeartCheck();
    checkDescLife();
  }
}

} // namespace spmc
} // namespace vring
} // namespace vdds
} // namespace as