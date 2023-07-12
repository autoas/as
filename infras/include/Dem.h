/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Diagnostic Event Manager AUTOSAR CP Release 4.4.0
 */
#ifndef DEM_H
#define DEM_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* default supported operation cycle */
#define DEM_OPERATION_CYCLE_POWER 0
#define DEM_OPERATION_CYCLE_IGNITION 1

/* UDS status bit */
#define DEM_UDS_STATUS_TF ((Dem_UdsStatusByteType)0x01)
#define DEM_UDS_STATUS_TFTOC ((Dem_UdsStatusByteType)0x02)
#define DEM_UDS_STATUS_PDTC ((Dem_UdsStatusByteType)0x04)
#define DEM_UDS_STATUS_CDTC ((Dem_UdsStatusByteType)0x08)
#define DEM_UDS_STATUS_TNCSLC ((Dem_UdsStatusByteType)0x10)
#define DEM_UDS_STATUS_TFSLC ((Dem_UdsStatusByteType)0x20)
#define DEM_UDS_STATUS_TNCTOC ((Dem_UdsStatusByteType)0x40)
#define DEM_UDS_STATUS_WIR ((Dem_UdsStatusByteType)0x80)

#define DEM_EVENT_STATUS_PASSED ((Dem_EventStatusType)0x00)
#define DEM_EVENT_STATUS_FAILED ((Dem_EventStatusType)0x01)
#define DEM_EVENT_STATUS_PREPASSED ((Dem_EventStatusType)0x02)
#define DEM_EVENT_STATUS_PREFAILED ((Dem_EventStatusType)0x03)
#define DEM_EVENT_STATUS_FDC_THRESHOLD_REACHED ((Dem_EventStatusType)0x04)

#define DEM_EVENT_STATUS_UNKNOWN ((Dem_EventStatusType)0xFF)

#define DEM_DTC_FORMAT_OBD ((Dem_DTCFormatType)0x00)
#define DEM_DTC_FORMAT_UDS ((Dem_DTCFormatType)0x01)
#define DEM_DTC_FORMAT_J1939 ((Dem_DTCFormatType)0x02)

#define DEM_INIT_MONITOR_CLEAR ((Dem_InitMonitorReasonType)0x01)
#define DEM_INIT_MONITOR_RESTART ((Dem_InitMonitorReasonType)0x02)
#define DEM_INIT_MONITOR_REENABLED ((Dem_InitMonitorReasonType)0x03)
#define DEM_INIT_MONITOR_STORAGE_REENABLED ((Dem_InitMonitorReasonType)0x04)

#define DEM_MONITOR_STATUS_TF ((Dem_MonitorStatusType)0x01)     /* bit0 */
#define DEM_MONITOR_STATUS_TNCTOC ((Dem_MonitorStatusType)0x02) /* bit1 */

#define DEM_DTC_KIND_ALL_DTCS ((Dem_DTCKindType)0x01)
#define DEM_DTC_KIND_EMISSION_REL_DTCS ((Dem_DTCKindType)0x02)

#define DEM_FIRST_FAILED_DTC ((Dem_DTCRequestType)0x01)
#define DEM_MOST_RECENT_FAILED_DTC ((Dem_DTCRequestType)0x02)
#define DEM_FIRST_DET_CONFIRMED_DTC ((Dem_DTCRequestType)0x03)
#define DEM_MOST_REC_DET_CONFIRMED_DTC ((Dem_DTCRequestType)0x04)

#define DEM_SEVERITY_NO_SEVERITY ((Dem_DTCSeverityType)0x00)
#define DEM_SEVERITY_WWHOBD_CLASS_NO_CLASS ((Dem_DTCSeverityType)0x01)
#define DEM_SEVERITY_WWHOBD_CLASS_A ((Dem_DTCSeverityType)0x02)
#define DEM_SEVERITY_WWHOBD_CLASS_B1 ((Dem_DTCSeverityType)0x04)
#define DEM_SEVERITY_WWHOBD_CLASS_B2 ((Dem_DTCSeverityType)0x08)
#define DEM_SEVERITY_WWHOBD_CLASS_C ((Dem_DTCSeverityType)0x10)
#define DEM_SEVERITY_MAINTENANCE_ONLY ((Dem_DTCSeverityType)0x20)
#define DEM_SEVERITY_CHECK_AT_NEXT_HALT ((Dem_DTCSeverityType)0x40)
#define DEM_SEVERITY_CHECK_IMMEDIATELY ((Dem_DTCSeverityType)0x80)

#define DEM_DTC_ORIGIN_PRIMARY_MEMORY ((Dem_DTCOriginType)0x0001)
#define DEM_DTC_ORIGIN_MIRROR_MEMORY ((Dem_DTCOriginType)0x0002)
#define DEM_DTC_ORIGIN_PERMANENT_MEMORY ((Dem_DTCOriginType)0x0003)
#define DEM_DTC_ORIGIN_OBD_RELEVANT_MEMORY ((Dem_DTCOriginType)0x0004)

#define DEM_TEMPORARILY_DEFECTIVE ((Dem_DebouncingStateType)0x01)
#define DEM_FINALLY_DEFECTIVE ((Dem_DebouncingStateType)0x02)
#define DEM_TEMPORARILY_HEALED ((Dem_DebouncingStateType)0x04)
#define DEM_TEST_COMPLETE ((Dem_DebouncingStateType)0x05)
#define DEM_DTR_UPDATE ((Dem_DebouncingStateType)0x10)

#define DEM_DEBOUNCE_STATUS_FREEZE ((Dem_DebounceResetStatusType)0x00)
#define DEM_DEBOUNCE_STATUS_RESET ((Dem_DebounceResetStatusType)0x01)

#define DEM_OPERATION_CYCLE_STOPPED ((Dem_OperationCycleStateType)0)
#define DEM_OPERATION_CYCLE_STARTED ((Dem_OperationCycleStateType)1)

/* @SWS_Dem_00599 @SWS_Dem_00666 */
#define DEM_BUFFER_TOO_SMALL ((Std_ReturnType)21)
#define DEM_PENDING ((Std_ReturnType)4)
#define DEM_CLEAR_BUSY ((Std_ReturnType)5)
#define DEM_CLEAR_MEMORY_ERROR ((Std_ReturnType)6)
#define DEM_CLEAR_FAILED ((Std_ReturnType)7)
#define DEM_WRONG_DTC ((Std_ReturnType)8)
#define DEM_WRONG_DTCORIGIN ((Std_ReturnType)9)
#define DEM_E_NO_DTC_AVAILABLE ((Std_ReturnType)10)
#define DEM_E_NO_FDC_AVAILABLE ((Std_ReturnType)14)
#define DEM_BUSY ((Std_ReturnType)22)
#define DEM_NO_SUCH_ELEMENT ((Std_ReturnType)48)
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_Dem_00925 */
typedef uint16_t Dem_EventIdType;

typedef uint16_t Dem_DtcIdType;

/* @SWS_Dem_00926 */
typedef uint8_t Dem_EventStatusType;

/* @SWS_Dem_00928 */
typedef uint8_t Dem_UdsStatusByteType;

/* @SWS_Dem_00933 */
typedef uint8_t Dem_DTCFormatType;

/* @SWS_Dem_00942 */
typedef uint8_t Dem_InitMonitorReasonType;

/* @SWS_Dem_91036 */
typedef uint32_t Dem_MonitorDataType;

/* @SWS_Dem_91005 */
typedef uint8_t Dem_MonitorStatusType;

/* @SWS_Dem_00932 */
typedef uint8_t Dem_DTCKindType;

/* @SWS_Dem_00934 */
typedef uint16_t Dem_DTCOriginType;

/* @SWS_Dem_00935 */
typedef uint8_t Dem_DTCRequestType;

/* @SWS_Dem_00927 */
typedef uint8_t Dem_DebounceResetStatusType;

/* @SWS_Dem_00937 */
typedef uint8_t Dem_DTCSeverityType;

/* @SWS_Dem_01000 */
typedef uint8_t Dem_DebouncingStateType;

typedef uint8_t Dem_OperationCycleStateType;

/* @SWS_Dem_00924 */
typedef struct Dem_Config_s Dem_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_Dem_00179 */
void Dem_PreInit(void);
/* @SWS_Dem_00181 */
void Dem_Init(const Dem_ConfigType *ConfigPtr);
/* @SWS_Dem_00182 */
void Dem_Shutdown(void);

/* @SWS_Dem_00208 */
Std_ReturnType Dem_SetDTCFilter(uint8_t ClientId, uint8_t DTCStatusMask,
                                Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin,
                                boolean FilterWithSeverity, Dem_DTCSeverityType DTCSeverityMask,
                                boolean FilterForFaultDetectionCounter);

/* @SWS_Dem_00214 */
Std_ReturnType Dem_GetNumberOfFilteredDTC(uint8_t ClientId, uint16_t *NumberOfFilteredDTC);

/* @SWS_Dem_00215 */
Std_ReturnType Dem_GetNextFilteredDTC(uint8_t ClientId, uint32_t *DTC, uint8_t *DTCStatus);

/* @SWS_Dem_00227 */
Std_ReturnType Dem_GetNextFilteredDTCAndFDC(uint8_t ClientId, uint32_t *DTC,
                                            sint8_t *DTCFaultDetectionCounter);

/* @SWS_Dem_00281 */
Std_ReturnType Dem_GetNextFilteredDTCAndSeverity(uint8_t ClientId, uint32_t *DTC,
                                                 uint8_t *DTCStatus,
                                                 Dem_DTCSeverityType *DTCSeverity,
                                                 uint8_t *DTCFunctionalUnit);

/* @SWS_Dem_00209 */
Std_ReturnType Dem_SetFreezeFrameRecordFilter(uint8_t ClientId, Dem_DTCFormatType DTCFormat);

/* @SWS_Dem_00224 */
Std_ReturnType Dem_GetNextFilteredRecord(uint8_t ClientId, uint32_t *DTC, uint8_t *RecordNumber);

/* @SWS_Dem_00218 */
Std_ReturnType Dem_GetDTCByOccurrenceTime(uint8_t ClientId, Dem_DTCRequestType DTCRequest,
                                          uint32_t *DTC);

/* @SWS_Dem_91016 */
Std_ReturnType Dem_SelectDTC(uint8_t ClientId, uint32_t DTC, Dem_DTCFormatType DTCFormat,
                             Dem_DTCOriginType DTCOrigin);

/* @SWS_Dem_00665 */
Std_ReturnType Dem_ClearDTC(uint8_t ClientId);

/* @SWS_Dem_00242 */
Std_ReturnType Dem_DisableDTCSetting(uint8_t ClientId);
/* @SWS_Dem_00243 */
Std_ReturnType Dem_EnableDTCSetting(uint8_t ClientId);

/* @SWS_Dem_00233 */
Std_ReturnType Dem_DisableDTCRecordUpdate(uint8_t ClientId);
/* @SWS_Dem_00234 */
Std_ReturnType Dem_EnableDTCRecordUpdate(uint8_t ClientId);

/* @SWS_Dem_00240 */
Std_ReturnType Dem_GetSizeOfExtendedDataRecordSelection(uint8_t ClientId,
                                                        uint16_t *SizeOfExtendedDataRecord);

/* @SWS_Dem_00238 */
Std_ReturnType Dem_GetSizeOfFreezeFrameSelection(uint8_t ClientId, uint16_t *SizeOfFreezeFrame);

/* @SWS_Dem_00239 */
Std_ReturnType Dem_GetNextExtendedDataRecord(uint8_t ClientId, uint8_t *DestBuffer,
                                             uint16_t *BufSize);

/* @SWS_Dem_00236 */
Std_ReturnType Dem_GetNextFreezeFrameData(uint8_t ClientId, uint8_t *DestBuffer, uint16_t *BufSize);

/* @SWS_Dem_91017 */
Std_ReturnType Dem_SelectExtendedDataRecord(uint8_t ClientId, uint8_t ExtendedDataNumber);

/* @SWS_Dem_91015 */
Std_ReturnType Dem_SelectFreezeFrameData(uint8_t ClientId, uint8_t RecordNumber);

/* @SWS_Dem_91191 */
Std_ReturnType Dem_GetNumberOfFreezeFrameRecords(uint8_t ClientId,
                                                 uint16_t *NumberOfFilteredRecords);

/* @SWS_Dem_00683 */
Std_ReturnType Dem_ResetEventDebounceStatus(Dem_EventIdType EventId,
                                            Dem_DebounceResetStatusType DebounceResetStatus);

/* @SWS_Dem_00679 */
Std_ReturnType Dem_SetOperationCycleState(uint8_t operationCycleId,
                                          Dem_OperationCycleStateType cycleState);

/* @SWS_Dem_00194 */
Std_ReturnType Dem_RestartOperationCycle(uint8_t OperationCycleId);

/* @SWS_Dem_00183 */
Std_ReturnType Dem_SetEventStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus);

/* @SWS_Dem_91037 */
Std_ReturnType Dem_SetEventStatusWithMonitorData(Dem_EventIdType EventId,
                                                 Dem_EventStatusType EventStatus,
                                                 Dem_MonitorDataType monitorData0,
                                                 Dem_MonitorDataType monitorData1);

/* @SWS_Dem_00556 */
Std_ReturnType Dem_SetStorageCondition(uint8_t StorageConditionID, boolean ConditionFulfilled);

/* @SWS_Dem_00185 */
Std_ReturnType Dem_ResetEventStatus(Dem_EventIdType EventId);

/* @SWS_Dem_00188 */
Std_ReturnType Dem_PrestoreFreezeFrame(Dem_EventIdType EventId);

/* @SWS_Dem_00201 */
Std_ReturnType Dem_SetEnableCondition(uint8_t EnableConditionID, boolean ConditionFulfilled);

/* @SWS_Dem_01080 */
Std_ReturnType Dem_SetEventAvailable(Dem_EventIdType EventId, boolean AvailableStatus);

/* @SWS_Dem_00203 */
Std_ReturnType Dem_GetFaultDetectionCounter(Dem_EventIdType EventId,
                                            sint8_t *FaultDetectionCounter);
/* @SWS_Dem_00266 */
void Dem_MainFunction(void);

/* @SWS_Dem_91008 */
Std_ReturnType Dem_GetEventUdsStatus(Dem_EventIdType EventId, Dem_UdsStatusByteType *UDSStatusByte);
#endif /* DEM_H */
