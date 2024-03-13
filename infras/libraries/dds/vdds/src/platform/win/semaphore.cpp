/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <windows.h>
#include <synchapi.h>
#include <sddl.h>

#include "Std_Debug.h"

#include <iostream>
#include <mutex>
#include <set>
#include <map>
#include <string>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_SEM 0
#define AS_LOG_SEME 2
/* ================================ [ TYPES     ] ============================================== */

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static std::mutex lSemMutex;
static std::map<std::string, HANDLE> lSemNameToHandleMap;
/* ================================ [ LOCALS    ] ============================================== */
static HANDLE sem_create_win32_semaphore(LONG value, LPCSTR name) {
  SECURITY_ATTRIBUTES securityAttribute;
  SECURITY_DESCRIPTOR securityDescriptor;
  InitializeSecurityDescriptor(&securityDescriptor, SECURITY_DESCRIPTOR_REVISION);

  auto permissions = TEXT("D:") TEXT("(A;OICI;GA;;;BG)") // access to built-in guests
    TEXT("(A;OICI;GA;;;AN)")                             // access to anonymous logon
    TEXT("(A;OICI;GRGWGX;;;AU)")                         // access to authenticated users
    TEXT("(A;OICI;GA;;;BA)");                            // access to administrators

  ConvertStringSecurityDescriptorToSecurityDescriptor(
    reinterpret_cast<LPCSTR>(permissions), static_cast<DWORD>(SDDL_REVISION_1),
    static_cast<PSECURITY_DESCRIPTOR *>(&(securityAttribute.lpSecurityDescriptor)),
    static_cast<PULONG>(NULL));
  securityAttribute.nLength = sizeof(SECURITY_ATTRIBUTES);
  securityAttribute.lpSecurityDescriptor = &securityDescriptor;
  securityAttribute.bInheritHandle = FALSE;

  HANDLE returnValue = CreateSemaphoreA(&securityAttribute, value, LONG_MAX, name);
  return returnValue;
}
/* ================================ [ FUNCTIONS ] ============================================== */
sem_t *sem_open(const char *name, int oflag, mode_t mode, unsigned int value) {
  int ercd = 0;
  HANDLE handle = nullptr;
  if (oflag & (O_CREAT | O_EXCL)) {
    handle = sem_create_win32_semaphore(value, name);
    if ((oflag & O_EXCL) && (GetLastError() == ERROR_ALREADY_EXISTS)) {
      sem_close((sem_t *)handle);
      ercd = EEXIST;
    } else if (nullptr == handle) {
      ercd = EFAULT;
    }
  } else {
    handle = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, false, name);
    if (nullptr == handle) {
      ercd = EACCES;
    }
  }
  ASLOG(SEM, ("sem_open(%s, %X, %X, %u) %p = %d\n", name, oflag, mode, value, handle, ercd));
  return (sem_t *)handle;
}

int sem_close(sem_t *sem) {
  int ercd = 0;
  BOOL rv = CloseHandle((HANDLE)sem);
  if (false == rv) {
    ercd = EACCES;
  }
  return ercd;
}

int sem_post(sem_t *sem) {
  int ercd = 0;
  BOOL rv = ReleaseSemaphore((HANDLE)sem, 1, nullptr);
  if (false == rv) {
    ASLOG(SEME, ("sem_post(%p) = %d\n", sem, GetLastError()));
    ercd = EACCES;
  }
  return ercd;
}

int sem_wait(sem_t *sem) {
  int ercd = 0;
  DWORD dwWaitResult = WaitForSingleObject((HANDLE)sem, INFINITE);
  if (WAIT_OBJECT_0 == dwWaitResult) {
  } else if (WAIT_TIMEOUT == dwWaitResult) {
    ercd = ETIME;
  } else {
    ASLOG(SEME, ("sem_wait(%p) = %d\n", sem, dwWaitResult));
    ercd = EACCES;
  }

  ASLOG(SEM, ("sem_wait(%p) = %d\n", sem, GetLastError()));
  return ercd;
}

int sem_timedwait(sem_t *sem, const struct timespec *abstime) {
  int ercd = 0;
  struct timespec ts;
  DWORD timeoutMs = 0;
  clock_gettime(CLOCK_REALTIME, &ts);
  if (abstime->tv_sec > ts.tv_sec) {
    timeoutMs += 1000 * (abstime->tv_sec - ts.tv_sec);
  }
  if (abstime->tv_nsec > ts.tv_nsec) {
    timeoutMs += (abstime->tv_sec - ts.tv_sec) / 1000000;
  }
  DWORD dwWaitResult = WaitForSingleObject((HANDLE)sem, timeoutMs);
  if (WAIT_OBJECT_0 == dwWaitResult) {
  } else if (WAIT_TIMEOUT == dwWaitResult) {
    ercd = ETIMEDOUT;
  } else {
    ASLOG(SEME, ("sem_timedwait(%p) = %d\n", sem, dwWaitResult));
    ercd = EACCES;
  }

  ASLOG(SEM, ("sem_timedwait(%p) = %d\n", sem, GetLastError()));
  return ercd;
}