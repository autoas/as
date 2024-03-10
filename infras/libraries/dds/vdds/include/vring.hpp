/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef _VRING_HPP_
#define _VRING_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <string>
#include <map>
#include <mutex>
#include <chrono>
#include <thread>
#include <vector>
#include <cstring>
#include <semaphore.h>

namespace as {
namespace vdds {
/* ================================ [ MACROS    ] ============================================== */
#define VRING_ALIGN(sz, align) (((sz) + (align)-1) & (~((align)-1)))

#define VRING_SIZE_OF_META(capability, align)                                                      \
  VRING_ALIGN(sizeof(VRing_MetaType) * capability, align)

#define VRING_SIZE_OF_DESC(capability, align)                                                      \
  VRING_ALIGN((sizeof(VRing_DescType) + sizeof(uint32_t) * capability) * capability, align)

#define VRING_SIZE_OF_AVAIL(capability, align)                                                     \
  VRING_ALIGN((sizeof(VRing_AvailType) + sizeof(uint32_t) * capability) * capability, align)

#define VRING_SIZE_OF_USED(capability, align)                                                      \
  VRING_ALIGN((sizeof(VRing_UsedType) + sizeof(VRing_UsedElemType) * capability) * capability,     \
              align)
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint32_t maxReaders;
  uint32_t msgSize;
  uint32_t capability;
  uint32_t align;
} VRing_MetaType;

/* Virtio ring descriptors: 16 bytes.  These can chain together via "next". */
typedef struct {
  /* Address (guest-physical). */
  uint64_t addr;
  /* Length. */
  uint32_t len;
  /* The spinlock to pretect the ref */
  int32_t spin;
  /* The reference counter */
  int32_t ref;
  /* The flags as indicated above. */
  uint32_t flags;
  /* We chain unused descriptors via this, too */
  uint32_t next;
} VRing_DescType;

typedef struct {
  uint32_t lastIdx;
  uint32_t flags;
  /* The spinlock to ensure the idx and ring content updated atomic */
  int32_t spin;
  uint32_t idx;
  uint32_t ring[];
} VRing_AvailType;

/* u32 is used here for ids for padding reasons. */
typedef struct {
  /* Index of start of used descriptor chain. */
  uint32_t id;
  /* Total length of the descriptor chain which was used (written to) */
  uint32_t len;
} VRing_UsedElemType;

typedef struct {
  int32_t ref;    /* atomic reference counter */
  uint32_t heart; /* atomic heart beat counter */
  uint32_t lastDescId;
  uint32_t lastHeart;
  uint32_t lastIdx;
  uint32_t flags;
  uint32_t idx;
  VRing_UsedElemType ring[];
} VRing_UsedType;

class VRingBase {
public:
  VRingBase(std::string name, uint32_t maxReaders = 8, uint32_t capability = 8,
            uint32_t align = 64);
  ~VRingBase();

  int getLastError() {
    return m_Err;
  }

protected:
  uint32_t size();
  void *getVA(uint64_t PA, uint32_t size, int oflag = O_RDWR);

  void spinLock(int32_t *pLock);
  void spinUnlock(int32_t *pLock);

protected:
  struct ShmInfo {
    void *addr;
    int fd;
  };

protected:
  std::string m_Name;
  uint32_t m_MaxReaders = 8;
  uint32_t m_Capability = 8;
  uint32_t m_Align = 64;

  VRing_MetaType *m_Meta = nullptr;
  VRing_DescType *m_Desc = nullptr;
  VRing_AvailType *m_Avail = nullptr;
  VRing_UsedType *m_Used = nullptr;

  void *m_SharedMemory = nullptr;
  int m_ShmFd = -1;

  int m_Err = 0;

  std::mutex m_Lock;
  std::map<uint64_t, ShmInfo> m_ShmMap;
};

/*The Virtio Ring Writer*/
class VRingWriter : public VRingBase {
public:
  VRingWriter(std::string name, uint32_t msgSize = 256 * 1024, uint32_t maxReaders = 8,
              uint32_t capability = 8, uint32_t align = 64);
  ~VRingWriter();

  /* get an avaiable buffer from the avaiable ring */
  void *get(uint32_t *idx, uint32_t *len, uint32_t timeoutMs = 1000);

  /* put the avaiable buffer to the used ring */
  void put(uint32_t idx, uint32_t len);

  /* drop the avaiable buffer to the avaiable ring */
  void drop(uint32_t idx);

private:
  void setup();
  void releaseDesc(uint32_t idx, uint32_t readerIdx, const char *prefix);
  void removeAbnormalReader(VRing_UsedType *used, uint32_t readerIdx);
  void readerHeartCheck();
  void threadMain();

private:
  uint32_t m_MsgSize; /* the size for each message */
  bool m_Stop = false;
  std::thread m_Thread;

  sem_t *m_SemAvail = nullptr;
  std::vector<sem_t *> m_UsedSems;
};

class VRingReader : public VRingBase {
public:
  VRingReader(std::string name, uint32_t maxReaders = 8, uint32_t capability = 8,
              uint32_t align = 64);
  ~VRingReader();

  /* get an buffer with data from the used ring */
  void *get(uint32_t *idx, uint32_t *len, uint32_t timeoutMs = 1000);

  /* put the buffer back to the avaiable ring */
  void put(uint32_t idx);

private:
  void threadMain();

private:
  uint32_t m_ReaderIdx;
  bool m_Stop = false;
  std::thread m_Thread;
  sem_t *m_SemUsed = nullptr;
  sem_t *m_SemAvail = nullptr;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace vdds
} // namespace as
#endif /* _VRING_HPP_ */
