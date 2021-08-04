/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Diagnostic CommunicationManager AUTOSAR CP Release 4.4.0
 */
#ifndef DCM_H
#define DCM_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* For DCM physical addressing */
#define DCM_P2P_PDU 0
/* For DCM functional addressing */
#define DCM_P2A_PDU 1

/* @SWS_Dcm_00976 */
#define DCM_E_OK (Dcm_StatusType)0x00
#define DCM_E_ROE_NOT_ACCEPTED (Dcm_StatusType)0x06
#define DCM_E_PERIODICID_NOT_ACCEPTED (Dcm_StatusType)0x07
/* @SWS_Dcm_00769 */
#define DCM_E_PENDING (Std_ReturnType)10
/* SWS_Dcm_00690 */
#define DCM_E_FORCE_RCRRP (Std_ReturnType)12

/* @SWS_Dcm_91015 */
#define DCM_INITIAL 0x00
#define DCM_PENDING 0x01
#define DCM_CANCEL 0x02
#define DCM_FORCE_RCRRP_OK 0x03
#define DCM_POS_RESPONSE_SENT 0x04
#define DCM_POS_RESPONSE_FAILED 0x04
#define DCM_NEG_RESPONSE_SENT 0x06
#define DCM_NEG_RESPONSE_FAILED 0x07

#define DCM_READ_OK (Dcm_ReturnReadMemoryType)0x00
#define DCM_READ_PENDING (Dcm_ReturnReadMemoryType)0x01
#define DCM_READ_FAILED (Dcm_ReturnReadMemoryType)0x02
#define DCM_READ_FORCE_RCRRP (Dcm_ReturnReadMemoryType)0x03

#define DCM_WRITE_OK (Dcm_ReturnWriteMemoryType)0x00
#define DCM_WRITE_PENDING (Dcm_ReturnWriteMemoryType)0x01
#define DCM_WRITE_FAILED (Dcm_ReturnWriteMemoryType)0x02
#define DCM_WRITE_FORCE_RCRRP (Dcm_ReturnWriteMemoryType)0x03

#define DCM_ERASE_OK (Dcm_ReturnEraseMemoryType)0x00
#define DCM_ERASE_PENDING (Dcm_ReturnEraseMemoryType)0x01
#define DCM_ERASE_FAILED (Dcm_ReturnEraseMemoryType)0x02
#define DCM_ERASE_FORCE_RCRRP (Dcm_ReturnEraseMemoryType)0x03

#define DCM_COLD_START (Dcm_EcuStartModeType)0x00
#define DCM_WARM_START (Dcm_EcuStartModeType)0x01

#define DCM_PHYSICAL_REQUEST 0
#define DCM_FUNCTIONAL_REQUEST 1

#define DCM_NOT_SUPRESS_POSITIVE_RESPONCE 0
#define DCM_SUPRESS_POSITIVE_RESPONCE 1

/* @SWS_Dcm_00978 */
#define DCM_DEFAULT_SESSION 0x01
#define DCM_PROGRAMMING_SESSION 0x02
#define DCM_EXTENDED_DIAGNOSTIC_SESSION 0x03
#define DCM_SAFETY_SYSTEM_DIAGNOSTIC_SESSION 0x04
/* For this DCM implementation, only 4 customer defined session are supported */

/* @SWS_Dcm_00977 */
#define DCM_SEC_LEV_LOCKED 0x00
#define DCM_SEC_LEVEL1 0x01
#define DCM_SEC_LEVEL2 0x02
#define DCM_SEC_LEVEL3 0x03
#define DCM_SEC_LEVEL4 0x04
#define DCM_SEC_LEVEL5 0x04
#define DCM_SEC_LEVEL6 0x06
#define DCM_SEC_LEVEL7 0x07

/* @SWS_Dcm_00980 */
#define DCM_POS_RESP ((Dcm_NegativeResponseCodeType)0x00)
#define DCM_E_GENERAL_REJECT ((Dcm_NegativeResponseCodeType)0x10)
#define DCM_E_SERVICE_NOT_SUPPORTED ((Dcm_NegativeResponseCodeType)0x11)
#define DCM_E_SUB_FUNCTION_NOT_SUPPORTED ((Dcm_NegativeResponseCodeType)0x12)
#define DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT ((Dcm_NegativeResponseCodeType)0x13)
#define DCM_E_RESPONSE_TOO_LONG ((Dcm_NegativeResponseCodeType)0x14)
#define DCM_E_CONDITIONS_NOT_CORRECT ((Dcm_NegativeResponseCodeType)0x22)
#define DCM_E_REQUEST_SEQUENCE_ERROR ((Dcm_NegativeResponseCodeType)0x24)
#define DCM_E_REQUEST_OUT_OF_RANGE ((Dcm_NegativeResponseCodeType)0x31)
#define DCM_E_SECUTITY_ACCESS_DENIED ((Dcm_NegativeResponseCodeType)0x33)
#define DCM_E_INVALID_KEY ((Dcm_NegativeResponseCodeType)0x35)
#define DCM_E_EXCEED_NUMBER_OF_ATTEMPTS ((Dcm_NegativeResponseCodeType)0x36)
#define DCM_E_UPLOAD_DOWNLOAD_NOT_ACCEPTED ((Dcm_NegativeResponseCodeType)0x70)
#define DCM_E_TRANSFER_DATA_SUSPENDED ((Dcm_NegativeResponseCodeType)0x71)
#define DCM_E_GENERAL_PROGRAMMING_FAILURE ((Dcm_NegativeResponseCodeType)0x72)
#define DCM_E_WRONG_BLOCK_SEQUENCE_COUNTER ((Dcm_NegativeResponseCodeType)0x73)
#define DCM_E_RESPONSE_PENDING ((Dcm_NegativeResponseCodeType)0x78)
#define DCM_E_SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION ((Dcm_NegativeResponseCodeType)0x7E)
#define DCM_E_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION ((Dcm_NegativeResponseCodeType)0x7F)
/* ================================ [ TYPES     ] ==============================================
 */
/* @SWS_Dcm_00976 */
typedef uint8_t Dcm_StatusType;

/* @SWS_Dcm_91015 */
typedef uint8_t Dcm_ExtendedOpStatusType;
/* @SWS_Dcm_00527 */
typedef Dcm_ExtendedOpStatusType Dcm_OpStatusType;

/* @SWS_Dcm_00980 */
typedef uint8_t Dcm_NegativeResponseCodeType;

/* @SWS_Dcm_00985 */
typedef uint8_t Dcm_ReturnReadMemoryType;

/* @SWS_Dcm_00986 */
typedef uint8_t Dcm_ReturnWriteMemoryType;

typedef uint8_t Dcm_ReturnEraseMemoryType;

/* @SWS_Dcm_01165 */
typedef uint8_t *Dcm_RequestDataArrayType;

/* @SWS_Dcm_00987 */
typedef uint8_t Dcm_EcuStartModeType;

/* @SWS_Dcm_00989 */
typedef uint8_t Dcm_MsgItemType;

/* @SWS_Dcm_00990 */
typedef Dcm_MsgItemType *Dcm_MsgType;

/* @SWS_Dcm_00991 */
typedef uint32_t Dcm_MsgLenType;

/* @SWS_Dcm_00993 */
typedef uint8_t Dcm_IdContextType;

typedef uint8_t Dcm_SecLevelType;
typedef uint8_t Dcm_SesCtrlType;

/* @SWS_Dcm_00992 */
typedef struct {
  /*0 = physical request, 1 = functional request*/
  uint8_t reqType : 1;
  /* 0 = no (do not suppress), 1 = yes (no positive response will be sent) */
  uint8_t suppressPosResponse : 1;
} Dcm_MsgAddInfoType;

/* @SWS_Dcm_00994 */
typedef struct {
  Dcm_MsgType reqData;
  Dcm_MsgLenType reqDataLen;
  Dcm_MsgType resData;
  Dcm_MsgLenType resDataLen;
  Dcm_MsgAddInfoType msgAddInfo;
  Dcm_MsgLenType resMaxDataLen;
  Dcm_IdContextType idContext;
  PduIdType dcmRxPduId;
} Dcm_MsgContextType;

/* @SWS_Dcm_00988 */
typedef struct {
  uint16_t ConnectionId;
  uint16_t TesterAddress;
  uint8_t Sid;
  uint8_t SubFncId;
  boolean Reprograming;
  boolean ApplUpdated;
  boolean ResponseRequired;
} Dcm_ProgConditionsType;

/* @SWS_Dcm_00982 */
typedef struct Dcm_Config_s Dcm_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
extern Std_ReturnType Dcm_Transmit(const uint8_t *buffer, PduLengthType length, int functional);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_Dcm_00037 */
void Dcm_Init(const Dcm_ConfigType *ConfigPtr);
/* @SWS_Dcm_00053 */
void Dcm_MainFunction(void);
/* @SWS_Dcm_00094 */
BufReq_ReturnType Dcm_StartOfReception(PduIdType id, const PduInfoType *info,
                                       PduLengthType TpSduLength, PduLengthType *bufferSizePtr);
/* @ SWS_Dcm_00556 */
BufReq_ReturnType Dcm_CopyRxData(PduIdType id, const PduInfoType *info,
                                 PduLengthType *bufferSizePtr);
/* @SWS_Dcm_00093 */
void Dcm_TpRxIndication(PduIdType id, Std_ReturnType result);
/* @SWS_Dcm_00092 */
BufReq_ReturnType Dcm_CopyTxData(PduIdType id, const PduInfoType *info, const RetryInfoType *retry,
                                 PduLengthType *availableDataPtr);
/* @SWS_Dcm_00351 */
void Dcm_TpTxConfirmation(PduIdType id, Std_ReturnType result);
/* @SWS_Dcm_01092 */
void Dcm_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);

void Dcm_MainFunction_Request(void);
void Dcm_MainFunction_Response(void);

/* @SWS_Dcm_00338 */
Std_ReturnType Dcm_GetSecurityLevel(Dcm_SecLevelType *SecLevel);
Std_ReturnType Dcm_SetSecurityLevel(Dcm_SecLevelType SecLevel);
/* @SWS_Dcm_00339 */
Std_ReturnType Dcm_GetSesCtrlType(Dcm_SesCtrlType *SesCtrlType);
Std_ReturnType Dcm_SetSesCtrlType(Dcm_SesCtrlType SesCtrlType);
/* @SWS_Dcm_00520 */
Std_ReturnType Dcm_ResetToDefaultSession(void);

void Dcm_SessionChangeIndication(Dcm_SesCtrlType sesCtrlTypeActive, Dcm_SesCtrlType sesCtrlTypeNew);

void Dcm_PerformReset(uint8_t resetType);
#endif /* DCM_H */
