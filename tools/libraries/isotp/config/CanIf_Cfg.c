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
/* ================================ [ MACROS    ] ============================================== */
#define CANIF_MAX_RX_PDU 4
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void Can_ReConfig(uint8_t Controller, const char *device, int port, uint32_t baudrate);
/* ================================ [ DATAS     ] ============================================== */
static CanIf_RxPduType CanIf_RxPdus[CANTP_MAX_CHANNELS][CANIF_MAX_RX_PDU];
static CanIf_TxPduType CanIf_TxPdus[CANTP_MAX_CHANNELS];
static CanIf_CtrlContextType CanIf_CtrlContexts[CANTP_MAX_CHANNELS];
static CanIf_CtrlConfigType CanIf_CtrlConfigs[CANTP_MAX_CHANNELS];
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

void CanIf_CanTpSetTxCanId(uint8_t Channel, uint32_t TxCanId) {
  CanIf_TxPdus[Channel].canid = TxCanId;
}

uint32_t CanIf_CanTpGetTxCanId(uint8_t Channel) {
  return CanIf_TxPdus[Channel].canid;
}

uint32_t CanIf_CanTpGetRxCanId(uint8_t Channel) {
  return CanIf_RxPdus[Channel][0].canid;
}
