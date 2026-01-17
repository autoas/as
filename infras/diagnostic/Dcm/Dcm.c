/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Diagnostic CommunicationManager AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dcm_Priv.h"
#include "Std_Debug.h"
#include "PduR_Dcm.h"
#include <string.h>
#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DCM 0
#define AS_LOG_DCME 1

/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static Dcm_ContextType Dcm_Context;
extern CONSTANT(Dcm_ConfigType, DCM_CONST) Dcm_Config;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Dcm_ContextType *Dcm_GetContext(void) {
  return (&Dcm_Context);
}

P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) Dcm_GetConfig(void) {
  return (&Dcm_Config);
}

void Dcm_Init(P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) ConfigPtr) {
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();

  (void)ConfigPtr;

  (void)memset(context, 0, sizeof(Dcm_ContextType));
  context->rxBufferState = DCM_BUFFER_IDLE;
  context->txBufferState = DCM_BUFFER_IDLE;
  context->curPduId = DCM_INVALID_PDU_ID;
  Dcm_DslInit();
  Dcm_DspInit();
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
  if (Dcm_NvmSecurityAccess_Ram.AttemptCounter >= config->SecurityNumAttDelay) {
    context->securityDelayTimer = config->SecurityDelayTime;
  }
#endif
}

void Dcm_MainFunction(void) {
  Dcm_MainFunction_Request();
  Dcm_DslMainFunction();
  Dcm_MainFunction_Response();
#ifdef DCM_USE_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER
  Dcm_MainFunction_ReadPeriodicDID();
#endif
}

void Dcm_MainFunction_Response(void) {
  Std_ReturnType r;
  PduInfoType PduInfo;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();

  if (DCM_RESPONSE_PENDING == context->responsePending) {
    if (context->curPduId < config->numOfChls) {
      PduInfo.MetaDataPtr = NULL;
      PduInfo.SduDataPtr = NULL;
      PduInfo.SduLength = 3;
      context->responsePending = DCM_RESPONSE_PENDING_PROVIDED;
      r = PduR_DcmTransmit(config->channles[context->curPduId].TxPduId, &PduInfo);
      if (E_OK != r) {
        context->responsePending = DCM_RESPONSE_PENDING; /* try next time */
      }
    } else {
      ASLOG(DCME, ("Fatal ERROR as invalid PDU ID in response pending\n"));
      context->responsePending = DCM_NO_RESPONSE_PENDING; /* drop it */
    }
  } else if (DCM_BUFFER_FULL == context->txBufferState) {
    if (context->curPduId < config->numOfChls) {
      PduInfo.MetaDataPtr = NULL;
      PduInfo.SduDataPtr = config->txBuffer;
      PduInfo.SduLength = context->TxTpSduLength;
      context->txBufferState = DCM_BUFFER_PROVIDED;
      context->TxIndex = 0;
      ASLOG(DCM, ("Tx %02X %02X %02X ...\n", config->txBuffer[0], config->txBuffer[1],
                  config->txBuffer[2]));
      r = PduR_DcmTransmit(config->channles[context->curPduId].TxPduId, &PduInfo);
      if (E_OK != r) {
        /* This Dcm will ensure only 1 tx request, so if Transmit failed, it was a fatal error! */
        context->txBufferState = DCM_BUFFER_IDLE;
        ASLOG(DCME, ("Tx Failed!\n"));
      }
    } else {
      ASLOG(DCME, ("Fatal ERROR as invalid PDU ID in transmit\n"));
      context->txBufferState = DCM_BUFFER_IDLE; /* drop it */
    }
  } else {
    /* do nothing */
  }
}

Std_ReturnType Dcm_GetRxPduId(PduIdType *PduId) {
  Std_ReturnType ret = E_OK;
  Dcm_ContextType *context = Dcm_GetContext();

  DET_VALIDATE(NULL != PduId, 0xF1, DCM_E_PARAM_POINTER, return E_NOT_OK);

  *PduId = context->msgContext.dcmRxPduId;

  return ret;
}

BufReq_ReturnType Dcm_StartOfReception(PduIdType id, const PduInfoType *info,
                                       PduLengthType TpSduLength, PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();

  DET_VALIDATE(id < config->numOfChls, 0x46, DCM_E_PARAM, return BUFREQ_E_NOT_OK);
  DET_VALIDATE((NULL != info) && (NULL != info->SduDataPtr), 0x46, DCM_E_PARAM_POINTER,
               return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != bufferSizePtr, 0x46, DCM_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);

  if ((2u == TpSduLength) && (0x3Eu == info->SduDataPtr[0]) && (0x80u == info->SduDataPtr[1])) {
    /* @SWS_Dcm_00557, @SWS_Dcm_01145
     * Only support 3E request without positive response */
    if (context->currentSession != DCM_DEFAULT_SESSION) {
      context->timerS3Server = config->timing->S3Server;
    }
    ret = BUFREQ_E_BUSY;
  } else if ((DCM_BUFFER_IDLE != context->txBufferState) && (id != context->curPduId)) {
    /* As this Dcm share the same context & buffers across all the Dcm protocal channels, thus
     * if there is a responsing on going, can't accept the new incoming request from the other
     * channel. */
    ret = BUFREQ_E_BUSY;
  } else if (DCM_BUFFER_IDLE == context->rxBufferState) {
    if (TpSduLength > config->rxBufferSize) {
      /* @SWS_Dcm_00444 */
      ret = BUFREQ_E_OVFL;
    } else if (0u == TpSduLength) {
      /* @SWS_Dcm_00642 */
      ret = BUFREQ_E_NOT_OK;
    } else {
      *bufferSizePtr = config->rxBufferSize;
      context->curPduId = id;
      context->RxIndex = 0u;
      context->RxTpSduLength = TpSduLength;
      context->rxBufferState = DCM_BUFFER_PROVIDED;
    }
  } else {
    ret = BUFREQ_E_BUSY;
  }

  return ret;
}

BufReq_ReturnType Dcm_CopyRxData(PduIdType id, const PduInfoType *info,
                                 PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();

  DET_VALIDATE(id < config->numOfChls, 0x44, DCM_E_PARAM, return BUFREQ_E_NOT_OK);
  DET_VALIDATE((NULL != info) && (NULL != info->SduDataPtr), 0x44, DCM_E_PARAM_POINTER,
               return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != bufferSizePtr, 0x44, DCM_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);

  if (DCM_BUFFER_PROVIDED == context->rxBufferState) {
    if (context->curPduId == id) {
      if (0u == info->SduLength) {
        /* @SWS_Dcm_00996 */
        ret = BUFREQ_OK;
      } else if (((context->RxIndex + info->SduLength) <= context->RxTpSduLength) &&
                 (context->RxTpSduLength <= config->rxBufferSize)) {
        (void)memcpy(&config->rxBuffer[context->RxIndex], info->SduDataPtr, info->SduLength);
        context->RxIndex += info->SduLength;
        ret = BUFREQ_OK;
      } else {
        /* NOT OK*/
      }
    }
  }

  if (BUFREQ_OK == ret) {
    /* @SWS_Dcm_00443 */
    *bufferSizePtr = context->RxTpSduLength - context->RxIndex;
  } else {
    ASLOG(DCME, ("Fatal Error when copy Rx Data, reset to Idle\n"));
    context->rxBufferState = DCM_BUFFER_IDLE;
  }

  return ret;
}

void Dcm_TpRxIndication(PduIdType id, Std_ReturnType result) {
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();

  DET_VALIDATE(id < config->numOfChls, 0x45, DCM_E_PARAM, return);

  if ((E_OK == result) && (DCM_BUFFER_PROVIDED == context->rxBufferState) &&
      (context->curPduId == id)) {
    if (context->RxIndex == context->RxTpSduLength) {
      context->msgContext.msgAddInfo.reqType = config->channles[id].reqType;
      context->msgContext.msgAddInfo.suppressPosResponse = DCM_NOT_SUPRESS_POSITIVE_RESPONCE;
      context->msgContext.dcmRxPduId = id;
      /* context->msgContext.idContext = 0; */
      context->msgContext.reqData = &config->rxBuffer[1];
      context->msgContext.reqDataLen = context->RxTpSduLength - 1u;
      context->msgContext.resData = &config->txBuffer[1];
      context->msgContext.resDataLen = 0;
      context->msgContext.resMaxDataLen = config->txBufferSize - 1u;
      context->opStatus = DCM_INITIAL;
      context->rxBufferState = DCM_BUFFER_FULL;
      ASLOG(DCM, ("Rx %02X %02X %02X ...\n", config->rxBuffer[0], config->rxBuffer[1],
                  config->rxBuffer[2]));
    } else {
      ASLOG(DCME, ("Fatal Error when do RxInd, reset to Idle\n"));
    }
  } else {
    ASLOG(DCME, ("Error when do RxInd, reset to Idle\n"));
    context->rxBufferState = DCM_BUFFER_IDLE;
  }
}

BufReq_ReturnType Dcm_CopyTxData(PduIdType id, const PduInfoType *info, const RetryInfoType *retry,
                                 PduLengthType *availableDataPtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  (void)retry;

  DET_VALIDATE(id < config->numOfChls, 0x43, DCM_E_PARAM, return BUFREQ_E_NOT_OK);
  DET_VALIDATE((NULL != info) && (NULL != info->SduDataPtr), 0x43, DCM_E_PARAM_POINTER,
               return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != availableDataPtr, 0x43, DCM_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);

  if (DCM_RESPONSE_PENDING_PROVIDED == context->responsePending) {
    if (info->SduLength >= 3u) {
      info->SduDataPtr[0] = SID_NEGATIVE_RESPONSE;
      info->SduDataPtr[1] = context->currentSID;
      info->SduDataPtr[2] = DCM_E_RESPONSE_PENDING;
      context->responsePending = DCM_RESPONSE_PENDING_TXING;
      *availableDataPtr = 0u;
      ret = BUFREQ_OK;
    } else {
      context->responsePending = DCM_NO_RESPONSE_PENDING;
      ASLOG(DCME, ("Fatal Error when copy Tx Data, reset response pending\n"));
    }
  } else if (DCM_BUFFER_PROVIDED == context->txBufferState) {
    if (context->curPduId == id) {
      if (((context->TxIndex + info->SduLength) <= context->TxTpSduLength) &&
          (context->TxTpSduLength <= config->txBufferSize)) {
        (void)memcpy(info->SduDataPtr, &config->txBuffer[context->TxIndex], info->SduLength);
        context->TxIndex += info->SduLength;
        *availableDataPtr = context->TxTpSduLength - context->TxIndex;
        ret = BUFREQ_OK;
      }
    }
    if (BUFREQ_OK != ret) {
      ASLOG(DCME, ("Fatal Error when copy Tx Data, reset to Idle\n"));
      context->txBufferState = DCM_BUFFER_IDLE;
    }
  } else {
    ASLOG(DCME, ("Fatal Error when copy Tx Data, do nothing\n"));
  }

  return ret;
}

void Dcm_TpTxConfirmation(PduIdType id, Std_ReturnType result) {
  Dcm_ContextType *context = Dcm_GetContext();

  DET_VALIDATE(id < Dcm_GetConfig()->numOfChls, 0x48, DCM_E_PARAM, return);

  if (DCM_RESPONSE_PENDING_TXING == context->responsePending) {
    context->responsePending = DCM_NO_RESPONSE_PENDING;
  } else {
    if ((E_OK == result) && (DCM_BUFFER_PROVIDED == context->txBufferState) &&
        (context->curPduId == id)) {
      if (context->TxIndex != context->TxTpSduLength) {
        ASLOG(DCME, ("Fatal Error when do TxConfirmation, reset to Idle\n"));
      }
    } else {
      ASLOG(DCME, ("Error when do TxConfirmation, reset to Idle\n"));
    }
    /* comment out for Periodic DID */
    /* context->curPduId = DCM_INVALID_PDU_ID; */
    context->txBufferState = DCM_BUFFER_IDLE;
  }
}

void Dcm_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x24, DCM_E_PARAM_POINTER, return);

  versionInfo->vendorID = STD_VENDOR_ID_AS;
  versionInfo->moduleID = MODULE_ID_DCM;
  versionInfo->sw_major_version = 4;
  versionInfo->sw_minor_version = 0;
  versionInfo->sw_patch_version = 5;
}
/** @brief release notes
 * - 4.0.1: Typo Fix: responcePending -> responsePending
 * - 4.0.2: Fixed: Suppressing positive response for UDS 0x3E 0x80 (Tester Present)
 *    no longer disrupts physical communication.
 * - 4.0.3: Fixed: Dcm_SessionChangeIndication() with wrong current session issue.
 * - 4.0.4: Fixed: TransferData force response pending issue.
 * - 4.0.5: OPT: Ensure the same length format response for request download/upload.
 */
