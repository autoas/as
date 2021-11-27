/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dcm_Internal.h"
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
Std_ReturnType App_GetSessionChangePermission(Dcm_SesCtrlType sesCtrlTypeActive,
                                              Dcm_SesCtrlType sesCtrlTypeNew,
                                              Dcm_NegativeResponseCodeType *nrc);

#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
Std_ReturnType App_GetProgramSessionSeed(uint8_t *seed, Dcm_NegativeResponseCodeType *errorCode);
Std_ReturnType App_CompareProgramSessionKey(const uint8_t *key,
                                            Dcm_NegativeResponseCodeType *errorCode);
Std_ReturnType App_GetExtendedSessionSeed(uint8_t *seed, Dcm_NegativeResponseCodeType *errorCode);
Std_ReturnType App_CompareExtendedSessionKey(const uint8_t *key,
                                             Dcm_NegativeResponseCodeType *errorCode);
#endif
/* ================================ [ DATAS     ] ============================================== */
static uint8_t rxBuffer[DCM_DEFAULT_RXBUF_SIZE];
static uint8_t txBuffer[DCM_DEFAULT_TXBUF_SIZE];

static const Dcm_SesCtrlType Dcm_SesCtrls[] = {DCM_DEFAULT_SESSION, DCM_PROGRAMMING_SESSION,
                                               DCM_EXTENDED_DIAGNOSTIC_SESSION};

static const Dcm_SessionControlConfigType Dcm_SessionControlConfig = {
  App_GetSessionChangePermission,
  Dcm_SesCtrls,
  ARRAY_SIZE(Dcm_SesCtrls),
};

#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
static const Dcm_SecLevelConfigType Dcm_SecLevelConfigs[] = {
  {
    App_GetExtendedSessionSeed,
    App_CompareExtendedSessionKey,
    DCM_SEC_LEVEL1,
    4,
    4,
    DCM_EXTDS_MASK,
  },
  {
    App_GetProgramSessionSeed,
    App_CompareProgramSessionKey,
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

#ifdef DCM_USE_SERVICE_ECU_RESET
static const Dcm_EcuResetConfigType Dcm_EcuResetConfig = {
  DCM_CONVERT_MS_TO_MAIN_CYCLES(100),
};
#endif

#ifdef DCM_USE_SERVICE_READ_DTC_INFORMATION
static const Dcm_ReadDTCSubFunctionConfigType Dcm_ReadDTCSubFunctions[] = {
  {Dem_DspReportNumberOfDTCByStatusMask, 0x01},
  {Dem_DspReportDTCByStatusMask, 0x02},
  {Dem_DspReportDTCSnapshotIdentification, 0x03},
  {Dem_DspReportDTCSnapshotRecordByDTCNumber, 0x04},
  {Dem_DspReportDTCExtendedDataRecordByDTCNumber, 0x06},
};

static const Dcm_ReadDTCInfoConfigType Dcm_ReadDTCInfoConfig = {
  Dcm_ReadDTCSubFunctions,
  ARRAY_SIZE(Dcm_ReadDTCSubFunctions),
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
#ifdef DCM_USE_SERVICE_CONTROL_DTC_SETTING
  {
    SID_CONTROL_DTC_SETTING,
    {
      DCM_EXTDS_MASK,
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
      DCM_SEC_LOCKED_MASK | DCM_SEC_LEVEL1_MASK | DCM_SEC_LEVEL2_MASK,
#endif
      DCM_MISC_PHYSICAL | DCM_MISC_FUNCTIONAL | DCM_MISC_SUB_FUNCTION,
    },
    Dcm_DspControlDTCSetting,
    NULL,
  },
#endif

#ifdef DCM_USE_SERVICE_CLEAR_DIAGNOSTIC_INFORMATION
  {
    SID_CLEAR_DIAGNOSTIC_INFORMATION,
    {
      DCM_EXTDS_MASK,
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
      DCM_SEC_LOCKED_MASK | DCM_SEC_LEVEL1_MASK | DCM_SEC_LEVEL2_MASK,
#endif
      DCM_MISC_PHYSICAL | DCM_MISC_FUNCTIONAL,
    },
    Dcm_DspClearDTC,
    NULL,
  },
#endif

#ifdef DCM_USE_SERVICE_READ_DTC_INFORMATION
  {
    SID_READ_DTC_INFORMATION,
    {
      DCM_EXTDS_MASK,
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
      DCM_SEC_LOCKED_MASK | DCM_SEC_LEVEL1_MASK | DCM_SEC_LEVEL2_MASK,
#endif
      DCM_MISC_PHYSICAL | DCM_MISC_FUNCTIONAL | DCM_MISC_SUB_FUNCTION,
    },
    Dcm_DspReadDTCInformation,
    &Dcm_ReadDTCInfoConfig,
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
