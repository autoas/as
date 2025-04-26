/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef SECOC_PRIV_H
#define SECOC_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef DET_THIS_MODULE_ID
#define DET_THIS_MODULE_ID MODULE_ID_SECOC
#endif

#define SECOC_TXPDU_PROC_STATE_IDLE ((SecOC_TxPduProcStateType)0)
#define SECOC_TXPDU_PROC_STATE_REQUEST ((SecOC_TxPduProcStateType)1)
#define SECOC_TXPDU_PROC_STATE_ASSEMBLE ((SecOC_TxPduProcStateType)2)
#define SECOC_TXPDU_PROC_STATE_READY ((SecOC_TxPduProcStateType)3)
#define SECOC_TXPDU_PROC_STATE_WAIT_TX_DONE ((SecOC_TxPduProcStateType)4)

#define SECOC_RXPDU_PROC_STATE_IDLE ((SecOC_RxPduProcStateType)0)
#define SECOC_RXPDU_PROC_STATE_RECEIVED ((SecOC_RxPduProcStateType)1)
#define SECOC_RXPDU_PROC_STATE_ASSEMBLE ((SecOC_RxPduProcStateType)2)
#define SECOC_RXPDU_PROC_STATE_CHECK ((SecOC_RxPduProcStateType)3)
#define SECOC_RXPDU_PROC_STATE_TP_RECV ((SecOC_RxPduProcStateType)4)

#define SECOC_RX_OVERFLOW_QUEUE ((SecOC_ReceptionOverflowStrategyType)0x0)
#define SECOC_RX_OVERFLOW_REJECT ((SecOC_ReceptionOverflowStrategyType)0x1)
#define SECOC_RX_OVERFLOW_REPLACE ((SecOC_ReceptionOverflowStrategyType)0x2)
/* ================================ [ TYPES     ] ============================================== */
typedef Std_ReturnType (*SecOC_GetFreshnessFncType)(uint8_t *FreshnessValue,
                                                    uint32_t *FreshnessValueLength);

typedef struct {
  SecOC_GetFreshnessFncType GetFreshnessFnc;
  uint16_t length; /* in bits */
} SecOC_FreshnessValueType;

typedef uint8_t SecOC_TxPduProcStateType;

typedef struct {
  PduLengthType SduLength;
  SecOC_TxPduProcStateType state;
} SecOC_TxPduProcContextType;

typedef struct { /* @ECUC_SecOC_00012 */
  SecOC_TxPduProcContextType *context;
  uint8_t *buffer;                 /* working buffer */
  uint32_t TxAuthServiceConfigRef; /* Symbolic name reference to CsmJob */
  PduIdType FwTxPduId;             /* forward it to lower layer */
  PduIdType UpTxPduId;             /* notify upper layer*/
  uint16_t bufLen;                 /* working buffer length */
  uint16_t AuthInfoLength;
  uint16_t AuthInfoTruncLength; /* The length in bits of the authentication code */
  uint16_t DataId;
  uint16_t FreshnessValueId;
  uint8_t FreshnessValueLength; /* The length in bits of the Freshness Value, range 0 .. 64 */
  uint8_t FreshnessValueTruncLength;
  uint8_t TxPduUnusedAreasDefault;
  uint8_t AuthPduHeaderLength;
  uint8_t AuthPduOffset; /* AuthPduOffset > 2 for DataId and AuthPduOffset > AuthPduHeaderLength */
#if 0
  boolean ProvideTxTruncatedFreshnessValue;
  boolean ReAuthenticateAfterTriggerTransmit;
  boolean UseTxConfirmation;
#endif
} SecOC_TxPduProcessingType;

typedef uint8_t SecOC_ReceptionOverflowStrategyType;

typedef uint8_t SecOC_RxPduProcStateType;

typedef struct {
  PduLengthType SduLength;
  SecOC_RxPduProcStateType state;
} SecOC_RxPduProcContextType;

typedef struct { /* ECUC_SecOC_00011 */
  SecOC_RxPduProcContextType *context;
  uint8_t *buffer;                 /* working buffer */
  uint32_t RxAuthServiceConfigRef; /* Symbolic name reference to CsmJob */
  PduIdType UpRxPduId;             /* notify upper layer*/
  uint16_t bufLen;                 /* working buffer length */
  uint16_t AuthDataFreshnessLen;
  uint16_t AuthDataFreshnessStartPosition;
#if 0
  uint16_t AuthenticationBuildAttempts;
  uint16_t AuthenticationVerifyAttempts;
#endif
  uint16_t AuthInfoLength;
  uint16_t AuthInfoTruncLength;
  uint16_t DataId;
  uint16_t FreshnessValueId;
  uint8_t FreshnessValueLength;
  uint8_t FreshnessValueTruncLength;
  uint8_t AuthPduHeaderLength;
#if 0
  uint8_t SecOC_ReceptionOverflowStrategyType ReceptionOverflowStrategy;
  uint8_t ReceptionQueueSize;
  boolean DynamicRuntimeLengthHandling;
#endif
  /** @param UseAuthDataFreshness
   * A Boolean value that indicates if a part of the Authentic-PDU shall be passed on
   * to the SWC that verifies and generates the Freshness. If it is set to TRUE, the
   * values AuthDataFreshnessStartPosition and AuthDataFreshnessLen must be set to
   * specify the bit position and length within the Authentic-PDU.
   */
  boolean UseAuthDataFreshness;
} SecOC_RxPduProcessingType;

struct SecOC_Config_s {
  const SecOC_FreshnessValueType *FrVals;
  const SecOC_TxPduProcessingType *TxPduProcs;
  const SecOC_RxPduProcessingType *RxPduProcs;
  uint16_t numTxPduProcs;
  uint16_t numRxPduProcs;
  uint16_t numFrVals;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
const SecOC_ConfigType* SecOC_GetConfig(void);
#endif /* SECOC_PRIV_H */
