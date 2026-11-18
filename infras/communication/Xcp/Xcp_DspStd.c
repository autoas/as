/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Module XCP AUTOSAR CP R23-11
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Xcp.h"
#include "Xcp_Cfg.h"
#include "Xcp_Priv.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType Xcp_DspConnect(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  P2CONST(Xcp_ConnectConfigType, AUTOMATIC, XCP_CONST)
  serCfg = (P2CONST(Xcp_ConnectConfigType, AUTOMATIC, XCP_CONST))Xcp_GetCurServiceConfig();
  uint32 endianMask = 0xdeadbeef;

  if (msgContext->ReqDataLen >= 1) {
    ret = serCfg->GetConnectPermissionFnc(msgContext->ReqData[0], nrc);
    if (E_OK == ret) {
      msgContext->ResData[0] = config->Resource;
      /* report communication mode info. only byte granularity is supported */
      msgContext->ResData[1] = 0;
      if (0xde == (*(uint8_t *)&endianMask)) { /* big endian */
        msgContext->ResData[1] |= 0x01;
      }
      /* report max CTO data length */
      msgContext->ResData[2] = XCP_PATCKET_MAX_SIZE;
      /* report max DTO data length */
      msgContext->ResData[3] = 0;
      msgContext->ResData[4] = XCP_PATCKET_MAX_SIZE;
      /* report msb of protocol layer version number */
      msgContext->ResData[5] = 1;
      /* report msb of transport layer version number */
      msgContext->ResData[6] = 1;
      msgContext->ResDataLen = 7;
    }
  } else {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  }

  return ret;
}

Std_ReturnType Xcp_DspDisconnect(Xcp_MsgContextType *msgContext,
                                 Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  P2CONST(Xcp_DisconnectConfigType, AUTOMATIC, XCP_CONST)
  serCfg = (P2CONST(Xcp_DisconnectConfigType, AUTOMATIC, XCP_CONST))Xcp_GetCurServiceConfig();

  (void)serCfg->DisconnectFnc();
  context->ActiveChannel = XCP_INVALID_CHL;
  context->LockedResource = XCP_RES_MASK; /* lock all resource again */
  context->RequestResource = 0;
#ifdef XCP_USE_SERVICE_PGM_PROGRAM_START
  context->PgmFlags = 0;
#endif
  return ret;
}

Std_ReturnType Xcp_DspGetSeed(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  P2CONST(Xcp_GetSeedConfigType, AUTOMATIC, XCP_CONST)
  serCfg = (P2CONST(Xcp_GetSeedConfigType, AUTOMATIC, XCP_CONST))Xcp_GetCurServiceConfig();
  uint16_t seedLen = msgContext->ResDataLen - 1;

  if (msgContext->ReqDataLen >= 2) {
    if ((0 != msgContext->ReqData[0])) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else if ((0 == (msgContext->ReqData[1] & XCP_RES_MASK)) ||
               (0 != (msgContext->ReqData[1] & (~XCP_RES_MASK)))) {
      ret = E_NOT_OK;
      *nrc = XCP_E_CMD_SYNTAX;
    } else {
      ret = serCfg->GetSeedFnc(msgContext->ReqData[0], msgContext->ReqData[1],
                               &msgContext->ResData[1], &seedLen, nrc);
      if (E_OK == ret) {
        msgContext->ResData[0] = seedLen;
        msgContext->ResDataLen = seedLen + 1;
        context->RequestResource = msgContext->ReqData[1];
      }
    }
  } else {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  }

  return ret;
}

Std_ReturnType Xcp_DspUnlock(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  P2CONST(Xcp_UnlockConfigType, AUTOMATIC, XCP_CONST)
  serCfg = (P2CONST(Xcp_UnlockConfigType, AUTOMATIC, XCP_CONST))Xcp_GetCurServiceConfig();
  uint16_t keyLen;

  if (msgContext->ReqDataLen < 2) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else if (0 == context->RequestResource) {
    ret = E_NOT_OK;
    *nrc = XCP_E_SEQUENCE;
  } else {
    keyLen = msgContext->ReqData[0];
    if ((keyLen + 1) <= msgContext->ReqDataLen) {
      ret = serCfg->CompareKeyFnc(&msgContext->ReqData[1], keyLen, nrc);
      if (E_OK == ret) {
        context->LockedResource &= ~context->RequestResource;
        context->RequestResource = 0;
        msgContext->ResData[0] = context->LockedResource;
        msgContext->ResDataLen = 1;
      }
    } else {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspGetStatus(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();

  msgContext->ResData[0] = 0;                       /* Session Status */
  msgContext->ResData[1] = context->LockedResource; /* Protect */
  msgContext->ResData[2] = 0;                       /* Reserved */
  msgContext->ResData[3] = 0;                       /* Session Configuration ID */
  msgContext->ResData[4] = 0;
  msgContext->ResDataLen = 5;
  return ret;
}

Std_ReturnType Xcp_DspSetMTA(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  Xcp_MtaContextType *mta = (Xcp_MtaContextType *)context->Cache;

  asAssert(sizeof(Xcp_MtaContextType) <= sizeof(context->Cache));

  if (msgContext->ReqDataLen < 7) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    mta->Extension = msgContext->ReqData[2];
    mta->Address = Xcp_GetU32(&msgContext->ReqData[3]);
    context->CacheOwner = XCP_PID_CMD_STD_SET_MTA;
  }

  return ret;
}

Std_ReturnType Xcp_DspUpload(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  Xcp_MtaContextType *mta = (Xcp_MtaContextType *)context->Cache;
  uint8_t length = 0;
  uint8_t doSz = 0;
  uint8_t offset = 0;

  asAssert(sizeof(Xcp_MtaContextType) <= sizeof(context->Cache));

  if (msgContext->ReqDataLen < 1) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else if (XCP_PID_CMD_STD_SET_MTA != context->CacheOwner) {
    ret = E_NOT_OK;
    *nrc = XCP_E_SEQUENCE;
  } else {
    length = msgContext->ReqData[0];
    if (XCP_INITIAL == msgContext->OpStatus) {
      /* this used to be as offset, as we know packet size >= 8 for sure */
      msgContext->ReqData[3] = 0;
    }
    offset = msgContext->ReqData[3];
    if ((length - offset) > msgContext->ResMaxDataLen) {
      doSz = msgContext->ResMaxDataLen;
    } else {
      doSz = (length - offset);
    }
    ret = Xcp_MtaRead(mta->Extension, mta->Address + offset, msgContext->ResData, doSz, nrc);
    if (E_OK == ret) {
      offset += doSz;
      if (offset >= length) {
        ret = E_OK;
      } else {
        ret = XCP_E_PENDING;
      }
      msgContext->ReqData[3] = offset;
      msgContext->ResDataLen = doSz;
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspShortUpload(Xcp_MsgContextType *msgContext,
                                  Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  uint8_t length = 0;
  uint8_t extension;
  uint32_t address;
  uint8_t doSz = 0;
  uint8_t *offset = (uint8_t *)context->Cache;

  if (msgContext->ReqDataLen < 7) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    length = msgContext->ReqData[0];
    extension = msgContext->ReqData[2];
    address = Xcp_GetU32(&msgContext->ReqData[3]);
    if (XCP_INITIAL == msgContext->OpStatus) {
      *offset = 0; /* this used to be as offset */
      context->CacheOwner = XCP_PID_CMD_STD_SHORT_UPLOAD;
    }
    if ((length - *offset) > msgContext->ResMaxDataLen) {
      doSz = msgContext->ResMaxDataLen;
    } else {
      doSz = (length - *offset);
    }
    ret = Xcp_MtaRead(extension, address + *offset, msgContext->ResData, doSz, nrc);
    if (E_OK == ret) {
      *offset += doSz;
      if (*offset >= length) {
        ret = E_OK;
      } else {
        ret = XCP_E_PENDING;
      }
      msgContext->ResDataLen = doSz;
    }
  }

  return ret;
}
