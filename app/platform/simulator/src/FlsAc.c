/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Fls.h"
#include "Std_Debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define FLS_TOTAL_SIZE (1 * 1024 * 1024)

#define IS_FLS_ADDRESS(a) ((a) <= FLS_TOTAL_SIZE)

#define AS_LOG_FLSAC 0
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
typedef enum
{
  FLS_AC_JOB_NONE,
  FLS_AC_JOB_ERASE,
  FLS_AC_JOB_WRITE,
} FlsAc_JobType;

typedef enum
{
  FLS_AC_JOB_IDEL,
  FLS_AC_JOB_ONGOING,
  FLS_AC_JOB_DONE,
  FLS_AC_JOB_FAIL,
} FlsAc_JobStatusType;
/* ================================ [ DATAS     ] ============================================== */
static FILE *lFls = NULL;
static pthread_t lThread;
static pthread_mutex_t lMutex;
static sem_t lSem;
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
  if (NULL != lFls) {
    lStoped = TRUE;
    lJobType = FLS_AC_JOB_NONE;
    sem_post(&lSem);
    pthread_join(lThread, NULL);
    fclose(lFls);
  }
}

static void *_fls_engine(void *arg) {
  const uint8_t EraseMask = 0xFF;
  uint8_t u8V;
  boolean conditionOk;
  while (FALSE == lStoped) {
    sem_wait(&lSem);
    pthread_mutex_lock(&lMutex);
    switch (lJobType) {
    case FLS_AC_JOB_ERASE:
      ASLOG(FLSAC, ("erase(0x%X, %d)\n", lAddress, lLength));
      fseek(lFls, lAddress, SEEK_SET);
      for (int i = 0; i < lLength; i++) {
        fwrite(&EraseMask, 1, 1, lFls);
        g_FlsAcMirror[lAddress + i] = EraseMask;
      }
      lJobStatus = FLS_AC_JOB_DONE;
      break;
    case FLS_AC_JOB_WRITE:
      ASLOG(FLSAC, ("write(0x%X, %p, %d)\n", lAddress, lData, lLength));
      fseek(lFls, lAddress, SEEK_SET);
      conditionOk = TRUE;
      for (int i = 0; i < lLength; i++) {
        fread(&u8V, 1, 1, lFls);
        if (u8V != 0xFF) {
          conditionOk = FALSE;
          break;
        }
      }
      if (FALSE == conditionOk) {
        ASLOG(ERROR, ("FLS write without erase\n"));
        lJobStatus = FLS_AC_JOB_FAIL;
      } else {
        fseek(lFls, lAddress, SEEK_SET);
        fwrite(lData, lLength, 1, lFls);
        memcpy(&g_FlsAcMirror[lAddress], lData, lLength);
        lJobStatus = FLS_AC_JOB_DONE;
      }
      break;
    default:
      break;
    }
    pthread_mutex_unlock(&lMutex);
  }

  return NULL;
}
#ifdef USE_FLS
static void __attribute__((constructor)) _fls_start(void) {
  size_t sz;
  uint8_t EraseMask = 0xFF;
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
    pthread_mutex_init(&lMutex, NULL);
    sem_init(&lSem, 0, 0);
    pthread_create(&lThread, NULL, _fls_engine, NULL);
  } else {
    ASLOG(ERROR, ("Failed to create Fls.img\n"));
  }
  atexit(_fls_stop);
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
void Fls_AcInit(void) {
}

Std_ReturnType Fls_AcErase(Fls_AddressType address, Fls_LengthType length) {
  Std_ReturnType r = E_NOT_OK;

  if (IS_FLS_ADDRESS(address) && IS_FLS_ADDRESS(address + length) && (NULL != lFls)) {
    pthread_mutex_lock(&lMutex);
    switch (lJobType) {
    case FLS_AC_JOB_NONE:
      lData = NULL;
      lAddress = address;
      lLength = length;
      lJobType = FLS_AC_JOB_ERASE;
      lJobStatus = FLS_AC_JOB_ONGOING;
      sem_post(&lSem);
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
    pthread_mutex_unlock(&lMutex);
  }

  return r;
}

Std_ReturnType Fls_AcWrite(Fls_AddressType address, const uint8_t *data, Fls_LengthType length) {
  Std_ReturnType r = E_NOT_OK;

  if (IS_FLS_ADDRESS(address) && IS_FLS_ADDRESS(address + length) && (NULL != lFls)) {
    pthread_mutex_lock(&lMutex);
    switch (lJobType) {
    case FLS_AC_JOB_NONE:
      lData = (uint8_t *)data;
      lAddress = address;
      lLength = length;
      lJobType = FLS_AC_JOB_WRITE;
      lJobStatus = FLS_AC_JOB_ONGOING;
      sem_post(&lSem);
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
    pthread_mutex_unlock(&lMutex);
  } else {
    r = E_NOT_OK;
  }

  return r;
}

Std_ReturnType Fls_AcRead(Fls_AddressType address, uint8_t *data, Fls_LengthType length) {
  Std_ReturnType r = E_OK;

  if (IS_FLS_ADDRESS(address) && IS_FLS_ADDRESS(address + length) && (NULL != lFls)) {
    ASLOG(FLSAC, ("read(0x%X, %p, %d)\n", address, data, length));
    pthread_mutex_lock(&lMutex);
    fseek(lFls, address, SEEK_SET);
    fread(data, length, 1, lFls);
    pthread_mutex_unlock(&lMutex);
  } else {
    r = E_NOT_OK;
  }

  return r;
}

Std_ReturnType Fls_AcCompare(Fls_AddressType address, uint8_t *data, Fls_LengthType length) {
  Std_ReturnType r = E_OK;

  uint8_t u8V;

  if (IS_FLS_ADDRESS(address) && IS_FLS_ADDRESS(address + length) && (NULL != lFls)) {
    pthread_mutex_lock(&lMutex);
    fseek(lFls, address, SEEK_SET);
    for (int i = 0; i < length; i++) {
      fread(&u8V, 1, 1, lFls);
      if (u8V != data[i]) {
        r = E_FLS_INCONSISTENT;
        break;
      }
    }
    pthread_mutex_unlock(&lMutex);
  } else {
    r = E_NOT_OK;
  }

  return r;
}

Std_ReturnType Fls_AcBlankCheck(Fls_AddressType address, Fls_LengthType length) {
  Std_ReturnType r = E_OK;

  uint8_t u8V;

  if (IS_FLS_ADDRESS(address) && IS_FLS_ADDRESS(address + length) && (NULL != lFls)) {
    pthread_mutex_lock(&lMutex);
    fseek(lFls, address, SEEK_SET);
    for (int i = 0; i < length; i++) {
      fread(&u8V, 1, 1, lFls);
      if (u8V != 0xFF) {
        r = E_FLS_INCONSISTENT;
        break;
      }
    }
    pthread_mutex_unlock(&lMutex);
  } else {
    r = E_NOT_OK;
  }

  return r;
}