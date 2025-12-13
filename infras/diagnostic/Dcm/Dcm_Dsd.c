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
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DCM 0
#define AS_LOG_DCMI 0
#define AS_LOG_DCME 3
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static P2CONST(Dcm_ServiceType, AUTOMATIC, DCM_CONST)
  Dsd_FindService(P2CONST(Dcm_ServiceTableType, AUTOMATIC, DCM_CONST) servieTable, uint8_t SID) {
  uint16_t i;
  P2CONST(Dcm_ServiceType, AUTOMATIC, DCM_CONST) service = NULL;

  for (i = 0u; i < servieTable->numOfServices; i++) {
    if (servieTable->services[i].SID == SID) {
      service = &servieTable->services[i];
      break;
    }
  }

  return service;
}

static void Dsd_HandleRequest(Dcm_ContextType *context,
                              P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config) {
  /* @SWS_Dcm_00084 */
  Dcm_NegativeResponseCodeType nrc = DCM_POS_RESP;
  Std_ReturnType r = E_NOT_OK;
  P2CONST(Dcm_ServiceTableType, AUTOMATIC, DCM_CONST)
  servieTable = Dcm_GetActiveServiceTable(context, config);
  P2CONST(Dcm_ServiceType, AUTOMATIC, DCM_CONST) service;
  uint8_t SID = config->rxBuffer[0];
  /* atomic on this to prevent ISR tx confirm that changed this 'responsePending' state */
  uint8_t responsePending = context->responsePending;

  if (DCM_INITIAL == context->opStatus) {
    ASLOG(DCMI, ("%s service %02X, len=%d\n",
                 context->msgContext.msgAddInfo.reqType ? "funtional" : "physical", SID,
                 context->RxTpSduLength));
    /* always restart the s3server timer on any request */
    context->timerS3Server = config->timing->S3Server;

    if (NULL != config->ServiceVerificationFnc) {
      r = config->ServiceVerificationFnc(context->msgContext.dcmRxPduId, config->rxBuffer,
                                         context->RxTpSduLength, &nrc);
      if (E_REQUEST_NOT_ACCEPTED == r) { /* @SWS_Dcm_00462:  reject and no response */
        context->rxBufferState = DCM_BUFFER_IDLE;
        context->timerP2Server = 0;
        responsePending = TRUE;
      } else if (E_OK != r) {
        if (DCM_POS_RESP == nrc) {
          ASLOG(DCME, ("Fatal verify service %X forgot to set NRC\n", SID));
          nrc = DCM_E_CONDITIONS_NOT_CORRECT;
        }
      } else {
        /* accpeted by application */
      }
    } else {
      r = E_OK;
    }

    if (E_OK == r) {
      service = Dsd_FindService(servieTable, SID);
      if (NULL != service) {
        r = Dcm_DslServiceSesSecPhyFuncCheck(context, &service->SesSecAccess, &nrc);
        if (E_OK == r) {
          context->currentSID = config->rxBuffer[0];
          context->curService = service;
          context->msgContext.msgAddInfo.suppressPosResponse = DCM_NOT_SUPRESS_POSITIVE_RESPONCE;
          if (0u != (service->SesSecAccess.miscMask & DCM_MISC_SUB_FUNCTION)) {
            if (0u != (SUPPRESS_POS_RESP_BIT & config->rxBuffer[1])) {
              context->msgContext.msgAddInfo.suppressPosResponse = DCM_SUPRESS_POSITIVE_RESPONCE;
              config->rxBuffer[1] &= ~SUPPRESS_POS_RESP_BIT;
            }
          }
        }
      } else {
        r = E_NOT_OK;
        nrc = DCM_E_SERVICE_NOT_SUPPORTED;
      }
    }
  } else if (DCM_PENDING == context->opStatus) {
    r = E_OK;
  } else if (DCM_FORCE_RCRRP_OK == context->opStatus) {
    if (DCM_NO_RESPONSE_PENDING == responsePending) {
      r = E_OK; /* SWS_Dcm_00528: the Dcm shall not realize further invocation of the operation till
                   RCR-RP is transmitted */
    }
  } else {
    /* fatal memory or logical error */
    r = E_NOT_OK;
    nrc = DCM_E_CONDITIONS_NOT_CORRECT;
  }

  if (DCM_NO_RESPONSE_PENDING != responsePending) {
    /* a pending response is on going, wait it done */
    nrc = DCM_E_RESPONSE_PENDING;
  } else if (E_OK == r) {
    r = context->curService->dspServiceFnc(&context->msgContext, &nrc);
    if (r != E_OK) {
      if (DCM_POS_RESP == nrc) {
        if ((DCM_E_PENDING == r) || (DCM_E_FORCE_RCRRP == r)) {
          nrc = DCM_E_RESPONSE_PENDING;
        } else {
          ASLOG(DCME, ("Fatal service %X forgot to set NRC\n", SID));
          nrc = DCM_E_CONDITIONS_NOT_CORRECT;
        }
      }
    }
  } else {
    /* failed case, do nothing */
  }

  if (DCM_E_RESPONSE_PENDING == nrc) {
    if (DCM_E_FORCE_RCRRP == r) {
      /* @SWS_Dcm_00528 */
      if (DCM_INITIAL == context->opStatus) {
        context->respPendCnt = 1;
      } else {
        context->respPendCnt++;
      }
      Dcm_DslProcessingDone(context, config, DCM_E_RESPONSE_PENDING);
      context->timerP2Server = config->timing->P2StarServerMax - config->timing->P2StarServerAdjust;
      /* @SWS_Dcm_00529 */
      context->opStatus = DCM_FORCE_RCRRP_OK;
    } else {
      if (DCM_INITIAL == context->opStatus) {
        ASLOG(DCM, ("start p2server\n"));
        context->respPendCnt = 0;
        /* @SWS_Dcm_00024 */
        context->timerP2Server = config->timing->P2ServerMax - config->timing->P2ServerAdjust;
      }
      context->opStatus = DCM_PENDING;
    }
  } else {
    if (DCM_NO_RESPONSE_PENDING == responsePending) {
      Dcm_DslProcessingDone(context, config, nrc);
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
P2CONST(Dcm_ServiceTableType, AUTOMATIC, DCM_CONST)
Dcm_GetActiveServiceTable(Dcm_ContextType *context,
                          P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config) {
  (void)context;
  return config->serviceTables[0];
}

void Dcm_MainFunction_Request(void) {
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  if ((DCM_BUFFER_FULL == context->rxBufferState) && (DCM_BUFFER_IDLE == context->txBufferState)) {
    Dsd_HandleRequest(context, config);
  }
}
