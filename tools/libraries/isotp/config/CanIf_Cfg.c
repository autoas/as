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
extern void Can_ReConfig(uint8_t Controller, const char *device, int port, uint32_t baudrate);
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
    CanIf_CtrlConfigs[i].rxPdus = CanIf_RxPdus[i];
    CanIf_CtrlConfigs[i].numOfRxPdus = CANIF_MAX_RX_PDU;
    for (j = 0; j < CANIF_MAX_RX_PDU; j++) {
      CanIf_RxPdus[i][j].mask = 0xFFFFFFFF;
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void CanIf_CanTpReconfig(uint8_t Channel, CanTp_ParamType *params) {
  CanIf_RxPdus[Channel][0].rxInd = CanTp_RxIndication;
  CanIf_RxPdus[Channel][0].rxPduId = Channel;
  CanIf_RxPdus[Channel][0].canid = params->RxCanId;
  CanIf_RxPdus[Channel][0].mask = 0x1FFFFFFF;
  CanIf_RxPdus[Channel][0].hoh = Channel;

  CanIf_TxPdus[Channel].txConfirm = CanTp_TxConfirmation;
  CanIf_TxPdus[Channel].txPduId = Channel;
  CanIf_TxPdus[Channel].canid = params->TxCanId;
  CanIf_TxPdus[Channel].p_canid = NULL;
  CanIf_TxPdus[Channel].hoh = Channel;

  Can_ReConfig(Channel, params->device, params->port, params->baudrate);

  CanIf_CtrlContexts[Channel].PduMode = CANIF_ONLINE;
}

void CanIf_J1939TpReconfig(uint8_t Channel, J1939Tp_ParamType *params) {
  uint32_t index = CANTP_MAX_CHANNELS + 4 * Channel;

  Can_HwHandleType hoh = Channel + CANTP_MAX_CHANNELS;

  CanIf_TxPdus[index].txConfirm = J1939Tp_TxConfirmation;
  CanIf_TxPdus[index].txPduId = 4 * Channel;
  CanIf_TxPdus[index].canid = params->TX.CM;
  CanIf_TxPdus[index].p_canid = NULL;
  CanIf_TxPdus[index].hoh = hoh;

  CanIf_TxPdus[index + 1].txConfirm = J1939Tp_TxConfirmation;
  CanIf_TxPdus[index + 1].txPduId = 4 * Channel + 1;
  CanIf_TxPdus[index + 1].canid = params->TX.DT;
  CanIf_TxPdus[index + 1].p_canid = NULL;
  CanIf_TxPdus[index + 1].hoh = hoh;

  CanIf_TxPdus[index + 2].txConfirm = J1939Tp_TxConfirmation;
  CanIf_TxPdus[index + 2].txPduId = 4 * Channel + 2;
  CanIf_TxPdus[index + 2].canid = params->TX.Direct;
  CanIf_TxPdus[index + 2].p_canid = NULL;
  CanIf_TxPdus[index + 2].hoh = hoh;

  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][3].rxInd = J1939Tp_RxIndication;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][3].rxPduId = 4 * Channel + 3;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][3].canid = params->TX.FC;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][3].mask = 0x1FFFFFFF;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][3].hoh = hoh;

  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][0].rxInd = J1939Tp_RxIndication;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][0].rxPduId = 4 * Channel;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][0].canid = params->RX.CM;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][0].mask = 0x1FFFFFFF;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][0].hoh = hoh;

  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][1].rxInd = J1939Tp_RxIndication;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][1].rxPduId = 4 * Channel + 1;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][1].canid = params->RX.DT;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][1].mask = 0x1FFFFFFF;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][1].hoh = hoh;

  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][2].rxInd = J1939Tp_RxIndication;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][2].rxPduId = 4 * Channel + 2;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][2].canid = params->RX.Direct;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][2].mask = 0x1FFFFFFF;
  CanIf_RxPdus[CANTP_MAX_CHANNELS + Channel][2].hoh = hoh;

  CanIf_TxPdus[index + 3].txConfirm = J1939Tp_TxConfirmation;
  CanIf_TxPdus[index + 3].txPduId = 4 * Channel + 3;
  CanIf_TxPdus[index + 3].canid = params->RX.FC;
  CanIf_TxPdus[index + 3].p_canid = NULL;
  CanIf_TxPdus[index + 3].hoh = hoh;

  Can_ReConfig(hoh, params->device, params->port, params->baudrate);

  CanIf_CtrlContexts[hoh].PduMode = CANIF_ONLINE;
}

void CanIf_CanTpSetTxCanId(uint8_t Channel, uint32_t TxCanId) {
  CanIf_TxPdus[Channel].canid = TxCanId;
}

uint32_t CanIf_CanTpGetTxCanId(uint8_t Channel) {
  return CanIf_TxPdus[Channel].canid;
}

uint32_t CanIf_CanTpGetRxCanId(uint8_t Channel) {
  return CanIf_RxPdus[Channel][0].canid;
}
