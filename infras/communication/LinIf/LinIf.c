/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref:Specification of LIN Interface AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <string.h>
#include "LinIf_Priv.h"
#include "Std_Debug.h"
#include "Det.h"

#ifdef USE_MIRROR
#include "Mirror.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_LINIF 0
#define AS_LOG_LINIFE 0

/* just 2 state to make things simple */
#define LINIF_STATUS_IDLE 0
#define LINIF_STATUS_BUSY 1

#ifdef LINIF_USE_PB_CONFIG
#define LINIF_CONFIG linifConfig
#else
#define LINIF_CONFIG (&LinIf_Config)
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const LinIf_ConfigType LinIf_Config;
/* ================================ [ DATAS     ] ============================================== */
#ifdef LINIF_USE_PB_CONFIG
static const LinIf_ConfigType *linifConfig = NULL;
#endif
/* ================================ [ LOCALS    ] ============================================== */
#if (LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER
static void LinIf_ScheduleEntry(uint8_t channel) {
  LinIf_ChannelContextType *context;
  const LinIf_ChannelConfigType *config;
  const LinIf_ScheduleTableEntryType *entry;
  Std_ReturnType result = LINIF_R_OK;

  context = &LINIF_CONFIG->ChannelContexts[channel];
  config = &LINIF_CONFIG->ChannelConfigs[channel];
  entry = &context->ScheduleTable->Entries[context->CurSch];

  ASLOG(LINIF,
        ("%d: begin schedule %d: id=%X dlc=%d\n", channel, context->CurSch, entry->Id, entry->Dlc));

  if (LINIF_MAX_DATA_LENGHT >= entry->Dlc) {
    context->Frame.Pid = entry->Id;
    context->Frame.Dl = entry->Dlc;
    context->Frame.SduPtr = context->Data;
    context->Frame.Drc = entry->Drc;
    context->Frame.Cs = entry->Cs;
    if (LIN_FRAMERESPONSE_TX == entry->Drc) {
      result = entry->Callback(channel, &context->Frame, LINIF_R_TRIGGER_TRANSMIT);
    }
  } else {
    result = LINIF_R_NOT_OK;
    ASLOG(LINIFE, ("context buffer too small\n"));
  }

  if (LINIF_R_OK == result) {
    result = Lin_SendFrame(config->LinChannel, &context->Frame);
    if (E_OK == result) {
      context->Status = LINIF_STATUS_BUSY;
    } else {
      ASLOG(LINIF, ("%d: FAIL to send\n", channel));
    }
  } else {
    ASLOG(LINIFE, ("%d: FAIL to provide Tx buffer\n", channel));
  }
}

#ifdef LINIF_SCHED_MODE_POLLING
static void LinIf_CheckEntry(uint8_t channel) {
  LinIf_ChannelContextType *context;
  const LinIf_ChannelConfigType *config;
  const LinIf_ScheduleTableEntryType *entry;
  Std_ReturnType result = LINIF_R_NOT_OK;
  Lin_StatusType status;

  context = &LINIF_CONFIG->ChannelContexts[channel];
  config = &LINIF_CONFIG->ChannelConfigs[channel];
  entry = &context->ScheduleTable->Entries[context->CurSch];

  status = Lin_GetStatus(config->LinChannel, &context->Frame.SduPtr);
  if (LIN_FRAMERESPONSE_TX == entry->Drc) {
    if (LIN_TX_OK == status) {
      result = LINIF_R_TX_COMPLETED;
    }
  } else {
    if (LIN_RX_OK == status) {
      result = LINIF_R_RECEIVED_OK;
      ASLOG(LINIF, ("[%d]RX ID=%02X len=%X data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n",
                    channel, context->Frame.Pid, context->Frame.Dl, context->Frame.SduPtr[0],
                    context->Frame.SduPtr[1], context->Frame.SduPtr[2], context->Frame.SduPtr[3],
                    context->Frame.SduPtr[4], context->Frame.SduPtr[5], context->Frame.SduPtr[6],
                    context->Frame.SduPtr[7]));
    }
  }

  if (result != LINIF_R_NOT_OK) {
    ASLOG(LINIF, ("%d: TX done with result %d\n", channel, result));
    (void)entry->Callback(channel, &context->Frame, result);
    context->Status = LINIF_STATUS_IDLE;
  }

#ifdef USE_MIRROR
  if ((TRUE == context->MirroringActive) &&
      ((status == LIN_TX_OK) || (status == LIN_TX_HEADER_ERROR) || (status == LIN_TX_ERROR) ||
       (status == LIN_RX_OK) || (status == LIN_RX_ERROR) || (status == LIN_RX_NO_RESPONSE))) {
    PduInfoType PduInfo;
    PduInfo.MetaDataPtr = NULL;
    PduInfo.SduDataPtr = context->Data;
    PduInfo.SduLength = context->Frame.Dl;
    Mirror_ReportLinFrame(channel, context->Frame.Pid, &PduInfo, status);
    context->Status = LINIF_STATUS_IDLE;
  }
#endif
}
#endif /* LINIF_SCHED_MODE_POLLING */
#endif /* LINIF_VARIANT_MASTER */

#if (LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER
static void LinIf_MainFunction_Master(NetworkHandleType channel) {
  LinIf_ChannelContextType *context;
  const LinIf_ChannelConfigType *config;
  Std_ReturnType ret = E_OK;

  context = &LINIF_CONFIG->ChannelContexts[channel];
  config = &LINIF_CONFIG->ChannelConfigs[channel];
  if (LINIF_CHANNEL_SLEEP_PENDING == context->State) {
    if (LINIF_STATUS_IDLE == context->Status) {
      /* invalidate schedule table before going to sleep mode */
      context->ScheduleTable = NULL;
      context->CurSch = 0;
      context->ScheduleRequested = LINIF_INVALD_SCHEDULE_TABLE;
      ret = Lin_GoToSleep(config->LinChannel);
      if (E_OK == ret) {
        context->State = LINIF_CHANNEL_SLEEP_COMMAND;
      } else {
        ASLOG(LINIF, ("%d: FAIL to send\n", channel));
      }
    } else {
      /* wait till the channel to be idle */
    }
  } else if (LINIF_CHANNEL_SLEEP_COMMAND == context->State) {
    /* General the Lin schedule cycle is longer than 1 message tranmist time, so anyway to force
     * to sleep without the checking of Lin status */
    ret = Lin_GoToSleepInternal(config->LinChannel);
    if (E_OK == ret) {
      context->State = LINIF_CHANNEL_SLEEP;
    }
  } else {
    /* do nothing */
  }

  if (LINIF_CHANNEL_OPERATIONAL != context->State) { /* @SWS_LinIf_00043 */
    ret = E_NOT_OK;
    /* not allowed to do scheduling but continue the timer counting down */
    if (context->Timer > 0) {
      context->Timer--;
    }
    if (0 == context->Timer) {
      if (LINIF_STATUS_IDLE != context->Status) {
        ASLOG(LINIFE, ("%d: master timeout\n", channel));
      }
      context->Status = LINIF_STATUS_IDLE;
    }
  }

  if ((E_OK == ret) && (context->ScheduleRequested != LINIF_INVALD_SCHEDULE_TABLE)) {
    if (LINIF_STATUS_IDLE == context->Status) {
      /* switch only when system is IDLE */
      context->ScheduleTable = &LINIF_CONFIG->ScheduleTables[context->ScheduleRequested];
      ASLOG(LINIF, ("%d: switch to schedule table %d\n", channel, context->ScheduleRequested));
      context->ScheduleRequested = LINIF_INVALD_SCHEDULE_TABLE;
      context->Timer = 0;
      if (0 < context->ScheduleTable->NumOfEntries) {
        context->CurSch = context->ScheduleTable->NumOfEntries - 1; /* default to the end */
      } else {
        /* this is an empty schedule table, do nothing */
        ret = E_NOT_OK;
      }
    }
  }

  if ((E_OK == ret) && (NULL != context->ScheduleTable)) {
    if (context->Timer > 0) {
      context->Timer--;
    }
    if (0 == context->Timer) {
      if (LINIF_STATUS_IDLE != context->Status) {
        ASLOG(LINIFE, ("%d: master timeout for entry %u\n", channel, context->CurSch));
      }
      context->CurSch++;
      if (context->CurSch >= context->ScheduleTable->NumOfEntries) {
        context->CurSch = 0;
      }
      context->Timer = context->ScheduleTable->Entries[context->CurSch].Delay;
      LinIf_ScheduleEntry(channel);
    }
  }
}
#endif

#if (LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE
static void LinIf_MainFunction_Slave(NetworkHandleType channel) {
  LinIf_ChannelContextType *context;
  const LinIf_ChannelConfigType *config;
  const LinIf_ScheduleTableEntryType *entry;

  context = &LINIF_CONFIG->ChannelContexts[channel];
  if (context->Timer > 0) {
    context->Timer--;
    if (0 == context->Timer) {
      ASLOG(LINIFE, ("%d: slave timeout\n", channel));
      context->Status = LINIF_STATUS_IDLE;
      context->Timer = 0;
      config = &LINIF_CONFIG->ChannelConfigs[channel];
      if (context->CurSch < config->ScheduleTable->NumOfEntries) {
        entry = &config->ScheduleTable->Entries[context->CurSch];
        /* notify timeout */
        (void)entry->Callback(channel, &context->Frame, LINIF_R_NOT_OK);
      }
    }
  }
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
void LinIf_Init(const LinIf_ConfigType *configPtr) {
  int i;
  LinIf_ChannelContextType *context;
#ifdef LINIF_USE_PB_CONFIG
  if (NULL != configPtr) {
    LINIF_CONFIG = configPtr;
  } else {
    LINIF_CONFIG = &LinIf_Config;
  }
#else
  (void)configPtr;
#endif
  for (i = 0; i < LINIF_CONFIG->NumOfChannels; i++) {
    context = &LINIF_CONFIG->ChannelContexts[i];
    context->Status = LINIF_STATUS_IDLE;
#if (LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER
    context->ScheduleTable = NULL;
    context->CurSch = 0;
    context->ScheduleRequested = LINIF_INVALD_SCHEDULE_TABLE;
#endif
    context->State = LINIF_CHANNEL_SLEEP;
    context->Timer = 0;
#ifdef USE_MIRROR
    context->MirroringActive = FALSE;
#endif
  }
}

Std_ReturnType LinIf_GotoSleep(NetworkHandleType channel) {
  Std_ReturnType ret = E_NOT_OK;
  LinIf_ChannelContextType *context;
#if (LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE
  const LinIf_ChannelConfigType *config;
#endif
  DET_VALIDATE(NULL != LINIF_CONFIG, 0x06, LINIF_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(channel < LINIF_CONFIG->NumOfChannels, 0x06, LINIF_E_NONEXISTENT_CHANNEL,
               return E_NOT_OK);
#if (LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE
  config = &LINIF_CONFIG->ChannelConfigs[channel];
#endif
  context = &LINIF_CONFIG->ChannelContexts[channel];
#if LINIF_VARIANT == LINIF_VARIANT_BOTH
  if (LINIF_SLAVE == config->NodeType) {
#endif
#if (LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE
    ret = Lin_GoToSleepInternal(config->LinChannel);
    if (E_OK == ret) {
      context->State = LINIF_CHANNEL_SLEEP;
    }
#endif
#if LINIF_VARIANT == LINIF_VARIANT_BOTH
  } else {
#endif
#if (LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER
    if (LINIF_CHANNEL_SLEEP != context->State) {
      context->State = LINIF_CHANNEL_SLEEP_PENDING;
      ret = E_OK;
    } else { /* already in sleep mode, dothing */
      ret = E_OK;
    }
#endif
#if LINIF_VARIANT == LINIF_VARIANT_BOTH
  }
#endif

  return ret;
}

Std_ReturnType LinIf_WakeUp(NetworkHandleType channel) {
  Std_ReturnType ret = E_NOT_OK;
  LinIf_ChannelContextType *context;
  const LinIf_ChannelConfigType *config;

  DET_VALIDATE(NULL != LINIF_CONFIG, 0x07, LINIF_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(channel < LINIF_CONFIG->NumOfChannels, 0x07, LINIF_E_NONEXISTENT_CHANNEL,
               return E_NOT_OK);

  config = &LINIF_CONFIG->ChannelConfigs[channel];
  context = &LINIF_CONFIG->ChannelContexts[channel];
  if (LINIF_CHANNEL_SLEEP == context->State) {
    /* @SWS_LinIf_00296 */
    ret = Lin_Wakeup(config->LinChannel);
    if (E_OK == ret) { /* @SWS_LinIf_00478 */
      context->State = LINIF_CHANNEL_OPERATIONAL;
    }
  } else {
    ret = E_OK; /* @SWS_LinIf_00432 */
  }

  return ret;
}

void LinIf_DeInit(void) {
  int i;
  LinIf_ChannelContextType *context;
  const LinIf_ChannelConfigType *config;
  DET_VALIDATE(NULL != LINIF_CONFIG, 0xF0, LINIF_E_UNINIT, return);
  for (i = 0; i < LINIF_CONFIG->NumOfChannels; i++) {
    config = &LINIF_CONFIG->ChannelConfigs[i];
    context = &LINIF_CONFIG->ChannelContexts[i];
    context->Status = LINIF_STATUS_IDLE;
    context->Timer = 0;
    context->State = LINIF_CHANNEL_UNINIT;
    Lin_GoToSleepInternal(config->LinChannel);
  }
}

#if (LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER
Std_ReturnType LinIf_ScheduleRequest(NetworkHandleType channel, LinIf_SchHandleType schedule) {
  Std_ReturnType r = E_OK;
  LinIf_ChannelContextType *context;

  DET_VALIDATE(NULL != LINIF_CONFIG, 0x05, LINIF_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(channel < LINIF_CONFIG->NumOfChannels, 0x05, LINIF_E_NONEXISTENT_CHANNEL,
               return E_NOT_OK);
  DET_VALIDATE(schedule < LINIF_CONFIG->NumOfSchTbls, 0x05, LINIF_E_PARAMETER, return E_NOT_OK);

#if LINIF_VARIANT == LINIF_VARIANT_BOTH
  DET_VALIDATE(LINIF_MASTER == LINIF_CONFIG->ChannelConfigs[channel].NodeType, 0x05,
               LINIF_E_PARAMETER, return E_NOT_OK);
#endif

  context = &LINIF_CONFIG->ChannelContexts[channel];
  DET_VALIDATE(LINIF_CHANNEL_OPERATIONAL == context->State, 0x05, LINIF_E_PARAMETER,
               return E_NOT_OK);
  context->ScheduleRequested = schedule;

  return r;
}
#endif /* LINIF_VARIANT_MASTER */

void LinIf_MainFunction_Read(void) {
#if ((LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER) &&                            \
  defined(LINIF_SCHED_MODE_POLLING)
  int i;
  LinIf_ChannelContextType *context;

  DET_VALIDATE(NULL != LINIF_CONFIG, 0x80, LINIF_E_UNINIT, return);
  for (i = 0; (i < LINIF_CONFIG->NumOfChannels); i++) {
    context = &LINIF_CONFIG->ChannelContexts[i];

    if (LINIF_STATUS_IDLE != context->Status) {
      LinIf_CheckEntry(i);
    }
  }
#endif
}

void LinIf_MainFunction(void) {
  int i;
#if LINIF_VARIANT == LINIF_VARIANT_BOTH
  const LinIf_ChannelConfigType *config;
#endif
  DET_VALIDATE(NULL != LINIF_CONFIG, 0x80, LINIF_E_UNINIT, return);
  for (i = 0; (i < LINIF_CONFIG->NumOfChannels); i++) {
#if LINIF_VARIANT == LINIF_VARIANT_BOTH
    config = &LINIF_CONFIG->ChannelConfigs[i];
    if (LINIF_MASTER == config->NodeType) {
#endif
#if (LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER
      LinIf_MainFunction_Master((NetworkHandleType)i);
#endif
#if LINIF_VARIANT == LINIF_VARIANT_BOTH
    } else {
#endif
#if (LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE
      LinIf_MainFunction_Slave((NetworkHandleType)i);
#endif
#if LINIF_VARIANT == LINIF_VARIANT_BOTH
    }
#endif
  }
}

#if (LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE
Std_ReturnType LinIf_HeaderIndication(NetworkHandleType channel, Lin_PduType *pduPtr) {
  LinIf_ChannelContextType *context;
  const LinIf_ChannelConfigType *config;
  const LinIf_ScheduleTableEntryType *entry;
  Std_ReturnType ret;
  uint16_t l;
  uint16_t h;
  uint16_t m;

  DET_VALIDATE(NULL != LINIF_CONFIG, 0x78, LINIF_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(channel < LINIF_CONFIG->NumOfChannels, 0x78, LINIF_E_NONEXISTENT_CHANNEL,
               return E_NOT_OK);

  context = &LINIF_CONFIG->ChannelContexts[channel];
  config = &LINIF_CONFIG->ChannelConfigs[channel];
#if LINIF_VARIANT == LINIF_VARIANT_BOTH
  DET_VALIDATE(LINIF_SLAVE == config->NodeType, 0x78, LINIF_E_PARAMETER, return E_NOT_OK);
#endif

  if (LINIF_STATUS_IDLE != context->Status) {
    ASLOG(LINIFE, ("%d: slave get header when busy, curSch=%d\n", channel, context->CurSch));
    /* anyway, stop slave RX */
    context->Status = LINIF_STATUS_IDLE;
  }

  l = 0;
  h = config->ScheduleTable->NumOfEntries - 1;
  ret = E_NOT_OK;
  entry = &config->ScheduleTable->Entries[0];
  if (pduPtr->Pid < entry->Id) {
    l = h + 1; /* avoid the underflow of "m - 1" */
  }
  while ((E_NOT_OK == ret) && (l <= h)) {
    m = l + ((h - l) >> 1);
    entry = &config->ScheduleTable->Entries[m];
    if (entry->Id > pduPtr->Pid) {
      h = m - 1;
    } else if (entry->Id < pduPtr->Pid) {
      l = m + 1;
    } else {
      ret = E_OK;
    }
  }

  if (E_OK == ret) {
    if (LIN_FRAMERESPONSE_RX == entry->Drc) {
      context->CurSch = m;
      pduPtr->Dl = entry->Dlc;
      pduPtr->Cs = entry->Cs;
      pduPtr->Drc = entry->Drc;
      context->Status = LINIF_STATUS_BUSY;
      context->Timer = config->Timeout;
    } else if (LIN_FRAMERESPONSE_TX == entry->Drc) {
      context->Frame.Pid = entry->Id;
      context->Frame.Dl = entry->Dlc;
      context->Frame.SduPtr = context->Data;
      context->Frame.Drc = entry->Drc;
      ret = entry->Callback(channel, &context->Frame, LINIF_R_TRIGGER_TRANSMIT);
      if (LINIF_R_OK == ret) {
        pduPtr->Pid = entry->Id;
        pduPtr->Cs = entry->Cs;
        pduPtr->Drc = entry->Drc;
        pduPtr->Dl = entry->Dlc;
        pduPtr->SduPtr = context->Data;
        ret = E_OK;
      } else {
        ret = E_NOT_OK;
      }
    } else {
      /* ignore */
    }
  }

  return ret;
}
#endif /* LINIF_VARIANT_SLAVE */

void LinIf_RxIndication(NetworkHandleType channel, uint8 *Lin_SduPtr) {
  LinIf_ChannelContextType *context;
#if ((LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE) ||                              \
  (LINIF_VARIANT == LINIF_VARIANT_BOTH)
  const LinIf_ChannelConfigType *config;
#endif
  const LinIf_ScheduleTableEntryType *entry;
  Std_ReturnType ret = E_OK;

  DET_VALIDATE(NULL != LINIF_CONFIG, 0x79, LINIF_E_UNINIT, return);
  DET_VALIDATE(channel < LINIF_CONFIG->NumOfChannels, 0x79, LINIF_E_NONEXISTENT_CHANNEL, return);

  context = &LINIF_CONFIG->ChannelContexts[channel];
#if ((LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE) ||                              \
  (LINIF_VARIANT == LINIF_VARIANT_BOTH)
  config = &LINIF_CONFIG->ChannelConfigs[channel];
#endif

  if (LINIF_STATUS_IDLE == context->Status) {
    ASLOG(LINIFE, ("%d: get data in idle\n", channel));
    ret = E_NOT_OK;
    context->Timer = 0;
  } else
#if (LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE
    if (
#if LINIF_VARIANT == LINIF_VARIANT_BOTH
      (LINIF_SLAVE == config->NodeType) &&
#endif
      (context->CurSch < config->ScheduleTable->NumOfEntries)) {
    entry = &config->ScheduleTable->Entries[context->CurSch];
    context->Timer = 0;
    context->Status = LINIF_STATUS_IDLE;
  }
#else
    if (
#if LINIF_VARIANT == LINIF_VARIANT_BOTH
      (LINIF_MASTER == config->NodeType) &&
#endif
      (NULL != context->ScheduleTable) &&
      (context->CurSch < context->ScheduleTable->NumOfEntries)) {
    entry = &context->ScheduleTable->Entries[context->CurSch];
    context->Status = LINIF_STATUS_IDLE;
  }
#endif
  else {
    ASLOG(LINIFE, ("%d: get data with invalid curSch=%u\n", channel, context->CurSch));
    ret = E_NOT_OK;
    context->Timer = 0;
    context->Status = LINIF_STATUS_IDLE;
  }

  if (E_OK == ret) {
    if (LIN_FRAMERESPONSE_RX == entry->Drc) {
      context->Frame.Pid = entry->Id;
      context->Frame.Dl = entry->Dlc;
      context->Frame.SduPtr = context->Data;
      context->Frame.Drc = entry->Drc;
      memcpy(context->Frame.SduPtr, Lin_SduPtr, entry->Dlc);
      (void)entry->Callback(channel, &context->Frame, LINIF_R_RECEIVED_OK);
#ifdef USE_MIRROR
      if (TRUE == context->MirroringActive) {
        PduInfoType PduInfo;
        PduInfo.MetaDataPtr = NULL;
        PduInfo.SduDataPtr = context->Data;
        PduInfo.SduLength = context->Frame.Dl;
        Mirror_ReportLinFrame(channel, context->Frame.Pid, &PduInfo, LIN_RX_OK);
      }
#endif
    } else {
      ASLOG(LINIFE,
            ("%d: rx data with invalid direction for entry %u\n", channel, context->CurSch));
    }
  }
}

void LinIf_TxConfirmation(NetworkHandleType channel) {
  LinIf_ChannelContextType *context;
#if (LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE
  const LinIf_ChannelConfigType *config;
#endif
  const LinIf_ScheduleTableEntryType *entry;
  Std_ReturnType ret = E_OK;
  DET_VALIDATE(NULL != LINIF_CONFIG, 0x7a, LINIF_E_UNINIT, return);
  DET_VALIDATE(channel < LINIF_CONFIG->NumOfChannels, 0x7a, LINIF_E_NONEXISTENT_CHANNEL, return);

  context = &LINIF_CONFIG->ChannelContexts[channel];
#if (LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE
  config = &LINIF_CONFIG->ChannelConfigs[channel];
#endif

  if (LINIF_STATUS_IDLE == context->Status) {
    ASLOG(LINIFE, ("%d: Tx confirm in idle\n", channel));
    ret = E_NOT_OK;
    context->Timer = 0;
  } else
#if (LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE
    if (context->CurSch < config->ScheduleTable->NumOfEntries) {
    entry = &config->ScheduleTable->Entries[context->CurSch];
    context->Timer = 0;
    context->Status = LINIF_STATUS_IDLE;
  }
#else
    if ((NULL != context->ScheduleTable) &&
        (context->CurSch < context->ScheduleTable->NumOfEntries)) {
    entry = &context->ScheduleTable->Entries[context->CurSch];
    context->Status = LINIF_STATUS_IDLE;
  }
#endif
  else {
    ASLOG(LINIFE, ("%d: Tx confirm with invalid curSch=%u\n", channel, context->CurSch));
    ret = E_NOT_OK;
    context->Timer = 0;
    context->Status = LINIF_STATUS_IDLE;
  }

  if (E_OK == ret) {
    if (LIN_FRAMERESPONSE_TX == entry->Drc) {
      (void)entry->Callback(channel, &context->Frame, LINIF_R_TX_COMPLETED);
#ifdef USE_MIRROR
      if (TRUE == context->MirroringActive) {
        PduInfoType PduInfo;
        PduInfo.MetaDataPtr = NULL;
        PduInfo.SduDataPtr = context->Data;
        PduInfo.SduLength = context->Frame.Dl;
        Mirror_ReportLinFrame(channel, context->Frame.Pid, &PduInfo, LIN_TX_OK);
      }
#endif
    } else {
      ASLOG(LINIFE,
            ("%d: Tx confirm with invalid direction for entry %u\n", channel, context->CurSch));
    }
  }
}

#ifdef USE_MIRROR
Std_ReturnType LinIf_EnableBusMirroring(NetworkHandleType channel, boolean mirroringActive) {
  Std_ReturnType ret = E_OK;
  DET_VALIDATE(NULL != LINIF_CONFIG, 0x7f, LINIF_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(channel < LINIF_CONFIG->NumOfChannels, 0x7f, LINIF_E_NONEXISTENT_CHANNEL,
               return E_NOT_OK);

  LINIF_CONFIG->ChannelContexts[channel].MirroringActive = mirroringActive;
  return ret;
}
#endif

void LinIf_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x03, LINIF_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_LINIF;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 2;
}

/** @brief release notes
 * - 4.0.1: Fix to ensure wake up to schedule the 1st frame from the schedule table.
 * - 4.0.2: For slave, when timer timeout, notify the application about the timeout.
 */
