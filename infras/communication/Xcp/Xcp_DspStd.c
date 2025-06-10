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

  if (msgContext->reqDataLen >= 1) {
    ret = serCfg->GetConnectPermissionFnc(msgContext->reqData[0], nrc);
    if (E_OK == ret) {
      msgContext->resData[0] = config->resource;
      /* report communication mode info. only byte granularity is supported */
      msgContext->resData[1] = 0;
      if (0xde == (*(uint8_t *)&endianMask)) { /* big endian */
        msgContext->resData[1] |= 0x01;
      }
      /* report max CTO data length */
      msgContext->resData[2] = XCP_PATCKET_MAX_SIZE;
      /* report max DTO data length */
      msgContext->resData[3] = 0;
      msgContext->resData[4] = XCP_PATCKET_MAX_SIZE;
      /* report msb of protocol layer version number */
      msgContext->resData[5] = 1;
      /* report msb of transport layer version number */
      msgContext->resData[6] = 1;
      msgContext->resDataLen = 7;
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
  context->activeChannel = XCP_INVALID_CHL;
  context->lockedResource = XCP_RES_MASK; /* lock all resource again */
  context->requestResource = 0;
#ifdef XCP_USE_SERVICE_PGM_PROGRAM_START
  context->pgmFlags = 0;
#endif
  return ret;
}

Std_ReturnType Xcp_DspGetSeed(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  P2CONST(Xcp_GetSeedConfigType, AUTOMATIC, XCP_CONST)
  serCfg = (P2CONST(Xcp_GetSeedConfigType, AUTOMATIC, XCP_CONST))Xcp_GetCurServiceConfig();
  uint16_t seedLen = msgContext->resDataLen - 1;

  if (msgContext->reqDataLen >= 2) {
    if ((0 != msgContext->reqData[0])) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else if ((0 == (msgContext->reqData[1] & XCP_RES_MASK)) ||
               (0 != (msgContext->reqData[1] & (~XCP_RES_MASK)))) {
      ret = E_NOT_OK;
      *nrc = XCP_E_CMD_SYNTAX;
    } else {
      ret = serCfg->GetSeedFnc(msgContext->reqData[0], msgContext->reqData[1],
                               &msgContext->resData[1], &seedLen, nrc);
      if (E_OK == ret) {
        msgContext->resData[0] = seedLen;
        msgContext->resDataLen = seedLen + 1;
        context->requestResource = msgContext->reqData[1];
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

  if (msgContext->reqDataLen < 2) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else if (0 == context->requestResource) {
    ret = E_NOT_OK;
    *nrc = XCP_E_SEQUENCE;
  } else {
    keyLen = msgContext->reqData[0];
    if ((keyLen + 1) <= msgContext->reqDataLen) {
      ret = serCfg->CompareKeyFnc(&msgContext->reqData[1], keyLen, nrc);
      if (E_OK == ret) {
        context->lockedResource &= ~context->requestResource;
        context->requestResource = 0;
        msgContext->resData[0] = context->lockedResource;
        msgContext->resDataLen = 1;
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

  msgContext->resData[0] = 0;                       /* Session Status */
  msgContext->resData[1] = context->lockedResource; /* Protect */
  msgContext->resData[2] = 0;                       /* Reserved */
  msgContext->resData[3] = 0;                       /* Session Configuration ID */
  msgContext->resData[4] = 0;
  msgContext->resDataLen = 5;
  return ret;
}

Std_ReturnType Xcp_DspSetMTA(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  Xcp_MtaContextType *mta = (Xcp_MtaContextType *)context->cache;

  asAssert(sizeof(Xcp_MtaContextType) <= sizeof(context->cache));

  if (msgContext->reqDataLen < 7) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    mta->extension = msgContext->reqData[2];
    mta->address = Xcp_GetU32(&msgContext->reqData[3]);
    context->cacheOwner = XCP_PID_CMD_STD_SET_MTA;
  }

  return ret;
}

Std_ReturnType Xcp_DspUpload(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  Xcp_MtaContextType *mta = (Xcp_MtaContextType *)context->cache;
  uint8_t length = 0;
  uint8_t doSz = 0;
  uint8_t offset = 0;

  asAssert(sizeof(Xcp_MtaContextType) <= sizeof(context->cache));

  if (msgContext->reqDataLen < 1) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else if (XCP_PID_CMD_STD_SET_MTA != context->cacheOwner) {
    ret = E_NOT_OK;
    *nrc = XCP_E_SEQUENCE;
  } else {
    length = msgContext->reqData[0];
    if (XCP_INITIAL == msgContext->opStatus) {
      /* this used to be as offset, as we know packet size >= 8 for sure */
      msgContext->reqData[3] = 0;
    }
    offset = msgContext->reqData[3];
    if ((length - offset) > msgContext->resMaxDataLen) {
      doSz = msgContext->resMaxDataLen;
    } else {
      doSz = (length - offset);
    }
    ret = Xcp_MtaRead(mta->extension, mta->address + offset, msgContext->resData, doSz, nrc);
    if (E_OK == ret) {
      offset += doSz;
      if (offset >= length) {
        ret = E_OK;
      } else {
        ret = XCP_E_PENDING;
      }
      msgContext->reqData[3] = offset;
      msgContext->resDataLen = doSz;
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
  uint8_t *offset = (uint8_t *)context->cache;

  if (msgContext->reqDataLen < 7) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    length = msgContext->reqData[0];
    extension = msgContext->reqData[2];
    address = Xcp_GetU32(&msgContext->reqData[3]);
    if (XCP_INITIAL == msgContext->opStatus) {
      *offset = 0; /* this used to be as offset */
      context->cacheOwner = XCP_PID_CMD_STD_SHORT_UPLOAD;
    }
    if ((length - *offset) > msgContext->resMaxDataLen) {
      doSz = msgContext->resMaxDataLen;
    } else {
      doSz = (length - *offset);
    }
    ret = Xcp_MtaRead(extension, address + *offset, msgContext->resData, doSz, nrc);
    if (E_OK == ret) {
      *offset += doSz;
      if (*offset >= length) {
        ret = E_OK;
      } else {
        ret = XCP_E_PENDING;
      }
      msgContext->resDataLen = doSz;
    }
  }

  return ret;
}
