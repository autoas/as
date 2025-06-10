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
#include "Dem.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DCM 0
#define AS_LOG_DCME 3
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
#ifdef DCM_USE_SERVICE_ROUTINE_CONTROL
static Std_ReturnType
Dcm_DspRoutineControlStart(Dcm_MsgContextType *msgContext, Dcm_OpStatusType OpStatus,
                           P2CONST(Dcm_RoutineControlType, AUTOMATIC, DCM_CONST) rtCtrl,
                           Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  uint16_t currentDataLength = msgContext->reqDataLen - 3u;

  r = rtCtrl->StartRoutineFnc(&msgContext->reqData[3], OpStatus, &msgContext->resData[3],
                              &currentDataLength, nrc);

  if (E_OK == r) {
    msgContext->resData[0] = 0x01u;
    msgContext->resData[1] = (rtCtrl->id >> 8u) & 0xFFu;
    msgContext->resData[2] = rtCtrl->id & 0xFFu;
    msgContext->resDataLen = (Dcm_MsgLenType)(3u + currentDataLength);
  }

  return r;
}

static Std_ReturnType
Dcm_DspRoutineControlStop(Dcm_MsgContextType *msgContext, Dcm_OpStatusType OpStatus,
                          P2CONST(Dcm_RoutineControlType, AUTOMATIC, DCM_CONST) rtCtrl,
                          Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  uint16_t currentDataLength = msgContext->reqDataLen - 3u;

  if (rtCtrl->StopRoutineFnc != NULL) {
    r = rtCtrl->StopRoutineFnc(&msgContext->reqData[3], OpStatus, &msgContext->resData[3],
                               &currentDataLength, nrc);
  } else {
    *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
  }

  if (E_OK == r) {
    msgContext->resData[0] = 0x02u;
    msgContext->resData[1] = (rtCtrl->id >> 8u) & 0xFFu;
    msgContext->resData[2] = rtCtrl->id & 0xFFu;
    msgContext->resDataLen = (Dcm_MsgLenType)(3u + currentDataLength);
  }

  return r;
}

static Std_ReturnType
Dcm_DspRoutineControlResult(Dcm_MsgContextType *msgContext, Dcm_OpStatusType OpStatus,
                            P2CONST(Dcm_RoutineControlType, AUTOMATIC, DCM_CONST) rtCtrl,
                            Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  uint16_t currentDataLength = msgContext->reqDataLen - 3u;
  if (rtCtrl->RequestResultRoutineFnc != NULL) {
    r = rtCtrl->RequestResultRoutineFnc(&msgContext->reqData[3], OpStatus, &msgContext->resData[3],
                                        &currentDataLength, nrc);
  } else {
    *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
  }
  if (E_OK == r) {
    msgContext->resData[0] = 0x03u;
    msgContext->resData[1] = (rtCtrl->id >> 8u) & 0xFFu;
    msgContext->resData[2] = rtCtrl->id & 0xFFu;
    msgContext->resDataLen = (Dcm_MsgLenType)(3u + currentDataLength);
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_DYNAMICALLY_DEFINE_DATA_IDENTIFIER
static void Dcm_DDDID_Init(void) {
  uint16_t i;
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  P2CONST(Dcm_DDDIDConfigType, AUTOMATIC, DCM_CONST) DDDID;
  for (i = 0; i < config->numOfDDDIDs; i++) {
    DDDID = &config->DDDIDs[i];
    (void)memset(DDDID->context, 0, sizeof(Dcm_DDDIDContextType));
    (void)memset(DDDID->rDID, 0, sizeof(Dcm_rDIDConfigType));
  }
}

static Std_ReturnType Dcm_DspDefineDIDById(Dcm_MsgContextType *msgContext,
                                           Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  uint16_t defID = 0xFFFFu;
  uint16_t srcID;
  uint8_t position;
  uint8_t size;
  uint16_t length = 0u;
  uint16_t i;
  uint16_t j;
  uint16_t numOfDIDs = (msgContext->reqDataLen - 3u) / 4u;
  P2CONST(Dcm_DDDIDConfigType, AUTOMATIC, DCM_CONST) DDDID;
  P2CONST(Dcm_rDIDConfigType, AUTOMATIC, DCM_CONST) rDID;
  Dcm_DDDIDEntryType *entry;
  Dcm_SesSecAccessType SesSecAccess = {0xFF,
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
                                       0xFF,
#endif
                                       0xFF};

  if (msgContext->reqDataLen == ((4u * numOfDIDs) + 3u)) {
    defID = ((uint16_t)msgContext->reqData[1] << 8u) + msgContext->reqData[2];
    if (numOfDIDs > DCM_DDDID_MAX_ENTRY) {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    } else if (0u == numOfDIDs) {
      *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    } else {
      r = E_OK;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  /* check that the DID never defined or exists */
  for (i = 0; (E_OK == r) && (i < config->numOfDDDIDs); i++) {
    DDDID = &config->DDDIDs[i];
    if (DDDID->context->numOfEntry > 0u) {
      if (DDDID->rDID->id == defID) {
        *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        r = E_NOT_OK;
      }
    }
  }

  for (i = 0; (E_OK == r) && (i < config->numOfrDIDs); i++) {
    if (config->rDIDs[i].id == defID) {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    for (i = 0u; (E_OK == r) && (i < numOfDIDs); i++) {
      srcID =
        ((uint16_t)msgContext->reqData[3u + (4u * i)] << 8) + msgContext->reqData[4u + (4u * i)];
      position = msgContext->reqData[5u + (4u * i)];
      size = msgContext->reqData[6u + (4u * i)];

      rDID = NULL;
      for (j = 0u; (NULL == rDID) && (j < config->numOfrDIDs); j++) {
        if (config->rDIDs[j].id == srcID) {
          rDID = &config->rDIDs[j];
        }
      }
      if (NULL == rDID) {
        *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        r = E_NOT_OK;
      } else { /* @SWS_Dcm_00725, SWS_Dcm_00726 */
        r = Dcm_DslServiceDIDSesSecPhyFuncCheck(context, &rDID->SesSecAccess, nrc);
      }
      if (E_OK == r) {
        if (0u == position) {
          *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
          r = E_NOT_OK;
        } else {
          /* index start from 1 */
          position -= 1u;
        }
      }

      if (E_OK == r) {
        if ((position >= rDID->length) || (((uint16_t)position + size) > rDID->length)) {
          *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
          r = E_NOT_OK;
        }
      }

      if (E_OK == r) {
        if ((length + rDID->length + 2u) > msgContext->resMaxDataLen) {
          *nrc = DCM_E_RESPONSE_TOO_LONG;
          r = E_NOT_OK;
        }
        length += size;
      }
    }
  }

  /* find a free slot to define the DDDID */
  if (E_OK == r) {
    DDDID = NULL;
    for (i = 0; (NULL == DDDID) && (i < config->numOfDDDIDs); i++) {
      if (config->DDDIDs[i].context->numOfEntry == 0u) {
        DDDID = &config->DDDIDs[i];
      }
    }
    if (NULL == DDDID) {
      r = E_NOT_OK;
      *nrc = DCM_E_GENERAL_REJECT;
    }
  }

  if (E_OK == r) {
    ASLOG(DCM, ("define DDDID=%X length=%d\n", defID, length));
    for (i = 0u; (E_OK == r) && (i < numOfDIDs); i++) {
      srcID =
        ((uint16_t)msgContext->reqData[3u + (4u * i)] << 8) + msgContext->reqData[4u + (4u * i)];
      position = msgContext->reqData[5u + (4u * i)];
      size = msgContext->reqData[6u + (4u * i)];
      entry = &DDDID->context->entry[i];
      rDID = NULL;
      for (j = 0u; (NULL == rDID) && (j < config->numOfrDIDs); j++) {
        if (config->rDIDs[j].id == srcID) {
          rDID = &config->rDIDs[j];
          entry->index = j;
          entry->position = position - 1u;
          entry->size = size;
          entry->opStatus = DCM_CANCEL;
          SesSecAccess.sessionMask &= rDID->SesSecAccess.sessionMask;
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
          SesSecAccess.securityMask &= rDID->SesSecAccess.securityMask;
#endif
          SesSecAccess.miscMask &= rDID->SesSecAccess.miscMask;
          ASLOG(DCM, ("  define entry ID=%X(%d) position=%d size=%d\n", srcID, j, entry->position,
                      entry->size));
        }
      }
      if (NULL == rDID) {
        r = E_NOT_OK;
        *nrc = DCM_E_CONDITIONS_NOT_CORRECT;
      }
    }
    if (E_OK == r) {
      DDDID->context->numOfEntry = numOfDIDs;
      DDDID->rDID->id = defID;
      DDDID->rDID->length = length;
      DDDID->rDID->readDIdFnc = DDDID->readDIdFnc;
      DDDID->rDID->SesSecAccess = SesSecAccess;
    }
  }

  if (E_OK == r) {
    msgContext->resData[0] = 0x01u;
    msgContext->resData[1] = msgContext->reqData[1];
    msgContext->resData[2] = msgContext->reqData[2];
    msgContext->resDataLen = 3u;
  }

  return r;
}

static Std_ReturnType Dcm_DspDefineDIDByMemoryAddress(Dcm_MsgContextType *msgContext,
                                                      Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  (void)msgContext;
  *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
  return r;
}

static Std_ReturnType Dcm_DspClearDID(Dcm_MsgContextType *msgContext,
                                      Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  uint16_t defID;
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  P2CONST(Dcm_DDDIDConfigType, AUTOMATIC, DCM_CONST) DDDID = NULL;
  uint16_t i;

  if (msgContext->reqDataLen == 3u) {
    defID = ((uint16_t)msgContext->reqData[1] << 8) + msgContext->reqData[2];
    for (i = 0u; i < config->numOfDDDIDs; i++) {
      if (config->DDDIDs[i].context->numOfEntry > 0u) {
        if (config->DDDIDs[i].rDID->id == defID) {
          DDDID = &config->DDDIDs[i];
          r = E_OK;
        }
      }
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    ASLOG(DCM, ("clear DDDID=%X\n", defID));
    (void)memset(DDDID->context, 0, sizeof(Dcm_DDDIDContextType));
    (void)memset(DDDID->rDID, 0, sizeof(Dcm_rDIDConfigType));
    msgContext->resData[0] = 0x03u;
    msgContext->resData[1] = msgContext->reqData[1];
    msgContext->resData[2] = msgContext->reqData[2];
    msgContext->resDataLen = 3u;
  }

  return r;
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType Dcm_DspSessionControl(Dcm_MsgContextType *msgContext,
                                     Dcm_NegativeResponseCodeType *nrc) {

  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  P2CONST(Dcm_SessionControlConfigType, AUTOMATIC, DCM_CONST)
  sesCtrlConfig =
    (P2CONST(Dcm_SessionControlConfigType, AUTOMATIC, DCM_CONST))context->curService->config;
  Dcm_SesCtrlType sesCtrl = msgContext->reqData[0];
  uint16_t i;
  uint16_t u16V;

  if (1u == msgContext->reqDataLen) {
    for (i = 0u; i < sesCtrlConfig->numOfSesCtrls; i++) {
      if (sesCtrl == sesCtrlConfig->sesCtrls[i]) {
        break;
      }
    }
    if (i >= sesCtrlConfig->numOfSesCtrls) {
      /* @SWS_Dcm_00307 */
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
    } else {
      r = E_OK;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    r = sesCtrlConfig->GetSesChgPermissionFnc(context->currentSession, sesCtrl, nrc);
    if (E_OK == r) {
      if (DCM_E_RESPONSE_PENDING == *nrc) {
        r = DCM_E_PENDING; /* pending on session check */
      }
    }
  }

  if (E_OK == r) {
    Dcm_DslInit();
    Dcm_SessionChangeIndication(context->currentSession, sesCtrl, FALSE);
    context->currentSession = sesCtrl;
#ifdef DCM_USE_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER
    Dcm_ReadPeriodicDID_OnSessionSecurityChange(); /* @SWS_Dcm_01111 */
#endif
    u16V = config->timing->P2ServerMax * DCM_MAIN_FUNCTION_PERIOD;
    msgContext->resData[0] = sesCtrl;
    msgContext->resData[1] = (u16V >> 8) & 0xFFu; /* P2Server_max */
    msgContext->resData[2] = u16V & 0xFFu;
    u16V = config->timing->P2StarServerMax * DCM_MAIN_FUNCTION_PERIOD / 10u;
    msgContext->resData[3] = (u16V >> 8) & 0xFFu; /* P2*Server_max */
    msgContext->resData[4] = u16V & 0xFFu;
    msgContext->resDataLen = 5u;
    context->timerS3Server = config->timing->S3Server;
  }

  return r;
}
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
Std_ReturnType Dcm_DspSecurityAccess(Dcm_MsgContextType *msgContext,
                                     Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_SecurityAccessConfigType, AUTOMATIC, DCM_CONST)
  secAccConfig =
    (P2CONST(Dcm_SecurityAccessConfigType, AUTOMATIC, DCM_CONST))context->curService->config;
  P2CONST(Dcm_SecLevelConfigType, AUTOMATIC, DCM_CONST) secLevelConfig = NULL;
  Dcm_SecLevelType secLevel = (msgContext->reqData[0] + 1u) / 2u;
  uint16_t i;

  if (msgContext->reqDataLen >= 1u) {
    for (i = 0u; i < secAccConfig->numOfSesLevels; i++) {
      if (secAccConfig->secLevelConfig[i].secLevel == secLevel) {
        secLevelConfig = &secAccConfig->secLevelConfig[i];
      }
    }
    if (NULL != secLevelConfig) {
      if (0u != (msgContext->reqData[0] & 0x01u)) { /* request seed */
        if (1u == msgContext->reqDataLen) {
          r = E_OK;
        } else {
          *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
        }
      } else {
        if ((1u + secLevelConfig->keySize) == msgContext->reqDataLen) {
          r = E_OK;
        } else {
          *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
        }
      }

      if (E_OK == r) {
        r = Dcm_DslIsSessionSupported(context->currentSession, secLevelConfig->sessionMask);
        if (E_OK != r) {
          *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION;
        }
      }
    } else { /* @SWS_Dcm_00321 */
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    if ((context->securityDelayTimer > 0u)
#ifdef DCM_USE_SECURITY_SEED_PROTECTION
        && (0u != (msgContext->reqData[0] & 0x01u)) /* request seed */
#endif
    ) { /* @SWS_Dcm_01350 */
      *nrc = DCM_E_REQUIRED_TIME_DELAY_NOT_EXPIRED;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    if (0u != (msgContext->reqData[0] & 0x01u)) { /* request seed */
      r = Dcm_DslSecurityAccessRequestSeed(msgContext, secLevelConfig, nrc);
    } else {
      r = Dcm_DslSecurityAccessCompareKey(msgContext, secLevelConfig, nrc);
    }
  }
  return r;
}
#endif

#ifdef DCM_USE_SERVICE_ROUTINE_CONTROL
Std_ReturnType Dcm_DspRoutineControl(Dcm_MsgContextType *msgContext,
                                     Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_RoutineControlConfigType, AUTOMATIC, DCM_CONST)
  rtCtrlConfig =
    (P2CONST(Dcm_RoutineControlConfigType, AUTOMATIC, DCM_CONST))context->curService->config;
  P2CONST(Dcm_RoutineControlType, AUTOMATIC, DCM_CONST) rtCtrl = NULL;
  uint16_t id;
  uint16_t i;

  if (msgContext->reqDataLen >= 3u) {
    id = ((uint16_t)msgContext->reqData[1] << 8) + msgContext->reqData[2];
    for (i = 0; i < rtCtrlConfig->numOfRtCtrls; i++) {
      if (rtCtrlConfig->rtCtrls[i].id == id) {
        rtCtrl = &rtCtrlConfig->rtCtrls[i];
        break;
      }
    }
    if (NULL != rtCtrl) {
      r = Dcm_DslServiceDIDSesSecPhyFuncCheck(context, &rtCtrl->SesSecAccess, nrc);
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    switch (msgContext->reqData[0]) {
    case 0x01u:
      r = Dcm_DspRoutineControlStart(msgContext, context->opStatus, rtCtrl, nrc);
      break;
    case 0x02u:
      r = Dcm_DspRoutineControlStop(msgContext, context->opStatus, rtCtrl, nrc);
      break;
    case 0x03u:
      r = Dcm_DspRoutineControlResult(msgContext, context->opStatus, rtCtrl, nrc);
      break;
    default:
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
      break;
    }
  }

  return r;
}
#endif
#ifdef DCM_USE_SERVICE_REQUEST_DOWNLOAD
Std_ReturnType Dcm_DspRequestDownload(Dcm_MsgContextType *msgContext,
                                      Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  P2CONST(Dcm_RequestDownloadConfigType, AUTOMATIC, DCM_CONST)
  rdConfig =
    (P2CONST(Dcm_RequestDownloadConfigType, AUTOMATIC, DCM_CONST))context->curService->config;
  uint8_t dataFormatIdentifier;
  uint8_t memorySizeLen;
  uint8_t memoryAddressLen;
  uint32_t memoryAddress;
  uint32_t memorySize;
  /* @SWS_Dcm_01420 */
  uint32_t BlockLength = config->rxBufferSize;
  uint16_t i;

  if (msgContext->reqDataLen > 3) {
    dataFormatIdentifier = msgContext->reqData[0];
    memorySizeLen = (msgContext->reqData[1] >> 4) & 0xFu;
    memoryAddressLen = msgContext->reqData[1] & 0xFu;
    if ((memorySizeLen >= 1u) && (memorySizeLen <= 4u) && (memoryAddressLen >= 1u) &&
        (memoryAddressLen <= 4u)) {
      if ((2 + memoryAddressLen + memorySizeLen) == msgContext->reqDataLen) {
        /* @SWS_Dcm_00856: support all possible case */
        r = E_OK;
        memoryAddress = 0u;
        for (i = 0u; i < memoryAddressLen; i++) {
          memoryAddress = (memoryAddress << 8) + msgContext->reqData[2u + i];
        }
        memorySize = 0u;
        for (i = 0u; i < memorySizeLen; i++) {
          memorySize = (memorySize << 8) + msgContext->reqData[2u + memoryAddressLen + i];
        }
      } else {
        *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
      }
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    if (DCM_UDT_IDLE_STATE != context->UDTData.state) {
      *nrc = DCM_E_REQUEST_SEQUENCE_ERROR;
    }
  }

  if (E_OK == r) {
    ASLOG(DCM, ("download memoryAddress=0x%X memorySize=0x%X\n", memoryAddress, memorySize));
    /* @SWS_Dcm_91070: MemoryIdentifier is not used, set it to 0x00 */
    r = rdConfig->ProcessRequestDownloadFnc(context->opStatus, dataFormatIdentifier, 0x00,
                                            memoryAddress, memorySize, &BlockLength, nrc);
  }

  if (E_OK == r) {
    msgContext->resData[0] = 0x20u; /* lengthFormatIdentifier */
    msgContext->resData[1] = (BlockLength >> 8) & 0xFFu;
    msgContext->resData[2] = BlockLength & 0xFFu;
    msgContext->resDataLen = 3u;
    context->UDTData.state = DCM_UDT_DOWNLOAD_STATE;
    context->UDTData.memoryAddress = memoryAddress;
    context->UDTData.memorySize = memorySize;
    context->UDTData.offset = 0u;
    context->UDTData.blockSequenceCounter = 1u;
  }

  return r;
}
#endif

#if defined(DCM_USE_SERVICE_REQUEST_UPLOAD)
Std_ReturnType Dcm_DspRequestUpload(Dcm_MsgContextType *msgContext,
                                    Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  P2CONST(Dcm_RequestUploadConfigType, AUTOMATIC, DCM_CONST)
  ruConfig =
    (P2CONST(Dcm_RequestUploadConfigType, AUTOMATIC, DCM_CONST))context->curService->config;
  uint8_t dataFormatIdentifier;
  uint8_t memorySizeLen;
  uint8_t memoryAddressLen;
  uint32_t memoryAddress;
  uint32_t memorySize;
  /* @SWS_Dcm_01422 */
  uint32_t BlockLength = msgContext->resMaxDataLen + 1;
  uint8_t i;

  if (msgContext->reqDataLen > 3) {
    dataFormatIdentifier = msgContext->reqData[0];
    memorySizeLen = (msgContext->reqData[1] >> 4) & 0xFu;
    memoryAddressLen = msgContext->reqData[1] & 0xFu;
    if ((memorySizeLen >= 1u) && (memorySizeLen <= 4u) && (memoryAddressLen >= 1u) &&
        (memoryAddressLen <= 4u)) {
      if ((2 + memoryAddressLen + memorySizeLen) == msgContext->reqDataLen) {
        /* @SWS_Dcm_00857: support all possible case */
        r = E_OK;
        memoryAddress = 0u;
        for (i = 0u; i < memoryAddressLen; i++) {
          memoryAddress = (memoryAddress << 8) + msgContext->reqData[2 + i];
        }
        memorySize = 0u;
        for (i = 0u; i < memorySizeLen; i++) {
          memorySize = (memorySize << 8) + msgContext->reqData[2 + memoryAddressLen + i];
        }
      } else {
        *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
      }
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    if (DCM_UDT_IDLE_STATE != context->UDTData.state) {
      *nrc = DCM_E_REQUEST_SEQUENCE_ERROR;
    }
  }

  if (E_OK == r) {
    ASLOG(DCM, ("upload memoryAddress=0x%X memorySize=0x%X\n", memoryAddress, memorySize));
    /* @SWS_Dcm_91070: MemoryIdentifier is not used, set it to 0x00 */
    r = ruConfig->ProcessRequestUploadFnc(context->opStatus, dataFormatIdentifier, 0x00,
                                          memoryAddress, memorySize, &BlockLength, nrc);
  }

  if (E_OK == r) {
    msgContext->resData[0] = 0x20u; /* lengthFormatIdentifier */
    msgContext->resData[1] = (BlockLength >> 8) & 0xFFu;
    msgContext->resData[2] = BlockLength & 0xFFu;
    msgContext->resDataLen = 3u;
    context->UDTData.state = DCM_UDT_UPLOAD_STATE;
    context->UDTData.memoryAddress = memoryAddress;
    context->UDTData.memorySize = memorySize;
    context->UDTData.offset = 0u;
    context->UDTData.blockSequenceCounter = 1u;
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_TRANSFER_DATA
Std_ReturnType Dcm_DspTransferData(Dcm_MsgContextType *msgContext,
                                   Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_TransferDataConfigType, AUTOMATIC, DCM_CONST)
  tfdConfig =
    (P2CONST(Dcm_TransferDataConfigType, AUTOMATIC, DCM_CONST))context->curService->config;
  uint32_t memoryAddress = context->UDTData.memoryAddress + context->UDTData.offset;
  uint32_t memorySize = context->UDTData.memorySize - context->UDTData.offset;
  Dcm_ReturnWriteMemoryType retW;
  Dcm_ReturnReadMemoryType retR;

  if (msgContext->reqDataLen >= 1) {
    if (context->UDTData.state != DCM_UDT_IDLE_STATE) {
      if (context->UDTData.blockSequenceCounter == msgContext->reqData[0]) {
        if (DCM_UDT_DOWNLOAD_STATE == context->UDTData.state) {
          if (tfdConfig->WriteFnc != NULL) {
            r = E_OK;
          } else {
            *nrc = DCM_E_CONDITIONS_NOT_CORRECT;
          }
        } else {
          if (tfdConfig->ReadFnc != NULL) {
            r = E_OK;
          } else {
            *nrc = DCM_E_CONDITIONS_NOT_CORRECT;
          }
        }
      } else {
        *nrc = DCM_E_WRONG_BLOCK_SEQUENCE_COUNTER;
      }
    } else {
      *nrc = DCM_E_REQUEST_SEQUENCE_ERROR;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    if (DCM_UDT_DOWNLOAD_STATE == context->UDTData.state) {
      if (memorySize < (msgContext->reqDataLen - 1)) {
        *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
        r = E_NOT_OK;
      } else {
        memorySize = msgContext->reqDataLen - 1;
      }
    } else {
      if (1 == msgContext->reqDataLen) {
        if (memorySize > (msgContext->resMaxDataLen - 1)) {
          memorySize = msgContext->resMaxDataLen - 1;
        }
      } else {
        *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
        r = E_NOT_OK;
      }
    }
  }

  if (E_OK == r) {
    if (DCM_UDT_DOWNLOAD_STATE == context->UDTData.state) {
      retW = tfdConfig->WriteFnc(context->opStatus, 0x00, memoryAddress, memorySize,
                                 &msgContext->reqData[1], nrc);
      if (DCM_WRITE_PENDING == retW) {
        *nrc = DCM_E_RESPONSE_PENDING;
      } else if (DCM_WRITE_FORCE_RCRRP == retW) {
        r = DCM_FORCE_RCRRP_OK;
      } else if (DCM_WRITE_OK == retW) {
        context->UDTData.offset += memorySize;
        msgContext->resData[0] = context->UDTData.blockSequenceCounter;
        msgContext->resDataLen = 1;
        context->UDTData.blockSequenceCounter++;
      } else { /* FAILED */
        r = E_NOT_OK;
        if (DCM_POS_RESP == *nrc) {
          *nrc = DCM_E_GENERAL_PROGRAMMING_FAILURE;
        }
      }
    } else {
      retR = tfdConfig->ReadFnc(context->opStatus, 0x00, memoryAddress, memorySize,
                                &msgContext->resData[1], nrc);
      if (DCM_READ_PENDING == retR) {
        *nrc = DCM_E_RESPONSE_PENDING;
      } else if (DCM_READ_FORCE_RCRRP == retR) {
        r = DCM_FORCE_RCRRP_OK;
      } else if (DCM_READ_OK == retR) {
        context->UDTData.offset += memorySize;
        msgContext->resData[0] = context->UDTData.blockSequenceCounter;
        msgContext->resDataLen = 1 + memorySize;
        context->UDTData.blockSequenceCounter++;
      } else { /* FAILED */
        r = E_NOT_OK;
        if (DCM_POS_RESP == *nrc) {
          *nrc = DCM_E_CONDITIONS_NOT_CORRECT;
        }
      }
    }
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_REQUEST_TRANSFER_EXIT
Std_ReturnType Dcm_DspRequestTransferExit(Dcm_MsgContextType *msgContext,
                                          Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_TransferExitConfigType, AUTOMATIC, DCM_CONST)
  tfeConfig =
    (P2CONST(Dcm_TransferExitConfigType, AUTOMATIC, DCM_CONST))context->curService->config;

  if (0 == msgContext->reqDataLen) {
    r = E_OK;
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    if (DCM_UDT_IDLE_STATE == context->UDTData.state) {
      r = E_NOT_OK;
      *nrc = DCM_E_REQUEST_SEQUENCE_ERROR;
    }
  }

  if (E_OK == r) {
    r = tfeConfig->TransferExitFnc(context->opStatus, nrc);
  }

  if (E_OK == r) {
    context->UDTData.state = DCM_UDT_IDLE_STATE;
    context->UDTData.blockSequenceCounter = 0;
    context->UDTData.memoryAddress = 0;
    context->UDTData.memorySize = 0;
    context->UDTData.offset = 0;
  }

  return r;
}
#endif
#ifdef DCM_USE_SERVICE_ECU_RESET
Std_ReturnType Dcm_DspEcuReset(Dcm_MsgContextType *msgContext, Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_EcuResetConfigType, AUTOMATIC, DCM_CONST)
  rstConfig = (P2CONST(Dcm_EcuResetConfigType, AUTOMATIC, DCM_CONST))context->curService->config;
  if (1u == msgContext->reqDataLen) {
    r = E_OK;
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (r == E_OK) {
    switch (msgContext->reqData[0]) {
    case 0x01u: /* hard reset */
    case 0x03u: /* soft reset */
      context->resetType = msgContext->reqData[0];
      if (rstConfig->delay == 0u) {
        context->timer2Reset = 1u;
      } else {
        context->timer2Reset = rstConfig->delay;
      }
      msgContext->resData[0] = context->resetType;
      msgContext->resDataLen = 1u;
      break;
    default:
      r = E_NOT_OK;
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
      break;
    }
  }

  if (E_OK == r) {
    r = rstConfig->GetEcuResetPermissionFnc(context->opStatus, nrc);
    if ((E_OK == r) && (DCM_E_RESPONSE_PENDING == *nrc)) {
      context->timer2Reset = 0;
    }
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_READ_DATA_BY_IDENTIFIER
Std_ReturnType Dcm_DspReadDataByIdentifier(Dcm_MsgContextType *msgContext,
                                           Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ReadDIDConfigType, AUTOMATIC, DCM_CONST)
  rDidConfig = (P2CONST(Dcm_ReadDIDConfigType, AUTOMATIC, DCM_CONST))context->curService->config;
  P2CONST(Dcm_ReadDIDType, AUTOMATIC, DCM_CONST) rDid = NULL;
  uint16_t id;
  uint16_t numOfDids = 0;
  Dcm_MsgLenType totalResLength = 0;
  boolean forceRCRRP = FALSE;
  uint16_t i;
  uint16_t j;

  if ((msgContext->reqDataLen >= 2u) && ((msgContext->reqDataLen & 0x01u) == 0u)) {
    numOfDids = msgContext->reqDataLen >> 1;
    for (i = 0u; (i < numOfDids) && (E_OK == r) && (DCM_INITIAL == context->opStatus); i++) {
      rDid = NULL;
      id = ((uint16_t)msgContext->reqData[i * 2u] << 8) + msgContext->reqData[(i * 2u) + 1u];
      for (j = 0u; j < rDidConfig->numOfDIDs; j++) {
        if (rDidConfig->DIDs[j].rDID->id == id) {
          rDid = &rDidConfig->DIDs[j];
          break;
        }
      }
      if (NULL != rDid) {
        totalResLength += rDid->rDID->length + 2u;
        r = Dcm_DslServiceDIDSesSecPhyFuncCheck(context, &rDid->rDID->SesSecAccess, nrc);
      } else {
        *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        r = E_NOT_OK;
      }
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    r = E_NOT_OK;
  }

  if (E_OK == r) {
    if (totalResLength > msgContext->resMaxDataLen) {
      *nrc = DCM_E_RESPONSE_TOO_LONG;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    totalResLength = 0u;
    for (i = 0u; (i < numOfDids) && (E_OK == r); i++) {
      rDid = NULL;
      id = ((uint16_t)msgContext->reqData[i * 2u] << 8) + msgContext->reqData[(i * 2u) + 1u];
      for (j = 0u; j < rDidConfig->numOfDIDs; j++) {
        if (rDidConfig->DIDs[j].rDID->id == id) {
          rDid = &rDidConfig->DIDs[j];
          break;
        }
      }
      if (NULL != rDid) {
        msgContext->resData[totalResLength] = (id >> 8) & 0xFFu;
        msgContext->resData[totalResLength + 1u] = id & 0xFFu;
        if ((DCM_INITIAL == context->opStatus) || (DCM_CANCEL == context->opStatus) ||
            (DCM_PENDING == rDid->context->opStatus)) {
          rDid->context->opStatus = DCM_CANCEL; /* set to invalid */
          r = rDid->rDID->readDIdFnc(context->opStatus, &msgContext->resData[totalResLength + 2u],
                                     rDid->rDID->length, nrc);
          if (DCM_E_PENDING == r) {
            r = E_OK;
            *nrc = DCM_E_RESPONSE_PENDING;
          } else if (DCM_E_FORCE_RCRRP == r) {
            r = E_OK;
            *nrc = DCM_E_RESPONSE_PENDING;
            forceRCRRP = TRUE;
          } else {
            /* do nothing */
          }
          if (E_OK == r) {
            if (DCM_E_RESPONSE_PENDING == *nrc) {
              /* only in this case, schedule the call of readDIdFnc again in the next cycle */
              rDid->context->opStatus = DCM_PENDING;
            }
          }
        }
        totalResLength += rDid->rDID->length + 2u;
      } else {
        *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        r = E_NOT_OK;
      }
    }
    msgContext->resDataLen = totalResLength;
  }

  if (E_OK == r) {
    if ((DCM_E_RESPONSE_PENDING == *nrc) && (TRUE == forceRCRRP)) {
      r = DCM_E_FORCE_RCRRP;
    }
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_READ_SCALING_DATA_BY_IDENTIFIER
Std_ReturnType Dcm_DspReadScalingDataByIdentifier(Dcm_MsgContextType *msgContext,
                                                  Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ReadScalingDIDConfigType, AUTOMATIC, DCM_CONST)
  rDidConfig =
    (P2CONST(Dcm_ReadScalingDIDConfigType, AUTOMATIC, DCM_CONST))context->curService->config;
  P2CONST(Dcm_rDIDConfigType, AUTOMATIC, DCM_CONST) rDid = NULL;
  uint16_t id;
  uint16_t i;

  if (2u == msgContext->reqDataLen) {
    id = ((uint16_t)msgContext->reqData[0] << 8) + msgContext->reqData[1];
    for (i = 0; i < rDidConfig->numOfDIDs; i++) {
      if (rDidConfig->DIDs[i].id == id) {
        rDid = &rDidConfig->DIDs[i];
        break;
      }
    }
    if (NULL != rDid) {
      r = Dcm_DslServiceDIDSesSecPhyFuncCheck(context, &rDid->SesSecAccess, nrc);
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
      r = E_NOT_OK;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    r = E_NOT_OK;
  }

  if (E_OK == r) {
    msgContext->resData[0] = (rDid->id >> 8) & 0xFF;
    msgContext->resData[1] = rDid->id & 0xFF;
    r = rDid->readDIdFnc(context->opStatus, &msgContext->resData[2], rDid->length, nrc);
    msgContext->resDataLen = 2 + rDid->length;
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_WRITE_DATA_BY_IDENTIFIER
Std_ReturnType Dcm_DspWriteDataByIdentifier(Dcm_MsgContextType *msgContext,
                                            Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_WriteDIDConfigType, AUTOMATIC, DCM_CONST)
  wDidConfig = (P2CONST(Dcm_WriteDIDConfigType, AUTOMATIC, DCM_CONST))context->curService->config;
  P2CONST(Dcm_WriteDIDType, AUTOMATIC, DCM_CONST) wDid = NULL;
  uint16_t id;
  uint16_t i;

  if (msgContext->reqDataLen > 2) {
    wDid = NULL;
    id = ((uint16_t)msgContext->reqData[0] << 8) + msgContext->reqData[1];
    for (i = 0; i < wDidConfig->numOfDIDs; i++) {
      if (wDidConfig->DIDs[i].id == id) {
        wDid = &wDidConfig->DIDs[i];
        break;
      }
    }

    if (NULL != wDid) {
      r = Dcm_DslServiceDIDSesSecPhyFuncCheck(context, &wDid->SesSecAccess, nrc);
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
      r = E_NOT_OK;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    r = E_NOT_OK;
  }

  if (E_OK == r) {
    if ((wDid->length + 2) != msgContext->reqDataLen) {
      *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    msgContext->resData[0] = (wDid->id >> 8) & 0xFF;
    msgContext->resData[1] = wDid->id & 0xFF;
    r = wDid->writeDIdFnc(context->opStatus, &msgContext->reqData[2], wDid->length, nrc);
    msgContext->resDataLen = 2;
  }

  return r;
}
#endif
#ifdef DCM_USE_SERVICE_TESTER_PRESENT
Std_ReturnType Dcm_DspTesterPresent(Dcm_MsgContextType *msgContext,
                                    Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;

  if (1u == msgContext->reqDataLen) {
    if (0x00u == msgContext->reqData[0]) {
      msgContext->resData[0] = 0x00u;
      msgContext->resDataLen = 1u;
      r = E_OK;
    } else {
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_CONTROL_DTC_SETTING
Std_ReturnType Dcm_DspControlDTCSetting(Dcm_MsgContextType *msgContext,
                                        Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;

  if (1 == msgContext->reqDataLen) {
    switch (msgContext->reqData[0]) {
    case 0x01u: /* ON */
      r = Dem_EnableDTCSetting(0u);
      if (E_OK != r) {
        *nrc = DCM_E_CONDITIONS_NOT_CORRECT;
      }
      break;
    case 0x02u: /* OFF */
      r = Dem_DisableDTCSetting(0u);
      if (E_OK != r) {
        *nrc = DCM_E_CONDITIONS_NOT_CORRECT;
      }
      break;
    default:
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
      break;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    msgContext->resData[0] = msgContext->reqData[0];
    msgContext->resDataLen = 1u;
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_CLEAR_DIAGNOSTIC_INFORMATION
Std_ReturnType Dcm_DspClearDTC(Dcm_MsgContextType *msgContext, Dcm_NegativeResponseCodeType *nrc) {

  Std_ReturnType r = E_NOT_OK;
  uint32_t groupDTC = 0x0;
  Dcm_ContextType *context = Dcm_GetContext();

  if (3 == msgContext->reqDataLen) {
    groupDTC = ((uint32_t)msgContext->reqData[0] << 16) + ((uint32_t)msgContext->reqData[1] << 8) +
               msgContext->reqData[2];
    r = E_OK;
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    if (DCM_INITIAL == context->opStatus) {
      r = Dem_SelectDTC(0, groupDTC, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY);
      if (E_OK == r) {
        r = Dem_ClearDTC(0);
      }
      if (E_OK != r) {
        *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
      } else {
#ifdef USE_NVM
        *nrc = DCM_E_RESPONSE_PENDING;
#endif
      }
    } else {
#ifdef USE_NVM
      if (MEMIF_IDLE != NvM_GetStatus()) {
        *nrc = DCM_E_RESPONSE_PENDING;
      }
#endif
    }
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_READ_DTC_INFORMATION
Std_ReturnType Dem_DspReportNumberOfDTCByStatusMask_Impl(Dcm_MsgContextType *msgContext,
                                                         Dcm_NegativeResponseCodeType *nrc,
                                                         Dem_DTCOriginType DTCOrigin) {
  Std_ReturnType r = E_NOT_OK;
  uint8_t statusMask;
  uint16_t NumberOfFilteredDTC = 0;

  if (2 == msgContext->reqDataLen) {
    statusMask = msgContext->reqData[1];

    r = Dem_SetDTCFilter(0, statusMask, DEM_DTC_FORMAT_UDS, DTCOrigin, FALSE, 0, FALSE);
    if (E_OK == r) {
      r = Dem_GetNumberOfFilteredDTC(0, &NumberOfFilteredDTC);
      if (E_OK == r) {
        r = Dem_GetDTCStatusAvailabilityMask(0, &statusMask, DTCOrigin);
      }
    }

    if (E_OK == r) {
      msgContext->resData[0] = msgContext->reqData[0];
      msgContext->resData[1] = statusMask;
      msgContext->resData[2] = DEM_DTC_FORMAT_UDS; /* DTCFormatIdentifier */
      msgContext->resData[3] = (uint8_t)((NumberOfFilteredDTC >> 8) & 0xFF);
      msgContext->resData[4] = (uint8_t)(NumberOfFilteredDTC & 0xFF);
      msgContext->resDataLen = 5;
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  return r;
}

Std_ReturnType Dem_DspReportNumberOfDTCByStatusMask(Dcm_MsgContextType *msgContext,
                                                    Dcm_NegativeResponseCodeType *nrc) {
  return Dem_DspReportNumberOfDTCByStatusMask_Impl(msgContext, nrc, DEM_DTC_ORIGIN_PRIMARY_MEMORY);
}

Std_ReturnType Dem_DspReportNumberOfMirrorMemoryDTCByStatusMask(Dcm_MsgContextType *msgContext,
                                                                Dcm_NegativeResponseCodeType *nrc) {
  return Dem_DspReportNumberOfDTCByStatusMask_Impl(msgContext, nrc, DEM_DTC_ORIGIN_MIRROR_MEMORY);
}

Std_ReturnType Dem_DspReportDTCByStatusMask_Impl(Dcm_MsgContextType *msgContext,
                                                 Dcm_NegativeResponseCodeType *nrc,
                                                 Dem_DTCOriginType DTCOrigin) {
  Std_ReturnType r = E_NOT_OK;
  uint8_t statusMask;
  uint16_t NumberOfFilteredDTC = 0;
  uint32_t DTCNumber;
  Dem_UdsStatusByteType udsStatus;
  uint16_t i;

  if (2 == msgContext->reqDataLen) {
    statusMask = msgContext->reqData[1];

    r = Dem_SetDTCFilter(0, statusMask, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, 0,
                         FALSE);
    if (E_OK == r) {
      r = Dem_GetNumberOfFilteredDTC(0, &NumberOfFilteredDTC);
      if (E_OK == r) {
        r = Dem_GetDTCStatusAvailabilityMask(0, &statusMask, DTCOrigin);
      }
    }

    if (E_OK == r) {
      if ((NumberOfFilteredDTC * 4 + 2) <= msgContext->resMaxDataLen) {
        msgContext->resData[0] = msgContext->reqData[0];
        msgContext->resData[1] = statusMask;
        for (i = 0; (i < NumberOfFilteredDTC) && (E_OK == r); i++) {
          r = Dem_GetNextFilteredDTC(0, &DTCNumber, &udsStatus);
          if (E_OK == r) {
            msgContext->resData[2 + 4 * i] = (uint8_t)((DTCNumber >> 16) & 0xFF);
            msgContext->resData[3 + 4 * i] = (uint8_t)((DTCNumber >> 8) & 0xFF);
            msgContext->resData[4 + 4 * i] = (uint8_t)(DTCNumber & 0xFF);
            msgContext->resData[5 + 4 * i] = udsStatus;
          }
        }
        if (E_OK == r) {
          msgContext->resDataLen = NumberOfFilteredDTC * 4 + 2;
        } else {
          *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        }
      } else {
        *nrc = DCM_E_RESPONSE_TOO_LONG;
        r = E_NOT_OK;
      }
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  return r;
}

Std_ReturnType Dem_DspReportDTCByStatusMask(Dcm_MsgContextType *msgContext,
                                            Dcm_NegativeResponseCodeType *nrc) {
  return Dem_DspReportDTCByStatusMask_Impl(msgContext, nrc, DEM_DTC_ORIGIN_PRIMARY_MEMORY);
}

Std_ReturnType Dem_DspReportMirrorMemoryDTCByStatusMask(Dcm_MsgContextType *msgContext,
                                                        Dcm_NegativeResponseCodeType *nrc) {
  return Dem_DspReportDTCByStatusMask_Impl(msgContext, nrc, DEM_DTC_ORIGIN_MIRROR_MEMORY);
}

Std_ReturnType Dem_DspReportDTCSnapshotIdentification(Dcm_MsgContextType *msgContext,
                                                      Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  uint16_t NumberOfFilteredRecords = 0;
  uint32_t DTCNumber;
  uint8_t RecordNumber;
  uint16_t i;

  if (1 == msgContext->reqDataLen) {
    r = Dem_SelectDTC(0, 0xFFFFFF, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY);
    if (E_OK == r) {
      r = Dem_SetFreezeFrameRecordFilter(0, DEM_DTC_FORMAT_UDS);
    }
    if (E_OK == r) {
      r = Dem_GetNumberOfFreezeFrameRecords(0, &NumberOfFilteredRecords);
    }

    if (E_OK == r) {
      if ((NumberOfFilteredRecords * 4 + 1) <= msgContext->resMaxDataLen) {
        msgContext->resData[0] = 0x03;
        for (i = 0; (i < NumberOfFilteredRecords) && (E_OK == r); i++) {
          r = Dem_GetNextFilteredRecord(0, &DTCNumber, &RecordNumber);
          if (E_OK == r) {
            msgContext->resData[1 + 4 * i] = (uint8_t)((DTCNumber >> 16) & 0xFF);
            msgContext->resData[2 + 4 * i] = (uint8_t)((DTCNumber >> 8) & 0xFF);
            msgContext->resData[3 + 4 * i] = (uint8_t)(DTCNumber & 0xFF);
            msgContext->resData[4 + 4 * i] = RecordNumber;
          }
        }
        if (E_OK == r) {
          msgContext->resDataLen = NumberOfFilteredRecords * 4 + 1;
        } else {
          *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        }
      } else {
        *nrc = DCM_E_RESPONSE_TOO_LONG;
        r = E_NOT_OK;
      }
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  return r;
}

Std_ReturnType Dem_DspReportDTCSnapshotRecordByDTCNumber(Dcm_MsgContextType *msgContext,
                                                         Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  uint32_t DTCNumber;
  uint8_t RecordNumber;
  uint16_t resLen = msgContext->resMaxDataLen - 1;

  if (5 == msgContext->reqDataLen) {
    DTCNumber = ((uint32_t)msgContext->reqData[1] << 16) + ((uint32_t)msgContext->reqData[2] << 8) +
                msgContext->reqData[3];
    RecordNumber = msgContext->reqData[4];
    r = Dem_SelectDTC(0, DTCNumber, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY);
    if (E_OK == r) {
      r = Dem_SelectFreezeFrameData(0, RecordNumber);
    }

    if (E_OK == r) {
      r = Dem_GetNextFreezeFrameData(0, &msgContext->resData[1], &resLen);
    }

    if (E_OK == r) {
      msgContext->resData[0] = 0x04;
      msgContext->resDataLen = resLen + 1;
    } else if (DEM_BUFFER_TOO_SMALL == r) {
      *nrc = DCM_E_RESPONSE_TOO_LONG;
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  return r;
}

Std_ReturnType Dem_DspReportDTCExtendedDataRecordByDTCNumber_Impl(Dcm_MsgContextType *msgContext,
                                                                  Dcm_NegativeResponseCodeType *nrc,
                                                                  Dem_DTCOriginType DTCOrigin) {
  Std_ReturnType r = E_NOT_OK;
  uint32_t DTCNumber;
  uint8_t RecordNumber;
  uint16_t resLen = msgContext->resMaxDataLen - 1;

  if (5 == msgContext->reqDataLen) {
    DTCNumber = ((uint32_t)msgContext->reqData[1] << 16) + ((uint32_t)msgContext->reqData[2] << 8) +
                msgContext->reqData[3];
    RecordNumber = msgContext->reqData[4];
    r = Dem_SelectDTC(0, DTCNumber, DEM_DTC_FORMAT_UDS, DTCOrigin);
    if (E_OK == r) {
      r = Dem_SelectFreezeFrameData(0, RecordNumber);
    }

    if (E_OK == r) {
      r = Dem_GetNextExtendedDataRecord(0, &msgContext->resData[1], &resLen);
    }

    if (E_OK == r) {
      msgContext->resData[0] = 0x06;
      msgContext->resDataLen = resLen + 1;
    } else if (DEM_BUFFER_TOO_SMALL == r) {
      *nrc = DCM_E_RESPONSE_TOO_LONG;
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  return r;
}

Std_ReturnType Dem_DspReportDTCExtendedDataRecordByDTCNumber(Dcm_MsgContextType *msgContext,
                                                             Dcm_NegativeResponseCodeType *nrc) {
  return Dem_DspReportDTCExtendedDataRecordByDTCNumber_Impl(msgContext, nrc,
                                                            DEM_DTC_ORIGIN_PRIMARY_MEMORY);
}

Std_ReturnType
Dem_DspReportMirrorMemoryDTCExtendedDataRecordByDTCNumber(Dcm_MsgContextType *msgContext,
                                                          Dcm_NegativeResponseCodeType *nrc) {
  return Dem_DspReportDTCExtendedDataRecordByDTCNumber_Impl(msgContext, nrc,
                                                            DEM_DTC_ORIGIN_MIRROR_MEMORY);
}

Std_ReturnType Dem_DspReportSupportedDTC(Dcm_MsgContextType *msgContext,
                                         Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  uint16_t NumberOfFilteredDTC = 0;
  uint32_t DTCNumber;
  Dem_UdsStatusByteType udsStatus;
  uint16_t i;

  if (1 == msgContext->reqDataLen) {
    r = Dem_SetDTCFilter(0, 0, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, 0, FALSE);
    if (E_OK == r) {
      r = Dem_GetNumberOfFilteredDTC(0, &NumberOfFilteredDTC);
      if (E_OK == r) {
        r = Dem_GetDTCStatusAvailabilityMask(0, &udsStatus, DEM_DTC_ORIGIN_PRIMARY_MEMORY);
      }
    }

    if (E_OK == r) {
      if ((NumberOfFilteredDTC * 4 + 2) <= msgContext->resMaxDataLen) {
        msgContext->resData[0] = msgContext->reqData[0];
        msgContext->resData[1] = udsStatus;
        for (i = 0; (i < NumberOfFilteredDTC) && (E_OK == r); i++) {
          r = Dem_GetNextFilteredDTC(0, &DTCNumber, &udsStatus);
          if (E_OK == r) {
            msgContext->resData[2 + 4 * i] = (uint8_t)((DTCNumber >> 16) & 0xFF);
            msgContext->resData[3 + 4 * i] = (uint8_t)((DTCNumber >> 8) & 0xFF);
            msgContext->resData[4 + 4 * i] = (uint8_t)(DTCNumber & 0xFF);
            msgContext->resData[5 + 4 * i] = udsStatus;
          }
        }
        if (E_OK == r) {
          msgContext->resDataLen = NumberOfFilteredDTC * 4 + 2;
        } else {
          *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        }
      } else {
        *nrc = DCM_E_RESPONSE_TOO_LONG;
        r = E_NOT_OK;
      }
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  return r;
}

Std_ReturnType Dcm_DspReadDTCInformation(Dcm_MsgContextType *msgContext,
                                         Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ReadDTCInfoConfigType, AUTOMATIC, DCM_CONST)
  config = (P2CONST(Dcm_ReadDTCInfoConfigType, AUTOMATIC, DCM_CONST))context->curService->config;
  P2CONST(Dcm_ReadDTCSubFunctionConfigType, AUTOMATIC, DCM_CONST) subFunction = NULL;
  uint8_t type;
  uint16_t i;

  if (msgContext->reqDataLen >= 1) {
    type = msgContext->reqData[0];
    for (i = 0; i < config->numOfSubFunctions; i++) {
      if (config->subFunctions[i].type == type) {
        subFunction = &config->subFunctions[i];
        r = E_OK;
        break;
      }
    }
    if (E_OK != r) {
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
    } else {
      r = Dcm_DslServiceSesSecPhyFuncCheck(context, &subFunction->SesSecAccess, nrc);
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    r = subFunction->subFnc(msgContext, nrc);
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER
static void Dcm_IOCtlByID_Init(void) {
  Dcm_NegativeResponseCodeType nrc = 0;
  uint16_t resDataLen = 0;
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  P2CONST(Dcm_IOControlConfigType, AUTOMATIC, DCM_CONST)
  IOCtlConfig = config->IOCtlConfig;
  P2CONST(Dcm_IOControlType, AUTOMATIC, DCM_CONST) IOCtrl;
  uint16_t i;
  for (i = 0; i < IOCtlConfig->numOfIOCtrls; i++) {
    IOCtrl = &IOCtlConfig->IOCtrls[i];
    if (IOCtrl->context->requestMask != 0u) {
      if (IOCtrl->ReturnControlToEcuFnc != NULL) {
        ASLOG(DCM, ("IOCTL %X return to ECU\n", IOCtrl->id));
        (void)IOCtrl->ReturnControlToEcuFnc(NULL, 0, config->txBuffer, &resDataLen, &nrc);
      }
    }
    IOCtrl->context->requestMask = 0;
  }
}

Std_ReturnType Dcm_DspIOControlByIdentifier(Dcm_MsgContextType *msgContext,
                                            Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_IOControlConfigType, AUTOMATIC, DCM_CONST)
  config = (P2CONST(Dcm_IOControlConfigType, AUTOMATIC, DCM_CONST))context->curService->config;
  P2CONST(Dcm_IOControlType, AUTOMATIC, DCM_CONST) IOCtrl = NULL;
  uint16_t id;
  uint8_t action;
  uint16_t resDataLen = msgContext->resMaxDataLen - 3u;
  uint16_t i;
  P2CONST(Dcm_IOCtrlExecuteFncType, AUTOMATIC, DCM_CONST) ExecuteFncs;

  if (msgContext->reqDataLen >= 3u) {
    id = ((uint16_t)msgContext->reqData[0] << 8) + msgContext->reqData[1];
    action = msgContext->reqData[2];
    for (i = 0; i < config->numOfIOCtrls; i++) {
      if (config->IOCtrls[i].id == id) {
        IOCtrl = &config->IOCtrls[i];
        r = E_OK;
        break;
      }
    }

    if (E_OK != r) {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    } else {
      r = Dcm_DslServiceDIDSesSecPhyFuncCheck(context, &IOCtrl->SesSecAccess, nrc);
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    ExecuteFncs = &(IOCtrl->ReturnControlToEcuFnc);
    if (action <= DCM_IOCTRL_SHORT_TERM_ADJUSTMENT) {
      if (ExecuteFncs[action] != NULL) {
        r = ExecuteFncs[action](&msgContext->reqData[3], msgContext->reqDataLen - 3u,
                                &msgContext->resData[3], &resDataLen, nrc);
      } else {
        *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        r = E_NOT_OK;
      }
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    if (action != DCM_IOCTRL_RETURN_CTRL_TO_ECU) {
      IOCtrl->context->requestMask |= ((uint8_t)1 << action);
    } else {
      IOCtrl->context->requestMask = 0u;
    }
    msgContext->resData[0] = (IOCtrl->id >> 8) & 0xFFu;
    msgContext->resData[1] = IOCtrl->id & 0xFFu;
    msgContext->resData[2] = action;
    msgContext->resDataLen = 3u + resDataLen;
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_COMMUNICATION_CONTROL
Std_ReturnType Dcm_DspCommunicationControl(Dcm_MsgContextType *msgContext,
                                           Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_CommunicationControlConfigType, AUTOMATIC, DCM_CONST)
  config =
    (P2CONST(Dcm_CommunicationControlConfigType, AUTOMATIC, DCM_CONST))context->curService->config;
  P2CONST(Dcm_ComCtrlType, AUTOMATIC, DCM_CONST)
  ComCtrl = NULL;
  uint8_t id;
  uint8_t comType;
  uint16_t i;

  if (2 == msgContext->reqDataLen) {
    id = msgContext->reqData[0];
    comType = msgContext->reqData[1];
    for (i = 0; i < config->numOfComCtrls; i++) {
      if (config->ComCtrls[i].id == id) {
        ComCtrl = &config->ComCtrls[i];
        r = E_OK;
        break;
      }
    }

    if (E_OK != r) {
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    if (ComCtrl->comCtrlFnc != NULL) {
      r = ComCtrl->comCtrlFnc(comType, nrc);
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    msgContext->resData[0] = id;
    msgContext->resDataLen = 1;
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER
void Dcm_ReadPeriodicDID_Init(void) {
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  P2CONST(Dcm_ReadPeriodicDIDConfigType, AUTOMATIC, DCM_CONST) pDIDConfig = config->rPDIDConfig;
  P2CONST(Dcm_ReadPeriodicDIDType, AUTOMATIC, DCM_CONST) rDid = NULL;
  uint16_t i;
  if (NULL != pDIDConfig) {
    for (i = 0; i < pDIDConfig->numOfDIDs; i++) {
      rDid = &pDIDConfig->DIDs[i];
      (void)memset(rDid->context, 0, sizeof(Dcm_ReadPeriodicDIDContextType));
      rDid->context->opStatus = DCM_CANCEL;
    }
  }
}

void Dcm_ReadPeriodicDID_OnSessionSecurityChange(void) {
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  P2CONST(Dcm_ReadPeriodicDIDConfigType, AUTOMATIC, DCM_CONST) pDIDConfig = config->rPDIDConfig;
  P2CONST(Dcm_ReadPeriodicDIDType, AUTOMATIC, DCM_CONST) rDid = NULL;
  Dcm_ContextType *context = Dcm_GetContext();
  Dcm_NegativeResponseCodeType nrc;
  boolean stopIt;
  uint16_t i;
  Std_ReturnType r;

  if (NULL != pDIDConfig) {
    for (i = 0; i < pDIDConfig->numOfDIDs; i++) {
      rDid = &pDIDConfig->DIDs[i];
      stopIt = FALSE;
      if (DCM_DEFAULT_SESSION == context->currentSession) { /* @SWS_Dcm_01107*/
        stopIt = TRUE;
      } else {
        /*  @SWS_Dcm_01108, @SWS_Dcm_01109 */
        nrc = DCM_POS_RESP;
        r = Dcm_DslServiceDIDSesSecPhyFuncCheck(context, &rDid->DID->SesSecAccess, &nrc);
        if ((E_OK != r) && (DCM_E_SERVICE_NOT_SUPPORTED != nrc)) {
          stopIt = TRUE;
        }
      }
      if (TRUE == stopIt) {
        (void)memset(rDid->context, 0, sizeof(Dcm_ReadPeriodicDIDContextType));
        rDid->context->opStatus = DCM_CANCEL;
      }
    }
  }
}

void Dcm_MainFunction_ReadPeriodicDID(void) {
  Std_ReturnType r = E_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  P2CONST(Dcm_ReadPeriodicDIDConfigType, AUTOMATIC, DCM_CONST) pDIDConfig = config->rPDIDConfig;
  P2CONST(Dcm_ReadPeriodicDIDType, AUTOMATIC, DCM_CONST) rDid = NULL;
  uint16_t i;
  Dcm_MsgType resData = config->txBuffer;
  Dcm_MsgLenType totalResLength;
  Dcm_NegativeResponseCodeType nrc;

  if (NULL != pDIDConfig) {
    for (i = 0; i < pDIDConfig->numOfDIDs; i++) {
      rDid = &pDIDConfig->DIDs[i];
      if (rDid->context->reload > 0u) {
        if (rDid->context->timer > 0u) {
          rDid->context->timer--;
          if (0u == rDid->context->timer) {
            rDid->context->opStatus = DCM_INITIAL;
            rDid->context->timer = rDid->context->reload;
          }
        } else {
          ASLOG(DCME, ("read periodic timer stopt\n"));
          rDid->context->timer = rDid->context->reload;
        }
      }
    }
  }

  if ((NULL != pDIDConfig) && (DCM_BUFFER_IDLE == context->txBufferState)) {
    resData[0] = 0x6Au;
    totalResLength = 1u;
    for (i = 0u; i < pDIDConfig->numOfDIDs; i++) {
      rDid = &pDIDConfig->DIDs[i];
      if (rDid->context->opStatus != DCM_CANCEL) {
        if ((totalResLength + 1u + rDid->DID->length) > config->txBufferSize) {
          break;
        }
        nrc = DCM_POS_RESP;
        r = rDid->DID->readDIdFnc(rDid->context->opStatus, &resData[totalResLength + 1u],
                                  rDid->DID->length, &nrc);
        if ((DCM_E_PENDING == r) || (DCM_E_FORCE_RCRRP == r)) {
          r = E_OK;
          nrc = DCM_E_RESPONSE_PENDING;
        }
        if (r != E_OK) {
          ASLOG(DCME, ("read periodic DID FAILED\n"));
          rDid->context->opStatus = DCM_CANCEL;
        } else if (DCM_POS_RESP == nrc) { /* reading is done */
          resData[totalResLength] = rDid->DID->id & 0xFFu;
          totalResLength += 1u + rDid->DID->length;
          rDid->context->opStatus = DCM_CANCEL;
        } else if (DCM_E_RESPONSE_PENDING == nrc) {
          rDid->context->opStatus = DCM_PENDING;
        } else {
          ASLOG(DCME, ("read periodic DID meet invalid NRC\n"));
          rDid->context->opStatus = DCM_CANCEL;
        }
      }
    }
    if (totalResLength > 1u) {
      context->TxTpSduLength = totalResLength;
      context->txBufferState = DCM_BUFFER_FULL;
    }
  }
}

Std_ReturnType Dcm_DspReadDataByPeriodicIdentifier(Dcm_MsgContextType *msgContext,
                                                   Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ReadPeriodicDIDConfigType, AUTOMATIC, DCM_CONST)
  config =
    (P2CONST(Dcm_ReadPeriodicDIDConfigType, AUTOMATIC, DCM_CONST))context->curService->config;
  P2CONST(Dcm_ReadPeriodicDIDType, AUTOMATIC, DCM_CONST) rDid = NULL;
  uint8_t id;
  uint8_t transmissionMode;
  uint16_t numOfDids = 0;
  Dcm_MsgLenType totalResLength = 0;
  uint16_t reload = 0;
  uint16_t i;
  uint16_t j;

  if (msgContext->reqDataLen >= 2u) {
    transmissionMode = msgContext->reqData[0];
    numOfDids = msgContext->reqDataLen - 1u;
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    r = E_NOT_OK;
  }

  if (E_OK == r) {
    switch (transmissionMode) {
    case DCM_TM_SEND_AT_SLOW_RATE:
      reload = (uint16_t)DCM_CONVERT_MS_TO_MAIN_CYCLES(DCM_TM_SLOW_TIME_MS);
      break;
    case DCM_TM_SEND_AT_MEDIUM_RATE:
      reload = (uint16_t)DCM_CONVERT_MS_TO_MAIN_CYCLES(DCM_TM_MEDIUM_TIME_MS);
      break;
    case DCM_TM_SEND_AT_FAST_RATE:
      reload = (uint16_t)DCM_CONVERT_MS_TO_MAIN_CYCLES(DCM_TM_FAST_TIME_MS);
      break;
    case DCM_TM_STOP_SENDING:
      reload = 0;
      break;
    default:
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
      r = E_NOT_OK;
      break;
    }
  }

  if (E_OK == r) {
    for (i = 0; (i < numOfDids) && (E_OK == r); i++) {
      rDid = NULL;
      id = msgContext->reqData[i + 1u];
      for (j = 0; j < config->numOfDIDs; j++) {
        if (config->DIDs[j].DID->id == ((uint16_t)id + 0xF200u)) {
          rDid = &config->DIDs[j];
          break;
        }
      }
      if (NULL != rDid) {
        totalResLength += rDid->DID->length + 1u;
        r = Dcm_DslServiceDIDSesSecPhyFuncCheck(context, &rDid->DID->SesSecAccess, nrc);
      } else {
        *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        r = E_NOT_OK;
      }
    }
  }

  if (E_OK == r) {
    if (totalResLength > msgContext->resMaxDataLen) {
      *nrc = DCM_E_RESPONSE_TOO_LONG;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    for (i = 0u; i < numOfDids; i++) {
      rDid = NULL;
      id = msgContext->reqData[i + 1u];
      for (j = 0u; j < config->numOfDIDs; j++) {
        if (config->DIDs[j].DID->id == ((uint16_t)id + 0xF200u)) {
          rDid = &config->DIDs[j];
          if (0u == rDid->context->reload) {
            rDid->context->opStatus = DCM_CANCEL;
          }
          rDid->context->reload = reload;
          if (DCM_CANCEL == rDid->context->opStatus) {
            rDid->context->timer = 1;
          }
        }
      }
    }
    /* don't give response here for this service */
    context->msgContext.msgAddInfo.suppressPosResponse = DCM_SUPRESS_POSITIVE_RESPONCE;
  }
  return r;
}
#endif

#ifdef DCM_USE_SERVICE_READ_MEMORY_BY_ADDRESS
static Std_ReturnType Dcm_DspIsMemoryValid(uint8_t addressAndLengthFormat, uint8_t attr,
                                           uint32_t memoryAddress, uint32_t memorySize,
                                           Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  P2CONST(Dcm_DspMemoryConfigType, AUTOMATIC, DCM_CONST)
  memoryConfig = config->MemoryConfig;
  uint16_t i;

  if (memoryConfig->AddressAndLengthFormatIdentifiers != NULL) {
    r = E_NOT_OK;
    for (i = 0; (E_NOT_OK == r) && (i < memoryConfig->numOfAALFIs); i++) {
      if (memoryConfig->AddressAndLengthFormatIdentifiers[i] == addressAndLengthFormat) {
        r = E_OK;
      }
    }
    if (E_NOT_OK == r) {
      *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
  }

  if (E_OK == r) {
    r = E_NOT_OK;
    for (i = 0u; (E_NOT_OK == r) && (i < memoryConfig->numOfMems); i++) {
      if ((0u != (attr & memoryConfig->Mems[i].attr)) &&
          (memoryAddress >= memoryConfig->Mems[i].low) &&
          ((memoryAddress + memorySize) <= memoryConfig->Mems[i].high)) {
        r = Dcm_DslServiceSesSecPhyFuncCheck(context, &memoryConfig->Mems[i].SesSecAccess, nrc);
      }
    }
    if (E_NOT_OK == r) {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  }

  return r;
}

Std_ReturnType Dcm_DspReadMemoryByAddress(Dcm_MsgContextType *msgContext,
                                          Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ReturnReadMemoryType rm;
  Dcm_ContextType *context = Dcm_GetContext();

  uint8_t memorySizeLen;
  uint8_t memoryAddressLen;
  uint32_t memoryAddress;
  uint32_t memorySize;
  uint16_t i;

  if (msgContext->reqDataLen > 3u) {
    memorySizeLen = (msgContext->reqData[0] >> 4) & 0xFu;
    memoryAddressLen = msgContext->reqData[0] & 0xFu;
    if ((memorySizeLen >= 1u) && (memorySizeLen <= 4u) && (memoryAddressLen >= 1u) &&
        (memoryAddressLen <= 4u)) {
      if ((1u + memoryAddressLen + memorySizeLen) == msgContext->reqDataLen) {
        memoryAddress = 0;
        for (i = 0; i < memoryAddressLen; i++) {
          memoryAddress = (memoryAddress << 8) + msgContext->reqData[1u + i];
        }
        memorySize = 0;
        for (i = 0; i < memorySizeLen; i++) {
          memorySize = (memorySize << 8) + msgContext->reqData[1u + memoryAddressLen + i];
        }
        r = E_OK;
      } else {
        *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
      }
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    r = Dcm_DspIsMemoryValid(msgContext->reqData[0], DCM_MEM_ATTR_READ, memoryAddress, memorySize,
                             nrc);
  }

  if (E_OK == r) {
    if (memorySize > msgContext->resMaxDataLen) {
      *nrc = DCM_E_RESPONSE_TOO_LONG;
    }
  }

  if (E_OK == r) {
    ASLOG(DCM, ("read memoryAddress=0x%X memorySize=0x%X\n", memoryAddress, memorySize));
    /* @SWS_Dcm_91070: MemoryIdentifier is not used, set it to 0x00 */
    rm = Dcm_ReadMemory(context->opStatus, 0x00, memoryAddress, memorySize, &msgContext->resData[0],
                        nrc);
    if (DCM_READ_PENDING == rm) {
      *nrc = DCM_E_RESPONSE_PENDING;
      r = DCM_E_PENDING;
    } else if (DCM_READ_FORCE_RCRRP == rm) {
      *nrc = DCM_E_RESPONSE_PENDING;
      r = DCM_E_FORCE_RCRRP;
    } else if (DCM_READ_OK != rm) {
      r = E_NOT_OK;
    } else {
      /* pass */
    }
  }

  if (E_OK == r) {
    msgContext->resDataLen = memorySize;
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_WRITE_MEMORY_BY_ADDRESS
Std_ReturnType Dcm_DspWriteMemoryByAddress(Dcm_MsgContextType *msgContext,
                                           Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ReturnWriteMemoryType rwm;
  Dcm_ContextType *context = Dcm_GetContext();
  uint8_t memorySizeLen;
  uint8_t memoryAddressLen;
  uint32_t memoryAddress;
  uint32_t memorySize;
  uint16_t i;

  if (msgContext->reqDataLen > 3) {
    memorySizeLen = (msgContext->reqData[0] >> 4) & 0xF;
    memoryAddressLen = msgContext->reqData[0] & 0xF;
    if ((memorySizeLen >= 1) && (memorySizeLen <= 4) && (memoryAddressLen >= 1) &&
        (memoryAddressLen <= 4)) {
      if ((1 + memoryAddressLen + memorySizeLen) < msgContext->reqDataLen) {
        memoryAddress = 0;
        for (i = 0; i < memoryAddressLen; i++) {
          memoryAddress = (memoryAddress << 8) + msgContext->reqData[1 + i];
        }
        memorySize = 0;
        for (i = 0; i < memorySizeLen; i++) {
          memorySize = (memorySize << 8) + msgContext->reqData[1 + memoryAddressLen + i];
        }
        if ((1 + memoryAddressLen + memorySizeLen + memorySize) == msgContext->reqDataLen) {
          r = E_OK;
        } else {
          *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
        }
      } else {
        *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
      }
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    r = Dcm_DspIsMemoryValid(msgContext->reqData[0], DCM_MEM_ATTR_WRITE, memoryAddress, memorySize,
                             nrc);
  }

  if (E_OK == r) {
    if (memorySize > msgContext->resMaxDataLen) {
      *nrc = DCM_E_RESPONSE_TOO_LONG;
    }
  }

  if (E_OK == r) {
    ASLOG(DCM, ("write memoryAddress=0x%X memorySize=0x%X\n", memoryAddress, memorySize));
    /* @SWS_Dcm_91070: MemoryIdentifier is not used, set it to 0x00 */
    rwm = Dcm_WriteMemory(context->opStatus, 0x00, memoryAddress, memorySize,
                          &msgContext->reqData[1 + memoryAddressLen + memorySizeLen], nrc);
    if (DCM_WRITE_PENDING == rwm) {
      *nrc = DCM_E_RESPONSE_PENDING;
      r = DCM_E_PENDING;
    } else if (DCM_WRITE_FORCE_RCRRP == rwm) {
      *nrc = DCM_E_RESPONSE_PENDING;
      r = DCM_E_FORCE_RCRRP;
    } else if (DCM_WRITE_OK != rwm) {
      r = E_NOT_OK;
    } else {
      /* pass */
    }
  }

  if (E_OK == r) {
    memcpy(msgContext->resData, msgContext->reqData, 1 + memoryAddressLen + memorySizeLen);
    msgContext->resDataLen = 1 + memoryAddressLen + memorySizeLen;
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_DYNAMICALLY_DEFINE_DATA_IDENTIFIER
Std_ReturnType Dcm_DspDynamicallyDefineDataIdentifier(Dcm_MsgContextType *msgContext,
                                                      Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  if (msgContext->reqDataLen >= 3u) {
    switch (msgContext->reqData[0]) {
    case 0x01u:
      r = Dcm_DspDefineDIDById(msgContext, nrc);
      break;
    case 0x02u:
      r = Dcm_DspDefineDIDByMemoryAddress(msgContext, nrc);
      break;
    case 0x03u:
      r = Dcm_DspClearDID(msgContext, nrc);
      break;
    default:
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
      break;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }
  return r;
}

Std_ReturnType Dcm_DspReadDDDID(P2CONST(Dcm_DDDIDConfigType, AUTOMATIC, DCM_CONST) DDConfig,
                                Dcm_OpStatusType opStatus, uint8_t *data, uint16_t length,
                                Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_OK;
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  P2CONST(Dcm_rDIDConfigType, AUTOMATIC, DCM_CONST)
  rDID;
  Dcm_DDDIDEntryType *entry;
  uint8_t *tmp;
  uint16_t offset = 0;
  uint16_t i;
  boolean forceRCRRP = FALSE;

  (void)length;

  ASLOG(DCM, ("read DDDID=%X length=%d\n", DDConfig->rDID->id, length));
  for (i = 0; (i < DDConfig->context->numOfEntry) && (E_OK == r); i++) {
    entry = &DDConfig->context->entry[i];
    rDID = NULL;
    if (entry->index < config->numOfrDIDs) {
      rDID = &config->rDIDs[entry->index];
    }

    if (NULL != rDID) {
      if ((entry->position >= rDID->length) ||
          (((uint16_t)entry->position + entry->size) > rDID->length)) {
        ASLOG(DCME,
              ("Fatal Memory Error as invalid position and size for DDDID id = 0x%X entry %d\n",
               DDConfig->rDID->id, i));
        *nrc = DCM_E_CONDITIONS_NOT_CORRECT;
        r = E_NOT_OK;
      }
    } else {
      ASLOG(DCME, ("Fatal Memory Error as invalid index DDDID id = 0x%X entry %d\n",
                   DDConfig->rDID->id, i));
      *nrc = DCM_E_CONDITIONS_NOT_CORRECT;
      r = E_NOT_OK;
    }

    if (E_OK == r) {
      if ((DCM_INITIAL == opStatus) || (DCM_CANCEL == opStatus) ||
          (DCM_PENDING == entry->opStatus)) {
        entry->opStatus = DCM_CANCEL; /* set to invalid */
        /* NOTE: use the end of TX buffer to saving the whole DID data */
        tmp = &config->txBuffer[config->txBufferSize - rDID->length];
        r = rDID->readDIdFnc(opStatus, tmp, rDID->length, nrc);
        if (DCM_E_PENDING == r) {
          r = E_OK;
          *nrc = DCM_E_RESPONSE_PENDING;
        } else if (DCM_E_FORCE_RCRRP == r) {
          r = E_OK;
          *nrc = DCM_E_RESPONSE_PENDING;
          forceRCRRP = TRUE;
        } else {
          /* do nothing */
        }
        if (E_OK == r) {
          if (DCM_E_RESPONSE_PENDING == *nrc) {
            /* only in this case, schedule the call of readDIdFnc again in the next cycle */
            entry->opStatus = DCM_PENDING;
          } else {
            ASLOG(DCM, ("  read entry ID=%X(%d) position=%d size=%d\n", rDID->id, entry->index,
                        entry->position, entry->size));
            (void)memcpy(&data[offset], &tmp[entry->position], entry->size);
          }
        }
      }
      offset += entry->size;
    }
  }

  if (E_OK == r) {
    if ((DCM_E_RESPONSE_PENDING == *nrc) && (TRUE == forceRCRRP)) {
      r = DCM_E_FORCE_RCRRP;
    }
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_AUTHENTICATION
Std_ReturnType Dcm_DspAuthentication(Dcm_MsgContextType *msgContext,
                                     Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_AuthenticationConfigType, AUTOMATIC, DCM_CONST)
  config = (P2CONST(Dcm_AuthenticationConfigType, AUTOMATIC, DCM_CONST))context->curService->config;
  P2CONST(Dcm_AuthenticationType, AUTOMATIC, DCM_CONST)
  auth = NULL;
  uint8_t id;
  uint16_t len;
  uint16_t i;

  if (msgContext->reqDataLen >= 1) {
    id = msgContext->reqData[0];
    for (i = 0; i < config->numOfAuthentications; i++) {
      if (config->Authentications[i].id == id) {
        auth = &config->Authentications[i];
        r = E_OK;
        break;
      }
    }

    if (E_OK != r) {
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    if (auth->authenticationFnc != NULL) {
      len = msgContext->resMaxDataLen - 1;
      r = auth->authenticationFnc(context->opStatus, &msgContext->reqData[1],
                                  msgContext->reqDataLen - 1, &msgContext->resData[1], &len, nrc);
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    msgContext->resData[0] = id;
    msgContext->resDataLen = 1 + len;
  }

  return r;
}

Std_ReturnType Dcm_DspDeAuthentication(Dcm_OpStatusType OpStatus, const uint8_t *dataIn,
                                       uint16_t dataInLen, uint8_t *dataOut, uint16_t *dataOutLen,
                                       Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_NOT_OK;
  return r;
}

Std_ReturnType Dcm_DspVerifyCertificateUnidirectional(Dcm_OpStatusType OpStatus,
                                                      const uint8_t *dataIn, uint16_t dataInLen,
                                                      uint8_t *dataOut, uint16_t *dataOutLen,
                                                      Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_OK;
  uint16_t publicKeyLen = 0;
  const uint8_t *publicKey;
  uint16_t signatureLen = 0;
  const uint8_t *signature;
  if (0x00 != dataIn[0]) {
    r = E_NOT_OK;
    *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
  }

  if (E_OK == r) {
    publicKey = &dataIn[3];
    publicKeyLen = ((uint16_t)dataIn[1] << 8) + dataIn[2];
    signature = &dataIn[5 + publicKeyLen];
    signatureLen = ((uint16_t)dataIn[3 + publicKeyLen] << 8) + dataIn[4 + publicKeyLen];
    if ((publicKeyLen + signatureLen + 5) != dataInLen) {
      r = E_NOT_OK;
      *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
  }

  if (E_OK == r) {
    r = Dcm_AuthenticationVerifyTesterCertificate(publicKey, publicKeyLen, signature, signatureLen,
                                                  ErrorCode);
  }

  if (E_OK == r) {
    *dataOutLen -= 1;
    r = Dcm_AuthenticationGetChallenge(&dataOut[1], dataOutLen);
  }

  if (E_OK == r) {
    dataOut[0] = 0x11;
    *dataOutLen += 1;
  }

  return r;
}

Std_ReturnType Dcm_DspVerifyCertificateBidirectional(Dcm_OpStatusType OpStatus,
                                                     const uint8_t *dataIn, uint16_t dataInLen,
                                                     uint8_t *dataOut, uint16_t *dataOutLen,
                                                     Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_NOT_OK;
  return r;
}

Std_ReturnType Dcm_DspProofOfOwnership(Dcm_OpStatusType OpStatus, const uint8_t *dataIn,
                                       uint16_t dataInLen, uint8_t *dataOut, uint16_t *dataOutLen,
                                       Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_OK;

  r = Dcm_AuthenticationVerifyProofOfOwnership(dataIn, dataInLen, ErrorCode);

  if (E_OK == r) {
    dataOut[0] = 0x12;
    *dataOutLen = 1;
  }

  return r;
}

Std_ReturnType Dcm_DspTransmitCertificate(Dcm_OpStatusType OpStatus, const uint8_t *dataIn,
                                          uint16_t dataInLen, uint8_t *dataOut,
                                          uint16_t *dataOutLen,
                                          Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_NOT_OK;
  return r;
}

Std_ReturnType Dcm_DspRequestChallengeForAuthentication(Dcm_OpStatusType OpStatus,
                                                        const uint8_t *dataIn, uint16_t dataInLen,
                                                        uint8_t *dataOut, uint16_t *dataOutLen,
                                                        Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_NOT_OK;
  return r;
}

Std_ReturnType Dcm_DspVerifyProofOfOwnershipUnidirectional(
  Dcm_OpStatusType OpStatus, const uint8_t *dataIn, uint16_t dataInLen, uint8_t *dataOut,
  uint16_t *dataOutLen, Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_NOT_OK;
  return r;
}

Std_ReturnType Dcm_DspVerifyProofOfOwnershipBidirectional(Dcm_OpStatusType OpStatus,
                                                          const uint8_t *dataIn, uint16_t dataInLen,
                                                          uint8_t *dataOut, uint16_t *dataOutLen,
                                                          Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_NOT_OK;
  return r;
}

Std_ReturnType Dcm_DspAuthenticationConfiguration(Dcm_OpStatusType OpStatus, const uint8_t *dataIn,
                                                  uint16_t dataInLen, uint8_t *dataOut,
                                                  uint16_t *dataOutLen,
                                                  Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_OK;
  *dataOutLen = 1;
  dataOut[0] = DCM_AUTHENTICATION_TYPE;
  return r;
}
#endif /* DCM_USE_SERVICE_AUTHENTICATION */

void Dcm_DspInit(void) {
#ifdef DCM_USE_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER
  Dcm_ReadPeriodicDID_Init();
#endif
#ifdef DCM_USE_SERVICE_DYNAMICALLY_DEFINE_DATA_IDENTIFIER
  Dcm_DDDID_Init();
#endif
#ifdef DCM_USE_SERVICE_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER
  Dcm_IOCtlByID_Init();
#endif
}
