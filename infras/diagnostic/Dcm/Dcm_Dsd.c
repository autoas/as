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
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DCM 1
#define AS_LOG_DCME 3
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static const Dcm_ServiceType *Dsd_FindService(const Dcm_ServiceTableType *servieTable,
                                              uint8_t SID) {
  int i;
  const Dcm_ServiceType *service = NULL;

  for (i = 0; i < servieTable->numOfServices; i++) {
    if (servieTable->services[i].SID == SID) {
      service = &servieTable->services[i];
      break;
    }
  }

  return service;
}

static void Dsd_HandleRequest(Dcm_ContextType *context, const Dcm_ConfigType *config) {
  /* @SWS_Dcm_00084 */
  Dcm_NegativeResponseCodeType nrc = DCM_POS_RESP;
  Std_ReturnType r = E_NOT_OK;
  const Dcm_ServiceTableType *servieTable = Dcm_GetActiveServiceTable(context, config);
  const Dcm_ServiceType *service;
  uint8_t SID = config->rxBuffer[0];

  if (DCM_INITIAL == context->opStatus) {
    ASLOG(DCM, ("%s service %02X, len=%d\n",
                context->msgContext.msgAddInfo.reqType ? "funtional" : "physical", SID,
                context->RxTpSduLength));
    /* always restart the s3server timer on any request */
    context->timerS3Server = config->timing->S3Server;
    service = Dsd_FindService(servieTable, SID);
    if (NULL != service) {
      r = Dcm_DslServiceSesSecPhyFuncCheck(context, &service->SesSecAccess, &nrc);
      if (E_OK == r) {
        context->currentSID = config->rxBuffer[0];
        context->curService = service;
        context->msgContext.msgAddInfo.suppressPosResponse = DCM_NOT_SUPRESS_POSITIVE_RESPONCE;
        if (service->SesSecAccess.miscMask & DCM_MISC_SUB_FUNCTION) {
          if (SUPPRESS_POS_RESP_BIT & config->rxBuffer[1]) {
            context->msgContext.msgAddInfo.suppressPosResponse = DCM_SUPRESS_POSITIVE_RESPONCE;
            config->rxBuffer[1] &= ~SUPPRESS_POS_RESP_BIT;
          }
        }
      }
    } else {
      nrc = DCM_E_SERVICE_NOT_SUPPORTED;
    }
  } else if (DCM_PENDING == context->opStatus) {
    r = E_OK;
  } else {
    /* fatal memory or logical error */
    r = E_NOT_OK;
    nrc = DCM_E_CONDITIONS_NOT_CORRECT;
  }

  if (E_OK == r) {
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
  }

  if (DCM_E_RESPONSE_PENDING == nrc) {
    if (DCM_E_FORCE_RCRRP == r) {
      Dcm_DslProcessingDone(context, config, DCM_E_RESPONSE_PENDING);
    } else {
      if (DCM_INITIAL == context->opStatus) {
        ASLOG(DCM, ("start p2server\n"));
        context->respPendCnt = 0;
        context->timerP2Server = config->timing->P2ServerMin;
      }
      context->opStatus = DCM_PENDING;
    }
  } else {
    Dcm_DslProcessingDone(context, config, nrc);
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
const Dcm_ServiceTableType *Dcm_GetActiveServiceTable(Dcm_ContextType *context,
                                                      const Dcm_ConfigType *config) {
  return config->serviceTables[0];
}

void Dcm_MainFunction_Request(void) {
  Dcm_ContextType *context = Dcm_GetContext();
  const Dcm_ConfigType *config = Dcm_GetConfig();
  if ((DCM_BUFFER_FULL == context->rxBufferState) && (DCM_BUFFER_IDLE == context->txBufferState)) {
    Dsd_HandleRequest(context, config);
  }
}