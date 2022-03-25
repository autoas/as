/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "isotp.h"
#include "isotp_types.h"
#include "Can.h"
#include "CanIf.h"
#include "CanIf_Can.h"
#include "CanTp.h"
#include "PduR_CanTp.h"
#include "Dcm.h"
#include "Std_Debug.h"
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void Can_ReConfig(uint8_t Controller, const char *device, int port, uint32_t baudrate);
extern void CanTp_ReConfig(uint8_t Channel, uint8_t ll_dl);
extern int Dcm_IsResponseReady(void);
extern int Dcm_IsTxCompleted(void);
extern int Dcm_Receive(uint8_t *buffer, PduLengthType length, int *functional);
extern Std_ReturnType Dcm_Transmit(const uint8_t *buffer, PduLengthType length, int functional);
/* ================================ [ DATAS     ] ==============================================
 */
static isotp_t lIsoTp;
static int lServerUp = FALSE;
static pthread_mutex_t lMutex = PTHREAD_MUTEX_INITIALIZER;
/* ================================ [ LOCALS    ] ============================================== */
static void *can_server_main(void *args) {
  isotp_t *isotp = (isotp_t *)args;
  Std_TimerType timer10ms;

  CanTp_ReConfig(0, (uint8_t)isotp->params->ll_dl);
  Can_ReConfig(0, isotp->params->device, isotp->params->port, isotp->params->baudrate);
  Can_Init(NULL);
  Can_SetControllerMode(0, CAN_CS_STARTED);
  CanTp_Init(NULL);
  Dcm_Init(NULL);
  Std_TimerStart(&timer10ms);
  Std_TimerStop(&isotp->timerErrorNotify);

  lServerUp = TRUE;
  sem_post(&isotp->sem);

  while (lServerUp) {
    if (Std_GetTimerElapsedTime(&timer10ms) >= 10000) {
      pthread_mutex_lock(&lMutex);
      CanTp_MainFunction();
      pthread_mutex_unlock(&lMutex);
      Std_TimerStart(&timer10ms);
    }
    pthread_mutex_lock(&lMutex);
    Dcm_MainFunction_Response();
    Can_MainFunction_Write();
    Can_MainFunction_Read();
    pthread_mutex_unlock(&lMutex);

    pthread_mutex_lock(&lMutex);
    if (Dcm_IsTxCompleted()) {
      isotp->result = 0;
      Std_TimerStop(&isotp->timerErrorNotify);
      sem_post(&isotp->sem);
    }
    pthread_mutex_unlock(&lMutex);

    pthread_mutex_lock(&lMutex);
    if (Dcm_IsResponseReady()) {
      isotp->result = 0;
      Std_TimerStop(&isotp->timerErrorNotify);
      sem_post(&isotp->sem);
    }
    pthread_mutex_unlock(&lMutex);

    pthread_mutex_lock(&lMutex);
    if (Std_IsTimerStarted(&isotp->timerErrorNotify)) {
      if (Std_GetTimerElapsedTime(&isotp->timerErrorNotify) >= isotp->errorTimeout) {
        isotp->result = -99;
        Std_TimerStop(&isotp->timerErrorNotify);
        sem_post(&isotp->sem);
      }
    }
    pthread_mutex_unlock(&lMutex);

    usleep(1000);
  }

  Can_SetControllerMode(0, CAN_CS_STOPPED);
  Can_DeInit();

  return NULL;
}
/* ================================ [ FUNCTIONS ] ============================================== */

void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr) {
  if (lIsoTp.params->U.CAN.RxCanId == Mailbox->CanId) {
    CanTp_RxIndication((PduIdType)0, PduInfoPtr);
  }
}

void CanIf_TxConfirmation(PduIdType CanTxPduId) {
  if (0 == CanTxPduId) {
    CanTp_TxConfirmation(0, E_OK);
  }
}

Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  Can_PduType canPdu;

  canPdu.swPduHandle = TxPduId;
  canPdu.length = PduInfoPtr->SduLength;
  canPdu.sdu = PduInfoPtr->SduDataPtr;

  if (0 == TxPduId) {
    canPdu.id = lIsoTp.params->U.CAN.TxCanId;
    ret = Can_Write(0, &canPdu);
  } else if (1 == TxPduId) {
    canPdu.id = 0x7DF; /* TODO: get it from config */
    ret = Can_Write(0, &canPdu);
  }

  return ret;
}

isotp_t *isotp_can_create(isotp_parameter_t *params) {
  isotp_t *isotp;
  int r = 0;
  pthread_mutexattr_t attr;

  if (lServerUp == TRUE) {
    return NULL;
  }

  isotp = &lIsoTp;
  isotp->params = params;
  isotp->result = 0;
  isotp->data = NULL;
  isotp->length = 0;
  isotp->errorTimeout = 0;
  Std_TimerStop(&isotp->timerErrorNotify);

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  r = pthread_mutex_init(&isotp->mutex, &attr);
  r |= sem_init(&isotp->sem, 0, 0);
  r |= pthread_create(&isotp->serverThread, NULL, can_server_main, isotp);

  if (0 != r) {
    isotp = NULL;
  }

  if (0 == r) {
    sem_wait(&isotp->sem);
    r = isotp->result;
  }

  return isotp;
}

int isotp_can_transmit(isotp_t *isotp, const uint8_t *txBuffer, size_t txSize, uint8_t *rxBuffer,
                       size_t rxSize) {
  int r = 0;
  pthread_mutex_lock(&lMutex);
  isotp->result = -1;
  isotp->errorTimeout = 5000000;
  Std_TimerStart(&isotp->timerErrorNotify);
  r = (int)Dcm_Transmit(txBuffer, txSize, 0);
  pthread_mutex_unlock(&lMutex);

  if (r == E_OK) {
    sem_wait(&isotp->sem);
    r = isotp->result;

    if (0 == r) {
      if (NULL != rxBuffer) {
        pthread_mutex_lock(&lMutex);
        isotp->errorTimeout = 5000000;
        Std_TimerStart(&isotp->timerErrorNotify);
        pthread_mutex_unlock(&lMutex);
        sem_wait(&isotp->sem);
        r = isotp->result;
        if (0 == r) {
          pthread_mutex_lock(&lMutex);
          r = Dcm_Receive(rxBuffer, rxSize, NULL);
          pthread_mutex_unlock(&lMutex);
        }
      }
    }
  } else {
    r = -2;
  }

  pthread_mutex_lock(&lMutex);
  Std_TimerStop(&isotp->timerErrorNotify);
  pthread_mutex_unlock(&lMutex);

  return r;
}

int isotp_can_receive(isotp_t *isotp, uint8_t *rxBuffer, size_t rxSize) {

  int r = 0;
  pthread_mutex_lock(&lMutex);
  isotp->errorTimeout = 5000000;
  Std_TimerStart(&isotp->timerErrorNotify);
  pthread_mutex_unlock(&lMutex);

  sem_wait(&isotp->sem);
  r = isotp->result;
  if (0 == r) {
    pthread_mutex_lock(&lMutex);
    r = Dcm_Receive(rxBuffer, rxSize, NULL);
    pthread_mutex_unlock(&lMutex);
  }

  pthread_mutex_lock(&lMutex);
  Std_TimerStop(&isotp->timerErrorNotify);
  pthread_mutex_unlock(&lMutex);

  return r;
}

void isotp_can_destory(isotp_t *isotp) {
  lServerUp = FALSE;
  pthread_join(isotp->serverThread, NULL);
}
