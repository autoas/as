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
static Dcm_ContextType dcmContext;
extern CONSTANT(Dcm_ConfigType, DCM_CONST) Dcm_Config;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Dcm_ContextType *Dcm_GetContext(void) {
  return (&dcmContext);
}

P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) Dcm_GetConfig(void) {
  return (&Dcm_Config);
}

void Dcm_Init(P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) configPtr) {
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();

  (void)configPtr;

  (void)memset(context, 0, sizeof(Dcm_ContextType));
  context->RxBufferState = DCM_BUFFER_IDLE;
  context->TxBufferState = DCM_BUFFER_IDLE;
  context->CurPduId = DCM_INVALID_PDU_ID;
  Dcm_DslInit();
  Dcm_DspInit();
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
  if (Dcm_NvmSecurityAccess_Ram.AttemptCounter >= config->SecurityNumAttDelay) {
    context->SecurityDelayTimer = config->SecurityDelayTime;
    ASLOG(DCME, ("Security Delay Timer: %d\n", config->SecurityDelayTime));
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

  if (DCM_RESPONSE_PENDING == context->ResponsePending) {
    if (context->CurPduId < config->NumOfChls) {
      PduInfo.MetaDataPtr = NULL;
      PduInfo.SduDataPtr = NULL;
      PduInfo.SduLength = 3;
      context->ResponsePending = DCM_RESPONSE_PENDING_PROVIDED;
      r = PduR_DcmTransmit(config->Channles[context->CurPduId].TxPduId, &PduInfo);
      if (E_OK != r) {
        context->ResponsePending = DCM_RESPONSE_PENDING; /* try next time */
      }
    } else {
      ASLOG(DCME, ("Fatal ERROR as invalid PDU ID in response pending\n"));
      context->ResponsePending = DCM_NO_RESPONSE_PENDING; /* drop it */
    }
  } else if (DCM_BUFFER_FULL == context->TxBufferState) {
    if (context->CurPduId < config->NumOfChls) {
      PduInfo.MetaDataPtr = NULL;
      PduInfo.SduDataPtr = config->TxBuffer;
      PduInfo.SduLength = context->TxTpSduLength;
      context->TxBufferState = DCM_BUFFER_PROVIDED;
      context->TxIndex = 0;
      ASLOG(DCM, ("Tx %02X %02X %02X ...\n", config->TxBuffer[0], config->TxBuffer[1],
                  config->TxBuffer[2]));
      r = PduR_DcmTransmit(config->Channles[context->CurPduId].TxPduId, &PduInfo);
      if (E_OK != r) {
        /* This Dcm will ensure only 1 tx request, so if Transmit failed, it was a fatal error! */
        context->TxBufferState = DCM_BUFFER_IDLE;
        ASLOG(DCME, ("Tx Failed!\n"));
      }
    } else {
      ASLOG(DCME, ("Fatal ERROR as invalid PDU ID in transmit\n"));
      context->TxBufferState = DCM_BUFFER_IDLE; /* drop it */
    }
  } else {
    /* do nothing */
  }
}

Std_ReturnType Dcm_GetRxPduId(PduIdType *pduId) {
  Std_ReturnType ret = E_OK;
  Dcm_ContextType *context = Dcm_GetContext();

  DET_VALIDATE(NULL != pduId, 0xF1, DCM_E_PARAM_POINTER, return E_NOT_OK);

  *pduId = context->MsgContext.DcmRxPduId;

  return ret;
}

BufReq_ReturnType Dcm_StartOfReception(PduIdType id, const PduInfoType *info,
                                       PduLengthType tpSduLength, PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();

  DET_VALIDATE(id < config->NumOfChls, 0x46, DCM_E_PARAM, return BUFREQ_E_NOT_OK);
  DET_VALIDATE((NULL != info) && (NULL != info->SduDataPtr), 0x46, DCM_E_PARAM_POINTER,
               return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != bufferSizePtr, 0x46, DCM_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);

  if ((2u == tpSduLength) && (0x3Eu == info->SduDataPtr[0]) && (0x80u == info->SduDataPtr[1])) {
    /* @SWS_Dcm_00557, @SWS_Dcm_01145
     * Only support 3E request without positive response */
    if (context->CurrentSession != DCM_DEFAULT_SESSION) {
      context->TimerS3Server = config->Timing->S3Server;
    }
    ret = BUFREQ_E_BUSY;
  } else if ((DCM_BUFFER_IDLE != context->TxBufferState) && (id != context->CurPduId)) {
    /* As this Dcm share the same context & buffers across all the Dcm protocal channels, thus
     * if there is a responsing on going, can't accept the new incoming request from the other
     * channel. */
    ret = BUFREQ_E_BUSY;
  } else if (DCM_BUFFER_IDLE == context->RxBufferState) {
    if (tpSduLength > config->RxBufferSize) {
      /* @SWS_Dcm_00444 */
      ret = BUFREQ_E_OVFL;
    } else if (0u == tpSduLength) {
      /* @SWS_Dcm_00642 */
      ret = BUFREQ_E_NOT_OK;
    } else {
      *bufferSizePtr = config->RxBufferSize;
      context->CurPduId = id;
      context->RxIndex = 0u;
      context->RxTpSduLength = tpSduLength;
      context->RxBufferState = DCM_BUFFER_PROVIDED;
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

  DET_VALIDATE(id < config->NumOfChls, 0x44, DCM_E_PARAM, return BUFREQ_E_NOT_OK);
  DET_VALIDATE((NULL != info) && (NULL != info->SduDataPtr), 0x44, DCM_E_PARAM_POINTER,
               return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != bufferSizePtr, 0x44, DCM_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);

  if (DCM_BUFFER_PROVIDED == context->RxBufferState) {
    if (context->CurPduId == id) {
      if (0u == info->SduLength) {
        /* @SWS_Dcm_00996 */
        ret = BUFREQ_OK;
      } else if (((context->RxIndex + info->SduLength) <= context->RxTpSduLength) &&
                 (context->RxTpSduLength <= config->RxBufferSize)) {
        (void)memcpy(&config->RxBuffer[context->RxIndex], info->SduDataPtr, info->SduLength);
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
    context->RxBufferState = DCM_BUFFER_IDLE;
  }

  return ret;
}

void Dcm_TpRxIndication(PduIdType id, Std_ReturnType result) {
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();

  DET_VALIDATE(id < config->NumOfChls, 0x45, DCM_E_PARAM, return);

  if ((E_OK == result) && (DCM_BUFFER_PROVIDED == context->RxBufferState) &&
      (context->CurPduId == id)) {
    if (context->RxIndex == context->RxTpSduLength) {
      context->MsgContext.MsgAddInfo.ReqType = config->Channles[id].ReqType;
      context->MsgContext.MsgAddInfo.SuppressPosResponse = DCM_NOT_SUPRESS_POSITIVE_RESPONCE;
      context->MsgContext.DcmRxPduId = id;
      /* context->MsgContext.idContext = 0; */
      context->MsgContext.ReqData = &config->RxBuffer[1];
      context->MsgContext.ReqDataLen = context->RxTpSduLength - 1u;
      context->MsgContext.ResData = &config->TxBuffer[1];
      context->MsgContext.ResDataLen = 0;
      context->MsgContext.ResMaxDataLen = config->TxBufferSize - 1u;
      context->OpStatus = DCM_INITIAL;
      context->RxBufferState = DCM_BUFFER_FULL;
      ASLOG(DCM, ("Rx %02X %02X %02X ...\n", config->RxBuffer[0], config->RxBuffer[1],
                  config->RxBuffer[2]));
    } else {
      ASLOG(DCME, ("Fatal Error when do RxInd, reset to Idle\n"));
    }
  } else {
    ASLOG(DCME, ("Error when do RxInd, reset to Idle\n"));
    context->RxBufferState = DCM_BUFFER_IDLE;
  }
}

BufReq_ReturnType Dcm_CopyTxData(PduIdType id, const PduInfoType *info, const RetryInfoType *retry,
                                 PduLengthType *availableDataPtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  (void)retry;

  DET_VALIDATE(id < config->NumOfChls, 0x43, DCM_E_PARAM, return BUFREQ_E_NOT_OK);
  DET_VALIDATE((NULL != info) && (NULL != info->SduDataPtr), 0x43, DCM_E_PARAM_POINTER,
               return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != availableDataPtr, 0x43, DCM_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);

  if (DCM_RESPONSE_PENDING_PROVIDED == context->ResponsePending) {
    if (info->SduLength >= 3u) {
      info->SduDataPtr[0] = SID_NEGATIVE_RESPONSE;
      info->SduDataPtr[1] = context->CurrentSID;
      info->SduDataPtr[2] = DCM_E_RESPONSE_PENDING;
      context->ResponsePending = DCM_RESPONSE_PENDING_TXING;
      *availableDataPtr = 0u;
      ret = BUFREQ_OK;
    } else {
      context->ResponsePending = DCM_NO_RESPONSE_PENDING;
      ASLOG(DCME, ("Fatal Error when copy Tx Data, reset response pending\n"));
    }
  } else if (DCM_BUFFER_PROVIDED == context->TxBufferState) {
    if (context->CurPduId == id) {
      if (((context->TxIndex + info->SduLength) <= context->TxTpSduLength) &&
          (context->TxTpSduLength <= config->TxBufferSize)) {
        (void)memcpy(info->SduDataPtr, &config->TxBuffer[context->TxIndex], info->SduLength);
        context->TxIndex += info->SduLength;
        *availableDataPtr = context->TxTpSduLength - context->TxIndex;
        ret = BUFREQ_OK;
      }
    }
    if (BUFREQ_OK != ret) {
      ASLOG(DCME, ("Fatal Error when copy Tx Data, reset to Idle\n"));
      context->TxBufferState = DCM_BUFFER_IDLE;
    }
  } else {
    ASLOG(DCME, ("Fatal Error when copy Tx Data, do nothing\n"));
  }

  return ret;
}

void Dcm_TpTxConfirmation(PduIdType id, Std_ReturnType result) {
  Dcm_ContextType *context = Dcm_GetContext();

  DET_VALIDATE(id < Dcm_GetConfig()->NumOfChls, 0x48, DCM_E_PARAM, return);

  if (DCM_RESPONSE_PENDING_TXING == context->ResponsePending) {
    context->ResponsePending = DCM_NO_RESPONSE_PENDING;
  } else {
    if ((E_OK == result) && (DCM_BUFFER_PROVIDED == context->TxBufferState) &&
        (context->CurPduId == id)) {
      if (context->TxIndex != context->TxTpSduLength) {
        ASLOG(DCME, ("Fatal Error when do TxConfirmation, reset to Idle\n"));
      }
    } else {
      ASLOG(DCME, ("Error when do TxConfirmation, reset to Idle\n"));
    }
    /* comment out for Periodic DID */
    /* context->CurPduId = DCM_INVALID_PDU_ID; */
    context->TxBufferState = DCM_BUFFER_IDLE;
  }
}

void Dcm_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x24, DCM_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_DCM;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 7;
}
/** @brief release notes
 * - 4.0.1: Typo Fix: responcePending -> responsePending
 * - 4.0.2: Fixed: Suppressing positive response for UDS 0x3E 0x80 (Tester Present)
 *    no longer disrupts physical communication.
 * - 4.0.3: Fixed: Dcm_SessionChangeIndication() with wrong current session issue.
 * - 4.0.4: Fixed: TransferData force response pending issue.
 * - 4.0.5: OPT: Ensure the same length format response for request download/upload.
 * - 4.0.6: Add read DID with Dynamic length support.
 * - 4.0.7: Add routine control state tracking with automatic stop when session change.
 */
