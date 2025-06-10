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

  if (DCM_RESPONSE_PENDING == context->responcePending) {
    PduInfo.MetaDataPtr = NULL;
    PduInfo.SduDataPtr = NULL;
    PduInfo.SduLength = 3;
    context->responcePending = DCM_RESPONSE_PENDING_PROVIDED;
    r = PduR_DcmTransmit(config->channles[context->curPduId].TxPduId, &PduInfo);
    if (E_OK != r) {
      context->responcePending = DCM_RESPONSE_PENDING; /* try next time */
    }
  } else if (DCM_BUFFER_FULL == context->txBufferState) {
    PduInfo.MetaDataPtr = NULL;
    PduInfo.SduDataPtr = config->txBuffer;
    PduInfo.SduLength = context->TxTpSduLength;
    context->txBufferState = DCM_BUFFER_PROVIDED;
    context->TxIndex = 0;
    ASLOG(DCM, ("Tx %02X %02X %02X ...\n", config->txBuffer[0], config->txBuffer[1],
                config->txBuffer[2]));
    r = PduR_DcmTransmit(config->channles[context->curPduId].TxPduId, &PduInfo);
    if (E_OK != r) {
      /* This Dcm will ensure only 1 tx request, so if Transmit failed, it was a fatal error!*/
      context->txBufferState = DCM_BUFFER_IDLE;
      ASLOG(DCME, ("Tx Failed!\n"));
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

  if (DCM_BUFFER_IDLE == context->rxBufferState) {
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
    if ((2u == TpSduLength) && (0x3Eu == info->SduDataPtr[0]) && (0x80u == info->SduDataPtr[1]) &&
        ((DCM_P2P_PDU == id) || (DCM_P2A_PDU == id))) {
      /* @SWS_Dcm_00557, @SWS_Dcm_01145
       * Only support 3E request without positive response */
      if (context->currentSession != DCM_DEFAULT_SESSION) {
        context->timerS3Server = config->timing->S3Server;
      }
    }
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
      } else {
        (void)memcpy(&config->rxBuffer[context->RxIndex], info->SduDataPtr, info->SduLength);
        context->RxIndex += info->SduLength;
      }
      /* @SWS_Dcm_00443 */
      *bufferSizePtr = context->RxTpSduLength - context->RxIndex;
      ret = BUFREQ_OK;
    } else {
      ASLOG(DCME, ("Fatal Error when copy Rx Data, reset to Idle\n"));
      context->rxBufferState = DCM_BUFFER_IDLE;
    }
  }
  return ret;
}

void Dcm_TpRxIndication(PduIdType id, Std_ReturnType result) {
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();

  DET_VALIDATE(id < config->numOfChls, 0x45, DCM_E_PARAM, return );

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

  if (DCM_RESPONSE_PENDING_PROVIDED == context->responcePending) {
    info->SduDataPtr[0] = SID_NEGATIVE_RESPONSE;
    info->SduDataPtr[1] = context->currentSID;
    info->SduDataPtr[2] = DCM_E_RESPONSE_PENDING;
    context->responcePending = DCM_RESPONSE_PENDING_TXING;
    *availableDataPtr = 0u;
    ret = BUFREQ_OK;
  } else if (DCM_BUFFER_PROVIDED == context->txBufferState) {
    if (context->curPduId == id) {
      (void)memcpy(info->SduDataPtr, &config->txBuffer[context->TxIndex], info->SduLength);
      context->TxIndex += info->SduLength;
      *availableDataPtr = context->TxTpSduLength - context->TxIndex;
      ret = BUFREQ_OK;
    } else {
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

  DET_VALIDATE(id < Dcm_GetConfig()->numOfChls, 0x48, DCM_E_PARAM, return );

  if (DCM_RESPONSE_PENDING_TXING == context->responcePending) {
    context->responcePending = DCM_NO_RESPONSE_PENDING;
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
