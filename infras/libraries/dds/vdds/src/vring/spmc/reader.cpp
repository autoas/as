/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "vring/spmc/reader.hpp"
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
Reader::Reader(std::string name, uint32_t numDesc) : Base(name, numDesc) {
}

int Reader::init() {
  uint32_t i;
  VRing_UsedType *used;
  int32_t ref;
  int ret = 0;

  auto sharedMemory = std::make_shared<SharedMemory>(m_Name, 0, size());
  if (nullptr == sharedMemory) {
    ret = ENOMEM;
  } else {
    ret = sharedMemory->create();
  }

  if (0 == ret) {
    m_SharedMemory = sharedMemory;
    m_Meta = (VRing_MetaType *)m_SharedMemory->getVA();
    assert(m_Meta->numDesc == m_NumDesc);
    m_Desc = (VRing_DescType *)(((uintptr_t)m_Meta) + VRING_SIZE_OF_META());
    m_Avail = (VRing_AvailType *)(((uintptr_t)m_Desc) + VRING_SIZE_OF_DESC(m_NumDesc));
    used = (VRing_UsedType *)(((uintptr_t)m_Avail) + VRING_SIZE_OF_AVAIL(m_NumDesc));
    for (i = 0; i < VRING_MAX_READERS; i++) {
      if (VRING_USED_STATE_FREE == __atomic_load_n(&used->state, __ATOMIC_RELAXED)) {
        ref = __atomic_fetch_add(&used->state, 1, __ATOMIC_RELAXED);
        if (VRING_USED_STATE_FREE == ref) {
          m_ReaderIdx = i;
          m_Used = used;
          __atomic_fetch_add(&m_Used->heart, 1, __ATOMIC_RELAXED);
          ref = __atomic_add_fetch(&m_Used->state, 1, __ATOMIC_RELAXED);
          assert(VRING_USED_STATE_READY == ref);
          break;
        } else {
          ASLOG(VRING, ("vring reader %s: race on %u\n", m_Name.c_str(), i));
          __atomic_fetch_sub(&used->state, 1, __ATOMIC_RELAXED);
        }
      }
      used = (VRing_UsedType *)(((uintptr_t)used) + VRING_SIZE_OF_USED(m_NumDesc));
    }

    if (nullptr == m_Used) {
      ASLOG(VRINGE, ("vring reader %s no free used ring\n", m_Name.c_str()));
      ret = ENOSPC;
    }
  } else {
    ASLOG(VRINGE, ("vring reader can't open shm %s\n", m_Name.c_str()));
  }

  if (0 == ret) {
    std::string semName = m_Name + "_used" + std::to_string(m_ReaderIdx);
    m_SemUsed = std::make_shared<NamedSemaphore>(semName);
    if (nullptr == m_SemUsed) {
      ret = ENOMEM;
    } else {
      ret = m_SemUsed->create();
    }
  }

  if (0 == ret) {
    m_SemAvail = std::make_shared<NamedSemaphore>(m_Name);
    if (nullptr == m_SemAvail) {
      ret = ENOMEM;
    } else {
      ret = m_SemAvail->create();
    }
  }

  for (i = 0; (i < m_NumDesc) && (0 == ret); i++) {
    std::string shmFile = m_Name + "_" + std::to_string(i) + "_" + std::to_string(m_Desc[i].len);
    auto dmaMemory = std::make_shared<DmaMemory>(shmFile, m_Desc[i].handle, m_Desc[i].len);
    if (nullptr != dmaMemory) {
      ret = dmaMemory->create();
      if (0 == ret) {
        m_DmaMems.push_back(dmaMemory);
      } else {
        m_DmaMems.clear();
      }
    } else {
      ret = ENOMEM;
    }
  }

  if (0 == ret) {
    ASLOG(VRINGI, ("vring reader %s@%u online: msgSize = %u,  numDesc = %u\n", m_Name.c_str(),
                   m_ReaderIdx, m_Meta->msgSize, m_NumDesc));
    m_Thread = std::thread(&Reader::threadMain, this);
  }

  return ret;
}

Reader::~Reader() {
  void *addr;
  uint32_t idx = -1;
  uint32_t len = 0;
  int ret = 0;
  int32_t ref;

  m_Stop = true;
  if (m_Thread.joinable()) {
    m_Thread.join();
  }

  if (nullptr != m_Used) {
    ref = __atomic_load_n(&m_Used->state, __ATOMIC_RELAXED);
    if (VRING_USED_STATE_READY == ref) {
      ret = get(addr, idx, len, 0);
      while (0 == ret) {
        (void)put(idx);
        ret = get(addr, idx, len, 0);
        ASLOG(VRINGI, ("vring reader %s@%u, release unconsumed buffer at %u\n", m_Name.c_str(),
                       m_ReaderIdx, idx));
      }
      __atomic_sub_fetch(&m_Used->state, VRING_USED_STATE_READY, __ATOMIC_RELAXED);
      ASLOG(VRINGI, ("vring reader %s@%u clear up\n", m_Name.c_str(), m_ReaderIdx));
    } else {
      ASLOG(VRINGE, ("vring reader %s@%u killed\n", m_Name.c_str(), m_ReaderIdx));
    }
  }

  m_SharedMemory = nullptr;
  m_DmaMems.clear();

  m_SemAvail = nullptr;
  m_SemUsed = nullptr;
}

int Reader::get(void *&buf, uint32_t &idx, uint32_t &len, uint32_t timeoutMs) {
  VRing_UsedElemType *used;
  int ret = 0;

  (void)m_SemUsed->wait(timeoutMs);

  if (VRING_USED_STATE_READY != __atomic_load_n(&m_Used->state, __ATOMIC_RELAXED)) {
    ASLOG(VRINGE, ("vring reader %s@%u get killed by writer\n", m_Name.c_str(), m_ReaderIdx));
    ret = EBADF; /* killed by the Writer */
  } else if (m_Used->lastIdx == m_Used->idx) {
    /* no used buffer available */
    ret = ENOMSG;
  } else {
    used = &m_Used->ring[m_Used->lastIdx % m_NumDesc];
    idx = used->id;
    len = used->len;
    m_Used->lastIdx++;

    buf = m_DmaMems[idx]->getVA();

    /* if the app crashed after this before call the put, then the desc is in detached state
     * that need the monitor to recycle it.
     */
  }

  return ret;
}

int Reader::put(uint32_t idx) {
  int32_t ref;
  int ret = 0;
  bool doSemPost = false;

  if (VRING_USED_STATE_READY != __atomic_load_n(&m_Used->state, __ATOMIC_RELAXED)) {
    ret = EBADF; /* killed by the Writer */
    ASLOG(VRINGE, ("vring reader %s@%u put killed by writer\n", m_Name.c_str(), m_ReaderIdx));
  } else if (idx > m_NumDesc) {
    ret = EINVAL;
  } else {
    ret = spinLock(&m_Desc[idx].spin);
    if (0 == ret) {
      ref = __atomic_sub_fetch(&m_Desc[idx].ref, 1, __ATOMIC_RELAXED);
      if (0 < ref) {
        /* still used by others */
      } else if (0 == ref) {
        ret = spinLock(&m_Avail->spin);
        if (0 == ret) {
          m_Avail->ring[m_Avail->idx % m_NumDesc] = idx;
          ASLOG(VRING, ("vring reader %s@%u: put DESC[%u]; AVAIL: lastIdx = %u, idx = %u\n",
                        m_Name.c_str(), m_ReaderIdx, idx, m_Avail->lastIdx, m_Avail->idx));
          m_Avail->idx++;
          spinUnlock(&m_Avail->spin);
          doSemPost = true;
        } else {
          ASLOG(VRINGE, ("vring reader %s: put lock AVAIL spin timeout\n", m_Name.c_str(), idx));
        }
      } else {
        ASLOG(VRINGE, ("vring reader %s@%u: put DESC[%u], ref = %d\n", m_Name.c_str(), m_ReaderIdx,
                       idx, ref));
        assert(0);
        ret = EFAULT;
      }
      spinUnlock(&m_Desc[idx].spin);
    } else {
      ASLOG(VRINGE, ("vring reader %s: put lock DESC[%u] spin timeout\n", m_Name.c_str(), idx));
    }

    if (doSemPost) {
      ret = m_SemAvail->post();
    }
  }

  return ret;
}

void Reader::threadMain() {
  while (false == m_Stop) {
    __atomic_fetch_add(&m_Used->heart, 1, __ATOMIC_RELAXED);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

} // namespace spmc
} // namespace vring
} // namespace vdds
} // namespace as