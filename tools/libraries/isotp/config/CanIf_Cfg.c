/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "CanIf.h"
#include "CanIf_Priv.h"
#include "CanIf_Cfg.h"
#include "CanTp.h"
#include "CanTp_Cfg.h"
#include "J1939Tp.h"
#include "J1939Tp_Cfg.h"
/* ================================ [ MACROS    ] ============================================== */
#define CANIF_MAX_RX_PDU 4
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void Can_ReConfig(uint8_t controller, const char *device, int port, uint32_t baudrate);
/* ================================ [ DATAS     ] ============================================== */
static CanIf_RxPduType CanIf_RxPdus[CANTP_MAX_CHANNELS + J1939TP_MAX_CHANNELS][CANIF_MAX_RX_PDU];
static CanIf_TxPduType CanIf_TxPdus[CANTP_MAX_CHANNELS + J1939TP_MAX_CHANNELS * 4];
static CanIf_CtrlContextType CanIf_CtrlContexts[CANTP_MAX_CHANNELS + J1939TP_MAX_CHANNELS];
static CanIf_CtrlConfigType CanIf_CtrlConfigs[CANTP_MAX_CHANNELS + J1939TP_MAX_CHANNELS];
const CanIf_ConfigType CanIf_Config = {
  CanIf_TxPdus,
  CanIf_CtrlContexts,
  CanIf_CtrlConfigs,
  ARRAY_SIZE(CanIf_TxPdus),
  ARRAY_SIZE(CanIf_CtrlContexts),
};

/* ================================ [ LOCALS    ] ============================================== */
INITIALIZER(_isotp_canif_init) {
  int i, j;
  for (i = 0; i < ARRAY_SIZE(CanIf_CtrlContexts); i++) {
    CanIf_CtrlContexts[i].PduMode = CANIF_ONLINE;
    CanIf_CtrlConfigs[i].RxPdus = CanIf_RxPdus[i];
    CanIf_CtrlConfigs[i].NumOfRxPdus = CANIF_MAX_RX_PDU;
    for (j = 0; j < CANIF_MAX_RX_PDU; j++) {
      CanIf_RxPdus[i][j].Mask = 0xFFFFFFFF;
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void CanIf_CanTpReconfig(uint8_t channel, CanTp_ParamType *params) {
  CanIf_RxPdus[channel][0].RxInd = CanTp_RxIndication;
  CanIf_RxPdus[channel][0].RxPduId = channel;
  CanIf_RxPdus[channel][0].CanId = params->RxCanId;
  CanIf_RxPdus[channel][0].Mask = 0x1FFFFFFF;
  CanIf_RxPdus[channel][0].Hoh = channel;

  CanIf_TxPdus[channel].TxConfirm = CanTp_TxConfirmation;
  CanIf_TxPdus[channel].TxPduId = channel;
  CanIf_TxPdus[channel].CanId = params->TxCanId;
  CanIf_TxPdus[channel].CanIdPtr = NULL;
  CanIf_TxPdus[channel].Hoh = channel;

  Can_ReConfig(channel, params->device, params->port, params->baudrate);

  CanIf_CtrlContexts[channel].PduMode = CANIF_ONLINE;
}

void CanIf_J1939TpReconfig(uint8_t channel, J1939Tp_ParamType *params) {
  uint32_t index = CANTP_MAX_CHANNELS + 4 * channel;

  Can_HwHandleType hoh = channel + CANTP_MAX_CHANNELS;

  CanIf_TxPdus[index].TxConfirm = J1939Tp_TxConfirmation;
  CanIf_TxPdus[index].TxPduId = 4 * channel;
  CanIf_TxPdus[index].CanId = params->TX.CM;
  CanIf_TxPdus[index].CanIdPtr = NULL;
  CanIf_TxPdus[index].Hoh = hoh;

  CanIf_TxPdus[index + 1].TxConfirm = J1939Tp_TxConfirmation;
  CanIf_TxPdus[index + 1].TxPduId = 4 * channel + 1;
  CanIf_TxPdus[index + 1].CanId = params->TX.DT;
  CanIf_TxPdus[index + 1].CanIdPtr = NULL;
  CanIf_TxPdus[index + 1].Hoh = hoh;

  CanIf_TxPdus[index + 2].TxConfirm = J1939Tp_TxConfirmation;
  CanIf_TxPdus[index + 2].TxPduId = 4 * channel + 2;
  CanIf_TxPdus[index + 2].CanId = params->TX.Direct;
  CanIf_TxPdus[index + 2].CanIdPtr = NULL;
  CanIf_TxPdus[index + 2].Hoh = hoh;

  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][3].RxInd = J1939Tp_RxIndication;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][3].RxPduId = 4 * channel + 3;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][3].CanId = params->TX.FC;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][3].Mask = 0x1FFFFFFF;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][3].Hoh = hoh;

  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][0].RxInd = J1939Tp_RxIndication;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][0].RxPduId = 4 * channel;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][0].CanId = params->RX.CM;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][0].Mask = 0x1FFFFFFF;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][0].Hoh = hoh;

  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][1].RxInd = J1939Tp_RxIndication;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][1].RxPduId = 4 * channel + 1;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][1].CanId = params->RX.DT;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][1].Mask = 0x1FFFFFFF;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][1].Hoh = hoh;

  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][2].RxInd = J1939Tp_RxIndication;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][2].RxPduId = 4 * channel + 2;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][2].CanId = params->RX.Direct;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][2].Mask = 0x1FFFFFFF;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + channel][2].Hoh = hoh;

  CanIf_TxPdus[index + 3].TxConfirm = J1939Tp_TxConfirmation;
  CanIf_TxPdus[index + 3].TxPduId = 4 * channel + 3;
  CanIf_TxPdus[index + 3].CanId = params->RX.FC;
  CanIf_TxPdus[index + 3].CanIdPtr = NULL;
  CanIf_TxPdus[index + 3].Hoh = hoh;

  Can_ReConfig(hoh, params->device, params->port, params->baudrate);

  CanIf_CtrlContexts[hoh].PduMode = CANIF_ONLINE;
}

void CanIf_CanTpSetTxCanId(uint8_t channel, uint32_t txCanId) {
  CanIf_TxPdus[channel].CanId = txCanId;
}

uint32_t CanIf_CanTpGetTxCanId(uint8_t channel) {
  return CanIf_TxPdus[channel].CanId;
}

uint32_t CanIf_CanTpGetRxCanId(uint8_t channel) {
  return CanIf_RxPdus[channel][0].CanId;
}
