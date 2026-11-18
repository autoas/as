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
#define CanNmEnterCritical() EnterCritical()
#define CanNmExitCritical() ExitCritical();
#else
#define CanNmEnterCritical()
#define CanNmExitCritical()
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
#define CanNmSetAlarm(Timer, v)                                                                    \
  do {                                                                                             \
    Std_TimerInit(&context->Alarm.Timer, ((v) * 1000u * CANNM_MAIN_FUNCTION_PERIOD));              \
  } while (0)
#define CanNmSignalAlarm(Timer)
#define CanNmIsAlarmTimeout(Timer) Std_IsTimerTimeout(&context->Alarm.Timer)
#define CanNmIsAlarmStarted(Timer) Std_IsTimerStarted(&context->Alarm.Timer)
#define CanNmCancelAlarm(Timer) Std_TimerStop(&context->Alarm.Timer)
#else
/* Alarm Management */
#define CanNmSetAlarm(Timer, v)                                                                    \
  do {                                                                                             \
    context->Alarm.Timer = 1u + (v);                                                               \
  } while (0)

/* signal the alarm to process one step/tick forward */
#define CanNmSignalAlarm(Timer)                                                                    \
  do {                                                                                             \
    if (context->Alarm.Timer > 1u) {                                                               \
      (context->Alarm.Timer)--;                                                                    \
    }                                                                                              \
  } while (0)

#define CanNmIsAlarmTimeout(Timer) (1u == context->Alarm.Timer)

#define CanNmIsAlarmStarted(Timer) (0u != context->Alarm.Timer)

#define CanNmCancelAlarm(Timer)                                                                    \
  do {                                                                                             \
    context->Alarm.Timer = 0;                                                                      \
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
      context->Data[i] = nmUserDataPtr[j];
      j++;
    }
  }
}

static void CanNm_SendMessage(CanNm_ChannelContextType *context,
                              const CanNm_ChannelConfigType *config, uint16_t reload) {
  Std_ReturnType ret = E_OK;
  PduInfoType pdu;
#ifdef CANNM_COM_USER_DATA_SUPPORT
  uint8_t userData[8] = {0, 0, 0, 0, 0, 0, 0, 0};
#endif

  if (0u == (context->Flags & CANNM_DISABLE_COMMUNICATION_REQUEST_MASK)) {
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

    pdu.SduLength = sizeof(context->Data);
    pdu.SduDataPtr = context->Data;
    pdu.MetaDataPtr = NULL;
    ret = CanIf_Transmit(config->TxPdu, &pdu);
    if (E_OK == ret) { /* @SWS_CanNm_00064 */
      CanNmSetAlarm(TxTimeout, config->MsgTimeoutTime);
      if (reload > 0u) {
        CanNmSetAlarm(Tx, reload);
      }
    } else {
      /* @SWS_CanNm_00335 */
      CanNmSetAlarm(Tx, 0u);
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
          (context->RxPdu[config->CarWakeUpBytePosition] & (1u << config->CarWakeUpBitPosition))) {
#ifdef USE_NM
        Nm_CarWakeUpIndication(config->NetworkHandle);
#endif
      }
    }
  }
}
#endif

/* This API called from state BusSleep or PrePreareBusSleep */
static void CanNm_EnterNetworkMode(CanNm_ChannelContextType *context,
                                   const CanNm_ChannelConfigType *config) {
  context->State = NM_STATE_REPEAT_MESSAGE;
  context->TxCounter = 0;

  if (0u != (context->Flags & CANNM_REQUEST_MASK)) {
    if ((config->ActiveWakeupBitEnabled) && (config->PduCbvPosition < CANNM_PDU_OFF)) {
      context->Data[config->PduCbvPosition] |= CANNM_CBV_ACTIVE_WAKEUP; /* @SWS_CanNm_00401 */
    }
#ifdef CANNM_REQUEST_BIT_AUTO_SET
    if (config->PduCbvPosition < CANNM_PDU_OFF) {
      context->Data[config->PduCbvPosition] |= CANNM_CBV_REPEAT_MESSAGE_REQUEST;
    }
#endif
  }

  if (
#ifndef CANNM_PASSIVE_STARTUP_REPEAT_ENABLED
    (context->Flags & CANNM_REQUEST_MASK) &&
#endif
    (config->ImmediateNmTransmissions > 0u)) {
    /* @SWS_CanNm_00334 */
    CanNm_SendMessage(context, config, config->ImmediateNmCycleTime);
  } else {
    /* SWS_CanNm_00005 */
    CanNmSetAlarm(Tx, config->MsgCycleOffset);
  }

  CanNmSetAlarm(NmTimeout, config->NmTimeoutTime);
  CanNmSetAlarm(RepeatMessage, config->RepeatMessageTime);

  CanNmEnterCritical();
  context->Flags &= ~(CANNM_NM_PDU_RECEIVED_MASK | CANNM_PASSIVE_STARTUP_REQUEST_MASK |
                      CANNM_REMOTE_SLEEP_IND_MASK | CANNM_RX_INDICATION_REQUEST_MASK |
                      CANNM_COORDINATOR_SLEEP_SYNC_MASK | CANNM_PN_ENABLED_MASK |
                      CANNM_RX_REPEAT_MESSAGE_BIT_SET_MASK);
  CanNmExitCritical();
  ASLOG(CANNM,
        ("%d: Enter Repeat Message Mode, flags %02X\n", config->NetworkHandle, context->Flags));
}

static void CanNm_RepeatMessageMain(CanNm_ChannelContextType *context,
                                    const CanNm_ChannelConfigType *config) {
  uint16_t reload = config->MsgCycleTime;

  if (0u != (context->Flags & CANNM_RX_INDICATION_REQUEST_MASK)) {
    CanNmEnterCritical();
    context->Flags &= ~(CANNM_RX_INDICATION_REQUEST_MASK | CANNM_RX_REPEAT_MESSAGE_BIT_SET_MASK);
    CanNmExitCritical();
  }

#ifdef CANNM_REQUEST_BIT_AUTO_SET /* auto set repeat message bit if network requested, robust! */
  if (0u != (context->Flags & CANNM_REQUEST_MASK)) {
    if (config->PduCbvPosition < CANNM_PDU_OFF) {
      context->Data[config->PduCbvPosition] |= CANNM_CBV_REPEAT_MESSAGE_REQUEST;
    }
  }
#endif

  if (
#ifndef CANNM_PASSIVE_STARTUP_REPEAT_ENABLED
    (0u != (context->Flags & CANNM_REQUEST_MASK)) &&
#endif
    (config->ImmediateNmTransmissions > 0u)) {
    /* @SWS_CanNm_00334 */
    if (context->TxCounter < (config->ImmediateNmTransmissions - 1u)) {
      reload = config->ImmediateNmCycleTime;
    }
  }

  if (CanNmIsAlarmStarted(TxTimeout)) {
    CanNmSignalAlarm(TxTimeout);
    if (CanNmIsAlarmTimeout(TxTimeout)) {
      CanNmCancelAlarm(TxTimeout);
#ifdef USE_NM /* @SWS_CanNm_00066 */
      Nm_TxTimeoutException(config->NetworkHandle);
#endif
#if defined(CANNM_GLOBAL_PN_SUPPORT) && defined(USE_CANSM) /* SWS_CanNm_00446 */
      CanSM_TxTimeoutException((NetworkHandleType)(config - CANNM_CONFIG->ChannelConfigs));
#endif
    }
  }

  if (CanNmIsAlarmStarted(NmTimeout)) {
    CanNmSignalAlarm(NmTimeout);
    if (CanNmIsAlarmTimeout(NmTimeout)) {
      CanNmSetAlarm(NmTimeout, config->NmTimeoutTime);
    }
  } else {
    CanNmSetAlarm(NmTimeout, config->NmTimeoutTime);
  }

  if (CanNmIsAlarmStarted(Tx)) {
    CanNmSignalAlarm(Tx);
    if (CanNmIsAlarmTimeout(Tx)) {
      CanNm_SendMessage(context, config, reload);
    }
  } else {
    CanNmSetAlarm(Tx, reload);
  }

  if (CanNmIsAlarmStarted(RepeatMessage)) {
    CanNmSignalAlarm(RepeatMessage);
    if (CanNmIsAlarmTimeout(RepeatMessage)) {
      if (0u != (context->Flags & CANNM_REQUEST_MASK)) {
        context->State = NM_STATE_NORMAL_OPERATION;
#ifdef USE_NM /* @SWS_CanNm_00166 */
        Nm_StateChangeNotification(config->NetworkHandle, NM_STATE_REPEAT_MESSAGE,
                                   NM_STATE_NORMAL_OPERATION);
#endif
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED /* @SWS_CanNm_00149, @SWS_CanNm_00150 */
        CanNmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime);
#endif
        ASLOG(CANNM, ("%d: Enter Normal Operation Mode, flags %02X\n", config->NetworkHandle,
                      context->Flags));
      } else {
        context->State = NM_STATE_READY_SLEEP;
#ifdef USE_NM /* @SWS_CanNm_00166 */
        Nm_StateChangeNotification(config->NetworkHandle, NM_STATE_REPEAT_MESSAGE,
                                   NM_STATE_READY_SLEEP);
#endif
        ASLOG(CANNM,
              ("%d: Enter Ready Sleep Mode, flags %02X\n", config->NetworkHandle, context->Flags));
      }
      if (config->PduCbvPosition < CANNM_PDU_OFF) {
        /* @SWS_CanNm_00107 */
        CanNmEnterCritical();
        context->Data[config->PduCbvPosition] &= ~CANNM_CBV_REPEAT_MESSAGE_REQUEST;
        CanNmExitCritical();
      }
    }
  } else {
    CanNmSetAlarm(RepeatMessage, config->RepeatMessageTime);
  }
}

static void CanNm_BusSleepMain(CanNm_ChannelContextType *context,
                               const CanNm_ChannelConfigType *config) {
  if (0u != (context->Flags & (CANNM_REQUEST_MASK | CANNM_PASSIVE_STARTUP_REQUEST_MASK))) {
#ifdef USE_NM
    Nm_NetworkMode(config->NetworkHandle);
    Nm_StateChangeNotification(config->NetworkHandle, NM_STATE_BUS_SLEEP,
                               NM_STATE_REPEAT_MESSAGE); /* @SWS_CanNm_00166 */
#endif
    CanNm_EnterNetworkMode(context, config);
  } else {
    if (0u != (context->Flags & CANNM_RX_INDICATION_REQUEST_MASK)) {
      /* @SWS_CanNm_00127 */
      CanNmEnterCritical();
      context->Flags &= ~(CANNM_RX_INDICATION_REQUEST_MASK | CANNM_RX_REPEAT_MESSAGE_BIT_SET_MASK);
      CanNmExitCritical();
#ifdef USE_NM
      Nm_NetworkStartIndication(config->NetworkHandle);
#endif
    }
  }
}

static void CanNm_PrepareBusSleepMain(CanNm_ChannelContextType *context,
                                      const CanNm_ChannelConfigType *config) {
  if (0u != (context->Flags & (CANNM_REQUEST_MASK | CANNM_PASSIVE_STARTUP_REQUEST_MASK |
                               CANNM_RX_INDICATION_REQUEST_MASK))) {
    if (0u != (context->Flags & CANNM_RX_INDICATION_REQUEST_MASK)) {
      CanNmEnterCritical();
      context->Flags &= ~(CANNM_RX_INDICATION_REQUEST_MASK | CANNM_RX_REPEAT_MESSAGE_BIT_SET_MASK);
      CanNmExitCritical();
    }
#ifdef USE_NM
    Nm_NetworkMode(config->NetworkHandle);
    Nm_StateChangeNotification(config->NetworkHandle, NM_STATE_PREPARE_BUS_SLEEP,
                               NM_STATE_REPEAT_MESSAGE); /* @SWS_CanNm_00166 */
#endif
    CanNm_EnterNetworkMode(context, config);
  } else {
    if (CanNmIsAlarmStarted(WaitBusSleep)) {
      CanNmSignalAlarm(WaitBusSleep);
      if (CanNmIsAlarmTimeout(WaitBusSleep)) {
        context->State = NM_STATE_BUS_SLEEP;
#ifdef USE_NM /* @SWS_CanNm_00166 */
        Nm_StateChangeNotification(config->NetworkHandle, NM_STATE_PREPARE_BUS_SLEEP,
                                   NM_STATE_BUS_SLEEP);
#endif
        ASLOG(CANNM,
              ("%d: Enter Bus Sleep Mode, flags %02X\n", config->NetworkHandle, context->Flags));
#ifdef USE_NM
        Nm_BusSleepMode(config->NetworkHandle);
#endif
      }
    } else {
      CanNmSetAlarm(WaitBusSleep, config->WaitBusSleepTime);
    }
  }
}

/* this only called form ReadySleep or NormalOperationMode */
static void CanNm_EnterRepeatMessageMode(CanNm_ChannelContextType *context,
                                         const CanNm_ChannelConfigType *config) {
  context->State = NM_STATE_REPEAT_MESSAGE;
  context->TxCounter = 0;
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
  if (0u != (context->Flags & CANNM_REMOTE_SLEEP_IND_MASK)) {
    /* @SWS_CanNm_00152 */
    Nm_RemoteSleepCancellation(config->NetworkHandle);
  }
#endif

  if (0u != (context->Flags & CANNM_REPEAT_MESSAGE_REQUEST_MASK)) {
    /* @SWS_CanNm_00121, @SWS_CanNm_00113 */
    if (TRUE == config->NodeDetectionEnabled) {
      if (config->PduCbvPosition < CANNM_PDU_OFF) {
        context->Data[config->PduCbvPosition] |= CANNM_CBV_REPEAT_MESSAGE_REQUEST;
      }
    }
  }

#ifdef CANNM_REQUEST_BIT_AUTO_SET
  if (0u != (context->Flags & CANNM_REQUEST_MASK)) {
    if (config->PduCbvPosition < CANNM_PDU_OFF) {
      context->Data[config->PduCbvPosition] |= CANNM_CBV_REPEAT_MESSAGE_REQUEST;
    }
  }
#endif

  CanNmEnterCritical();
  context->Flags &= ~(CANNM_REPEAT_MESSAGE_REQUEST_MASK | CANNM_REMOTE_SLEEP_IND_MASK |
                      CANNM_RX_REPEAT_MESSAGE_BIT_SET_MASK);
  CanNmExitCritical();
  ASLOG(CANNM,
        ("%d: Enter Repeat Message Mode, flags %02X\n", config->NetworkHandle, context->Flags));
#ifdef CANNM_PASSIVE_STARTUP_REPEAT_ENABLED
  CanNm_SendMessage(context, config, config->ImmediateNmCycleTime);
#else
  /* @SWS_CanNm_00005 */
  CanNmSetAlarm(Tx, config->MsgCycleOffset);
#endif
  CanNmSetAlarm(RepeatMessage, config->RepeatMessageTime);
}

static void CanNm_ReadySleepMain(CanNm_ChannelContextType *context,
                                 const CanNm_ChannelConfigType *config) {
  if (0u != (context->Flags & CANNM_RX_INDICATION_REQUEST_MASK)) {
    CanNmEnterCritical();
    context->Flags &= ~CANNM_RX_INDICATION_REQUEST_MASK;
    CanNmExitCritical();
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
    if (0u != (context->Flags & CANNM_REMOTE_SLEEP_IND_MASK)) {
      CanNmEnterCritical();
      context->Flags &= ~CANNM_REMOTE_SLEEP_IND_MASK;
      CanNmExitCritical();
#ifdef USE_NM
      /* @SWS_CanNm_00151 */
      Nm_RemoteSleepCancellation(config->NetworkHandle);
#endif
    }
#endif
  }

  if (0u != (context->Flags &
             (CANNM_REPEAT_MESSAGE_REQUEST_MASK | CANNM_RX_REPEAT_MESSAGE_BIT_SET_MASK))) {
#ifdef USE_NM /* @SWS_CanNm_00166 */
    Nm_StateChangeNotification(config->NetworkHandle, NM_STATE_READY_SLEEP,
                               NM_STATE_REPEAT_MESSAGE);
#endif
    CanNm_EnterRepeatMessageMode(context, config);
  } else if (0u != (context->Flags & CANNM_REQUEST_MASK)) {
    context->State = NM_STATE_NORMAL_OPERATION;
#ifdef USE_NM /* @SWS_CanNm_00166 */
    Nm_StateChangeNotification(config->NetworkHandle, NM_STATE_READY_SLEEP,
                               NM_STATE_NORMAL_OPERATION);
#endif
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED /* @SWS_CanNm_00149, @SWS_CanNm_00150 */
    CanNmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime);
#endif
    CanNm_SendMessage(context, config, config->MsgCycleTime); /* @SWS_CanNm_00006 */
    ASLOG(CANNM,
          ("%d: Enter Normal Operation Mode, flags %02X\n", config->NetworkHandle, context->Flags));
  } else {
  }

  if (CanNmIsAlarmStarted(NmTimeout)) {
    CanNmSignalAlarm(NmTimeout);
    if (0u != (context->Flags & CANNM_DISABLE_COMMUNICATION_REQUEST_MASK)) {
      CanNmCancelAlarm(NmTimeout); /* @SWS_CanNm_00174 */
    }
    if (CanNmIsAlarmTimeout(NmTimeout)) {
      if (NM_STATE_READY_SLEEP == context->State) {
        CanNmCancelAlarm(NmTimeout);
        CanNmSetAlarm(WaitBusSleep, config->WaitBusSleepTime);
        context->State = NM_STATE_PREPARE_BUS_SLEEP;
        if ((config->ActiveWakeupBitEnabled) && (config->PduCbvPosition < CANNM_PDU_OFF)) {
          context->Data[config->PduCbvPosition] &= ~CANNM_CBV_ACTIVE_WAKEUP; /* @SWS_CanNm_00402 */
        }
#ifdef USE_NM /* @SWS_CanNm_00166 */
        Nm_StateChangeNotification(config->NetworkHandle, NM_STATE_READY_SLEEP,
                                   NM_STATE_PREPARE_BUS_SLEEP);
#endif
#ifdef USE_NM
        Nm_PrepareBusSleepMode(config->NetworkHandle);
#endif
        ASLOG(CANNM, ("%d: Enter Prepare Bus Sleep Mode, flags %02X\n", config->NetworkHandle,
                      context->Flags));
      } else {
        CanNmSetAlarm(NmTimeout, config->NmTimeoutTime);
      }
    }
  } else {
    if (0u == (context->Flags & CANNM_DISABLE_COMMUNICATION_REQUEST_MASK)) {
      /* @SWS_CanNm_00178, @SWS_CanNm_00179 */
      ASLOG(ERROR, ("CANNM NMTimeout not started\n"));
      CanNmSetAlarm(NmTimeout, config->NmTimeoutTime);
    }
  }
}

static void CanNm_NetworkNormalOperationMain(CanNm_ChannelContextType *context,
                                             const CanNm_ChannelConfigType *config) {
  if (CanNmIsAlarmStarted(TxTimeout)) {
    CanNmSignalAlarm(TxTimeout);
    if (CanNmIsAlarmTimeout(TxTimeout)) {
      CanNmCancelAlarm(TxTimeout);
#ifdef USE_NM /* @SWS_CanNm_00066 */
      Nm_TxTimeoutException(config->NetworkHandle);
#endif
#if defined(CANNM_GLOBAL_PN_SUPPORT) && defined(USE_CANSM) /* SWS_CanNm_00446 */
      CanSM_TxTimeoutException((NetworkHandleType)(config - CANNM_CONFIG->ChannelConfigs));
#endif
    }
  }

  if (CanNmIsAlarmStarted(NmTimeout)) {
    CanNmSignalAlarm(NmTimeout);
    if (CanNmIsAlarmTimeout(NmTimeout)) {
      CanNmSetAlarm(NmTimeout, config->NmTimeoutTime);
    }
  } else {
    CanNmSetAlarm(NmTimeout, config->NmTimeoutTime);
  }

  if (0u != (context->Flags & CANNM_RX_INDICATION_REQUEST_MASK)) {
    CanNmEnterCritical();
    context->Flags &= ~CANNM_RX_INDICATION_REQUEST_MASK;
    CanNmExitCritical();
    if (config->MsgReducedTime > 0u) {
      CanNmSetAlarm(Tx, config->MsgReducedTime);
    }
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
    CanNmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime);
    if (0u != (context->Flags & CANNM_REMOTE_SLEEP_IND_MASK)) {
      CanNmEnterCritical();
      context->Flags &= ~CANNM_REMOTE_SLEEP_IND_MASK;
      CanNmExitCritical();
#ifdef USE_NM
      /* @SWS_CanNm_00151 */
      Nm_RemoteSleepCancellation(config->NetworkHandle);
#endif
    }
#endif
  }

  if (CanNmIsAlarmStarted(Tx)) {
    CanNmSignalAlarm(Tx);
    if (CanNmIsAlarmTimeout(Tx)) {
      CanNm_SendMessage(context, config, config->MsgCycleTime);
    }
  } else {
    CanNmSetAlarm(Tx, config->MsgCycleTime);
  }

#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED /* @SWS_CanNm_00149, @SWS_CanNm_00150 */
  if (CanNmIsAlarmStarted(RemoteSleepInd)) {
    CanNmSignalAlarm(RemoteSleepInd);
    if (CanNmIsAlarmTimeout(RemoteSleepInd)) {
      CanNmCancelAlarm(RemoteSleepInd);
      if (0u == (context->Flags & CANNM_DISABLE_COMMUNICATION_REQUEST_MASK)) {
#ifdef USE_NM
        /* @SWS_CanNm_00175 */
        Nm_RemoteSleepIndication(config->NetworkHandle);
#endif
        CanNmEnterCritical();
        context->Flags |= CANNM_REMOTE_SLEEP_IND_MASK;
        CanNmExitCritical();
      }
    }
  }
#endif

  if (0u != (context->Flags &
             (CANNM_REPEAT_MESSAGE_REQUEST_MASK | CANNM_RX_REPEAT_MESSAGE_BIT_SET_MASK))) {
#ifdef USE_NM /* @SWS_CanNm_00166 */
    Nm_StateChangeNotification(config->NetworkHandle, NM_STATE_NORMAL_OPERATION,
                               NM_STATE_REPEAT_MESSAGE);
#endif
    CanNm_EnterRepeatMessageMode(context, config);
  } else if (0u == (context->Flags & CANNM_REQUEST_MASK)) {
    context->State = NM_STATE_READY_SLEEP;
#ifdef USE_NM /* @SWS_CanNm_00166 */
    Nm_StateChangeNotification(config->NetworkHandle, NM_STATE_NORMAL_OPERATION,
                               NM_STATE_READY_SLEEP);
#endif
    ASLOG(CANNM,
          ("%d: Enter Ready Sleep Mode, flags %02X\n", config->NetworkHandle, context->Flags));
  } else {
  }
}

#ifdef CANNM_GLOBAL_PN_SUPPORT
static Std_ReturnType CanNm_RxFilter(CanNm_ChannelContextType *context,
                                     const CanNm_ChannelConfigType *config,
                                     const PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_OK;
  int i;
  if (0u != (context->Flags & CANNM_PN_ENABLED_MASK)) {
    if (0 ==
        (pduInfoPtr->SduDataPtr[config->PduCbvPosition] & CANNM_CBV_PARTIAL_NETWORK_INFORMATION)) {
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
        if (pduInfoPtr->SduDataPtr[config->PnInfoOffset + i] & config->PnFilterMaskByte[i]) {
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

  for (i = 0u; i < CANNM_CONFIG->NumOfChannels; i++) {
    context = &CANNM_CONFIG->ChannelContexts[i];
    config = &CANNM_CONFIG->ChannelConfigs[i];
    context->State = NM_STATE_BUS_SLEEP; /* @SWS_CanNm_00141 */
    context->TxCounter = 0u;
    context->Flags = 0u; /* @SWS_CanNm_00403 */
    (void)memset(context->RxPdu, 0xFF, sizeof(context->RxPdu));
    (void)memset(context->Data, 0xFF, sizeof(context->Data)); /* @SWS_CanNm_00025 */
    if (config->PduNidPosition < CANNM_PDU_OFF) {
      context->Data[config->PduNidPosition] = config->NodeId; /* @SWS_CanNm_00013 */
    }
    if (config->PduCbvPosition < CANNM_PDU_OFF) {
      context->Data[config->PduCbvPosition] = 0x00; /* @SWS_CanNm_00085 */
#ifdef CANNM_GLOBAL_PN_SUPPORT
      if (config->PnEnabled) { /* @SWS_CanNm_00413 */
        context->Data[config->PduCbvPosition] |= CANNM_CBV_PARTIAL_NETWORK_INFORMATION;
        (void)memcpy(&context->Data[config->PnInfoOffset], config->PnFilterMaskByte,
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
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->NumOfChannels, 0x01, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  switch ((context->State)) { /* @SWS_CanNm_00128 */
  case NM_STATE_BUS_SLEEP:
  case NM_STATE_PREPARE_BUS_SLEEP:
    CanNmEnterCritical();
    context->Flags |= CANNM_PASSIVE_STARTUP_REQUEST_MASK;
    CanNmExitCritical();
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
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->NumOfChannels, 0x02, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  CanNmEnterCritical();
  context->Flags |= CANNM_REQUEST_MASK;
  CanNmExitCritical();

  return ret;
}

Std_ReturnType CanNm_NetworkRelease(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x03, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->NumOfChannels, 0x03, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  CanNmEnterCritical();
  context->Flags &= ~CANNM_REQUEST_MASK;
  CanNmExitCritical();

  return ret;
}

Std_ReturnType CanNm_RepeatMessageRequest(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x08, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->NumOfChannels, 0x08, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);
  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  switch (context->State) {
  case NM_STATE_READY_SLEEP:
  case NM_STATE_NORMAL_OPERATION:
    CanNmEnterCritical();
    context->Flags |= CANNM_REPEAT_MESSAGE_REQUEST_MASK;
    CanNmExitCritical();
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
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->NumOfChannels, 0x0C, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  /* @SWS_CanNm_00170 */
  CanNmEnterCritical();
  context->Flags |= CANNM_DISABLE_COMMUNICATION_REQUEST_MASK;
  CanNmExitCritical();
  /* could see codes that check CANNM_DISABLE_COMMUNICATION_REQUEST_MASK in the MainFunction,
   * That is because that Alarm operation is not atomic, so the codes that added to check
   * CANNM_DISABLE_COMMUNICATION_REQUEST_MASK is a workaroud and which is not good but it
   * satisfied the requirement */
  CanNmCancelAlarm(Tx);        /* @SWS_CanNm_00173 */
  CanNmCancelAlarm(NmTimeout); /* @SWS_CanNm_00174 */
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
  CanNmCancelAlarm(RemoteSleepInd); /* @SWS_CanNm_00175 */
#endif

  return ret;
}

Std_ReturnType CanNm_EnableCommunication(NetworkHandleType nmChannelHandle) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x0D, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->NumOfChannels, 0x0D, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
  CanNmEnterCritical();
  context->Flags &= ~CANNM_DISABLE_COMMUNICATION_REQUEST_MASK;
  CanNmEnterCritical();
  CanNmSetAlarm(Tx, 0u);                           /* @SWS_CanNm_00178 */
  CanNmSetAlarm(NmTimeout, config->NmTimeoutTime); /* @SWS_CanNm_00179 */
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
  CanNmSetAlarm(RemoteSleepInd, config->RemoteSleepIndTime); /* @SWS_CanNm_00180 */
#endif

  return ret;
}

void CanNm_TxConfirmation(PduIdType txPduId, Std_ReturnType result) {
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x40, CANNM_E_UNINIT, return);
  DET_VALIDATE(txPduId < CANNM_CONFIG->NumOfChannels, 0x40, CANNM_E_INVALID_CHANNEL, return);

  context = &CANNM_CONFIG->ChannelContexts[txPduId];
  config = &CANNM_CONFIG->ChannelConfigs[txPduId];
#ifdef CANNM_COM_USER_DATA_SUPPORT
  PduR_CanNmTxConfirmation(config->UserDataTxPdu, result);
#endif
  if (E_OK == result) {
    if (context->TxCounter < 0xFFu) {
      context->TxCounter++;
    }
    CanNmCancelAlarm(TxTimeout); /* @SWS_CanNm_00065 */

    if ((NM_STATE_NORMAL_OPERATION == context->State) ||
        (NM_STATE_REPEAT_MESSAGE == context->State) || (NM_STATE_READY_SLEEP == context->State)) {
      CanNmSetAlarm(NmTimeout, config->NmTimeoutTime);
    }
  } else {
#ifdef USE_NM /* @SWS_CanNm_00066 */
    Nm_TxTimeoutException(config->NetworkHandle);
#endif
  }
}

void CanNm_RxIndication(PduIdType rxPduId, const PduInfoType *pduInfoPtr) {
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;
  uint16_t flags = CANNM_RX_INDICATION_REQUEST_MASK | CANNM_NM_PDU_RECEIVED_MASK;
  Std_ReturnType ret = E_NOT_OK;
#ifdef CANNM_CAR_WAKEUP_SUPPORT
  const Can_HwType *Mailbox = (const Can_HwType *)pduInfoPtr->MetaDataPtr;
#endif

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x42, CANNM_E_UNINIT, return);
  DET_VALIDATE(rxPduId < CANNM_CONFIG->NumOfChannels, 0x42, CANNM_E_INVALID_CHANNEL, return);
  DET_VALIDATE((NULL != pduInfoPtr) && (NULL != pduInfoPtr->SduDataPtr), 0x42,
               CANNM_E_PARAM_POINTER, return);

  context = &CANNM_CONFIG->ChannelContexts[rxPduId];
  config = &CANNM_CONFIG->ChannelConfigs[rxPduId];
#ifdef CANNM_GLOBAL_PN_SUPPORT
  ret = CanNm_RxFilter(context, config, pduInfoPtr);
#else
  ret = E_OK;
#endif

  if (E_OK == ret) {
    /* @SWS_CanNm_00035 */
    (void)memcpy(context->RxPdu, pduInfoPtr->SduDataPtr, sizeof(context->RxPdu));
    if (config->PduCbvPosition < CANNM_PDU_OFF) {
      if (0u != (context->RxPdu[config->PduCbvPosition] & CANNM_CBV_REPEAT_MESSAGE_REQUEST)) {
        if (TRUE == config->NodeDetectionEnabled) {
          /* @SWS_CanNm_00119, @SWS_CanNm_00111 */
          if ((NM_STATE_NORMAL_OPERATION == context->State) ||
              (NM_STATE_READY_SLEEP == context->State)) {
            flags |= CANNM_RX_REPEAT_MESSAGE_BIT_SET_MASK;
          }
        }

        /* @SWS_CanNm_00014 */
        if ((TRUE == config->RepeatMsgIndEnabled) && (TRUE == config->NodeDetectionEnabled)) {
#ifdef USE_NM
          Nm_RepeatMessageIndication(config->NetworkHandle);
#endif
        }
      }
#ifdef CANNM_COORDINATOR_SYNC_SUPPORT
      if ((NM_STATE_NORMAL_OPERATION == context->State) ||
          (NM_STATE_REPEAT_MESSAGE == context->State) || (NM_STATE_READY_SLEEP == context->State)) {
        if (0u != (context->RxPdu[config->PduCbvPosition] & CANNM_CBV_NM_COORDINATOR_SLEEP)) {
          /* @SWS_CanNm_00341 */
          if (0u == (context->Flags & CANNM_COORDINATOR_SLEEP_SYNC_MASK)) {
            flags |= CANNM_COORDINATOR_SLEEP_SYNC_MASK;
#ifdef USE_NM
            Nm_CoordReadyToSleepIndication(config->NetworkHandle);
#endif
          }
        } else if (0u != (context->Flags & CANNM_COORDINATOR_SLEEP_SYNC_MASK)) {
#ifdef USE_NM /* @SWS_CanNm_00348 */
          Nm_CoordReadyToSleepCancellation(config->NetworkHandle);
#endif
          CanNmEnterCritical();
          context->Flags &= ~CANNM_COORDINATOR_SLEEP_SYNC_MASK;
          CanNmExitCritical();
        } else {
          /* do nothing */
        }
      }
#endif
#ifdef CANNM_CAR_WAKEUP_SUPPORT
      CanNm_CarWakeupProcess(config, context, Mailbox->CanId & config->NodeMask);
#endif
    }
    if ((NM_STATE_NORMAL_OPERATION == context->State) ||
        (NM_STATE_REPEAT_MESSAGE == context->State) || (NM_STATE_READY_SLEEP == context->State)) {
      CanNmSetAlarm(NmTimeout, config->NmTimeoutTime);
    }
    CanNmEnterCritical();
    context->Flags |= flags;
    CanNmExitCritical();
  }
}

Std_ReturnType CanNm_GetLocalNodeIdentifier(NetworkHandleType nmChannelHandle,
                                            uint8_t *nmNodeIdPtr) {
  Std_ReturnType ret = E_OK;
  const CanNm_ChannelConfigType *config;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x07, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->NumOfChannels, 0x07, CANNM_E_INVALID_CHANNEL,
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
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->NumOfChannels, 0x06, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);
  DET_VALIDATE(NULL != nmNodeIdPtr, 0x06, CANNM_E_PARAM_POINTER, return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
  /* @SWS_CanNm_00132 */
  if (config->PduNidPosition < CANNM_PDU_OFF) {
    nodeId = context->RxPdu[config->PduNidPosition];
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
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->NumOfChannels, 0x04, CANNM_E_INVALID_CHANNEL,
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
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->NumOfChannels, 0x05, CANNM_E_INVALID_CHANNEL,
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
      nmUserDataPtr[j] = context->Data[i];
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
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->NumOfChannels, 0x0A, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);
  DET_VALIDATE(NULL != nmPduDataPtr, 0x0A, CANNM_E_PARAM_POINTER, return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  if (0u != (context->Flags & CANNM_NM_PDU_RECEIVED_MASK)) {
    (void)memcpy(nmPduDataPtr, &context->RxPdu, 8);
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
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->NumOfChannels, 0x0B, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);
  DET_VALIDATE((NULL != nmStatePtr) && (NULL != nmModePtr), 0x0B, CANNM_E_PARAM_POINTER,
               return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
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

  return ret;
}

#ifdef CANNM_GLOBAL_PN_SUPPORT
void CanNm_ConfirmPnAvailability(NetworkHandleType nmChannelHandle) {
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x16, CANNM_E_UNINIT, return);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->NumOfChannels, 0x16, CANNM_E_INVALID_CHANNEL,
               return);
  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
  if (config->PnEnabled) {
    ASLOG(CANNM, ("%d: confirm PN, flags %X\n", nmChannelHandle, context->Flags));
    /* @SWS_CanNm_00404 */
    CanNmEnterCritical();
    context->Flags |= CANNM_PN_ENABLED_MASK;
    CanNmExitCritical();
  }
}
#endif

#ifdef CANNM_COORDINATOR_SYNC_SUPPORT
Std_ReturnType CanNm_SetSleepReadyBit(NetworkHandleType nmChannelHandle, boolean nmSleepReadyBit) {
  Std_ReturnType ret = E_OK;
  CanNm_ChannelContextType *context;
  const CanNm_ChannelConfigType *config;

  DET_VALIDATE(NULL != CANNM_CONFIG, 0x17, CANNM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->NumOfChannels, 0x17, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  config = &CANNM_CONFIG->ChannelConfigs[nmChannelHandle];
  CanNmEnterCritical();
  if (config->PduCbvPosition < CANNM_PDU_OFF) {
    /* @SWS_CanNm_00342 */
    if (TRUE == nmSleepReadyBit) {
      context->Data[config->PduCbvPosition] |= CANNM_CBV_NM_COORDINATOR_SLEEP;
    } else {
      context->Data[config->PduCbvPosition] &= ~CANNM_CBV_NM_COORDINATOR_SLEEP;
    }
  } else {
    ret = E_NOT_OK;
  }
  CanNmEnterCritical();
  if (E_OK == ret) {
    CanNm_SendMessage(context, config, 0);
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
  DET_VALIDATE(nmChannelHandle < CANNM_CONFIG->NumOfChannels, 0xD0, CANNM_E_INVALID_CHANNEL,
               return E_NOT_OK);
  DET_VALIDATE(NULL != nmRemoteSleepIndPtr, 0xD0, CANNM_E_PARAM_POINTER, return E_NOT_OK);

  context = &CANNM_CONFIG->ChannelContexts[nmChannelHandle];
  switch (context->State) {
  case NM_STATE_NORMAL_OPERATION:
    if (0u != (context->Flags & CANNM_REMOTE_SLEEP_IND_MASK)) {
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

  for (i = 0u; i < CANNM_CONFIG->NumOfChannels; i++) {
    context = &CANNM_CONFIG->ChannelContexts[i];
    config = &CANNM_CONFIG->ChannelConfigs[i];
    switch (context->State) {
    case NM_STATE_BUS_SLEEP:
      CanNm_BusSleepMain(context, config);
      break;
    case NM_STATE_PREPARE_BUS_SLEEP:
      CanNm_PrepareBusSleepMain(context, config);
      break;
    case NM_STATE_READY_SLEEP:
      CanNm_ReadySleepMain(context, config);
      break;
    case NM_STATE_REPEAT_MESSAGE:
      CanNm_RepeatMessageMain(context, config);
      break;
    case NM_STATE_NORMAL_OPERATION:
      CanNm_NetworkNormalOperationMain(context, config);
      break;
    default:
      break;
    }
  }
}

void CanNm_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0xf1, CANNM_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_CANNM;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 1;
}

/** @brief release notes
 * - 4.0.1: Replace accidental CanNmEnterCritical() with CanNmExitCritical() in
 * RepeatMessageRequest/DisableCommunication.
 */
