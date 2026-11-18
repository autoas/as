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

#ifdef J1939TP_FIX_LINK_LAYER_DATA_LENGTH
#define J1939Tp_GetDL(len, LinkLayerDataLength) LinkLayerDataLength
#endif

#define J1939TpIsFd(config) (config->LinkLayerDataLength > 8u)

#define J1939TP_CM_RTS 16u
#define J1939TP_CM_CTS 17u
#define J1939TP_CM_EOMA 19u
#define J1939TP_CM_BAM 32u
#define J1939TP_CM_ABORT 0xFFu

#define DT_DATA_SIZE (config->LinkLayerDataLength - 1u)
#define J1939TpNumDtPackets(s) (((s) + DT_DATA_SIZE - 1u) / DT_DATA_SIZE)

#define J1939TP_NUM_OF_DT_PACKETS J1939TpNumDtPackets(config->Context->TpSduLength)

#define J1939TP_FD_CM_RTS ((config->S << 4) | 0u)
#define J1939TP_FD_CM_CTS ((config->S << 4) | 1u)
#define J1939TP_FD_CM_EOMS ((config->S << 4) | 2u)
#define J1939TP_FD_CM_EOMA ((config->S << 4) | 3u)
#define J1939TP_FD_CM_BAM ((config->S << 4) | 4u)
#define J1939TP_FD_CM_ABORT ((config->S << 4) | 15u)
#define J1939TP_FD_DTFI ((config->S << 4) | 0u)

#define FD_DT_DATA_SIZE (config->LinkLayerDataLength - 4u)
#define J1939TpNumFdDtPackets(s) (((s) + FD_DT_DATA_SIZE - 1u) / FD_DT_DATA_SIZE)
#define NUM_OF_FD_DT_PACKETS J1939TpNumFdDtPackets(config->Context->TpSduLength)

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
#ifndef J1939TP_FIX_LINK_LAYER_DATA_LENGTH
static const PduLengthType lLinkLayerDataLengths[] = {8u, 12u, 16u, 20u, 24u, 32u, 48u};
#endif
#ifdef J1939TP_USE_PB_CONFIG
static const J1939Tp_ConfigType *j1939tpConfig = NULL;
#endif
/* ================================ [ LOCALS    ] ============================================== */
#ifndef J1939TP_FIX_LINK_LAYER_DATA_LENGTH
static PduLengthType J1939Tp_GetDL(PduLengthType len, PduLengthType linkLayerDataLength) {
  PduLengthType dl = linkLayerDataLength;
  PduLengthType i;

  for (i = 0u; i < ARRAY_SIZE(lLinkLayerDataLengths); i++) {
    if (len <= lLinkLayerDataLengths[i]) {
      dl = lLinkLayerDataLengths[i];
      break;
    }
  }
  return dl;
}
#endif

static void J1939Tp_TxResetToIdle(const J1939Tp_TxChannelType *config) {
  (void)memset(config->Context, 0u, sizeof(J1939Tp_TxChannelContextType));
  config->Context->PgPGN = config->TxPgPGN;
}

static void J1939Tp_RxResetToIdle(const J1939Tp_RxChannelType *config) {
  uint32_t PgPGN = config->Context->PgPGN;
  (void)memset(config->Context, 0u, sizeof(J1939Tp_RxChannelContextType));
  config->Context->PgPGN = PgPGN;
}

static void J1939Tp_SendDirectPG(PduIdType txPduId) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[txPduId];
  PduInfoType PduInfo;
  uint8_t *data;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
  PduLengthType pos = 0u;
  Std_ReturnType ret;

  data = config->Data;
  PduInfo.MetaDataPtr = (uint8_t *)&pos;
  PduInfo.SduDataPtr = data;
  PduInfo.SduLength = config->Context->TpSduLength;

  bufReq = PduR_J1939TpCopyTxData(config->PduR_TxPduId, &PduInfo, NULL, &bufferSize);
  if (BUFREQ_OK == bufReq) {
    ASLOG(J1939TPI, ("[%d]TX Direct=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", txPduId, data[0u],
                     data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
    if (PduInfo.SduLength > 8u) {
      pos = PduInfo.SduLength;
      PduInfo.SduLength = J1939Tp_GetDL(pos, config->LinkLayerDataLength);
      while (pos < PduInfo.SduLength) {
        data[pos] = config->Padding;
        pos++;
      }
    }

    config->Context->PduInfo.SduDataPtr = data;
    config->Context->PduInfo.SduLength = PduInfo.SduLength;

    ret = CanIf_Transmit(config->CanIf_TxDirectNPdu, &config->Context->PduInfo);
    if (E_OK == ret) {
      config->Context->State = J1939TP_WAIT_DIRECT_PG_TX_COMPLETED;
      config->Context->Timer = config->Tr;
    } else {
      config->Context->State = J1939TP_RESEND_DIRECT_PG;
      config->Context->Timer = config->Tr;
    }
  } else {
    ASLOG(J1939TPE, ("[%d]DirectPG: failed to provide TX data, reset to idle\n", txPduId));
    J1939Tp_TxResetToIdle(config);
    PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
  }
}

static void J1939Tp_StartToSend_CMDT(PduIdType txPduId) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[txPduId];
  uint8_t *data;
  Std_ReturnType ret;

  data = config->Data;

  config->Context->PduInfo.SduDataPtr = data;

  if (J1939TpIsFd(config)) {
    data[0u] = J1939TP_FD_CM_RTS;

    data[1] = config->Context->TpSduLength & 0xFFu;
    data[2] = (config->Context->TpSduLength >> 8) & 0xFFu;
    data[3] = (config->Context->TpSduLength >> 16) & 0xFFu;

    data[4] = NUM_OF_FD_DT_PACKETS & 0xFFu;
    data[5] = (NUM_OF_FD_DT_PACKETS >> 8) & 0xFFu;
    data[6] = (NUM_OF_FD_DT_PACKETS >> 16) & 0xFFu;

    data[7] = config->TxMaxPacketsPerBlock;

    data[8] = config->ADType;

    data[9] = config->Context->PgPGN & 0xFFu;
    data[10] = (config->Context->PgPGN >> 8) & 0xFFu;
    data[11] = (config->Context->PgPGN >> 16) & 0xFFu;
    config->Context->PduInfo.SduLength = 12u;

    ASLOG(J1939TPI,
          ("[%d]TX FD CM.RTS=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n",
           txPduId, data[0u], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
           data[8], data[9], data[10], data[11]));
  } else {
    /* CM_RTS message */
    data[0u] = J1939TP_CM_RTS;

    /* Message size in bytes, low byte at the begin. */
    data[1] = config->Context->TpSduLength & 0xFFu;
    data[2] = (config->Context->TpSduLength >> 8) & 0xFFu;

    data[3] = J1939TP_NUM_OF_DT_PACKETS; /* Number of DT packets */

    data[4] = config->TxMaxPacketsPerBlock;
    data[5] = config->Context->PgPGN & 0xFFu;
    data[6] = (config->Context->PgPGN >> 8) & 0xFFu;
    data[7] = (config->Context->PgPGN >> 16) & 0xFFu;
    config->Context->PduInfo.SduLength = 8;

    ASLOG(J1939TPI, ("[%d]TX CM.RTS=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", txPduId, data[0u],
                     data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
  }

  ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->Context->PduInfo);
  if (E_OK == ret) {
    config->Context->State = J1939TP_WAIT_CM_RTS_TX_COMPLETED;
    config->Context->Timer = config->Tr;
  } else {
    config->Context->State = J1939TP_RESEND_CM_RTS;
    config->Context->Timer = config->Tr;
  }
}

static void J1939Tp_SendDT(PduIdType txPduId) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[txPduId];
  PduInfoType PduInfo;
  uint8_t *data;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
  PduLengthType linkLayerDataLength;
  PduLengthType pos;
  PduLengthType offset = 0u;
  Std_ReturnType ret;

  if (J1939TpIsFd(config)) {
    data = config->Data;
    data[0u] = J1939TP_FD_DTFI;
    data[1] = (config->Context->NextSN + 1u) & 0xFFu;
    data[2] = ((config->Context->NextSN + 1u) >> 8) & 0xFFu;
    data[3] = ((config->Context->NextSN + 1u) >> 16) & 0xFFu;
    pos = 4;

    offset = config->Context->NextSN * FD_DT_DATA_SIZE;

    bufferSize = config->Context->TpSduLength - offset;
    if (bufferSize > (config->LinkLayerDataLength - 4u)) {
      bufferSize = config->LinkLayerDataLength - 4u;
    }
  } else {
    data = config->Data;
    data[0u] = config->Context->NextSN + 1u;
    pos = 1;

    offset = config->Context->NextSN * DT_DATA_SIZE;

    bufferSize = config->Context->TpSduLength - offset;
    if (bufferSize > (config->LinkLayerDataLength - 1u)) {
      bufferSize = config->LinkLayerDataLength - 1u;
    }
  }

  PduInfo.MetaDataPtr = (uint8_t *)&offset;
  PduInfo.SduDataPtr = &data[pos];
  PduInfo.SduLength = bufferSize;

  bufReq = PduR_J1939TpCopyTxData(config->PduR_TxPduId, &PduInfo, NULL, &bufferSize);
  if (BUFREQ_OK == bufReq) {
    pos += PduInfo.SduLength;
    linkLayerDataLength = J1939Tp_GetDL(pos, config->LinkLayerDataLength);
    while (pos < linkLayerDataLength) {
      data[pos] = config->Padding;
      pos++;
    }
    ASLOG(J1939TPI, ("[%d]TX DT=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", txPduId, data[0u],
                     data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
    config->Context->PduInfo.SduDataPtr = data;
    config->Context->PduInfo.SduLength = linkLayerDataLength;

    if ((J1939TP_WAIT_CM_CTS_RX == config->Context->State) || (0u == config->STMin)) {
      ret = CanIf_Transmit(config->CanIf_TxDtNPdu, &config->Context->PduInfo);
      if (E_OK == ret) {
        config->Context->State = J1939TP_WAIT_DT_TX_COMPLETED;
        config->Context->Timer = config->Tr;
      } else {
        config->Context->State = J1939TP_RESEND_DT;
        config->Context->Timer = config->Tr;
      }
    } else {
      config->Context->State = J1939TP_WAIT_DT_STMIN_TIMEOUT;
      config->Context->Timer = config->STMin;
    }
  } else {
    ASLOG(J1939TPE, ("[%d]DT: failed to provide TX data, reset to idle\n", txPduId));
    J1939Tp_TxResetToIdle(config);
    PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
  }
}

static void J1939Tp_SendEOMS(PduIdType txPduId) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[txPduId];
  uint8_t *data;
  Std_ReturnType ret;

  data = config->Data;

  config->Context->PduInfo.SduDataPtr = data;

  data[0u] = J1939TP_FD_CM_EOMS;

  data[1] = config->Context->TpSduLength & 0xFFu;
  data[2] = (config->Context->TpSduLength >> 8) & 0xFFu;
  data[3] = (config->Context->TpSduLength >> 16) & 0xFFu;

  data[4] = NUM_OF_FD_DT_PACKETS & 0xFFu;
  data[5] = (NUM_OF_FD_DT_PACKETS >> 8) & 0xFFu;
  data[6] = (NUM_OF_FD_DT_PACKETS >> 16) & 0xFFu;

  data[7] = 0u; /* TODO: AD Size */

  data[8] = config->ADType;

  data[9] = config->Context->PgPGN & 0xFFu;
  data[10] = (config->Context->PgPGN >> 8) & 0xFFu;
  data[11] = (config->Context->PgPGN >> 16) & 0xFFu;
  /* TODO: append assurance data: CRC32 for example */
  config->Context->PduInfo.SduLength = 12u;

  ASLOG(J1939TPI, ("[%d]TX EOMS=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", txPduId, data[0u],
                   data[1], data[2], data[3], data[4], data[5], data[6], data[7]));

  ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->Context->PduInfo);
  if (E_OK == ret) {
    config->Context->State = J1939TP_WAIT_CM_EOMS_TX_COMPLETED;
    config->Context->Timer = config->Tr;
  } else {
    config->Context->State = J1939TP_RESEND_CM_EOMS;
    config->Context->Timer = config->Tr;
  }
}

static void J1939Tp_StartToSend_BAM(PduIdType txPduId) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[txPduId];
  uint8_t *data;
  Std_ReturnType ret;

  data = config->Data;

  config->Context->PduInfo.SduDataPtr = data;

  if (J1939TpIsFd(config)) {
    data[0u] = J1939TP_FD_CM_BAM;

    data[1] = config->Context->TpSduLength & 0xFFu;
    data[2] = (config->Context->TpSduLength >> 8) & 0xFFu;
    data[3] = (config->Context->TpSduLength >> 16) & 0xFFu;

    data[4] = NUM_OF_FD_DT_PACKETS & 0xFFu;
    data[5] = (NUM_OF_FD_DT_PACKETS >> 8) & 0xFFu;
    data[6] = (NUM_OF_FD_DT_PACKETS >> 16) & 0xFFu;

    data[7] = 0xFFu;

    data[8] = config->ADType;

    data[9] = config->Context->PgPGN & 0xFFu;
    data[10] = (config->Context->PgPGN >> 8) & 0xFFu;
    data[11] = (config->Context->PgPGN >> 16) & 0xFFu;
    config->Context->PduInfo.SduLength = 12u;

    ASLOG(J1939TPI,
          ("[%d]TX FD CM.BAM=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n",
           txPduId, data[0u], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
           data[8], data[9], data[10], data[11]));
  } else {
    /* CM_BAM message */
    data[0u] = J1939TP_CM_BAM;

    /* Message size in bytes, low byte at the begin. */
    data[1] = config->Context->TpSduLength & 0xFFu;
    data[2] = (config->Context->TpSduLength >> 8) & 0xFFu;

    data[3] = J1939TP_NUM_OF_DT_PACKETS; /* Number of DT packets */

    data[4] = 0xFFu;
    data[5] = config->Context->PgPGN & 0xFFu;
    data[6] = (config->Context->PgPGN >> 8) & 0xFFu;
    data[7] = (config->Context->PgPGN >> 16) & 0xFFu;
    config->Context->PduInfo.SduLength = 8;

    ASLOG(J1939TPI, ("[%d]TX CM.BAM=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", txPduId, data[0u],
                     data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
  }

  ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->Context->PduInfo);
  if (E_OK == ret) {
    config->Context->State = J1939TP_WAIT_CM_BAM_TX_COMPLETED;
    config->Context->Timer = config->Tr;
  } else {
    config->Context->State = J1939TP_RESEND_CM_BAM;
    config->Context->Timer = config->Tr;
  }
}

static void J1939Tp_StartToSend(PduIdType txPduId) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[txPduId];

  if (config->LinkLayerDataLength >= config->Context->TpSduLength) {
    J1939Tp_SendDirectPG(txPduId);
  } else if (J1939TP_PROTOCOL_CMDT == config->TxProtocol) {
    J1939Tp_StartToSend_CMDT(txPduId);
  } else { /* BAM */
    J1939Tp_StartToSend_BAM(txPduId);
  }
}

static void J1939Tp_TxSendCmAbort(PduIdType channel, uint8_t abortReason) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[channel];
  uint8_t *data;
  Std_ReturnType ret;

  data = config->Data;
  config->Context->PduInfo.SduDataPtr = data;

  if (J1939TpIsFd(config)) {
    data[0u] = J1939TP_FD_CM_ABORT;
    /* Reserved */
    data[1] = 0xFFu;
    data[2] = 0xFFu;
    data[3] = 0xFFu;
    data[4] = 0xFFu;
    data[5] = 0xFFu;
    data[6] = 0xFFu;
    data[7] = 0xFFu;
    data[8] = abortReason;
    data[9] = config->Context->PgPGN & 0xFFu;
    data[10] = (config->Context->PgPGN >> 8) & 0xFFu;
    data[11] = (config->Context->PgPGN >> 16) & 0xFFu;
    config->Context->PduInfo.SduLength = 12u;
  } else {
    /* CM Connection abort message */
    data[0u] = J1939TP_CM_ABORT;
    /* abort reason */
    data[1] = abortReason;
    /* Reserved */
    data[2] = 0xFFu;
    data[3] = 0xFFu;
    data[4] = 0xFFu;
    data[5] = config->Context->PgPGN & 0xFFu;
    data[6] = (config->Context->PgPGN >> 8) & 0xFFu;
    data[7] = (config->Context->PgPGN >> 16) & 0xFFu;
    config->Context->PduInfo.SduLength = 8;
  }

  ASLOG(J1939TPI, ("[%d]TX CM.ABORT=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", channel, data[0u],
                   data[1], data[2], data[3], data[4], data[5], data[6], data[7]));

  ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->Context->PduInfo);
  if (E_OK == ret) {
    config->Context->State = J1939TP_WAIT_CM_ABORT_TX_COMPLETED;
    config->Context->Timer = config->Tr;
  } else {
    config->Context->State = J1939TP_RESEND_CM_ABORT;
    config->Context->Timer = config->Tr;
  }
}

static void J1939Tp_RxSendCmAbort(PduIdType channel, uint8_t abortReason) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[channel];
  uint8_t *data;
  Std_ReturnType ret;

  if (J1939TP_PROTOCOL_CMDT == config->RxProtocol) {
    data = config->Data;
    config->Context->PduInfo.SduDataPtr = data;

    if (J1939TpIsFd(config)) {
      data[0u] = J1939TP_FD_CM_ABORT;
      /* Reserved */
      data[1] = 0xFFu;
      data[2] = 0xFFu;
      data[3] = 0xFFu;
      data[4] = 0xFFu;
      data[5] = 0xFFu;
      data[6] = 0xFFu;
      data[7] = 0xFFu;
      data[8] = abortReason;
      data[9] = config->Context->PgPGN & 0xFFu;
      data[10] = (config->Context->PgPGN >> 8) & 0xFFu;
      data[11] = (config->Context->PgPGN >> 16) & 0xFFu;
      config->Context->PduInfo.SduLength = 12u;
    } else {
      /* CM Connection abort message */
      data[0u] = J1939TP_CM_ABORT;
      /* abort reason */
      data[1] = abortReason;
      /* Reserved */
      data[2] = 0xFFu;
      data[3] = 0xFFu;
      data[4] = 0xFFu;
      data[5] = config->Context->PgPGN & 0xFFu;
      data[6] = (config->Context->PgPGN >> 8) & 0xFFu;
      data[7] = (config->Context->PgPGN >> 16) & 0xFFu;
      config->Context->PduInfo.SduLength = 8;
    }

    ASLOG(J1939TPI, ("[%d]RX -> TX CM.ABORT=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", channel,
                     data[0u], data[1], data[2], data[3], data[4], data[5], data[6], data[7]));

    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->Context->PduInfo);
    if (E_OK == ret) {
      config->Context->State = J1939TP_WAIT_CM_ABORT_TX_COMPLETED;
      config->Context->Timer = config->Tr;
    } else {
      config->Context->State = J1939TP_RESEND_CM_ABORT;
      config->Context->Timer = config->Tr;
    }
  } else {
    J1939Tp_RxResetToIdle(config);
    PduR_J1939TpRxIndication(config->PduR_RxPduId, E_NOT_OK);
  }
}

static void J1939Tp_RxSendCmCTS(PduIdType channel) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[channel];
  uint8_t *data;
  Std_ReturnType ret;
  uint32_t nextSN;
  uint8_t numOfPackets;

  if (J1939TpIsFd(config)) {
    numOfPackets = NUM_OF_FD_DT_PACKETS - config->Context->NextSN;
  } else {
    numOfPackets = J1939TP_NUM_OF_DT_PACKETS - config->Context->NextSN;
  }

  if (numOfPackets > config->RxPacketsPerBlock) {
    numOfPackets = config->RxPacketsPerBlock;
  }

  if (numOfPackets > config->Context->MaxPacketsPerBlock) {
    numOfPackets = config->Context->MaxPacketsPerBlock;
  }

  config->Context->NumPacketsThisBlock = numOfPackets;

  if (J1939TP_PROTOCOL_CMDT == config->RxProtocol) {
    data = config->Data;
    config->Context->PduInfo.SduDataPtr = data;
    nextSN = config->Context->NextSN + 1u;

    if (J1939TpIsFd(config)) {
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
      data[9] = config->Context->PgPGN & 0xFFu;
      data[10] = (config->Context->PgPGN >> 8) & 0xFFu;
      data[11] = (config->Context->PgPGN >> 16) & 0xFFu;
      config->Context->PduInfo.SduLength = 12u;
    } else {
      data[0u] = J1939TP_CM_CTS;
      data[1] = numOfPackets;
      data[2] = nextSN & 0xFFu;
      data[3] = 0xFFu;
      data[4] = 0xFFu;
      data[5] = config->Context->PgPGN & 0xFFu;
      data[6] = (config->Context->PgPGN >> 8) & 0xFFu;
      data[7] = (config->Context->PgPGN >> 16) & 0xFFu;
      config->Context->PduInfo.SduLength = 8;
    }

    ASLOG(J1939TPI, ("[%d]RX -> TX CM.CTS=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", channel,
                     data[0u], data[1], data[2], data[3], data[4], data[5], data[6], data[7]));

    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->Context->PduInfo);
    if (E_OK == ret) {
      config->Context->State = J1939TP_WAIT_CM_CTS_TX_COMPLETED;
      config->Context->Timer = config->Tr;
    } else {
      config->Context->State = J1939TP_RESEND_CM_CTS;
      config->Context->Timer = config->Tr;
    }
  } else {
    config->Context->State = J1939TP_WAIT_DT_RX;
    config->Context->Timer = config->T1;
  }
}

static void J1939Tp_RxSendCmEOMA(PduIdType channel) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[channel];
  uint8_t *data;
  Std_ReturnType ret;

  if (J1939TP_PROTOCOL_CMDT == config->RxProtocol) {
    data = config->Data;
    config->Context->PduInfo.SduDataPtr = data;

    if (J1939TpIsFd(config)) {
      data[0u] = J1939TP_FD_CM_EOMA;
      data[1] = config->Context->TpSduLength & 0xFFu;
      data[2] = (config->Context->TpSduLength >> 8) & 0xFFu;
      data[3] = (config->Context->TpSduLength >> 16) & 0xFFu;

      data[4] = NUM_OF_FD_DT_PACKETS & 0xFFu;
      data[5] = (NUM_OF_FD_DT_PACKETS >> 8) & 0xFFu;
      data[6] = (NUM_OF_FD_DT_PACKETS >> 16) & 0xFFu;
      data[7] = 0xFFu;
      data[8] = 0xFFu;
      data[9] = config->Context->PgPGN & 0xFFu;
      data[10] = (config->Context->PgPGN >> 8) & 0xFFu;
      data[11] = (config->Context->PgPGN >> 16) & 0xFFu;
      config->Context->PduInfo.SduLength = 12u;
    } else {
      data[0u] = J1939TP_CM_EOMA;
      data[1] = config->Context->TpSduLength & 0xFFu;
      data[2] = (config->Context->TpSduLength >> 8) & 0xFFu;
      data[3] = J1939TP_NUM_OF_DT_PACKETS;
      data[4] = 0xFFu;
      data[5] = config->Context->PgPGN & 0xFFu;
      data[6] = (config->Context->PgPGN >> 8) & 0xFFu;
      data[7] = (config->Context->PgPGN >> 16) & 0xFFu;
      config->Context->PduInfo.SduLength = 8;
    }

    ASLOG(J1939TPI, ("[%d]RX -> TX CM.EOMA=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", channel,
                     data[0u], data[1], data[2], data[3], data[4], data[5], data[6], data[7]));

    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->Context->PduInfo);
    if (E_OK == ret) {
      config->Context->State = J1939TP_WAIT_CM_EOMA_TX_COMPLETED;
      config->Context->Timer = config->Tr;
    } else {
      config->Context->State = J1939TP_RESEND_CM_EOMS;
      config->Context->Timer = config->Tr;
    }
  } else {
    J1939Tp_RxResetToIdle(config);
  }
}

static void J1939Tp_HandleDT_TxConfirmation_TxChannel(PduIdType channel, Std_ReturnType result) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[channel];
  if (E_OK == result) {
    if ((J1939TpIsFd(config) && ((config->Context->NextSN + 1u) < NUM_OF_FD_DT_PACKETS)) ||
        ((config->Context->NextSN + 1u) < J1939TP_NUM_OF_DT_PACKETS)) {
      config->Context->NextSN++;
      if (J1939TP_PROTOCOL_CMDT == config->TxProtocol) {
        if (config->Context->NumPacketsToSend > 0u) {
          config->Context->NumPacketsToSend--;
        }
        if (0u == config->Context->NumPacketsToSend) {
          config->Context->State = J1939TP_WAIT_CM_CTS_RX;
          config->Context->Timer = config->T3;
        } else {
          J1939Tp_SendDT(channel);
        }
      } else {
        J1939Tp_SendDT(channel);
      }
    } else { /* reach the end */
      if (J1939TpIsFd(config)) {
        J1939Tp_SendEOMS(channel);
      } else {
        if (J1939TP_PROTOCOL_CMDT == config->TxProtocol) {
          config->Context->State = J1939TP_WAIT_END_OF_MSG_ACK_RX;
          config->Context->Timer = config->T3;
        } else {
          J1939Tp_TxResetToIdle(config);
          PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_OK);
        }
      }
    }
  } else {
    J1939Tp_TxResetToIdle(config);
    PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
    ASLOG(J1939TPE, ("[%d] Fatal Error, send DT failed\n", channel));
    /* as maybe network issue, bus off, etc, so not send Abort message */
  }
}

static void J1939Tp_TxConfirmation_TxChannel(PduIdType channel, Std_ReturnType result) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[channel];
  switch (config->Context->State) {
  case J1939TP_WAIT_DIRECT_PG_TX_COMPLETED:
    J1939Tp_TxResetToIdle(config);
    PduR_J1939TpTxConfirmation(config->PduR_TxPduId, result);
    break;
  case J1939TP_WAIT_CM_RTS_TX_COMPLETED:
    if (E_OK != result) {
      J1939Tp_TxResetToIdle(config);
      PduR_J1939TpTxConfirmation(config->PduR_TxPduId, result);
    } else {
      config->Context->State = J1939TP_WAIT_CM_CTS_RX;
      config->Context->Timer = config->T3;
    }
    break;
  case J1939TP_WAIT_CM_BAM_TX_COMPLETED:
    if (E_OK != result) {
      J1939Tp_TxResetToIdle(config);
      PduR_J1939TpTxConfirmation(config->PduR_TxPduId, result);
    } else {
      config->Context->NumPacketsToSend = 0xFFu;
      J1939Tp_SendDT(channel);
    }
    break;
  case J1939TP_WAIT_CM_ABORT_TX_COMPLETED:
    J1939Tp_TxResetToIdle(config);
    PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
    break;
  case J1939TP_WAIT_DT_TX_COMPLETED:
    J1939Tp_HandleDT_TxConfirmation_TxChannel(channel, result);
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
        config->Context->State = J1939TP_WAIT_END_OF_MSG_ACK_RX;
        config->Context->Timer = config->T3;
      }
    }
    break;
  default:
    ASLOG(J1939TPE,
          ("[%d] TX TxConfirmation when in state %d, abort\n", channel, config->Context->State));
    J1939Tp_TxResetToIdle(config);
    break;
  }
}

static void J1939Tp_HandleCM_CTS(PduIdType channel, const PduInfoType *pduInfoPtr) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[channel];
  uint8_t NumPacketsToSend;
  uint32_t NextSN;
  uint32_t PgPGN;
  uint8_t RequestField = 0u;
  uint32_t numOfDtPackets = 0;
  Std_ReturnType ret = E_NOT_OK;
  if (J1939TpIsFd(config)) {
    if ((J1939TP_FD_CM_CTS == pduInfoPtr->SduDataPtr[0u]) &&
        (12u == pduInfoPtr->SduLength)) { /* clear to send */
      NextSN = pduInfoPtr->SduDataPtr[4] + ((uint32_t)pduInfoPtr->SduDataPtr[5] << 8) +
               ((uint32_t)pduInfoPtr->SduDataPtr[6] << 16);
      NumPacketsToSend = pduInfoPtr->SduDataPtr[7];
      RequestField = pduInfoPtr->SduDataPtr[9];
      PgPGN = pduInfoPtr->SduDataPtr[9] + ((uint32_t)pduInfoPtr->SduDataPtr[10] << 8) +
              ((uint32_t)pduInfoPtr->SduDataPtr[11] << 16);
      numOfDtPackets = J1939TP_NUM_OF_DT_PACKETS;
      ret = E_OK;
    }
  } else {
    if ((J1939TP_CM_CTS == pduInfoPtr->SduDataPtr[0u]) &&
        ((8u == pduInfoPtr->SduLength))) { /* clear to send */
      NumPacketsToSend = pduInfoPtr->SduDataPtr[1];
      NextSN = pduInfoPtr->SduDataPtr[2];
      PgPGN = pduInfoPtr->SduDataPtr[5] + ((uint32_t)pduInfoPtr->SduDataPtr[6] << 8) +
              ((uint32_t)pduInfoPtr->SduDataPtr[7] << 16);
      numOfDtPackets = NUM_OF_FD_DT_PACKETS;
      ret = E_OK;
    }
  }
  if (E_OK == ret) {
    if (PgPGN == config->Context->PgPGN) {
      if (NumPacketsToSend == 0u) {
        /* Receiver wants to keep the connection open but can't receive packets */
        config->Context->Timer = config->T4;
      } else if ((1u == RequestField) && (0xFFFFFFu == NextSN)) {
        /* Request the FD.TP.CM_EndOfMsgStatus to be resent */
        J1939Tp_SendEOMS(channel);
      } else if ((NextSN == (config->Context->NextSN + 1u)) &&
                 (((uint32_t)config->Context->NextSN + NumPacketsToSend) <= numOfDtPackets)) {
        config->Context->NumPacketsToSend = NumPacketsToSend;
        J1939Tp_SendDT(channel);
      } else {
        PduR_J1939TpTxConfirmation(config->PduR_TxPduId, E_NOT_OK);
        J1939Tp_TxSendCmAbort(channel, J1939TP_ABORT_REASON_BAD_SEQUENCE_NUMBER);
      }
    }
  } else {
    ASLOG(J1939TPE,
          ("[%d] TX Get Packet CM %02X len=%d when in state %d, ignore\n", channel,
           pduInfoPtr->SduDataPtr[0u], (int)pduInfoPtr->SduLength, config->Context->State));
  }
}

static void J1939Tp_HandleCM_EOMA(PduIdType channel, const PduInfoType *pduInfoPtr) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[channel];
  uint32_t totalMsgSize;
  uint32_t totalNumPackets;
  uint32_t PgPGN;
  if (J1939TpIsFd(config)) {
    if ((J1939TP_FD_CM_EOMA == pduInfoPtr->SduDataPtr[0u]) && (12u <= pduInfoPtr->SduLength)) {
      PgPGN = pduInfoPtr->SduDataPtr[9] + ((uint32_t)pduInfoPtr->SduDataPtr[10] << 8) +
              ((uint32_t)pduInfoPtr->SduDataPtr[11] << 16);

      if (PgPGN == config->Context->PgPGN) {
        totalMsgSize = pduInfoPtr->SduDataPtr[1] + ((uint32_t)pduInfoPtr->SduDataPtr[2] << 8) +
                       ((uint32_t)pduInfoPtr->SduDataPtr[3] << 16);
        totalNumPackets = pduInfoPtr->SduDataPtr[4] + ((uint32_t)pduInfoPtr->SduDataPtr[5] << 8) +
                          ((uint32_t)pduInfoPtr->SduDataPtr[6] << 16);
        if ((totalMsgSize == config->Context->TpSduLength) &&
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
    if ((J1939TP_CM_EOMA == pduInfoPtr->SduDataPtr[0u]) && (8u == pduInfoPtr->SduLength)) {
      PgPGN = pduInfoPtr->SduDataPtr[5] + ((uint32_t)pduInfoPtr->SduDataPtr[6] << 8) +
              ((uint32_t)pduInfoPtr->SduDataPtr[7] << 16);
      if (PgPGN == config->Context->PgPGN) {
        totalMsgSize = pduInfoPtr->SduDataPtr[1] + ((uint32_t)pduInfoPtr->SduDataPtr[2] << 8);
        totalNumPackets = pduInfoPtr->SduDataPtr[3];
        if ((totalMsgSize == config->Context->TpSduLength) &&
            (totalNumPackets == J1939TP_NUM_OF_DT_PACKETS)) {
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

static void J1939Tp_HandleCM(PduIdType channel, const PduInfoType *pduInfoPtr) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[channel];
  if (J1939TP_WAIT_CM_CTS_RX == config->Context->State) {
    J1939Tp_HandleCM_CTS(channel, pduInfoPtr);
  } else if (J1939TP_WAIT_END_OF_MSG_ACK_RX == config->Context->State) {
    J1939Tp_HandleCM_EOMA(channel, pduInfoPtr);
  } else {
    ASLOG(J1939TPE,
          ("[%d] TX Get Packet CM when in state %d, ignore\n", channel, config->Context->State));
  }
}

static void J1939Tp_RxIndication_TxChannel(PduIdType channel, J1939Tp_PacketType packetType,
                                           const PduInfoType *pduInfoPtr) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[channel];

  switch (packetType) {
  case J1939TP_PACKET_CM:
    J1939Tp_HandleCM(channel, pduInfoPtr);
    break;
  default:
    ASLOG(J1939TPE, ("[%d] TX Get Packet %d when in state %d, ignore\n", channel, packetType,
                     config->Context->State));
    break;
  }
}

static void J1939Tp_TxConfirmation_RxChannel(PduIdType channel, Std_ReturnType result) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[channel];

  if (E_OK == result) {
    switch (config->Context->State) {
    case J1939TP_WAIT_CM_ABORT_TX_COMPLETED:
      J1939Tp_RxResetToIdle(config);
      PduR_J1939TpTxConfirmation(config->PduR_RxPduId, E_NOT_OK);
      break;
    case J1939TP_WAIT_CM_CTS_TX_COMPLETED:
      config->Context->State = J1939TP_WAIT_DT_RX;
      config->Context->Timer = config->T2;
      break;
    case J1939TP_WAIT_CM_EOMA_TX_COMPLETED:
      J1939Tp_RxResetToIdle(config);
      break;
    default:
      ASLOG(J1939TPE,
            ("[%d] RX TxConfirmation when in state %d, abort\n", channel, config->Context->State));
      J1939Tp_RxResetToIdle(config);
      break;
    }
  }
}

static void J1939Tp_RxIndication_RxChannel_CM_RTS(PduIdType channel,
                                                  const PduInfoType *pduInfoPtr) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[channel];
  Std_ReturnType ret = E_NOT_OK;
  uint32_t totalMsgSize;
  uint32_t totalNumPackets;
  uint32_t PgPGN;
  uint8_t maxPacketsPerBlock;
  uint8_t ADType = 0u;
  PduLengthType bufferSize;
  BufReq_ReturnType bufReq;

  if (J1939TpIsFd(config) && (J1939TP_IDLE == config->Context->State)) {
    if (12u == pduInfoPtr->SduLength) {
      PgPGN = pduInfoPtr->SduDataPtr[9] + ((uint32_t)pduInfoPtr->SduDataPtr[10] << 8) +
              ((uint32_t)pduInfoPtr->SduDataPtr[11] << 16);
      totalMsgSize = pduInfoPtr->SduDataPtr[1] + ((uint32_t)pduInfoPtr->SduDataPtr[2] << 8) +
                     ((uint32_t)pduInfoPtr->SduDataPtr[3] << 16);
      totalNumPackets = pduInfoPtr->SduDataPtr[4] + ((uint32_t)pduInfoPtr->SduDataPtr[5] << 8) +
                        ((uint32_t)pduInfoPtr->SduDataPtr[6] << 16);
      maxPacketsPerBlock = pduInfoPtr->SduDataPtr[7];
      ADType = pduInfoPtr->SduDataPtr[8];
      if ((J1939TpNumFdDtPackets(totalMsgSize) == totalNumPackets) && (0u != maxPacketsPerBlock)) {
        if (0u == ADType) {
          ret = E_OK;
        } else {
          /* TODO */
          ASLOG(J1939TPE, ("[%d]RX Get FD CM RTS with assurance is not supported\n", channel));
        }
      }
    } else {
      ASLOG(J1939TPE, ("[%d]RX Get FD CM RTS with invalid length\n", channel));
    }
  } else if (J1939TP_IDLE == config->Context->State) {
    PgPGN = pduInfoPtr->SduDataPtr[5] + ((uint32_t)pduInfoPtr->SduDataPtr[6] << 8) +
            ((uint32_t)pduInfoPtr->SduDataPtr[7] << 16);
    totalMsgSize = pduInfoPtr->SduDataPtr[1] + ((uint32_t)pduInfoPtr->SduDataPtr[2] << 8);
    totalNumPackets = pduInfoPtr->SduDataPtr[3];
    maxPacketsPerBlock = pduInfoPtr->SduDataPtr[4];
    if ((J1939TpNumDtPackets(totalMsgSize) == totalNumPackets) && (0u != maxPacketsPerBlock)) {
      ret = E_OK;
    }
  } else {
    ASLOG(J1939TPE, ("[%d]RX Get CM RTS %02X when in state %d, ignore\n", channel,
                     pduInfoPtr->SduDataPtr[0u], config->Context->State));
  }

  if (E_OK == ret) {
    config->Context->PgPGN = PgPGN;
    bufReq = PduR_J1939TpStartOfReception(config->PduR_RxPduId, NULL, totalMsgSize, &bufferSize);
    if (BUFREQ_OK == bufReq) {
      config->Context->NextSN = 0u;
      config->Context->TpSduLength = totalMsgSize;
      config->Context->MaxPacketsPerBlock = maxPacketsPerBlock;
      J1939Tp_RxSendCmCTS(channel);
    } else {
      J1939Tp_RxSendCmAbort(channel, J1939TP_ABORT_REASON_MESSAGE_TOO_BIG);
    }
  }
}

static void J1939Tp_RxIndication_RxChannel_CM_BAM(PduIdType channel,
                                                  const PduInfoType *pduInfoPtr) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[channel];
  Std_ReturnType ret = E_NOT_OK;
  uint32_t totalMsgSize;
  uint32_t totalNumPackets;
  uint32_t PgPGN;
  uint8_t ADType = 0u;
  PduLengthType bufferSize;
  BufReq_ReturnType bufReq;

  if (J1939TpIsFd(config) && (J1939TP_IDLE == config->Context->State)) {
    if (12u == pduInfoPtr->SduLength) {
      PgPGN = pduInfoPtr->SduDataPtr[9] + ((uint32_t)pduInfoPtr->SduDataPtr[10] << 8) +
              ((uint32_t)pduInfoPtr->SduDataPtr[11] << 16);
      totalMsgSize = pduInfoPtr->SduDataPtr[1] + ((uint32_t)pduInfoPtr->SduDataPtr[2] << 8) +
                     ((uint32_t)pduInfoPtr->SduDataPtr[3] << 16);
      totalNumPackets = pduInfoPtr->SduDataPtr[4] + ((uint32_t)pduInfoPtr->SduDataPtr[5] << 8) +
                        ((uint32_t)pduInfoPtr->SduDataPtr[6] << 16);
      ADType = pduInfoPtr->SduDataPtr[8];
      if (J1939TpNumFdDtPackets(totalMsgSize) == totalNumPackets) {
        if (0u == ADType) {
          ret = E_OK;
        } else {
          /* TODO */
          ASLOG(J1939TPE, ("[%d]RX Get FD CM BAM with assurance is not supported\n", channel));
        }
      }

    } else {
      ASLOG(J1939TPE, ("[%d]RX Get FD CM BAM with invalid length\n", channel));
    }
  } else if (J1939TP_IDLE == config->Context->State) {
    PgPGN = pduInfoPtr->SduDataPtr[5] + ((uint32_t)pduInfoPtr->SduDataPtr[6] << 8) +
            ((uint32_t)pduInfoPtr->SduDataPtr[7] << 16);
    totalMsgSize = pduInfoPtr->SduDataPtr[1] + ((uint32_t)pduInfoPtr->SduDataPtr[2] << 8);
    totalNumPackets = pduInfoPtr->SduDataPtr[3];
    if (J1939TpNumDtPackets(totalMsgSize) == totalNumPackets) {
      ret = E_OK;
    }
  } else {
    ASLOG(J1939TPE, ("[%d]RX Get CM BAM %02X when in state %d, ignore\n", channel,
                     pduInfoPtr->SduDataPtr[0u], config->Context->State));
  }

  if (E_OK == ret) {
    config->Context->PgPGN = PgPGN;
    bufReq = PduR_J1939TpStartOfReception(config->PduR_RxPduId, NULL, totalMsgSize, &bufferSize);
    if (BUFREQ_OK == bufReq) {
      config->Context->NextSN = 0u;
      config->Context->TpSduLength = totalMsgSize;
      config->Context->MaxPacketsPerBlock = 0xFFu;
      config->Context->State = J1939TP_WAIT_DT_RX;
      config->Context->Timer = config->T2;
    }
  }
}

static void J1939Tp_RxIndication_RxChannel_CM_EOMS(PduIdType channel,
                                                   const PduInfoType *pduInfoPtr) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[channel];

  uint32_t totalMsgSize;
  uint32_t totalNumPackets;
  uint32_t PgPGN;
  uint8_t ADSize;
  uint8_t ADType;

  if (J1939TP_WAIT_CM_EOMS_RX == config->Context->State) {
    PgPGN = pduInfoPtr->SduDataPtr[9] + ((uint32_t)pduInfoPtr->SduDataPtr[10] << 8) +
            ((uint32_t)pduInfoPtr->SduDataPtr[11] << 16);
    if (PgPGN == config->Context->PgPGN) {
      totalMsgSize = pduInfoPtr->SduDataPtr[1] + ((uint32_t)pduInfoPtr->SduDataPtr[2] << 8) +
                     ((uint32_t)pduInfoPtr->SduDataPtr[3] << 16);
      totalNumPackets = pduInfoPtr->SduDataPtr[4] + ((uint32_t)pduInfoPtr->SduDataPtr[5] << 8) +
                        ((uint32_t)pduInfoPtr->SduDataPtr[6] << 16);
      ADSize = pduInfoPtr->SduDataPtr[7];
      ADType = pduInfoPtr->SduDataPtr[8];
      if ((ADSize + 12u) == pduInfoPtr->SduLength) {
        if ((totalMsgSize == config->Context->TpSduLength) &&
            (totalNumPackets == J1939TP_NUM_OF_DT_PACKETS)) {
          if ((0u == ADType) && (0u == ADSize)) { /* No assurance data (default) */
            PduR_J1939TpRxIndication(config->PduR_RxPduId, E_OK);
            J1939Tp_RxSendCmEOMA(channel);
          } else {
            /* TODO: */
            ASLOG(J1939TPE, ("[%d]RX Get CM EOMS with assurance is not supported\n", channel));
            J1939Tp_RxSendCmAbort(channel, J1939TP_ABORT_REASON_ASSURANCE_DATA_NOT_MATCH);
          }
        }
      } else {
        ASLOG(J1939TPE, ("[%d]RX Get CM EOMS with invalid length\n", channel));
      }
    }
  } else {
    ASLOG(J1939TPE, ("[%d]RX Get CM EOMS %02X when in state %d, ignore\n", channel,
                     pduInfoPtr->SduDataPtr[0u], config->Context->State));
  }
}

static void J1939Tp_RxIndication_RxChannel_CM(PduIdType channel, const PduInfoType *pduInfoPtr) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[channel];
  if (J1939TpIsFd(config)) {
    if (12u <= pduInfoPtr->SduLength) {
      if (J1939TP_FD_CM_RTS == pduInfoPtr->SduDataPtr[0u]) {
        J1939Tp_RxIndication_RxChannel_CM_RTS(channel, pduInfoPtr);
      } else if (J1939TP_FD_CM_EOMS == pduInfoPtr->SduDataPtr[0u]) {
        J1939Tp_RxIndication_RxChannel_CM_EOMS(channel, pduInfoPtr);
      } else if (J1939TP_FD_CM_BAM == pduInfoPtr->SduDataPtr[0u]) {
        J1939Tp_RxIndication_RxChannel_CM_BAM(channel, pduInfoPtr);
      } else {
        ASLOG(J1939TPE, ("[%d]RX FD unknwon CM %02X\n", channel, pduInfoPtr->SduDataPtr[0u]));
      }
    } else {
      ASLOG(J1939TPE,
            ("[%d]RX FD CM with invalid length: %d\n", channel, (int)pduInfoPtr->SduLength));
    }
  } else {
    if (8u == pduInfoPtr->SduLength) {
      if (J1939TP_CM_RTS == pduInfoPtr->SduDataPtr[0u]) {
        J1939Tp_RxIndication_RxChannel_CM_RTS(channel, pduInfoPtr);
      } else if (J1939TP_CM_BAM == pduInfoPtr->SduDataPtr[0u]) {
        J1939Tp_RxIndication_RxChannel_CM_BAM(channel, pduInfoPtr);
      } else {
        ASLOG(J1939TPE, ("[%d]RX FD unknwon CM %02X\n", channel, pduInfoPtr->SduDataPtr[0u]));
      }
    } else {
      ASLOG(J1939TPE, ("[%d]RX CM with invalid length\n", channel));
    }
  }
}

static void J1939Tp_RxIndication_RxChannel_DT(PduIdType channel, const PduInfoType *pduInfoPtr) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[channel];
  Std_ReturnType ret = E_NOT_OK;
  PduInfoType PduInfo;
  uint32_t SN;
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
  PduLengthType offset = 0u;

  if (J1939TP_WAIT_DT_RX == config->Context->State) {
    if (J1939TpIsFd(config)) {
      if ((J1939TP_FD_DTFI == pduInfoPtr->SduDataPtr[0u]) && (pduInfoPtr->SduLength > 4u)) {
        SN = pduInfoPtr->SduDataPtr[1] + ((uint32_t)pduInfoPtr->SduDataPtr[2] << 8) +
             ((uint32_t)pduInfoPtr->SduDataPtr[3] << 16);
        offset = config->Context->NextSN * FD_DT_DATA_SIZE;
        PduInfo.SduLength = config->Context->TpSduLength - offset;
        PduInfo.SduDataPtr = &pduInfoPtr->SduDataPtr[4];
        if (PduInfo.SduLength > (pduInfoPtr->SduLength - 4u)) {
          PduInfo.SduLength = pduInfoPtr->SduLength - 4u;
        }
        ret = E_OK;
      } else {
        ASLOG(J1939TPE, ("[%d]RX DT witn invalid DTFI or Length\n", channel));
      }
    } else {
      if (pduInfoPtr->SduLength > 1u) {
        SN = pduInfoPtr->SduDataPtr[0u];
        offset = config->Context->NextSN * DT_DATA_SIZE;
        PduInfo.SduLength = config->Context->TpSduLength - offset;
        PduInfo.SduDataPtr = &pduInfoPtr->SduDataPtr[1];
        if (PduInfo.SduLength > (pduInfoPtr->SduLength - 1u)) {
          PduInfo.SduLength = pduInfoPtr->SduLength - 1u;
        }
        ret = E_OK;
      } else {
        ASLOG(J1939TPE, ("[%d]RX DT witn invalid Length\n", channel));
      }
    }
  } else {
    ASLOG(J1939TPE, ("[%d]RX DT %02X when in state %d, ignore\n", channel,
                     pduInfoPtr->SduDataPtr[0u], config->Context->State));
  }

  if (E_OK == ret) {
    if (SN == (config->Context->NextSN + 1u)) {
      config->Context->NextSN++;
      PduInfo.MetaDataPtr = (uint8_t *)&offset;
      bufReq = PduR_J1939TpCopyRxData(config->PduR_RxPduId, &PduInfo, &bufferSize);
      if (BUFREQ_OK == bufReq) {
        if (J1939TpIsFd(config) && (config->Context->NextSN >= NUM_OF_FD_DT_PACKETS)) {
          config->Context->State = J1939TP_WAIT_CM_EOMS_RX;
          config->Context->Timer = config->T1;
        } else if (config->Context->NextSN >= J1939TP_NUM_OF_DT_PACKETS) {
          PduR_J1939TpRxIndication(config->PduR_RxPduId, E_OK);
          J1939Tp_RxSendCmEOMA(channel);
        } else {
          if (config->Context->NumPacketsThisBlock > 0u) {
            config->Context->NumPacketsThisBlock--;
          }
          if (0u == config->Context->NumPacketsThisBlock) {
            J1939Tp_RxSendCmCTS(channel);
          } else {
            config->Context->State = J1939TP_WAIT_DT_RX;
            config->Context->Timer = config->T1;
          }
        }
      } else {
        ASLOG(J1939TPE, ("[%d]RX DT CopyRxData failed.\n", channel));
        J1939Tp_RxSendCmAbort(channel, J1939TP_ABORT_REASON_OUT_OF_RESOURCE);
      }
    } else {
      ASLOG(J1939TPE, ("[%d]RX DT with invalid SN\n", channel));
      J1939Tp_RxSendCmAbort(channel, J1939TP_ABORT_REASON_BAD_SEQUENCE_NUMBER);
      ret = E_NOT_OK;
    }
  }
}

static void J1939Tp_RxIndication_RxChannel_Direct(PduIdType channel,
                                                  const PduInfoType *pduInfoPtr) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[channel];
  BufReq_ReturnType bufReq;
  PduLengthType bufferSize;
  PduLengthType offset = 0u;
  PduInfoType PduInfo;

  bufReq = PduR_J1939TpStartOfReception(config->PduR_RxPduId, pduInfoPtr, pduInfoPtr->SduLength,
                                        &bufferSize);
  if (BUFREQ_OK == bufReq) {
    PduInfo.SduDataPtr = pduInfoPtr->SduDataPtr;
    PduInfo.SduLength = pduInfoPtr->SduLength;
    PduInfo.MetaDataPtr = (uint8_t *)&offset;
    bufReq = PduR_J1939TpCopyRxData(config->PduR_RxPduId, &PduInfo, &bufferSize);
    if (BUFREQ_OK == bufReq) {
      PduR_J1939TpRxIndication(config->PduR_RxPduId, E_OK);
    }
  }
}

static void J1939Tp_RxIndication_RxChannel(PduIdType channel, J1939Tp_PacketType packetType,
                                           const PduInfoType *pduInfoPtr) {
  ASLOG(J1939TP, ("[%d]RX Packet Type %d\n", channel, packetType));
  switch (packetType) {
  case J1939TP_PACKET_CM:
    J1939Tp_RxIndication_RxChannel_CM(channel, pduInfoPtr);
    break;
  case J1939TP_PACKET_DT:
    J1939Tp_RxIndication_RxChannel_DT(channel, pduInfoPtr);
    break;
  case J1939TP_PACKET_DIRECT:
    J1939Tp_RxIndication_RxChannel_Direct(channel, pduInfoPtr);
    break;
  default:
    break;
  }
}

/* ================================ [ FUNCTIONS ] ============================================== */
void J1939Tp_MainFunction_TxChannelFast(uint16_t channel) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[channel];
  Std_ReturnType ret;

  switch (config->Context->State) {
  case J1939TP_RESEND_DIRECT_PG:
    ret = CanIf_Transmit(config->CanIf_TxDirectNPdu, &config->Context->PduInfo);
    if (E_OK == ret) {
      config->Context->State = J1939TP_WAIT_DIRECT_PG_TX_COMPLETED;
      config->Context->Timer = config->Tr;
    }
    break;
  case J1939TP_RESEND_CM_RTS:
    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->Context->PduInfo);
    if (E_OK == ret) {
      config->Context->State = J1939TP_WAIT_CM_RTS_TX_COMPLETED;
      config->Context->Timer = config->Tr;
    }
    break;
  case J1939TP_RESEND_CM_BAM:
    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->Context->PduInfo);
    if (E_OK == ret) {
      config->Context->State = J1939TP_WAIT_CM_BAM_TX_COMPLETED;
      config->Context->Timer = config->Tr;
    }
    break;
  case J1939TP_RESEND_CM_ABORT:
    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->Context->PduInfo);
    if (E_OK == ret) {
      config->Context->State = J1939TP_WAIT_CM_ABORT_TX_COMPLETED;
      config->Context->Timer = config->Tr;
    }
    break;
  case J1939TP_RESEND_DT:
    ret = CanIf_Transmit(config->CanIf_TxDtNPdu, &config->Context->PduInfo);
    if (E_OK == ret) {
      config->Context->State = J1939TP_WAIT_DT_TX_COMPLETED;
      config->Context->Timer = config->Tr;
    }
    break;
  case J1939TP_RESEND_CM_EOMS:
    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->Context->PduInfo);
    if (E_OK == ret) {
      config->Context->State = J1939TP_WAIT_CM_EOMS_TX_COMPLETED;
      config->Context->Timer = config->Tr;
    }
    break;
  default:
    break;
  }
}

void J1939Tp_MainFunction_TxChannel(uint16_t channel) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[channel];
  Std_ReturnType ret;

  if (config->Context->Timer > 0u) {
    config->Context->Timer--;
    if (0u == config->Context->Timer) {
      if (J1939TP_WAIT_DT_STMIN_TIMEOUT != config->Context->State) {
        ASLOG(J1939TPE, ("[%d]TX timer timeout in state %d\n", channel, config->Context->State));
      }
      switch (config->Context->State) {
      case J1939TP_WAIT_DT_STMIN_TIMEOUT:
        ret = CanIf_Transmit(config->CanIf_TxDtNPdu, &config->Context->PduInfo);
        if (E_OK == ret) {
          config->Context->State = J1939TP_WAIT_DT_TX_COMPLETED;
          config->Context->Timer = config->Tr;
        } else {
          config->Context->State = J1939TP_RESEND_DT;
          config->Context->Timer = config->Tr;
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
        J1939Tp_TxSendCmAbort(channel, J1939TP_ABORT_REASON_TIMEOUT);
        break;
      default:
        J1939Tp_TxResetToIdle(config);
        break;
      }
    }
  }
}

void J1939Tp_MainFunction_RxChannelFast(uint16_t channel) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[channel];
  Std_ReturnType ret;

  switch (config->Context->State) {
  case J1939TP_RESEND_CM_CTS:
    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->Context->PduInfo);
    if (E_OK == ret) {
      config->Context->State = J1939TP_WAIT_CM_CTS_TX_COMPLETED;
      config->Context->Timer = config->Tr;
    }
    break;
  case J1939TP_RESEND_CM_EMOA:
    ret = CanIf_Transmit(config->CanIf_TxCmNPdu, &config->Context->PduInfo);
    if (E_OK == ret) {
      config->Context->State = J1939TP_WAIT_CM_EOMA_TX_COMPLETED;
      config->Context->Timer = config->Tr;
    }
    break;
  default:
    break;
  }
}

void J1939Tp_MainFunction_RxChannel(uint16_t channel) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[channel];

  if (config->Context->Timer > 0u) {
    config->Context->Timer--;
    if (0u == config->Context->Timer) {
      ASLOG(J1939TPE, ("[%d]RX timer timeout in state %d\n", channel, config->Context->State));
      switch (config->Context->State) {
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

void J1939Tp_InitRxChannel(PduIdType channel) {
  const J1939Tp_RxChannelType *config = &J1939TP_CONFIG->RxChannels[channel];
  J1939Tp_RxResetToIdle(config);
}

void J1939Tp_InitTxChannel(PduIdType channel) {
  const J1939Tp_TxChannelType *config = &J1939TP_CONFIG->TxChannels[channel];
  J1939Tp_TxResetToIdle(config);
}

Std_ReturnType J1939Tp_GetRxPgPGN(PduIdType channel, uint32_t *pgPGN) {
  Std_ReturnType ret = E_OK;
  const J1939Tp_RxChannelType *config;

  DET_VALIDATE(NULL != J1939TP_CONFIG, 0xF0, J1939TP_E_UNINIT, return E_NOT_OK);

  if (channel < J1939TP_CONFIG->NumOfRxChannels) {
    config = &J1939TP_CONFIG->RxChannels[channel];
    *pgPGN = config->Context->PgPGN;
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType J1939Tp_SetTxPgPGN(PduIdType channel, uint32_t pgPGN) {
  Std_ReturnType ret = E_OK;
  const J1939Tp_TxChannelType *config;

  DET_VALIDATE(NULL != J1939TP_CONFIG, 0xF1, J1939TP_E_UNINIT, return E_NOT_OK);

  if (channel < J1939TP_CONFIG->NumOfTxChannels) {
    config = &J1939TP_CONFIG->TxChannels[channel];
    config->Context->PgPGN = pgPGN;
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

void J1939Tp_Init(const J1939Tp_ConfigType *configPtr) {
  uint16_t i;
#ifdef J1939TP_USE_PB_CONFIG
  if (NULL != configPtr) {
    J1939TP_CONFIG = configPtr;
  } else {
    J1939TP_CONFIG = &J1939Tp_Config;
  }
#else
  (void)configPtr;
#endif
  for (i = 0u; i < J1939TP_CONFIG->NumOfTxChannels; i++) {
    J1939Tp_InitTxChannel(i);
  }

  for (i = 0u; i < J1939TP_CONFIG->NumOfRxChannels; i++) {
    J1939Tp_InitRxChannel(i);
  }
}

Std_ReturnType J1939Tp_Transmit(PduIdType txPduId, const PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_OK;
  const J1939Tp_TxChannelType *config;

  DET_VALIDATE(NULL != J1939TP_CONFIG, 0x49, J1939TP_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(txPduId < J1939TP_CONFIG->NumOfTxChannels, 0x49, J1939TP_E_PARAM_POINTER,
               return E_NOT_OK);
  DET_VALIDATE(NULL != pduInfoPtr, 0x49, J1939TP_E_PARAM_POINTER, return E_NOT_OK);

  config = &J1939TP_CONFIG->TxChannels[txPduId];
  if (J1939TP_IDLE == config->Context->State) {
    config->Context->TpSduLength = pduInfoPtr->SduLength;
    if (FALSE == J1939TpIsFd(config)) {
      if (J1939TP_NUM_OF_DT_PACKETS > 0xFFu) {
        ret = E_NOT_OK;
      }
    }
    if (E_OK == ret) {
      J1939Tp_StartToSend(txPduId);
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

void J1939Tp_TxConfirmation(PduIdType txPduId, Std_ReturnType result) {
  const J1939Tp_PduInfoType *PduInfo;

  DET_VALIDATE(NULL != J1939TP_CONFIG, 0x40, J1939TP_E_UNINIT, return);
  DET_VALIDATE(txPduId < J1939TP_CONFIG->NumOfTxPduInfos, 0x40, J1939TP_E_PARAM_POINTER, return);

  PduInfo = &(J1939TP_CONFIG->TxPduInfos[txPduId]);
  if (J1939TP_RX_CHANNEL == PduInfo->ChannelType) {
    J1939Tp_TxConfirmation_RxChannel(PduInfo->RxPduInfo->RxChannel, result);
  } else {
    J1939Tp_TxConfirmation_TxChannel(PduInfo->TxPduInfo->TxChannel, result);
  }
}

void J1939Tp_RxIndication(PduIdType rxPduId, const PduInfoType *pduInfoPtr) {
  const J1939Tp_PduInfoType *PduInfo;

  DET_VALIDATE(NULL != J1939TP_CONFIG, 0x42, J1939TP_E_UNINIT, return);
  DET_VALIDATE(rxPduId < J1939TP_CONFIG->NumOfRxPduInfos, 0x42, J1939TP_E_PARAM_POINTER, return);
  DET_VALIDATE((NULL != pduInfoPtr) && (NULL != pduInfoPtr->SduDataPtr) &&
                 (pduInfoPtr->SduLength > 0),
               0x42, J1939TP_E_PARAM_POINTER, return);

  ASLOG(J1939TP, ("[%d]RX len=%d data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", rxPduId,
                  pduInfoPtr->SduLength, pduInfoPtr->SduDataPtr[0u], pduInfoPtr->SduDataPtr[1],
                  pduInfoPtr->SduDataPtr[2], pduInfoPtr->SduDataPtr[3], pduInfoPtr->SduDataPtr[4],
                  pduInfoPtr->SduDataPtr[5], pduInfoPtr->SduDataPtr[6], pduInfoPtr->SduDataPtr[7]));

  PduInfo = &(J1939TP_CONFIG->RxPduInfos[rxPduId]);
  if (J1939TP_RX_CHANNEL == PduInfo->ChannelType) {
    J1939Tp_RxIndication_RxChannel(PduInfo->RxPduInfo->RxChannel, PduInfo->RxPduInfo->PacketType,
                                   pduInfoPtr);
  } else {
    J1939Tp_RxIndication_TxChannel(PduInfo->TxPduInfo->TxChannel, PduInfo->TxPduInfo->PacketType,
                                   pduInfoPtr);
  }
}

void J1939Tp_MainFunction(void) {
  uint16_t i;

  DET_VALIDATE(NULL != J1939TP_CONFIG, 0x04, J1939TP_E_UNINIT, return);

  for (i = 0u; i < J1939TP_CONFIG->NumOfTxChannels; i++) {
    J1939Tp_MainFunction_TxChannel(i);
  }

  for (i = 0u; i < J1939TP_CONFIG->NumOfRxChannels; i++) {
    J1939Tp_MainFunction_RxChannel(i);
  }
}

void J1939Tp_MainFunctionFast(void) {
  uint16_t i;

  DET_VALIDATE(NULL != J1939TP_CONFIG, 0x04, J1939TP_E_UNINIT, return);

  for (i = 0u; i < J1939TP_CONFIG->NumOfTxChannels; i++) {
    J1939Tp_MainFunction_TxChannelFast(i);
  }

  for (i = 0u; i < J1939TP_CONFIG->NumOfRxChannels; i++) {
    J1939Tp_MainFunction_RxChannelFast(i);
  }
}

void J1939Tp_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x03, J1939TP_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_J1939TP;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 1;
}
/** @brief release notes
 * - 4.0.1: Fix FD DT transmit SN check issue
 */
