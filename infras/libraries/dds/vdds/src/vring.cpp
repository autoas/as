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
VRingBase::VRingBase(std::string name, uint32_t maxReaders, uint32_t capability, uint32_t align)
  : m_Name(name), m_MaxReaders(maxReaders), m_Capability(capability), m_Align(align) {
}

VRingBase::~VRingBase() {
}

uint32_t VRingBase::size() {
  return VRING_SIZE_OF_META(m_Capability, m_Align) + VRING_SIZE_OF_DESC(m_Capability, m_Align) +
         VRING_SIZE_OF_AVAIL(m_Capability, m_Align) +
         VRING_SIZE_OF_USED(m_Capability, m_Align) * m_MaxReaders;
}

void *VRingBase::getVA(uint64_t PA, uint32_t size, int oflag) {
  void *addr = nullptr;

  std::unique_lock<std::mutex> lck(m_Lock);
  auto it = m_ShmMap.find(PA);
  int err = 0;
  if (it == m_ShmMap.end()) {
    std::string shmFile = toShmName(m_Name + "-" + std::to_string(PA) + "-" + std::to_string(size));
    int fd = shm_open(shmFile.c_str(), oflag, 0600);
    if (fd >= 0) {
#if defined(linux)
      if (oflag & O_CREAT) {
        err = ftruncate(fd, size);
      }
#endif
      if (0 == err) {
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
  while (__atomic_exchange_n(pLock, 1, __ATOMIC_ACQUIRE))
    ;
}
void VRingBase::spinUnlock(int32_t *pLock) {
  __atomic_store_n(pLock, 0, __ATOMIC_RELEASE);
}

VRingWriter::VRingWriter(std::string name, uint32_t msgSize, uint32_t maxReaders,
                         uint32_t capability, uint32_t align)
  : VRingBase(name, maxReaders, capability, align), m_MsgSize(msgSize) {
  void *addr = nullptr;
  int fd;

  fd = shm_open(toShmName(m_Name).c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
  if (fd >= 0) {
#if defined(linux)
    m_Err = ftruncate(fd, size());
    if (0 != m_Err) {
      ASLOG(VRINGE,
            ("vring writer set shm %s size %" PRIu64 " error %d\n", m_Name.c_str(), size(), m_Err));
    }
#endif
    if (0 == m_Err) {
      addr = mmap(NULL, size(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if (nullptr == addr) {
        ASLOG(VRINGE, ("vring writer %s can't mmap shm\n", m_Name.c_str()));
        shm_unlink(toShmName(m_Name).c_str());
        m_Err = EFAULT;
      }
    }
  } else {
    ASLOG(VRINGE, ("vring writer can't open shm %s\n", m_Name.c_str()));
    m_Err = ENOMEM;
  }

  if (0 == m_Err) {
    m_SharedMemory = addr;
    m_ShmFd = fd;
    m_Meta = (VRing_MetaType *)m_SharedMemory;
    m_Desc = (VRing_DescType *)(((uintptr_t)m_Meta) + VRING_SIZE_OF_META(m_Capability, m_Align));
    m_Avail = (VRing_AvailType *)(((uintptr_t)m_Desc) + VRING_SIZE_OF_DESC(m_Capability, m_Align));
    m_Used = (VRing_UsedType *)(((uintptr_t)m_Avail) + VRING_SIZE_OF_AVAIL(m_Capability, m_Align));
    setup();
  }

  if (0 == m_Err) {
    std::string semName = toSemName(m_Name);
    m_SemAvail = sem_open(semName.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600, m_Capability);
    if (nullptr == m_SemAvail) {
      ASLOG(VRINGE, ("vring writer can't create sem %s\n", semName.c_str()));
      m_Err = EPIPE;
    }
  }

  if (0 == m_Err) {
    m_UsedSems.resize(maxReaders);
    for (uint32_t i = 0; i < maxReaders; i++) {
      std::string semName = toSemName(m_Name + "/used" + std::to_string(i));
      m_UsedSems[i] = sem_open(semName.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600, 0);
      if (nullptr == m_UsedSems[i]) {
        ASLOG(VRINGE, ("vring writer can't create sem %s\n", semName.c_str()));
        m_Err = EPIPE;
      }
    }
  }

  if (0 == m_Err) {
    ASLOG(VRING,
          ("vring writer %s online: msgSize = %u, maxReaders = %u, capability = %u, align = %u\n",
           m_Name.c_str(), m_MsgSize, m_MaxReaders, m_Capability, m_Align));
    m_Thread = std::thread(&VRingWriter::threadMain, this);
  }
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

void VRingWriter::setup() {
  uint32_t i;
  int err = 0;
  void *addr;
  memset(m_SharedMemory, 0, size());
  m_Meta->maxReaders = m_MaxReaders;
  m_Meta->msgSize = m_MsgSize;
  m_Meta->capability = m_Capability;
  m_Meta->align = m_Align;
  for (i = 0; (i < m_Capability) && (0 == err); i++) {
    addr = getVA(i, m_MsgSize, O_CREAT | O_EXCL | O_RDWR);
    if (nullptr != addr) {
      m_Desc[i].addr = i;
      m_Desc[i].len = m_MsgSize;
      m_Desc[i].flags = 0;
      m_Desc[i].next = 0;
      m_Avail->ring[i] = i;
      m_Avail->idx++;
    } else {
      err = ENOMEM;
    }
  }

  m_Err = err;
}

void *VRingWriter::get(uint32_t *idx, uint32_t *len, uint32_t timeoutMs) {
  void *buf = nullptr;
  int err = 0;

  if (0 != timeoutMs) {
    struct timespec ts;
    err = clock_gettime(CLOCK_REALTIME, &ts);
    if (0 == err) {
      ts.tv_sec += timeoutMs / 1000;
      ts.tv_nsec += (timeoutMs % 1000) * 1000000;
      (void)sem_timedwait(m_SemAvail, &ts);
    }
  }
  spinLock(&m_Avail->spin);
  if (m_Avail->lastIdx == m_Avail->idx) {
    /* no buffers */
  } else {
    *idx = m_Avail->ring[m_Avail->lastIdx % m_Capability];
    __atomic_store_n(&m_Desc[*idx].ref, 0, __ATOMIC_RELAXED);

    buf = (void *)getVA(m_Desc[*idx].addr, m_Desc[*idx].len);
    *len = m_Desc[*idx].len;

    m_Avail->lastIdx++;
    ASLOG(VRING, ("vring writer %s: get desc idx = %u, len = %u; avail: lastIdx = %u, idx = %u\n",
                  m_Name.c_str(), *idx, *len, m_Avail->lastIdx, m_Avail->idx));
  }
  spinUnlock(&m_Avail->spin);

  return buf;
}

void VRingWriter::put(uint32_t idx, uint32_t len) {
  VRing_UsedType *used;
  VRing_UsedElemType *usedElem;
  uint32_t i;
  bool isUSed = false;

  if (idx > m_Capability) {
    assert(0);
  } else {
    spinLock(&m_Desc[idx].spin);
    for (i = 0; i < m_MaxReaders; i++) {
      used =
        (VRing_UsedType *)(((uintptr_t)m_Used) + VRING_SIZE_OF_USED(m_Capability, m_Align) * i);
      /* ref == 1, the used ring is in good status */
      if (1 == __atomic_load_n(&used->ref, __ATOMIC_RELAXED)) {
        usedElem = &used->ring[used->idx % m_Capability];
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
        sem_post(m_UsedSems[i]);
      }
    }
    spinUnlock(&m_Desc[idx].spin);

    if (false == isUSed) {
      /* OK, put it back */
      drop(idx);
    }
  }
}

void VRingWriter::drop(uint32_t idx) {
  if (idx > m_Capability) {
    assert(0);
  } else {
    __atomic_store_n(&m_Desc[idx].ref, 0, __ATOMIC_RELAXED);
    spinLock(&m_Avail->spin);
    m_Avail->ring[m_Avail->idx % m_Capability] = idx;
    m_Avail->idx++;
    spinUnlock(&m_Avail->spin);
    ASLOG(VRING, ("vring writer %s: drop desc idx = %u; avail: lastIdx = %u, idx = %u\n",
                  m_Name.c_str(), idx, m_Avail->lastIdx, m_Avail->idx));
    (void)sem_post(m_SemAvail);
  }
}

void VRingWriter::releaseDesc(uint32_t idx, uint32_t readerIdx, const char *prefix) {
  uint32_t ref;
  spinLock(&m_Desc[idx].spin);
  ref = __atomic_sub_fetch(&m_Desc[idx].ref, 1, __ATOMIC_RELAXED);
  if (0 < ref) {
    /* still used by others */
  } else if (0 == ref) {
    spinLock(&m_Avail->spin);
    m_Avail->ring[m_Avail->idx % m_Capability] = idx;
    m_Avail->idx++;
    spinUnlock(&m_Avail->spin);
    ASLOG(VRINGE, ("vring reader %s@%u is dead: release %s buffer %u\n", m_Name.c_str(), readerIdx,
                   prefix, idx));
    (void)sem_post(m_SemAvail);
  } else {
    assert(0);
    m_Err = EFAULT;
  }
  spinUnlock(&m_Desc[idx].spin);
  (void)sem_post(m_SemAvail);
}

void VRingWriter::removeAbnormalReader(VRing_UsedType *used, uint32_t readerIdx) {
  VRing_UsedElemType *usedElem;
  uint32_t ref;
  uint32_t idx;

  ASLOG(VRINGE, ("vring reader %s@%u is dead\n", m_Name.c_str(), readerIdx));
  /* set ref > 1, mark as dead to stop the writer to put data on this used ring */
  /* step 1: release the DESC in the reader used ring */
  __atomic_add_fetch(&used->ref, 1, __ATOMIC_RELAXED);
  while (used->lastIdx != used->idx) {
    usedElem = &used->ring[used->lastIdx % m_Capability];
    idx = usedElem->id;
    releaseDesc(idx, readerIdx, "used");
    used->lastIdx++;
  }

  /* step 2: Figure out DESC which in attached state for a situation as below:
   * The Reader.get get a DESC from the USED ring, the DESC.ref is 1, the reader crashed before
   * call Reader.put, then the DESC is in detached state, it was not in any USED ring or the AVAIL
   * ring. */
  idx = __atomic_load_n(&m_Used->lastDescId, __ATOMIC_RELAXED);
  if (idx < m_Capability) {
    releaseDesc(idx, readerIdx, "detached");
    __atomic_store_n(&m_Used->lastDescId, m_Capability, __ATOMIC_RELAXED);
  }

  ref = __atomic_sub_fetch(&used->ref, 2, __ATOMIC_RELAXED);
  assert(0 == ref);
}

void VRingWriter::readerHeartCheck() {
  VRing_UsedType *used;
  uint32_t i;
  uint32_t curHeart;
  for (i = 0; i < m_MaxReaders; i++) {
    used = (VRing_UsedType *)(((uintptr_t)m_Used) + VRING_SIZE_OF_USED(m_Capability, m_Align) * i);
    if (__atomic_load_n(&used->ref, __ATOMIC_RELAXED) > 0) {
      curHeart = __atomic_load_n(&used->heart, __ATOMIC_RELAXED);
      if (curHeart == used->lastHeart) { /* the reader is dead or stuck */
        removeAbnormalReader(used, i);
      } else {
        used->lastHeart = curHeart;
      }
    }
  }
}

void VRingWriter::threadMain() {

  while (false == m_Stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    readerHeartCheck();
  }
}

VRingReader::VRingReader(std::string name, uint32_t maxReaders, uint32_t capability, uint32_t align)
  : VRingBase(name, maxReaders, capability, align) {
  void *addr = nullptr;
  int fd;
  uint32_t i;
  VRing_UsedType *used;
  int32_t ref;

  fd = shm_open(toShmName(m_Name).c_str(), O_RDWR, 0600);
  if (fd >= 0) {
    addr = mmap(NULL, size(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (nullptr == addr) {
      ASLOG(VRINGE, ("vring reader %s can't mmap shm\n", m_Name.c_str()));
      shm_unlink(toShmName(m_Name).c_str());
      m_Err = EFAULT;
    }
  } else {
    ASLOG(VRINGE, ("vring reader %s can't open shm\n", m_Name.c_str()));
    m_Err = ENOMEM;
  }

  if (0 == m_Err) {
    m_SharedMemory = addr;
    m_ShmFd = fd;
    m_Meta = (VRing_MetaType *)m_SharedMemory;
    assert(m_Meta->maxReaders == m_MaxReaders);
    assert(m_Meta->capability == m_Capability);
    assert(m_Meta->align == m_Align);
    m_Desc = (VRing_DescType *)(((uintptr_t)m_Meta) + VRING_SIZE_OF_META(m_Capability, m_Align));
    m_Avail = (VRing_AvailType *)(((uintptr_t)m_Desc) + VRING_SIZE_OF_DESC(m_Capability, m_Align));
    used = (VRing_UsedType *)(((uintptr_t)m_Avail) + VRING_SIZE_OF_AVAIL(m_Capability, m_Align));
    for (i = 0; i < m_MaxReaders; i++) {
      if (0 == __atomic_load_n(&used->ref, __ATOMIC_RELAXED)) {
        ref = __atomic_fetch_add(&used->ref, 1, __ATOMIC_RELAXED);
        if (0 == ref) {
          m_ReaderIdx = i;
          m_Used = used;
          __atomic_store_n(&m_Used->lastDescId, m_Capability, __ATOMIC_RELAXED);
          break;
        } else {
          ASLOG(VRING, ("vring reader %s: race on %u\n", m_Name.c_str(), i));
          __atomic_fetch_sub(&used->ref, 1, __ATOMIC_RELAXED);
        }
      }
      used = (VRing_UsedType *)(((uintptr_t)used) + VRING_SIZE_OF_USED(m_Capability, m_Align));
    }

    if (nullptr == m_Used) {
      ASLOG(VRINGE, ("vring reader %s no free used ring\n", m_Name.c_str()));
      m_Err = ENOSPC;
    }

    if (0 == m_Err) {
      std::string semName = toSemName(m_Name + "/used" + std::to_string(m_ReaderIdx));
      m_SemUsed = sem_open(semName.c_str(), O_RDWR, 0600, 0);
      if (nullptr == m_SemUsed) {
        ASLOG(VRINGE, ("vring reader %s@%u can't open used sem\n", m_Name.c_str(), m_ReaderIdx));
        m_Err = EPIPE;
      }
    }

    if (0 == m_Err) {
      std::string semName = toSemName(m_Name);
      m_SemAvail = sem_open(semName.c_str(), O_RDWR, 0600, 0);
      if (nullptr == m_SemAvail) {
        ASLOG(VRINGE, ("vring reader %s@%u can't open avail sem\n", m_Name.c_str(), m_ReaderIdx));
        m_Err = EPIPE;
      }
    }

    if (0 == m_Err) {
      ASLOG(VRING,
            ("vring reader %s@%u online: msgSize = %u, maxReaders = %u, capability = %u, "
             "align = %u\n",
             m_Name.c_str(), m_ReaderIdx, m_Meta->msgSize, m_MaxReaders, m_Capability, m_Align));
      __atomic_fetch_add(&m_Used->heart, 1, __ATOMIC_RELAXED);
      m_Thread = std::thread(&VRingReader::threadMain, this);
    }
  }
}

VRingReader::~VRingReader() {
  void *addr;
  uint32_t idx;
  uint32_t len;

  m_Stop = true;
  if (m_Thread.joinable()) {
    m_Thread.join();
  }

  if (nullptr != m_Used) {
    addr = get(&idx, &len, 0);
    while (nullptr != addr) {
      put(idx);
      addr = get(&idx, &len, 0);
      ASLOG(VRINGI, ("vring reader %s@%u, release unconsumed buffer at %u\n", m_Name.c_str(),
                     m_ReaderIdx, idx));
    }
    __atomic_sub_fetch(&m_Used->ref, 1, __ATOMIC_RELAXED);
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

void *VRingReader::get(uint32_t *idx, uint32_t *len, uint32_t timeoutMs) {
  void *buf = nullptr;
  VRing_UsedElemType *used;
  int err = 0;

  if (0 != timeoutMs) {
    struct timespec ts;
    err = clock_gettime(CLOCK_REALTIME, &ts);
    if (0 == err) {
      ts.tv_sec += timeoutMs / 1000;
      ts.tv_nsec += (timeoutMs % 1000) * 1000000;
      (void)sem_timedwait(m_SemUsed, &ts);
    }
  }
  if (1 != __atomic_load_n(&m_Used->ref, __ATOMIC_RELAXED)) {
    ASLOG(VRINGE, ("vring reader %s@%u get killed by writer\n", m_Name.c_str(), m_ReaderIdx));
    m_Err = EBADF; /* killed by the Writer */
  } else if (m_Used->lastIdx == m_Used->idx) {
    /* no used buffer available */
  } else {
    used = &m_Used->ring[m_Used->lastIdx % m_Capability];
    *idx = used->id;
    *len = used->len;

    buf = (void *)getVA(m_Desc[*idx].addr, m_Desc[*idx].len);
    __atomic_store_n(&m_Used->lastDescId, *idx, __ATOMIC_RELAXED);
    m_Used->lastIdx++;

    /* if the app crashed after this before call the put, then the desc is in detached state
     * that need the monitor to recycle it.
     */
  }

  return buf;
}

void VRingReader::put(uint32_t idx) {
  int32_t ref;
  if (1 != __atomic_load_n(&m_Used->ref, __ATOMIC_RELAXED)) {
    m_Err = EBADF; /* killed by the Writer */
    ASLOG(VRINGE, ("vring reader %s@%u put killed by writer\n", m_Name.c_str(), m_ReaderIdx));
  } else if (idx > m_Capability) {
    assert(0);
  } else {
    spinLock(&m_Desc[idx].spin);
    ref = __atomic_sub_fetch(&m_Desc[idx].ref, 1, __ATOMIC_RELAXED);
    if (0 < ref) {
      /* still used by others */
    } else if (0 == ref) {
      spinLock(&m_Avail->spin);
      m_Avail->ring[m_Avail->idx % m_Capability] = idx;
      ASLOG(VRING, ("vring reader %s@%u: put desc idx = %u; avail: lastIdx = %u, idx = %u\n",
                    m_Name.c_str(), m_ReaderIdx, idx, m_Avail->lastIdx, m_Avail->idx));
      __atomic_store_n(&m_Used->lastDescId, m_Capability, __ATOMIC_RELAXED);
      m_Avail->idx++;
      spinUnlock(&m_Avail->spin);
    } else {
      assert(0);
      m_Err = EFAULT;
    }
    spinUnlock(&m_Desc[idx].spin);
    (void)sem_post(m_SemAvail);
  }
}

void VRingReader::threadMain() {
  while (false == m_Stop) {
    __atomic_fetch_add(&m_Used->heart, 1, __ATOMIC_RELAXED);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

} // namespace vdds
} // namespace as
