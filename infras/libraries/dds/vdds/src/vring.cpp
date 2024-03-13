/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "vring.hpp"
#include <assert.h>
#include <unistd.h>
#include "Std_Debug.h"
#include <cinttypes>

namespace as {
namespace vdds {
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_VRING 0
#define AS_LOG_VRINGI 2
#define AS_LOG_VRINGE 2

#define VRING_DESC_TIMEOUT (2000000)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static std::string replace(std::string resource_str, std::string sub_str, std::string new_str) {
  std::string dst_str = resource_str;
  std::string::size_type pos = 0;
  while ((pos = dst_str.find(sub_str)) != std::string::npos) {
    dst_str.replace(pos, sub_str.length(), new_str);
  }
  return dst_str;
}

static std::string toShmName(std::string name) {
  std::string fname = "as" + replace(name, "/", "_");
  return fname;
}

static std::string toSemName(std::string name) {
  std::string fname = "as" + replace(name, "/", "_");
#if defined(_WIN32)
  fname = "Local/" + fname;
#endif
  return fname;
}
/* ================================ [ FUNCTIONS ] ============================================== */
VRingBase::VRingBase(std::string name, uint32_t numDesc) : m_Name(name), m_NumDesc(numDesc) {
}

VRingBase::~VRingBase() {
}

uint32_t VRingBase::size() {
  return VRING_SIZE_OF_META(m_NumDesc) + VRING_SIZE_OF_DESC(m_NumDesc) +
         VRING_SIZE_OF_AVAIL(m_NumDesc) + VRING_SIZE_OF_ALL_USED(m_NumDesc);
}

uint64_t VRingBase::timestamp() {
  uint64_t tsp = 0;
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  tsp = (ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);

  return tsp;
}

void *VRingBase::getVA(uint64_t PA, uint32_t size, int oflag) {
  void *addr = nullptr;

  std::unique_lock<std::mutex> lck(m_Lock);
  auto it = m_ShmMap.find(PA);
  int ret = 0;
  if (it == m_ShmMap.end()) {
    std::string shmFile = toShmName(m_Name + "-" + std::to_string(PA) + "-" + std::to_string(size));
    int fd = shm_open(shmFile.c_str(), oflag, 0600);
    if (fd >= 0) {
#if defined(linux)
      if (oflag & O_CREAT) {
        ret = ftruncate(fd, size);
      }
#endif
      if (0 == ret) {
        addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (addr != nullptr) {
          m_ShmMap[PA] = {addr, fd};
        }
      } else {
        ASLOG(VRINGE, ("vring %s can't getVA %" PRIu64 "\n", m_Name.c_str(), PA));
      }
    }
  } else {
    addr = it->second.addr;
  }
  return addr;
}

/* https://rigtorp.se/spinlock/ */
void VRingBase::spinLock(int32_t *pLock) {
  /* NOTE: if the the others do spinLock and then crashed, that's diaster to the things left */
  while (__atomic_exchange_n(pLock, 1, __ATOMIC_ACQUIRE)) {
  }
}

void VRingBase::spinUnlock(int32_t *pLock) {
  __atomic_store_n(pLock, 0, __ATOMIC_RELEASE);
}

VRingWriter::VRingWriter(std::string name, uint32_t msgSize, uint32_t numDesc)
  : VRingBase(name, numDesc), m_MsgSize(msgSize) {
}

int VRingWriter::init() {
  void *addr = nullptr;
  int fd;
  int ret = 0;

  fd = shm_open(toShmName(m_Name).c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
  if (fd >= 0) {
#if defined(linux)
    ret = ftruncate(fd, size());
    if (0 != ret) {
      ASLOG(VRINGE,
            ("vring writer set shm %s size %" PRIu64 " retor %d\n", m_Name.c_str(), size(), ret));
    }
#endif
    if (0 == ret) {
      addr = mmap(NULL, size(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if (nullptr == addr) {
        ASLOG(VRINGE, ("vring writer %s can't mmap shm\n", m_Name.c_str()));
        shm_unlink(toShmName(m_Name).c_str());
        ret = EFAULT;
      }
    }
  } else {
    ASLOG(VRINGE, ("vring writer can't open shm %s\n", m_Name.c_str()));
    ret = ENOMEM;
  }

  if (0 == ret) {
    m_SharedMemory = addr;
    m_ShmFd = fd;
    m_Meta = (VRing_MetaType *)m_SharedMemory;
    m_Desc = (VRing_DescType *)(((uintptr_t)m_Meta) + VRING_SIZE_OF_META(m_NumDesc));
    m_Avail = (VRing_AvailType *)(((uintptr_t)m_Desc) + VRING_SIZE_OF_DESC(m_NumDesc));
    m_Used = (VRing_UsedType *)(((uintptr_t)m_Avail) + VRING_SIZE_OF_AVAIL(m_NumDesc));
    ret = setup();
  }

  if (0 == ret) {
    std::string semName = toSemName(m_Name);
    m_SemAvail = sem_open(semName.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600, m_NumDesc);
    if (nullptr == m_SemAvail) {
      ASLOG(VRINGE, ("vring writer can't create sem %s\n", semName.c_str()));
      ret = EPIPE;
    }
  }

  if (0 == ret) {
    for (uint32_t i = 0; i < VRING_MAX_READERS; i++) {
      std::string semName = toSemName(m_Name + "/used" + std::to_string(i));
      m_UsedSems[i] = sem_open(semName.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600, 0);
      if (nullptr == m_UsedSems[i]) {
        ASLOG(VRINGE, ("vring writer can't create sem %s\n", semName.c_str()));
        ret = EPIPE;
      }
    }
  }

  if (0 == ret) {
    ASLOG(VRING, ("vring writer %s online: msgSize = %u,  numDesc = %u\n", m_Name.c_str(),
                  m_MsgSize, m_NumDesc));
    m_Thread = std::thread(&VRingWriter::threadMain, this);
  }

  return ret;
}

VRingWriter::~VRingWriter() {
  m_Stop = true;
  if (m_Thread.joinable()) {
    m_Thread.join();
  }

  if (nullptr != m_SemAvail) {
    std::string semName = toSemName(m_Name);
    sem_close(m_SemAvail);
    sem_unlink(semName.c_str());
  }

  for (uint32_t i = 0; i < m_UsedSems.size(); i++) {
    if (nullptr != m_UsedSems[i]) {
      std::string semName = toSemName(m_Name + "/used" + std::to_string(i));
      sem_close(m_UsedSems[i]);
      sem_unlink(semName.c_str());
    }
  }

  if (nullptr != m_SharedMemory) {
    shm_unlink(toShmName(m_Name).c_str());
  }

  std::unique_lock<std::mutex> lck(m_Lock);
  for (auto &it : m_ShmMap) {
    std::string shmFile =
      toShmName(m_Name + "-" + std::to_string(it.first) + "-" + std::to_string(m_MsgSize));
    shm_unlink(shmFile.c_str());
  }
}

int VRingWriter::setup() {
  uint32_t i;
  int ret = 0;
  void *addr;
  memset(m_SharedMemory, 0, size());
  m_Meta->msgSize = m_MsgSize;
  m_Meta->numDesc = m_NumDesc;
  for (i = 0; (i < m_NumDesc) && (0 == ret); i++) {
    addr = getVA(i, m_MsgSize, O_CREAT | O_EXCL | O_RDWR);
    if (nullptr != addr) {
      m_Desc[i].addr = i;
      m_Desc[i].len = m_MsgSize;
      m_Avail->ring[i] = i;
      m_Avail->idx++;
    } else {
      ret = ENOMEM;
    }
  }

  return ret;
}

int VRingWriter::get(void *&buf, uint32_t &idx, uint32_t &len, uint32_t timeoutMs) {
  int ret = 0;

  if (0 != timeoutMs) {
    struct timespec ts;
    ret = clock_gettime(CLOCK_REALTIME, &ts);
    if (0 == ret) {
      ts.tv_sec += timeoutMs / 1000;
      ts.tv_nsec += (timeoutMs % 1000) * 1000000;
      ret = sem_timedwait(m_SemAvail, &ts);
    }
  }

  if (0 == ret) {
    spinLock(&m_Avail->spin);
    if (m_Avail->lastIdx == m_Avail->idx) {
      /* no buffers */
      ret = ENODATA;
    } else {
      idx = m_Avail->ring[m_Avail->lastIdx % m_NumDesc];
      __atomic_store_n(&m_Desc[idx].ref, 0, __ATOMIC_RELAXED);
      buf = (void *)getVA(m_Desc[idx].addr, m_Desc[idx].len);
      len = m_Desc[idx].len;
      m_Avail->lastIdx++;
      ASLOG(VRING, ("vring writer %s: get desc idx = %u, len = %u; avail: lastIdx = %u, idx = %u\n",
                    m_Name.c_str(), idx, len, m_Avail->lastIdx, m_Avail->idx));
    }
    spinUnlock(&m_Avail->spin);
  }

  return ret;
}

int VRingWriter::put(uint32_t idx, uint32_t len) {
  VRing_UsedType *used;
  VRing_UsedElemType *usedElem;
  uint32_t i;
  bool isUSed = false;
  int ret = 0;

  if (idx > m_NumDesc) {
    ret = EINVAL;
  } else {
    spinLock(&m_Desc[idx].spin);
    m_Desc[idx].timestamp = timestamp();
    for (i = 0; (i < VRING_MAX_READERS) && (0 == ret); i++) {
      used = (VRing_UsedType *)(((uintptr_t)m_Used) + VRING_SIZE_OF_USED(m_NumDesc) * i);
      /* state == 1, the used ring is in good status */
      if (VRING_USED_STATE_READY == __atomic_load_n(&used->state, __ATOMIC_RELAXED)) {
        usedElem = &used->ring[used->idx % m_NumDesc];
        usedElem->id = idx;
        usedElem->len = len;
        __atomic_fetch_add(&m_Desc[idx].ref, 1, __ATOMIC_RELAXED);
        /* NOTE: risk here if context switch if there is more than 1 readers */
        isUSed = true;
        ASLOG(VRING,
              ("vring writer %s@%u: put desc idx = %u, len = %u ref = %d; used: lastIdx = "
               "%u, idx = %u\n",
               m_Name.c_str(), i, idx, len, __atomic_load_n(&m_Desc[idx].ref, __ATOMIC_RELAXED),
               used->lastIdx, used->idx));
        used->idx++;
        ret = sem_post(m_UsedSems[i]);
        if (0 != ret) {
          ASLOG(VRING, ("vring writer %s@%u: sem post failed: %d\n", m_Name.c_str(), i, ret));
        }
      }
    }
    spinUnlock(&m_Desc[idx].spin);

    if (false == isUSed) {
      /* OK, put it back */
      (void)drop(idx);
      ret = ENOLINK;
    }
  }

  return ret;
}

int VRingWriter::drop(uint32_t idx) {
  int ret = 0;

  if (idx > m_NumDesc) {
    ret = EINVAL;
  } else {
    spinLock(&m_Avail->spin);
    m_Avail->ring[m_Avail->idx % m_NumDesc] = idx;
    m_Avail->idx++;
    spinUnlock(&m_Avail->spin);
    ASLOG(VRING, ("vring writer %s: drop desc idx = %u; avail: lastIdx = %u, idx = %u\n",
                  m_Name.c_str(), idx, m_Avail->lastIdx, m_Avail->idx));
    ret = sem_post(m_SemAvail);
  }

  return ret;
}

void VRingWriter::releaseDesc(uint32_t idx) {
  int32_t ref;
  ref = __atomic_sub_fetch(&m_Desc[idx].ref, 1, __ATOMIC_RELAXED);
  if (0 < ref) {
    /* still used by others */
  } else if (0 == ref) {
    spinLock(&m_Avail->spin);
    m_Avail->ring[m_Avail->idx % m_NumDesc] = idx;
    m_Avail->idx++;
    spinUnlock(&m_Avail->spin);
    ASLOG(VRINGE, ("vring writer %s: release DESC %d\n", m_Name.c_str(), idx));
    (void)sem_post(m_SemAvail);
  } else {
    ASLOG(VRING,
          ("vring writer %s: release desc idx = %u ref = %d failed\n", m_Name.c_str(), idx, ref));
    assert(0);
  }
}

void VRingWriter::removeAbnormalReader(VRing_UsedType *used, uint32_t readerIdx) {
  VRing_UsedElemType *usedElem;
  uint32_t ref;
  uint32_t idx;

  ASLOG(VRINGE, ("vring reader %s@%u is dead\n", m_Name.c_str(), readerIdx));
  /* set ref > 1, mark as dead to stop the writer to put data on this used ring */
  /* step 1: release the DESC in the reader used ring */
  ref = __atomic_add_fetch(&used->state, 1, __ATOMIC_RELAXED);
  assert(VRING_USED_STATE_KILLED == ref);
  while (used->lastIdx != used->idx) {
    usedElem = &used->ring[used->lastIdx % m_NumDesc];
    idx = usedElem->id;
    spinLock(&m_Desc[idx].spin);
    releaseDesc(idx);
    spinUnlock(&m_Desc[idx].spin);
    used->lastIdx++;
  }

  ref = __atomic_sub_fetch(&used->state, VRING_USED_STATE_KILLED, __ATOMIC_RELAXED);
  assert(VRING_USED_STATE_FREE == ref);
}

void VRingWriter::readerHeartCheck() {
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

void VRingWriter::checkDescLife() {
  uint64_t elapsed;
  uint32_t idx;
  int32_t ref;

  for (idx = 0; idx < m_NumDesc; idx++) {
    spinLock(&m_Desc[idx].spin);
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
  }
}

void VRingWriter::threadMain() {
  while (false == m_Stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    readerHeartCheck();
    checkDescLife();
  }
}

VRingReader::VRingReader(std::string name, uint32_t numDesc) : VRingBase(name, numDesc) {
}

int VRingReader::init() {
  void *addr = nullptr;
  int fd;
  uint32_t i;
  VRing_UsedType *used;
  int32_t ref;
  int ret = 0;

  fd = shm_open(toShmName(m_Name).c_str(), O_RDWR, 0600);
  if (fd >= 0) {
    addr = mmap(NULL, size(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (nullptr == addr) {
      ASLOG(VRINGE, ("vring reader %s can't mmap shm\n", m_Name.c_str()));
      shm_unlink(toShmName(m_Name).c_str());
      ret = EFAULT;
    }
  } else {
    ASLOG(VRINGE, ("vring reader %s can't open shm\n", m_Name.c_str()));
    ret = ENOMEM;
  }

  if (0 == ret) {
    m_SharedMemory = addr;
    m_ShmFd = fd;
    m_Meta = (VRing_MetaType *)m_SharedMemory;
    assert(m_Meta->numDesc == m_NumDesc);
    m_Desc = (VRing_DescType *)(((uintptr_t)m_Meta) + VRING_SIZE_OF_META(m_NumDesc));
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
  }

  if (0 == ret) {
    std::string semName = toSemName(m_Name + "/used" + std::to_string(m_ReaderIdx));
    m_SemUsed = sem_open(semName.c_str(), O_RDWR, 0600, 0);
    if (nullptr == m_SemUsed) {
      ASLOG(VRINGE, ("vring reader %s@%u can't open used sem\n", m_Name.c_str(), m_ReaderIdx));
      ret = EPIPE;
    }
  }

  if (0 == ret) {
    std::string semName = toSemName(m_Name);
    m_SemAvail = sem_open(semName.c_str(), O_RDWR, 0600, 0);
    if (nullptr == m_SemAvail) {
      ASLOG(VRINGE, ("vring reader %s@%u can't open avail sem\n", m_Name.c_str(), m_ReaderIdx));
      ret = EPIPE;
    }
  }

  if (0 == ret) {
    ASLOG(VRING, ("vring reader %s@%u online: msgSize = %u,  numDesc = %u\n", m_Name.c_str(),
                  m_ReaderIdx, m_Meta->msgSize, m_NumDesc));
    m_Thread = std::thread(&VRingReader::threadMain, this);
  }

  return ret;
}

VRingReader::~VRingReader() {
  void *addr;
  uint32_t idx = -1;
  uint32_t len = 0;
  int ret = 0;

  m_Stop = true;
  if (m_Thread.joinable()) {
    m_Thread.join();
  }

  if (nullptr != m_Used) {
    ret = get(addr, idx, len, 0);
    while (0 == ret) {
      (void)put(idx);
      ret = get(addr, idx, len, 0);
      ASLOG(VRINGI, ("vring reader %s@%u, release unconsumed buffer at %u\n", m_Name.c_str(),
                     m_ReaderIdx, idx));
    }
    __atomic_sub_fetch(&m_Used->state, VRING_USED_STATE_READY, __ATOMIC_RELAXED);
    ASLOG(VRINGI, ("vring reader %s@%u clear up\n", m_Name.c_str(), m_ReaderIdx));
  }

#if defined(_WIN32)
  if (nullptr != m_SharedMemory) {
    shm_unlink(toShmName(m_Name).c_str());
  }

  std::unique_lock<std::mutex> lck(m_Lock);
  for (auto &it : m_ShmMap) {
    std::string shmFile =
      toShmName(m_Name + "-" + std::to_string(it.first) + "-" + std::to_string(m_Meta->msgSize));
    shm_unlink(shmFile.c_str());
  }
#else
  if (nullptr != m_SharedMemory) {
    close(m_ShmFd);
  }

  std::unique_lock<std::mutex> lck(m_Lock);
  for (auto &it : m_ShmMap) {
    close(it.second.fd);
  }
#endif

  if (nullptr != m_SemAvail) {
    sem_close(m_SemAvail);
  }

  if (nullptr != m_SemUsed) {
    sem_close(m_SemUsed);
  }
}

int VRingReader::get(void *&buf, uint32_t &idx, uint32_t &len, uint32_t timeoutMs) {
  VRing_UsedElemType *used;
  int ret = 0;

  if (0 != timeoutMs) {
    struct timespec ts;
    ret = clock_gettime(CLOCK_REALTIME, &ts);
    if (0 == ret) {
      ts.tv_sec += timeoutMs / 1000;
      ts.tv_nsec += (timeoutMs % 1000) * 1000000;
      ret = sem_timedwait(m_SemUsed, &ts);
    }
  }

  if (0 == ret) {
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

      buf = (void *)getVA(m_Desc[idx].addr, m_Desc[idx].len);
      m_Used->lastIdx++;

      /* if the app crashed after this before call the put, then the desc is in detached state
       * that need the monitor to recycle it.
       */
    }
  }

  return ret;
}

int VRingReader::put(uint32_t idx) {
  int32_t ref;
  int ret = 0;

  if (VRING_USED_STATE_READY != __atomic_load_n(&m_Used->state, __ATOMIC_RELAXED)) {
    ret = EBADF; /* killed by the Writer */
    ASLOG(VRINGE, ("vring reader %s@%u put killed by writer\n", m_Name.c_str(), m_ReaderIdx));
  } else if (idx > m_NumDesc) {
    ret = EINVAL;
  } else {
    spinLock(&m_Desc[idx].spin);
    ref = __atomic_sub_fetch(&m_Desc[idx].ref, 1, __ATOMIC_RELAXED);
    if (0 < ref) {
      /* still used by others */
    } else if (0 == ref) {
      spinLock(&m_Avail->spin);
      m_Avail->ring[m_Avail->idx % m_NumDesc] = idx;
      ASLOG(VRING, ("vring reader %s@%u: put desc idx = %u; avail: lastIdx = %u, idx = %u\n",
                    m_Name.c_str(), m_ReaderIdx, idx, m_Avail->lastIdx, m_Avail->idx));
      m_Avail->idx++;
      spinUnlock(&m_Avail->spin);
      ret = sem_post(m_SemAvail);
    } else {
      ASLOG(VRINGE, ("vring reader %s@%u: put desc idx = %u, ref = %d\n", m_Name.c_str(),
                     m_ReaderIdx, idx, ref));
      assert(0);
      ret = EFAULT;
    }
    spinUnlock(&m_Desc[idx].spin);
  }

  return ret;
}

void VRingReader::threadMain() {
  while (false == m_Stop) {
    __atomic_fetch_add(&m_Used->heart, 1, __ATOMIC_RELAXED);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

} // namespace vdds
} // namespace as
