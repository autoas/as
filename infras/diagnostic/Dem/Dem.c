/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Diagnostic Event Manager AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dem_Priv.h"
#include "Std_Debug.h"
#include "Std_Critical.h"
#ifdef DEM_USE_NVM
#include "NvM.h"
#endif
#include <string.h>
#include "Std_Flag.h"
#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DEM 0
#define AS_LOG_DEMI 0
#define AS_LOG_DEME 2

#ifdef DEM_USE_PB_CONFIG
#define DEM_CONFIG demConfig
#else
#define DEM_CONFIG (&Dem_Config)
#endif

#define DEM_STATE_OFF ((Dem_StateType)0x00)
#define DEM_STATE_PRE_INIT ((Dem_StateType)0x01)
#define DEM_STATE_INIT ((Dem_StateType)0x02)

#define DEM_FREEZE_FRAME_SLOT_FREE 0xFFu
#define DEM_EXTENDED_DATA_SLOT_FREE 0xFFu

#define DEM_FILTER_BY_MASK 0x00
#define DEM_FILTER_BY_SELECTION 0x00
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  boolean disableDtcSetting;
  struct {
    uint32_t DTCNumber;
    uint16_t index;
    Dem_DTCOriginType DTCOrigin;
    uint8_t statusMask;
    uint8_t RecordNumber;
  } filter;
#ifdef DEM_USE_ENABLE_CONDITION
  uint32_t conditionMask;
#endif
} Dem_ContextType;
/* ================================ [ DECLARES  ] ============================================== */
extern CONSTANT(Dem_ConfigType, DEM_CONST) Dem_Config;
static uint8_t *Dem_MallocFreezeFrameRecord(Dem_DtcIdType DtcId,
                                            P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST)
                                              memory,
                                            uint16_t *ffId);
static Dem_FreezeFrameRecordType *Dem_LookupFreezeFrameRecordByRecordNumber(
  Dem_DtcIdType DtcId, P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory,
  uint8_t *recNum, uint8_t **data, uint16_t *where);
static void Dem_FreeFreezeFrameRecord(Dem_DtcIdType DtcId, P2CONST(Dem_MemoryDestinationType,
                                                                   AUTOMATIC, DEM_CONST) memory);
static void Dem_FreeExtendedDataRecord(Dem_DtcIdType DtcId, P2CONST(Dem_MemoryDestinationType,
                                                                    AUTOMATIC, DEM_CONST) memory);
/* ================================ [ DATAS     ] ============================================== */
static Dem_ContextType Dem_Context;
#ifdef DEM_USE_PB_CONFIG
static const Dem_ConfigType *demConfig = NULL;
#endif
/* ================================ [ LOCALS    ] ============================================== */
static void Dem_EventInit(Dem_EventIdType EventId) {
  Dem_EventContextType *EventContext = &DEM_CONFIG->EventContexts[EventId];
  Dem_EventStatusRecordType *EventStatus = DEM_CONFIG->EventStatusRecords[EventId];
  (void)EventStatus;

  EventContext->status = DEM_EVENT_STATUS_UNKNOWN;
  EventContext->flag = 0;
  EventContext->debouneCounter = 0;

  ASLOG(DEMI, ("Event %d status=%02X, testFailedCounter=%d\n", EventId, EventStatus->status,
               EventStatus->testFailedCounter));
}

#ifndef DEM_USE_NVM
static void Dem_SetDirty(uint8_t *mask, uint16_t pos) {
  mask[pos / 8] |= (pos & 0x07) << 1;
}

static void Dem_ClearDirty(uint8_t *mask, uint16_t pos) {
  mask[pos / 8] &= ~(pos & 0x07) << 1;
}

static boolean Dem_IsDirty(uint8_t *mask, uint16_t pos) {
  return (0u != (mask[pos / 8] & ((pos & 0x07) << 1)));
}
#endif
static P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST)
  Dem_LookupMemory(Dem_DTCOriginType DTCOrigin) {
  uint16_t i;
  P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory = NULL;

  for (i = 0; i < DEM_CONFIG->numOfMemoryDestination; i++) {
    if (DEM_CONFIG->MemoryDestination[i].DTCOrigin == DTCOrigin) {
      memory = &DEM_CONFIG->MemoryDestination[i];
      break;
    }
  }
  return memory;
}

static void Dem_DtcUpdateOnOperationCycleStart(P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) Dtc) {
  Dem_DtcStatusRecordType *StatusRecord;
  P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory;
  uint16_t i;
  for (i = 0; i < Dtc->DTCAttributes->numOfMemoryDestination; i++) {
    memory = Dtc->DTCAttributes->MemoryDestination[i];
    StatusRecord = memory->StatusRecords[Dtc->DtcId];
#ifdef DEM_STATUS_BIT_STORAGE_TEST_FAILED
/* @SWS_Dem_00525: retain the information for UDS status bit 0 over power cycles (non-volatile) */
#else
    /* @SWS_Dem_00388 */
    Dem_UdsBitClear(StatusRecord->status, DEM_UDS_STATUS_TF);
#endif

    /* @SWS_Dem_00389 */
    Dem_UdsBitClear(StatusRecord->status, DEM_UDS_STATUS_TFTOC);
    /* @SWS_Dem_00394 */
    Dem_UdsBitSet(StatusRecord->status, DEM_UDS_STATUS_TNCTOC);
  }
}

static void Dem_StartOperationCycle(uint8_t OperationCycleId) {
  uint16_t i;
  Dem_EventContextType *EventContext;
  P2CONST(Dem_EventConfigType, AUTOMATIC, DEM_CONST) EventConfig;
  Dem_EventStatusRecordType *EventStatus;

  ASLOG(DEMI, ("Operation Cycle %d Start\n", OperationCycleId));

  for (i = 0; i < DEM_CONFIG->numOfEvents; i++) {
    EventConfig = &DEM_CONFIG->EventConfigs[i];
    EventContext = &DEM_CONFIG->EventContexts[i];
    EventStatus = DEM_CONFIG->EventStatusRecords[i];

    if (EventConfig->OperationCycleRef == OperationCycleId) {
      EventContext->status = DEM_EVENT_STATUS_UNKNOWN;
      EventContext->flag = 0;
      EventContext->debouneCounter = 0;
#ifdef DEM_STATUS_BIT_STORAGE_TEST_FAILED
#else
      Dem_UdsBitClear(EventStatus->status, DEM_UDS_STATUS_TF);
#endif
      /* @SWS_Dem_00389 */
      Dem_UdsBitClear(EventStatus->status, DEM_UDS_STATUS_TFTOC);
      /* @SWS_Dem_00394 */
      Dem_UdsBitSet(EventStatus->status, DEM_UDS_STATUS_TNCTOC);
      if (NULL != EventConfig->DTCRef) {
        Dem_DtcUpdateOnOperationCycleStart(EventConfig->DTCRef);
      }
    }

    if (NULL != EventConfig->DemCallbackInitMForE) {
      /* @SWS_Dem_00679 */
      EventConfig->DemCallbackInitMForE(DEM_INIT_MONITOR_RESTART);
    }
  }
}

static void Dem_DtcUpdateOnOperationCycleStop(P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) Dtc,
                                              boolean *bAged) {
  Dem_DtcStatusRecordType *StatusRecord;
  P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory;
  uint16_t i;
  boolean bDirty;
  *bAged = FALSE;
  for (i = 0; i < Dtc->DTCAttributes->numOfMemoryDestination; i++) {
    bDirty = FALSE;
    memory = Dtc->DTCAttributes->MemoryDestination[i];
    StatusRecord = memory->StatusRecords[Dtc->DtcId];
    if (0u == (StatusRecord->status & (DEM_UDS_STATUS_TNCTOC | DEM_UDS_STATUS_TFTOC))) {
      /* ref: Figure 7.43: General diagnostic event deletion processing */
      if (0u != (StatusRecord->status & DEM_UDS_STATUS_PDTC)) {
        /* @SWS_Dem_00390 */
        Dem_UdsBitClear(StatusRecord->status, DEM_UDS_STATUS_PDTC);
        bDirty = TRUE;
      }
      if (0u != (StatusRecord->status & DEM_UDS_STATUS_CDTC)) {
        if ((TRUE == Dtc->DTCAttributes->AgingAllowed) &&
            (StatusRecord->agingCounter < Dtc->DTCAttributes->AgingCycleCounterThreshold)) {
          StatusRecord->agingCounter++;
          bDirty = TRUE;
          ASLOG(DEMI,
                ("DTC %d aging %d out of %d on origin %d\n", Dtc->DtcId, StatusRecord->agingCounter,
                 Dtc->DTCAttributes->AgingCycleCounterThreshold, memory->DTCOrigin));
          if (StatusRecord->agingCounter >= Dtc->DTCAttributes->AgingCycleCounterThreshold) {
            *bAged = TRUE;
            /* @SWS_Dem_00498 */
            Dem_UdsBitClear(StatusRecord->status, DEM_UDS_STATUS_CDTC);
#if DEM_STATUS_BIT_HANDLING_TEST_FAILED_SINCE_LAST_CLEAR == DEM_STATUS_BIT_AGING_AND_DISPLACEMENT
            /* @SWS_Dem_01054 */
            Dem_UdsBitClear(StatusRecord->status, DEM_UDS_STATUS_TFSLC);
#endif
            StatusRecord->agingCounter = 0;
            StatusRecord->faultOccuranceCounter = 0;
#ifdef DEM_USE_CYCLES_SINCE_LAST_FAILED
            StatusRecord->cyclesSinceLastFailed = DEM_CYCLE_COUNTER_STOPPED;
#endif
            /* @SWS_Dem_01075 */
            Dem_FreeFreezeFrameRecord(Dtc->DtcId, memory);
#ifdef DEM_USE_NVM_EXTENDED_DATA
            Dem_FreeExtendedDataRecord(Dtc->DtcId, memory);
#endif

            if (StatusRecord->agedCounter < UINT8_MAX) {
              StatusRecord->agedCounter++;
            }
          }
        }
      }
    }

#ifdef DEM_USE_CYCLES_SINCE_LAST_FAILED /* @SWS_Dem_00773 @SWS_Dem_00774 */
    if (StatusRecord->cyclesSinceLastFailed < DEM_CYCLE_COUNTER_MAX) {
      StatusRecord->cyclesSinceLastFailed = ++;
      bDirty = TRUE;
    }
#endif

    if (TRUE == bDirty) {
#ifdef DEM_USE_NVM
      (void)NvM_WriteBlock(memory->StatusNvmBlockIds[Dtc->DtcId], NULL);
#else
      Dem_SetDirty(memory->StatusRecordsDirty, Dtc->DtcId);
#endif
    }
  }
}

static void Dem_StopOperationCycle(uint8_t OperationCycleId) {
  uint16_t i;
  uint16_t j;
  P2CONST(Dem_EventConfigType, AUTOMATIC, DEM_CONST) EventConfig;
  Dem_EventStatusRecordType *EventStatus;
  P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) DTCRef;
  boolean bAged = FALSE;
  boolean bUpdated = FALSE;
  Dem_EventIdType EventId;

  ASLOG(DEMI, ("Operation Cycle %d Stop\n", OperationCycleId));

  for (i = 0; i < DEM_CONFIG->numOfDtcs; i++) {
    DTCRef = &DEM_CONFIG->Dtcs[i];
    EventId = DTCRef->EventIdRefs[0];
    /* the main event that this DTC belongs to */
    EventConfig = &DEM_CONFIG->EventConfigs[EventId];
    if (EventConfig->OperationCycleRef == OperationCycleId) {
      Dem_DtcUpdateOnOperationCycleStop(DTCRef, &bAged);
    }

    if (TRUE == bAged) {
      /* @SWS_Dem_00442 */
      for (j = 0; j < DTCRef->numOfEvents; j++) {
        EventId = DTCRef->EventIdRefs[j];
        EventStatus = DEM_CONFIG->EventStatusRecords[EventId];
        bUpdated = FALSE;

        if (0u != (EventStatus->status & DEM_UDS_STATUS_CDTC)) { /* @SWS_Dem_00498 */
          Dem_UdsBitClear(EventStatus->status, DEM_UDS_STATUS_CDTC);
          bUpdated = TRUE;
        }
#if DEM_STATUS_BIT_HANDLING_TEST_FAILED_SINCE_LAST_CLEAR == DEM_STATUS_BIT_AGING_AND_DISPLACEMENT
        if (0u != (EventStatus->status & DEM_UDS_STATUS_TFSLC)) { /* @SWS_Dem_01054 */
          Dem_UdsBitClear(EventStatus->status, DEM_UDS_STATUS_TFSLC);
          bUpdated = TRUE;
        }
#endif

        if (EventStatus->testFailedCounter != 0u) {
          EventStatus->testFailedCounter = 0;
          bUpdated = TRUE;
        }

        if (TRUE == bUpdated) {
#ifdef DEM_USE_NVM
          (void)NvM_WriteBlock(DEM_CONFIG->EventStatusNvmBlockIds[EventId], NULL);
#else
          Dem_SetDirty(DEM_CONFIG->EventStatusDirty, EventId);
#endif
        }
      }
    } else {
      for (j = 0; j < DTCRef->numOfEvents; j++) {
        EventId = DTCRef->EventIdRefs[j];
        EventStatus = DEM_CONFIG->EventStatusRecords[EventId];
        if (0u == (EventStatus->status & (DEM_UDS_STATUS_TNCTOC | DEM_UDS_STATUS_TFTOC))) {
          if (0u != (EventStatus->status & DEM_UDS_STATUS_PDTC)) {
            Dem_UdsBitClear(EventStatus->status, DEM_UDS_STATUS_PDTC);
            EventStatus->testFailedCounter = 0;
#ifdef DEM_USE_NVM
            (void)NvM_WriteBlock(DEM_CONFIG->EventStatusNvmBlockIds[EventId], NULL);
#else
            Dem_SetDirty(DEM_CONFIG->EventStatusDirty, EventId);
#endif
          }
        }
      }
    }
  }
}

static Std_ReturnType Dem_IsOperationCycleStarted(uint8_t OperationCycleId) {
  Std_ReturnType r = E_NOT_OK;
  if (DEM_OPERATION_CYCLE_STARTED == DEM_CONFIG->OperationCycleStates[OperationCycleId]) {
    r = E_OK;
  }
  return r;
}

#ifdef DEM_USE_ENABLE_CONDITION
static Std_ReturnType Dem_IsEventConditionEnabled(P2CONST(Dem_EventConfigType, AUTOMATIC, DEM_CONST)
                                                    EventConfig) {
  Std_ReturnType r = E_NOT_OK;
  if ((0u == EventConfig->ConditionRefMask) ||
      (EventConfig->ConditionRefMask ==
       (Dem_Context.conditionMask & EventConfig->ConditionRefMask))) {
    r = E_OK;
  }
  return r;
}
#endif

static Std_ReturnType Dem_SetEventStatusDebounce(Dem_EventIdType EventId,
                                                 Dem_EventStatusType EventStatus) {
  Std_ReturnType r = E_OK;
  P2CONST(Dem_EventConfigType, AUTOMATIC, DEM_CONST) EventConfig;
  Dem_EventContextType *EventContext;
  P2CONST(Dem_DebounceCounterBasedConfigType, AUTOMATIC, DEM_CONST) DebounceCfg;
  Dem_EventStatusType DebouncedStatus;

  EventConfig = &DEM_CONFIG->EventConfigs[EventId];
  EventContext = &DEM_CONFIG->EventContexts[EventId];
  DebounceCfg = EventConfig->DebounceCounterBased;
  DebouncedStatus = EventContext->status;

  switch (EventStatus) {
  case DEM_EVENT_STATUS_PASSED:
    EventContext->debouneCounter = DebounceCfg->DebounceCounterPassedThreshold;
    break;
  case DEM_EVENT_STATUS_FAILED:
    EventContext->debouneCounter = DebounceCfg->DebounceCounterFailedThreshold;
    break;
  case DEM_EVENT_STATUS_PREPASSED:
    /* @SWS_Dem_00423 */
    if ((DebounceCfg->DebounceCounterJumpDown) &&
        (EventContext->debouneCounter > DebounceCfg->DebounceCounterJumpDownValue)) {
      EventContext->debouneCounter = DebounceCfg->DebounceCounterJumpDownValue;
    }

    if (EventContext->debouneCounter >= (DebounceCfg->DebounceCounterPassedThreshold +
                                         DebounceCfg->DebounceCounterDecrementStepSize)) {
      EventContext->debouneCounter -= DebounceCfg->DebounceCounterDecrementStepSize;
    } else {
      EventContext->debouneCounter = DebounceCfg->DebounceCounterPassedThreshold;
    }
    break;
  case DEM_EVENT_STATUS_PREFAILED:
    /* @SWS_Dem_00425 */
    if ((DebounceCfg->DebounceCounterJumpUp) &&
        (EventContext->debouneCounter < DebounceCfg->DebounceCounterJumpUpValue)) {
      EventContext->debouneCounter = DebounceCfg->DebounceCounterJumpUpValue;
    }

    if (EventContext->debouneCounter <= (DebounceCfg->DebounceCounterFailedThreshold -
                                         DebounceCfg->DebounceCounterIncrementStepSize)) {
      EventContext->debouneCounter += DebounceCfg->DebounceCounterIncrementStepSize;
    } else {
      EventContext->debouneCounter = DebounceCfg->DebounceCounterFailedThreshold;
    }
    break;
  default:
    r = E_NOT_OK;
    break;
  }

  ASLOG(DEMI, ("Event %d debounce counter = %d, PASS=%d(-%d), FAIL=%d(+%d)\n", EventId,
               (int)EventContext->debouneCounter, (int)DebounceCfg->DebounceCounterPassedThreshold,
               (int)DebounceCfg->DebounceCounterDecrementStepSize,
               (int)DebounceCfg->DebounceCounterFailedThreshold,
               (int)DebounceCfg->DebounceCounterIncrementStepSize));

  if (EventContext->debouneCounter <= DebounceCfg->DebounceCounterPassedThreshold) {
    DebouncedStatus = DEM_EVENT_STATUS_PASSED;
  } else if (EventContext->debouneCounter >= DebounceCfg->DebounceCounterFailedThreshold) {
    DebouncedStatus = DEM_EVENT_STATUS_FAILED;
  } else {
    /* keep no change */
  }

  EventContext->status = DebouncedStatus;

  return r;
}

static uint16_t Dem_FillExtendedData(P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) Dtc,
                                     P2CONST(Dem_ExtendedDataRecordClassType, AUTOMATIC, DEM_CONST)
                                       ExtendedDataRecordClass,
                                     uint8_t *data, Dem_DTCOriginType DTCOrigin) {
  uint16_t offset = 0;
  uint16_t i;
  uint8_t index;
  Std_ReturnType ret;

  for (i = 0; i < ExtendedDataRecordClass->numOfExtendedData; i++) {
    index = ExtendedDataRecordClass->ExtendedDataNumberIndex[i];
    ret = DEM_CONFIG->ExtendedDataConfigs[index].GetExtendedDataFnc(Dtc->DtcId, &data[offset],
                                                                    DTCOrigin);
    if (E_OK == ret) {
      offset += DEM_CONFIG->ExtendedDataConfigs[index].length;
    } else {
      offset = 0; /* error return */
      break;
    }
  }

  return offset;
}

#ifdef DEM_RESET_CONFIRMED_BIT_ON_OVERFLOW
static void Dem_HandleDtcDisplacement(Dem_DtcIdType DtcId, P2CONST(Dem_MemoryDestinationType,
                                                                   AUTOMATIC, DEM_CONST) memory) {
  P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) Dtc;
  Dem_EventIdType EventId;
  Dem_EventStatusRecordType *EventStatus;
  uint16_t i;
  boolean bDirty;
  if (memory->DTCOrigin == DEM_DTC_ORIGIN_PRIMARY_MEMORY) {
    /* TODO: is this right that do this only for origin primary */
    Dtc = &DEM_CONFIG->Dtcs[DtcId];
    /* @SWS_Dem_00443 */
    for (i = 0; i < Dtc->numOfEvents; i++) {
      EventId = Dtc->EventIdRefs[i];
      EventStatus = DEM_CONFIG->EventStatusRecords[EventId];
      bDirty = FALSE;

      /* @SWS_Dem_00409 */
      if (0u != ((DEM_UDS_STATUS_PDTC | DEM_UDS_STATUS_CDTC) & EventStatus->status)) {
        Dem_UdsBitClear(EventStatus->status, DEM_UDS_STATUS_PDTC | DEM_UDS_STATUS_CDTC);
        bDirty = TRUE;
      }

#if DEM_STATUS_BIT_HANDLING_TEST_FAILED_SINCE_LAST_CLEAR == DEM_STATUS_BIT_AGING_AND_DISPLACEMENT
      /* @SWS_Dem_01186 */
      if (0u != (DEM_UDS_STATUS_TFSLC & EventStatus->status)) {
        Dem_UdsBitClear(EventStatus->status, DEM_UDS_STATUS_TFSLC);
        bDirty = TRUE;
      }
#endif
      if (TRUE == bDirty) {
#ifdef DEM_USE_NVM
        (void)NvM_WriteBlock(DEM_CONFIG->EventStatusNvmBlockIds[EventId], NULL);
#else
        Dem_SetDirty(DEM_CONFIG->EventStatusDirty, EventId);
#endif
      }
    }
  }
}
#endif

static uint8_t *Dem_MallocFreezeFrameRecord(Dem_DtcIdType DtcId,
                                            P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST)
                                              memory,
                                            uint16_t *ffId) {
  Dem_FreezeFrameRecordType *record = NULL;
  uint8_t *data;
  uint16_t i;
  uint16_t slot;
  uint8_t lowPriority;
  Dem_DtcIdType lowDtcId;
  *ffId = ((uint16_t)-1);

  EnterCritical();

  data = NULL;

  for (i = 0; i < memory->numOfFreezeFrameRecords; i++) {
    if (DEM_INVALID_DTC_ID == memory->FreezeFrameRecords[i]->DtcId) {
      if (NULL == record) {
        record = memory->FreezeFrameRecords[i];
        *ffId = i;
      }
    } else if (DtcId == memory->FreezeFrameRecords[i]->DtcId) {
      record = memory->FreezeFrameRecords[i];
      *ffId = i;
      break;
    } else {
    }
  }

  if (NULL == record) {
    /* looking for displacement according to priority */
    lowPriority = DEM_CONFIG->Dtcs[DtcId].DTCAttributes->Priority;
    for (i = 0; i < memory->numOfFreezeFrameRecords; i++) {
      lowDtcId = memory->FreezeFrameRecords[i]->DtcId;
      if (lowDtcId < DEM_CONFIG->numOfDtcs) {
        if (DEM_CONFIG->Dtcs[lowDtcId].DTCAttributes->Priority > lowPriority) {
          record = memory->FreezeFrameRecords[i];
          *ffId = i;
          lowPriority = DEM_CONFIG->Dtcs[lowDtcId].DTCAttributes->Priority;
        }
      } else {
        ASLOG(DEME, ("an invalid freeze frame found, drop it\n"));
        record = memory->FreezeFrameRecords[i];
        *ffId = i;
        break;
      }
    }
  }

  if (NULL != record) {
    if (DtcId != record->DtcId) {
      if (record->DtcId < DEM_CONFIG->numOfDtcs) {
        ASLOG(DEMI, ("replace DTC %d freeze frame @ %d for DTC %d\n", record->DtcId, *ffId, DtcId));
#ifdef DEM_RESET_CONFIRMED_BIT_ON_OVERFLOW
        Dem_HandleDtcDisplacement(record->DtcId, memory);
#endif
      }
      /* newly created */
      for (slot = 0; slot < DEM_MAX_FREEZE_FRAME_NUMBER; slot++) {
        record->FreezeFrameData[slot][0] =
          DEM_FREEZE_FRAME_SLOT_FREE; /* First byte set to 0xFFu: means free */
      }
      data = record->FreezeFrameData[0];
      record->DtcId = DtcId;
    } else {
      for (slot = 0; slot < DEM_MAX_FREEZE_FRAME_NUMBER; slot++) {
        if (DEM_FREEZE_FRAME_SLOT_FREE == record->FreezeFrameData[slot][0]) {
          data = record->FreezeFrameData[slot];
          break;
        }
      }

      if (NULL == data) {
        /* overwrite the last one with latest environment data */
        data = record->FreezeFrameData[DEM_MAX_FREEZE_FRAME_NUMBER - 1u];
      }
    }
  }

  ExitCritical();

  return data;
}

#ifdef DEM_USE_NVM_EXTENDED_DATA
static uint8_t *Dem_MallocExtendedDataRecord(Dem_DtcIdType DtcId,
                                             P2CONST(Dem_MemoryDestinationType, AUTOMATIC,
                                                     DEM_CONST) memory,
                                             uint16_t *eeId) {
  Dem_ExtendedDataRecordType *record = NULL;
  uint8_t *data;
  uint16_t i;
  uint8_t lowPriority;
  Dem_DtcIdType lowDtcId;
  *eeId = ((uint16_t)-1);

  EnterCritical();

  data = NULL;

  for (i = 0; i < memory->numOfExtendedDataRecords; i++) {
    if (DEM_INVALID_DTC_ID == memory->ExtendedDataRecords[i]->DtcId) {
      if (NULL == record) {
        record = memory->ExtendedDataRecords[i];
        *eeId = i;
      }
    } else if (DtcId == memory->ExtendedDataRecords[i]->DtcId) {
      record = memory->ExtendedDataRecords[i];
      *eeId = i;
      break;
    } else {
    }
  }

  if (NULL == record) {
    /* looking for displacement according to priority */
    lowPriority = DEM_CONFIG->Dtcs[DtcId].DTCAttributes->Priority;
    for (i = 0; i < memory->numOfExtendedDataRecords; i++) {
      lowDtcId = memory->ExtendedDataRecords[i]->DtcId;
      if (lowDtcId < DEM_CONFIG->numOfDtcs) {
        if (DEM_CONFIG->Dtcs[lowDtcId].DTCAttributes->Priority > lowPriority) {
          record = memory->ExtendedDataRecords[i];
          *eeId = i;
          lowPriority = DEM_CONFIG->Dtcs[lowDtcId].DTCAttributes->Priority;
        }
      } else {
        ASLOG(DEME, ("an invalid extended data found, drop it\n"));
        record = memory->ExtendedDataRecords[i];
        *eeId = i;
        break;
      }
    }
  }

  if (NULL != record) {
    if (DtcId != record->DtcId) {
      if (record->DtcId < DEM_CONFIG->numOfDtcs) {
        ASLOG(DEMI,
              ("replace DTC %d extended data @ %d for DTC %d\n", record->DtcId, *eeId, DtcId));
      }
      /* newly created */
      (void)memset(record->ExtendedData, DEM_EXTENDED_DATA_SLOT_FREE, sizeof(record->ExtendedData));
      data = record->ExtendedData;
      record->DtcId = DtcId;
    } else {
      data = record->ExtendedData;
    }
  }

  ExitCritical();

  return data;
}

static Dem_ExtendedDataRecordType *
Dem_LookupExtendedDataRecord(Dem_DtcIdType DtcId,
                             P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory) {
  Dem_ExtendedDataRecordType *record = NULL;
  uint16_t i;

  for (i = 0; i < memory->numOfExtendedDataRecords; i++) {
    if (DtcId == memory->ExtendedDataRecords[i]->DtcId) {
      record = memory->ExtendedDataRecords[i];
    }
  }

  return record;
}
#endif

static Dem_FreezeFrameRecordType *Dem_LookupFreezeFrameRecordByRecordNumber(
  Dem_DtcIdType DtcId, P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory,
  uint8_t *recNum, uint8_t **data, uint16_t *where) {
  Dem_FreezeFrameRecordType *record = NULL;
  uint16_t index;
  uint16_t i;
  uint16_t slot;

  for (index = *where; index < (memory->numOfFreezeFrameRecords * DEM_MAX_FREEZE_FRAME_NUMBER);
       index++) {
    i = index / DEM_MAX_FREEZE_FRAME_NUMBER;
    slot = index % DEM_MAX_FREEZE_FRAME_NUMBER;
    if ((DtcId == memory->FreezeFrameRecords[i]->DtcId) &&
        (DEM_FREEZE_FRAME_SLOT_FREE != memory->FreezeFrameRecords[i]->FreezeFrameData[slot][0])) {
      if ((*recNum == memory->FreezeFrameRecords[i]->FreezeFrameData[slot][0]) ||
          (0xFFu == *recNum)) {
        record = memory->FreezeFrameRecords[i];
        if (data != NULL) {
          *data = &memory->FreezeFrameRecords[i]->FreezeFrameData[slot][1];
        }
        *recNum = memory->FreezeFrameRecords[i]->FreezeFrameData[slot][0];
        *where = index + 1u;
        break;
      }
    }
  }

  return record;
}

static uint16_t Dem_GetNumberOfFreezeFrameRecordsByRecordNumber(
  Dem_DtcIdType DtcId, P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory,
  uint8_t recNum) {
  uint16_t r = 0;
  uint16_t index = 0;
  uint8_t _recNum;

  Dem_FreezeFrameRecordType *record = NULL;

  do {
    _recNum = recNum;
    record = Dem_LookupFreezeFrameRecordByRecordNumber(DtcId, memory, &_recNum, NULL, &index);
    if (record != NULL) {
      r++;
    }
  } while (record != NULL);

  return r;
}

static Dem_FreezeFrameRecordType *
Dem_LookupFreezeFrameRecordByIndex(P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory,
                                   uint16_t index, uint8_t *recNum, uint8_t **data) {
  Dem_FreezeFrameRecordType *record = NULL;
  uint16_t i;
  uint16_t slot;

  i = index / DEM_MAX_FREEZE_FRAME_NUMBER;
  slot = index % DEM_MAX_FREEZE_FRAME_NUMBER;

  if (i < memory->numOfFreezeFrameRecords) {
    if ((DEM_INVALID_DTC_ID != memory->FreezeFrameRecords[i]->DtcId) &&
        (DEM_FREEZE_FRAME_SLOT_FREE != memory->FreezeFrameRecords[i]->FreezeFrameData[slot][0])) {
      record = memory->FreezeFrameRecords[i];
      if (NULL != recNum) {
        *recNum = record->FreezeFrameData[slot][0];
      }
      if (NULL != data) {
        *data = &record->FreezeFrameData[slot][1];
      }
    }
  }

  return record;
}

static void Dem_FreeFreezeFrameRecord(Dem_DtcIdType DtcId, P2CONST(Dem_MemoryDestinationType,
                                                                   AUTOMATIC, DEM_CONST) memory) {
  Dem_FreezeFrameRecordType *record = NULL;
  uint16_t i;

  for (i = 0; i < memory->numOfFreezeFrameRecords; i++) {
    if (DtcId == memory->FreezeFrameRecords[i]->DtcId) {
      ASLOG(DEMI,
            ("Delete DTC %d freeze frame @ %d from origin %d\n", DtcId, i, memory->DTCOrigin));
      record = memory->FreezeFrameRecords[i];
      record->DtcId = DEM_INVALID_DTC_ID;
      (void)memset(record->FreezeFrameData, DEM_FREEZE_FRAME_SLOT_FREE,
                   sizeof(record->FreezeFrameData));
#ifdef DEM_USE_NVM
      (void)NvM_WriteBlock(memory->FreezeFrameNvmBlockIds[i], NULL);
#else
      Dem_SetDirty(memory->FreezeFrameRecordsDirty, i);
#endif
    }
  }
}
#ifdef DEM_USE_NVM_EXTENDED_DATA
static void Dem_FreeExtendedDataRecord(Dem_DtcIdType DtcId, P2CONST(Dem_MemoryDestinationType,
                                                                    AUTOMATIC, DEM_CONST) memory) {
  Dem_ExtendedDataRecordType *record = NULL;
  uint16_t i;

  for (i = 0; i < memory->numOfExtendedDataRecords; i++) {
    if (DtcId == memory->ExtendedDataRecords[i]->DtcId) {
      ASLOG(DEMI,
            ("Delete DTC %d extended data @ %d from origin %d\n", DtcId, i, memory->DTCOrigin));
      record = memory->ExtendedDataRecords[i];
      record->DtcId = DEM_INVALID_DTC_ID;
      (void)memset(record->ExtendedData, DEM_EXTENDED_DATA_SLOT_FREE, sizeof(record->ExtendedData));
#ifdef DEM_USE_NVM
      (void)NvM_WriteBlock(memory->ExtendedDataNvmBlockIds[i], NULL);
#else
      Dem_SetDirty(memory->ExtendedDataRecordsDirty, i);
#endif
    }
  }
}
#endif

static uint16_t Dem_GetExtendedDataRecordSize(P2CONST(Dem_ExtendedDataRecordClassType, AUTOMATIC,
                                                      DEM_CONST) ExtendedDataRecordClass) {
  uint16_t sz = 0;
  uint16_t index;
  uint16_t i;

  for (i = 0; i < ExtendedDataRecordClass->numOfExtendedData; i++) {
    index = ExtendedDataRecordClass->ExtendedDataNumberIndex[i];
    sz += DEM_CONFIG->ExtendedDataConfigs[index].length;
  }

  return sz;
}

static uint16_t Dem_GetFreezeFrameSize(P2CONST(Dem_FreezeFrameRecordClassType, AUTOMATIC, DEM_CONST)
                                         FreezeFrameRecordClass) {
  uint16_t sz = 0;
  uint16_t i;
  uint16_t index;

  for (i = 0; i < FreezeFrameRecordClass->numOfFreezeFrameData; i++) {
    index = FreezeFrameRecordClass->freezeFrameDataIndex[i];
    sz += DEM_CONFIG->FreeFrameDataConfigs[index].length;
  }

  return sz;
}
static void Dem_FillSnapshotRecord(uint8_t *DestBuf, uint8_t *data,
                                   P2CONST(Dem_FreezeFrameRecordClassType, AUTOMATIC, DEM_CONST)
                                     FreezeFrameRecordClass) {
  uint16_t i;
  uint16_t index;
  uint8_t *pIn = data;
  uint8_t *pOut = DestBuf;

  for (i = 0; i < FreezeFrameRecordClass->numOfFreezeFrameData; i++) {
    index = FreezeFrameRecordClass->freezeFrameDataIndex[i];
    pOut[0] = (DEM_CONFIG->FreeFrameDataConfigs[index].id >> 8) & 0xFFu;
    pOut[1] = DEM_CONFIG->FreeFrameDataConfigs[index].id & 0xFFu;
    (void)memcpy(&pOut[2], pIn, DEM_CONFIG->FreeFrameDataConfigs[index].length);
    pOut += 2u + DEM_CONFIG->FreeFrameDataConfigs[index].length;
    pIn += DEM_CONFIG->FreeFrameDataConfigs[index].length;
  }
}

static uint8_t Dem_GetNextRecordNumber(Dem_DtcIdType DtcId, P2CONST(Dem_MemoryDestinationType,
                                                                    AUTOMATIC, DEM_CONST) memory) {
  uint8_t recNum;
  uint16_t numOfRecs;
  P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) Dtc = &DEM_CONFIG->Dtcs[DtcId];
  const Dem_FreezeFrameRecNumClassType *FreezeFrameRecNumClass =
    Dtc->DTCAttributes->FreezeFrameRecNumClass;

  numOfRecs = Dem_GetNumberOfFreezeFrameRecordsByRecordNumber(DtcId, memory, 0xFFu);

  if (DEM_FF_RECNUM_CONFIGURED == DEM_CONFIG->TypeOfFreezeFrameRecordNumeration) {
    /* @SWS_Dem_00337 @SWS_Dem_00582 */
    if (numOfRecs < FreezeFrameRecNumClass->numOfFreezeFrameRecNums) {
      recNum = FreezeFrameRecNumClass->FreezeFrameRecNums[numOfRecs];
    } else {
      recNum = FreezeFrameRecNumClass
                 ->FreezeFrameRecNums[FreezeFrameRecNumClass->numOfFreezeFrameRecNums - 1u];
    }
  } else {
    /* @SWS_Dem_00581 */
    recNum = (uint8_t)numOfRecs + 1u;
  }

  return recNum;
}

static void Dem_TrigerStoreFreezeFrame(P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) DTCRef,
                                       P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST)
                                         memory) {
  P2CONST(Dem_FreezeFrameRecordClassType, AUTOMATIC, DEM_CONST)
  FreezeFrameRecordClass = DTCRef->DTCAttributes->FreezeFrameRecordClass;
  uint8_t *data = NULL;
  uint16_t ffId = 0;
  uint16_t offset = 1;
  uint16_t i;
  uint16_t index;

  data = Dem_MallocFreezeFrameRecord(DTCRef->DtcId, memory, &ffId);
  if (data != NULL) {
    for (i = 0; i < FreezeFrameRecordClass->numOfFreezeFrameData; i++) {
      index = FreezeFrameRecordClass->freezeFrameDataIndex[i];
      if ((offset + DEM_CONFIG->FreeFrameDataConfigs[index].length) <=
          DEM_MAX_FREEZE_FRAME_DATA_SIZE) {
        DEM_CONFIG->FreeFrameDataConfigs[index].GetFrezeFrameDataFnc(DTCRef->DtcId, &data[offset],
                                                                     memory->DTCOrigin);
        offset += DEM_CONFIG->FreeFrameDataConfigs[index].length;
      } else {
        ASLOG(DEME, ("FreezeFrame record size too small\n"));
      }
    }
    data[0] = Dem_GetNextRecordNumber(DTCRef->DtcId, memory);
    ASLOG(DEMI, ("DTC %d capture freeze frame with record number %d on origin %d\n", DTCRef->DtcId,
                 data[0], memory->DTCOrigin));
#ifdef DEM_USE_NVM
    (void)NvM_WriteBlock(memory->FreezeFrameNvmBlockIds[ffId], NULL);
#else
    Dem_SetDirty(memory->FreezeFrameRecordsDirty, ffId);
#endif
  } else {
    ASLOG(DEMI, ("DTC %d freeze failed as no slot\n", DTCRef->DtcId));
  }
}

#ifdef DEM_USE_NVM_EXTENDED_DATA
static void Dem_TrigerStoreExtendedData(P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) DTCRef,
                                        P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST)
                                          memory) {
  P2CONST(Dem_ExtendedDataClassType, AUTOMATIC, DEM_CONST)
  ExtendedDataClass = DTCRef->DTCAttributes->ExtendedDataClass;
  P2CONST(Dem_ExtendedDataRecordClassType, AUTOMATIC, DEM_CONST) ExtendedDataRecordClassRef;
  uint16_t i;
  uint16_t eeId = 0;
  uint8_t *data = NULL;
  uint16_t offset = 0;
  uint16_t ret = 0;

  data = Dem_MallocExtendedDataRecord(DTCRef->DtcId, memory, &eeId);
  if (NULL != data) {
    for (i = 0; i < ExtendedDataClass->numOfExtendedDataRecordClassRef; i++) {
      ExtendedDataRecordClassRef = ExtendedDataClass->ExtendedDataRecordClassRef[i];
      data[offset] = ExtendedDataRecordClassRef->ExtendedDataRecordNumber;
      offset += 1u;
      ret =
        Dem_FillExtendedData(DTCRef, ExtendedDataRecordClassRef, &data[offset], memory->DTCOrigin);
      if (ret > 0u) {
        offset += ret;
        ASLOG(DEMI,
              ("DTC %d capture extended data with record number %d on origin %d\n", DTCRef->DtcId,
               ExtendedDataRecordClassRef->ExtendedDataRecordNumber, memory->DTCOrigin));
      } else {
        ASLOG(DEME, ("DTC %d capture extended data with record number %d on origin %d failed\n",
                     DTCRef->DtcId, ExtendedDataRecordClassRef->ExtendedDataRecordNumber,
                     memory->DTCOrigin));
        break;
      }
    }

    if (ret > 0u) {
#ifdef DEM_USE_NVM
      (void)NvM_WriteBlock(memory->ExtendedDataNvmBlockIds[eeId], NULL);
#else
      Dem_SetDirty(memory->ExtendedDataRecordsDirty, eeId);
#endif
    } else {
      Dem_FreeExtendedDataRecord(DTCRef->DtcId, memory);
    }
  } else {
    ASLOG(DEMI, ("DTC %d store extended data failed as no slot\n", DTCRef->DtcId));
  }
}
#endif

static void Dem_UpdateDtcStatus(P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) DTCRef,
                                P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory) {
  uint16_t i;
  Dem_DtcStatusRecordType *StatusRecord = memory->StatusRecords[DTCRef->DtcId];
  Dem_EventStatusRecordType *EventStatus;
  Dem_UdsStatusByteType status = 0;
  Dem_EventIdType EventId;
  /* @SWS_Dem_00441 */
  for (i = 0; i < DTCRef->numOfEvents; i++) {
    EventId = DTCRef->EventIdRefs[i];
    EventStatus = DEM_CONFIG->EventStatusRecords[EventId];
    status |= EventStatus->status;
  }

  if (0u != (DEM_UDS_STATUS_TFSLC & status)) {
    status &= ~DEM_UDS_STATUS_TNCSLC;
  }

  if (0u != (DEM_UDS_STATUS_TFTOC & status)) {
    status &= ~DEM_UDS_STATUS_TNCTOC;
  }

  StatusRecord->status = status;
}

/* Figure 7.27: General diagnostic event storage processing */
static void Dem_DtcUpdateOnFailed(Dem_EventIdType EventId,
                                  P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) DTCRef, int origin) {
  Dem_EventContextType *EventContext = &DEM_CONFIG->EventContexts[EventId];
  const Dem_DTCAttributesType *DTCAttributes = DTCRef->DTCAttributes;
  P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST)
  memory = DTCRef->DTCAttributes->MemoryDestination[origin];
  Dem_DtcStatusRecordType *StatusRecord = memory->StatusRecords[DTCRef->DtcId];
  Dem_UdsStatusByteType oldStatus = StatusRecord->status;
  boolean bDirty = FALSE;
  boolean bCaptureFF = FALSE;
#ifdef DEM_USE_NVM_EXTENDED_DATA
  boolean bCaptureEE = FALSE;
#endif
  Dem_UpdateDtcStatus(DTCRef, memory);

  StatusRecord->agingCounter = 0;

  if (0u == (oldStatus & DEM_UDS_STATUS_TF)) {
    /* @SWS_Dem_00524 */
    if (DEM_PROCESS_OCCCTR_TF == DTCAttributes->OccurrenceCounterProcessing) {
      if (StatusRecord->faultOccuranceCounter < UINT8_MAX) {
        StatusRecord->faultOccuranceCounter++;
        bDirty = TRUE;
      }
    }

    if (DEM_TRIGGER_ON_TEST_FAILED == DTCAttributes->FreezeFrameRecordTrigger) {
      /* @SWS_Dem_00800 */
      bCaptureFF = TRUE;
    }
#ifdef DEM_USE_NVM_EXTENDED_DATA
    if (DEM_TRIGGER_ON_TEST_FAILED == DTCAttributes->ExtendedDataRecordTrigger) {
      /* @SWS_Dem_00812 */
      bCaptureEE = TRUE;
    }
#endif
#ifdef DEM_USE_CYCLES_SINCE_LAST_FAILED /* @SWS_Dem_00771 @SWS_Dem_00772 */
    StatusRecord->cyclesSinceLastFailed = 0;
#endif
  }

  if (0u == (oldStatus & DEM_UDS_STATUS_TFTOC)) {
    if (DEM_PROCESS_OCCCTR_TFTOC == DTCAttributes->OccurrenceCounterProcessing) {
      if (StatusRecord->faultOccuranceCounter < UINT8_MAX) {
        StatusRecord->faultOccuranceCounter++;
        bDirty = TRUE;
      }
    }
  }

  if ((0u == (oldStatus & DEM_UDS_STATUS_PDTC)) &&
      (0u != (StatusRecord->status & DEM_UDS_STATUS_PDTC))) {
    /* @SWS_Dem_00801 */
    if (DEM_TRIGGER_ON_PENDING == DTCAttributes->FreezeFrameRecordTrigger) {
      bCaptureFF = TRUE;
    }
#ifdef DEM_USE_NVM_EXTENDED_DATA
    if (DEM_TRIGGER_ON_PENDING == DTCAttributes->ExtendedDataRecordTrigger) {
      /* @SWS_Dem_00813 */
      bCaptureEE = TRUE;
    }
#endif
  }

  if ((0u == (oldStatus & DEM_UDS_STATUS_CDTC)) &&
      (0u != (StatusRecord->status & DEM_UDS_STATUS_CDTC))) {
    /* @SWS_Dem_00580 */
    if (DEM_PROCESS_OCCCTR_CDTC == DTCAttributes->OccurrenceCounterProcessing) {
      if (StatusRecord->faultOccuranceCounter < UINT8_MAX) {
        StatusRecord->faultOccuranceCounter++;
        bDirty = TRUE;
      }
    }

    if (DEM_TRIGGER_ON_CONFIRMED == DTCAttributes->FreezeFrameRecordTrigger) {
      /* @SWS_Dem_00802 */
      bCaptureFF = TRUE;
    }
#ifdef DEM_USE_NVM_EXTENDED_DATA
    if (DEM_TRIGGER_ON_CONFIRMED == DTCAttributes->ExtendedDataRecordTrigger) {
      /* @SWS_Dem_00814 */
      bCaptureEE = TRUE;
    }
#endif
  }

  if (DEM_TRIGGER_ON_EVERY_TEST_FAILED == DTCAttributes->FreezeFrameRecordTrigger) {
    /* @SWS_Dem_01308 */
    bCaptureFF = TRUE;
  }

  if ((oldStatus & DEM_NVM_STORE_CARED_BITS) != (StatusRecord->status & DEM_NVM_STORE_CARED_BITS)) {
    bDirty = TRUE;
  }

  if (TRUE == bCaptureFF) {
    if (DEM_CAPTURE_SYNCHRONOUS_TO_REPORTING == DTCRef->DTCAttributes->EnvironmentDataCapture) {
      Dem_TrigerStoreFreezeFrame(DTCRef, memory); /* @SWS_Dem_00805 */
    } else {
      Std_FlagSet(EventContext->flag, DEM_EVENT_FLAG_CAPTURE_FF << origin);
    }
  }

#ifdef DEM_USE_NVM_EXTENDED_DATA
  if (TRUE == bCaptureEE) {
    if (DEM_CAPTURE_SYNCHRONOUS_TO_REPORTING == DTCRef->DTCAttributes->EnvironmentDataCapture) {
      Dem_TrigerStoreExtendedData(DTCRef, memory); /* @SWS_Dem_01081 */
    } else {
      Std_FlagSet(EventContext->flag, DEM_EVENT_FLAG_CAPTURE_EE << origin);
    }
  }
#endif

  if (TRUE == bDirty) {
#ifdef DEM_USE_NVM
    (void)NvM_WriteBlock(memory->StatusNvmBlockIds[DTCRef->DtcId], NULL);
#else
    Dem_SetDirty(memory->StatusRecordsDirty, DTCRef->DtcId);
#endif
  }
}

static void Dem_EventUpdateOnFailed(Dem_EventIdType EventId) {
  P2CONST(Dem_EventConfigType, AUTOMATIC, DEM_CONST)
  EventConfig = &DEM_CONFIG->EventConfigs[EventId];
  Dem_EventStatusRecordType *EventStatus = DEM_CONFIG->EventStatusRecords[EventId];
  Dem_UdsStatusByteType oldStatus = EventStatus->status;
  P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) DTCRef = EventConfig->DTCRef;
  boolean bUpdated = FALSE;
  uint16_t i;

  ASLOG(DEMI, ("Event %d Test Failed\n", EventId));

  Dem_UdsBitClear(EventStatus->status, DEM_UDS_STATUS_TNCTOC | DEM_UDS_STATUS_TNCSLC);

  Dem_UdsBitSet(EventStatus->status, DEM_UDS_STATUS_TF | DEM_UDS_STATUS_TFTOC |
                                       DEM_UDS_STATUS_PDTC | DEM_UDS_STATUS_TFSLC);

  if (EventStatus->testFailedCounter < UINT8_MAX) {
    EventStatus->testFailedCounter++;
    bUpdated = TRUE;
  }

  if (EventStatus->testFailedCounter >= EventConfig->ConfirmationThreshold) {
    ASLOG(DEMI, ("Event %d confirmed\n", EventId));
    Dem_UdsBitSet(EventStatus->status, DEM_UDS_STATUS_CDTC);
  }

  if ((oldStatus & DEM_NVM_STORE_CARED_BITS) != (EventStatus->status & DEM_NVM_STORE_CARED_BITS)) {
    bUpdated = TRUE;
  }

  if (TRUE == bUpdated) {
#ifdef DEM_USE_NVM
    (void)NvM_WriteBlock(DEM_CONFIG->EventStatusNvmBlockIds[EventId], NULL);
#else
    Dem_SetDirty(DEM_CONFIG->EventStatusDirty, EventId);
#endif
  }

  if (NULL != DTCRef) {
    /* @SWS_Dem_01199 @SWS_Dem_01207 */
    for (i = 0; i < DTCRef->DTCAttributes->numOfMemoryDestination; i++) {
      /* @SWS_Dem_01063
       * Note: The mirror memory may have a different behaviour which is project
       * specific and is not described in this document.*/
      Dem_DtcUpdateOnFailed(EventId, DTCRef, i);
    }
  }
}

static void Dem_DtcUpdateOnPass(Dem_EventIdType EventId,
                                P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) DTCRef,
                                P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory) {
  Dem_DtcStatusRecordType *StatusRecord = memory->StatusRecords[DTCRef->DtcId];
  Dem_UdsStatusByteType oldStatus = StatusRecord->status;
  boolean bUpdated = FALSE;
  (void)EventId;
  Dem_UpdateDtcStatus(DTCRef, memory);

  if ((oldStatus & DEM_NVM_STORE_CARED_BITS) != (StatusRecord->status & DEM_NVM_STORE_CARED_BITS)) {
    bUpdated = TRUE;
  }

  if (TRUE == bUpdated) {
#ifdef DEM_USE_NVM
    (void)NvM_WriteBlock(memory->StatusNvmBlockIds[DTCRef->DtcId], NULL);
#else
    Dem_SetDirty(memory->StatusRecordsDirty, DTCRef->DtcId);
#endif
  }
}

static void Dem_EventUpdateOnPass(Dem_EventIdType EventId) {
  P2CONST(Dem_EventConfigType, AUTOMATIC, DEM_CONST)
  EventConfig = &DEM_CONFIG->EventConfigs[EventId];
  Dem_EventStatusRecordType *EventStatus = DEM_CONFIG->EventStatusRecords[EventId];
  Dem_EventContextType *EventContext = &DEM_CONFIG->EventContexts[EventId];
  P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) DTCRef = EventConfig->DTCRef;
  Dem_UdsStatusByteType oldStatus = EventStatus->status;
  boolean bPass = TRUE;
  uint16_t i;

  ASLOG(DEMI, ("Event %d Test Passed\n", EventId));

  Dem_UdsBitClear(EventStatus->status,
                  DEM_UDS_STATUS_TNCTOC | DEM_UDS_STATUS_TNCSLC | DEM_UDS_STATUS_TF);

  if ((oldStatus & DEM_NVM_STORE_CARED_BITS) != (EventStatus->status & DEM_NVM_STORE_CARED_BITS)) {
#ifdef DEM_USE_NVM
    (void)NvM_WriteBlock(DEM_CONFIG->EventStatusNvmBlockIds[EventId], NULL);
#else
    Dem_SetDirty(DEM_CONFIG->EventStatusDirty, EventId);
#endif
  }

  if (NULL != DTCRef) {
    for (i = 0; i < DTCRef->numOfEvents; i++) {
      EventContext = &DEM_CONFIG->EventContexts[DTCRef->EventIdRefs[i]];
      if (DEM_EVENT_STATUS_PASSED != EventContext->status) {
        bPass = FALSE;
      }
    }
    if (TRUE == bPass) {
      for (i = 0; i < DTCRef->DTCAttributes->numOfMemoryDestination; i++) {
        Dem_DtcUpdateOnPass(EventId, DTCRef, DTCRef->DTCAttributes->MemoryDestination[i]);
      }
    }
  }
}

static boolean Dem_IsFilteredDTC(P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) Dtc,
                                 P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory) {
  boolean r = TRUE;

  /* Values: 0x00: Autosar-specific value to deactivate the status-byte filtering (different meaning
   * than in ISO 14229-1) to report all supported DTCs (used for service 0x19 subfunctions
   * 0x0A/0x15) */
  if ((0u != Dem_Context.filter.statusMask) &&
      (0u == (Dem_Context.filter.statusMask & memory->StatusRecords[Dtc->DtcId]->status))) {
    r = FALSE;
  }

  return r;
}

static boolean Dem_IsFilteredFF(int index) {
  /* only UDS FF are supported */
  (void)index;
  return TRUE;
}

static P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) Dem_LookupDtcByDTCNumber(uint32_t DTCNumber) {
  uint16_t i;
  P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) Dtc = NULL;

  for (i = 0; i < DEM_CONFIG->numOfDtcs; i++) {
    if (DEM_CONFIG->Dtcs[i].DtcNumber == DTCNumber) {
      Dtc = &DEM_CONFIG->Dtcs[i];
      break;
    }
  }

  return Dtc;
}

static Std_ReturnType Dem_IsDtcInGroup(Dem_DtcIdType DtcId, uint32_t groupDTC) {
  Std_ReturnType r = E_NOT_OK;
  P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) Dtc;
  if (DtcId < DEM_CONFIG->numOfDtcs) {
    Dtc = &DEM_CONFIG->Dtcs[DtcId];
    if (0xFFFFFFu == groupDTC) {
      r = E_OK;
    } else if (Dtc->DtcNumber == groupDTC) {
      r = E_OK;
    } else {
      /* not in the group */
    }
  } else if (DtcId != DEM_INVALID_EVENT_ID) {
    /* note: as this API is only used by ClearDTC, so if DtcId invalid, just clear it */
    ASLOG(DEME, ("Found invalid DTC with id %x\n", DtcId));
    r = E_OK;
  } else {
    /* do nothing */
  }
  return r;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Dem_PreInit(void) {
  /* NOTE: BSW Event is not supported, so this API is dummy */
}

void Dem_Init(const Dem_ConfigType *ConfigPtr) {
  uint16_t i;
#if !defined(DEM_USE_NVM) || AS_LOG_DEMI > 0
  P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory;
#endif
#if AS_LOG_DEMI > 0
  int j, k;
  P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) Dtc;
#endif

#ifdef DEM_USE_PB_CONFIG
  if (NULL != ConfigPtr) {
    DEM_CONFIG = ConfigPtr;
  } else {
    DEM_CONFIG = &Dem_Config;
  }
#else
  (void)ConfigPtr;
#endif

  for (i = 0; i < DEM_CONFIG->numOfOperationCycles; i++) {
    DEM_CONFIG->OperationCycleStates[i] = DEM_OPERATION_CYCLE_STOPPED;
  }

  for (i = 0; i < DEM_CONFIG->numOfEvents; i++) {
    Dem_EventInit((Dem_EventIdType)i);
  }
#ifndef DEM_USE_NVM
  (void)memset(DEM_CINFIG->EventStatusDirty, 0, (DEM_CONFIG->numOfEvents + 7) / 8);
#endif
#if AS_LOG_DEMI > 0
  for (i = 0; i < DEM_CONFIG->numOfDtcs; i++) {
    Dtc = &DEM_CONFIG->Dtcs[i];
    for (j = 0; j < Dtc->DTCAttributes->numOfMemoryDestination; j++) {
      memory = Dtc->DTCAttributes->MemoryDestination[j];
      ASLOG(DEMI,
            ("Origin %d DTC %d status=%02X, agingCounter=%d, agedCounter=%d occuranceCounter=%d\n",
             memory->DTCOrigin, i, memory->StatusRecords[Dtc->DtcId]->status,
             memory->StatusRecords[Dtc->DtcId]->agingCounter,
             memory->StatusRecords[Dtc->DtcId]->agedCounter,
             memory->StatusRecords[Dtc->DtcId]->faultOccuranceCounter));
    }
  }
#endif

  for (i = 0; i < DEM_CONFIG->numOfMemoryDestination; i++) {
#if !defined(DEM_USE_NVM) || AS_LOG_DEMI > 0
    memory = &DEM_CONFIG->MemoryDestination[i];
#endif
#if AS_LOG_DEMI > 0
    for (j = 0; j < memory->numOfFreezeFrameRecords; j++) {
      if (memory->FreezeFrameRecords[j]->DtcId != DEM_INVALID_DTC_ID) {
        for (k = 0; k < DEM_MAX_FREEZE_FRAME_NUMBER; k++) {
          if (DEM_FREEZE_FRAME_SLOT_FREE != memory->FreezeFrameRecords[j]->FreezeFrameData[k][0]) {
            ASLOG(DEMI, ("Origin %d DTC %d has freeze frame at slot %d with record number %d\n",
                         memory->DTCOrigin, memory->FreezeFrameRecords[j]->DtcId, k,
                         memory->FreezeFrameRecords[j]->FreezeFrameData[k][0]));
          }
        }
      }
    }
#endif
#ifndef DEM_USE_NVM
    (void)memset(memory->StatusRecordsDirty, 0, (memory->numOfStatusRecords + 7) / 8);
    (void)memset(memory->FreezeFrameRecordsDirty, 0, (memory->numOfFreezeFrameRecords + 7) / 8);
#ifdef DEM_USE_NVM_EXTENDED_DATA
    (void)memset(memory->ExtendedDataRecordsDirty, 0, (memory->numOfExtendedDataRecords + 7) / 8);
#endif
#endif
  }

  (void)memset(&Dem_Context, 0, sizeof(Dem_Context));
  Dem_Context.disableDtcSetting = FALSE;
}

Std_ReturnType Dem_EXTD_GetFaultOccuranceCounter(Dem_DtcIdType DtcId, uint8_t *data,
                                                 Dem_DTCOriginType DTCOrigin) {
  Std_ReturnType r = E_OK;
  P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory;

  memory = Dem_LookupMemory(DTCOrigin);
  if (NULL != memory) {
    if (DtcId < memory->numOfStatusRecords) {
      data[0] = memory->StatusRecords[DtcId]->faultOccuranceCounter;
    } else {
      r = E_NOT_OK;
    }
  } else {
    r = DEM_WRONG_DTCORIGIN;
  }

  return r;
}

Std_ReturnType Dem_EXTD_GetAgingCounter(Dem_DtcIdType DtcId, uint8_t *data,
                                        Dem_DTCOriginType DTCOrigin) {
  Std_ReturnType r = E_OK;
  P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory;

  memory = Dem_LookupMemory(DTCOrigin);
  if (NULL != memory) {
    if (DtcId < memory->numOfStatusRecords) {
      data[0] = memory->StatusRecords[DtcId]->agingCounter;
    } else {
      r = E_NOT_OK;
    }
  } else {
    r = DEM_WRONG_DTCORIGIN;
  }

  return r;
}

Std_ReturnType Dem_EXTD_GetAgedCounter(Dem_DtcIdType DtcId, uint8_t *data,
                                       Dem_DTCOriginType DTCOrigin) {
  Std_ReturnType r = E_OK;
  P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory;

  memory = Dem_LookupMemory(DTCOrigin);
  if (NULL != memory) {
    if (DtcId < memory->numOfStatusRecords) {
      data[0] = memory->StatusRecords[DtcId]->agedCounter;
    } else {
      r = E_NOT_OK;
    }
  } else {
    r = DEM_WRONG_DTCORIGIN;
  }

  return r;
}

Std_ReturnType Dem_SetOperationCycleState(uint8_t OperationCycleId,
                                          Dem_OperationCycleStateType cycleState) {
  Std_ReturnType r = E_OK;

  DET_VALIDATE(NULL != DEM_CONFIG, 0xF1, DEM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(OperationCycleId < DEM_CONFIG->numOfOperationCycles, 0xF1, DEM_E_PARAM_DATA,
               return E_NOT_OK);

  if (cycleState != DEM_CONFIG->OperationCycleStates[OperationCycleId]) {
    if (DEM_OPERATION_CYCLE_STARTED == cycleState) {
      Dem_StartOperationCycle(OperationCycleId);
    } else {
      Dem_StopOperationCycle(OperationCycleId);
    }
    DEM_CONFIG->OperationCycleStates[OperationCycleId] = cycleState;
  }

  return r;
}

Std_ReturnType Dem_RestartOperationCycle(uint8_t OperationCycleId) {
  Std_ReturnType r = E_OK;

  DET_VALIDATE(NULL != DEM_CONFIG, 0x08, DEM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(NULL != DEM_CONFIG, 0x08, DEM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(OperationCycleId < DEM_CONFIG->numOfOperationCycles, 0x08, DEM_E_PARAM_DATA,
               return E_NOT_OK);

  if (DEM_OPERATION_CYCLE_STARTED == DEM_CONFIG->OperationCycleStates[OperationCycleId]) {
    Dem_StopOperationCycle(OperationCycleId);
    Dem_StartOperationCycle(OperationCycleId);
  } else {
    r = E_NOT_OK;
  }

  return r;
}

Std_ReturnType Dem_SetEventStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus) {
  Std_ReturnType r = E_OK;
  P2CONST(Dem_EventConfigType, AUTOMATIC, DEM_CONST) EventConfig;
  Dem_EventContextType *EventContext;
  Dem_EventStatusType oldStatus;

  DET_VALIDATE(NULL != DEM_CONFIG, 0x04, DEM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(EventId < DEM_CONFIG->numOfEvents, 0x04, DEM_E_PARAM_DATA, return E_NOT_OK);
  DET_VALIDATE(EventStatus <= DEM_EVENT_STATUS_PREFAILED, 0x04, DEM_E_PARAM_DATA, return E_NOT_OK);

  EventConfig = &DEM_CONFIG->EventConfigs[EventId];
  EventContext = &DEM_CONFIG->EventContexts[EventId];
  r = Dem_IsOperationCycleStarted(EventConfig->OperationCycleRef);

  if (E_OK == r) {
    if (TRUE == Dem_Context.disableDtcSetting) {
      r = E_NOT_OK;
    }
  }

#ifdef DEM_USE_ENABLE_CONDITION
  if (E_OK == r) {
    /* @SWS_Dem_00449 */
    r = Dem_IsEventConditionEnabled(EventConfig);
  }
#endif

  if (E_OK == r) {
    ASLOG(DEMI, ("Set Event %d status %d\n", EventId, EventStatus));
    oldStatus = EventContext->status;
    r = Dem_SetEventStatusDebounce(EventId, EventStatus);
  }

  if (E_OK == r) {
    if (oldStatus != EventContext->status) {
      if (DEM_EVENT_STATUS_FAILED == EventContext->status) {
        Dem_EventUpdateOnFailed(EventId);
      } else {
        Dem_EventUpdateOnPass(EventId);
      }
    }
  }
  return r;
}

Std_ReturnType Dem_GetEventUdsStatus(Dem_EventIdType EventId,
                                     Dem_UdsStatusByteType *UDSStatusByte) {
  Std_ReturnType r = E_OK;
  Dem_EventStatusRecordType *EventStatus;

  DET_VALIDATE(NULL != DEM_CONFIG, 0xB6, DEM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(EventId < DEM_CONFIG->numOfEvents, 0xB6, DEM_E_PARAM_DATA, return E_NOT_OK);
  DET_VALIDATE(NULL != UDSStatusByte, 0xB6, DEM_E_PARAM_POINTER, return E_NOT_OK);

  /* @SWS_Dem_00006 @SWS_Dem_01276 */
  EventStatus = DEM_CONFIG->EventStatusRecords[EventId];
  *UDSStatusByte = EventStatus->status;

  return r;
}

Std_ReturnType Dem_DisableDTCSetting(uint8_t ClientId) {
  Std_ReturnType r = E_OK;

  (void)ClientId; /* not used */
  ASLOG(DEMI, ("disable DTC setting\n"));

  Dem_Context.disableDtcSetting = TRUE;

  return r;
}

Std_ReturnType Dem_EnableDTCSetting(uint8_t ClientId) {
  Std_ReturnType r = E_OK;
  P2CONST(Dem_EventConfigType, AUTOMATIC, DEM_CONST) EventConfig;
  uint16_t i;

  (void)ClientId; /* not used */
  ASLOG(DEMI, ("enable DTC setting\n"));
  DET_VALIDATE(NULL != DEM_CONFIG, 0x25, DEM_E_UNINIT, return E_NOT_OK);
  Dem_Context.disableDtcSetting = FALSE;

  for (i = 0; i < DEM_CONFIG->numOfEvents; i++) {
    EventConfig = &DEM_CONFIG->EventConfigs[i];
    if (NULL != EventConfig->DemCallbackInitMForE) {
      /* @SWS_Dem_00682 */
      EventConfig->DemCallbackInitMForE(DEM_INIT_MONITOR_REENABLED);
    }
  }

  return r;
}

Std_ReturnType Dem_ClearDTC(uint8_t ClientId) {
  Std_ReturnType r = E_OK;
  P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) Dtc = NULL;
  uint16_t i;
  uint16_t j;
  Dem_EventIdType EventId;
  uint32_t groupDTC = Dem_Context.filter.DTCNumber;
  Dem_EventContextType *EventContext;
  P2CONST(Dem_EventConfigType, AUTOMATIC, DEM_CONST) EventConfig;
  Dem_EventStatusRecordType *EventStatus;
  P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory;
  Dem_DtcStatusRecordType *StatusRecord;
  (void)ClientId; /* not used */

  DET_VALIDATE(NULL != DEM_CONFIG, 0x23, DEM_E_UNINIT, return E_NOT_OK);

  if (0xFFFFFFu == groupDTC) {
    /* clear all DTC */
  } else {
    Dtc = Dem_LookupDtcByDTCNumber(groupDTC);
    if (NULL != Dtc) {
      /* clear specific DTC */
    } else {
      r = DEM_WRONG_DTC;
    }
  }

  if (E_OK == r) {
    memory = Dem_LookupMemory(Dem_Context.filter.DTCOrigin);
    if (NULL == memory) {
      r = DEM_WRONG_DTCORIGIN;
    }
  }

  if (E_OK == r) {
    /* NOTE: 0xFFu is initial value of NVM, means free */
    for (i = 0; i < memory->numOfFreezeFrameRecords; i++) {
      r = Dem_IsDtcInGroup(memory->FreezeFrameRecords[i]->DtcId, groupDTC);
      if (E_OK == r) {
        (void)memset(memory->FreezeFrameRecords[i], DEM_FREEZE_FRAME_SLOT_FREE,
                     sizeof(Dem_FreezeFrameRecordType));
#ifdef DEM_USE_NVM
        (void)NvM_WriteBlock(memory->FreezeFrameNvmBlockIds[i], NULL);
#else
        Dem_SetDirty(memory->FreezeFrameRecordsDirty, i);
#endif
      }
      r = E_OK;
    }

#ifdef DEM_USE_NVM_EXTENDED_DATA
    for (i = 0; i < memory->numOfExtendedDataRecords; i++) {
      r = Dem_IsDtcInGroup(memory->ExtendedDataRecords[i]->DtcId, groupDTC);
      if (E_OK == r) {
        (void)memset(memory->ExtendedDataRecords[i], DEM_EXTENDED_DATA_SLOT_FREE,
                     sizeof(Dem_ExtendedDataRecordType));
#ifdef DEM_USE_NVM
        (void)NvM_WriteBlock(memory->ExtendedDataNvmBlockIds[i], NULL);
#else
        Dem_SetDirty(memory->ExtendedDataRecordsDirty, i);
#endif
      }
      r = E_OK;
    }
#endif
  }

  if (E_OK == r) {
    for (i = 0; i < DEM_CONFIG->numOfDtcs; i++) {
      Dtc = &DEM_CONFIG->Dtcs[i];
      r = Dem_IsDtcInGroup(Dtc->DtcId, groupDTC);
      if (E_OK == r) {
        StatusRecord = memory->StatusRecords[Dtc->DtcId];
        if (StatusRecord->status != (DEM_UDS_STATUS_TNCTOC | DEM_UDS_STATUS_TNCSLC)) {
          (void)memset(StatusRecord, 0x00, sizeof(Dem_DtcStatusRecordType));
          /* @SWS_Dem_00394 @SWS_Dem_00392 */
          StatusRecord->status = DEM_UDS_STATUS_TNCTOC | DEM_UDS_STATUS_TNCSLC;
#ifdef DEM_USE_CYCLES_SINCE_LAST_FAILED
          StatusRecord->cyclesSinceLastFailed = DEM_CYCLE_COUNTER_STOPPED;
#endif
#ifdef DEM_USE_NVM
          (void)NvM_WriteBlock(memory->StatusNvmBlockIds[Dtc->DtcId], NULL);
#else
          Dem_SetDirty(memory->StatusRecordsDirty, Dtc->DtcId);
#endif
        }

        for (j = 0; j < Dtc->numOfEvents; j++) {
          EventId = Dtc->EventIdRefs[j];
          EventContext = &DEM_CONFIG->EventContexts[EventId];
          EventConfig = &DEM_CONFIG->EventConfigs[EventId];
          EventStatus = DEM_CONFIG->EventStatusRecords[EventId];
          (void)memset(EventContext, 0, sizeof(Dem_EventContextType));
          if ((EventStatus->status != (DEM_UDS_STATUS_TNCTOC | DEM_UDS_STATUS_TNCSLC)) ||
              (EventStatus->testFailedCounter != 0u)) {
            EventStatus->status = DEM_UDS_STATUS_TNCTOC | DEM_UDS_STATUS_TNCSLC;
            EventStatus->testFailedCounter = 0;
#ifdef DEM_USE_NVM
            (void)NvM_WriteBlock(DEM_CONFIG->EventStatusNvmBlockIds[EventId], NULL);
#else
            Dem_SetDirty(DEM_CONFIG->EventStatusDirty, EventId);
#endif
          }
          if (NULL != EventConfig->DemCallbackInitMForE) {
            /* @ SWS_Dem_00680 */
            EventConfig->DemCallbackInitMForE(DEM_INIT_MONITOR_CLEAR);
          }
        }
      }
      r = E_OK;
    }
  }

  return r;
}

Std_ReturnType Dem_GetDTCStatusAvailabilityMask(uint8_t ClientId,
                                                Dem_UdsStatusByteType *DTCStatusMask,
                                                Dem_DTCOriginType DTCOrigin) {
  (void)ClientId;
  (void)DTCOrigin;
  DET_VALIDATE(NULL != DEM_CONFIG, 0x16, DEM_E_UNINIT, return E_NOT_OK);
  /* StatusAvailabilityMask: except WIR, all supported */
  *DTCStatusMask = DEM_CONFIG->StatusAvailabilityMask;
  return E_OK;
}

Std_ReturnType Dem_SetDTCFilter(uint8_t ClientId, uint8_t DTCStatusMask,
                                Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin,
                                boolean FilterWithSeverity, Dem_DTCSeverityType DTCSeverityMask,
                                boolean FilterForFaultDetectionCounter) {
  Std_ReturnType r = E_OK;
  (void)ClientId;
  (void)DTCFormat;
  (void)FilterWithSeverity;
  (void)DTCSeverityMask;
  (void)FilterForFaultDetectionCounter;

  DET_VALIDATE(NULL != DEM_CONFIG, 0x13, DEM_E_UNINIT, return E_NOT_OK);
  Dem_Context.filter.statusMask = DTCStatusMask & DEM_CONFIG->StatusAvailabilityMask;
  Dem_Context.filter.index = 0;
  Dem_Context.filter.DTCOrigin = DTCOrigin;

  return r;
}

Std_ReturnType Dem_GetNumberOfFilteredDTC(uint8_t ClientId, uint16_t *NumberOfFilteredDTC) {
  Std_ReturnType r = E_OK;
  boolean IsFiltered;
  uint16_t i;
  uint16_t number = 0;
  P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory;

  DET_VALIDATE(NULL != DEM_CONFIG, 0x17, DEM_E_UNINIT, return E_NOT_OK);

  (void)ClientId;
  memory = Dem_LookupMemory(Dem_Context.filter.DTCOrigin);
  if (NULL != memory) {
    for (i = 0; i < DEM_CONFIG->numOfDtcs; i++) {
      IsFiltered = Dem_IsFilteredDTC(&DEM_CONFIG->Dtcs[i], memory);
      if (TRUE == IsFiltered) {
        number++;
      }
    }
    ASLOG(DEMI, ("NumberOfFilteredDTC = %d\n", number));
  } else {
    r = DEM_WRONG_DTCORIGIN;
  }

  *NumberOfFilteredDTC = number;

  return r;
}

Std_ReturnType Dem_GetNextFilteredDTC(uint8_t ClientId, uint32_t *DTC, uint8_t *DTCStatus) {
  Std_ReturnType r = E_NOT_OK;
  boolean IsFiltered;
  uint16_t i;
  P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) Dtc;

  P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory;

  DET_VALIDATE(NULL != DEM_CONFIG, 0x18, DEM_E_UNINIT, return E_NOT_OK);
  (void)ClientId;
  memory = Dem_LookupMemory(Dem_Context.filter.DTCOrigin);
  if (NULL != memory) {
    for (i = Dem_Context.filter.index; i < DEM_CONFIG->numOfDtcs; i++) {
      Dtc = &DEM_CONFIG->Dtcs[i];
      IsFiltered = Dem_IsFilteredDTC(Dtc, memory);
      if (TRUE == IsFiltered) {
        Dtc = &DEM_CONFIG->Dtcs[i];
        *DTC = Dtc->DtcNumber;
        *DTCStatus = memory->StatusRecords[Dtc->DtcId]->status & DEM_CONFIG->StatusAvailabilityMask;
        Dem_Context.filter.index = i + 1u;
        ASLOG(DEMI, ("Get DTC %d: %06X %02X\n", i, *DTC, *DTCStatus));
        r = E_OK;
        break;
      }
    }
  } else {
    r = DEM_WRONG_DTCORIGIN;
  }
  return r;
}

Std_ReturnType Dem_SetFreezeFrameRecordFilter(uint8_t ClientId, Dem_DTCFormatType DTCFormat) {
  Std_ReturnType r = E_OK;
  (void)ClientId;
  (void)DTCFormat;
  Dem_Context.filter.index = 0;

  return r;
}

Std_ReturnType Dem_GetNumberOfFreezeFrameRecords(uint8_t ClientId,
                                                 uint16_t *NumberOfFilteredRecords) {
  Std_ReturnType r = E_OK;
  uint16_t i;
  uint16_t slot;
  uint16_t number = 0;
  Dem_FreezeFrameRecordType *record;
  P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory;
  (void)ClientId;
  DET_VALIDATE(NULL != DEM_CONFIG, 0x5a, DEM_E_UNINIT, return E_NOT_OK);
  memory = Dem_LookupMemory(Dem_Context.filter.DTCOrigin);
  if (NULL != memory) {
    for (i = 0; i < memory->numOfFreezeFrameRecords; i++) {
      if (DEM_INVALID_DTC_ID != memory->FreezeFrameRecords[i]->DtcId) {
        record = memory->FreezeFrameRecords[i];
        for (slot = 0; slot < DEM_MAX_FREEZE_FRAME_NUMBER; slot++) {
          if (DEM_FREEZE_FRAME_SLOT_FREE != record->FreezeFrameData[slot][0]) {
            number++;
          }
        }
      }
    }
  } else {
    r = DEM_WRONG_DTCORIGIN;
  }

  *NumberOfFilteredRecords = number;

  return r;
}

Std_ReturnType Dem_GetNextFilteredRecord(uint8_t ClientId, uint32_t *DTC, uint8_t *RecordNumber) {
  Std_ReturnType r = E_NOT_OK;
  boolean IsFiltered;
  uint16_t i;
  P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) DtcRef;
  Dem_FreezeFrameRecordType *record;
  uint8_t recNum;
  P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory;
  (void)ClientId;
  DET_VALIDATE(NULL != DEM_CONFIG, 0x3a, DEM_E_UNINIT, return E_NOT_OK);
  memory = Dem_LookupMemory(Dem_Context.filter.DTCOrigin);
  if (NULL != memory) {
    *RecordNumber = 0;
    for (i = Dem_Context.filter.index;
         i < (memory->numOfFreezeFrameRecords * DEM_MAX_FREEZE_FRAME_NUMBER); i++) {
      IsFiltered = Dem_IsFilteredFF(i);
      record = Dem_LookupFreezeFrameRecordByIndex(memory, i, &recNum, NULL);
      if (IsFiltered && (NULL != record)) {
        DtcRef = &DEM_CONFIG->Dtcs[record->DtcId];
        *DTC = DtcRef->DtcNumber;
        *RecordNumber = recNum;

        Dem_Context.filter.index = i + 1u;
        r = E_OK;
        break;
      }
    }
  } else {
    r = DEM_WRONG_DTCORIGIN;
  }

  return r;
}

Std_ReturnType Dem_SelectDTC(uint8_t ClientId, uint32_t DTC, Dem_DTCFormatType DTCFormat,
                             Dem_DTCOriginType DTCOrigin) {
  Std_ReturnType r = E_OK;
  (void)ClientId;
  (void)DTCFormat;
  Dem_Context.filter.DTCNumber = DTC;
  Dem_Context.filter.DTCOrigin = DTCOrigin;

  return r;
}

Std_ReturnType Dem_SelectFreezeFrameData(uint8_t ClientId, uint8_t RecordNumber) {
  Std_ReturnType r = E_OK;
  (void)ClientId;
  Dem_Context.filter.RecordNumber = RecordNumber;

  return r;
}

Std_ReturnType Dem_SelectExtendedDataRecord(uint8_t ClientId, uint8_t ExtendedDataNumber) {
  Std_ReturnType r = E_OK;
  (void)ClientId;
  Dem_Context.filter.RecordNumber = ExtendedDataNumber;

  return r;
}

Std_ReturnType Dem_GetNextFreezeFrameData(uint8_t ClientId, uint8_t *DestBuffer,
                                          uint16_t *BufSize) {
  Std_ReturnType r = E_OK;
  Dem_FreezeFrameRecordType *record;
  P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory;
  P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) Dtc;
  P2CONST(Dem_FreezeFrameRecordClassType, AUTOMATIC, DEM_CONST) FreezeFrameRecordClass;
  uint8_t recNum;
  uint8_t *data;
  uint16_t i;
  uint16_t index = 0;
  uint16_t offset;
  uint8_t numOfRecords = 0;
  uint16_t freezeFrameSize = 0;
  (void)ClientId;

  DET_VALIDATE(NULL != DEM_CONFIG, 0x1d, DEM_E_UNINIT, return E_NOT_OK);
  Dtc = Dem_LookupDtcByDTCNumber(Dem_Context.filter.DTCNumber);

  if (NULL != Dtc) {
    memory = Dem_LookupMemory(Dem_Context.filter.DTCOrigin);
    if (NULL == memory) {
      r = DEM_WRONG_DTCORIGIN;
    }
  } else {
    r = DEM_WRONG_DTC;
  }

  if (E_OK == r) {
    if (0xFFu == Dem_Context.filter.RecordNumber) {
      /* OK as request any */
    } else {
      r = DEM_NO_SUCH_ELEMENT;
      for (i = 0; i < Dtc->DTCAttributes->FreezeFrameRecNumClass->numOfFreezeFrameRecNums; i++) {
        if (Dem_Context.filter.RecordNumber ==
            Dtc->DTCAttributes->FreezeFrameRecNumClass->FreezeFrameRecNums[i]) {
          r = E_OK;
          break;
        }
      }
    }
  }

  if (E_OK == r) {
    numOfRecords = Dem_GetNumberOfFreezeFrameRecordsByRecordNumber(Dtc->DtcId, memory,
                                                                   Dem_Context.filter.RecordNumber);

    FreezeFrameRecordClass = Dtc->DTCAttributes->FreezeFrameRecordClass;
    freezeFrameSize = Dem_GetFreezeFrameSize(FreezeFrameRecordClass) +
                      (2u * FreezeFrameRecordClass->numOfFreezeFrameData);
    if (((uint32_t)numOfRecords * ((uint32_t)freezeFrameSize + 2u) + 4u) > (uint32_t)(*BufSize)) {
      r = DEM_BUFFER_TOO_SMALL;
    } else {
      /* OK */
    }
  }

  if (E_OK == r) {
    DestBuffer[0] = (uint8_t)((Dtc->DtcNumber >> 16) & 0xFFu);
    DestBuffer[1] = (uint8_t)((Dtc->DtcNumber >> 8) & 0xFFu);
    DestBuffer[2] = (uint8_t)(Dtc->DtcNumber & 0xFFu);
    DestBuffer[3] = memory->StatusRecords[Dtc->DtcId]->status & DEM_CONFIG->StatusAvailabilityMask;
    *BufSize = 4u + ((freezeFrameSize + 2u) * numOfRecords);
    for (i = 0; i < numOfRecords; i++) {
      offset = 4u + ((freezeFrameSize + 2u) * i);
      recNum = Dem_Context.filter.RecordNumber;
      record =
        Dem_LookupFreezeFrameRecordByRecordNumber(Dtc->DtcId, memory, &recNum, &data, &index);
      if (record != NULL) {
        DestBuffer[offset] = recNum;
        DestBuffer[offset + 1u] = FreezeFrameRecordClass->numOfFreezeFrameData;
        Dem_FillSnapshotRecord(&DestBuffer[offset + 2u], data, FreezeFrameRecordClass);
      } else {
        ASLOG(DEME, ("Dem record changed during read\n"));
        r = E_NOT_OK;
      }
    }
  }

  return r;
}

Std_ReturnType Dem_GetNextExtendedDataRecord(uint8_t ClientId, uint8_t *DestBuffer,
                                             uint16_t *BufSize) {
  Std_ReturnType r = E_OK;
  P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) Dtc;
  P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) memory;
  P2CONST(Dem_ExtendedDataClassType, AUTOMATIC, DEM_CONST) ExtendedDataClass;
  P2CONST(Dem_ExtendedDataRecordClassType, AUTOMATIC, DEM_CONST) ExtendedDataRecordClass;
#ifdef DEM_USE_NVM_EXTENDED_DATA
  Dem_ExtendedDataRecordType *record;
  uint16_t sz;
  uint16_t offset = 0;
#endif
  uint16_t length = 4u;
  uint16_t i = 0;
  (void)ClientId;
  DET_VALIDATE(NULL != DEM_CONFIG, 0x20, DEM_E_UNINIT, return E_NOT_OK);
  Dtc = Dem_LookupDtcByDTCNumber(Dem_Context.filter.DTCNumber);

  if (NULL != Dtc) {
    memory = Dem_LookupMemory(Dem_Context.filter.DTCOrigin);
    if (NULL == memory) {
      r = DEM_WRONG_DTCORIGIN;
    }
  } else {
    r = DEM_WRONG_DTC;
  }
#ifdef DEM_USE_NVM_EXTENDED_DATA
  if (E_OK == r) {
    record = Dem_LookupExtendedDataRecord(Dtc->DtcId, memory);
    if (NULL == record) {
      r = DEM_NO_SUCH_ELEMENT;
    }
  }
#endif
  if (E_OK == r) {
    ExtendedDataClass = Dtc->DTCAttributes->ExtendedDataClass;
    for (i = 0; i < ExtendedDataClass->numOfExtendedDataRecordClassRef; i++) {
      ExtendedDataRecordClass = ExtendedDataClass->ExtendedDataRecordClassRef[i];
#ifdef DEM_USE_NVM_EXTENDED_DATA
      if (record->ExtendedData[offset] != DEM_EXTENDED_DATA_SLOT_FREE) {
        if ((0xFFu == Dem_Context.filter.RecordNumber) ||
            (record->ExtendedData[offset] == Dem_Context.filter.RecordNumber)) {
          asAssert(record->ExtendedData[offset] ==
                   ExtendedDataRecordClass->ExtendedDataRecordNumber);
          length += 1u + Dem_GetExtendedDataRecordSize(ExtendedDataRecordClass);
        }
      }
      offset += 1u + Dem_GetExtendedDataRecordSize(ExtendedDataRecordClass);
#else
      if ((0xFFu == Dem_Context.filter.RecordNumber) ||
          (ExtendedDataRecordClass->ExtendedDataRecordNumber == Dem_Context.filter.RecordNumber)) {
        length += 1u + Dem_GetExtendedDataRecordSize(ExtendedDataRecordClass);
      } else {
        r = E_NOT_OK;
      }
#endif
    }
    if (E_OK == r) {
      if ((uint32_t)length > *BufSize) {
        r = DEM_BUFFER_TOO_SMALL;
      }
    }
  }

  if (E_OK == r) {
    DestBuffer[0] = (uint8_t)((Dtc->DtcNumber >> 16) & 0xFFu);
    DestBuffer[1] = (uint8_t)((Dtc->DtcNumber >> 8) & 0xFFu);
    DestBuffer[2] = (uint8_t)(Dtc->DtcNumber & 0xFFu);
    DestBuffer[3] = memory->StatusRecords[Dtc->DtcId]->status & DEM_CONFIG->StatusAvailabilityMask;
    *BufSize = length;
    length = 4u; /* acting as offset to DestBuffer */
#ifdef DEM_USE_NVM_EXTENDED_DATA
    offset = 0;
#endif
    for (i = 0; i < ExtendedDataClass->numOfExtendedDataRecordClassRef; i++) {
      ExtendedDataRecordClass = ExtendedDataClass->ExtendedDataRecordClassRef[i];
#ifdef DEM_USE_NVM_EXTENDED_DATA
      if (record->ExtendedData[offset] != DEM_EXTENDED_DATA_SLOT_FREE) {
        if ((0xFFu == Dem_Context.filter.RecordNumber) ||
            (record->ExtendedData[offset] == Dem_Context.filter.RecordNumber)) {
          sz = 1u + Dem_GetExtendedDataRecordSize(ExtendedDataRecordClass);
          (void)memcpy(&DestBuffer[length], &record->ExtendedData[offset], sz);
          length += sz;
        }
      }
      offset += 1u + Dem_GetExtendedDataRecordSize(ExtendedDataRecordClass);
#else
      if ((0xFFu == Dem_Context.filter.RecordNumber) ||
          (ExtendedDataRecordClass->ExtendedDataRecordNumber == Dem_Context.filter.RecordNumber)) {
        DestBuffer[length] = ExtendedDataRecordClass->ExtendedDataRecordNumber;
        length += 1u;
        length += Dem_FillExtendedData(Dtc, ExtendedDataRecordClass, &DestBuffer[length],
                                       memory->DTCOrigin);
      }
#endif
    }
  }

#ifdef DEM_USE_NVM_EXTENDED_DATA
  if (DEM_NO_SUCH_ELEMENT == r) {
    DestBuffer[0] = (uint8_t)((Dtc->DtcNumber >> 16) & 0xFFu);
    DestBuffer[1] = (uint8_t)((Dtc->DtcNumber >> 8) & 0xFFu);
    DestBuffer[2] = (uint8_t)(Dtc->DtcNumber & 0xFFu);
    DestBuffer[3] = memory->StatusRecords[Dtc->DtcId]->status & DEM_CONFIG->StatusAvailabilityMask;
    *BufSize = 4u;
    r = E_OK;
  }
#endif
  return r;
}

#ifdef DEM_USE_ENABLE_CONDITION
Std_ReturnType Dem_SetEnableCondition(uint8_t EnableConditionID, boolean ConditionFulfilled) {
  Std_ReturnType r = E_OK;
  Std_ReturnType ret;
  P2CONST(Dem_EventConfigType, AUTOMATIC, DEM_CONST) EventConfig;
  uint16_t i;
  DET_VALIDATE(NULL != DEM_CONFIG, 0x39, DEM_E_UNINIT, return E_NOT_OK);
  if (EnableConditionID < DEM_NUM_OF_ENABLE_CONDITION) {
    if (TRUE == ConditionFulfilled) {
      Dem_Context.conditionMask |= (1 << EnableConditionID);
      for (i = 0; i < DEM_CONFIG->numOfEvents; i++) {
        EventConfig = &DEM_CONFIG->EventConfigs[i];
        if (NULL != EventConfig->DemCallbackInitMForE) {
          /* @SWS_Dem_00681 */
          ret = Dem_IsEventConditionEnabled(EventConfig);
          if (E_OK == ret) {
            EventConfig->DemCallbackInitMForE(DEM_INIT_MONITOR_REENABLED);
          }
        }
      }
    } else {
      Dem_Context.conditionMask &= ~(1 << EnableConditionID);
    }
  } else {
    r = E_NOT_OK;
  }
  return r;
}
#endif

void Dem_MainFunction(void) {
  uint16_t i;
  uint16_t j;
  P2CONST(Dem_EventConfigType, AUTOMATIC, DEM_CONST) EventConfig;
  Dem_EventContextType *EventContext;
  P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) DTCRef;

  DET_VALIDATE(NULL != DEM_CONFIG, 0x55, DEM_E_UNINIT, return);
  for (i = 0; i < DEM_CONFIG->numOfEvents; i++) {
    EventContext = &DEM_CONFIG->EventContexts[i];
    EventConfig = &DEM_CONFIG->EventConfigs[i];
    DTCRef = EventConfig->DTCRef;
    if (NULL != DTCRef) {
      for (j = 0; j < DTCRef->DTCAttributes->numOfMemoryDestination; j++) {
        if (Std_IsFlagSet(EventContext->flag, DEM_EVENT_FLAG_CAPTURE_FF << j)) {
          /* @SWS_Dem_00461 */
          Dem_TrigerStoreFreezeFrame(DTCRef, DTCRef->DTCAttributes->MemoryDestination[j]);
          Std_FlagClear(EventContext->flag, DEM_EVENT_FLAG_CAPTURE_FF << j);
        }
#ifdef DEM_USE_NVM_EXTENDED_DATA
        if (Std_IsFlagSet(EventContext->flag, DEM_EVENT_FLAG_CAPTURE_EE << j)) {
          /* @SWS_Dem_01081 */
          Dem_TrigerStoreExtendedData(DTCRef, DTCRef->DTCAttributes->MemoryDestination[j]);

          Std_FlagClear(EventContext->flag, DEM_EVENT_FLAG_CAPTURE_EE << j);
        }
#endif
      }
    }
  }
}
