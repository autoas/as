/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Diagnostic CommunicationManager AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dcm_Internal.h"
#include "Std_Debug.h"
#include "PduR_Dcm.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DCM 0
#define AS_LOG_DCME 1

/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static Dcm_ContextType Dcm_Context;
extern const Dcm_ConfigType Dcm_Config;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Dcm_ContextType *Dcm_GetContext(void) {
  return (&Dcm_Context);
}

const Dcm_ConfigType *Dcm_GetConfig(void) {
  return (&Dcm_Config);
}

void Dcm_Init(const Dcm_ConfigType *ConfigPtr) {
  Dcm_ContextType *context = Dcm_GetContext();
  const Dcm_ConfigType *config = Dcm_GetConfig();

  (void)ConfigPtr;

  memset(context, 0, sizeof(Dcm_ContextType));
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
  const Dcm_ConfigType *config = Dcm_GetConfig();

  if (DCM_RESPONSE_PENDING == context->responcePending) {
    PduInfo.MetaDataPtr = NULL;
    PduInfo.SduDataPtr = NULL;
    PduInfo.SduLength = 3;
    context->responcePending = DCM_RESPONSE_PENDING_PROVIDED;
    r = PduR_DcmTransmit(context->curPduId, &PduInfo);
    if (E_OK != r) {
      context->responcePending = DCM_RESPONSE_PENDING; /* try next time */
    }
  } else if (DCM_BUFFER_FULL == context->txBufferState) {
    PduInfo.MetaDataPtr = NULL;
    PduInfo.SduDataPtr = config->txBuffer;
    PduInfo.SduLength = context->TxTpSduLength;
    context->txBufferState = DCM_BUFFER_PROVIDED;
    context->TxIndex = 0;
    ASLOG(DCM, ("Tx %02X %02X ...\n", config->txBuffer[0], config->txBuffer[1]));
    r = PduR_DcmTransmit(context->curPduId, &PduInfo);
    if (E_OK != r) {
      context->txBufferState = DCM_BUFFER_FULL; /* try next time */
    }
  } else {
    /* do nothing */
  }
}

BufReq_ReturnType Dcm_StartOfReception(PduIdType id, const PduInfoType *info,
                                       PduLengthType TpSduLength, PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  const Dcm_ConfigType *config = Dcm_GetConfig();

  if (DCM_BUFFER_IDLE == context->rxBufferState) {
    /* only going to support P2P and P2A across all protocal(CAN/OBD/LIN/DOIP, etc)*/
    if ((DCM_P2P_PDU == id) || (DCM_P2A_PDU == id)) {
      if (TpSduLength > config->rxBufferSize) {
        /* @SWS_Dcm_00444 */
        ret = BUFREQ_E_OVFL;
      } else if (0 == TpSduLength) {
        /* @SWS_Dcm_00642 */
        ret = BUFREQ_E_NOT_OK;
      } else {
        *bufferSizePtr = config->rxBufferSize;
        context->curPduId = id;
        context->RxIndex = 0;
        context->RxTpSduLength = TpSduLength;
        context->rxBufferState = DCM_BUFFER_PROVIDED;
      }
    } else {
      /* @SWS_Dcm_00790 */
      ret = BUFREQ_E_NOT_OK;
    }
  } else {
    if ((2 == TpSduLength) && (0x3E == info->SduDataPtr[0]) && (0x80 == info->SduDataPtr[1]) &&
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
  const Dcm_ConfigType *config = Dcm_GetConfig();

  if (DCM_BUFFER_PROVIDED == context->rxBufferState) {
    if (context->curPduId == id) {
      if (0 == info->SduLength) {
        /* @SWS_Dcm_00996 */
      } else {
        memcpy(&config->rxBuffer[context->RxIndex], info->SduDataPtr, info->SduLength);
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
  const Dcm_ConfigType *config = Dcm_GetConfig();

  if ((E_OK == result) && (DCM_BUFFER_PROVIDED == context->rxBufferState) &&
      (context->curPduId == id)) {
    if (context->RxIndex == context->RxTpSduLength) {
      if (DCM_P2P_PDU == id) {
        context->msgContext.msgAddInfo.reqType = DCM_PHYSICAL_REQUEST;
      } else {
        context->msgContext.msgAddInfo.reqType = DCM_FUNCTIONAL_REQUEST;
      }
      context->msgContext.msgAddInfo.suppressPosResponse = DCM_NOT_SUPRESS_POSITIVE_RESPONCE;
      context->msgContext.dcmRxPduId = id;
      context->msgContext.idContext = 0;
      context->msgContext.reqData = &config->rxBuffer[1];
      context->msgContext.reqDataLen = context->RxTpSduLength - 1;
      context->msgContext.resData = &config->txBuffer[1];
      context->msgContext.resDataLen = 0;
      context->msgContext.resMaxDataLen = config->txBufferSize - 1;
      context->opStatus = DCM_INITIAL;
      context->rxBufferState = DCM_BUFFER_FULL;
      ASLOG(DCM, ("Rx %02X %02X ...\n", config->rxBuffer[0], config->rxBuffer[1]));
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
  const Dcm_ConfigType *config = Dcm_GetConfig();

  if (DCM_RESPONSE_PENDING_PROVIDED == context->responcePending) {
    info->SduDataPtr[0] = SID_NEGATIVE_RESPONSE;
    info->SduDataPtr[1] = context->currentSID;
    info->SduDataPtr[2] = DCM_E_RESPONSE_PENDING;
    context->responcePending = DCM_RESPONSE_PENDING_TXING;
    *availableDataPtr = 0;
    ret = BUFREQ_OK;
  } else if (DCM_BUFFER_PROVIDED == context->txBufferState) {
    if (context->curPduId == id) {
      memcpy(info->SduDataPtr, &config->txBuffer[context->TxIndex], info->SduLength);
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