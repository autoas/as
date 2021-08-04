/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Generated at Fri Jul 30 09:13:24 2021
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dem_Priv.h"
#include "NvM_Cfg.h"
/* ================================ [ MACROS    ] ============================================== */
#define DEM_FFD_Battery 0
#define DEM_FFD_VehileSpeed 1
#define DEM_FFD_EngineSpeed 2
#define DEM_FFD_Time 3

#define DEM_EXTD_FaultOccuranceCounter 0
#define DEM_EXTD_AgingCounter 1
#define DEM_EXTD_AgedCounter 2

#define DEM_EXTD_FaultOccuranceCounter_NUMBER 0x1
#define DEM_EXTD_AgingCounter_NUMBER 0x2
#define DEM_EXTD_AgedCounter_NUMBER 0x3
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
Std_ReturnType Dem_FFD_GetBattery(Dem_EventIdType EventId, uint8_t *data);
Std_ReturnType Dem_FFD_GetVehileSpeed(Dem_EventIdType EventId, uint8_t *data);
Std_ReturnType Dem_FFD_GetEngineSpeed(Dem_EventIdType EventId, uint8_t *data);
Std_ReturnType Dem_FFD_GetTime(Dem_EventIdType EventId, uint8_t *data);

Std_ReturnType Dem_EXTD_GetFaultOccuranceCounter(Dem_EventIdType EventId, uint8_t *data);
Std_ReturnType Dem_EXTD_GetAgingCounter(Dem_EventIdType EventId, uint8_t *data);
Std_ReturnType Dem_EXTD_GetAgedCounter(Dem_EventIdType EventId, uint8_t *data);

extern Dem_EventStatusRecordType Dem_NvmEventStatusRecord0_Ram; /* DTC0 */
extern Dem_EventStatusRecordType Dem_NvmEventStatusRecord1_Ram; /* DTC1 */
extern Dem_EventStatusRecordType Dem_NvmEventStatusRecord2_Ram; /* DTC2 */
extern Dem_EventStatusRecordType Dem_NvmEventStatusRecord3_Ram; /* DTC3 */
extern Dem_EventStatusRecordType Dem_NvmEventStatusRecord4_Ram; /* DTC4 */
#if DEM_MAX_FREEZE_FRAME_RECORD > 0
extern Dem_FreezeFrameRecordType Dem_NvmFreezeFrameRecord0_Ram;
#endif
#if DEM_MAX_FREEZE_FRAME_RECORD > 1
extern Dem_FreezeFrameRecordType Dem_NvmFreezeFrameRecord1_Ram;
#endif
#if DEM_MAX_FREEZE_FRAME_RECORD > 2
extern Dem_FreezeFrameRecordType Dem_NvmFreezeFrameRecord2_Ram;
#endif
#if DEM_MAX_FREEZE_FRAME_RECORD > 3
extern Dem_FreezeFrameRecordType Dem_NvmFreezeFrameRecord3_Ram;
#endif
#if DEM_MAX_FREEZE_FRAME_RECORD > 4
extern Dem_FreezeFrameRecordType Dem_NvmFreezeFrameRecord4_Ram;
#endif
/* ================================ [ DATAS     ] ============================================== */
static Dem_FreezeFrameRecordType* const Dem_NvmFreezeFrameRecord[DEM_MAX_FREEZE_FRAME_RECORD] = {
#if DEM_MAX_FREEZE_FRAME_RECORD > 0
  &Dem_NvmFreezeFrameRecord0_Ram,
#endif
#if DEM_MAX_FREEZE_FRAME_RECORD > 1
  &Dem_NvmFreezeFrameRecord1_Ram,
#endif
#if DEM_MAX_FREEZE_FRAME_RECORD > 2
  &Dem_NvmFreezeFrameRecord2_Ram,
#endif
#if DEM_MAX_FREEZE_FRAME_RECORD > 3
  &Dem_NvmFreezeFrameRecord3_Ram,
#endif
#if DEM_MAX_FREEZE_FRAME_RECORD > 4
  &Dem_NvmFreezeFrameRecord4_Ram,
#endif
};

#ifdef DEM_USE_NVM
static const uint16_t Dem_NvmFreezeFrameNvmBlockIds[DEM_MAX_FREEZE_FRAME_RECORD] = {
#if DEM_MAX_FREEZE_FRAME_RECORD > 0
  NVM_BLOCKID_Dem_NvmFreezeFrameRecord0,
#endif
#if DEM_MAX_FREEZE_FRAME_RECORD > 1
  NVM_BLOCKID_Dem_NvmFreezeFrameRecord1,
#endif
#if DEM_MAX_FREEZE_FRAME_RECORD > 2
  NVM_BLOCKID_Dem_NvmFreezeFrameRecord2,
#endif
#if DEM_MAX_FREEZE_FRAME_RECORD > 3
  NVM_BLOCKID_Dem_NvmFreezeFrameRecord3,
#endif
#if DEM_MAX_FREEZE_FRAME_RECORD > 4
  NVM_BLOCKID_Dem_NvmFreezeFrameRecord4,
#endif
};
#endif

static const Dem_FreeFrameDataConfigType FreeFrameDataConfigs[] = {
  {Dem_FFD_GetBattery, 0x1001, 2},
  {Dem_FFD_GetVehileSpeed, 0x1002, 2},
  {Dem_FFD_GetEngineSpeed, 0x1003, 2},
  {Dem_FFD_GetTime, 0x1004, 6},
};

static const Dem_ExtendedDataConfigType ExtendedDataConfigs[] = {
  {Dem_EXTD_GetFaultOccuranceCounter, DEM_EXTD_FaultOccuranceCounter_NUMBER, 1},
  {Dem_EXTD_GetAgingCounter, DEM_EXTD_AgingCounter_NUMBER, 1},
  {Dem_EXTD_GetAgedCounter, DEM_EXTD_AgedCounter_NUMBER, 1},
};

static const Dem_DebounceCounterBasedConfigType Dem_DebounceCounterBasedDefault = {
  DEM_DEBOUNCE_FREEZE,
  /* DebounceCounterDecrementStepSize */ 2,
  /* DebounceCounterFailedThreshold */ 10,
  /* DebounceCounterIncrementStepSize */ 1,
  /* DebounceCounterJumpDown */ FALSE,
  /* DebounceCounterJumpDownValue */ 0,
  /* DebounceCounterJumpUp */ TRUE,
  /* DebounceCounterJumpUpValue */ 0,
  /* DebounceCounterPassedThreshold */ -10,
};

/* each Event can have different environment data that cares about */
static const uint16_t Dem_FreezeFrameDataIndexDefault[] = {
  DEM_FFD_Battery,
  DEM_FFD_VehileSpeed,
  DEM_FFD_EngineSpeed,
  DEM_FFD_Time,
};

/* each Event can have different extended data that cares about*/
static const uint8_t Dem_ExtendedDataNumberIndexDefault[] = {
  DEM_EXTD_FaultOccuranceCounter,
  DEM_EXTD_AgingCounter,
  DEM_EXTD_AgedCounter,
};

static const Dem_FreezeFrameRecordClassType Dem_FreezeFrameRecordClassDefault = {
  Dem_FreezeFrameDataIndexDefault,
  ARRAY_SIZE(Dem_FreezeFrameDataIndexDefault),
};

static const Dem_ExtendedDataClassType Dem_ExtendedDataClassDefault = {
  Dem_ExtendedDataNumberIndexDefault,
  ARRAY_SIZE(Dem_ExtendedDataNumberIndexDefault),
};

static const uint8_t Dem_FreezeFrameRecNumsForDTC0[] = {1, 2};
static const uint8_t Dem_FreezeFrameRecNumsForDTC1[] = {3, 4};
static const uint8_t Dem_FreezeFrameRecNumsForDTC2[] = {5, 6};
static const uint8_t Dem_FreezeFrameRecNumsForDTC3[] = {7, 8};
static const uint8_t Dem_FreezeFrameRecNumsForDTC4[] = {9, 10};
static const Dem_FreezeFrameRecNumClassType Dem_FreezeFrameRecNumClass[] = {
  {
    Dem_FreezeFrameRecNumsForDTC0,
    ARRAY_SIZE(Dem_FreezeFrameRecNumsForDTC0),
  },
  {
    Dem_FreezeFrameRecNumsForDTC1,
    ARRAY_SIZE(Dem_FreezeFrameRecNumsForDTC1),
  },
  {
    Dem_FreezeFrameRecNumsForDTC2,
    ARRAY_SIZE(Dem_FreezeFrameRecNumsForDTC2),
  },
  {
    Dem_FreezeFrameRecNumsForDTC3,
    ARRAY_SIZE(Dem_FreezeFrameRecNumsForDTC3),
  },
  {
    Dem_FreezeFrameRecNumsForDTC4,
    ARRAY_SIZE(Dem_FreezeFrameRecNumsForDTC4),
  },
};

static const Dem_DTCAttributesType Dem_DTCAttributesDefault = {
  /* AgingAllowed */ TRUE,
  /* AgingCycleCounterThreshold */ 2,
  /* OperationCycleRef */ DEM_OPERATION_CYCLE_IGNITION,
  /* EventConfirmationThreshold */ 1,
  /* OccurrenceCounterProcessing */ DEM_PROCESS_OCCCTR_TF,
  /* FreezeFrameRecordTrigger */ DEM_TRIGGER_ON_TEST_FAILED,
  /* DebounceAlgorithmClass */ DEM_DEBOUNCE_COUNTER_BASED,
  &Dem_DebounceCounterBasedDefault,
  &Dem_ExtendedDataClassDefault,
  &Dem_FreezeFrameRecordClassDefault,
};

static const Dem_EventConfigType EventConfigs[DTC_ENVENT_NUM] = {
  {
    0x112200,
    &Dem_NvmEventStatusRecord0_Ram,
    &Dem_FreezeFrameRecNumClass[0],
    &Dem_DTCAttributesDefault,
    0,
#ifdef DEM_USE_NVM
    NVM_BLOCKID_Dem_NvmEventStatusRecord0,
#endif
  },
  {
    0x112201,
    &Dem_NvmEventStatusRecord1_Ram,
    &Dem_FreezeFrameRecNumClass[1],
    &Dem_DTCAttributesDefault,
    1,
#ifdef DEM_USE_NVM
    NVM_BLOCKID_Dem_NvmEventStatusRecord1,
#endif
  },
  {
    0x112202,
    &Dem_NvmEventStatusRecord2_Ram,
    &Dem_FreezeFrameRecNumClass[2],
    &Dem_DTCAttributesDefault,
    2,
#ifdef DEM_USE_NVM
    NVM_BLOCKID_Dem_NvmEventStatusRecord2,
#endif
  },
  {
    0x112203,
    &Dem_NvmEventStatusRecord3_Ram,
    &Dem_FreezeFrameRecNumClass[3],
    &Dem_DTCAttributesDefault,
    3,
#ifdef DEM_USE_NVM
    NVM_BLOCKID_Dem_NvmEventStatusRecord3,
#endif
  },
  {
    0x112204,
    &Dem_NvmEventStatusRecord4_Ram,
    &Dem_FreezeFrameRecNumClass[4],
    &Dem_DTCAttributesDefault,
    4,
#ifdef DEM_USE_NVM
    NVM_BLOCKID_Dem_NvmEventStatusRecord4,
#endif
  },
};

static Dem_EventContextType EventContexts[DTC_ENVENT_NUM];
static Dem_OperationCycleStateType Dem_OperationCycleStates[2];
const Dem_ConfigType Dem_Config = {
  FreeFrameDataConfigs,
  ARRAY_SIZE(FreeFrameDataConfigs),
  ExtendedDataConfigs,
  ARRAY_SIZE(ExtendedDataConfigs),
  Dem_NvmFreezeFrameRecord,
#ifdef DEM_USE_NVM
  Dem_NvmFreezeFrameNvmBlockIds,
#endif
  ARRAY_SIZE(Dem_NvmFreezeFrameRecord),
  EventConfigs,
  EventContexts,
  ARRAY_SIZE(EventConfigs),
  Dem_OperationCycleStates,
  ARRAY_SIZE(Dem_OperationCycleStates),
  /* TypeOfFreezeFrameRecordNumeration */ DEM_FF_RECNUM_CONFIGURED,
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
