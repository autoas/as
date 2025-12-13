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
#define nmSetAlarm(Timer)                                                                          \
  Std_TimerInit(&context->Alarm._##Timer, config->_##Timer * 1000 * OSEKNM_MAIN_FUNCTION_PERIOD)
#define nmSingalAlarm(Timer)
#define nmIsAlarmTimeout(Timer) Std_IsTimerTimeout(&context->Alarm._##Timer)
#define nmIsAlarmStarted(Timer) Std_IsTimerStarted(&context->Alarm._##Timer)
#define nmCancelAlarm(Timer) Std_TimerStop(&context->Alarm._##Timer)
#else
/* Alarm Management */
#define nmSetAlarm(Timer)                                                                          \
  do {                                                                                             \
    context->Alarm._##Timer = 1u + config->_##Timer;                                               \
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
    context->Alarm._##Timer = 0u;                                                                  \
  } while (0)
#endif

#define nmSendMessage()                                                                            \
  do {                                                                                             \
    Std_ReturnType ercd;                                                                           \
    ercd = OsekNm_D_WindowDataReq(NetId, &(context->nmTxPdu), 8u);                                 \
    if (ercd != E_OK) {                                                                            \
      nmSetAlarm(TTx); /* re-Transmit after TTx */                                                 \
    }                                                                                              \
  } while (0)

#define NMInit(NetId)

#define NM_CONFIG (&OsekNm_Config)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const OsekNm_ConfigType OsekNm_Config;

static void nmInit3(NetworkHandleType NetId);
static void nmAddtoPresent(NetworkHandleType NetId, OsekNm_NodeIdType NodeId);
static void nmAddtoLimphome(NetworkHandleType NetId, OsekNm_NodeIdType NodeId);
static void nmInitReset5(NetworkHandleType NetId);
static void nmInitReset6(NetworkHandleType NetId);
static void nmNormalStandard(NetworkHandleType NetId, OsekNm_PduType *nmPdu);
static OsekNm_NodeIdType nmDetermineLS(OsekNm_NodeIdType S, OsekNm_NodeIdType R,
                                       OsekNm_NodeIdType L);
static boolean nmIsMeSkipped(OsekNm_NodeIdType S, OsekNm_NodeIdType R, OsekNm_NodeIdType D);
static void nmBusSleep(NetworkHandleType NetId);
static void nmNormalTwbsMain(NetworkHandleType NetId);
static void nmNormalPrepSleepMain(NetworkHandleType NetId);
static void nmLimphomeMain(NetworkHandleType NetId);
static void nmLimphomePrepSleepMain(NetworkHandleType NetId);
static void nmNormalMain(NetworkHandleType NetId);
static void nmTwbsLimphomeMain(NetworkHandleType NetId);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* nmInit3 is special doesn't mean all the operation of "Leave NMBusSleep" */
static void nmInit3(NetworkHandleType NetId) {
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];
  /* step 3: config.limphome  := 0 */
  /* step 4: */
  context->nmRxCount = 0;
  context->nmTxCount = 0;
  OsekNm_D_Online(NetId);
  nmInitReset6(NetId);
}

static void nmAddtoPresent(NetworkHandleType NetId, OsekNm_NodeIdType NodeId) {
  /* TODO: */
  (void)NetId;
  (void)NodeId;
}

static void nmAddtoLimphome(NetworkHandleType NetId, OsekNm_NodeIdType NodeId) {
  /* TODO: */
  (void)NetId;
  (void)NodeId;
}

static void nmInitReset5(NetworkHandleType NetId) {
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];

  nmCancelAlarm(TError);
  context->nmMerker &= ~OSEKNM_MERKER_LIMPHOME;

  context->nmRxCount = 0;
  context->nmTxCount = 0;
  OsekNm_D_Online(NetId);
  nmInitReset6(NetId);
}

static void nmInitReset6(NetworkHandleType NetId) {
  const OsekNm_ChannelConfigType *config = &NM_CONFIG->channelConfigs[NetId];
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];

  context->nmTxPdu.Source = config->NodeId;
  /* config.present = own station */
  nmAddtoPresent(NetId, config->NodeId);
  /* logical successor := own station */
  context->nmTxPdu.Destination = config->NodeId;
  context->nmRxCount += 1u;
  /* Initialise the nmPdu(Data,OpCode) */
  context->nmTxPdu.OpCode = 0;
  (void)memset(context->nmTxPdu.RingData, 0, sizeof(OsekNm_RingDataType));
  /* Cancel all Alarm */
  nmCancelAlarm(TTx);
  nmCancelAlarm(TTyp);
  nmCancelAlarm(TMax);
  nmCancelAlarm(TWbs);
  nmCancelAlarm(TError);
  if (0u != (context->nmStatus.NetworkStatus & OSEKNM_STATUS_ACTIVE)) {
    context->nmTxCount += 1u;
    /* Send A alive message */
    context->nmTxPdu.OpCode = OSEKNM_MASK_ALIVE;
    nmSendMessage();
    ASLOG(OSEK_NM, ("[%d] nmInitReset6:Send Alive\n", NetId));
  }
  if ((context->nmTxCount <= config->tx_limit) && (context->nmRxCount <= config->rx_limit)) {
    nmSetAlarm(TTyp);
    context->nmState = OSEKNM_STATE_NORMAL;
    ASLOG(OSEK_NM, ("[%d] Set TTyp, enter Normal state.\n", NetId));
  } else {
    nmSetAlarm(TError);
    context->nmState = OSEKNM_STATE_LIMPHOME;
    ASLOG(OSEK_NM, ("[%d] Set TError, Enter Limphome state.\n", NetId));
  }
}

static void nmNormalStandard(NetworkHandleType NetId, OsekNm_PduType *nmPdu) {
  const OsekNm_ChannelConfigType *config = &NM_CONFIG->channelConfigs[NetId];
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];
  boolean bIsMeSkipped;
  context->nmRxCount = 0;
  if (0u != (nmPdu->OpCode & OSEKNM_MASK_LIMPHOME)) {
    /* add sender to config.limphome */
    nmAddtoLimphome(NetId, nmPdu->Source);
  } else {
    /* add sender to config.present */
    nmAddtoPresent(NetId, nmPdu->Source);
    /* determine logical successor */
    context->nmTxPdu.Destination =
      nmDetermineLS(nmPdu->Source, config->NodeId, context->nmTxPdu.Destination);
    if (0u != (nmPdu->OpCode & OSEKNM_MASK_RING)) {
      nmCancelAlarm(TTyp);
      nmCancelAlarm(TMax);
      /* see nm253 Figure 4 Mechanism to transfer application data via the logical ring */
      (void)memcpy(context->nmTxPdu.RingData, nmPdu->RingData, sizeof(OsekNm_RingDataType));
      if ((nmPdu->Destination == config->NodeId)      /* to me */
          || (nmPdu->Destination == nmPdu->Source)) { /* or D = S */
        nmSetAlarm(TTyp);
        ASLOG(OSEK_NM, ("[%d] Set TTyp\n", NetId));
        /* Do Ring Data indication */
        /* TODO */
      } else {
        nmSetAlarm(TMax);
        bIsMeSkipped = nmIsMeSkipped(nmPdu->Source, config->NodeId, nmPdu->Destination);
        if (TRUE == bIsMeSkipped) {
          if (0u != (context->nmStatus.NetworkStatus & OSEKNM_STATUS_ACTIVE)) {
            context->nmTxPdu.OpCode = OSEKNM_MASK_ALIVE;
            context->nmTxPdu.Destination = config->NodeId;
            if (0u != (context->nmStatus.NetworkStatus & OSEKNM_STATUS_BUS_SLEEP)) {
              context->nmTxPdu.OpCode |= OSEKNM_MASK_SI;
            }
            nmSendMessage();
          }
        }
      }
    }
  }
}

static OsekNm_NodeIdType nmDetermineLS(OsekNm_NodeIdType S, OsekNm_NodeIdType R,
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

static boolean nmIsMeSkipped(OsekNm_NodeIdType S, OsekNm_NodeIdType R, OsekNm_NodeIdType D) {
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

static void nmOffMain(NetworkHandleType NetId) {
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];
  if (TRUE == context->bRequestOn) { /* see nm253.pdf Figure 30 Start-up of the network */
    /* step 1: */
    context->nmTxPdu.OpCode &= ~(OSEKNM_MASK_SI | OSEKNM_MASK_SA);
    /* step 2: enter NMInit */
    NMInit(NetId);
    OsekNm_D_Init(NetId, OSEKNM_ROUTINE_BUS_INIT);
    context->nmState = OSEKNM_STATE_BUS_SLEEP;
  }
}

static void nmBusSleepMain(NetworkHandleType NetId) {
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];

  if ((OSEKNM_AWAKE == context->requestMode) || (TRUE == context->bWakeupSignal)) {
    context->bWakeupSignal = FALSE; /* always set it FALSE on startup */
    OsekNm_D_Init(NetId, OSEKNM_ROUTINE_BUS_AWAKE);
    nmInit3(NetId);
  }
}

static void nmNormalMain(NetworkHandleType NetId) {
  const OsekNm_ChannelConfigType *config = &NM_CONFIG->channelConfigs[NetId];
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];
  if (nmIsAlarmStarted(TTyp)) {
    nmSingalAlarm(TTyp);
    if (nmIsAlarmTimeout(TTyp)) {
      nmCancelAlarm(TTyp);
      nmCancelAlarm(TMax);
      nmSetAlarm(TMax);
      if (0u != (context->nmStatus.NetworkStatus & OSEKNM_STATUS_ACTIVE)) {
        context->nmTxPdu.OpCode = OSEKNM_MASK_RING;
        if (0u != (context->nmStatus.NetworkStatus & OSEKNM_STATUS_BUS_SLEEP)) {
          context->nmTxPdu.OpCode |= OSEKNM_MASK_SI;
        }
        context->nmTxCount++;
        nmSendMessage();
      } else {
      }
      if (context->nmTxCount > config->tx_limit) {
        ASLOG(OSEK_NM, ("[%d] Goto limphome mode\n", NetId));
        context->nmState = OSEKNM_STATE_LIMPHOME;
        nmSetAlarm(TError);
      } else {
        if (0u != (context->nmMerker & OSEKNM_MERKER_STABLE)) {
          context->nmStatus.NetworkStatus |= OSEKNM_STATUS_CONFIGURATION_STABLE;
        } else {
          context->nmMerker |= OSEKNM_MERKER_STABLE;
        }
      }
    }
  }

  if (nmIsAlarmStarted(TMax)) {
    nmSingalAlarm(TMax);
    if (nmIsAlarmTimeout(TMax)) {
      nmCancelAlarm(TMax);
      nmInitReset6(NetId);
    }
  }

  if (OSEKNM_BUS_SLEEP == context->requestMode) {
    if (0u != (context->nmStatus.NetworkStatus & OSEKNM_STATUS_ACTIVE)) {
      ASLOG(OSEK_NM, ("[%d] GotoMode_BusSleep, Keep in Normal as active.\n", NetId));
    } else {
      ASLOG(OSEK_NM, ("[%d] GotoMode_BusSleep, Enter NormalPrepSleep.\n", NetId));
      context->nmState = OSEKNM_STATE_NORMAL_PREPARE_SLEEP;
    }
  }
}

static void nmLimphomeMain(NetworkHandleType NetId) {
  const OsekNm_ChannelConfigType *config = &NM_CONFIG->channelConfigs[NetId];
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];
  if (nmIsAlarmStarted(TError)) {
    nmSingalAlarm(TError);
    if (nmIsAlarmTimeout(TError)) {
      nmCancelAlarm(TError);
      OsekNm_D_Online(NetId);
      context->nmTxPdu.OpCode = OSEKNM_MASK_LIMPHOME;
      if (0u != (context->nmStatus.NetworkStatus & OSEKNM_STATUS_BUS_SLEEP)) {
        nmSetAlarm(TMax);
        context->nmTxPdu.OpCode |= OSEKNM_MASK_SI;
        context->nmState = OSEKNM_STATE_LIMPHOME_PREPARE_SLEEP;
        ASLOG(OSEK_NM, ("[%d] Goto Limphome Prepare Sleep\n", NetId));
      } else {
        nmSetAlarm(TError);
      }
      if (0u != (context->nmStatus.NetworkStatus & OSEKNM_STATUS_ACTIVE)) {
        nmSendMessage();
      }
    }
  } else { /* for roboustness, auto start TError if it was not started */
    nmSetAlarm(TError);
  }
}
static void nmNormalPrepSleepMain(NetworkHandleType NetId) {
  const OsekNm_ChannelConfigType *config = &NM_CONFIG->channelConfigs[NetId];
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];
  if (nmIsAlarmStarted(TTyp)) {
    nmSingalAlarm(TTyp);
    if (nmIsAlarmTimeout(TTyp)) {
      nmCancelAlarm(TTyp);
      if (0u != (context->nmStatus.NetworkStatus & OSEKNM_STATUS_ACTIVE)) {
        /* Send ring message with set sleep.ack bit */
        context->nmTxPdu.OpCode = OSEKNM_MASK_RING | OSEKNM_MASK_SI | OSEKNM_MASK_SA;
        nmSendMessage();
      } else {
        OsekNm_D_Offline(NetId);
        nmSetAlarm(TWbs);
        context->nmState = OSEKNM_STATE_WAIT_BUS_SLEEP_NORMAL;
        ASLOG(OSEK_NM, ("[%d] Goto Normal Wait Bus Sleep as not active\n", NetId));
      }
    }
  } else {
    if (nmIsAlarmStarted(TMax)) {
      nmSingalAlarm(TMax);
      if (nmIsAlarmTimeout(TMax)) {
        nmCancelAlarm(TMax);
        OsekNm_D_Offline(NetId);
        nmSetAlarm(TWbs);
        context->nmState = OSEKNM_STATE_WAIT_BUS_SLEEP_NORMAL;
        ASLOG(OSEK_NM, ("[%d] Goto Normal Wait Bus Sleep as TMax timeout\n", NetId));
      }
    }
  }

  if (OSEKNM_AWAKE == context->requestMode) {
    ASLOG(OSEK_NM, ("[%d] GotoMode_Awake, Enter Normal\n", NetId));
    context->nmState = OSEKNM_STATE_NORMAL;
  }
}

static void nmBusSleep(NetworkHandleType NetId) {
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];
  OsekNm_D_Init(NetId, OSEKNM_ROUTINE_BUS_SLEEP);
  context->nmState = OSEKNM_STATE_BUS_SLEEP;
  ASLOG(OSEK_NM, ("[%d] Goto Bus Sleep\n", NetId));
}

static void nmNormalTwbsMain(NetworkHandleType NetId) {
  const OsekNm_ChannelConfigType *config = &NM_CONFIG->channelConfigs[NetId];
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];
  (void)config;
  if (nmIsAlarmStarted(TWbs)) {
    nmSingalAlarm(TWbs);
    if (nmIsAlarmTimeout(TWbs)) {
      nmCancelAlarm(TWbs);
      nmBusSleep(NetId);
    }
  }

  if (OSEKNM_AWAKE == context->requestMode) {
    ASLOG(OSEK_NM, ("[%d] GotoMode_Awake, Exit Normal Wait Bus Sleep\n", NetId));
    nmCancelAlarm(TWbs);
    nmInitReset6(NetId);
  }
}

static void nmLimphomePrepSleepMain(NetworkHandleType NetId) {
  const OsekNm_ChannelConfigType *config = &NM_CONFIG->channelConfigs[NetId];
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];
  if (nmIsAlarmStarted(TMax)) {
    nmSingalAlarm(TMax);
    if (nmIsAlarmTimeout(TMax)) {
      nmCancelAlarm(TMax);
      /* 7 NMInitBusSleep */
      OsekNm_D_Offline(NetId);
      nmSetAlarm(TWbs);
      context->nmState = OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME;
      ASLOG(OSEK_NM, ("[%d] Goto Limphome Wait Bus Sleep\n", NetId));
    }
  }

  if (OSEKNM_AWAKE == context->requestMode) {
    ASLOG(OSEK_NM, ("[%d] GotoMode_Awake, Enter Limphome\n", NetId));
    context->nmState = OSEKNM_STATE_LIMPHOME;
    nmSetAlarm(TError);
  }
}

static void nmTwbsLimphomeMain(NetworkHandleType NetId) {
  const OsekNm_ChannelConfigType *config = &NM_CONFIG->channelConfigs[NetId];
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];
  (void)config;
  if (nmIsAlarmStarted(TWbs)) {
    nmSingalAlarm(TWbs);
    if (nmIsAlarmTimeout(TWbs)) {
      nmCancelAlarm(TWbs);
      nmBusSleep(NetId);
    }
  }

  if (OSEKNM_AWAKE == context->requestMode) {
    ASLOG(OSEK_NM, ("[%d] GotoMode_Awake, Enter Limphome\n", NetId));
    nmCancelAlarm(TWbs);
    context->nmState = OSEKNM_STATE_LIMPHOME;
    nmSetAlarm(TError);
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void OsekNm_Init(const OsekNm_ConfigType *ConfigPtr) {
  NetworkHandleType NetId;
  const OsekNm_ChannelConfigType *config;
  OsekNm_ChannelContextType *context;
  (void)ConfigPtr;
  for (NetId = 0; NetId < NM_CONFIG->numOfChannels; NetId++) {
    config = &NM_CONFIG->channelConfigs[NetId];
    context = &NM_CONFIG->channelContexts[NetId];
    (void)memset(context, 0, sizeof(*context));
    context->nmState = OSEKNM_STATE_OFF;
    context->nmTxPdu.Source = config->NodeId;
  }
}

Std_ReturnType OsekNm_Start(NetworkHandleType NetId) {
  Std_ReturnType ercd = E_NOT_OK;
  OsekNm_ChannelContextType *context;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    context->bRequestOn = TRUE;
    ercd = E_OK;
  }

  return ercd;
}

Std_ReturnType OsekNm_Stop(NetworkHandleType NetId) {
  Std_ReturnType ercd = E_NOT_OK;
  OsekNm_ChannelContextType *context;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    context->bRequestOn = FALSE;
    ercd = E_OK;
  }

  return ercd;
}

Std_ReturnType OsekNm_GotoMode(NetworkHandleType NetId, OsekNm_ModeNameType NewMode) {
  Std_ReturnType ercd = E_OK;
  OsekNm_ChannelContextType *context;
  const OsekNm_ChannelConfigType *config;
  (void)config;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    config = &NM_CONFIG->channelConfigs[NetId];
    if (OSEKNM_BUS_SLEEP == NewMode) {
      context->nmStatus.NetworkStatus |= OSEKNM_STATUS_BUS_SLEEP;
      context->requestMode = OSEKNM_BUS_SLEEP;
    } else if (OSEKNM_AWAKE == NewMode) {
      context->nmStatus.NetworkStatus &= ~OSEKNM_STATUS_BUS_SLEEP;
      context->requestMode = OSEKNM_AWAKE;
    } else {
      ercd = E_NOT_OK;
    }
  } else {
    ercd = E_NOT_OK;
  }
  return ercd;
}

Std_ReturnType OsekNm_NetworkRequest(NetworkHandleType NetId) {
  Std_ReturnType ret = E_NOT_OK;
  OsekNm_ChannelContextType *context;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    context->bRequestOn = TRUE;
    context->nmStatus.NetworkStatus &= ~OSEKNM_STATUS_BUS_SLEEP;
    context->requestMode = OSEKNM_AWAKE;
    context->nmStatus.NetworkStatus |= OSEKNM_STATUS_ACTIVE;
    ret = E_OK;
  }

  return ret;
}

Std_ReturnType OsekNm_NetworkRelease(NetworkHandleType NetId) {
  Std_ReturnType ret = E_NOT_OK;
  OsekNm_ChannelContextType *context;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    context->nmStatus.NetworkStatus |= OSEKNM_STATUS_BUS_SLEEP;
    context->requestMode = OSEKNM_BUS_SLEEP;
    ret = E_OK;
  }

  return ret;
}

Std_ReturnType OsekNm_Silent(NetworkHandleType NetId) {
  Std_ReturnType ercd = E_OK;
  OsekNm_ChannelContextType *context;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    context->nmStatus.NetworkStatus &= ~OSEKNM_STATUS_ACTIVE;
  } else {
    ercd = E_NOT_OK;
  }

  return ercd;
}

Std_ReturnType OsekNm_Talk(NetworkHandleType NetId) {
  Std_ReturnType ercd = E_OK;
  OsekNm_ChannelContextType *context;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    context->nmStatus.NetworkStatus |= OSEKNM_STATUS_ACTIVE;
  }

  return ercd;
}

void OsekNm_BusErrorIndication(NetworkHandleType NetId) {
  OsekNm_ChannelContextType *context;
  const OsekNm_ChannelConfigType *config;
  (void)config;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    config = &NM_CONFIG->channelConfigs[NetId];
    OsekNm_D_Offline(NetId);
    OsekNm_D_Init(NetId, OSEKNM_ROUTINE_BUS_RESTART);
    nmSetAlarm(TError);
    context->nmState = OSEKNM_STATE_LIMPHOME;
  }
}

Std_ReturnType OsekNm_GetState(NetworkHandleType NetId, Nm_ModeType *nmModePtr) {
  Std_ReturnType ret = E_OK;
  OsekNm_ChannelContextType *context;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    switch (context->nmState) {
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

void OsekNm_TxConfirmation(PduIdType NetId, Std_ReturnType result) {
  OsekNm_ChannelContextType *context;
  const OsekNm_ChannelConfigType *config;
  (void)config;

  if (E_OK != result) {
    /* TK failed, do nothing */
  } else if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    config = &NM_CONFIG->channelConfigs[NetId];
    context->nmTxCount = 0;
    switch (context->nmState) {
    case OSEKNM_STATE_NORMAL: {
      if (0u != (context->nmTxPdu.OpCode & OSEKNM_MASK_RING)) {
        nmCancelAlarm(TTyp);
        nmCancelAlarm(TMax);
        nmSetAlarm(TMax);
        if (0u != (context->nmTxPdu.OpCode & OSEKNM_MASK_SI)) {
          if (0u != (context->nmStatus.NetworkStatus & OSEKNM_STATUS_BUS_SLEEP)) {
            context->nmTxPdu.OpCode |= OSEKNM_MASK_SA;
            ASLOG(OSEK_NM, ("[%d] Goto Normal Prepare Sleep\n", NetId));
            context->nmState = OSEKNM_STATE_NORMAL_PREPARE_SLEEP;
          }
        }
      }
      break;
    }
    case OSEKNM_STATE_NORMAL_PREPARE_SLEEP: { /* 2 NMInitBusSleep */
      if (0u != (context->nmTxPdu.OpCode & OSEKNM_MASK_RING)) {
        OsekNm_D_Offline(NetId);
        nmSetAlarm(TWbs);
        ASLOG(OSEK_NM, ("[%d] Goto Normal Wait Bus Sleep\n", NetId));
        context->nmState = OSEKNM_STATE_WAIT_BUS_SLEEP_NORMAL;
      }
      break;
    }
    case OSEKNM_STATE_LIMPHOME: {
      if (0u != (context->nmTxPdu.OpCode & OSEKNM_MASK_LIMPHOME)) {
        context->nmMerker |= OSEKNM_MERKER_LIMPHOME;
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

Std_ReturnType OsekNm_D_WindowDataReq(NetworkHandleType NetId, OsekNm_PduType *nmPdu,
                                      uint8_t DataLengthTx) {
  Std_ReturnType ercd;
  PduInfoType PduInfo;
  const OsekNm_ChannelConfigType *config;
  config = &NM_CONFIG->channelConfigs[NetId];

  PduInfo.SduLength = DataLengthTx;
  PduInfo.SduDataPtr = &nmPdu->Destination;
  PduInfo.MetaDataPtr = NULL;

  ercd = CanIf_Transmit(config->txPduId, &PduInfo);

  if (E_OK == ercd) {
    ASLOG(OSEK_NM,
          ("[%d] TX Src=%02X Dst=%02X Type=%02X data=[%02X,%02X,%02X,%02X,%02X,%02X], state=%d \n",
           NetId, nmPdu->Source, nmPdu->Destination, nmPdu->OpCode, nmPdu->RingData[0],
           nmPdu->RingData[1], nmPdu->RingData[2], nmPdu->RingData[3], nmPdu->RingData[4],
           nmPdu->RingData[5], NM_CONFIG->channelContexts[NetId].nmState));
  }

  return ercd;
}

void OsekNm_RxIndication(PduIdType NetId, const PduInfoType *PduInfoPtr) {
  OsekNm_ChannelContextType *context;
  const OsekNm_ChannelConfigType *config;
  OsekNm_PduType nmPdu;
  const Can_HwType *Mailbox = (const Can_HwType *)PduInfoPtr->MetaDataPtr;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    config = &NM_CONFIG->channelConfigs[NetId];
    nmPdu.Source = Mailbox->CanId & config->NodeMask;
    (void)memcpy(&nmPdu.Destination, PduInfoPtr->SduDataPtr, 8);

    ASLOG(OSEK_NM,
          ("[%d] RX Src=%02X Dst=%02X Type=%02X data=[%02X,%02X,%02X,%02X,%02X,%02X], state=%d \n",
           NetId, nmPdu.Source, nmPdu.Destination, nmPdu.OpCode, nmPdu.RingData[0],
           nmPdu.RingData[1], nmPdu.RingData[2], nmPdu.RingData[3], nmPdu.RingData[4],
           nmPdu.RingData[5], context->nmState));

    switch (context->nmState) {
    case OSEKNM_STATE_NORMAL: /* intend no break */
    case OSEKNM_STATE_NORMAL_PREPARE_SLEEP: {
      nmNormalStandard(NetId, &nmPdu);
      if (0u != (nmPdu.OpCode & OSEKNM_MASK_SA)) {
        if (0u !=
            (context->nmStatus.NetworkStatus & OSEKNM_STATUS_BUS_SLEEP)) { /* 2 NMInitBusSleep */
          OsekNm_D_Offline(NetId);
          nmSetAlarm(TWbs);
          ASLOG(OSEK_NM, ("[%d] Goto Normal Wait Bus Sleep\n", NetId));
          context->nmState = OSEKNM_STATE_WAIT_BUS_SLEEP_NORMAL;
        }
      }
      /* Special process for OSEKNM_STATE_NORMAL_PREPARE_SLEEP only */
      if (OSEKNM_STATE_NORMAL_PREPARE_SLEEP == context->nmState) {
        if (0u != (nmPdu.OpCode & OSEKNM_MASK_SI)) {
        } else {
          ASLOG(OSEK_NM, ("[%d] Goto Normal\n", NetId));
          context->nmState = OSEKNM_STATE_NORMAL;
        }
      }
      break;
    }
    case OSEKNM_STATE_WAIT_BUS_SLEEP_NORMAL: {
      if (0u != (nmPdu.OpCode & OSEKNM_MASK_SI)) { /* = 1 */
      } else {                                     /* = 0 */
        nmCancelAlarm(TWbs);
        nmInitReset6(NetId);
      }
      break;
    }
    case OSEKNM_STATE_BUS_SLEEP: /* for robustness, do wakeup */
      OsekNm_D_Init(NetId, OSEKNM_ROUTINE_BUS_AWAKE);
      nmInit3(NetId);
      break;
    case OSEKNM_STATE_LIMPHOME: {
      if (0u != (context->nmStatus.NetworkStatus & OSEKNM_STATUS_ACTIVE)) {
        if (0u != (context->nmMerker & OSEKNM_MERKER_LIMPHOME)) {
          if ((0u != (context->nmStatus.NetworkStatus & OSEKNM_STATUS_BUS_SLEEP)) &&
              (0u != (nmPdu.OpCode & OSEKNM_MASK_SA))) {
            OsekNm_D_Offline(NetId);
            nmSetAlarm(TWbs);
            context->nmState = OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME;
          } else {
            nmInitReset5(NetId);
          }
        } else {
          if (0u != (nmPdu.OpCode & OSEKNM_MASK_SA)) {
            OsekNm_D_Offline(NetId);
            nmSetAlarm(TWbs);
            context->nmState = OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME;
          } else {
            nmInitReset5(NetId);
          }
        }
      } else {
        if (0u != (context->nmMerker & OSEKNM_MERKER_LIMPHOME)) {
          if ((0u != (context->nmStatus.NetworkStatus & OSEKNM_STATUS_BUS_SLEEP)) &&
              (0u != (nmPdu.OpCode & OSEKNM_MASK_SA))) {
            OsekNm_D_Offline(NetId);
            nmSetAlarm(TWbs);
            context->nmState = OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME;
          } else {
            nmInitReset5(NetId);
          }
        } else {
          if (0u != (nmPdu.OpCode & OSEKNM_MASK_SA)) {
            OsekNm_D_Offline(NetId);
            nmSetAlarm(TWbs);
            context->nmState = OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME;
          } else {
            nmInitReset5(NetId);
          }
        }
      }
      break;
    }
    case OSEKNM_STATE_LIMPHOME_PREPARE_SLEEP: {
      if (0u == (nmPdu.OpCode & OSEKNM_MASK_SI)) {
      } else {
        context->nmState = OSEKNM_STATE_LIMPHOME;
        nmSetAlarm(TError);
      }
      break;
    }
    case OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME: {
      if (0u == (nmPdu.OpCode & OSEKNM_MASK_SI)) {
        nmCancelAlarm(TWbs);
        context->nmState = OSEKNM_STATE_LIMPHOME;
        nmSetAlarm(TError);
      } else {
      }
      break;
    }
    default:
      break;
    }
  }
}

void OsekNm_WakeupIndication(NetworkHandleType NetId) {
  OsekNm_ChannelContextType *context;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    context->bWakeupSignal = TRUE;
  }
}

void OsekNm_MainFunction(void) {
  NetworkHandleType NetId;
  const OsekNm_ChannelConfigType *config;
  OsekNm_ChannelContextType *context;
  for (NetId = 0; NetId < NM_CONFIG->numOfChannels; NetId++) {
    config = &NM_CONFIG->channelConfigs[NetId];
    context = &NM_CONFIG->channelContexts[NetId];
    if (nmIsAlarmStarted(TTx)) {
      nmSingalAlarm(TTx);
      if (nmIsAlarmTimeout(TTx)) {
        nmCancelAlarm(TTx);
        nmSendMessage();
        continue; /* skip the process of state */
      }
    }

    /* state machine according to nm253 Page 110 Figure 68 Right */
    switch (context->nmState) {
    case OSEKNM_STATE_OFF:
      nmOffMain(NetId);
      break;
    case OSEKNM_STATE_NORMAL:
      nmNormalMain(NetId);
      break;
    case OSEKNM_STATE_LIMPHOME:
      nmLimphomeMain(NetId);
      break;
    case OSEKNM_STATE_NORMAL_PREPARE_SLEEP:
      nmNormalPrepSleepMain(NetId);
      break;
    case OSEKNM_STATE_WAIT_BUS_SLEEP_NORMAL:
      nmNormalTwbsMain(NetId);
      break;
    case OSEKNM_STATE_BUS_SLEEP:
      nmBusSleepMain(NetId);
      break;
    case OSEKNM_STATE_LIMPHOME_PREPARE_SLEEP:
      nmLimphomePrepSleepMain(NetId);
      break;
    case OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME:
      nmTwbsLimphomeMain(NetId);
      break;
    default:
      break;
    }

    if (OSEKNM_STATE_OFF != context->nmState) {
      if (FALSE == context->bRequestOn) {
        OsekNm_D_Init(NetId, OSEKNM_ROUTINE_BUS_SHUTDOWN);
        context->nmState = OSEKNM_STATE_OFF;
      }
    }
  }
}

void OsekNm_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  versionInfo->vendorID = STD_VENDOR_ID_AS;
  versionInfo->moduleID = 0;
  versionInfo->sw_major_version = 4;
  versionInfo->sw_minor_version = 0;
  versionInfo->sw_patch_version = 0;
}
