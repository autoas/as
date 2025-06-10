/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Debug.h"
#include "Dcm.h"
#include "NvM.h"
#include "Fee.h"
#include "Fls.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#if defined(_WIN32) || defined(linux)
extern uint8_t g_FlsAcMirror[];
#define FEE_ADDRESS(v) ((void *)(&g_FlsAcMirror[(v)]))
#else
#define FEE_ADDRESS(v) ((void *)(v))
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType NvM_TestStart(const uint8_t *dataIn, Dcm_OpStatusType OpStatus, uint8_t *dataOut,
                             uint16_t *currentDataLength, Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType ret;
  uint8_t cmd = dataIn[0];
  NvM_BlockIdType BlockId = ((uint16_t)dataIn[1] << 8) + dataIn[2];
  void *pNvmData;
  uint16_t length;
#ifdef USE_FEE
  Fee_AdminInfoType adminInfo;
  uint32_t address;
  uint32_t size;
#endif
  NvM_RequestResultType result = NVM_REQ_OK;
  if ((0 == cmd) || (1 == cmd)) {
    /* check read/write nvm block id is correct */
    ret = NvM_GetBlockDataPtrAndLength(BlockId, &pNvmData, &length);
  } else {
    ret = E_OK;
  }
  if (E_OK != ret) {
    *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
  } else if (0 == cmd) { /* write NvM block */
    if ((length + 3) == *currentDataLength) {
      if (DCM_INITIAL == OpStatus) {
        memcpy(pNvmData, &dataIn[3], length);
        ret = NvM_WriteBlock(BlockId, pNvmData);
        if (E_OK == ret) {
          *ErrorCode = DCM_E_RESPONSE_PENDING;
        } else {
          *ErrorCode = DCM_E_GENERAL_REJECT;
        }
      } else {
        ret = NvM_GetErrorStatus(BlockId, &result);
        if (E_OK == ret) {
          if (NVM_REQ_OK == result) {
            dataOut[0] = cmd;
            dataOut[1] = (BlockId >> 8) & 0xFF;
            dataOut[2] = BlockId & 0xFF;
            memcpy(&dataOut[3], &dataIn[3], length);
            *currentDataLength = length + 3;
          } else if (NVM_REQ_PENDING == result) {
            *ErrorCode = DCM_E_RESPONSE_PENDING;
          } else {
            *ErrorCode = DCM_E_CONDITIONS_NOT_CORRECT;
          }
        } else {
          *ErrorCode = DCM_E_GENERAL_REJECT;
        }
      }
    } else {
      ret = E_NOT_OK;
      *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
  } else if (1 == cmd) { /* read NvM block */
    memcpy(dataOut, pNvmData, length);
    *currentDataLength = length;
  } else if (2 == cmd) { /* Get Fee status */
#ifdef USE_FEE
    Fee_GetAdminInfo(&adminInfo);
    dataOut[0] = adminInfo.curWrokingBank;
    dataOut[1] = (adminInfo.adminFreeAddr >> 24) & 0xFF;
    dataOut[2] = (adminInfo.adminFreeAddr >> 16) & 0xFF;
    dataOut[3] = (adminInfo.adminFreeAddr >> 8) & 0xFF;
    dataOut[4] = adminInfo.adminFreeAddr & 0xFF;
    dataOut[5] = (adminInfo.dataFreeAddr >> 24) & 0xFF;
    dataOut[6] = (adminInfo.dataFreeAddr >> 16) & 0xFF;
    dataOut[7] = (adminInfo.dataFreeAddr >> 8) & 0xFF;
    dataOut[8] = adminInfo.dataFreeAddr & 0xFF;
    dataOut[9] = (adminInfo.eraseNumber >> 24) & 0xFF;
    dataOut[10] = (adminInfo.eraseNumber >> 16) & 0xFF;
    dataOut[11] = (adminInfo.eraseNumber >> 8) & 0xFF;
    dataOut[12] = adminInfo.eraseNumber & 0xFF;
    *currentDataLength = 13;
#else
    ret = E_NOT_OK;
    *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
#endif
  } else if (3 == cmd) { /* Dump Fee */
#if defined(USE_FEE)
    if (9 == *currentDataLength) {
      address = ((uint32_t)dataIn[1] << 24) + ((uint32_t)dataIn[2] << 16) +
                ((uint32_t)dataIn[3] << 8) + dataIn[4];
      size = ((uint32_t)dataIn[5] << 24) + ((uint32_t)dataIn[6] << 16) +
             ((uint32_t)dataIn[7] << 8) + dataIn[8];
#if defined(FLS_DIRECT_ACCESS)
      memcpy(dataOut, FEE_ADDRESS(address), size);
      *currentDataLength = (uint16_t)size;
      ret = E_OK;
#else
      do {
        ret = Fls_AcRead(address, dataOut, size);
      } while (E_FLS_PENDING == ret);
      if (E_OK == ret) {
        *currentDataLength = (uint16_t)size;
      } else {
        *ErrorCode = DCM_E_CONDITIONS_NOT_CORRECT;
      }
#endif

    } else {
      ret = E_NOT_OK;
      *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
#else
    ret = E_NOT_OK;
    *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
#endif
  } else {
    ret = E_NOT_OK;
    *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
  }
  return ret;
}

Std_ReturnType NvM_TestGetResult(const uint8_t *dataIn, Dcm_OpStatusType OpStatus, uint8_t *dataOut,
                                 uint16_t *currentDataLength,
                                 Dcm_NegativeResponseCodeType *ErrorCode) {
  MemIf_StatusType status = NvM_GetStatus();
  dataOut[0] = status;
  *currentDataLength = 1;
  return E_OK;
}
