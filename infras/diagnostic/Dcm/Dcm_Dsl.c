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
#include <string.h>
#include "NvM.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DCM 0
#define AS_LOG_DCMI 0
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
  uint8_t mask = 0u;

  if (sesCtrl <= 4u) {
    mask = (uint8_t)(1u << (sesCtrl - 1u));
  } else {
    Dcm_DslCustomerSession2Mask(mask, sesCtrl);
  }

  return mask;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Dcm_DslInit(void) {
  Dcm_ContextType *context = Dcm_GetContext();
  context->CurrentSession = DCM_DEFAULT_SESSION;
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
  context->CurrentLevel = DCM_SEC_LEV_LOCKED;
  context->RequestLevel = DCM_SEC_LEV_LOCKED;
#endif
  context->TimerS3Server = 0;
  context->TimerP2Server = 0;
  context->OpStatus = DCM_INITIAL;
#if defined(DCM_USE_SERVICE_REQUEST_DOWNLOAD) || defined(DCM_USE_SERVICE_REQUEST_UPLOAD)
  context->UDTData.State = DCM_UDT_IDLE_STATE;
  context->UDTData.BlockSequenceCounter = 0;
  context->UDTData.MemoryAddress = 0;
  context->UDTData.MemorySize = 0;
  context->UDTData.Offset = 0;
#endif

#ifdef DCM_USE_SERVICE_ECU_RESET
  context->ResetType = 0;
  context->Timer2Reset = 0;
#endif
}

void Dcm_DslProcessingDone(Dcm_ContextType *context,
                           P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config,
                           Dcm_NegativeResponseCodeType nrc) {

  if (DCM_BUFFER_IDLE != context->TxBufferState) {
    ASLOG(ERROR, ("Dcm Tx buffer is not idel when processing is done, drop previous response\n"));
  }

  ASLOG(DCM, ("Done with NRC %" PRIx8 "\n", nrc));
  if (DCM_POS_RESP == nrc) {
    context->RxBufferState = DCM_BUFFER_IDLE;
    if (DCM_SUPRESS_POSITIVE_RESPONCE == context->MsgContext.MsgAddInfo.SuppressPosResponse) {
      context->TxBufferState = DCM_BUFFER_IDLE;
    } else {
      config->TxBuffer[0] = config->RxBuffer[0] | SID_RESPONSE_BIT;
      context->TxTpSduLength = context->MsgContext.ResDataLen + 1u;
      context->TxBufferState = DCM_BUFFER_FULL;
    }
    context->OpStatus = DCM_INITIAL;
    context->TimerP2Server = 0;
  } else if ((DCM_FUNCTIONAL_REQUEST == context->MsgContext.MsgAddInfo.ReqType) &&
             ((DCM_E_SERVICE_NOT_SUPPORTED == nrc) ||
              (DCM_E_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION == nrc) ||
              (DCM_E_SUB_FUNCTION_NOT_SUPPORTED == nrc) ||
              (DCM_E_SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION == nrc) ||
              (DCM_E_REQUEST_OUT_OF_RANGE == nrc))) {
    context->RxBufferState = DCM_BUFFER_IDLE;
    /* generally no response for funtional request if service or sub function is not supported */
  } else if (DCM_E_RESPONSE_PENDING == nrc) {
    context->OpStatus = DCM_PENDING;
    context->ResponsePending = DCM_RESPONSE_PENDING;
  } else {
    config->TxBuffer[0] = SID_NEGATIVE_RESPONSE;
    config->TxBuffer[1] = config->RxBuffer[0];
    config->TxBuffer[2] = nrc;

    context->TxTpSduLength = 3;
    context->TxBufferState = DCM_BUFFER_FULL;
    context->RxBufferState = DCM_BUFFER_IDLE;

    context->OpStatus = DCM_INITIAL;
    context->TimerP2Server = 0;
  }
  /* restart S3Server if any response transmited */
  context->TimerS3Server = config->Timing->S3Server;
}

Std_ReturnType Dcm_DslIsSessionSupported(Dcm_SesCtrlType sesCtrl, uint8_t sesMask) {
  Std_ReturnType r = E_NOT_OK;
  uint8_t mask = Dcm_DslSession2Mask(sesCtrl);

  if (0u != (sesMask & mask)) {
    r = E_OK;
  }

  return r;
}

Std_ReturnType Dcm_DslServiceSesSecPhyFuncCheck(Dcm_ContextType *context,
                                                P2CONST(Dcm_SesSecAccessType, AUTOMATIC, DCM_CONST)
                                                  sesSecAccess,
                                                Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;

  r = Dcm_DslIsSessionSupported(context->CurrentSession, sesSecAccess->SessionMask);
  if (E_OK == r) {
    r = E_NOT_OK;
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
    if (0u != (sesSecAccess->SecurityMask & (1u << context->CurrentLevel))) {
#endif
      if (0u != (sesSecAccess->MiscMask & (1u << context->MsgContext.MsgAddInfo.ReqType))) {
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
                                                       P2CONST(Dcm_SesSecAccessType, AUTOMATIC,
                                                               DCM_CONST) sesSecAccess,
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
                                                   P2CONST(Dcm_SesSecAccessType, AUTOMATIC,
                                                           DCM_CONST) sesSecAccess,
                                                   Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = Dcm_DslServiceSesSecPhyFuncCheck(context, sesSecAccess, nrc);

  if (E_OK != r) {
    if (DCM_E_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION == *nrc) {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    } else if (DCM_E_SERVICE_NOT_SUPPORTED == *nrc) {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    } else {
      /* do nothing */
    }
  }

  return r;
}

Std_ReturnType Dcm_GetSecurityLevel(Dcm_SecLevelType *secLevel) {
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
  Dcm_ContextType *context = Dcm_GetContext();
  *secLevel = context->CurrentLevel;
#else
  *secLevel = DCM_SEC_LEV_LOCKED;
#endif
  return E_OK;
}

Std_ReturnType Dcm_GetSesCtrlType(Dcm_SesCtrlType *sesCtrlType) {
  Dcm_ContextType *context = Dcm_GetContext();
  *sesCtrlType = context->CurrentSession;
  return E_OK;
}

void Dcm_DslMainFunction(void) {
  Dcm_NegativeResponseCodeType nrc;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();

  if (context->TimerP2Server > 0u) { /* @SWS_Dcm_00024 */
    context->TimerP2Server--;
    if (0u == context->TimerP2Server) {
      if (context->RespPendCnt < config->DslDisgResp->MaxNumRespPend) {
        ASLOG(DCM, ("p2server timeout!\n"));
        Dcm_DslProcessingDone(context, config, DCM_E_RESPONSE_PENDING);
        context->RespPendCnt++;
        context->TimerP2Server =
          config->Timing->P2StarServerMax - config->Timing->P2StarServerAdjust;
      } else { /* @SWS_Dcm_00120 */
        context->OpStatus = DCM_CANCEL;
        if (context->CurService != NULL) {
          (void)context->CurService->DspServiceFnc(&context->MsgContext, &nrc);
          (void)nrc;
        } else {
          ASLOG(DCME, ("Fatal ERROR as null service\n"));
        }
        Dcm_DslProcessingDone(context, config, DCM_E_GENERAL_REJECT);
      }
    }
  }

  if (context->TimerS3Server > 0u) {
    context->TimerS3Server--;
    if (0u == context->TimerS3Server) {
      Dcm_SessionChangeIndication(context->CurrentSession, DCM_DEFAULT_SESSION, TRUE);
      (void)memset(context, 0, sizeof(Dcm_ContextType));
      context->RxBufferState = DCM_BUFFER_IDLE;
      context->TxBufferState = DCM_BUFFER_IDLE;
      context->CurPduId = DCM_INVALID_PDU_ID;
      Dcm_DslInit();
      Dcm_DspInit();
      ASLOG(INFO, ("DCM s3server timeout!\n"));
    }
  }

#ifdef DCM_USE_SERVICE_ECU_RESET
  if (context->Timer2Reset > 0u) {
    context->Timer2Reset--;
    if (0u == context->Timer2Reset) {
      Dcm_PerformReset(context->ResetType);
    }
  }
#endif

#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
  if (context->SecurityDelayTimer > 0u) {
    context->SecurityDelayTimer--;
    if (0u == context->SecurityDelayTimer) {
      ASLOG(DCM, ("DCM security timer timeout!\n"));
    }
  }
#endif
}

#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
Std_ReturnType Dcm_DslSecurityAccessRequestSeed(Dcm_MsgContextType *msgContext,
                                                P2CONST(Dcm_SecLevelConfigType, AUTOMATIC,
                                                        DCM_CONST) secLevelConfig,
                                                Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
#ifdef DCM_USE_SECURITY_SEED_PROTECTION
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
#endif

  if (secLevelConfig->SecLevel == context->CurrentLevel) {
    /* @SWS_Dcm_00323: already unlocked send 0 seed */
    r = E_OK;
    (void)memset(&msgContext->ResData[1], 0, secLevelConfig->SeedSize);
  } else {
#ifdef DCM_USE_SECURITY_SEED_PROTECTION
    if (context->RequestLevel != secLevelConfig->SecLevel) {
      r = secLevelConfig->GetSeedFnc(&msgContext->ResData[1], nrc);
      if (E_OK == r) {
        (void)memcpy(context->CachedSeed, &msgContext->ResData[1], secLevelConfig->SeedSize);
      }
    } else {
      (void)memcpy(&msgContext->ResData[1], context->CachedSeed, secLevelConfig->SeedSize);
      r = E_OK; /* given the same seed for seed protection purpose */
    }
#else
    r = secLevelConfig->GetSeedFnc(&msgContext->ResData[1], nrc);
#endif
  }

  if (E_OK == r) {
    msgContext->ResData[0] = msgContext->ReqData[0];
    msgContext->ResDataLen = 1u + secLevelConfig->SeedSize;
    context->RequestLevel = secLevelConfig->SecLevel;
#ifdef DCM_USE_SECURITY_SEED_PROTECTION
    if (Dcm_NvmSecurityAccess_Ram.AttemptCounter < (config->SecurityNumAttDelay + 2u)) {
      Dcm_NvmSecurityAccess_Ram.AttemptCounter++;
#ifdef USE_NVM
      (void)NvM_WriteBlock(config->SecurityNvMBlkId, NULL);
#endif
      ASLOG(DCMI, ("Seurity Att Cnt=%" PRIu8 "\n", Dcm_NvmSecurityAccess_Ram.AttemptCounter));
    }
    if (Dcm_NvmSecurityAccess_Ram.AttemptCounter > (config->SecurityNumAttDelay + 1u)) {
      context->SecurityDelayTimer = config->SecurityDelayTime;
    } else if (Dcm_NvmSecurityAccess_Ram.AttemptCounter == (config->SecurityNumAttDelay + 1u)) {
      context->SecurityDelayTimer = config->SecurityDelayTime;
      /* allow to generate a new seed if timer timeout */
      context->RequestLevel = DCM_SEC_LEV_LOCKED;
      *nrc = DCM_E_EXCEED_NUMBER_OF_ATTEMPTS;
      r = E_NOT_OK;
    } else {
      /* do nothing */
    }
#endif
  } else {
    context->RequestLevel = DCM_SEC_LEV_LOCKED;
  }

  return r;
}

Std_ReturnType Dcm_DslSecurityAccessCompareKey(Dcm_MsgContextType *msgContext,
                                               P2CONST(Dcm_SecLevelConfigType, AUTOMATIC, DCM_CONST)
                                                 secLevelConfig,
                                               Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
#if !defined(DCM_USE_SECURITY_SEED_PROTECTION) || defined(USE_NVM)
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
#endif
  if (context->RequestLevel != secLevelConfig->SecLevel) {
    *nrc = DCM_E_REQUEST_SEQUENCE_ERROR;
  } else {
    context->RequestLevel = DCM_SEC_LEV_LOCKED; /* reset request level */
    r = secLevelConfig->CompareKeyFnc(&msgContext->ReqData[1], nrc);
  }

  if (E_OK == r) {
    msgContext->ResData[0] = msgContext->ReqData[0];
    msgContext->ResDataLen = 1;
    context->CurrentLevel = secLevelConfig->SecLevel;
#ifdef DCM_USE_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER
    Dcm_ReadPeriodicDID_OnSessionSecurityChange(); /* @SWS_Dcm_01112 */
#endif
    if (Dcm_NvmSecurityAccess_Ram.AttemptCounter != 0u) {
      Dcm_NvmSecurityAccess_Ram.AttemptCounter = 0u;
#ifdef USE_NVM
      (void)NvM_WriteBlock(config->SecurityNvMBlkId, NULL);
      ASLOG(DCMI, ("Seurity Att Cnt=0\n"));
#endif
    }
  } else {
    if (*nrc == DCM_POS_RESP) {
      *nrc = DCM_E_INVALID_KEY;
    }

    if (DCM_E_INVALID_KEY == *nrc) {
#ifndef DCM_USE_SECURITY_SEED_PROTECTION /* @SWS_Dcm_01349 */
      if (Dcm_NvmSecurityAccess_Ram.AttemptCounter < config->SecurityNumAttDelay) {
        Dcm_NvmSecurityAccess_Ram.AttemptCounter++;
#ifdef USE_NVM
        NvM_WriteBlock(config->SecurityNvMBlkId, NULL);
#endif
        ASLOG(DCMI, ("Seurity Att Cnt=%" PRIu8 "\n", Dcm_NvmSecurityAccess_Ram.AttemptCounter));
      }
      if (Dcm_NvmSecurityAccess_Ram.AttemptCounter >= config->SecurityNumAttDelay) {
        context->SecurityDelayTimer = config->SecurityDelayTime;
        *nrc = DCM_E_EXCEED_NUMBER_OF_ATTEMPTS;
      }
#endif
    }
  }

  return r;
}

Std_ReturnType Dcm_SetSecurityLevel(Dcm_SecLevelType secLevel) {
  Std_ReturnType r = E_OK;
  Dcm_ContextType *context = Dcm_GetContext();

  context->CurrentLevel = secLevel;

  return r;
}
#endif /* DCM_USE_SERVICE_SECURITY_ACCESS */

Std_ReturnType Dcm_SetSesCtrlType(Dcm_SesCtrlType sesCtrlType) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  uint8_t mask = Dcm_DslSession2Mask(sesCtrlType);

  if (mask != 0u) {
    (void)memset(context, 0, sizeof(Dcm_ContextType));
    context->RxBufferState = DCM_BUFFER_IDLE;
    context->TxBufferState = DCM_BUFFER_IDLE;
    context->CurPduId = DCM_INVALID_PDU_ID;
    Dcm_DslInit();
    context->CurrentSession = sesCtrlType;
    if (sesCtrlType != DCM_DEFAULT_SESSION) {
      context->TimerS3Server = config->Timing->S3Server;
    }
    r = E_OK;
  }

  return r;
}

Std_ReturnType Dcm_ResetToDefaultSession(void) {
  return Dcm_SetSesCtrlType(DCM_DEFAULT_SESSION);
}
