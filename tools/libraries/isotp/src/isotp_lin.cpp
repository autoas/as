/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifdef USE_LINTP
/* ================================ [ INCLUDES  ] ============================================== */
#include "isotp.h"
#include "isotp_types.hpp"
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
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
/* ID start from 1 as 0 mean LINIF_NULL_SCHEDULE */
#define LINIF_SCH_TABLE_APPLICATIVE 1
#define LINIF_SCH_TABLE_DIAG_REQUEST 2
#define LINIF_SCH_TABLE_DIAG_RESPONSE 3

#define UDS_TIMEOUT 5000000
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern "C" {
void LinTp_ReConfig(uint8_t channel, uint8_t linkLayerDataLength, uint16_t N_TA);
#ifdef USE_LINIF
void LinIf_ReConfig(uint8_t channel, uint8_t linkLayerDataLength, uint32_t rxid, uint32_t txid);
void Lin_ReConfig(uint8_t controller, const char *device, int port, uint32_t baudrate);
#endif

Std_ReturnType LinIf_DiagMRFCallback(uint8_t channel, Lin_PduType *frame,
                                     Std_ReturnType notifyResult);
Std_ReturnType LinIf_DiagSRFCallback(uint8_t channel, Lin_PduType *frame,
                                     Std_ReturnType notifyResult);
}
void IsoTp_LinDestroy(IsoTp_Type *isotp);
/* ================================ [ DATAS     ] ============================================== */
static IsoTp_Type isotpTp;
static int isotpServerUp = FALSE;
static std::mutex isotpMutex;
#ifndef USE_LINIF
static LinIf_SchHandleType isotpDiagRequestType = LINIF_NULL_SCHEDULE;
#endif
/* ================================ [ LOCALS    ] ============================================== */
#ifndef USE_LINIF
Std_ReturnType LinIf_ScheduleRequest(NetworkHandleType channel, LinIf_SchHandleType schedule) {
  isotpDiagRequestType = schedule;
  return E_OK;
}

static void IsoTp_LinSchedule(int busId, IsoTp_Type *isotp) {
  uint8_t data[64];
  Lin_PduType frame;
  Std_ReturnType r;
  bool ret;
  bool enahnced = true;
  int timeout = isotp->Params.U.LIN.Timeout + isotp->Params.U.LIN.Timeout / 5; /* ms */
  frame.SduPtr = data;
  frame.Dl = isotp->Params.LinkLayerDataLength;
  if (LINIF_SCH_TABLE_DIAG_REQUEST == isotpDiagRequestType) {
    r = LinIf_DiagMRFCallback(0, &frame, LINIF_R_TRIGGER_TRANSMIT);
    if (LINIF_R_OK == r) {
      ret = LinL_Write(busId, isotp->Params.U.LIN.TxId, frame.Dl, frame.SduPtr, enahnced);
      if (false == ret) {
        isotpTp.Result = -__LINE__;
        Std_TimerStop(&isotpTp.TimerErrorNotify);
        LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
        isotpTp.Sem.Post();
      }
    }
  } else if (LINIF_SCH_TABLE_DIAG_RESPONSE == isotpDiagRequestType) {
    ret = LinL_Read(busId, isotp->Params.U.LIN.RxId, frame.Dl, frame.SduPtr, enahnced, timeout);
    if (ret) {
      (void)LinIf_DiagSRFCallback(0, &frame, LINIF_R_RECEIVED_OK);
    }
  } else {
  }
}

extern "C" uint32_t LinIf_CanTpGetTxId(uint8_t channel) {
  return isotpTp.Params.U.LIN.TxId;
}

extern "C" uint32_t LinIf_CanTpGetRxId(uint8_t channel) {
  return isotpTp.Params.U.LIN.RxId;
}
#endif
static void IsoTp_LinServerMain(IsoTp_Type *isotp) {
  Std_TimerType timer10ms = {0};
#ifndef USE_LINIF
  int busId = -1;
  uint32_t timeout = isotp->Params.U.LIN.Timeout * 1000;
#endif

  LinTp_ReConfig(0, (uint8_t)isotp->Params.LinkLayerDataLength, (uint16_t)isotp->Params.N_TA);
#ifdef USE_LINIF
  LinIf_ReConfig(0, (uint8_t)isotp->Params.LinkLayerDataLength, isotp->Params.U.LIN.RxId,
                 isotp->Params.U.LIN.TxId);
  Lin_ReConfig(0, isotp->Params.Device, isotp->Params.Port, isotp->Params.Baudrate);
#else
  busId = LinL_Open(isotp->Params.Device, isotp->Params.Port, isotp->Params.Baudrate);
  if (busId < 0) {
    isotp->Result = -__LINE__;
    isotp->Sem.Post();
    return;
  }
  Dev_IoCtl(busId, DEV_IOCTL_SET_TIMEOUT, &timeout, sizeof(timeout));
#endif
#ifdef USE_LINIF
  Lin_Init(NULL);
  LinIf_Init(NULL);
#endif
  LinTp_Init(NULL);
  Std_TimerSet(&timer10ms, 10000);
  Std_TimerStop(&isotp->TimerErrorNotify);

  isotpServerUp = TRUE;
  isotp->Sem.Post();

  while (isotpServerUp) {
    if (TRUE == Std_IsTimerTimeout(&timer10ms)) {
      std::lock_guard<std::mutex> lg(isotp->Mutex);
      LinTp_MainFunction();
      Std_TimerSet(&timer10ms, 10000);
    }

    {
      std::lock_guard<std::mutex> lg(isotp->Mutex);
#ifdef USE_LINIF
      Lin_MainFunction();
      LinIf_MainFunction();
      Lin_MainFunction_Read();
#else
      IsoTp_LinSchedule(busId, isotp);
#endif
    }

    {
      std::lock_guard<std::mutex> lg(isotp->Mutex);
      if (Std_IsTimerStarted(&isotp->TimerErrorNotify)) {
        if (Std_GetTimerElapsedTime(&isotp->TimerErrorNotify) >= isotp->ErrorTimeout) {
          isotp->Result = -__LINE__;
          LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
          Std_TimerStop(&isotp->TimerErrorNotify);
          isotp->Sem.Post();
        }
      }
    }

    std::this_thread::sleep_for(1ms);
  }

#ifdef USE_LINIF
  LinIf_DeInit();
#else
  LinL_Close(busId);
#endif
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
    if (isotpTp.Params.LinkLayerDataLength == pduInfo.SduLength) {
      ret = LinTp_TriggerTransmit(0, &pduInfo);
      if (E_OK == ret) {
        r = LINIF_R_OK;
      } else {
        isotpTp.Result = -__LINE__;
        Std_TimerStop(&isotpTp.TimerErrorNotify);
        LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
        isotpTp.Sem.Post();
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
    if (isotpTp.Params.LinkLayerDataLength == pduInfo.SduLength) {
      LinTp_RxIndication(0, &pduInfo);
    }
  } else {
    isotpTp.Result = -__LINE__;
    Std_TimerStop(&isotpTp.TimerErrorNotify);
    LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
    isotpTp.Sem.Post();
  }

  return r;
}

extern "C" BufReq_ReturnType LinTp_StartOfReception(PduIdType id, const PduInfoType *info,
                                                    PduLengthType tpSduLength,
                                                    PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_OK;

  if (tpSduLength <= sizeof(isotpTp.Rx.Data)) {
    *bufferSizePtr = isotpTp.Rx.Length;
    isotpTp.Rx.Length = tpSduLength;
    isotpTp.Rx.Index = 0;
  } else {
    ret = BUFREQ_E_NOT_OK;
  }

  return ret;
}

extern "C" BufReq_ReturnType LinTp_CopyRxData(PduIdType id, const PduInfoType *info,
                                              PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_OK;

  if (isotpTp.Rx.Index < isotpTp.Rx.Length) {
    memcpy(&isotpTp.Rx.Data[isotpTp.Rx.Index], info->SduDataPtr, info->SduLength);
    isotpTp.Rx.Index += info->SduLength;
    *bufferSizePtr = isotpTp.Rx.Length - isotpTp.Rx.Index;
  } else {
    ret = BUFREQ_E_NOT_OK;
  }

  return ret;
}

extern "C" void LinTp_TpRxIndication(PduIdType id, Std_ReturnType result) {
  if (E_OK == result) {
    isotpTp.Result = 0;
    Std_TimerStop(&isotpTp.TimerErrorNotify);
    LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
    isotpTp.Sem.Post();
  } else {
    /* FAILED */
    isotpTp.Result = -__LINE__;
    Std_TimerStop(&isotpTp.TimerErrorNotify);
    LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
    isotpTp.Sem.Post();
  }
}

extern "C" BufReq_ReturnType LinTp_CopyTxData(PduIdType id, const PduInfoType *info,
                                              const RetryInfoType *retry,
                                              PduLengthType *availableDataPtr) {

  BufReq_ReturnType ret = BUFREQ_OK;

  if ((isotpTp.Tx.Data != NULL) && (isotpTp.Tx.Index < isotpTp.Tx.Length)) {
    memcpy(info->SduDataPtr, &isotpTp.Tx.Data[isotpTp.Tx.Index], info->SduLength);
    isotpTp.Tx.Index += info->SduLength;
    *availableDataPtr = isotpTp.Tx.Length - isotpTp.Tx.Index;
  } else {
    ret = BUFREQ_E_NOT_OK;
  }

  return ret;
}

extern "C" void LinTp_TpTxConfirmation(PduIdType id, Std_ReturnType result) {
  if (E_OK == result) {
    isotpTp.Result = 0;
    Std_TimerStop(&isotpTp.TimerErrorNotify);
    LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
    isotpTp.Sem.Post();
  } else {
    isotpTp.Result = -88;
    Std_TimerStop(&isotpTp.TimerErrorNotify);
    LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
    isotpTp.Sem.Post();
  }
}

IsoTp_Type *IsoTp_LinCreate(IsoTp_ParameterType *params) {
  IsoTp_Type *isotp;
  int r = 0;

  std::unique_lock<std::mutex> lck(isotpMutex);
  if (isotpServerUp == TRUE) {
    return NULL;
  }

  isotp = &isotpTp;
  isotp->Params = *params;
  Std_TimerStop(&isotp->TimerErrorNotify);

  isotp->ServerThread = std::thread(IsoTp_LinServerMain, isotp);

  isotp->Sem.Wait();
  r = isotp->Result;
  if (0 != r) {
    isotp->Running = FALSE;
    IsoTp_LinDestroy(isotp);
    isotp = NULL;
  }

  return isotp;
}

int IsoTp_LinTransmit(IsoTp_Type *isotp, const uint8_t *txBuffer, size_t txSize, uint8_t *rxBuffer,
                      size_t rxSize) {
  int r = 0;
  PduInfoType PduInfo;

  {
    std::lock_guard<std::mutex> lg(isotp->Mutex);
    isotp->Result = -1;
    isotp->Tx.Data = (uint8_t *)txBuffer;
    isotp->Tx.Length = txSize;
    isotp->Tx.Index = 0;
    isotp->Rx.Length = sizeof(isotp->Rx.Data);
    isotp->Rx.Index = 0;
    PduInfo.SduDataPtr = (uint8_t *)txBuffer;
    PduInfo.SduLength = txSize;
    isotp->ErrorTimeout = UDS_TIMEOUT;
    r = LinTp_Transmit(0, &PduInfo);
    if (E_OK == r) {
      LinIf_ScheduleRequest(0, LINIF_SCH_TABLE_DIAG_REQUEST);
      Std_TimerStart(&isotp->TimerErrorNotify);
    }
  }

  if (E_OK == r) {
    isotp->Sem.Wait();
    r = isotp->Result;

    if (0 == r) {
      if (NULL != rxBuffer) {
        if (isotp->Params.U.LIN.DelayUs > 0) {
          /* give slave some time to process, generally wait response to be ready */
          std::this_thread::sleep_for(std::chrono::microseconds(isotp->Params.U.LIN.DelayUs));
        }
        {
          std::lock_guard<std::mutex> lg(isotp->Mutex);
          LinIf_ScheduleRequest(0, LINIF_SCH_TABLE_DIAG_RESPONSE);
          isotp->ErrorTimeout = UDS_TIMEOUT;
          Std_TimerStart(&isotp->TimerErrorNotify);
          isotp->Sem.Wait();
        }
        r = isotp->Result;
        if (0 == r) {
          std::lock_guard<std::mutex> lg(isotp->Mutex);
          r = isotp->Rx.Index;
          if ((int)rxSize >= r) {
            memcpy(rxBuffer, isotp->Rx.Data, r);
          } else {
            r = -__LINE__;
          }
        }
      }
    }
  } else {
    r = -__LINE__;
  }

  {
    std::lock_guard<std::mutex> lg(isotp->Mutex);
    isotp->Tx.Data = NULL;
    LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
    Std_TimerStop(&isotp->TimerErrorNotify);
  }

  if (0 == r) {
    if (isotp->Params.U.LIN.DelayUs > 0) {
      /* give slave some time to process */
      std::this_thread::sleep_for(std::chrono::microseconds(isotp->Params.U.LIN.DelayUs));
    }
  }

  return r;
}

int IsoTp_LinReceive(IsoTp_Type *isotp, uint8_t *rxBuffer, size_t rxSize) {

  int r = 0;
  {
    std::lock_guard<std::mutex> lg(isotp->Mutex);
    LinIf_ScheduleRequest(0, LINIF_SCH_TABLE_DIAG_RESPONSE);
    isotp->ErrorTimeout = UDS_TIMEOUT;
    Std_TimerStart(&isotp->TimerErrorNotify);
  }
  isotp->Sem.Wait();
  r = isotp->Result;
  if (0 == r) {
    std::lock_guard<std::mutex> lg(isotp->Mutex);
    r = isotp->Rx.Index;
    if ((int)rxSize >= r) {
      memcpy(rxBuffer, isotp->Rx.Data, r);
    } else {
      r = -__LINE__;
    }
  }

  std::lock_guard<std::mutex> lg(isotp->Mutex);
  isotp->Rx.Length = sizeof(isotp->Rx.Data);
  isotp->Rx.Index = 0;
  LinIf_ScheduleRequest(0, LINIF_NULL_SCHEDULE);
  Std_TimerStop(&isotp->TimerErrorNotify);

  return r;
}

void IsoTp_LinDestroy(IsoTp_Type *isotp) {
  isotpServerUp = FALSE;
  if (isotp->ServerThread.joinable()) {
    isotp->ServerThread.join();
  }
}

int IsoTp_LinIoCtl(IsoTp_Type *isotp, int cmd, const void *data, size_t size) {
  return -__LINE__;
}
#endif /* USE_LINTP */
