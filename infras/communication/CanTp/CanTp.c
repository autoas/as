/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Transport Layer AUTOSAR CP Release 4.4.0
 *  Figure 4: Summary of N_PCI bytes @SWS_CanTp_00350
 *      www.can-cia.org/fileadmin/resources/documents/proceedings/2015_hartkopp.pdf
 *      github.com/hartkopp/can-isotp/blob/master/net/can/isotp.c
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "CanTp.h"
#include "CanIf.h"
#ifndef CANTP_CFG_H
#include "CanTp_Cfg.h"
#endif
#include "CanTp_Priv.h"
#include "PduR_CanTp.h"
#include "Std_Debug.h"
#include <string.h>
#include "Std_Topic.h"
#include "Std_Critical.h"

#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_CANTP 0
#define AS_LOG_CANTPI 0
#define AS_LOG_CANTPE 2

#ifndef CANTP_STMIN_ADJUST
#define CANTP_STMIN_ADJUST 0
#endif

#ifdef CANTP_USE_PB_CONFIG
#define CANTP_CONFIG cantpConfig
#else
#define CANTP_CONFIG (&CanTp_Config)
#endif

/* see ISO 15765-2 2004 */
#define N_PCI_MASK 0xF0u
#define N_PCI_SF 0x00u
#define N_PCI_FF 0x10u
#define N_PCI_CF 0x20u
#define N_PCI_FC 0x30u
#define N_PCI_SF_DL 0x0Fu
/* Flow Control Status Mask */
#define N_PCI_FS 0x0Fu
/* Flow Control Status */
#define N_PCI_CTS 0x00u
#define N_PCI_WT 0x01u
#define N_PCI_OVFLW 0x02u

#define N_PCI_SN 0x0Fu

#ifdef CANTP_FIX_LL_DL
#define CanTp_GetDL(len, LL_DL) LL_DL
#endif

#ifdef USE_CANTP_CRITICAL
#define tpEnterCritical() EnterCritical()
#define tpExitCritical() ExitCritical();
#else
#define tpEnterCritical()
#define tpExitCritical()
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const CanTp_ConfigType CanTp_Config;

static void CanTp_SendSF(PduIdType TxPduId);
#ifndef CANTP_NO_FC
static void CanTp_SendFC(PduIdType TxPduId, uint8_t FlowStatus);
#endif
static void CanTp_SendCF(PduIdType TxPduId);
/* ================================ [ DATAS     ] ============================================== */
#ifdef CANTP_USE_PB_CONFIG
static const CanTp_ConfigType *cantpConfig = NULL;
#endif
/* ================================ [ LOCALS    ] ============================================== */
static void CanTp_ResetToIdle(CanTp_ChannelContextType *context) {
  CanTpCancelAlarm();
  context->state = CANTP_IDLE;
  context->TpSduLength = 0;
}

#ifndef CANTP_FIX_LL_DL
static PduLengthType CanTp_GetDL(PduLengthType len, PduLengthType LL_DL) {
  PduLengthType dl = LL_DL;
  uint32_t i;
  const PduLengthType lLL_DLs[] = {8, 12, 16, 20, 24, 32, 48};

  for (i = 0; i < ARRAY_SIZE(lLL_DLs); i++) {
    if (len <= lLL_DLs[i]) {
      dl = lLL_DLs[i];
      break;
    }
  }
  return dl;
}
#endif

static uint8_t CanTp_GetSFMaxLen(const CanTp_ChannelConfigType *config) {
  PduLengthType sfMaxLen;
  if (config->LL_DL > 8u) {
    if (CANTP_EXTENDED == config->AddressingFormat) {
      sfMaxLen = config->LL_DL - 3u;
    } else {
      sfMaxLen = config->LL_DL - 2u;
    }
  } else {
    if (CANTP_EXTENDED == config->AddressingFormat) {
      sfMaxLen = config->LL_DL - 2u;
    } else {
      sfMaxLen = config->LL_DL - 1u;
    }
  }
  return (uint8_t)sfMaxLen;
}

static void CanTp_HandleSF(PduIdType RxPduId, uint8_t pci, uint8_t *data, uint8_t length) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  PduInfoType PduInfo;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
  PduLengthType sfMaxLen;
  PduLengthType offset = 0;
  uint8_t dataLen = length;

  context = &(CANTP_CONFIG->channelContexts[RxPduId]);
  config = &(CANTP_CONFIG->channelConfigs[RxPduId]);

  if (context->state != CANTP_IDLE) {
    ASLOG(CANTPE, ("[%d]SF received in state %d!\n", RxPduId, context->state));
    switch (context->state) {
    case CANTP_WAIT_CF:
    case CANTP_RESEND_FC_CTS:
    case CANTP_WAIT_FC_CTS_TX_COMPLETED:
      /* Rx new message when previous Rx is not finished */
      PduR_CanTpRxIndication(config->PduR_RxPduId, E_NOT_OK);
      break;
    default:
      /* Rx new message when previous Tx is not finished */
      PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
      break;
    }
    CanTp_ResetToIdle(context);
  }

  sfMaxLen = CanTp_GetSFMaxLen(config);

  PduInfo.SduLength = (PduLengthType)pci & N_PCI_SF_DL;
  if ((0u == PduInfo.SduLength) && (config->LL_DL > 8u)) {
    PduInfo.SduLength = data[0];
    PduInfo.SduDataPtr = &data[1];
    dataLen -= 1u;
  } else {
    PduInfo.SduDataPtr = data;
  }
  PduInfo.MetaDataPtr = (uint8_t *)&offset;

  if ((PduInfo.SduLength <= sfMaxLen) && (PduInfo.SduLength > 0u) &&
      (PduInfo.SduLength <= dataLen)) {
    bufReq =
      PduR_CanTpStartOfReception(config->PduR_RxPduId, &PduInfo, PduInfo.SduLength, &bufferSize);

    if (BUFREQ_OK == bufReq) {
      bufReq = PduR_CanTpCopyRxData(config->PduR_RxPduId, &PduInfo, &bufferSize);
      if (BUFREQ_OK != bufReq) {
        PduR_CanTpRxIndication(config->PduR_RxPduId, E_NOT_OK);
      }
    }

    if (BUFREQ_OK == bufReq) {
      PduR_CanTpRxIndication(config->PduR_RxPduId, E_OK);
    }

    if (BUFREQ_OK != bufReq) {
      ASLOG(CANTPE, ("[%d]SF received with buffer status %d!\n", RxPduId, bufReq));
    }
  } else {
    ASLOG(CANTPE, ("[%d]SF received with invalid len: %d %d %d!\n", RxPduId, (int)PduInfo.SduLength,
                   (int)sfMaxLen, (int)length));
  }
}

#ifndef CANTP_NO_FC
static void CanTp_SendFC(PduIdType RxPduId, uint8_t FlowStatus) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
#ifndef CANTP_USE_TRIGGER_TRANSMIT
  Std_ReturnType r;
#endif
  uint8_t *data;
  uint8_t pos = 0;

  context = &(CANTP_CONFIG->channelContexts[RxPduId]);
  config = &(CANTP_CONFIG->channelConfigs[RxPduId]);

  data = config->data;

  if (CANTP_EXTENDED == config->AddressingFormat) {
    data[pos] = config->N_TA;
    pos++;
  }

  data[pos] = N_PCI_FC | FlowStatus;
  pos++;
  data[pos] = config->BS;
  pos++;
  data[pos] = config->STmin;
  pos++;
  while (pos < 8u) {
    data[pos] = config->padding;
    pos++;
  }
  ASLOG(CANTPI, ("[%d]TX data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", RxPduId, data[0],
                 data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
  context->PduInfo.SduDataPtr = data;
  context->PduInfo.SduLength = 8;

#ifdef CANTP_USE_TRIGGER_TRANSMIT
  CanTpSetAlarm(config->N_As);
  if (N_PCI_CTS == FlowStatus) {
    context->state = CANTP_RESEND_FC_CTS;
  } else {
    context->state = CANTP_RESEND_FC_OVFLW;
  }
#else
  r = CanIf_Transmit(config->CanIfTxPduId, &context->PduInfo);
  if (E_OK == r) {
    STD_TOPIC_ISOTP(RxPduId, FALSE, CanIf_CanTpGetTxCanId(config->CanIfTxPduId),
                    context->PduInfo.SduLength, context->PduInfo.SduDataPtr);
    if (N_PCI_CTS == FlowStatus) {
#ifdef CANTP_USE_TX_CONFIRMATION
      context->state = CANTP_WAIT_FC_CTS_TX_COMPLETED;
#else
      context->state = CANTP_WAIT_CF;
#endif
      CanTpSetAlarm(config->N_Cr);
    } else {
#ifdef CANTP_USE_TX_CONFIRMATION
      context->state = CANTP_WAIT_FC_OVFLW_TX_COMPLETED;
#else
      CanTp_ResetToIdle(context);
#endif
    }
    context->BS = config->BS;
  } else {
    CanTpSetAlarm(config->N_As);
    if (N_PCI_CTS == FlowStatus) {
      context->state = CANTP_RESEND_FC_CTS;
    } else {
      context->state = CANTP_RESEND_FC_OVFLW;
    }
  }
#endif
}
#endif

static void CanTp_HandleFF(PduIdType RxPduId, uint8_t pci, uint8_t *data, uint8_t length) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  PduInfoType PduInfo;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
  PduLengthType ffLen;
  PduLengthType sfMaxLen;
  uint32_t TpSduLength;
  PduLengthType offset = 0;

  context = &(CANTP_CONFIG->channelContexts[RxPduId]);
  config = &(CANTP_CONFIG->channelConfigs[RxPduId]);

  if (context->state != CANTP_IDLE) {
    ASLOG(CANTPE, ("[%d]FF received in state %d!\n", RxPduId, context->state));
    switch (context->state) {
    case CANTP_WAIT_CF:
    case CANTP_RESEND_FC_CTS:
    case CANTP_WAIT_FC_CTS_TX_COMPLETED:
      /* Rx new message when previous Rx is not finished */
      PduR_CanTpRxIndication(config->PduR_RxPduId, E_NOT_OK);
      break;
    default:
      /* Rx new message when previous Tx is not finished */
      PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
      break;
    }
    CanTp_ResetToIdle(context);
  }

  TpSduLength = (((uint32_t)pci & 0x0Fu) << 8) + data[0];
  PduInfo.SduDataPtr = &data[1];

  PduInfo.SduLength = (PduLengthType)length - 1u;
  if (CANTP_EXTENDED == config->AddressingFormat) {
    ffLen = config->LL_DL - 3u;
  } else {
    ffLen = config->LL_DL - 2u;
  }

  if ((config->LL_DL > 8u) && (0u == TpSduLength)) {
    TpSduLength = ((uint32_t)data[2] << 24) + ((uint32_t)data[3] << 16) + ((uint32_t)data[4] << 8) +
                  ((uint32_t)data[5]);
    PduInfo.SduDataPtr = &data[6];
    PduInfo.SduLength -= 4u;
    ffLen -= 4u;
  }
  PduInfo.MetaDataPtr = (uint8_t *)&offset;

  sfMaxLen = CanTp_GetSFMaxLen(config);

  if (TpSduLength > PDU_LENGHT_MAX) {
    ASLOG(CANTPE, ("[%d]FF size too big %u!\n", RxPduId, TpSduLength));
  } else if ((TpSduLength <= ffLen) || (TpSduLength <= sfMaxLen)) {
    ASLOG(CANTPE, ("[%d]FF size invalid %u(<=%d,%d)!\n", RxPduId, TpSduLength, ffLen, sfMaxLen));
  } else if (PduInfo.SduLength == ffLen) {
    bufReq = PduR_CanTpStartOfReception(config->PduR_RxPduId, &PduInfo, (PduLengthType)TpSduLength,
                                        &bufferSize);

    if (BUFREQ_OK == bufReq) {
      bufReq = PduR_CanTpCopyRxData(config->PduR_RxPduId, &PduInfo, &bufferSize);

      if (BUFREQ_OK != bufReq) {
        PduR_CanTpRxIndication(config->PduR_RxPduId, E_NOT_OK);
      }
    }

    if (BUFREQ_OK != bufReq) {
#ifdef CANTP_NO_FC
      ASLOG(CANTPE, ("[%d]FF received with buffer status %d!\n", RxPduId, bufReq));
      CanTp_ResetToIdle(context);
#else
      CanTp_SendFC(RxPduId, N_PCI_OVFLW);
#endif
    } else {
      context->TpSduLength = (PduLengthType)TpSduLength - ffLen;
      context->TpSduOffset = ffLen;
      context->SN = 1;
#ifdef CANTP_NO_FC
      context->state = CANTP_WAIT_CF;
      CanTpSetAlarm(config->N_Cr);
      context->BS = 0;
#else
      CanTp_SendFC(RxPduId, N_PCI_CTS);
#endif
    }
  } else {
    ASLOG(CANTPE,
          ("[%d]FF received with invalid len %d(!=%d)!\n", RxPduId, PduInfo.SduLength, ffLen));
  }
}

static void CanTp_HandleCF(PduIdType RxPduId, uint8_t pci, uint8_t *data, uint8_t length) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  PduInfoType PduInfo;
  BufReq_ReturnType bufReq = BUFREQ_OK;
  PduLengthType bufferSize;
  PduLengthType cfLen;

  context = &(CANTP_CONFIG->channelContexts[RxPduId]);
  config = &(CANTP_CONFIG->channelConfigs[RxPduId]);

  /* A robust logic to handle cases where TxConfirm occurs after RxInd or is missing. */
  if (CANTP_WAIT_FC_CTS_TX_COMPLETED == context->state) {
    ASLOG(CANTPE, ("[%d]CF received before FC CTS TxConfirm\n", RxPduId));
    context->state = CANTP_WAIT_CF;
  }

  if (CANTP_EXTENDED == config->AddressingFormat) {
    cfLen = config->LL_DL - 2u;
  } else {
    cfLen = config->LL_DL - 1u;
  }

  if (context->state != CANTP_WAIT_CF) {
    ASLOG(CANTPE, ("[%d]CF received when in state %d.\n", RxPduId, context->state));
    bufReq = BUFREQ_E_NOT_OK;
  } else if (context->TpSduLength > length) {
    if (length != cfLen) {
      bufReq = BUFREQ_E_NOT_OK;
    }
  } else {
    /* OK */
  }
  if (BUFREQ_OK == bufReq) {
    if (context->SN == (pci & N_PCI_SN)) {
      context->SN++;
      if (context->SN > 15u) {
        context->SN = 0u;
      }
      PduInfo.SduLength = context->TpSduLength;
      if (PduInfo.SduLength > length) {
        PduInfo.SduLength = length;
      }
      PduInfo.MetaDataPtr = (uint8_t *)&context->TpSduOffset;
      PduInfo.SduDataPtr = data;

      bufReq = PduR_CanTpCopyRxData(config->PduR_RxPduId, &PduInfo, &bufferSize);

      if (BUFREQ_OK == bufReq) {
        context->TpSduLength -= PduInfo.SduLength;
        context->TpSduOffset += PduInfo.SduLength;
        if (0u == context->TpSduLength) {
          CanTp_ResetToIdle(context);
          PduR_CanTpRxIndication(config->PduR_RxPduId, E_OK);
        } else {
          CanTpSetAlarm(config->N_Cr);
#ifndef CANTP_NO_FC
          if (context->BS > 0u) {
            context->BS--;
            if (0u == context->BS) {
              CanTp_SendFC(RxPduId, N_PCI_CTS);
            }
          }
#endif
        }
      } else {
        CanTp_ResetToIdle(context);
        PduR_CanTpRxIndication(config->PduR_RxPduId, E_NOT_OK);
        ASLOG(CANTPE, ("[%d]CF CopyRxData failed.\n", RxPduId));
      }
    } else {
      CanTp_ResetToIdle(context);
      PduR_CanTpRxIndication(config->PduR_RxPduId, E_NOT_OK);
      ASLOG(CANTPE, ("[%d]Sequence Number Wrong, Abort Current Receiving.\n", RxPduId));
    }
  }
}

#ifndef CANTP_NO_FC
static void CanTp_HandleFC(PduIdType RxPduId, uint8_t pci, uint8_t *data, uint8_t length) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;

  context = &(CANTP_CONFIG->channelContexts[RxPduId]);
  config = &(CANTP_CONFIG->channelConfigs[RxPduId]);

  /* A robust logic to handle cases where TxConfirm occurs after RxInd or is missing. */
  if (CANTP_WAIT_FF_TX_COMPLETED == context->state) {
    ASLOG(CANTPE, ("[%d]FC received before FF TxConfirm\n", RxPduId));
    context->state = CANTP_WAIT_FIRST_FC;
  } else if (CANTP_WAIT_CF_TX_COMPLETED == context->state) {
    if (1u == context->BS) {
      ASLOG(CANTPE, ("[%d]FC received before CF TxConfirm\n", RxPduId));
      context->BS = context->cfgBS;
      context->state = CANTP_WAIT_FC;
      context->WftCounter = 0u;
      CanTpSetAlarm(config->N_Bs);
    }
  } else {
    /* OK, do nothing */
  }

  if ((context->state != CANTP_WAIT_FIRST_FC) && (context->state != CANTP_WAIT_FC)) {
    ASLOG(CANTPE, ("[%d]FC received when in state %d.\n", RxPduId, context->state));
  } else {
    switch ((pci & N_PCI_FS)) {
    case N_PCI_CTS:
      if (length < 2u) {
        ASLOG(CANTPE, ("[%d]FC invalid DLC.\n", RxPduId));
      } else {
        if (CANTP_WAIT_FIRST_FC == context->state) {
          context->cfgBS = data[0];
          context->STmin = data[1];
        }
        context->BS = context->cfgBS;
        CanTpCancelAlarm();
        context->state = CANTP_SEND_CF_START;
      }
      break;
    case N_PCI_WT:
      if (context->WftCounter < 0xFFu) {
        context->WftCounter++;
      }
      if (context->WftCounter > config->CanTpRxWftMax) {
        CanTp_ResetToIdle(context);
        PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
      } else {
        CanTpSetAlarm(config->N_Bs);
      }
      break;
    case N_PCI_OVFLW:
      CanTp_ResetToIdle(context);
      PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
      break;
    default:
      /* do nothing */
      ASLOG(CANTPE, ("[%d]Invalid Flow Control Status.\n", RxPduId));
      break;
    }
  }
}
#endif

static void CanTp_SendSF(PduIdType TxPduId) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  PduLengthType pos = 0;
  PduInfoType PduInfo;
  uint8_t *data;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
  PduLengthType ll_dl;
  PduLengthType offset = 0;
#ifndef CANTP_USE_TRIGGER_TRANSMIT
  Std_ReturnType r;
#endif

  context = &(CANTP_CONFIG->channelContexts[TxPduId]);
  config = &(CANTP_CONFIG->channelConfigs[TxPduId]);
  data = config->data;
  if (CANTP_EXTENDED == config->AddressingFormat) {
    data[pos] = config->N_TA;
    pos++;
  }

  if ((config->LL_DL > 8u) && ((7u - pos) < context->TpSduLength)) {
    data[pos] = N_PCI_SF;
    pos++;
    data[pos] = (uint8_t)context->TpSduLength;
    pos++;
  } else {
    data[pos] = N_PCI_SF | (uint8_t)(context->TpSduLength & 0x7u);
    pos++;
  }

  PduInfo.MetaDataPtr = (uint8_t *)&offset;
  PduInfo.SduDataPtr = &data[pos];
  PduInfo.SduLength = context->TpSduLength;

  bufReq = PduR_CanTpCopyTxData(config->PduR_TxPduId, &PduInfo, NULL, &bufferSize);
  if (BUFREQ_OK == bufReq) {
    pos += context->TpSduLength;
    ll_dl = CanTp_GetDL(pos, config->LL_DL);
    while (pos < ll_dl) {
      data[pos] = config->padding;
      pos++;
    }
    ASLOG(CANTPI, ("[%d]TX data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", TxPduId, data[0],
                   data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
    context->PduInfo.SduDataPtr = data;
    context->PduInfo.SduLength = ll_dl;
#ifdef CANTP_USE_TRIGGER_TRANSMIT
    context->state = CANTP_RESEND_SF;
    CanTpSetAlarm(config->N_As);
#else
    r = CanIf_Transmit(config->CanIfTxPduId, &context->PduInfo);
    if (E_OK == r) {
      STD_TOPIC_ISOTP(TxPduId, FALSE, CanIf_CanTpGetTxCanId(config->CanIfTxPduId),
                      context->PduInfo.SduLength, context->PduInfo.SduDataPtr);
#ifdef CANTP_USE_TX_CONFIRMATION
      context->state = CANTP_WAIT_SF_TX_COMPLETED;
      CanTpSetAlarm(config->N_As);
#else
      CanTp_ResetToIdle(context);
      PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_OK);
#endif
    } else {
      context->state = CANTP_RESEND_SF;
      CanTpSetAlarm(config->N_As);
    }
#endif
  } else {
    ASLOG(CANTPE, ("[%d]SF: failed to provide TX data, reset to idle\n", TxPduId));
    CanTp_ResetToIdle(context);
    PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
  }
}

static void CanTp_SendFF(PduIdType TxPduId) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  PduLengthType pos = 0u;
  PduInfoType PduInfo;
  uint8_t *data;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
#ifndef CANTP_USE_TRIGGER_TRANSMIT
  Std_ReturnType r;
#endif
  PduLengthType offset = 0;

  context = &(CANTP_CONFIG->channelContexts[TxPduId]);
  config = &(CANTP_CONFIG->channelConfigs[TxPduId]);
  data = config->data;
  if (CANTP_EXTENDED == config->AddressingFormat) {
    data[pos] = config->N_TA;
    pos++;
  }

  if ((config->LL_DL > 8u) && (context->TpSduLength > 4095u)) {
    data[pos] = N_PCI_FF;
    pos++;
    data[pos] = 0u;
    pos++;
    data[pos] = (uint8_t)(context->TpSduLength >> 24) & 0xFFu;
    pos++;
    data[pos] = (uint8_t)(context->TpSduLength >> 16) & 0xFFu;
    pos++;
    data[pos] = (uint8_t)(context->TpSduLength >> 8) & 0xFFu;
    pos++;
    data[pos] = (uint8_t)context->TpSduLength & 0xFFu;
    pos++;
  } else {
    data[pos] = N_PCI_FF | (uint8_t)((context->TpSduLength >> 8) & 0x0Fu);
    pos++;
    data[pos] = (uint8_t)context->TpSduLength & 0xFFu;
    pos++;
  }

  PduInfo.MetaDataPtr = (uint8_t *)&offset;
  PduInfo.SduDataPtr = &data[pos];
  PduInfo.SduLength = config->LL_DL - pos;

  bufReq = PduR_CanTpCopyTxData(config->PduR_TxPduId, &PduInfo, NULL, &bufferSize);
  if (BUFREQ_OK == bufReq) {
    context->TpSduLength -= PduInfo.SduLength;
    context->TpSduOffset = PduInfo.SduLength;
    context->PduInfo.SduDataPtr = data;
    context->PduInfo.SduLength = config->LL_DL;
    context->SN = 1;
    ASLOG(CANTPI, ("[%d]TX data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", TxPduId, data[0],
                   data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
#ifdef CANTP_USE_TRIGGER_TRANSMIT
    context->state = CANTP_RESEND_FF;
    CanTpSetAlarm(config->N_As);
#else
    r = CanIf_Transmit(config->CanIfTxPduId, &context->PduInfo);
    if (E_OK == r) {
      STD_TOPIC_ISOTP(TxPduId, FALSE, CanIf_CanTpGetTxCanId(config->CanIfTxPduId),
                      context->PduInfo.SduLength, context->PduInfo.SduDataPtr);
#ifdef CANTP_USE_TX_CONFIRMATION
      context->state = CANTP_WAIT_FF_TX_COMPLETED;
      CanTpSetAlarm(config->N_As);
#else
      context->state = CANTP_WAIT_FIRST_FC;
      context->WftCounter = 0u;
      CanTpSetAlarm(config->N_Bs);
#endif
    } else {
      context->state = CANTP_RESEND_FF;
      CanTpSetAlarm(config->N_As);
    }
#endif
  } else {
    ASLOG(CANTPE, ("[%d]FF: failed to provide TX data, reset to idle\n", TxPduId));
    CanTp_ResetToIdle(context);
    PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
  }
}

static void CanTp_SendCF(PduIdType TxPduId) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  PduLengthType pos = 0u;
  PduInfoType PduInfo;
  uint8_t *data;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
  PduLengthType ll_dl;
#ifndef CANTP_USE_TRIGGER_TRANSMIT
  Std_ReturnType r;
#endif
  context = &(CANTP_CONFIG->channelContexts[TxPduId]);
  config = &(CANTP_CONFIG->channelConfigs[TxPduId]);
  data = config->data;
  if (CANTP_EXTENDED == config->AddressingFormat) {
    data[pos] = config->N_TA;
    pos++;
  }

  data[pos] = N_PCI_CF | context->SN;
  pos++;
  context->SN++;
  if (context->SN > 15u) {
    context->SN = 0u;
  }

  bufferSize = config->LL_DL - pos;

  if (bufferSize > context->TpSduLength) {
    bufferSize = context->TpSduLength;
  }

  PduInfo.MetaDataPtr = (uint8_t *)&context->TpSduOffset;
  PduInfo.SduDataPtr = &data[pos];
  PduInfo.SduLength = bufferSize;

  bufReq = PduR_CanTpCopyTxData(config->PduR_TxPduId, &PduInfo, NULL, &bufferSize);
  if (BUFREQ_OK == bufReq) {
    context->TpSduLength -= PduInfo.SduLength;
    context->TpSduOffset += PduInfo.SduLength;
    pos += PduInfo.SduLength;
    ll_dl = CanTp_GetDL(pos, config->LL_DL);
    while (pos < ll_dl) {
      data[pos] = config->padding;
      pos++;
    }
    ASLOG(CANTPI, ("[%d]TX data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", TxPduId, data[0],
                   data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
    context->PduInfo.SduDataPtr = data;
    context->PduInfo.SduLength = ll_dl;
#ifdef CANTP_USE_TRIGGER_TRANSMIT
    context->state = CANTP_RESEND_CF;
    CanTpSetAlarm(config->N_As);
#else
    r = CanIf_Transmit(config->CanIfTxPduId, &context->PduInfo);
    if (E_OK == r) {
      STD_TOPIC_ISOTP(TxPduId, FALSE, CanIf_CanTpGetTxCanId(config->CanIfTxPduId),
                      context->PduInfo.SduLength, context->PduInfo.SduDataPtr);
      context->state = CANTP_WAIT_CF_TX_COMPLETED;
      CanTpSetAlarm(config->N_As);
    } else {
      context->state = CANTP_RESEND_CF;
      CanTpSetAlarm(config->N_As);
    }
#endif
  } else {
    ASLOG(CANTPE, ("[%d]CF: failed to provide TX data, reset to idle\n", TxPduId));
    CanTp_ResetToIdle(context);
    PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
  }
}

static void CanTp_HandleCFTxCompleted(PduIdType TxPduId) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  context = &(CANTP_CONFIG->channelContexts[TxPduId]);
  config = &(CANTP_CONFIG->channelConfigs[TxPduId]);

  if (context->TpSduLength > 0u) {
    if (context->cfgBS > 0u) {
      if (context->BS > 0u) {
        context->BS--;
        if (0u == context->BS) {
          context->BS = context->cfgBS;
          context->state = CANTP_WAIT_FC;
          context->WftCounter = 0u;
          CanTpSetAlarm(config->N_Bs);
        }
      }
    }
    if (CANTP_WAIT_FC != context->state) {
#ifdef CANTP_USE_TRIGGER_TRANSMIT
      CanTp_SendCF(TxPduId);
#else
      if (context->STmin > 0u) {
        context->state = CANTP_SEND_CF_DELAY;
        CanTpSetAlarm(CANTP_CONVERT_MS_TO_MAIN_CYCLES(context->STmin + CANTP_STMIN_ADJUST));
      } else {
        CanTp_SendCF(TxPduId);
      }
#endif
    }
  } else {
    CanTp_ResetToIdle(context);
    PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_OK);
  }
}

#ifdef CANTP_USE_TRIGGER_TRANSMIT
Std_ReturnType CanTp_ReSend(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
#else
static void CanTp_ReSend(PduIdType TxPduId) {
#endif
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  Std_ReturnType r;

  context = &(CANTP_CONFIG->channelContexts[TxPduId]);
  config = &(CANTP_CONFIG->channelConfigs[TxPduId]);

#ifdef CANTP_USE_TRIGGER_TRANSMIT
  if (PduInfoPtr->SduLength >= context->PduInfo.SduLength) {
    memcpy(PduInfoPtr->SduDataPtr, context->PduInfo.SduDataPtr, context->PduInfo.SduLength);
    if (context->PduInfo.SduLength < PduInfoPtr->SduLength) {
      memset(&PduInfoPtr->SduDataPtr[context->PduInfo.SduLength], config->padding,
             PduInfoPtr->SduLength - context->PduInfo.SduLength);
    }
    r = E_OK;
  } else {
    r = E_NOT_OK;
    ASLOG(CANTPE, ("[%d] ReSend FAIL in state %d\n", TxPduId, context->state));
  }
#else
  r = CanIf_Transmit(config->CanIfTxPduId, &context->PduInfo);
#endif
  if (E_OK == r) {
    STD_TOPIC_ISOTP(TxPduId, FALSE, CanIf_CanTpGetTxCanId(config->CanIfTxPduId),
                    context->PduInfo.SduLength, context->PduInfo.SduDataPtr);
#ifdef CANTP_USE_TX_CONFIRMATION
    if (CANTP_RESEND_SF == context->state) {
      context->state = CANTP_WAIT_SF_TX_COMPLETED;
    } else if (CANTP_RESEND_FF == context->state) {
      context->state = CANTP_WAIT_FF_TX_COMPLETED;
    } else if (CANTP_RESEND_FC_CTS == context->state) {
      context->state = CANTP_WAIT_FC_CTS_TX_COMPLETED;
    } else if (CANTP_RESEND_FC_OVFLW == context->state) {
      context->state = CANTP_WAIT_FC_OVFLW_TX_COMPLETED;
    } else if (CANTP_RESEND_CF == context->state) {
      context->state = CANTP_WAIT_CF_TX_COMPLETED;
    } else {
      ASLOG(CANTPE, ("[%d]resend in wrong state %d, impossible case", TxPduId, context->state));
    }
    CanTpSetAlarm(config->N_As);
#else
    if (CANTP_RESEND_SF == context->state) {
      CanTp_ResetToIdle(context);
      PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_OK);
    } else if (CANTP_RESEND_FF == context->state) {
#if defined(CANTP_NO_FC)
      CanTp_SendCF(TxPduId);
#else
      context->state = CANTP_WAIT_FIRST_FC;
      context->WftCounter = 0u;
      CanTpSetAlarm(config->N_Bs);
#endif
    } else if (CANTP_RESEND_FC_CTS == context->state) {
      context->state = CANTP_WAIT_CF;
      CanTpSetAlarm(config->N_Cr);
    } else if (CANTP_RESEND_FC_OVFLW == context->state) {
      CanTp_ResetToIdle(context);
    } else if (CANTP_RESEND_CF == context->state) {
#ifdef CANTP_USE_TRIGGER_TRANSMIT
      CanTp_HandleCFTxCompleted(TxPduId);
#else
      context->state = CANTP_WAIT_CF_TX_COMPLETED;
      CanTpSetAlarm(config->N_As);
#endif
    } else {
      ASLOG(CANTPE, ("[%d]resend in wrong state %d, impossible case", TxPduId, context->state));
    }
#endif
  }
#ifdef CANTP_USE_TRIGGER_TRANSMIT
  return r;
#endif
}

static Std_ReturnType CanTp_StartToSend(PduIdType TxPduId) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  PduLengthType sfMaxLen;
  Std_ReturnType ret = E_OK;

  context = &(CANTP_CONFIG->channelContexts[TxPduId]);
  config = &(CANTP_CONFIG->channelConfigs[TxPduId]);
  sfMaxLen = CanTp_GetSFMaxLen(config);

  if (sfMaxLen >= context->TpSduLength) {
    CanTp_SendSF(TxPduId);
  } else {
    if (CANTP_PHYSICAL == config->comType) {
      CanTp_SendFF(TxPduId);
    } else {
      /* SWS_CanTp_00093: for communication type is functional, the CanTp module shall reject the
       * request */
      ASLOG(CANTPE, ("response too long %d > %d for functional request\n",
                     (int)context->TpSduLength, (int)sfMaxLen));
      ret = E_NOT_OK;
    }
  }

  return ret;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void CanTp_InitChannel(uint8_t Channel) {
  CanTp_ChannelContextType *context;
  context = &(CANTP_CONFIG->channelContexts[Channel]);
  context->state = CANTP_IDLE;
  CanTpCancelAlarm();
  context->STmin = 0;
  context->PduInfo.MetaDataPtr = NULL;
  context->TpSduLength = 0;
}

void CanTp_Init(const CanTp_ConfigType *CfgPtr) {
  uint8_t i;
#ifdef CANTP_USE_PB_CONFIG
  if (NULL != CfgPtr) {
    CANTP_CONFIG = CfgPtr;
  } else {
    CANTP_CONFIG = &CanTp_Config;
  }
#else
  (void)CfgPtr;
#endif
  for (i = 0u; i < CANTP_CONFIG->numOfChannels; i++) {
    CanTp_InitChannel(i);
  }
}

void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  const CanTp_ChannelConfigType *config;
  uint8_t pci;
  uint8_t *data;
  uint8_t length;
  Std_ReturnType r = E_OK;
  /* @SWS_CanTp_00322 */
  DET_VALIDATE(NULL != CANTP_CONFIG, 0x42, CANTP_E_UNINIT, return);
  DET_VALIDATE((NULL != PduInfoPtr) && (NULL != PduInfoPtr->SduDataPtr), 0x42,
               CANTP_E_PARAM_POINTER, return);

  /* @SWS_CanTp_00359 */
  DET_VALIDATE(RxPduId < CANTP_CONFIG->numOfChannels, 0x42, CANTP_E_INVALID_RX_ID, return);

  ASLOG(CANTPI, ("[%d]RX len=%d data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X], state=%d left=%d\n",
                 RxPduId, PduInfoPtr->SduLength, PduInfoPtr->SduDataPtr[0],
                 PduInfoPtr->SduDataPtr[1], PduInfoPtr->SduDataPtr[2], PduInfoPtr->SduDataPtr[3],
                 PduInfoPtr->SduDataPtr[4], PduInfoPtr->SduDataPtr[5], PduInfoPtr->SduDataPtr[6],
                 PduInfoPtr->SduDataPtr[7], CANTP_CONFIG->channelContexts[RxPduId].state,
                 CANTP_CONFIG->channelContexts[RxPduId].TpSduLength));
  STD_TOPIC_ISOTP(RxPduId, TRUE, CanIf_CanTpGetRxCanId(RxPduId), PduInfoPtr->SduLength,
                  PduInfoPtr->SduDataPtr);
  if (PduInfoPtr->SduLength > 1u) {
    config = &(CANTP_CONFIG->channelConfigs[RxPduId]);

    if (PduInfoPtr->SduLength <= config->LL_DL) {
      if (CANTP_EXTENDED == config->AddressingFormat) {
        if (config->N_TA != PduInfoPtr->SduDataPtr[0]) {
          ASLOG(CANTPI, ("[%d]not for me\n", RxPduId));
          r = E_NOT_OK;
        } else if (PduInfoPtr->SduLength > 2u) {
          pci = PduInfoPtr->SduDataPtr[1];
          data = &PduInfoPtr->SduDataPtr[2];
          length = PduInfoPtr->SduLength - 2u;
        } else {
          ASLOG(CANTPE, ("[%d]invalid DLC\n", RxPduId));
          r = E_NOT_OK;
        }
      } else {
        pci = PduInfoPtr->SduDataPtr[0];
        data = &PduInfoPtr->SduDataPtr[1];
        length = PduInfoPtr->SduLength - 1u;
      }
    } else {
      ASLOG(CANTPE, ("[%d]invalid LL_DL\n", RxPduId));
      r = E_NOT_OK;
    }

    if (E_OK == r) {
      switch (pci & N_PCI_MASK) {
      case N_PCI_SF:
        /* OK for both functional and physical */
        break;
      default:
        if (CANTP_PHYSICAL != config->comType) {
          r = E_NOT_OK; /* only single frame supported for functional communication type */
        }
        break;
      }
    }

    if (E_OK == r) {
      switch (pci & N_PCI_MASK) {
      case N_PCI_SF:
        CanTp_HandleSF(RxPduId, pci, data, length);
        break;
      case N_PCI_FF:
        CanTp_HandleFF(RxPduId, pci, data, length);
        break;
      case N_PCI_CF:
        CanTp_HandleCF(RxPduId, pci, data, length);
        break;
#ifndef CANTP_NO_FC
      case N_PCI_FC:
        CanTp_HandleFC(RxPduId, pci, data, length);
        break;
#endif
      default:
        ASLOG(CANTPE, ("[%d]RX with invalid PCI 0x%02X\n", RxPduId, pci));
        break;
      }
    }
  } else {
    ASLOG(CANTPE, ("[%d]RX with invalid args\n", RxPduId));
  }
}

void CanTp_TxConfirmation(PduIdType TxPduId, Std_ReturnType result) {
#ifdef CANTP_USE_TX_CONFIRMATION
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;

  DET_VALIDATE(NULL != CANTP_CONFIG, 0x40, CANTP_E_UNINIT, return);
  DET_VALIDATE(TxPduId < CANTP_CONFIG->numOfChannels, 0x40, CANTP_E_INVALID_TX_ID, return);

  if (E_OK == result) {
    config = &(CANTP_CONFIG->channelConfigs[TxPduId]);
    context = &(CANTP_CONFIG->channelContexts[TxPduId]);
    switch (context->state) {
    case CANTP_WAIT_SF_TX_COMPLETED:
      CanTp_ResetToIdle(context);
      PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_OK);
      break;
    case CANTP_WAIT_CF_TX_COMPLETED:
      CanTp_HandleCFTxCompleted(TxPduId);
      break;
    case CANTP_WAIT_FF_TX_COMPLETED:
      context->state = CANTP_WAIT_FIRST_FC;
      context->WftCounter = 0;
      CanTpSetAlarm(config->N_Bs);
      break;
    case CANTP_WAIT_FC_CTS_TX_COMPLETED:
      context->state = CANTP_WAIT_CF;
      CanTpSetAlarm(config->N_Cr);
      break;
    case CANTP_WAIT_FC_OVFLW_TX_COMPLETED:
      CanTp_ResetToIdle(context);
      break;
    default:
      break;
    }
  }
#endif
}

Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType r = E_OK;
  CanTp_ChannelContextType *context;

  DET_VALIDATE(NULL != CANTP_CONFIG, 0x49, CANTP_E_UNINIT, return E_NOT_OK);

  /* @SWS_CanTp_00321 */
  DET_VALIDATE((NULL != PduInfoPtr) && (PduInfoPtr->SduLength > 0), 0x49, CANTP_E_PARAM_POINTER,
               return E_NOT_OK);

  /* @SWS_CanTp_00356 */
  DET_VALIDATE(TxPduId < CANTP_CONFIG->numOfChannels, 0x49, CANTP_E_INVALID_TX_ID, return E_NOT_OK);

  context = &(CANTP_CONFIG->channelContexts[TxPduId]);
  if (CANTP_IDLE == context->state) {
    context->TpSduLength = PduInfoPtr->SduLength;
    context->TpSduOffset = 0;
    r = CanTp_StartToSend(TxPduId);
  } else {
    r = E_NOT_OK;
  }

  return r;
}

void CanTp_MainFunction_Channel(uint8_t Channel) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  boolean bTimeout = FALSE;

  DET_VALIDATE(NULL != CANTP_CONFIG, 0x06, CANTP_E_UNINIT, return);

  context = &(CANTP_CONFIG->channelContexts[Channel]);
  config = &(CANTP_CONFIG->channelConfigs[Channel]);

  tpEnterCritical();
  if (CanTpIsAlarmStarted()) {
    CanTpSignalAlarm();
    if (CanTpIsAlarmTimeout()) {
      CanTpCancelAlarm();
      bTimeout = TRUE;
    }
  }
  tpExitCritical();

  if (TRUE == bTimeout) {
    if (CANTP_SEND_CF_DELAY != context->state) {
      ASLOG(CANTPE, ("[%d] timer timeout in state %d\n", Channel, context->state));
    }

    switch (context->state) {
    case CANTP_WAIT_CF:
#ifdef CANTP_USE_TX_CONFIRMATION
    case CANTP_WAIT_FC_CTS_TX_COMPLETED:
#endif
      CanTp_ResetToIdle(context);
      PduR_CanTpRxIndication(config->PduR_RxPduId, E_NOT_OK);
      break;
#ifdef CANTP_USE_TX_CONFIRMATION
    case CANTP_WAIT_FC_OVFLW_TX_COMPLETED:
      CanTp_ResetToIdle(context);
      break;
#endif
    case CANTP_SEND_CF_DELAY:
      CanTp_SendCF((PduIdType)Channel);
      break;
    default:
      CanTp_ResetToIdle(context);
      PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
      break;
    }
  }

  if (CANTP_SEND_CF_START == context->state) { /* FC allow CF */
    CanTp_SendCF((PduIdType)Channel);
  }
}

void CanTp_MainFunction_ChannelFast(uint8_t Channel) {
#ifndef CANTP_USE_TRIGGER_TRANSMIT
  CanTp_ChannelContextType *context;

  DET_VALIDATE(NULL != CANTP_CONFIG, 0x06, CANTP_E_UNINIT, return);
  context = &(CANTP_CONFIG->channelContexts[Channel]);

  switch (context->state) {
  case CANTP_RESEND_SF:
  case CANTP_RESEND_FF:
  case CANTP_RESEND_FC_CTS:
  case CANTP_RESEND_FC_OVFLW:
  case CANTP_RESEND_CF:
    CanTp_ReSend((PduIdType)Channel);
    break;
#ifndef CANTP_USE_TX_CONFIRMATION
  case CANTP_WAIT_CF_TX_COMPLETED:
    CanTp_HandleCFTxCompleted(Channel);
    break;
#endif
  default:
    break;
  }
#endif
}

/* Note: The period of this function is generally 10 ms */
void CanTp_MainFunction(void) {
#if 1 != CANTP_MAIN_FUNCTION_PERIOD
  uint8_t i;
  DET_VALIDATE(NULL != CANTP_CONFIG, 0x06, CANTP_E_UNINIT, return);
  for (i = 0; i < CANTP_CONFIG->numOfChannels; i++) {
    CanTp_MainFunction_Channel(i);
  }
#endif
}

/* Note: The period of this function is generally 1 ms */
void CanTp_MainFunction_Fast(void) {
  uint8_t i;
  DET_VALIDATE(NULL != CANTP_CONFIG, 0x06, CANTP_E_UNINIT, return);
  for (i = 0; i < CANTP_CONFIG->numOfChannels; i++) {
#if 1 == CANTP_MAIN_FUNCTION_PERIOD
    CanTp_MainFunction_Channel(i);
#endif
    CanTp_MainFunction_ChannelFast(i);
  }
}

#ifdef CANTP_USE_TRIGGER_TRANSMIT
PduLengthType CanTp_GetTxPacketLength(PduIdType TxPduId) {
  PduLengthType ret = 0;
  CanTp_ChannelContextType *context;

  DET_VALIDATE(NULL != CANTP_CONFIG, 0xFF, CANTP_E_UNINIT, return 0);

  context = &(CANTP_CONFIG->channelContexts[TxPduId]);
  switch (context->state) {
  case CANTP_RESEND_SF:
  case CANTP_RESEND_FF:
  case CANTP_RESEND_FC_CTS:
  case CANTP_RESEND_FC_OVFLW:
  case CANTP_RESEND_CF:
    ret = context->PduInfo.SduLength;
    break;
  default:
    break;
  }

  return ret;
}

PduLengthType CanTp_GetRxLeftLength(PduIdType RxPduId) {
  PduLengthType ret = 0;
  CanTp_ChannelContextType *context;

  DET_VALIDATE(NULL != CANTP_CONFIG, 0xFE, CANTP_E_UNINIT, return 0);

  context = &(CANTP_CONFIG->channelContexts[RxPduId]);
  switch (context->state) {
  case CANTP_WAIT_CF:
  case CANTP_WAIT_CF_TX_COMPLETED:
    ret = context->TpSduLength;
    break;
  default:
    break;
  }

  return ret;
}

Std_ReturnType CanTp_TriggerTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_OK;
  CanTp_ChannelContextType *context;

  DET_VALIDATE(NULL != CANTP_CONFIG, 0xF0, CANTP_E_UNINIT, return E_NOT_OK);
  /* 0xF0: self defined CanTp API */
  DET_VALIDATE((NULL != PduInfoPtr) && (NULL != PduInfoPtr->SduDataPtr) &&
                 (PduInfoPtr->SduLength > 0),
               0xF0, CANTP_E_PARAM_POINTER, return E_NOT_OK);

  DET_VALIDATE(TxPduId < CANTP_CONFIG->numOfChannels, 0xF0, CANTP_E_INVALID_TX_ID, return E_NOT_OK);

  context = &(CANTP_CONFIG->channelContexts[TxPduId]);
  switch (context->state) {
  case CANTP_RESEND_SF:
  case CANTP_RESEND_FF:
  case CANTP_RESEND_FC_CTS:
  case CANTP_RESEND_FC_OVFLW:
  case CANTP_RESEND_CF:
    ret = CanTp_ReSend(TxPduId, PduInfoPtr);
    break;
  default:
    ASLOG(CANTPI, ("[%d] trigger transmit in state %d\n", TxPduId, context->state));
    ret = E_NOT_OK;
    break;
  }

  return ret;
}
#endif
