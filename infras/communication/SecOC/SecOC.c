/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Secure Onboard Communication AUTOSAR CP R23-11
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "SecOC.h"
#include "SecOC_Priv.h"
#include "SecOC_Cfg.h"
#include "PduR_SecOC.h"
#include "Det.h"
#include "Csm.h"
#include "Std_Debug.h"
#include "Std_Bit.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#ifdef SECOC_USE_PB_CONFIG
#define SECOC_CONFIG secocConfig
#else
#define SECOC_CONFIG (&SecOC_Config)
#endif

#define AS_LOG_SECOC 0
#define AS_LOG_SECOCE 3
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const SecOC_ConfigType SecOC_Config;
/* ================================ [ DATAS     ] ============================================== */
#ifdef SECOC_USE_PB_CONFIG
static const SecOC_ConfigType *secocConfig = NULL;
#endif
/* ================================ [ LOCALS    ] ============================================== */
static void SecOC_BitMove(uint8_t *buffer, uint16_t toBitPos, uint16_t fromBitPos,
                          uint16_t bitSize) {
  if (toBitPos != fromBitPos) {
    Std_BitCopy(buffer, toBitPos, buffer, fromBitPos, bitSize);
  }
}

static void SecOC_ProcTx(PduIdType txPduId) {
  Std_ReturnType ret = E_OK;
  const SecOC_TxPduProcessingType *TxPduProc;
  uint32_t FreshnessValueLength;
  uint16_t dataLength;
  uint32_t macLength;
  uint32_t bitPos;
  uint8_t i;
  PduInfoType PduInfo;
  TxPduProc = &SECOC_CONFIG->TxPduProcs[txPduId];
  if (SECOC_TXPDU_PROC_STATE_REQUEST == TxPduProc->Context->State) {
    /* @SWS_SecOC_00034 */
    /* DataToAuthenticator = Data Identifier | Authentic I-PDU | Complete Freshness Value */
    TxPduProc->Buffer[TxPduProc->AuthPduOffset - 2u] = (TxPduProc->DataId >> 8) & 0xFFu;
    TxPduProc->Buffer[TxPduProc->AuthPduOffset - 1u] = TxPduProc->DataId & 0xFFu;
    FreshnessValueLength = TxPduProc->FreshnessValueLength;
    if (FreshnessValueLength > 0u) {
      ret = SecOC_GetTxFreshness(
        TxPduProc->FreshnessValueId,
        &TxPduProc->Buffer[TxPduProc->AuthPduOffset + TxPduProc->Context->SduLength],
        &FreshnessValueLength);
    }
    if (E_OK == ret) {
      dataLength = 2u + TxPduProc->Context->SduLength + ((FreshnessValueLength + 7u) >> 3);
      macLength = (TxPduProc->AuthInfoLength + 7u) >> 3;
      ret =
        Csm_MacGenerate(TxPduProc->TxAuthServiceConfigRef, CRYPTO_OPERATION_MODE_START,
                        &TxPduProc->Buffer[TxPduProc->AuthPduOffset - 2u], dataLength,
                        &TxPduProc->Buffer[TxPduProc->AuthPduOffset - 2u + dataLength], &macLength);
    }
    if (E_OK == ret) {
      ret =
        Csm_MacGenerate(TxPduProc->TxAuthServiceConfigRef, CRYPTO_OPERATION_MODE_UPDATE,
                        &TxPduProc->Buffer[TxPduProc->AuthPduOffset - 2u], dataLength,
                        &TxPduProc->Buffer[TxPduProc->AuthPduOffset - 2u + dataLength], &macLength);
    }
    if (E_OK == ret) {
      ret =
        Csm_MacGenerate(TxPduProc->TxAuthServiceConfigRef, CRYPTO_OPERATION_MODE_FINISH,
                        &TxPduProc->Buffer[TxPduProc->AuthPduOffset - 2u], dataLength,
                        &TxPduProc->Buffer[TxPduProc->AuthPduOffset - 2u + dataLength], &macLength);
    }

    if (E_OK == ret) {
      TxPduProc->Context->State = SECOC_TXPDU_PROC_STATE_ASSEMBLE;
    } else {
      ASLOG(SECOCE, ("[%u] TxPdu update FAIL\n", txPduId));
    }
  }

  if (SECOC_TXPDU_PROC_STATE_ASSEMBLE == TxPduProc->Context->State) {
    /* @SWS_SecOC_00262 */
    dataLength = TxPduProc->Context->SduLength;
    for (i = 0; i < TxPduProc->AuthPduHeaderLength; i++) {
      TxPduProc->Buffer[TxPduProc->AuthPduOffset - 1u - i] = dataLength & 0xFFu;
      dataLength = dataLength >> 8;
    }
    /* @SWS_SecOC_00261, @SWS_SecOC_00037 */
    /* SecuredPDU = SecuredIPDUHeader (optional) | AuthenticIPDU | FreshnessValue
     * [SecOCFreshnessValueTruncLength] (optional) | Authenticator [SecOCAuthInfoTruncLength] */
    bitPos = (TxPduProc->AuthPduOffset + TxPduProc->Context->SduLength) * 8u;
    SecOC_BitMove(TxPduProc->Buffer, bitPos,
                  bitPos + FreshnessValueLength - TxPduProc->FreshnessValueTruncLength,
                  TxPduProc->FreshnessValueTruncLength);
    bitPos += TxPduProc->FreshnessValueTruncLength;
    SecOC_BitMove(TxPduProc->Buffer, bitPos,
                  bitPos + FreshnessValueLength - TxPduProc->FreshnessValueTruncLength,
                  TxPduProc->AuthInfoTruncLength);
    bitPos += TxPduProc->AuthInfoTruncLength;
    TxPduProc->Context->State = SECOC_TXPDU_PROC_STATE_READY;
  }

  if (SECOC_TXPDU_PROC_STATE_READY == TxPduProc->Context->State) {
    PduInfo.SduDataPtr =
      &TxPduProc->Buffer[TxPduProc->AuthPduOffset - TxPduProc->AuthPduHeaderLength];
    PduInfo.SduLength =
      TxPduProc->Context->SduLength + TxPduProc->AuthPduHeaderLength +
      (((uint16_t)TxPduProc->FreshnessValueTruncLength + TxPduProc->AuthInfoTruncLength + 7u) >> 3);
    ret = PduR_SecOCTransmit(TxPduProc->FwTxPduId, &PduInfo);
    if (E_OK == ret) {
      TxPduProc->Context->State = SECOC_TXPDU_PROC_STATE_WAIT_TX_DONE;
    }
#ifdef SECOC_SELF_TEST
    SecOC_RxIndication(txPduId, &PduInfo);
#endif
  }
}

static void SecOC_ProcRx(PduIdType rxPduId) {
  Std_ReturnType ret = E_OK;
  const SecOC_RxPduProcessingType *RxPduProc;
  uint32_t FreshnessValueLength;
  uint16_t dataLength;
  uint32_t macLength;
  uint32_t bitPos;
  uint8_t i;
  PduInfoType PduInfo;

  RxPduProc = &SECOC_CONFIG->RxPduProcs[rxPduId];
  if (SECOC_RXPDU_PROC_STATE_RECEIVED == RxPduProc->Context->State) {
    macLength =
      (((uint16_t)RxPduProc->FreshnessValueTruncLength + RxPduProc->AuthInfoTruncLength + 7) >> 3);
    if (RxPduProc->AuthPduHeaderLength != 0) {
      dataLength = 0;
      for (i = 0; i < RxPduProc->AuthPduHeaderLength; i++) {
        dataLength = (dataLength << 8) | RxPduProc->Buffer[2u + i];
      }
    } else {
      dataLength = RxPduProc->Context->SduLength - macLength;
    }
    ASLOG(SECOC, ("[%u] RxPdu data length = %u, backup %u auth info to pos %u\n", rxPduId,
                  dataLength, macLength, RxPduProc->BufLen - macLength));
    /* backup authentication information to the end of the woking buffer */
    (void)memcpy(&RxPduProc->Buffer[RxPduProc->BufLen - macLength],
                 &RxPduProc->Buffer[2u + RxPduProc->AuthPduHeaderLength + dataLength], macLength);

    /* DataToAuthenticator = Data Identifier | Authentic I-PDU | Complete Freshness Value */
    RxPduProc->Buffer[2u + RxPduProc->AuthPduHeaderLength - 2u] = (RxPduProc->DataId >> 8) & 0xFFu;
    RxPduProc->Buffer[2u + RxPduProc->AuthPduHeaderLength - 1u] = RxPduProc->DataId & 0xFFu;
    FreshnessValueLength = RxPduProc->FreshnessValueLength;
    if (FreshnessValueLength > 0u) {
      ret =
        SecOC_GetTxFreshness(RxPduProc->FreshnessValueId,
                             &RxPduProc->Buffer[2u + RxPduProc->AuthPduHeaderLength + dataLength],
                             &FreshnessValueLength);
    }
    if (E_OK == ret) {
      macLength = (RxPduProc->AuthInfoLength + 7u) >> 3;
      ret = Csm_MacGenerate(RxPduProc->RxAuthServiceConfigRef, CRYPTO_OPERATION_MODE_START,
                            &RxPduProc->Buffer[RxPduProc->AuthPduHeaderLength],
                            2u + dataLength + ((FreshnessValueLength + 7u) >> 3),
                            &RxPduProc->Buffer[RxPduProc->AuthPduHeaderLength + 2u + dataLength +
                                               ((FreshnessValueLength + 7u) >> 3)],
                            &macLength);
    }
    if (E_OK == ret) {
      ret = Csm_MacGenerate(RxPduProc->RxAuthServiceConfigRef, CRYPTO_OPERATION_MODE_UPDATE,
                            &RxPduProc->Buffer[RxPduProc->AuthPduHeaderLength],
                            2u + dataLength + ((FreshnessValueLength + 7u) >> 3),
                            &RxPduProc->Buffer[RxPduProc->AuthPduHeaderLength + 2u + dataLength +
                                               ((FreshnessValueLength + 7u) >> 3)],
                            &macLength);
    }
    if (E_OK == ret) {
      ret = Csm_MacGenerate(RxPduProc->RxAuthServiceConfigRef, CRYPTO_OPERATION_MODE_FINISH,
                            &RxPduProc->Buffer[RxPduProc->AuthPduHeaderLength],
                            2u + dataLength + ((FreshnessValueLength + 7u) >> 3),
                            &RxPduProc->Buffer[RxPduProc->AuthPduHeaderLength + 2u + dataLength +
                                               ((FreshnessValueLength + 7u) >> 3)],
                            &macLength);
    }

    if (E_OK == ret) {
      /* assemble */
      bitPos = (2u + RxPduProc->AuthPduHeaderLength + dataLength) * 8u;
      SecOC_BitMove(RxPduProc->Buffer, bitPos,
                    bitPos + FreshnessValueLength - RxPduProc->FreshnessValueTruncLength,
                    RxPduProc->FreshnessValueTruncLength);
      bitPos += RxPduProc->FreshnessValueTruncLength;
      SecOC_BitMove(RxPduProc->Buffer, bitPos,
                    bitPos + FreshnessValueLength - RxPduProc->FreshnessValueTruncLength,
                    RxPduProc->AuthInfoTruncLength);
      macLength =
        (((uint16_t)RxPduProc->FreshnessValueTruncLength + RxPduProc->AuthInfoTruncLength + 7) >>
         3);
      for (i = 0; i < macLength; i++) {
        if (RxPduProc->Buffer[2u + RxPduProc->AuthPduHeaderLength + dataLength + i] !=
            RxPduProc->Buffer[RxPduProc->BufLen - macLength + i]) {
          ret = E_NOT_OK;
          break;
        }
      }
    }

    if (E_OK != ret) {
      ASLOG(SECOCE, ("[%u] RxPdu verify FAIL\n", rxPduId));
    } else {
      ASLOG(SECOC, ("[%u] RxPdu verify PASS\n", rxPduId));
      PduInfo.SduDataPtr = &RxPduProc->Buffer[2u + RxPduProc->AuthPduHeaderLength];
      PduInfo.SduLength = dataLength;
      PduR_SecOCRxIndication(RxPduProc->UpRxPduId, &PduInfo);
    }
    RxPduProc->Context->State = SECOC_RXPDU_PROC_STATE_IDLE;
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
const SecOC_ConfigType *SecOC_GetConfig(void) {
  return SECOC_CONFIG;
}

void SecOC_Init(const SecOC_ConfigType *config) {
  uint16_t i;
  const SecOC_TxPduProcessingType *TxPduProc;
  const SecOC_RxPduProcessingType *RxPduProc;
#ifdef SECOC_USE_PB_CONFIG
  if (NULL != config) {
    SECOC_CONFIG = config;
  } else {
    SECOC_CONFIG = &SecOC_Config;
  }
#else
  (void)config;
#endif

  for (i = 0; i < SECOC_CONFIG->NumTxPduProcs; i++) {
    TxPduProc = &SECOC_CONFIG->TxPduProcs[i];
    TxPduProc->Context->State = SECOC_TXPDU_PROC_STATE_IDLE;
  }

  for (i = 0; i < SECOC_CONFIG->NumRxPduProcs; i++) {
    RxPduProc = &SECOC_CONFIG->RxPduProcs[i];
    RxPduProc->Context->State = SECOC_RXPDU_PROC_STATE_IDLE;
  }
}

Std_ReturnType SecOC_IfTransmit(PduIdType txPduId, const PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_OK;
  const SecOC_TxPduProcessingType *TxPduProc;
  DET_VALIDATE(NULL != SECOC_CONFIG, 0x49, SECOC_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(txPduId < SECOC_CONFIG->NumTxPduProcs, 0x49, SECOC_E_INVALID_PDU_SDU_ID,
               return E_NOT_OK);
  TxPduProc = &SECOC_CONFIG->TxPduProcs[txPduId];
  DET_VALIDATE((NULL != pduInfoPtr) && (NULL != pduInfoPtr->SduDataPtr) &&
                 (pduInfoPtr->SduLength > 0),
               0x49, SECOC_E_PARAM_POINTER, return E_NOT_OK);
  DET_VALIDATE((pduInfoPtr->SduLength + TxPduProc->AuthPduOffset +
                ((TxPduProc->FreshnessValueLength + 7) / 8) +
                ((TxPduProc->AuthInfoLength + 7) / 8)) < TxPduProc->BufLen,
               0x49, SECOC_E_PARAM_POINTER, return E_NOT_OK);
  (void)memcpy(&TxPduProc->Buffer[TxPduProc->AuthPduOffset], pduInfoPtr->SduDataPtr,
               pduInfoPtr->SduLength);

  TxPduProc->Context->State = SECOC_TXPDU_PROC_STATE_REQUEST;
  TxPduProc->Context->SduLength = pduInfoPtr->SduLength;
  return ret;
}

void SecOC_TxConfirmation(PduIdType txPduId, Std_ReturnType result) {
  const SecOC_TxPduProcessingType *TxPduProc;
  DET_VALIDATE(NULL != SECOC_CONFIG, 0x40, SECOC_E_UNINIT, return);
  DET_VALIDATE(txPduId < SECOC_CONFIG->NumTxPduProcs, 0x40, SECOC_E_INVALID_PDU_SDU_ID, return);

  TxPduProc = &SECOC_CONFIG->TxPduProcs[txPduId];
  if (SECOC_TXPDU_PROC_STATE_WAIT_TX_DONE == TxPduProc->Context->State) {
    TxPduProc->Context->State = SECOC_TXPDU_PROC_STATE_IDLE;
    ASLOG(SECOC, ("[%u] Tx Confirm result=%u\n", txPduId, result));
    PduR_SecOCTxConfirmation(TxPduProc->UpTxPduId, result);
  } else {
    ASLOG(SECOCE, ("[%u] TxPdu in state %u\n", txPduId, TxPduProc->Context->State));
  }
}

void SecOC_RxIndication(PduIdType rxPduId, const PduInfoType *pduInfoPtr) {
  const SecOC_RxPduProcessingType *RxPduProc;
  DET_VALIDATE(NULL != SECOC_CONFIG, 0x42, SECOC_E_UNINIT, return);
  DET_VALIDATE(rxPduId < SECOC_CONFIG->NumRxPduProcs, 0x42, SECOC_E_INVALID_PDU_SDU_ID, return);
  DET_VALIDATE((NULL != pduInfoPtr) && (NULL != pduInfoPtr->SduDataPtr) &&
                 (pduInfoPtr->SduLength > 0),
               0x42, SECOC_E_PARAM_POINTER, return);

  RxPduProc = &SECOC_CONFIG->RxPduProcs[rxPduId];
  DET_VALIDATE((pduInfoPtr->SduLength + 2u + ((RxPduProc->FreshnessValueLength + 7) / 8) +
                ((RxPduProc->AuthInfoLength + 7) / 8)) < RxPduProc->BufLen,
               0x42, SECOC_E_PARAM_POINTER, return);
  if (SECOC_RXPDU_PROC_STATE_IDLE != RxPduProc->Context->State) {
    ASLOG(SECOCE, ("[%u] RxPdu in state %u\n", rxPduId, RxPduProc->Context->State));
  }

  (void)memcpy(&RxPduProc->Buffer[2u], pduInfoPtr->SduDataPtr, pduInfoPtr->SduLength);
  RxPduProc->Context->SduLength = pduInfoPtr->SduLength;
  RxPduProc->Context->State = SECOC_RXPDU_PROC_STATE_RECEIVED;
}

BufReq_ReturnType SecOC_CopyTxData(PduIdType txPduId, const PduInfoType *info,
                                   const RetryInfoType *retry, PduLengthType *availableDataPtr) {
  BufReq_ReturnType bufReq = BUFREQ_E_NOT_OK;
  PduLengthType offset = 0;
  const SecOC_TxPduProcessingType *TxPduProc;
  PduLengthType dataLength;
  DET_VALIDATE(NULL != SECOC_CONFIG, 0x43, SECOC_E_UNINIT, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(txPduId < SECOC_CONFIG->NumTxPduProcs, 0x43, SECOC_E_INVALID_PDU_SDU_ID,
               return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != info->MetaDataPtr, 0x43, SECOC_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);
  TxPduProc = &SECOC_CONFIG->TxPduProcs[txPduId];
  if ((SECOC_TXPDU_PROC_STATE_READY == TxPduProc->Context->State) ||
      (SECOC_TXPDU_PROC_STATE_WAIT_TX_DONE == TxPduProc->Context->State)) {
    offset = *(PduLengthType *)info->MetaDataPtr;
    dataLength =
      TxPduProc->Context->SduLength + TxPduProc->AuthPduHeaderLength +
      (((uint16_t)TxPduProc->FreshnessValueTruncLength + TxPduProc->AuthInfoTruncLength + 7u) >> 3);
    if ((offset + info->SduLength) <= dataLength) {
      (void)memcpy(
        info->SduDataPtr,
        &TxPduProc->Buffer[TxPduProc->AuthPduOffset - TxPduProc->AuthPduHeaderLength + offset],
        info->SduLength);
      *availableDataPtr = dataLength - (offset + info->SduLength);
      bufReq = BUFREQ_OK;
    }
  } else {
    ASLOG(SECOCE, ("[%u] CopyTx in state %u\n", txPduId, TxPduProc->Context->State));
  }
  return bufReq;
}

BufReq_ReturnType SecOC_StartOfReception(PduIdType rxPduId, const PduInfoType *info,
                                         PduLengthType tpSduLength, PduLengthType *bufferSizePtr) {
  BufReq_ReturnType bufReq = BUFREQ_E_NOT_OK;
  const SecOC_RxPduProcessingType *RxPduProc;
  DET_VALIDATE(NULL != SECOC_CONFIG, 0x46, SECOC_E_UNINIT, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(rxPduId < SECOC_CONFIG->NumRxPduProcs, 0x46, SECOC_E_INVALID_PDU_SDU_ID,
               return BUFREQ_E_NOT_OK);
  DET_VALIDATE(tpSduLength > 0, 0x46, SECOC_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);
  (void)info;
  RxPduProc = &SECOC_CONFIG->RxPduProcs[rxPduId];
  DET_VALIDATE((tpSduLength + 2u + ((RxPduProc->FreshnessValueLength + 7) / 8) +
                ((RxPduProc->AuthInfoLength + 7) / 8)) < RxPduProc->BufLen,
               0x46, SECOC_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);
  if (SECOC_RXPDU_PROC_STATE_IDLE != RxPduProc->Context->State) {
    ASLOG(SECOCE, ("[%u] Start Rx in state %u\n", rxPduId, RxPduProc->Context->State));
  } else {
    RxPduProc->Context->SduLength = tpSduLength;
    RxPduProc->Context->State = SECOC_RXPDU_PROC_STATE_TP_RECV;
    *bufferSizePtr = RxPduProc->BufLen - 2u - ((RxPduProc->FreshnessValueLength + 7) / 8) -
                     ((RxPduProc->AuthInfoLength + 7) / 8);
    bufReq = BUFREQ_OK;
  }

  return bufReq;
}

BufReq_ReturnType SecOC_CopyRxData(PduIdType rxPduId, const PduInfoType *info,
                                   PduLengthType *bufferSizePtr) {
  BufReq_ReturnType bufReq = BUFREQ_E_NOT_OK;
  PduLengthType offset = 0;
  const SecOC_RxPduProcessingType *RxPduProc;

  DET_VALIDATE(NULL != SECOC_CONFIG, 0x44, SECOC_E_UNINIT, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(rxPduId < SECOC_CONFIG->NumRxPduProcs, 0x44, SECOC_E_INVALID_PDU_SDU_ID,
               return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != bufferSizePtr, 0x44, SECOC_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != info, 0x44, SECOC_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != info->MetaDataPtr, 0x44, SECOC_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);
  RxPduProc = &SECOC_CONFIG->RxPduProcs[rxPduId];
  if (SECOC_RXPDU_PROC_STATE_TP_RECV != RxPduProc->Context->State) {
    ASLOG(SECOCE, ("[%u] CopyRx in state %u\n", rxPduId, RxPduProc->Context->State));
  } else {
    offset = *(PduLengthType *)info->MetaDataPtr;
    (void)memcpy(&RxPduProc->Buffer[2u + offset], info->SduDataPtr, info->SduLength);
    bufReq = BUFREQ_OK;
  }
  return bufReq;
}

void SecOC_TpRxIndication(PduIdType rxPduId, Std_ReturnType result) {
  const SecOC_RxPduProcessingType *RxPduProc;
  DET_VALIDATE(NULL != SECOC_CONFIG, 0x45, SECOC_E_UNINIT, return);
  DET_VALIDATE(rxPduId < SECOC_CONFIG->NumRxPduProcs, 0x45, SECOC_E_INVALID_PDU_SDU_ID, return);

  RxPduProc = &SECOC_CONFIG->RxPduProcs[rxPduId];
  if (SECOC_RXPDU_PROC_STATE_TP_RECV != RxPduProc->Context->State) {
    ASLOG(SECOCE, ("[%u] TpRx in state %u\n", rxPduId, RxPduProc->Context->State));
  } else {
    if (E_OK == result) {
      RxPduProc->Context->State = SECOC_RXPDU_PROC_STATE_RECEIVED;
    } else {
      RxPduProc->Context->State = SECOC_RXPDU_PROC_STATE_IDLE;
    }
  }
}

void SecOC_MainFunctionTx(void) {
  uint16_t i;
  const SecOC_TxPduProcessingType *TxPduProc;
  for (i = 0; i < SECOC_CONFIG->NumTxPduProcs; i++) {
    TxPduProc = &SECOC_CONFIG->TxPduProcs[i];
    if (SECOC_TXPDU_PROC_STATE_IDLE != TxPduProc->Context->State) {
      SecOC_ProcTx(i);
    }
  }
}

void SecOC_MainFunctionRx(void) {
  uint16_t i;
  const SecOC_RxPduProcessingType *RxPduProc;
  for (i = 0; i < SECOC_CONFIG->NumRxPduProcs; i++) {
    RxPduProc = &SECOC_CONFIG->RxPduProcs[i];
    if (SECOC_RXPDU_PROC_STATE_IDLE != RxPduProc->Context->State) {
      SecOC_ProcRx(i);
    }
  }
}

void SecOC_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x02, SECOC_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_SECOC;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 0;
}
