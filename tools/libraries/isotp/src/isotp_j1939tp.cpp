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
extern "C" int Can_MainFunction_ReadChannelById(uint8_t channel, uint32_t byId);
int IsoTp_J1939TpReceive(IsoTp_Type *isotp, uint8_t *rxBuffer, size_t rxSize);
void IsoTp_J1939TpDestroy(IsoTp_Type *isotp);
/* ================================ [ DATAS     ] ============================================== */
static IsoTp_Type isotpTp[J1939TP_MAX_CHANNELS];
static std::mutex isotpMutex;
/* ================================ [ LOCALS    ] ============================================== */
static void IsoTp_J1939TpServerMain(IsoTp_Type *isotp) {
  Std_TimerType timer10ms = {0};
  uint8_t Channel = isotp->Channel;
  J1939Tp_ParamType param;
  Std_ReturnType ret;
  uint8_t controller = CANTP_MAX_CHANNELS + Channel;

  param.device = isotp->Params.Device;
  param.baudrate = isotp->Params.Baudrate;
  param.port = isotp->Params.Port;
  param.RX.CM = isotp->Params.U.J1939TP.RX.CM;
  param.RX.DT = isotp->Params.U.J1939TP.RX.DT;
  param.RX.Direct = isotp->Params.U.J1939TP.RX.Direct;
  param.RX.FC = isotp->Params.U.J1939TP.RX.FC;
  param.TX.CM = isotp->Params.U.J1939TP.TX.CM;
  param.TX.DT = isotp->Params.U.J1939TP.TX.DT;
  param.TX.Direct = isotp->Params.U.J1939TP.TX.Direct;
  param.TX.FC = isotp->Params.U.J1939TP.TX.FC;
  param.STMin = isotp->Params.U.J1939TP.STMin;
  param.Tr = isotp->Params.U.J1939TP.Tr;
  param.T1 = isotp->Params.U.J1939TP.T1;
  param.T2 = isotp->Params.U.J1939TP.T2;
  param.T3 = isotp->Params.U.J1939TP.T3;
  param.T4 = isotp->Params.U.J1939TP.T4;
  param.TxMaxPacketsPerBlock = isotp->Params.U.J1939TP.TxMaxPacketsPerBlock;
  param.protocol = (uint8_t)isotp->Params.U.J1939TP.Protocol;
  param.LinkLayerDataLength = (uint8_t)isotp->Params.LinkLayerDataLength;

  J1939Tp_ReConfig(Channel, &param);
  J1939Tp_InitRxChannel(Channel);
  J1939Tp_InitTxChannel(Channel);
  J1939Tp_SetTxPgPGN(Channel, isotp->Params.U.J1939TP.PgPGN);
  ret = Can_SetControllerMode(controller, CAN_CS_STARTED);

  Std_TimerSet(&timer10ms, 10000);
  Std_TimerStop(&isotp->TimerErrorNotify);

  if (E_OK == ret) {
    isotp->Result = 0;
  } else {
    ASLOG(J1939TPE, ("[%d] failed to open %s:%d\n", Channel, param.device, param.port));
    isotp->Result = __LINE__;
    isotp->Running = FALSE;
  }
  isotp->Sem.Post();

  while (TRUE == isotp->Running) {
    {
      std::lock_guard<std::mutex> lg(isotp->Mutex);
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
      std::lock_guard<std::mutex> lg(isotp->Mutex);
      J1939Tp_MainFunction_RxChannel(Channel);
      J1939Tp_MainFunction_TxChannel(Channel);
      Std_TimerSet(&timer10ms, 10000);
    }

    {
      std::lock_guard<std::mutex> lg(isotp->Mutex);
      J1939Tp_MainFunction_RxChannelFast(Channel);
      J1939Tp_MainFunction_TxChannelFast(Channel);
      if (Std_IsTimerStarted(&isotp->TimerErrorNotify)) {
        if (Std_GetTimerElapsedTime(&isotp->TimerErrorNotify) >= isotp->ErrorTimeout) {
          isotp->Result = -__LINE__;
          Std_TimerStop(&isotp->TimerErrorNotify);
          isotp->Sem.Post();
        }
      }
    }

    std::this_thread::sleep_for(1ms);
  }

  Can_SetControllerMode(controller, CAN_CS_STOPPED);
}

static IsoTp_Type *IsoTp_Get(PduIdType id) {
  IsoTp_Type *isotp = NULL;
  if (id < J1939TP_MAX_CHANNELS) {
    if (isotpTp[id].Running) {
      isotp = &isotpTp[id];
    } else {
      ASLOG(J1939TPE, ("[%d] is dead\n", id));
    }
  } else {
    ASLOG(J1939TPE, ("isotp tx with invalid id %d\n", id));
  }
  return isotp;
}
/* ================================ [ FUNCTIONS ] ============================================== */
IsoTp_Type *IsoTp_J1939TpCreate(IsoTp_ParameterType *params) {
  IsoTp_Type *isotp = NULL;
  int r = 0;
  size_t i;

  std::lock_guard<std::mutex> lg(isotpMutex);
  for (i = 0; i < ARRAY_SIZE(isotpTp); i++) {
    if (TRUE == isotpTp[i].Running) {
      if ((0 == strcmp(isotpTp[i].Params.Device, params->Device)) &&
          (isotpTp[i].Params.Port == params->Port) &&
          ((0 == memcmp(&isotpTp[i].Params.U.J1939TP.RX, &params->U.J1939TP.RX, 16)) ||
           (0 == memcmp(&isotpTp[i].Params.U.J1939TP.TX, &params->U.J1939TP.TX, 16)))) {
        ASLOG(J1939TPE,
              ("isotp CAN %s:%d Rx[%X %X %X %X] Tx[%X %X %X %X] already opened\n", params->Device,
               params->Port, params->U.J1939TP.RX.CM, params->U.J1939TP.RX.DT,
               params->U.J1939TP.RX.Direct, params->U.J1939TP.RX.FC, params->U.J1939TP.TX.CM,
               params->U.J1939TP.TX.DT, params->U.J1939TP.TX.Direct, params->U.J1939TP.TX.FC));
        r = -__LINE__;
        break;
      }
    }
  }

  if (0 == r) {
    for (i = 0; i < ARRAY_SIZE(isotpTp); i++) {
      if (FALSE == isotpTp[i].Running) {
        isotp = &isotpTp[i];
        isotp->Running = TRUE;
        isotp->Channel = (uint8_t)i;
        break;
      }
    }
  }

  if (NULL != isotp) {
    isotp->Params = *params;
    Std_TimerStop(&isotp->TimerErrorNotify);
    isotp->ServerThread = std::thread(IsoTp_J1939TpServerMain, isotp);
    isotp->Sem.Wait();
    r = isotp->Result;
    if (0 != r) {
      isotp->Running = FALSE;
      IsoTp_J1939TpDestroy(isotp);
      isotp = NULL;
    }
  }

  return isotp;
}

int IsoTp_J1939TpTransmit(IsoTp_Type *isotp, const uint8_t *txBuffer, size_t txSize,
                          uint8_t *rxBuffer, size_t rxSize) {
  int r = 0;
  PduInfoType PduInfo;
  PduInfo.SduDataPtr = (uint8_t *)txBuffer;
  PduInfo.SduLength = txSize;

  do { /* current server thread has a chance post sem for several times, so here consume all the
          original sem post */
    r = isotp->Sem.Wait(0);
  } while (0 == r);

  {
    std::lock_guard<std::mutex> lg(isotp->Mutex);
    isotp->Result = -__LINE__;
    isotp->ErrorTimeout = 5000000;
    Std_TimerStart(&isotp->TimerErrorNotify);
    isotp->Tx.Data = (uint8_t *)txBuffer;
    isotp->Tx.Length = txSize;
    isotp->Tx.Index = 0;
    isotp->Rx.Length = sizeof(isotp->Rx.Data);
    isotp->Rx.Index = 0;
    isotp->Rx.IsInUse = FALSE;
    r = (int)PduR_J1939TpTransmit(isotp->Channel + 2 * CANTP_MAX_CHANNELS, &PduInfo);
  }

  if (r == E_OK) {
    isotp->Sem.Wait();
    r = isotp->Result;

    if (0 == r) {
      if (NULL != rxBuffer) {
        r = IsoTp_J1939TpReceive(isotp, rxBuffer, rxSize);
      }
    }
  } else {
    r = -__LINE__;
  }

  std::lock_guard<std::mutex> lg(isotp->Mutex);
  isotp->Tx.Data = NULL;
  Std_TimerStop(&isotp->TimerErrorNotify);

  return r;
}

int IsoTp_J1939TpReceive(IsoTp_Type *isotp, uint8_t *rxBuffer, size_t rxSize) {

  int r = 0;
  {
    std::lock_guard<std::mutex> lg(isotp->Mutex);
    isotp->ErrorTimeout = 5000000;
    Std_TimerStart(&isotp->TimerErrorNotify);
  }
  isotp->Sem.Wait();
  r = isotp->Result;
  if (0 == r) {
    std::lock_guard<std::mutex> lg(isotp->Mutex);
    r = isotp->Rx.Index;
    if ((rxSize >= isotp->Rx.Index) && (isotp->Rx.Length == isotp->Rx.Index)) {
      memcpy(rxBuffer, isotp->Rx.Data, r);
    } else {
      r = -__LINE__;
    }
  }

  std::lock_guard<std::mutex> lg(isotp->Mutex);
  isotp->Rx.Length = sizeof(isotp->Rx.Data);
  isotp->Rx.Index = 0;
  isotp->Rx.IsInUse = FALSE;
  Std_TimerStop(&isotp->TimerErrorNotify);

  return r;
}

int IsoTp_J1939TpIoCtl(IsoTp_Type *isotp, int cmd, const void *data, size_t size) {
  int r = -__LINE__;
  std::lock_guard<std::mutex> lg(isotp->Mutex);
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

void IsoTp_J1939TpDestroy(IsoTp_Type *isotp) {
  isotp->Running = FALSE;
  if (isotp->ServerThread.joinable()) {
    isotp->ServerThread.join();
  }
}

extern "C" BufReq_ReturnType IsoTp_J1939TpStartOfReception(PduIdType id, const PduInfoType *info,
                                                           PduLengthType tpSduLength,
                                                           PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  IsoTp_Type *isotp = IsoTp_Get(id);
  if (NULL != isotp) {
    while (TRUE == isotp->Rx.IsInUse) {
      /* it was possible that the response pending and the positive response received at the same
       * time, thus we need to ensure the previous received buffer consumed */
      isotp->Mutex.unlock();
      std::this_thread::sleep_for(1ms); /* wait the buffer to be released */
      isotp->Mutex.lock();
    }
    if (sizeof(isotp->Rx.Data) >= tpSduLength) {
      ASLOG(J1939TP, ("[%d] start reception\n", id));
      *bufferSizePtr = (PduLengthType)isotp->Rx.Length;
      isotp->Rx.Length = tpSduLength;
      isotp->Rx.Index = 0;
      ret = BUFREQ_OK;
    } else {
      ASLOG(J1939TPE, ("[%d] listen buffer too small %d < %d\n", id, (int)isotp->Rx.Length,
                       (int)tpSduLength));
      isotp->Result = -__LINE__;
      isotp->Sem.Post();
    }
  }

  return ret;
}

extern "C" BufReq_ReturnType IsoTp_J1939TpCopyRxData(PduIdType id, const PduInfoType *info,
                                                     PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  IsoTp_Type *isotp = IsoTp_Get(id);
  if (NULL != isotp) {
    if ((isotp->Rx.Index + info->SduLength) <= isotp->Rx.Length) {
      ASLOG(J1939TP, ("[%d] copy rx data(%d)\n", id, info->SduLength));
      memcpy(&isotp->Rx.Data[isotp->Rx.Index], info->SduDataPtr, info->SduLength);
      isotp->Rx.Index += info->SduLength;
      ret = BUFREQ_OK;
      Std_TimerStart(&isotp->TimerErrorNotify);
    } else {
      ASLOG(J1939TPE, ("[%d] listen buffer overflow\n", id));
      isotp->Result = -__LINE__;
      isotp->Sem.Post();
    }
  }
  return ret;
}

extern "C" BufReq_ReturnType IsoTp_J1939TpCopyTxData(PduIdType id, const PduInfoType *info,
                                                     const RetryInfoType *retry,
                                                     PduLengthType *availableDataPtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  IsoTp_Type *isotp = IsoTp_Get(id);
  if (NULL != isotp) {
    if (isotp->Tx.Data != NULL) {
      if ((isotp->Tx.Index + info->SduLength) <= isotp->Tx.Length) {
        ASLOG(J1939TP, ("[%d] copy tx data(%d)\n", id, info->SduLength));
        memcpy(info->SduDataPtr, &isotp->Tx.Data[isotp->Tx.Index], info->SduLength);
        isotp->Tx.Index += info->SduLength;
        ret = BUFREQ_OK;
        Std_TimerStart(&isotp->TimerErrorNotify);
      } else {
        ASLOG(J1939TPE, ("[%d] transmit buffer overflow\n", id));
        isotp->Result = -__LINE__;
        isotp->Sem.Post();
      }
    } else {
      ASLOG(J1939TPE, ("[%d] no transmit buffer\n", id));
    }
  }
  return ret;
}

extern "C" void IsoTp_J1939TpRxIndication(PduIdType id, Std_ReturnType result) {
  IsoTp_Type *isotp = IsoTp_Get(id);
  if (NULL != isotp) {
    if (E_OK == result) {
      ASLOG(J1939TP, ("[%d] rx done\n", id));
      isotp->Result = 0;
    } else {
      ASLOG(J1939TPE, ("[%d] rx failed\n", id));
      isotp->Result = -__LINE__;
    }
    isotp->Sem.Post();
  }
}

extern "C" void IsoTp_J1939TpTxConfirmation(PduIdType id, Std_ReturnType result) {
  IsoTp_Type *isotp = IsoTp_Get(id);
  if (NULL != isotp) {
    if (E_OK == result) {
      ASLOG(J1939TP, ("[%d] tx done\n", id));
      isotp->Result = 0;
    } else {
      ASLOG(J1939TPE, ("[%d] tx failed\n", id));
      isotp->Result = -__LINE__;
    }
    isotp->Sem.Post();
  }
}
