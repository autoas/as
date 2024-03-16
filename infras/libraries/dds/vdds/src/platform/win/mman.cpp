/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <windows.h>

#include "Std_Debug.h"

#include <iostream>
#include <mutex>
#include <set>
#include <map>
#include <string>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_MMAN 0
#define AS_LOG_MMANE 2

#define SHM_MAX_SIZE ((uint64_t)1024 * 1024 * 1024)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static std::mutex lShmMutex;
static std::map<std::string, int> lShmNameToFdMap;
static std::map<int, HANDLE> lShmFdToHandleMap;
static int lShmFdAllocator = 0;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int shm_open(const char *name, int oflag, mode_t mode) {
  int ercd = 0;
  int fd = -1;
  HANDLE handle = nullptr;

  auto printErrorMessage = [&] {
    ASLOG(MMANE, ("Failed to create shared memory with shm_open(name = %s"
                  ", [only consider O_CREAT and O_EXCL] oflag = 0x%x"
                  ", [always assume read, write, execute for everyone] mode = 0x%x)\n",
                  name, oflag, mode));
  };

  if (oflag & O_CREAT) {
    // we do not yet support ACL and rights for data partitions in windows
    // DWORD access = (oflag & O_RDWR) ? PAGE_READWRITE : PAGE_READONLY;
    DWORD access = PAGE_READWRITE | SEC_RESERVE;
    DWORD MAXIMUM_SIZE_LOW = static_cast<DWORD>(SHM_MAX_SIZE & 0xFFFFFFFF);
    DWORD MAXIMUM_SIZE_HIGH = static_cast<DWORD>((SHM_MAX_SIZE >> 32) & 0xFFFFFFFF);

    SetLastError(0);
    handle = CreateFileMapping(static_cast<HANDLE>(INVALID_HANDLE_VALUE),
                               static_cast<LPSECURITY_ATTRIBUTES>(nullptr),
                               static_cast<DWORD>(access), static_cast<DWORD>(MAXIMUM_SIZE_HIGH),
                               static_cast<DWORD>(MAXIMUM_SIZE_LOW), static_cast<LPCSTR>(name));

    if (handle == nullptr) {
      ercd = EACCES;
      printErrorMessage();
    }

    if ((0 == ercd) && (oflag & O_EXCL) && GetLastError() == ERROR_ALREADY_EXISTS) {
      ercd = EEXIST;
      if (handle != nullptr) {
        CloseHandle(handle);
      }
    }
  } else {
    SetLastError(0);
    handle = OpenFileMapping(static_cast<DWORD>(FILE_MAP_ALL_ACCESS), static_cast<BOOL>(false),
                             static_cast<LPCSTR>(name));

    if (handle == nullptr) {
      ercd = ENOENT;
    }

    if (GetLastError() != 0) {
      printErrorMessage();
      ercd = EACCES;
      CloseHandle(handle);
    }
  }

  if (0 == ercd) {
    std::lock_guard<std::mutex> lock(lShmMutex);
    fd = ++lShmFdAllocator;
    lShmNameToFdMap[name] = fd;
    lShmFdToHandleMap[fd] = handle;
  }

  ASLOG(MMAN, ("shm_open(%s, oflag=%x, mode=%x)=%d\n", name, oflag, mode, fd));

  return fd;
}

int shm_unlink(const char *name) {
  int ercd = 0;
  int fd = -1;
  HANDLE handle = nullptr;

  std::lock_guard<std::mutex> lock(lShmMutex);
  auto iter = lShmNameToFdMap.find(name);
  if (iter != lShmNameToFdMap.end()) {
    fd = iter->second;
    auto it = lShmFdToHandleMap.find(fd);
    if (it != lShmFdToHandleMap.end()) {
      handle = it->second;
      CloseHandle(handle);
      lShmFdToHandleMap.erase(it);
    } else {
      ercd = ENOENT;
    }
    lShmNameToFdMap.erase(iter);
  } else {
    ercd = ENOENT;
  }

  ASLOG(MMAN, ("shm_unlink(%s)\n", name));
  return ercd;
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off) {
  int ercd = 0;
  void *mappedObject = nullptr;
  DWORD desiredAccess = FILE_MAP_ALL_ACCESS;
  DWORD fileOffsetHigh = 0;
  DWORD fileOffsetLow = 0;
  DWORD numberOfBytesToMap = len;
  HANDLE handle = nullptr;

  auto printErrorMessage = [&] {
    ASLOG(MMANE, ("Failed to map file mapping into process space with mmap(addr = %x, length = "
                  "%d, [always assume PROT_READ | PROT_WRITE] prot = %x, [always assume "
                  "MAP_SHARED] flags = %x, fd = %d, [always assume 0] offset = %d)\n",
                  addr, len, prot, flags, fd, off));
  };

  std::lock_guard<std::mutex> lock(lShmMutex);
  auto it = lShmFdToHandleMap.find(fd);
  if (it != lShmFdToHandleMap.end()) {
    handle = it->second;
  } else {
    ercd = ENOENT;
  }

  if (0 == ercd) {
    mappedObject =
      MapViewOfFile(handle, desiredAccess, fileOffsetHigh, fileOffsetLow, numberOfBytesToMap);
    if (mappedObject == nullptr) {
      printErrorMessage();
      ercd = EACCES;
    }
  }

  if (0 == ercd) {
    // windows only reserves memory but does not allocate it right away (see SEC_RESERVE in
    // shm_open) this call actually allocates the right amount of bytes
    mappedObject = VirtualAlloc(mappedObject, numberOfBytesToMap, MEM_COMMIT, PAGE_READWRITE);
    if (mappedObject == nullptr) {
      printErrorMessage();
      ercd = ENOMEM;
    }
  }

  ASLOG(MMAN, ("shm mmap fd=%d, vaddr=%p, size=%d\n", fd, mappedObject, (int)len));
  return mappedObject;
}

int munmap(void *addr, size_t len) {
  int ercd = 0;
  BOOL rv;

  rv = UnmapViewOfFile(addr);
  if (false == rv) {
    ASLOG(MMANE,
          ("Failed to unmap memory region with munmap( addr = %p, length = %d)", addr, (int)len));
    ercd = EFAULT;
  }

  ASLOG(MMAN, ("shm munmap vaddr=%p, size=%d\n", addr, (int)len));

  return ercd;
}
