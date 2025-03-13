/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "isotp.h"
#include "isotp_types.hpp"
#include "Can.h"
#include "CanIf.h"
#include "CanIf_Can.h"
#include "CanTp.h"
#include "PduR_Dcm.h"
#include "Std_Debug.h"
#include <stdio.h>
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

#define ISOTP_CAN_RELAX 20
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern "C" int Can_MainFunction_ReadChannelById(uint8_t Channel, uint32_t byId);
extern "C" void CanIf_CanTpSetTxCanId(uint8_t Channel, uint32_t TxCanId);
extern "C" void Can_Wait(uint8_t Channel, uint32_t canid, uint32_t timeoutMs);
int isotp_can_receive(isotp_t *isotp, uint8_t *rxBuffer, size_t rxSize);
void isotp_can_destory(isotp_t *isotp);
/* ================================ [ DATAS     ] ============================================== */
static isotp_t lIsoTp[CANTP_MAX_CHANNELS];
static std::mutex lMutex;
/* ================================ [ LOCALS    ] ============================================== */
static void can_server_main(isotp_t *isotp) {
  Std_TimerType timer10ms = {0};
  uint8_t Channel = isotp->Channel;
  CanTp_ParamType param;
  Std_ReturnType ret;
  int relax = ISOTP_CAN_RELAX;

  param.device = isotp->params.device;
  param.baudrate = isotp->params.baudrate;
  param.port = isotp->params.port;
  param.RxCanId = isotp->params.U.CAN.RxCanId;
  param.TxCanId = isotp->params.U.CAN.TxCanId;
  param.N_TA = isotp->params.N_TA;
  param.ll_dl = (uint8_t)isotp->params.ll_dl;
  param.STmin = isotp->params.U.CAN.STmin;

  CanTp_ReConfig(Channel, &param);
  CanTp_InitChannel(Channel);
  ret = Can_SetControllerMode(Channel, CAN_CS_STARTED);

  Std_TimerSet(&timer10ms, 10000);
  Std_TimerStop(&isotp->timerErrorNotify);

  if (E_OK == ret) {
    isotp->result = 0;
  } else {
    ASLOG(ISOTPE, ("[%d] failed to open %s:%d\n", Channel, param.device, param.port));
    isotp->result = __LINE__;
    isotp->running = FALSE;
  }
  isotp->sem.post();

  while (TRUE == isotp->running) {
    {
      std::lock_guard<std::mutex> lg(isotp->mutex);
      Can_MainFunction_WriteChannel(Channel);
      Can_MainFunction_ReadChannelById(Channel, param.RxCanId);
    }

    if (TRUE == Std_IsTimerTimeout(&timer10ms)) {
      std::lock_guard<std::mutex> lg(isotp->mutex);
      CanTp_MainFunction_Channel(Channel);
      Std_TimerSet(&timer10ms, 10000);
    }

    {
      std::lock_guard<std::mutex> lg(isotp->mutex);
      if (Std_IsTimerStarted(&isotp->timerErrorNotify)) {
        if (Std_GetTimerElapsedTime(&isotp->timerErrorNotify) >= isotp->errorTimeout) {
          isotp->result = -__LINE__;
          ASLOG(ISOTPE, ("[%d] timeout\n", Channel));
          Std_TimerStop(&isotp->timerErrorNotify);
          isotp->sem.post();
        }
      }
    }

    if (relax > 0) { /* for the purpose to speed up downloading when STmin = 0 */
      relax--;
      if (0 == relax) {
        Can_Wait(1, param.RxCanId, 1);
        relax = ISOTP_CAN_RELAX;
      }
    }
  }

  Can_SetControllerMode(Channel, CAN_CS_STOPPED);
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
  size_t i;

  std::unique_lock<std::mutex> lck(lMutex);
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
        isotp->running = TRUE;
        isotp->Channel = (uint8_t)i;
        break;
      }
    }
  }

  if (NULL != isotp) {
    isotp->params = *params;
    Std_TimerStop(&isotp->timerErrorNotify);
    isotp->serverThread = std::thread(can_server_main, isotp);
    isotp->sem.wait();
    r = isotp->result;
    if (0 != r) {
      isotp->running = FALSE;
      isotp_can_destory(isotp);
      isotp = NULL;
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

  do { /* current server thread has a chance post sem for several times, so here consume all the
          original sem post */
    r = isotp->sem.wait(0);
  } while (0 == r);

  {
    std::lock_guard<std::mutex> lg(isotp->mutex);
    isotp->result = -__LINE__;
    isotp->errorTimeout = 5000000;
    Std_TimerStart(&isotp->timerErrorNotify);
    isotp->TX.data = (uint8_t *)txBuffer;
    isotp->TX.length = txSize;
    isotp->TX.index = 0;
    isotp->RX.length = sizeof(isotp->RX.data);
    isotp->RX.index = 0;
    isotp->RX.bInUse = FALSE;
    r = (int)PduR_DcmTransmit(isotp->Channel, &PduInfo);
  }

  if (r == E_OK) {
    isotp->sem.wait();
    r = isotp->result;
    if (0 == r) {
      if (NULL != rxBuffer) {
        r = isotp_can_receive(isotp, rxBuffer, rxSize);
      }
    }
  } else {
    r = -__LINE__;
  }

  std::lock_guard<std::mutex> lg(isotp->mutex);
  isotp->TX.data = NULL;
  Std_TimerStop(&isotp->timerErrorNotify);

  return r;
}

int isotp_can_receive(isotp_t *isotp, uint8_t *rxBuffer, size_t rxSize) {

  int r = 0;
  {
    std::lock_guard<std::mutex> lg(isotp->mutex);
    isotp->errorTimeout = 5000000;
    Std_TimerStart(&isotp->timerErrorNotify);
  }

  isotp->sem.wait();
  r = isotp->result;
  if (0 == r) {
    std::lock_guard<std::mutex> lg(isotp->mutex);
    if ((rxSize >= isotp->RX.index) && (isotp->RX.length == isotp->RX.index)) {
      r = isotp->RX.index;
      memcpy(rxBuffer, isotp->RX.data, r);
    } else {
      r = -__LINE__;
    }
  }

  std::lock_guard<std::mutex> lg(isotp->mutex);
  isotp->RX.length = sizeof(isotp->RX.data);
  isotp->RX.index = 0;
  isotp->RX.bInUse = FALSE;
  Std_TimerStop(&isotp->timerErrorNotify);

  return r;
}

int isotp_can_ioctl(isotp_t *isotp, int cmd, const void *data, size_t size) {
  int r = -__LINE__;
  std::lock_guard<std::mutex> lg(isotp->mutex);
  switch (cmd) {
  case ISOTP_IOCTL_SET_TX_ID:
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

  return r;
}

void isotp_can_destory(isotp_t *isotp) {
  isotp->running = FALSE;
  if (isotp->serverThread.joinable()) {
    isotp->serverThread.join();
  }
}

extern "C" BufReq_ReturnType IsoTp_CanTpStartOfReception(PduIdType id, const PduInfoType *info,
                                                         PduLengthType TpSduLength,
                                                         PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  isotp_t *isotp = isotp_get(id);
  if (NULL != isotp) {
    while (TRUE == isotp->RX.bInUse) {
      /* it was possible that the response pending and the positive response received at the same
       * time, thus we need to ensure the previous received buffer consumed */
      isotp->mutex.unlock();
      std::this_thread::sleep_for(1ms); /* wait the buffer to be released */
      isotp->mutex.lock();
    }
    if (sizeof(isotp->RX.data) >= TpSduLength) {
      ASLOG(ISOTP, ("[%d] start reception\n", id));
      *bufferSizePtr = (PduLengthType)isotp->RX.length;
      isotp->RX.length = TpSduLength;
      isotp->RX.index = 0;
      isotp->RX.bInUse = TRUE;
      ret = BUFREQ_OK;
    } else {
      ASLOG(ISOTPE, ("[%d] listen buffer too small %d < %d\n", id, (int)isotp->RX.length,
                     (int)TpSduLength));
      isotp->result = -__LINE__;
      isotp->sem.post();
    }
  }

  return ret;
}

extern "C" BufReq_ReturnType IsoTp_CanTpCopyRxData(PduIdType id, const PduInfoType *info,
                                                   PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  isotp_t *isotp = isotp_get(id);
  if (NULL != isotp) {
    if ((isotp->RX.index + info->SduLength) <= isotp->RX.length) {
      ASLOG(ISOTP, ("[%d] copy rx data(%d)\n", id, info->SduLength));
      memcpy(&isotp->RX.data[isotp->RX.index], info->SduDataPtr, info->SduLength);
      isotp->RX.index += info->SduLength;
      ret = BUFREQ_OK;
      Std_TimerStart(&isotp->timerErrorNotify);
    } else {
      ASLOG(ISOTPE, ("[%d] listen buffer overflow\n", id));
      isotp->result = -__LINE__;
      isotp->sem.post();
    }
  }
  return ret;
}

extern "C" BufReq_ReturnType IsoTp_CanTpCopyTxData(PduIdType id, const PduInfoType *info,
                                                   const RetryInfoType *retry,
                                                   PduLengthType *availableDataPtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  isotp_t *isotp = isotp_get(id);
  if (NULL != isotp) {
    if (isotp->TX.data != NULL) {
      if ((isotp->TX.index + info->SduLength) <= isotp->TX.length) {
        ASLOG(ISOTP, ("[%d] copy tx data(%d)\n", id, info->SduLength));
        memcpy(info->SduDataPtr, &isotp->TX.data[isotp->TX.index], info->SduLength);
        isotp->TX.index += info->SduLength;
        ret = BUFREQ_OK;
        Std_TimerStart(&isotp->timerErrorNotify);
      } else {
        ASLOG(ISOTPE, ("[%d] transmit buffer overflow\n", id));
        isotp->result = -__LINE__;
        isotp->sem.post();
      }
    } else {
      ASLOG(ISOTPE, ("[%d] no transmit buffer\n", id));
    }
  }
  return ret;
}

extern "C" void IsoTp_CanTpRxIndication(PduIdType id, Std_ReturnType result) {
  isotp_t *isotp = isotp_get(id);
  if (NULL != isotp) {
    if (E_OK == result) {
      ASLOG(ISOTP, ("[%d] rx done\n", id));
      isotp->result = 0;
    } else {
      ASLOG(ISOTPE, ("[%d] rx failed\n", id));
      isotp->result = -__LINE__;
    }
    isotp->sem.post();
  }
}

extern "C" void IsoTp_CanTpTxConfirmation(PduIdType id, Std_ReturnType result) {
  isotp_t *isotp = isotp_get(id);
  if (NULL != isotp) {
    if (E_OK == result) {
      ASLOG(ISOTP, ("[%d] tx done\n", id));
      isotp->result = 0;
    } else {
      ASLOG(ISOTPE, ("[%d] tx failed\n", id));
      isotp->result = -__LINE__;
    }
    isotp->sem.post();
  }
}
