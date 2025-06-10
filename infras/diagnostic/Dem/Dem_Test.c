/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Debug.h"
#include "Dcm.h"
#include "Dem.h"
#include "Dem_Priv.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType Dem_TestStart(const uint8_t *dataIn, Dcm_OpStatusType OpStatus, uint8_t *dataOut,
                             uint16_t *currentDataLength, Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType ret;
  uint8_t cmd = dataIn[0];
  uint8_t operationCycleId;
  Dem_OperationCycleStateType cycleState;
  Dem_EventIdType EventId;
  Dem_EventStatusType EventStatus;
#ifdef DEM_USE_ENABLE_CONDITION
  uint8_t EnableConditionID;
  boolean ConditionFulfilled;
#endif

  if (0 == cmd) { /* set operation cycle state */
    if (3 == *currentDataLength) {
      *currentDataLength = 0;
      operationCycleId = dataIn[1];
      cycleState = dataIn[2];
      ret = Dem_SetOperationCycleState(operationCycleId, cycleState);
      if (E_OK != ret) {
        *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
      }
    } else {
      ret = E_NOT_OK;
      *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
  } else if (1 == cmd) { /* set condition status */
    if (3 == *currentDataLength) {
      *currentDataLength = 0;
#ifdef DEM_USE_ENABLE_CONDITION
      EnableConditionID = dataIn[1];
      ConditionFulfilled = dataIn[2];
      ret = Dem_SetEnableCondition(EnableConditionID, ConditionFulfilled);
      if (E_OK != ret) {
        *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
      }
#else
      ret = E_OK;
#endif
    } else {
      ret = E_NOT_OK;
      *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
  } else if (2 == cmd) { /* set DTC status */
    if (4 == *currentDataLength) {
      *currentDataLength = 0;
      EventId = ((uint16_t)dataIn[1] << 8) + dataIn[2];
      EventStatus = dataIn[3];
      ret = Dem_SetEventStatus(EventId, EventStatus);
      if (E_OK != ret) {
        *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
      }
    } else {
      ret = E_NOT_OK;
      *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
  } else {
    ret = E_NOT_OK;
    *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
  }

  return ret;
}
