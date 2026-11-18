/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of UDP Network Management AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Nm.h"
#include "UdpNm.h"
#include "UdpNm_Cfg.h"
#include "UdpNm_Priv.h"
#include "Std_Critical.h"
#include "SoAd.h"
#include "Std_Debug.h"
#include <string.h>

#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_UDPNM 1

#ifdef USE_UDPNM_CRITICAL
#define UdpNmEnterCritical() EnterCritical()
#define UdpNmExitCritical() ExitCritical();
#else
#define UdpNmEnterCritical()
#define UdpNmExitCritical()
#endif

#define UDPNM_CONFIG (&UdpNm_Config)

#define UDPNM_REQUEST_MASK 0x01
#define UDPNM_PASSIVE_STARTUP_REQUEST_MASK 0x02
#define UDPNM_REPEAT_MESSAGE_REQUEST_MASK 0x04
#define UDPNM_DISABLE_COMMUNICATION_REQUEST_MASK 0x08
#define UDPNM_RX_INDICATION_REQUEST_MASK 0x10
#define UDPNM_NM_PDU_RECEIVED_MASK 0x20
#define UDPNM_REMOTE_SLEEP_IND_MASK 0x40
#define UDPNM_COORDINATOR_SLEEP_SYNC_MASK 0x80
#define UDPNM_PN_ENABLED_MASK 0x100

#ifdef _WIN32
#define UdpNmSetAlarm(Timer, v)                                                                    \
  Std_TimerSet(&context->Alarm.Timer, ((v) * 1000 * UDPNM_MAIN_FUNCTION_PERIOD))
#define UdpNmSignalAlarm(Timer)
#define UdpNmIsAlarmTimeout(Timer) Std_IsTimerTimeout(&context->Alarm.Timer)
#define UdpNmIsAlarmStarted(Timer) Std_IsTimerStarted(&context->Alarm.Timer)
#define UdpNmCancelAlarm(Timer) Std_TimerStop(&context->Alarm.Timer)
#else
/* Alarm Management */
#define UdpNmSetAlarm(Timer, v)                                                                    \
  do {                                                                                             \
    context->Alarm.Timer = 1 + (v);                                                                \
  } while (0)

/* signal the alarm to process one step/tick forward */
#define UdpNmSignalAlarm(Timer)                                                                    \
  do {                                                                                             \
    if (context->Alarm.Timer > 1) {                                                                \
      (context->Alarm.Timer)--;                                                                    \
    }                                                                                              \
  } while (0)

#define UdpNmIsAlarmTimeout(Timer) (1 == context->Alarm.Timer)

#define UdpNmIsAlarmStarted(Timer) (0 != context->Alarm.Timer)

#define UdpNmCancelAlarm(Timer)                                                                    \
  do {                                                                                             \
    context->Alarm.Timer = 0;                                                                      \
  } while (0)
#endif

/* @SWS_UdpNm_00045 */
#define UDPNM_CBV_REPEAT_MESSAGE_REQUEST 0x01
#define UDPNM_CBV_NM_COORDINATOR_SLEEP 0x08
#define UDPNM_CBV_ACTIVE_WAKEUP 0x10
#define UDPNM_CBV_PARTIAL_NETWORK_INFORMATION 0x40
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const UdpNm_ConfigType UdpNm_Config;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static void UdpNm_SendMessage(UdpNm_ChannelContextType *context,
                              const UdpNm_ChannelConfigType *config, uint16_t reload) {
  Std_ReturnType ret;
  PduInfoType pdu;

  pdu.SduLength = sizeof(context->Data);
  pdu.SduDataPtr = context->Data;
  pdu.MetaDataPtr = NULL;
  if (0 == (context->Flags & UDPNM_DISABLE_COMMUNICATION_REQUEST_MASK)) {
    ret = SoAd_IfTransmit(config->TxPdu, &pdu);
    if (E_OK == ret) {
      UdpNmSetAlarm(TxTimeout, config->MsgTimeoutTime);
      if (reload > 0) {
        UdpNmSetAlarm(Tx, reload);
      }
    } else {
      /* @SWS_UdpNm_00330 */
      UdpNmSetAlarm(Tx, 0);
    }
  }
}

static void UdpNm_EnterNetworkMode(UdpNm_ChannelContextType *context,
                                   const UdpNm_ChannelConfigType *config) {
  context->State = NM_STATE_REPEAT_MESSAGE;
  context->TxCounter = 0;
  if ((context->Flags & UDPNM_REQUEST_MASK) && (config->ImmediateNmTransmissions > 0)) {
    /* @SWS_UdpNm_00334 */
    UdpNm_SendMessage(context, config, config->ImmediateNmCycleTime);
  } else {
    /* SWS_UdpNm_00005 */
    UdpNmSetAlarm(Tx, config->MsgCycleOffset);
  }
  UdpNmSetAlarm(NmTimeout, config->NmTimeoutTime);
  UdpNmSetAlarm(RepeatMessage, config->RepeatMessageTime);

  UdpNmEnterCritical();
  context->Flags &= ~(UDPNM_NM_PDU_RECEIVED_MASK | UDPNM_PASSIVE_STARTUP_REQUEST_MASK |
                      UDPNM_REMOTE_SLEEP_IND_MASK | UDPNM_RX_INDICATION_REQUEST_MASK |
                      UDPNM_COORDINATOR_SLEEP_SYNC_MASK | UDPNM_PN_ENABLED_MASK);
  UdpNmExitCritical();

  Nm_NetworkMode(config->NmNetworkHandle);
  ASLOG(UDPNM,
        ("%d: Enter Repeat Message Mode, flags %02X\n", config->NmNetworkHandle, context->Flags));
}

static void UdpNm_RepeatMessageMain(UdpNm_ChannelContextType *context,
                                    const UdpNm_ChannelConfigType *config) {
  uint16_t reload = config->MsgCycleTime;

  if (context->Flags & UDPNM_RX_INDICATION_REQUEST_MASK) {
    UdpNmEnterCritical();
    context->Flags &= ~UDPNM_RX_INDICATION_REQUEST_MASK;
    UdpNmExitCritical();
  }

  if ((context->Flags & UDPNM_REQUEST_MASK) && (config->ImmediateNmTransmissions > 0)) {
    /* @SWS_UdpNm_00334 */
    if (context->TxCounter < (config->ImmediateNmTransmissions - 1)) {
      reload = config->ImmediateNmCycleTime;
    }
  }

  if (UdpNmIsAlarmStarted(TxTimeout)) {
    UdpNmSignalAlarm(TxTimeout);
    if (UdpNmIsAlarmTimeout(TxTimeout)) {
      UdpNmCancelAlarm(TxTimeout);
      Nm_TxTimeoutException(config->NmNetworkHandle);
    }
  }

  if (UdpNmIsAlarmStarted(NmTimeout)) {
    UdpNmSignalAlarm(NmTimeout);
    if (UdpNmIsAlarmTimeout(NmTimeout)) {
      UdpNmSetAlarm(NmTimeout, config->NmTimeoutTime);
    }
  } else {
    UdpNmSetAlarm(NmTimeout, config->NmTimeoutTime);
  }

  if (UdpNmIsAlarmStarted(Tx)) {
    UdpNmSignalAlarm(Tx);
    if (UdpNmIsAlarmTimeout(Tx)) {
      UdpNm_SendMessage(context, config, reload);
    }
  } else {
    UdpNmSetAlarm(Tx, reload);
  }

  if (UdpNmIsAlarmStarted(RepeatMessage)) {
    UdpNmSignalAlarm(RepeatMessage);
    if (UdpNmIsAlarmTimeout(RepeatMessage)) {
      if (context->Flags & UDPNM_REQUEST_MASK) {
        context->State = NM_STATE_NORMAL_OPERATION;
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED /* @SWS_UdpNm_00149, @SWS_UdpNm_00150 */
        UdpNmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime);
#endif
        ASLOG(UDPNM, ("%d: Enter Normal Operation Mode, flags %02X\n", config->NmNetworkHandle,
                      context->Flags));
      } else {
        context->State = NM_STATE_READY_SLEEP;
        ASLOG(UDPNM, ("%d: Enter Ready Sleep Mode, flags %02X\n", config->NmNetworkHandle,
                      context->Flags));
      }
      if (config->PduCbvPosition < UDPNM_PDU_OFF) {
        /* @SWS_UdpNm_00107 */
        UdpNmEnterCritical();
        context->Data[config->PduCbvPosition] &= ~UDPNM_CBV_REPEAT_MESSAGE_REQUEST;
        UdpNmExitCritical();
      }
    }
  } else {
    UdpNmSetAlarm(RepeatMessage, config->RepeatMessageTime);
  }
}

static void UdpNm_BusSleepMain(UdpNm_ChannelContextType *context,
                               const UdpNm_ChannelConfigType *config) {
  if (context->Flags & (UDPNM_REQUEST_MASK | UDPNM_PASSIVE_STARTUP_REQUEST_MASK)) {
    UdpNm_EnterNetworkMode(context, config);
  } else {
    if (context->Flags & UDPNM_RX_INDICATION_REQUEST_MASK) {
      /* @SWS_UdpNm_00127 */
      UdpNmEnterCritical();
      context->Flags &= ~UDPNM_RX_INDICATION_REQUEST_MASK;
      UdpNmExitCritical();
      Nm_NetworkStartIndication(config->NmNetworkHandle);
    }
  }
}

static void UdpNm_PrepareBusSleepMain(UdpNm_ChannelContextType *context,
                                      const UdpNm_ChannelConfigType *config) {
  if (context->Flags & (UDPNM_REQUEST_MASK | UDPNM_PASSIVE_STARTUP_REQUEST_MASK |
                        UDPNM_RX_INDICATION_REQUEST_MASK)) {
    if (context->Flags & UDPNM_RX_INDICATION_REQUEST_MASK) {
      UdpNmEnterCritical();
      context->Flags &= ~UDPNM_RX_INDICATION_REQUEST_MASK;
      UdpNmExitCritical();
    }
    UdpNm_EnterNetworkMode(context, config);
  } else {
    if (UdpNmIsAlarmStarted(WaitBusSleep)) {
      UdpNmSignalAlarm(WaitBusSleep);
      if (UdpNmIsAlarmTimeout(WaitBusSleep)) {
        context->State = NM_STATE_BUS_SLEEP;
        ASLOG(UDPNM,
              ("%d: Enter Bus Sleep Mode, flags %02X\n", config->NmNetworkHandle, context->Flags));
        Nm_BusSleepMode(config->NmNetworkHandle);
      }
    } else {
      UdpNmSetAlarm(WaitBusSleep, config->WaitBusSleepTime);
    }
  }
}

static void UdpNm_EnterRepeatMessageMode(UdpNm_ChannelContextType *context,
                                         const UdpNm_ChannelConfigType *config) {
  context->State = NM_STATE_REPEAT_MESSAGE;
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED
  if (context->Flags & UDPNM_REMOTE_SLEEP_IND_MASK) {
    /* @SWS_UdpNm_00152 */
    Nm_RemoteSleepCancellation(config->NmNetworkHandle);
  }
#endif
  UdpNmEnterCritical();
  context->Flags &= ~(UDPNM_REPEAT_MESSAGE_REQUEST_MASK | UDPNM_REMOTE_SLEEP_IND_MASK);
  UdpNmExitCritical();
  ASLOG(UDPNM,
        ("%d: Enter Repeat Message Mode, flags %02X\n", config->NmNetworkHandle, context->Flags));
  /* @SWS_UdpNm_00005 */
  UdpNmSetAlarm(Tx, config->MsgCycleOffset);
  UdpNmSetAlarm(RepeatMessage, config->RepeatMessageTime);
}

static void UdpNm_ReadySleepMain(UdpNm_ChannelContextType *context,
                                 const UdpNm_ChannelConfigType *config) {
  if (context->Flags & UDPNM_RX_INDICATION_REQUEST_MASK) {
    UdpNmEnterCritical();
    context->Flags &= ~UDPNM_RX_INDICATION_REQUEST_MASK;
    UdpNmExitCritical();
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED
    if (context->Flags & UDPNM_REMOTE_SLEEP_IND_MASK) {
      UdpNmEnterCritical();
      context->Flags &= ~UDPNM_REMOTE_SLEEP_IND_MASK;
      UdpNmExitCritical();
      /* @SWS_UdpNm_00151 */
      Nm_RemoteSleepCancellation(config->NmNetworkHandle);
    }
#endif
  }

  if (context->Flags & UDPNM_REPEAT_MESSAGE_REQUEST_MASK) {
    UdpNm_EnterRepeatMessageMode(context, config);
  } else if (context->Flags & UDPNM_REQUEST_MASK) {
    context->State = NM_STATE_NORMAL_OPERATION;
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED /* @SWS_UdpNm_00149, @SWS_UdpNm_00150 */
    UdpNmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime);
#endif
    UdpNm_SendMessage(context, config, config->MsgCycleTime); /* @SWS_UdpNm_00006 */
    ASLOG(UDPNM, ("%d: Enter Normal Operation Mode, flags %02X\n", config->NmNetworkHandle,
                  context->Flags));
  } else {
  }

  if (UdpNmIsAlarmStarted(NmTimeout)) {
    UdpNmSignalAlarm(NmTimeout);
    if (context->Flags & UDPNM_DISABLE_COMMUNICATION_REQUEST_MASK) {
      UdpNmCancelAlarm(NmTimeout); /* @SWS_UdpNm_00174 */
    }
    if (UdpNmIsAlarmTimeout(NmTimeout)) {
      if (NM_STATE_READY_SLEEP == context->State) {
        UdpNmCancelAlarm(NmTimeout);
        UdpNmSetAlarm(WaitBusSleep, config->WaitBusSleepTime);
        context->State = NM_STATE_PREPARE_BUS_SLEEP;
        if ((config->ActiveWakeupBitEnabled) && (config->PduCbvPosition < UDPNM_PDU_OFF)) {
          context->Data[config->PduCbvPosition] &= ~UDPNM_CBV_ACTIVE_WAKEUP; /* @SWS_UdpNm_00367 */
        }
        Nm_PrepareBusSleepMode(config->NmNetworkHandle);
        ASLOG(UDPNM, ("%d: Enter Prepare Bus Sleep Mode, flags %02X\n", config->NmNetworkHandle,
                      context->Flags));
      } else {
        UdpNmSetAlarm(NmTimeout, config->NmTimeoutTime);
      }
    }
  } else {
    if (0 == (context->Flags & UDPNM_DISABLE_COMMUNICATION_REQUEST_MASK)) {
      /* @SWS_CanNm_00178, @SWS_CanNm_00179 */
      ASLOG(ERROR, ("UDPNM NMTimeout not started\n"));
      UdpNmSetAlarm(NmTimeout, config->NmTimeoutTime);
    }
  }
}

static void UdpNm_NetworkNormalOperationMain(UdpNm_ChannelContextType *context,
                                             const UdpNm_ChannelConfigType *config) {
  if (UdpNmIsAlarmStarted(TxTimeout)) {
    UdpNmSignalAlarm(TxTimeout);
    if (UdpNmIsAlarmTimeout(TxTimeout)) {
      UdpNmCancelAlarm(TxTimeout);
      Nm_TxTimeoutException(config->NmNetworkHandle);
    }
  }

  if (UdpNmIsAlarmStarted(NmTimeout)) {
    UdpNmSignalAlarm(NmTimeout);
    if (UdpNmIsAlarmTimeout(NmTimeout)) {
      UdpNmSetAlarm(NmTimeout, config->NmTimeoutTime);
    }
  } else {
    UdpNmSetAlarm(NmTimeout, config->NmTimeoutTime);
  }

  if (context->Flags & UDPNM_RX_INDICATION_REQUEST_MASK) {
    UdpNmEnterCritical();
    context->Flags &= ~UDPNM_RX_INDICATION_REQUEST_MASK;
    UdpNmExitCritical();
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED
    UdpNmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime);
    if (context->Flags & UDPNM_REMOTE_SLEEP_IND_MASK) {
      UdpNmEnterCritical();
      context->Flags &= ~UDPNM_REMOTE_SLEEP_IND_MASK;
      UdpNmExitCritical();
      /* @SWS_UdpNm_00151 */
      Nm_RemoteSleepCancellation(config->NmNetworkHandle);
    }
#endif
  }

  if (UdpNmIsAlarmStarted(Tx)) {
    UdpNmSignalAlarm(Tx);
    if (UdpNmIsAlarmTimeout(Tx)) {
      UdpNm_SendMessage(context, config, config->MsgCycleTime);
    }
  } else {
    UdpNmSetAlarm(Tx, config->MsgCycleTime);
  }

#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED /* @SWS_UdpNm_00149, @SWS_UdpNm_00150 */
  if (UdpNmIsAlarmStarted(RemoteSleepInd)) {
    UdpNmSignalAlarm(RemoteSleepInd);
    if (UdpNmIsAlarmTimeout(RemoteSleepInd)) {
      UdpNmCancelAlarm(RemoteSleepInd);
      if (0 == (context->Flags & UDPNM_DISABLE_COMMUNICATION_REQUEST_MASK)) {
        /* @00175 */
        Nm_RemoteSleepIndication(config->NmNetworkHandle);
        UdpNmEnterCritical();
        context->Flags |= UDPNM_REMOTE_SLEEP_IND_MASK;
        UdpNmExitCritical();
      }
    }
  }
#endif

  if (context->Flags & UDPNM_REPEAT_MESSAGE_REQUEST_MASK) {
    UdpNm_EnterRepeatMessageMode(context, config);
  } else if (0 == (context->Flags & UDPNM_REQUEST_MASK)) {
    context->State = NM_STATE_READY_SLEEP;
    ASLOG(UDPNM,
          ("%d: Enter Ready Sleep Mode, flags %02X\n", config->NmNetworkHandle, context->Flags));
  } else {
  }
}

#ifdef UDPNM_GLOBAL_PN_SUPPORT
static Std_ReturnType UdpNm_RxFilter(UdpNm_ChannelContextType *context,
                                     const UdpNm_ChannelConfigType *config,
                                     const PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_OK;
  int i;
  if (context->Flags & UDPNM_PN_ENABLED_MASK) {
    if (0 ==
        (pduInfoPtr->SduDataPtr[config->PduCbvPosition] & UDPNM_CBV_PARTIAL_NETWORK_INFORMATION)) {
      if (config->AllNmMessagesKeepAwake) { /* @SWS_UdpNm_00329 */
        ret = E_OK;
      } else {
        /* @SWS_UdpNm_00462 */
        ret = E_NOT_OK;
      }
    } else {
      /* @SWS_UdpNm_00331 */
      ret = E_NOT_OK;
      for (i = 0; i < config->PnInfoLength; i++) {
        /* @SWS_UdpNm_00338, @SWS_UdpNm_00339 */
        if (pduInfoPtr->SduDataPtr[config->PnInfoOffset + i] & config->PnFilterMaskByte[i]) {
          ret = E_OK;
          break;
        }
      }
    }
  } else {
    /* @SWS_UdpNm_00328 */
  }

  return ret;
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
void UdpNm_Init(const UdpNm_ConfigType *udpnmConfigPtr) {
  int i;
  UdpNm_ChannelContextType *context;
  const UdpNm_ChannelConfigType *config;
  (void)udpnmConfigPtr;

  for (i = 0; i < UDPNM_CONFIG->NumOfChannels; i++) {
    context = &UDPNM_CONFIG->ChannelContexts[i];
    config = &UDPNM_CONFIG->ChannelConfigs[i];
    context->State = NM_STATE_BUS_SLEEP; /* @SWS_UdpNm_00141 */
    context->Flags = 0;                  /* @SWS_UdpNm_00143 */
    memset(context->RxPdu, 0xFF, sizeof(context->RxPdu));
    memset(context->Data, 0xFF, sizeof(context->Data)); /* @SWS_UdpNm_00025 */
    if (config->PduNidPosition < UDPNM_PDU_OFF) {
      context->Data[config->PduNidPosition] = config->NodeId; /* @SWS_UdpNm_00013 */
    }
    if (config->PduCbvPosition < UDPNM_PDU_OFF) {
      context->Data[config->PduCbvPosition] = 0x00; /* @SWS_UdpNm_00085 */
      if (config->PnEnabled) {                      /* @SWS_UdpNm_00332 */
        context->Data[config->PduCbvPosition] |= UDPNM_CBV_PARTIAL_NETWORK_INFORMATION;
        memcpy(&context->Data[config->PnInfoOffset], config->PnFilterMaskByte,
               config->PnInfoLength);
      }
    }
  }
}

Std_ReturnType UdpNm_PassiveStartUp(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;

  if (nmChannelHandle < UDPNM_CONFIG->NumOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    switch ((context->State)) {
    case NM_STATE_BUS_SLEEP:
    case NM_STATE_PREPARE_BUS_SLEEP:
      UdpNmEnterCritical();
      context->Flags |= UDPNM_PASSIVE_STARTUP_REQUEST_MASK;
      UdpNmExitCritical();
      break;
    default:
      /* @SWS_UdpNm_00147 */
      ret = E_NOT_OK;
      break;
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType UdpNm_NetworkRequest(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;
  const UdpNm_ChannelConfigType *config;

  if (nmChannelHandle < UDPNM_CONFIG->NumOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &UDPNM_CONFIG->ChannelConfigs[nmChannelHandle];
    UdpNmEnterCritical();
    context->Flags |= UDPNM_REQUEST_MASK;
    if ((config->ActiveWakeupBitEnabled) && (config->PduCbvPosition < UDPNM_PDU_OFF)) {
      context->Data[config->PduCbvPosition] |= UDPNM_CBV_ACTIVE_WAKEUP; /* @SWS_UdpNm_00366 */
    }
    UdpNmExitCritical();
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType UdpNm_NetworkRelease(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;

  if (nmChannelHandle < UDPNM_CONFIG->NumOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    UdpNmEnterCritical();
    context->Flags &= ~UDPNM_REQUEST_MASK;
    UdpNmExitCritical();
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType UdpNm_RepeatMessageRequest(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;
  const UdpNm_ChannelConfigType *config;

  if (nmChannelHandle < UDPNM_CONFIG->NumOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &UDPNM_CONFIG->ChannelConfigs[nmChannelHandle];
    switch (context->State) {
    case NM_STATE_READY_SLEEP:
    case NM_STATE_NORMAL_OPERATION:
      UdpNmEnterCritical();
      context->Flags |= UDPNM_REPEAT_MESSAGE_REQUEST_MASK;
      /* @WS_UdpNm_00121, @SWS_UdpNm_00113 */
      if (config->NodeDetectionEnabled) {
        if (config->PduCbvPosition < UDPNM_PDU_OFF) {
          context->Data[config->PduCbvPosition] |= UDPNM_CBV_REPEAT_MESSAGE_REQUEST;
        }
      }
      UdpNmEnterCritical();
      break;
    default:
      /* @SWS_UdpNm_00137 */
      ret = E_NOT_OK;
      break;
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType UdpNm_DisableCommunication(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;

  if (nmChannelHandle < UDPNM_CONFIG->NumOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    /* @SWS_UdpNm_00170 */
    UdpNmEnterCritical();
    context->Flags |= UDPNM_DISABLE_COMMUNICATION_REQUEST_MASK;
    UdpNmEnterCritical();
    /* could see codes that check UDPNM_DISABLE_COMMUNICATION_REQUEST_MASK in the MainFunction,
     * That is because that Alarm operation is not atomic, so the codes that added to check
     * UDPNM_DISABLE_COMMUNICATION_REQUEST_MASK is a workaroud and which is not good but it
     * satisfied the requirement */
    UdpNmCancelAlarm(Tx);        /* @SWS_UdpNm_00173 */
    UdpNmCancelAlarm(NmTimeout); /* @SWS_UdpNm_00174 */
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED
    UdpNmCancelAlarm(RemoteSleepInd); /* @SWS_UdpNm_00175 */
#endif
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType UdpNm_EnableCommunication(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;
  const UdpNm_ChannelConfigType *config;

  if (nmChannelHandle < UDPNM_CONFIG->NumOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &UDPNM_CONFIG->ChannelConfigs[nmChannelHandle];
    UdpNmEnterCritical();
    context->Flags &= ~UDPNM_DISABLE_COMMUNICATION_REQUEST_MASK;
    UdpNmEnterCritical();
    UdpNmSetAlarm(Tx, 0);                            /* @SWS_UdpNm_00178 */
    UdpNmSetAlarm(NmTimeout, config->NmTimeoutTime); /* @SWS_UdpNm_00179 */
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED
    UdpNmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime); /* @SWS_UdpNm_00180 */
#endif
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

void UdpNm_SoAdIfTxConfirmation(PduIdType txPduId, Std_ReturnType result) {
  UdpNm_ChannelContextType *context;
  const UdpNm_ChannelConfigType *config;
  if (txPduId < UDPNM_CONFIG->NumOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[txPduId];
    config = &UDPNM_CONFIG->ChannelConfigs[txPduId];
    if (E_OK == result) {
      if (context->TxCounter < 0xFF) {
        context->TxCounter++;
      }
      UdpNmCancelAlarm(TxTimeout);

      if ((NM_STATE_NORMAL_OPERATION == context->State) ||
          (NM_STATE_REPEAT_MESSAGE == context->State) || (NM_STATE_READY_SLEEP == context->State)) {
        UdpNmSetAlarm(NmTimeout, config->NmTimeoutTime);
      }
    } else {
      /* @SWS_UdpNm_00379 */
      Nm_TxTimeoutException(config->NmNetworkHandle);
    }
  } else {
    ASLOG(ERROR, ("UdpNm_TxConfirmation with invalid txPduId %d\n", txPduId));
  }
}

void UdpNm_SoAdIfRxIndication(PduIdType rxPduId, const PduInfoType *pduInfoPtr) {
  UdpNm_ChannelContextType *context;
  const UdpNm_ChannelConfigType *config;
  uint8_t flags = UDPNM_RX_INDICATION_REQUEST_MASK | UDPNM_NM_PDU_RECEIVED_MASK;
  Std_ReturnType ret = E_NOT_OK;
  if (rxPduId < UDPNM_CONFIG->NumOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[rxPduId];
    config = &UDPNM_CONFIG->ChannelConfigs[rxPduId];
#ifdef UDPNM_GLOBAL_PN_SUPPORT
    ret = UdpNm_RxFilter(context, config, pduInfoPtr);
#else
    ret = E_OK;
#endif
  } else {
    ASLOG(ERROR, ("UdpNm_RxIndication with invalid RxPduId %d\n", rxPduId));
  }
  if (E_OK == ret) {
    /* @SWS_UdpNm_00035 */
    memcpy(context->RxPdu, pduInfoPtr->SduDataPtr, sizeof(context->RxPdu));
    if (config->PduCbvPosition < UDPNM_PDU_OFF) {
      if (context->RxPdu[config->PduCbvPosition] & UDPNM_CBV_REPEAT_MESSAGE_REQUEST) {
        if (config->NodeDetectionEnabled) {
          /* @SWS_UdpNm_00119, @SWS_UdpNm_00111 */
          if ((NM_STATE_NORMAL_OPERATION == context->State) ||
              (NM_STATE_READY_SLEEP == context->State)) {
            flags |= UDPNM_REPEAT_MESSAGE_REQUEST_MASK;
          }
        }
        /* @SWS_UdpNm_00014 */
        if (config->RepeatMsgIndEnabled && config->NodeDetectionEnabled) {
          Nm_RepeatMessageIndication(config->NmNetworkHandle);
        }
      }
#ifdef UDPNM_COORDINATOR_SYNC_SUPPORT
      if ((NM_STATE_NORMAL_OPERATION == context->State) ||
          (NM_STATE_REPEAT_MESSAGE == context->State) || (NM_STATE_READY_SLEEP == context->State)) {
        if (context->RxPdu[config->PduCbvPosition] & UDPNM_CBV_NM_COORDINATOR_SLEEP) {
          /* @SWS_UdpNm_00364 */
          if (0 == (context->Flags & UDPNM_COORDINATOR_SLEEP_SYNC_MASK)) {
            flags |= UDPNM_COORDINATOR_SLEEP_SYNC_MASK;
            Nm_CoordReadyToSleepIndication(config->NmNetworkHandle);
          }
        } else if (context->Flags & UDPNM_COORDINATOR_SLEEP_SYNC_MASK) {
          /* @SWS_UdpNm_00320 */
          Nm_CoordReadyToSleepCancellation(config->NmNetworkHandle);
          UdpNmEnterCritical();
          context->Flags &= ~UDPNM_COORDINATOR_SLEEP_SYNC_MASK;
          UdpNmExitCritical();
        }
      }
#endif
    }
    if ((NM_STATE_NORMAL_OPERATION == context->State) ||
        (NM_STATE_REPEAT_MESSAGE == context->State) || (NM_STATE_READY_SLEEP == context->State)) {
      UdpNmSetAlarm(NmTimeout, config->NmTimeoutTime);
    }
    UdpNmEnterCritical();
    context->Flags |= flags;
    UdpNmExitCritical();
  }
}

Std_ReturnType UdpNm_GetLocalNodeIdentifier(NetworkHandleType nmChannelHandle,
                                            uint8_t *nmNodeIdPtr) {
  Std_ReturnType ret = E_OK;
  const UdpNm_ChannelConfigType *config;

  if ((nmChannelHandle < UDPNM_CONFIG->NumOfChannels) && (NULL != nmNodeIdPtr)) {
    config = &UDPNM_CONFIG->ChannelConfigs[nmChannelHandle];
    *nmNodeIdPtr = config->NodeId;
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType UdpNm_GetNodeIdentifier(NetworkHandleType nmChannelHandle, uint8_t *nmNodeIdPtr) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;
  const UdpNm_ChannelConfigType *config;
  uint8_t nodeId = UDPNM_INVALID_NODE_ID;

  if ((nmChannelHandle < UDPNM_CONFIG->NumOfChannels) && (NULL != nmNodeIdPtr)) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &UDPNM_CONFIG->ChannelConfigs[nmChannelHandle];
    /* @SWS_UdpNm_00132 */
    if (config->PduNidPosition < UDPNM_PDU_OFF) {
      nodeId = context->RxPdu[config->PduNidPosition];
    }
    if (UDPNM_INVALID_NODE_ID == nodeId) {
      ret = E_NOT_OK;
    } else {
      *nmNodeIdPtr = nodeId;
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType UdpNm_SetUserData(NetworkHandleType nmChannelHandle, const uint8_t *nmUserDataPtr) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;
  const UdpNm_ChannelConfigType *config;
  uint8_t notUserDataMask = 0;
  uint8_t userDataLength = UDPNM_PDU_LENGTH;
  int i, j = 0;

  if ((nmChannelHandle < UDPNM_CONFIG->NumOfChannels) && (NULL != nmUserDataPtr)) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &UDPNM_CONFIG->ChannelConfigs[nmChannelHandle];
    if (config->PduNidPosition != UDPNM_PDU_OFF) {
      notUserDataMask |= (1 << config->PduNidPosition);
      userDataLength--;
    }
    if (config->PduCbvPosition != UDPNM_PDU_OFF) {
      notUserDataMask |= (1 << config->PduCbvPosition);
      userDataLength--;
    }
#ifdef UDPNM_GLOBAL_PN_SUPPORT
    if (config->PnEnabled) {
      userDataLength -= config->PnInfoLength;
      for (i = 0; i < config->PnInfoLength; i++) {
        notUserDataMask |= (1 << (config->PnInfoOffset + i));
      }
    }
#endif
    for (i = 0; i < UDPNM_PDU_LENGTH; i++) {
      if (0 == (notUserDataMask & (1 << i))) {
        context->Data[i] = nmUserDataPtr[j];
        j++;
      }
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType UdpNm_GetUserData(NetworkHandleType nmChannelHandle, uint8_t *nmUserDataPtr) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;
  const UdpNm_ChannelConfigType *config;
  uint8_t notUserDataMask = 0;
  uint8_t userDataLength = UDPNM_PDU_LENGTH;
  int i, j = 0;

  if ((nmChannelHandle < UDPNM_CONFIG->NumOfChannels) && (NULL != nmUserDataPtr)) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &UDPNM_CONFIG->ChannelConfigs[nmChannelHandle];
    if (config->PduNidPosition != UDPNM_PDU_OFF) {
      notUserDataMask |= (1 << config->PduNidPosition);
      userDataLength--;
    }
    if (config->PduCbvPosition != UDPNM_PDU_OFF) {
      notUserDataMask |= (1 << config->PduCbvPosition);
      userDataLength--;
    }
#ifdef UDPNM_GLOBAL_PN_SUPPORT
    if (config->PnEnabled) {
      userDataLength -= config->PnInfoLength;
      for (i = 0; i < config->PnInfoLength; i++) {
        notUserDataMask |= (1 << (config->PnInfoOffset + i));
      }
    }
#endif
    for (i = 0; i < UDPNM_PDU_LENGTH; i++) {
      if (0 == (notUserDataMask & (1 << i))) {
        nmUserDataPtr[j] = context->Data[i];
        j++;
      }
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType UdpNm_GetPduData(NetworkHandleType nmChannelHandle, uint8_t *nmPduDataPtr) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;

  if ((nmChannelHandle < UDPNM_CONFIG->NumOfChannels) && (NULL != nmPduDataPtr)) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    if (context->Flags & UDPNM_NM_PDU_RECEIVED_MASK) {
      memcpy(nmPduDataPtr, &context->RxPdu, UDPNM_PDU_LENGTH);
    } else {
      ret = E_NOT_OK;
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType UdpNm_GetState(NetworkHandleType nmChannelHandle, Nm_StateType *nmStatePtr,
                              Nm_ModeType *nmModePtr) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;

  if ((nmChannelHandle < UDPNM_CONFIG->NumOfChannels) && (NULL != nmStatePtr) &&
      (NULL != nmModePtr)) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    *nmStatePtr = context->State;
    switch (context->State) {
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

#ifdef UDPNM_GLOBAL_PN_SUPPORT
void UdpNm_ConfirmPnAvailability(NetworkHandleType nmChannelHandle) {
  UdpNm_ChannelContextType *context;
  const UdpNm_ChannelConfigType *config;
  if (nmChannelHandle < UDPNM_CONFIG->NumOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &UDPNM_CONFIG->ChannelConfigs[nmChannelHandle];
    if (config->PnEnabled) {
      ASLOG(UDPNM, ("%d: confirm PN, flags %X\n", nmChannelHandle, context->Flags));
      /* @00404 */
      UdpNmEnterCritical();
      context->Flags |= UDPNM_PN_ENABLED_MASK;
      UdpNmExitCritical();
    }
  }
}
#endif

#ifdef UDPNM_COORDINATOR_SYNC_SUPPORT
Std_ReturnType UdpNm_SetSleepReadyBit(NetworkHandleType nmChannelHandle, boolean nmSleepReadyBit) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;
  const UdpNm_ChannelConfigType *config;

  if (nmChannelHandle < UDPNM_CONFIG->NumOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &UDPNM_CONFIG->ChannelConfigs[nmChannelHandle];
    UdpNmEnterCritical();
    if (config->PduCbvPosition < UDPNM_PDU_OFF) {
      /* @00342 */
      if (nmSleepReadyBit) {
        context->Data[config->PduCbvPosition] |= UDPNM_CBV_NM_COORDINATOR_SLEEP;
      } else {
        context->Data[config->PduCbvPosition] &= ~UDPNM_CBV_NM_COORDINATOR_SLEEP;
      }
    } else {
      ret = E_NOT_OK;
    }
    UdpNmEnterCritical();
    if (E_OK == ret) {
      UdpNm_SendMessage(context, config, 0);
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}
#endif

#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED
Std_ReturnType UdpNm_CheckRemoteSleepIndication(NetworkHandleType nmChannelHandle,
                                                boolean *nmRemoteSleepIndPtr) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;

  if ((nmChannelHandle < UDPNM_CONFIG->NumOfChannels) && (nmRemoteSleepIndPtr)) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    switch (context->State) {
    case NM_STATE_NORMAL_OPERATION:
      if (context->Flags & UDPNM_REMOTE_SLEEP_IND_MASK) {
        *nmRemoteSleepIndPtr = TRUE;
      } else {
        *nmRemoteSleepIndPtr = FALSE;
      }
      break;
    default:
      /* @SWS_UdpNm_00154 */
      ret = E_NOT_OK;
      break;
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}
#endif

void UdpNm_MainFunction(void) {
  int i;
  UdpNm_ChannelContextType *context;
  const UdpNm_ChannelConfigType *config;

  for (i = 0; i < UDPNM_CONFIG->NumOfChannels; i++) {
    context = &UDPNM_CONFIG->ChannelContexts[i];
    config = &UDPNM_CONFIG->ChannelConfigs[i];
    switch (context->State) {
    case NM_STATE_BUS_SLEEP:
      UdpNm_BusSleepMain(context, config);
      break;
    case NM_STATE_PREPARE_BUS_SLEEP:
      UdpNm_PrepareBusSleepMain(context, config);
      break;
    case NM_STATE_READY_SLEEP:
      UdpNm_ReadySleepMain(context, config);
      break;
    case NM_STATE_REPEAT_MESSAGE:
      UdpNm_RepeatMessageMain(context, config);
      break;
    case NM_STATE_NORMAL_OPERATION:
      UdpNm_NetworkNormalOperationMain(context, config);
      break;
    default:
      break;
    }
  }
}

BufReq_ReturnType UdpNm_SoAdStartOfReception(PduIdType rxPduId, const PduInfoType *info,
                                             PduLengthType tpSduLength,
                                             PduLengthType *bufferSizePtr) {
  BufReq_ReturnType bufReq = BUFREQ_OK;
  UdpNm_SoAdIfRxIndication(rxPduId, info);
  return bufReq;
}

void UdpNm_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x09, UDPNM_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_UDPNM;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 0;
}
