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
#include "mempool.h"
#include "CanIf.h"
#include <string.h>
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DAQ 1
#define AS_LOG_DAQE 3

#define XCP_DAQ_STOP 0
#define XCP_DAQ_START 1
#define XCP_DAQ_SELECT 2

#define XCP_DAQ_STOP_ALL 0
#define XCP_DAQ_START_SELECT 1
#define XCP_DAQ_STOP_SELECT 2

#define XCP_DAQ_LIST_MODE_DIRECTION 0x01
#define XCP_DAQ_LIST_MODE_TIMESTAMP 0x10
#define XCP_DAQ_LIST_MODE_PID_OFF 0x20

#define XCP_DAQ_CONFIG_TYPE_STATIC 0x00
#define XCP_DAQ_CONFIG_TYPE_DYNAMIC 0x01
#define XCP_DAQ_PRESCALER_SUPPORTED 0x02
#define XCP_DAQ_RESUME_SUPPORTED 0x04
#define XCP_DAQ_BIT_STIM_SUPPORTED 0x08
#define XCP_DAQ_TIMESTAMP_SUPPORTED 0x10
#define XCP_DAQ_PID_OFF_SUPPORTED 0x20
#define XCP_DAQ_OVERLOAD_EVENT 0x40
#define XCP_DAQ_OVERLOAD_MSB 0x80

/* PROPERTIES */
#define XCP_DAQ_LIST_PREDEFINED 0x01
#define XCP_DAQ_LIST_EVENT_FIXED 0x02
#define XCP_DAQ_LIST_DAQ 0x04
#define XCP_DAQ_LIST_STIM 0x08

#define XCP_DAQ_LIST_ALLOCATED 0xFF
#define XCP_ODT_ALLOCATED 0xFF
#define XCP_ODT_ENTRY_ALLOCATED 0xFF

#ifndef XCP_MAX_ODT_ENTRY_SIZE_DAQ
#define XCP_MAX_ODT_ENTRY_SIZE_DAQ 7
#endif

#define XCP_INVALID_DAQ_NUMBER 0xFFFFU

#define XCP_INVALID_ODT_PID 0xFFU

#define XCP_MAX_ODT_NUMBER 252

#define XCP_MAX_DAQ (config->numOfDaqList)
#define XCP_MIN_DAQ (config->numOfStaticDaqList)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static Xcp_OdtType *Xcp_AllocODT(uint16_t odtCount) {
  uint16_t index = 0;
  Xcp_OdtType *pOdts = NULL;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();

  for (index = 0; index < config->numOfDynOdtSlots; index++) {
    if (0 == config->dynOdtSlots[index].EntryMaxSize) {
      break;
    }
  }

  if ((config->numOfDynOdtSlots - index) >= odtCount) {
    pOdts = &config->dynOdtSlots[index];
    ASLOG(DAQ, ("Alloc %d ODT from %d\n", (int)odtCount, (int)index));
    for (index = 0; index < odtCount; index++) {
      pOdts[index].EntryMaxSize = XCP_ODT_ALLOCATED;
    }
  }

  return pOdts;
}

static Xcp_OdtEntryType *Xcp_AllocODTEntry(uint16_t odtEntryCount) {
  uint16_t index = 0;
  Xcp_OdtEntryType *pEntries = NULL;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();

  for (index = 0; index < config->numOfDynOdtEntrySlots; index++) {
    if (0 == config->dynOdtEntrySlots[index].Length) {
      break;
    }
  }

  if ((config->numOfDynOdtEntrySlots - index) >= odtEntryCount) {
    pEntries = &config->dynOdtEntrySlots[index];
    ASLOG(DAQ, ("Alloc %d ODT Entry from %d\n", (int)odtEntryCount, (int)index));
    for (index = 0; index < odtEntryCount; index++) {
      pEntries[index].Length = XCP_ODT_ENTRY_ALLOCATED;
    }
  }

  return pEntries;
}

static Std_ReturnType Xcp_IsDaqListConfigured(const Xcp_DaqListType *daqList) {
  Std_ReturnType ret = E_OK;
  Xcp_OdtEntryType *pEntries = NULL;
  uint16_t i;
  uint16_t j;

  if (NULL == daqList->Odts) {
    ret = E_NOT_OK;
  } else {
    for (i = 0; (i < daqList->MaxOdt) && (E_OK == ret); i++) {
      if (XCP_ODT_ALLOCATED == daqList->Odts[i].EntryMaxSize) {
        ret = E_NOT_OK;
      } else {
        pEntries = daqList->Odts[i].OdtEntries;
        for (j = 0; (j < daqList->Odts[i].EntryMaxSize) && (E_OK == ret); j++) {
          if (XCP_ODT_ENTRY_ALLOCATED == pEntries[j].Length) {
            ret = E_NOT_OK;
          }
        }
      }
    }
  }

  return ret;
}

static Std_ReturnType Xcp_IsTheDaqPtrWriteValid(uint16_t daqListNumber, uint8_t odtNumber,
                                                uint8_t odtEntryNumber,
                                                Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Xcp_DaqListType *pDaq;
  if (daqListNumber >= XCP_MAX_DAQ) {
    ret = E_NOT_OK;
    *nrc = XCP_E_OUT_OF_RANGE;
  } else if (daqListNumber < XCP_MIN_DAQ) {
    ret = E_NOT_OK;
    *nrc = XCP_E_WRITE_PROTECTED;
  } else {
    pDaq = (Xcp_DaqListType *)config->daqList[daqListNumber];
    if (NULL == pDaq->Odts) {
      /* ODT not allocated */
      ret = E_NOT_OK;
      *nrc = XCP_E_SEQUENCE;
    } else if (odtNumber >= pDaq->MaxOdt) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else if (NULL == pDaq->Odts[odtNumber].OdtEntries) {
      /* ODT entries not allocated */
      ret = E_NOT_OK;
      *nrc = XCP_E_SEQUENCE;
    } else if (odtEntryNumber >= pDaq->Odts[odtNumber].EntryMaxSize) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else {
      /* VALID */
    }
  }
  return ret;
}

static Std_ReturnType Xcp_IsTheDaqPtrReadValid(uint16_t daqListNumber, uint8_t odtNumber,
                                               uint8_t odtEntryNumber,
                                               Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Xcp_DaqListType *pDaq;
  if (daqListNumber >= XCP_MAX_DAQ) {
    ret = E_NOT_OK;
    *nrc = XCP_E_OUT_OF_RANGE;
  } else {
    pDaq = (Xcp_DaqListType *)config->daqList[daqListNumber];
    if (NULL == pDaq->Odts) {
      /* ODT not allocated */
      ret = E_NOT_OK;
      *nrc = XCP_E_SEQUENCE;
    } else if (odtNumber >= pDaq->MaxOdt) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else if (NULL == pDaq->Odts[odtNumber].OdtEntries) {
      /* ODT entries not allocated */
      ret = E_NOT_OK;
      *nrc = XCP_E_SEQUENCE;
    } else if (odtEntryNumber >= pDaq->Odts[odtNumber].EntryMaxSize) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else {
      /* VALID */
    }
  }
  return ret;
}

static void Xcp_MainFunction_DaqDriveTx(uint16_t daqListNumber, uint8_t pidBase) {
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  const Xcp_DaqListType *pDaq = config->daqList[daqListNumber];
  Xcp_DaqListContextType *pDaqCtx = &config->daqContexts[daqListNumber];
  uint8_t data[XCP_PATCKET_MAX_SIZE];
  PduInfoType PduInfo = {data, NULL, sizeof(data)};
  uint8_t offset = 1;
  uint8_t i;
  uint8_t nrc = 0;
  Std_ReturnType ret = E_OK;

  data[0] = pidBase + pDaqCtx->curPid;
  for (i = 0; (i < pDaq->Odts[pDaqCtx->curPid].EntryMaxSize) && (E_OK == ret); i++) {
    if ((XCP_PATCKET_MAX_SIZE - offset) >= pDaq->Odts[pDaqCtx->curPid].OdtEntries[i].Length) {
      ret = Xcp_MtaRead(pDaq->Odts[pDaqCtx->curPid].OdtEntries[i].Extension,
                        pDaq->Odts[pDaqCtx->curPid].OdtEntries[i].Address, &data[offset],
                        pDaq->Odts[pDaqCtx->curPid].OdtEntries[i].Length, &nrc);
      if (E_OK == ret) {
        offset += pDaq->Odts[pDaqCtx->curPid].OdtEntries[i].Length;
      } else {
        ASLOG(DAQE,
              ("DAQ %d read for PID %d Entry %d failed\n", daqListNumber, pDaqCtx->curPid, i));
        pDaqCtx->curPid = XCP_INVALID_ODT_PID;
        pDaqCtx->timer = 0; /* stop it */
      }
    } else {
      ASLOG(DAQE, ("DAQ %d buffer overflow\n", daqListNumber));
      pDaqCtx->curPid = XCP_INVALID_ODT_PID;
      pDaqCtx->timer = 0; /* stop it */
    }
  }

  if (E_OK == ret) {
    PduInfo.SduLength = offset;
    ret = CanIf_Transmit(config->CanIfTxPduId, &PduInfo);
    if (E_OK == ret) {
      pDaqCtx->curPid++;
    }
  }
}

static void Xcp_MainFunction_DaqDriveTimer(uint16_t daqListNumber) {
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Xcp_DaqListContextType *pDaqCtx = &config->daqContexts[daqListNumber];
  if (pDaqCtx->timer > 0) {
    pDaqCtx->timer--;
    if (0 == pDaqCtx->timer) {
      ASLOG(DAQ, ("DAQ %d trigger!\n", daqListNumber));
      pDaqCtx->curPid = 0;
      asAssert(pDaqCtx->evChl < config->numOfEvChls);
      pDaqCtx->timer = config->evChls[pDaqCtx->evChl].TimeCycle * pDaqCtx->prescaler;
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Xcp_DspDaqInit(void) {
  uint16_t i;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Xcp_DaqListType *pDaq;
  memset(config->dynOdtSlots, 0, sizeof(Xcp_OdtType) * config->numOfDynOdtSlots);
  memset(config->dynOdtEntrySlots, 0, sizeof(Xcp_OdtEntryType) * config->numOfDynOdtEntrySlots);
  memset(config->daqContexts, 0, sizeof(Xcp_DaqListContextType) * XCP_MAX_DAQ);
  for (i = XCP_MIN_DAQ; i < XCP_MAX_DAQ; i++) {
    pDaq = (Xcp_DaqListType *)config->daqList[i];
    memset(pDaq, 0, sizeof(Xcp_DaqListType));
  }
  for (i = 0; i < XCP_MAX_DAQ; i++) {
    config->daqContexts[i].evChl = config->numOfEvChls; /* mark as invalid */
    config->daqContexts[i].curPid = XCP_INVALID_ODT_PID;
  }
}

Std_ReturnType Xcp_DspClearDAQList(Xcp_MsgContextType *msgContext,
                                   Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  uint16_t daqListNumber;
  Xcp_DaqListType *pDaq;
  Xcp_OdtEntryType *pEntries = NULL;
  uint16_t i;
  uint16_t j;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();

  if (msgContext->reqDataLen < 3) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    daqListNumber = Xcp_GetU16(&msgContext->reqData[1]);
    if (daqListNumber >= XCP_MAX_DAQ) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else {
      pDaq = (Xcp_DaqListType *)config->daqList[daqListNumber];
      if (NULL == pDaq->Odts) {
        ret = E_NOT_OK;
        *nrc = XCP_E_OUT_OF_RANGE;
      } else {
        for (i = 0; (i < pDaq->MaxOdt) && (E_OK == ret); i++) {
          if (XCP_ODT_ALLOCATED == pDaq->Odts[i].EntryMaxSize) {
            ret = E_NOT_OK;
            *nrc = XCP_E_OUT_OF_RANGE;
          } else {
            if (daqListNumber >= XCP_MIN_DAQ) {
              pEntries = pDaq->Odts[i].OdtEntries;
              for (j = 0; j < pDaq->Odts[i].EntryMaxSize; j++) {
                pEntries[j].Length = XCP_ODT_ENTRY_ALLOCATED;
              }
            }
            config->daqContexts[daqListNumber].state = 0;
          }
        }
      }
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspStartStopDAQList(Xcp_MsgContextType *msgContext,
                                       Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  uint8_t mode;
  uint16_t daqListNumber;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Xcp_DaqListType *pDaq;
  uint16_t i;

  if (msgContext->reqDataLen < 3) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    mode = msgContext->reqData[0];
    daqListNumber = Xcp_GetU16(&msgContext->reqData[1]);
    if (daqListNumber >= XCP_MAX_DAQ) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else {
      pDaq = (Xcp_DaqListType *)config->daqList[daqListNumber];
      ret = Xcp_IsDaqListConfigured(pDaq);
      if (E_OK != ret) {
        *nrc = XCP_E_DAQ_CONFIG;
      }
    }
    if (E_OK == ret) {
      switch (mode) {
      case XCP_DAQ_STOP:
        config->daqContexts[daqListNumber].state = XCP_DAQ_STOP;
        config->daqContexts[daqListNumber].curPid = XCP_INVALID_ODT_PID;
        break;
      case XCP_DAQ_START:
        config->daqContexts[daqListNumber].state = XCP_DAQ_START;
        break;
      case XCP_DAQ_SELECT:
        config->daqContexts[daqListNumber].state = XCP_DAQ_SELECT;
        break;
      default:
        ret = E_NOT_OK;
        *nrc = XCP_E_OUT_OF_RANGE;
        break;
      }
    }

    if ((E_OK == ret) && (XCP_DAQ_STOP != mode)) {
      /* calculate the first PID */
      msgContext->resData[0] = 0;
      for (i = 0; (i < daqListNumber) && (E_OK == ret); i++) {
        ret = Xcp_IsDaqListConfigured(config->daqList[i]);
        if (E_OK != ret) {
          *nrc = XCP_E_SEQUENCE;
        } else if (XCP_MAX_ODT_NUMBER >=
                   ((uint16_t)msgContext->resData[0] + config->daqList[i]->MaxOdt)) {
          msgContext->resData[0] += config->daqList[i]->MaxOdt;
        } else {
          *nrc = XCP_E_RESOURCE_TEMPORARY_NOT_ACCESSIBLE;
        }
      }
      msgContext->resDataLen = 1;
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspDAQStartStopSynch(Xcp_MsgContextType *msgContext,
                                        Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  uint8_t mode;
  uint16_t i;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();

  if (msgContext->reqDataLen < 1) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    mode = msgContext->reqData[0];
    switch (mode) {
    case XCP_DAQ_STOP_ALL:
      for (i = 0; i < XCP_MAX_DAQ; i++) {
        config->daqContexts[i].state = XCP_DAQ_STOP;
        config->daqContexts[i].curPid = XCP_INVALID_ODT_PID;
      }
      break;
    case XCP_DAQ_START_SELECT:
      for (i = 0; i < XCP_MAX_DAQ; i++) {
        if (XCP_DAQ_SELECT == config->daqContexts[i].state) {
          config->daqContexts[i].state = XCP_DAQ_START;
        }
      }
      break;
    case XCP_DAQ_STOP_SELECT:
      for (i = 0; i < XCP_MAX_DAQ; i++) {
        if (XCP_DAQ_SELECT == config->daqContexts[i].state) {
          config->daqContexts[i].state = XCP_DAQ_STOP;
        }
      }
      break;
    default:
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
      break;
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspSetDAQListMode(Xcp_MsgContextType *msgContext,
                                     Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  uint8_t mode;
  uint16_t daqListNumber;
  uint16_t eventChannel;
  uint8_t prescaler;
  uint8_t priority;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();

  if (msgContext->reqDataLen < 7) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    mode = msgContext->reqData[0];
    daqListNumber = Xcp_GetU16(&msgContext->reqData[1]);
    eventChannel = Xcp_GetU16(&msgContext->reqData[3]);
    prescaler = msgContext->reqData[5];
    priority = msgContext->reqData[6];
    if (0 != priority) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else if (0 == prescaler) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else if (0 != (mode & XCP_DAQ_LIST_MODE_DIRECTION)) {
      /* from master to slave */
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else if (0 != (mode & XCP_DAQ_LIST_MODE_TIMESTAMP)) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else if (0 != (mode & XCP_DAQ_LIST_MODE_PID_OFF)) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else if (daqListNumber >= XCP_MAX_DAQ) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else if (eventChannel >= (uint16_t)config->numOfEvChls) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else {
      /* config->daqContexts[daqListNumber].mode = mode; */
      config->daqContexts[daqListNumber].prescaler = prescaler;
      config->daqContexts[daqListNumber].evChl = (uint8_t)eventChannel;
      if (((uint32_t)config->evChls[eventChannel].TimeCycle * prescaler) <= 0xFFFFUL) {
        config->daqContexts[daqListNumber].timer =
          config->evChls[eventChannel].TimeCycle * prescaler;
      }
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspSetDAQPtr(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  uint16_t daqListNumber;
  uint8_t odtNumber;
  uint8_t odtEntryNumber;
  Xcp_ContextType *context = Xcp_GetContext();
  Xcp_SetDaqPtrContextType *sdpCtx = (Xcp_SetDaqPtrContextType *)context->cache;

  if (msgContext->reqDataLen < 5) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    daqListNumber = Xcp_GetU16(&msgContext->reqData[1]);
    odtNumber = msgContext->reqData[3];
    odtEntryNumber = msgContext->reqData[4];
    ret = Xcp_IsTheDaqPtrWriteValid(daqListNumber, odtNumber, odtEntryNumber, nrc);
    if (E_OK == ret) {
      sdpCtx->daqListNumber = daqListNumber;
      sdpCtx->odtNumber = odtNumber;
      sdpCtx->odtEntryNumber = odtEntryNumber;
      context->cacheOwner = XCP_PID_CMD_DAQ_SET_DAQ_PTR;
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspWriteDAQ(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Xcp_DaqListType *pDaq;
  Xcp_OdtEntryType *pEntry;
  Xcp_ContextType *context = Xcp_GetContext();
  Xcp_SetDaqPtrContextType *sdpCtx = (Xcp_SetDaqPtrContextType *)context->cache;
  uint8_t bitOffset;
  uint8_t length;
  uint8_t extension;
  uint32_t address;

  if (msgContext->reqDataLen < 7) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else if (XCP_PID_CMD_DAQ_SET_DAQ_PTR != context->cacheOwner) {
    ret = E_NOT_OK;
    *nrc = XCP_E_SEQUENCE;
  } else if (msgContext->reqData[2] >= XCP_MTA_EXT_MAX) {
    ret = E_NOT_OK;
    *nrc = XCP_E_OUT_OF_RANGE;
  } else {
    ret = Xcp_IsTheDaqPtrWriteValid(sdpCtx->daqListNumber, sdpCtx->odtNumber,
                                    sdpCtx->odtEntryNumber, nrc);
    if (E_OK == ret) {
      bitOffset = msgContext->reqData[0];
      length = msgContext->reqData[1];
      extension = msgContext->reqData[2];
      address = Xcp_GetU32(&msgContext->reqData[3]);
      pDaq = (Xcp_DaqListType *)config->daqList[sdpCtx->daqListNumber];
      pEntry = &(pDaq->Odts[sdpCtx->odtNumber].OdtEntries[sdpCtx->odtEntryNumber]);
      pEntry->Address = address;
      pEntry->BitOffset = bitOffset;
      pEntry->Length = length;
      pEntry->Extension = extension;
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspReadDAQ(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Xcp_DaqListType *pDaq;
  Xcp_OdtEntryType *pEntry;
  Xcp_ContextType *context = Xcp_GetContext();
  Xcp_SetDaqPtrContextType *sdpCtx = (Xcp_SetDaqPtrContextType *)context->cache;
  uint8_t bitOffset;
  uint8_t length;
  uint8_t extension;
  uint32_t address;

  if (XCP_PID_CMD_DAQ_SET_DAQ_PTR != context->cacheOwner) {
    ret = E_NOT_OK;
    *nrc = XCP_E_SEQUENCE;
  } else {
    ret = Xcp_IsTheDaqPtrReadValid(sdpCtx->daqListNumber, sdpCtx->odtNumber, sdpCtx->odtEntryNumber,
                                   nrc);
    if (E_OK == ret) {
      bitOffset = msgContext->reqData[0];
      length = msgContext->reqData[1];
      extension = msgContext->reqData[2];
      address = Xcp_GetU32(&msgContext->reqData[3]);
      pDaq = (Xcp_DaqListType *)config->daqList[sdpCtx->daqListNumber];
      pEntry = &(pDaq->Odts[sdpCtx->odtNumber].OdtEntries[sdpCtx->odtEntryNumber]);
      pEntry->Address = address;
      pEntry->BitOffset = bitOffset;
      pEntry->Length = length;
      pEntry->Extension = extension;

      msgContext->resData[0] = pEntry->BitOffset;
      msgContext->resData[1] = pEntry->Length;
      msgContext->resData[2] = pEntry->Extension;
      Xcp_SetU32(&msgContext->resData[3], pEntry->Address);
      msgContext->resDataLen = 7;
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspGetDAQProcessorInfo(Xcp_MsgContextType *msgContext,
                                          Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();

  msgContext->resData[0] = XCP_DAQ_CONFIG_TYPE_STATIC | XCP_DAQ_PRESCALER_SUPPORTED |
                           XCP_DAQ_PID_OFF_SUPPORTED; /* PROPERTIES */
  if (XCP_MAX_DAQ > XCP_MIN_DAQ) {
    msgContext->resData[0] |= XCP_DAQ_CONFIG_TYPE_DYNAMIC;
  }
  Xcp_SetU16(&msgContext->resData[1], XCP_MAX_DAQ);
  msgContext->resData[3] = 0; /* MAX_EVENT_CHANNEL */
  msgContext->resData[4] = config->numOfEvChls;
  Xcp_SetU16(&msgContext->resData[5], XCP_MIN_DAQ);
  msgContext->resData[7] = 0; /* DAQ_KEY_BYTE */
  msgContext->resDataLen = 7;

  return ret;
}

Std_ReturnType Xcp_DspGetDAQResolutionInfo(Xcp_MsgContextType *msgContext,
                                           Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;

  msgContext->resData[0] = 1; /* GRANULARITY_ODT_SIZE_DAQ_ODT */
  msgContext->resData[1] = 8; /* MAX_SIZE_DAT_ODT */
  msgContext->resData[2] = 0; /* GRANULARITY_ODT_SIZE_STIM_ODT */
  msgContext->resData[3] = 0; /* MAX_SIZE_STIM_ODT */
  msgContext->resData[4] = 0; /* TIMESTAMP_MODE */
  msgContext->resData[5] = 0; /* MIN_DAQ */
  msgContext->resData[6] = 0; /* TIMESTAMP_TICKS */
  msgContext->resData[7] = 0;
  msgContext->resDataLen = 7;

  return ret;
}

Std_ReturnType Xcp_DspGetDAQListInfo(Xcp_MsgContextType *msgContext,
                                     Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  uint16_t daqListNumber;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Xcp_DaqListType *pDaq;
  uint8_t i;
  uint8_t maxOdtEntries = 0;

  if (msgContext->reqDataLen < 3) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    daqListNumber = Xcp_GetU16(&msgContext->reqData[1]);

    if (daqListNumber >= XCP_MAX_DAQ) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else {
      pDaq = (Xcp_DaqListType *)config->daqList[daqListNumber];
      ret = Xcp_IsDaqListConfigured(pDaq);
      if (E_OK == ret) {
        for (i = 0; i < pDaq->MaxOdt; i++) {
          if (pDaq->Odts[i].EntryMaxSize > maxOdtEntries) {
            maxOdtEntries = pDaq->Odts[i].EntryMaxSize;
          }
        }
        msgContext->resData[0] = XCP_DAQ_LIST_DAQ; /* PROPERTIES: only support DAQ */
        if (daqListNumber < XCP_MIN_DAQ) {
          msgContext->resData[0] |= XCP_DAQ_LIST_PREDEFINED;
        }
        msgContext->resData[1] = pDaq->MaxOdt;  /* MAX_ODT */
        msgContext->resData[2] = maxOdtEntries; /* MAX_ODT_ENTRIES */
        msgContext->resData[3] = 0;             /* FIXED_EVENT */
        msgContext->resData[4] = 0;
        msgContext->resDataLen = 5;
      } else {
        ret = E_NOT_OK;
        *nrc = XCP_E_OUT_OF_RANGE;
      }
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspGetDAQEventInfo(Xcp_MsgContextType *msgContext,
                                      Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  uint16_t eventChannelNumber;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  uint32_t timeMs;

  if (msgContext->reqDataLen < 3) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    eventChannelNumber = Xcp_GetU16(&msgContext->reqData[1]);
    if (eventChannelNumber >= (uint16_t)config->numOfEvChls) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else {
      msgContext->resData[0] = XCP_DAQ_LIST_DAQ; /* PROPERTIES: only support DAQ */
      msgContext->resData[1] = 0xFF;             /* MAX_DAQ: unlimit as each DAQ has a timer */
      msgContext->resData[2] = 0;                /* Length: event channel name is not supported */
      timeMs = config->evChls[eventChannelNumber].TimeCycle * XCP_MAIN_FUNCTION_PERIOD;
      if (timeMs < 255) {
        msgContext->resData[3] = timeMs;                 /* Time Cycle */
        msgContext->resData[4] = XCP_TIMESTAMP_UNIT_1MS; /* Time Unit */
      } else if (timeMs < 2550) {
        msgContext->resData[3] = timeMs / 10;             /* Time Cycle */
        msgContext->resData[4] = XCP_TIMESTAMP_UNIT_10MS; /* Time Unit */
      } else {
        msgContext->resData[3] = timeMs / 100;            /* Time Cycle */
        msgContext->resData[4] = XCP_TIMESTAMP_UNIT_10MS; /* Time Unit */
      }
      msgContext->resData[5] = 0; /* Priority: not supported */
      msgContext->resDataLen = 6;
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspFreeDAQ(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  uint16_t i;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Xcp_DaqListType *pDaq;

  if (0 == config->daqList[XCP_MIN_DAQ]->MaxOdt) {
    /* No DAQ allocated */
    ret = E_NOT_OK;
    *nrc = XCP_E_SEQUENCE;
  } else {
    for (i = XCP_MIN_DAQ; i < XCP_MAX_DAQ; i++) {
      pDaq = (Xcp_DaqListType *)config->daqList[i];
      memset(pDaq, 0, sizeof(Xcp_DaqListType));
      config->daqContexts[i].state = 0;
      config->daqContexts[i].curPid = XCP_INVALID_ODT_PID;
    }
    memset(config->dynOdtSlots, 0, sizeof(Xcp_OdtType) * config->numOfDynOdtSlots);
    memset(config->dynOdtEntrySlots, 0, sizeof(Xcp_OdtEntryType) * config->numOfDynOdtEntrySlots);
  }

  return ret;
}

Std_ReturnType Xcp_DspAllocDAQ(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  uint16_t daqCount;
  uint16_t i;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Xcp_DaqListType *pDaq;

  if (msgContext->reqDataLen < 3) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    daqCount = Xcp_GetU16(&msgContext->reqData[1]);
    if (daqCount > (XCP_MAX_DAQ - XCP_MIN_DAQ)) {
      ret = E_NOT_OK;
      *nrc = XCP_E_MEMORY_OVERFLOW;
    } else if (0 != config->daqList[XCP_MIN_DAQ]->MaxOdt) {
      /* DAT already allocated, need to be free firstly for new allocation */
      ret = E_NOT_OK;
      *nrc = XCP_E_SEQUENCE;
    } else {
      for (i = 0; i < daqCount; i++) {
        pDaq = (Xcp_DaqListType *)config->daqList[XCP_MIN_DAQ + i];
        pDaq->MaxOdt = XCP_DAQ_LIST_ALLOCATED;
        pDaq->Odts = NULL;
      }
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspAllocODT(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  uint16_t daqListNumber;
  uint8_t odtCount;
  Xcp_OdtType *pOdts;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Xcp_DaqListType *pDaq;

  if (msgContext->reqDataLen < 4) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    daqListNumber = Xcp_GetU16(&msgContext->reqData[1]);
    odtCount = msgContext->reqData[3];
    if ((daqListNumber >= XCP_MAX_DAQ) || (daqListNumber < XCP_MIN_DAQ)) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else if ((0 == odtCount) || (odtCount == XCP_DAQ_LIST_ALLOCATED)) {
      ret = E_NOT_OK;
      *nrc = XCP_E_CMD_SYNTAX;
    } else {
      pDaq = (Xcp_DaqListType *)config->daqList[daqListNumber];
      if (XCP_DAQ_LIST_ALLOCATED != pDaq->MaxOdt) {
        /* DAQ not allocated or this ODT already allocated */
        ret = E_NOT_OK;
        *nrc = XCP_E_SEQUENCE;
      } else {
        pOdts = Xcp_AllocODT((uint16_t)odtCount);
        if (NULL == pOdts) {
          ret = E_NOT_OK;
          *nrc = XCP_E_MEMORY_OVERFLOW;
        } else {
          pDaq->Odts = pOdts;
          pDaq->MaxOdt = odtCount;
        }
      }
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspAllocODTEntry(Xcp_MsgContextType *msgContext,
                                    Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  uint16_t daqListNumber;
  uint8_t odtNumber;
  uint8_t odtEntryCount;
  Xcp_OdtEntryType *pEntries;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Xcp_DaqListType *pDaq;

  if (msgContext->reqDataLen < 5) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    daqListNumber = Xcp_GetU16(&msgContext->reqData[1]);
    odtNumber = msgContext->reqData[3];
    odtEntryCount = msgContext->reqData[4];
    if ((daqListNumber >= XCP_MAX_DAQ) || (daqListNumber < XCP_MIN_DAQ)) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else {
      pDaq = (Xcp_DaqListType *)config->daqList[daqListNumber];
      if (NULL == pDaq->Odts) {
        /* ODT not allocated */
        ret = E_NOT_OK;
        *nrc = XCP_E_SEQUENCE;
      } else if (odtNumber >= pDaq->MaxOdt) {
        ret = E_NOT_OK;
        *nrc = XCP_E_OUT_OF_RANGE;
      } else if (XCP_ODT_ALLOCATED != pDaq->Odts[odtNumber].EntryMaxSize) {
        ret = E_NOT_OK;
        *nrc = XCP_E_SEQUENCE;
      } else if ((0 == odtEntryCount) || (odtEntryCount > XCP_MAX_ODT_ENTRY_SIZE_DAQ)) {
        ret = E_NOT_OK;
        *nrc = XCP_E_CMD_SYNTAX;
      } else {
        pEntries = Xcp_AllocODTEntry(odtEntryCount);
        if (NULL == pEntries) {
          ret = E_NOT_OK;
          *nrc = XCP_E_MEMORY_OVERFLOW;
        } else {
          pDaq->Odts[odtNumber].OdtEntries = pEntries;
          pDaq->Odts[odtNumber].EntryMaxSize = odtEntryCount;
        }
      }
    }
  }

  return ret;
}

void Xcp_MainFunction_Daq(void) {
  uint16_t i;

  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  for (i = 0; i < XCP_MAX_DAQ; i++) {
    if (XCP_DAQ_START == config->daqContexts[i].state) {
      Xcp_MainFunction_DaqDriveTimer(i);
    }
  }
}

void Xcp_MainFunction_DaqWrite(void) {
  uint16_t i;
  uint8_t pidBase = 0;
  const Xcp_DaqListType *pDaq;
  Xcp_DaqListContextType *pDaqCtx;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  for (i = 0; i < XCP_MAX_DAQ; i++) {
    pDaq = config->daqList[i];
    pDaqCtx = &config->daqContexts[i];
    if (pDaqCtx->curPid < pDaq->MaxOdt) {
      Xcp_MainFunction_DaqDriveTx(i, pidBase);
      break;
    }
    pidBase += pDaq->MaxOdt;
  }
}
