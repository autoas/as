/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <sys/queue.h>
#include "osal.h"
#include "Std_Types.h"
#include <stdlib.h>
#include <windows.h>
#include <shlwapi.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#if 0
OSAL_ThreadType OSAL_ThreadCreate(OSAL_ThreadEntryType entry, void *args) {
  OSAL_ThreadType thread = NULL;

  thread = (OSAL_ThreadType)CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)entry, args, 0, NULL);

  return thread;
}

int OSAL_ThreadJoin(OSAL_ThreadType thread) {
  int ret = 0;
  int timeout = 500;
  DWORD exitCode;
  BOOL rv;

  do {
    Sleep(100); /* workaround to wait 100ms to ensure thread exit */
    rv = GetExitCodeThread((HANDLE)thread, &exitCode);
    if (FALSE == rv) {
      if (timeout > 0) {
        timeout--;
        Sleep(10);
      } else {
        ret = -ETIMEDOUT;
        break;
      }
    }
  } while (FALSE == rv);

  return ret;
}

int OSAL_ThreadDestory(OSAL_ThreadType thread) {
  int ret = 0;
  BOOL rv = CloseHandle((HANDLE)thread);
  if (TRUE == rv) {
    ret = -EFAULT;
  }
  return ret;
}
#endif
void OSAL_SleepUs(uint32_t us) {
  Sleep(us / 1000);
}

void OSAL_Start(void) {
}

#if 0
int OSAL_MutexAttrInit(OSAL_MutexAttrType *attr) {
  attr->type = OSAL_MUTEX_NORMAL;
  return 0;
}

int OSAL_MutexAttrSetType(OSAL_MutexAttrType *attr, int type) {
  attr->type = OSAL_MUTEX_RECURSIVE;
  return 0;
}

OSAL_MutexType OSAL_MutexCreate(OSAL_MutexAttrType *attr) {
  OSAL_MutexType mutex = NULL;

  mutex = (OSAL_MutexType)CreateMutex(NULL, TRUE, NULL);

  return mutex;
}

int OSAL_MutexLock(OSAL_MutexType mutex) {
  return WaitForSingleObject((HANDLE)mutex, INFINITE);
}

int OSAL_MutexUnlock(OSAL_MutexType mutex) {
  return ReleaseMutex((HANDLE)mutex);
}

int OSAL_MutexDestory(OSAL_MutexType mutex) {
  int ret = 0;
  BOOL rv = CloseHandle((HANDLE)mutex);
  if (TRUE == rv) {
    ret = -EFAULT;
  }
  return ret;
}
#endif

OSAL_SemType OSAL_SemaphoreCreate(int value) {
  OSAL_SemType sem = NULL;

  sem = (OSAL_SemType)CreateSemaphoreA(NULL, value, LONG_MAX, NULL);

  return sem;
}

int OSAL_SemaphoreWait(OSAL_SemType sem) {
  return WaitForSingleObject((HANDLE)sem, INFINITE);
}

int OSAL_SemaphoreTryWait(OSAL_SemType sem) {
  return WaitForSingleObject((HANDLE)sem, 0);
}

int OSAL_SemaphoreTimedWait(OSAL_SemType sem, uint32_t timeoutMs) {
  return WaitForSingleObject((HANDLE)sem, timeoutMs);
}

int OSAL_SemaphorePost(OSAL_SemType sem) {
  return ReleaseSemaphore((HANDLE)sem, 1, NULL);
}

int OSAL_SemaphoreDestory(OSAL_SemType sem) {
  int ret = 0;
  BOOL rv = CloseHandle((HANDLE)sem);
  if (TRUE == rv) {
    ret = -EFAULT;
  }
  return ret;
}

bool OSAL_FileExists(const char *file) {
  return PathFileExistsA(file);
}

void *OSAL_DlOpen(const char *path) {
  return LoadLibrary(path);
}

void *OSAL_DlSym(void *dll, const char *symbol) {
  return GetProcAddress(dll, symbol);
}

void OSAL_DlClose(void *dll) {
  FreeLibrary(dll);
}
