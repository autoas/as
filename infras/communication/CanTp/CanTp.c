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
#define CANTP_N_PCI_MASK 0xF0u
#define CANTP_N_PCI_SF 0x00u
#define CANTP_N_PCI_FF 0x10u
#define CANTP_N_PCI_CF 0x20u
#define CANTP_N_PCI_FC 0x30u
#define CANTP_N_PCI_SF_DL 0x0Fu
/* Flow Control Status Mask */
#define CANTP_N_PCI_FS 0x0Fu
/* Flow Control Status */
#define CANTP_N_PCI_CTS 0x00u
#define CANTP_N_PCI_WT 0x01u
#define CANTP_N_PCI_OVFLW 0x02u

#define CANTP_N_PCI_SN 0x0Fu

#ifdef CANTP_FIX_LINK_LAYER_DATA_LENGTH
#define CanTpGetDL(len, linkLayerDataLength) linkLayerDataLength
#endif

#ifdef USE_CANTP_CRITICAL
#define CanTpEnterCritical() EnterCritical()
#define CanTpExitCritical() ExitCritical();
#else
#define CanTpEnterCritical()
#define CanTpExitCritical()
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const CanTp_ConfigType CanTp_Config;

static void CanTp_SendSF(PduIdType txPduId);
#ifndef CANTP_NO_FC
static void CanTp_SendFC(PduIdType txPduId, uint8_t flowStatus);
#endif
static void CanTp_SendCF(PduIdType txPduId);
/* ================================ [ DATAS     ] ============================================== */
#ifdef CANTP_USE_PB_CONFIG
static const CanTp_ConfigType *cantpConfig = NULL;
#endif
/* ================================ [ LOCALS    ] ============================================== */
static void CanTp_ResetToIdle(CanTp_ChannelContextType *context) {
  CanTpCancelAlarm();
  context->State = CANTP_IDLE;
  context->TpSduLength = 0;
}

#ifndef CANTP_FIX_LINK_LAYER_DATA_LENGTH
static PduLengthType CanTpGetDL(PduLengthType len, PduLengthType linkLayerDataLength) {
  PduLengthType dataLength = linkLayerDataLength;
  uint32_t i;
  const PduLengthType linkLayerDataLengths[] = {8, 12, 16, 20, 24, 32, 48};

  for (i = 0; i < ARRAY_SIZE(linkLayerDataLengths); i++) {
    if (len <= linkLayerDataLengths[i]) {
      dataLength = linkLayerDataLengths[i];
      break;
    }
  }
  return dataLength;
}
#endif

static uint8_t CanTp_GetSFMaxLen(const CanTp_ChannelConfigType *config) {
  PduLengthType sfMaxLen;
  if (config->LinkLayerDataLength > 8u) {
    if (CANTP_EXTENDED == config->AddressingFormat) {
      sfMaxLen = config->LinkLayerDataLength - 3u;
    } else {
      sfMaxLen = config->LinkLayerDataLength - 2u;
    }
  } else {
    if (CANTP_EXTENDED == config->AddressingFormat) {
      sfMaxLen = config->LinkLayerDataLength - 2u;
    } else {
      sfMaxLen = config->LinkLayerDataLength - 1u;
    }
  }
  return (uint8_t)sfMaxLen;
}

static BufReq_ReturnType CanTp_RxPreCheckState(CanTp_ChannelContextType *context,
                                               const CanTp_ChannelConfigType *config) {
  BufReq_ReturnType bufReq = BUFREQ_OK;

  if (context->State != CANTP_IDLE) {
    switch (context->State) {
    case CANTP_WAIT_CF:
    case CANTP_RESEND_FC_CTS:
    case CANTP_RESEND_FC_OVFLW:
    case CANTP_WAIT_FC_CTS_TX_COMPLETED:
    case CANTP_WAIT_FC_OVFLW_TX_COMPLETED:
      /* Rx new message when previous Rx is not finished */
      PduR_CanTpRxIndication(config->PduR_RxPduId, E_NOT_OK);
      CanTp_ResetToIdle(context);
      break;
    default:
      /* Rx new message when previous Tx is not finished */
      bufReq = BUFREQ_E_NOT_OK; /* wait it finished */
      break;
    }
  }

  return bufReq;
}

static void CanTp_HandleSF(PduIdType rxPduId, uint8_t pci, uint8_t *data, uint8_t length) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  PduInfoType pduInfo;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
  PduLengthType sfMaxLen;
  PduLengthType offset = 0;
  uint8_t dataLen = length;

  context = &(CANTP_CONFIG->ChannelContexts[rxPduId]);
  config = &(CANTP_CONFIG->ChannelConfigs[rxPduId]);

  bufReq = CanTp_RxPreCheckState(context, config);
  if (BUFREQ_OK == bufReq) {
    sfMaxLen = CanTp_GetSFMaxLen(config);

    pduInfo.SduLength = (PduLengthType)pci & CANTP_N_PCI_SF_DL;
    if ((0u == pduInfo.SduLength) && (config->LinkLayerDataLength > 8u)) {
      pduInfo.SduLength = data[0];
      pduInfo.SduDataPtr = &data[1];
      dataLen -= 1u;
    } else {
      pduInfo.SduDataPtr = data;
    }
    pduInfo.MetaDataPtr = (uint8_t *)&offset;
  }

  if (BUFREQ_OK != bufReq) {
    ASLOG(CANTPE, ("[%d]SF received in state %d!\n", rxPduId, context->State));
  } else if ((pduInfo.SduLength <= sfMaxLen) && (pduInfo.SduLength > 0u) &&
             (pduInfo.SduLength <= dataLen)) {
    bufReq =
      PduR_CanTpStartOfReception(config->PduR_RxPduId, &pduInfo, pduInfo.SduLength, &bufferSize);

    if (BUFREQ_OK == bufReq) {
      bufReq = PduR_CanTpCopyRxData(config->PduR_RxPduId, &pduInfo, &bufferSize);
      if (BUFREQ_OK != bufReq) {
        PduR_CanTpRxIndication(config->PduR_RxPduId, E_NOT_OK);
      }
    }

    if (BUFREQ_OK == bufReq) {
      PduR_CanTpRxIndication(config->PduR_RxPduId, E_OK);
    }

    if (BUFREQ_OK != bufReq) {
      ASLOG(CANTPE, ("[%d]SF received with buffer status %d!: SID=%x\n", rxPduId, bufReq,
                     pduInfo.SduDataPtr[1]));
    }
  } else {
    ASLOG(CANTPE, ("[%d]SF received with invalid len: %d %d %d!\n", rxPduId, (int)pduInfo.SduLength,
                   (int)sfMaxLen, (int)length));
  }
}

#ifndef CANTP_NO_FC
static void CanTp_SendFC(PduIdType rxPduId, uint8_t flowStatus) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
#ifndef CANTP_USE_TRIGGER_TRANSMIT
  Std_ReturnType r;
#endif
  uint8_t *data;
  uint8_t pos = 0;

  context = &(CANTP_CONFIG->ChannelContexts[rxPduId]);
  config = &(CANTP_CONFIG->ChannelConfigs[rxPduId]);

  data = config->Data;

  if (CANTP_EXTENDED == config->AddressingFormat) {
    data[pos] = config->N_TA;
    pos++;
  }

  data[pos] = CANTP_N_PCI_FC | flowStatus;
  pos++;
  data[pos] = config->BS;
  pos++;
  data[pos] = config->STmin;
  pos++;
  while (pos < 8u) {
    data[pos] = config->Padding;
    pos++;
  }
  ASLOG(CANTPI, ("[%d]TX len=%d data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", rxPduId, 8,
                 data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
  context->PduInfo.SduDataPtr = data;
  context->PduInfo.SduLength = 8;

#ifdef CANTP_USE_TRIGGER_TRANSMIT
  CanTpSetAlarm(config->N_As);
  if (CANTP_N_PCI_CTS == flowStatus) {
    context->State = CANTP_RESEND_FC_CTS;
  } else {
    context->State = CANTP_RESEND_FC_OVFLW;
  }
#else
  r = CanIf_Transmit(config->CanIfTxPduId, &context->PduInfo);
  if (E_OK == r) {
    STD_TOPIC_ISOTP(rxPduId, FALSE, CanIf_CanTpGetTxCanId(config->CanIfTxPduId),
                    context->PduInfo.SduLength, context->PduInfo.SduDataPtr);
    if (CANTP_N_PCI_CTS == flowStatus) {
#ifdef CANTP_USE_TX_CONFIRMATION
      context->State = CANTP_WAIT_FC_CTS_TX_COMPLETED;
#else
      context->State = CANTP_WAIT_CF;
#endif
      CanTpSetAlarm(config->N_Cr);
    } else {
#ifdef CANTP_USE_TX_CONFIRMATION
      context->State = CANTP_WAIT_FC_OVFLW_TX_COMPLETED;
#else
      CanTp_ResetToIdle(context);
#endif
    }
    context->BS = config->BS;
  } else {
    CanTpSetAlarm(config->N_As);
    if (CANTP_N_PCI_CTS == flowStatus) {
      context->State = CANTP_RESEND_FC_CTS;
    } else {
      context->State = CANTP_RESEND_FC_OVFLW;
    }
  }
#endif
}
#endif

static void CanTp_HandleFF(PduIdType rxPduId, uint8_t pci, uint8_t *data, uint8_t length) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  PduInfoType pduInfo;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
  PduLengthType ffLen;
  PduLengthType sfMaxLen;
  uint32_t tpSduLength;
  PduLengthType offset = 0;

  context = &(CANTP_CONFIG->ChannelContexts[rxPduId]);
  config = &(CANTP_CONFIG->ChannelConfigs[rxPduId]);

  bufReq = CanTp_RxPreCheckState(context, config);
  if (BUFREQ_OK == bufReq) {
    tpSduLength = (((uint32_t)pci & 0x0Fu) << 8) + data[0];
    pduInfo.SduDataPtr = &data[1];

    pduInfo.SduLength = (PduLengthType)length - 1u;
    if (CANTP_EXTENDED == config->AddressingFormat) {
      ffLen = config->LinkLayerDataLength - 3u;
    } else {
      ffLen = config->LinkLayerDataLength - 2u;
    }

    if ((config->LinkLayerDataLength > 8u) && (0u == tpSduLength)) {
      tpSduLength = ((uint32_t)data[2] << 24) + ((uint32_t)data[3] << 16) +
                    ((uint32_t)data[4] << 8) + ((uint32_t)data[5]);
      pduInfo.SduDataPtr = &data[6];
      pduInfo.SduLength -= 4u;
      ffLen -= 4u;
    }
    pduInfo.MetaDataPtr = (uint8_t *)&offset;

    sfMaxLen = CanTp_GetSFMaxLen(config);
  }

  if (BUFREQ_OK != bufReq) {
    ASLOG(CANTPE, ("[%d] FF received in state %d, ignore!\n", rxPduId, context->State));
  } else if (tpSduLength > PDU_LENGTH_MAX) {
    ASLOG(CANTPE, ("[%d]FF size too big %u!\n", rxPduId, tpSduLength));
  } else if ((tpSduLength <= ffLen) || (tpSduLength <= sfMaxLen)) {
    ASLOG(CANTPE, ("[%d]FF size invalid %u(<=%d,%d)!\n", rxPduId, tpSduLength, ffLen, sfMaxLen));
  } else if (pduInfo.SduLength == ffLen) {
    bufReq = PduR_CanTpStartOfReception(config->PduR_RxPduId, &pduInfo, (PduLengthType)tpSduLength,
                                        &bufferSize);
    if (BUFREQ_OK == bufReq) {
      bufReq = PduR_CanTpCopyRxData(config->PduR_RxPduId, &pduInfo, &bufferSize);

      if (BUFREQ_OK != bufReq) {
        PduR_CanTpRxIndication(config->PduR_RxPduId, E_NOT_OK);
      }
    }

    if (BUFREQ_OK != bufReq) {
#ifdef CANTP_NO_FC
      ASLOG(CANTPE, ("[%d]FF received with buffer status %d!\n", rxPduId, bufReq));
      CanTp_ResetToIdle(context);
#else
      CanTp_SendFC(rxPduId, CANTP_N_PCI_OVFLW);
#endif
    } else {
      context->TpSduLength = (PduLengthType)tpSduLength - ffLen;
      context->TpSduOffset = ffLen;
      context->SN = 1;
#ifdef CANTP_NO_FC
      context->State = CANTP_WAIT_CF;
      CanTpSetAlarm(config->N_Cr);
      context->BS = 0;
#else
      CanTp_SendFC(rxPduId, CANTP_N_PCI_CTS);
#endif
    }
  } else {
    ASLOG(CANTPE,
          ("[%d]FF received with invalid len %d(!=%d)!\n", rxPduId, pduInfo.SduLength, ffLen));
  }
}

static void CanTp_HandleCF(PduIdType rxPduId, uint8_t pci, uint8_t *data, uint8_t length) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  PduInfoType pduInfo;
  BufReq_ReturnType bufReq = BUFREQ_OK;
  PduLengthType bufferSize;
  PduLengthType cfLen;

  context = &(CANTP_CONFIG->ChannelContexts[rxPduId]);
  config = &(CANTP_CONFIG->ChannelConfigs[rxPduId]);

  /* A robust logic to handle cases where TxConfirm occurs after RxInd or is missing. */
  if (CANTP_WAIT_FC_CTS_TX_COMPLETED == context->State) {
    ASLOG(CANTPE, ("[%d]CF received before FC CTS TxConfirm\n", rxPduId));
    context->State = CANTP_WAIT_CF;
  }

  if (CANTP_EXTENDED == config->AddressingFormat) {
    cfLen = config->LinkLayerDataLength - 2u;
  } else {
    cfLen = config->LinkLayerDataLength - 1u;
  }

  if (context->State != CANTP_WAIT_CF) {
    ASLOG(CANTPE, ("[%d]CF received when in state %d.\n", rxPduId, context->State));
    bufReq = BUFREQ_E_NOT_OK;
  } else if (context->TpSduLength > length) {
    if (length != cfLen) {
      bufReq = BUFREQ_E_NOT_OK;
    }
  } else {
    /* OK */
  }
  if (BUFREQ_OK == bufReq) {
    if (context->SN == (pci & CANTP_N_PCI_SN)) {
      context->SN++;
      if (context->SN > 15u) {
        context->SN = 0u;
      }
      pduInfo.SduLength = context->TpSduLength;
      if (pduInfo.SduLength > length) {
        pduInfo.SduLength = length;
      }
      pduInfo.MetaDataPtr = (uint8_t *)&context->TpSduOffset;
      pduInfo.SduDataPtr = data;

      bufReq = PduR_CanTpCopyRxData(config->PduR_RxPduId, &pduInfo, &bufferSize);

      if (BUFREQ_OK == bufReq) {
        context->TpSduLength -= pduInfo.SduLength;
        context->TpSduOffset += pduInfo.SduLength;
        if (0u == context->TpSduLength) {
          CanTp_ResetToIdle(context);
          PduR_CanTpRxIndication(config->PduR_RxPduId, E_OK);
        } else {
          CanTpSetAlarm(config->N_Cr);
#ifndef CANTP_NO_FC
          if (context->BS > 0u) {
            context->BS--;
            if (0u == context->BS) {
              CanTp_SendFC(rxPduId, CANTP_N_PCI_CTS);
            }
          }
#endif
        }
      } else {
        CanTp_ResetToIdle(context);
        PduR_CanTpRxIndication(config->PduR_RxPduId, E_NOT_OK);
        ASLOG(CANTPE, ("[%d]CF CopyRxData failed.\n", rxPduId));
      }
    } else {
      CanTp_ResetToIdle(context);
      PduR_CanTpRxIndication(config->PduR_RxPduId, E_NOT_OK);
      ASLOG(CANTPE, ("[%d]Sequence Number Wrong, Abort Current Receiving.\n", rxPduId));
    }
  }
}

#ifndef CANTP_NO_FC
static void CanTp_HandleFC(PduIdType rxPduId, uint8_t pci, uint8_t *data, uint8_t length) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;

  context = &(CANTP_CONFIG->ChannelContexts[rxPduId]);
  config = &(CANTP_CONFIG->ChannelConfigs[rxPduId]);

  /* A robust logic to handle cases where TxConfirm occurs after RxInd or is missing. */
  if (CANTP_WAIT_FF_TX_COMPLETED == context->State) {
    ASLOG(CANTPE, ("[%d]FC received before FF TxConfirm\n", rxPduId));
    context->State = CANTP_WAIT_FIRST_FC;
  } else if (CANTP_WAIT_CF_TX_COMPLETED == context->State) {
    if (1u == context->BS) {
      ASLOG(CANTPE, ("[%d]FC received before CF TxConfirm\n", rxPduId));
      context->BS = context->CfgBS;
      context->State = CANTP_WAIT_FC;
      context->WftCounter = 0u;
      CanTpSetAlarm(config->N_Bs);
    }
  } else {
    /* OK, do nothing */
  }

  if ((context->State != CANTP_WAIT_FIRST_FC) && (context->State != CANTP_WAIT_FC)) {
    ASLOG(CANTPE, ("[%d]FC received when in state %d.\n", rxPduId, context->State));
  } else {
    switch ((pci & CANTP_N_PCI_FS)) {
    case CANTP_N_PCI_CTS:
      if (length < 2u) {
        ASLOG(CANTPE, ("[%d]FC invalid DLC.\n", rxPduId));
      } else {
        if (CANTP_WAIT_FIRST_FC == context->State) {
          context->CfgBS = data[0];
          context->STmin = data[1];
        }
        context->BS = context->CfgBS;
        CanTpCancelAlarm();
        context->State = CANTP_SEND_CF_START;
      }
      break;
    case CANTP_N_PCI_WT:
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
    case CANTP_N_PCI_OVFLW:
      CanTp_ResetToIdle(context);
      PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
      break;
    default:
      /* do nothing */
      ASLOG(CANTPE, ("[%d]Invalid Flow Control Status.\n", rxPduId));
      break;
    }
  }
}
#endif

static void CanTp_SendSF(PduIdType txPduId) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  PduLengthType pos = 0;
  PduInfoType pduInfo;
  uint8_t *data;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
  PduLengthType linkLayerDataLength;
  PduLengthType offset = 0;
#ifndef CANTP_USE_TRIGGER_TRANSMIT
  Std_ReturnType r;
#endif

  context = &(CANTP_CONFIG->ChannelContexts[txPduId]);
  config = &(CANTP_CONFIG->ChannelConfigs[txPduId]);
  data = config->Data;
  if (CANTP_EXTENDED == config->AddressingFormat) {
    data[pos] = config->N_TA;
    pos++;
  }

  if ((config->LinkLayerDataLength > 8u) && ((7u - pos) < context->TpSduLength)) {
    data[pos] = CANTP_N_PCI_SF;
    pos++;
    data[pos] = (uint8_t)context->TpSduLength;
    pos++;
  } else {
    data[pos] = CANTP_N_PCI_SF | (uint8_t)(context->TpSduLength & 0x7u);
    pos++;
  }

  pduInfo.MetaDataPtr = (uint8_t *)&offset;
  pduInfo.SduDataPtr = &data[pos];
  pduInfo.SduLength = context->TpSduLength;

  bufReq = PduR_CanTpCopyTxData(config->PduR_TxPduId, &pduInfo, NULL, &bufferSize);
  if (BUFREQ_OK == bufReq) {
    pos += context->TpSduLength;
    linkLayerDataLength = CanTpGetDL(pos, config->LinkLayerDataLength);
    while (pos < linkLayerDataLength) {
      data[pos] = config->Padding;
      pos++;
    }
    ASLOG(CANTPI, ("[%d]TX len=%d data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", txPduId,
                   linkLayerDataLength, data[0], data[1], data[2], data[3], data[4], data[5],
                   data[6], data[7]));
    context->PduInfo.SduDataPtr = data;
    context->PduInfo.SduLength = linkLayerDataLength;
#ifdef CANTP_USE_TRIGGER_TRANSMIT
    context->State = CANTP_RESEND_SF;
    CanTpSetAlarm(config->N_As);
#else
    r = CanIf_Transmit(config->CanIfTxPduId, &context->PduInfo);
    if (E_OK == r) {
      STD_TOPIC_ISOTP(txPduId, FALSE, CanIf_CanTpGetTxCanId(config->CanIfTxPduId),
                      context->PduInfo.SduLength, context->PduInfo.SduDataPtr);
#ifdef CANTP_USE_TX_CONFIRMATION
      context->State = CANTP_WAIT_SF_TX_COMPLETED;
      CanTpSetAlarm(config->N_As);
#else
      CanTp_ResetToIdle(context);
      PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_OK);
#endif
    } else {
      context->State = CANTP_RESEND_SF;
      CanTpSetAlarm(config->N_As);
    }
#endif
  } else {
    ASLOG(CANTPE, ("[%d]SF: failed to provide TX data, reset to idle\n", txPduId));
    CanTp_ResetToIdle(context);
    PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
  }
}

static void CanTp_SendFF(PduIdType txPduId) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  PduLengthType pos = 0u;
  PduInfoType pduInfo;
  uint8_t *data;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
#ifndef CANTP_USE_TRIGGER_TRANSMIT
  Std_ReturnType r;
#endif
  PduLengthType offset = 0;

  context = &(CANTP_CONFIG->ChannelContexts[txPduId]);
  config = &(CANTP_CONFIG->ChannelConfigs[txPduId]);
  data = config->Data;
  if (CANTP_EXTENDED == config->AddressingFormat) {
    data[pos] = config->N_TA;
    pos++;
  }

  if ((config->LinkLayerDataLength > 8u) && (context->TpSduLength > 4095u)) {
    data[pos] = CANTP_N_PCI_FF;
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
    data[pos] = CANTP_N_PCI_FF | (uint8_t)((context->TpSduLength >> 8) & 0x0Fu);
    pos++;
    data[pos] = (uint8_t)context->TpSduLength & 0xFFu;
    pos++;
  }

  pduInfo.MetaDataPtr = (uint8_t *)&offset;
  pduInfo.SduDataPtr = &data[pos];
  pduInfo.SduLength = config->LinkLayerDataLength - pos;

  bufReq = PduR_CanTpCopyTxData(config->PduR_TxPduId, &pduInfo, NULL, &bufferSize);
  if (BUFREQ_OK == bufReq) {
    context->TpSduLength -= pduInfo.SduLength;
    context->TpSduOffset = pduInfo.SduLength;
    context->PduInfo.SduDataPtr = data;
    context->PduInfo.SduLength = config->LinkLayerDataLength;
    context->SN = 1;
    ASLOG(CANTPI, ("[%d]TX len=%d data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", txPduId,
                   config->LinkLayerDataLength, data[0], data[1], data[2], data[3], data[4],
                   data[5], data[6], data[7]));
#ifdef CANTP_USE_TRIGGER_TRANSMIT
    context->State = CANTP_RESEND_FF;
    CanTpSetAlarm(config->N_As);
#else
    r = CanIf_Transmit(config->CanIfTxPduId, &context->PduInfo);
    if (E_OK == r) {
      STD_TOPIC_ISOTP(txPduId, FALSE, CanIf_CanTpGetTxCanId(config->CanIfTxPduId),
                      context->PduInfo.SduLength, context->PduInfo.SduDataPtr);
#ifdef CANTP_USE_TX_CONFIRMATION
      context->State = CANTP_WAIT_FF_TX_COMPLETED;
      CanTpSetAlarm(config->N_As);
#else
      context->State = CANTP_WAIT_FIRST_FC;
      context->WftCounter = 0u;
      CanTpSetAlarm(config->N_Bs);
#endif
    } else {
      context->State = CANTP_RESEND_FF;
      CanTpSetAlarm(config->N_As);
    }
#endif
  } else {
    ASLOG(CANTPE, ("[%d]FF: failed to provide TX data, reset to idle\n", txPduId));
    CanTp_ResetToIdle(context);
    PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
  }
}

static void CanTp_SendCF(PduIdType txPduId) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  PduLengthType pos = 0u;
  PduInfoType pduInfo;
  uint8_t *data;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
  PduLengthType linkLayerDataLength;
#ifndef CANTP_USE_TRIGGER_TRANSMIT
  Std_ReturnType r;
#endif
  context = &(CANTP_CONFIG->ChannelContexts[txPduId]);
  config = &(CANTP_CONFIG->ChannelConfigs[txPduId]);
  data = config->Data;
  if (CANTP_EXTENDED == config->AddressingFormat) {
    data[pos] = config->N_TA;
    pos++;
  }

  data[pos] = CANTP_N_PCI_CF | context->SN;
  pos++;
  context->SN++;
  if (context->SN > 15u) {
    context->SN = 0u;
  }

  bufferSize = config->LinkLayerDataLength - pos;

  if (bufferSize > context->TpSduLength) {
    bufferSize = context->TpSduLength;
  }

  pduInfo.MetaDataPtr = (uint8_t *)&context->TpSduOffset;
  pduInfo.SduDataPtr = &data[pos];
  pduInfo.SduLength = bufferSize;

  bufReq = PduR_CanTpCopyTxData(config->PduR_TxPduId, &pduInfo, NULL, &bufferSize);
  if (BUFREQ_OK == bufReq) {
    context->TpSduLength -= pduInfo.SduLength;
    context->TpSduOffset += pduInfo.SduLength;
    pos += pduInfo.SduLength;
    linkLayerDataLength = CanTpGetDL(pos, config->LinkLayerDataLength);
    while (pos < linkLayerDataLength) {
      data[pos] = config->Padding;
      pos++;
    }
    ASLOG(CANTPI, ("[%d]TX len=%d data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", txPduId,
                   linkLayerDataLength, data[0], data[1], data[2], data[3], data[4], data[5],
                   data[6], data[7]));
    context->PduInfo.SduDataPtr = data;
    context->PduInfo.SduLength = linkLayerDataLength;
#ifdef CANTP_USE_TRIGGER_TRANSMIT
    context->State = CANTP_RESEND_CF;
    CanTpSetAlarm(config->N_As);
#else
    r = CanIf_Transmit(config->CanIfTxPduId, &context->PduInfo);
    if (E_OK == r) {
      STD_TOPIC_ISOTP(txPduId, FALSE, CanIf_CanTpGetTxCanId(config->CanIfTxPduId),
                      context->PduInfo.SduLength, context->PduInfo.SduDataPtr);
      context->State = CANTP_WAIT_CF_TX_COMPLETED;
      CanTpSetAlarm(config->N_As);
    } else {
      context->State = CANTP_RESEND_CF;
      CanTpSetAlarm(config->N_As);
    }
#endif
  } else {
    ASLOG(CANTPE, ("[%d]CF: failed to provide TX data, reset to idle\n", txPduId));
    CanTp_ResetToIdle(context);
    PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
  }
}

static void CanTp_HandleCFTxCompleted(PduIdType txPduId) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  context = &(CANTP_CONFIG->ChannelContexts[txPduId]);
  config = &(CANTP_CONFIG->ChannelConfigs[txPduId]);

  if (context->TpSduLength > 0u) {
    if (context->CfgBS > 0u) {
      if (context->BS > 0u) {
        context->BS--;
        if (0u == context->BS) {
          context->BS = context->CfgBS;
          context->State = CANTP_WAIT_FC;
          context->WftCounter = 0u;
          CanTpSetAlarm(config->N_Bs);
        }
      }
    }
    if (CANTP_WAIT_FC != context->State) {
#ifdef CANTP_USE_TRIGGER_TRANSMIT
      CanTp_SendCF(txPduId);
#else
      if (context->STmin > 0u) {
        context->State = CANTP_SEND_CF_DELAY;
        CanTpSetAlarm(CANTP_CONVERT_MS_TO_MAIN_CYCLES(context->STmin + CANTP_STMIN_ADJUST));
      } else {
        CanTp_SendCF(txPduId);
      }
#endif
    }
  } else {
    CanTp_ResetToIdle(context);
    PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_OK);
  }
}

#ifdef CANTP_USE_TRIGGER_TRANSMIT
Std_ReturnType CanTp_ReSend(PduIdType txPduId, const PduInfoType *pduInfoPtr) {
#else
static void CanTp_ReSend(PduIdType txPduId) {
#endif
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  Std_ReturnType r;

  context = &(CANTP_CONFIG->ChannelContexts[txPduId]);
  config = &(CANTP_CONFIG->ChannelConfigs[txPduId]);

#ifdef CANTP_USE_TRIGGER_TRANSMIT
  if (pduInfoPtr->SduLength >= context->PduInfo.SduLength) {
    memcpy(pduInfoPtr->SduDataPtr, context->PduInfo.SduDataPtr, context->PduInfo.SduLength);
    if (context->PduInfo.SduLength < pduInfoPtr->SduLength) {
      memset(&pduInfoPtr->SduDataPtr[context->PduInfo.SduLength], config->Padding,
             pduInfoPtr->SduLength - context->PduInfo.SduLength);
    }
    r = E_OK;
  } else {
    r = E_NOT_OK;
    ASLOG(CANTPE, ("[%d] ReSend FAIL in state %d\n", txPduId, context->State));
  }
#else
  r = CanIf_Transmit(config->CanIfTxPduId, &context->PduInfo);
#endif
  if (E_OK == r) {
    STD_TOPIC_ISOTP(txPduId, FALSE, CanIf_CanTpGetTxCanId(config->CanIfTxPduId),
                    context->PduInfo.SduLength, context->PduInfo.SduDataPtr);
#ifdef CANTP_USE_TX_CONFIRMATION
    if (CANTP_RESEND_SF == context->State) {
      context->State = CANTP_WAIT_SF_TX_COMPLETED;
    } else if (CANTP_RESEND_FF == context->State) {
      context->State = CANTP_WAIT_FF_TX_COMPLETED;
    } else if (CANTP_RESEND_FC_CTS == context->State) {
      context->State = CANTP_WAIT_FC_CTS_TX_COMPLETED;
    } else if (CANTP_RESEND_FC_OVFLW == context->State) {
      context->State = CANTP_WAIT_FC_OVFLW_TX_COMPLETED;
    } else if (CANTP_RESEND_CF == context->State) {
      context->State = CANTP_WAIT_CF_TX_COMPLETED;
    } else {
      ASLOG(CANTPE, ("[%d]resend in wrong state %d, impossible case", txPduId, context->State));
    }
    CanTpSetAlarm(config->N_As);
#else
    if (CANTP_RESEND_SF == context->State) {
      CanTp_ResetToIdle(context);
      PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_OK);
    } else if (CANTP_RESEND_FF == context->State) {
#if defined(CANTP_NO_FC)
      CanTp_SendCF(txPduId);
#else
      context->State = CANTP_WAIT_FIRST_FC;
      context->WftCounter = 0u;
      CanTpSetAlarm(config->N_Bs);
#endif
    } else if (CANTP_RESEND_FC_CTS == context->State) {
      context->State = CANTP_WAIT_CF;
      CanTpSetAlarm(config->N_Cr);
    } else if (CANTP_RESEND_FC_OVFLW == context->State) {
      CanTp_ResetToIdle(context);
    } else if (CANTP_RESEND_CF == context->State) {
#ifdef CANTP_USE_TRIGGER_TRANSMIT
      CanTp_HandleCFTxCompleted(txPduId);
#else
      context->State = CANTP_WAIT_CF_TX_COMPLETED;
      CanTpSetAlarm(config->N_As);
#endif
    } else {
      ASLOG(CANTPE, ("[%d]resend in wrong state %d, impossible case", txPduId, context->State));
    }
#endif
  }
#ifdef CANTP_USE_TRIGGER_TRANSMIT
  return r;
#endif
}

static Std_ReturnType CanTp_StartToSend(PduIdType txPduId) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  PduLengthType sfMaxLen;
  Std_ReturnType ret = E_OK;

  context = &(CANTP_CONFIG->ChannelContexts[txPduId]);
  config = &(CANTP_CONFIG->ChannelConfigs[txPduId]);
  sfMaxLen = CanTp_GetSFMaxLen(config);

  if (sfMaxLen >= context->TpSduLength) {
    CanTp_SendSF(txPduId);
  } else {
    if (CANTP_PHYSICAL == config->ComType) {
      CanTp_SendFF(txPduId);
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
void CanTp_InitChannel(uint8_t channel) {
  CanTp_ChannelContextType *context;
  context = &(CANTP_CONFIG->ChannelContexts[channel]);
  memset(context, 0, sizeof(*context));
  context->State = CANTP_IDLE;
  CanTpCancelAlarm();
  context->STmin = 0;
  context->PduInfo.MetaDataPtr = NULL;
  context->TpSduLength = 0;
}

void CanTp_Init(const CanTp_ConfigType *cfgPtr) {
  uint8_t i;
#ifdef CANTP_USE_PB_CONFIG
  if (NULL != cfgPtr) {
    CANTP_CONFIG = cfgPtr;
  } else {
    CANTP_CONFIG = &CanTp_Config;
  }
#else
  (void)cfgPtr;
#endif
  for (i = 0u; i < CANTP_CONFIG->NumOfChannels; i++) {
    CanTp_InitChannel(i);
  }
}

void CanTp_RxIndication(PduIdType rxPduId, const PduInfoType *pduInfoPtr) {
  const CanTp_ChannelConfigType *config;
  uint8_t pci;
  uint8_t *data;
  uint8_t length;
  Std_ReturnType r = E_OK;
  /* @SWS_CanTp_00322 */
  DET_VALIDATE(NULL != CANTP_CONFIG, 0x42, CANTP_E_UNINIT, return);
  DET_VALIDATE((NULL != pduInfoPtr) && (NULL != pduInfoPtr->SduDataPtr), 0x42,
               CANTP_E_PARAM_POINTER, return);

  /* @SWS_CanTp_00359 */
  DET_VALIDATE(rxPduId < CANTP_CONFIG->NumOfChannels, 0x42, CANTP_E_INVALID_RX_ID, return);

  ASLOG(CANTPI, ("[%d]RX len=%d data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X], state=%d left=%d\n",
                 rxPduId, pduInfoPtr->SduLength, pduInfoPtr->SduDataPtr[0],
                 pduInfoPtr->SduDataPtr[1], pduInfoPtr->SduDataPtr[2], pduInfoPtr->SduDataPtr[3],
                 pduInfoPtr->SduDataPtr[4], pduInfoPtr->SduDataPtr[5], pduInfoPtr->SduDataPtr[6],
                 pduInfoPtr->SduDataPtr[7], CANTP_CONFIG->ChannelContexts[rxPduId].State,
                 CANTP_CONFIG->ChannelContexts[rxPduId].TpSduLength));
  STD_TOPIC_ISOTP(rxPduId, TRUE, CanIf_CanTpGetRxCanId(rxPduId), pduInfoPtr->SduLength,
                  pduInfoPtr->SduDataPtr);
  if (pduInfoPtr->SduLength > 1u) {
    config = &(CANTP_CONFIG->ChannelConfigs[rxPduId]);

    if (pduInfoPtr->SduLength <= config->LinkLayerDataLength) {
      if (CANTP_EXTENDED == config->AddressingFormat) {
        if (config->N_TA != pduInfoPtr->SduDataPtr[0]) {
          ASLOG(CANTPI, ("[%d]not for me\n", rxPduId));
          r = E_NOT_OK;
        } else if (pduInfoPtr->SduLength > 2u) {
          pci = pduInfoPtr->SduDataPtr[1];
          data = &pduInfoPtr->SduDataPtr[2];
          length = pduInfoPtr->SduLength - 2u;
        } else {
          ASLOG(CANTPE, ("[%d]invalid DLC\n", rxPduId));
          r = E_NOT_OK;
        }
      } else {
        pci = pduInfoPtr->SduDataPtr[0];
        data = &pduInfoPtr->SduDataPtr[1];
        length = pduInfoPtr->SduLength - 1u;
      }
    } else {
      ASLOG(CANTPE, ("[%d]invalid LinkLayerDataLength\n", rxPduId));
      r = E_NOT_OK;
    }

    if (E_OK == r) {
      switch (pci & CANTP_N_PCI_MASK) {
      case CANTP_N_PCI_SF:
        /* OK for both functional and physical */
        break;
      default:
        if (CANTP_PHYSICAL != config->ComType) {
          r = E_NOT_OK; /* only single frame supported for functional communication type */
        }
        break;
      }
    }

    if (CanTpGetDL(pduInfoPtr->SduLength, config->LinkLayerDataLength) != pduInfoPtr->SduLength) {
      ASLOG(CANTPE, ("[%d]RX with invalid DLC\n", rxPduId));
      r = E_NOT_OK;
    }

    if (E_OK == r) {
      switch (pci & CANTP_N_PCI_MASK) {
      case CANTP_N_PCI_SF:
        CanTp_HandleSF(rxPduId, pci, data, length);
        break;
      case CANTP_N_PCI_FF:
        CanTp_HandleFF(rxPduId, pci, data, length);
        break;
      case CANTP_N_PCI_CF:
        CanTp_HandleCF(rxPduId, pci, data, length);
        break;
#ifndef CANTP_NO_FC
      case CANTP_N_PCI_FC:
        CanTp_HandleFC(rxPduId, pci, data, length);
        break;
#endif
      default:
        ASLOG(CANTPE, ("[%d]RX with invalid PCI 0x%02X\n", rxPduId, pci));
        break;
      }
    }
  } else {
    ASLOG(CANTPE, ("[%d]RX with invalid args\n", rxPduId));
  }
}

void CanTp_TxConfirmation(PduIdType txPduId, Std_ReturnType result) {
#ifdef CANTP_USE_TX_CONFIRMATION
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;

  DET_VALIDATE(NULL != CANTP_CONFIG, 0x40, CANTP_E_UNINIT, return);
  DET_VALIDATE(txPduId < CANTP_CONFIG->NumOfChannels, 0x40, CANTP_E_INVALID_TX_ID, return);

  if (E_OK == result) {
    config = &(CANTP_CONFIG->ChannelConfigs[txPduId]);
    context = &(CANTP_CONFIG->ChannelContexts[txPduId]);
    switch (context->State) {
    case CANTP_WAIT_SF_TX_COMPLETED:
      CanTp_ResetToIdle(context);
      PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_OK);
      break;
    case CANTP_WAIT_CF_TX_COMPLETED:
      CanTp_HandleCFTxCompleted(txPduId);
      break;
    case CANTP_WAIT_FF_TX_COMPLETED:
      context->State = CANTP_WAIT_FIRST_FC;
      context->WftCounter = 0;
      CanTpSetAlarm(config->N_Bs);
      break;
    case CANTP_WAIT_FC_CTS_TX_COMPLETED:
      context->State = CANTP_WAIT_CF;
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

Std_ReturnType CanTp_Transmit(PduIdType txPduId, const PduInfoType *pduInfoPtr) {
  Std_ReturnType r = E_OK;
  CanTp_ChannelContextType *context;

  DET_VALIDATE(NULL != CANTP_CONFIG, 0x49, CANTP_E_UNINIT, return E_NOT_OK);

  /* @SWS_CanTp_00321 */
  DET_VALIDATE((NULL != pduInfoPtr) && (pduInfoPtr->SduLength > 0), 0x49, CANTP_E_PARAM_POINTER,
               return E_NOT_OK);

  /* @SWS_CanTp_00356 */
  DET_VALIDATE(txPduId < CANTP_CONFIG->NumOfChannels, 0x49, CANTP_E_INVALID_TX_ID, return E_NOT_OK);

  context = &(CANTP_CONFIG->ChannelContexts[txPduId]);
  if (CANTP_IDLE == context->State) {
    context->TpSduLength = pduInfoPtr->SduLength;
    context->TpSduOffset = 0;
    r = CanTp_StartToSend(txPduId);
  } else {
    r = E_NOT_OK;
  }

  return r;
}

void CanTp_MainFunction_Channel(uint8_t channel) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  boolean bTimeout = FALSE;

  DET_VALIDATE(NULL != CANTP_CONFIG, 0x06, CANTP_E_UNINIT, return);

  context = &(CANTP_CONFIG->ChannelContexts[channel]);
  config = &(CANTP_CONFIG->ChannelConfigs[channel]);

  CanTpEnterCritical();
  if (CanTpIsAlarmStarted()) {
    CanTpSignalAlarm();
    if (CanTpIsAlarmTimeout()) {
      CanTpCancelAlarm();
      bTimeout = TRUE;
    }
  }
  CanTpExitCritical();

  if (TRUE == bTimeout) {
    if (CANTP_SEND_CF_DELAY != context->State) {
      ASLOG(CANTPE, ("[%d] timer timeout in state %d\n", channel, context->State));
    }

    switch (context->State) {
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
      CanTp_SendCF((PduIdType)channel);
      break;
    default:
      CanTp_ResetToIdle(context);
      PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
      break;
    }
  }

  if (CANTP_SEND_CF_START == context->State) { /* FC allow CF */
    CanTp_SendCF((PduIdType)channel);
  }
}

void CanTp_MainFunction_ChannelFast(uint8_t channel) {
#ifndef CANTP_USE_TRIGGER_TRANSMIT
  CanTp_ChannelContextType *context;

  DET_VALIDATE(NULL != CANTP_CONFIG, 0x06, CANTP_E_UNINIT, return);
  context = &(CANTP_CONFIG->ChannelContexts[channel]);

  switch (context->State) {
  case CANTP_RESEND_SF:
  case CANTP_RESEND_FF:
  case CANTP_RESEND_FC_CTS:
  case CANTP_RESEND_FC_OVFLW:
  case CANTP_RESEND_CF:
    CanTp_ReSend((PduIdType)channel);
    break;
#ifndef CANTP_USE_TX_CONFIRMATION
  case CANTP_WAIT_CF_TX_COMPLETED:
    CanTp_HandleCFTxCompleted(channel);
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
  for (i = 0; i < CANTP_CONFIG->NumOfChannels; i++) {
    CanTp_MainFunction_Channel(i);
  }
#endif
}

/* Note: The period of this function is generally 1 ms */
void CanTp_MainFunction_Fast(void) {
  uint8_t i;
  DET_VALIDATE(NULL != CANTP_CONFIG, 0x06, CANTP_E_UNINIT, return);
  for (i = 0; i < CANTP_CONFIG->NumOfChannels; i++) {
#if 1 == CANTP_MAIN_FUNCTION_PERIOD
    CanTp_MainFunction_Channel(i);
#endif
    CanTp_MainFunction_ChannelFast(i);
  }
}

#ifdef CANTP_USE_TRIGGER_TRANSMIT
PduLengthType CanTp_GetTxPacketLength(PduIdType txPduId) {
  PduLengthType ret = 0;
  CanTp_ChannelContextType *context;

  DET_VALIDATE(NULL != CANTP_CONFIG, 0xFF, CANTP_E_UNINIT, return 0);
  DET_VALIDATE(txPduId < CANTP_CONFIG->NumOfChannels, 0xFF, CANTP_E_INVALID_TX_ID, return 0);

  context = &(CANTP_CONFIG->ChannelContexts[txPduId]);
  switch (context->State) {
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

PduLengthType CanTp_GetRxLeftLength(PduIdType rxPduId) {
  PduLengthType ret = 0;
  CanTp_ChannelContextType *context;

  DET_VALIDATE(NULL != CANTP_CONFIG, 0xFE, CANTP_E_UNINIT, return 0);
  DET_VALIDATE(rxPduId < CANTP_CONFIG->NumOfChannels, 0xFE, CANTP_E_INVALID_RX_ID, return 0);

  context = &(CANTP_CONFIG->ChannelContexts[rxPduId]);
  switch (context->State) {
  case CANTP_WAIT_CF:
  case CANTP_WAIT_CF_TX_COMPLETED:
    ret = context->TpSduLength;
    break;
  default:
    break;
  }

  return ret;
}

Std_ReturnType CanTp_TriggerTransmit(PduIdType txPduId, const PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_OK;
  CanTp_ChannelContextType *context;

  DET_VALIDATE(NULL != CANTP_CONFIG, 0xF0, CANTP_E_UNINIT, return E_NOT_OK);
  /* 0xF0: self defined CanTp API */
  DET_VALIDATE((NULL != pduInfoPtr) && (NULL != pduInfoPtr->SduDataPtr) &&
                 (pduInfoPtr->SduLength > 0),
               0xF0, CANTP_E_PARAM_POINTER, return E_NOT_OK);

  DET_VALIDATE(txPduId < CANTP_CONFIG->NumOfChannels, 0xF0, CANTP_E_INVALID_TX_ID, return E_NOT_OK);

  context = &(CANTP_CONFIG->ChannelContexts[txPduId]);
  switch (context->State) {
  case CANTP_RESEND_SF:
  case CANTP_RESEND_FF:
  case CANTP_RESEND_FC_CTS:
  case CANTP_RESEND_FC_OVFLW:
  case CANTP_RESEND_CF:
    ret = CanTp_ReSend(txPduId, pduInfoPtr);
    break;
  default:
    ASLOG(CANTPI, ("[%d] trigger transmit in state %d\n", txPduId, context->State));
    ret = E_NOT_OK;
    break;
  }

  return ret;
}
#endif

void CanTp_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x07, CANTP_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = DET_THIS_MODULE_ID;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 3;
}

/** @brief release notes
 * - 4.0.1: Added zero initialization of the context and DET-based validation to
 *    the private API for improved robustness and safety.
 * - 4.0.2: Don't abort transmission when new request received.
 * - 4.0.3: Add strict RX message DLC validation.
 */
