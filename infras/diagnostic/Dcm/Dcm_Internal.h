/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef DCM_INTERNAL_H
#define DCM_INTERNAL_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dcm.h"
#include "NvM.h"
/* ================================ [ MACROS    ] ============================================== */
#define DCM_INVALID_PDU_ID ((PduIdType)-1)

/* @SWS_Dcm_00978 */
#define DCM_DFTS_MASK 0x01
#define DCM_PRGS_MASK 0x02
#define DCM_EXTDS_MASK 0x04
#define DCM_SSDS_MASK 0x08

#define DCM_ANY_SESSION_MASK 0xFF

/* @SWS_Dcm_00977 */
#define DCM_SEC_LOCKED_MASK 0x01
#define DCM_SEC_LEVEL1_MASK 0x02
#define DCM_SEC_LEVEL2_MASK 0x04
#define DCM_SEC_LEVEL3_MASK 0x08
#define DCM_SEC_LEVEL4_MASK 0x10
#define DCM_SEC_LEVEL5_MASK 0x20
#define DCM_SEC_LEVEL6_MASK 0x40
#define DCM_SEC_LEVEL7_MASK 0x80

#define DCM_ANY_SECURITY_MASK 0xFF

/* @SWS_Dcm_00442 UDS Services */
#define SID_DIAGNOSTIC_SESSION_CONTROL 0x10
#define SID_ECU_RESET 0x11
#define SID_CLEAR_DIAGNOSTIC_INFORMATION 0x14
#define SID_READ_DTC_INFORMATION 0x19
#define SID_READ_DATA_BY_IDENTIFIER 0x22
#define SID_READ_MEMORY_BY_ADDRESS 0x23
#define SID_SECURITY_ACCESS 0x27
#define SID_COMMUNICATION_CONTROL 0x28
#define SID_AUTHENTICATION 0x29
#define SID_READ_DATA_BY_PERIODIC_IDENTIFIER 0x2A
#define SID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER 0x2C
#define SID_WRITE_DATA_BY_IDENTIFIER 0x2E
#define SID_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER 0x2F
#define SID_ROUTINE_CONTROL 0x31
#define SID_REQUEST_DOWNLOAD 0x34
#define SID_REQUEST_UPLOAD 0x35
#define SID_TRANSFER_DATA 0x36
#define SID_REQUEST_TRANSFER_EXIT 0x37
#define SID_WRITE_MEMORY_BY_ADDRESS 0x3D
#define SID_TESTER_PRESENT 0x3E
#define SID_CONTROL_DTC_SETTING 0x85

#define SID_NEGATIVE_RESPONSE 0x7F

#define SUPPRESS_POS_RESP_BIT (uint8_t)0x80
#define SID_RESPONSE_BIT (uint8_t)0x40

/* @SWS_Dcm_00992 */
#define DCM_MISC_PHYSICAL 0x01
#define DCM_MISC_FUNCTIONAL 0x02
#define DCM_MISC_SUB_FUNCTION 0x04

#define DCM_NO_RESPONSE_PENDING 0
#define DCM_RESPONSE_PENDING 1
#define DCM_RESPONSE_PENDING_PROVIDED 2
#define DCM_RESPONSE_PENDING_TXING 3

/* @SWS_Dcm_00579 */
#define DCM_IOCTRL_RETURN_CTRL_TO_ECU 0x00
#define DCM_IOCTRL_RESET_TO_DEFAULT 0x01
#define DCM_IOCTRL_FREEZE_CURRENT_STATE 0x02
#define DCM_IOCTRL_SHORT_TERM_ADJUSTMENT 0x03

#define DCM_COMCTRL_ENABLE_RX_AND_TX 0x00
#define DCM_COMCTRL_ENABLE_RX_AND_DISABLE_TX 0x01
#define DCM_COMCTRL_DISABLE_RX_AND_ENABLE_TX 0x02
#define DCM_COMCTRL_DISABLE_RX_AND_TX 0x03

#define DCM_COMTYPE_NORMAL 0x01
#define DCM_COMTYPE_NM 0x02
#define DCM_COMTYPE_NM_AND_NORMAL 0x03

#define DCM_ENABLE_RX_TX_NORM ((Dcm_CommunicationModeType)0x00)
#define DCM_ENABLE_RX_DISABLE_TX_NORM ((Dcm_CommunicationModeType)0x01)
#define DCM_DISABLE_RX_ENABLE_TX_NORM ((Dcm_CommunicationModeType)0x02)
#define DCM_DISABLE_RX_TX_NORMAL ((Dcm_CommunicationModeType)0x03)

#define DCM_ENABLE_RX_TX_NM ((Dcm_CommunicationModeType)0x04)
#define DCM_ENABLE_RX_DISABLE_TX_NM ((Dcm_CommunicationModeType)0x05)
#define DCM_DISABLE_RX_ENABLE_TX_NM ((Dcm_CommunicationModeType)0x06)
#define DCM_DISABLE_RX_TX_NM ((Dcm_CommunicationModeType)0x07)

#define DCM_ENABLE_RX_TX_NORM_NM ((Dcm_CommunicationModeType)0x08)
#define DCM_ENABLE_RX_DISABLE_TX_NORM_NM ((Dcm_CommunicationModeType)0x09)
#define DCM_DISABLE_RX_ENABLE_TX_NORM_NM ((Dcm_CommunicationModeType)0x0A)
#define DCM_DISABLE_RX_TX_NORM_NM ((Dcm_CommunicationModeType)0x0B)

#define DCM_PERIODIC_SLOT_IDLE (Dcm_ReadPeriodicDIDSlotStatusType)0x00
#define DCM_PERIODIC_SLOT_ACTIVATE (Dcm_ReadPeriodicDIDSlotStatusType)0x01
#define DCM_PERIODIC_SLOT_RESP_PENDING (Dcm_ReadPeriodicDIDSlotStatusType)0x02

#define DCM_TM_SEND_AT_SLOW_RATE 0x01
#define DCM_TM_SEND_AT_MEDIUM_RATE 0x02
#define DCM_TM_SEND_AT_FAST_RATE 0x03
#define DCM_TM_STOP_SENDING 0x04

#ifndef DCM_TM_SLOW_TIME_MS
#define DCM_TM_SLOW_TIME_MS 3000
#endif

#ifndef DCM_TM_MEDIUM_TIME_MS
#define DCM_TM_MEDIUM_TIME_MS 1500
#endif

#ifndef DCM_TM_FAST_TIME_MS
#define DCM_TM_FAST_TIME_MS 500
#endif

#ifndef DCM_DDDID_MAX_ENTRY
#define DCM_DDDID_MAX_ENTRY 32
#endif

#define DCM_MEM_ATTR_READ ((uint8_t)0x01)
#define DCM_MEM_ATTR_WRITE ((uint8_t)0x02)
#define DCM_MEM_ATTR_EXECUTE ((uint8_t)0x04)

/* Authentication with PKI Certificate Exchange (APCE) (Will be supported by DEXT and AUTOSAR) */
#define DCM_AUTHENTICATION_WITH_APCE 0x02
/* Authentication with ChallengeResponse (ACR) and asymmetric cryptography*/
#define DCM_AUTHENTICATION_WITH_ACRA 0x03
/* Authentication with ChallengeResponse (ACR) and symmetric cryptograph */
#define DCM_AUTHENTICATION_WITH_ACRS 0x04

#ifndef DCM_AUTHENTICATION_TYPE
#define DCM_AUTHENTICATION_TYPE DCM_AUTHENTICATION_WITH_APCE
#endif
/* ================================ [ TYPES     ] ============================================== */
enum {
  DCM_BUFFER_IDLE = 0,
  DCM_BUFFER_PROVIDED,
  DCM_BUFFER_FULL,
  DCM_BUFFER_DEAD, /* dead as processing has finished */
};

typedef struct Dcm_Service_s Dcm_ServiceType;

#if defined(DCM_USE_SERVICE_REQUEST_DOWNLOAD) || defined(DCM_USE_SERVICE_REQUEST_UPLOAD)
/* UDT = Upload Download Transfer */
typedef enum {
  DCM_UDT_IDLE_STATE = 0,
  DCM_UDT_UPLOAD_STATE,
  DCM_UDT_DOWNLOAD_STATE
} Dcm_UDTStateType;

typedef struct {
  Dcm_UDTStateType state;
  uint32_t memoryAddress;
  uint32_t memorySize;
  uint32_t offset;
  uint8_t blockSequenceCounter;
} Dcm_UDTType;
#endif

typedef struct {
  PduIdType curPduId;
  PduLengthType RxTpSduLength;
  PduLengthType RxIndex;
  PduLengthType TxTpSduLength;
  PduLengthType TxIndex;
  uint16_t timerS3Server;
  uint16_t timerP2Server;
  uint8_t respPendCnt;
  uint8_t currentSID;
  Dcm_SesCtrlType currentSession;
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
  Dcm_SecLevelType currentLevel;
  Dcm_SecLevelType requestLevel;
  uint16_t securityDelayTimer;
#endif
  uint8_t rxBufferState;
  uint8_t txBufferState;
  Dcm_OpStatusType opStatus;
  const Dcm_ServiceType *curService;
  Dcm_MsgContextType msgContext;
  uint8_t responcePending;
#if defined(DCM_USE_SERVICE_REQUEST_DOWNLOAD) || defined(DCM_USE_SERVICE_REQUEST_UPLOAD)
  Dcm_UDTType UDTData;
#endif
#ifdef DCM_USE_SERVICE_ECU_RESET
  uint8_t resetType;
  uint16_t timer2Reset;
#endif
} Dcm_ContextType;

typedef struct {
  uint8_t sessionMask;
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
  uint8_t securityMask;
#endif
  uint8_t miscMask;
} Dcm_SesSecAccessType;

typedef Std_ReturnType (*Dcm_DspServiceFncType)(Dcm_MsgContextType *msgContext,
                                                Dcm_NegativeResponseCodeType *nrc);
typedef Std_ReturnType (*Dcm_CallbackGetSesChgPermissionFncType)(Dcm_SesCtrlType sesCtrlTypeActive,
                                                                 Dcm_SesCtrlType sesCtrlTypeNew,
                                                                 Dcm_NegativeResponseCodeType *nrc);

typedef struct {
  Dcm_CallbackGetSesChgPermissionFncType GetSesChgPermissionFnc;
  const Dcm_SesCtrlType *sesCtrls;
  uint8_t numOfSesCtrls;
} Dcm_SessionControlConfigType;

/* @SWS_Dcm_91003 */
typedef Std_ReturnType (*Dcm_CallbackGetSeedFncType)(uint8_t *Seed,
                                                     Dcm_NegativeResponseCodeType *ErrorCode);
/* @SWS_Dcm_91004 */
typedef Std_ReturnType (*Dcm_CallbackCompareKeyFncType)(const uint8_t *Key,
                                                        Dcm_NegativeResponseCodeType *ErrorCode);
typedef struct {
  Dcm_CallbackGetSeedFncType GetSeedFnc;
  Dcm_CallbackCompareKeyFncType CompareKeyFnc;
  Dcm_SecLevelType secLevel;
  uint8_t seedSize;
  uint8_t keySize;
  uint8_t sessionMask;
} Dcm_SecLevelConfigType;

typedef struct {
  const Dcm_SecLevelConfigType *secLevelConfig;
  uint8_t numOfSesLevels;
} Dcm_SecurityAccessConfigType;

/* SWS_Dcm_01203 */
typedef Std_ReturnType (*Dcm_StartRoutineFncType)(const uint8_t *dataIn, Dcm_OpStatusType OpStatus,
                                                  uint8_t *dataOut,
                                                  uint16_t *currentDataLength /*InOut*/,
                                                  Dcm_NegativeResponseCodeType *ErrorCode);

/* @SWS_Dcm_01204 */
typedef Std_ReturnType (*Dcm_StopRoutineFncType)(const uint8_t *dataIn, Dcm_OpStatusType OpStatus,
                                                 uint8_t *dataOut,
                                                 uint16_t *currentDataLength /*InOut*/,
                                                 Dcm_NegativeResponseCodeType *ErrorCode);

/* @SWS_Dcm_91013 */
typedef Std_ReturnType (*Dcm_RequestResultRoutineFncType)(const uint8_t *dataIn,
                                                          Dcm_OpStatusType OpStatus,
                                                          uint8_t *dataOut,
                                                          uint16_t *currentDataLength /*InOut*/,
                                                          Dcm_NegativeResponseCodeType *ErrorCode);
typedef struct {
  uint16_t id;
  Dcm_StartRoutineFncType StartRoutineFnc;
  Dcm_StopRoutineFncType StopRoutineFnc;
  Dcm_RequestResultRoutineFncType RequestResultRoutineFnc;
  Dcm_SesSecAccessType SesSecAccess;
} Dcm_RoutineControlType;

typedef struct {
  const Dcm_RoutineControlType *rtCtrls;
  uint8_t numOfRtCtrls;
} Dcm_RoutineControlConfigType;

/* length: the length of additional ControlRecord(controlOptionRecord + controlEnableMaskRecord)
 * resDataLen: [In/Out]
 *   In: the max length of resData,
 *   Out: the length of additional controlStatusRecord responsed */
typedef Std_ReturnType (*Dcm_IOCtrlExecuteFncType)(uint8_t *ControlRecord, uint16_t length,
                                                   uint8_t *resData, uint16_t *resDataLen,
                                                   uint8_t *nrc);

/* @ECUC_Dcm_00672 */
typedef Dcm_IOCtrlExecuteFncType Dcm_IOCtrlReturnControlToEcuFncType;
/* @ECUC_Dcm_00673 */
typedef Dcm_IOCtrlExecuteFncType Dcm_IOCtrlResetToDefaultFncType;
/* @ECUC_Dcm_00674 */
typedef Dcm_IOCtrlExecuteFncType Dcm_IOCtrlFreezeCurrentStateFncType;
/* @ECUC_Dcm_00675 */
typedef Dcm_IOCtrlExecuteFncType Dcm_IOCtrlShortTermAdjustmentFncType;

typedef struct {
  uint8_t requestMask;
} Dcm_IOControlContextType;

typedef struct {
  Dcm_IOControlContextType *context;
  uint16_t id;
  Dcm_IOCtrlReturnControlToEcuFncType ReturnControlToEcuFnc;
  Dcm_IOCtrlResetToDefaultFncType ResetToDefaultFnc;
  Dcm_IOCtrlFreezeCurrentStateFncType FreezeCurrentStateFnc;
  Dcm_IOCtrlShortTermAdjustmentFncType ShortTermAdjustmentFnc;
  Dcm_SesSecAccessType SesSecAccess;
} Dcm_IOControlType;

typedef struct {
  const Dcm_IOControlType *IOCtrls;
  uint8_t numOfIOCtrls;
} Dcm_IOControlConfigType;

/* @SWS_Dcm_00754 */
typedef Std_ReturnType (*Dcm_ProcessRequestDownloadFncType)(
  Dcm_OpStatusType OpStatus, uint8_t DataFormatIdentifier, uint8_t MemoryIdentifier,
  uint32_t MemoryAddress, uint32_t MemorySize, uint32_t *BlockLength /*InOut*/,
  Dcm_NegativeResponseCodeType *ErrorCode);

/* @SWS_Dcm_00756 */
typedef Std_ReturnType (*Dcm_ProcessRequestUploadFncType)(
  Dcm_OpStatusType OpStatus, uint8_t DataFormatIdentifier, uint8_t MemoryIdentifier,
  uint32_t MemoryAddress, uint32_t MemorySize, uint32_t *BlockLength /*InOut*/,
  Dcm_NegativeResponseCodeType *ErrorCode);

typedef struct {
  Dcm_ProcessRequestDownloadFncType ProcessRequestDownloadFnc;
} Dcm_RequestDownloadConfigType;

typedef struct {
  Dcm_ProcessRequestUploadFncType ProcessRequestUploadFnc;
} Dcm_RequestUploadConfigType;

typedef Std_ReturnType (*Dcm_ComCtrlFncType)(uint8_t comType,
                                             Dcm_NegativeResponseCodeType *ErrorCode);

/* @SWS_Dcm_00981 */
typedef uint8_t Dcm_CommunicationModeType;
typedef struct {
  uint8_t id;
  Dcm_ComCtrlFncType comCtrlFnc;
} Dcm_ComCtrlType;

typedef struct {
  const Dcm_ComCtrlType *ComCtrls;
  uint8_t numOfComCtrls;
} Dcm_CommunicationControlConfigType;

typedef Std_ReturnType (*Dcm_AuthenticationFncType)(Dcm_OpStatusType OpStatus,
                                                    const uint8_t *dataIn, uint16_t dataInLen,
                                                    uint8_t *dataOut,
                                                    uint16_t *dataOutLen, /* InOut */
                                                    Dcm_NegativeResponseCodeType *ErrorCode);

typedef struct {
  uint8_t id;
  Dcm_AuthenticationFncType authenticationFnc;
} Dcm_AuthenticationType;

typedef struct {
  const Dcm_AuthenticationType *Authentications;
  uint8_t numOfAuthentications;
} Dcm_AuthenticationConfigType;

/* @SWS_Dcm_91071 */
typedef Dcm_ReturnWriteMemoryType (*Dcm_ProcessTransferDataWriteFncType)(
  Dcm_OpStatusType OpStatus, uint8_t MemoryIdentifier, uint32_t MemoryAddress, uint32_t MemorySize,
  const Dcm_RequestDataArrayType MemoryData, Dcm_NegativeResponseCodeType *ErrorCode);

/* @SWS_Dcm_91070 */
typedef Dcm_ReturnReadMemoryType (*Dcm_ProcessTransferDataReadFncType)(
  Dcm_OpStatusType OpStatus, uint8_t MemoryIdentifier, uint32_t MemoryAddress, uint32_t MemorySize,
  Dcm_RequestDataArrayType MemoryData, Dcm_NegativeResponseCodeType *ErrorCode);

typedef struct {
  Dcm_ProcessTransferDataWriteFncType WriteFnc;
  Dcm_ProcessTransferDataReadFncType ReadFnc;
} Dcm_TransferDataConfigType;

/* @SWS_Dcm_00755 */
typedef Std_ReturnType (*Dcm_ProcessRequestTransferExitFncType)(
  Dcm_OpStatusType OpStatus, Dcm_NegativeResponseCodeType *ErrorCode);

typedef struct {
  Dcm_ProcessRequestTransferExitFncType TransferExitFnc;
} Dcm_TransferExitConfigType;

typedef Std_ReturnType (*Dcm_GetEcuResetPermissionFncType)(Dcm_OpStatusType OpStatus,
                                                           Dcm_NegativeResponseCodeType *ErrorCode);

typedef struct {
  Dcm_GetEcuResetPermissionFncType GetEcuResetPermissionFnc;
  uint16_t delay;
} Dcm_EcuResetConfigType;

typedef Std_ReturnType (*Dcm_CallbackReadDidFncType)(Dcm_OpStatusType opStatus, uint8_t *data,
                                                     uint16_t length,
                                                     Dcm_NegativeResponseCodeType *errorCode);
typedef Std_ReturnType (*Dcm_CallbackWriteDidFncType)(Dcm_OpStatusType opStatus, uint8_t *data,
                                                      uint16_t length,
                                                      Dcm_NegativeResponseCodeType *errorCode);

typedef struct {
  Dcm_OpStatusType opStatus;
} Dcm_ReadDIDContextType;

typedef struct {
  uint16_t id;
  uint16_t length;
  Dcm_CallbackReadDidFncType readDIdFnc;
  Dcm_SesSecAccessType SesSecAccess;
} Dcm_rDIDConfigType;

typedef struct {
  Dcm_ReadDIDContextType *context;
  const Dcm_rDIDConfigType *rDID;
} Dcm_ReadDIDType;

typedef struct {
  uint16_t id;
  uint16_t length;
  Dcm_CallbackWriteDidFncType writeDIdFnc;
  Dcm_SesSecAccessType SesSecAccess;
} Dcm_WriteDIDType;

typedef struct {
  const Dcm_ReadDIDType *DIDs;
  uint8_t numOfDIDs;
} Dcm_ReadDIDConfigType;

typedef struct {
  Dcm_OpStatusType opStatus;
  uint16_t reload; /* timer reload value */
  uint16_t timer;
} Dcm_ReadPeriodicDIDContextType;

typedef struct {
  Dcm_ReadPeriodicDIDContextType *context;
  const Dcm_rDIDConfigType *DID;
} Dcm_ReadPeriodicDIDType;

typedef struct {
  const Dcm_ReadPeriodicDIDType *DIDs;
  uint8_t numOfDIDs;
} Dcm_ReadPeriodicDIDConfigType;

typedef struct {
  const Dcm_WriteDIDType *DIDs;
  uint8_t numOfDIDs;
} Dcm_WriteDIDConfigType;

typedef struct {
  Dcm_DspServiceFncType subFnc;
  uint8_t type;
} Dcm_ReadDTCSubFunctionConfigType;

typedef struct {
  const Dcm_ReadDTCSubFunctionConfigType *subFunctions;
  uint8_t numOfSubFunctions;
} Dcm_ReadDTCInfoConfigType;

struct Dcm_Service_s {
  uint8_t SID;
  Dcm_SesSecAccessType SesSecAccess;
  Dcm_DspServiceFncType dspServiceFnc;
  const void *config;
};

typedef struct {
  const Dcm_ServiceType *services;
  uint8_t numOfServices;
} Dcm_ServiceTableType;

typedef struct {
  uint16_t S3Server;
  uint16_t P2ServerMin;
  uint16_t P2ServerMax;
} Dcm_TimingConfigType;

typedef struct {
  uint8_t MaxNumRespPend; /* @ECUC_Dcm_00693 */
} Dcm_DslDiagRespConfigType;

typedef struct {
  Dcm_OpStatusType opStatus;
  uint8_t index;
  uint8_t position;
  uint8_t size;
} Dcm_DDDIDEntryType;

typedef struct {
  Dcm_DDDIDEntryType entry[DCM_DDDID_MAX_ENTRY];
  uint8_t numOfEntry;
} Dcm_DDDIDContextType;

typedef struct {
  Dcm_rDIDConfigType *rDID;
  Dcm_DDDIDContextType *context;
  Dcm_CallbackReadDidFncType readDIdFnc;
} Dcm_DDDIDConfigType;

typedef struct {
  uint32_t low;
  uint32_t high;
  uint8_t attr;
  Dcm_SesSecAccessType SesSecAccess;
} Dcm_DspMemoryRangeInfoType;

typedef struct {
  const uint8_t *AddressAndLengthFormatIdentifiers; /* @ECUC_Dcm_00964 */
  uint8_t numOfAALFIs;
  const Dcm_DspMemoryRangeInfoType *Mems;
  uint8_t numOfMems;
} Dcm_DspMemoryConfigType;

struct Dcm_Config_s {
  uint8_t *rxBuffer;
  uint8_t *txBuffer;
  PduLengthType rxBufferSize;
  PduLengthType txBufferSize;
  const Dcm_ServiceTableType **serviceTables;
  uint8_t numOfServiceTables;
  const Dcm_TimingConfigType *timing;
  const Dcm_DslDiagRespConfigType *dslDisgResp;
#ifdef DCM_USE_SERVICE_DYNAMICALLY_DEFINE_DATA_IDENTIFIER
  const Dcm_DDDIDConfigType *DDDIDs;
  uint8_t numOfDDDIDs;
  const Dcm_rDIDConfigType *rDIDs;
  uint8_t numOfrDIDs;
#endif
#ifdef DCM_USE_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER
  const Dcm_ReadPeriodicDIDConfigType *rPDIDConfig;
#endif
#ifdef DCM_USE_SERVICE_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER
  const Dcm_IOControlConfigType *IOCtlConfig;
#endif
#if defined(DCM_USE_SERVICE_READ_MEMORY_BY_ADDRESS) ||                                             \
  defined(DCM_USE_SERVICE_WRITE_MEMORY_BY_ADDRESS)
  const Dcm_DspMemoryConfigType *MemoryConfig;
#endif
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
  uint8_t SecurityNumAttDelay; /* @ECUC_Dcm_00762 */
  uint16_t SecurityDelayTime;  /* @ECUC_Dcm_00757 */
#ifdef USE_NVM
  NvM_BlockIdType SecurityNvMBlkId;
#endif
#endif
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
extern Dcm_ContextType *Dcm_GetContext(void);
const Dcm_ConfigType *Dcm_GetConfig(void);

void Dcm_DslProcessingDone(Dcm_ContextType *context, const Dcm_ConfigType *config,
                           Dcm_NegativeResponseCodeType nrc);

Std_ReturnType Dcm_DspSessionControl(Dcm_MsgContextType *msgContext,
                                     Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DspSecurityAccess(Dcm_MsgContextType *msgContext,
                                     Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DspRoutineControl(Dcm_MsgContextType *msgContext,
                                     Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DspRequestDownload(Dcm_MsgContextType *msgContext,
                                      Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DspRequestUpload(Dcm_MsgContextType *msgContext,
                                    Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DspTransferData(Dcm_MsgContextType *msgContext,
                                   Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DspRequestTransferExit(Dcm_MsgContextType *msgContext,
                                          Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DspEcuReset(Dcm_MsgContextType *msgContext, Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DspReadDataByIdentifier(Dcm_MsgContextType *msgContext,
                                           Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DspWriteDataByIdentifier(Dcm_MsgContextType *msgContext,
                                            Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DspCommunicationControl(Dcm_MsgContextType *msgContext,
                                           Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DspTesterPresent(Dcm_MsgContextType *msgContext,
                                    Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DspControlDTCSetting(Dcm_MsgContextType *msgContext,
                                        Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DspClearDTC(Dcm_MsgContextType *msgContext, Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DspReadDTCInformation(Dcm_MsgContextType *msgContext,
                                         Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dem_DspReportNumberOfDTCByStatusMask(Dcm_MsgContextType *msgContext,
                                                    Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dem_DspReportNumberOfMirrorMemoryDTCByStatusMask(Dcm_MsgContextType *msgContext,
                                                                Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dem_DspReportDTCByStatusMask(Dcm_MsgContextType *msgContext,
                                            Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dem_DspReportMirrorMemoryDTCByStatusMask(Dcm_MsgContextType *msgContext,
                                                        Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dem_DspReportDTCSnapshotIdentification(Dcm_MsgContextType *msgContext,
                                                      Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dem_DspReportDTCSnapshotRecordByDTCNumber(Dcm_MsgContextType *msgContext,
                                                         Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dem_DspReportDTCExtendedDataRecordByDTCNumber(Dcm_MsgContextType *msgContext,
                                                             Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType
Dem_DspReportMirrorMemoryDTCExtendedDataRecordByDTCNumber(Dcm_MsgContextType *msgContext,
                                                          Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DspIOControlByIdentifier(Dcm_MsgContextType *msgContext,
                                            Dcm_NegativeResponseCodeType *nrc);
void Dcm_DslInit(void);
void Dcm_DspInit(void);
void Dcm_DslMainFunction(void);
Std_ReturnType Dcm_DslIsSessionSupported(Dcm_SesCtrlType sesCtrl, uint8_t sesMask);
Std_ReturnType Dcm_DslServiceSesSecPhyFuncCheck(Dcm_ContextType *context,
                                                const Dcm_SesSecAccessType *sesSecAccess,
                                                Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DslServiceSubFuncSesSecPhyFuncCheck(Dcm_ContextType *context,
                                                       const Dcm_SesSecAccessType *sesSecAccess,
                                                       Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DslServiceDIDSesSecPhyFuncCheck(Dcm_ContextType *context,
                                                   const Dcm_SesSecAccessType *sesSecAccess,
                                                   Dcm_NegativeResponseCodeType *nrc);

const Dcm_ServiceTableType *Dcm_GetActiveServiceTable(Dcm_ContextType *context,
                                                      const Dcm_ConfigType *config);

Std_ReturnType Dcm_DslSecurityAccessRequestSeed(Dcm_MsgContextType *msgContext,
                                                const Dcm_SecLevelConfigType *secLevelConfig,
                                                Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DslSecurityAccessCompareKey(Dcm_MsgContextType *msgContext,
                                               const Dcm_SecLevelConfigType *secLevelConfig,
                                               Dcm_NegativeResponseCodeType *nrc);
void Dcm_ReadPeriodicDID_Init(void);
void Dcm_MainFunction_ReadPeriodicDID(void);
void Dcm_ReadPeriodicDID_OnSessionSecurityChange(void);
Std_ReturnType Dcm_DspReadDataByPeriodicIdentifier(Dcm_MsgContextType *msgContext,
                                                   Dcm_NegativeResponseCodeType *nrc);

Std_ReturnType Dcm_DspReadMemoryByAddress(Dcm_MsgContextType *msgContext,
                                          Dcm_NegativeResponseCodeType *nrc);
Std_ReturnType Dcm_DspWriteMemoryByAddress(Dcm_MsgContextType *msgContext,
                                           Dcm_NegativeResponseCodeType *nrc);

Std_ReturnType Dcm_DspDynamicallyDefineDataIdentifier(Dcm_MsgContextType *msgContext,
                                                      Dcm_NegativeResponseCodeType *nrc);

Std_ReturnType Dcm_DspReadDDDID(const Dcm_DDDIDConfigType *DDConfig, Dcm_OpStatusType opStatus,
                                uint8_t *data, uint16_t length, Dcm_NegativeResponseCodeType *nrc);

Std_ReturnType Dcm_DspAuthentication(Dcm_MsgContextType *msgContext,
                                     Dcm_NegativeResponseCodeType *nrc);

Std_ReturnType Dcm_DspDeAuthentication(Dcm_OpStatusType OpStatus, const uint8_t *dataIn,
                                       uint16_t dataInLen, uint8_t *dataOut, uint16_t *dataOutLen,
                                       Dcm_NegativeResponseCodeType *ErrorCode);
Std_ReturnType Dcm_DspVerifyCertificateUnidirectional(Dcm_OpStatusType OpStatus,
                                                      const uint8_t *dataIn, uint16_t dataInLen,
                                                      uint8_t *dataOut, uint16_t *dataOutLen,
                                                      Dcm_NegativeResponseCodeType *ErrorCode);
Std_ReturnType Dcm_DspVerifyCertificateBidirectional(Dcm_OpStatusType OpStatus,
                                                     const uint8_t *dataIn, uint16_t dataInLen,
                                                     uint8_t *dataOut, uint16_t *dataOutLen,
                                                     Dcm_NegativeResponseCodeType *ErrorCode);
Std_ReturnType Dcm_DspProofOfOwnership(Dcm_OpStatusType OpStatus, const uint8_t *dataIn,
                                       uint16_t dataInLen, uint8_t *dataOut, uint16_t *dataOutLen,
                                       Dcm_NegativeResponseCodeType *ErrorCode);
Std_ReturnType Dcm_DspTransmitCertificate(Dcm_OpStatusType OpStatus, const uint8_t *dataIn,
                                          uint16_t dataInLen, uint8_t *dataOut,
                                          uint16_t *dataOutLen,
                                          Dcm_NegativeResponseCodeType *ErrorCode);
Std_ReturnType Dcm_DspRequestChallengeForAuthentication(Dcm_OpStatusType OpStatus,
                                                        const uint8_t *dataIn, uint16_t dataInLen,
                                                        uint8_t *dataOut, uint16_t *dataOutLen,
                                                        Dcm_NegativeResponseCodeType *ErrorCode);
Std_ReturnType Dcm_DspVerifyProofOfOwnershipUnidirectional(Dcm_OpStatusType OpStatus,
                                                           const uint8_t *dataIn,
                                                           uint16_t dataInLen, uint8_t *dataOut,
                                                           uint16_t *dataOutLen,
                                                           Dcm_NegativeResponseCodeType *ErrorCode);
Std_ReturnType Dcm_DspVerifyProofOfOwnershipBidirectional(Dcm_OpStatusType OpStatus,
                                                          const uint8_t *dataIn, uint16_t dataInLen,
                                                          uint8_t *dataOut, uint16_t *dataOutLen,
                                                          Dcm_NegativeResponseCodeType *ErrorCode);
Std_ReturnType Dcm_DspAuthenticationConfiguration(Dcm_OpStatusType OpStatus, const uint8_t *dataIn,
                                                  uint16_t dataInLen, uint8_t *dataOut,
                                                  uint16_t *dataOutLen,
                                                  Dcm_NegativeResponseCodeType *ErrorCode);
#endif /* DCM_INTERNAL_H */
