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
#include <array>
#include <cstring>
#include <semaphore.h>

namespace as {
namespace vdds {
/* ================================ [ MACROS    ] ============================================== */
#ifndef VRING_ALIGNMENT
#define VRING_ALIGNMENT 64
#endif

#ifndef VRING_MAX_READERS
#define VRING_MAX_READERS 8
#endif

#define VRING_ALIGN(sz) (((sz) + (VRING_ALIGNMENT)-1) & (~((VRING_ALIGNMENT)-1)))

#define VRING_SIZE_OF_META(numDesc) VRING_ALIGN(sizeof(VRing_MetaType) * numDesc)

#define VRING_SIZE_OF_DESC(numDesc)                                                                \
  VRING_ALIGN((sizeof(VRing_DescType) + sizeof(uint32_t) * numDesc) * numDesc)

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
  uint64_t timestamp; /* timestamp in microseconds when publish this DESC */
  uint64_t addr;      /* Address (guest-physical). */
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

class VRingBase {
public:
  VRingBase(std::string name, uint32_t numDesc = 8);
  ~VRingBase();

  uint64_t timestamp();

protected:
  uint32_t size();
  void *getVA(uint64_t PA, uint32_t size, int oflag = O_RDWR);

  int spinLock(int32_t *pLock);
  void spinUnlock(int32_t *pLock);

protected:
  struct ShmInfo {
    void *addr;
    int fd;
  };

protected:
  std::string m_Name;
  uint32_t m_NumDesc = 8;

  VRing_MetaType *m_Meta = nullptr;
  VRing_DescType *m_Desc = nullptr;
  VRing_AvailType *m_Avail = nullptr;
  VRing_UsedType *m_Used = nullptr;

  void *m_SharedMemory = nullptr;
  int m_ShmFd = -1;

  std::mutex m_Lock;
  std::map<uint64_t, ShmInfo> m_ShmMap;
};

/*The Virtio Ring Writer*/
class VRingWriter : public VRingBase {
public:
  VRingWriter(std::string name, uint32_t msgSize = 256 * 1024, uint32_t numDesc = 8);
  ~VRingWriter();

  int init();

  /* get an avaiable buffer from the avaiable ring
   * Positive errors: ETIMEDOUT, ENODATA
   */
  int get(void *&buf, uint32_t &idx, uint32_t &len, uint32_t timeoutMs = 1000);

  /* put the avaiable buffer to the used ring */
  int put(uint32_t idx, uint32_t len);

  /* drop the avaiable buffer to the avaiable ring */
  int drop(uint32_t idx);

private:
  int setup();
  void releaseDesc(uint32_t idx);
  void removeAbnormalReader(VRing_UsedType *used, uint32_t readerIdx);
  void readerHeartCheck();
  void checkDescLife();
  void threadMain();

private:
  uint32_t m_MsgSize; /* the size for each message */
  bool m_Stop = false;
  std::thread m_Thread;

  sem_t *m_SemAvail = nullptr;
  std::array<sem_t *, VRING_MAX_READERS> m_UsedSems;
};

class VRingReader : public VRingBase {
public:
  VRingReader(std::string name, uint32_t numDesc = 8);
  ~VRingReader();

  int init();

  /* get an buffer with data from the used ring
   * Positive errors: ETIMEDOUT, ENOMSG */
  int get(void *&buf, uint32_t &idx, uint32_t &len, uint32_t timeoutMs = 1000);

  /* put the buffer back to the avaiable ring */
  int put(uint32_t idx);

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
