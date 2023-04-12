/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifdef USE_EEP
/* ================================ [ INCLUDES  ] ============================================== */
#include "Eep.h"
#include "Std_Debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define EEP_TOTAL_SIZE (4 * 1024)

#define IS_EEP_ADDRESS(a) ((a) <= EEP_TOTAL_SIZE)

#define AS_LOG_EEPAC 0
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
typedef enum
{
  EEP_AC_JOB_NONE,
  EEP_AC_JOB_ERASE,
  EEP_AC_JOB_WRITE,
} EepAc_JobType;

typedef enum
{
  EEP_AC_JOB_IDEL,
  EEP_AC_JOB_ONGOING,
  EEP_AC_JOB_DONE,
  EEP_AC_JOB_FAIL,
} EepAc_JobStatusType;
/* ================================ [ DATAS     ] ============================================== */
static FILE *lEep = NULL;
static pthread_t lThread;
static pthread_mutex_t lMutex;
static sem_t lSem;
/* From HW perspective lJobStatus reflect the HW staus idle/busy/completed */
static EepAc_JobStatusType lJobStatus = EEP_AC_JOB_IDEL;
static uint8_t *lData = NULL;
static Eep_AddressType lAddress;
static Eep_LengthType lLength;
static int lStoped = FALSE;
static EepAc_JobType lJobType = EEP_AC_JOB_NONE;
uint8_t g_EepAcMirror[EEP_TOTAL_SIZE];
/* ================================ [ LOCALS    ] ============================================== */
static void _eep_stop(void) {
  if (NULL != lEep) {
    lStoped = TRUE;
    lJobType = EEP_AC_JOB_NONE;
    sem_post(&lSem);
    pthread_join(lThread, NULL);
    fclose(lEep);
  }
}

static void *_eep_engine(void *arg) {
  const uint8_t EraseMask = 0xFF;
  uint8_t u8V;
  boolean conditionOk;
  while (FALSE == lStoped) {
    sem_wait(&lSem);
    pthread_mutex_lock(&lMutex);
    switch (lJobType) {
    case EEP_AC_JOB_ERASE:
      ASLOG(EEPAC, ("erase(0x%X, %d)\n", lAddress, lLength));
      fseek(lEep, lAddress, SEEK_SET);
      for (int i = 0; i < lLength; i++) {
        fwrite(&EraseMask, 1, 1, lEep);
        g_EepAcMirror[lAddress + i] = EraseMask;
      }
      lJobStatus = EEP_AC_JOB_DONE;
      break;
    case EEP_AC_JOB_WRITE:
      ASLOG(EEPAC, ("write(0x%X, %p, %d)\n", lAddress, lData, lLength));
      fseek(lEep, lAddress, SEEK_SET);
      conditionOk = TRUE;
      for (int i = 0; i < lLength; i++) {
        fread(&u8V, 1, 1, lEep);
        if (u8V != 0xFF) {
          conditionOk = FALSE;
          break;
        }
      }
      if (FALSE == conditionOk) {
        ASLOG(ERROR, ("EEP write without erase\n"));
        lJobStatus = EEP_AC_JOB_FAIL;
      } else {
        fseek(lEep, lAddress, SEEK_SET);
        fwrite(lData, lLength, 1, lEep);
        memcpy(&g_EepAcMirror[lAddress], lData, lLength);
        lJobStatus = EEP_AC_JOB_DONE;
      }
      break;
    default:
      break;
    }
    pthread_mutex_unlock(&lMutex);
  }

  return NULL;
}

static void __attribute__((constructor)) _eep_start(void) {
  size_t sz;
  uint8_t EraseMask = 0xFF;
  lEep = fopen("Eep.img", "rb+");
  if (NULL == lEep) {
    lEep = fopen("Eep.img", "wb+");
  }
  if (lEep) {
    fseek(lEep, 0, SEEK_END);
    sz = ftell(lEep);
    if (sz < EEP_TOTAL_SIZE) {
      for (size_t i = sz; i < EEP_TOTAL_SIZE; i++) {
        fwrite(&EraseMask, 1, 1, lEep);
      }
    }
    fseek(lEep, 0, SEEK_SET);
    fread(g_EepAcMirror, EEP_TOTAL_SIZE, 1, lEep);
    pthread_mutex_init(&lMutex, NULL);
    sem_init(&lSem, 0, 0);
    pthread_create(&lThread, NULL, _eep_engine, NULL);
  } else {
    ASLOG(ERROR, ("Failed to create Eep.img\n"));
  }
  atexit(_eep_stop);
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Eep_AcInit(void) {
}

Std_ReturnType Eep_AcErase(Eep_AddressType address, Eep_LengthType length) {
  Std_ReturnType r = E_NOT_OK;

  if (IS_EEP_ADDRESS(address) && IS_EEP_ADDRESS(address + length) && (NULL != lEep)) {
    pthread_mutex_lock(&lMutex);
    switch (lJobType) {
    case EEP_AC_JOB_NONE:
      lData = NULL;
      lAddress = address;
      lLength = length;
      lJobType = EEP_AC_JOB_ERASE;
      lJobStatus = EEP_AC_JOB_ONGOING;
      sem_post(&lSem);
      r = E_EEP_PENDING;
      break;
    case EEP_AC_JOB_ERASE:
      if (EEP_AC_JOB_ONGOING == lJobStatus) {
        r = E_EEP_PENDING;
      } else if (EEP_AC_JOB_DONE == lJobStatus) {
        r = E_OK;
        lJobType = EEP_AC_JOB_NONE;
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

Std_ReturnType Eep_AcWrite(Eep_AddressType address, const uint8_t *data, Eep_LengthType length) {
  Std_ReturnType r = E_NOT_OK;

  if (IS_EEP_ADDRESS(address) && IS_EEP_ADDRESS(address + length) && (NULL != lEep)) {
    pthread_mutex_lock(&lMutex);
    switch (lJobType) {
    case EEP_AC_JOB_NONE:
      lData = (uint8_t *)data;
      lAddress = address;
      lLength = length;
      lJobType = EEP_AC_JOB_WRITE;
      lJobStatus = EEP_AC_JOB_ONGOING;
      sem_post(&lSem);
      r = E_EEP_PENDING;
      break;
    case EEP_AC_JOB_WRITE:
      if (EEP_AC_JOB_ONGOING == lJobStatus) {
        r = E_EEP_PENDING;
      } else if (EEP_AC_JOB_DONE == lJobStatus) {
        r = E_OK;
        lJobType = EEP_AC_JOB_NONE;
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

Std_ReturnType Eep_AcRead(Eep_AddressType address, uint8_t *data, Eep_LengthType length) {
  Std_ReturnType r = E_OK;

  if (IS_EEP_ADDRESS(address) && IS_EEP_ADDRESS(address + length) && (NULL != lEep)) {
    ASLOG(EEPAC, ("read(0x%X, %p, %d)\n", address, data, length));
    pthread_mutex_lock(&lMutex);
    fseek(lEep, address, SEEK_SET);
    fread(data, length, 1, lEep);
    pthread_mutex_unlock(&lMutex);
  } else {
    r = E_NOT_OK;
  }

  return r;
}

Std_ReturnType Eep_AcCompare(Eep_AddressType address, uint8_t *data, Eep_LengthType length) {
  Std_ReturnType r = E_OK;

  uint8_t u8V;

  if (IS_EEP_ADDRESS(address) && IS_EEP_ADDRESS(address + length) && (NULL != lEep)) {
    pthread_mutex_lock(&lMutex);
    fseek(lEep, address, SEEK_SET);
    for (int i = 0; i < length; i++) {
      fread(&u8V, 1, 1, lEep);
      if (u8V != data[i]) {
        r = E_EEP_INCONSISTENT;
        break;
      }
    }
    pthread_mutex_unlock(&lMutex);
  } else {
    r = E_NOT_OK;
  }

  return r;
}

Std_ReturnType Eep_AcBlankCheck(Eep_AddressType address, Eep_LengthType length) {
  Std_ReturnType r = E_OK;

  uint8_t u8V;

  if (IS_EEP_ADDRESS(address) && IS_EEP_ADDRESS(address + length) && (NULL != lEep)) {
    pthread_mutex_lock(&lMutex);
    fseek(lEep, address, SEEK_SET);
    for (int i = 0; i < length; i++) {
      fread(&u8V, 1, 1, lEep);
      if (u8V != 0xFF) {
        r = E_EEP_INCONSISTENT;
        break;
      }
    }
    pthread_mutex_unlock(&lMutex);
  } else {
    r = E_NOT_OK;
  }

  return r;
}
#endif
