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
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_UDPNM 1

#ifdef USE_UDPNM_CRITICAL
#define nmEnterCritical() EnterCritical()
#define nmExitCritical() ExitCritical();
#else
#define nmEnterCritical()
#define nmExitCritical()
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
#define nmSetAlarm(Timer, v)                                                                       \
  Std_TimerSet(&context->Alarm._##Timer, ((v)*1000 * UDPNM_MAIN_FUNCTION_PERIOD))
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
static void nmSendMessage(UdpNm_ChannelContextType *context, const UdpNm_ChannelConfigType *config,
                          uint16_t reload) {
  Std_ReturnType ret;
  PduInfoType pdu;

  pdu.SduLength = sizeof(context->data);
  pdu.SduDataPtr = context->data;
  if (0 == (context->flags & UDPNM_DISABLE_COMMUNICATION_REQUEST_MASK)) {
    ret = SoAd_IfTransmit(config->TxPdu, &pdu);
    if (E_OK == ret) {
      nmSetAlarm(TxTimeout, config->MsgTimeoutTime);
      if (reload > 0) {
        nmSetAlarm(Tx, reload);
      }
    } else {
      /* @SWS_UdpNm_00330 */
      nmSetAlarm(Tx, 0);
    }
  }
}

static void nmEnterNetworkMode(UdpNm_ChannelContextType *context,
                               const UdpNm_ChannelConfigType *config) {
  context->state = NM_STATE_REPEAT_MESSAGE;
  context->TxCounter = 0;
  if ((context->flags & UDPNM_REQUEST_MASK) && (config->ImmediateNmTransmissions > 0)) {
    /* @SWS_UdpNm_00334 */
    nmSendMessage(context, config, config->ImmediateNmCycleTime);
  } else {
    /* SWS_UdpNm_00005 */
    nmSetAlarm(Tx, config->MsgCycleOffset);
  }
  nmSetAlarm(NMTimeout, config->NmTimeoutTime);
  nmSetAlarm(RepeatMessage, config->RepeatMessageTime);

  nmEnterCritical();
  context->flags &= ~(UDPNM_NM_PDU_RECEIVED_MASK | UDPNM_PASSIVE_STARTUP_REQUEST_MASK |
                      UDPNM_REMOTE_SLEEP_IND_MASK | UDPNM_RX_INDICATION_REQUEST_MASK |
                      UDPNM_COORDINATOR_SLEEP_SYNC_MASK | UDPNM_PN_ENABLED_MASK);
  nmExitCritical();

  Nm_NetworkMode(config->nmNetworkHandle);
  ASLOG(UDPNM,
        ("%d: Enter Repeat Message Mode, flags %02X\n", config->nmNetworkHandle, context->flags));
}

static void nmRepeatMessageMain(UdpNm_ChannelContextType *context,
                                const UdpNm_ChannelConfigType *config) {
  uint16_t reload = config->MsgCycleTime;

  if (context->flags & UDPNM_RX_INDICATION_REQUEST_MASK) {
    nmEnterCritical();
    context->flags &= ~UDPNM_RX_INDICATION_REQUEST_MASK;
    nmExitCritical();
  }

  if ((context->flags & UDPNM_REQUEST_MASK) && (config->ImmediateNmTransmissions > 0)) {
    /* @SWS_UdpNm_00334 */
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
      if (context->flags & UDPNM_REQUEST_MASK) {
        context->state = NM_STATE_NORMAL_OPERATION;
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED /* @SWS_UdpNm_00149, @SWS_UdpNm_00150 */
        nmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime);
#endif
        ASLOG(UDPNM, ("%d: Enter Normal Operation Mode, flags %02X\n", config->nmNetworkHandle,
                      context->flags));
      } else {
        context->state = NM_STATE_READY_SLEEP;
        ASLOG(UDPNM, ("%d: Enter Ready Sleep Mode, flags %02X\n", config->nmNetworkHandle,
                      context->flags));
      }
      if (config->PduCbvPosition < UDPNM_PDU_OFF) {
        /* @SWS_UdpNm_00107 */
        nmEnterCritical();
        context->data[config->PduCbvPosition] &= ~UDPNM_CBV_REPEAT_MESSAGE_REQUEST;
        nmExitCritical();
      }
    }
  } else {
    nmSetAlarm(RepeatMessage, config->RepeatMessageTime);
  }
}

static void nmBusSleepMain(UdpNm_ChannelContextType *context,
                           const UdpNm_ChannelConfigType *config) {
  if (context->flags & (UDPNM_REQUEST_MASK | UDPNM_PASSIVE_STARTUP_REQUEST_MASK)) {
    nmEnterNetworkMode(context, config);
  } else {
    if (context->flags & UDPNM_RX_INDICATION_REQUEST_MASK) {
      /* @SWS_UdpNm_00127 */
      nmEnterCritical();
      context->flags &= ~UDPNM_RX_INDICATION_REQUEST_MASK;
      nmExitCritical();
      Nm_NetworkStartIndication(config->nmNetworkHandle);
    }
  }
}

static void nmPrepareBusSleepMain(UdpNm_ChannelContextType *context,
                                  const UdpNm_ChannelConfigType *config) {
  if (context->flags & (UDPNM_REQUEST_MASK | UDPNM_PASSIVE_STARTUP_REQUEST_MASK |
                        UDPNM_RX_INDICATION_REQUEST_MASK)) {
    if (context->flags & UDPNM_RX_INDICATION_REQUEST_MASK) {
      nmEnterCritical();
      context->flags &= ~UDPNM_RX_INDICATION_REQUEST_MASK;
      nmExitCritical();
    }
    nmEnterNetworkMode(context, config);
  } else {
    if (nmIsAlarmStarted(WaitBusSleep)) {
      nmSingalAlarm(WaitBusSleep);
      if (nmIsAlarmTimeout(WaitBusSleep)) {
        context->state = NM_STATE_BUS_SLEEP;
        ASLOG(UDPNM,
              ("%d: Enter Bus Sleep Mode, flags %02X\n", config->nmNetworkHandle, context->flags));
        Nm_BusSleepMode(config->nmNetworkHandle);
      }
    } else {
      nmSetAlarm(WaitBusSleep, config->WaitBusSleepTime);
    }
  }
}

static void nmEnterRepeatMessageMode(UdpNm_ChannelContextType *context,
                                     const UdpNm_ChannelConfigType *config) {
  context->state = NM_STATE_REPEAT_MESSAGE;
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED
  if (context->flags & UDPNM_REMOTE_SLEEP_IND_MASK) {
    /* @SWS_UdpNm_00152 */
    Nm_RemoteSleepCancellation(config->nmNetworkHandle);
  }
#endif
  nmEnterCritical();
  context->flags &= ~(UDPNM_REPEAT_MESSAGE_REQUEST_MASK | UDPNM_REMOTE_SLEEP_IND_MASK);
  nmExitCritical();
  ASLOG(UDPNM,
        ("%d: Enter Repeat Message Mode, flags %02X\n", config->nmNetworkHandle, context->flags));
  /* @SWS_UdpNm_00005 */
  nmSetAlarm(Tx, config->MsgCycleOffset);
  nmSetAlarm(RepeatMessage, config->RepeatMessageTime);
}

static void nmReadySleepMain(UdpNm_ChannelContextType *context,
                             const UdpNm_ChannelConfigType *config) {
  if (context->flags & UDPNM_RX_INDICATION_REQUEST_MASK) {
    nmEnterCritical();
    context->flags &= ~UDPNM_RX_INDICATION_REQUEST_MASK;
    nmExitCritical();
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED
    if (context->flags & UDPNM_REMOTE_SLEEP_IND_MASK) {
      nmEnterCritical();
      context->flags &= ~UDPNM_REMOTE_SLEEP_IND_MASK;
      nmExitCritical();
      /* @SWS_UdpNm_00151 */
      Nm_RemoteSleepCancellation(config->nmNetworkHandle);
    }
#endif
  }

  if (context->flags & UDPNM_REPEAT_MESSAGE_REQUEST_MASK) {
    nmEnterRepeatMessageMode(context, config);
  } else if (context->flags & UDPNM_REQUEST_MASK) {
    context->state = NM_STATE_NORMAL_OPERATION;
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED /* @SWS_UdpNm_00149, @SWS_UdpNm_00150 */
    nmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime);
#endif
    nmSendMessage(context, config, config->MsgCycleTime); /* @SWS_UdpNm_00006 */
    ASLOG(UDPNM, ("%d: Enter Normal Operation Mode, flags %02X\n", config->nmNetworkHandle,
                  context->flags));
  } else {
  }

  if (nmIsAlarmStarted(NMTimeout)) {
    nmSingalAlarm(NMTimeout);
    if (context->flags & UDPNM_DISABLE_COMMUNICATION_REQUEST_MASK) {
      nmCancelAlarm(NMTimeout); /* @SWS_UdpNm_00174 */
    }
    if (nmIsAlarmTimeout(NMTimeout)) {
      if (NM_STATE_READY_SLEEP == context->state) {
        nmCancelAlarm(NMTimeout);
        nmSetAlarm(WaitBusSleep, config->WaitBusSleepTime);
        context->state = NM_STATE_PREPARE_BUS_SLEEP;
        if ((config->ActiveWakeupBitEnabled) && (config->PduCbvPosition < UDPNM_PDU_OFF)) {
          context->data[config->PduCbvPosition] &= ~UDPNM_CBV_ACTIVE_WAKEUP; /* @SWS_UdpNm_00367 */
        }
        Nm_PrepareBusSleepMode(config->nmNetworkHandle);
        ASLOG(UDPNM, ("%d: Enter Prepare Bus Sleep Mode, flags %02X\n", config->nmNetworkHandle,
                      context->flags));
      } else {
        nmSetAlarm(NMTimeout, config->NmTimeoutTime);
      }
    }
  } else {
    if (0 == (context->flags & UDPNM_DISABLE_COMMUNICATION_REQUEST_MASK)) {
      /* @SWS_CanNm_00178, @SWS_CanNm_00179 */
      ASLOG(ERROR, ("UDPNM NMTimeout not started\n"));
      nmSetAlarm(NMTimeout, config->NmTimeoutTime);
    }
  }
}

static void nmNetworkNormalOperationMain(UdpNm_ChannelContextType *context,
                                         const UdpNm_ChannelConfigType *config) {
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

  if (context->flags & UDPNM_RX_INDICATION_REQUEST_MASK) {
    nmEnterCritical();
    context->flags &= ~UDPNM_RX_INDICATION_REQUEST_MASK;
    nmExitCritical();
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED
    nmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime);
    if (context->flags & UDPNM_REMOTE_SLEEP_IND_MASK) {
      nmEnterCritical();
      context->flags &= ~UDPNM_REMOTE_SLEEP_IND_MASK;
      nmExitCritical();
      /* @SWS_UdpNm_00151 */
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

#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED /* @SWS_UdpNm_00149, @SWS_UdpNm_00150 */
  if (nmIsAlarmStarted(RemoteSleepInd)) {
    nmSingalAlarm(RemoteSleepInd);
    if (nmIsAlarmTimeout(RemoteSleepInd)) {
      nmCancelAlarm(RemoteSleepInd);
      if (0 == (context->flags & UDPNM_DISABLE_COMMUNICATION_REQUEST_MASK)) {
        /* @00175 */
        Nm_RemoteSleepIndication(config->nmNetworkHandle);
        nmEnterCritical();
        context->flags |= UDPNM_REMOTE_SLEEP_IND_MASK;
        nmExitCritical();
      }
    }
  }
#endif

  if (context->flags & UDPNM_REPEAT_MESSAGE_REQUEST_MASK) {
    nmEnterRepeatMessageMode(context, config);
  } else if (0 == (context->flags & UDPNM_REQUEST_MASK)) {
    context->state = NM_STATE_READY_SLEEP;
    ASLOG(UDPNM,
          ("%d: Enter Ready Sleep Mode, flags %02X\n", config->nmNetworkHandle, context->flags));
  } else {
  }
}

#ifdef UDPNM_GLOBAL_PN_SUPPORT
static Std_ReturnType nmRxFilter(UdpNm_ChannelContextType *context,
                                 const UdpNm_ChannelConfigType *config,
                                 const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_OK;
  int i;
  if (context->flags & UDPNM_PN_ENABLED_MASK) {
    if (0 ==
        (PduInfoPtr->SduDataPtr[config->PduCbvPosition] & UDPNM_CBV_PARTIAL_NETWORK_INFORMATION)) {
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
        if (PduInfoPtr->SduDataPtr[config->PnInfoOffset + i] & config->PnFilterMaskByte[i]) {
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

  for (i = 0; i < UDPNM_CONFIG->numOfChannels; i++) {
    context = &UDPNM_CONFIG->ChannelContexts[i];
    config = &UDPNM_CONFIG->ChannelConfigs[i];
    context->state = NM_STATE_BUS_SLEEP; /* @SWS_UdpNm_00141 */
    context->flags = 0;                  /* @SWS_UdpNm_00143 */
    memset(context->rxPdu, 0xFF, sizeof(context->rxPdu));
    memset(context->data, 0xFF, sizeof(context->data)); /* @SWS_UdpNm_00025 */
    if (config->PduNidPosition < UDPNM_PDU_OFF) {
      context->data[config->PduNidPosition] = config->NodeId; /* @SWS_UdpNm_00013 */
    }
    if (config->PduCbvPosition < UDPNM_PDU_OFF) {
      context->data[config->PduCbvPosition] = 0x00; /* @SWS_UdpNm_00085 */
      if (config->PnEnabled) {                      /* @SWS_UdpNm_00332 */
        context->data[config->PduCbvPosition] |= UDPNM_CBV_PARTIAL_NETWORK_INFORMATION;
        memcpy(&context->data[config->PnInfoOffset], config->PnFilterMaskByte,
               config->PnInfoLength);
      }
    }
  }
}

Std_ReturnType UdpNm_PassiveStartUp(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;

  if (nmChannelHandle < UDPNM_CONFIG->numOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    switch ((context->state)) {
    case NM_STATE_BUS_SLEEP:
    case NM_STATE_PREPARE_BUS_SLEEP:
      nmEnterCritical();
      context->flags |= UDPNM_PASSIVE_STARTUP_REQUEST_MASK;
      nmExitCritical();
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

  if (nmChannelHandle < UDPNM_CONFIG->numOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &UDPNM_CONFIG->ChannelConfigs[nmChannelHandle];
    nmEnterCritical();
    context->flags |= UDPNM_REQUEST_MASK;
    if ((config->ActiveWakeupBitEnabled) && (config->PduCbvPosition < UDPNM_PDU_OFF)) {
      context->data[config->PduCbvPosition] |= UDPNM_CBV_ACTIVE_WAKEUP; /* @SWS_UdpNm_00366 */
    }
    nmExitCritical();
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType UdpNm_NetworkRelease(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;

  if (nmChannelHandle < UDPNM_CONFIG->numOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    nmEnterCritical();
    context->flags &= ~UDPNM_REQUEST_MASK;
    nmExitCritical();
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType UdpNm_RepeatMessageRequest(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;
  const UdpNm_ChannelConfigType *config;

  if (nmChannelHandle < UDPNM_CONFIG->numOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &UDPNM_CONFIG->ChannelConfigs[nmChannelHandle];
    switch (context->state) {
    case NM_STATE_READY_SLEEP:
    case NM_STATE_NORMAL_OPERATION:
      nmEnterCritical();
      context->flags |= UDPNM_REPEAT_MESSAGE_REQUEST_MASK;
      /* @WS_UdpNm_00121, @SWS_UdpNm_00113 */
      if (config->NodeDetectionEnabled) {
        if (config->PduCbvPosition < UDPNM_PDU_OFF) {
          context->data[config->PduCbvPosition] |= UDPNM_CBV_REPEAT_MESSAGE_REQUEST;
        }
      }
      nmEnterCritical();
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

  if (nmChannelHandle < UDPNM_CONFIG->numOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    /* @SWS_UdpNm_00170 */
    nmEnterCritical();
    context->flags |= UDPNM_DISABLE_COMMUNICATION_REQUEST_MASK;
    nmEnterCritical();
    /* could see codes that check UDPNM_DISABLE_COMMUNICATION_REQUEST_MASK in the MainFunction,
     * That is because that Alarm operation is not atomic, so the codes that added to check
     * UDPNM_DISABLE_COMMUNICATION_REQUEST_MASK is a workaroud and which is not good but it
     * satisfied the requirement */
    nmCancelAlarm(Tx);        /* @SWS_UdpNm_00173 */
    nmCancelAlarm(NMTimeout); /* @SWS_UdpNm_00174 */
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED
    nmCancelAlarm(RemoteSleepInd); /* @SWS_UdpNm_00175 */
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

  if (nmChannelHandle < UDPNM_CONFIG->numOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &UDPNM_CONFIG->ChannelConfigs[nmChannelHandle];
    nmEnterCritical();
    context->flags &= ~UDPNM_DISABLE_COMMUNICATION_REQUEST_MASK;
    nmEnterCritical();
    nmSetAlarm(Tx, 0);                            /* @SWS_UdpNm_00178 */
    nmSetAlarm(NMTimeout, config->NmTimeoutTime); /* @SWS_UdpNm_00179 */
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED
    nmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime); /* @SWS_UdpNm_00180 */
#endif
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

void UdpNm_SoAdIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result) {
  UdpNm_ChannelContextType *context;
  const UdpNm_ChannelConfigType *config;
  if (TxPduId < UDPNM_CONFIG->numOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[TxPduId];
    config = &UDPNM_CONFIG->ChannelConfigs[TxPduId];
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
      /* @SWS_UdpNm_00379 */
      Nm_TxTimeoutException(config->nmNetworkHandle);
    }
  } else {
    ASLOG(ERROR, ("UdpNm_TxConfirmation with invalid TxPduId %d\n", TxPduId));
  }
}

void UdpNm_SoAdIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  UdpNm_ChannelContextType *context;
  const UdpNm_ChannelConfigType *config;
  uint8_t flags = UDPNM_RX_INDICATION_REQUEST_MASK | UDPNM_NM_PDU_RECEIVED_MASK;
  Std_ReturnType ret = E_NOT_OK;
  if (RxPduId < UDPNM_CONFIG->numOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[RxPduId];
    config = &UDPNM_CONFIG->ChannelConfigs[RxPduId];
#ifdef UDPNM_GLOBAL_PN_SUPPORT
    ret = nmRxFilter(context, config, PduInfoPtr);
#else
    ret = E_OK;
#endif
  } else {
    ASLOG(ERROR, ("UdpNm_RxIndication with invalid RxPduId %d\n", RxPduId));
  }
  if (E_OK == ret) {
    /* @SWS_UdpNm_00035 */
    memcpy(context->rxPdu, PduInfoPtr->SduDataPtr, sizeof(context->rxPdu));
    if (config->PduCbvPosition < UDPNM_PDU_OFF) {
      if (context->rxPdu[config->PduCbvPosition] & UDPNM_CBV_REPEAT_MESSAGE_REQUEST) {
        if (config->NodeDetectionEnabled) {
          /* @SWS_UdpNm_00119, @SWS_UdpNm_00111 */
          if ((NM_STATE_NORMAL_OPERATION == context->state) ||
              (NM_STATE_READY_SLEEP == context->state)) {
            flags |= UDPNM_REPEAT_MESSAGE_REQUEST_MASK;
          }
        }
        /* @SWS_UdpNm_00014 */
        if (config->RepeatMsgIndEnabled && config->NodeDetectionEnabled) {
          Nm_RepeatMessageIndication(config->nmNetworkHandle);
        }
      }
#ifdef UDPNM_COORDINATOR_SYNC_SUPPORT
      if ((NM_STATE_NORMAL_OPERATION == context->state) ||
          (NM_STATE_REPEAT_MESSAGE == context->state) || (NM_STATE_READY_SLEEP == context->state)) {
        if (context->rxPdu[config->PduCbvPosition] & UDPNM_CBV_NM_COORDINATOR_SLEEP) {
          /* @SWS_UdpNm_00364 */
          if (0 == (context->flags & UDPNM_COORDINATOR_SLEEP_SYNC_MASK)) {
            flags |= UDPNM_COORDINATOR_SLEEP_SYNC_MASK;
            Nm_CoordReadyToSleepIndication(config->nmNetworkHandle);
          }
        } else if (context->flags & UDPNM_COORDINATOR_SLEEP_SYNC_MASK) {
          /* @SWS_UdpNm_00320 */
          Nm_CoordReadyToSleepCancellation(config->nmNetworkHandle);
          nmEnterCritical();
          context->flags &= ~UDPNM_COORDINATOR_SLEEP_SYNC_MASK;
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

Std_ReturnType UdpNm_GetLocalNodeIdentifier(NetworkHandleType nmChannelHandle,
                                            uint8_t *nmNodeIdPtr) {
  Std_ReturnType ret = E_OK;
  const UdpNm_ChannelConfigType *config;

  if ((nmChannelHandle < UDPNM_CONFIG->numOfChannels) && (NULL != nmNodeIdPtr)) {
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

  if ((nmChannelHandle < UDPNM_CONFIG->numOfChannels) && (NULL != nmNodeIdPtr)) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &UDPNM_CONFIG->ChannelConfigs[nmChannelHandle];
    /* @SWS_UdpNm_00132 */
    if (config->PduNidPosition < UDPNM_PDU_OFF) {
      nodeId = context->rxPdu[config->PduNidPosition];
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

  if ((nmChannelHandle < UDPNM_CONFIG->numOfChannels) && (NULL != nmUserDataPtr)) {
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
        context->data[i] = nmUserDataPtr[j];
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

  if ((nmChannelHandle < UDPNM_CONFIG->numOfChannels) && (NULL != nmUserDataPtr)) {
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
        nmUserDataPtr[j] = context->data[i];
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

  if ((nmChannelHandle < UDPNM_CONFIG->numOfChannels) && (NULL != nmPduDataPtr)) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    if (context->flags & UDPNM_NM_PDU_RECEIVED_MASK) {
      memcpy(nmPduDataPtr, &context->rxPdu, UDPNM_PDU_LENGTH);
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

  if ((nmChannelHandle < UDPNM_CONFIG->numOfChannels) && (NULL != nmStatePtr) &&
      (NULL != nmModePtr)) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
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

#ifdef UDPNM_GLOBAL_PN_SUPPORT
void UdpNm_ConfirmPnAvailability(NetworkHandleType nmChannelHandle) {
  UdpNm_ChannelContextType *context;
  const UdpNm_ChannelConfigType *config;
  if (nmChannelHandle < UDPNM_CONFIG->numOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &UDPNM_CONFIG->ChannelConfigs[nmChannelHandle];
    if (config->PnEnabled) {
      ASLOG(UDPNM, ("%d: confirm PN, flags %X\n", nmChannelHandle, context->flags));
      /* @00404 */
      nmEnterCritical();
      context->flags |= UDPNM_PN_ENABLED_MASK;
      nmExitCritical();
    }
  }
}
#endif

#ifdef UDPNM_COORDINATOR_SYNC_SUPPORT
Std_ReturnType UdpNm_SetSleepReadyBit(NetworkHandleType nmChannelHandle, boolean nmSleepReadyBit) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;
  const UdpNm_ChannelConfigType *config;

  if (nmChannelHandle < UDPNM_CONFIG->numOfChannels) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    config = &UDPNM_CONFIG->ChannelConfigs[nmChannelHandle];
    nmEnterCritical();
    if (config->PduCbvPosition < UDPNM_PDU_OFF) {
      /* @00342 */
      if (nmSleepReadyBit) {
        context->data[config->PduCbvPosition] |= UDPNM_CBV_NM_COORDINATOR_SLEEP;
      } else {
        context->data[config->PduCbvPosition] &= ~UDPNM_CBV_NM_COORDINATOR_SLEEP;
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

#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED
Std_ReturnType UdpNm_CheckRemoteSleepIndication(NetworkHandleType nmChannelHandle,
                                                boolean *nmRemoteSleepIndPtr) {
  Std_ReturnType ret = E_OK;
  UdpNm_ChannelContextType *context;

  if ((nmChannelHandle < UDPNM_CONFIG->numOfChannels) && (nmRemoteSleepIndPtr)) {
    context = &UDPNM_CONFIG->ChannelContexts[nmChannelHandle];
    switch (context->state) {
    case NM_STATE_NORMAL_OPERATION:
      if (context->flags & UDPNM_REMOTE_SLEEP_IND_MASK) {
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

  for (i = 0; i < UDPNM_CONFIG->numOfChannels; i++) {
    context = &UDPNM_CONFIG->ChannelContexts[i];
    config = &UDPNM_CONFIG->ChannelConfigs[i];
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
