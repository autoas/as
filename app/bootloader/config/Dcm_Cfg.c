/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dcm_Internal.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#ifndef DCM_DEFAULT_RXBUF_SIZE
#define DCM_DEFAULT_RXBUF_SIZE 514
#endif

#ifndef DCM_DEFAULT_TXBUF_SIZE
#define DCM_DEFAULT_TXBUF_SIZE 514
#endif

#define DCM_S3SERVER_CFG_TIMEOUT_MS 5000
#define DCM_P2SERVER_CFG_TIMEOUT_MS 500

#define DCM_RESET_DELAY_CFG_MS 100 /* give time for the response */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
Std_ReturnType BL_GetSessionChangePermission(Dcm_SesCtrlType sesCtrlTypeActive,
                                             Dcm_SesCtrlType sesCtrlTypeNew,
                                             Dcm_NegativeResponseCodeType *nrc);

#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
Std_ReturnType BL_GetProgramSessionSeed(uint8_t *seed, Dcm_NegativeResponseCodeType *errorCode);
Std_ReturnType BL_CompareProgramSessionKey(const uint8_t *key,
                                           Dcm_NegativeResponseCodeType *errorCode);
Std_ReturnType BL_GetExtendedSessionSeed(uint8_t *seed, Dcm_NegativeResponseCodeType *errorCode);
Std_ReturnType BL_CompareExtendedSessionKey(const uint8_t *key,
                                            Dcm_NegativeResponseCodeType *errorCode);
#endif

#ifdef DCM_USE_SERVICE_ROUTINE_CONTROL
Std_ReturnType BL_StartEraseFlash(const uint8_t *dataIn, Dcm_OpStatusType OpStatus,
                                  uint8_t *dataOut, uint16_t *currentDataLength,
                                  Dcm_NegativeResponseCodeType *ErrorCode);
Std_ReturnType BL_CheckIntegrity(const uint8_t *dataIn, Dcm_OpStatusType OpStatus, uint8_t *dataOut,
                                 uint16_t *currentDataLength,
                                 Dcm_NegativeResponseCodeType *ErrorCode);
#endif

#ifdef DCM_USE_SERVICE_REQUEST_DOWNLOAD
Std_ReturnType BL_ProcessRequestDownload(Dcm_OpStatusType OpStatus, uint8_t DataFormatIdentifier,
                                         uint8_t MemoryIdentifier, uint32_t MemoryAddress,
                                         uint32_t MemorySize, uint32_t *BlockLength,
                                         Dcm_NegativeResponseCodeType *ErrorCode);
#endif

#ifdef DCM_USE_SERVICE_TRANSFER_DATA
Dcm_ReturnWriteMemoryType BL_ProcessTransferDataWrite(Dcm_OpStatusType OpStatus,
                                                      uint8_t MemoryIdentifier,
                                                      uint32_t MemoryAddress, uint32_t MemorySize,
                                                      const Dcm_RequestDataArrayType MemoryData,
                                                      Dcm_NegativeResponseCodeType *ErrorCode);

Dcm_ReturnReadMemoryType BL_ProcessTransferDataRead(Dcm_OpStatusType OpStatus,
                                                    uint8_t MemoryIdentifier,
                                                    uint32_t MemoryAddress, uint32_t MemorySize,
                                                    Dcm_RequestDataArrayType MemoryData,
                                                    Dcm_NegativeResponseCodeType *ErrorCode);
#endif
#ifdef DCM_USE_SERVICE_REQUEST_TRANSFER_EXIT
Std_ReturnType BL_ProcessRequestTransferExit(Dcm_OpStatusType OpStatus,
                                             Dcm_NegativeResponseCodeType *ErrorCode);
#endif
/* ================================ [ DATAS     ] ============================================== */
static uint8_t rxBuffer[DCM_DEFAULT_RXBUF_SIZE];
static uint8_t txBuffer[DCM_DEFAULT_TXBUF_SIZE];

static const Dcm_SesCtrlType Dcm_SesCtrls[] = {DCM_DEFAULT_SESSION, DCM_PROGRAMMING_SESSION,
                                               DCM_EXTENDED_DIAGNOSTIC_SESSION};

static const Dcm_SessionControlConfigType Dcm_SessionControlConfig = {
  BL_GetSessionChangePermission,
  Dcm_SesCtrls,
  ARRAY_SIZE(Dcm_SesCtrls),
};

#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
static const Dcm_SecLevelConfigType Dcm_SecLevelConfigs[] = {
  {
    BL_GetExtendedSessionSeed,
    BL_CompareExtendedSessionKey,
    DCM_SEC_LEVEL1,
    4,
    4,
    DCM_EXTDS_MASK,
  },
  {
    BL_GetProgramSessionSeed,
    BL_CompareProgramSessionKey,
    DCM_SEC_LEVEL2,
    4,
    4,
    DCM_PRGS_MASK,
  },
};

static const Dcm_SecurityAccessConfigType Dcm_SecurityAccessConfig = {
  Dcm_SecLevelConfigs,
  ARRAY_SIZE(Dcm_SecLevelConfigs),

};
#endif

#ifdef DCM_USE_SERVICE_ROUTINE_CONTROL
static const Dcm_RoutineControlType Dcm_RoutineControls[] = {
  {
    0xFF01,
    BL_StartEraseFlash,
    {
      DCM_PRGS_MASK,
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
      DCM_SEC_LEVEL2_MASK,
#endif
      DCM_MISC_PHYSICAL,
    },
  },
  {
    0xFF02,
    BL_CheckIntegrity,
    {
      DCM_PRGS_MASK,
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
      DCM_SEC_LEVEL2_MASK,
#endif
      DCM_MISC_PHYSICAL,
    },
  },
};

static const Dcm_RoutineControlConfigType Dcm_RoutineControlConfig = {
  Dcm_RoutineControls,
  ARRAY_SIZE(Dcm_RoutineControls),
};
#endif

#ifdef DCM_USE_SERVICE_REQUEST_DOWNLOAD
static const Dcm_RequestDownloadConfigType Dcm_RequestDownloadConfig = {
  BL_ProcessRequestDownload,
};
#endif

#ifdef DCM_USE_SERVICE_TRANSFER_DATA
static const Dcm_TransferDataConfigType Dcm_TransferDataConfig = {
  BL_ProcessTransferDataWrite,
  BL_ProcessTransferDataRead,
};
#endif

#ifdef DCM_USE_SERVICE_REQUEST_TRANSFER_EXIT
static const Dcm_TransferExitConfigType Dcm_TransferExitConfig = {
  BL_ProcessRequestTransferExit,
};
#endif

#ifdef DCM_USE_SERVICE_ECU_RESET
static const Dcm_EcuResetConfigType Dcm_EcuResetConfig = {
  DCM_CONVERT_MS_TO_MAIN_CYCLES(100),
};
#endif

static const Dcm_ServiceType Dcm_UdsServices[] = {
  {
    SID_DIAGNOSTIC_SESSION_CONTROL,
    {
      DCM_DFTS_MASK | DCM_PRGS_MASK | DCM_EXTDS_MASK,
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
      DCM_SEC_LOCKED_MASK | DCM_SEC_LEVEL1_MASK | DCM_SEC_LEVEL2_MASK,
#endif
      DCM_MISC_PHYSICAL | DCM_MISC_FUNCTIONAL | DCM_MISC_SUB_FUNCTION,
    },
    Dcm_DspSessionControl,
    (const void *)&Dcm_SessionControlConfig,
  },
#ifdef DCM_USE_SERVICE_ECU_RESET
  {
    SID_ECU_RESET,
    {
      DCM_PRGS_MASK | DCM_EXTDS_MASK,
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
      DCM_SEC_LEVEL1_MASK | DCM_SEC_LEVEL2_MASK,
#endif
      DCM_MISC_PHYSICAL | DCM_MISC_FUNCTIONAL | DCM_MISC_SUB_FUNCTION,
    },
    Dcm_DspEcuReset,
    (const void *)&Dcm_EcuResetConfig,
  },
#endif
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
  {
    SID_SECURITY_ACCESS,
    {
      DCM_DFTS_MASK | DCM_PRGS_MASK | DCM_EXTDS_MASK,
      DCM_SEC_LOCKED_MASK | DCM_SEC_LEVEL1_MASK | DCM_SEC_LEVEL2_MASK,
      DCM_MISC_PHYSICAL | DCM_MISC_SUB_FUNCTION,
    },
    Dcm_DspSecurityAccess,
    (const void *)&Dcm_SecurityAccessConfig,
  },
#endif
#ifdef DCM_USE_SERVICE_ROUTINE_CONTROL
  {
    SID_ROUTINE_CONTROL,
    {
      DCM_PRGS_MASK,
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
      DCM_SEC_LEVEL2_MASK,
#endif
      DCM_MISC_PHYSICAL | DCM_MISC_SUB_FUNCTION,
    },
    Dcm_DspRoutineControl,
    (const void *)&Dcm_RoutineControlConfig,
  },
#endif

#ifdef DCM_USE_SERVICE_REQUEST_DOWNLOAD
  {
    SID_REQUEST_DOWNLOAD,
    {
      DCM_PRGS_MASK,
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
      DCM_SEC_LEVEL2_MASK,
#endif
      DCM_MISC_PHYSICAL,
    },
    Dcm_DspRequestDownload,
    (const void *)&Dcm_RequestDownloadConfig,
  },
#endif
#ifdef DCM_USE_SERVICE_TRANSFER_DATA
  {
    SID_TRANSFER_DATA,
    {
      DCM_PRGS_MASK,
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
      DCM_SEC_LEVEL2_MASK,
#endif
      DCM_MISC_PHYSICAL,
    },
    Dcm_DspTransferData,
    (const void *)&Dcm_TransferDataConfig,
  },
#endif
#ifdef DCM_USE_SERVICE_REQUEST_TRANSFER_EXIT
  {
    SID_REQUEST_TRANSFER_EXIT,
    {
      DCM_PRGS_MASK,
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
      DCM_SEC_LEVEL2_MASK,
#endif
      DCM_MISC_PHYSICAL,
    },
    Dcm_DspRequestTransferExit,
    (const void *)&Dcm_TransferExitConfig,
  },
#endif
#ifdef DCM_USE_SERVICE_TESTER_PRESENT
  {
    SID_TESTER_PRESENT,
    {
      DCM_DFTS_MASK | DCM_PRGS_MASK | DCM_EXTDS_MASK,
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
      DCM_SEC_LOCKED_MASK | DCM_SEC_LEVEL1_MASK | DCM_SEC_LEVEL2_MASK,
#endif
      DCM_MISC_PHYSICAL | DCM_MISC_FUNCTIONAL | DCM_MISC_SUB_FUNCTION,
    },
    Dcm_DspTesterPresent,
    NULL,
  },
#endif
};

static const Dcm_ServiceTableType Dcm_UdsServiceTable = {
  Dcm_UdsServices,
  ARRAY_SIZE(Dcm_UdsServices),
};

static const Dcm_ServiceTableType *Dcm_ServiceTables[] = {
  &Dcm_UdsServiceTable,
};

static const Dcm_TimingConfigType Dcm_TimingConfig = {
  DCM_CONVERT_MS_TO_MAIN_CYCLES(5000),
  DCM_CONVERT_MS_TO_MAIN_CYCLES(450),
  DCM_CONVERT_MS_TO_MAIN_CYCLES(500),
};

const Dcm_ConfigType Dcm_Config = {
  rxBuffer,          txBuffer,          sizeof(rxBuffer),
  sizeof(txBuffer),  Dcm_ServiceTables, ARRAY_SIZE(Dcm_ServiceTables),
  &Dcm_TimingConfig,
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType Dcm_Transmit(const uint8_t *buffer, PduLengthType length, int functional) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();

  if ((DCM_BUFFER_IDLE == context->txBufferState) && (Dcm_Config.txBufferSize >= length)) {
    r = E_OK;
    if (functional) {
      context->curPduId = DCM_P2A_PDU;
    } else {
      context->curPduId = DCM_P2P_PDU;
    }
    memcpy(Dcm_Config.txBuffer, buffer, (size_t)length);
    context->TxTpSduLength = (PduLengthType)length;
    context->txBufferState = DCM_BUFFER_FULL;
  }

  return r;
}