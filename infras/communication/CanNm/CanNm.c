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
#include "PduR_CanNm.h"
#include <string.h>
#include "CanSM_CanIf.h"

#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_CANNM 0
#define AS_LOG_CANNME 2

#ifdef USE_CANNM_CRITICAL
#define nmEnterCritical() EnterCritical()
#define nmExitCritical() ExitCritical();
#else
#define nmEnterCritical()
#define nmExitCritical()
#endif

#ifdef CANNM_USE_PB_CONFIG
#define CANNM_CONFIG cannmConfig
#else
#define CANNM_CONFIG (&CanNm_Config)
#endif

#define CANNM_REQUEST_MASK 0x01u
#define CANNM_PASSIVE_STARTUP_REQUEST_MASK 0x02u
#define CANNM_REPEAT_MESSAGE_REQUEST_MASK 0x04u
#define CANNM_DISABLE_COMMUNICATION_REQUEST_MASK 0x08u
#define CANNM_RX_INDICATION_REQUEST_MASK 0x10u
#define CANNM_NM_PDU_RECEIVED_MASK 0x20u
#define CANNM_REMOTE_SLEEP_IND_MASK 0x40u
#define CANNM_COORDINATOR_SLEEP_SYNC_MASK 0x80u
#define CANNM_PN_ENABLED_MASK 0x100u
#define CANNM_RX_REPEAT_MESSAGE_BIT_SET_MASK 0x200u

#ifdef CANNM_USE_STD_TIMER
#define nmSetAlarm(Timer, v)                                                                       \
  do {                                                                                             \
    Std_TimerInit(&context->Alarm._##Timer, ((v) * 1000u * CANNM_MAIN_FUNCTION_PERIOD));           \
  } while (0)
#define nmSingalAlarm(Timer)
#define nmIsAlarmTimeout(Timer) Std_IsTimerTimeout(&context->Alarm._##Timer)
#define nmIsAlarmStarted(Timer) Std_IsTimerStarted(&context->Alarm._##Timer)
#define nmCancelAlarm(Timer) Std_TimerStop(&context->Alarm._##Timer)
#else
/* Alarm Management */
#define nmSetAlarm(Timer, v)                                                                       \
  do {                                                                                             \
    context->Alarm._##Timer = 1u + (v);                                                            \
  } while (0)

/* signal the alarm to process one step/tick forward */
#define nmSingalAlarm(Timer)                                                                       \
  do {                                                                                             \
    if (context->Alarm._##Timer > 1u) {                                                            \
      (context->Alarm._##Timer)--;                                                                 \
    }                                                                                              \
  } while (0)

#define nmIsAlarmTimeout(Timer) (1u == context->Alarm._##Timer)

#define nmIsAlarmStarted(Timer) (0u != context->Alarm._##Timer)

#define nmCancelAlarm(Timer)                                                                       \
  do {                                                                                             \
    context->Alarm._##Timer = 0;                                                                   \
  } while (0)
#endif

/* @SWS_CanNm_00045, SWS_CanNm_00085 */
#define CANNM_CBV_REPEAT_MESSAGE_REQUEST 0x01u
#define CANNM_CBV_NM_COORDINATOR_SLEEP 0x08u
#define CANNM_CBV_ACTIVE_WAKEUP 0x10u
#define CANNM_CBV_PARTIAL_NETWORK_INFORMATION 0x40u
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const CanNm_ConfigType CanNm_Config;
/* ================================ [ DATAS     ] ============================================== */
#ifdef CANNM_USE_PB_CONFIG
static const CanNm_ConfigType *cannmConfig = NULL;
#endif
/* ================================ [ LOCALS    ] ============================================== */
static void CanNm_SetUserDataImpl(CanNm_ChannelContextType *context,
                                  const CanNm_ChannelConfigType *config,
                                  const uint8_t *nmUserDataPtr) {
  uint8_t notUserDataMask = 0u;
  uint16_t i;
  uint16_t j = 0u;
  if (config->PduNidPosition != CANNM_PDU_OFF) {
    notUserDataMask |= (1u << config->PduNidPosition);
  }
  if (config->PduCbvPosition != CANNM_PDU_OFF) {
    notUserDataMask |= (1u << config->PduCbvPosition);
  }
#ifdef CANNM_GLOBAL_PN_SUPPORT
  if (config->PnEnabled) {
    for (i = 0u; i < config->PnInfoLength; i++) {
      notUserDataMask |= (1u << (config->PnInfoOffset + i));
    }
  }
#endif
  for (i = 0u; i < 8u; i++) {
    if (0u == (notUserDataMask & (1u << i))) {
      context->data[i] = nmUserDataPtr[j];
      j++;
    }
  }
}

static void nmSendMessage(CanNm_ChannelContextType *context, const CanNm_ChannelConfigType *config,
                          uint16_t reload) {
  Std_ReturnType ret = E_OK;
  PduInfoType pdu;
#ifdef CANNM_COM_USER_DATA_SUPPORT
  uint8_t userData[8] = {0, 0, 0, 0, 0, 0, 0, 0};
#endif

  if (0u == (context->flags & CANNM_DISABLE_COMMUNICATION_REQUEST_MASK)) {
#ifdef CANNM_COM_USER_DATA_SUPPORT
    pdu.SduLength = sizeof(userData);
    pdu.SduDataPtr = userData;
    if (config->PduCbvPosition != CANNM_PDU_OFF) {
      pdu.SduLength--;
    }
    if (config->PduNidPosition != CANNM_PDU_OFF) {
      pdu.SduLength--;
    }
#ifdef CANNM_GLOBAL_PN_SUPPORT
    if (config->PnEnabled) {
      pdu.SduLength -= config->PnInfoLength;
    }
#endif
    ret = PduR_CanNmTriggerTransmit(config->UserDataTxPdu, &pdu);
    if (E_OK == ret) {
      CanNm_SetUserDataImpl(context, config, userData);
    } else {
      ASLOG(CANNME, ("Failed to get user data with Pdu %d\n", config->UserDataTxPdu));
    }
#endif

    pdu.SduLength = sizeof(context->data);
    pdu.SduDataPtr = context->data;
    pdu.MetaDataPtr = NULL;
    ret = CanIf_Transmit(config->TxPdu, &pdu);
    if (E_OK == ret) { /* @SWS_CanNm_00064 */
      nmSetAlarm(TxTimeout, config->MsgTimeoutTime);
      if (reload > 0u) {
        nmSetAlarm(Tx, reload);
      }
    } else {
      /* @SWS_CanNm_00335 */
      nmSetAlarm(Tx, 0u);
    }
  }
}

#ifdef CANNM_CAR_WAKEUP_SUPPORT
static void CanNm_CarWakeupProcess(const CanNm_ChannelConfigType *config,
                                   CanNm_ChannelContextType *context, uint8_t nodeId) {
  boolean bProcess = FALSE;
  if (TRUE == config->CarWakeUpRxEnabled) {
    if (TRUE == config->CarWakeUpFilterEnabled) {
      if (nodeId == config->CarWakeUpFilterNodeId) {
        bProcess = TRUE; /* @SWS_CanNm_00408 */
      }
    } else {
      bProcess = TRUE; /* @SWS_CanNm_00406 */
    }
    if (TRUE == bProcess) {
      if (0u !=
          (context->rxPdu[config->CarWakeUpBytePosition] & (1u << config->CarWakeUpBitPosition))) {
#ifdef USE_NM
        Nm_CarWakeUpIndication(config->nmNetworkHandle);
#endif
      }
    }
  }
}
#endif

/* This API called from state BusSleep or PrePreareBusSleep */
static void nmEnterNetworkMode(CanNm_ChannelContextType *context,
                               const CanNm_ChannelConfigType *config) {
  context->state = NM_STATE_REPEAT_MESSAGE;
  context->TxCounter = 0;

  if (0u != (context->flags & CANNM_REQUEST_MASK)) {
    if ((config->ActiveWakeupBitEnabled) && (config->PduCbvPosition < CANNM_PDU_OFF)) {
      context->data[config->PduCbvPosition] |= CANNM_CBV_ACTIVE_WAKEUP; /* @SWS_CanNm_00401 */
    }
#ifdef CANNM_REQUEST_BIT_AUTO_SET
    if (config->PduCbvPosition < CANNM_PDU_OFF) {
      context->data[config->PduCbvPosition] |= CANNM_CBV_REPEAT_MESSAGE_REQUEST;
    }
#endif
  }

  if (
#ifndef CANNM_PASSIVE_STARTUP_REPEAT_ENABLED
    (context->flags & CANNM_REQUEST_MASK) &&
#endif
    (config->ImmediateNmTransmissions > 0u)) {
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
                      CANNM_COORDINATOR_SLEEP_SYNC_MASK | CANNM_PN_ENABLED_MASK |
                      CANNM_RX_REPEAT_MESSAGE_BIT_SET_MASK);
  nmExitCritical();
  ASLOG(CANNM,
        ("%d: Enter Repeat Message Mode, flags %02X\n", config->nmNetworkHandle, context->flags));
}

static void nmRepeatMessageMain(CanNm_ChannelContextType *context,
                                const CanNm_ChannelConfigType *config) {
  uint16_t reload = config->MsgCycleTime;

  if (0u != (context->flags & CANNM_RX_INDICATION_REQUEST_MASK)) {
    nmEnterCritical();
    context->flags &= ~(CANNM_RX_INDICATION_REQUEST_MASK | CANNM_RX_REPEAT_MESSAGE_BIT_SET_MASK);
    nmExitCritical();
  }

#ifdef CANNM_REQUEST_BIT_AUTO_SET /* auto set repeat message bit if network requested, robust! */
  if (0u != (context->flags & CANNM_REQUEST_MASK)) {
    if (config->PduCbvPosition < CANNM_PDU_OFF) {
      context->data[config->PduCbvPosition] |= CANNM_CBV_REPEAT_MESSAGE_REQUEST;
    }
  }
#endif

  if (
#ifndef CANNM_PASSIVE_STARTUP_REPEAT_ENABLED
    (0u != (context->flags & CANNM_REQUEST_MASK)) &&
#endif
    (config->ImmediateNmTransmissions > 0u)) {
    /* @SWS_CanNm_00334 */
    if (context->TxCounter < (config->ImmediateNmTransmissions - 1u)) {
      reload = config->ImmediateNmCycleTime;
    }
  }

  if (nmIsAlarmStarted(TxTimeout)) {
    nmSingalAlarm(TxTimeout);
    if (nmIsAlarmTimeout(TxTimeout)) {
      nmCancelAlarm(TxTimeout);
#ifdef USE_NM /* @SWS_CanNm_00066 */
      Nm_TxTimeoutException(config->nmNetworkHandle);
#endif
#if defined(CANNM_GLOBAL_PN_SUPPORT) && defined(USE_CANSM) /* SWS_CanNm_00446 */
      CanSM_TxTimeoutException((NetworkHandleType)(config - CANNM_CONFIG->ChannelConfigs));
#endif
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
      if (0u != (context->flags & CANNM_REQUEST_MASK)) {
        context->state = NM_STATE_NORMAL_OPERATION;
#ifdef USE_NM /* @SWS_CanNm_00166 */
        Nm_StateChangeNotification(config->nmNetworkHandle, NM_STATE_REPEAT_MESSAGE,
                                   NM_STATE_NORMAL_OPERATION);
#endif
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED /* @SWS_CanNm_00149, @SWS_CanNm_00150 */
        nmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime);
#endif
        ASLOG(CANNM, ("%d: Enter Normal Operation Mode, flags %02X\n", config->nmNetworkHandle,
                      context->flags));
      } else {
        context->state = NM_STATE_READY_SLEEP;
#ifdef USE_NM /* @SWS_CanNm_00166 */
        Nm_StateChangeNotification(config->nmNetworkHandle, NM_STATE_REPEAT_MESSAGE,
                                   NM_STATE_READY_SLEEP);
#endif
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
  if (0u != (context->flags & (CANNM_REQUEST_MASK | CANNM_PASSIVE_STARTUP_REQUEST_MASK))) {
#ifdef USE_NM
    Nm_NetworkMode(config->nmNetworkHandle);
    Nm_StateChangeNotification(config->nmNetworkHandle, NM_STATE_BUS_SLEEP,
                               NM_STATE_REPEAT_MESSAGE); /* @SWS_CanNm_00166 */
#endif
    nmEnterNetworkMode(context, config);
  } else {
    if (0u != (context->flags & CANNM_RX_INDICATION_REQUEST_MASK)) {
      /* @SWS_CanNm_00127 */
      nmEnterCritical();
      context->flags &= ~(CANNM_RX_INDICATION_REQUEST_MASK | CANNM_RX_REPEAT_MESSAGE_BIT_SET_MASK);
      nmExitCritical();
#ifdef USE_NM
      Nm_NetworkStartIndication(config->nmNetworkHandle);
#endif
    }
  }
}

static void nmPrepareBusSleepMain(CanNm_ChannelContextType *context,
                                  const CanNm_ChannelConfigType *config) {
  if (0u != (context->flags & (CANNM_REQUEST_MASK | CANNM_PASSIVE_STARTUP_REQUEST_MASK |
                               CANNM_RX_INDICATION_REQUEST_MASK))) {
    if (0u != (context->flags & CANNM_RX_INDICATION_REQUEST_MASK)) {
      nmEnterCritical();
      context->flags &= ~(CANNM_RX_INDICATION_REQUEST_MASK | CANNM_RX_REPEAT_MESSAGE_BIT_SET_MASK);
      nmExitCritical();
    }
#ifdef USE_NM
    Nm_NetworkMode(config->nmNetworkHandle);
    Nm_StateChangeNotification(config->nmNetworkHandle, NM_STATE_PREPARE_BUS_SLEEP,
                               NM_STATE_REPEAT_MESSAGE); /* @SWS_CanNm_00166 */
#endif
    nmEnterNetworkMode(context, config);
  } else {
    if (nmIsAlarmStarted(WaitBusSleep)) {
      nmSingalAlarm(WaitBusSleep);
      if (nmIsAlarmTimeout(WaitBusSleep)) {
        context->state = NM_STATE_BUS_SLEEP;
#ifdef USE_NM /* @SWS_CanNm_00166 */
        Nm_StateChangeNotification(config->nmNetworkHandle, NM_STATE_PREPARE_BUS_SLEEP,
                                   NM_STATE_BUS_SLEEP);
#endif
        ASLOG(CANNM,
              ("%d: Enter Bus Sleep Mode, flags %02X\n", config->nmNetworkHandle, context->flags));
#ifdef USE_NM
        Nm_BusSleepMode(config->nmNetworkHandle);
#endif
      }
    } else {
      nmSetAlarm(WaitBusSleep, config->WaitBusSleepTime);
    }
  }
}

/* this only called form ReadySleep or NormalOperationMode */
static void nmEnterRepeatMessageMode(CanNm_ChannelContextType *context,
                                     const CanNm_ChannelConfigType *config) {
  context->state = NM_STATE_REPEAT_MESSAGE;
  context->TxCounter = 0;
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
  if (0u != (context->flags & CANNM_REMOTE_SLEEP_IND_MASK)) {
    /* @SWS_CanNm_00152 */
    Nm_RemoteSleepCancellation(config->nmNetworkHandle);
  }
#endif

  if (0u != (context->flags & CANNM_REPEAT_MESSAGE_REQUEST_MASK)) {
    /* @SWS_CanNm_00121, @SWS_CanNm_00113 */
    if (TRUE == config->NodeDetectionEnabled) {
      if (config->PduCbvPosition < CANNM_PDU_OFF) {
        context->data[config->PduCbvPosition] |= CANNM_CBV_REPEAT_MESSAGE_REQUEST;
      }
    }
  }

#ifdef CANNM_REQUEST_BIT_AUTO_SET
  if (0u != (context->flags & CANNM_REQUEST_MASK)) {
    if (config->PduCbvPosition < CANNM_PDU_OFF) {
      context->data[config->PduCbvPosition] |= CANNM_CBV_REPEAT_MESSAGE_REQUEST;
    }
  }
#endif

  nmEnterCritical();
  context->flags &= ~(CANNM_REPEAT_MESSAGE_REQUEST_MASK | CANNM_REMOTE_SLEEP_IND_MASK |
                      CANNM_RX_REPEAT_MESSAGE_BIT_SET_MASK);
  nmExitCritical();
  ASLOG(CANNM,
        ("%d: Enter Repeat Message Mode, flags %02X\n", config->nmNetworkHandle, context->flags));
#ifdef CANNM_PASSIVE_STARTUP_REPEAT_ENABLED
  nmSendMessage(context, config, config->ImmediateNmCycleTime);
#else
  /* @SWS_CanNm_00005 */
  nmSetAlarm(Tx, config->MsgCycleOffset);
#endif
  nmSetAlarm(RepeatMessage, config->RepeatMessageTime);
}

static void nmReadySleepMain(CanNm_ChannelContextType *context,
                             const CanNm_ChannelConfigType *config) {
  if (0u != (context->flags & CANNM_RX_INDICATION_REQUEST_MASK)) {
    nmEnterCritical();
    context->flags &= ~CANNM_RX_INDICATION_REQUEST_MASK;
    nmExitCritical();
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
    if (0u != (context->flags & CANNM_REMOTE_SLEEP_IND_MASK)) {
      nmEnterCritical();
      context->flags &= ~CANNM_REMOTE_SLEEP_IND_MASK;
      nmExitCritical();
#ifdef USE_NM
      /* @SWS_CanNm_00151 */
      Nm_RemoteSleepCancellation(config->nmNetworkHandle);
#endif
    }
#endif
  }

  if (0u != (context->flags &
             (CANNM_REPEAT_MESSAGE_REQUEST_MASK | CANNM_RX_REPEAT_MESSAGE_BIT_SET_MASK))) {
#ifdef USE_NM /* @SWS_CanNm_00166 */
    Nm_StateChangeNotification(config->nmNetworkHandle, NM_STATE_READY_SLEEP,
                               NM_STATE_REPEAT_MESSAGE);
#endif
    nmEnterRepeatMessageMode(context, config);
  } else if (0u != (context->flags & CANNM_REQUEST_MASK)) {
    context->state = NM_STATE_NORMAL_OPERATION;
#ifdef USE_NM /* @SWS_CanNm_00166 */
    Nm_StateChangeNotification(config->nmNetworkHandle, NM_STATE_READY_SLEEP,
                               NM_STATE_NORMAL_OPERATION);
#endif
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
    if (0u != (context->flags & CANNM_DISABLE_COMMUNICATION_REQUEST_MASK)) {
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
#ifdef USE_NM /* @SWS_CanNm_00166 */
        Nm_StateChangeNotification(config->nmNetworkHandle, NM_STATE_READY_SLEEP,
                                   NM_STATE_PREPARE_BUS_SLEEP);
#endif
#ifdef USE_NM
        Nm_PrepareBusSleepMode(config->nmNetworkHandle);
#endif
        ASLOG(CANNM, ("%d: Enter Prepare Bus Sleep Mode, flags %02X\n", config->nmNetworkHandle,
                      context->flags));
      } else {
        nmSetAlarm(NMTimeout, config->NmTimeoutTime);
      }
    }
  } else {
    if (0u == (context->flags & CANNM_DISABLE_COMMUNICATION_REQUEST_MASK)) {
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
#ifdef USE_NM /* @SWS_CanNm_00066 */
      Nm_TxTimeoutException(config->nmNetworkHandle);
#endif
#if defined(CANNM_GLOBAL_PN_SUPPORT) && defined(USE_CANSM) /* SWS_CanNm_00446 */
      CanSM_TxTimeoutException((NetworkHandleType)(config - CANNM_CONFIG->ChannelConfigs));
#endif
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

  if (0u != (context->flags & CANNM_RX_INDICATION_REQUEST_MASK)) {
    nmEnterCritical();
    context->flags &= ~CANNM_RX_INDICATION_REQUEST_MASK;
    nmExitCritical();
    if (config->MsgReducedTime > 0u) {
      nmSetAlarm(Tx, config->MsgReducedTime);
    }
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
    nmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime);
    if (0u != (context->flags & CANNM_REMOTE_SLEEP_IND_MASK)) {
      nmEnterCritical();
      context->flags &= ~CANNM_REMOTE_SLEEP_IND_MASK;
      nmExitCritical();
#ifdef USE_NM
      /* @SWS_CanNm_00151 */
      Nm_RemoteSleepCancellation(config->nmNetworkHandle);
#endif
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
      if (0u == (context->flags & CANNM_DISABLE_COMMUNICATION_REQUEST_MASK)) {
#ifdef USE_NM
        /* @SWS_CanNm_00175 */
        Nm_RemoteSleepIndication(config->nmNetworkHandle);
#endif
        nmEnterCritical();
        context->flags |= CANNM_REMOTE_SLEEP_IND_MASK;
        nmExitCritical();
      }
    }
  }
#endif

  if (0u != (context->flags &
             (CANNM_REPEAT_MESSAGE_REQUEST_MASK | CANNM_RX_REPEAT_MESSAGE_BIT_SET_MASK))) {
#ifdef USE_NM /* @SWS_CanNm_00166 */
    Nm_StateChangeNotification(config->nmNetworkHandle, NM_STATE_NORMAL_OPERATION,
                               NM_STATE_REPEAT_MESSAGE);
#endif
    nmEnterRepeatMessageMode(context, config);
  } else if (0u == (context->flags & CANNM_REQUEST_MASK)) {
    context->state = NM_STATE_READY_SLEEP;
#ifdef USE_NM /* @SWS_CanNm_00166 */
    Nm_StateChangeNotification(config->nmNetworkHandle, NM_STATE_NORMAL_OPERATION,
                               NM_STATE_READY_SLEEP);
#endif
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
  if (0u != (context->flags & CANNM_PN_ENABLED_MASK)) {
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
  uint16_t i;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;

#ifdef CANNM_USE_PB_CONFIG
  if (NULL != cannmConfigPtr) {
    CANNM_CONFIG = cannmConfigPtr;
  } else {
    CANNM_CONFIG = &CanNm_Config;
  }
#else
  (void)cannmConfigPtr;
#endif

  for (i = 0u; i < CANNM_CONFIG->numOfChannels; i++) {
    context = &CANNM_CONFIG->ChannelContexts[i];
    config = &CANNM_CONFIG->ChannelConfigs[i];
    context->state = NM_STATE_BUS_SLEEP; /* @SWS_CanNm_00141 */
    context->TxCounter = 0u;
    context->flags = 0u; /* @SWS_CanNm_00403 */
    (void)memset(context->rxPdu, 0xFF, sizeof(context->rxPdu));
    (void)memset(context->data, 0xFF, sizeof(context->data)); /* @SWS_CanNm_00025 */
    if (config->PduNidPosition < CANNM_PDU_OFF) {
      context->data[config->PduNidPosition] = config->NodeId; /* @SWS_CanNm_00013 */
    }
    if (config->PduCbvPosition < CANNM_PDU_OFF) {
      context->data[config->PduCbvPosition] = 0x00; /* @SWS_CanNm_00085 */
#ifdef CANNM_GLOBAL_PN_SUPPORT
      if (config->PnEnabled) { /* @SWS_CanNm_00413 */
        context->data[config->PduCbvPosition] |= CANNM_CBV_PARTIAL_NETWORK_INFORMATION;
        (void)memcpy(&context->data[config->PnInfoOffset], config->PnFilterMaskByte,
                     config->PnInfoLength);
      }
#endif
    }
  }
}

Std_ReturnType CanNm_PassiveStartUp(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x01, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->numOfChannels, 0x01, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  switch ((context->state)) { /* @SWS_CanNm_00128 */
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

  return ret;
}

Std_ReturnType CanNm_NetworkRequest(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;
  DET_VALIDATE(NULL != CANNM_CONFIG, 0x02, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->numOfChannels, 0x02, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  nmEnterCritical();
  context->flags |= CANNM_REQUEST_MASK;
  nmExitCritical();

  return ret;
}

Std_ReturnType CanNm_NetworkRelease(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x03, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->numOfChannels, 0x03, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  nmEnterCritical();
  context->flags &= ~CANNM_REQUEST_MASK;
  nmExitCritical();

  return ret;
}

Std_ReturnType CanNm_RepeatMessageRequest(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x08, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->numOfChannels, 0x08, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);
  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  switch (context->state) {
  case NM_STATE_READY_SLEEP:
  case NM_STATE_NORMAL_OPERATION:
    nmEnterCritical();
    context->flags |= CANNM_REPEAT_MESSAGE_REQUEST_MASK;
    nmExitCritical();
    break;
  default:
    /* @SWS_CanNm_00137 */
    ret = E_NOT_OK;
    break;
  }

  return ret;
}

Std_ReturnType CanNm_DisableCommunication(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x0C, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->numOfChannels, 0x0C, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  /* @SWS_CanNm_00170 */
  nmEnterCritical();
  context->flags |= CANNM_DISABLE_COMMUNICATION_REQUEST_MASK;
  nmExitCritical();
  /* could see codes that check CANNM_DISABLE_COMMUNICATION_REQUEST_MASK in the MainFunction,
   * That is because that Alarm operation is not atomic, so the codes that added to check
   * CANNM_DISABLE_COMMUNICATION_REQUEST_MASK is a workaroud and which is not good but it
   * satisfied the requirement */
  nmCancelAlarm(Tx);        /* @SWS_CanNm_00173 */
  nmCancelAlarm(NMTimeout); /* @SWS_CanNm_00174 */
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
  nmCancelAlarm(RemoteSleepInd); /* @SWS_CanNm_00175 */
#endif

  return ret;
}

Std_ReturnType CanNm_EnableCommunication(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x0D, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->numOfChannels, 0x0D, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
  nmEnterCritical();
  context->flags &= ~CANNM_DISABLE_COMMUNICATION_REQUEST_MASK;
  nmEnterCritical();
  nmSetAlarm(Tx, 0u);                           /* @SWS_CanNm_00178 */
  nmSetAlarm(NMTimeout, config->NmTimeoutTime); /* @SWS_CanNm_00179 */
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
  nmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime); /* @SWS_CanNm_00180 */
#endif

  return ret;
}

void CanNm_TxConfirmation(PduIdType TxPduId, Std_ReturnType result) {
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x40, CANNM_E_UNINIT, return);
  DET_VALIDATE(TxPduId < CANNM_CONFIG->numOfChannels, 0x40, CANNM_E_INVALID_CHANNEL, return);

  context = &CANNM_CONFIG->ChannelContexts[TxPduId];
  config = &CANNM_CONFIG->ChannelConfigs[TxPduId];
#ifdef CANNM_COM_USER_DATA_SUPPORT
  PduR_CanNmTxConfirmation(config->UserDataTxPdu, result);
#endif
  if (E_OK == result) {
    if (context->TxCounter < 0xFFu) {
      context->TxCounter++;
    }
    nmCancelAlarm(TxTimeout); /* @SWS_CanNm_00065 */

    if ((NM_STATE_NORMAL_OPERATION == context->state) ||
        (NM_STATE_REPEAT_MESSAGE == context->state) || (NM_STATE_READY_SLEEP == context->state)) {
      nmSetAlarm(NMTimeout, config->NmTimeoutTime);
    }
  } else {
#ifdef USE_NM /* @SWS_CanNm_00066 */
    Nm_TxTimeoutException(config->nmNetworkHandle);
#endif
  }
}

void CanNm_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;
  uint16_t flags = CANNM_RX_INDICATION_REQUEST_MASK | CANNM_NM_PDU_RECEIVED_MASK;
  Std_ReturnType ret = E_NOT_OK;
#ifdef CANNM_CAR_WAKEUP_SUPPORT
  const Can_HwType *Mailbox = (const Can_HwType *)PduInfoPtr->MetaDataPtr;
#endif

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x42, CANNM_E_UNINIT, return);
  DET_VALIDATE(RxPduId < CANNM_CONFIG->numOfChannels, 0x42, CANNM_E_INVALID_CHANNEL, return);
  DET_VALIDATE((NULL != PduInfoPtr) && (NULL != PduInfoPtr->SduDataPtr), 0x42,
               CANNM_E_PARAM_POINTER, return);

  context = &CANNM_CONFIG->ChannelContexts[RxPduId];
  config = &CANNM_CONFIG->ChannelConfigs[RxPduId];
#ifdef CANNM_GLOBAL_PN_SUPPORT
  ret = nmRxFilter(context, config, PduInfoPtr);
#else
  ret = E_OK;
#endif

  if (E_OK == ret) {
    /* @SWS_CanNm_00035 */
    (void)memcpy(context->rxPdu, PduInfoPtr->SduDataPtr, sizeof(context->rxPdu));
    if (config->PduCbvPosition < CANNM_PDU_OFF) {
      if (0u != (context->rxPdu[config->PduCbvPosition] & CANNM_CBV_REPEAT_MESSAGE_REQUEST)) {
        if (TRUE == config->NodeDetectionEnabled) {
          /* @SWS_CanNm_00119, @SWS_CanNm_00111 */
          if ((NM_STATE_NORMAL_OPERATION == context->state) ||
              (NM_STATE_READY_SLEEP == context->state)) {
            flags |= CANNM_RX_REPEAT_MESSAGE_BIT_SET_MASK;
          }
        }

        /* @SWS_CanNm_00014 */
        if ((TRUE == config->RepeatMsgIndEnabled) && (TRUE == config->NodeDetectionEnabled)) {
#ifdef USE_NM
          Nm_RepeatMessageIndication(config->nmNetworkHandle);
#endif
        }
      }
#ifdef CANNM_COORDINATOR_SYNC_SUPPORT
      if ((NM_STATE_NORMAL_OPERATION == context->state) ||
          (NM_STATE_REPEAT_MESSAGE == context->state) || (NM_STATE_READY_SLEEP == context->state)) {
        if (0u != (context->rxPdu[config->PduCbvPosition] & CANNM_CBV_NM_COORDINATOR_SLEEP)) {
          /* @SWS_CanNm_00341 */
          if (0u == (context->flags & CANNM_COORDINATOR_SLEEP_SYNC_MASK)) {
            flags |= CANNM_COORDINATOR_SLEEP_SYNC_MASK;
#ifdef USE_NM
            Nm_CoordReadyToSleepIndication(config->nmNetworkHandle);
#endif
          }
        } else if (0u != (context->flags & CANNM_COORDINATOR_SLEEP_SYNC_MASK)) {
#ifdef USE_NM /* @SWS_CanNm_00348 */
          Nm_CoordReadyToSleepCancellation(config->nmNetworkHandle);
#endif
          nmEnterCritical();
          context->flags &= ~CANNM_COORDINATOR_SLEEP_SYNC_MASK;
          nmExitCritical();
        } else {
          /* do nothing */
        }
      }
#endif
#ifdef CANNM_CAR_WAKEUP_SUPPORT
      CanNm_CarWakeupProcess(config, context, Mailbox->CanId & config->NodeMask);
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

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x07, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->numOfChannels, 0x07, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);
  DET_VALIDATE(NULL != nmNodeIdPtr, 0x07, CANNM_E_PARAM_POINTER, return E_NOT_OK);

  config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
  *nmNodeIdPtr = config->NodeId;

  return ret;
}

Std_ReturnType CanNm_GetNodeIdentifier(NetworkHandleType nmChannelHandle, uint8_t *nmNodeIdPtr) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;
  uint8_t nodeId = CANNM_INVALID_NODE_ID;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x06, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->numOfChannels, 0x06, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);
  DET_VALIDATE(NULL != nmNodeIdPtr, 0x06, CANNM_E_PARAM_POINTER, return E_NOT_OK);

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

  return ret;
}

#ifndef CANNM_COM_USER_DATA_SUPPORT
Std_ReturnType CanNm_SetUserData(NetworkHandleType nmChannelHandle, const uint8_t *nmUserDataPtr) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x04, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->numOfChannels, 0x04, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);
  DET_VALIDATE(NULL != nmUserDataPtr, 0x04, CANNM_E_PARAM_POINTER, return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
  CanNm_SetUserDataImpl(context, config, nmUserDataPtr);

  return ret;
}

Std_ReturnType CanNm_GetUserData(NetworkHandleType nmChannelHandle, uint8_t *nmUserDataPtr) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;
  uint8_t notUserDataMask = 0;
  uint16_t i;
  uint16_t j = 0;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x05, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->numOfChannels, 0x05, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);
  DET_VALIDATE(NULL != nmUserDataPtr, 0x05, CANNM_E_PARAM_POINTER, return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
  if (config->PduNidPosition != CANNM_PDU_OFF) {
    notUserDataMask |= (1u << config->PduNidPosition);
  }
  if (config->PduCbvPosition != CANNM_PDU_OFF) {
    notUserDataMask |= (1u << config->PduCbvPosition);
  }
#ifdef CANNM_GLOBAL_PN_SUPPORT
  if (config->PnEnabled) {
    for (i = 0u; i < config->PnInfoLength; i++) {
      notUserDataMask |= (1u << (config->PnInfoOffset + i));
    }
  }
#endif
  for (i = 0; i < 8u; i++) {
    if (0u == (notUserDataMask & (1u << i))) {
      nmUserDataPtr[j] = context->data[i];
      j++;
    }
  }

  return ret;
}
#endif /* CANNM_COM_USER_DATA_SUPPORT */

Std_ReturnType CanNm_GetPduData(NetworkHandleType nmChannelHandle, uint8_t *nmPduDataPtr) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x0A, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->numOfChannels, 0x0A, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);
  DET_VALIDATE(NULL != nmPduDataPtr, 0x0A, CANNM_E_PARAM_POINTER, return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  if (0u != (context->flags & CANNM_NM_PDU_RECEIVED_MASK)) {
    (void)memcpy(nmPduDataPtr, &context->rxPdu, 8);
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType CanNm_GetState(NetworkHandleType nmChannelHandle, Nm_StateType *nmStatePtr,
                              Nm_ModeType *nmModePtr) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x0B, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->numOfChannels, 0x0B, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);
  DET_VALIDATE((NULL != nmStatePtr) && (NULL != nmModePtr), 0x0B, CANNM_E_PARAM_POINTER,
               return E_NOT_OK);

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

  return ret;
}

#ifdef CANNM_GLOBAL_PN_SUPPORT
void CanNm_ConfirmPnAvailability(NetworkHandleType nmChannelHandle) {
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x16, CANNM_E_UNINIT, return);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->numOfChannels, 0x16, CANNM_E_INVALID_CHANNEL,
               return);
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
#endif

#ifdef CANNM_COORDINATOR_SYNC_SUPPORT
Std_ReturnType CanNm_SetSleepReadyBit(NetworkHandleType nmChannelHandle, boolean nmSleepReadyBit) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x17, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->numOfChannels, 0x17, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
  nmEnterCritical();
  if (config->PduCbvPosition < CANNM_PDU_OFF) {
    /* @SWS_CanNm_00342 */
    if (TRUE == nmSleepReadyBit) {
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

  return ret;
}
#endif

#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
Std_ReturnType CanNm_CheckRemoteSleepIndication(NetworkHandleType nmChannelHandle,
                                                boolean *nmRemoteSleepIndPtr) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0xD0, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->numOfChannels, 0xD0, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);
  DET_VALIDATE(NULL != nmRemoteSleepIndPtr, 0xD0, CANNM_E_PARAM_POINTER, return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  switch (context->state) {
  case NM_STATE_NORMAL_OPERATION:
    if (0u != (context->flags & CANNM_REMOTE_SLEEP_IND_MASK)) {
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

  return ret;
}
#endif

void CanNm_MainFunction(void) {
  uint16_t i;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x13, CANNM_E_UNINIT, return);

  for (i = 0u; i < CANNM_CONFIG->numOfChannels; i++) {
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

void CanNm_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0xf1, CANNM_E_PARAM_POINTER, return);

  versionInfo->vendorID = STD_VENDOR_ID_AS;
  versionInfo->moduleID = MODULE_ID_CANNM;
  versionInfo->sw_major_version = 4;
  versionInfo->sw_minor_version = 0;
  versionInfo->sw_patch_version = 1;
}

/** @brief release notes
 * - 4.0.1: Replace accidental nmEnterCritical() with nmExitCritical() in RepeatMessageRequest/DisableCommunication.
 */
