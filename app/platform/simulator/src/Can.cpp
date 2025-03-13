/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2022 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Driver AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Can.h"
#include "CanIf_Can.h"
#include "CanIf.h"
#include "Can_Priv.h"
#include "canlib.h"
#include "Can_Cfg.h"
#include "Std_Critical.h"
#include "Std_Debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Std_Timer.h"
#include <queue>
#include "shell.h"
/* ================================ [ MACROS    ] ============================================== */
/* this simulation just alow only one HTH/HRH for each CAN controller */
#define CAN_MAX_HOH 64

#define CAN_CONFIG (&Can_Config)

#ifndef CAN_BUSOFF_SIMULAE_CANID
#define CAN_BUSOFF_SIMULAE_CANID 0xdeadbeef
#endif

#ifndef CAN_TX_TIMEOUT_SIMULAE_CANID
#define CAN_TX_TIMEOUT_SIMULAE_CANID 0xbeefdead
#endif

#ifndef USE_CAN_FILE_LOG
#define logCan(isRx, Controller, canid, dlc, data)
#endif
/* ================================ [ TYPES     ] ============================================== */
struct CanFrame {
  PduIdType handle;
  uint32_t canid;
  uint8_t dlc;
  uint8_t data[64];
};
/* ================================ [ DECLARES  ] ============================================== */
extern "C" Can_ConfigType Can_Config;
/* ================================ [ DATAS     ] ============================================== */
static uint64_t lOpenFlag = 0;
static uint64_t lWriteFlag = 0;
static PduIdType lswPduHandle[CAN_MAX_HOH];
static int lBusIdMap[CAN_MAX_HOH];
#ifdef USE_CAN_FILE_LOG
static FILE *lBusLog[CAN_MAX_HOH];
#endif
static std::queue<CanFrame> lPendingFrames[CAN_MAX_HOH];
static bool lStopConfirm[CAN_MAX_HOH];
/* ================================ [ LOCALS    ] ============================================== */
#ifndef _MSC_VER
FUNC(void, __weak) CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr) {
}

FUNC(void, __weak) CanIf_TxConfirmation(PduIdType CanTxPduId) {
}
#endif
#ifdef USE_CAN_FILE_LOG
static void logCan(boolean isRx, uint8_t Controller, uint32_t canid, uint8_t dlc,
                   const uint8_t *data) {
  static Std_TimerType timer;
  FILE *canLog = lBusLog[Controller];

  if (false == Std_IsTimerStarted(&timer)) {
    Std_TimerStart(&timer);
  }
  if (NULL != canLog) {
    uint32_t i;
    std_time_t elapsedTime = Std_GetTimerElapsedTime(&timer);
    float rtim = elapsedTime / 1000000.0;

    fprintf(canLog, "busid=%d %s canid=%04X dlc=%d data=[", Controller, isRx ? "rx" : "tx", canid,
            dlc);
    if (dlc < 8) {
      dlc = 8;
    }
    for (i = 0; i < dlc; i++) {
      fprintf(canLog, "%02X,", data[i]);
    }
    fprintf(canLog, "] @ %f s\n", rtim);
  }
}

static void __mcal_can_sim_deinit(void) {
  uint8_t i;
  for (i = 0; i < CAN_MAX_HOH; i++) {
    if (NULL != lBusLog[i]) {
      fprintf(lBusLog[i], "\n >> CLOSED << \n");
      fclose(lBusLog[i]);
    }
  }
}

INITIALIZER(__mcal_can_sim_init) {
  atexit(__mcal_can_sim_deinit);
}
#endif /* USE_CAN_FILE_LOG */

static void clear_queue(uint8_t Controller) {
  auto &queue = lPendingFrames[Controller];
  while (false == queue.empty()) {
    queue.pop();
  }
}

static void push_to_queue(uint8_t Controller, const Can_PduType *PduInfo) {
  auto &queue = lPendingFrames[Controller];
  CanFrame frame;
  frame.handle = PduInfo->swPduHandle;
  frame.canid = PduInfo->id;
  frame.dlc = PduInfo->length;
  memcpy(frame.data, PduInfo->sdu, PduInfo->length);
  queue.push(frame);
}

static bool pop_from_queue(uint8_t Controller, CanFrame &frame) {
  auto &queue = lPendingFrames[Controller];
  bool ret = false;

  if (false == queue.empty()) {
    frame = queue.front();
    queue.pop();
    ret = true;
  }

  return ret;
}
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType CanAc_SetupPinMode(const Can_CtrlPinType *pin) {
  return E_OK;
}

Std_ReturnType CanAc_WritePin(const Can_CtrlPinType *pin, uint8_t value) {
  return E_OK;
}

Std_ReturnType CanAc_Init(uint8_t Controller, const Can_ChannelConfigType *config) {
  Std_ReturnType ret = E_NOT_OK;
  if (0 == (lOpenFlag & ((uint64_t)1 << Controller))) {
    lBusIdMap[Controller] = can_open(config->device, config->hwInstanceId, config->baudrate);
    if (lBusIdMap[Controller] >= 0) {
      ret = E_OK;
      lOpenFlag |= ((uint64_t)1 << Controller);
      lWriteFlag = 0;
      clear_queue(Controller);
#ifdef USE_CAN_FILE_LOG
      snprintf(path, sizeof(path), ".CAN%d-%s-%d.log", Controller, config->device, config->port);
      lBusLog[Controller] = fopen(path, "wb");
#endif
      lStopConfirm[Controller] = false;
    }
  } else {
    ret = E_OK;
  }
  return ret;
}

Std_ReturnType CanAc_DeInit(uint8_t Controller, const Can_ChannelConfigType *config) {
  Std_ReturnType ret = E_NOT_OK;
  int rv;
  if (0 != (lOpenFlag & ((uint64_t)1 << Controller))) {
    rv = can_close(lBusIdMap[Controller]);
    if (TRUE == rv) {
      ret = E_OK;
      lOpenFlag &= ~((uint64_t)1 << Controller);
#ifdef USE_CAN_FILE_LOG
      if (NULL != lBusLog[Controller]) {
        fclose(lBusLog[Controller]);
        lBusLog[Controller] = NULL;
      }
#endif
      clear_queue(Controller);
    }
  } else {
    ret = E_OK;
  }
  return ret;
}

Std_ReturnType CanAc_SetSleepMode(uint8_t Controller, const Can_ChannelConfigType *config) {
  return CanAc_DeInit(Controller, config);
}

Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo) {
  Std_ReturnType ret = E_OK;
  int r;

  EnterCritical();
  if (lOpenFlag & ((uint64_t)1 << Hth)) {
    if (0 == (lWriteFlag & ((uint64_t)1 << Hth))) {
      lWriteFlag |= ((uint64_t)1 << Hth);
      InterLeaveCritical();
      r = can_write(lBusIdMap[Hth], PduInfo->id, PduInfo->length, PduInfo->sdu);
      InterEnterCritical();
      if (TRUE == r) {
        logCan(FALSE, Hth, PduInfo->id, PduInfo->length, PduInfo->sdu);
        lswPduHandle[Hth] = PduInfo->swPduHandle;
      } else {
        lWriteFlag &= ~((uint64_t)1 << Hth);
        ret = E_NOT_OK;
      }
    } else {
      push_to_queue(Hth, PduInfo);
    }
  } else {
    ret = E_NOT_OK;
  }
  ExitCritical();

  return ret;
}

void Can_MainFunction_WriteChannel(uint8_t Channel) {
  PduIdType swPduHandle;
  EnterCritical();
  if (Channel < CAN_MAX_HOH) {
    if (lWriteFlag & ((uint64_t)1 << Channel)) {
      swPduHandle = lswPduHandle[Channel];
      lWriteFlag &= ~((uint64_t)1 << Channel);
      if (STDIO_TX_CAN_HANDLE == swPduHandle) {
      } else {
        InterLeaveCritical();
        if (false == lStopConfirm[Channel]) {
          CanIf_TxConfirmation(swPduHandle);
        }
        InterEnterCritical();
      }
    }
    CanFrame frame;
    auto ret = pop_from_queue(Channel, frame);
    if (ret) {
      InterLeaveCritical();
      auto r = can_write(lBusIdMap[Channel], frame.canid, frame.dlc, frame.data);
      InterEnterCritical();
      if (TRUE == r) {
        logCan(FALSE, Channel, frame.canid, frame.dlc, frame.data);
        InterLeaveCritical();
        if (STDIO_TX_CAN_HANDLE == frame.handle) {
        } else {
          if (false == lStopConfirm[Channel]) {
            CanIf_TxConfirmation(frame.handle);
          }
        }
        InterEnterCritical();
      }
    }
  }
  ExitCritical();
}

void Can_MainFunction_Write(void) {
  uint8_t i;
  for (i = 0; i < CAN_MAX_HOH; i++) {
    Can_MainFunction_WriteChannel(i);
  }
}

extern "C" int Can_MainFunction_ReadChannelById(uint8_t Channel, uint32_t byId) {
  int r = FALSE;
  uint32_t canid;
  uint8_t dlc;
  uint8_t data[64];
  Can_HwType Mailbox;
  PduInfoType PduInfo;

  EnterCritical();
  if (Channel < CAN_MAX_HOH) {
    if (lOpenFlag & ((uint64_t)1 << Channel)) {
      canid = byId;
      dlc = sizeof(data);
      InterLeaveCritical();
      r = can_read(lBusIdMap[Channel], &canid, &dlc, data);
      InterEnterCritical();
#if defined(USE_STDIO_CAN) && defined(USE_SHELL)
      if ((TRUE == r) && (STDIO_RX_CANID == canid)) {
        for (r = 0; r < dlc; r++) {
          Shell_Input((char)data[r]);
        }
        r = FALSE;
      }
#endif
#ifdef USE_CANSM
      if ((TRUE == r) && (CAN_BUSOFF_SIMULAE_CANID == canid)) {
        ASLOG(INFO, ("[%d] Trigger BusOff\n", Channel));
        CanIf_ControllerBusOff(Channel);
        r = FALSE;
      }
      if ((TRUE == r) && (CAN_TX_TIMEOUT_SIMULAE_CANID == canid)) {
        ASLOG(INFO, ("[%d] Trigger TxTimeout\n", Channel));
        lStopConfirm[Channel] = true;
        r = FALSE;
      }
#endif
      if (TRUE == r) {
        Mailbox.CanId = canid;
        Mailbox.ControllerId = Channel;
        Mailbox.Hoh = Channel;
        PduInfo.SduLength = dlc;
        PduInfo.SduDataPtr = data;
        PduInfo.MetaDataPtr = (uint8_t *)&Mailbox;
        logCan(TRUE, Channel, canid, dlc, data);
        InterLeaveCritical();
        CanIf_RxIndication(&Mailbox, &PduInfo);
        InterEnterCritical();
      }
    }
  }
  ExitCritical();

  return r;
}

extern "C" boolean Can_WakeupCheck() { /* polling method to check wakeup CAN message */
  boolean bWakeup = FALSE;
  uint8_t Channel;
  uint32_t canid;
  uint8_t dlc;
  uint8_t data[64];
  bool r;

  EnterCritical();
  for (Channel = 0; Channel < CAN_MAX_HOH; Channel++) {
    if (lOpenFlag & ((uint64_t)1 << Channel)) {
      canid = -1;
      dlc = sizeof(data);
      InterLeaveCritical();
      r = can_read(lBusIdMap[Channel], &canid, &dlc, data);
      InterEnterCritical();
      if (TRUE == r) {
        bWakeup = TRUE;
        break;
      }
    }
  }
  ExitCritical();

  return bWakeup;
}

void Can_MainFunction_ReadChannel(uint8_t Channel) {
  Can_MainFunction_ReadChannelById(Channel, -1);
}

void Can_MainFunction_Read(void) {
  uint8_t i;
  for (i = 0; i < CAN_MAX_HOH; i++) {
    Can_MainFunction_ReadChannel(i);
  }
}

extern "C" void Can_Wait(uint8_t Channel, uint32_t canid, uint32_t timeoutMs) {

  EnterCritical();
  if (Channel < CAN_MAX_HOH) {
    if (lOpenFlag & ((uint64_t)1 << Channel)) {
      InterLeaveCritical();
      (void)can_wait(lBusIdMap[Channel], canid, timeoutMs);
      InterEnterCritical();
    }
  }
  ExitCritical();
}
