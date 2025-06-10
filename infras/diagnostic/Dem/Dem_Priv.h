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
#define DET_THIS_MODULE_ID MODULE_ID_DEM

#ifdef DEM_STATUS_BIT_STORAGE_TEST_FAILED /* @SWS_Dem_00525 @ECUC_Dem_00784 */
#define DEM_NVM_STORE_TEST_FAILED DEM_UDS_STATUS_TF
#else
#define DEM_NVM_STORE_TEST_FAILED 0
#endif

/* @SWS_Dem_01183 @SWS_Dem_00391 @SWS_Dem_00392 @SWS_Dem_00393 @SWS_Dem_00395 */
#define DEM_NVM_STORE_CARED_BITS                                                                   \
  (DEM_UDS_STATUS_PDTC | DEM_UDS_STATUS_CDTC | DEM_UDS_STATUS_TNCSLC | DEM_UDS_STATUS_TFSLC |      \
   DEM_UDS_STATUS_WIR | DEM_NVM_STORE_TEST_FAILED)

#define DEM_TRIGGER_ON_TEST_FAILED ((Dem_ActionTriggerType)0x00)
#define DEM_TRIGGER_ON_CONFIRMED ((Dem_ActionTriggerType)0x01)
#define DEM_TRIGGER_ON_FDC_THRESHOLD ((Dem_ActionTriggerType)0x02)
#define DEM_TRIGGER_ON_MIRROR ((Dem_ActionTriggerType)0x03)
#define DEM_TRIGGER_ON_PASSED ((Dem_ActionTriggerType)0x04)
#define DEM_TRIGGER_ON_PENDING ((Dem_ActionTriggerType)0x05)
#define DEM_TRIGGER_ON_EVERY_TEST_FAILED ((Dem_ActionTriggerType)0x06)

#define DEM_CAPTURE_ASYNCHRONOUS_TO_REPORTING ((Dem_EnvironmentDataCaptureType)0x00)
#define DEM_CAPTURE_SYNCHRONOUS_TO_REPORTING ((Dem_EnvironmentDataCaptureType)0x01)

#define DEM_DISPLACEMENT_FULL ((Dem_EventDisplacementStrategyType)0x00)
#define DEM_DISPLACEMENT_NONE ((Dem_EventDisplacementStrategyType)0x01)
#define DEM_DISPLACEMENT_PRIO_OCC ((Dem_EventDisplacementStrategyType)0x02)

#define DEM_PROCESS_OCCCTR_CDTC ((Dem_OccurrenceCounterProcessingType)0x00)
#define DEM_PROCESS_OCCCTR_TF ((Dem_OccurrenceCounterProcessingType)0x01)
#define DEM_PROCESS_OCCCTR_TFTOC ((Dem_OccurrenceCounterProcessingType)0x02)

#define DEM_DEBOUNCE_COUNTER_BASED ((Dem_DebounceAlgorithmClassType)0x00)
#define DEM_DEBOUNCE_MONITOR_INTERNAL ((Dem_DebounceAlgorithmClassType)0x01)
#define DEM_DEBOUNCE_TIME_BASED ((Dem_DebounceAlgorithmClassType)0x02)

#define DEM_DEBOUNCE_FREEZE ((Dem_DebounceBehaviorType)0x00)
#define DEM_DEBOUNCE_RESET ((Dem_DebounceBehaviorType)0x01)

#define DEM_FF_RECNUM_CALCULATED ((Dem_TypeOfFreezeFrameRecordNumerationType)0x00)
#define DEM_FF_RECNUM_CONFIGURED ((Dem_TypeOfFreezeFrameRecordNumerationType)0x01)

#define DEM_NV_STORE_DURING_SHUTDOWN ((Dem_NvStorageStrategyType)0x00)
#define DEM_NV_STORE_IMMEDIATE_AT_FIRST_OCCURRENCE ((Dem_NvStorageStrategyType)0x01)

#define DEM_INVALID_EVENT_ID ((Dem_EventIdType)0xFFFF)
#define DEM_INVALID_DTC_ID ((Dem_DtcIdType)0xFFFF)

#define DEM_UPDATE_RECORD_NO ((uint8_t)0)
#define DEM_UPDATE_RECORD_YES ((uint8_t)1)

/* @ECUC_Dem_00784 */
#define DEM_STATUS_BIT_AGING_AND_DISPLACEMENT 1
#define DEM_STATUS_BIT_NORMAL 0 /* default */

#define Dem_UdsBitClear(UdsByte, bit)                                                              \
  do {                                                                                             \
    UdsByte &= ~(bit);                                                                             \
  } while (0)

#define Dem_UdsBitSet(UdsByte, bit)                                                                \
  do {                                                                                             \
    UdsByte |= (bit);                                                                              \
  } while (0)

/* BIT 0~3: there is a request to capture a freeze frame */
#define DEM_EVENT_FLAG_CAPTURE_FF ((Dem_EventFlagType)0x01)

/* BIT 4~7: there is a request to capture a extended data */
#define DEM_EVENT_FLAG_CAPTURE_EE ((Dem_EventFlagType)0x10)

#define DEM_CYCLE_COUNTER_STOPPED 0xFF
#define DEM_CYCLE_COUNTER_MAX 0xFE
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

/* @ECUC_Dem_00127 */
typedef uint8_t Dem_NvStorageStrategyType;

/* @ECUC_Dem_00901 */
typedef struct {
  Dem_EnvironmentDataCaptureType EnvironmentDataCapture;
  Dem_EventDisplacementStrategyType EventDisplacementStrategy;
  Dem_EventMemoryEntryStorageTriggerType EventMemoryEntryStorageTrigger;
  uint8_t MaxNumberEventEntryPrimary;
  Dem_OccurrenceCounterProcessingType OccurrenceCounterProcessing;
} Dem_PrimaryMemoryType;

typedef Std_ReturnType (*Dem_GetDataFncType)(Dem_EventIdType, uint8_t *, Dem_DTCOriginType);

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
  uint8_t faultOccuranceCounter;
  uint8_t agingCounter;
  uint8_t agedCounter;
#ifdef DEM_USE_CYCLES_SINCE_LAST_FAILED
  uint8_t cyclesSinceLastFailed; /* @SWS_Dem_00984: let's define 0xFF as not started, so the real
                                    max value is 0xFE */
#endif
} Dem_DtcStatusRecordType;

typedef struct {
  Dem_DtcIdType DtcId; /* is DEM_INVALID_DTC_ID if this slot if free */
                       /* FreezeFrameData[][0] is reserved for record Number */
  uint8_t FreezeFrameData[DEM_MAX_FREEZE_FRAME_NUMBER][DEM_MAX_FREEZE_FRAME_DATA_SIZE];
} Dem_FreezeFrameRecordType;

typedef struct {
  Dem_DtcIdType DtcId; /* is DEM_INVALID_DTC_ID if this slot if free */
                       /* ExtendedData[0] is reserved for record Number */
  uint8_t ExtendedData[DEM_MAX_EXTENDED_DATA_SIZE];
} Dem_ExtendedDataRecordType;

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
  P2CONST(uint16_t, AUTOMATIC, DEM_CONST) freezeFrameDataIndex;
  uint8_t numOfFreezeFrameData;
} Dem_FreezeFrameRecordClassType;

typedef struct {
  P2CONST(uint8_t, AUTOMATIC, DEM_CONST) ExtendedDataNumberIndex;
  uint8_t numOfExtendedData;
  uint8_t ExtendedDataRecordNumber;
} Dem_ExtendedDataRecordClassType;

typedef struct {
  P2CONST(Dem_ExtendedDataRecordClassType *const, AUTOMATIC, DEM_CONST) ExtendedDataRecordClassRef;
  uint8_t numOfExtendedDataRecordClassRef;
} Dem_ExtendedDataClassType;

/* @ECUC_Dem_00778 */
typedef uint8_t Dem_TypeOfFreezeFrameRecordNumerationType;

/* @ECUC_Dem_00776 */
typedef struct {
  /* 1..254, 0xFF reserved for read all records */
  P2CONST(uint8_t, AUTOMATIC, DEM_CONST) FreezeFrameRecNums;
  uint8_t numOfFreezeFrameRecNums;
} Dem_FreezeFrameRecNumClassType;

typedef struct {
  CONSTP2VAR(Dem_DtcStatusRecordType *const, AUTOMATIC, DEM_CONST) StatusRecords;
  /* A FreezeFrame pool to be used to store the most important DTCs'
   * snapshot when the DTC occurred. */
  CONSTP2VAR(Dem_FreezeFrameRecordType *const, AUTOMATIC, DEM_CONST) FreezeFrameRecords;
#ifdef DEM_USE_NVM_EXTENDED_DATA
  CONSTP2VAR(Dem_ExtendedDataRecordType *const, AUTOMATIC, DEM_CONST) ExtendedDataRecords;
#endif
#ifndef DEM_USE_NVM
  uint8_t *StatusRecordsDirty;
  uint8_t *FreezeFrameRecordsDirty;
#ifdef DEM_USE_NVM_EXTENDED_DATA
  uint8_t *ExtendedDataRecordsDirty;
#endif
#else
  P2CONST(uint16_t, AUTOMATIC, DEM_CONST) StatusNvmBlockIds;
  P2CONST(uint16_t, AUTOMATIC, DEM_CONST) FreezeFrameNvmBlockIds;
#ifdef DEM_USE_NVM_EXTENDED_DATA
  P2CONST(uint16_t, AUTOMATIC, DEM_CONST) ExtendedDataNvmBlockIds;
#endif
#endif
  uint16_t numOfStatusRecords;
  uint16_t numOfFreezeFrameRecords;
#ifdef DEM_USE_NVM_EXTENDED_DATA
  uint16_t numOfExtendedDataRecords;
#endif
  Dem_DTCOriginType DTCOrigin;
} Dem_MemoryDestinationType;

/* @ECUC_Dem_00641 */
typedef struct {
  P2CONST(Dem_ExtendedDataClassType, AUTOMATIC, DEM_CONST) ExtendedDataClass;
  P2CONST(Dem_FreezeFrameRecordClassType, AUTOMATIC, DEM_CONST) FreezeFrameRecordClass;
  P2CONST(Dem_FreezeFrameRecNumClassType, AUTOMATIC, DEM_CONST) FreezeFrameRecNumClass;
  /* @ECUC_Dem_00890 */
  P2CONST(Dem_MemoryDestinationType *const, AUTOMATIC, DEM_CONST) MemoryDestination;
  uint8_t numOfMemoryDestination; /* 1 or 2 */
  uint8_t Priority;               /* A lower value means higher priority. @ECUC_Dem_00662 */
  boolean AgingAllowed;
  uint8_t AgingCycleCounterThreshold;
  Dem_OccurrenceCounterProcessingType OccurrenceCounterProcessing;
  Dem_FreezeFrameRecordTriggerType FreezeFrameRecordTrigger;
#ifdef DEM_USE_NVM_EXTENDED_DATA
  Dem_ExtendedDataRecordTriggerType ExtendedDataRecordTrigger;
#endif
  Dem_DebounceAlgorithmClassType DebounceAlgorithmClass;
  Dem_EnvironmentDataCaptureType EnvironmentDataCapture;
} Dem_DTCAttributesType;

/* @SWS_Dem_00256 */
typedef Std_ReturnType (*Dem_CallbackInitMonitorForEventFncType)(
  Dem_InitMonitorReasonType InitMonitorReason);

/* @ECUC_Dem_00886 */
typedef struct {
  P2CONST(Dem_DTCAttributesType, AUTOMATIC, DEM_CONST) DTCAttributes;
  P2CONST(Dem_EventIdType, AUTOMATIC, DEM_CONST) EventIdRefs;
  uint32_t DtcNumber; /* @ECUC_Dem_00887 */
  Dem_DtcIdType DtcId;
  uint8_t numOfEvents;
#if 0
  uint8_t FunctionalUnit;
  Dem_DTCSeverityType Severity;
  Dem_NvStorageStrategyType NvStorageStrategy;
#endif
} Dem_DTCType;

/* @ECUC_Dem_00661 */
typedef struct {
  P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) DTCRef;
  Dem_CallbackInitMonitorForEventFncType DemCallbackInitMForE;
  P2CONST(Dem_DebounceCounterBasedConfigType, AUTOMATIC, DEM_CONST) DebounceCounterBased;
#ifdef DEM_USE_ENABLE_CONDITION
  uint32_t ConditionRefMask; /* @ECUC_Dem_00746 */
#endif
  uint8_t ConfirmationThreshold; /* @ECUC_Dem_00924 */
  uint8_t OperationCycleRef;     /* @ECUC_Dem_00702 */
  boolean RecoverableInSameOperationCycle;
} Dem_EventConfigType;

typedef uint8_t Dem_EventFlagType;

typedef struct {
  Dem_EventStatusType status;
  Dem_EventFlagType flag;
  sint16_t debouneCounter;
} Dem_EventContextType;

typedef struct {
  Dem_UdsStatusByteType status;
  uint8_t testFailedCounter;
} Dem_EventStatusRecordType;

struct Dem_Config_s {
  P2CONST(Dem_FreeFrameDataConfigType, AUTOMATIC, DEM_CONST) FreeFrameDataConfigs;
  P2CONST(Dem_ExtendedDataConfigType, AUTOMATIC, DEM_CONST) ExtendedDataConfigs;
  P2CONST(Dem_EventConfigType, AUTOMATIC, DEM_CONST) EventConfigs;
  Dem_EventContextType *EventContexts;
  CONSTP2VAR(Dem_EventStatusRecordType *const, AUTOMATIC, DEM_CONST) EventStatusRecords;
#ifdef DEM_USE_NVM
  P2CONST(uint16_t, AUTOMATIC, DEM_CONST) EventStatusNvmBlockIds;
#else
  uint8_t *EventStatusDirty; /* to indicate the Event status record has updates */
#endif
  P2CONST(Dem_DTCType, AUTOMATIC, DEM_CONST) Dtcs;
  P2CONST(Dem_MemoryDestinationType, AUTOMATIC, DEM_CONST) MemoryDestination;
  Dem_OperationCycleStateType *OperationCycleStates;
  uint16_t numOfFreeFrameDataConfigs;
  uint16_t numOfEvents;
  uint16_t numOfDtcs;
  uint8_t numOfMemoryDestination;
  uint8_t numOfExtendedDataConfigs;
  uint8_t numOfOperationCycles;
  Dem_TypeOfFreezeFrameRecordNumerationType TypeOfFreezeFrameRecordNumeration;
  uint8_t StatusAvailabilityMask;
};

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* standard EXTENDED DATA supported by DEM  */
Std_ReturnType Dem_EXTD_GetFaultOccuranceCounter(Dem_EventIdType EventId, uint8_t *data,
                                                 Dem_DTCOriginType DTCOrigin);
Std_ReturnType Dem_EXTD_GetAgingCounter(Dem_EventIdType EventId, uint8_t *data,
                                        Dem_DTCOriginType DTCOrigin);
Std_ReturnType Dem_EXTD_GetAgedCounter(Dem_EventIdType EventId, uint8_t *data,
                                       Dem_DTCOriginType DTCOrigin);
#endif /* DEM_PRIV_H */
