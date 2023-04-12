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
#include "PduR_Dcm.h"
#include "Std_Debug.h"
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "../config/CanTp_Cfg.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef CANTP_MAX_CHANNELS
#define CANTP_MAX_CHANNELS 32
#endif

#define AS_LOG_ISOTP 0
#define AS_LOG_ISOTPE 3
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void Can_MainFunction_ReadChannelById(uint8_t Channel, uint32_t byId);
extern void CanIf_CanTpSetTxCanId(uint8_t Channel, uint32_t TxCanId);
int isotp_can_receive(isotp_t *isotp, uint8_t *rxBuffer, size_t rxSize);
/* ================================ [ DATAS     ] ============================================== */
static isotp_t lIsoTp[CANTP_MAX_CHANNELS];
static pthread_mutex_t lMutex = PTHREAD_MUTEX_INITIALIZER;
/* ================================ [ LOCALS    ] ============================================== */
static void *can_server_main(void *args) {
  isotp_t *isotp = (isotp_t *)args;
  Std_TimerType timer10ms;
  uint8_t Channel = isotp->Channel;
  CanTp_ParamType param;
  Std_ReturnType ret;

  param.device = isotp->params.device;
  param.baudrate = isotp->params.baudrate;
  param.port = isotp->params.port;
  param.RxCanId = isotp->params.U.CAN.RxCanId;
  param.TxCanId = isotp->params.U.CAN.TxCanId;
  param.ll_dl = (uint8_t)isotp->params.ll_dl;

  CanTp_ReConfig(Channel, &param);
  CanTp_InitChannel(Channel);
  ret = Can_SetControllerMode(Channel, CAN_CS_STARTED);

  Std_TimerStart(&timer10ms);
  Std_TimerStop(&isotp->timerErrorNotify);

  if (E_OK == ret) {
    isotp->result = 0;
  } else {
    ASLOG(ISOTPE, ("[%d] failed to open %s:%d\n", Channel, param.device, param.port));
    isotp->result = __LINE__;
    isotp->running = FALSE;
  }
  sem_post(&isotp->sem);

  while (TRUE == isotp->running) {
    Can_MainFunction_WriteChannel(Channel);
    Can_MainFunction_ReadChannelById(Channel, param.RxCanId);

    if (Std_GetTimerElapsedTime(&timer10ms) >= 10000) {
      pthread_mutex_lock(&isotp->mutex);
      CanTp_MainFunction_Channel(Channel);
      pthread_mutex_unlock(&isotp->mutex);
      Std_TimerStart(&timer10ms);
    }

    pthread_mutex_lock(&isotp->mutex);
    if (Std_IsTimerStarted(&isotp->timerErrorNotify)) {
      if (Std_GetTimerElapsedTime(&isotp->timerErrorNotify) >= isotp->errorTimeout) {
        isotp->result = -__LINE__;
        Std_TimerStop(&isotp->timerErrorNotify);
        sem_post(&isotp->sem);
      }
    }
    pthread_mutex_unlock(&isotp->mutex);

    usleep(1000);
  }

  Can_SetControllerMode(Channel, CAN_CS_STOPPED);

  return NULL;
}

static isotp_t *isotp_get(PduIdType id) {
  isotp_t *isotp = NULL;
  if (id < CANTP_MAX_CHANNELS) {
    if (lIsoTp[id].running) {
      isotp = &lIsoTp[id];
    } else {
      ASLOG(ISOTPE, ("[%d] is dead\n", id));
    }
  } else {
    ASLOG(ISOTPE, ("isotp tx with invalid id %d\n", id));
  }
  return isotp;
}
/* ================================ [ FUNCTIONS ] ============================================== */
isotp_t *isotp_can_create(isotp_parameter_t *params) {
  isotp_t *isotp = NULL;
  int r = 0;
  pthread_mutexattr_t attr;
  size_t i;

  pthread_mutex_lock(&lMutex);
  for (i = 0; i < ARRAY_SIZE(lIsoTp); i++) {
    if (TRUE == lIsoTp[i].running) {
      if ((0 == strcmp(lIsoTp[i].params.device, params->device)) &&
          (lIsoTp[i].params.port == params->port) &&
          ((lIsoTp[i].params.U.CAN.RxCanId == params->U.CAN.RxCanId) ||
           (lIsoTp[i].params.U.CAN.TxCanId == params->U.CAN.TxCanId))) {
        ASLOG(ISOTPE, ("isotp CAN %s:%d %X:%X already opened\n", params->device, params->port,
                       params->U.CAN.RxCanId, params->U.CAN.TxCanId));
        r = -__LINE__;
        break;
      }
    }
  }

  if (0 == r) {
    for (i = 0; i < ARRAY_SIZE(lIsoTp); i++) {
      if (FALSE == lIsoTp[i].running) {
        isotp = &lIsoTp[i];
        memset(isotp, 0, sizeof(isotp_t));
        isotp->running = TRUE;
        isotp->Channel = (uint8_t)i;
        break;
      }
    }
  }
  pthread_mutex_unlock(&lMutex);

  if (NULL != isotp) {
    isotp->params = *params;
    Std_TimerStop(&isotp->timerErrorNotify);

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    r = pthread_mutex_init(&isotp->mutex, &attr);
    r |= sem_init(&isotp->sem, 0, 0);
    r |= pthread_create(&isotp->serverThread, NULL, can_server_main, isotp);

    if (0 != r) {
      isotp->running = FALSE;
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
  }

  return isotp;
}

int isotp_can_transmit(isotp_t *isotp, const uint8_t *txBuffer, size_t txSize, uint8_t *rxBuffer,
                       size_t rxSize) {
  int r = 0;
  PduInfoType PduInfo;
  PduInfo.SduDataPtr = (uint8_t *)txBuffer;
  PduInfo.SduLength = txSize;
  pthread_mutex_lock(&isotp->mutex);
  sem_init(&isotp->sem, 0, 0);
  isotp->result = -__LINE__;
  isotp->errorTimeout = 5000000;
  Std_TimerStart(&isotp->timerErrorNotify);
  isotp->TX.data = (uint8_t *)txBuffer;
  isotp->TX.length = txSize;
  isotp->TX.index = 0;
  isotp->RX.length = sizeof(isotp->RX.data);
  isotp->RX.index = 0;
  r = (int)PduR_DcmTransmit(isotp->Channel, &PduInfo);
  pthread_mutex_unlock(&isotp->mutex);

  if (r == E_OK) {
    sem_wait(&isotp->sem);
    r = isotp->result;

    if (0 == r) {
      if (NULL != rxBuffer) {
        r = isotp_can_receive(isotp, rxBuffer, rxSize);
      }
    }
  } else {
    r = -__LINE__;
  }

  pthread_mutex_lock(&isotp->mutex);
  isotp->TX.data = NULL;
  Std_TimerStop(&isotp->timerErrorNotify);
  pthread_mutex_unlock(&isotp->mutex);

  return r;
}

int isotp_can_receive(isotp_t *isotp, uint8_t *rxBuffer, size_t rxSize) {

  int r = 0;
  pthread_mutex_lock(&isotp->mutex);
  isotp->errorTimeout = 5000000;
  Std_TimerStart(&isotp->timerErrorNotify);
  pthread_mutex_unlock(&isotp->mutex);

  sem_wait(&isotp->sem);
  r = isotp->result;
  if (0 == r) {
    pthread_mutex_lock(&isotp->mutex);
    r = isotp->RX.index;
    if ((int)rxSize >= r) {
      memcpy(rxBuffer, isotp->RX.data, r);
    } else {
      r = -__LINE__;
    }
    pthread_mutex_unlock(&isotp->mutex);
  }

  pthread_mutex_lock(&isotp->mutex);
  isotp->RX.length = sizeof(isotp->RX.data);
  isotp->RX.index = 0;
  Std_TimerStop(&isotp->timerErrorNotify);
  pthread_mutex_unlock(&isotp->mutex);

  return r;
}

int isotp_can_ioctl(isotp_t *isotp, int cmd, const void *data, size_t size) {
  int r = -__LINE__;
  pthread_mutex_lock(&isotp->mutex);
  switch (cmd) {
  case ISOTP_OTCTL_SET_TX_ID:
    if ((NULL != data) && (size == sizeof(uint32_t))) {
      uint32_t txId = *(uint32_t *)data;
      *(uint32_t *)data = isotp->params.U.CAN.TxCanId;
      isotp->params.U.CAN.TxCanId = txId;
      CanIf_CanTpSetTxCanId(isotp->Channel, txId);
      r = 0;
    }
    break;
  default:
    break;
  }
  pthread_mutex_unlock(&isotp->mutex);
  return r;
}

void isotp_can_destory(isotp_t *isotp) {
  isotp->running = FALSE;
  pthread_join(isotp->serverThread, NULL);
}

BufReq_ReturnType IsoTp_CanTpStartOfReception(PduIdType id, const PduInfoType *info,
                                              PduLengthType TpSduLength,
                                              PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  isotp_t *isotp = isotp_get(id);
  if (NULL != isotp) {
    pthread_mutex_lock(&isotp->mutex);
    if (sizeof(isotp->RX.data) >= TpSduLength) {
      ASLOG(ISOTP, ("[%d] start reception\n", id));
      *bufferSizePtr = (PduLengthType)isotp->RX.length;
      isotp->RX.length = TpSduLength;
      isotp->RX.index = 0;
      ret = BUFREQ_OK;
    } else {
      ASLOG(ISOTPE, ("[%d] listen buffer too small %d < %d\n", id, (int)isotp->RX.length,
                     (int)TpSduLength));
      isotp->result = -__LINE__;
      sem_post(&isotp->sem);
    }
    pthread_mutex_unlock(&isotp->mutex);
  }

  return ret;
}

BufReq_ReturnType IsoTp_CanCopyRxData(PduIdType id, const PduInfoType *info,
                                      PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  isotp_t *isotp = isotp_get(id);
  if (NULL != isotp) {
    pthread_mutex_lock(&isotp->mutex);
    if ((isotp->RX.index + info->SduLength) <= isotp->RX.length) {
      ASLOG(ISOTP, ("[%d] copy rx data(%d)\n", id, info->SduLength));
      memcpy(&isotp->RX.data[isotp->RX.index], info->SduDataPtr, info->SduLength);
      isotp->RX.index += info->SduLength;
      ret = BUFREQ_OK;
    } else {
      ASLOG(ISOTPE, ("[%d] listen buffer overflow\n", id));
      isotp->result = -__LINE__;
      sem_post(&isotp->sem);
    }

    pthread_mutex_unlock(&isotp->mutex);
  }
  return ret;
}

BufReq_ReturnType IsoTp_CanTpCopyTxData(PduIdType id, const PduInfoType *info,
                                        const RetryInfoType *retry,
                                        PduLengthType *availableDataPtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  isotp_t *isotp = isotp_get(id);
  if (NULL != isotp) {
    pthread_mutex_lock(&isotp->mutex);
    if (isotp->TX.data != NULL) {
      if ((isotp->TX.index + info->SduLength) <= isotp->TX.length) {
        ASLOG(ISOTP, ("[%d] copy tx data(%d)\n", id, info->SduLength));
        memcpy(info->SduDataPtr, &isotp->TX.data[isotp->TX.index], info->SduLength);
        isotp->TX.index += info->SduLength;
        ret = BUFREQ_OK;
      } else {
        ASLOG(ISOTPE, ("[%d] transmit buffer overflow\n", id));
        isotp->result = -__LINE__;
        sem_post(&isotp->sem);
      }
    } else {
      ASLOG(ISOTPE, ("[%d] no transmit buffer\n", id));
    }
    pthread_mutex_unlock(&isotp->mutex);
  }
  return ret;
}

void IsoTp_CanTpRxIndication(PduIdType id, Std_ReturnType result) {
  isotp_t *isotp = isotp_get(id);
  if (NULL != isotp) {
    pthread_mutex_lock(&isotp->mutex);
    if (E_OK == result) {
      ASLOG(ISOTP, ("[%d] rx done\n", id));
      isotp->result = 0;
    } else {
      ASLOG(ISOTPE, ("[%d] rx failed\n", id));
      isotp->result = -__LINE__;
    }
    sem_post(&isotp->sem);
    pthread_mutex_unlock(&isotp->mutex);
  }
}

void IsoTp_CanTpTxConfirmation(PduIdType id, Std_ReturnType result) {
  isotp_t *isotp = isotp_get(id);
  if (NULL != isotp) {
    pthread_mutex_lock(&isotp->mutex);
    if (E_OK == result) {
      ASLOG(ISOTP, ("[%d] tx done\n", id));
      isotp->result = 0;
    } else {
      ASLOG(ISOTPE, ("[%d] tx failed\n", id));
      isotp->result = -__LINE__;
    }
    sem_post(&isotp->sem);
    pthread_mutex_unlock(&isotp->mutex);
  }
}
