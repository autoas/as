/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifdef USE_LINTP
/* ================================ [ INCLUDES  ] ============================================== */
#include "isotp.h"
#include "isotp_types.h"
#include "Lin.h"
#ifndef USE_LINIF
#define LinIf_ScheduleRequest LinIf_TpScheduleRequest
#endif
#include "LinIf.h"
#include "linlib.h"
#include "devlib.h"
#include "LinTp.h"
#include "Std_Debug.h"
#include "Std_Timer.h"
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
/* ================================ [ MACROS    ] ============================================== */
/* ID start from 1 as 0 mean LINIF_NULL_SCHEDULE */
#define LINIF_SCH_TABLE_APPLICATIVE 1
#define LINIF_SCH_TABLE_DIAG_REQUEST 2
#define LINIF_SCH_TABLE_DIAG_RESPONSE 3

#define UDS_TIMEOUT 5000000
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
void LinTp_ReConfig(uint8_t Channel, uint8_t ll_dl, uint16_t N_TA);
#ifdef USE_LINIF
void LinIf_ReConfig(uint8_t Channel, uint8_t ll_dl, uint32_t rxid, uint32_t txid);
void Lin_ReConfig(uint8_t Controller, const char *device, int port, uint32_t baudrate);
#endif

Std_ReturnType LinIf_DiagMRFCallback(uint8_t channel, Lin_PduType *frame,
                                     Std_ReturnType notifyResult);
Std_ReturnType LinIf_DiagSRFCallback(uint8_t channel, Lin_PduType *frame,
                                     Std_ReturnType notifyResult);
/* ================================ [ DATAS     ] ============================================== */
static isotp_t lIsoTp;
static int lServerUp = FALSE;
static pthread_mutex_t lMutex = PTHREAD_MUTEX_INITIALIZER;
#ifndef USE_LINIF
static LinIf_SchHandleType lDiagRequestType = LINIF_NULL_SCHEDULE;
#endif
/* ================================ [ LOCALS    ] ============================================== */
#ifndef USE_LINIF
Std_ReturnType LinIf_ScheduleRequest(NetworkHandleType Channel, LinIf_SchHandleType Schedule) {
  lDiagRequestType = Schedule;
  return E_OK;
}

static void lin_sched(int busId, isotp_t *isotp) {
  uint8_t data[64];
  Lin_PduType frame;
  Std_ReturnType r;
  bool ret;
  bool enahnced = true;
  int timeout = isotp->params.U.LIN.timeout + isotp->params.U.LIN.timeout / 5; /* ms */
  frame.SduPtr = data;
  frame.Dl = isotp->params.ll_dl;
  if (LINIF_SCH_TABLE_DIAG_REQUEST == lDiagRequestType) {
    r = LinIf_DiagMRFCallback(0, &frame, LINIF_R_TRIGGER_TRANSMIT);
    if (LINIF_R_OK == r) {
      ret = lin_write(busId, isotp->params.U.LIN.TxId, frame.Dl, frame.SduPtr, enahnced);
      if (false == ret) {
        lIsoTp.result = -__LINE__;
        Std_TimerStop(&lIsoTp.timerErrorNotify);
        LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
        sem_post(&lIsoTp.sem);
      }
    }
  } else if (LINIF_SCH_TABLE_DIAG_RESPONSE == lDiagRequestType) {
    ret = lin_read(busId, isotp->params.U.LIN.RxId, frame.Dl, frame.SduPtr, enahnced, timeout);
    if (ret) {
      (void)LinIf_DiagSRFCallback(0, &frame, LINIF_R_RECEIVED_OK);
    }
  } else {
  }
}

uint32_t LinIf_CanTpGetTxId(uint8_t Channel) {
  return lIsoTp.params.U.LIN.TxId;
}

uint32_t LinIf_CanTpGetRxId(uint8_t Channel) {
  return lIsoTp.params.U.LIN.RxId;
}
#endif
static void *lin_server_main(void *args) {
  isotp_t *isotp = (isotp_t *)args;
  Std_TimerType timer10ms;
#ifndef USE_LINIF
  int busId = -1;
  uint32_t timeout = isotp->params.U.LIN.timeout * 1000;
#endif

  LinTp_ReConfig(0, (uint8_t)isotp->params.ll_dl, (uint16_t)isotp->params.N_TA);
#ifdef USE_LINIF
  LinIf_ReConfig(0, (uint8_t)isotp->params.ll_dl, isotp->params.U.LIN.RxId,
                 isotp->params.U.LIN.TxId);
  Lin_ReConfig(0, isotp->params.device, isotp->params.port, isotp->params.baudrate);
#else
  busId = lin_open(isotp->params.device, isotp->params.port, isotp->params.baudrate);
  if (busId < 0) {
    isotp->result = -__LINE__;
    sem_post(&isotp->sem);
    return NULL;
  }
  dev_ioctl(busId, DEV_IOCTL_SET_TIMEOUT, &timeout, sizeof(timeout));
#endif
#ifdef USE_LINIF
  Lin_Init(NULL);
  LinIf_Init(NULL);
#endif
  LinTp_Init(NULL);
  Std_TimerStart(&timer10ms);
  Std_TimerStop(&isotp->timerErrorNotify);

  lServerUp = TRUE;
  sem_post(&isotp->sem);

  while (lServerUp) {
    if (Std_GetTimerElapsedTime(&timer10ms) >= 10000) {
      pthread_mutex_lock(&lMutex);
      LinTp_MainFunction();
      pthread_mutex_unlock(&lMutex);
      Std_TimerStart(&timer10ms);
    }

    pthread_mutex_lock(&lMutex);
#ifdef USE_LINIF
    Lin_MainFunction();
    LinIf_MainFunction();
    Lin_MainFunction_Read();
#else
    lin_sched(busId, isotp);
#endif
    pthread_mutex_unlock(&lMutex);

    pthread_mutex_lock(&lMutex);
    if (Std_IsTimerStarted(&isotp->timerErrorNotify)) {
      if (Std_GetTimerElapsedTime(&isotp->timerErrorNotify) >= isotp->errorTimeout) {
        isotp->result = -__LINE__;
        LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
        Std_TimerStop(&isotp->timerErrorNotify);
        sem_post(&isotp->sem);
      }
    }
    pthread_mutex_unlock(&lMutex);

    usleep(1000);
  }

#ifdef USE_LINIF
  LinIf_DeInit();
#else
  lin_close(busId);
#endif

  return NULL;
}
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType LinIf_ApplicativeCallback(uint8_t channel, Lin_PduType *frame,
                                         Std_ReturnType notifyResult) {
  assert(0);
  return E_NOT_OK;
}

Std_ReturnType LinIf_DiagMRFCallback(uint8_t channel, Lin_PduType *frame,
                                     Std_ReturnType notifyResult) {
  Std_ReturnType r = LINIF_R_NOT_OK;
  Std_ReturnType ret;
  PduInfoType pduInfo;

  if (LINIF_R_TRIGGER_TRANSMIT == notifyResult) {
    pduInfo.SduDataPtr = frame->SduPtr;
    pduInfo.SduLength = frame->Dl;
    if (lIsoTp.params.ll_dl == pduInfo.SduLength) {
      ret = LinTp_TriggerTransmit(0, &pduInfo);
      if (E_OK == ret) {
        r = LINIF_R_OK;
      } else {
        lIsoTp.result = -__LINE__;
        Std_TimerStop(&lIsoTp.timerErrorNotify);
        LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
        sem_post(&lIsoTp.sem);
      }
    }
  }

  return r;
}

Std_ReturnType LinIf_DiagSRFCallback(uint8_t channel, Lin_PduType *frame,
                                     Std_ReturnType notifyResult) {
  Std_ReturnType r = LINIF_R_NOT_OK;
  PduInfoType pduInfo;

  if (LINIF_R_RECEIVED_OK == notifyResult) {
    pduInfo.SduDataPtr = frame->SduPtr;
    pduInfo.SduLength = frame->Dl;
    if (lIsoTp.params.ll_dl == pduInfo.SduLength) {
      LinTp_RxIndication(0, &pduInfo);
    }
  } else {
    lIsoTp.result = -__LINE__;
    Std_TimerStop(&lIsoTp.timerErrorNotify);
    LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
    sem_post(&lIsoTp.sem);
  }

  return r;
}

BufReq_ReturnType LinTp_StartOfReception(PduIdType id, const PduInfoType *info,
                                         PduLengthType TpSduLength, PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_OK;

  if (TpSduLength <= sizeof(lIsoTp.RX.data)) {
    *bufferSizePtr = lIsoTp.RX.length;
    lIsoTp.RX.length = TpSduLength;
    lIsoTp.RX.index = 0;
  } else {
    ret = BUFREQ_E_NOT_OK;
  }

  return ret;
}

BufReq_ReturnType LinTp_CopyRxData(PduIdType id, const PduInfoType *info,
                                   PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_OK;

  if (lIsoTp.RX.index < lIsoTp.RX.length) {
    memcpy(&lIsoTp.RX.data[lIsoTp.RX.index], info->SduDataPtr, info->SduLength);
    lIsoTp.RX.index += info->SduLength;
    *bufferSizePtr = lIsoTp.RX.length - lIsoTp.RX.index;
  } else {
    ret = BUFREQ_E_NOT_OK;
  }

  return ret;
}

void LinTp_TpRxIndication(PduIdType id, Std_ReturnType result) {
  if (E_OK == result) {
    lIsoTp.result = 0;
    Std_TimerStop(&lIsoTp.timerErrorNotify);
    LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
    sem_post(&lIsoTp.sem);
  } else {
    /* FAILED */
    lIsoTp.result = -__LINE__;
    Std_TimerStop(&lIsoTp.timerErrorNotify);
    LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
    sem_post(&lIsoTp.sem);
  }
}

BufReq_ReturnType LinTp_CopyTxData(PduIdType id, const PduInfoType *info,
                                   const RetryInfoType *retry, PduLengthType *availableDataPtr) {

  BufReq_ReturnType ret = BUFREQ_OK;

  if ((lIsoTp.TX.data != NULL) && (lIsoTp.TX.index < lIsoTp.TX.length)) {
    memcpy(info->SduDataPtr, &lIsoTp.TX.data[lIsoTp.TX.index], info->SduLength);
    lIsoTp.TX.index += info->SduLength;
    *availableDataPtr = lIsoTp.TX.length - lIsoTp.TX.index;
  } else {
    ret = BUFREQ_E_NOT_OK;
  }

  return ret;
}

void LinTp_TpTxConfirmation(PduIdType id, Std_ReturnType result) {
  if (E_OK == result) {
    lIsoTp.result = 0;
    Std_TimerStop(&lIsoTp.timerErrorNotify);
    LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
    sem_post(&lIsoTp.sem);
  } else {
    lIsoTp.result = -88;
    Std_TimerStop(&lIsoTp.timerErrorNotify);
    LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
    sem_post(&lIsoTp.sem);
  }
}

isotp_t *isotp_lin_create(isotp_parameter_t *params) {
  isotp_t *isotp;
  int r = 0;
  pthread_mutexattr_t attr;

  if (lServerUp == TRUE) {
    return NULL;
  }

  isotp = &lIsoTp;
  memset(isotp, 0, sizeof(isotp_t));
  isotp->params = *params;
  Std_TimerStop(&isotp->timerErrorNotify);

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  r = pthread_mutex_init(&isotp->mutex, &attr);
  r |= sem_init(&isotp->sem, 0, 0);
  r |= pthread_create(&isotp->serverThread, NULL, lin_server_main, isotp);

  if (0 != r) {
    isotp = NULL;
  }

  if (0 == r) {
    sem_wait(&isotp->sem);
    r = isotp->result;
    if (0 != r) {
      isotp->running = FALSE;
      isotp = NULL;
    }
  }

  return isotp;
}

int isotp_lin_transmit(isotp_t *isotp, const uint8_t *txBuffer, size_t txSize, uint8_t *rxBuffer,
                       size_t rxSize) {
  int r = 0;
  PduInfoType PduInfo;

  pthread_mutex_lock(&lMutex);
  isotp->result = -1;
  isotp->TX.data = (uint8_t *)txBuffer;
  isotp->TX.length = txSize;
  isotp->TX.index = 0;
  isotp->RX.length = sizeof(isotp->RX.data);
  isotp->RX.index = 0;
  PduInfo.SduDataPtr = (uint8_t *)txBuffer;
  PduInfo.SduLength = txSize;
  isotp->errorTimeout = UDS_TIMEOUT;
  r = LinTp_Transmit(0, &PduInfo);
  if (E_OK == r) {
    LinIf_ScheduleRequest(0, LINIF_SCH_TABLE_DIAG_REQUEST);
    Std_TimerStart(&isotp->timerErrorNotify);
  }
  pthread_mutex_unlock(&lMutex);

  if (E_OK == r) {
    sem_wait(&isotp->sem);
    r = isotp->result;

    if (0 == r) {
      if (NULL != rxBuffer) {
        if (isotp->params.U.LIN.delayUs > 0) {
          /* give slave some time to process, generally wait response to be ready */
          usleep(isotp->params.U.LIN.delayUs);
        }
        pthread_mutex_lock(&lMutex);
        LinIf_ScheduleRequest(0, LINIF_SCH_TABLE_DIAG_RESPONSE);
        isotp->errorTimeout = UDS_TIMEOUT;
        Std_TimerStart(&isotp->timerErrorNotify);
        pthread_mutex_unlock(&lMutex);
        sem_wait(&isotp->sem);
        r = isotp->result;
        if (0 == r) {
          pthread_mutex_lock(&lMutex);
          r = isotp->RX.index;
          if ((int)rxSize >= r) {
            memcpy(rxBuffer, isotp->RX.data, r);
          } else {
            r = -__LINE__;
          }
          pthread_mutex_unlock(&lMutex);
        }
      }
    }
  } else {
    r = -__LINE__;
  }

  pthread_mutex_lock(&lMutex);
  isotp->TX.data = NULL;
  LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
  Std_TimerStop(&isotp->timerErrorNotify);
  pthread_mutex_unlock(&lMutex);

  if (0 == r) {
    if (isotp->params.U.LIN.delayUs > 0) {
      /* give slave some time to process */
      usleep(isotp->params.U.LIN.delayUs);
    }
  }

  return r;
}

int isotp_lin_receive(isotp_t *isotp, uint8_t *rxBuffer, size_t rxSize) {

  int r = 0;
  pthread_mutex_lock(&lMutex);
  LinIf_ScheduleRequest(0, LINIF_SCH_TABLE_DIAG_RESPONSE);
  isotp->errorTimeout = UDS_TIMEOUT;
  Std_TimerStart(&isotp->timerErrorNotify);
  pthread_mutex_unlock(&lMutex);

  sem_wait(&isotp->sem);
  r = isotp->result;
  if (0 == r) {
    pthread_mutex_lock(&lMutex);
    r = isotp->RX.index;
    if ((int)rxSize >= r) {
      memcpy(rxBuffer, isotp->RX.data, r);
    } else {
      r = -__LINE__;
    }
    pthread_mutex_unlock(&lMutex);
  }

  pthread_mutex_lock(&lMutex);
  isotp->RX.length = sizeof(isotp->RX.data);
  isotp->RX.index = 0;
  LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
  Std_TimerStop(&isotp->timerErrorNotify);
  pthread_mutex_unlock(&lMutex);

  return r;
}

void isotp_lin_destory(isotp_t *isotp) {
  lServerUp = FALSE;
  pthread_join(isotp->serverThread, NULL);
}

int isotp_lin_ioctl(isotp_t *isotp, int cmd, const void *data, size_t size) {
  return -__LINE__;
}
#endif /* USE_LINTP */