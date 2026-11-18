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
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType Xcp_DspDownload(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  Xcp_DownloadContextType *dnCtx = (Xcp_DownloadContextType *)context->Cache;
  uint8_t length = 0;

  if (msgContext->ReqDataLen < 2) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else if (XCP_PID_CMD_STD_SET_MTA != context->CacheOwner) {
    ret = E_NOT_OK;
    *nrc = XCP_E_SEQUENCE;
  } else {
    length = msgContext->ReqData[0];
    if (XCP_INITIAL == msgContext->OpStatus) {
      dnCtx->Length = length;
      dnCtx->Offset = 0;
      context->CacheOwner = XCP_PID_CMD_CAL_DOWNLOAD;
    }
    if (length > (msgContext->ReqDataLen - 1)) {
      length = msgContext->ReqDataLen - 1;
    }
    ret = Xcp_MtaWrite(dnCtx->Extension, dnCtx->Address, &msgContext->ReqData[1], length, nrc);
    if (E_OK == ret) {
      dnCtx->Offset += length;
      if (dnCtx->Offset >= dnCtx->Length) {
        ret = E_OK;
      } else {
        ret = XCP_E_OK_NO_RES; /* Also OK but expecting Download Next */
      }
      msgContext->ResDataLen = 0;
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspDownloadNext(Xcp_MsgContextType *msgContext,
                                   Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  Xcp_DownloadContextType *dnCtx = (Xcp_DownloadContextType *)context->Cache;
  uint8_t length = 0;

  if (msgContext->ReqDataLen < 2) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else if (XCP_PID_CMD_CAL_DOWNLOAD != context->CacheOwner) {
    ret = E_NOT_OK;
    *nrc = XCP_E_SEQUENCE;
  } else {
    length = msgContext->ReqData[0];
    if (length == (msgContext->ReqDataLen - 1)) {
      if (length <= (dnCtx->Length - dnCtx->Offset)) {
        ret = Xcp_MtaWrite(dnCtx->Extension, dnCtx->Address + dnCtx->Offset,
                           &msgContext->ReqData[1], length, nrc);
        if (E_OK == ret) {
          dnCtx->Offset += length;
          if (dnCtx->Offset >= dnCtx->Length) {
            ret = E_OK;
          } else {
            ret = XCP_E_OK_NO_RES; /* Also OK but expecting Download Next */
          }
          msgContext->ResDataLen = 0;
        }
      } else {
        /* given too much data */
        ret = E_NOT_OK;
        *nrc = XCP_E_SEQUENCE;
      }
    } else {
      ret = E_NOT_OK;
      *nrc = XCP_E_CMD_SYNTAX;
    }
  }

  return ret;
}
