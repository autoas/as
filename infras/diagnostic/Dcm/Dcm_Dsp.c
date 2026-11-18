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
static void Dcm_RoutineControl_Init(void) {
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  P2CONST(Dcm_RoutineControlConfigType, AUTOMATIC, DCM_CONST) rtCtrlConfig = config->RtCtrlConfig;
  P2CONST(Dcm_RoutineControlType, AUTOMATIC, DCM_CONST) rtCtrl;
  uint16_t i;

  for (i = 0; i < rtCtrlConfig->NumOfRtCtrls; i++) {
    rtCtrl = &rtCtrlConfig->RtCtrls[i];
    if (DCM_ROUTINE_STATE_IDLE != rtCtrl->Context->State) {
      if (rtCtrl->StopRoutineFnc != NULL) {
        Dcm_NegativeResponseCodeType nrc = 0x00;
        uint16_t currentDataLength = 0;
        ASLOG(DCM, ("Routine %X stop on session change\n", rtCtrl->Id));
        (void)rtCtrl->StopRoutineFnc(NULL, DCM_INITIAL, NULL, &currentDataLength, &nrc);
      }
      rtCtrl->Context->State = DCM_ROUTINE_STATE_IDLE;
    }
  }
}

static Std_ReturnType
Dcm_DspRoutineControlStart(Dcm_MsgContextType *msgContext, Dcm_OpStatusType opStatus,
                           P2CONST(Dcm_RoutineControlType, AUTOMATIC, DCM_CONST) rtCtrl,
                           Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  uint16_t currentDataLength = msgContext->ReqDataLen - 3u;

  /* let the user application to decide whether a routine can be restarted without stop */
  r = rtCtrl->StartRoutineFnc(&msgContext->ReqData[3], opStatus, &msgContext->ResData[3],
                              &currentDataLength, nrc);

  if (E_OK == r) {
    msgContext->ResData[0] = 0x01u;
    msgContext->ResData[1] = (rtCtrl->Id >> 8u) & 0xFFu;
    msgContext->ResData[2] = rtCtrl->Id & 0xFFu;
    msgContext->ResDataLen = (Dcm_MsgLenType)(3u + currentDataLength);
    rtCtrl->Context->State = DCM_ROUTINE_STATE_STARTED;
  }

  return r;
}

static Std_ReturnType
Dcm_DspRoutineControlStop(Dcm_MsgContextType *msgContext, Dcm_OpStatusType opStatus,
                          P2CONST(Dcm_RoutineControlType, AUTOMATIC, DCM_CONST) rtCtrl,
                          Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  uint16_t currentDataLength = msgContext->ReqDataLen - 3u;

  if (rtCtrl->StopRoutineFnc != NULL) {
    if (DCM_ROUTINE_STATE_IDLE != rtCtrl->Context->State) {
      r = rtCtrl->StopRoutineFnc(&msgContext->ReqData[3], opStatus, &msgContext->ResData[3],
                                 &currentDataLength, nrc);
      if (E_OK == r) {
        rtCtrl->Context->State = DCM_ROUTINE_STATE_IDLE;
      }
    } else {
      *nrc = DCM_E_REQUEST_SEQUENCE_ERROR;
    }
  } else {
    *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
  }

  if (E_OK == r) {
    msgContext->ResData[0] = 0x02u;
    msgContext->ResData[1] = (rtCtrl->Id >> 8u) & 0xFFu;
    msgContext->ResData[2] = rtCtrl->Id & 0xFFu;
    msgContext->ResDataLen = (Dcm_MsgLenType)(3u + currentDataLength);
  }

  return r;
}

static Std_ReturnType
Dcm_DspRoutineControlResult(Dcm_MsgContextType *msgContext, Dcm_OpStatusType opStatus,
                            P2CONST(Dcm_RoutineControlType, AUTOMATIC, DCM_CONST) rtCtrl,
                            Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  uint16_t currentDataLength = msgContext->ReqDataLen - 3u;
  if (rtCtrl->RequestResultRoutineFnc != NULL) {
    if (DCM_ROUTINE_STATE_IDLE != rtCtrl->Context->State) {
      r = rtCtrl->RequestResultRoutineFnc(&msgContext->ReqData[3], opStatus,
                                          &msgContext->ResData[3], &currentDataLength, nrc);
    } else {
      *nrc = DCM_E_REQUEST_SEQUENCE_ERROR;
    }
  } else {
    *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
  }
  if (E_OK == r) {
    msgContext->ResData[0] = 0x03u;
    msgContext->ResData[1] = (rtCtrl->Id >> 8u) & 0xFFu;
    msgContext->ResData[2] = rtCtrl->Id & 0xFFu;
    msgContext->ResDataLen = (Dcm_MsgLenType)(3u + currentDataLength);
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_DYNAMICALLY_DEFINE_DATA_IDENTIFIER
static void Dcm_DDDID_Init(void) {
  uint16_t i;
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  P2CONST(Dcm_DDDIDConfigType, AUTOMATIC, DCM_CONST) DDDID;
  for (i = 0; i < config->NumOfDDDIDs; i++) {
    DDDID = &config->DDDIDs[i];
    (void)memset(DDDID->Context, 0, sizeof(Dcm_DDDIDContextType));
    (void)memset(DDDID->RDID, 0, sizeof(Dcm_rDIDConfigType));
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
  uint16_t numOfDIDs = (msgContext->ReqDataLen - 3u) / 4u;
  P2CONST(Dcm_DDDIDConfigType, AUTOMATIC, DCM_CONST) DDDID;
  P2CONST(Dcm_rDIDConfigType, AUTOMATIC, DCM_CONST) rDID;
  Dcm_DDDIDEntryType *entry;
  Dcm_SesSecAccessType SesSecAccess = {0xFF,
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
                                       0xFF,
#endif
                                       0xFF};

  if (msgContext->ReqDataLen == ((4u * numOfDIDs) + 3u)) {
    defID = ((uint16_t)msgContext->ReqData[1] << 8u) + msgContext->ReqData[2];
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
  for (i = 0; (E_OK == r) && (i < config->NumOfDDDIDs); i++) {
    DDDID = &config->DDDIDs[i];
    if (DDDID->Context->NumOfEntry > 0u) {
      if (DDDID->RDID->Id == defID) {
        *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        r = E_NOT_OK;
      }
    }
  }

  for (i = 0; (E_OK == r) && (i < config->NumOfRDIDs); i++) {
    if (config->RDIDs[i].Id == defID) {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    for (i = 0u; (E_OK == r) && (i < numOfDIDs); i++) {
      srcID =
        ((uint16_t)msgContext->ReqData[3u + (4u * i)] << 8) + msgContext->ReqData[4u + (4u * i)];
      position = msgContext->ReqData[5u + (4u * i)];
      size = msgContext->ReqData[6u + (4u * i)];

      rDID = NULL;
      for (j = 0u; (NULL == rDID) && (j < config->NumOfRDIDs); j++) {
        if (config->RDIDs[j].Id == srcID) {
          rDID = &config->RDIDs[j];
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
        if ((position >= rDID->Length) || (((uint16_t)position + size) > rDID->Length)) {
          *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
          r = E_NOT_OK;
        }
      }

      if (E_OK == r) {
        if ((length + rDID->Length + 2u) > msgContext->ResMaxDataLen) {
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
    for (i = 0; (NULL == DDDID) && (i < config->NumOfDDDIDs); i++) {
      if (config->DDDIDs[i].Context->NumOfEntry == 0u) {
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
        ((uint16_t)msgContext->ReqData[3u + (4u * i)] << 8) + msgContext->ReqData[4u + (4u * i)];
      position = msgContext->ReqData[5u + (4u * i)];
      size = msgContext->ReqData[6u + (4u * i)];
      entry = &DDDID->Context->Entry[i];
      rDID = NULL;
      for (j = 0u; (NULL == rDID) && (j < config->NumOfRDIDs); j++) {
        if (config->RDIDs[j].Id == srcID) {
          rDID = &config->RDIDs[j];
          entry->Index = j;
          entry->Position = position - 1u;
          entry->Size = size;
          entry->OpStatus = DCM_CANCEL;
          SesSecAccess.SessionMask &= rDID->SesSecAccess.SessionMask;
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
          SesSecAccess.SecurityMask &= rDID->SesSecAccess.SecurityMask;
#endif
          SesSecAccess.MiscMask &= rDID->SesSecAccess.MiscMask;
          ASLOG(DCM, ("  define entry ID=%X(%d) position=%d size=%d\n", srcID, j, entry->Position,
                      entry->Size));
        }
      }
      if (NULL == rDID) {
        r = E_NOT_OK;
        *nrc = DCM_E_CONDITIONS_NOT_CORRECT;
      }
    }
    if (E_OK == r) {
      DDDID->Context->NumOfEntry = numOfDIDs;
      DDDID->RDID->Id = defID;
      DDDID->RDID->Length = length;
      DDDID->RDID->ReadDIdFnc = DDDID->ReadDIdFnc;
      DDDID->RDID->SesSecAccess = SesSecAccess;
    }
  }

  if (E_OK == r) {
    msgContext->ResData[0] = 0x01u;
    msgContext->ResData[1] = msgContext->ReqData[1];
    msgContext->ResData[2] = msgContext->ReqData[2];
    msgContext->ResDataLen = 3u;
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

  if (msgContext->ReqDataLen == 3u) {
    defID = ((uint16_t)msgContext->ReqData[1] << 8) + msgContext->ReqData[2];
    for (i = 0u; i < config->NumOfDDDIDs; i++) {
      if (config->DDDIDs[i].Context->NumOfEntry > 0u) {
        if (config->DDDIDs[i].RDID->Id == defID) {
          DDDID = &config->DDDIDs[i];
          r = E_OK;
        }
      }
    }
    if (E_OK != r) {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    ASLOG(DCM, ("clear DDDID=%X\n", defID));
    (void)memset(DDDID->Context, 0, sizeof(Dcm_DDDIDContextType));
    (void)memset(DDDID->RDID, 0, sizeof(Dcm_rDIDConfigType));
    msgContext->ResData[0] = 0x03u;
    msgContext->ResData[1] = msgContext->ReqData[1];
    msgContext->ResData[2] = msgContext->ReqData[2];
    msgContext->ResDataLen = 3u;
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
    (P2CONST(Dcm_SessionControlConfigType, AUTOMATIC, DCM_CONST))context->CurService->Config;
  Dcm_SesCtrlType sesCtrl = msgContext->ReqData[0];
  uint16_t i;
  uint16_t u16V;

  if (1u == msgContext->ReqDataLen) {
    for (i = 0u; i < sesCtrlConfig->NumOfSesCtrls; i++) {
      if (sesCtrl == sesCtrlConfig->SesCtrls[i]) {
        break;
      }
    }
    if (i >= sesCtrlConfig->NumOfSesCtrls) {
      /* @SWS_Dcm_00307 */
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
    } else {
      r = E_OK;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    r = sesCtrlConfig->GetSesChgPermissionFnc(context->CurrentSession, sesCtrl, nrc);
    if (E_OK == r) {
      if (DCM_E_RESPONSE_PENDING == *nrc) {
        r = DCM_E_PENDING; /* pending on session check */
      }
    }
  }

  if (E_OK == r) {
    Dcm_SessionChangeIndication(context->CurrentSession, sesCtrl, FALSE);
    Dcm_DslInit();
    context->CurrentSession = sesCtrl;
#ifdef DCM_USE_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER
    Dcm_ReadPeriodicDID_OnSessionSecurityChange(); /* @SWS_Dcm_01111 */
#endif
    u16V = config->Timing->P2ServerMax * DCM_MAIN_FUNCTION_PERIOD;
    msgContext->ResData[0] = sesCtrl;
    msgContext->ResData[1] = (u16V >> 8) & 0xFFu; /* P2Server_max */
    msgContext->ResData[2] = u16V & 0xFFu;
    u16V = config->Timing->P2StarServerMax * DCM_MAIN_FUNCTION_PERIOD / 10u;
    msgContext->ResData[3] = (u16V >> 8) & 0xFFu; /* P2*Server_max */
    msgContext->ResData[4] = u16V & 0xFFu;
    msgContext->ResDataLen = 5u;
    context->TimerS3Server = config->Timing->S3Server;
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
    (P2CONST(Dcm_SecurityAccessConfigType, AUTOMATIC, DCM_CONST))context->CurService->Config;
  P2CONST(Dcm_SecLevelConfigType, AUTOMATIC, DCM_CONST) secLevelConfig = NULL;
  Dcm_SecLevelType secLevel = (msgContext->ReqData[0] + 1u) / 2u;
  uint16_t i;

  if (msgContext->ReqDataLen >= 1u) {
    for (i = 0u; i < secAccConfig->NumOfSesLevels; i++) {
      if (secAccConfig->SecLevelConfig[i].SecLevel == secLevel) {
        secLevelConfig = &secAccConfig->SecLevelConfig[i];
      }
    }
    if (NULL != secLevelConfig) {
      if (0u != (msgContext->ReqData[0] & 0x01u)) { /* request seed */
        if (1u == msgContext->ReqDataLen) {
          r = E_OK;
        } else {
          *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
        }
      } else {
        if ((1u + secLevelConfig->KeySize) == msgContext->ReqDataLen) {
          r = E_OK;
        } else {
          *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
        }
      }

      if (E_OK == r) {
        r = Dcm_DslIsSessionSupported(context->CurrentSession, secLevelConfig->SessionMask);
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
    if ((context->SecurityDelayTimer > 0u)
#ifdef DCM_USE_SECURITY_SEED_PROTECTION
        && (0u != (msgContext->ReqData[0] & 0x01u)) /* request seed */
#endif
    ) { /* @SWS_Dcm_01350 */
      *nrc = DCM_E_REQUIRED_TIME_DELAY_NOT_EXPIRED;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    if (0u != (msgContext->ReqData[0] & 0x01u)) { /* request seed */
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
    (P2CONST(Dcm_RoutineControlConfigType, AUTOMATIC, DCM_CONST))context->CurService->Config;
  P2CONST(Dcm_RoutineControlType, AUTOMATIC, DCM_CONST) rtCtrl = NULL;
  uint16_t id;
  uint16_t i;

  if (msgContext->ReqDataLen >= 3u) {
    id = ((uint16_t)msgContext->ReqData[1] << 8) + msgContext->ReqData[2];
    for (i = 0; i < rtCtrlConfig->NumOfRtCtrls; i++) {
      if (rtCtrlConfig->RtCtrls[i].Id == id) {
        rtCtrl = &rtCtrlConfig->RtCtrls[i];
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
    switch (msgContext->ReqData[0]) {
    case 0x01u:
      r = Dcm_DspRoutineControlStart(msgContext, context->OpStatus, rtCtrl, nrc);
      break;
    case 0x02u:
      r = Dcm_DspRoutineControlStop(msgContext, context->OpStatus, rtCtrl, nrc);
      break;
    case 0x03u:
      r = Dcm_DspRoutineControlResult(msgContext, context->OpStatus, rtCtrl, nrc);
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
    (P2CONST(Dcm_RequestDownloadConfigType, AUTOMATIC, DCM_CONST))context->CurService->Config;
  uint8_t dataFormatIdentifier;
  uint8_t memorySizeLen;
  uint8_t memoryAddressLen;
  uint32_t memoryAddress;
  uint32_t memorySize;
  /* @SWS_Dcm_01420 */
  uint32_t BlockLength = config->RxBufferSize;
  uint16_t i;

  if (msgContext->ReqDataLen > 3) {
    dataFormatIdentifier = msgContext->ReqData[0];
    memorySizeLen = (msgContext->ReqData[1] >> 4) & 0xFu;
    memoryAddressLen = msgContext->ReqData[1] & 0xFu;
    if ((memorySizeLen >= 1u) && (memorySizeLen <= 4u) && (memoryAddressLen >= 1u) &&
        (memoryAddressLen <= 4u)) {
      if ((2 + memoryAddressLen + memorySizeLen) == msgContext->ReqDataLen) {
        /* @SWS_Dcm_00856: support all possible case */
        r = E_OK;
        memoryAddress = 0u;
        for (i = 0u; i < memoryAddressLen; i++) {
          memoryAddress = (memoryAddress << 8) + msgContext->ReqData[2u + i];
        }
        memorySize = 0u;
        for (i = 0u; i < memorySizeLen; i++) {
          memorySize = (memorySize << 8) + msgContext->ReqData[2u + memoryAddressLen + i];
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
    if (DCM_UDT_IDLE_STATE != context->UDTData.State) {
      *nrc = DCM_E_REQUEST_SEQUENCE_ERROR;
    }
  }

  if (E_OK == r) {
    ASLOG(DCM, ("download memoryAddress=0x%X memorySize=0x%X\n", memoryAddress, memorySize));
    /* @SWS_Dcm_91070: MemoryIdentifier is not used, set it to 0x00 */
    r = rdConfig->ProcessRequestDownloadFnc(context->OpStatus, dataFormatIdentifier, 0x00,
                                            memoryAddress, memorySize, &BlockLength, nrc);
  }

  if (E_OK == r) {
    if ((BlockLength > 0xFFFF) && (memorySizeLen <= 2)) {
      memorySizeLen = 4;
    }
    msgContext->ResData[0] = memorySizeLen << 4; /* lengthFormatIdentifier */
    for (i = 0u; i < memorySizeLen; i++) {
      msgContext->ResData[memorySizeLen - i] = (BlockLength >> (8 * i)) & 0xFFu;
    }
    msgContext->ResDataLen = 1u + memorySizeLen;
    context->UDTData.State = DCM_UDT_DOWNLOAD_STATE;
    context->UDTData.MemoryAddress = memoryAddress;
    context->UDTData.MemorySize = memorySize;
    context->UDTData.Offset = 0u;
    context->UDTData.BlockSequenceCounter = 1u;
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
    (P2CONST(Dcm_RequestUploadConfigType, AUTOMATIC, DCM_CONST))context->CurService->Config;
  uint8_t dataFormatIdentifier;
  uint8_t memorySizeLen;
  uint8_t memoryAddressLen;
  uint32_t memoryAddress;
  uint32_t memorySize;
  /* @SWS_Dcm_01422 */
  uint32_t BlockLength = msgContext->ResMaxDataLen + 1;
  uint8_t i;

  if (msgContext->ReqDataLen > 3) {
    dataFormatIdentifier = msgContext->ReqData[0];
    memorySizeLen = (msgContext->ReqData[1] >> 4) & 0xFu;
    memoryAddressLen = msgContext->ReqData[1] & 0xFu;
    if ((memorySizeLen >= 1u) && (memorySizeLen <= 4u) && (memoryAddressLen >= 1u) &&
        (memoryAddressLen <= 4u)) {
      if ((2 + memoryAddressLen + memorySizeLen) == msgContext->ReqDataLen) {
        /* @SWS_Dcm_00857: support all possible case */
        r = E_OK;
        memoryAddress = 0u;
        for (i = 0u; i < memoryAddressLen; i++) {
          memoryAddress = (memoryAddress << 8) + msgContext->ReqData[2 + i];
        }
        memorySize = 0u;
        for (i = 0u; i < memorySizeLen; i++) {
          memorySize = (memorySize << 8) + msgContext->ReqData[2 + memoryAddressLen + i];
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
    if (DCM_UDT_IDLE_STATE != context->UDTData.State) {
      *nrc = DCM_E_REQUEST_SEQUENCE_ERROR;
    }
  }

  if (E_OK == r) {
    ASLOG(DCM, ("upload memoryAddress=0x%X memorySize=0x%X\n", memoryAddress, memorySize));
    /* @SWS_Dcm_91070: MemoryIdentifier is not used, set it to 0x00 */
    r = ruConfig->ProcessRequestUploadFnc(context->OpStatus, dataFormatIdentifier, 0x00,
                                          memoryAddress, memorySize, &BlockLength, nrc);
  }

  if (E_OK == r) {
    if ((BlockLength > 0xFFFF) && (memorySizeLen <= 2)) {
      memorySizeLen = 4;
    }
    msgContext->ResData[0] = memorySizeLen << 4; /* lengthFormatIdentifier */
    for (i = 0u; i < memorySizeLen; i++) {
      msgContext->ResData[memorySizeLen - i] = (BlockLength >> (8 * i)) & 0xFFu;
    }
    msgContext->ResDataLen = 1u + memorySizeLen;
    context->UDTData.State = DCM_UDT_UPLOAD_STATE;
    context->UDTData.MemoryAddress = memoryAddress;
    context->UDTData.MemorySize = memorySize;
    context->UDTData.Offset = 0u;
    context->UDTData.BlockSequenceCounter = 1u;
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
    (P2CONST(Dcm_TransferDataConfigType, AUTOMATIC, DCM_CONST))context->CurService->Config;
  uint32_t memoryAddress = context->UDTData.MemoryAddress + context->UDTData.Offset;
  uint32_t memorySize = context->UDTData.MemorySize - context->UDTData.Offset;
  Dcm_ReturnWriteMemoryType retW;
  Dcm_ReturnReadMemoryType retR;

  if (msgContext->ReqDataLen >= 1) {
    if (context->UDTData.State != DCM_UDT_IDLE_STATE) {
      if (context->UDTData.BlockSequenceCounter == msgContext->ReqData[0]) {
        if (DCM_UDT_DOWNLOAD_STATE == context->UDTData.State) {
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
    if (DCM_UDT_DOWNLOAD_STATE == context->UDTData.State) {
      if (memorySize < (msgContext->ReqDataLen - 1)) {
        *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
        r = E_NOT_OK;
      } else {
        memorySize = msgContext->ReqDataLen - 1;
      }
    } else {
      if (1 == msgContext->ReqDataLen) {
        if (memorySize > (msgContext->ResMaxDataLen - 1)) {
          memorySize = msgContext->ResMaxDataLen - 1;
        }
      } else {
        *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
        r = E_NOT_OK;
      }
    }
  }

  if (E_OK == r) {
    if (DCM_UDT_DOWNLOAD_STATE == context->UDTData.State) {
      retW = tfdConfig->WriteFnc(context->OpStatus, 0x00, memoryAddress, memorySize,
                                 &msgContext->ReqData[1], nrc);
      if (DCM_WRITE_PENDING == retW) {
        *nrc = DCM_E_RESPONSE_PENDING;
      } else if (DCM_WRITE_FORCE_RCRRP == retW) {
        r = DCM_E_FORCE_RCRRP;
      } else if (DCM_WRITE_OK == retW) {
        context->UDTData.Offset += memorySize;
        msgContext->ResData[0] = context->UDTData.BlockSequenceCounter;
        msgContext->ResDataLen = 1;
        context->UDTData.BlockSequenceCounter++;
      } else { /* FAILED */
        r = E_NOT_OK;
        if (DCM_POS_RESP == *nrc) {
          *nrc = DCM_E_GENERAL_PROGRAMMING_FAILURE;
        }
      }
    } else {
      retR = tfdConfig->ReadFnc(context->OpStatus, 0x00, memoryAddress, memorySize,
                                &msgContext->ResData[1], nrc);
      if (DCM_READ_PENDING == retR) {
        *nrc = DCM_E_RESPONSE_PENDING;
      } else if (DCM_READ_FORCE_RCRRP == retR) {
        r = DCM_E_FORCE_RCRRP;
      } else if (DCM_READ_OK == retR) {
        context->UDTData.Offset += memorySize;
        msgContext->ResData[0] = context->UDTData.BlockSequenceCounter;
        msgContext->ResDataLen = 1 + memorySize;
        context->UDTData.BlockSequenceCounter++;
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
    (P2CONST(Dcm_TransferExitConfigType, AUTOMATIC, DCM_CONST))context->CurService->Config;

  if (0 == msgContext->ReqDataLen) {
    r = E_OK;
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    if (DCM_UDT_IDLE_STATE == context->UDTData.State) {
      r = E_NOT_OK;
      *nrc = DCM_E_REQUEST_SEQUENCE_ERROR;
    }
  }

  if (E_OK == r) {
    r = tfeConfig->TransferExitFnc(context->OpStatus, nrc);
  }

  if (E_OK == r) {
    context->UDTData.State = DCM_UDT_IDLE_STATE;
    context->UDTData.BlockSequenceCounter = 0;
    context->UDTData.MemoryAddress = 0;
    context->UDTData.MemorySize = 0;
    context->UDTData.Offset = 0;
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_ECU_RESET
Std_ReturnType Dcm_DspEcuReset(Dcm_MsgContextType *msgContext, Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_EcuResetConfigType, AUTOMATIC, DCM_CONST)
  rstConfig = (P2CONST(Dcm_EcuResetConfigType, AUTOMATIC, DCM_CONST))context->CurService->Config;
  if (1u == msgContext->ReqDataLen) {
    r = E_OK;
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    switch (msgContext->ReqData[0]) {
    case 0x01u: /* hard reset */
    case 0x03u: /* soft reset */
      context->ResetType = msgContext->ReqData[0];
      msgContext->ResData[0] = context->ResetType;
      msgContext->ResDataLen = 1u;
      break;
    default:
      r = E_NOT_OK;
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
      break;
    }
  }

  if (E_OK == r) {
    r = rstConfig->GetEcuResetPermissionFnc(context->OpStatus, nrc);
    if (E_OK == r) {
      if (DCM_E_RESPONSE_PENDING == *nrc) {
        context->Timer2Reset = 0;
      } else {
        if (rstConfig->Delay == 0u) {
          context->Timer2Reset = 1u;
        } else {
          context->Timer2Reset = rstConfig->Delay;
        }
      }
    } else {
      context->Timer2Reset = 0;
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
  rDidConfig = (P2CONST(Dcm_ReadDIDConfigType, AUTOMATIC, DCM_CONST))context->CurService->Config;
  P2CONST(Dcm_ReadDIDType, AUTOMATIC, DCM_CONST) rDid = NULL;
  uint16_t id;
  uint16_t numOfDids = 0;
  Dcm_MsgLenType totalResLength = 0;
  boolean forceRCRRP = FALSE;
  uint16_t i;
  uint16_t j;
  uint16_t didLength;

  if ((msgContext->ReqDataLen >= 2u) && ((msgContext->ReqDataLen & 0x01u) == 0u)) {
    numOfDids = msgContext->ReqDataLen >> 1;
    for (i = 0u; (i < numOfDids) && (E_OK == r) && (DCM_INITIAL == context->OpStatus); i++) {
      rDid = NULL;
      id = ((uint16_t)msgContext->ReqData[i * 2u] << 8) + msgContext->ReqData[(i * 2u) + 1u];
      for (j = 0u; j < rDidConfig->NumOfDIDs; j++) {
        if (rDidConfig->DIDs[j].RDID->Id == id) {
          rDid = &rDidConfig->DIDs[j];
          break;
        }
      }
      if (NULL == rDid) {
        *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        r = E_NOT_OK;
      }
#ifdef DCM_USE_READ_DID_WITH_DYNAMIC_LENGTH
      else if ((TRUE == rDid->RDID->DynamicLength) && (i != (numOfDids - 1u))) {
        /* read DID with dynamic length, it must be the last one */
        *nrc = DCM_E_GENERAL_REJECT;
        r = E_NOT_OK;
      }
#endif
      else {
        totalResLength += rDid->RDID->Length + 2u;
#ifdef DCM_USE_READ_DID_WITH_DYNAMIC_LENGTH
        if (TRUE == rDid->RDID->DynamicLength) {
          totalResLength += 2; /* 2 more bytes to store the internal dynamic length */
        }
#endif
        r = Dcm_DslServiceDIDSesSecPhyFuncCheck(context, &rDid->RDID->SesSecAccess, nrc);
      }
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    r = E_NOT_OK;
  }

  if (E_OK == r) {
    if (totalResLength > msgContext->ResMaxDataLen) {
      *nrc = DCM_E_RESPONSE_TOO_LONG;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    totalResLength = 0u;
    for (i = 0u; (i < numOfDids) && (E_OK == r); i++) {
      rDid = NULL;
      id = ((uint16_t)msgContext->ReqData[i * 2u] << 8) + msgContext->ReqData[(i * 2u) + 1u];
      for (j = 0u; j < rDidConfig->NumOfDIDs; j++) {
        if (rDidConfig->DIDs[j].RDID->Id == id) {
          rDid = &rDidConfig->DIDs[j];
          break;
        }
      }
      if (NULL == rDid) {
        *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        r = E_NOT_OK;
      } else {
        didLength = rDid->RDID->Length;
      }
      if (E_OK == r) {
        msgContext->ResData[totalResLength] = (id >> 8) & 0xFFu;
        msgContext->ResData[totalResLength + 1u] = id & 0xFFu;
        if ((DCM_INITIAL == context->OpStatus) || (DCM_CANCEL == context->OpStatus) ||
            (DCM_PENDING == rDid->Context->OpStatus)) {
          rDid->Context->OpStatus = DCM_CANCEL; /* set to invalid */
          r = rDid->RDID->ReadDIdFnc(context->OpStatus, &msgContext->ResData[totalResLength + 2u],
                                     didLength, nrc);
#ifdef DCM_USE_READ_DID_WITH_DYNAMIC_LENGTH
          if (E_OK == r) {
            if (TRUE == rDid->RDID->DynamicLength) {
              didLength =
                ((uint16_t)msgContext->ResData[totalResLength + 2u + rDid->RDID->Length] << 8u) +
                msgContext->ResData[totalResLength + 2u + rDid->RDID->Length + 1];
            }
          }
#endif
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
              rDid->Context->OpStatus = DCM_PENDING;
            }
          }
        }
        totalResLength += didLength + 2u;
      }
    }
    msgContext->ResDataLen = totalResLength;
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
    (P2CONST(Dcm_ReadScalingDIDConfigType, AUTOMATIC, DCM_CONST))context->CurService->Config;
  P2CONST(Dcm_rDIDConfigType, AUTOMATIC, DCM_CONST) rDid = NULL;
  uint16_t id;
  uint16_t i;

  if (2u == msgContext->ReqDataLen) {
    id = ((uint16_t)msgContext->ReqData[0] << 8) + msgContext->ReqData[1];
    for (i = 0; i < rDidConfig->NumOfDIDs; i++) {
      if (rDidConfig->DIDs[i].Id == id) {
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
    msgContext->ResData[0] = (rDid->Id >> 8) & 0xFF;
    msgContext->ResData[1] = rDid->Id & 0xFF;
    r = rDid->ReadDIdFnc(context->OpStatus, &msgContext->ResData[2], rDid->Length, nrc);
    msgContext->ResDataLen = 2 + rDid->Length;
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
  wDidConfig = (P2CONST(Dcm_WriteDIDConfigType, AUTOMATIC, DCM_CONST))context->CurService->Config;
  P2CONST(Dcm_WriteDIDType, AUTOMATIC, DCM_CONST) wDid = NULL;
  uint16_t id;
  uint16_t i;

  if (msgContext->ReqDataLen > 2) {
    wDid = NULL;
    id = ((uint16_t)msgContext->ReqData[0] << 8) + msgContext->ReqData[1];
    for (i = 0; i < wDidConfig->NumOfDIDs; i++) {
      if (wDidConfig->DIDs[i].Id == id) {
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
    if ((wDid->Length + 2) != msgContext->ReqDataLen) {
      *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    msgContext->ResData[0] = (wDid->Id >> 8) & 0xFF;
    msgContext->ResData[1] = wDid->Id & 0xFF;
    r = wDid->WriteDIdFnc(context->OpStatus, &msgContext->ReqData[2], wDid->Length, nrc);
    msgContext->ResDataLen = 2;
  }

  return r;
}
#endif
#ifdef DCM_USE_SERVICE_TESTER_PRESENT
Std_ReturnType Dcm_DspTesterPresent(Dcm_MsgContextType *msgContext,
                                    Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;

  if (1u == msgContext->ReqDataLen) {
    if (0x00u == msgContext->ReqData[0]) {
      msgContext->ResData[0] = 0x00u;
      msgContext->ResDataLen = 1u;
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

  if (1 == msgContext->ReqDataLen) {
    switch (msgContext->ReqData[0]) {
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
    msgContext->ResData[0] = msgContext->ReqData[0];
    msgContext->ResDataLen = 1u;
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_CLEAR_DIAGNOSTIC_INFORMATION
Std_ReturnType Dcm_DspClearDTC(Dcm_MsgContextType *msgContext, Dcm_NegativeResponseCodeType *nrc) {

  Std_ReturnType r = E_NOT_OK;
  uint32_t groupDTC = 0x0;
  Dcm_ContextType *context = Dcm_GetContext();

  if (3 == msgContext->ReqDataLen) {
    groupDTC = ((uint32_t)msgContext->ReqData[0] << 16) + ((uint32_t)msgContext->ReqData[1] << 8) +
               msgContext->ReqData[2];
    r = E_OK;
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    if (DCM_INITIAL == context->OpStatus) {
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
                                                         Dem_DTCOriginType dTCOrigin) {
  Std_ReturnType r = E_NOT_OK;
  uint8_t statusMask;
  uint16_t NumberOfFilteredDTC = 0;

  if (2 == msgContext->ReqDataLen) {
    statusMask = msgContext->ReqData[1];

    r = Dem_SetDTCFilter(0, statusMask, DEM_DTC_FORMAT_UDS, dTCOrigin, FALSE, 0, FALSE);
    if (E_OK == r) {
      r = Dem_GetNumberOfFilteredDTC(0, &NumberOfFilteredDTC);
      if (E_OK == r) {
        r = Dem_GetDTCStatusAvailabilityMask(0, &statusMask, dTCOrigin);
      }
    }

    if (E_OK == r) {
      msgContext->ResData[0] = msgContext->ReqData[0];
      msgContext->ResData[1] = statusMask;
      msgContext->ResData[2] = DEM_DTC_FORMAT_UDS; /* DTCFormatIdentifier */
      msgContext->ResData[3] = (uint8_t)((NumberOfFilteredDTC >> 8) & 0xFF);
      msgContext->ResData[4] = (uint8_t)(NumberOfFilteredDTC & 0xFF);
      msgContext->ResDataLen = 5;
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
                                                 Dem_DTCOriginType dTCOrigin) {
  Std_ReturnType r = E_NOT_OK;
  uint8_t statusMask;
  uint16_t NumberOfFilteredDTC = 0;
  uint32_t DTCNumber;
  Dem_UdsStatusByteType udsStatus;
  uint16_t i;

  if (2 == msgContext->ReqDataLen) {
    statusMask = msgContext->ReqData[1];

    r = Dem_SetDTCFilter(0, statusMask, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, 0,
                         FALSE);
    if (E_OK == r) {
      r = Dem_GetNumberOfFilteredDTC(0, &NumberOfFilteredDTC);
      if (E_OK == r) {
        r = Dem_GetDTCStatusAvailabilityMask(0, &statusMask, dTCOrigin);
      }
    }

    if (E_OK == r) {
      if ((NumberOfFilteredDTC * 4 + 2) <= msgContext->ResMaxDataLen) {
        msgContext->ResData[0] = msgContext->ReqData[0];
        msgContext->ResData[1] = statusMask;
        for (i = 0; (i < NumberOfFilteredDTC) && (E_OK == r); i++) {
          r = Dem_GetNextFilteredDTC(0, &DTCNumber, &udsStatus);
          if (E_OK == r) {
            msgContext->ResData[2 + 4 * i] = (uint8_t)((DTCNumber >> 16) & 0xFF);
            msgContext->ResData[3 + 4 * i] = (uint8_t)((DTCNumber >> 8) & 0xFF);
            msgContext->ResData[4 + 4 * i] = (uint8_t)(DTCNumber & 0xFF);
            msgContext->ResData[5 + 4 * i] = udsStatus;
          }
        }
        if (E_OK == r) {
          msgContext->ResDataLen = NumberOfFilteredDTC * 4 + 2;
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

  if (1 == msgContext->ReqDataLen) {
    r = Dem_SelectDTC(0, 0xFFFFFF, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY);
    if (E_OK == r) {
      r = Dem_SetFreezeFrameRecordFilter(0, DEM_DTC_FORMAT_UDS);
    }
    if (E_OK == r) {
      r = Dem_GetNumberOfFreezeFrameRecords(0, &NumberOfFilteredRecords);
    }

    if (E_OK == r) {
      if ((NumberOfFilteredRecords * 4 + 1) <= msgContext->ResMaxDataLen) {
        msgContext->ResData[0] = 0x03;
        for (i = 0; (i < NumberOfFilteredRecords) && (E_OK == r); i++) {
          r = Dem_GetNextFilteredRecord(0, &DTCNumber, &RecordNumber);
          if (E_OK == r) {
            msgContext->ResData[1 + 4 * i] = (uint8_t)((DTCNumber >> 16) & 0xFF);
            msgContext->ResData[2 + 4 * i] = (uint8_t)((DTCNumber >> 8) & 0xFF);
            msgContext->ResData[3 + 4 * i] = (uint8_t)(DTCNumber & 0xFF);
            msgContext->ResData[4 + 4 * i] = RecordNumber;
          }
        }
        if (E_OK == r) {
          msgContext->ResDataLen = NumberOfFilteredRecords * 4 + 1;
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
  uint16_t resLen = msgContext->ResMaxDataLen - 1;

  if (5 == msgContext->ReqDataLen) {
    DTCNumber = ((uint32_t)msgContext->ReqData[1] << 16) + ((uint32_t)msgContext->ReqData[2] << 8) +
                msgContext->ReqData[3];
    RecordNumber = msgContext->ReqData[4];
    r = Dem_SelectDTC(0, DTCNumber, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY);
    if (E_OK == r) {
      r = Dem_SelectFreezeFrameData(0, RecordNumber);
    }

    if (E_OK == r) {
      r = Dem_GetNextFreezeFrameData(0, &msgContext->ResData[1], &resLen);
    }

    if (E_OK == r) {
      msgContext->ResData[0] = 0x04;
      msgContext->ResDataLen = resLen + 1;
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
                                                                  Dem_DTCOriginType dTCOrigin) {
  Std_ReturnType r = E_NOT_OK;
  uint32_t DTCNumber;
  uint8_t RecordNumber;
  uint16_t resLen = msgContext->ResMaxDataLen - 1;

  if (5 == msgContext->ReqDataLen) {
    DTCNumber = ((uint32_t)msgContext->ReqData[1] << 16) + ((uint32_t)msgContext->ReqData[2] << 8) +
                msgContext->ReqData[3];
    RecordNumber = msgContext->ReqData[4];
    r = Dem_SelectDTC(0, DTCNumber, DEM_DTC_FORMAT_UDS, dTCOrigin);
    if (E_OK == r) {
      r = Dem_SelectFreezeFrameData(0, RecordNumber);
    }

    if (E_OK == r) {
      r = Dem_GetNextExtendedDataRecord(0, &msgContext->ResData[1], &resLen);
    }

    if (E_OK == r) {
      msgContext->ResData[0] = 0x06;
      msgContext->ResDataLen = resLen + 1;
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

  if (1 == msgContext->ReqDataLen) {
    r = Dem_SetDTCFilter(0, 0, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, 0, FALSE);
    if (E_OK == r) {
      r = Dem_GetNumberOfFilteredDTC(0, &NumberOfFilteredDTC);
      if (E_OK == r) {
        r = Dem_GetDTCStatusAvailabilityMask(0, &udsStatus, DEM_DTC_ORIGIN_PRIMARY_MEMORY);
      }
    }

    if (E_OK == r) {
      if ((NumberOfFilteredDTC * 4 + 2) <= msgContext->ResMaxDataLen) {
        msgContext->ResData[0] = msgContext->ReqData[0];
        msgContext->ResData[1] = udsStatus;
        for (i = 0; (i < NumberOfFilteredDTC) && (E_OK == r); i++) {
          r = Dem_GetNextFilteredDTC(0, &DTCNumber, &udsStatus);
          if (E_OK == r) {
            msgContext->ResData[2 + 4 * i] = (uint8_t)((DTCNumber >> 16) & 0xFF);
            msgContext->ResData[3 + 4 * i] = (uint8_t)((DTCNumber >> 8) & 0xFF);
            msgContext->ResData[4 + 4 * i] = (uint8_t)(DTCNumber & 0xFF);
            msgContext->ResData[5 + 4 * i] = udsStatus;
          }
        }
        if (E_OK == r) {
          msgContext->ResDataLen = NumberOfFilteredDTC * 4 + 2;
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
  config = (P2CONST(Dcm_ReadDTCInfoConfigType, AUTOMATIC, DCM_CONST))context->CurService->Config;
  P2CONST(Dcm_ReadDTCSubFunctionConfigType, AUTOMATIC, DCM_CONST) subFunction = NULL;
  uint8_t type;
  uint16_t i;

  if (msgContext->ReqDataLen >= 1) {
    type = msgContext->ReqData[0];
    for (i = 0; i < config->NumOfSubFunctions; i++) {
      if (config->SubFunctions[i].Type == type) {
        subFunction = &config->SubFunctions[i];
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
    r = subFunction->SubFnc(msgContext, nrc);
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
  for (i = 0; i < IOCtlConfig->NumOfIOCtrls; i++) {
    IOCtrl = &IOCtlConfig->IOCtrls[i];
    if (IOCtrl->Context->RequestMask != 0u) {
      if (IOCtrl->ReturnControlToEcuFnc != NULL) {
        ASLOG(DCM, ("IOCTL %X return to ECU\n", IOCtrl->Id));
        (void)IOCtrl->ReturnControlToEcuFnc(NULL, 0, config->TxBuffer, &resDataLen, &nrc);
      }
    }
    IOCtrl->Context->RequestMask = 0;
  }
}

Std_ReturnType Dcm_DspIOControlByIdentifier(Dcm_MsgContextType *msgContext,
                                            Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_IOControlConfigType, AUTOMATIC, DCM_CONST)
  config = (P2CONST(Dcm_IOControlConfigType, AUTOMATIC, DCM_CONST))context->CurService->Config;
  P2CONST(Dcm_IOControlType, AUTOMATIC, DCM_CONST) IOCtrl = NULL;
  uint16_t id;
  uint8_t action;
  uint16_t resDataLen = msgContext->ResMaxDataLen - 3u;
  uint16_t i;
  P2CONST(Dcm_IOCtrlExecuteFncType, AUTOMATIC, DCM_CONST) ExecuteFncs;

  if (msgContext->ReqDataLen >= 3u) {
    id = ((uint16_t)msgContext->ReqData[0] << 8) + msgContext->ReqData[1];
    action = msgContext->ReqData[2];
    for (i = 0; i < config->NumOfIOCtrls; i++) {
      if (config->IOCtrls[i].Id == id) {
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
        r = ExecuteFncs[action](&msgContext->ReqData[3], msgContext->ReqDataLen - 3u,
                                &msgContext->ResData[3], &resDataLen, nrc);
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
      IOCtrl->Context->RequestMask |= ((uint8_t)1 << action);
    } else {
      IOCtrl->Context->RequestMask = 0u;
    }
    msgContext->ResData[0] = (IOCtrl->Id >> 8) & 0xFFu;
    msgContext->ResData[1] = IOCtrl->Id & 0xFFu;
    msgContext->ResData[2] = action;
    msgContext->ResDataLen = 3u + resDataLen;
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
    (P2CONST(Dcm_CommunicationControlConfigType, AUTOMATIC, DCM_CONST))context->CurService->Config;
  P2CONST(Dcm_ComCtrlType, AUTOMATIC, DCM_CONST)
  ComCtrl = NULL;
  uint8_t id;
  uint8_t comType;
  uint16_t i;

  if (2 == msgContext->ReqDataLen) {
    id = msgContext->ReqData[0];
    comType = msgContext->ReqData[1];
    for (i = 0; i < config->NumOfComCtrls; i++) {
      if (config->ComCtrls[i].Id == id) {
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
    if (ComCtrl->ComCtrlFnc != NULL) {
      r = ComCtrl->ComCtrlFnc(comType, nrc);
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    msgContext->ResData[0] = id;
    msgContext->ResDataLen = 1;
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER
void Dcm_ReadPeriodicDID_Init(void) {
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  P2CONST(Dcm_ReadPeriodicDIDConfigType, AUTOMATIC, DCM_CONST) pDIDConfig = config->RPDIDConfig;
  P2CONST(Dcm_ReadPeriodicDIDType, AUTOMATIC, DCM_CONST) rDid = NULL;
  uint16_t i;
  if (NULL != pDIDConfig) {
    for (i = 0; i < pDIDConfig->NumOfDIDs; i++) {
      rDid = &pDIDConfig->DIDs[i];
      (void)memset(rDid->Context, 0, sizeof(Dcm_ReadPeriodicDIDContextType));
      rDid->Context->OpStatus = DCM_CANCEL;
    }
  }
}

void Dcm_ReadPeriodicDID_OnSessionSecurityChange(void) {
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  P2CONST(Dcm_ReadPeriodicDIDConfigType, AUTOMATIC, DCM_CONST) pDIDConfig = config->RPDIDConfig;
  P2CONST(Dcm_ReadPeriodicDIDType, AUTOMATIC, DCM_CONST) rDid = NULL;
  Dcm_ContextType *context = Dcm_GetContext();
  Dcm_NegativeResponseCodeType nrc;
  boolean stopIt;
  uint16_t i;
  Std_ReturnType r;

  if (NULL != pDIDConfig) {
    for (i = 0; i < pDIDConfig->NumOfDIDs; i++) {
      rDid = &pDIDConfig->DIDs[i];
      stopIt = FALSE;
      if (DCM_DEFAULT_SESSION == context->CurrentSession) { /* @SWS_Dcm_01107*/
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
        (void)memset(rDid->Context, 0, sizeof(Dcm_ReadPeriodicDIDContextType));
        rDid->Context->OpStatus = DCM_CANCEL;
      }
    }
  }
}

void Dcm_MainFunction_ReadPeriodicDID(void) {
  Std_ReturnType r = E_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ConfigType, AUTOMATIC, DCM_CONST) config = Dcm_GetConfig();
  P2CONST(Dcm_ReadPeriodicDIDConfigType, AUTOMATIC, DCM_CONST) pDIDConfig = config->RPDIDConfig;
  P2CONST(Dcm_ReadPeriodicDIDType, AUTOMATIC, DCM_CONST) rDid = NULL;
  uint16_t i;
  Dcm_MsgType resData = config->TxBuffer;
  Dcm_MsgLenType totalResLength;
  Dcm_NegativeResponseCodeType nrc;

  if (NULL != pDIDConfig) {
    for (i = 0; i < pDIDConfig->NumOfDIDs; i++) {
      rDid = &pDIDConfig->DIDs[i];
      if (rDid->Context->Reload > 0u) {
        if (rDid->Context->Timer > 0u) {
          rDid->Context->Timer--;
          if (0u == rDid->Context->Timer) {
            rDid->Context->OpStatus = DCM_INITIAL;
            rDid->Context->Timer = rDid->Context->Reload;
          }
        } else {
          ASLOG(DCME, ("read periodic timer stopt\n"));
          rDid->Context->Timer = rDid->Context->Reload;
        }
      }
    }
  }

  if ((NULL != pDIDConfig) && (DCM_BUFFER_IDLE == context->TxBufferState)) {
    resData[0] = 0x6Au;
    totalResLength = 1u;
    for (i = 0u; i < pDIDConfig->NumOfDIDs; i++) {
      rDid = &pDIDConfig->DIDs[i];
      if (rDid->Context->OpStatus != DCM_CANCEL) {
        if ((totalResLength + 1u + rDid->DID->Length) > config->TxBufferSize) {
          break;
        }
        nrc = DCM_POS_RESP;
        r = rDid->DID->ReadDIdFnc(rDid->Context->OpStatus, &resData[totalResLength + 1u],
                                  rDid->DID->Length, &nrc);
        if ((DCM_E_PENDING == r) || (DCM_E_FORCE_RCRRP == r)) {
          r = E_OK;
          nrc = DCM_E_RESPONSE_PENDING;
        }
        if (r != E_OK) {
          ASLOG(DCME, ("read periodic DID FAILED\n"));
          rDid->Context->OpStatus = DCM_CANCEL;
        } else if (DCM_POS_RESP == nrc) { /* reading is done */
          resData[totalResLength] = rDid->DID->Id & 0xFFu;
          totalResLength += 1u + rDid->DID->Length;
          rDid->Context->OpStatus = DCM_CANCEL;
        } else if (DCM_E_RESPONSE_PENDING == nrc) {
          rDid->Context->OpStatus = DCM_PENDING;
        } else {
          ASLOG(DCME, ("read periodic DID meet invalid NRC\n"));
          rDid->Context->OpStatus = DCM_CANCEL;
        }
      }
    }
    if (totalResLength > 1u) {
      context->TxTpSduLength = totalResLength;
      context->TxBufferState = DCM_BUFFER_FULL;
    }
  }
}

Std_ReturnType Dcm_DspReadDataByPeriodicIdentifier(Dcm_MsgContextType *msgContext,
                                                   Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  P2CONST(Dcm_ReadPeriodicDIDConfigType, AUTOMATIC, DCM_CONST)
  config =
    (P2CONST(Dcm_ReadPeriodicDIDConfigType, AUTOMATIC, DCM_CONST))context->CurService->Config;
  P2CONST(Dcm_ReadPeriodicDIDType, AUTOMATIC, DCM_CONST) rDid = NULL;
  uint8_t id;
  uint8_t transmissionMode;
  uint16_t numOfDids = 0;
  Dcm_MsgLenType totalResLength = 0;
  uint16_t reload = 0;
  uint16_t i;
  uint16_t j;

  if (msgContext->ReqDataLen >= 2u) {
    transmissionMode = msgContext->ReqData[0];
    numOfDids = msgContext->ReqDataLen - 1u;
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
      id = msgContext->ReqData[i + 1u];
      for (j = 0; j < config->NumOfDIDs; j++) {
        if (config->DIDs[j].DID->Id == ((uint16_t)id + 0xF200u)) {
          rDid = &config->DIDs[j];
          break;
        }
      }
      if (NULL != rDid) {
        totalResLength += rDid->DID->Length + 1u;
        r = Dcm_DslServiceDIDSesSecPhyFuncCheck(context, &rDid->DID->SesSecAccess, nrc);
      } else {
        *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        r = E_NOT_OK;
      }
    }
  }

  if (E_OK == r) {
    if (totalResLength > msgContext->ResMaxDataLen) {
      *nrc = DCM_E_RESPONSE_TOO_LONG;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    for (i = 0u; i < numOfDids; i++) {
      rDid = NULL;
      id = msgContext->ReqData[i + 1u];
      for (j = 0u; j < config->NumOfDIDs; j++) {
        if (config->DIDs[j].DID->Id == ((uint16_t)id + 0xF200u)) {
          rDid = &config->DIDs[j];
          if (0u == rDid->Context->Reload) {
            rDid->Context->OpStatus = DCM_CANCEL;
          }
          rDid->Context->Reload = reload;
          if (DCM_CANCEL == rDid->Context->OpStatus) {
            rDid->Context->Timer = 1;
          }
        }
      }
    }
    /* don't give response here for this service */
    context->MsgContext.MsgAddInfo.SuppressPosResponse = DCM_SUPRESS_POSITIVE_RESPONCE;
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
    for (i = 0; (E_NOT_OK == r) && (i < memoryConfig->NumOfAALFIs); i++) {
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
    for (i = 0u; (E_NOT_OK == r) && (i < memoryConfig->NumOfMems); i++) {
      if ((0u != (attr & memoryConfig->Mems[i].Attr)) &&
          (memoryAddress >= memoryConfig->Mems[i].Low) &&
          ((memoryAddress + memorySize) <= memoryConfig->Mems[i].High)) {
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

  if (msgContext->ReqDataLen > 3u) {
    memorySizeLen = (msgContext->ReqData[0] >> 4) & 0xFu;
    memoryAddressLen = msgContext->ReqData[0] & 0xFu;
    if ((memorySizeLen >= 1u) && (memorySizeLen <= 4u) && (memoryAddressLen >= 1u) &&
        (memoryAddressLen <= 4u)) {
      if ((1u + memoryAddressLen + memorySizeLen) == msgContext->ReqDataLen) {
        memoryAddress = 0;
        for (i = 0; i < memoryAddressLen; i++) {
          memoryAddress = (memoryAddress << 8) + msgContext->ReqData[1u + i];
        }
        memorySize = 0;
        for (i = 0; i < memorySizeLen; i++) {
          memorySize = (memorySize << 8) + msgContext->ReqData[1u + memoryAddressLen + i];
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
    r = Dcm_DspIsMemoryValid(msgContext->ReqData[0], DCM_MEM_ATTR_READ, memoryAddress, memorySize,
                             nrc);
  }

  if (E_OK == r) {
    if (memorySize > msgContext->ResMaxDataLen) {
      *nrc = DCM_E_RESPONSE_TOO_LONG;
    }
  }

  if (E_OK == r) {
    ASLOG(DCM, ("read memoryAddress=0x%X memorySize=0x%X\n", memoryAddress, memorySize));
    /* @SWS_Dcm_91070: MemoryIdentifier is not used, set it to 0x00 */
    rm = Dcm_ReadMemory(context->OpStatus, 0x00, memoryAddress, memorySize, &msgContext->ResData[0],
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
    msgContext->ResDataLen = memorySize;
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

  if (msgContext->ReqDataLen > 3) {
    memorySizeLen = (msgContext->ReqData[0] >> 4) & 0xF;
    memoryAddressLen = msgContext->ReqData[0] & 0xF;
    if ((memorySizeLen >= 1) && (memorySizeLen <= 4) && (memoryAddressLen >= 1) &&
        (memoryAddressLen <= 4)) {
      if ((1 + memoryAddressLen + memorySizeLen) < msgContext->ReqDataLen) {
        memoryAddress = 0;
        for (i = 0; i < memoryAddressLen; i++) {
          memoryAddress = (memoryAddress << 8) + msgContext->ReqData[1 + i];
        }
        memorySize = 0;
        for (i = 0; i < memorySizeLen; i++) {
          memorySize = (memorySize << 8) + msgContext->ReqData[1 + memoryAddressLen + i];
        }
        if ((1 + memoryAddressLen + memorySizeLen + memorySize) == msgContext->ReqDataLen) {
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
    r = Dcm_DspIsMemoryValid(msgContext->ReqData[0], DCM_MEM_ATTR_WRITE, memoryAddress, memorySize,
                             nrc);
  }

  if (E_OK == r) {
    if (memorySize > msgContext->ResMaxDataLen) {
      *nrc = DCM_E_RESPONSE_TOO_LONG;
    }
  }

  if (E_OK == r) {
    ASLOG(DCM, ("write memoryAddress=0x%X memorySize=0x%X\n", memoryAddress, memorySize));
    /* @SWS_Dcm_91070: MemoryIdentifier is not used, set it to 0x00 */
    rwm = Dcm_WriteMemory(context->OpStatus, 0x00, memoryAddress, memorySize,
                          &msgContext->ReqData[1 + memoryAddressLen + memorySizeLen], nrc);
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
    memcpy(msgContext->ResData, msgContext->ReqData, 1 + memoryAddressLen + memorySizeLen);
    msgContext->ResDataLen = 1 + memoryAddressLen + memorySizeLen;
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_DYNAMICALLY_DEFINE_DATA_IDENTIFIER
Std_ReturnType Dcm_DspDynamicallyDefineDataIdentifier(Dcm_MsgContextType *msgContext,
                                                      Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  if (msgContext->ReqDataLen >= 3u) {
    switch (msgContext->ReqData[0]) {
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

Std_ReturnType Dcm_DspReadDDDID(P2CONST(Dcm_DDDIDConfigType, AUTOMATIC, DCM_CONST) dDConfig,
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

  ASLOG(DCM, ("read DDDID=%X length=%d\n", dDConfig->RDID->Id, length));
  for (i = 0; (i < dDConfig->Context->NumOfEntry) && (E_OK == r); i++) {
    entry = &dDConfig->Context->Entry[i];
    rDID = NULL;
    if (entry->Index < config->NumOfRDIDs) {
      rDID = &config->RDIDs[entry->Index];
    }

    if (NULL != rDID) {
      if ((entry->Position >= rDID->Length) ||
          (((uint16_t)entry->Position + entry->Size) > rDID->Length)) {
        ASLOG(DCME,
              ("Fatal Memory Error as invalid position and size for DDDID id = 0x%X entry %d\n",
               dDConfig->RDID->Id, i));
        *nrc = DCM_E_CONDITIONS_NOT_CORRECT;
        r = E_NOT_OK;
      }
    } else {
      ASLOG(DCME, ("Fatal Memory Error as invalid index DDDID id = 0x%X entry %d\n",
                   dDConfig->RDID->Id, i));
      *nrc = DCM_E_CONDITIONS_NOT_CORRECT;
      r = E_NOT_OK;
    }

    if (E_OK == r) {
      if ((DCM_INITIAL == opStatus) || (DCM_CANCEL == opStatus) ||
          (DCM_PENDING == entry->OpStatus)) {
        entry->OpStatus = DCM_CANCEL; /* set to invalid */
        /* NOTE: use the end of TX buffer to saving the whole DID data */
        tmp = &config->TxBuffer[config->TxBufferSize - rDID->Length];
        r = rDID->ReadDIdFnc(opStatus, tmp, rDID->Length, nrc);
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
            entry->OpStatus = DCM_PENDING;
          } else {
            ASLOG(DCM, ("  read entry ID=%X(%d) position=%d size=%d\n", rDID->Id, entry->Index,
                        entry->Position, entry->Size));
            (void)memcpy(&data[offset], &tmp[entry->Position], entry->Size);
          }
        }
      }
      offset += entry->Size;
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
  config = (P2CONST(Dcm_AuthenticationConfigType, AUTOMATIC, DCM_CONST))context->CurService->Config;
  P2CONST(Dcm_AuthenticationType, AUTOMATIC, DCM_CONST)
  auth = NULL;
  uint8_t id;
  uint16_t len;
  uint16_t i;

  if (msgContext->ReqDataLen >= 1) {
    id = msgContext->ReqData[0];
    for (i = 0; i < config->NumOfAuthentications; i++) {
      if (config->Authentications[i].Id == id) {
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
    if (auth->AuthenticationFnc != NULL) {
      len = msgContext->ResMaxDataLen - 1;
      r = auth->AuthenticationFnc(context->OpStatus, &msgContext->ReqData[1],
                                  msgContext->ReqDataLen - 1, &msgContext->ResData[1], &len, nrc);
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    msgContext->ResData[0] = id;
    msgContext->ResDataLen = 1 + len;
  }

  return r;
}

Std_ReturnType Dcm_DspDeAuthentication(Dcm_OpStatusType opStatus, const uint8_t *dataIn,
                                       uint16_t dataInLen, uint8_t *dataOut, uint16_t *dataOutLen,
                                       Dcm_NegativeResponseCodeType *errorCode) {
  Std_ReturnType r = E_NOT_OK;
  return r;
}

Std_ReturnType Dcm_DspVerifyCertificateUnidirectional(Dcm_OpStatusType opStatus,
                                                      const uint8_t *dataIn, uint16_t dataInLen,
                                                      uint8_t *dataOut, uint16_t *dataOutLen,
                                                      Dcm_NegativeResponseCodeType *errorCode) {
  Std_ReturnType r = E_OK;
  uint16_t publicKeyLen = 0;
  const uint8_t *publicKey;
  uint16_t signatureLen = 0;
  const uint8_t *signature;
  if (0x00 != dataIn[0]) {
    r = E_NOT_OK;
    *errorCode = DCM_E_REQUEST_OUT_OF_RANGE;
  }

  if (E_OK == r) {
    publicKey = &dataIn[3];
    publicKeyLen = ((uint16_t)dataIn[1] << 8) + dataIn[2];
    signature = &dataIn[5 + publicKeyLen];
    signatureLen = ((uint16_t)dataIn[3 + publicKeyLen] << 8) + dataIn[4 + publicKeyLen];
    if ((publicKeyLen + signatureLen + 5) != dataInLen) {
      r = E_NOT_OK;
      *errorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
  }

  if (E_OK == r) {
    r = Dcm_AuthenticationVerifyTesterCertificate(publicKey, publicKeyLen, signature, signatureLen,
                                                  errorCode);
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

Std_ReturnType Dcm_DspVerifyCertificateBidirectional(Dcm_OpStatusType opStatus,
                                                     const uint8_t *dataIn, uint16_t dataInLen,
                                                     uint8_t *dataOut, uint16_t *dataOutLen,
                                                     Dcm_NegativeResponseCodeType *errorCode) {
  Std_ReturnType r = E_NOT_OK;
  return r;
}

Std_ReturnType Dcm_DspProofOfOwnership(Dcm_OpStatusType opStatus, const uint8_t *dataIn,
                                       uint16_t dataInLen, uint8_t *dataOut, uint16_t *dataOutLen,
                                       Dcm_NegativeResponseCodeType *errorCode) {
  Std_ReturnType r = E_OK;

  r = Dcm_AuthenticationVerifyProofOfOwnership(dataIn, dataInLen, errorCode);

  if (E_OK == r) {
    dataOut[0] = 0x12;
    *dataOutLen = 1;
  }

  return r;
}

Std_ReturnType Dcm_DspTransmitCertificate(Dcm_OpStatusType opStatus, const uint8_t *dataIn,
                                          uint16_t dataInLen, uint8_t *dataOut,
                                          uint16_t *dataOutLen,
                                          Dcm_NegativeResponseCodeType *errorCode) {
  Std_ReturnType r = E_NOT_OK;
  return r;
}

Std_ReturnType Dcm_DspRequestChallengeForAuthentication(Dcm_OpStatusType opStatus,
                                                        const uint8_t *dataIn, uint16_t dataInLen,
                                                        uint8_t *dataOut, uint16_t *dataOutLen,
                                                        Dcm_NegativeResponseCodeType *errorCode) {
  Std_ReturnType r = E_NOT_OK;
  return r;
}

Std_ReturnType Dcm_DspVerifyProofOfOwnershipUnidirectional(
  Dcm_OpStatusType opStatus, const uint8_t *dataIn, uint16_t dataInLen, uint8_t *dataOut,
  uint16_t *dataOutLen, Dcm_NegativeResponseCodeType *errorCode) {
  Std_ReturnType r = E_NOT_OK;
  return r;
}

Std_ReturnType Dcm_DspVerifyProofOfOwnershipBidirectional(Dcm_OpStatusType opStatus,
                                                          const uint8_t *dataIn, uint16_t dataInLen,
                                                          uint8_t *dataOut, uint16_t *dataOutLen,
                                                          Dcm_NegativeResponseCodeType *errorCode) {
  Std_ReturnType r = E_NOT_OK;
  return r;
}

Std_ReturnType Dcm_DspAuthenticationConfiguration(Dcm_OpStatusType opStatus, const uint8_t *dataIn,
                                                  uint16_t dataInLen, uint8_t *dataOut,
                                                  uint16_t *dataOutLen,
                                                  Dcm_NegativeResponseCodeType *errorCode) {
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
#ifdef DCM_USE_SERVICE_ROUTINE_CONTROL
  Dcm_RoutineControl_Init();
#endif
}
