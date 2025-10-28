/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifdef USE_FLS
/* ================================ [ INCLUDES  ] ============================================== */
#include "Fls.h"
#include "Std_Debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mutex>
#include <thread>
#include "Semaphore.hpp"

#ifdef USE_TRACE_APP
#include "TraceApp_Cfg.h"
#else
#define STD_TRACE_APP(ev)
#endif

using namespace as;
/* ================================ [ MACROS    ] ============================================== */
#ifndef FLS_TOTAL_SIZE
#define FLS_TOTAL_SIZE (2 * 1024 * 1024)
#endif

#define IS_FLS_ADDRESS(a) ((a) <= FLS_TOTAL_SIZE)

#define AS_LOG_FLSAC 0

#ifndef FLS_ERASED_VALUE
#define FLS_ERASED_VALUE 0xFF
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
typedef enum {
  FLS_AC_JOB_NONE,
  FLS_AC_JOB_ERASE,
  FLS_AC_JOB_WRITE,
} FlsAc_JobType;

typedef enum {
  FLS_AC_JOB_IDEL,
  FLS_AC_JOB_ONGOING,
  FLS_AC_JOB_DONE,
  FLS_AC_JOB_FAIL,
} FlsAc_JobStatusType;
/* ================================ [ DATAS     ] ============================================== */
#ifndef FLS_AC_RAM_ONLY
static FILE *lFls = NULL;
#endif
static std::thread lThread;
static std::mutex lMutex;
static Semaphore lSem;
/* From HW perspective lJobStatus reflect the HW staus idle/busy/completed */
static FlsAc_JobStatusType lJobStatus = FLS_AC_JOB_IDEL;
static uint8_t *lData = NULL;
static Fls_AddressType lAddress;
static Fls_LengthType lLength;
static int lStoped = FALSE;
static FlsAc_JobType lJobType = FLS_AC_JOB_NONE;
uint8_t g_FlsAcMirror[FLS_TOTAL_SIZE];
/* ================================ [ LOCALS    ] ============================================== */
static void _fls_stop(void) {
#ifndef FLS_AC_RAM_ONLY
  if (NULL != lFls) {
#endif
    lStoped = TRUE;
    lJobType = FLS_AC_JOB_NONE;
    lSem.post();
    if (lThread.joinable()) {
      lThread.join();
    }
#ifndef FLS_AC_RAM_ONLY
    fclose(lFls);
  }
#endif
}

static void _fls_engine(void *arg) {
  const uint8_t EraseMask = FLS_ERASED_VALUE;
  uint8_t u8V;
  boolean conditionOk;
  while (FALSE == lStoped) {
    lSem.wait();
    std::lock_guard<std::mutex> lck(lMutex);
    switch (lJobType) {
    case FLS_AC_JOB_ERASE:
      ASLOG(FLSAC, ("erase(0x%X, %d)\n", lAddress, lLength));
#ifndef FLS_AC_RAM_ONLY
      fseek(lFls, lAddress, SEEK_SET);
#endif
      for (Fls_LengthType i = 0; i < lLength; i++) {
#ifndef FLS_AC_RAM_ONLY
        fwrite(&EraseMask, 1, 1, lFls);
#endif
        g_FlsAcMirror[lAddress + i] = EraseMask;
      }
      lJobStatus = FLS_AC_JOB_DONE;
      break;
    case FLS_AC_JOB_WRITE:
      ASLOG(FLSAC, ("write(0x%X, %p, %d)\n", lAddress, lData, lLength));
#ifndef FLS_AC_RAM_ONLY
      fseek(lFls, lAddress, SEEK_SET);
#endif
      conditionOk = TRUE;
      for (Fls_LengthType i = 0; i < lLength; i++) {
#ifndef FLS_AC_RAM_ONLY
        fread(&u8V, 1, 1, lFls);
#else
        u8V = g_FlsAcMirror[lAddress + i];
#endif
        if (u8V != FLS_ERASED_VALUE) {
          conditionOk = FALSE;
          break;
        }
      }
      if (FALSE == conditionOk) {
        ASLOG(ERROR, ("FLS write without erase @ %X\n", lAddress));
        lJobStatus = FLS_AC_JOB_FAIL;
      } else {
#ifndef FLS_AC_RAM_ONLY
        fseek(lFls, lAddress, SEEK_SET);
        fwrite(lData, lLength, 1, lFls);
#endif
        memcpy(&g_FlsAcMirror[lAddress], lData, lLength);
        lJobStatus = FLS_AC_JOB_DONE;
      }
      break;
    default:
      break;
    }
  }
}

INITIALIZER(_fls_start) {
#ifndef FLS_AC_RAM_ONLY
  size_t sz;
  uint8_t EraseMask = FLS_ERASED_VALUE;
  lFls = fopen("Fls.img", "rb+");
  if (NULL == lFls) {
    lFls = fopen("Fls.img", "wb+");
  }
  if (lFls) {
    fseek(lFls, 0, SEEK_END);
    sz = ftell(lFls);
    if (sz < FLS_TOTAL_SIZE) {
      for (size_t i = sz; i < FLS_TOTAL_SIZE; i++) {
        fwrite(&EraseMask, 1, 1, lFls);
      }
    }
    fseek(lFls, 0, SEEK_SET);
    fread(g_FlsAcMirror, FLS_TOTAL_SIZE, 1, lFls);
    lThread = std::thread(_fls_engine, nullptr);
  } else {
    ASLOG(ERROR, ("Failed to create Fls.img\n"));
  }
#else
  memset(g_FlsAcMirror, FLS_ERASED_VALUE, sizeof(g_FlsAcMirror));
  lThread = std::thread(_fls_engine, nullptr);
#endif
  atexit(_fls_stop);
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Fls_AcInit(void) {
  lJobType = FLS_AC_JOB_NONE;
}

boolean Fls_AcIsIdle(void) {
  boolean idle = TRUE;
  std::lock_guard<std::mutex> lg(lMutex);
  if ((FLS_AC_JOB_NONE != lJobType) && (lJobStatus == FLS_AC_JOB_ONGOING)) {
    idle = FALSE;
  }
  return idle;
}

Std_ReturnType Fls_AcErase(Fls_AddressType address, Fls_LengthType length) {
  Std_ReturnType r = E_NOT_OK;

  if (IS_FLS_ADDRESS(address) && IS_FLS_ADDRESS(address + length)) {
    STD_TRACE_APP(FLS_AC_ERASE);
    std::lock_guard<std::mutex> lg(lMutex);
    switch (lJobType) {
    case FLS_AC_JOB_NONE:
      lData = NULL;
      lAddress = address;
      lLength = length;
      lJobType = FLS_AC_JOB_ERASE;
      lJobStatus = FLS_AC_JOB_ONGOING;
      lSem.post();
      r = E_FLS_PENDING;
      break;
    case FLS_AC_JOB_ERASE:
      if (FLS_AC_JOB_ONGOING == lJobStatus) {
        r = E_FLS_PENDING;
      } else if (FLS_AC_JOB_DONE == lJobStatus) {
        r = E_OK;
        lJobType = FLS_AC_JOB_NONE;
      } else {
        /* Error */
      }
      break;
    default:
      break;
    }
  }

  return r;
}

Std_ReturnType Fls_AcWrite(Fls_AddressType address, const uint8_t *data, Fls_LengthType length) {
  Std_ReturnType r = E_NOT_OK;

  if (IS_FLS_ADDRESS(address) && IS_FLS_ADDRESS(address + length)) {
    STD_TRACE_APP(FLS_AC_WRITE);
    std::lock_guard<std::mutex> lg(lMutex);
    switch (lJobType) {
    case FLS_AC_JOB_NONE:
      lData = (uint8_t *)data;
      lAddress = address;
      lLength = length;
      lJobType = FLS_AC_JOB_WRITE;
      lJobStatus = FLS_AC_JOB_ONGOING;
      lSem.post();
      r = E_FLS_PENDING;
      break;
    case FLS_AC_JOB_WRITE:
      if (FLS_AC_JOB_ONGOING == lJobStatus) {
        r = E_FLS_PENDING;
      } else if (FLS_AC_JOB_DONE == lJobStatus) {
        r = E_OK;
        lJobType = FLS_AC_JOB_NONE;
      } else {
        /* Error */
      }
      break;
    default:
      break;
    }
  } else {
    r = E_NOT_OK;
  }

  return r;
}

Std_ReturnType Fls_AcRead(Fls_AddressType address, uint8_t *data, Fls_LengthType length) {
  Std_ReturnType r = E_OK;

  if (IS_FLS_ADDRESS(address) && IS_FLS_ADDRESS(address + length)) {
    ASLOG(FLSAC, ("read(0x%X, %p, %d)\n", address, data, length));
    std::lock_guard<std::mutex> lg(lMutex);
#ifndef FLS_AC_RAM_ONLY
    fseek(lFls, address, SEEK_SET);
    fread(data, length, 1, lFls);
#else
    memcpy(data, &g_FlsAcMirror[address], length);
#endif
  } else {
    r = E_NOT_OK;
  }

  return r;
}

Std_ReturnType Fls_AcCompare(Fls_AddressType address, uint8_t *data, Fls_LengthType length) {
  Std_ReturnType r = E_OK;
#ifndef FLS_AC_RAM_ONLY
  uint8_t u8V;
#endif
  if (IS_FLS_ADDRESS(address) && IS_FLS_ADDRESS(address + length)) {
    std::lock_guard<std::mutex> lg(lMutex);
#ifndef FLS_AC_RAM_ONLY
    fseek(lFls, address, SEEK_SET);
    for (Fls_LengthType i = 0; i < length; i++) {
      fread(&u8V, 1, 1, lFls);
      if (u8V != data[i]) {
        r = E_FLS_INCONSISTENT;
        break;
      }
    }
#else
    if (0 != memcmp(data, &g_FlsAcMirror[address], length)) {
      r = E_FLS_INCONSISTENT;
    }
#endif
  } else {
    r = E_NOT_OK;
  }

  return r;
}

Std_ReturnType Fls_AcBlankCheck(Fls_AddressType address, Fls_LengthType length) {
  Std_ReturnType r = E_OK;

  uint8_t u8V;

  if (IS_FLS_ADDRESS(address) && IS_FLS_ADDRESS(address + length)) {
    std::lock_guard<std::mutex> lg(lMutex);
#ifndef FLS_AC_RAM_ONLY
    fseek(lFls, address, SEEK_SET);
    for (Fls_LengthType i = 0; i < length; i++) {
      fread(&u8V, 1, 1, lFls);
      if (u8V != FLS_ERASED_VALUE) {
        r = E_FLS_INCONSISTENT;
        break;
      }
    }
#else
    for (Fls_LengthType i = 0; i < length; i++) {
      u8V = g_FlsAcMirror[address + i];
      if (u8V != FLS_ERASED_VALUE) {
        r = E_FLS_INCONSISTENT;
        break;
      }
    }
#endif
  } else {
    r = E_NOT_OK;
  }

  return r;
}
#endif
