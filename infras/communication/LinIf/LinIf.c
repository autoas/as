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
static void LinIf_ScheduleEntry(uint8_t Channel) {
  LinIf_ChannelContextType *context;
  const LinIf_ChannelConfigType *config;
  const LinIf_ScheduleTableEntryType *entry;
  Std_ReturnType result = LINIF_R_OK;

  context = &LINIF_CONFIG->channelContexts[Channel];
  config = &LINIF_CONFIG->channelConfigs[Channel];
  entry = &context->scheduleTable->entrys[context->curSch];

  ASLOG(LINIF,
        ("%d: begin schedule %d: id=%X dlc=%d\n", Channel, context->curSch, entry->id, entry->dlc));

  if (LINIF_MAX_DATA_LENGHT >= entry->dlc) {
    context->frame.Pid = entry->id;
    context->frame.Dl = entry->dlc;
    context->frame.SduPtr = context->data;
    context->frame.Drc = entry->Drc;
    context->frame.Cs = entry->Cs;
    if (LIN_FRAMERESPONSE_TX == entry->Drc) {
      result = entry->callback(Channel, &context->frame, LINIF_R_TRIGGER_TRANSMIT);
    }
  } else {
    result = LINIF_R_NOT_OK;
    ASLOG(LINIFE, ("context buffer too small\n"));
  }

  if (LINIF_R_OK == result) {
    result = Lin_SendFrame(config->linChannel, &context->frame);
    if (E_OK == result) {
      context->status = LINIF_STATUS_BUSY;
    } else {
      ASLOG(LINIF, ("%d: FAIL to send\n", Channel));
    }
  } else {
    ASLOG(LINIFE, ("%d: FAIL to provide Tx buffer\n", Channel));
  }
}

static void LinIf_CheckEntry(uint8_t Channel) {
  LinIf_ChannelContextType *context;
  const LinIf_ChannelConfigType *config;
  const LinIf_ScheduleTableEntryType *entry;
  Std_ReturnType result = LINIF_R_NOT_OK;
  Lin_StatusType status;

  context = &LINIF_CONFIG->channelContexts[Channel];
  config = &LINIF_CONFIG->channelConfigs[Channel];
  entry = &context->scheduleTable->entrys[context->curSch];

  status = Lin_GetStatus(config->linChannel, &context->frame.SduPtr);
  if (LIN_FRAMERESPONSE_TX == entry->Drc) {
    if (LIN_TX_OK == status) {
      result = LINIF_R_TX_COMPLETED;
    }
  } else {
    if (LIN_RX_OK == status) {
      result = LINIF_R_RECEIVED_OK;
      ASLOG(LINIF, ("[%d]RX ID=%02X len=%X data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n",
                    Channel, context->frame.Pid, context->frame.Dl, context->frame.SduPtr[0],
                    context->frame.SduPtr[1], context->frame.SduPtr[2], context->frame.SduPtr[3],
                    context->frame.SduPtr[4], context->frame.SduPtr[5], context->frame.SduPtr[6],
                    context->frame.SduPtr[7]));
    }
  }

  if (result != LINIF_R_NOT_OK) {
    ASLOG(LINIF, ("%d: TX done with result %d\n", Channel, result));
    (void)entry->callback(Channel, &context->frame, result);
    context->status = LINIF_STATUS_IDLE;
  }
}
#endif /* LINIF_VARIANT_MASTER */

#if (LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER
static void LinIf_MainFunction_Master(NetworkHandleType Channel) {
  LinIf_ChannelContextType *context;
  const LinIf_ChannelConfigType *config;
  Std_ReturnType ret = E_OK;

  context = &LINIF_CONFIG->channelContexts[Channel];
  config = &LINIF_CONFIG->channelConfigs[Channel];
  if (LINIF_CHANNEL_SLEEP_PENDING == context->state) {
    if (LINIF_STATUS_IDLE == context->status) {
      ret = Lin_GoToSleep(config->linChannel);
      if (E_OK == ret) {
        context->state = LINIF_CHANNEL_SLEEP_COMMAND;
      } else {
        ASLOG(LINIF, ("%d: FAIL to send\n", Channel));
      }
    } else {
      /* wait till the channel to be idle */
    }
  } else if (LINIF_CHANNEL_SLEEP_COMMAND == context->state) {
    /* General the Lin schedule cycle is longer than 1 message tranmist time, so anyway to force
     * to sleep without the checking of Lin status */
    ret = Lin_GoToSleepInternal(config->linChannel);
    if (E_OK == ret) {
      context->state = LINIF_CHANNEL_SLEEP;
    }
  } else {
    /* do nothing */
  }

  if (LINIF_CHANNEL_OPERATIONAL != context->state) { /* @SWS_LinIf_00043 */
    ret = E_NOT_OK;
    /* not allowed to do scheduling but continue the timer counting down */
    if (context->timer > 0) {
      context->timer--;
    }
    if (0 == context->timer) {
      if (LINIF_STATUS_IDLE != context->status) {
        ASLOG(LINIFE, ("%d: master timeout\n", Channel));
      }
      context->status = LINIF_STATUS_IDLE;
    }
  }

  if ((E_OK == ret) && (context->scheduleRequested != LINIF_INVALD_SCHEDULE_TABLE)) {
    if (LINIF_STATUS_IDLE == context->status) {
      /* switch only when system is IDLE */
      context->scheduleTable = &LINIF_CONFIG->scheduleTables[context->scheduleRequested];
      ASLOG(LINIF, ("%d: switch to schedule table %d\n", Channel, context->scheduleRequested));
      context->scheduleRequested = LINIF_INVALD_SCHEDULE_TABLE;
      context->timer = 0;
      if (0 < context->scheduleTable->numOfEntries) {
        context->curSch = context->scheduleTable->numOfEntries - 1; /* default to the end */
      } else {
        /* this is an empty schedule table, do nothing */
        ret = E_NOT_OK;
      }
    }
  }

  if ((E_OK == ret) && (NULL != context->scheduleTable)) {
    if (context->timer > 0) {
      context->timer--;
    }
    if (0 == context->timer) {
      if (LINIF_STATUS_IDLE != context->status) {
        ASLOG(LINIFE, ("%d: master timeout\n", Channel));
      }
      context->curSch++;
      if (context->curSch >= context->scheduleTable->numOfEntries) {
        context->curSch = 0;
      }
      context->timer = context->scheduleTable->entrys[context->curSch].delay;
      LinIf_ScheduleEntry(Channel);
    }
  }
}
#endif

#if (LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE
static void LinIf_MainFunction_Slave(NetworkHandleType Channel) {
  LinIf_ChannelContextType *context;
  context = &LINIF_CONFIG->channelContexts[Channel];

  if (context->timer > 0) {
    context->timer--;
    if (0 == context->timer) {
      ASLOG(LINIFE, ("%d: slave timeout\n", Channel));
      context->status = LINIF_STATUS_IDLE;
      context->timer = 0;
      /* TODO: */
    }
  }
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
void LinIf_Init(const LinIf_ConfigType *ConfigPtr) {
  int i;
  LinIf_ChannelContextType *context;
#ifdef LINIF_USE_PB_CONFIG
  if (NULL != ConfigPtr) {
    LINIF_CONFIG = ConfigPtr;
  } else {
    LINIF_CONFIG = &LinIf_Config;
  }
#else
  (void)ConfigPtr;
#endif
  for (i = 0; i < LINIF_CONFIG->numOfChannels; i++) {
    context = &LINIF_CONFIG->channelContexts[i];
    context->status = LINIF_STATUS_IDLE;
#if (LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER
    context->scheduleTable = NULL;
    context->curSch = 0;
    context->scheduleRequested = LINIF_INVALD_SCHEDULE_TABLE;
#endif
    context->state = LINIF_CHANNEL_SLEEP;
    context->timer = 0;
  }
}

Std_ReturnType LinIf_GotoSleep(NetworkHandleType Channel) {
  Std_ReturnType ret = E_NOT_OK;
  LinIf_ChannelContextType *context;
#if (LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE
  const LinIf_ChannelConfigType *config;
#endif
  DET_VALIDATE(NULL != LINIF_CONFIG, 0x06, LINIF_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(Channel < LINIF_CONFIG->numOfChannels, 0x06, LINIF_E_NONEXISTENT_CHANNEL,
               return E_NOT_OK);
#if (LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE
  config = &LINIF_CONFIG->channelConfigs[Channel];
#endif
  context = &LINIF_CONFIG->channelContexts[Channel];
#if LINIF_VARIANT == LINIF_VARIANT_BOTH
  if (LINIF_SLAVE == config->nodeType) {
#endif
#if (LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE
    ret = Lin_GoToSleepInternal(config->linChannel);
    if (E_OK == ret) {
      context->state = LINIF_CHANNEL_SLEEP;
    }
#endif
#if LINIF_VARIANT == LINIF_VARIANT_BOTH
  } else {
#endif
#if (LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER
    if (LINIF_CHANNEL_SLEEP != context->state) {
      context->state = LINIF_CHANNEL_SLEEP_PENDING;
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

Std_ReturnType LinIf_WakeUp(NetworkHandleType Channel) {
  Std_ReturnType ret = E_NOT_OK;
  LinIf_ChannelContextType *context;
  const LinIf_ChannelConfigType *config;

  DET_VALIDATE(NULL != LINIF_CONFIG, 0x07, LINIF_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(Channel < LINIF_CONFIG->numOfChannels, 0x07, LINIF_E_NONEXISTENT_CHANNEL,
               return E_NOT_OK);

  config = &LINIF_CONFIG->channelConfigs[Channel];
  context = &LINIF_CONFIG->channelContexts[Channel];
  if (LINIF_CHANNEL_SLEEP == context->state) {
    /* @SWS_LinIf_00296 */
    ret = Lin_Wakeup(config->linChannel);
    if (E_OK == ret) { /* @SWS_LinIf_00478 */
      context->state = LINIF_CHANNEL_OPERATIONAL;
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
  for (i = 0; i < LINIF_CONFIG->numOfChannels; i++) {
    config = &LINIF_CONFIG->channelConfigs[i];
    context = &LINIF_CONFIG->channelContexts[i];
    context->status = LINIF_STATUS_IDLE;
    context->timer = 0;
    context->state = LINIF_CHANNEL_UNINIT;
    Lin_GoToSleepInternal(config->linChannel);
  }
}

#if (LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER
Std_ReturnType LinIf_ScheduleRequest(NetworkHandleType Channel, LinIf_SchHandleType Schedule) {
  Std_ReturnType r = E_OK;
  LinIf_ChannelContextType *context;

  DET_VALIDATE(NULL != LINIF_CONFIG, 0x05, LINIF_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(Channel < LINIF_CONFIG->numOfChannels, 0x05, LINIF_E_NONEXISTENT_CHANNEL,
               return E_NOT_OK);
  DET_VALIDATE(Schedule < LINIF_CONFIG->numOfSchTbls, 0x05, LINIF_E_PARAMETER, return E_NOT_OK);

#if LINIF_VARIANT == LINIF_VARIANT_BOTH
  DET_VALIDATE(LINIF_MASTER == LINIF_CONFIG->channelConfigs[Channel].nodeType, 0x05,
               LINIF_E_PARAMETER, return E_NOT_OK);
#endif

  context = &LINIF_CONFIG->channelContexts[Channel];
  DET_VALIDATE(LINIF_CHANNEL_OPERATIONAL == context->state, 0x05, LINIF_E_PARAMETER,
               return E_NOT_OK);
  context->scheduleRequested = Schedule;

  return r;
}
#endif /* LINIF_VARIANT_MASTER */

void LinIf_MainFunction_Read(void) {
#if (LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER
  int i;
  LinIf_ChannelContextType *context;

  DET_VALIDATE(NULL != LINIF_CONFIG, 0x80, LINIF_E_UNINIT, return);
  for (i = 0; (i < LINIF_CONFIG->numOfChannels); i++) {
    context = &LINIF_CONFIG->channelContexts[i];

    if (LINIF_STATUS_IDLE != context->status) {
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
  for (i = 0; (i < LINIF_CONFIG->numOfChannels); i++) {
#if LINIF_VARIANT == LINIF_VARIANT_BOTH
    config = &LINIF_CONFIG->channelConfigs[i];
    if (LINIF_MASTER == config->nodeType) {
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
Std_ReturnType LinIf_HeaderIndication(NetworkHandleType Channel, Lin_PduType *PduPtr) {
  LinIf_ChannelContextType *context;
  const LinIf_ChannelConfigType *config;
  const LinIf_ScheduleTableEntryType *entry;
  Std_ReturnType ret;
  uint16_t l;
  uint16_t h;
  uint16_t m;

  DET_VALIDATE(NULL != LINIF_CONFIG, 0x78, LINIF_E_UNINIT, return);
  DET_VALIDATE(Channel < LINIF_CONFIG->numOfChannels, 0x78, LINIF_E_NONEXISTENT_CHANNEL,
               return E_NOT_OK);

  context = &LINIF_CONFIG->channelContexts[Channel];
  config = &LINIF_CONFIG->channelConfigs[Channel];
#if LINIF_VARIANT == LINIF_VARIANT_BOTH
  DET_VALIDATE(LINIF_SLAVE == config->nodeType, 0x78, LINIF_E_PARAMETER, return E_NOT_OK);
#endif

  if (LINIF_STATUS_IDLE != context->status) {
    ASLOG(LINIFE, ("%d: slave get header when busy, curSch=%d\n", Channel, context->curSch));
    /* anyway, stop slave RX */
    context->status = LINIF_STATUS_IDLE;
  }

  l = 0;
  h = config->scheduleTable->numOfEntries - 1;
  ret = E_NOT_OK;
  entry = &config->scheduleTable->entrys[0];
  if (PduPtr->Pid < entry->id) {
    l = h + 1; /* avoid the underflow of "m - 1" */
  }
  while ((E_NOT_OK == ret) && (l <= h)) {
    m = l + ((h - l) >> 1);
    entry = &config->scheduleTable->entrys[m];
    if (entry->id > PduPtr->Pid) {
      h = m - 1;
    } else if (entry->id < PduPtr->Pid) {
      l = m + 1;
    } else {
      ret = E_OK;
    }
  }

  if (E_OK == ret) {
    if (LIN_FRAMERESPONSE_RX == entry->Drc) {
      context->curSch = m;
      PduPtr->Dl = entry->dlc;
      PduPtr->Cs = entry->Cs;
      PduPtr->Drc = entry->Drc;
      context->status = LINIF_STATUS_BUSY;
      context->timer = config->timeout;
    } else if (LIN_FRAMERESPONSE_TX == entry->Drc) {
      context->frame.Pid = entry->id;
      context->frame.Dl = entry->dlc;
      context->frame.SduPtr = context->data;
      context->frame.Drc = entry->Drc;
      ret = entry->callback(Channel, &context->frame, LINIF_R_TRIGGER_TRANSMIT);
      if (LINIF_R_OK == ret) {
        PduPtr->Pid = entry->id;
        PduPtr->Cs = entry->Cs;
        PduPtr->Drc = entry->Drc;
        PduPtr->Dl = entry->dlc;
        PduPtr->SduPtr = context->data;
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

void LinIf_RxIndication(NetworkHandleType Channel, uint8 *Lin_SduPtr) {
  LinIf_ChannelContextType *context;
  const LinIf_ChannelConfigType *config;
  const LinIf_ScheduleTableEntryType *entry;
  Std_ReturnType ret = E_OK;

  DET_VALIDATE(NULL != LINIF_CONFIG, 0x79, LINIF_E_UNINIT, return);
  DET_VALIDATE(Channel < LINIF_CONFIG->numOfChannels, 0x79, LINIF_E_NONEXISTENT_CHANNEL, return);

  context = &LINIF_CONFIG->channelContexts[Channel];
  config = &LINIF_CONFIG->channelConfigs[Channel];
#if LINIF_VARIANT == LINIF_VARIANT_BOTH
  DET_VALIDATE(LINIF_SLAVE == config->nodeType, 0x79, LINIF_E_PARAMETER, return);
#endif

  if (LINIF_STATUS_IDLE == context->status) {
    ASLOG(LINIFE, ("%d: slave get data in idle\n", Channel));
    ret = E_NOT_OK;
    context->timer = 0;
  } else if (context->curSch < config->scheduleTable->numOfEntries) {
    entry = &config->scheduleTable->entrys[context->curSch];
    context->timer = 0;
    context->status = LINIF_STATUS_IDLE;
  } else {
    ASLOG(LINIFE, ("%d: slave get data with invalid curSch=%X\n", Channel, context->curSch));
    ret = E_NOT_OK;
    context->timer = 0;
    context->status = LINIF_STATUS_IDLE;
  }

  if (E_OK == ret) {
    context->frame.Pid = entry->id;
    context->frame.Dl = entry->dlc;
    context->frame.SduPtr = context->data;
    context->frame.Drc = entry->Drc;
    memcpy(context->frame.SduPtr, Lin_SduPtr, entry->dlc);
    (void)entry->callback(Channel, &context->frame, LINIF_R_RECEIVED_OK);
  }
}
#endif /* LINIF_VARIANT_SLAVE */
