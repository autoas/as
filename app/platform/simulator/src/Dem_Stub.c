/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dem.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#if defined(USE_DCM) && !defined(USE_DEM)

Std_ReturnType Dem_DisableDTCSetting(uint8_t ClientId) {
  return E_NOT_OK;
}

Std_ReturnType Dem_EnableDTCSetting(uint8_t ClientId) {
  return E_NOT_OK;
}

Std_ReturnType Dem_SelectDTC(uint8_t ClientId, uint32_t DTC, Dem_DTCFormatType DTCFormat,
                             Dem_DTCOriginType DTCOrigin) {
  return E_NOT_OK;
}

Std_ReturnType Dem_ClearDTC(uint8_t ClientId) {
  return E_NOT_OK;
}

Std_ReturnType Dem_SetDTCFilter(uint8_t ClientId, uint8_t DTCStatusMask,
                                Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin,
                                boolean FilterWithSeverity, Dem_DTCSeverityType DTCSeverityMask,
                                boolean FilterForFaultDetectionCounter) {
  return E_NOT_OK;
}

Std_ReturnType Dem_GetNumberOfFilteredDTC(uint8_t ClientId, uint16_t *NumberOfFilteredDTC) {
  return E_NOT_OK;
}

Std_ReturnType Dem_GetNextFilteredDTC(uint8_t ClientId, uint32_t *DTC, uint8_t *DTCStatus) {
  return E_NOT_OK;
}

Std_ReturnType Dem_SetFreezeFrameRecordFilter(uint8_t ClientId, Dem_DTCFormatType DTCFormat) {
  return E_NOT_OK;
}

Std_ReturnType Dem_GetNumberOfFreezeFrameRecords(uint8_t ClientId,
                                                 uint16_t *NumberOfFilteredRecords) {
  return E_NOT_OK;
}

Std_ReturnType Dem_GetNextFilteredRecord(uint8_t ClientId, uint32_t *DTC, uint8_t *RecordNumber) {
  return E_NOT_OK;
}

Std_ReturnType Dem_SelectFreezeFrameData(uint8_t ClientId, uint8_t RecordNumber) {
  return E_NOT_OK;
}

Std_ReturnType Dem_GetNextFreezeFrameData(uint8_t ClientId, uint8_t *DestBuffer,
                                          uint16_t *BufSize) {
  return E_NOT_OK;
}

Std_ReturnType Dem_GetNextExtendedDataRecord(uint8_t ClientId, uint8_t *DestBuffer,
                                             uint16_t *BufSize) {
  return E_NOT_OK;
}
#endif
