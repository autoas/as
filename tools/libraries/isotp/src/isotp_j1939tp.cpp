/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "isotp.h"
#include "isotp_types.hpp"
#include "Can.h"
#include "CanIf.h"
#include "CanIf_Can.h"
#include "J1939Tp.h"
#include "PduR_J1939Tp.h"
#include "Std_Debug.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../config/J1939Tp_Cfg.h"
#include "../config/CanIf_Cfg.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_J1939TP 0
#define AS_LOG_J1939TPE 3
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern "C" int Can_MainFunction_ReadChannelById(uint8_t Channel, uint32_t byId);
int isotp_j1939tp_receive(isotp_t *isotp, uint8_t *rxBuffer, size_t rxSize);
void isotp_j1939tp_destory(isotp_t *isotp);
/* ================================ [ DATAS     ] ============================================== */
static isotp_t lIsoTp[J1939TP_MAX_CHANNELS];
static std::mutex lMutex;
/* ================================ [ LOCALS    ] ============================================== */
static void j1939tp_server_main(isotp_t *isotp) {
  Std_TimerType timer10ms = {0};
  uint8_t Channel = isotp->Channel;
  J1939Tp_ParamType param;
  Std_ReturnType ret;
  uint8_t controller = CANTP_MAX_CHANNELS + Channel;

  param.device = isotp->params.device;
  param.baudrate = isotp->params.baudrate;
  param.port = isotp->params.port;
  param.RX.CM = isotp->params.U.J1939TP.RX.CM;
  param.RX.DT = isotp->params.U.J1939TP.RX.DT;
  param.RX.Direct = isotp->params.U.J1939TP.RX.Direct;
  param.RX.FC = isotp->params.U.J1939TP.RX.FC;
  param.TX.CM = isotp->params.U.J1939TP.TX.CM;
  param.TX.DT = isotp->params.U.J1939TP.TX.DT;
  param.TX.Direct = isotp->params.U.J1939TP.TX.Direct;
  param.TX.FC = isotp->params.U.J1939TP.TX.FC;
  param.STMin = isotp->params.U.J1939TP.STMin;
  param.Tr = isotp->params.U.J1939TP.Tr;
  param.T1 = isotp->params.U.J1939TP.T1;
  param.T2 = isotp->params.U.J1939TP.T2;
  param.T3 = isotp->params.U.J1939TP.T3;
  param.T4 = isotp->params.U.J1939TP.T4;
  param.TxMaxPacketsPerBlock = isotp->params.U.J1939TP.TxMaxPacketsPerBlock;
  param.protocol = (uint8_t)isotp->params.U.J1939TP.protocol;
  param.ll_dl = (uint8_t)isotp->params.ll_dl;

  J1939Tp_ReConfig(Channel, &param);
  J1939Tp_InitRxChannel(Channel);
  J1939Tp_InitTxChannel(Channel);
  J1939Tp_SetTxPgPGN(Channel, isotp->params.U.J1939TP.PgPGN);
  ret = Can_SetControllerMode(controller, CAN_CS_STARTED);

  Std_TimerSet(&timer10ms, 10000);
  Std_TimerStop(&isotp->timerErrorNotify);

  if (E_OK == ret) {
    isotp->result = 0;
  } else {
    ASLOG(J1939TPE, ("[%d] failed to open %s:%d\n", Channel, param.device, param.port));
    isotp->result = __LINE__;
    isotp->running = FALSE;
  }
  isotp->sem.post();

  while (TRUE == isotp->running) {
    {
      std::lock_guard<std::mutex> lg(isotp->mutex);
      Can_MainFunction_WriteChannel(controller);

#if 0 /* the order is very important for J1939Tp */
    {
      int hasFrame;
      hasFrame = Can_MainFunction_ReadChannelById(controller, param.RX.DT);
      if (FALSE == hasFrame) {
        hasFrame = Can_MainFunction_ReadChannelById(controller, param.RX.Direct);
      }
      if (FALSE == hasFrame) {
        hasFrame = Can_MainFunction_ReadChannelById(controller, param.TX.FC);
      }
      if (FALSE == hasFrame) {
        hasFrame = Can_MainFunction_ReadChannelById(controller, param.RX.CM);
      }
    }
#else
      Can_MainFunction_ReadChannelById(controller, -1);
#endif
    }

    if (TRUE == Std_IsTimerTimeout(&timer10ms)) {
      std::lock_guard<std::mutex> lg(isotp->mutex);
      J1939Tp_MainFunction_RxChannel(Channel);
      J1939Tp_MainFunction_TxChannel(Channel);
      Std_TimerSet(&timer10ms, 10000);
    }

    {
      std::lock_guard<std::mutex> lg(isotp->mutex);
      J1939Tp_MainFunction_RxChannelFast(Channel);
      J1939Tp_MainFunction_TxChannelFast(Channel);
      if (Std_IsTimerStarted(&isotp->timerErrorNotify)) {
        if (Std_GetTimerElapsedTime(&isotp->timerErrorNotify) >= isotp->errorTimeout) {
          isotp->result = -__LINE__;
          Std_TimerStop(&isotp->timerErrorNotify);
          isotp->sem.post();
        }
      }
    }

    std::this_thread::sleep_for(1ms);
  }

  Can_SetControllerMode(controller, CAN_CS_STOPPED);
}

static isotp_t *isotp_get(PduIdType id) {
  isotp_t *isotp = NULL;
  if (id < J1939TP_MAX_CHANNELS) {
    if (lIsoTp[id].running) {
      isotp = &lIsoTp[id];
    } else {
      ASLOG(J1939TPE, ("[%d] is dead\n", id));
    }
  } else {
    ASLOG(J1939TPE, ("isotp tx with invalid id %d\n", id));
  }
  return isotp;
}
/* ================================ [ FUNCTIONS ] ============================================== */
isotp_t *isotp_j1939tp_create(isotp_parameter_t *params) {
  isotp_t *isotp = NULL;
  int r = 0;
  size_t i;

  std::lock_guard<std::mutex> lg(lMutex);
  for (i = 0; i < ARRAY_SIZE(lIsoTp); i++) {
    if (TRUE == lIsoTp[i].running) {
      if ((0 == strcmp(lIsoTp[i].params.device, params->device)) &&
          (lIsoTp[i].params.port == params->port) &&
          ((0 == memcmp(&lIsoTp[i].params.U.J1939TP.RX, &params->U.J1939TP.RX, 16)) ||
           (0 == memcmp(&lIsoTp[i].params.U.J1939TP.TX, &params->U.J1939TP.TX, 16)))) {
        ASLOG(J1939TPE,
              ("isotp CAN %s:%d Rx[%X %X %X %X] Tx[%X %X %X %X] already opened\n", params->device,
               params->port, params->U.J1939TP.RX.CM, params->U.J1939TP.RX.DT,
               params->U.J1939TP.RX.Direct, params->U.J1939TP.RX.FC, params->U.J1939TP.TX.CM,
               params->U.J1939TP.TX.DT, params->U.J1939TP.TX.Direct, params->U.J1939TP.TX.FC));
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
    isotp->serverThread = std::thread(j1939tp_server_main, isotp);
    isotp->sem.wait();
    r = isotp->result;
    if (0 != r) {
      isotp->running = FALSE;
      isotp_j1939tp_destory(isotp);
      isotp = NULL;
    }
  }

  return isotp;
}

int isotp_j1939tp_transmit(isotp_t *isotp, const uint8_t *txBuffer, size_t txSize,
                           uint8_t *rxBuffer, size_t rxSize) {
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
    r = (int)PduR_J1939TpTransmit(isotp->Channel + 2 * CANTP_MAX_CHANNELS, &PduInfo);
  }

  if (r == E_OK) {
    isotp->sem.wait();
    r = isotp->result;

    if (0 == r) {
      if (NULL != rxBuffer) {
        r = isotp_j1939tp_receive(isotp, rxBuffer, rxSize);
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

int isotp_j1939tp_receive(isotp_t *isotp, uint8_t *rxBuffer, size_t rxSize) {

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
    r = isotp->RX.index;
    if ((rxSize >= isotp->RX.index) && (isotp->RX.length == isotp->RX.index)) {
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

int isotp_j1939tp_ioctl(isotp_t *isotp, int cmd, const void *data, size_t size) {
  int r = -__LINE__;
  std::lock_guard<std::mutex> lg(isotp->mutex);
  switch (cmd) {
  case ISOTP_IOCTL_J1939TP_GET_PGN:
    if ((NULL != data) && (size == sizeof(uint32_t))) {
      J1939Tp_GetRxPgPGN(isotp->Channel, (uint32_t *)data);
      r = 0;
    }
    break;
  default:
    break;
  }

  return r;
}

void isotp_j1939tp_destory(isotp_t *isotp) {
  isotp->running = FALSE;
  if (isotp->serverThread.joinable()) {
    isotp->serverThread.join();
  }
}

extern "C" BufReq_ReturnType IsoTp_J1939TpStartOfReception(PduIdType id, const PduInfoType *info,
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
      ASLOG(J1939TP, ("[%d] start reception\n", id));
      *bufferSizePtr = (PduLengthType)isotp->RX.length;
      isotp->RX.length = TpSduLength;
      isotp->RX.index = 0;
      ret = BUFREQ_OK;
    } else {
      ASLOG(J1939TPE, ("[%d] listen buffer too small %d < %d\n", id, (int)isotp->RX.length,
                       (int)TpSduLength));
      isotp->result = -__LINE__;
      isotp->sem.post();
    }
  }

  return ret;
}

extern "C" BufReq_ReturnType IsoTp_J1939TpCopyRxData(PduIdType id, const PduInfoType *info,
                                                     PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  isotp_t *isotp = isotp_get(id);
  if (NULL != isotp) {
    if ((isotp->RX.index + info->SduLength) <= isotp->RX.length) {
      ASLOG(J1939TP, ("[%d] copy rx data(%d)\n", id, info->SduLength));
      memcpy(&isotp->RX.data[isotp->RX.index], info->SduDataPtr, info->SduLength);
      isotp->RX.index += info->SduLength;
      ret = BUFREQ_OK;
      Std_TimerStart(&isotp->timerErrorNotify);
    } else {
      ASLOG(J1939TPE, ("[%d] listen buffer overflow\n", id));
      isotp->result = -__LINE__;
      isotp->sem.post();
    }
  }
  return ret;
}

extern "C" BufReq_ReturnType IsoTp_J1939TpCopyTxData(PduIdType id, const PduInfoType *info,
                                                     const RetryInfoType *retry,
                                                     PduLengthType *availableDataPtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  isotp_t *isotp = isotp_get(id);
  if (NULL != isotp) {
    if (isotp->TX.data != NULL) {
      if ((isotp->TX.index + info->SduLength) <= isotp->TX.length) {
        ASLOG(J1939TP, ("[%d] copy tx data(%d)\n", id, info->SduLength));
        memcpy(info->SduDataPtr, &isotp->TX.data[isotp->TX.index], info->SduLength);
        isotp->TX.index += info->SduLength;
        ret = BUFREQ_OK;
        Std_TimerStart(&isotp->timerErrorNotify);
      } else {
        ASLOG(J1939TPE, ("[%d] transmit buffer overflow\n", id));
        isotp->result = -__LINE__;
        isotp->sem.post();
      }
    } else {
      ASLOG(J1939TPE, ("[%d] no transmit buffer\n", id));
    }
  }
  return ret;
}

extern "C" void IsoTp_J1939TpRxIndication(PduIdType id, Std_ReturnType result) {
  isotp_t *isotp = isotp_get(id);
  if (NULL != isotp) {
    if (E_OK == result) {
      ASLOG(J1939TP, ("[%d] rx done\n", id));
      isotp->result = 0;
    } else {
      ASLOG(J1939TPE, ("[%d] rx failed\n", id));
      isotp->result = -__LINE__;
    }
    isotp->sem.post();
  }
}

extern "C" void IsoTp_J1939TpTxConfirmation(PduIdType id, Std_ReturnType result) {
  isotp_t *isotp = isotp_get(id);
  if (NULL != isotp) {
    if (E_OK == result) {
      ASLOG(J1939TP, ("[%d] tx done\n", id));
      isotp->result = 0;
    } else {
      ASLOG(J1939TPE, ("[%d] tx failed\n", id));
      isotp->result = -__LINE__;
    }
    isotp->sem.post();
  }
}
