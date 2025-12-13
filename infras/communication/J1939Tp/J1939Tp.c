/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of a Transport Layer for SAE J1939 AUTOSAR CP R23-11
 * ref: CAN: SAE-J1939-21-2006
 * ref: CAN-FD: SAE J1939-22_202107.pdf
 * ref: Figure A1 - RTS/CTS data transfer without errors
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "J1939Tp.h"
#include "J1939Tp_Priv.h"
#include "J1939Tp_Cfg.h"
#include "PduR_J1939Tp.h"
#include "CanIf.h"

#include "Std_Debug.h"

#include "Det.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_J1939TP 0u
#define AS_LOG_J1939TPI 0u
#define AS_LOG_J1939TPE 3

#ifdef J1939TP_USE_PB_CONFIG
#define J1939TP_CONFIG j1939tpConfig
#else
#define J1939TP_CONFIG (&J1939Tp_Config)
#endif

#ifdef J1939TP_FIX_LL_DL
#define J1939Tp_GetDL(len, LL_DL) LL_DL
#endif

#define IS_J1939TP_FD(config) (config->LL_DL > 8u)

#define J1939TP_CM_RTS 16u
#define J1939TP_CM_CTS 17u
#define J1939TP_CM_EOMA 19u
#define J1939TP_CM_BAM 32u
#define J1939TP_CM_ABORT 0xFFu

#define DT_DATA_SIZE (config->LL_DL - 1u)
#define NUM_DT_PACKETS(s) (((s) + DT_DATA_SIZE - 1u) / DT_DATA_SIZE)

#define NUM_OF_DT_PACKETS NUM_DT_PACKETS(config->context->TpSduLength)

#define J1939TP_FD_CM_RTS ((config->S << 4) | 0u)
#define J1939TP_FD_CM_CTS ((config->S << 4) | 1u)
#define J1939TP_FD_CM_EOMS ((config->S << 4) | 2u)
#define J1939TP_FD_CM_EOMA ((config->S << 4) | 3u)
#define J1939TP_FD_CM_BAM ((config->S << 4) | 4u)
#define J1939TP_FD_CM_ABORT ((config->S << 4) | 15u)
#define J1939TP_FD_DTFI ((config->S << 4) | 0u)

#define FD_DT_DATA_SIZE (config->LL_DL - 4u)
#define NUM_FD_DT_PACKETS(s) (((s) + FD_DT_DATA_SIZE - 1u) / FD_DT_DATA_SIZE)
#define NUM_OF_FD_DT_PACKETS NUM_FD_DT_PACKETS(config->context->TpSduLength)

/* System resources were needed for another task so this connection managed session was terminated.
 */
#define J1939TP_ABORT_REASON_OUT_OF_RESOURCE 2u

/* A timeout occurred and this is the connection abort to close the session */
#define J1939TP_ABORT_REASON_TIMEOUT 3u

/* Bad sequence/segment number (software cannot recover) */
#define J1939TP_ABORT_REASON_BAD_SEQUENCE_NUMBER 7u

/* Total Message Size exceeds system resources */
#define J1939TP_ABORT_REASON_MESSAGE_TOO_BIG 9u

/* Assurance Data does not match expected value (software cannot recover) */
#define J1939TP_ABORT_REASON_ASSURANCE_DATA_NOT_MATCH 10u

/* Assurance Data not received (if required) */
#define J1939TP_ABORT_REASON_ASSURANCE_DATA_MISSING 11u
/* ================================ [ TYPES     ] ============================================== */
enum {
  J1939TP_IDLE = 0,
  J1939TP_RESEND_DIRECT_PG,
  J1939TP_RESEND_CM_RTS,
  J1939TP_RESEND_CM_BAM,
  J1939TP_RESEND_CM_ABORT,
  J1939TP_WAIT_DT_STMIN_TIMEOUT,
  J1939TP_RESEND_DT,
  J1939TP_RESEND_CM_EOMS,
  J1939TP_RESEND_CM_CTS,
  J1939TP_RESEND_CM_EMOA,
  J1939TP_WAIT_DIRECT_PG_TX_COMPLETED,
  J1939TP_WAIT_CM_RTS_TX_COMPLETED,
  J1939TP_WAIT_CM_BAM_TX_COMPLETED,
  J1939TP_WAIT_CM_ABORT_TX_COMPLETED,
  J1939TP_WAIT_DT_TX_COMPLETED,
  J1939TP_WAIT_CM_EOMS_TX_COMPLETED,
  J1939TP_WAIT_CM_CTS_RX,
  J1939TP_WAIT_END_OF_MSG_ACK_RX,
  J1939TP_WAIT_CM_CTS_TX_COMPLETED,
  J1939TP_WAIT_CM_EOMA_TX_COMPLETED,
  J1939TP_WAIT_DT_RX,
  J1939TP_WAIT_CM_EOMS_RX,
};
/* ================================ [ DECLARES  ] ============================================== */
extern const J1939Tp_ConfigType J1939Tp_Config;
/* ================================ [ DATAS     ] ============================================== */
#ifndef J1939TP_FIX_LL_DL
static const PduLengthType lLL_DLs[] = {8u, 12u, 16u, 20u, 24u, 32u, 48u};
#endif
#ifdef J1939TP_USE_PB_CONFIG
static const J1939Tp_ConfigType *j1939tpConfig = NULL;
#endif
/* ================================ [ LOCALS    ] ============================================== */
#ifndef J1939TP_FIX_LL_DL
static PduLengthType J1939Tp_GetDL(PduLengthType len, PduLengthType LL_DL) {
  PduLengthType dl = LL_DL;
  PduLengthType i;

  for (i = 0u; i < ARRAY_SIZE(lLL_DLs); i++) {
    if (len <= lLL_DLs[i]) {
      dl = lLL_DLs[i];
      break;
    }
  }
  return dl;
}
#endif

static void J1939Tp_TxResetToIdle(const J1939Tp_TxChannelType *config) {
  (void)memset(config->context, 0u, sizeof(J1939Tp_TxChannelContextType));
  config->context->PgPGN = config->TxPgPGN;
}

static void J1939Tp_RxResetToIdle(const J1939Tp_RxChannelType *config) {
  uint32_t PgPGN = config->context->PgPGN;
  (void)memset(config->context, 0u, sizeof(J1939Tp_RxChannelContextType));
  config->context->PgPGN = PgPGN;
}

static void J1939Tp_SendDirectPG(PduIdType TxPduId) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[TxPduId];
  PduInfoType PduInfo;
  uint8_t *data;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
  PduLengthType pos = 0u;
  Std_ReturnType ret;

  data = config->data;
  PduInfo.MetaDataPtr = (uint8_t *)&pos;
  PduInfo.SduDataPtr = data;
  PduInfo.SduLength = config->context->TpSduLength;

  bufReq = PduR_J1939TpCopyTxData(config->PduR_TxPduId, &PduInfo, NULL, &bufferSize);
  if (BUFREQ_OK == bufReq) {
    ASLOG(J1939TPI, ("[%d]TX Direct=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", TxPduId, data[0u],
                     data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
    if (PduInfo.SduLength > 8u) {
      pos = PduInfo.SduLength;
      PduInfo.SduLength = J1939Tp_GetDL(pos, config->LL_DL);
      while (pos < PduInfo.SduLength) {
        data[pos] = config->padding;
        pos++;
      }
    }

    config->context->PduInfo.SduDataPtr = data;
    config->context->PduInfo.SduLength = PduInfo.SduLength;

    ret = CanIf_Transmit(config->CanIf_TxDirectNPdu, &config->context->PduInfo);
    if (E_OK == ret) {
      config->context->state = J1939TP_WAIT_DIRECT_PG_TX_COMPLETED;
      config->context->timer = config->Tr;
    } else {
      config->context->state = J1939TP_RESEND_DIRECT_PG;
      config->context->timer = config->Tr;
    }
  } else {
    ASLOG(J1939TPE, ("[%d]DirectPG: failed to provide TX data, reset to idle\n", TxPduId));
    J1939Tp_TxResetToIdle(config);
    PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
  }
}

static void J1939Tp_StartToSend_CMDT(PduIdType TxPduId) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[TxPduId];
  uint8_t *data;
  Std_ReturnType ret;

  data = config->data;

  config->context->PduInfo.SduDataPtr = data;

  if (IS_J1939TP_FD(config)) {
    data[0u] = J1939TP_FD_CM_RTS;

    data[1] = config->context->TpSduLength & 0xFFu;
    data[2] = (config->context->TpSduLength >> 8) & 0xFFu;
    data[3] = (config->context->TpSduLength >> 16) & 0xFFu;

    data[4] = NUM_OF_FD_DT_PACKETS & 0xFFu;
    data[5] = (NUM_OF_FD_DT_PACKETS >> 8) & 0xFFu;
    data[6] = (NUM_OF_FD_DT_PACKETS >> 16) & 0xFFu;

    data[7] = config->TxMaxPacketsPerBlock;

    data[8] = config->ADType;

    data[9] = config->context->PgPGN & 0xFFu;
    data[10] = (config->context->PgPGN >> 8) & 0xFFu;
    data[11] = (config->context->PgPGN >> 16) & 0xFFu;
    config->context->PduInfo.SduLength = 12u;

    ASLOG(J1939TPI,
          ("[%d]TX FD CM.RTS=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n",
           TxPduId, data[0u], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
           data[8], data[9], data[10], data[11]));
  } else {
    /* CM_RTS message */
    data[0u] = J1939TP_CM_RTS;

    /* Message size in bytes, low byte at the begin. */
    data[1] = config->context->TpSduLength & 0xFFu;
    data[2] = (config->context->TpSduLength >> 8) & 0xFFu;

    data[3] = NUM_OF_DT_PACKETS; /* Number of DT packets */

    data[4] = config->TxMaxPacketsPerBlock;
    data[5] = config->context->PgPGN & 0xFFu;
    data[6] = (config->context->PgPGN >> 8) & 0xFFu;
    data[7] = (config->context->PgPGN >> 16) & 0xFFu;
    config->context->PduInfo.SduLength = 8;

    ASLOG(J1939TPI, ("[%d]TX CM.RTS=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", TxPduId, data[0u],
                     data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
  }

  ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->context->PduInfo);
  if (E_OK == ret) {
    config->context->state = J1939TP_WAIT_CM_RTS_TX_COMPLETED;
    config->context->timer = config->Tr;
  } else {
    config->context->state = J1939TP_RESEND_CM_RTS;
    config->context->timer = config->Tr;
  }
}

static void J1939Tp_SendDT(PduIdType TxPduId) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[TxPduId];
  PduInfoType PduInfo;
  uint8_t *data;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
  PduLengthType ll_dl;
  PduLengthType pos;
  PduLengthType offset = 0u;
  Std_ReturnType ret;

  if (IS_J1939TP_FD(config)) {
    data = config->data;
    data[0u] = J1939TP_FD_DTFI;
    data[1] = (config->context->NextSN + 1u) & 0xFFu;
    data[2] = ((config->context->NextSN + 1u) >> 8) & 0xFFu;
    data[3] = ((config->context->NextSN + 1u) >> 16) & 0xFFu;
    pos = 4;

    offset = config->context->NextSN * FD_DT_DATA_SIZE;

    bufferSize = config->context->TpSduLength - offset;
    if (bufferSize > (config->LL_DL - 4u)) {
      bufferSize = config->LL_DL - 4u;
    }
  } else {
    data = config->data;
    data[0u] = config->context->NextSN + 1u;
    pos = 1;

    offset = config->context->NextSN * DT_DATA_SIZE;

    bufferSize = config->context->TpSduLength - offset;
    if (bufferSize > (config->LL_DL - 1u)) {
      bufferSize = config->LL_DL - 1u;
    }
  }

  PduInfo.MetaDataPtr = (uint8_t *)&offset;
  PduInfo.SduDataPtr = &data[pos];
  PduInfo.SduLength = bufferSize;

  bufReq = PduR_J1939TpCopyTxData(config->PduR_TxPduId, &PduInfo, NULL, &bufferSize);
  if (BUFREQ_OK == bufReq) {
    pos += PduInfo.SduLength;
    ll_dl = J1939Tp_GetDL(pos, config->LL_DL);
    while (pos < ll_dl) {
      data[pos] = config->padding;
      pos++;
    }
    ASLOG(J1939TPI, ("[%d]TX DT=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", TxPduId, data[0u],
                     data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
    config->context->PduInfo.SduDataPtr = data;
    config->context->PduInfo.SduLength = ll_dl;

    if ((J1939TP_WAIT_CM_CTS_RX == config->context->state) || (0u == config->STMin)) {
      ret = CanIf_Transmit(config->CanIf_TxDtNPdu, &config->context->PduInfo);
      if (E_OK == ret) {
        config->context->state = J1939TP_WAIT_DT_TX_COMPLETED;
        config->context->timer = config->Tr;
      } else {
        config->context->state = J1939TP_RESEND_DT;
        config->context->timer = config->Tr;
      }
    } else {
      config->context->state = J1939TP_WAIT_DT_STMIN_TIMEOUT;
      config->context->timer = config->STMin;
    }
  } else {
    ASLOG(J1939TPE, ("[%d]DT: failed to provide TX data, reset to idle\n", TxPduId));
    J1939Tp_TxResetToIdle(config);
    PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
  }
}

static void J1939Tp_SendEOMS(PduIdType TxPduId) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[TxPduId];
  uint8_t *data;
  Std_ReturnType ret;

  data = config->data;

  config->context->PduInfo.SduDataPtr = data;

  data[0u] = J1939TP_FD_CM_EOMS;

  data[1] = config->context->TpSduLength & 0xFFu;
  data[2] = (config->context->TpSduLength >> 8) & 0xFFu;
  data[3] = (config->context->TpSduLength >> 16) & 0xFFu;

  data[4] = NUM_OF_FD_DT_PACKETS & 0xFFu;
  data[5] = (NUM_OF_FD_DT_PACKETS >> 8) & 0xFFu;
  data[6] = (NUM_OF_FD_DT_PACKETS >> 16) & 0xFFu;

  data[7] = 0u; /* TODO: AD Size */

  data[8] = config->ADType;

  data[9] = config->context->PgPGN & 0xFFu;
  data[10] = (config->context->PgPGN >> 8) & 0xFFu;
  data[11] = (config->context->PgPGN >> 16) & 0xFFu;
  /* TODO: append assurance data: CRC32 for example */
  config->context->PduInfo.SduLength = 12u;

  ASLOG(J1939TPI, ("[%d]TX EOMS=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", TxPduId, data[0u],
                   data[1], data[2], data[3], data[4], data[5], data[6], data[7]));

  ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->context->PduInfo);
  if (E_OK == ret) {
    config->context->state = J1939TP_WAIT_CM_EOMS_TX_COMPLETED;
    config->context->timer = config->Tr;
  } else {
    config->context->state = J1939TP_RESEND_CM_EOMS;
    config->context->timer = config->Tr;
  }
}

static void J1939Tp_StartToSend_BAM(PduIdType TxPduId) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[TxPduId];
  uint8_t *data;
  Std_ReturnType ret;

  data = config->data;

  config->context->PduInfo.SduDataPtr = data;

  if (IS_J1939TP_FD(config)) {
    data[0u] = J1939TP_FD_CM_BAM;

    data[1] = config->context->TpSduLength & 0xFFu;
    data[2] = (config->context->TpSduLength >> 8) & 0xFFu;
    data[3] = (config->context->TpSduLength >> 16) & 0xFFu;

    data[4] = NUM_OF_FD_DT_PACKETS & 0xFFu;
    data[5] = (NUM_OF_FD_DT_PACKETS >> 8) & 0xFFu;
    data[6] = (NUM_OF_FD_DT_PACKETS >> 16) & 0xFFu;

    data[7] = 0xFFu;

    data[8] = config->ADType;

    data[9] = config->context->PgPGN & 0xFFu;
    data[10] = (config->context->PgPGN >> 8) & 0xFFu;
    data[11] = (config->context->PgPGN >> 16) & 0xFFu;
    config->context->PduInfo.SduLength = 12u;

    ASLOG(J1939TPI,
          ("[%d]TX FD CM.BAM=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n",
           TxPduId, data[0u], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
           data[8], data[9], data[10], data[11]));
  } else {
    /* CM_BAM message */
    data[0u] = J1939TP_CM_BAM;

    /* Message size in bytes, low byte at the begin. */
    data[1] = config->context->TpSduLength & 0xFFu;
    data[2] = (config->context->TpSduLength >> 8) & 0xFFu;

    data[3] = NUM_OF_DT_PACKETS; /* Number of DT packets */

    data[4] = 0xFFu;
    data[5] = config->context->PgPGN & 0xFFu;
    data[6] = (config->context->PgPGN >> 8) & 0xFFu;
    data[7] = (config->context->PgPGN >> 16) & 0xFFu;
    config->context->PduInfo.SduLength = 8;

    ASLOG(J1939TPI, ("[%d]TX CM.BAM=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", TxPduId, data[0u],
                     data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
  }

  ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->context->PduInfo);
  if (E_OK == ret) {
    config->context->state = J1939TP_WAIT_CM_BAM_TX_COMPLETED;
    config->context->timer = config->Tr;
  } else {
    config->context->state = J1939TP_RESEND_CM_BAM;
    config->context->timer = config->Tr;
  }
}

static void J1939Tp_StartToSend(PduIdType TxPduId) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[TxPduId];

  if (config->LL_DL >= config->context->TpSduLength) {
    J1939Tp_SendDirectPG(TxPduId);
  } else if (J1939TP_PROTOCOL_CMDT == config->TxProtocol) {
    J1939Tp_StartToSend_CMDT(TxPduId);
  } else { /* BAM */
    J1939Tp_StartToSend_BAM(TxPduId);
  }
}

static void J1939Tp_TxSendCmAbort(PduIdType Channel, uint8_t AbortReason) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[Channel];
  uint8_t *data;
  Std_ReturnType ret;

  data = config->data;
  config->context->PduInfo.SduDataPtr = data;

  if (IS_J1939TP_FD(config)) {
    data[0u] = J1939TP_FD_CM_ABORT;
    /* Reserved */
    data[1] = 0xFFu;
    data[2] = 0xFFu;
    data[3] = 0xFFu;
    data[4] = 0xFFu;
    data[5] = 0xFFu;
    data[6] = 0xFFu;
    data[7] = 0xFFu;
    data[8] = AbortReason;
    data[9] = config->context->PgPGN & 0xFFu;
    data[10] = (config->context->PgPGN >> 8) & 0xFFu;
    data[11] = (config->context->PgPGN >> 16) & 0xFFu;
    config->context->PduInfo.SduLength = 12u;
  } else {
    /* CM Connection abort message */
    data[0u] = J1939TP_CM_ABORT;
    /* abort reason */
    data[1] = AbortReason;
    /* Reserved */
    data[2] = 0xFFu;
    data[3] = 0xFFu;
    data[4] = 0xFFu;
    data[5] = config->context->PgPGN & 0xFFu;
    data[6] = (config->context->PgPGN >> 8) & 0xFFu;
    data[7] = (config->context->PgPGN >> 16) & 0xFFu;
    config->context->PduInfo.SduLength = 8;
  }

  ASLOG(J1939TPI, ("[%d]TX CM.ABORT=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", Channel, data[0u],
                   data[1], data[2], data[3], data[4], data[5], data[6], data[7]));

  ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->context->PduInfo);
  if (E_OK == ret) {
    config->context->state = J1939TP_WAIT_CM_ABORT_TX_COMPLETED;
    config->context->timer = config->Tr;
  } else {
    config->context->state = J1939TP_RESEND_CM_ABORT;
    config->context->timer = config->Tr;
  }
}

static void J1939Tp_RxSendCmAbort(PduIdType Channel, uint8_t AbortReason) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[Channel];
  uint8_t *data;
  Std_ReturnType ret;

  if (J1939TP_PROTOCOL_CMDT == config->RxProtocol) {
    data = config->data;
    config->context->PduInfo.SduDataPtr = data;

    if (IS_J1939TP_FD(config)) {
      data[0u] = J1939TP_FD_CM_ABORT;
      /* Reserved */
      data[1] = 0xFFu;
      data[2] = 0xFFu;
      data[3] = 0xFFu;
      data[4] = 0xFFu;
      data[5] = 0xFFu;
      data[6] = 0xFFu;
      data[7] = 0xFFu;
      data[8] = AbortReason;
      data[9] = config->context->PgPGN & 0xFFu;
      data[10] = (config->context->PgPGN >> 8) & 0xFFu;
      data[11] = (config->context->PgPGN >> 16) & 0xFFu;
      config->context->PduInfo.SduLength = 12u;
    } else {
      /* CM Connection abort message */
      data[0u] = J1939TP_CM_ABORT;
      /* abort reason */
      data[1] = AbortReason;
      /* Reserved */
      data[2] = 0xFFu;
      data[3] = 0xFFu;
      data[4] = 0xFFu;
      data[5] = config->context->PgPGN & 0xFFu;
      data[6] = (config->context->PgPGN >> 8) & 0xFFu;
      data[7] = (config->context->PgPGN >> 16) & 0xFFu;
      config->context->PduInfo.SduLength = 8;
    }

    ASLOG(J1939TPI, ("[%d]RX -> TX CM.ABORT=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", Channel,
                     data[0u], data[1], data[2], data[3], data[4], data[5], data[6], data[7]));

    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->context->PduInfo);
    if (E_OK == ret) {
      config->context->state = J1939TP_WAIT_CM_ABORT_TX_COMPLETED;
      config->context->timer = config->Tr;
    } else {
      config->context->state = J1939TP_RESEND_CM_ABORT;
      config->context->timer = config->Tr;
    }
  } else {
    J1939Tp_RxResetToIdle(config);
    PduR_J1939TpRxIndication(config->PduR_RxPduId, E_NOT_OK);
  }
}

static void J1939Tp_RxSendCmCTS(PduIdType Channel) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[Channel];
  uint8_t *data;
  Std_ReturnType ret;
  uint32_t nextSN;
  uint8_t numOfPackets;

  if (IS_J1939TP_FD(config)) {
    numOfPackets = NUM_OF_FD_DT_PACKETS - config->context->NextSN;
  } else {
    numOfPackets = NUM_OF_DT_PACKETS - config->context->NextSN;
  }

  if (numOfPackets > config->RxPacketsPerBlock) {
    numOfPackets = config->RxPacketsPerBlock;
  }

  if (numOfPackets > config->context->MaxPacketsPerBlock) {
    numOfPackets = config->context->MaxPacketsPerBlock;
  }

  config->context->NumPacketsThisBlock = numOfPackets;

  if (J1939TP_PROTOCOL_CMDT == config->RxProtocol) {
    data = config->data;
    config->context->PduInfo.SduDataPtr = data;
    nextSN = config->context->NextSN + 1u;

    if (IS_J1939TP_FD(config)) {
      data[0u] = J1939TP_FD_CM_CTS;
      /* Reserved */
      data[1] = 0xFFu;
      data[2] = 0xFFu;
      data[3] = 0xFFu;

      data[4] = nextSN & 0xFFu;
      data[5] = (nextSN >> 8) & 0xFFu;
      data[6] = (nextSN >> 16) & 0xFFu;
      data[7] = numOfPackets;
      data[8] = 0u; /* RQST = 0u */
      data[9] = config->context->PgPGN & 0xFFu;
      data[10] = (config->context->PgPGN >> 8) & 0xFFu;
      data[11] = (config->context->PgPGN >> 16) & 0xFFu;
      config->context->PduInfo.SduLength = 12u;
    } else {
      data[0u] = J1939TP_CM_CTS;
      data[1] = numOfPackets;
      data[2] = nextSN & 0xFFu;
      data[3] = 0xFFu;
      data[4] = 0xFFu;
      data[5] = config->context->PgPGN & 0xFFu;
      data[6] = (config->context->PgPGN >> 8) & 0xFFu;
      data[7] = (config->context->PgPGN >> 16) & 0xFFu;
      config->context->PduInfo.SduLength = 8;
    }

    ASLOG(J1939TPI, ("[%d]RX -> TX CM.CTS=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", Channel,
                     data[0u], data[1], data[2], data[3], data[4], data[5], data[6], data[7]));

    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->context->PduInfo);
    if (E_OK == ret) {
      config->context->state = J1939TP_WAIT_CM_CTS_TX_COMPLETED;
      config->context->timer = config->Tr;
    } else {
      config->context->state = J1939TP_RESEND_CM_CTS;
      config->context->timer = config->Tr;
    }
  } else {
    config->context->state = J1939TP_WAIT_DT_RX;
    config->context->timer = config->T1;
  }
}

static void J1939Tp_RxSendCmEOMA(PduIdType Channel) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[Channel];
  uint8_t *data;
  Std_ReturnType ret;

  if (J1939TP_PROTOCOL_CMDT == config->RxProtocol) {
    data = config->data;
    config->context->PduInfo.SduDataPtr = data;

    if (IS_J1939TP_FD(config)) {
      data[0u] = J1939TP_FD_CM_EOMA;
      data[1] = config->context->TpSduLength & 0xFFu;
      data[2] = (config->context->TpSduLength >> 8) & 0xFFu;
      data[3] = (config->context->TpSduLength >> 16) & 0xFFu;

      data[4] = NUM_OF_FD_DT_PACKETS & 0xFFu;
      data[5] = (NUM_OF_FD_DT_PACKETS >> 8) & 0xFFu;
      data[6] = (NUM_OF_FD_DT_PACKETS >> 16) & 0xFFu;
      data[7] = 0xFFu;
      data[8] = 0xFFu;
      data[9] = config->context->PgPGN & 0xFFu;
      data[10] = (config->context->PgPGN >> 8) & 0xFFu;
      data[11] = (config->context->PgPGN >> 16) & 0xFFu;
      config->context->PduInfo.SduLength = 12u;
    } else {
      data[0u] = J1939TP_CM_EOMA;
      data[1] = config->context->TpSduLength & 0xFFu;
      data[2] = (config->context->TpSduLength >> 8) & 0xFFu;
      data[3] = NUM_OF_DT_PACKETS;
      data[4] = 0xFFu;
      data[5] = config->context->PgPGN & 0xFFu;
      data[6] = (config->context->PgPGN >> 8) & 0xFFu;
      data[7] = (config->context->PgPGN >> 16) & 0xFFu;
      config->context->PduInfo.SduLength = 8;
    }

    ASLOG(J1939TPI, ("[%d]RX -> TX CM.EOMA=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", Channel,
                     data[0u], data[1], data[2], data[3], data[4], data[5], data[6], data[7]));

    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->context->PduInfo);
    if (E_OK == ret) {
      config->context->state = J1939TP_WAIT_CM_EOMA_TX_COMPLETED;
      config->context->timer = config->Tr;
    } else {
      config->context->state = J1939TP_RESEND_CM_EOMS;
      config->context->timer = config->Tr;
    }
  } else {
    J1939Tp_RxResetToIdle(config);
  }
}

static void J1939Tp_HandleDT_TxConfirmation_TxChannel(PduIdType Channel, Std_ReturnType result) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[Channel];
  if (E_OK == result) {
    if ((IS_J1939TP_FD(config) && ((config->context->NextSN + 1u) < NUM_OF_FD_DT_PACKETS)) ||
        ((config->context->NextSN + 1u) < NUM_OF_DT_PACKETS)) {
      config->context->NextSN++;
      if (J1939TP_PROTOCOL_CMDT == config->TxProtocol) {
        if (config->context->NumPacketsToSend > 0u) {
          config->context->NumPacketsToSend--;
        }
        if (0u == config->context->NumPacketsToSend) {
          config->context->state = J1939TP_WAIT_CM_CTS_RX;
          config->context->timer = config->T3;
        } else {
          J1939Tp_SendDT(Channel);
        }
      } else {
        J1939Tp_SendDT(Channel);
      }
    } else { /* reach the end */
      if (IS_J1939TP_FD(config)) {
        J1939Tp_SendEOMS(Channel);
      } else {
        if (J1939TP_PROTOCOL_CMDT == config->TxProtocol) {
          config->context->state = J1939TP_WAIT_END_OF_MSG_ACK_RX;
          config->context->timer = config->T3;
        } else {
          J1939Tp_TxResetToIdle(config);
          PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_OK);
        }
      }
    }
  } else {
    J1939Tp_TxResetToIdle(config);
    PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
    ASLOG(J1939TPE, ("[%d] Fatal Error, send DT failed\n", Channel));
    /* as maybe network issue, bus off, etc, so not send Abort message */
  }
}

static void J1939Tp_TxConfirmation_TxChannel(PduIdType Channel, Std_ReturnType result) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[Channel];
  switch (config->context->state) {
  case J1939TP_WAIT_DIRECT_PG_TX_COMPLETED:
    J1939Tp_TxResetToIdle(config);
    PduR_J1939TpTxConfirmation(config->PduR_TxPduId, result);
    break;
  case J1939TP_WAIT_CM_RTS_TX_COMPLETED:
    if (E_OK != result) {
      J1939Tp_TxResetToIdle(config);
      PduR_J1939TpTxConfirmation(config->PduR_TxPduId, result);
    } else {
      config->context->state = J1939TP_WAIT_CM_CTS_RX;
      config->context->timer = config->T3;
    }
    break;
  case J1939TP_WAIT_CM_BAM_TX_COMPLETED:
    if (E_OK != result) {
      J1939Tp_TxResetToIdle(config);
      PduR_J1939TpTxConfirmation(config->PduR_TxPduId, result);
    } else {
      config->context->NumPacketsToSend = 0xFFu;
      J1939Tp_SendDT(Channel);
    }
    break;
  case J1939TP_WAIT_CM_ABORT_TX_COMPLETED:
    J1939Tp_TxResetToIdle(config);
    PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
    break;
  case J1939TP_WAIT_DT_TX_COMPLETED:
    J1939Tp_HandleDT_TxConfirmation_TxChannel(Channel, result);
    break;
  case J1939TP_WAIT_CM_EOMS_TX_COMPLETED:
    if (E_OK != result) {
      J1939Tp_TxResetToIdle(config);
      PduR_J1939TpTxConfirmation(config->PduR_TxPduId, result);
    } else {
      if (J1939TP_PROTOCOL_BAM == config->TxProtocol) {
        J1939Tp_TxResetToIdle(config);
        PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_OK);
      } else {
        config->context->state = J1939TP_WAIT_END_OF_MSG_ACK_RX;
        config->context->timer = config->T3;
      }
    }
    break;
  default:
    ASLOG(J1939TPE,
          ("[%d] TX TxConfirmation when in state %d, abort\n", Channel, config->context->state));
    J1939Tp_TxResetToIdle(config);
    break;
  }
}

static void J1939Tp_HandleCM_CTS(PduIdType Channel, const PduInfoType *PduInfoPtr) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[Channel];
  uint8_t NumPacketsToSend;
  uint32_t NextSN;
  uint32_t PgPGN;
  uint8_t RequestField = 0u;
  uint32_t numOfDtPackets = 0;
  Std_ReturnType ret = E_NOT_OK;
  if (IS_J1939TP_FD(config)) {
    if ((J1939TP_FD_CM_CTS == PduInfoPtr->SduDataPtr[0u]) &&
        (12u == PduInfoPtr->SduLength)) { /* clear to send */
      NextSN = PduInfoPtr->SduDataPtr[4] + ((uint32_t)PduInfoPtr->SduDataPtr[5] << 8) +
               ((uint32_t)PduInfoPtr->SduDataPtr[6] << 16);
      NumPacketsToSend = PduInfoPtr->SduDataPtr[7];
      RequestField = PduInfoPtr->SduDataPtr[9];
      PgPGN = PduInfoPtr->SduDataPtr[9] + ((uint32_t)PduInfoPtr->SduDataPtr[10] << 8) +
              ((uint32_t)PduInfoPtr->SduDataPtr[11] << 16);
      numOfDtPackets = NUM_OF_DT_PACKETS;
      ret = E_OK;
    }
  } else {
    if ((J1939TP_CM_CTS == PduInfoPtr->SduDataPtr[0u]) &&
        ((8u == PduInfoPtr->SduLength))) { /* clear to send */
      NumPacketsToSend = PduInfoPtr->SduDataPtr[1];
      NextSN = PduInfoPtr->SduDataPtr[2];
      PgPGN = PduInfoPtr->SduDataPtr[5] + ((uint32_t)PduInfoPtr->SduDataPtr[6] << 8) +
              ((uint32_t)PduInfoPtr->SduDataPtr[7] << 16);
      numOfDtPackets = NUM_OF_FD_DT_PACKETS;
      ret = E_OK;
    }
  }
  if (E_OK == ret) {
    if (PgPGN == config->context->PgPGN) {
      if (NumPacketsToSend == 0u) {
        /* Receiver wants to keep the connection open but can't receive packets */
        config->context->timer = config->T4;
      } else if ((1u == RequestField) && (0xFFFFFFu == NextSN)) {
        /* Request the FD.TP.CM_EndOfMsgStatus to be resent */
        J1939Tp_SendEOMS(Channel);
      } else if ((NextSN == (config->context->NextSN + 1u)) &&
                 (((uint32_t)config->context->NextSN + NumPacketsToSend) <= numOfDtPackets)) {
        config->context->NumPacketsToSend = NumPacketsToSend;
        J1939Tp_SendDT(Channel);
      } else {
        PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
        J1939Tp_TxSendCmAbort(Channel, J1939TP_ABORT_REASON_BAD_SEQUENCE_NUMBER);
      }
    }
  } else {
    ASLOG(J1939TPE,
          ("[%d] TX Get Packet CM %02X len=%d when in state %d, ignore\n", Channel,
           PduInfoPtr->SduDataPtr[0u], (int)PduInfoPtr->SduLength, config->context->state));
  }
}

static void J1939Tp_HandleCM_EOMA(PduIdType Channel, const PduInfoType *PduInfoPtr) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[Channel];
  uint32_t totalMsgSize;
  uint32_t totalNumPackets;
  uint32_t PgPGN;
  if (IS_J1939TP_FD(config)) {
    if ((J1939TP_FD_CM_EOMA == PduInfoPtr->SduDataPtr[0u]) && (12u <= PduInfoPtr->SduLength)) {
      PgPGN = PduInfoPtr->SduDataPtr[9] + ((uint32_t)PduInfoPtr->SduDataPtr[10] << 8) +
              ((uint32_t)PduInfoPtr->SduDataPtr[11] << 16);

      if (PgPGN == config->context->PgPGN) {
        totalMsgSize = PduInfoPtr->SduDataPtr[1] + ((uint32_t)PduInfoPtr->SduDataPtr[2] << 8) +
                       ((uint32_t)PduInfoPtr->SduDataPtr[3] << 16);
        totalNumPackets = PduInfoPtr->SduDataPtr[4] + ((uint32_t)PduInfoPtr->SduDataPtr[5] << 8) +
                          ((uint32_t)PduInfoPtr->SduDataPtr[6] << 16);
        if ((totalMsgSize == config->context->TpSduLength) &&
            (totalNumPackets == NUM_OF_FD_DT_PACKETS)) {
          J1939Tp_TxResetToIdle(config);
          PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_OK);
        } else {
          J1939Tp_TxResetToIdle(config);
          PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
        }
      }
    }
  } else {
    if ((J1939TP_CM_EOMA == PduInfoPtr->SduDataPtr[0u]) && (8u == PduInfoPtr->SduLength)) {
      PgPGN = PduInfoPtr->SduDataPtr[5] + ((uint32_t)PduInfoPtr->SduDataPtr[6] << 8) +
              ((uint32_t)PduInfoPtr->SduDataPtr[7] << 16);
      if (PgPGN == config->context->PgPGN) {
        totalMsgSize = PduInfoPtr->SduDataPtr[1] + ((uint32_t)PduInfoPtr->SduDataPtr[2] << 8);
        totalNumPackets = PduInfoPtr->SduDataPtr[3];
        if ((totalMsgSize == config->context->TpSduLength) &&
            (totalNumPackets == NUM_OF_DT_PACKETS)) {
          J1939Tp_TxResetToIdle(config);
          PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_OK);
        } else {
          J1939Tp_TxResetToIdle(config);
          PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
        }
      }
    }
  }
}

static void J1939Tp_HandleCM(PduIdType Channel, const PduInfoType *PduInfoPtr) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[Channel];
  if (J1939TP_WAIT_CM_CTS_RX == config->context->state) {
    J1939Tp_HandleCM_CTS(Channel, PduInfoPtr);
  } else if (J1939TP_WAIT_END_OF_MSG_ACK_RX == config->context->state) {
    J1939Tp_HandleCM_EOMA(Channel, PduInfoPtr);
  } else {
    ASLOG(J1939TPE,
          ("[%d] TX Get Packet CM when in state %d, ignore\n", Channel, config->context->state));
  }
}

static void J1939Tp_RxIndication_TxChannel(PduIdType Channel, J1939Tp_PacketType PacketType,
                                           const PduInfoType *PduInfoPtr) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[Channel];

  switch (PacketType) {
  case J1939TP_PACKET_CM:
    J1939Tp_HandleCM(Channel, PduInfoPtr);
    break;
  default:
    ASLOG(J1939TPE, ("[%d] TX Get Packet %d when in state %d, ignore\n", Channel, PacketType,
                     config->context->state));
    break;
  }
}

static void J1939Tp_TxConfirmation_RxChannel(PduIdType Channel, Std_ReturnType result) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[Channel];

  if (E_OK == result) {
    switch (config->context->state) {
    case J1939TP_WAIT_CM_ABORT_TX_COMPLETED:
      J1939Tp_RxResetToIdle(config);
      PduR_J1939TpTxConfirmation(config->PduR_RxPduId, E_NOT_OK);
      break;
    case J1939TP_WAIT_CM_CTS_TX_COMPLETED:
      config->context->state = J1939TP_WAIT_DT_RX;
      config->context->timer = config->T2;
      break;
    case J1939TP_WAIT_CM_EOMA_TX_COMPLETED:
      J1939Tp_RxResetToIdle(config);
      break;
    default:
      ASLOG(J1939TPE,
            ("[%d] RX TxConfirmation when in state %d, abort\n", Channel, config->context->state));
      J1939Tp_RxResetToIdle(config);
      break;
    }
  }
}

static void J1939Tp_RxIndication_RxChannel_CM_RTS(PduIdType Channel,
                                                  const PduInfoType *PduInfoPtr) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[Channel];
  Std_ReturnType ret = E_NOT_OK;
  uint32_t totalMsgSize;
  uint32_t totalNumPackets;
  uint32_t PgPGN;
  uint8_t maxPacketsPerBlock;
  uint8_t ADType = 0u;
  PduLengthType bufferSize;
  BufReq_ReturnType bufReq;

  if (IS_J1939TP_FD(config) && (J1939TP_IDLE == config->context->state)) {
    if (12u == PduInfoPtr->SduLength) {
      PgPGN = PduInfoPtr->SduDataPtr[9] + ((uint32_t)PduInfoPtr->SduDataPtr[10] << 8) +
              ((uint32_t)PduInfoPtr->SduDataPtr[11] << 16);
      totalMsgSize = PduInfoPtr->SduDataPtr[1] + ((uint32_t)PduInfoPtr->SduDataPtr[2] << 8) +
                     ((uint32_t)PduInfoPtr->SduDataPtr[3] << 16);
      totalNumPackets = PduInfoPtr->SduDataPtr[4] + ((uint32_t)PduInfoPtr->SduDataPtr[5] << 8) +
                        ((uint32_t)PduInfoPtr->SduDataPtr[6] << 16);
      maxPacketsPerBlock = PduInfoPtr->SduDataPtr[7];
      ADType = PduInfoPtr->SduDataPtr[8];
      if ((NUM_FD_DT_PACKETS(totalMsgSize) == totalNumPackets) && (0u != maxPacketsPerBlock)) {
        if (0u == ADType) {
          ret = E_OK;
        } else {
          /* TODO */
          ASLOG(J1939TPE, ("[%d]RX Get FD CM RTS with assurance is not supported\n", Channel));
        }
      }
    } else {
      ASLOG(J1939TPE, ("[%d]RX Get FD CM RTS with invalid length\n", Channel));
    }
  } else if (J1939TP_IDLE == config->context->state) {
    PgPGN = PduInfoPtr->SduDataPtr[5] + ((uint32_t)PduInfoPtr->SduDataPtr[6] << 8) +
            ((uint32_t)PduInfoPtr->SduDataPtr[7] << 16);
    totalMsgSize = PduInfoPtr->SduDataPtr[1] + ((uint32_t)PduInfoPtr->SduDataPtr[2] << 8);
    totalNumPackets = PduInfoPtr->SduDataPtr[3];
    maxPacketsPerBlock = PduInfoPtr->SduDataPtr[4];
    if ((NUM_DT_PACKETS(totalMsgSize) == totalNumPackets) && (0u != maxPacketsPerBlock)) {
      ret = E_OK;
    }
  } else {
    ASLOG(J1939TPE, ("[%d]RX Get CM RTS %02X when in state %d, ignore\n", Channel,
                     PduInfoPtr->SduDataPtr[0u], config->context->state));
  }

  if (E_OK == ret) {
    config->context->PgPGN = PgPGN;
    bufReq = PduR_J1939TpStartOfReception(config->PduR_RxPduId, NULL, totalMsgSize, &bufferSize);
    if (BUFREQ_OK == bufReq) {
      config->context->NextSN = 0u;
      config->context->TpSduLength = totalMsgSize;
      config->context->MaxPacketsPerBlock = maxPacketsPerBlock;
      J1939Tp_RxSendCmCTS(Channel);
    } else {
      J1939Tp_RxSendCmAbort(Channel, J1939TP_ABORT_REASON_MESSAGE_TOO_BIG);
    }
  }
}

static void J1939Tp_RxIndication_RxChannel_CM_BAM(PduIdType Channel,
                                                  const PduInfoType *PduInfoPtr) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[Channel];
  Std_ReturnType ret = E_NOT_OK;
  uint32_t totalMsgSize;
  uint32_t totalNumPackets;
  uint32_t PgPGN;
  uint8_t ADType = 0u;
  PduLengthType bufferSize;
  BufReq_ReturnType bufReq;

  if (IS_J1939TP_FD(config) && (J1939TP_IDLE == config->context->state)) {
    if (12u == PduInfoPtr->SduLength) {
      PgPGN = PduInfoPtr->SduDataPtr[9] + ((uint32_t)PduInfoPtr->SduDataPtr[10] << 8) +
              ((uint32_t)PduInfoPtr->SduDataPtr[11] << 16);
      totalMsgSize = PduInfoPtr->SduDataPtr[1] + ((uint32_t)PduInfoPtr->SduDataPtr[2] << 8) +
                     ((uint32_t)PduInfoPtr->SduDataPtr[3] << 16);
      totalNumPackets = PduInfoPtr->SduDataPtr[4] + ((uint32_t)PduInfoPtr->SduDataPtr[5] << 8) +
                        ((uint32_t)PduInfoPtr->SduDataPtr[6] << 16);
      ADType = PduInfoPtr->SduDataPtr[8];
      if (NUM_FD_DT_PACKETS(totalMsgSize) == totalNumPackets) {
        if (0u == ADType) {
          ret = E_OK;
        } else {
          /* TODO */
          ASLOG(J1939TPE, ("[%d]RX Get FD CM BAM with assurance is not supported\n", Channel));
        }
      }

    } else {
      ASLOG(J1939TPE, ("[%d]RX Get FD CM BAM with invalid length\n", Channel));
    }
  } else if (J1939TP_IDLE == config->context->state) {
    PgPGN = PduInfoPtr->SduDataPtr[5] + ((uint32_t)PduInfoPtr->SduDataPtr[6] << 8) +
            ((uint32_t)PduInfoPtr->SduDataPtr[7] << 16);
    totalMsgSize = PduInfoPtr->SduDataPtr[1] + ((uint32_t)PduInfoPtr->SduDataPtr[2] << 8);
    totalNumPackets = PduInfoPtr->SduDataPtr[3];
    if (NUM_DT_PACKETS(totalMsgSize) == totalNumPackets) {
      ret = E_OK;
    }
  } else {
    ASLOG(J1939TPE, ("[%d]RX Get CM BAM %02X when in state %d, ignore\n", Channel,
                     PduInfoPtr->SduDataPtr[0u], config->context->state));
  }

  if (E_OK == ret) {
    config->context->PgPGN = PgPGN;
    bufReq = PduR_J1939TpStartOfReception(config->PduR_RxPduId, NULL, totalMsgSize, &bufferSize);
    if (BUFREQ_OK == bufReq) {
      config->context->NextSN = 0u;
      config->context->TpSduLength = totalMsgSize;
      config->context->MaxPacketsPerBlock = 0xFFu;
      config->context->state = J1939TP_WAIT_DT_RX;
      config->context->timer = config->T2;
    }
  }
}

static void J1939Tp_RxIndication_RxChannel_CM_EOMS(PduIdType Channel,
                                                   const PduInfoType *PduInfoPtr) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[Channel];

  uint32_t totalMsgSize;
  uint32_t totalNumPackets;
  uint32_t PgPGN;
  uint8_t ADSize;
  uint8_t ADType;

  if (J1939TP_WAIT_CM_EOMS_RX == config->context->state) {
    PgPGN = PduInfoPtr->SduDataPtr[9] + ((uint32_t)PduInfoPtr->SduDataPtr[10] << 8) +
            ((uint32_t)PduInfoPtr->SduDataPtr[11] << 16);
    if (PgPGN == config->context->PgPGN) {
      totalMsgSize = PduInfoPtr->SduDataPtr[1] + ((uint32_t)PduInfoPtr->SduDataPtr[2] << 8) +
                     ((uint32_t)PduInfoPtr->SduDataPtr[3] << 16);
      totalNumPackets = PduInfoPtr->SduDataPtr[4] + ((uint32_t)PduInfoPtr->SduDataPtr[5] << 8) +
                        ((uint32_t)PduInfoPtr->SduDataPtr[6] << 16);
      ADSize = PduInfoPtr->SduDataPtr[7];
      ADType = PduInfoPtr->SduDataPtr[8];
      if ((ADSize + 12u) == PduInfoPtr->SduLength) {
        if ((totalMsgSize == config->context->TpSduLength) &&
            (totalNumPackets == NUM_OF_DT_PACKETS)) {
          if ((0u == ADType) && (0u == ADSize)) { /* No assurance data (default) */
            PduR_J1939TpRxIndication(config->PduR_RxPduId, E_OK);
            J1939Tp_RxSendCmEOMA(Channel);
          } else {
            /* TODO: */
            ASLOG(J1939TPE, ("[%d]RX Get CM EOMS with assurance is not supported\n", Channel));
            J1939Tp_RxSendCmAbort(Channel, J1939TP_ABORT_REASON_ASSURANCE_DATA_NOT_MATCH);
          }
        }
      } else {
        ASLOG(J1939TPE, ("[%d]RX Get CM EOMS with invalid length\n", Channel));
      }
    }
  } else {
    ASLOG(J1939TPE, ("[%d]RX Get CM EOMS %02X when in state %d, ignore\n", Channel,
                     PduInfoPtr->SduDataPtr[0u], config->context->state));
  }
}

static void J1939Tp_RxIndication_RxChannel_CM(PduIdType Channel, const PduInfoType *PduInfoPtr) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[Channel];
  if (IS_J1939TP_FD(config)) {
    if (12u <= PduInfoPtr->SduLength) {
      if (J1939TP_FD_CM_RTS == PduInfoPtr->SduDataPtr[0u]) {
        J1939Tp_RxIndication_RxChannel_CM_RTS(Channel, PduInfoPtr);
      } else if (J1939TP_FD_CM_EOMS == PduInfoPtr->SduDataPtr[0u]) {
        J1939Tp_RxIndication_RxChannel_CM_EOMS(Channel, PduInfoPtr);
      } else if (J1939TP_FD_CM_BAM == PduInfoPtr->SduDataPtr[0u]) {
        J1939Tp_RxIndication_RxChannel_CM_BAM(Channel, PduInfoPtr);
      } else {
        ASLOG(J1939TPE, ("[%d]RX FD unknwon CM %02X\n", Channel, PduInfoPtr->SduDataPtr[0u]));
      }
    } else {
      ASLOG(J1939TPE,
            ("[%d]RX FD CM with invalid length: %d\n", Channel, (int)PduInfoPtr->SduLength));
    }
  } else {
    if (8u == PduInfoPtr->SduLength) {
      if (J1939TP_CM_RTS == PduInfoPtr->SduDataPtr[0u]) {
        J1939Tp_RxIndication_RxChannel_CM_RTS(Channel, PduInfoPtr);
      } else if (J1939TP_CM_BAM == PduInfoPtr->SduDataPtr[0u]) {
        J1939Tp_RxIndication_RxChannel_CM_BAM(Channel, PduInfoPtr);
      } else {
        ASLOG(J1939TPE, ("[%d]RX FD unknwon CM %02X\n", Channel, PduInfoPtr->SduDataPtr[0u]));
      }
    } else {
      ASLOG(J1939TPE, ("[%d]RX CM with invalid length\n", Channel));
    }
  }
}

static void J1939Tp_RxIndication_RxChannel_DT(PduIdType Channel, const PduInfoType *PduInfoPtr) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[Channel];
  Std_ReturnType ret = E_NOT_OK;
  PduInfoType PduInfo;
  uint32_t SN;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
  PduLengthType offset = 0u;

  if (J1939TP_WAIT_DT_RX == config->context->state) {
    if (IS_J1939TP_FD(config)) {
      if ((J1939TP_FD_DTFI == PduInfoPtr->SduDataPtr[0u]) && (PduInfoPtr->SduLength > 4u)) {
        SN = PduInfoPtr->SduDataPtr[1] + ((uint32_t)PduInfoPtr->SduDataPtr[2] << 8) +
             ((uint32_t)PduInfoPtr->SduDataPtr[3] << 16);
        offset = config->context->NextSN * FD_DT_DATA_SIZE;
        PduInfo.SduLength = config->context->TpSduLength - offset;
        PduInfo.SduDataPtr = &PduInfoPtr->SduDataPtr[4];
        if (PduInfo.SduLength > (PduInfoPtr->SduLength - 4u)) {
          PduInfo.SduLength = PduInfoPtr->SduLength - 4u;
        }
        ret = E_OK;
      } else {
        ASLOG(J1939TPE, ("[%d]RX DT witn invalid DTFI or Length\n", Channel));
      }
    } else {
      if (PduInfoPtr->SduLength > 1u) {
        SN = PduInfoPtr->SduDataPtr[0u];
        offset = config->context->NextSN * DT_DATA_SIZE;
        PduInfo.SduLength = config->context->TpSduLength - offset;
        PduInfo.SduDataPtr = &PduInfoPtr->SduDataPtr[1];
        if (PduInfo.SduLength > (PduInfoPtr->SduLength - 1u)) {
          PduInfo.SduLength = PduInfoPtr->SduLength - 1u;
        }
        ret = E_OK;
      } else {
        ASLOG(J1939TPE, ("[%d]RX DT witn invalid Length\n", Channel));
      }
    }
  } else {
    ASLOG(J1939TPE, ("[%d]RX DT %02X when in state %d, ignore\n", Channel,
                     PduInfoPtr->SduDataPtr[0u], config->context->state));
  }

  if (E_OK == ret) {
    if (SN == (config->context->NextSN + 1u)) {
      config->context->NextSN++;
      PduInfo.MetaDataPtr = (uint8_t *)&offset;
      bufReq = PduR_J1939TpCopyRxData(config->PduR_RxPduId, &PduInfo, &bufferSize);
      if (BUFREQ_OK == bufReq) {
        if (IS_J1939TP_FD(config) && (config->context->NextSN >= NUM_OF_FD_DT_PACKETS)) {
          config->context->state = J1939TP_WAIT_CM_EOMS_RX;
          config->context->timer = config->T1;
        } else if (config->context->NextSN >= NUM_OF_DT_PACKETS) {
          PduR_J1939TpRxIndication(config->PduR_RxPduId, E_OK);
          J1939Tp_RxSendCmEOMA(Channel);
        } else {
          if (config->context->NumPacketsThisBlock > 0u) {
            config->context->NumPacketsThisBlock--;
          }
          if (0u == config->context->NumPacketsThisBlock) {
            J1939Tp_RxSendCmCTS(Channel);
          } else {
            config->context->state = J1939TP_WAIT_DT_RX;
            config->context->timer = config->T1;
          }
        }
      } else {
        ASLOG(J1939TPE, ("[%d]RX DT CopyRxData failed.\n", Channel));
        J1939Tp_RxSendCmAbort(Channel, J1939TP_ABORT_REASON_OUT_OF_RESOURCE);
      }
    } else {
      ASLOG(J1939TPE, ("[%d]RX DT with invalid SN\n", Channel));
      J1939Tp_RxSendCmAbort(Channel, J1939TP_ABORT_REASON_BAD_SEQUENCE_NUMBER);
      ret = E_NOT_OK;
    }
  }
}

static void J1939Tp_RxIndication_RxChannel_Direct(PduIdType Channel,
                                                  const PduInfoType *PduInfoPtr) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[Channel];
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
  PduLengthType offset = 0u;
  PduInfoType PduInfo;

  bufReq = PduR_J1939TpStartOfReception(config->PduR_RxPduId, PduInfoPtr, PduInfoPtr->SduLength,
                                        &bufferSize);
  if (BUFREQ_OK == bufReq) {
    PduInfo.SduDataPtr = PduInfoPtr->SduDataPtr;
    PduInfo.SduLength = PduInfoPtr->SduLength;
    PduInfo.MetaDataPtr = (uint8_t *)&offset;
    bufReq = PduR_J1939TpCopyRxData(config->PduR_RxPduId, &PduInfo, &bufferSize);
    if (BUFREQ_OK == bufReq) {
      PduR_J1939TpRxIndication(config->PduR_RxPduId, E_OK);
    }
  }
}

static void J1939Tp_RxIndication_RxChannel(PduIdType Channel, J1939Tp_PacketType PacketType,
                                           const PduInfoType *PduInfoPtr) {
  ASLOG(J1939TP, ("[%d]RX Packet Type %d\n", Channel, PacketType));
  switch (PacketType) {
  case J1939TP_PACKET_CM:
    J1939Tp_RxIndication_RxChannel_CM(Channel, PduInfoPtr);
    break;
  case J1939TP_PACKET_DT:
    J1939Tp_RxIndication_RxChannel_DT(Channel, PduInfoPtr);
    break;
  case J1939TP_PACKET_DIRECT:
    J1939Tp_RxIndication_RxChannel_Direct(Channel, PduInfoPtr);
    break;
  default:
    break;
  }
}

/* ================================ [ FUNCTIONS ] ============================================== */
void J1939Tp_MainFunction_TxChannelFast(uint16_t Channel) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[Channel];
  Std_ReturnType ret;

  switch (config->context->state) {
  case J1939TP_RESEND_DIRECT_PG:
    ret = CanIf_Transmit(config->CanIf_TxDirectNPdu, &config->context->PduInfo);
    if (E_OK == ret) {
      config->context->state = J1939TP_WAIT_DIRECT_PG_TX_COMPLETED;
      config->context->timer = config->Tr;
    }
    break;
  case J1939TP_RESEND_CM_RTS:
    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->context->PduInfo);
    if (E_OK == ret) {
      config->context->state = J1939TP_WAIT_CM_RTS_TX_COMPLETED;
      config->context->timer = config->Tr;
    }
    break;
  case J1939TP_RESEND_CM_BAM:
    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->context->PduInfo);
    if (E_OK == ret) {
      config->context->state = J1939TP_WAIT_CM_BAM_TX_COMPLETED;
      config->context->timer = config->Tr;
    }
    break;
  case J1939TP_RESEND_CM_ABORT:
    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->context->PduInfo);
    if (E_OK == ret) {
      config->context->state = J1939TP_WAIT_CM_ABORT_TX_COMPLETED;
      config->context->timer = config->Tr;
    }
    break;
  case J1939TP_RESEND_DT:
    ret = CanIf_Transmit(config->CanIf_TxDtNPdu, &config->context->PduInfo);
    if (E_OK == ret) {
      config->context->state = J1939TP_WAIT_DT_TX_COMPLETED;
      config->context->timer = config->Tr;
    }
    break;
  case J1939TP_RESEND_CM_EOMS:
    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->context->PduInfo);
    if (E_OK == ret) {
      config->context->state = J1939TP_WAIT_CM_EOMS_TX_COMPLETED;
      config->context->timer = config->Tr;
    }
    break;
  default:
    break;
  }
}

void J1939Tp_MainFunction_TxChannel(uint16_t Channel) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[Channel];
  Std_ReturnType ret;

  if (config->context->timer > 0u) {
    config->context->timer--;
    if (0u == config->context->timer) {
      if (J1939TP_WAIT_DT_STMIN_TIMEOUT != config->context->state) {
        ASLOG(J1939TPE, ("[%d]TX timer timeout in state %d\n", Channel, config->context->state));
      }
      switch (config->context->state) {
      case J1939TP_WAIT_DT_STMIN_TIMEOUT:
        ret = CanIf_Transmit(config->CanIf_TxDtNPdu, &config->context->PduInfo);
        if (E_OK == ret) {
          config->context->state = J1939TP_WAIT_DT_TX_COMPLETED;
          config->context->timer = config->Tr;
        } else {
          config->context->state = J1939TP_RESEND_DT;
          config->context->timer = config->Tr;
        }
        break;
      case J1939TP_WAIT_DIRECT_PG_TX_COMPLETED:
      case J1939TP_WAIT_CM_RTS_TX_COMPLETED:
      case J1939TP_WAIT_CM_BAM_TX_COMPLETED:
      case J1939TP_WAIT_CM_ABORT_TX_COMPLETED:
        J1939Tp_TxResetToIdle(config);
        PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
        break;
      case J1939TP_WAIT_CM_CTS_RX:
      case J1939TP_WAIT_DT_TX_COMPLETED:
        J1939Tp_TxSendCmAbort(Channel, J1939TP_ABORT_REASON_TIMEOUT);
        break;
      default:
        J1939Tp_TxResetToIdle(config);
        break;
      }
    }
  }
}

void J1939Tp_MainFunction_RxChannelFast(uint16_t Channel) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[Channel];
  Std_ReturnType ret;

  switch (config->context->state) {
  case J1939TP_RESEND_CM_CTS:
    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->context->PduInfo);
    if (E_OK == ret) {
      config->context->state = J1939TP_WAIT_CM_CTS_TX_COMPLETED;
      config->context->timer = config->Tr;
    }
    break;
  case J1939TP_RESEND_CM_EMOA:
    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->context->PduInfo);
    if (E_OK == ret) {
      config->context->state = J1939TP_WAIT_CM_EOMA_TX_COMPLETED;
      config->context->timer = config->Tr;
    }
    break;
  default:
    break;
  }
}

void J1939Tp_MainFunction_RxChannel(uint16_t Channel) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[Channel];

  if (config->context->timer > 0u) {
    config->context->timer--;
    if (0u == config->context->timer) {
      ASLOG(J1939TPE, ("[%d]RX timer timeout in state %d\n", Channel, config->context->state));
      switch (config->context->state) {
      case J1939TP_WAIT_CM_CTS_TX_COMPLETED:
        J1939Tp_RxResetToIdle(config);
        PduR_J1939TpRxIndication(config->PduR_RxPduId, E_NOT_OK);
        break;
      case J1939TP_WAIT_CM_EOMA_TX_COMPLETED:
        J1939Tp_RxResetToIdle(config); /* do nothing */
        break;
      default:
        J1939Tp_RxResetToIdle(config);
        break;
      }
    }
  }
}

void J1939Tp_InitRxChannel(PduIdType Channel) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[Channel];
  J1939Tp_RxResetToIdle(config);
}

void J1939Tp_InitTxChannel(PduIdType Channel) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[Channel];
  J1939Tp_TxResetToIdle(config);
}

Std_ReturnType J1939Tp_GetRxPgPGN(PduIdType Channel, uint32_t *PgPGN) {
  Std_ReturnType ret = E_OK;
  const J1939Tp_RxChannelType *config;

  DET_VALIDATE(NULL != J1939TP_CONFIG, 0xF0, J1939TP_E_UNINIT, return E_NOT_OK);

  if (Channel < J1939TP_CONFIG->numOfRxChannels) {
    config = &J1939TP_CONFIG->RxChannels[Channel];
    *PgPGN = config->context->PgPGN;
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType J1939Tp_SetTxPgPGN(PduIdType Channel, uint32_t PgPGN) {
  Std_ReturnType ret = E_OK;
  const J1939Tp_TxChannelType *config;

  DET_VALIDATE(NULL != J1939TP_CONFIG, 0xF1, J1939TP_E_UNINIT, return E_NOT_OK);

  if (Channel < J1939TP_CONFIG->numOfTxChannels) {
    config = &J1939TP_CONFIG->TxChannels[Channel];
    config->context->PgPGN = PgPGN;
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

void J1939Tp_Init(const J1939Tp_ConfigType *ConfigPtr) {
  uint16_t i;
#ifdef J1939TP_USE_PB_CONFIG
  if (NULL != ConfigPtr) {
    J1939TP_CONFIG = ConfigPtr;
  } else {
    J1939TP_CONFIG = &J1939Tp_Config;
  }
#else
  (void)ConfigPtr;
#endif
  for (i = 0u; i < J1939TP_CONFIG->numOfTxChannels; i++) {
    J1939Tp_InitTxChannel(i);
  }

  for (i = 0u; i < J1939TP_CONFIG->numOfRxChannels; i++) {
    J1939Tp_InitRxChannel(i);
  }
}

Std_ReturnType J1939Tp_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_OK;
  const J1939Tp_TxChannelType *config;

  DET_VALIDATE(NULL != J1939TP_CONFIG, 0x49, J1939TP_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(TxPduId < J1939TP_CONFIG->numOfTxChannels, 0x49, J1939TP_E_PARAM_POINTER,
               return E_NOT_OK);
  DET_VALIDATE(NULL != PduInfoPtr, 0x49, J1939TP_E_PARAM_POINTER, return E_NOT_OK);

  config = &J1939TP_CONFIG->TxChannels[TxPduId];
  if (J1939TP_IDLE == config->context->state) {
    config->context->TpSduLength = PduInfoPtr->SduLength;
    if (FALSE == IS_J1939TP_FD(config)) {
      if (NUM_OF_DT_PACKETS > 0xFFu) {
        ret = E_NOT_OK;
      }
    }
    if (E_OK == ret) {
      J1939Tp_StartToSend(TxPduId);
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

void J1939Tp_TxConfirmation(PduIdType TxPduId, Std_ReturnType result) {
  const J1939Tp_PduInfoType *PduInfo;

  DET_VALIDATE(NULL != J1939TP_CONFIG, 0x40, J1939TP_E_UNINIT, return);
  DET_VALIDATE(TxPduId < J1939TP_CONFIG->numOfTxPduInfos, 0x40, J1939TP_E_PARAM_POINTER, return);

  PduInfo = &(J1939TP_CONFIG->TxPduInfos[TxPduId]);
  if (J1939TP_RX_CHANNEL == PduInfo->ChannelType) {
    J1939Tp_TxConfirmation_RxChannel(PduInfo->RxPduInfo->RxChannel, result);
  } else {
    J1939Tp_TxConfirmation_TxChannel(PduInfo->TxPduInfo->TxChannel, result);
  }
}

void J1939Tp_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  const J1939Tp_PduInfoType *PduInfo;

  DET_VALIDATE(NULL != J1939TP_CONFIG, 0x42, J1939TP_E_UNINIT, return);
  DET_VALIDATE(RxPduId < J1939TP_CONFIG->numOfRxPduInfos, 0x42, J1939TP_E_PARAM_POINTER, return);
  DET_VALIDATE((NULL != PduInfoPtr) && (NULL != PduInfoPtr->SduDataPtr) &&
                 (PduInfoPtr->SduLength > 0),
               0x42, J1939TP_E_PARAM_POINTER, return);

  ASLOG(J1939TP, ("[%d]RX len=%d data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", RxPduId,
                  PduInfoPtr->SduLength, PduInfoPtr->SduDataPtr[0u], PduInfoPtr->SduDataPtr[1],
                  PduInfoPtr->SduDataPtr[2], PduInfoPtr->SduDataPtr[3], PduInfoPtr->SduDataPtr[4],
                  PduInfoPtr->SduDataPtr[5], PduInfoPtr->SduDataPtr[6], PduInfoPtr->SduDataPtr[7]));

  PduInfo = &(J1939TP_CONFIG->RxPduInfos[RxPduId]);
  if (J1939TP_RX_CHANNEL == PduInfo->ChannelType) {
    J1939Tp_RxIndication_RxChannel(PduInfo->RxPduInfo->RxChannel, PduInfo->RxPduInfo->PacketType,
                                   PduInfoPtr);
  } else {
    J1939Tp_RxIndication_TxChannel(PduInfo->TxPduInfo->TxChannel, PduInfo->TxPduInfo->PacketType,
                                   PduInfoPtr);
  }
}

void J1939Tp_MainFunction(void) {
  uint16_t i;

  DET_VALIDATE(NULL != J1939TP_CONFIG, 0x04, J1939TP_E_UNINIT, return);

  for (i = 0u; i < J1939TP_CONFIG->numOfTxChannels; i++) {
    J1939Tp_MainFunction_TxChannel(i);
  }

  for (i = 0u; i < J1939TP_CONFIG->numOfRxChannels; i++) {
    J1939Tp_MainFunction_RxChannel(i);
  }
}

void J1939Tp_MainFunctionFast(void) {
  uint16_t i;

  DET_VALIDATE(NULL != J1939TP_CONFIG, 0x04, J1939TP_E_UNINIT, return);

  for (i = 0u; i < J1939TP_CONFIG->numOfTxChannels; i++) {
    J1939Tp_MainFunction_TxChannelFast(i);
  }

  for (i = 0u; i < J1939TP_CONFIG->numOfRxChannels; i++) {
    J1939Tp_MainFunction_RxChannelFast(i);
  }
}

void J1939Tp_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x03, J1939TP_E_PARAM_POINTER, return);

  versionInfo->vendorID = STD_VENDOR_ID_AS;
  versionInfo->moduleID = MODULE_ID_J1939TP;
  versionInfo->sw_major_version = 4;
  versionInfo->sw_minor_version = 0;
  versionInfo->sw_patch_version = 1;
}
/** @brief release notes
 * - 4.0.1: Fix FD DT transmit SN check issue
 */
