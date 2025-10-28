/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Debug.h"
#include "Dcm.h"
#include "Mirror.h"
/* ================================ [ MACROS    ] ============================================== */
#define CMD_MIRROR_SET_STATIC_FILTER_STATE 0u
#define CMD_MIRROR_GET_STATIC_FILTER_STATE 1u
#define CMD_MIRROR_ADD_CAN_RANGE_FILTER 2u
#define CMD_MIRROR_ADD_CAN_MASK_FILTER 3u
#define CMD_MIRROR_ADD_LIN_RANGE_FILTER 4u
#define CMD_MIRROR_ADD_LIN_MASK_FILTER 5u
#define CMD_MIRROR_REMOVE_FILTER 6u
#define CMD_MIRROR_SWITCH_DEST 7u
#define CMD_MIRROR_START_STOP_SOURCE 8u
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType Mirror_TestStart(const uint8_t *dataIn, Dcm_OpStatusType OpStatus, uint8_t *dataOut,
                                uint16_t *currentDataLength,
                                Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType ret;
  uint8_t cmd = dataIn[0];
  NetworkHandleType network;
  uint8_t filterId;
  boolean isActive;
  uint32_t lower;
  uint32_t upper;
  uint32_t mask;
  uint32_t code;
  if (CMD_MIRROR_SET_STATIC_FILTER_STATE == cmd) {
    if (4 == *currentDataLength) {
      *currentDataLength = 0;
      network = dataIn[1];
      filterId = dataIn[2];
      isActive = dataIn[3];
      ret = Mirror_SetStaticFilterState(network, filterId, isActive);
      if (E_OK != ret) {
        *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
      }
    } else {
      ret = E_NOT_OK;
      *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
  } else if (CMD_MIRROR_GET_STATIC_FILTER_STATE == cmd) {
    if (3 == *currentDataLength) {
      *currentDataLength = 1;
      network = dataIn[1];
      filterId = dataIn[2];
      ret = Mirror_GetStaticFilterState(network, filterId, &isActive);
      if (E_OK != ret) {
        *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
      } else {
        dataOut[0] = isActive;
      }
    } else {
      ret = E_NOT_OK;
      *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
  } else if (CMD_MIRROR_ADD_CAN_RANGE_FILTER == cmd) {
    if (10 == *currentDataLength) {
      *currentDataLength = 1;
      network = dataIn[1];
      lower = ((uint32_t)dataIn[2] << 24) + ((uint32_t)dataIn[3] << 16) +
              ((uint32_t)dataIn[4] << 8) + dataIn[5];
      upper = ((uint32_t)dataIn[6] << 24) + ((uint32_t)dataIn[7] << 16) +
              ((uint32_t)dataIn[8] << 8) + dataIn[9];
      ret = Mirror_AddCanRangeFilter(network, &filterId, lower, upper);
      if (E_OK != ret) {
        *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
      } else {
        dataOut[0] = filterId;
      }
    } else {
      ret = E_NOT_OK;
      *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
  } else if (CMD_MIRROR_ADD_CAN_MASK_FILTER == cmd) {
    if (10 == *currentDataLength) {
      *currentDataLength = 1;
      network = dataIn[1];
      code = ((uint32_t)dataIn[2] << 24) + ((uint32_t)dataIn[3] << 16) +
             ((uint32_t)dataIn[4] << 8) + dataIn[5];
      mask = ((uint32_t)dataIn[6] << 24) + ((uint32_t)dataIn[7] << 16) +
             ((uint32_t)dataIn[8] << 8) + dataIn[9];
      ret = Mirror_AddCanMaskFilter(network, &filterId, code, mask);
      if (E_OK != ret) {
        *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
      } else {
        dataOut[0] = filterId;
      }
    } else {
      ret = E_NOT_OK;
      *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
  } else if (CMD_MIRROR_ADD_LIN_RANGE_FILTER == cmd) {
    if (4 == *currentDataLength) {
      *currentDataLength = 1;
      network = dataIn[1];
      ret = Mirror_AddLinRangeFilter(network, &filterId, dataIn[2], dataIn[3]);
      if (E_OK != ret) {
        *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
      } else {
        dataOut[0] = filterId;
      }
    } else {
      ret = E_NOT_OK;
      *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
  } else if (CMD_MIRROR_ADD_LIN_MASK_FILTER == cmd) {
    if (4 == *currentDataLength) {
      *currentDataLength = 1;
      network = dataIn[1];
      ret = Mirror_AddLinMaskFilter(network, &filterId, dataIn[2], dataIn[3]);
      if (E_OK != ret) {
        *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
      } else {
        dataOut[0] = filterId;
      }
    } else {
      ret = E_NOT_OK;
      *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
  } else if (CMD_MIRROR_REMOVE_FILTER == cmd) {
    if (3 == *currentDataLength) {
      *currentDataLength = 0;
      network = dataIn[1];
      filterId = dataIn[2];
      ret = Mirror_RemoveFilter(network, filterId);
      if (E_OK != ret) {
        *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
      }
    } else {
      ret = E_NOT_OK;
      *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
  } else if (CMD_MIRROR_SWITCH_DEST == cmd) {
    if (2 == *currentDataLength) {
      *currentDataLength = 0;
      network = dataIn[1];
      ret = Mirror_SwitchDestNetwork(network);
      if (E_OK != ret) {
        *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
      }
    } else {
      ret = E_NOT_OK;
      *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
  } else if (CMD_MIRROR_START_STOP_SOURCE == cmd) {
    if (3 == *currentDataLength) {
      *currentDataLength = 0;
      isActive = dataIn[1];
      network = dataIn[2];
      if (TRUE == isActive) {
        ret = Mirror_StartSourceNetwork(network);
      } else {
        ret = Mirror_StopSourceNetwork(network);
      }
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
