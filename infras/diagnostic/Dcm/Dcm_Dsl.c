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
#include <string.h>
#include "NvM.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DCM 1
#define AS_LOG_DCME 3

#ifndef Dcm_DslCustomerSession2Mask
#define Dcm_DslCustomerSession2Mask(mask, sesCtrl)
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
#ifndef USE_NVM
Dcm_Nvm_SecurityAccessType Dcm_NvmSecurityAccess_Ram;
#endif
#endif
/* ================================ [ LOCALS    ] ============================================== */
static uint8_t Dcm_DslSession2Mask(Dcm_SesCtrlType sesCtrl) {
  uint8_t mask = 0;

  if (sesCtrl <= 4) {
    mask = 1 << (sesCtrl - 1);
  } else {
    Dcm_DslCustomerSession2Mask(mask, sesCtrl);
  }

  return mask;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Dcm_DslInit(void) {
  Dcm_ContextType *context = Dcm_GetContext();
  context->currentSession = DCM_DEFAULT_SESSION;
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
  context->currentLevel = DCM_SEC_LEV_LOCKED;
  context->requestLevel = DCM_SEC_LEV_LOCKED;
#endif
  context->timerS3Server = 0;
  context->timerP2Server = 0;
  context->opStatus = DCM_INITIAL;
#if defined(DCM_USE_SERVICE_REQUEST_DOWNLOAD) || defined(DCM_USE_SERVICE_REQUEST_UPLOAD)
  context->UDTData.state = DCM_UDT_IDLE_STATE;
  context->UDTData.blockSequenceCounter = 0;
  context->UDTData.memoryAddress = 0;
  context->UDTData.memorySize = 0;
  context->UDTData.offset = 0;
#endif

#ifdef DCM_USE_SERVICE_ECU_RESET
  context->resetType = 0;
  context->timer2Reset = 0;
#endif
}

void Dcm_DslProcessingDone(Dcm_ContextType *context, const Dcm_ConfigType *config,
                           Dcm_NegativeResponseCodeType nrc) {

  if (DCM_BUFFER_IDLE != context->txBufferState) {
    ASLOG(ERROR, ("Dcm Tx buffer is not idel when processing is done, drop previous response\n"));
  }

  if (DCM_POS_RESP == nrc) {
    context->rxBufferState = DCM_BUFFER_IDLE;
    if (DCM_SUPRESS_POSITIVE_RESPONCE == context->msgContext.msgAddInfo.suppressPosResponse) {
      context->txBufferState = DCM_BUFFER_IDLE;
    } else {
      config->txBuffer[0] = config->rxBuffer[0] | SID_RESPONSE_BIT;
      context->TxTpSduLength = context->msgContext.resDataLen + 1;
      context->txBufferState = DCM_BUFFER_FULL;
    }
    context->opStatus = DCM_INITIAL;
    context->timerP2Server = 0;
  } else if ((DCM_FUNCTIONAL_REQUEST == context->msgContext.msgAddInfo.reqType) &&
             (DCM_E_SERVICE_NOT_SUPPORTED == nrc)) {
    context->rxBufferState = DCM_BUFFER_IDLE;
    /* generally no response for funtional request if service is not supported */
  } else if (DCM_E_RESPONSE_PENDING == nrc) {
    context->opStatus = DCM_PENDING;
    context->timerP2Server = config->timing->P2ServerMin;
    context->responcePending = DCM_RESPONSE_PENDING;
  } else {
    config->txBuffer[0] = SID_NEGATIVE_RESPONSE;
    config->txBuffer[1] = config->rxBuffer[0];
    config->txBuffer[2] = nrc;

    context->TxTpSduLength = 3;
    context->txBufferState = DCM_BUFFER_FULL;
    context->rxBufferState = DCM_BUFFER_IDLE;

    context->opStatus = DCM_INITIAL;
    context->timerP2Server = 0;
  }
}

Std_ReturnType Dcm_DslIsSessionSupported(Dcm_SesCtrlType sesCtrl, uint8_t sesMask) {
  Std_ReturnType r = E_NOT_OK;
  uint8_t mask = Dcm_DslSession2Mask(sesCtrl);

  if (sesMask & mask) {
    r = E_OK;
  }

  return r;
}

Std_ReturnType Dcm_DslServiceSesSecPhyFuncCheck(Dcm_ContextType *context,
                                                const Dcm_SesSecAccessType *sesSecAccess,
                                                Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;

  r = Dcm_DslIsSessionSupported(context->currentSession, sesSecAccess->sessionMask);
  if (E_OK == r) {
    r = E_NOT_OK;
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
    if (sesSecAccess->securityMask & (1 << context->currentLevel)) {
#endif
      if (sesSecAccess->miscMask & (1 << context->msgContext.msgAddInfo.reqType)) {
        r = E_OK;
      } else {
        *nrc = DCM_E_SERVICE_NOT_SUPPORTED;
      }
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
    } else {
      *nrc = DCM_E_SECUTITY_ACCESS_DENIED;
    }
#endif
  } else {
    *nrc = DCM_E_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION;
  }

  return r;
}

Std_ReturnType Dcm_DslServiceSubFuncSesSecPhyFuncCheck(Dcm_ContextType *context,
                                                       const Dcm_SesSecAccessType *sesSecAccess,
                                                       Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = Dcm_DslServiceSesSecPhyFuncCheck(context, sesSecAccess, nrc);

  if (E_OK != r) {
    if (DCM_E_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION == *nrc) {
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION;
    }
  }

  return r;
}
Std_ReturnType Dcm_DslServiceDIDSesSecPhyFuncCheck(Dcm_ContextType *context,
                                                   const Dcm_SesSecAccessType *sesSecAccess,
                                                   Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = Dcm_DslServiceSesSecPhyFuncCheck(context, sesSecAccess, nrc);

  if (E_OK != r) {
    if (DCM_E_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION == *nrc) {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    } else if (DCM_E_SERVICE_NOT_SUPPORTED == *nrc) {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  }

  return r;
}

Std_ReturnType Dcm_GetSecurityLevel(Dcm_SecLevelType *SecLevel) {
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
  Dcm_ContextType *context = Dcm_GetContext();
  *SecLevel = context->currentLevel;
#else
  *SecLevel = DCM_SEC_LEV_LOCKED;
#endif
  return E_OK;
}

Std_ReturnType Dcm_GetSesCtrlType(Dcm_SesCtrlType *SesCtrlType) {
  Dcm_ContextType *context = Dcm_GetContext();
  *SesCtrlType = context->currentSession;
  return E_OK;
}

void Dcm_DslMainFunction(void) {
  Dcm_NegativeResponseCodeType nrc;
  Dcm_ContextType *context = Dcm_GetContext();
  const Dcm_ConfigType *config = Dcm_GetConfig();

  if (context->timerP2Server > 0) { /* @SWS_Dcm_00024 */
    context->timerP2Server--;
    if (0 == context->timerP2Server) {
      if (context->respPendCnt < config->dslDisgResp->MaxNumRespPend) {
        ASLOG(DCM, ("p2server timeout!\n"));
        Dcm_DslProcessingDone(context, config, DCM_E_RESPONSE_PENDING);
        context->respPendCnt++;
      } else { /* @SWS_Dcm_00120 */
        context->opStatus = DCM_CANCEL;
        if (context->curService != NULL) {
          (void)context->curService->dspServiceFnc(&context->msgContext, &nrc);
          (void)nrc;
        } else {
          ASLOG(DCME, ("Fatal ERROR as null service\n"));
        }
        Dcm_DslProcessingDone(context, config, DCM_E_GENERAL_REJECT);
      }
    }
  }

  if (context->timerS3Server > 0) {
    context->timerS3Server--;
    if (0 == context->timerS3Server) {
      Dcm_SessionChangeIndication(context->currentSession, DCM_DEFAULT_SESSION, TRUE);
      memset(context, 0, sizeof(Dcm_ContextType));
      context->rxBufferState = DCM_BUFFER_IDLE;
      context->txBufferState = DCM_BUFFER_IDLE;
      context->curPduId = DCM_INVALID_PDU_ID;
      Dcm_DslInit();
      Dcm_DspInit();
      ASLOG(INFO, ("DCM s3server timeout!\n"));
    }
  }

#ifdef DCM_USE_SERVICE_ECU_RESET
  if (context->timer2Reset > 0) {
    context->timer2Reset--;
    if (0 == context->timer2Reset) {
      Dcm_PerformReset(context->resetType);
    }
  }
#endif

#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
  if (context->securityDelayTimer > 0) {
    context->securityDelayTimer--;
    if (0 == context->securityDelayTimer) {
      ASLOG(INFO, ("DCM security timer timeout!\n"));
    }
  }
#endif
}

#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
Std_ReturnType Dcm_DslSecurityAccessRequestSeed(Dcm_MsgContextType *msgContext,
                                                const Dcm_SecLevelConfigType *secLevelConfig,
                                                Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  if (secLevelConfig->secLevel == context->currentLevel) {
    /* @SWS_Dcm_00323: already unlocked send 0 seed */
    r = E_OK;
    memset(&msgContext->resData[1], 0, secLevelConfig->seedSize);
  } else {
    r = secLevelConfig->GetSeedFnc(&msgContext->resData[1], nrc);
  }

  if (E_OK == r) {
    msgContext->resData[0] = msgContext->reqData[0];
    msgContext->resDataLen = 1 + secLevelConfig->seedSize;
    context->requestLevel = secLevelConfig->secLevel;
  }

  return r;
}

Std_ReturnType Dcm_DslSecurityAccessCompareKey(Dcm_MsgContextType *msgContext,
                                               const Dcm_SecLevelConfigType *secLevelConfig,
                                               Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  const Dcm_ConfigType *config = Dcm_GetConfig();

  if (context->requestLevel != secLevelConfig->secLevel) {
    *nrc = DCM_E_REQUEST_SEQUENCE_ERROR;
  } else {
    r = secLevelConfig->CompareKeyFnc(&msgContext->reqData[1], nrc);
  }

  if (E_OK == r) {
    msgContext->resData[0] = msgContext->reqData[0];
    msgContext->resDataLen = 1;
    context->currentLevel = secLevelConfig->secLevel;
    context->requestLevel = DCM_SEC_LEV_LOCKED;
#ifdef DCM_USE_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER
    Dcm_ReadPeriodicDID_OnSessionSecurityChange(); /* @SWS_Dcm_01112 */
#endif
    if (Dcm_NvmSecurityAccess_Ram.AttemptCounter != 0) {
      Dcm_NvmSecurityAccess_Ram.AttemptCounter = 0;
#ifdef USE_NVM
      NvM_WriteBlock(config->SecurityNvMBlkId, NULL);
#endif
    }
  } else {
    if (*nrc == DCM_POS_RESP) {
      *nrc = DCM_E_INVALID_KEY;
    }

    if (DCM_E_INVALID_KEY == *nrc) {
      /* @SWS_Dcm_01349 */
      if (Dcm_NvmSecurityAccess_Ram.AttemptCounter < config->SecurityNumAttDelay) {
        Dcm_NvmSecurityAccess_Ram.AttemptCounter++;
#ifdef USE_NVM
        NvM_WriteBlock(config->SecurityNvMBlkId, NULL);
#endif
      }
      if (Dcm_NvmSecurityAccess_Ram.AttemptCounter >= config->SecurityNumAttDelay) {
        context->securityDelayTimer = config->SecurityDelayTime;
        *nrc = DCM_E_EXCEED_NUMBER_OF_ATTEMPTS;
      }
    }
  }

  return r;
}

Std_ReturnType Dcm_SetSecurityLevel(Dcm_SecLevelType SecLevel) {
  Std_ReturnType r = E_OK;
  Dcm_ContextType *context = Dcm_GetContext();

  context->currentLevel = SecLevel;

  return r;
}
#endif /* DCM_USE_SERVICE_SECURITY_ACCESS */

Std_ReturnType Dcm_SetSesCtrlType(Dcm_SesCtrlType SesCtrlType) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  const Dcm_ConfigType *config = Dcm_GetConfig();
  uint8_t mask = Dcm_DslSession2Mask(SesCtrlType);

  if (mask != 0) {
    memset(context, 0, sizeof(Dcm_ContextType));
    context->rxBufferState = DCM_BUFFER_IDLE;
    context->txBufferState = DCM_BUFFER_IDLE;
    context->curPduId = DCM_INVALID_PDU_ID;
    Dcm_DslInit();
    context->currentSession = SesCtrlType;
    if (SesCtrlType != DCM_DEFAULT_SESSION) {
      context->timerS3Server = config->timing->S3Server;
    }
    r = E_OK;
  }

  return r;
}

Std_ReturnType Dcm_ResetToDefaultSession(void) {
  return Dcm_SetSesCtrlType(DCM_DEFAULT_SESSION);
}
