/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Secure Onboard Communication AUTOSAR CP R23-11
 */
#ifndef SECOC_H
#define SECOC_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* @SWS_SecOC_00101 */
#define SECOC_E_PARAM_POINTER 0x01u
#define SECOC_E_UNINIT 0x02u
#define SECOC_E_INVALID_PDU_SDU_ID 0x03u
#define SECOC_E_CRYPTO_FAILURE 0x04u
#define SECOC_E_INIT_FAILED 0x07u

/* ================================ [ TYPES     ] ============================================== */
typedef struct SecOC_Config_s SecOC_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_SecOC_00106 */
void SecOC_Init(const SecOC_ConfigType *config);

/* @SWS_SecOC_00112 */
Std_ReturnType SecOC_IfTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_SecOC_91008 */
Std_ReturnType SecOC_TpTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_SecOC_00124 */
void SecOC_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_SecOC_00126 */
void SecOC_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);

/* @SecOC_TpRxIndication */
void SecOC_TpRxIndication(PduIdType id, Std_ReturnType result);

/* @SWS_SecOC_00152 */
void SecOC_TpTxConfirmation(PduIdType id, Std_ReturnType result);

/* @SWS_SecOC_00130 */
BufReq_ReturnType SecOC_StartOfReception(PduIdType id, const PduInfoType *info,
                                         PduLengthType TpSduLength, PduLengthType *bufferSizePtr);

/* @SWS_SecOC_00128 */
BufReq_ReturnType SecOC_CopyRxData(PduIdType id, const PduInfoType *info,
                                   PduLengthType *bufferSizePtr);

/* @SWS_SecOC_00129 */
BufReq_ReturnType SecOC_CopyTxData(PduIdType id, const PduInfoType *info,
                                   const RetryInfoType *retry, PduLengthType *availableDataPtr);

/* @SWS_SecOC_91007 */
Std_ReturnType SecOC_GetRxFreshness(uint16_t SecOCFreshnessValueID,
                                    const uint8_t *SecOCTruncatedFreshnessValue,
                                    uint32_t SecOCTruncatedFreshnessValueLength,
                                    uint16_t SecOCAuthVerifyAttempts, uint8_t *SecOCFreshnessValue,
                                    uint32_t *SecOCFreshnessValueLength);

/* @SWS_SecOC_91006 */
Std_ReturnType SecOC_GetRxFreshnessAuthData(
  uint16_t SecOCFreshnessValueID, const uint8_t *SecOCTruncatedFreshnessValue,
  uint32_t SecOCTruncatedFreshnessValueLength, const uint8_t *SecOCAuthDataFreshnessValue,
  uint16_t SecOCAuthDataFreshnessValueLength, uint16_t SecOCAuthVerifyAttempts,
  uint8_t *SecOCFreshnessValue, uint32_t *SecOCFreshnessValueLength);

/* @SWS_SecOC_91004 */
/**
 * @brief This API returns the freshness value from the Most Significant Bits in the first byte in
 * the array (SecOCFreshnessValue), in big endian format
 * @param SecOCFreshnessValueLength Holds the length of the provided freshness in bits
 */
Std_ReturnType SecOC_GetTxFreshness(uint16_t SecOCFreshnessValueID, uint8_t *SecOCFreshnessValue,
                                    uint32_t *SecOCFreshnessValueLength);

/* @SWS_SecOC_91003 */
Std_ReturnType SecOC_GetTxFreshnessTruncData(uint16_t SecOCFreshnessValueID,
                                             uint8_t *SecOCFreshnessValue,
                                             uint32_t *SecOCFreshnessValueLength,
                                             uint8_t *SecOCTruncatedFreshnessValue,
                                             uint32_t *SecOCTruncatedFreshnessValueLength);

/* @SWS_SecOC_91005 */
void SecOC_SPduTxConfirmation(uint16_t SecOCFreshnessValueID);

/* @SWS_SecOC_00171 */
void SecOC_MainFunctionRx(void);

/* @SWS_SecOC_00176 */
void SecOC_MainFunctionTx(void);

/* @SWS_SecOC_00161 */
void SecOC_DeInit(void);

/* @SWS_SecOC_00107 */
void SecOC_GetVersionInfo(Std_VersionInfoType *versionInfo);
#endif /* SECOC_H */
