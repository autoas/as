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
#define nmDebug(str) ASLOG(OSEK_NM, (str))

#ifndef OSEKNM_MAIN_FUNCTION_PERIOD
#define OSEKNM_MAIN_FUNCTION_PERIOD 10
#endif

/* Macros for OpCode */
#define NM_MaskAlive 0x01
#define NM_MaskRing 0x02
#define NM_MaskLimphome 0x04
#define NM_MaskSI 0x10 /* sleep ind */
#define NM_MaskSA 0x20 /* sleep ack */

#ifdef _WIN32
#define nmSetAlarm(Timer)                                                                          \
  Std_TimerSet(&context->Alarm._##Timer, config->_##Timer * 1000 * OSEKNM_MAIN_FUNCTION_PERIOD)
#define nmSingalAlarm(Timer)
#define nmIsAlarmTimeout(Timer) Std_IsTimerTimeout(&context->Alarm._##Timer)
#define nmIsAlarmStarted(Timer) Std_IsTimerStarted(&context->Alarm._##Timer)
#define nmCancelAlarm(Timer) Std_TimerStop(&context->Alarm._##Timer)
#else
/* Alarm Management */
#define nmSetAlarm(Timer)                                                                          \
  do {                                                                                             \
    context->Alarm._##Timer = 1 + config->_##Timer;                                                \
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

#define nmSendMessage()                                                                            \
  do {                                                                                             \
    StatusType ercd;                                                                               \
    ercd = D_WindowDataReq(NetId, &(context->nmTxPdu), 8);                                         \
    if (ercd != E_OK) {                                                                            \
      nmSetAlarm(TTx); /* re-Transmit after TTx */                                                 \
    }                                                                                              \
  } while (0)

#define NMInit(NetId)

#define NM_CONFIG (&OsekNm_Config)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const OsekNm_ConfigType OsekNm_Config;

static void nmInit3(NetIdType NetId);
static void nmAddtoPresent(NetIdType NetId, NodeIdType NodeId);
static void nmAddtoLimphome(NetIdType NetId, NodeIdType NodeId);
static void nmInitReset5(NetIdType NetId);
static void nmInitReset6(NetIdType NetId);
static void nmNormalStandard(NetIdType NetId, NMPduType *nmPdu);
static NodeIdType nmDetermineLS(NodeIdType S, NodeIdType R, NodeIdType L);
static boolean nmIsMeSkipped(NodeIdType S, NodeIdType R, NodeIdType D);
static void nmBusSleep(NetIdType NetId);
static void nmNormalTwbsMain(NetIdType NetId);
static void nmNormalPrepSleepMain(NetIdType NetId);
static void nmLimphomeMain(NetIdType NetId);
static void nmLimphomePrepSleepMain(NetIdType NetId);
static void nmNormalMain(NetIdType NetId);
static void nmTwbsLimphomeMain(NetIdType NetId);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* nmInit3 is special doesn't mean all the operation of "Leave NMBusSleep" */
static void nmInit3(NetIdType NetId) {
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];
  /* step 3: config.limphome  := 0 */
  /* step 4: */
  context->nmRxCount = 0;
  context->nmTxCount = 0;
  D_Online(NetId);
  nmInitReset6(NetId);
}

static void nmAddtoPresent(NetIdType NetId, NodeIdType NodeId) {
  /* TODO: */
}

static void nmAddtoLimphome(NetIdType NetId, NodeIdType NodeId) {
  /* TODO: */
}

static void nmInitReset5(NetIdType NetId) {
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];

  nmCancelAlarm(TError);
  context->nmMerker.W.limphome = 0;

  context->nmRxCount = 0;
  context->nmTxCount = 0;
  D_Online(NetId);
  nmInitReset6(NetId);
}

static void nmInitReset6(NetIdType NetId) {
  const OsekNm_ChannelConfigType *config = &NM_CONFIG->channelConfigs[NetId];
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];

  context->nmTxPdu.Source = config->NodeId;
  context->nmState = NM_stInitReset;
  /* config.present = own station */
  nmAddtoPresent(NetId, config->NodeId);
  /* logical successor := own station */
  context->nmTxPdu.Destination = config->NodeId;
  context->nmRxCount += 1;
  /* Initialise the nmPdu(Data,OpCode) */
  context->nmTxPdu.OpCode.b = 0;
  memset(context->nmTxPdu.RingData, 0, sizeof(RingDataType));
  /* Cancel all Alarm */
  nmCancelAlarm(TTx);
  nmCancelAlarm(TTyp);
  nmCancelAlarm(TMax);
  nmCancelAlarm(TWbs);
  nmCancelAlarm(TError);
  if (context->nmStatus.NetworkStatus.W.NMactive) {
    context->nmTxCount += 1;
    /* Send A alive message */
    context->nmTxPdu.OpCode.b = NM_MaskAlive;
    nmSendMessage();
    nmDebug("nmInitReset6:Send Alive,");
  }
  if ((context->nmTxCount <= config->tx_limit) && (context->nmRxCount <= config->rx_limit)) {
    nmSetAlarm(TTyp);
    context->nmState = NM_stNormal;
    nmDebug("Set TTyp, enter Normal state.\n");
  } else {
    nmSetAlarm(TError);
    context->nmState = NM_stLimphome;
    nmDebug("Set TError, Enter Limphome state.\n");
  }
}

static void nmNormalStandard(NetIdType NetId, NMPduType *nmPdu) {
  const OsekNm_ChannelConfigType *config = &NM_CONFIG->channelConfigs[NetId];
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];
  context->nmRxCount = 0;
  if (nmPdu->OpCode.B.Limphome) {
    /* add sender to config.limphome */
    nmAddtoLimphome(NetId, nmPdu->Source);
  } else {
    /* add sender to config.present */
    nmAddtoPresent(NetId, nmPdu->Source);
    /* determine logical successor */
    context->nmTxPdu.Destination =
      nmDetermineLS(nmPdu->Source, config->NodeId, context->nmTxPdu.Destination);
    if (nmPdu->OpCode.B.Ring) {
      nmCancelAlarm(TTyp);
      nmCancelAlarm(TMax);
      /* see nm253 Figure 4 Mechanism to transfer application data via the logical ring */
      memcpy(context->nmTxPdu.RingData, nmPdu->RingData, sizeof(RingDataType));
      if ((nmPdu->Destination == config->NodeId)      /* to me */
          || (nmPdu->Destination == nmPdu->Source)) { /* or D = S */
        nmSetAlarm(TTyp);
        /* Do Ring Data indication */
        /* TODO */
      } else {
        nmSetAlarm(TMax);
        if (nmIsMeSkipped(nmPdu->Source, config->NodeId, nmPdu->Destination)) {
          if (context->nmStatus.NetworkStatus.W.NMactive) {
            context->nmTxPdu.OpCode.b = NM_MaskAlive;
            context->nmTxPdu.Destination = config->NodeId;
            if (context->nmStatus.NetworkStatus.W.bussleep) {
              context->nmTxPdu.OpCode.B.SleepInd = 1;
            }
            nmSendMessage();
          }
        }
      }
    }
  }
}

static NodeIdType nmDetermineLS(NodeIdType S, NodeIdType R, NodeIdType L) {
  NodeIdType newL = L;
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

static boolean nmIsMeSkipped(NodeIdType S, NodeIdType R, NodeIdType D) {
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

static void nmNormalMain(NetIdType NetId) {
  const OsekNm_ChannelConfigType *config = &NM_CONFIG->channelConfigs[NetId];
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];
  if (nmIsAlarmStarted(TTyp)) {
    nmSingalAlarm(TTyp);
    if (nmIsAlarmTimeout(TTyp)) {
      nmCancelAlarm(TTyp);
      nmCancelAlarm(TMax);
      nmSetAlarm(TMax);
      if (context->nmStatus.NetworkStatus.W.NMactive) {
        context->nmTxPdu.OpCode.b = NM_MaskRing;
        if (context->nmStatus.NetworkStatus.W.bussleep) {
          context->nmTxPdu.OpCode.B.SleepInd = 1;
        }
        context->nmTxCount++;
        nmSendMessage();
      } else {
      }
      if (context->nmTxCount > config->tx_limit) {
        context->nmState = NM_stLimphome;
        nmSetAlarm(TError);
      } else {
        if (context->nmMerker.W.stable) {
          context->nmStatus.NetworkStatus.W.configurationstable = 1;
        } else {
          context->nmMerker.W.stable = 1;
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
}

static void nmLimphomeMain(NetIdType NetId) {
  const OsekNm_ChannelConfigType *config = &NM_CONFIG->channelConfigs[NetId];
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];
  if (nmIsAlarmStarted(TError)) {
    nmSingalAlarm(TError);
    if (nmIsAlarmTimeout(TError)) {
      nmCancelAlarm(TError);
      D_Online(NetId);
      context->nmTxPdu.OpCode.b = NM_MaskLimphome;
      if (context->nmStatus.NetworkStatus.W.bussleep) {
        nmSetAlarm(TMax);
        context->nmTxPdu.OpCode.B.SleepInd = 1;
        context->nmState = NM_stLimphomePrepSleep;
      } else {
        nmSetAlarm(TError);
      }
      if (context->nmStatus.NetworkStatus.W.NMactive) {
        nmSendMessage();
      }
    }
  }
}
static void nmNormalPrepSleepMain(NetIdType NetId) {
  const OsekNm_ChannelConfigType *config = &NM_CONFIG->channelConfigs[NetId];
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];
  if (nmIsAlarmStarted(TTyp)) {
    nmSingalAlarm(TTyp);
    if (nmIsAlarmTimeout(TTyp)) {
      nmCancelAlarm(TTyp);
      if (context->nmStatus.NetworkStatus.W.NMactive) {
        /* Send ring message with set sleep.ack bit */
        context->nmTxPdu.OpCode.b = NM_MaskRing | NM_MaskSI | NM_MaskSA;
        nmSendMessage();
      }
    }
  }
}

static void nmBusSleep(NetIdType NetId) {
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];
  D_Init(NetId, BusSleep);
  context->nmState = NM_stBusSleep;
}

static void nmNormalTwbsMain(NetIdType NetId) {
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
}

static void nmLimphomePrepSleepMain(NetIdType NetId) {
  const OsekNm_ChannelConfigType *config = &NM_CONFIG->channelConfigs[NetId];
  OsekNm_ChannelContextType *context = &NM_CONFIG->channelContexts[NetId];
  if (nmIsAlarmStarted(TMax)) {
    nmSingalAlarm(TMax);
    if (nmIsAlarmTimeout(TMax)) {
      nmCancelAlarm(TMax);
      /* 7 NMInitBusSleep */
      D_Offline(NetId);
      nmSetAlarm(TWbs);
      context->nmState = NM_stTwbsLimphome;
    }
  }
}

static void nmTwbsLimphomeMain(NetIdType NetId) {
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
}
/* ================================ [ FUNCTIONS ] ============================================== */
void OsekNm_Init(const OsekNm_ConfigType *ConfigPtr) {
  NetIdType NetId;
  const OsekNm_ChannelConfigType *config;
  OsekNm_ChannelContextType *context;
  for (NetId = 0; NetId < NM_CONFIG->numOfChannels; NetId++) {
    config = &NM_CONFIG->channelConfigs[NetId];
    context = &NM_CONFIG->channelContexts[NetId];
    memset(context, 0, sizeof(*context));
    context->nmState = NM_stOff;
    context->nmTxPdu.Source = config->NodeId;
  }
}

/* see nm253.pdf Figure 30 Start-up of the network */
StatusType StartNM(NetIdType NetId) {
  StatusType ercd = E_NOT_OK;
  OsekNm_ChannelContextType *context;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    /* step 1: */
    context->nmTxPdu.OpCode.B.SleepInd = 0;
    context->nmTxPdu.OpCode.B.SleepAck = 0;
    /* step 2: enter NMInit */
    context->nmState = NM_stInit;
    NMInit(NetId);
    D_Init(NetId, BusInit);
    nmInit3(NetId);
    ercd = E_OK;
  }

  return ercd;
}

StatusType StopNM(NetIdType NetId) {
  StatusType ercd = E_NOT_OK;
  OsekNm_ChannelContextType *context;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    context->nmState = NM_stOff;
    ercd = E_OK;
  }

  return ercd;
}

StatusType GotoMode(NetIdType NetId, NMModeName NewMode) {
  StatusType ercd = E_OK;
  OsekNm_ChannelContextType *context;
  const OsekNm_ChannelConfigType *config;
  (void)config;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    config = &NM_CONFIG->channelConfigs[NetId];
    if (NewMode == NM_BusSleep) {
      context->nmStatus.NetworkStatus.W.bussleep = 1;
      switch (context->nmState) {
      case NM_stNormal: {
        if (context->nmStatus.NetworkStatus.W.NMactive) {
          /* NMNormal */
        } else {
          nmDebug("GotoMode_BusSleep,Enter NormalPrepSleep.\n");
          context->nmState = NM_stNormalPrepSleep;
        }
        break;
      }
      default:
        break;
      }
    } else if (NewMode == NM_Awake) {
      context->nmStatus.NetworkStatus.W.bussleep = 0;
      switch (context->nmState) {
      case NM_stNormalPrepSleep: {
        context->nmState = NM_stNormal;
        break;
      }
      case NM_stTwbsNormal: {
        nmCancelAlarm(TWbs);
        nmInitReset6(NetId);
        break;
      }
      case NM_stBusSleep: {
        D_Init(NetId, BusAwake);
        nmInit3(NetId);
        break;
      }
      case NM_stLimphomePrepSleep: {
        context->nmState = NM_stLimphome;
        nmSetAlarm(TError);
        break;
      }
      case NM_stTwbsLimphome: {
        nmCancelAlarm(TWbs);
        context->nmState = NM_stLimphome;
        nmSetAlarm(TError);
        break;
      }
      default:
        break;
      }
    }
  } else {
    ercd = E_NOT_OK;
  }
  return ercd;
}

StatusType SilentNM(NetIdType NetId) {
  StatusType ercd = E_OK;
  OsekNm_ChannelContextType *context;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    context->nmStatus.NetworkStatus.W.NMactive = 0;
  } else {
    ercd = E_NOT_OK;
  }

  return ercd;
}

StatusType TalkNM(NetIdType NetId) {
  StatusType ercd = E_OK;
  OsekNm_ChannelContextType *context;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    context->nmStatus.NetworkStatus.W.NMactive = 1;
  }

  return ercd;
}

void OsekNm_BusErrorIndication(NetIdType NetId) {
  OsekNm_ChannelContextType *context;
  const OsekNm_ChannelConfigType *config;
  (void)config;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    config = &NM_CONFIG->channelConfigs[NetId];
    D_Offline(NetId);
    D_Init(NetId, BusRestart);
    nmSetAlarm(TError);
    context->nmState = NM_stLimphome;
  }
}

StatusType OsekNm_GetState(NetIdType NetId, Nm_ModeType *nmModePtr) {
  StatusType ret = E_OK;
  OsekNm_ChannelContextType *context;
  const OsekNm_ChannelConfigType *config;
  (void)config;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    switch (context->nmState) {
    case NM_stNormalPrepSleep:
    case NM_stLimphomePrepSleep:
      *nmModePtr = NM_MODE_PREPARE_BUS_SLEEP;
      break;
    case NM_stOff:
    case NM_stBusSleep:
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

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    config = &NM_CONFIG->channelConfigs[NetId];
    context->nmTxCount = 0;
    switch (context->nmState) {
    case NM_stNormal: {
      if (context->nmTxPdu.OpCode.B.Ring) {
        nmCancelAlarm(TTyp);
        nmCancelAlarm(TMax);
        nmSetAlarm(TMax);
        if (context->nmTxPdu.OpCode.B.SleepInd) {
          if (context->nmStatus.NetworkStatus.W.bussleep) {
            context->nmTxPdu.OpCode.B.SleepAck = 1;
            context->nmState = NM_stNormalPrepSleep;
          }
        }
      }
      break;
    }
    case NM_stNormalPrepSleep: { /* 2 NMInitBusSleep */
      if (context->nmTxPdu.OpCode.B.Ring) {
        D_Offline(NetId);
        nmSetAlarm(TWbs);
        context->nmState = NM_stTwbsNormal;
      }
      break;
    }
    case NM_stLimphome: {
      if (context->nmTxPdu.OpCode.B.Limphome) {
        context->nmMerker.W.limphome = 1;
      }
      break;
    }
    default:
      break;
    }
  }
}

StatusType D_WindowDataReq(NetIdType NetId, NMPduType *nmPdu, uint8_t DataLengthTx) {
  StatusType ercd;
  PduInfoType PduInfo;
  const OsekNm_ChannelConfigType *config;
  config = &NM_CONFIG->channelConfigs[NetId];

  PduInfo.SduLength = DataLengthTx;
  PduInfo.SduDataPtr = &nmPdu->Destination;

  ercd = CanIf_Transmit(config->txPduId, &PduInfo);

  return ercd;
}

void OsekNm_RxIndication(PduIdType NetId, const PduInfoType *PduInfoPtr) {
  OsekNm_ChannelContextType *context;
  const OsekNm_ChannelConfigType *config;
  NMPduType nmPdu;
  const Can_HwType *Mailbox = (const Can_HwType *)PduInfoPtr->MetaDataPtr;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    config = &NM_CONFIG->channelConfigs[NetId];
    nmPdu.Source = Mailbox->CanId & config->NodeMask;
    memcpy(&nmPdu.Destination, PduInfoPtr->SduDataPtr, 8);

    switch (context->nmState) {
    case NM_stNormal:
    case NM_stNormalPrepSleep: {
      nmNormalStandard(NetId, &nmPdu);
      if (nmPdu.OpCode.B.SleepAck) {
        if (context->nmStatus.NetworkStatus.W.bussleep) { /* 2 NMInitBusSleep */
          D_Offline(NetId);
          nmSetAlarm(TWbs);
          context->nmState = NM_stTwbsNormal;
        }
      }
      /* Special process for NM_stNormalPrepSleep only */
      if (NM_stNormalPrepSleep == context->nmState) {
        if (nmPdu.OpCode.B.SleepInd) {
        } else {
          context->nmState = NM_stNormal;
        }
      }
      break;
    }
    case NM_stTwbsNormal: {
      if (nmPdu.OpCode.B.SleepInd) { /* = 1 */
      } else {                       /* = 0 */
        nmCancelAlarm(TWbs);
        nmInitReset6(NetId);
      }
      break;
    }
    case NM_stLimphome: {
      if (context->nmStatus.NetworkStatus.W.NMactive) {
        if (context->nmMerker.W.limphome) {
          if ((context->nmStatus.NetworkStatus.W.bussleep) && (nmPdu.OpCode.B.SleepAck)) {
            D_Offline(NetId);
            nmSetAlarm(TWbs);
            context->nmState = NM_stTwbsLimphome;
          } else {
            nmInitReset5(NetId);
          }
        } else {
          if (nmPdu.OpCode.B.SleepAck) {
            D_Offline(NetId);
            nmSetAlarm(TWbs);
            context->nmState = NM_stTwbsLimphome;
          } else {
            nmInitReset5(NetId);
          }
        }
      } else {
        if (context->nmMerker.W.limphome) {
          if ((context->nmStatus.NetworkStatus.W.bussleep) && (nmPdu.OpCode.B.SleepAck)) {
            D_Offline(NetId);
            nmSetAlarm(TWbs);
            context->nmState = NM_stTwbsLimphome;
          } else {
            nmInitReset5(NetId);
          }
        } else {
          if (nmPdu.OpCode.B.SleepAck) {
            D_Offline(NetId);
            nmSetAlarm(TWbs);
            context->nmState = NM_stTwbsLimphome;
          } else {
            nmInitReset5(NetId);
          }
        }
      }
      break;
    }
    case NM_stLimphomePrepSleep: {
      if (0 == nmPdu.OpCode.B.SleepInd) {
      } else {
        context->nmState = NM_stLimphome;
      }
      break;
    }
    case NM_stTwbsLimphome: {
      if (0 == nmPdu.OpCode.B.SleepInd) {
        nmCancelAlarm(TWbs);
        context->nmState = NM_stLimphome;
      } else {
      }
      break;
    }
    default:
      break;
    }
  }
}

void OsekNm_WakeupIndication(NetIdType NetId) {
  OsekNm_ChannelContextType *context;

  if (NetId < NM_CONFIG->numOfChannels) {
    context = &NM_CONFIG->channelContexts[NetId];
    if (NM_stBusSleep == context->nmState) { /* 3 NMInit */
      /* OK Wakeup */
      D_Init(NetId, BusAwake);
      nmInit3(NetId);
    }
  }
}

void OsekNm_MainFunction(void) {
  NetIdType NetId;
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
    switch (context->nmState) {
    case NM_stNormal:
      nmNormalMain(NetId);
      break;
    case NM_stLimphome:
      nmLimphomeMain(NetId);
      break;
    case NM_stNormalPrepSleep:
      nmNormalPrepSleepMain(NetId);
      break;
    case NM_stTwbsNormal:
      nmNormalTwbsMain(NetId);
      break;
    case NM_stLimphomePrepSleep:
      nmLimphomePrepSleepMain(NetId);
      break;
    case NM_stTwbsLimphome:
      nmTwbsLimphomeMain(NetId);
      break;
    default:
      break;
    }
  }
}