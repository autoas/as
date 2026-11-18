/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: nm253
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "OsekNm.h"
#include "CanIf.h"
#include "OsekNm_Priv.h"
#include "Can.h"
#include <stdlib.h>
#include <string.h>
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_OSEK_NM 0

#ifndef OSEKNM_MAIN_FUNCTION_PERIOD
#define OSEKNM_MAIN_FUNCTION_PERIOD 10
#endif

/* Macros for OpCode */
#define OSEKNM_MASK_ALIVE ((uint8_t)0x01)
#define OSEKNM_MASK_RING ((uint8_t)0x02)
#define OSEKNM_MASK_LIMPHOME ((uint8_t)0x04)
#define OSEKNM_MASK_SI ((uint8_t)0x10) /* sleep ind */
#define OSEKNM_MASK_SA ((uint8_t)0x20) /* sleep ack */

#ifdef OSEKNM_USE_STD_TIMER
#define OsekNmSetAlarm(Timer)                                                                      \
  Std_TimerInit(&context->Alarm.Timer, config->Timer * 1000 * OSEKNM_MAIN_FUNCTION_PERIOD)
#define OsekNmSignalAlarm(Timer)
#define OsekNmIsAlarmTimeout(Timer) Std_IsTimerTimeout(&context->Alarm.Timer)
#define OsekNmIsAlarmStarted(Timer) Std_IsTimerStarted(&context->Alarm.Timer)
#define OsekNmCancelAlarm(Timer) Std_TimerStop(&context->Alarm.Timer)
#else
/* Alarm Management */
#define OsekNmSetAlarm(Timer)                                                                      \
  do {                                                                                             \
    context->Alarm.Timer = 1u + config->Timer;                                                     \
  } while (0)

/* signal the alarm to process one step/tick forward */
#define OsekNmSignalAlarm(Timer)                                                                   \
  do {                                                                                             \
    if (context->Alarm.Timer > 1u) {                                                               \
      (context->Alarm.Timer)--;                                                                    \
    }                                                                                              \
  } while (0)

#define OsekNmIsAlarmTimeout(Timer) (1u == context->Alarm.Timer)

#define OsekNmIsAlarmStarted(Timer) (0u != context->Alarm.Timer)

#define OsekNmCancelAlarm(Timer)                                                                   \
  do {                                                                                             \
    context->Alarm.Timer = 0u;                                                                     \
  } while (0)
#endif

/* the network handle is derived from the channel context of the caller, so
 * that this macro does not depend on the name of the function parameter */
#define OsekNmSendMessage()                                                                        \
  do {                                                                                             \
    Std_ReturnType ercd;                                                                           \
    NetworkHandleType channel = (NetworkHandleType)((context) - OSEKNM_CONFIG->ChannelContexts);   \
    ercd = OsekNm_D_WindowDataReq(channel, &(context->NmTxPdu), 8u);                               \
    if (ercd != E_OK) {                                                                            \
      OsekNmSetAlarm(TTx); /* re-Transmit after TTx */                                             \
    }                                                                                              \
  } while (0)

#define OsekNmInit(NetId)

#define OSEKNM_CONFIG (&OsekNm_Config)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const OsekNm_ConfigType OsekNm_Config;

static void OsekNm_Init3(NetworkHandleType netId);
static void OsekNm_AddToPresent(NetworkHandleType netId, OsekNm_NodeIdType nodeId);
static void OsekNm_AddToLimphome(NetworkHandleType netId, OsekNm_NodeIdType nodeId);
static void OsekNm_InitReset5(NetworkHandleType netId);
static void OsekNm_InitReset6(NetworkHandleType netId);
static void OsekNm_NormalStandard(NetworkHandleType netId, OsekNm_PduType *nmPdu);
static OsekNm_NodeIdType OsekNm_DetermineLS(OsekNm_NodeIdType S, OsekNm_NodeIdType R,
                                            OsekNm_NodeIdType L);
static boolean OsekNm_IsMeSkipped(OsekNm_NodeIdType S, OsekNm_NodeIdType R, OsekNm_NodeIdType D);
static void OsekNm_BusSleep(NetworkHandleType netId);
static void OsekNm_NormalTwbsMain(NetworkHandleType netId);
static void OsekNm_NormalPrepSleepMain(NetworkHandleType netId);
static void OsekNm_LimphomeMain(NetworkHandleType netId);
static void OsekNm_LimphomePrepSleepMain(NetworkHandleType netId);
static void OsekNm_NormalMain(NetworkHandleType netId);
static void OsekNm_TwbsLimphomeMain(NetworkHandleType netId);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* OsekNm_Init3 is special doesn't mean all the operation of "Leave NMBusSleep" */
static void OsekNm_Init3(NetworkHandleType netId) {
  OsekNm_ChannelContextType *context = &OSEKNM_CONFIG->ChannelContexts[netId];
  /* step 3: config.limphome  := 0 */
  /* step 4: */
  context->NmRxCount = 0;
  context->NmTxCount = 0;
  OsekNm_D_Online(netId);
  OsekNm_InitReset6(netId);
}

static void OsekNm_AddToPresent(NetworkHandleType netId, OsekNm_NodeIdType nodeId) {
  /* TODO: */
  (void)netId;
  (void)nodeId;
}

static void OsekNm_AddToLimphome(NetworkHandleType netId, OsekNm_NodeIdType nodeId) {
  /* TODO: */
  (void)netId;
  (void)nodeId;
}

static void OsekNm_InitReset5(NetworkHandleType netId) {
  OsekNm_ChannelContextType *context = &OSEKNM_CONFIG->ChannelContexts[netId];

  OsekNmCancelAlarm(TError);
  context->NmMerker &= ~OSEKNM_MERKER_LIMPHOME;

  context->NmRxCount = 0;
  context->NmTxCount = 0;
  OsekNm_D_Online(netId);
  OsekNm_InitReset6(netId);
}

static void OsekNm_InitReset6(NetworkHandleType netId) {
  const OsekNm_ChannelConfigType *config = &OSEKNM_CONFIG->ChannelConfigs[netId];
  OsekNm_ChannelContextType *context = &OSEKNM_CONFIG->ChannelContexts[netId];

  context->NmTxPdu.Source = config->NodeId;
  /* config.present = own station */
  OsekNm_AddToPresent(netId, config->NodeId);
  /* logical successor := own station */
  context->NmTxPdu.Destination = config->NodeId;
  context->NmRxCount += 1u;
  /* Initialise the nmPdu(Data,OpCode) */
  context->NmTxPdu.OpCode = 0;
  (void)memset(context->NmTxPdu.RingData, 0, sizeof(OsekNm_RingDataType));
  /* Cancel all Alarm */
  OsekNmCancelAlarm(TTx);
  OsekNmCancelAlarm(TTyp);
  OsekNmCancelAlarm(TMax);
  OsekNmCancelAlarm(TWbs);
  OsekNmCancelAlarm(TError);
  if (0u != (context->Status.NetworkStatus & OSEKNM_STATUS_ACTIVE)) {
    context->NmTxCount += 1u;
    /* Send A alive message */
    context->NmTxPdu.OpCode = OSEKNM_MASK_ALIVE;
    OsekNmSendMessage();
    ASLOG(OSEK_NM, ("[%d] OsekNm_InitReset6:Send Alive\n", netId));
  }
  if ((context->NmTxCount <= config->TxLimit) && (context->NmRxCount <= config->RxLimit)) {
    OsekNmSetAlarm(TTyp);
    context->NmState = OSEKNM_STATE_NORMAL;
    ASLOG(OSEK_NM, ("[%d] Set TTyp, enter Normal state.\n", netId));
  } else {
    OsekNmSetAlarm(TError);
    context->NmState = OSEKNM_STATE_LIMPHOME;
    ASLOG(OSEK_NM, ("[%d] Set TError, Enter Limphome state.\n", netId));
  }
}

static void OsekNm_NormalStandard(NetworkHandleType netId, OsekNm_PduType *nmPdu) {
  const OsekNm_ChannelConfigType *config = &OSEKNM_CONFIG->ChannelConfigs[netId];
  OsekNm_ChannelContextType *context = &OSEKNM_CONFIG->ChannelContexts[netId];
  boolean bIsMeSkipped;
  context->NmRxCount = 0;
  if (0u != (nmPdu->OpCode & OSEKNM_MASK_LIMPHOME)) {
    /* add sender to config.limphome */
    OsekNm_AddToLimphome(netId, nmPdu->Source);
  } else {
    /* add sender to config.present */
    OsekNm_AddToPresent(netId, nmPdu->Source);
    /* determine logical successor */
    context->NmTxPdu.Destination =
      OsekNm_DetermineLS(nmPdu->Source, config->NodeId, context->NmTxPdu.Destination);
    if (0u != (nmPdu->OpCode & OSEKNM_MASK_RING)) {
      OsekNmCancelAlarm(TTyp);
      OsekNmCancelAlarm(TMax);
      /* see nm253 Figure 4 Mechanism to transfer application data via the logical ring */
      (void)memcpy(context->NmTxPdu.RingData, nmPdu->RingData, sizeof(OsekNm_RingDataType));
      if ((nmPdu->Destination == config->NodeId)      /* to me */
          || (nmPdu->Destination == nmPdu->Source)) { /* or D = S */
        OsekNmSetAlarm(TTyp);
        ASLOG(OSEK_NM, ("[%d] Set TTyp\n", netId));
        /* Do Ring Data indication */
        /* TODO */
      } else {
        OsekNmSetAlarm(TMax);
        bIsMeSkipped = OsekNm_IsMeSkipped(nmPdu->Source, config->NodeId, nmPdu->Destination);
        if (TRUE == bIsMeSkipped) {
          if (0u != (context->Status.NetworkStatus & OSEKNM_STATUS_ACTIVE)) {
            context->NmTxPdu.OpCode = OSEKNM_MASK_ALIVE;
            context->NmTxPdu.Destination = config->NodeId;
            if (0u != (context->Status.NetworkStatus & OSEKNM_STATUS_BUS_SLEEP)) {
              context->NmTxPdu.OpCode |= OSEKNM_MASK_SI;
            }
            OsekNmSendMessage();
          }
        }
      }
    }
  }
}

static OsekNm_NodeIdType OsekNm_DetermineLS(OsekNm_NodeIdType S, OsekNm_NodeIdType R,
                                            OsekNm_NodeIdType L) {
  OsekNm_NodeIdType newL = L;
  if (L == R) {
    newL = S;
  } else {
    if (L < R) {
      if (S < L) {
        newL = S; /* SLR */
      } else {
        if (S < R) {
          /* LSR */
        } else {
          newL = S; /* LRS */
        }
      }
    } else {
      if (S < L) {
        if (S < R) {
          /* SRL */
        } else {
          newL = S; /* RSL */
        }
      } else {
        /* RLS */
      }
    }
  }
  return newL;
}

static boolean OsekNm_IsMeSkipped(OsekNm_NodeIdType S, OsekNm_NodeIdType R, OsekNm_NodeIdType D) {
  boolean isSkipped = FALSE;
  if (D < R) {
    if (S < D) {
      /* not skipped SDR */
    } else {
      if (S < R) {
        isSkipped = TRUE; /* DRS */
      } else {
        /* not skipped DSR */
      }
    }
  } else {
    if (S < D) {
      if (S < R) {
        isSkipped = TRUE; /* SRD */
      } else {
        /* RSD */
      }
    } else {
      isSkipped = TRUE; /* RDS */
    }
  }
  return isSkipped;
}

static void OsekNm_OffMain(NetworkHandleType netId) {
  OsekNm_ChannelContextType *context = &OSEKNM_CONFIG->ChannelContexts[netId];
  if (TRUE == context->RequestOn) { /* see nm253.pdf Figure 30 Start-up of the network */
    /* step 1: */
    context->NmTxPdu.OpCode &= ~(OSEKNM_MASK_SI | OSEKNM_MASK_SA);
    /* step 2: enter OsekNmInit */
    OsekNmInit(netId);
    OsekNm_D_Init(netId, OSEKNM_ROUTINE_BUS_INIT);
    context->NmState = OSEKNM_STATE_BUS_SLEEP;
  }
}

static void OsekNm_BusSleepMain(NetworkHandleType netId) {
  OsekNm_ChannelContextType *context = &OSEKNM_CONFIG->ChannelContexts[netId];

  if ((OSEKNM_AWAKE == context->RequestMode) || (TRUE == context->WakeupSignal)) {
    context->WakeupSignal = FALSE; /* always set it FALSE on startup */
    OsekNm_D_Init(netId, OSEKNM_ROUTINE_BUS_AWAKE);
    OsekNm_Init3(netId);
  }
}

static void OsekNm_NormalMain(NetworkHandleType netId) {
  const OsekNm_ChannelConfigType *config = &OSEKNM_CONFIG->ChannelConfigs[netId];
  OsekNm_ChannelContextType *context = &OSEKNM_CONFIG->ChannelContexts[netId];
  if (OsekNmIsAlarmStarted(TTyp)) {
    OsekNmSignalAlarm(TTyp);
    if (OsekNmIsAlarmTimeout(TTyp)) {
      OsekNmCancelAlarm(TTyp);
      OsekNmCancelAlarm(TMax);
      OsekNmSetAlarm(TMax);
      if (0u != (context->Status.NetworkStatus & OSEKNM_STATUS_ACTIVE)) {
        context->NmTxPdu.OpCode = OSEKNM_MASK_RING;
        if (0u != (context->Status.NetworkStatus & OSEKNM_STATUS_BUS_SLEEP)) {
          context->NmTxPdu.OpCode |= OSEKNM_MASK_SI;
        }
        context->NmTxCount++;
        OsekNmSendMessage();
      } else {
      }
      if (context->NmTxCount > config->TxLimit) {
        ASLOG(OSEK_NM, ("[%d] Goto limphome mode\n", netId));
        context->NmState = OSEKNM_STATE_LIMPHOME;
        OsekNmSetAlarm(TError);
      } else {
        if (0u != (context->NmMerker & OSEKNM_MERKER_STABLE)) {
          context->Status.NetworkStatus |= OSEKNM_STATUS_CONFIGURATION_STABLE;
        } else {
          context->NmMerker |= OSEKNM_MERKER_STABLE;
        }
      }
    }
  }

  if (OsekNmIsAlarmStarted(TMax)) {
    OsekNmSignalAlarm(TMax);
    if (OsekNmIsAlarmTimeout(TMax)) {
      OsekNmCancelAlarm(TMax);
      OsekNm_InitReset6(netId);
    }
  }

  if (OSEKNM_BUS_SLEEP == context->RequestMode) {
    if (0u != (context->Status.NetworkStatus & OSEKNM_STATUS_ACTIVE)) {
      ASLOG(OSEK_NM, ("[%d] GotoMode_BusSleep, Keep in Normal as active.\n", netId));
    } else {
      ASLOG(OSEK_NM, ("[%d] GotoMode_BusSleep, Enter NormalPrepSleep.\n", netId));
      context->NmState = OSEKNM_STATE_NORMAL_PREPARE_SLEEP;
    }
  }
}

static void OsekNm_LimphomeMain(NetworkHandleType netId) {
  const OsekNm_ChannelConfigType *config = &OSEKNM_CONFIG->ChannelConfigs[netId];
  OsekNm_ChannelContextType *context = &OSEKNM_CONFIG->ChannelContexts[netId];
  if (OsekNmIsAlarmStarted(TError)) {
    OsekNmSignalAlarm(TError);
    if (OsekNmIsAlarmTimeout(TError)) {
      OsekNmCancelAlarm(TError);
      OsekNm_D_Online(netId);
      context->NmTxPdu.OpCode = OSEKNM_MASK_LIMPHOME;
      if (0u != (context->Status.NetworkStatus & OSEKNM_STATUS_BUS_SLEEP)) {
        OsekNmSetAlarm(TMax);
        context->NmTxPdu.OpCode |= OSEKNM_MASK_SI;
        context->NmState = OSEKNM_STATE_LIMPHOME_PREPARE_SLEEP;
        ASLOG(OSEK_NM, ("[%d] Goto Limphome Prepare Sleep\n", netId));
      } else {
        OsekNmSetAlarm(TError);
      }
      if (0u != (context->Status.NetworkStatus & OSEKNM_STATUS_ACTIVE)) {
        OsekNmSendMessage();
      }
    }
  } else { /* for roboustness, auto start TError if it was not started */
    OsekNmSetAlarm(TError);
  }
}
static void OsekNm_NormalPrepSleepMain(NetworkHandleType netId) {
  const OsekNm_ChannelConfigType *config = &OSEKNM_CONFIG->ChannelConfigs[netId];
  OsekNm_ChannelContextType *context = &OSEKNM_CONFIG->ChannelContexts[netId];
  if (OsekNmIsAlarmStarted(TTyp)) {
    OsekNmSignalAlarm(TTyp);
    if (OsekNmIsAlarmTimeout(TTyp)) {
      OsekNmCancelAlarm(TTyp);
      if (0u != (context->Status.NetworkStatus & OSEKNM_STATUS_ACTIVE)) {
        /* Send ring message with set sleep.ack bit */
        context->NmTxPdu.OpCode = OSEKNM_MASK_RING | OSEKNM_MASK_SI | OSEKNM_MASK_SA;
        OsekNmSendMessage();
      } else {
        OsekNm_D_Offline(netId);
        OsekNmSetAlarm(TWbs);
        context->NmState = OSEKNM_STATE_WAIT_BUS_SLEEP_NORMAL;
        ASLOG(OSEK_NM, ("[%d] Goto Normal Wait Bus Sleep as not active\n", netId));
      }
    }
  } else {
    if (OsekNmIsAlarmStarted(TMax)) {
      OsekNmSignalAlarm(TMax);
      if (OsekNmIsAlarmTimeout(TMax)) {
        OsekNmCancelAlarm(TMax);
        OsekNm_D_Offline(netId);
        OsekNmSetAlarm(TWbs);
        context->NmState = OSEKNM_STATE_WAIT_BUS_SLEEP_NORMAL;
        ASLOG(OSEK_NM, ("[%d] Goto Normal Wait Bus Sleep as TMax timeout\n", netId));
      }
    }
  }

  if (OSEKNM_AWAKE == context->RequestMode) {
    ASLOG(OSEK_NM, ("[%d] GotoMode_Awake, Enter Normal\n", netId));
    context->NmState = OSEKNM_STATE_NORMAL;
  }
}

static void OsekNm_BusSleep(NetworkHandleType netId) {
  OsekNm_ChannelContextType *context = &OSEKNM_CONFIG->ChannelContexts[netId];
  OsekNm_D_Init(netId, OSEKNM_ROUTINE_BUS_SLEEP);
  context->NmState = OSEKNM_STATE_BUS_SLEEP;
  ASLOG(OSEK_NM, ("[%d] Goto Bus Sleep\n", netId));
}

static void OsekNm_NormalTwbsMain(NetworkHandleType netId) {
  const OsekNm_ChannelConfigType *config = &OSEKNM_CONFIG->ChannelConfigs[netId];
  OsekNm_ChannelContextType *context = &OSEKNM_CONFIG->ChannelContexts[netId];
  (void)config;
  if (OsekNmIsAlarmStarted(TWbs)) {
    OsekNmSignalAlarm(TWbs);
    if (OsekNmIsAlarmTimeout(TWbs)) {
      OsekNmCancelAlarm(TWbs);
      OsekNm_BusSleep(netId);
    }
  }

  if (OSEKNM_AWAKE == context->RequestMode) {
    ASLOG(OSEK_NM, ("[%d] GotoMode_Awake, Exit Normal Wait Bus Sleep\n", netId));
    OsekNmCancelAlarm(TWbs);
    OsekNm_InitReset6(netId);
  }
}

static void OsekNm_LimphomePrepSleepMain(NetworkHandleType netId) {
  const OsekNm_ChannelConfigType *config = &OSEKNM_CONFIG->ChannelConfigs[netId];
  OsekNm_ChannelContextType *context = &OSEKNM_CONFIG->ChannelContexts[netId];
  if (OsekNmIsAlarmStarted(TMax)) {
    OsekNmSignalAlarm(TMax);
    if (OsekNmIsAlarmTimeout(TMax)) {
      OsekNmCancelAlarm(TMax);
      /* 7 NMInitBusSleep */
      OsekNm_D_Offline(netId);
      OsekNmSetAlarm(TWbs);
      context->NmState = OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME;
      ASLOG(OSEK_NM, ("[%d] Goto Limphome Wait Bus Sleep\n", netId));
    }
  }

  if (OSEKNM_AWAKE == context->RequestMode) {
    ASLOG(OSEK_NM, ("[%d] GotoMode_Awake, Enter Limphome\n", netId));
    context->NmState = OSEKNM_STATE_LIMPHOME;
    OsekNmSetAlarm(TError);
  }
}

static void OsekNm_TwbsLimphomeMain(NetworkHandleType netId) {
  const OsekNm_ChannelConfigType *config = &OSEKNM_CONFIG->ChannelConfigs[netId];
  OsekNm_ChannelContextType *context = &OSEKNM_CONFIG->ChannelContexts[netId];
  (void)config;
  if (OsekNmIsAlarmStarted(TWbs)) {
    OsekNmSignalAlarm(TWbs);
    if (OsekNmIsAlarmTimeout(TWbs)) {
      OsekNmCancelAlarm(TWbs);
      OsekNm_BusSleep(netId);
    }
  }

  if (OSEKNM_AWAKE == context->RequestMode) {
    ASLOG(OSEK_NM, ("[%d] GotoMode_Awake, Enter Limphome\n", netId));
    OsekNmCancelAlarm(TWbs);
    context->NmState = OSEKNM_STATE_LIMPHOME;
    OsekNmSetAlarm(TError);
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void OsekNm_Init(const OsekNm_ConfigType *configPtr) {
  NetworkHandleType NetId;
  const OsekNm_ChannelConfigType *config;
  OsekNm_ChannelContextType *context;
  (void)configPtr;
  for (NetId = 0; NetId < OSEKNM_CONFIG->NumOfChannels; NetId++) {
    config = &OSEKNM_CONFIG->ChannelConfigs[NetId];
    context = &OSEKNM_CONFIG->ChannelContexts[NetId];
    (void)memset(context, 0, sizeof(*context));
    context->NmState = OSEKNM_STATE_OFF;
    context->NmTxPdu.Source = config->NodeId;
  }
}

Std_ReturnType OsekNm_Start(NetworkHandleType netId) {
  Std_ReturnType ercd = E_NOT_OK;
  OsekNm_ChannelContextType *context;

  if (netId < OSEKNM_CONFIG->NumOfChannels) {
    context = &OSEKNM_CONFIG->ChannelContexts[netId];
    context->RequestOn = TRUE;
    ercd = E_OK;
  }

  return ercd;
}

Std_ReturnType OsekNm_Stop(NetworkHandleType netId) {
  Std_ReturnType ercd = E_NOT_OK;
  OsekNm_ChannelContextType *context;

  if (netId < OSEKNM_CONFIG->NumOfChannels) {
    context = &OSEKNM_CONFIG->ChannelContexts[netId];
    context->RequestOn = FALSE;
    ercd = E_OK;
  }

  return ercd;
}

Std_ReturnType OsekNm_GotoMode(NetworkHandleType netId, OsekNm_ModeNameType newMode) {
  Std_ReturnType ercd = E_OK;
  OsekNm_ChannelContextType *context;
  const OsekNm_ChannelConfigType *config;
  (void)config;

  if (netId < OSEKNM_CONFIG->NumOfChannels) {
    context = &OSEKNM_CONFIG->ChannelContexts[netId];
    config = &OSEKNM_CONFIG->ChannelConfigs[netId];
    if (OSEKNM_BUS_SLEEP == newMode) {
      context->Status.NetworkStatus |= OSEKNM_STATUS_BUS_SLEEP;
      context->RequestMode = OSEKNM_BUS_SLEEP;
    } else if (OSEKNM_AWAKE == newMode) {
      context->Status.NetworkStatus &= ~OSEKNM_STATUS_BUS_SLEEP;
      context->RequestMode = OSEKNM_AWAKE;
    } else {
      ercd = E_NOT_OK;
    }
  } else {
    ercd = E_NOT_OK;
  }
  return ercd;
}

Std_ReturnType OsekNm_NetworkRequest(NetworkHandleType netId) {
  Std_ReturnType ret = E_NOT_OK;
  OsekNm_ChannelContextType *context;

  if (netId < OSEKNM_CONFIG->NumOfChannels) {
    context = &OSEKNM_CONFIG->ChannelContexts[netId];
    context->RequestOn = TRUE;
    context->Status.NetworkStatus &= ~OSEKNM_STATUS_BUS_SLEEP;
    context->RequestMode = OSEKNM_AWAKE;
    context->Status.NetworkStatus |= OSEKNM_STATUS_ACTIVE;
    ret = E_OK;
  }

  return ret;
}

Std_ReturnType OsekNm_NetworkRelease(NetworkHandleType netId) {
  Std_ReturnType ret = E_NOT_OK;
  OsekNm_ChannelContextType *context;

  if (netId < OSEKNM_CONFIG->NumOfChannels) {
    context = &OSEKNM_CONFIG->ChannelContexts[netId];
    context->Status.NetworkStatus |= OSEKNM_STATUS_BUS_SLEEP;
    context->RequestMode = OSEKNM_BUS_SLEEP;
    ret = E_OK;
  }

  return ret;
}

Std_ReturnType OsekNm_Silent(NetworkHandleType netId) {
  Std_ReturnType ercd = E_OK;
  OsekNm_ChannelContextType *context;

  if (netId < OSEKNM_CONFIG->NumOfChannels) {
    context = &OSEKNM_CONFIG->ChannelContexts[netId];
    context->Status.NetworkStatus &= ~OSEKNM_STATUS_ACTIVE;
  } else {
    ercd = E_NOT_OK;
  }

  return ercd;
}

Std_ReturnType OsekNm_Talk(NetworkHandleType netId) {
  Std_ReturnType ercd = E_OK;
  OsekNm_ChannelContextType *context;

  if (netId < OSEKNM_CONFIG->NumOfChannels) {
    context = &OSEKNM_CONFIG->ChannelContexts[netId];
    context->Status.NetworkStatus |= OSEKNM_STATUS_ACTIVE;
  }

  return ercd;
}

void OsekNm_BusErrorIndication(NetworkHandleType netId) {
  OsekNm_ChannelContextType *context;
  const OsekNm_ChannelConfigType *config;
  (void)config;

  if (netId < OSEKNM_CONFIG->NumOfChannels) {
    context = &OSEKNM_CONFIG->ChannelContexts[netId];
    config = &OSEKNM_CONFIG->ChannelConfigs[netId];
    OsekNm_D_Offline(netId);
    OsekNm_D_Init(netId, OSEKNM_ROUTINE_BUS_RESTART);
    OsekNmSetAlarm(TError);
    context->NmState = OSEKNM_STATE_LIMPHOME;
  }
}

Std_ReturnType OsekNm_GetState(NetworkHandleType netId, Nm_ModeType *nmModePtr) {
  Std_ReturnType ret = E_OK;
  OsekNm_ChannelContextType *context;

  if (netId < OSEKNM_CONFIG->NumOfChannels) {
    context = &OSEKNM_CONFIG->ChannelContexts[netId];
    switch (context->NmState) {
    case OSEKNM_STATE_NORMAL_PREPARE_SLEEP:
    case OSEKNM_STATE_LIMPHOME_PREPARE_SLEEP:
      *nmModePtr = NM_MODE_PREPARE_BUS_SLEEP;
      break;
    case OSEKNM_STATE_OFF:
    case OSEKNM_STATE_BUS_SLEEP:
      *nmModePtr = NM_MODE_BUS_SLEEP;
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

void OsekNm_TxConfirmation(PduIdType netId, Std_ReturnType result) {
  OsekNm_ChannelContextType *context;
  const OsekNm_ChannelConfigType *config;
  (void)config;

  if (E_OK != result) {
    /* TK failed, do nothing */
  } else if (netId < OSEKNM_CONFIG->NumOfChannels) {
    context = &OSEKNM_CONFIG->ChannelContexts[netId];
    config = &OSEKNM_CONFIG->ChannelConfigs[netId];
    context->NmTxCount = 0;
    switch (context->NmState) {
    case OSEKNM_STATE_NORMAL: {
      if (0u != (context->NmTxPdu.OpCode & OSEKNM_MASK_RING)) {
        OsekNmCancelAlarm(TTyp);
        OsekNmCancelAlarm(TMax);
        OsekNmSetAlarm(TMax);
        if (0u != (context->NmTxPdu.OpCode & OSEKNM_MASK_SI)) {
          if (0u != (context->Status.NetworkStatus & OSEKNM_STATUS_BUS_SLEEP)) {
            context->NmTxPdu.OpCode |= OSEKNM_MASK_SA;
            ASLOG(OSEK_NM, ("[%d] Goto Normal Prepare Sleep\n", netId));
            context->NmState = OSEKNM_STATE_NORMAL_PREPARE_SLEEP;
          }
        }
      }
      break;
    }
    case OSEKNM_STATE_NORMAL_PREPARE_SLEEP: { /* 2 NMInitBusSleep */
      if (0u != (context->NmTxPdu.OpCode & OSEKNM_MASK_RING)) {
        OsekNm_D_Offline(netId);
        OsekNmSetAlarm(TWbs);
        ASLOG(OSEK_NM, ("[%d] Goto Normal Wait Bus Sleep\n", netId));
        context->NmState = OSEKNM_STATE_WAIT_BUS_SLEEP_NORMAL;
      }
      break;
    }
    case OSEKNM_STATE_LIMPHOME: {
      if (0u != (context->NmTxPdu.OpCode & OSEKNM_MASK_LIMPHOME)) {
        context->NmMerker |= OSEKNM_MERKER_LIMPHOME;
      }
      break;
    }
    default:
      break;
    }
  } else {
    /* bad NetId, do nothing */
  }
}

Std_ReturnType OsekNm_D_WindowDataReq(NetworkHandleType netId, OsekNm_PduType *nmPdu,
                                      uint8_t dataLengthTx) {
  Std_ReturnType ercd;
  PduInfoType PduInfo;
  const OsekNm_ChannelConfigType *config;
  config = &OSEKNM_CONFIG->ChannelConfigs[netId];

  PduInfo.SduLength = dataLengthTx;
  PduInfo.SduDataPtr = &nmPdu->Destination;
  PduInfo.MetaDataPtr = NULL;

  ercd = CanIf_Transmit(config->TxPduId, &PduInfo);

  if (E_OK == ercd) {
    ASLOG(OSEK_NM,
          ("[%d] TX Src=%02X Dst=%02X Type=%02X data=[%02X,%02X,%02X,%02X,%02X,%02X], state=%d \n",
           netId, nmPdu->Source, nmPdu->Destination, nmPdu->OpCode, nmPdu->RingData[0],
           nmPdu->RingData[1], nmPdu->RingData[2], nmPdu->RingData[3], nmPdu->RingData[4],
           nmPdu->RingData[5], OSEKNM_CONFIG->ChannelContexts[netId].NmState));
  }

  return ercd;
}

void OsekNm_RxIndication(PduIdType netId, const PduInfoType *pduInfoPtr) {
  OsekNm_ChannelContextType *context;
  const OsekNm_ChannelConfigType *config;
  OsekNm_PduType nmPdu;
  const Can_HwType *Mailbox = (const Can_HwType *)pduInfoPtr->MetaDataPtr;

  if (netId < OSEKNM_CONFIG->NumOfChannels) {
    context = &OSEKNM_CONFIG->ChannelContexts[netId];
    config = &OSEKNM_CONFIG->ChannelConfigs[netId];
    nmPdu.Source = Mailbox->CanId & config->NodeMask;
    (void)memcpy(&nmPdu.Destination, pduInfoPtr->SduDataPtr, 8);

    ASLOG(OSEK_NM,
          ("[%d] RX Src=%02X Dst=%02X Type=%02X data=[%02X,%02X,%02X,%02X,%02X,%02X], state=%d \n",
           netId, nmPdu.Source, nmPdu.Destination, nmPdu.OpCode, nmPdu.RingData[0],
           nmPdu.RingData[1], nmPdu.RingData[2], nmPdu.RingData[3], nmPdu.RingData[4],
           nmPdu.RingData[5], context->NmState));

    switch (context->NmState) {
    case OSEKNM_STATE_NORMAL: /* intend no break */
    case OSEKNM_STATE_NORMAL_PREPARE_SLEEP: {
      OsekNm_NormalStandard(netId, &nmPdu);
      if (0u != (nmPdu.OpCode & OSEKNM_MASK_SA)) {
        if (0u !=
            (context->Status.NetworkStatus & OSEKNM_STATUS_BUS_SLEEP)) { /* 2 NMInitBusSleep */
          OsekNm_D_Offline(netId);
          OsekNmSetAlarm(TWbs);
          ASLOG(OSEK_NM, ("[%d] Goto Normal Wait Bus Sleep\n", netId));
          context->NmState = OSEKNM_STATE_WAIT_BUS_SLEEP_NORMAL;
        }
      }
      /* Special process for OSEKNM_STATE_NORMAL_PREPARE_SLEEP only */
      if (OSEKNM_STATE_NORMAL_PREPARE_SLEEP == context->NmState) {
        if (0u != (nmPdu.OpCode & OSEKNM_MASK_SI)) {
        } else {
          ASLOG(OSEK_NM, ("[%d] Goto Normal\n", netId));
          context->NmState = OSEKNM_STATE_NORMAL;
        }
      }
      break;
    }
    case OSEKNM_STATE_WAIT_BUS_SLEEP_NORMAL: {
      if (0u != (nmPdu.OpCode & OSEKNM_MASK_SI)) { /* = 1 */
      } else {                                     /* = 0 */
        OsekNmCancelAlarm(TWbs);
        OsekNm_InitReset6(netId);
      }
      break;
    }
    case OSEKNM_STATE_BUS_SLEEP: /* for robustness, do wakeup */
      OsekNm_D_Init(netId, OSEKNM_ROUTINE_BUS_AWAKE);
      OsekNm_Init3(netId);
      break;
    case OSEKNM_STATE_LIMPHOME: {
      if (0u != (context->Status.NetworkStatus & OSEKNM_STATUS_ACTIVE)) {
        if (0u != (context->NmMerker & OSEKNM_MERKER_LIMPHOME)) {
          if ((0u != (context->Status.NetworkStatus & OSEKNM_STATUS_BUS_SLEEP)) &&
              (0u != (nmPdu.OpCode & OSEKNM_MASK_SA))) {
            OsekNm_D_Offline(netId);
            OsekNmSetAlarm(TWbs);
            context->NmState = OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME;
          } else {
            OsekNm_InitReset5(netId);
          }
        } else {
          if (0u != (nmPdu.OpCode & OSEKNM_MASK_SA)) {
            OsekNm_D_Offline(netId);
            OsekNmSetAlarm(TWbs);
            context->NmState = OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME;
          } else {
            OsekNm_InitReset5(netId);
          }
        }
      } else {
        if (0u != (context->NmMerker & OSEKNM_MERKER_LIMPHOME)) {
          if ((0u != (context->Status.NetworkStatus & OSEKNM_STATUS_BUS_SLEEP)) &&
              (0u != (nmPdu.OpCode & OSEKNM_MASK_SA))) {
            OsekNm_D_Offline(netId);
            OsekNmSetAlarm(TWbs);
            context->NmState = OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME;
          } else {
            OsekNm_InitReset5(netId);
          }
        } else {
          if (0u != (nmPdu.OpCode & OSEKNM_MASK_SA)) {
            OsekNm_D_Offline(netId);
            OsekNmSetAlarm(TWbs);
            context->NmState = OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME;
          } else {
            OsekNm_InitReset5(netId);
          }
        }
      }
      break;
    }
    case OSEKNM_STATE_LIMPHOME_PREPARE_SLEEP: {
      if (0u == (nmPdu.OpCode & OSEKNM_MASK_SI)) {
      } else {
        context->NmState = OSEKNM_STATE_LIMPHOME;
        OsekNmSetAlarm(TError);
      }
      break;
    }
    case OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME: {
      if (0u == (nmPdu.OpCode & OSEKNM_MASK_SI)) {
        OsekNmCancelAlarm(TWbs);
        context->NmState = OSEKNM_STATE_LIMPHOME;
        OsekNmSetAlarm(TError);
      } else {
      }
      break;
    }
    default:
      break;
    }
  }
}

void OsekNm_WakeupIndication(NetworkHandleType netId) {
  OsekNm_ChannelContextType *context;

  if (netId < OSEKNM_CONFIG->NumOfChannels) {
    context = &OSEKNM_CONFIG->ChannelContexts[netId];
    context->WakeupSignal = TRUE;
  }
}

void OsekNm_MainFunction(void) {
  NetworkHandleType NetId;
  const OsekNm_ChannelConfigType *config;
  OsekNm_ChannelContextType *context;
  for (NetId = 0; NetId < OSEKNM_CONFIG->NumOfChannels; NetId++) {
    config = &OSEKNM_CONFIG->ChannelConfigs[NetId];
    context = &OSEKNM_CONFIG->ChannelContexts[NetId];
    if (OsekNmIsAlarmStarted(TTx)) {
      OsekNmSignalAlarm(TTx);
      if (OsekNmIsAlarmTimeout(TTx)) {
        OsekNmCancelAlarm(TTx);
        OsekNmSendMessage();
        continue; /* skip the process of state */
      }
    }

    /* state machine according to nm253 Page 110 Figure 68 Right */
    switch (context->NmState) {
    case OSEKNM_STATE_OFF:
      OsekNm_OffMain(NetId);
      break;
    case OSEKNM_STATE_NORMAL:
      OsekNm_NormalMain(NetId);
      break;
    case OSEKNM_STATE_LIMPHOME:
      OsekNm_LimphomeMain(NetId);
      break;
    case OSEKNM_STATE_NORMAL_PREPARE_SLEEP:
      OsekNm_NormalPrepSleepMain(NetId);
      break;
    case OSEKNM_STATE_WAIT_BUS_SLEEP_NORMAL:
      OsekNm_NormalTwbsMain(NetId);
      break;
    case OSEKNM_STATE_BUS_SLEEP:
      OsekNm_BusSleepMain(NetId);
      break;
    case OSEKNM_STATE_LIMPHOME_PREPARE_SLEEP:
      OsekNm_LimphomePrepSleepMain(NetId);
      break;
    case OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME:
      OsekNm_TwbsLimphomeMain(NetId);
      break;
    default:
      break;
    }

    if (OSEKNM_STATE_OFF != context->NmState) {
      if (FALSE == context->RequestOn) {
        OsekNm_D_Init(NetId, OSEKNM_ROUTINE_BUS_SHUTDOWN);
        context->NmState = OSEKNM_STATE_OFF;
      }
    }
  }
}

void OsekNm_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = 0;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 0;
}
