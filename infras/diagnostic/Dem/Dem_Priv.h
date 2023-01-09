/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Diagnostic Event Manager AUTOSAR CP Release 4.4.0
 *      https://karthik-balu.github.io/projects/2017/11/25/DTCs
 *      ISO 14229:2006(E)
 */
#ifndef DEM_PRIV_H
#define DEM_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dem.h"
#include "Dem_Cfg.h"
/* ================================ [ MACROS    ] ============================================== */
#define DEM_TRIGGER_ON_TEST_FAILED ((Dem_ActionTriggerType)0x00)
#define DEM_TRIGGER_ON_CONFIRMED ((Dem_ActionTriggerType)0x01)
#define DEM_TRIGGER_ON_FDC_THRESHOLD ((Dem_ActionTriggerType)0x02)
#define DEM_TRIGGER_ON_MIRROR ((Dem_ActionTriggerType)0x03)
#define DEM_TRIGGER_ON_PASSED ((Dem_ActionTriggerType)0x04)
#define DEM_TRIGGER_ON_PENDING ((Dem_ActionTriggerType)0x05)

#define DEM_CAPTURE_ASYNCHRONOUS_TO_REPORTING ((Dem_EnvironmentDataCaptureType)0x00)
#define DEM_CAPTURE_SYNCHRONOUS_TO_REPORTING ((Dem_EnvironmentDataCaptureType)0x01)

#define DEM_DISPLACEMENT_FULL ((Dem_EventDisplacementStrategyType)0x00)
#define DEM_DISPLACEMENT_NONE ((Dem_EventDisplacementStrategyType)0x01)
#define DEM_DISPLACEMENT_PRIO_OCC ((Dem_EventDisplacementStrategyType)0x02)

#define DEM_PROCESS_OCCCTR_CDTC ((Dem_OccurrenceCounterProcessingType)0x01)
#define DEM_PROCESS_OCCCTR_TF ((Dem_OccurrenceCounterProcessingType)0x00)

#define DEM_DEBOUNCE_COUNTER_BASED ((Dem_DebounceAlgorithmClassType)0x00)
#define DEM_DEBOUNCE_MONITOR_INTERNAL ((Dem_DebounceAlgorithmClassType)0x01)
#define DEM_DEBOUNCE_TIME_BASED ((Dem_DebounceAlgorithmClassType)0x02)

#define DEM_DEBOUNCE_FREEZE ((Dem_DebounceBehaviorType)0x00)
#define DEM_DEBOUNCE_RESET ((Dem_DebounceBehaviorType)0x01)

#define DEM_FF_RECNUM_CALCULATED ((Dem_TypeOfFreezeFrameRecordNumerationType)0x00)
#define DEM_FF_RECNUM_CONFIGURED ((Dem_TypeOfFreezeFrameRecordNumerationType)0x01)

#define DEM_INVALID_EVENT_ID ((Dem_EventIdType)0xFFFF)

#define Dem_UdsBitClear(UdsByte, bit)                                                              \
  do {                                                                                             \
    UdsByte &= ~(bit);                                                                             \
  } while (0)

#define Dem_UdsBitSet(UdsByte, bit)                                                                \
  do {                                                                                             \
    UdsByte |= (bit);                                                                              \
  } while (0)
/* ================================ [ TYPES     ] ============================================== */
typedef uint8_t Dem_ActionTriggerType;

/* @ECUC_Dem_00797 */
typedef Dem_ActionTriggerType Dem_EventMemoryEntryStorageTriggerType;

/* @ECUC_Dem_00895 */
typedef uint8_t Dem_EnvironmentDataCaptureType;

/* @ECUC_Dem_00742 */
typedef uint8_t Dem_EventDisplacementStrategyType;

/* @ECUC_Dem_00767 */
typedef uint8_t Dem_OccurrenceCounterProcessingType;

/* @ECUC_Dem_00803 */
typedef Dem_ActionTriggerType Dem_FreezeFrameRecordTriggerType;

/* @ECUC_Dem_00804 */
typedef Dem_ActionTriggerType Dem_ExtendedDataRecordTriggerType;

/* @ECUC_Dem_00901 */
typedef struct {
  Dem_EnvironmentDataCaptureType EnvironmentDataCapture;
  Dem_EventDisplacementStrategyType EventDisplacementStrategy;
  Dem_EventMemoryEntryStorageTriggerType EventMemoryEntryStorageTrigger;
  uint8_t MaxNumberEventEntryPrimary;
  Dem_OccurrenceCounterProcessingType OccurrenceCounterProcessing;
} Dem_PrimaryMemoryType;

typedef Std_ReturnType (*Dem_GetDataFncType)(Dem_EventIdType, uint8_t *);

typedef struct {
  Dem_GetDataFncType GetFrezeFrameDataFnc;
  uint16_t id;
  uint16_t length;
#ifdef USE_SHELL
  void (*print)(uint8_t *);
#endif
} Dem_FreeFrameDataConfigType;

typedef struct {
  Dem_GetDataFncType GetExtendedDataFnc;
  uint8_t ExtendedDataNumber;
  uint8_t length;
} Dem_ExtendedDataConfigType;

typedef struct {
  Dem_UdsStatusByteType status;
  uint8_t testFailedCounter;
  uint8_t faultOccuranceCounter;
  uint8_t agingCounter;
  uint8_t agedCounter;
} Dem_EventStatusRecordType;

typedef struct {
  Dem_EventIdType EventId; /* is DEM_INVALID_EVENT_ID if this slot if free */
                           /* FreezeFrameData[][0] is reserved for record Number */
  uint8_t FreezeFrameData[DEM_MAX_FREEZE_FRAME_NUMBER][DEM_MAX_FREEZE_FRAME_DATA_SIZE];
} Dem_FreezeFrameRecordType;

typedef struct {
  uint16_t *FreezeFrameDataIDs;
  uint16_t numOfFreezeFrameDataIDs;
} Dem_EventFreezeFrameConfigType;

/* @ECUC_Dem_00604 */
typedef uint8_t Dem_DebounceAlgorithmClassType;

/* @ECUC_Dem_00786 */
typedef uint8_t Dem_DebounceBehaviorType;

/* @ECUC_Dem_00711 */
typedef struct {
  Dem_DebounceBehaviorType DebounceBehavior;
  sint16_t DebounceCounterDecrementStepSize;
  sint16_t DebounceCounterFailedThreshold;
  sint16_t DebounceCounterIncrementStepSize;
  boolean DebounceCounterJumpDown;
  sint16_t DebounceCounterJumpDownValue;
  boolean DebounceCounterJumpUp;
  sint16_t DebounceCounterJumpUpValue;
  sint16_t DebounceCounterPassedThreshold;
} Dem_DebounceCounterBasedConfigType;

typedef struct {
  const uint16_t *freezeFrameDataIndex;
  uint8_t numOfFreezeFrameData;
} Dem_FreezeFrameRecordClassType;

typedef struct {
  const uint8_t *ExtendedDataNumberIndex;
  uint8_t numOfExtendedData;
} Dem_ExtendedDataClassType;

/* @ECUC_Dem_00778 */
typedef uint8_t Dem_TypeOfFreezeFrameRecordNumerationType;

/* @ECUC_Dem_00776 */
typedef struct {
  const uint8_t *FreezeFrameRecNums; /* 1..254, 0xFF reserved for read all records */
  uint8_t numOfFreezeFrameRecNums;
} Dem_FreezeFrameRecNumClassType;

/* @ECUC_Dem_00641 */
typedef struct {
  boolean AgingAllowed;
  uint8_t AgingCycleCounterThreshold;
  uint8_t OperationCycleRef;
  uint8_t EventConfirmationThreshold;
  Dem_OccurrenceCounterProcessingType OccurrenceCounterProcessing;
  Dem_FreezeFrameRecordTriggerType FreezeFrameRecordTrigger;
  Dem_DebounceAlgorithmClassType DebounceAlgorithmClass;
  const Dem_DebounceCounterBasedConfigType *DebounceCounterBased;
  const Dem_ExtendedDataClassType *ExtendedDataClass;
  const Dem_FreezeFrameRecordClassType *FreezeFrameRecordClass;
} Dem_DTCAttributesType;

typedef struct {
  uint32_t DtcNumber;
  Dem_EventStatusRecordType *EventStatusRecords;
  const Dem_FreezeFrameRecNumClassType *FreezeFrameRecNumClass;
  const Dem_DTCAttributesType *DTCAttributes;
  uint8_t Priority; /* A lower value means higher priority. @ECUC_Dem_00662 */
#ifdef DEM_USE_NVM
  uint16_t NvmBlockId;
#endif
#ifdef DEM_USE_ENABLE_CONDITION
  uint32_t ConditionRefMask;
#endif
} Dem_EventConfigType;

typedef struct {
  Dem_EventStatusType status;
  sint16_t debouneCounter;
} Dem_EventContextType;

struct Dem_Config_s {
  const Dem_FreeFrameDataConfigType *FreeFrameDataConfigs;
  uint16_t numOfFreeFrameDataConfigs;
  const Dem_ExtendedDataConfigType *ExtendedDataConfigs;
  uint8_t numOfExtendedDataConfigs;
  /* A FreezeFrame pool to be used to store the most important DTCs'
   * snapshot when the DTC occurred. */
  Dem_FreezeFrameRecordType *const *const FreezeFrameRecords;
#ifdef DEM_USE_NVM
  const uint16_t *FreezeFrameNvmBlockIds;
#endif
  uint16_t numOfFreezeFrameRecords;
  const Dem_EventConfigType *EventConfigs;
  Dem_EventContextType *EventContexts;
  uint16_t numOfEvents;
  Dem_OperationCycleStateType *OperationCycleStates;
  uint8_t numOfOperationCycles;
  Dem_TypeOfFreezeFrameRecordNumerationType TypeOfFreezeFrameRecordNumeration;
};

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* standard EXTENDED DATA supported by DEM  */
Std_ReturnType Dem_EXTD_GetFaultOccuranceCounter(Dem_EventIdType EventId, uint8_t *data);
Std_ReturnType Dem_EXTD_GetAgingCounter(Dem_EventIdType EventId, uint8_t *data);
Std_ReturnType Dem_EXTD_GetAgedCounter(Dem_EventIdType EventId, uint8_t *data);

Std_ReturnType Dem_GetExtendedDataByNumber(Dem_EventIdType EventId, uint8_t Number, uint8_t *data,
                                           uint8_t size);
#endif /* DEM_PRIV_H */
