/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref:
 * https://www.autosar.org/fileadmin/user_upload/standards/classic/4-3/AUTOSAR_SWS_CANNetworkManagement.pdf
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Nm.h"
#include "CanNm.h"
#include "CanNm_Cfg.h"
#include "CanNm_Priv.h"
#include "Std_Critical.h"
#include "CanIf.h"
#include "Std_Debug.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_CANNM 1

#ifdef USE_CANNM_CRITICAL
#define nmEnterCritical() EnterCritical()
#define nmExitCritical() ExitCritical();
#else
#define nmEnterCritical()
#define nmExitCritical()
#endif

#define CANNM_CONFIG (&CanNm_Config)

#define CANNM_REQUEST_MASK 0x01
#define CANNM_PASSIVE_STARTUP_REQUEST_MASK 0x02
#define CANNM_REPEAT_MESSAGE_REQUEST_MASK 0x04
#define CANNM_DISABLE_COMMUNICATION_REQUEST_MASK 0x08
#define CANNM_RX_INDICATION_REQUEST_MASK 0x10
#define CANNM_NM_PDU_RECEIVED_MASK 0x20
#define CANNM_REMOTE_SLEEP_IND_MASK 0x40
#define CANNM_COORDINATOR_SLEEP_SYNC_MASK 0x80
#define CANNM_PN_ENABLED_MASK 0x100

#ifdef _WIN32
#define nmSetAlarm(Timer, v)                                                                       \
  Std_TimerSet(&context->Alarm._##Timer, ((v)*1000 * CANNM_MAIN_FUNCTION_PERIOD))
#define nmSingalAlarm(Timer)
#define nmIsAlarmTimeout(Timer) Std_IsTimerTimeout(&context->Alarm._##Timer)
#define nmIsAlarmStarted(Timer) Std_IsTimerStarted(&context->Alarm._##Timer)
#define nmCancelAlarm(Timer) Std_TimerStop(&context->Alarm._##Timer)
#else
/* Alarm Management */
#define nmSetAlarm(Timer, v)                                                                       \
  do {                                                                                             \
    context->Alarm._##Timer = 1 + (v);                                                             \
  } while (0)

/* signal the alarm to process one step/tick forward */
#define nmSingalAlarm(Timer)                                                                       \
  do {                                                                                             \
    if (context->Alarm._##Timer > 1) {                                                             \
      (context->Alarm._##Timer)--;                                                                 \
    }                                                                                              \
  } while (0)

#define nmIsAlarmTimeout(Timer) (1 == context->Alarm._##Timer)

#define nmIsAlarmStarted(Timer) (0 != context->Alarm._##Timer)

#define nmCancelAlarm(Timer)                                                                       \
  do {                                                                                             \
    context->Alarm._##Timer = 0;                                                                   \
  } while (0)
#endif

/* @SWS_CanNm_00045 */
#define CANNM_CBV_REPEAT_MESSAGE_REQUEST 0x01
#define CANNM_CBV_NM_COORDINATOR_SLEEP 0x08
#define CANNM_CBV_ACTIVE_WAKEUP 0x10
#define CANNM_CBV_PARTIAL_NETWORK_INFORMATION 0x40
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const CanNm_ConfigType CanNm_Config;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static void nmSendMessage(CanNm_ChannelContextType *context, const CanNm_ChannelConfigType *config,
                          uint16_t reload) {
  Std_ReturnType ret;
  PduInfoType pdu;

  pdu.SduLength = sizeof(context->data);
  pdu.SduDataPtr = context->data;
  if (0 == (context->flags & CANNM_DISABLE_COMMUNICATION_REQUEST_MASK)) {
    ret = CanIf_Transmit(config->TxPdu, &pdu);
    if (E_OK == ret) {
      nmSetAlarm(TxTimeout, config->MsgTimeoutTime);
      if (reload > 0) {
        nmSetAlarm(Tx, reload);
      }
    } else {
      /* @SWS_CanNm_00335 */
      nmSetAlarm(Tx, 0);
    }
  }
}

static void nmEnterNetworkMode(CanNm_ChannelContextType *context,
                               const CanNm_ChannelConfigType *config) {
  context->state = NM_STATE_REPEAT_MESSAGE;
  context->TxCounter = 0;
  if ((context->flags & CANNM_REQUEST_MASK) && (config->ImmediateNmTransmissions > 0)) {
    /* @SWS_CanNm_00334 */
    nmSendMessage(context, config, config->ImmediateNmCycleTime);
  } else {
    /* SWS_CanNm_00005 */
    nmSetAlarm(Tx, config->MsgCycleOffset);
  }
  nmSetAlarm(NMTimeout, config->NmTimeoutTime);
  nmSetAlarm(RepeatMessage, config->RepeatMessageTime);

  nmEnterCritical();
  context->flags &= ~(CANNM_NM_PDU_RECEIVED_MASK | CANNM_PASSIVE_STARTUP_REQUEST_MASK |
                      CANNM_REMOTE_SLEEP_IND_MASK | CANNM_RX_INDICATION_REQUEST_MASK |
                      CANNM_COORDINATOR_SLEEP_SYNC_MASK | CANNM_PN_ENABLED_MASK);
  nmExitCritical();

  Nm_NetworkMode(config->nmNetworkHandle);
  ASLOG(CANNM,
        ("%d: Enter Repeat Message Mode, flags %02X\n", config->nmNetworkHandle, context->flags));
}

static void nmRepeatMessageMain(CanNm_ChannelContextType *context,
                                const CanNm_ChannelConfigType *config) {
  uint16_t reload = config->MsgCycleTime;

  if (context->flags & CANNM_RX_INDICATION_REQUEST_MASK) {
    nmEnterCritical();
    context->flags &= ~CANNM_RX_INDICATION_REQUEST_MASK;
    nmExitCritical();
  }

  if ((context->flags & CANNM_REQUEST_MASK) && (config->ImmediateNmTransmissions > 0)) {
    /* @SWS_CanNm_00334 */
    if (context->TxCounter < (config->ImmediateNmTransmissions - 1)) {
      reload = config->ImmediateNmCycleTime;
    }
  }

  if (nmIsAlarmStarted(TxTimeout)) {
    nmSingalAlarm(TxTimeout);
    if (nmIsAlarmTimeout(TxTimeout)) {
      nmCancelAlarm(TxTimeout);
      Nm_TxTimeoutException(config->nmNetworkHandle);
    }
  }

  if (nmIsAlarmStarted(NMTimeout)) {
    nmSingalAlarm(NMTimeout);
    if (nmIsAlarmTimeout(NMTimeout)) {
      nmSetAlarm(NMTimeout, config->NmTimeoutTime);
    }
  } else {
    nmSetAlarm(NMTimeout, config->NmTimeoutTime);
  }

  if (nmIsAlarmStarted(Tx)) {
    nmSingalAlarm(Tx);
    if (nmIsAlarmTimeout(Tx)) {
      nmSendMessage(context, config, reload);
    }
  } else {
    nmSetAlarm(Tx, reload);
  }

  if (nmIsAlarmStarted(RepeatMessage)) {
    nmSingalAlarm(RepeatMessage);
    if (nmIsAlarmTimeout(RepeatMessage)) {
      if (context->flags & CANNM_REQUEST_MASK) {
        context->state = NM_STATE_NORMAL_OPERATION;
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED /* @SWS_CanNm_00149, @SWS_CanNm_00150 */
        nmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime);
#endif
        ASLOG(CANNM, ("%d: Enter Normal Operation Mode, flags %02X\n", config->nmNetworkHandle,
                      context->flags));
      } else {
        context->state = NM_STATE_READY_SLEEP;
        ASLOG(CANNM, ("%d: Enter Ready Sleep Mode, flags %02X\n", config->nmNetworkHandle,
                      context->flags));
      }
      if (config->PduCbvPosition < CANNM_PDU_OFF) {
        /* @SWS_CanNm_00107 */
        nmEnterCritical();
        context->data[config->PduCbvPosition] &= ~CANNM_CBV_REPEAT_MESSAGE_REQUEST;
        nmExitCritical();
      }
    }
  } else {
    nmSetAlarm(RepeatMessage, config->RepeatMessageTime);
  }
}

static void nmBusSleepMain(CanNm_ChannelContextType *context,
                           const CanNm_ChannelConfigType *config) {
  if (context->flags & (CANNM_REQUEST_MASK | CANNM_PASSIVE_STARTUP_REQUEST_MASK)) {
    nmEnterNetworkMode(context, config);
  } else {
    if (context->flags & CANNM_RX_INDICATION_REQUEST_MASK) {
      /* @SWS_CanNm_00127 */
      nmEnterCritical();
      context->flags &= ~CANNM_RX_INDICATION_REQUEST_MASK;
      nmExitCritical();
      Nm_NetworkStartIndication(config->nmNetworkHandle);
    }
  }
}

static void nmPrepareBusSleepMain(CanNm_ChannelContextType *context,
                                  const CanNm_ChannelConfigType *config) {
  if (context->flags & (CANNM_REQUEST_MASK | CANNM_PASSIVE_STARTUP_REQUEST_MASK |
                        CANNM_RX_INDICATION_REQUEST_MASK)) {
    if (context->flags & CANNM_RX_INDICATION_REQUEST_MASK) {
      nmEnterCritical();
      context->flags &= ~CANNM_RX_INDICATION_REQUEST_MASK;
      nmExitCritical();
    }
    nmEnterNetworkMode(context, config);
  } else {
    if (nmIsAlarmStarted(WaitBusSleep)) {
      nmSingalAlarm(WaitBusSleep);
      if (nmIsAlarmTimeout(WaitBusSleep)) {
        context->state = NM_STATE_BUS_SLEEP;
        ASLOG(CANNM,
              ("%d: Enter Bus Sleep Mode, flags %02X\n", config->nmNetworkHandle, context->flags));
        Nm_BusSleepMode(config->nmNetworkHandle);
      }
    } else {
      nmSetAlarm(WaitBusSleep, config->WaitBusSleepTime);
    }
  }
}

static void nmEnterRepeatMessageMode(CanNm_ChannelContextType *context,
                                     const CanNm_ChannelConfigType *config) {
  context->state = NM_STATE_REPEAT_MESSAGE;
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
  if (context->flags & CANNM_REMOTE_SLEEP_IND_MASK) {
    /* @SWS_CanNm_00152 */
    Nm_RemoteSleepCancellation(config->nmNetworkHandle);
  }
#endif
  nmEnterCritical();
  context->flags &= ~(CANNM_REPEAT_MESSAGE_REQUEST_MASK | CANNM_REMOTE_SLEEP_IND_MASK);
  nmExitCritical();
  ASLOG(CANNM,
        ("%d: Enter Repeat Message Mode, flags %02X\n", config->nmNetworkHandle, context->flags));
  /* @SWS_CanNm_00005 */
  nmSetAlarm(Tx, config->MsgCycleOffset);
  nmSetAlarm(RepeatMessage, config->RepeatMessageTime);
}

static void nmReadySleepMain(CanNm_ChannelContextType *context,
                             const CanNm_ChannelConfigType *config) {
  if (context->flags & CANNM_RX_INDICATION_REQUEST_MASK) {
    nmEnterCritical();
    context->flags &= ~CANNM_RX_INDICATION_REQUEST_MASK;
    nmExitCritical();
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
    if (context->flags & CANNM_REMOTE_SLEEP_IND_MASK) {
      nmEnterCritical();
      context->flags &= ~CANNM_REMOTE_SLEEP_IND_MASK;
      nmExitCritical();
      /* @SWS_CanNm_00151 */
      Nm_RemoteSleepCancellation(config->nmNetworkHandle);
    }
#endif
  }

  if (context->flags & CANNM_REPEAT_MESSAGE_REQUEST_MASK) {
    nmEnterRepeatMessageMode(context, config);
  } else if (context->flags & CANNM_REQUEST_MASK) {
    context->state = NM_STATE_NORMAL_OPERATION;
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED /* @SWS_CanNm_00149, @SWS_CanNm_00150 */
    nmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime);
#endif
    nmSendMessage(context, config, config->MsgCycleTime); /* @SWS_CanNm_00006 */
    ASLOG(CANNM, ("%d: Enter Normal Operation Mode, flags %02X\n", config->nmNetworkHandle,
                  context->flags));
  } else {
  }

  if (nmIsAlarmStarted(NMTimeout)) {
    nmSingalAlarm(NMTimeout);
    if (context->flags & CANNM_DISABLE_COMMUNICATION_REQUEST_MASK) {
      nmCancelAlarm(NMTimeout); /* @SWS_CanNm_00174 */
    }
    if (nmIsAlarmTimeout(NMTimeout)) {
      if (NM_STATE_READY_SLEEP == context->state) {
        nmCancelAlarm(NMTimeout);
        nmSetAlarm(WaitBusSleep, config->WaitBusSleepTime);
        context->state = NM_STATE_PREPARE_BUS_SLEEP;
        if ((config->ActiveWakeupBitEnabled) && (config->PduCbvPosition < CANNM_PDU_OFF)) {
          context->data[config->PduCbvPosition] &= ~CANNM_CBV_ACTIVE_WAKEUP; /* @SWS_CanNm_00402 */
        }
        Nm_PrepareBusSleepMode(config->nmNetworkHandle);
        ASLOG(CANNM, ("%d: Enter Prepare Bus Sleep Mode, flags %02X\n", config->nmNetworkHandle,
                      context->flags));
      } else {
        nmSetAlarm(NMTimeout, config->NmTimeoutTime);
      }
    }
  } else {
    if (0 == (context->flags & CANNM_DISABLE_COMMUNICATION_REQUEST_MASK)) {
      /* @SWS_CanNm_00178, @SWS_CanNm_00179 */
      ASLOG(ERROR, ("CANNM NMTimeout not started\n"));
      nmSetAlarm(NMTimeout, config->NmTimeoutTime);
    }
  }
}

static void nmNetworkNormalOperationMain(CanNm_ChannelContextType *context,
                                         const CanNm_ChannelConfigType *config) {
  if (nmIsAlarmStarted(TxTimeout)) {
    nmSingalAlarm(TxTimeout);
    if (nmIsAlarmTimeout(TxTimeout)) {
      nmCancelAlarm(TxTimeout);
      Nm_TxTimeoutException(config->nmNetworkHandle);
    }
  }

  if (nmIsAlarmStarted(NMTimeout)) {
    nmSingalAlarm(NMTimeout);
    if (nmIsAlarmTimeout(NMTimeout)) {
      nmSetAlarm(NMTimeout, config->NmTimeoutTime);
    }
  } else {
    nmSetAlarm(NMTimeout, config->NmTimeoutTime);
  }

  if (context->flags & CANNM_RX_INDICATION_REQUEST_MASK) {
    nmEnterCritical();
    context->flags &= ~CANNM_RX_INDICATION_REQUEST_MASK;
    nmExitCritical();
    if (config->MsgReducedTime > 0) {
      nmSetAlarm(Tx, config->MsgReducedTime);
    }
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
    nmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime);
    if (context->flags & CANNM_REMOTE_SLEEP_IND_MASK) {
      nmEnterCritical();
      context->flags &= ~CANNM_REMOTE_SLEEP_IND_MASK;
      nmExitCritical();
      /* @SWS_CanNm_00151 */
      Nm_RemoteSleepCancellation(config->nmNetworkHandle);
    }
#endif
  }

  if (nmIsAlarmStarted(Tx)) {
    nmSingalAlarm(Tx);
    if (nmIsAlarmTimeout(Tx)) {
      nmSendMessage(context, config, config->MsgCycleTime);
    }
  } else {
    nmSetAlarm(Tx, config->MsgCycleTime);
  }

#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED /* @SWS_CanNm_00149, @SWS_CanNm_00150 */
  if (nmIsAlarmStarted(RemoteSleepInd)) {
    nmSingalAlarm(RemoteSleepInd);
    if (nmIsAlarmTimeout(RemoteSleepInd)) {
      nmCancelAlarm(RemoteSleepInd);
      if (0 == (context->flags & CANNM_DISABLE_COMMUNICATION_REQUEST_MASK)) {
        /* @SWS_CanNm_00175 */
        Nm_RemoteSleepIndication(config->nmNetworkHandle);
        nmEnterCritical();
        context->flags |= CANNM_REMOTE_SLEEP_IND_MASK;
        nmExitCritical();
      }
    }
  }
#endif

  if (context->flags & CANNM_REPEAT_MESSAGE_REQUEST_MASK) {
    nmEnterRepeatMessageMode(context, config);
  } else if (0 == (context->flags & CANNM_REQUEST_MASK)) {
    context->state = NM_STATE_READY_SLEEP;
    ASLOG(CANNM,
          ("%d: Enter Ready Sleep Mode, flags %02X\n", config->nmNetworkHandle, context->flags));
  } else {
  }
}

#ifdef CANNM_GLOBAL_PN_SUPPORT
static Std_ReturnType nmRxFilter(CanNm_ChannelContextType *context,
                                 const CanNm_ChannelConfigType *config,
                                 const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_OK;
  int i;
  if (context->flags & CANNM_PN_ENABLED_MASK) {
    if (0 ==
        (PduInfoPtr->SduDataPtr[config->PduCbvPosition] & CANNM_CBV_PARTIAL_NETWORK_INFORMATION)) {
      if (config->AllNmMessagesKeepAwake) { /* @SWS_CanNm_00410 */
        ret = E_OK;
      } else {
        /* @SWS_CanNm_00411 */
        ret = E_NOT_OK;
      }
    } else {
      /* @SWS_CanNm_00412 */
      ret = E_NOT_OK;
      for (i = 0; i < config->PnInfoLength; i++) {
        /* @SWS_CanNm_00417, @SWS_CanNm_00419 */
        if (PduInfoPtr->SduDataPtr[config->PnInfoOffset + i] & config->PnFilterMaskByte[i]) {
          ret = E_OK;
          break;
        }
      }
    }
  } else {
    /* @SWS_CanNm_00409 */
  }

  return ret;
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
void CanNm_Init(const CanNm_ConfigType *cannmConfigPtr) {
  int i;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;
  (void)cannmConfigPtr;

  for (i = 0; i < CANNM_CONFIG->numOfChannels; i++) {
    context = &CANNM_CONFIG->ChannelContexts[i];
    config = &CANNM_CONFIG->ChannelConfigs[i];
    context->state = NM_STATE_BUS_SLEEP; /* @SWS_CanNm_00141 */
    context->flags = 0;                  /* @SWS_CanNm_00403 */
    memset(context->rxPdu, 0xFF, sizeof(context->rxPdu));
    memset(context->data, 0xFF, sizeof(context->data)); /* @SWS_CanNm_00025 */
    if (config->PduNidPosition < CANNM_PDU_OFF) {
      context->data[config->PduNidPosition] = config->NodeId; /* @SWS_CanNm_00013 */
    }
    if (config->PduCbvPosition < CANNM_PDU_OFF) {
      context->data[config->PduCbvPosition] = 0x00; /* @SWS_CanNm_00085 */
      if (config->PnEnabled) {                      /* @SWS_CanNm_00413 */
        context->data[config->PduCbvPosition] |= CANNM_CBV_PARTIAL_NETWORK_INFORMATION;
        memcpy(&context->data[config->PnInfoOffset], config->PnFilterMaskByte,
               config->PnInfoLength);
      }
    }
  }
}

Std_ReturnType CanNm_PassiveStartUp(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;

  if (nmChannelHandle < CANNM_CONFIG->numOfChannels) {
    context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
    switch ((context->state)) {
    case NM_STATE_BUS_SLEEP:
    case NM_STATE_PREPARE_BUS_SLEEP:
      nmEnterCritical();
      context->flags |= CANNM_PASSIVE_STARTUP_REQUEST_MASK;
      nmExitCritical();
      break;
    default:
      /* @SWS_CanNm_00147 */
      ret = E_NOT_OK;
      break;
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType CanNm_NetworkRequest(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;

  if (nmChannelHandle < CANNM_CONFIG->numOfChannels) {
    context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
    nmEnterCritical();
    context->flags |= CANNM_REQUEST_MASK;
    if ((config->ActiveWakeupBitEnabled) && (config->PduCbvPosition < CANNM_PDU_OFF)) {
      context->data[config->PduCbvPosition] |= CANNM_CBV_ACTIVE_WAKEUP; /* @SWS_CanNm_00401 */
    }
    nmExitCritical();
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType CanNm_NetworkRelease(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;

  if (nmChannelHandle < CANNM_CONFIG->numOfChannels) {
    context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
    nmEnterCritical();
    context->flags &= ~CANNM_REQUEST_MASK;
    nmExitCritical();
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType CanNm_RepeatMessageRequest(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;

  if (nmChannelHandle < CANNM_CONFIG->numOfChannels) {
    context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
    switch (context->state) {
    case NM_STATE_READY_SLEEP:
    case NM_STATE_NORMAL_OPERATION:
      nmEnterCritical();
      context->flags |= CANNM_REPEAT_MESSAGE_REQUEST_MASK;
      /* @SWS_CanNm_00121, @SWS_CanNm_00113 */
      if (config->NodeDetectionEnabled) {
        if (config->PduCbvPosition < CANNM_PDU_OFF) {
          context->data[config->PduCbvPosition] |= CANNM_CBV_REPEAT_MESSAGE_REQUEST;
        }
      }
      nmEnterCritical();
      break;
    default:
      /* @SWS_CanNm_00137 */
      ret = E_NOT_OK;
      break;
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType CanNm_DisableCommunication(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;

  if (nmChannelHandle < CANNM_CONFIG->numOfChannels) {
    context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
    /* @SWS_CanNm_00170 */
    nmEnterCritical();
    context->flags |= CANNM_DISABLE_COMMUNICATION_REQUEST_MASK;
    nmEnterCritical();
    /* could see codes that check CANNM_DISABLE_COMMUNICATION_REQUEST_MASK in the MainFunction,
     * That is because that Alarm operation is not atomic, so the codes that added to check
     * CANNM_DISABLE_COMMUNICATION_REQUEST_MASK is a workaroud and which is not good but it
     * satisfied the requirement */
    nmCancelAlarm(Tx);        /* @SWS_CanNm_00173 */
    nmCancelAlarm(NMTimeout); /* @SWS_CanNm_00174 */
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
    nmCancelAlarm(RemoteSleepInd); /* @SWS_CanNm_00175 */
#endif
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType CanNm_EnableCommunication(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;

  if (nmChannelHandle < CANNM_CONFIG->numOfChannels) {
    context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
    nmEnterCritical();
    context->flags &= ~CANNM_DISABLE_COMMUNICATION_REQUEST_MASK;
    nmEnterCritical();
    nmSetAlarm(Tx, 0);                            /* @SWS_CanNm_00178 */
    nmSetAlarm(NMTimeout, config->NmTimeoutTime); /* @SWS_CanNm_00179 */
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
    nmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime); /* @SWS_CanNm_00180 */
#endif
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

void CanNm_TxConfirmation(PduIdType TxPduId, Std_ReturnType result) {
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;
  if (TxPduId < CANNM_CONFIG->numOfChannels) {
    context = &CANNM_CONFIG->ChannelContexts[TxPduId];
    config = &CANNM_CONFIG->ChannelConfigs[TxPduId];
    if (E_OK == result) {
      if (context->TxCounter < 0xFF) {
        context->TxCounter++;
      }
      nmCancelAlarm(TxTimeout);

      if ((NM_STATE_NORMAL_OPERATION == context->state) ||
          (NM_STATE_REPEAT_MESSAGE == context->state) || (NM_STATE_READY_SLEEP == context->state)) {
        nmSetAlarm(NMTimeout, config->NmTimeoutTime);
      }
    } else {
      /* @SWS_CanNm_00066 */
      Nm_TxTimeoutException(config->nmNetworkHandle);
    }
  } else {
    ASLOG(ERROR, ("CanNm_TxConfirmation with invalid TxPduId %d\n", TxPduId));
  }
}

void CanNm_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;
  uint8_t flags = CANNM_RX_INDICATION_REQUEST_MASK | CANNM_NM_PDU_RECEIVED_MASK;
  Std_ReturnType ret = E_NOT_OK;
  if (RxPduId < CANNM_CONFIG->numOfChannels) {
    context = &CANNM_CONFIG->ChannelContexts[RxPduId];
    config = &CANNM_CONFIG->ChannelConfigs[RxPduId];
#ifdef CANNM_GLOBAL_PN_SUPPORT
    ret = nmRxFilter(context, config, PduInfoPtr);
#else
    ret = E_OK;
#endif
  } else {
    ASLOG(ERROR, ("CanNm_RxIndication with invalid RxPduId %d\n", RxPduId));
  }
  if (E_OK == ret) {
    /* @SWS_CanNm_00035 */
    memcpy(context->rxPdu, PduInfoPtr->SduDataPtr, sizeof(context->rxPdu));
    if (config->PduCbvPosition < CANNM_PDU_OFF) {
      if (context->rxPdu[config->PduCbvPosition] & CANNM_CBV_REPEAT_MESSAGE_REQUEST) {
        if (config->NodeDetectionEnabled) {
          /* @SWS_CanNm_00119, @SWS_CanNm_00111 */
          if ((NM_STATE_NORMAL_OPERATION == context->state) ||
              (NM_STATE_READY_SLEEP == context->state)) {
            flags |= CANNM_REPEAT_MESSAGE_REQUEST_MASK;
          }
        }
        /* @SWS_CanNm_00014 */
        if (config->RepeatMsgIndEnabled && config->NodeDetectionEnabled) {
          Nm_RepeatMessageIndication(config->nmNetworkHandle);
        }
      }
#ifdef CANNM_COORDINATOR_SYNC_SUPPORT
      if ((NM_STATE_NORMAL_OPERATION == context->state) ||
          (NM_STATE_REPEAT_MESSAGE == context->state) || (NM_STATE_READY_SLEEP == context->state)) {
        if (context->rxPdu[config->PduCbvPosition] & CANNM_CBV_NM_COORDINATOR_SLEEP) {
          /* @SWS_CanNm_00341 */
          if (0 == (context->flags & CANNM_COORDINATOR_SLEEP_SYNC_MASK)) {
            flags |= CANNM_COORDINATOR_SLEEP_SYNC_MASK;
            Nm_CoordReadyToSleepIndication(config->nmNetworkHandle);
          }
        } else if (context->flags & CANNM_COORDINATOR_SLEEP_SYNC_MASK) {
          /* @SWS_CanNm_00348 */
          Nm_CoordReadyToSleepCancellation(config->nmNetworkHandle);
          nmEnterCritical();
          context->flags &= ~CANNM_COORDINATOR_SLEEP_SYNC_MASK;
          nmExitCritical();
        }
      }
#endif
    }
    if ((NM_STATE_NORMAL_OPERATION == context->state) ||
        (NM_STATE_REPEAT_MESSAGE == context->state) || (NM_STATE_READY_SLEEP == context->state)) {
      nmSetAlarm(NMTimeout, config->NmTimeoutTime);
    }
    nmEnterCritical();
    context->flags |= flags;
    nmExitCritical();
  }
}

Std_ReturnType CanNm_GetLocalNodeIdentifier(NetworkHandleType nmChannelHandle,
                                            uint8_t *nmNodeIdPtr) {
  Std_ReturnType ret = E_OK;
  const CanNm_ChannelConfigType *config;

  if ((nmChannelHandle < CANNM_CONFIG->numOfChannels) && (NULL != nmNodeIdPtr)) {
    config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
    *nmNodeIdPtr = config->NodeId;
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType CanNm_GetNodeIdentifier(NetworkHandleType nmChannelHandle, uint8_t *nmNodeIdPtr) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;
  uint8_t nodeId = CANNM_INVALID_NODE_ID;

  if ((nmChannelHandle < CANNM_CONFIG->numOfChannels) && (NULL != nmNodeIdPtr)) {
    context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
    /* @SWS_CanNm_00132 */
    if (config->PduNidPosition < CANNM_PDU_OFF) {
      nodeId = context->rxPdu[config->PduNidPosition];
    }
    if (CANNM_INVALID_NODE_ID == nodeId) {
      ret = E_NOT_OK;
    } else {
      *nmNodeIdPtr = nodeId;
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType CanNm_SetUserData(NetworkHandleType nmChannelHandle, const uint8_t *nmUserDataPtr) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;
  uint8_t notUserDataMask = 0;
  uint8_t userDataLength = 8;
  int i, j = 0;

  if ((nmChannelHandle < CANNM_CONFIG->numOfChannels) && (NULL != nmUserDataPtr)) {
    context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
    if (config->PduNidPosition != CANNM_PDU_OFF) {
      notUserDataMask |= (1 << config->PduNidPosition);
      userDataLength--;
    }
    if (config->PduCbvPosition != CANNM_PDU_OFF) {
      notUserDataMask |= (1 << config->PduCbvPosition);
      userDataLength--;
    }
#ifdef CANNM_GLOBAL_PN_SUPPORT
    if (config->PnEnabled) {
      userDataLength -= config->PnInfoLength;
      for (i = 0; i < config->PnInfoLength; i++) {
        notUserDataMask |= (1 << (config->PnInfoOffset + i));
      }
    }
#endif
    for (i = 0; i < 8; i++) {
      if (0 == (notUserDataMask & (1 << i))) {
        context->data[i] = nmUserDataPtr[j];
        j++;
      }
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType CanNm_GetUserData(NetworkHandleType nmChannelHandle, uint8_t *nmUserDataPtr) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;
  uint8_t notUserDataMask = 0;
  uint8_t userDataLength = 8;
  int i, j = 0;

  if ((nmChannelHandle < CANNM_CONFIG->numOfChannels) && (NULL != nmUserDataPtr)) {
    context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
    if (config->PduNidPosition != CANNM_PDU_OFF) {
      notUserDataMask |= (1 << config->PduNidPosition);
      userDataLength--;
    }
    if (config->PduCbvPosition != CANNM_PDU_OFF) {
      notUserDataMask |= (1 << config->PduCbvPosition);
      userDataLength--;
    }
#ifdef CANNM_GLOBAL_PN_SUPPORT
    if (config->PnEnabled) {
      userDataLength -= config->PnInfoLength;
      for (i = 0; i < config->PnInfoLength; i++) {
        notUserDataMask |= (1 << (config->PnInfoOffset + i));
      }
    }
#endif
    for (i = 0; i < 8; i++) {
      if (0 == (notUserDataMask & (1 << i))) {
        nmUserDataPtr[j] = context->data[i];
        j++;
      }
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType CanNm_GetPduData(NetworkHandleType nmChannelHandle, uint8_t *nmPduDataPtr) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;

  if ((nmChannelHandle < CANNM_CONFIG->numOfChannels) && (NULL != nmPduDataPtr)) {
    context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
    if (context->flags & CANNM_NM_PDU_RECEIVED_MASK) {
      memcpy(nmPduDataPtr, &context->rxPdu, 8);
    } else {
      ret = E_NOT_OK;
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType CanNm_GetState(NetworkHandleType nmChannelHandle, Nm_StateType *nmStatePtr,
                              Nm_ModeType *nmModePtr) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;

  if ((nmChannelHandle < CANNM_CONFIG->numOfChannels) && (NULL != nmStatePtr) &&
      (NULL != nmModePtr)) {
    context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
    *nmStatePtr = context->state;
    switch (context->state) {
    case NM_STATE_BUS_SLEEP:
      *nmModePtr = NM_MODE_BUS_SLEEP;
      break;
    case NM_STATE_PREPARE_BUS_SLEEP:
      *nmModePtr = NM_MODE_PREPARE_BUS_SLEEP;
      break;
    default:
      *nmModePtr = NM_MODE_NETWORK;
      break;
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

#ifdef CANNM_GLOBAL_PN_SUPPORT
void CanNm_ConfirmPnAvailability(NetworkHandleType nmChannelHandle) {
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;
  if (nmChannelHandle < CANNM_CONFIG->numOfChannels) {
    context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
    if (config->PnEnabled) {
      ASLOG(CANNM, ("%d: confirm PN, flags %X\n", nmChannelHandle, context->flags));
      /* @SWS_CanNm_00404 */
      nmEnterCritical();
      context->flags |= CANNM_PN_ENABLED_MASK;
      nmExitCritical();
    }
  }
}
#endif

#ifdef CANNM_COORDINATOR_SYNC_SUPPORT
Std_ReturnType CanNm_SetSleepReadyBit(NetworkHandleType nmChannelHandle, boolean nmSleepReadyBit) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;

  if (nmChannelHandle < CANNM_CONFIG->numOfChannels) {
    context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
    nmEnterCritical();
    if (config->PduCbvPosition < CANNM_PDU_OFF) {
      /* @SWS_CanNm_00342 */
      if (nmSleepReadyBit) {
        context->data[config->PduCbvPosition] |= CANNM_CBV_NM_COORDINATOR_SLEEP;
      } else {
        context->data[config->PduCbvPosition] &= ~CANNM_CBV_NM_COORDINATOR_SLEEP;
      }
    } else {
      ret = E_NOT_OK;
    }
    nmEnterCritical();
    if (E_OK == ret) {
      nmSendMessage(context, config, 0);
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}
#endif

#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
Std_ReturnType CanNm_CheckRemoteSleepIndication(NetworkHandleType nmChannelHandle,
                                                boolean *nmRemoteSleepIndPtr) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;

  if ((nmChannelHandle < CANNM_CONFIG->numOfChannels) && (nmRemoteSleepIndPtr)) {
    context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
    switch (context->state) {
    case NM_STATE_NORMAL_OPERATION:
      if (context->flags & CANNM_REMOTE_SLEEP_IND_MASK) {
        *nmRemoteSleepIndPtr = TRUE;
      } else {
        *nmRemoteSleepIndPtr = FALSE;
      }
      break;
    default:
      /* @SWS_CanNm_00154 */
      ret = E_NOT_OK;
      break;
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}
#endif

void CanNm_MainFunction(void) {
  int i;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;

  for (i = 0; i < CANNM_CONFIG->numOfChannels; i++) {
    context = &CANNM_CONFIG->ChannelContexts[i];
    config = &CANNM_CONFIG->ChannelConfigs[i];
    switch (context->state) {
    case NM_STATE_BUS_SLEEP:
      nmBusSleepMain(context, config);
      break;
    case NM_STATE_PREPARE_BUS_SLEEP:
      nmPrepareBusSleepMain(context, config);
      break;
    case NM_STATE_READY_SLEEP:
      nmReadySleepMain(context, config);
      break;
    case NM_STATE_REPEAT_MESSAGE:
      nmRepeatMessageMain(context, config);
      break;
    case NM_STATE_NORMAL_OPERATION:
      nmNetworkNormalOperationMain(context, config);
      break;
    default:
      break;
    }
  }
}
