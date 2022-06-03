/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "CanIf.h"
#include "CanIf_Priv.h"
#include "CanTp.h"
#include "CanTp_Cfg.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef CANTP_MAX_CHANNELS
#define CANTP_MAX_CHANNELS 32
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void Can_ReConfig(uint8_t Controller, const char *device, int port, uint32_t baudrate);
/* ================================ [ DATAS     ] ============================================== */
static CanIf_RxPduType CanIf_RxPdus[CANTP_MAX_CHANNELS];
static CanIf_TxPduType CanIf_TxPdus[CANTP_MAX_CHANNELS];

const CanIf_ConfigType CanIf_Config = {
  CanIf_RxPdus,
  CanIf_TxPdus,
  ARRAY_SIZE(CanIf_RxPdus),
  ARRAY_SIZE(CanIf_TxPdus),
};

/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void CanIf_CanTpReconfig(uint8_t Channel, CanTp_ParamType *params) {
  CanIf_RxPdus[Channel].rxInd = CanTp_RxIndication;
  CanIf_RxPdus[Channel].rxPduId = Channel;
  CanIf_RxPdus[Channel].canid = params->RxCanId;
  CanIf_RxPdus[Channel].mask = 0xFFFFFFFF;
  CanIf_RxPdus[Channel].hoh = Channel;

  CanIf_TxPdus[Channel].txConfirm = CanTp_TxConfirmation;
  CanIf_TxPdus[Channel].txPduId = Channel;
  CanIf_TxPdus[Channel].canid = params->TxCanId;
  CanIf_TxPdus[Channel].p_canid = NULL;
  CanIf_TxPdus[Channel].hoh = Channel;

  Can_ReConfig(Channel, params->device, params->port, params->baudrate);
}