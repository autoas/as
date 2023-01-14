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
#ifdef USE_SHELL
#include "shell.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DEM 0
#define AS_LOG_DEMI 1
#define AS_LOG_DEME 2

#define DEM_CONFIG (&Dem_Config)

#define DEM_STATE_OFF ((Dem_StateType)0x00)
#define DEM_STATE_PRE_INIT ((Dem_StateType)0x01)
#define DEM_STATE_INIT ((Dem_StateType)0x02)

#define DEM_FREEZE_FRAME_SLOT_FREE 0xFF

#define DEM_FILTER_BY_MASK 0x00
#define DEM_FILTER_BY_SELECTION 0x00
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  boolean disableDtcSetting;
  struct {
    uint32_t DTCNumber;
    int index;
    uint8_t statusMask;
    uint8_t RecordNumber;
  } filter;
#ifdef DEM_USE_ENABLE_CONDITION
  uint32_t conditionMask;
#endif
} Dem_ContextType;
/* ================================ [ DECLARES  ] ============================================== */
extern const Dem_ConfigType Dem_Config;
static uint8_t *Dem_MallocFreezeFrameRecord(Dem_EventIdType EventId);
static Dem_FreezeFrameRecordType *Dem_LookupFreezeFrameRecordByRecordNumber(Dem_EventIdType EventId,
                                                                            uint8_t *recNum,
                                                                            uint8_t **data,
                                                                            int *where);
static void Dem_FreeFreezeFrameRecord(Dem_EventIdType EventId);
/* ================================ [ DATAS     ] ============================================== */
static Dem_ContextType Dem_Context;
/* ================================ [ LOCALS    ] ============================================== */
static void Dem_EventInit(Dem_EventIdType EventId) {
  Dem_EventContextType *EventContext = &DEM_CONFIG->EventContexts[EventId];

  EventContext->status = DEM_EVENT_STATUS_UNKNOWN;
  EventContext->debouneCounter = 0;
}

static void Dem_StartOperationCycle(uint8_t OperationCycleId) {
  int i;
  Dem_EventContextType *EventContext;
  const Dem_EventConfigType *EventConfig;

  ASLOG(DEMI, ("Operation Cycle %d Start\n", OperationCycleId));

  for (i = 0; i < DEM_CONFIG->numOfEvents; i++) {
    EventConfig = &DEM_CONFIG->EventConfigs[i];
    EventContext = &DEM_CONFIG->EventContexts[i];

    EventContext->status = DEM_EVENT_STATUS_UNKNOWN;

    if (EventConfig->DTCAttributes->OperationCycleRef == OperationCycleId) {
      EventContext->debouneCounter = 0;
      Dem_UdsBitClear(EventConfig->EventStatusRecords->status, DEM_UDS_STATUS_TF);

      /* @SWS_Dem_00389 */
      Dem_UdsBitClear(EventConfig->EventStatusRecords->status, DEM_UDS_STATUS_TFTOC);
      /* @SWS_Dem_00394 */
      Dem_UdsBitSet(EventConfig->EventStatusRecords->status, DEM_UDS_STATUS_TNCTOC);
    }
  }
}

static void Dem_StopOperationCycle(uint8_t OperationCycleId) {
  int i;
  const Dem_EventConfigType *EventConfig;
#ifdef DEM_USE_NVM
  boolean bUpdated = FALSE;
#endif
  ASLOG(DEMI, ("Operation Cycle %d Stop\n", OperationCycleId));

  for (i = 0; i < DEM_CONFIG->numOfEvents; i++) {
    EventConfig = &DEM_CONFIG->EventConfigs[i];

    if (0 == (EventConfig->EventStatusRecords->status &
              (DEM_UDS_STATUS_TNCTOC | DEM_UDS_STATUS_TFTOC))) {
      if (EventConfig->EventStatusRecords->status & DEM_UDS_STATUS_PDTC) {
        /* @SWS_Dem_00390 */
        Dem_UdsBitClear(EventConfig->EventStatusRecords->status, DEM_UDS_STATUS_PDTC);
#ifdef DEM_USE_NVM
        bUpdated = TRUE;
#endif
      }
      if (EventConfig->EventStatusRecords->status & DEM_UDS_STATUS_CDTC) {
        if ((EventConfig->DTCAttributes->AgingAllowed) &&
            (EventConfig->EventStatusRecords->agingCounter <
             EventConfig->DTCAttributes->AgingCycleCounterThreshold)) {
          EventConfig->EventStatusRecords->agingCounter++;
#ifdef DEM_USE_NVM
          bUpdated = TRUE;
#endif
          ASLOG(DEMI,
                ("Event %d aging %d out of %d\n", i, EventConfig->EventStatusRecords->agingCounter,
                 EventConfig->DTCAttributes->AgingCycleCounterThreshold));
          if (EventConfig->EventStatusRecords->agingCounter >=
              EventConfig->DTCAttributes->AgingCycleCounterThreshold) {
            Dem_UdsBitClear(EventConfig->EventStatusRecords->status,
                            DEM_UDS_STATUS_CDTC | DEM_UDS_STATUS_TFSLC);
            EventConfig->EventStatusRecords->testFailedCounter = 0;
            EventConfig->EventStatusRecords->agingCounter = 0;
            EventConfig->EventStatusRecords->faultOccuranceCounter = 0;
            Dem_FreeFreezeFrameRecord((Dem_EventIdType)i);

            if (EventConfig->EventStatusRecords->agedCounter < UINT8_MAX) {
              EventConfig->EventStatusRecords->agedCounter++;
            }
          }
        }
      }
#ifdef DEM_USE_NVM
      if (bUpdated) {
        NvM_WriteBlock(EventConfig->NvmBlockId, NULL);
      }
#endif
    }
  }
}

static Std_ReturnType Dem_SetEventStatusDebounce(Dem_EventIdType EventId,
                                                 Dem_EventStatusType EventStatus) {
  Std_ReturnType r = E_OK;
  const Dem_EventConfigType *EventConfig;
  Dem_EventContextType *EventContext;
  const Dem_DebounceCounterBasedConfigType *DebounceCfg;
  Dem_EventStatusType DebouncedStatus;

  EventConfig = &DEM_CONFIG->EventConfigs[EventId];
  EventContext = &DEM_CONFIG->EventContexts[EventId];

  DebounceCfg = EventConfig->DTCAttributes->DebounceCounterBased;

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
  }

  EventContext->status = DebouncedStatus;

  return r;
}

static void Dem_FillExtendedData(Dem_EventIdType EventId, uint8_t *data) {
  const Dem_EventConfigType *EventConfig = &DEM_CONFIG->EventConfigs[EventId];
  const Dem_ExtendedDataClassType *ExtendedDataClass =
    EventConfig->DTCAttributes->ExtendedDataClass;
  int offset = 0;
  int i;
  int index;

  for (i = 0; i < ExtendedDataClass->numOfExtendedData; i++) {
    index = ExtendedDataClass->ExtendedDataNumberIndex[i];
    DEM_CONFIG->ExtendedDataConfigs[index].GetExtendedDataFnc(EventId, &data[offset]);
    offset += DEM_CONFIG->ExtendedDataConfigs[index].length;
  }
}

static uint8_t *Dem_MallocFreezeFrameRecord(Dem_EventIdType EventId) {
  Dem_FreezeFrameRecordType *record = NULL;
  uint8_t *data;
  int i, slot;
  uint8_t lowPriority;
  Dem_EventIdType lowEventId;
#ifdef DEM_USE_NVM
  uint16_t blockId = ((uint16_t)-1);
#endif

  EnterCritical();

  data = NULL;

  for (i = 0; i < DEM_CONFIG->numOfFreezeFrameRecords; i++) {
    if (DEM_INVALID_EVENT_ID == DEM_CONFIG->FreezeFrameRecords[i]->EventId) {
      if (NULL == record) {
        record = DEM_CONFIG->FreezeFrameRecords[i];
#ifdef DEM_USE_NVM
        blockId = DEM_CONFIG->FreezeFrameNvmBlockIds[i];
#endif
      }
    } else if (EventId == DEM_CONFIG->FreezeFrameRecords[i]->EventId) {
      record = DEM_CONFIG->FreezeFrameRecords[i];
#ifdef DEM_USE_NVM
      blockId = DEM_CONFIG->FreezeFrameNvmBlockIds[i];
#endif
      break;
    } else {
    }
  }

  if (NULL == record) {
    /* looking for displacement according to priority */
    lowPriority = DEM_CONFIG->EventConfigs[EventId].Priority;
    for (i = 0; i < DEM_CONFIG->numOfFreezeFrameRecords; i++) {
      lowEventId = DEM_CONFIG->FreezeFrameRecords[i]->EventId;
      if (lowEventId < DEM_CONFIG->numOfEvents) {
        if (DEM_CONFIG->EventConfigs[lowEventId].Priority > lowPriority) {
          record = DEM_CONFIG->FreezeFrameRecords[i];
#ifdef DEM_USE_NVM
          blockId = DEM_CONFIG->FreezeFrameNvmBlockIds[i];
#endif
          lowPriority =
            DEM_CONFIG->EventConfigs[DEM_CONFIG->FreezeFrameRecords[i]->EventId].Priority;
          ASLOG(DEMI,
                ("replace Event %d freeze frame @ %d for Event %d\n", lowEventId, i, EventId));
        }
      } else {
        ASLOG(DEME, ("an invalid freeze frame found, drop it\n"));
        record = DEM_CONFIG->FreezeFrameRecords[i];
#ifdef DEM_USE_NVM
        blockId = DEM_CONFIG->FreezeFrameNvmBlockIds[i];
#endif
        break;
      }
    }
  }

  if (NULL != record) {
    if (EventId != record->EventId) {
      /* newly created */
      for (slot = 0; slot < DEM_MAX_FREEZE_FRAME_NUMBER; slot++) {
        record->FreezeFrameData[slot][0] =
          DEM_FREEZE_FRAME_SLOT_FREE; /* First byte set to 0xFF: means free */
      }
      data = record->FreezeFrameData[0];
      record->EventId = EventId;
    } else {
      for (slot = 0; slot < DEM_MAX_FREEZE_FRAME_NUMBER; slot++) {
        if (DEM_FREEZE_FRAME_SLOT_FREE == record->FreezeFrameData[slot][0]) {
          data = record->FreezeFrameData[slot];
          break;
        }
      }
#if DEM_MAX_FREEZE_FRAME_NUMBER > 1
      if (NULL == data) {
        /* overwrite the last one with latest environment data */
        data = record->FreezeFrameData[DEM_MAX_FREEZE_FRAME_NUMBER - 1];
      }
#endif
    }
  }

  ExitCritical();

#ifdef DEM_USE_NVM
  if (data != NULL) {
    /* TODO: this is not a good idea that request the write here, but as NvM_WriteBlock is aysnc, so
     * it was safe to do so */
    NvM_WriteBlock(blockId, NULL);
  }
#endif

  return data;
}

static Dem_FreezeFrameRecordType *Dem_LookupFreezeFrameRecordByRecordNumber(Dem_EventIdType EventId,
                                                                            uint8_t *recNum,
                                                                            uint8_t **data,
                                                                            int *where) {
  Dem_FreezeFrameRecordType *record = NULL;
  int index, i, slot;

  for (index = *where; index < DEM_CONFIG->numOfFreezeFrameRecords * DEM_MAX_FREEZE_FRAME_NUMBER;
       index++) {
    i = index / DEM_MAX_FREEZE_FRAME_NUMBER;
    slot = index % DEM_MAX_FREEZE_FRAME_NUMBER;
    if ((EventId == DEM_CONFIG->FreezeFrameRecords[i]->EventId) &&
        (DEM_FREEZE_FRAME_SLOT_FREE !=
         DEM_CONFIG->FreezeFrameRecords[i]->FreezeFrameData[slot][0])) {
      if ((*recNum == DEM_CONFIG->FreezeFrameRecords[i]->FreezeFrameData[slot][0]) ||
          (0xFF == *recNum)) {
        record = DEM_CONFIG->FreezeFrameRecords[i];
        if (data != NULL) {
          *data = &DEM_CONFIG->FreezeFrameRecords[i]->FreezeFrameData[slot][1];
        }
        *recNum = DEM_CONFIG->FreezeFrameRecords[i]->FreezeFrameData[slot][0];
        *where = index + 1;
        break;
      }
    }
  }

  return record;
}

static uint16_t Dem_GetNumberOfFreezeFrameRecordsByRecordNumber(Dem_EventIdType EventId,
                                                                uint8_t recNum) {
  uint16_t r = 0;
  int index = 0;
  uint8_t _recNum;

  Dem_FreezeFrameRecordType *record = NULL;

  do {
    _recNum = recNum;
    record = Dem_LookupFreezeFrameRecordByRecordNumber(EventId, &_recNum, NULL, &index);
    if (record != NULL) {
      r++;
    }
  } while (record != NULL);

  return r;
}

static Dem_FreezeFrameRecordType *Dem_LookupFreezeFrameRecordByIndex(int index, uint8_t *recNum,
                                                                     uint8_t **data) {
  Dem_FreezeFrameRecordType *record = NULL;
  int i, slot;

  i = index / DEM_MAX_FREEZE_FRAME_NUMBER;
  slot = index % DEM_MAX_FREEZE_FRAME_NUMBER;

  if (i < DEM_CONFIG->numOfFreezeFrameRecords) {
    if ((DEM_INVALID_EVENT_ID != DEM_CONFIG->FreezeFrameRecords[i]->EventId) &&
        (DEM_FREEZE_FRAME_SLOT_FREE !=
         DEM_CONFIG->FreezeFrameRecords[i]->FreezeFrameData[slot][0])) {
      record = DEM_CONFIG->FreezeFrameRecords[i];
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

static void Dem_FreeFreezeFrameRecord(Dem_EventIdType EventId) {
  Dem_FreezeFrameRecordType *record = NULL;
  int i;

  for (i = 0; i < DEM_CONFIG->numOfFreezeFrameRecords; i++) {
    if (EventId == DEM_CONFIG->FreezeFrameRecords[i]->EventId) {
      ASLOG(DEMI, ("Delete Event %d freeze frame @ %d\n", EventId, i));
      record = DEM_CONFIG->FreezeFrameRecords[i];
      record->EventId = DEM_INVALID_EVENT_ID;
      memset(record->FreezeFrameData, 0xFF, sizeof(record->FreezeFrameData));
    }
  }
}

static uint16_t Dem_GetExtendedDataSize(const Dem_ExtendedDataClassType *ExtendedDataClass) {
  uint16_t sz = 0;
  int index, i;

  for (i = 0; i < ExtendedDataClass->numOfExtendedData; i++) {
    index = ExtendedDataClass->ExtendedDataNumberIndex[i];
    sz += DEM_CONFIG->ExtendedDataConfigs[index].length;
  }

  return sz;
}

static uint16_t
Dem_GetFreezeFrameSize(const Dem_FreezeFrameRecordClassType *FreezeFrameRecordClass) {
  uint16_t sz = 0;
  int index, i;

  for (i = 0; i < FreezeFrameRecordClass->numOfFreezeFrameData; i++) {
    index = FreezeFrameRecordClass->freezeFrameDataIndex[i];
    sz += DEM_CONFIG->FreeFrameDataConfigs[index].length;
  }

  return sz;
}
static void Dem_FillSnapshotRecord(uint8_t *DestBuf, uint8_t *data,
                                   const Dem_FreezeFrameRecordClassType *FreezeFrameRecordClass) {
  int index, i;
  uint8_t *pIn = data, *pOut = DestBuf;

  for (i = 0; i < FreezeFrameRecordClass->numOfFreezeFrameData; i++) {
    index = FreezeFrameRecordClass->freezeFrameDataIndex[i];
    pOut[0] = (DEM_CONFIG->FreeFrameDataConfigs[index].id >> 8) & 0xFF;
    pOut[1] = DEM_CONFIG->FreeFrameDataConfigs[index].id & 0xFF;
    memcpy(&pOut[2], pIn, DEM_CONFIG->FreeFrameDataConfigs[index].length);
    pOut += 2 + DEM_CONFIG->FreeFrameDataConfigs[index].length;
    pIn += DEM_CONFIG->FreeFrameDataConfigs[index].length;
  }
}

static uint8_t Dem_GetNextRecordNumber(Dem_EventIdType EventId) {
  uint8_t recNum;
  uint16_t numOfRecs;
  const Dem_EventConfigType *EventConfig = &DEM_CONFIG->EventConfigs[EventId];

  numOfRecs = Dem_GetNumberOfFreezeFrameRecordsByRecordNumber(EventId, 0xFF);

  if (DEM_FF_RECNUM_CONFIGURED == DEM_CONFIG->TypeOfFreezeFrameRecordNumeration) {
    if (numOfRecs < EventConfig->FreezeFrameRecNumClass->numOfFreezeFrameRecNums) {
      recNum = EventConfig->FreezeFrameRecNumClass->FreezeFrameRecNums[numOfRecs];
    } else {
      recNum =
        EventConfig->FreezeFrameRecNumClass
          ->FreezeFrameRecNums[EventConfig->FreezeFrameRecNumClass->numOfFreezeFrameRecNums - 1];
    }
  } else {
    recNum = (uint8_t)numOfRecs + 1;
  }

  return recNum;
}

static void Dem_TrigerStoreFreezeFrame(Dem_EventIdType EventId) {
  const Dem_EventConfigType *EventConfig = &DEM_CONFIG->EventConfigs[EventId];
  const Dem_FreezeFrameRecordClassType *FreezeFrameRecordClass =
    EventConfig->DTCAttributes->FreezeFrameRecordClass;
  uint8_t *data = NULL;
  int offset = 1;
  int i, index;

  data = Dem_MallocFreezeFrameRecord(EventId);

  if (data != NULL) {
    for (i = 0; i < FreezeFrameRecordClass->numOfFreezeFrameData; i++) {
      index = FreezeFrameRecordClass->freezeFrameDataIndex[i];
      if ((offset + DEM_CONFIG->FreeFrameDataConfigs[index].length) <=
          DEM_MAX_FREEZE_FRAME_DATA_SIZE) {
        DEM_CONFIG->FreeFrameDataConfigs[index].GetFrezeFrameDataFnc(EventId, &data[offset]);
        offset += DEM_CONFIG->FreeFrameDataConfigs[index].length;
      } else {
        ASLOG(DEME, ("FreezeFrame record size too small\n"));
      }
    }
    data[0] = Dem_GetNextRecordNumber(EventId);
    ASLOG(DEMI, ("Event %d capture freeze frame with record number %d\n", EventId, data[0]));
  } else {
    ASLOG(DEMI, ("Event %d freeze failed as no slot\n", EventId));
  }
}

static void Dem_EventUpdateOnFailed(Dem_EventIdType EventId) {
  const Dem_EventConfigType *EventConfig = &DEM_CONFIG->EventConfigs[EventId];
  Dem_UdsStatusByteType oldStatus = EventConfig->EventStatusRecords->status;
#ifdef DEM_USE_NVM
  boolean bUpdated = FALSE;
#endif
  ASLOG(DEMI, ("Event %d Test Failed\n", EventId));

  Dem_UdsBitClear(EventConfig->EventStatusRecords->status,
                  DEM_UDS_STATUS_TNCTOC | DEM_UDS_STATUS_TNCSLC);

  Dem_UdsBitSet(EventConfig->EventStatusRecords->status, DEM_UDS_STATUS_TF | DEM_UDS_STATUS_TFTOC |
                                                           DEM_UDS_STATUS_PDTC |
                                                           DEM_UDS_STATUS_TFSLC);

  EventConfig->EventStatusRecords->agingCounter = 0;

  if (EventConfig->EventStatusRecords->testFailedCounter < UINT8_MAX) {
    EventConfig->EventStatusRecords->testFailedCounter++;
#ifdef DEM_USE_NVM
    bUpdated = TRUE;
#endif
  }

  if (EventConfig->EventStatusRecords->testFailedCounter >=
      EventConfig->DTCAttributes->EventConfirmationThreshold) {
    ASLOG(DEMI, ("Event %d confirmed\n", EventId));
    Dem_UdsBitSet(EventConfig->EventStatusRecords->status, DEM_UDS_STATUS_CDTC);
  }

  if (0 == (oldStatus & DEM_UDS_STATUS_TF)) {
    /* @SWS_Dem_00524 */
    if (DEM_PROCESS_OCCCTR_TF == EventConfig->DTCAttributes->OccurrenceCounterProcessing) {
      if (EventConfig->EventStatusRecords->faultOccuranceCounter < UINT8_MAX) {
        EventConfig->EventStatusRecords->faultOccuranceCounter++;
#ifdef DEM_USE_NVM
        bUpdated = TRUE;
#endif
      }
    }

    if (DEM_TRIGGER_ON_TEST_FAILED == EventConfig->DTCAttributes->FreezeFrameRecordTrigger) {
      Dem_TrigerStoreFreezeFrame(EventId);
    }
  }

  if ((0 == (oldStatus & DEM_UDS_STATUS_CDTC)) &&
      (0 != (EventConfig->EventStatusRecords->status & DEM_UDS_STATUS_CDTC))) {
    /* @SWS_Dem_00580 */
    if (DEM_PROCESS_OCCCTR_CDTC == EventConfig->DTCAttributes->OccurrenceCounterProcessing) {
      if (EventConfig->EventStatusRecords->faultOccuranceCounter < UINT8_MAX) {
        EventConfig->EventStatusRecords->faultOccuranceCounter++;
#ifdef DEM_USE_NVM
        bUpdated = TRUE;
#endif
      }
    }

    if (DEM_TRIGGER_ON_CONFIRMED == EventConfig->DTCAttributes->FreezeFrameRecordTrigger) {
      Dem_TrigerStoreFreezeFrame(EventId);
    }
  }

#ifdef DEM_USE_NVM
  if (bUpdated || ((oldStatus & DEM_DEM_STORE_CARED_BITS) !=
                   (EventConfig->EventStatusRecords->status & DEM_DEM_STORE_CARED_BITS))) {
    NvM_WriteBlock(EventConfig->NvmBlockId, NULL);
  }
#endif
}

static void Dem_EventUpdateOnPass(Dem_EventIdType EventId) {
  const Dem_EventConfigType *EventConfig = &DEM_CONFIG->EventConfigs[EventId];
#ifdef DEM_USE_NVM
  Dem_UdsStatusByteType oldStatus = EventConfig->EventStatusRecords->status;
  boolean bUpdated = FALSE;
#endif

  if (EventConfig->EventStatusRecords->testFailedCounter > 0) {
    EventConfig->EventStatusRecords->testFailedCounter = 0;
#ifdef DEM_USE_NVM
    bUpdated = TRUE;
#endif
  }

  Dem_UdsBitClear(EventConfig->EventStatusRecords->status,
                  DEM_UDS_STATUS_TNCTOC | DEM_UDS_STATUS_TNCSLC);

  Dem_UdsBitClear(EventConfig->EventStatusRecords->status, DEM_UDS_STATUS_TF);
  ASLOG(DEMI, ("Event %d Test Passed\n", EventId));

#ifdef DEM_USE_NVM
  if (bUpdated || ((oldStatus & DEM_DEM_STORE_CARED_BITS) !=
                   (EventConfig->EventStatusRecords->status & DEM_DEM_STORE_CARED_BITS))) {
    NvM_WriteBlock(EventConfig->NvmBlockId, NULL);
  }
#endif
}

static boolean Dem_IsFilteredDTC(Dem_EventIdType EventId) {
  const Dem_EventConfigType *EventConfig = &DEM_CONFIG->EventConfigs[EventId];
  boolean r = TRUE;

  if (0 == (Dem_Context.filter.statusMask & EventConfig->EventStatusRecords->status)) {
    r = FALSE;
  }

  return r;
}

static boolean Dem_IsFilteredFF(int index) {
  /* only UDS FF are supported */
  return TRUE;
}

static const Dem_EventConfigType *Dem_LookupEventByDTCNumber(uint32_t DTCNumber,
                                                             Dem_EventIdType *EventId) {
  int i;
  const Dem_EventConfigType *EventConfig = NULL;

  for (i = 0; i < DEM_CONFIG->numOfEvents; i++) {
    if (DEM_CONFIG->EventConfigs[i].DtcNumber == DTCNumber) {
      EventConfig = &DEM_CONFIG->EventConfigs[i];
      *EventId = i;
      break;
    }
  }

  return EventConfig;
}
#ifdef USE_SHELL
static int cmdLsDtcFunc(int argc, const char *argv[]) {
  int i, j, k, offset;
  uint8_t *data;
  for (i = 0; i < DEM_CONFIG->numOfOperationCycles; i++) {
    printf("Operation Cycle %d %s\n", i,
           (DEM_OPERATION_CYCLE_STARTED == DEM_CONFIG->OperationCycleStates[i]) ? "started"
                                                                                : "stopt");
  }
  for (i = 0; i < DEM_CONFIG->numOfEvents; i++) {
    printf("Event %d status=%02X, agingCounter=%d, agedCounter=%d occuranceCounter=%d\n", i,
           DEM_CONFIG->EventConfigs[i].EventStatusRecords->status,
           DEM_CONFIG->EventConfigs[i].EventStatusRecords->agingCounter,
           DEM_CONFIG->EventConfigs[i].EventStatusRecords->agedCounter,
           DEM_CONFIG->EventConfigs[i].EventStatusRecords->faultOccuranceCounter);
  }
  for (i = 0; i < DEM_CONFIG->numOfFreezeFrameRecords; i++) {
    if (DEM_CONFIG->FreezeFrameRecords[i]->EventId != DEM_INVALID_EVENT_ID) {
      for (j = 0; j < DEM_MAX_FREEZE_FRAME_NUMBER; j++) {
        data = DEM_CONFIG->FreezeFrameRecords[i]->FreezeFrameData[j];
        if (DEM_FREEZE_FRAME_SLOT_FREE != data[0]) {
          printf("Event %d has freeze frame at slot %d with record number %d\n",
                 DEM_CONFIG->FreezeFrameRecords[i]->EventId, j, data[0]);
          offset = 1;
          for (k = 0; k < DEM_CONFIG->numOfFreeFrameDataConfigs; k++) {
            DEM_CONFIG->FreeFrameDataConfigs[k].print(&data[offset]);
            offset += DEM_CONFIG->FreeFrameDataConfigs[k].length;
          }
        }
      }
    }
  }
  return 0;
}
SHELL_REGISTER(lsdtc, "list DTC status and its related information\n", cmdLsDtcFunc);
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
void Dem_PreInit(void) {
  /*NOTE: BSW Event is not supported, so this API is dummy */
}

void Dem_Init(const Dem_ConfigType *ConfigPtr) {
  int i;

  for (i = 0; i < DEM_CONFIG->numOfOperationCycles; i++) {
    DEM_CONFIG->OperationCycleStates[i] = DEM_OPERATION_CYCLE_STOPPED;
  }

  for (i = 0; i < DEM_CONFIG->numOfEvents; i++) {
    Dem_EventInit((Dem_EventIdType)i);
    ASLOG(DEMI, ("Event %d status=%02X, agingCounter=%d, agedCounter=%d occuranceCounter=%d\n", i,
                 DEM_CONFIG->EventConfigs[i].EventStatusRecords->status,
                 DEM_CONFIG->EventConfigs[i].EventStatusRecords->agingCounter,
                 DEM_CONFIG->EventConfigs[i].EventStatusRecords->agedCounter,
                 DEM_CONFIG->EventConfigs[i].EventStatusRecords->faultOccuranceCounter));
  }

#if AS_LOG_DEMI >= AS_LOG_DEFAULT
  for (i = 0; i < DEM_CONFIG->numOfFreezeFrameRecords; i++) {
    if (DEM_CONFIG->FreezeFrameRecords[i]->EventId != DEM_INVALID_EVENT_ID) {
      int j;
      for (j = 0; j < DEM_MAX_FREEZE_FRAME_NUMBER; j++) {
        if (DEM_FREEZE_FRAME_SLOT_FREE !=
            DEM_CONFIG->FreezeFrameRecords[i]->FreezeFrameData[j][0]) {
          ASLOG(DEMI, ("Event %d has freeze frame at slot %d with record number %d\n",
                       DEM_CONFIG->FreezeFrameRecords[i]->EventId, j,
                       DEM_CONFIG->FreezeFrameRecords[i]->FreezeFrameData[j][0]));
        }
      }
    }
  }
#endif
  memset(&Dem_Context, 0, sizeof(Dem_Context));
  Dem_Context.disableDtcSetting = FALSE;
}

Std_ReturnType Dem_EXTD_GetFaultOccuranceCounter(Dem_EventIdType EventId, uint8_t *data) {
  Std_ReturnType r = E_OK;
  const Dem_EventConfigType *EventConfig;

  if (EventId < DEM_CONFIG->numOfEvents) {
    EventConfig = &DEM_CONFIG->EventConfigs[EventId];
    data[0] = EventConfig->EventStatusRecords->faultOccuranceCounter;
  } else {
    r = E_NOT_OK;
  }

  return r;
}

Std_ReturnType Dem_EXTD_GetAgingCounter(Dem_EventIdType EventId, uint8_t *data) {
  Std_ReturnType r = E_OK;
  const Dem_EventConfigType *EventConfig;

  if (EventId < DEM_CONFIG->numOfEvents) {
    EventConfig = &DEM_CONFIG->EventConfigs[EventId];
    data[0] = EventConfig->EventStatusRecords->agingCounter;
  } else {
    r = E_NOT_OK;
  }

  return r;
}

Std_ReturnType Dem_EXTD_GetAgedCounter(Dem_EventIdType EventId, uint8_t *data) {
  Std_ReturnType r = E_OK;
  const Dem_EventConfigType *EventConfig;

  if (EventId < DEM_CONFIG->numOfEvents) {
    EventConfig = &DEM_CONFIG->EventConfigs[EventId];
    data[0] = EventConfig->EventStatusRecords->agedCounter;
  } else {
    r = E_NOT_OK;
  }

  return r;
}

Std_ReturnType Dem_GetExtendedDataByNumber(Dem_EventIdType EventId, uint8_t Number, uint8_t *data,
                                           uint8_t size) {
  Std_ReturnType r = E_NOT_OK;
  const Dem_EventConfigType *EventConfig;
  const Dem_ExtendedDataClassType *ExtendedDataClass;
  int i;
  int length = 0;
  int index;

  if (EventId < DEM_CONFIG->numOfEvents) {
    EventConfig = &DEM_CONFIG->EventConfigs[EventId];
    ExtendedDataClass = EventConfig->DTCAttributes->ExtendedDataClass;
    for (i = 0; i < ExtendedDataClass->numOfExtendedData; i++) {
      index = ExtendedDataClass->ExtendedDataNumberIndex[i];
      if (DEM_CONFIG->ExtendedDataConfigs[index].ExtendedDataNumber == Number) {
        length = DEM_CONFIG->ExtendedDataConfigs[index].length;
        if (size >= length) {
          r = DEM_CONFIG->ExtendedDataConfigs[index].GetExtendedDataFnc(EventId, data);
        }
        break;
      }
    }
  }

  if (E_OK != r) {
    ASLOG(DEME, ("Event %d has no Extended Number 0x%02X\n", EventId, Number));
  }

  return r;
}

Std_ReturnType Dem_SetOperationCycleState(uint8_t OperationCycleId,
                                          Dem_OperationCycleStateType cycleState) {
  Std_ReturnType r = E_OK;

  if (OperationCycleId < DEM_CONFIG->numOfOperationCycles) {
    if (cycleState != DEM_CONFIG->OperationCycleStates[OperationCycleId]) {
      if (DEM_OPERATION_CYCLE_STARTED == cycleState) {
        Dem_StartOperationCycle(OperationCycleId);
      } else {
        Dem_StopOperationCycle(OperationCycleId);
      }
      DEM_CONFIG->OperationCycleStates[OperationCycleId] = cycleState;
    }
  } else {
    r = E_NOT_OK;
  }

  return r;
}

Std_ReturnType Dem_RestartOperationCycle(uint8_t OperationCycleId) {
  Std_ReturnType r = E_OK;

  if (OperationCycleId < DEM_CONFIG->numOfOperationCycles) {
    if (DEM_OPERATION_CYCLE_STARTED == DEM_CONFIG->OperationCycleStates[OperationCycleId]) {
      Dem_StopOperationCycle(OperationCycleId);
      Dem_StartOperationCycle(OperationCycleId);
    } else {
      r = E_NOT_OK;
    }
  } else {
    r = E_NOT_OK;
  }

  return r;
}

Std_ReturnType Dem_SetEventStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus) {
  Std_ReturnType r = E_OK;
  const Dem_EventConfigType *EventConfig;
  Dem_EventContextType *EventContext;
  Dem_EventStatusType OldStatus = DEM_EVENT_STATUS_UNKNOWN;

  if (EventId < DEM_CONFIG->numOfEvents) {
    EventConfig = &DEM_CONFIG->EventConfigs[EventId];
    EventContext = &DEM_CONFIG->EventContexts[EventId];
    if ((DEM_OPERATION_CYCLE_STARTED ==
         DEM_CONFIG->OperationCycleStates[EventConfig->DTCAttributes->OperationCycleRef]) &&
        (FALSE == Dem_Context.disableDtcSetting)) {
#ifdef DEM_USE_ENABLE_CONDITION
      /* @SWS_Dem_00449 */
      if ((0 != EventConfig->ConditionRefMask) &&
          (EventConfig->ConditionRefMask !=
           (Dem_Context.conditionMask & EventConfig->ConditionRefMask))) {
            r = E_NOT_OK;
      }
#endif
    } else {
      r = E_NOT_OK;
    }
  } else {
    r = E_NOT_OK;
  }

  if (E_OK == r) {
    ASLOG(DEMI, ("Set Event %d status %d\n", EventId, EventStatus));
    OldStatus = EventContext->status;
    r = Dem_SetEventStatusDebounce(EventId, EventStatus);
  }

  if (E_OK == r) {
    if (OldStatus != EventContext->status) {
      if (DEM_EVENT_STATUS_FAILED == EventContext->status) {
        Dem_EventUpdateOnFailed(EventId);
      } else {
        Dem_EventUpdateOnPass(EventId);
      }
    }
  }
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

  (void)ClientId; /* not used */
  ASLOG(DEMI, ("enable DTC setting\n"));

  Dem_Context.disableDtcSetting = FALSE;

  return r;
}

Std_ReturnType Dem_ClearDTC(uint8_t ClientId) {
  Std_ReturnType r = E_OK;
  const Dem_EventConfigType *EventConfig = NULL;
  Dem_EventIdType EventId = DEM_INVALID_EVENT_ID;
  int i;
  uint32_t groupDTC = Dem_Context.filter.DTCNumber;
  (void)ClientId; /* not used */

  if (0xFFFFFFu == groupDTC) {
    /* clear all DTC */
  } else {
    EventConfig = Dem_LookupEventByDTCNumber(groupDTC, &EventId);
    if (NULL != EventConfig) {
      /* clear specific DTC */
    } else {
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    /* NOTE: 0xFF is initial value of NVM, means free */
    for (i = 0; i < DEM_CONFIG->numOfFreezeFrameRecords; i++) {
      if ((DEM_CONFIG->FreezeFrameRecords[i]->EventId == EventId) ||
          (DEM_INVALID_EVENT_ID == EventId)) {
        memset(DEM_CONFIG->FreezeFrameRecords[i], 0xFF, sizeof(Dem_FreezeFrameRecordType));
#ifdef DEM_USE_NVM
        NvM_WriteBlock(DEM_CONFIG->FreezeFrameNvmBlockIds[i], NULL);
#endif
      }
    }

    for (i = 0; i < DEM_CONFIG->numOfEvents; i++) {
      if ((i == EventId) || (DEM_INVALID_EVENT_ID == EventId)) {
        EventConfig = &DEM_CONFIG->EventConfigs[i];
        memset(EventConfig->EventStatusRecords, 0x00, sizeof(Dem_EventStatusRecordType));
        EventConfig->EventStatusRecords->status = DEM_UDS_STATUS_TNCTOC | DEM_UDS_STATUS_TNCSLC;
#ifdef DEM_USE_NVM
        NvM_WriteBlock(EventConfig->NvmBlockId, NULL);
#endif
      }
    }
  }

  return r;
}

Std_ReturnType Dem_SetDTCFilter(uint8_t ClientId, uint8_t DTCStatusMask,
                                Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin,
                                boolean FilterWithSeverity, Dem_DTCSeverityType DTCSeverityMask,
                                boolean FilterForFaultDetectionCounter) {
  Std_ReturnType r = E_OK;

  Dem_Context.filter.statusMask = DTCStatusMask;
  Dem_Context.filter.index = 0;

  return r;
}

Std_ReturnType Dem_GetNumberOfFilteredDTC(uint8_t ClientId, uint16_t *NumberOfFilteredDTC) {
  Std_ReturnType r = E_OK;
  boolean IsFiltered;
  int i;
  uint16_t number = 0;

  for (i = 0; i < DEM_CONFIG->numOfEvents; i++) {
    IsFiltered = Dem_IsFilteredDTC((Dem_EventIdType)i);
    if (IsFiltered) {
      number++;
    }
  }

  ASLOG(DEMI, ("NumberOfFilteredDTC = %d\n", number));

  *NumberOfFilteredDTC = number;

  return r;
}

Std_ReturnType Dem_GetNextFilteredDTC(uint8_t ClientId, uint32_t *DTC, uint8_t *DTCStatus) {
  Std_ReturnType r = E_NOT_OK;
  boolean IsFiltered;
  int i;
  const Dem_EventConfigType *EventConfig;

  for (i = Dem_Context.filter.index; i < DEM_CONFIG->numOfEvents; i++) {
    IsFiltered = Dem_IsFilteredDTC((Dem_EventIdType)i);
    if (IsFiltered) {
      EventConfig = &DEM_CONFIG->EventConfigs[i];
      *DTC = EventConfig->DtcNumber;
      *DTCStatus = EventConfig->EventStatusRecords->status;
      Dem_Context.filter.index = i + 1;
      ASLOG(DEMI, ("Get Event %d: %06X %02X\n", i, *DTC, *DTCStatus));
      r = E_OK;
      break;
    }
  }

  return r;
}

Std_ReturnType Dem_SetFreezeFrameRecordFilter(uint8_t ClientId, Dem_DTCFormatType DTCFormat) {
  Std_ReturnType r = E_OK;

  Dem_Context.filter.index = 0;

  return r;
}

Std_ReturnType Dem_GetNumberOfFreezeFrameRecords(uint8_t ClientId,
                                                 uint16_t *NumberOfFilteredRecords) {
  Std_ReturnType r = E_OK;
  int i, slot;
  uint16_t number = 0;
  Dem_FreezeFrameRecordType *record;

  for (i = 0; i < DEM_CONFIG->numOfFreezeFrameRecords; i++) {
    if (DEM_INVALID_EVENT_ID != DEM_CONFIG->FreezeFrameRecords[i]->EventId) {
      record = DEM_CONFIG->FreezeFrameRecords[i];
      for (slot = 0; slot < DEM_MAX_FREEZE_FRAME_NUMBER; slot++) {
        if (DEM_FREEZE_FRAME_SLOT_FREE != record->FreezeFrameData[slot][0]) {
          number++;
        }
      }
    }
  }

  *NumberOfFilteredRecords = number;

  return r;
}

Std_ReturnType Dem_GetNextFilteredRecord(uint8_t ClientId, uint32_t *DTC, uint8_t *RecordNumber) {
  Std_ReturnType r = E_NOT_OK;
  boolean IsFiltered;
  int i;
  const Dem_EventConfigType *EventConfig;
  Dem_FreezeFrameRecordType *record;
  uint8_t recNum;

  *RecordNumber = 0;
  for (i = Dem_Context.filter.index;
       i < DEM_CONFIG->numOfFreezeFrameRecords * DEM_MAX_FREEZE_FRAME_NUMBER; i++) {
    IsFiltered = Dem_IsFilteredFF(i);
    record = Dem_LookupFreezeFrameRecordByIndex(i, &recNum, NULL);
    if (IsFiltered && (NULL != record)) {
      EventConfig = &DEM_CONFIG->EventConfigs[record->EventId];
      *DTC = EventConfig->DtcNumber;
      *RecordNumber = recNum;

      Dem_Context.filter.index = i + 1;
      r = E_OK;
      break;
    }
  }

  return r;
}

Std_ReturnType Dem_SelectDTC(uint8_t ClientId, uint32_t DTC, Dem_DTCFormatType DTCFormat,
                             Dem_DTCOriginType DTCOrigin) {
  Std_ReturnType r = E_OK;

  Dem_Context.filter.DTCNumber = DTC;

  return r;
}

Std_ReturnType Dem_SelectFreezeFrameData(uint8_t ClientId, uint8_t RecordNumber) {
  Std_ReturnType r = E_OK;

  Dem_Context.filter.RecordNumber = RecordNumber;

  return r;
}

Std_ReturnType Dem_SelectExtendedDataRecord(uint8_t ClientId, uint8_t ExtendedDataNumber) {
  Std_ReturnType r = E_OK;

  Dem_Context.filter.RecordNumber = ExtendedDataNumber;

  return r;
}

Std_ReturnType Dem_GetNextFreezeFrameData(uint8_t ClientId, uint8_t *DestBuffer,
                                          uint16_t *BufSize) {
  Std_ReturnType r = E_OK;
  Dem_FreezeFrameRecordType *record;
  const Dem_EventConfigType *EventConfig;
  const Dem_FreezeFrameRecordClassType *FreezeFrameRecordClass;
  Dem_EventIdType EventId;
  uint8_t recNum;
  uint8_t *data;
  int i, index = 0;
  int offset;
  uint8_t numOfRecords = 0;
  uint16_t freezeFrameSize = 0;

  EventConfig = Dem_LookupEventByDTCNumber(Dem_Context.filter.DTCNumber, &EventId);

  if (NULL != EventConfig) {
    numOfRecords =
      Dem_GetNumberOfFreezeFrameRecordsByRecordNumber(EventId, Dem_Context.filter.RecordNumber);

    FreezeFrameRecordClass = EventConfig->DTCAttributes->FreezeFrameRecordClass;
    freezeFrameSize = Dem_GetFreezeFrameSize(FreezeFrameRecordClass) +
                      2 * FreezeFrameRecordClass->numOfFreezeFrameData;
    if (((uint32_t)numOfRecords * (freezeFrameSize + 2) + 4) > *BufSize) {
      r = DEM_BUFFER_TOO_SMALL;
    }
  } else {
    r = DEM_WRONG_DTC;
  }

  if (E_OK == r) {
    DestBuffer[0] = (uint8_t)((Dem_Context.filter.DTCNumber >> 16) & 0xFF);
    DestBuffer[1] = (uint8_t)((Dem_Context.filter.DTCNumber >> 8) & 0xFF);
    DestBuffer[2] = (uint8_t)(Dem_Context.filter.DTCNumber & 0xFF);
    DestBuffer[3] = EventConfig->EventStatusRecords->status;
    *BufSize = 4 + (freezeFrameSize + 2) * numOfRecords;
    for (i = 0; i < numOfRecords; i++) {
      offset = 4 + (freezeFrameSize + 2) * i;
      recNum = Dem_Context.filter.RecordNumber;
      record = Dem_LookupFreezeFrameRecordByRecordNumber(EventId, &recNum, &data, &index);
      if (record != NULL) {
        DestBuffer[offset] = recNum;
        DestBuffer[offset + 1] = FreezeFrameRecordClass->numOfFreezeFrameData;
        Dem_FillSnapshotRecord(&DestBuffer[offset + 2], data, FreezeFrameRecordClass);
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
  const Dem_EventConfigType *EventConfig;
  const Dem_ExtendedDataClassType *ExtendedDataClass;
  Dem_EventIdType EventId;
  uint16_t ExtendedDataSize = 0;

  EventConfig = Dem_LookupEventByDTCNumber(Dem_Context.filter.DTCNumber, &EventId);

  if (NULL != EventConfig) {
    /* TODO: use configured RecordNum instead of 1 */
    if ((0xFF == Dem_Context.filter.RecordNumber) || (1 == Dem_Context.filter.RecordNumber)) {
      ExtendedDataClass = EventConfig->DTCAttributes->ExtendedDataClass;
      ExtendedDataSize = Dem_GetExtendedDataSize(ExtendedDataClass);
      if (((uint32_t)ExtendedDataSize + 5) > *BufSize) {
        r = DEM_BUFFER_TOO_SMALL;
      }
    } else {
      r = E_NOT_OK;
    }
  } else {
    r = DEM_WRONG_DTC;
  }

  if (E_OK == r) {
    DestBuffer[0] = (uint8_t)((Dem_Context.filter.DTCNumber >> 16) & 0xFF);
    DestBuffer[1] = (uint8_t)((Dem_Context.filter.DTCNumber >> 8) & 0xFF);
    DestBuffer[2] = (uint8_t)(Dem_Context.filter.DTCNumber & 0xFF);
    DestBuffer[3] = EventConfig->EventStatusRecords->status;
    DestBuffer[4] = 1; /* TODO: use configured Extended RecordNumber */
    Dem_FillExtendedData(EventId, &DestBuffer[5]);
    *BufSize = ExtendedDataSize + 5;
  }

  return r;
}

#ifdef DEM_USE_ENABLE_CONDITION
Std_ReturnType Dem_SetEnableCondition(uint8_t EnableConditionID, boolean ConditionFulfilled) {
  Std_ReturnType r = E_OK;
  if (EnableConditionID < DEM_NUM_OF_ENABLE_CONDITION) {
    if (ConditionFulfilled) {
      Dem_Context.conditionMask |= (1 << EnableConditionID);
    } else {
      Dem_Context.conditionMask &= ~(1 << EnableConditionID);
    }
  } else {
    r = E_NOT_OK;
  }
  return r;
}
#endif
