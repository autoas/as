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

static void SecOC_ProcTx(PduIdType TxPduId) {
  Std_ReturnType ret = E_OK;
  const SecOC_TxPduProcessingType *TxPduProc;
  uint32_t FreshnessValueLength;
  uint16_t dataLength;
  uint32_t macLength;
  uint32_t bitPos;
  uint8_t i;
  PduInfoType PduInfo;
  TxPduProc = &SECOC_CONFIG->TxPduProcs[TxPduId];
  if (SECOC_TXPDU_PROC_STATE_REQUEST == TxPduProc->context->state) {
    /* @SWS_SecOC_00034 */
    /* DataToAuthenticator = Data Identifier | Authentic I-PDU | Complete Freshness Value */
    TxPduProc->buffer[TxPduProc->AuthPduOffset - 2u] = (TxPduProc->DataId >> 8) & 0xFFu;
    TxPduProc->buffer[TxPduProc->AuthPduOffset - 1u] = TxPduProc->DataId & 0xFFu;
    FreshnessValueLength = TxPduProc->FreshnessValueLength;
    if (FreshnessValueLength > 0u) {
      ret = SecOC_GetTxFreshness(
        TxPduProc->FreshnessValueId,
        &TxPduProc->buffer[TxPduProc->AuthPduOffset + TxPduProc->context->SduLength],
        &FreshnessValueLength);
    }
    if (E_OK == ret) {
      dataLength = 2u + TxPduProc->context->SduLength + ((FreshnessValueLength + 7u) >> 3);
      macLength = (TxPduProc->AuthInfoLength + 7u) >> 3;
      ret =
        Csm_MacGenerate(TxPduProc->TxAuthServiceConfigRef, CRYPTO_OPERATION_MODE_START,
                        &TxPduProc->buffer[TxPduProc->AuthPduOffset - 2u], dataLength,
                        &TxPduProc->buffer[TxPduProc->AuthPduOffset - 2u + dataLength], &macLength);
    }
    if (E_OK == ret) {
      ret =
        Csm_MacGenerate(TxPduProc->TxAuthServiceConfigRef, CRYPTO_OPERATION_MODE_UPDATE,
                        &TxPduProc->buffer[TxPduProc->AuthPduOffset - 2u], dataLength,
                        &TxPduProc->buffer[TxPduProc->AuthPduOffset - 2u + dataLength], &macLength);
    }
    if (E_OK == ret) {
      ret =
        Csm_MacGenerate(TxPduProc->TxAuthServiceConfigRef, CRYPTO_OPERATION_MODE_FINISH,
                        &TxPduProc->buffer[TxPduProc->AuthPduOffset - 2u], dataLength,
                        &TxPduProc->buffer[TxPduProc->AuthPduOffset - 2u + dataLength], &macLength);
    }

    if (E_OK == ret) {
      TxPduProc->context->state = SECOC_TXPDU_PROC_STATE_ASSEMBLE;
    } else {
      ASLOG(SECOCE, ("[%u] TxPdu update FAIL\n", TxPduId));
    }
  }

  if (SECOC_TXPDU_PROC_STATE_ASSEMBLE == TxPduProc->context->state) {
    /* @SWS_SecOC_00262 */
    dataLength = TxPduProc->context->SduLength;
    for (i = 0; i < TxPduProc->AuthPduHeaderLength; i++) {
      TxPduProc->buffer[TxPduProc->AuthPduOffset - 1u - i] = dataLength & 0xFFu;
      dataLength = dataLength >> 8;
    }
    /* @SWS_SecOC_00261, @SWS_SecOC_00037 */
    /* SecuredPDU = SecuredIPDUHeader (optional) | AuthenticIPDU | FreshnessValue
     * [SecOCFreshnessValueTruncLength] (optional) | Authenticator [SecOCAuthInfoTruncLength] */
    bitPos = (TxPduProc->AuthPduOffset + TxPduProc->context->SduLength) * 8u;
    SecOC_BitMove(TxPduProc->buffer, bitPos,
                  bitPos + FreshnessValueLength - TxPduProc->FreshnessValueTruncLength,
                  TxPduProc->FreshnessValueTruncLength);
    bitPos += TxPduProc->FreshnessValueTruncLength;
    SecOC_BitMove(TxPduProc->buffer, bitPos,
                  bitPos + FreshnessValueLength - TxPduProc->FreshnessValueTruncLength,
                  TxPduProc->AuthInfoTruncLength);
    bitPos += TxPduProc->AuthInfoTruncLength;
    TxPduProc->context->state = SECOC_TXPDU_PROC_STATE_READY;
  }

  if (SECOC_TXPDU_PROC_STATE_READY == TxPduProc->context->state) {
    PduInfo.SduDataPtr =
      &TxPduProc->buffer[TxPduProc->AuthPduOffset - TxPduProc->AuthPduHeaderLength];
    PduInfo.SduLength =
      TxPduProc->context->SduLength + TxPduProc->AuthPduHeaderLength +
      (((uint16_t)TxPduProc->FreshnessValueTruncLength + TxPduProc->AuthInfoTruncLength + 7u) >> 3);
    ret = PduR_SecOCTransmit(TxPduProc->FwTxPduId, &PduInfo);
    if (E_OK == ret) {
      TxPduProc->context->state = SECOC_TXPDU_PROC_STATE_WAIT_TX_DONE;
    }
#ifdef SECOC_SELF_TEST
    SecOC_RxIndication(TxPduId, &PduInfo);
#endif
  }
}

static void SecOC_ProcRx(PduIdType RxPduId) {
  Std_ReturnType ret = E_OK;
  const SecOC_RxPduProcessingType *RxPduProc;
  uint32_t FreshnessValueLength;
  uint16_t dataLength;
  uint32_t macLength;
  uint32_t bitPos;
  uint8_t i;
  PduInfoType PduInfo;

  RxPduProc = &SECOC_CONFIG->RxPduProcs[RxPduId];
  if (SECOC_RXPDU_PROC_STATE_RECEIVED == RxPduProc->context->state) {
    macLength =
      (((uint16_t)RxPduProc->FreshnessValueTruncLength + RxPduProc->AuthInfoTruncLength + 7) >> 3);
    if (RxPduProc->AuthPduHeaderLength != 0) {
      dataLength = 0;
      for (i = 0; i < RxPduProc->AuthPduHeaderLength; i++) {
        dataLength = (dataLength << 8) | RxPduProc->buffer[2u + i];
      }
    } else {
      dataLength = RxPduProc->context->SduLength - macLength;
    }
    ASLOG(SECOC, ("[%u] RxPdu data length = %u, backup %u auth info to pos %u\n", RxPduId,
                  dataLength, macLength, RxPduProc->bufLen - macLength));
    /* backup authentication information to the end of the woking buffer */
    (void)memcpy(&RxPduProc->buffer[RxPduProc->bufLen - macLength],
                 &RxPduProc->buffer[2u + RxPduProc->AuthPduHeaderLength + dataLength], macLength);

    /* DataToAuthenticator = Data Identifier | Authentic I-PDU | Complete Freshness Value */
    RxPduProc->buffer[2u + RxPduProc->AuthPduHeaderLength - 2u] = (RxPduProc->DataId >> 8) & 0xFFu;
    RxPduProc->buffer[2u + RxPduProc->AuthPduHeaderLength - 1u] = RxPduProc->DataId & 0xFFu;
    FreshnessValueLength = RxPduProc->FreshnessValueLength;
    if (FreshnessValueLength > 0u) {
      ret =
        SecOC_GetTxFreshness(RxPduProc->FreshnessValueId,
                             &RxPduProc->buffer[2u + RxPduProc->AuthPduHeaderLength + dataLength],
                             &FreshnessValueLength);
    }
    if (E_OK == ret) {
      macLength = (RxPduProc->AuthInfoLength + 7u) >> 3;
      ret = Csm_MacGenerate(RxPduProc->RxAuthServiceConfigRef, CRYPTO_OPERATION_MODE_START,
                            &RxPduProc->buffer[RxPduProc->AuthPduHeaderLength],
                            2u + dataLength + ((FreshnessValueLength + 7u) >> 3),
                            &RxPduProc->buffer[RxPduProc->AuthPduHeaderLength + 2u + dataLength +
                                               ((FreshnessValueLength + 7u) >> 3)],
                            &macLength);
    }
    if (E_OK == ret) {
      ret = Csm_MacGenerate(RxPduProc->RxAuthServiceConfigRef, CRYPTO_OPERATION_MODE_UPDATE,
                            &RxPduProc->buffer[RxPduProc->AuthPduHeaderLength],
                            2u + dataLength + ((FreshnessValueLength + 7u) >> 3),
                            &RxPduProc->buffer[RxPduProc->AuthPduHeaderLength + 2u + dataLength +
                                               ((FreshnessValueLength + 7u) >> 3)],
                            &macLength);
    }
    if (E_OK == ret) {
      ret = Csm_MacGenerate(RxPduProc->RxAuthServiceConfigRef, CRYPTO_OPERATION_MODE_FINISH,
                            &RxPduProc->buffer[RxPduProc->AuthPduHeaderLength],
                            2u + dataLength + ((FreshnessValueLength + 7u) >> 3),
                            &RxPduProc->buffer[RxPduProc->AuthPduHeaderLength + 2u + dataLength +
                                               ((FreshnessValueLength + 7u) >> 3)],
                            &macLength);
    }

    if (E_OK == ret) {
      /* assemble */
      bitPos = (2u + RxPduProc->AuthPduHeaderLength + dataLength) * 8u;
      SecOC_BitMove(RxPduProc->buffer, bitPos,
                    bitPos + FreshnessValueLength - RxPduProc->FreshnessValueTruncLength,
                    RxPduProc->FreshnessValueTruncLength);
      bitPos += RxPduProc->FreshnessValueTruncLength;
      SecOC_BitMove(RxPduProc->buffer, bitPos,
                    bitPos + FreshnessValueLength - RxPduProc->FreshnessValueTruncLength,
                    RxPduProc->AuthInfoTruncLength);
      macLength =
        (((uint16_t)RxPduProc->FreshnessValueTruncLength + RxPduProc->AuthInfoTruncLength + 7) >>
         3);
      for (i = 0; i < macLength; i++) {
        if (RxPduProc->buffer[2u + RxPduProc->AuthPduHeaderLength + dataLength + i] !=
            RxPduProc->buffer[RxPduProc->bufLen - macLength + i]) {
          ret = E_NOT_OK;
          break;
        }
      }
    }

    if (E_OK != ret) {
      ASLOG(SECOCE, ("[%u] RxPdu verify FAIL\n", RxPduId));
    } else {
      ASLOG(SECOC, ("[%u] RxPdu verify PASS\n", RxPduId));
      PduInfo.SduDataPtr = &RxPduProc->buffer[2u + RxPduProc->AuthPduHeaderLength];
      PduInfo.SduLength = dataLength;
      PduR_SecOCRxIndication(RxPduProc->UpRxPduId, &PduInfo);
    }
    RxPduProc->context->state = SECOC_RXPDU_PROC_STATE_IDLE;
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

  for (i = 0; i < SECOC_CONFIG->numTxPduProcs; i++) {
    TxPduProc = &SECOC_CONFIG->TxPduProcs[i];
    TxPduProc->context->state = SECOC_TXPDU_PROC_STATE_IDLE;
  }

  for (i = 0; i < SECOC_CONFIG->numRxPduProcs; i++) {
    RxPduProc = &SECOC_CONFIG->RxPduProcs[i];
    RxPduProc->context->state = SECOC_RXPDU_PROC_STATE_IDLE;
  }
}

Std_ReturnType SecOC_IfTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_OK;
  const SecOC_TxPduProcessingType *TxPduProc;
  DET_VALIDATE(NULL != SECOC_CONFIG, 0x49, SECOC_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(TxPduId < SECOC_CONFIG->numTxPduProcs, 0x49, SECOC_E_INVALID_PDU_SDU_ID,
               return E_NOT_OK);
  TxPduProc = &SECOC_CONFIG->TxPduProcs[TxPduId];
  DET_VALIDATE((NULL != PduInfoPtr) && (NULL != PduInfoPtr->SduDataPtr) &&
                 (PduInfoPtr->SduLength > 0),
               0x49, SECOC_E_PARAM_POINTER, return E_NOT_OK);
  DET_VALIDATE((PduInfoPtr->SduLength + TxPduProc->AuthPduOffset +
                ((TxPduProc->FreshnessValueLength + 7) / 8) +
                ((TxPduProc->AuthInfoLength + 7) / 8)) < TxPduProc->bufLen,
               0x49, SECOC_E_PARAM_POINTER, return E_NOT_OK);
  (void)memcpy(&TxPduProc->buffer[TxPduProc->AuthPduOffset], PduInfoPtr->SduDataPtr,
               PduInfoPtr->SduLength);

  TxPduProc->context->state = SECOC_TXPDU_PROC_STATE_REQUEST;
  TxPduProc->context->SduLength = PduInfoPtr->SduLength;
  return ret;
}

void SecOC_TxConfirmation(PduIdType TxPduId, Std_ReturnType result) {
  const SecOC_TxPduProcessingType *TxPduProc;
  DET_VALIDATE(NULL != SECOC_CONFIG, 0x40, SECOC_E_UNINIT, return);
  DET_VALIDATE(TxPduId < SECOC_CONFIG->numTxPduProcs, 0x40, SECOC_E_INVALID_PDU_SDU_ID, return);

  TxPduProc = &SECOC_CONFIG->TxPduProcs[TxPduId];
  if (SECOC_TXPDU_PROC_STATE_WAIT_TX_DONE == TxPduProc->context->state) {
    TxPduProc->context->state = SECOC_TXPDU_PROC_STATE_IDLE;
    ASLOG(SECOCE, ("[%u] Tx Confirm result=%u\n", TxPduId, result));
    PduR_SecOCTxConfirmation(TxPduProc->UpTxPduId, result);
  } else {
    ASLOG(SECOCE, ("[%u] TxPdu in state %u\n", TxPduId, TxPduProc->context->state));
  }
}

void SecOC_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  const SecOC_RxPduProcessingType *RxPduProc;
  DET_VALIDATE(NULL != SECOC_CONFIG, 0x42, SECOC_E_UNINIT, return);
  DET_VALIDATE(RxPduId < SECOC_CONFIG->numRxPduProcs, 0x42, SECOC_E_INVALID_PDU_SDU_ID, return);
  DET_VALIDATE((NULL != PduInfoPtr) && (NULL != PduInfoPtr->SduDataPtr) &&
                 (PduInfoPtr->SduLength > 0),
               0x42, SECOC_E_PARAM_POINTER, return);

  RxPduProc = &SECOC_CONFIG->RxPduProcs[RxPduId];
  DET_VALIDATE((PduInfoPtr->SduLength + 2u + ((RxPduProc->FreshnessValueLength + 7) / 8) +
                ((RxPduProc->AuthInfoLength + 7) / 8)) < RxPduProc->bufLen,
               0x42, SECOC_E_PARAM_POINTER, return);
  if (SECOC_RXPDU_PROC_STATE_IDLE != RxPduProc->context->state) {
    ASLOG(SECOCE, ("[%u] RxPdu in state %u\n", RxPduId, RxPduProc->context->state));
  }

  (void)memcpy(&RxPduProc->buffer[2u], PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
  RxPduProc->context->SduLength = PduInfoPtr->SduLength;
  RxPduProc->context->state = SECOC_RXPDU_PROC_STATE_RECEIVED;
}

BufReq_ReturnType SecOC_CopyTxData(PduIdType TxPduId, const PduInfoType *info,
                                   const RetryInfoType *retry, PduLengthType *availableDataPtr) {
  BufReq_ReturnType bufReq = BUFREQ_E_NOT_OK;
  PduLengthType offset = 0;
  const SecOC_TxPduProcessingType *TxPduProc;
  PduLengthType dataLength;
  DET_VALIDATE(NULL != SECOC_CONFIG, 0x43, SECOC_E_UNINIT, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(TxPduId < SECOC_CONFIG->numTxPduProcs, 0x43, SECOC_E_INVALID_PDU_SDU_ID,
               return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != info->MetaDataPtr, 0x43, SECOC_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);
  TxPduProc = &SECOC_CONFIG->TxPduProcs[TxPduId];
  if ((SECOC_TXPDU_PROC_STATE_READY == TxPduProc->context->state) ||
      (SECOC_TXPDU_PROC_STATE_WAIT_TX_DONE == TxPduProc->context->state)) {
    offset = *(PduLengthType *)info->MetaDataPtr;
    dataLength =
      TxPduProc->context->SduLength + TxPduProc->AuthPduHeaderLength +
      (((uint16_t)TxPduProc->FreshnessValueTruncLength + TxPduProc->AuthInfoTruncLength + 7u) >> 3);
    if ((offset + info->SduLength) <= dataLength) {
      (void)memcpy(
        info->SduDataPtr,
        &TxPduProc->buffer[TxPduProc->AuthPduOffset - TxPduProc->AuthPduHeaderLength + offset],
        info->SduLength);
      *availableDataPtr = dataLength - (offset + info->SduLength);
      bufReq = BUFREQ_OK;
    }
  } else {
    ASLOG(SECOCE, ("[%u] CopyTx in state %u\n", TxPduId, TxPduProc->context->state));
  }
  return bufReq;
}

BufReq_ReturnType SecOC_StartOfReception(PduIdType RxPduId, const PduInfoType *info,
                                         PduLengthType TpSduLength, PduLengthType *bufferSizePtr) {
  BufReq_ReturnType bufReq = BUFREQ_E_NOT_OK;
  const SecOC_RxPduProcessingType *RxPduProc;
  DET_VALIDATE(NULL != SECOC_CONFIG, 0x46, SECOC_E_UNINIT, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(RxPduId < SECOC_CONFIG->numRxPduProcs, 0x46, SECOC_E_INVALID_PDU_SDU_ID,
               return BUFREQ_E_NOT_OK);
  DET_VALIDATE(TpSduLength > 0, 0x46, SECOC_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);
  (void)info;
  RxPduProc = &SECOC_CONFIG->RxPduProcs[RxPduId];
  DET_VALIDATE((TpSduLength + 2u + ((RxPduProc->FreshnessValueLength + 7) / 8) +
                ((RxPduProc->AuthInfoLength + 7) / 8)) < RxPduProc->bufLen,
               0x46, SECOC_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);
  if (SECOC_RXPDU_PROC_STATE_IDLE != RxPduProc->context->state) {
    ASLOG(SECOCE, ("[%u] Start Rx in state %u\n", RxPduId, RxPduProc->context->state));
  } else {
    RxPduProc->context->SduLength = TpSduLength;
    RxPduProc->context->state = SECOC_RXPDU_PROC_STATE_TP_RECV;
    *bufferSizePtr = RxPduProc->bufLen - 2u - ((RxPduProc->FreshnessValueLength + 7) / 8) -
                     ((RxPduProc->AuthInfoLength + 7) / 8);
    bufReq = BUFREQ_OK;
  }

  return bufReq;
}

BufReq_ReturnType SecOC_CopyRxData(PduIdType RxPduId, const PduInfoType *info,
                                   PduLengthType *bufferSizePtr) {
  BufReq_ReturnType bufReq = BUFREQ_E_NOT_OK;
  PduLengthType offset = 0;
  const SecOC_RxPduProcessingType *RxPduProc;

  DET_VALIDATE(NULL != SECOC_CONFIG, 0x44, SECOC_E_UNINIT, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(RxPduId < SECOC_CONFIG->numRxPduProcs, 0x44, SECOC_E_INVALID_PDU_SDU_ID,
               return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != bufferSizePtr, 0x44, SECOC_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != info, 0x44, SECOC_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != info->MetaDataPtr, 0x44, SECOC_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);
  RxPduProc = &SECOC_CONFIG->RxPduProcs[RxPduId];
  if (SECOC_RXPDU_PROC_STATE_TP_RECV != RxPduProc->context->state) {
    ASLOG(SECOCE, ("[%u] CopyRx in state %u\n", RxPduId, RxPduProc->context->state));
  } else {
    offset = *(PduLengthType *)info->MetaDataPtr;
    (void)memcpy(&RxPduProc->buffer[2u + offset], info->SduDataPtr, info->SduLength);
    bufReq = BUFREQ_OK;
  }
  return bufReq;
}

void SecOC_TpRxIndication(PduIdType RxPduId, Std_ReturnType result) {
  const SecOC_RxPduProcessingType *RxPduProc;
  DET_VALIDATE(NULL != SECOC_CONFIG, 0x45, SECOC_E_UNINIT, return);
  DET_VALIDATE(RxPduId < SECOC_CONFIG->numRxPduProcs, 0x45, SECOC_E_INVALID_PDU_SDU_ID, return);

  RxPduProc = &SECOC_CONFIG->RxPduProcs[RxPduId];
  if (SECOC_RXPDU_PROC_STATE_TP_RECV != RxPduProc->context->state) {
    ASLOG(SECOCE, ("[%u] TpRx in state %u\n", RxPduId, RxPduProc->context->state));
  } else {
    if (E_OK == result) {
      RxPduProc->context->state = SECOC_RXPDU_PROC_STATE_RECEIVED;
    } else {
      RxPduProc->context->state = SECOC_RXPDU_PROC_STATE_IDLE;
    }
  }
}

void SecOC_MainFunctionTx(void) {
  uint16_t i;
  const SecOC_TxPduProcessingType *TxPduProc;
  for (i = 0; i < SECOC_CONFIG->numTxPduProcs; i++) {
    TxPduProc = &SECOC_CONFIG->TxPduProcs[i];
    if (SECOC_TXPDU_PROC_STATE_IDLE != TxPduProc->context->state) {
      SecOC_ProcTx(i);
    }
  }
}

void SecOC_MainFunctionRx(void) {
  uint16_t i;
  const SecOC_RxPduProcessingType *RxPduProc;
  for (i = 0; i < SECOC_CONFIG->numRxPduProcs; i++) {
    RxPduProc = &SECOC_CONFIG->RxPduProcs[i];
    if (SECOC_RXPDU_PROC_STATE_IDLE != RxPduProc->context->state) {
      SecOC_ProcRx(i);
    }
  }
}
