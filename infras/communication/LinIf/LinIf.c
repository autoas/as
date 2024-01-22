/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref:Specification of LIN Interface AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <string.h>
#include "LinIf_Internal.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_LINIF 0
#define AS_LOG_LINIFE 2
/* low 4 bit for what has been requested */
#define LINIF_STATUS_SCHEDULE_REQUESTED 0x01
#define LINIF_STATUS_SCHEDULE_NEXT_ONE 0x02
#define LINIF_STATUS_SCHEDULE_START 0x04
/* high 4 bit for what is on going */
#define LINIF_STATUS_SENDING 0x10   /* for master node */
#define LINIF_STATUS_RECEIVING 0x20 /* for slave node */

#define IS_LINIF_IDEL(status) (0 == ((status) & 0xF0))

#define LINIF_CONFIG (&LinIf_Config)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const LinIf_ConfigType LinIf_Config;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
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
      context->status |= LINIF_STATUS_SENDING;
    } else {
      ASLOG(LINIFE, ("%d: FAIL to send\n", Channel));
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
    }
  }

  if (result != LINIF_R_NOT_OK) {
    ASLOG(LINIF, ("%d: TX done with result %d\n", Channel, result));
    (void)entry->callback(Channel, &context->frame, result);
    context->status &= ~LINIF_STATUS_SENDING;
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void LinIf_Init(const LinIf_ConfigType *ConfigPtr) {
  int i;
  LinIf_ChannelContextType *context;
  const LinIf_ChannelConfigType *config;
  (void)ConfigPtr;
  for (i = 0; i < LINIF_CONFIG->numOfChannels; i++) {
    config = &LINIF_CONFIG->channelConfigs[i];
    context = &LINIF_CONFIG->channelContexts[i];
    context->status = 0;
    context->scheduleTable = NULL;
    context->curSch = 0;
    context->scheduleRequested = LINIF_INVALD_SCHEDULE_TABLE;
    Lin_SetControllerMode(config->linChannel, LIN_CS_STARTED);
    Std_TimerStop(&context->timer);
  }
}

void LinIf_DeInit() {
  int i;
  LinIf_ChannelContextType *context;
  const LinIf_ChannelConfigType *config;
  for (i = 0; i < LINIF_CONFIG->numOfChannels; i++) {
    config = &LINIF_CONFIG->channelConfigs[i];
    context = &LINIF_CONFIG->channelContexts[i];
    context->status = 0;
    Lin_SetControllerMode(config->linChannel, LIN_CS_STOPPED);
    Std_TimerStop(&context->timer);
  }
}

Std_ReturnType LinIf_ScheduleRequest(NetworkHandleType Channel, LinIf_SchHandleType Schedule) {
  Std_ReturnType r = E_NOT_OK;
  LinIf_ChannelContextType *context;

  if ((Channel < LINIF_CONFIG->numOfChannels)) {
    context = &LINIF_CONFIG->channelContexts[Channel];

    if (Schedule < LINIF_CONFIG->numOfSchTbls) {
      context->scheduleRequested = Schedule;
      r = E_OK;
    }
  }

  return r;
}

void LinIf_MainFunction(void) {
  int i;
  uint32_t elapsed;
  LinIf_ChannelContextType *context;
  const LinIf_ChannelConfigType *config;

  for (i = 0; (i < LINIF_CONFIG->numOfChannels); i++) {
    context = &LINIF_CONFIG->channelContexts[i];
    config = &LINIF_CONFIG->channelConfigs[i];

    if (0 != (context->status & LINIF_STATUS_SENDING)) {
      LinIf_CheckEntry(i);
    }

    if (context->scheduleRequested != LINIF_INVALD_SCHEDULE_TABLE) {
      if (IS_LINIF_IDEL(context->status)) {
        /* switch only when system is IDLE */
        context->scheduleTable = &LINIF_CONFIG->scheduleTables[context->scheduleRequested];
        ASLOG(LINIF, ("%d: switch to schedule table %d\n", i, context->scheduleRequested));
        context->scheduleRequested = LINIF_INVALD_SCHEDULE_TABLE;
        Std_TimerStop(&context->timer);
        if (0 < context->scheduleTable->numOfEntries) {
          context->curSch = context->scheduleTable->numOfEntries - 1; /* default to the end */
          if (LINIF_MASTER == config->nodeType) {
            context->status = LINIF_STATUS_SCHEDULE_REQUESTED | LINIF_STATUS_SCHEDULE_NEXT_ONE;
          } else {
            context->status = 0;
          }
        } else {
          context->curSch = 0;
          context->status = 0;
        }
      }
    }

    if (LINIF_SLAVE == config->nodeType) {
      if (Std_IsTimerStarted(&context->timer)) {
        elapsed = Std_GetTimerElapsedTime(&context->timer);
        if (elapsed > config->timeout) {
          ASLOG(LINIFE, ("%d: slave %u us timeout, status=%X\n", i, elapsed, context->status));
          context->status = 0;
          Std_TimerStop(&context->timer);
        }
      }
      continue;
    }

    if (0 != (context->status & LINIF_STATUS_SCHEDULE_REQUESTED)) {
      if (NULL != context->scheduleTable) {
        if (Std_IsTimerStarted(&context->timer)) {
          elapsed = Std_GetTimerElapsedTime(&context->timer);
          if (elapsed > context->scheduleTable->entrys[context->curSch].delay) {
            if (FALSE == IS_LINIF_IDEL(context->status)) {
              ASLOG(LINIFE, ("%d: master %u us timeout, status=%X\n", i, elapsed, context->status));
            }
            context->status = LINIF_STATUS_SCHEDULE_REQUESTED | LINIF_STATUS_SCHEDULE_NEXT_ONE;
            Std_TimerStart(&context->timer);
          }
        } else {
          Std_TimerStart(&context->timer);
        }
      }
    }

    if (0 != (context->status & LINIF_STATUS_SCHEDULE_NEXT_ONE)) {
      context->status &= ~LINIF_STATUS_SCHEDULE_NEXT_ONE;
      if (NULL != context->scheduleTable) {
        context->curSch++;
        if (context->curSch >= context->scheduleTable->numOfEntries) {
          context->curSch = 0;
        }
        context->status |= LINIF_STATUS_SCHEDULE_START;
      } else {
        ASLOG(LINIFE, ("%d: schedule requested with NULL schedule\n", i));
      }
    }

    if (0 != (context->status & LINIF_STATUS_SCHEDULE_START)) {
      context->status &= ~LINIF_STATUS_SCHEDULE_START;
      LinIf_ScheduleEntry(i);
    }
  }
}

Std_ReturnType LinIf_HeaderIndication(NetworkHandleType Channel, Lin_PduType *PduPtr) {
  LinIf_ChannelContextType *context;
  const LinIf_ChannelConfigType *config;
  const LinIf_ScheduleTableEntryType *entry;
  Std_ReturnType ret = E_NOT_OK;
  int l, h, m;

  if (Channel < LINIF_CONFIG->numOfChannels) {
    context = &LINIF_CONFIG->channelContexts[Channel];
    config = &LINIF_CONFIG->channelConfigs[Channel];
    if (LINIF_SLAVE == config->nodeType) {
      if (context->scheduleTable != NULL) {
        ret = E_OK;
      }
    }
  }

  if (E_OK == ret) {
    if (FALSE == IS_LINIF_IDEL(context->status)) {
      ASLOG(LINIFE, ("%d: slave get header when status=%X curSch=%d\n", Channel, context->status,
                     context->curSch));
      /* anyway, stop slave RX */
      context->status &= ~LINIF_STATUS_RECEIVING;
    }
  }

  if (E_OK == ret) {
    l = 0;
    h = context->scheduleTable->numOfEntries - 1;
    ret = E_NOT_OK;
    entry = &context->scheduleTable->entrys[0];
    if (PduPtr->Pid < entry->id) {
      l = h + 1; /* avoid the underflow of "m - 1" */
    }
    while ((E_NOT_OK == ret) && (l <= h)) {
      m = l + ((h - l) >> 1);
      entry = &context->scheduleTable->entrys[m];
      if (entry->id > PduPtr->Pid) {
        h = m - 1;
      } else if (entry->id < PduPtr->Pid) {
        l = m + 1;
      } else {
        ret = E_OK;
      }
    }
  }

  if (E_OK == ret) {
    if (LIN_FRAMERESPONSE_RX == entry->Drc) {
      context->curSch = m;
      PduPtr->Dl = entry->dlc;
      PduPtr->Cs = entry->Cs;
      PduPtr->Drc = entry->Drc;
      context->status |= LINIF_STATUS_RECEIVING;
      Std_TimerStart(&context->timer);
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
  Std_ReturnType ret = E_NOT_OK;

  if (Channel < LINIF_CONFIG->numOfChannels) {
    context = &LINIF_CONFIG->channelContexts[Channel];
    config = &LINIF_CONFIG->channelConfigs[Channel];
    if (LINIF_SLAVE == config->nodeType) {
      if (context->scheduleTable != NULL) {
        ret = E_OK;
      }
    }
  }

  if (E_OK == ret) {
    if (0 == (context->status & LINIF_STATUS_RECEIVING)) {
      ASLOG(LINIFE, ("%d: slave get data when status=%X\n", Channel, context->status));
      ret = E_NOT_OK;
      Std_TimerStop(&context->timer);
    } else if (context->curSch < context->scheduleTable->numOfEntries) {
      entry = &context->scheduleTable->entrys[context->curSch];
      Std_TimerStop(&context->timer);
      context->status &= ~LINIF_STATUS_RECEIVING;
    } else {
      ASLOG(LINIFE, ("%d: slave get data with invalid curSch=%X\n", Channel, context->curSch));
      ret = E_NOT_OK;
      Std_TimerStop(&context->timer);
      context->status &= ~LINIF_STATUS_RECEIVING;
    }
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