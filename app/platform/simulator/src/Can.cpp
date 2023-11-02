/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2022 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Driver AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Can.h"
#include "CanIf_Can.h"
#include "canlib.h"
#include "Can_Lcfg.h"
#include <pthread.h>
#include "Std_Critical.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Std_Timer.h"
#include <queue>
#include "shell.h"
/* ================================ [ MACROS    ] ============================================== */
/* this simulation just alow only one HTH/HRH for each CAN controller */
#define CAN_MAX_HOH 32

#define CAN_CONFIG (&Can_Config)

#ifndef STDIO_TX_CANID
#define STDIO_TX_CANID 0x7FF
#endif
#ifndef STDIO_RX_CANID
#define STDIO_RX_CANID 0x7FE
#endif
#ifndef STDIO_TX_CAN_HANDLE
#define STDIO_TX_CAN_HANDLE 0xFFFE
#endif

#ifndef TRACE_TX_CANID
#define TRACE_TX_CANID 0x7FD
#endif

#ifndef TRACE_TX_CAN_HANDLE
#define TRACE_TX_CAN_HANDLE STDIO_TX_CAN_HANDLE
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
extern Can_ConfigType Can_Config;
/* ================================ [ DATAS     ] ============================================== */
static uint32_t lOpenFlag = 0;
static uint32_t lWriteFlag = 0;
static PduIdType lswPduHandle[32];
static int lBusIdMap[32];
#ifdef USE_CAN_FILE_LOG
static FILE *lBusLog[32];
#endif
static std::queue<CanFrame> lPendingFrames[32];
/* ================================ [ LOCALS    ] ============================================== */
__attribute__((weak)) void CanIf_RxIndication(const Can_HwType *Mailbox,
                                              const PduInfoType *PduInfoPtr) {
}
__attribute__((weak)) void CanIf_TxConfirmation(PduIdType CanTxPduId) {
}
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

static void __attribute__((constructor)) __mcal_can_sim_init(void) {
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
void Can_Init(const Can_ConfigType *Config) {
  (void)Config;
  lOpenFlag = 0;
  lWriteFlag = 0;
}

Std_ReturnType Can_SetControllerMode(uint8_t Controller, Can_ControllerStateType Transition) {
  Std_ReturnType ret = E_NOT_OK;
  Can_ChannelConfigType *config;
  int rv;
#ifdef USE_CAN_FILE_LOG
  static char path[128];
#endif
  EnterCritical();
  if (Controller < CAN_CONFIG->numOfChannels) {
    config = &CAN_CONFIG->channelConfigs[Controller];
    switch (Transition) {
    case CAN_CS_STARTED:
      lBusIdMap[Controller] = can_open(config->device, config->port, config->baudrate);
      if (lBusIdMap[Controller] >= 0) {
        ret = E_OK;
        lOpenFlag |= (1 << Controller);
        lWriteFlag = 0;
        clear_queue(Controller);
#ifdef USE_CAN_FILE_LOG
        snprintf(path, sizeof(path), ".CAN%d-%s-%d.log", Controller, config->device, config->port);
        lBusLog[Controller] = fopen(path, "wb");
#endif
      }
      break;
    case CAN_CS_STOPPED:
    case CAN_CS_SLEEP:
      rv = can_close(lBusIdMap[Controller]);
      if (TRUE == rv) {
        ret = E_OK;
        lOpenFlag &= ~(1 << Controller);
#ifdef USE_CAN_FILE_LOG
        if (NULL != lBusLog[Controller]) {
          fclose(lBusLog[Controller]);
          lBusLog[Controller] = NULL;
        }
#endif
        clear_queue(Controller);
      }
      break;
    default:
      break;
    }
  }
  ExitCritical();

  return ret;
}

Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo) {
  Std_ReturnType ret = E_OK;
  int r;

  EnterCritical();
  if (lOpenFlag & (1 << Hth)) {
    if (0 == (lWriteFlag & (1 << Hth))) {
      InterLeaveCritical();
      r = can_write(lBusIdMap[Hth], PduInfo->id, PduInfo->length, PduInfo->sdu);
      InterEnterCritical();
      if (TRUE == r) {
        logCan(FALSE, Hth, PduInfo->id, PduInfo->length, PduInfo->sdu);
        lWriteFlag |= (1 << Hth);
        lswPduHandle[Hth] = PduInfo->swPduHandle;
      } else {
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

void Can_DeInit(void) {
}

void Can_MainFunction_WriteChannel(uint8_t Channel) {
  PduIdType swPduHandle;
  EnterCritical();
  if (Channel < CAN_MAX_HOH) {
    if (lWriteFlag & (1 << Channel)) {
      swPduHandle = lswPduHandle[Channel];
      lWriteFlag &= ~(1 << Channel);
      if (STDIO_TX_CAN_HANDLE == swPduHandle) {
      } else {
        InterLeaveCritical();
        CanIf_TxConfirmation(swPduHandle);
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
        CanIf_TxConfirmation(frame.handle);
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

extern "C" void Can_MainFunction_ReadChannelById(uint8_t Channel, uint32_t byId) {
  int r;
  uint32_t canid;
  uint8_t dlc;
  uint8_t data[64];
  Can_HwType Mailbox;
  PduInfoType PduInfo;

  EnterCritical();
  if (Channel < CAN_MAX_HOH) {
    if (lOpenFlag & (1 << Channel)) {
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

extern "C" int stdio_can_put(uint8_t *data, uint8_t dlc) {
  Std_ReturnType ret;
  Can_PduType PduInfo;
  PduInfo.id = STDIO_TX_CANID;
  PduInfo.length = dlc;
  PduInfo.sdu = data;
  PduInfo.swPduHandle = STDIO_TX_CAN_HANDLE;

  ret = Can_Write(0, &PduInfo);

  return (int)ret;
}

extern "C" int trace_can_put(uint8_t *data, uint8_t dlc) {
  Std_ReturnType ret;
  Can_PduType PduInfo;
  PduInfo.id = TRACE_TX_CANID;
  PduInfo.length = dlc;
  PduInfo.sdu = data;
  PduInfo.swPduHandle = TRACE_TX_CAN_HANDLE;

  ret = Can_Write(0, &PduInfo);

  return (int)ret;
}
