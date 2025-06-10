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
#define XCP_PGM_MASTER_BLOCK_MODE 0x01
#define XCP_PGM_INTERLEAVED_MODE 0x02
#define XCP_PGM_SLAVE_BLOCK_MODE 0x40

#define XCP_PGM_FUNCTIONAL_ACCESS_MODE 0x01
#define XCP_PGM_ABSOLUTE_ACCESS_MODE 0x00

#define XCP_PGM_CLEAR_ALL_CAL_DATA 0x00000001UL
#define XCP_PGM_CLEAR_ALL_CODE 0x00000002UL
#define XCP_PGM_CLEAR_ALL_NVM 0x00000004UL

#define XCP_PGM_FLAG_STARTED 0x01
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static Std_ReturnType Xcp_FunctionalProgramClear(uint32_t range, uint8_t *nrc) {
  Std_ReturnType ret = E_OK;

  if (0 != (range & XCP_PGM_CLEAR_ALL_CAL_DATA)) {
    /* clear all calibration data areas */
  }
  if ((E_OK == ret) && (0 != (range & XCP_PGM_CLEAR_ALL_CODE))) {
    /* clear all code areas, except boot */
  }
  if ((E_OK == ret) && (0 != (range & XCP_PGM_CLEAR_ALL_NVM))) {
    /* clear all code NVRAM areas */
  }

  return ret;
}

static Std_ReturnType Xcp_AbsoluteProgramClear(uint8_t extension, uint32_t address, uint32_t range,
                                               uint8_t *nrc) {
  Std_ReturnType ret = E_OK;

  return ret;
}
/* ================================ [ FUNCTIONS ] ============================================== */
#ifdef XCP_USE_SERVICE_PGM_PROGRAM_START
Std_ReturnType Xcp_DspProgramStart(Xcp_MsgContextType *msgContext,
                                   Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  P2CONST(Xcp_ProgramStartConfigType, AUTOMATIC, XCP_CONST)
  serCfg = (P2CONST(Xcp_ProgramStartConfigType, AUTOMATIC, XCP_CONST))Xcp_GetCurServiceConfig();

  ret = serCfg->GetProgramStartPermissionFnc(msgContext->opStatus, nrc);

  if (E_OK == ret) {
    msgContext->resData[0] = 0; /* Reserved */
    /* no special communication mode supported during programming */
    msgContext->resData[1] = 0;                    /* COMM_MODE_PGM */
    msgContext->resData[2] = XCP_PATCKET_MAX_SIZE; /* MAX_CTO_PGM */
    msgContext->resData[3] = serCfg->maxBs;        /* MAX_BS_PGM */
    msgContext->resData[4] = serCfg->STmin;        /* MIN_ST_PGM */
    msgContext->resData[5] = serCfg->queSz;        /* QUEUE_SIZE */
    msgContext->resDataLen = 6;
    context->pgmFlags |= XCP_PGM_FLAG_STARTED;
  }

  return ret;
}
#endif

Std_ReturnType Xcp_DspProgramClear(Xcp_MsgContextType *msgContext,
                                   Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  Xcp_MtaContextType *mtaCtx = (Xcp_MtaContextType *)context->cache;
  uint8_t mode = 0;
  uint32_t range = 0;

  if (msgContext->reqDataLen < 7) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else if (0 == (XCP_PGM_FLAG_STARTED & context->pgmFlags)) {
    ret = E_NOT_OK;
    *nrc = XCP_E_SEQUENCE;
  } else {
    mode = msgContext->reqData[0];
    range = Xcp_GetU32(&msgContext->reqData[3]);
    if (XCP_PGM_FUNCTIONAL_ACCESS_MODE == mode) {
      ret = Xcp_FunctionalProgramClear(range, nrc);
    } else if (XCP_PGM_ABSOLUTE_ACCESS_MODE == mode) {
      if (XCP_PID_CMD_STD_SET_MTA != context->cacheOwner) {
        ret = E_NOT_OK;
        *nrc = XCP_E_SEQUENCE;
      } else {
        ret = Xcp_AbsoluteProgramClear(mtaCtx->extension, mtaCtx->address, range, nrc);
      }
    } else {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspProgram(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  Xcp_ProgramContextType *pgmCtx = (Xcp_ProgramContextType *)context->cache;
  uint8_t length = 0;

  if (msgContext->reqDataLen < 2) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else if (0 == (XCP_PGM_FLAG_STARTED & context->pgmFlags)) {
    ret = E_NOT_OK;
    *nrc = XCP_E_SEQUENCE;
  } else if (XCP_PID_CMD_STD_SET_MTA != context->cacheOwner) {
    ret = E_NOT_OK;
    *nrc = XCP_E_SEQUENCE;
  } else {
    length = msgContext->reqData[0];
    if (XCP_INITIAL == msgContext->opStatus) {
      pgmCtx->length = length;
      pgmCtx->offset = 0;
      context->cacheOwner = XCP_PID_CMD_PGM_PROGRAM;
    }
    if (length > (msgContext->reqDataLen - 1)) {
      length = msgContext->reqDataLen - 1;
    }
    ret = Xcp_MtaWrite(pgmCtx->extension, pgmCtx->address, &msgContext->reqData[1], length, nrc);
    if (E_OK == ret) {
      pgmCtx->offset += length;
      if (pgmCtx->offset >= pgmCtx->length) {
        ret = E_OK;
      } else {
        ret = XCP_E_OK_NO_RES; /* Also OK but expecting Program Next */
      }
      msgContext->resDataLen = 0;
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspProgramNext(Xcp_MsgContextType *msgContext,
                                  Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  Xcp_ProgramContextType *pgmCtx = (Xcp_ProgramContextType *)context->cache;
  uint8_t length = 0;

  if (msgContext->reqDataLen < 2) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else if (0 == (XCP_PGM_FLAG_STARTED & context->pgmFlags)) {
    ret = E_NOT_OK;
    *nrc = XCP_E_SEQUENCE;
  } else if (XCP_PID_CMD_PGM_PROGRAM != context->cacheOwner) {
    ret = E_NOT_OK;
    *nrc = XCP_E_SEQUENCE;
  } else {
    length = msgContext->reqData[0];
    if (length == (msgContext->reqDataLen - 1)) {
      if (length <= (pgmCtx->length - pgmCtx->offset)) {
        ret = Xcp_MtaWrite(pgmCtx->extension, pgmCtx->address + pgmCtx->offset,
                           &msgContext->reqData[1], length, nrc);
        if (E_OK == ret) {
          pgmCtx->offset += length;
          if (pgmCtx->offset >= pgmCtx->length) {
            ret = E_OK;
          } else {
            ret = XCP_E_OK_NO_RES; /* Also OK but expecting Program Next */
          }
          msgContext->resDataLen = 0;
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

#ifdef XCP_USE_SERVICE_PGM_PROGRAM_RESET
Std_ReturnType Xcp_DspProgramReset(Xcp_MsgContextType *msgContext,
                                   Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  Xcp_ContextType *context = Xcp_GetContext();
  P2CONST(Xcp_ProgramResetConfigType, AUTOMATIC, XCP_CONST)
  serCfg = (P2CONST(Xcp_ProgramResetConfigType, AUTOMATIC, XCP_CONST))Xcp_GetCurServiceConfig();

  ret = serCfg->GetProgramResetPermissionFnc(msgContext->opStatus, nrc);

  if (E_OK == ret) {
#ifdef XCP_USE_SERVICE_PGM_PROGRAM_START
    context->pgmFlags = 0;
#endif
    context->timer2Reset = serCfg->delay;
  }

  return ret;
}
#endif
