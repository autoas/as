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

#define XCP_MAX_DAQ (config->NumOfDaqList)
#define XCP_MIN_DAQ (config->NumOfStaticDaqList)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static Xcp_OdtType *Xcp_AllocODT(uint16_t odtCount) {
  uint16_t index = 0;
  Xcp_OdtType *pOdts = NULL;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();

  for (index = 0; index < config->NumOfDynOdtSlots; index++) {
    if (0 == config->DynOdtSlots[index].EntryMaxSize) {
      break;
    }
  }

  if ((config->NumOfDynOdtSlots - index) >= odtCount) {
    pOdts = &config->DynOdtSlots[index];
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

  for (index = 0; index < config->NumOfDynOdtEntrySlots; index++) {
    if (0 == config->DynOdtEntrySlots[index].Length) {
      break;
    }
  }

  if ((config->NumOfDynOdtEntrySlots - index) >= odtEntryCount) {
    pEntries = &config->DynOdtEntrySlots[index];
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
    pDaq = (Xcp_DaqListType *)config->DaqList[daqListNumber];
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
    pDaq = (Xcp_DaqListType *)config->DaqList[daqListNumber];
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
  const Xcp_DaqListType *pDaq = config->DaqList[daqListNumber];
  Xcp_DaqListContextType *pDaqCtx = &config->DaqContexts[daqListNumber];
  uint8_t data[XCP_PATCKET_MAX_SIZE];
  PduInfoType PduInfo = {data, NULL, sizeof(data)};
  uint8_t offset = 1;
  uint8_t i;
  uint8_t nrc = 0;
  Std_ReturnType ret = E_OK;

  data[0] = pidBase + pDaqCtx->CurPid;
  for (i = 0; (i < pDaq->Odts[pDaqCtx->CurPid].EntryMaxSize) && (E_OK == ret); i++) {
    if ((XCP_PATCKET_MAX_SIZE - offset) >= pDaq->Odts[pDaqCtx->CurPid].OdtEntries[i].Length) {
      ret = Xcp_MtaRead(pDaq->Odts[pDaqCtx->CurPid].OdtEntries[i].Extension,
                        pDaq->Odts[pDaqCtx->CurPid].OdtEntries[i].Address, &data[offset],
                        pDaq->Odts[pDaqCtx->CurPid].OdtEntries[i].Length, &nrc);
      if (E_OK == ret) {
        offset += pDaq->Odts[pDaqCtx->CurPid].OdtEntries[i].Length;
      } else {
        ASLOG(DAQE,
              ("DAQ %d read for PID %d Entry %d failed\n", daqListNumber, pDaqCtx->CurPid, i));
        pDaqCtx->CurPid = XCP_INVALID_ODT_PID;
        pDaqCtx->Timer = 0; /* stop it */
      }
    } else {
      ASLOG(DAQE, ("DAQ %d buffer overflow\n", daqListNumber));
      pDaqCtx->CurPid = XCP_INVALID_ODT_PID;
      pDaqCtx->Timer = 0; /* stop it */
    }
  }

  if (E_OK == ret) {
    PduInfo.SduLength = offset;
    ret = CanIf_Transmit(config->CanIfTxPduId, &PduInfo);
    if (E_OK == ret) {
      pDaqCtx->CurPid++;
    }
  }
}

static void Xcp_MainFunction_DaqDriveTimer(uint16_t daqListNumber) {
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Xcp_DaqListContextType *pDaqCtx = &config->DaqContexts[daqListNumber];
  if (pDaqCtx->Timer > 0) {
    pDaqCtx->Timer--;
    if (0 == pDaqCtx->Timer) {
      ASLOG(DAQ, ("DAQ %d trigger!\n", daqListNumber));
      pDaqCtx->CurPid = 0;
      asAssert(pDaqCtx->EvChl < config->NumOfEvChls);
      pDaqCtx->Timer = config->EvChls[pDaqCtx->EvChl].TimeCycle * pDaqCtx->Prescaler;
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Xcp_DspDaqInit(void) {
  uint16_t i;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Xcp_DaqListType *pDaq;
  memset(config->DynOdtSlots, 0, sizeof(Xcp_OdtType) * config->NumOfDynOdtSlots);
  memset(config->DynOdtEntrySlots, 0, sizeof(Xcp_OdtEntryType) * config->NumOfDynOdtEntrySlots);
  memset(config->DaqContexts, 0, sizeof(Xcp_DaqListContextType) * XCP_MAX_DAQ);
  for (i = XCP_MIN_DAQ; i < XCP_MAX_DAQ; i++) {
    pDaq = (Xcp_DaqListType *)config->DaqList[i];
    memset(pDaq, 0, sizeof(Xcp_DaqListType));
  }
  for (i = 0; i < XCP_MAX_DAQ; i++) {
    config->DaqContexts[i].EvChl = config->NumOfEvChls; /* mark as invalid */
    config->DaqContexts[i].CurPid = XCP_INVALID_ODT_PID;
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

  if (msgContext->ReqDataLen < 3) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    daqListNumber = Xcp_GetU16(&msgContext->ReqData[1]);
    if (daqListNumber >= XCP_MAX_DAQ) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else {
      pDaq = (Xcp_DaqListType *)config->DaqList[daqListNumber];
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
            config->DaqContexts[daqListNumber].State = 0;
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

  if (msgContext->ReqDataLen < 3) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    mode = msgContext->ReqData[0];
    daqListNumber = Xcp_GetU16(&msgContext->ReqData[1]);
    if (daqListNumber >= XCP_MAX_DAQ) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else {
      pDaq = (Xcp_DaqListType *)config->DaqList[daqListNumber];
      ret = Xcp_IsDaqListConfigured(pDaq);
      if (E_OK != ret) {
        *nrc = XCP_E_DAQ_CONFIG;
      }
    }
    if (E_OK == ret) {
      switch (mode) {
      case XCP_DAQ_STOP:
        config->DaqContexts[daqListNumber].State = XCP_DAQ_STOP;
        config->DaqContexts[daqListNumber].CurPid = XCP_INVALID_ODT_PID;
        break;
      case XCP_DAQ_START:
        config->DaqContexts[daqListNumber].State = XCP_DAQ_START;
        break;
      case XCP_DAQ_SELECT:
        config->DaqContexts[daqListNumber].State = XCP_DAQ_SELECT;
        break;
      default:
        ret = E_NOT_OK;
        *nrc = XCP_E_OUT_OF_RANGE;
        break;
      }
    }

    if ((E_OK == ret) && (XCP_DAQ_STOP != mode)) {
      /* calculate the first PID */
      msgContext->ResData[0] = 0;
      for (i = 0; (i < daqListNumber) && (E_OK == ret); i++) {
        ret = Xcp_IsDaqListConfigured(config->DaqList[i]);
        if (E_OK != ret) {
          *nrc = XCP_E_SEQUENCE;
        } else if (XCP_MAX_ODT_NUMBER >=
                   ((uint16_t)msgContext->ResData[0] + config->DaqList[i]->MaxOdt)) {
          msgContext->ResData[0] += config->DaqList[i]->MaxOdt;
        } else {
          *nrc = XCP_E_RESOURCE_TEMPORARY_NOT_ACCESSIBLE;
        }
      }
      msgContext->ResDataLen = 1;
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

  if (msgContext->ReqDataLen < 1) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    mode = msgContext->ReqData[0];
    switch (mode) {
    case XCP_DAQ_STOP_ALL:
      for (i = 0; i < XCP_MAX_DAQ; i++) {
        config->DaqContexts[i].State = XCP_DAQ_STOP;
        config->DaqContexts[i].CurPid = XCP_INVALID_ODT_PID;
      }
      break;
    case XCP_DAQ_START_SELECT:
      for (i = 0; i < XCP_MAX_DAQ; i++) {
        if (XCP_DAQ_SELECT == config->DaqContexts[i].State) {
          config->DaqContexts[i].State = XCP_DAQ_START;
        }
      }
      break;
    case XCP_DAQ_STOP_SELECT:
      for (i = 0; i < XCP_MAX_DAQ; i++) {
        if (XCP_DAQ_SELECT == config->DaqContexts[i].State) {
          config->DaqContexts[i].State = XCP_DAQ_STOP;
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

  if (msgContext->ReqDataLen < 7) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    mode = msgContext->ReqData[0];
    daqListNumber = Xcp_GetU16(&msgContext->ReqData[1]);
    eventChannel = Xcp_GetU16(&msgContext->ReqData[3]);
    prescaler = msgContext->ReqData[5];
    priority = msgContext->ReqData[6];
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
    } else if (eventChannel >= (uint16_t)config->NumOfEvChls) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else {
      /* config->DaqContexts[daqListNumber].mode = mode; */
      config->DaqContexts[daqListNumber].Prescaler = prescaler;
      config->DaqContexts[daqListNumber].EvChl = (uint8_t)eventChannel;
      if (((uint32_t)config->EvChls[eventChannel].TimeCycle * prescaler) <= 0xFFFFUL) {
        config->DaqContexts[daqListNumber].Timer =
          config->EvChls[eventChannel].TimeCycle * prescaler;
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
  Xcp_SetDaqPtrContextType *sdpCtx = (Xcp_SetDaqPtrContextType *)context->Cache;

  if (msgContext->ReqDataLen < 5) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    daqListNumber = Xcp_GetU16(&msgContext->ReqData[1]);
    odtNumber = msgContext->ReqData[3];
    odtEntryNumber = msgContext->ReqData[4];
    ret = Xcp_IsTheDaqPtrWriteValid(daqListNumber, odtNumber, odtEntryNumber, nrc);
    if (E_OK == ret) {
      sdpCtx->DaqListNumber = daqListNumber;
      sdpCtx->OdtNumber = odtNumber;
      sdpCtx->OdtEntryNumber = odtEntryNumber;
      context->CacheOwner = XCP_PID_CMD_DAQ_SET_DAQ_PTR;
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
  Xcp_SetDaqPtrContextType *sdpCtx = (Xcp_SetDaqPtrContextType *)context->Cache;
  uint8_t bitOffset;
  uint8_t length;
  uint8_t extension;
  uint32_t address;

  if (msgContext->ReqDataLen < 7) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else if (XCP_PID_CMD_DAQ_SET_DAQ_PTR != context->CacheOwner) {
    ret = E_NOT_OK;
    *nrc = XCP_E_SEQUENCE;
  } else if (msgContext->ReqData[2] >= XCP_MTA_EXT_MAX) {
    ret = E_NOT_OK;
    *nrc = XCP_E_OUT_OF_RANGE;
  } else {
    ret = Xcp_IsTheDaqPtrWriteValid(sdpCtx->DaqListNumber, sdpCtx->OdtNumber,
                                    sdpCtx->OdtEntryNumber, nrc);
    if (E_OK == ret) {
      bitOffset = msgContext->ReqData[0];
      length = msgContext->ReqData[1];
      extension = msgContext->ReqData[2];
      address = Xcp_GetU32(&msgContext->ReqData[3]);
      pDaq = (Xcp_DaqListType *)config->DaqList[sdpCtx->DaqListNumber];
      pEntry = &(pDaq->Odts[sdpCtx->OdtNumber].OdtEntries[sdpCtx->OdtEntryNumber]);
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
  Xcp_SetDaqPtrContextType *sdpCtx = (Xcp_SetDaqPtrContextType *)context->Cache;
  uint8_t bitOffset;
  uint8_t length;
  uint8_t extension;
  uint32_t address;

  if (XCP_PID_CMD_DAQ_SET_DAQ_PTR != context->CacheOwner) {
    ret = E_NOT_OK;
    *nrc = XCP_E_SEQUENCE;
  } else {
    ret = Xcp_IsTheDaqPtrReadValid(sdpCtx->DaqListNumber, sdpCtx->OdtNumber, sdpCtx->OdtEntryNumber,
                                   nrc);
    if (E_OK == ret) {
      bitOffset = msgContext->ReqData[0];
      length = msgContext->ReqData[1];
      extension = msgContext->ReqData[2];
      address = Xcp_GetU32(&msgContext->ReqData[3]);
      pDaq = (Xcp_DaqListType *)config->DaqList[sdpCtx->DaqListNumber];
      pEntry = &(pDaq->Odts[sdpCtx->OdtNumber].OdtEntries[sdpCtx->OdtEntryNumber]);
      pEntry->Address = address;
      pEntry->BitOffset = bitOffset;
      pEntry->Length = length;
      pEntry->Extension = extension;

      msgContext->ResData[0] = pEntry->BitOffset;
      msgContext->ResData[1] = pEntry->Length;
      msgContext->ResData[2] = pEntry->Extension;
      Xcp_SetU32(&msgContext->ResData[3], pEntry->Address);
      msgContext->ResDataLen = 7;
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspGetDAQProcessorInfo(Xcp_MsgContextType *msgContext,
                                          Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();

  msgContext->ResData[0] = XCP_DAQ_CONFIG_TYPE_STATIC | XCP_DAQ_PRESCALER_SUPPORTED |
                           XCP_DAQ_PID_OFF_SUPPORTED; /* PROPERTIES */
  if (XCP_MAX_DAQ > XCP_MIN_DAQ) {
    msgContext->ResData[0] |= XCP_DAQ_CONFIG_TYPE_DYNAMIC;
  }
  Xcp_SetU16(&msgContext->ResData[1], XCP_MAX_DAQ);
  msgContext->ResData[3] = 0; /* MAX_EVENT_CHANNEL */
  msgContext->ResData[4] = config->NumOfEvChls;
  Xcp_SetU16(&msgContext->ResData[5], XCP_MIN_DAQ);
  msgContext->ResData[7] = 0; /* DAQ_KEY_BYTE */
  msgContext->ResDataLen = 7;

  return ret;
}

Std_ReturnType Xcp_DspGetDAQResolutionInfo(Xcp_MsgContextType *msgContext,
                                           Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;

  msgContext->ResData[0] = 1; /* GRANULARITY_ODT_SIZE_DAQ_ODT */
  msgContext->ResData[1] = 8; /* MAX_SIZE_DAT_ODT */
  msgContext->ResData[2] = 0; /* GRANULARITY_ODT_SIZE_STIM_ODT */
  msgContext->ResData[3] = 0; /* MAX_SIZE_STIM_ODT */
  msgContext->ResData[4] = 0; /* TIMESTAMP_MODE */
  msgContext->ResData[5] = 0; /* MIN_DAQ */
  msgContext->ResData[6] = 0; /* TIMESTAMP_TICKS */
  msgContext->ResData[7] = 0;
  msgContext->ResDataLen = 7;

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

  if (msgContext->ReqDataLen < 3) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    daqListNumber = Xcp_GetU16(&msgContext->ReqData[1]);

    if (daqListNumber >= XCP_MAX_DAQ) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else {
      pDaq = (Xcp_DaqListType *)config->DaqList[daqListNumber];
      ret = Xcp_IsDaqListConfigured(pDaq);
      if (E_OK == ret) {
        for (i = 0; i < pDaq->MaxOdt; i++) {
          if (pDaq->Odts[i].EntryMaxSize > maxOdtEntries) {
            maxOdtEntries = pDaq->Odts[i].EntryMaxSize;
          }
        }
        msgContext->ResData[0] = XCP_DAQ_LIST_DAQ; /* PROPERTIES: only support DAQ */
        if (daqListNumber < XCP_MIN_DAQ) {
          msgContext->ResData[0] |= XCP_DAQ_LIST_PREDEFINED;
        }
        msgContext->ResData[1] = pDaq->MaxOdt;  /* MAX_ODT */
        msgContext->ResData[2] = maxOdtEntries; /* MAX_ODT_ENTRIES */
        msgContext->ResData[3] = 0;             /* FIXED_EVENT */
        msgContext->ResData[4] = 0;
        msgContext->ResDataLen = 5;
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

  if (msgContext->ReqDataLen < 3) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    eventChannelNumber = Xcp_GetU16(&msgContext->ReqData[1]);
    if (eventChannelNumber >= (uint16_t)config->NumOfEvChls) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else {
      msgContext->ResData[0] = XCP_DAQ_LIST_DAQ; /* PROPERTIES: only support DAQ */
      msgContext->ResData[1] = 0xFF;             /* MAX_DAQ: unlimit as each DAQ has a timer */
      msgContext->ResData[2] = 0;                /* Length: event channel name is not supported */
      timeMs = config->EvChls[eventChannelNumber].TimeCycle * XCP_MAIN_FUNCTION_PERIOD;
      if (timeMs < 255) {
        msgContext->ResData[3] = timeMs;                 /* Time Cycle */
        msgContext->ResData[4] = XCP_TIMESTAMP_UNIT_1MS; /* Time Unit */
      } else if (timeMs < 2550) {
        msgContext->ResData[3] = timeMs / 10;             /* Time Cycle */
        msgContext->ResData[4] = XCP_TIMESTAMP_UNIT_10MS; /* Time Unit */
      } else {
        msgContext->ResData[3] = timeMs / 100;            /* Time Cycle */
        msgContext->ResData[4] = XCP_TIMESTAMP_UNIT_10MS; /* Time Unit */
      }
      msgContext->ResData[5] = 0; /* Priority: not supported */
      msgContext->ResDataLen = 6;
    }
  }

  return ret;
}

Std_ReturnType Xcp_DspFreeDAQ(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  uint16_t i;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Xcp_DaqListType *pDaq;

  if (0 == config->DaqList[XCP_MIN_DAQ]->MaxOdt) {
    /* No DAQ allocated */
    ret = E_NOT_OK;
    *nrc = XCP_E_SEQUENCE;
  } else {
    for (i = XCP_MIN_DAQ; i < XCP_MAX_DAQ; i++) {
      pDaq = (Xcp_DaqListType *)config->DaqList[i];
      memset(pDaq, 0, sizeof(Xcp_DaqListType));
      config->DaqContexts[i].State = 0;
      config->DaqContexts[i].CurPid = XCP_INVALID_ODT_PID;
    }
    memset(config->DynOdtSlots, 0, sizeof(Xcp_OdtType) * config->NumOfDynOdtSlots);
    memset(config->DynOdtEntrySlots, 0, sizeof(Xcp_OdtEntryType) * config->NumOfDynOdtEntrySlots);
  }

  return ret;
}

Std_ReturnType Xcp_DspAllocDAQ(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  uint16_t daqCount;
  uint16_t i;
  P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) config = Xcp_GetConfig();
  Xcp_DaqListType *pDaq;

  if (msgContext->ReqDataLen < 3) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    daqCount = Xcp_GetU16(&msgContext->ReqData[1]);
    if (daqCount > (XCP_MAX_DAQ - XCP_MIN_DAQ)) {
      ret = E_NOT_OK;
      *nrc = XCP_E_MEMORY_OVERFLOW;
    } else if (0 != config->DaqList[XCP_MIN_DAQ]->MaxOdt) {
      /* DAT already allocated, need to be free firstly for new allocation */
      ret = E_NOT_OK;
      *nrc = XCP_E_SEQUENCE;
    } else {
      for (i = 0; i < daqCount; i++) {
        pDaq = (Xcp_DaqListType *)config->DaqList[XCP_MIN_DAQ + i];
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

  if (msgContext->ReqDataLen < 4) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    daqListNumber = Xcp_GetU16(&msgContext->ReqData[1]);
    odtCount = msgContext->ReqData[3];
    if ((daqListNumber >= XCP_MAX_DAQ) || (daqListNumber < XCP_MIN_DAQ)) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else if ((0 == odtCount) || (odtCount == XCP_DAQ_LIST_ALLOCATED)) {
      ret = E_NOT_OK;
      *nrc = XCP_E_CMD_SYNTAX;
    } else {
      pDaq = (Xcp_DaqListType *)config->DaqList[daqListNumber];
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

  if (msgContext->ReqDataLen < 5) {
    ret = E_NOT_OK;
    *nrc = XCP_E_CMD_SYNTAX;
  } else {
    daqListNumber = Xcp_GetU16(&msgContext->ReqData[1]);
    odtNumber = msgContext->ReqData[3];
    odtEntryCount = msgContext->ReqData[4];
    if ((daqListNumber >= XCP_MAX_DAQ) || (daqListNumber < XCP_MIN_DAQ)) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    } else {
      pDaq = (Xcp_DaqListType *)config->DaqList[daqListNumber];
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
    if (XCP_DAQ_START == config->DaqContexts[i].State) {
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
    pDaq = config->DaqList[i];
    pDaqCtx = &config->DaqContexts[i];
    if (pDaqCtx->CurPid < pDaq->MaxOdt) {
      Xcp_MainFunction_DaqDriveTx(i, pidBase);
      break;
    }
    pidBase += pDaq->MaxOdt;
  }
}
