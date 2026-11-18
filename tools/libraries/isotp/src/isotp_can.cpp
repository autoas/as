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

#define ISOTP_DEFAULT_TIMEOUT 5000000

#define AS_LOG_ISOTP 0
#define AS_LOG_ISOTPE 3

#define ISOTP_CAN_RELAX 20
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern "C" int Can_MainFunction_ReadChannelById(uint8_t channel, uint32_t byId);
extern "C" void CanIf_CanTpSetTxCanId(uint8_t channel, uint32_t txCanId);
extern "C" void Can_Wait(uint8_t channel, uint32_t canid, uint32_t timeoutMs);
int IsoTp_CanReceive(IsoTp_Type *isotp, uint8_t *rxBuffer, size_t rxSize);
void IsoTp_CanDestroy(IsoTp_Type *isotp);
/* ================================ [ DATAS     ] ============================================== */
static IsoTp_Type isotpTp[CANTP_MAX_CHANNELS];
static std::mutex isotpMutex;
/* ================================ [ LOCALS    ] ============================================== */
static void IsoTp_CanServerMain(IsoTp_Type *isotp) {
  Std_TimerType timer10ms = {0};
  uint8_t Channel = isotp->Channel;
  CanTp_ParamType param;
  Std_ReturnType ret;
  int relax = ISOTP_CAN_RELAX;

  param.device = isotp->Params.Device;
  param.baudrate = isotp->Params.Baudrate;
  param.port = isotp->Params.Port;
  param.RxCanId = isotp->Params.U.CAN.RxCanId;
  param.TxCanId = isotp->Params.U.CAN.TxCanId;
  param.N_TA = isotp->Params.N_TA;
  param.LinkLayerDataLength = (uint8_t)isotp->Params.LinkLayerDataLength;
  param.STmin = isotp->Params.U.CAN.STmin;

  CanTp_ReConfig(Channel, &param);
  CanTp_InitChannel(Channel);
  ret = Can_SetControllerMode(Channel, CAN_CS_STARTED);

  Std_TimerSet(&timer10ms, 10000);
  Std_TimerStop(&isotp->TimerErrorNotify);

  if (E_OK == ret) {
    isotp->Result = 0;
  } else {
    ASLOG(ISOTPE, ("[%d] failed to open %s:%d\n", Channel, param.device, param.port));
    isotp->Result = __LINE__;
    isotp->Running = FALSE;
  }
  isotp->Sem.Post();

  while (TRUE == isotp->Running) {
    {
      std::lock_guard<std::mutex> lg(isotp->Mutex);
      Can_MainFunction_WriteChannel(Channel);
      Can_MainFunction_ReadChannelById(Channel, param.RxCanId);
    }

    if (TRUE == Std_IsTimerTimeout(&timer10ms)) {
      std::lock_guard<std::mutex> lg(isotp->Mutex);
      CanTp_MainFunction_Channel(Channel);
      Std_TimerSet(&timer10ms, 10000);
    }

    {
      std::lock_guard<std::mutex> lg(isotp->Mutex);
      CanTp_MainFunction_ChannelFast(Channel);
      if (Std_IsTimerStarted(&isotp->TimerErrorNotify)) {
        if (Std_GetTimerElapsedTime(&isotp->TimerErrorNotify) >= isotp->ErrorTimeout) {
          isotp->Result = -__LINE__;
          ASLOG(ISOTPE, ("[%d] timeout\n", Channel));
          Std_TimerStop(&isotp->TimerErrorNotify);
          isotp->Sem.Post();
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

static IsoTp_Type *IsoTp_Get(PduIdType id) {
  IsoTp_Type *isotp = NULL;
  if (id < CANTP_MAX_CHANNELS) {
    if (isotpTp[id].Running) {
      isotp = &isotpTp[id];
    } else {
      ASLOG(ISOTPE, ("[%d] is dead\n", id));
    }
  } else {
    ASLOG(ISOTPE, ("isotp tx with invalid id %d\n", id));
  }
  return isotp;
}
/* ================================ [ FUNCTIONS ] ============================================== */
IsoTp_Type *IsoTp_CanCreate(IsoTp_ParameterType *params) {
  IsoTp_Type *isotp = NULL;
  int r = 0;
  size_t i;

  std::unique_lock<std::mutex> lck(isotpMutex);
  for (i = 0; i < ARRAY_SIZE(isotpTp); i++) {
    if (TRUE == isotpTp[i].Running) {
      if ((0 == strcmp(isotpTp[i].Params.Device, params->Device)) &&
          (isotpTp[i].Params.Port == params->Port) &&
          ((isotpTp[i].Params.U.CAN.RxCanId == params->U.CAN.RxCanId) ||
           (isotpTp[i].Params.U.CAN.TxCanId == params->U.CAN.TxCanId))) {
        ASLOG(ISOTPE, ("isotp CAN %s:%d %X:%X already opened\n", params->Device, params->Port,
                       params->U.CAN.RxCanId, params->U.CAN.TxCanId));
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
    isotp->ErrorTimeout = ISOTP_DEFAULT_TIMEOUT;
    Std_TimerStop(&isotp->TimerErrorNotify);
    isotp->ServerThread = std::thread(IsoTp_CanServerMain, isotp);
    isotp->Sem.Wait();
    r = isotp->Result;
    if (0 != r) {
      isotp->Running = FALSE;
      IsoTp_CanDestroy(isotp);
      isotp = NULL;
    }
  }

  return isotp;
}

int IsoTp_CanTransmit(IsoTp_Type *isotp, const uint8_t *txBuffer, size_t txSize, uint8_t *rxBuffer,
                      size_t rxSize) {
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
    Std_TimerStart(&isotp->TimerErrorNotify);
    isotp->Tx.Data = (uint8_t *)txBuffer;
    isotp->Tx.Length = txSize;
    isotp->Tx.Index = 0;
    isotp->Rx.Length = sizeof(isotp->Rx.Data);
    isotp->Rx.Index = 0;
    isotp->Rx.IsInUse = FALSE;
    r = (int)PduR_DcmTransmit(isotp->Channel, &PduInfo);
  }

  if (r == E_OK) {
    isotp->Sem.Wait();
    r = isotp->Result;
    if (0 == r) {
      if (NULL != rxBuffer) {
        r = IsoTp_CanReceive(isotp, rxBuffer, rxSize);
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

int IsoTp_CanReceive(IsoTp_Type *isotp, uint8_t *rxBuffer, size_t rxSize) {

  int r = 0;
  {
    std::lock_guard<std::mutex> lg(isotp->Mutex);
    Std_TimerStart(&isotp->TimerErrorNotify);
  }

  isotp->Sem.Wait();
  r = isotp->Result;
  if (0 == r) {
    std::lock_guard<std::mutex> lg(isotp->Mutex);
    if ((rxSize >= isotp->Rx.Index) && (isotp->Rx.Length == isotp->Rx.Index)) {
      r = isotp->Rx.Index;
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

int IsoTp_CanIoCtl(IsoTp_Type *isotp, int cmd, const void *data, size_t size) {
  int r = -__LINE__;
  std::lock_guard<std::mutex> lg(isotp->Mutex);
  switch (cmd) {
  case ISOTP_IOCTL_SET_TX_ID:
    if ((NULL != data) && (size == sizeof(uint32_t))) {
      uint32_t txId = *(uint32_t *)data;
      *(uint32_t *)data = isotp->Params.U.CAN.TxCanId;
      isotp->Params.U.CAN.TxCanId = txId;
      CanIf_CanTpSetTxCanId(isotp->Channel, txId);
      r = 0;
    }
    break;
  case ISOTP_IOCTL_SET_TIMEOUT:
    if ((NULL != data) && (size == sizeof(uint32_t))) {
      uint32_t timeoutUs = *(uint32_t *)data;
      *(uint32_t *)data = isotp->ErrorTimeout;
      isotp->ErrorTimeout = timeoutUs;
      r = 0;
    }
  default:
    break;
  }

  return r;
}

void IsoTp_CanDestroy(IsoTp_Type *isotp) {
  isotp->Running = FALSE;
  if (isotp->ServerThread.joinable()) {
    isotp->ServerThread.join();
  }
}

extern "C" BufReq_ReturnType IsoTp_CanTpStartOfReception(PduIdType id, const PduInfoType *info,
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
      ASLOG(ISOTP, ("[%d] start reception\n", id));
      *bufferSizePtr = (PduLengthType)isotp->Rx.Length;
      isotp->Rx.Length = tpSduLength;
      isotp->Rx.Index = 0;
      isotp->Rx.IsInUse = TRUE;
      ret = BUFREQ_OK;
    } else {
      ASLOG(ISOTPE, ("[%d] listen buffer too small %d < %d\n", id, (int)isotp->Rx.Length,
                     (int)tpSduLength));
      isotp->Result = -__LINE__;
      isotp->Sem.Post();
    }
  }

  return ret;
}

extern "C" BufReq_ReturnType IsoTp_CanTpCopyRxData(PduIdType id, const PduInfoType *info,
                                                   PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  IsoTp_Type *isotp = IsoTp_Get(id);
  if (NULL != isotp) {
    if ((isotp->Rx.Index + info->SduLength) <= isotp->Rx.Length) {
      ASLOG(ISOTP, ("[%d] copy rx data(%d)\n", id, info->SduLength));
      memcpy(&isotp->Rx.Data[isotp->Rx.Index], info->SduDataPtr, info->SduLength);
      isotp->Rx.Index += info->SduLength;
      ret = BUFREQ_OK;
      Std_TimerStart(&isotp->TimerErrorNotify);
    } else {
      ASLOG(ISOTPE, ("[%d] listen buffer overflow\n", id));
      isotp->Result = -__LINE__;
      isotp->Sem.Post();
    }
  }
  return ret;
}

extern "C" BufReq_ReturnType IsoTp_CanTpCopyTxData(PduIdType id, const PduInfoType *info,
                                                   const RetryInfoType *retry,
                                                   PduLengthType *availableDataPtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  IsoTp_Type *isotp = IsoTp_Get(id);
  if (NULL != isotp) {
    if (isotp->Tx.Data != NULL) {
      if ((isotp->Tx.Index + info->SduLength) <= isotp->Tx.Length) {
        ASLOG(ISOTP, ("[%d] copy tx data(%d)\n", id, info->SduLength));
        memcpy(info->SduDataPtr, &isotp->Tx.Data[isotp->Tx.Index], info->SduLength);
        isotp->Tx.Index += info->SduLength;
        ret = BUFREQ_OK;
        Std_TimerStart(&isotp->TimerErrorNotify);
      } else {
        ASLOG(ISOTPE, ("[%d] transmit buffer overflow\n", id));
        isotp->Result = -__LINE__;
        isotp->Sem.Post();
      }
    } else {
      ASLOG(ISOTPE, ("[%d] no transmit buffer\n", id));
    }
  }
  return ret;
}

extern "C" void IsoTp_CanTpRxIndication(PduIdType id, Std_ReturnType result) {
  IsoTp_Type *isotp = IsoTp_Get(id);
  if (NULL != isotp) {
    if (E_OK == result) {
      ASLOG(ISOTP, ("[%d] rx done\n", id));
      isotp->Result = 0;
    } else {
      ASLOG(ISOTPE, ("[%d] rx failed\n", id));
      isotp->Result = -__LINE__;
    }
    isotp->Sem.Post();
  }
}

extern "C" void IsoTp_CanTpTxConfirmation(PduIdType id, Std_ReturnType result) {
  IsoTp_Type *isotp = IsoTp_Get(id);
  if (NULL != isotp) {
    if (E_OK == result) {
      ASLOG(ISOTP, ("[%d] tx done\n", id));
      isotp->Result = 0;
    } else {
      ASLOG(ISOTPE, ("[%d] tx failed\n", id));
      isotp->Result = -__LINE__;
    }
    isotp->Sem.Post();
  }
}
