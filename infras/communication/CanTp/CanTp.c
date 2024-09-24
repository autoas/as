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
#include "CanTp_Types.h"
#include "PduR_CanTp.h"
#include "Std_Debug.h"
#include <string.h>
#include "Std_Topic.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_CANTP 0
#define AS_LOG_CANTPI 0
#define AS_LOG_CANTPE 2

#define CANTP_CONFIG (&CanTp_Config)

/* see ISO 15765-2 2004 */
#define N_PCI_MASK 0x30
#define N_PCI_SF 0x00
#define N_PCI_FF 0x10
#define N_PCI_CF 0x20
#define N_PCI_FC 0x30
#define N_PCI_SF_DL 0x07
/* Flow Control Status Mask */
#define N_PCI_FS 0x0F
/* Flow Control Status */
#define N_PCI_CTS 0x00
#define N_PCI_WT 0x01
#define N_PCI_OVFLW 0x02

#define N_PCI_SN 0x0F

#ifdef CANTP_FIX_LL_DL
#define CanTp_GetDL(len, LL_DL) LL_DL
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const CanTp_ConfigType CanTp_Config;

static void CanTp_SendSF(PduIdType TxPduId);
#ifndef CANTP_NO_FC
static void CanTp_SendFC(PduIdType TxPduId);
#endif
static void CanTp_SendCF(PduIdType TxPduId);
/* ================================ [ DATAS     ] ============================================== */
#ifndef CANTP_FIX_LL_DL
static const PduLengthType lLL_DLs[] = {8, 12, 16, 20, 24, 32, 48};
#endif
/* ================================ [ LOCALS    ] ============================================== */
static void CanTp_ResetToIdle(CanTp_ChannelContextType *context) {
  context->timer = 0;
  context->state = CANTP_IDLE;
  context->TpSduLength = 0;
}

#ifndef CANTP_FIX_LL_DL
static PduLengthType CanTp_GetDL(PduLengthType len, PduLengthType LL_DL) {
  PduLengthType dl = LL_DL;
  int i;

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
  if (config->LL_DL > 8) {
    if (CANTP_EXTENDED == config->AddressingFormat) {
      sfMaxLen = config->LL_DL - 3;
    } else {
      sfMaxLen = config->LL_DL - 2;
    }
  } else {
    if (CANTP_EXTENDED == config->AddressingFormat) {
      sfMaxLen = config->LL_DL - 2;
    } else {
      sfMaxLen = config->LL_DL - 1;
    }
  }
  return sfMaxLen;
}

static void CanTp_HandleSF(PduIdType RxPduId, uint8_t pci, uint8_t *data, uint8_t length) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  PduInfoType PduInfo;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
  PduLengthType sfMaxLen;

  context = &(CANTP_CONFIG->channelContexts[RxPduId]);
  config = &(CANTP_CONFIG->channelConfigs[RxPduId]);

  if (context->state != CANTP_IDLE) {
    ASLOG(CANTPE, ("[%d]SF received in state %d!\n", RxPduId, context->state));
    PduR_CanTpRxIndication(config->PduR_RxPduId, E_NOT_OK);
    CanTp_ResetToIdle(context);
  }

  sfMaxLen = CanTp_GetSFMaxLen(config);

  PduInfo.SduLength = pci & N_PCI_SF_DL;
  if ((0 == PduInfo.SduLength) && (config->LL_DL > 8)) {
    PduInfo.SduLength = data[0];
    PduInfo.SduDataPtr = &data[1];
  } else {
    PduInfo.SduDataPtr = data;
  }
  PduInfo.MetaDataPtr = NULL;

  if ((PduInfo.SduLength <= sfMaxLen) && (PduInfo.SduLength > 0)) {
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
    ASLOG(CANTPE,
          ("[%d]SF received with invalid len %d(>%d)!\n", RxPduId, PduInfo.SduLength, sfMaxLen));
  }
}

#ifndef CANTP_NO_FC
static void CanTp_SendFC(PduIdType RxPduId) {
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
    data[pos++] = config->N_TA;
  }

  data[pos++] = N_PCI_FC | N_PCI_CTS;
  data[pos++] = config->BS;
  data[pos++] = config->STmin;
  while (pos < 8) {
    data[pos++] = config->padding;
  }
  ASLOG(CANTPI, ("[%d]TX data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", RxPduId, data[0],
                 data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
  context->PduInfo.SduDataPtr = data;
  context->PduInfo.SduLength = 8;

#ifdef CANTP_USE_TRIGGER_TRANSMIT
  context->timer = config->N_As;
  context->state = CANTP_RESEND_FC;
#else
  r = CanIf_Transmit(config->CanIfTxPduId, &context->PduInfo);
  if (E_OK == r) {
    STD_TOPIC_ISOTP(RxPduId, FALSE, CanIf_CanTpGetTxCanId(config->CanIfTxPduId),
                    context->PduInfo.SduLength, context->PduInfo.SduDataPtr);
    context->state = CANTP_WAIT_CF;
    context->timer = config->N_Cr;
    context->BS = config->BS;
  } else {
    context->timer = config->N_As;
    context->state = CANTP_RESEND_FC;
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
  PduLengthType ffLen, sfMaxLen;
  uint32_t TpSduLength;

  context = &(CANTP_CONFIG->channelContexts[RxPduId]);
  config = &(CANTP_CONFIG->channelConfigs[RxPduId]);

  if (context->state != CANTP_IDLE) {
    ASLOG(CANTPE, ("[%d]FF received in state %d!\n", RxPduId, context->state));
    CanTp_ResetToIdle(context);
    PduR_CanTpRxIndication(config->PduR_RxPduId, E_NOT_OK);
  }

  TpSduLength = ((uint32_t)(pci & 0x0F) << 8) + data[0];
  PduInfo.SduDataPtr = &data[1];

  PduInfo.SduLength = length - 1;
  if (CANTP_EXTENDED == config->AddressingFormat) {
    ffLen = config->LL_DL - 3;
  } else {
    ffLen = config->LL_DL - 2;
  }

  if ((config->LL_DL > 8) && (0 == TpSduLength)) {
    TpSduLength = ((uint32_t)data[2] << 24) + ((uint32_t)data[3] << 16) + ((uint32_t)data[4] << 8) +
                  ((uint32_t)data[5]);
    PduInfo.SduDataPtr = &data[6];
    PduInfo.SduLength -= 4;
    ffLen -= 4;
  }
  PduInfo.MetaDataPtr = NULL;

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
      ASLOG(CANTPE, ("[%d]FF received with buffer status %d!\n", RxPduId, bufReq));
      CanTp_ResetToIdle(context);
    } else {
      context->TpSduLength = (PduLengthType)TpSduLength - ffLen;
      context->SN = 1;
#ifdef CANTP_NO_FC
      context->state = CANTP_WAIT_CF;
      context->timer = config->N_Cr;
      context->BS = 0;
#else
      CanTp_SendFC(RxPduId);
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
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;

  context = &(CANTP_CONFIG->channelContexts[RxPduId]);
  config = &(CANTP_CONFIG->channelConfigs[RxPduId]);

  if (context->state != CANTP_WAIT_CF) {
    ASLOG(CANTPE, ("[%d]CF received when in state %d.\n", RxPduId, context->state));
  } else {
    if (context->SN == (pci & N_PCI_SN)) {
      context->SN++;
      if (context->SN > 15) {
        context->SN = 0;
      }
      PduInfo.SduLength = context->TpSduLength;
      if (PduInfo.SduLength > length) {
        PduInfo.SduLength = length;
      }
      PduInfo.MetaDataPtr = NULL;
      PduInfo.SduDataPtr = data;

      bufReq = PduR_CanTpCopyRxData(config->PduR_RxPduId, &PduInfo, &bufferSize);

      if (BUFREQ_OK == bufReq) {
        context->TpSduLength -= PduInfo.SduLength;
        if (0 == context->TpSduLength) {
          CanTp_ResetToIdle(context);
          PduR_CanTpRxIndication(config->PduR_RxPduId, E_OK);
        } else {
          context->timer = config->N_Cr;
#ifndef CANTP_NO_FC
          if (context->BS > 0) {
            context->BS--;
            if (0 == context->BS) {
              CanTp_SendFC(RxPduId);
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

  if ((context->state != CANTP_WAIT_FIRST_FC) && (context->state != CANTP_WAIT_FC)) {
    ASLOG(CANTPE, ("[%d]FC received when in state %d.\n", RxPduId, context->state));
  } else {
    switch ((pci & N_PCI_FS)) {
    case N_PCI_CTS:
      if (CANTP_WAIT_FIRST_FC == context->state) {
        context->cfgBS = data[0];
        context->STmin = data[1];
      }
      context->BS = context->cfgBS;
      CanTp_SendCF(RxPduId);
      break;
    case N_PCI_WT:
      if (context->WftCounter < 0xFF) {
        context->WftCounter++;
      }
      if (context->WftCounter > config->CanTpRxWftMax) {
        CanTp_ResetToIdle(context);
        PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
      } else {
        context->timer = config->N_Bs;
      }
      break;
    case N_PCI_OVFLW:
      CanTp_ResetToIdle(context);
      PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
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
#ifndef CANTP_USE_TRIGGER_TRANSMIT
  Std_ReturnType r;
#endif

  context = &(CANTP_CONFIG->channelContexts[TxPduId]);
  config = &(CANTP_CONFIG->channelConfigs[TxPduId]);
  data = config->data;
  if (CANTP_EXTENDED == config->AddressingFormat) {
    data[pos++] = config->N_TA;
  }

  if ((config->LL_DL > 8) && ((7 - pos) < context->TpSduLength)) {
    data[pos++] = N_PCI_SF;
    data[pos++] = context->TpSduLength;
  } else {
    data[pos++] = N_PCI_SF | (uint8_t)(context->TpSduLength & 0x7);
  }

  PduInfo.MetaDataPtr = NULL;
  PduInfo.SduDataPtr = &data[pos];
  PduInfo.SduLength = context->TpSduLength;

  bufReq = PduR_CanTpCopyTxData(config->PduR_TxPduId, &PduInfo, NULL, &bufferSize);
  if (BUFREQ_OK == bufReq) {
    pos += context->TpSduLength;
    ll_dl = CanTp_GetDL(pos, config->LL_DL);
    while (pos < ll_dl) {
      data[pos++] = config->padding;
    }
    ASLOG(CANTPI, ("[%d]TX data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", TxPduId, data[0],
                   data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
    context->PduInfo.SduDataPtr = data;
    context->PduInfo.SduLength = ll_dl;
#ifdef CANTP_USE_TRIGGER_TRANSMIT
    context->state = CANTP_RESEND_SF;
    context->timer = config->N_As;
#else
    r = CanIf_Transmit(config->CanIfTxPduId, &context->PduInfo);
    if (E_OK == r) {
      STD_TOPIC_ISOTP(TxPduId, FALSE, CanIf_CanTpGetTxCanId(config->CanIfTxPduId),
                      context->PduInfo.SduLength, context->PduInfo.SduDataPtr);
#ifdef CANTP_USE_TX_CONFIRMATION
      context->state = CANTP_WAIT_SF_TX_COMPLETED;
      context->timer = config->N_As;
#else
      CanTp_ResetToIdle(context);
      PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_OK);
#endif
    } else {
      context->state = CANTP_RESEND_SF;
      context->timer = config->N_As;
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
  PduLengthType pos = 0;
  PduInfoType PduInfo;
  uint8_t *data;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
#ifndef CANTP_USE_TRIGGER_TRANSMIT
  Std_ReturnType r;
#endif

  context = &(CANTP_CONFIG->channelContexts[TxPduId]);
  config = &(CANTP_CONFIG->channelConfigs[TxPduId]);
  data = config->data;
  if (CANTP_EXTENDED == config->AddressingFormat) {
    data[pos++] = config->N_TA;
  }

  if ((config->LL_DL > 8) && (context->TpSduLength > 4095)) {
    data[pos++] = N_PCI_FF;
    data[pos++] = 0;
    data[pos++] = (uint8_t)(context->TpSduLength >> 24) & 0xFF;
    data[pos++] = (uint8_t)(context->TpSduLength >> 16) & 0xFF;
    data[pos++] = (uint8_t)(context->TpSduLength >> 8) & 0xFF;
    data[pos++] = (uint8_t)context->TpSduLength & 0xFF;
  } else {
    data[pos++] = N_PCI_FF | (uint8_t)((context->TpSduLength >> 8) & 0x0F);
    data[pos++] = (uint8_t)context->TpSduLength & 0xFF;
  }

  PduInfo.MetaDataPtr = NULL;
  PduInfo.SduDataPtr = &data[pos];
  PduInfo.SduLength = config->LL_DL - pos;

  bufReq = PduR_CanTpCopyTxData(config->PduR_TxPduId, &PduInfo, NULL, &bufferSize);
  if (BUFREQ_OK == bufReq) {
    context->TpSduLength -= PduInfo.SduLength;
    context->PduInfo.SduDataPtr = data;
    context->PduInfo.SduLength = config->LL_DL;
    context->SN = 1;
    ASLOG(CANTPI, ("[%d]TX data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", TxPduId, data[0],
                   data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
#ifdef CANTP_USE_TRIGGER_TRANSMIT
    context->state = CANTP_RESEND_FF;
    context->timer = config->N_As;
#else
    r = CanIf_Transmit(config->CanIfTxPduId, &context->PduInfo);
    if (E_OK == r) {
      STD_TOPIC_ISOTP(TxPduId, FALSE, CanIf_CanTpGetTxCanId(config->CanIfTxPduId),
                      context->PduInfo.SduLength, context->PduInfo.SduDataPtr);
#ifdef CANTP_USE_TX_CONFIRMATION
      context->state = CANTP_WAIT_FF_TX_COMPLETED;
      context->timer = config->N_As;
#else
      context->state = CANTP_WAIT_FIRST_FC;
      context->WftCounter = 0;
      context->timer = config->N_Bs;
#endif
    } else {
      context->state = CANTP_RESEND_FF;
      context->timer = config->N_As;
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
  PduLengthType pos = 0;
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
    data[pos++] = config->N_TA;
  }

  data[pos++] = N_PCI_CF | context->SN;
  context->SN++;
  if (context->SN > 15) {
    context->SN = 0;
  }

  bufferSize = config->LL_DL - pos;

  if (bufferSize > context->TpSduLength) {
    bufferSize = context->TpSduLength;
  }

  PduInfo.MetaDataPtr = NULL;
  PduInfo.SduDataPtr = &data[pos];
  PduInfo.SduLength = bufferSize;

  bufReq = PduR_CanTpCopyTxData(config->PduR_TxPduId, &PduInfo, NULL, &bufferSize);
  if (BUFREQ_OK == bufReq) {
    context->TpSduLength -= PduInfo.SduLength;
    pos += PduInfo.SduLength;
    ll_dl = CanTp_GetDL(pos, config->LL_DL);
    while (pos < ll_dl) {
      data[pos++] = config->padding;
    }
    ASLOG(CANTPI, ("[%d]TX data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", TxPduId, data[0],
                   data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
    context->PduInfo.SduDataPtr = data;
    context->PduInfo.SduLength = ll_dl;
#ifdef CANTP_USE_TRIGGER_TRANSMIT
    context->state = CANTP_RESEND_CF;
    context->timer = config->N_As;
#else
    r = CanIf_Transmit(config->CanIfTxPduId, &context->PduInfo);
    if (E_OK == r) {
      STD_TOPIC_ISOTP(TxPduId, FALSE, CanIf_CanTpGetTxCanId(config->CanIfTxPduId),
                      context->PduInfo.SduLength, context->PduInfo.SduDataPtr);
      context->state = CANTP_WAIT_CF_TX_COMPLETED;
      context->timer = config->N_As;
    } else {
      context->state = CANTP_RESEND_CF;
      context->timer = config->N_As;
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

  if (context->TpSduLength > 0) {
    if (context->cfgBS > 0) {
      if (context->BS > 0) {
        context->BS--;
        if (0 == context->BS) {
          context->BS = context->cfgBS;
          context->state = CANTP_WAIT_FC;
          context->WftCounter = 0;
          context->timer = config->N_Bs;
        }
      }
    }
    if (CANTP_WAIT_FC != context->state) {
#ifdef CANTP_USE_TRIGGER_TRANSMIT
      CanTp_SendCF(TxPduId);
#else
      if (context->STmin > 0) {
        context->state = CANTP_SEND_CF_DELAY;
        context->timer = CANTP_CONVERT_MS_TO_MAIN_CYCLES(context->STmin);
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
    } else if (CANTP_RESEND_FC == context->state) {
      context->state = CANTP_WAIT_FC_TX_COMPLETED;
    } else if (CANTP_RESEND_CF == context->state) {
      context->state = CANTP_WAIT_CF_TX_COMPLETED;
    } else {
      ASLOG(CANTPE, ("[%d]resend in wrong state %d, impossible case", TxPduId, context->state));
    }
    context->timer = config->N_As;
#else
    if (CANTP_RESEND_SF == context->state) {
      CanTp_ResetToIdle(context);
      PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_OK);
    } else if (CANTP_RESEND_FF == context->state) {
#if defined(CANTP_NO_FC)
      CanTp_SendCF(TxPduId);
#else
      context->state = CANTP_WAIT_FIRST_FC;
      context->WftCounter = 0;
      context->timer = config->N_Bs;
#endif
    } else if (CANTP_RESEND_FC == context->state) {
      context->state = CANTP_WAIT_CF;
      context->timer = config->N_Cr;
    } else if (CANTP_RESEND_CF == context->state) {
#ifdef CANTP_USE_TRIGGER_TRANSMIT
      CanTp_HandleCFTxCompleted(TxPduId);
#else
      context->state = CANTP_WAIT_CF_TX_COMPLETED;
      context->timer = config->N_As;
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

static void CanTp_StartToSend(PduIdType TxPduId) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  PduLengthType sfMaxLen;

  context = &(CANTP_CONFIG->channelContexts[TxPduId]);
  config = &(CANTP_CONFIG->channelConfigs[TxPduId]);
  sfMaxLen = CanTp_GetSFMaxLen(config);

  if (sfMaxLen >= context->TpSduLength) {
    CanTp_SendSF(TxPduId);
  } else {
    CanTp_SendFF(TxPduId);
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void CanTp_InitChannel(uint8_t Channel) {
  CanTp_ChannelContextType *context;
  context = &(CANTP_CONFIG->channelContexts[Channel]);
  context->state = CANTP_IDLE;
  context->timer = 0;
  context->STmin = 0;
  context->PduInfo.MetaDataPtr = NULL;
  context->TpSduLength = 0;
}

void CanTp_Init(const CanTp_ConfigType *CfgPtr) {
  uint8_t i;
  (void)CfgPtr;
  for (i = 0; i < CANTP_CONFIG->numOfChannels; i++) {
    CanTp_InitChannel(i);
  }
}

void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  const CanTp_ChannelConfigType *config;
  uint8_t pci;
  uint8_t *data;
  uint8_t length;
  Std_ReturnType r = E_OK;
  ASLOG(CANTPI, ("[%d]RX len=%d data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X], state=%d left=%d\n",
                 RxPduId, PduInfoPtr->SduLength, PduInfoPtr->SduDataPtr[0],
                 PduInfoPtr->SduDataPtr[1], PduInfoPtr->SduDataPtr[2], PduInfoPtr->SduDataPtr[3],
                 PduInfoPtr->SduDataPtr[4], PduInfoPtr->SduDataPtr[5], PduInfoPtr->SduDataPtr[6],
                 PduInfoPtr->SduDataPtr[7], CANTP_CONFIG->channelContexts[RxPduId].state,
                 CANTP_CONFIG->channelContexts[RxPduId].TpSduLength));
  STD_TOPIC_ISOTP(RxPduId, TRUE, CanIf_CanTpGetRxCanId(RxPduId), PduInfoPtr->SduLength,
                  PduInfoPtr->SduDataPtr);
  if ((RxPduId < CANTP_CONFIG->numOfChannels) && (NULL != PduInfoPtr) &&
      (NULL != PduInfoPtr->SduDataPtr) && (PduInfoPtr->SduLength > 0)) {
    config = &(CANTP_CONFIG->channelConfigs[RxPduId]);

    if (PduInfoPtr->SduLength <= config->LL_DL) {
      if (CANTP_EXTENDED == config->AddressingFormat) {
        if (config->N_TA != PduInfoPtr->SduDataPtr[0]) {
          ASLOG(CANTPI, ("[%d]not for me\n", RxPduId));
          r = E_NOT_OK;
        } else {
          pci = PduInfoPtr->SduDataPtr[1];
          data = &PduInfoPtr->SduDataPtr[2];
          length = PduInfoPtr->SduLength - 2;
        }
      } else {
        pci = PduInfoPtr->SduDataPtr[0];
        data = &PduInfoPtr->SduDataPtr[1];
        length = PduInfoPtr->SduLength - 1;
      }
    } else {
      ASLOG(CANTPE, ("[%d]invalid LL_DL\n", RxPduId));
      r = E_NOT_OK;
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
#endif
  CanTp_ChannelContextType *context;

  if ((TxPduId < CANTP_CONFIG->numOfChannels) && (E_OK == result)) {
#ifdef CANTP_USE_TX_CONFIRMATION
    config = &(CANTP_CONFIG->channelConfigs[TxPduId]);
#endif
    context = &(CANTP_CONFIG->channelContexts[TxPduId]);
    switch (context->state) {
#ifdef CANTP_USE_TX_CONFIRMATION
    case CANTP_WAIT_SF_TX_COMPLETED:
      CanTp_ResetToIdle(context);
      PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_OK);
      break;
#endif
    case CANTP_WAIT_CF_TX_COMPLETED:
      CanTp_HandleCFTxCompleted(TxPduId);
      break;
#ifdef CANTP_USE_TX_CONFIRMATION
    case CANTP_WAIT_FF_TX_COMPLETED:
      context->state = CANTP_WAIT_FIRST_FC;
      context->WftCounter = 0;
      context->timer = config->N_Bs;
      break;
#endif
#ifdef CANTP_USE_TX_CONFIRMATION
    case CANTP_WAIT_FC_TX_COMPLETED:
      context->state = CANTP_WAIT_CF;
      context->timer = config->N_Cr;
      break;
#endif
    default:
      break;
    }
  }
}

Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType r = E_OK;
  CanTp_ChannelContextType *context;

  if ((TxPduId < CANTP_CONFIG->numOfChannels) && (NULL != PduInfoPtr) &&
      (PduInfoPtr->SduLength > 0)) {
    context = &(CANTP_CONFIG->channelContexts[TxPduId]);
    if (CANTP_IDLE == context->state) {
      context->TpSduLength = PduInfoPtr->SduLength;
      CanTp_StartToSend(TxPduId);
    } else {
      r = E_NOT_OK;
    }
  }

  return r;
}

void CanTp_MainFunction_Channel(uint8_t Channel) {
  const CanTp_ChannelConfigType *config;
  CanTp_ChannelContextType *context;
  context = &(CANTP_CONFIG->channelContexts[Channel]);
  config = &(CANTP_CONFIG->channelConfigs[Channel]);
#ifndef CANTP_USE_TRIGGER_TRANSMIT
  switch (context->state) {
  case CANTP_RESEND_SF:
  case CANTP_RESEND_FF:
  case CANTP_RESEND_FC:
  case CANTP_RESEND_CF:
    CanTp_ReSend((PduIdType)Channel);
    break;
  default:
    break;
  }
#endif

  if (context->timer > 0) {
    context->timer--;
    if (0 == context->timer) {
      if (CANTP_SEND_CF_DELAY != context->state) {
        ASLOG(CANTPE, ("[%d] timer timeout in state %d\n", Channel, context->state));
      }

      switch (context->state) {
      case CANTP_WAIT_CF:
#ifdef CANTP_USE_TX_CONFIRMATION
      case CANTP_WAIT_FC_TX_COMPLETED:
#endif
        CanTp_ResetToIdle(context);
        PduR_CanTpRxIndication(config->PduR_RxPduId, E_NOT_OK);
        break;
      case CANTP_SEND_CF_DELAY:
        CanTp_SendCF((PduIdType)Channel);
        break;
      default:
        CanTp_ResetToIdle(context);
        PduR_CanTpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
        break;
      }
    }
  }
}

void CanTp_MainFunction(void) {
  uint8_t i;
  for (i = 0; i < CANTP_CONFIG->numOfChannels; i++) {
    CanTp_MainFunction_Channel(i);
  }
}

#ifdef CANTP_USE_TRIGGER_TRANSMIT
PduLengthType CanTp_GetTxPacketLength(PduIdType TxPduId) {
  PduLengthType ret = 0;
  CanTp_ChannelContextType *context;

  context = &(CANTP_CONFIG->channelContexts[TxPduId]);
  switch (context->state) {
  case CANTP_RESEND_SF:
  case CANTP_RESEND_FF:
  case CANTP_RESEND_FC:
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

  context = &(CANTP_CONFIG->channelContexts[TxPduId]);
  switch (context->state) {
  case CANTP_RESEND_SF:
  case CANTP_RESEND_FF:
  case CANTP_RESEND_FC:
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
