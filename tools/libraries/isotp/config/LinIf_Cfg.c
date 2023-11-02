/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "LinIf_Internal.h"
/* ================================ [ MACROS    ] ============================================== */
#ifdef _WIN32
#define LINIF_DELAY_US 100000
#define LINIF_TIMEOUT_US 1000000
#else
#define LINIF_DELAY_US 5000
#define LINIF_TIMEOUT_US 5000
#endif

#ifndef LINTP_LL_DL
#define LINTP_LL_DL 8
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
LinIf_ResultType LinIf_ApplicativeCallback(uint8_t channel, Lin_PduType *frame,
                                           LinIf_ResultType notifyResult);
LinIf_ResultType LinIf_DiagMRFCallback(uint8_t channel, Lin_PduType *frame,
                                       LinIf_ResultType notifyResult);
LinIf_ResultType LinIf_DiagSRFCallback(uint8_t channel, Lin_PduType *frame,
                                       LinIf_ResultType notifyResult);
/* ================================ [ DATAS     ] ============================================== */
LinIf_ScheduleTableEntryType entrysApplicative[] = {
  {0x01, 8, LINIF_UNCONDITIONAL, LIN_ENHANCED_CS, LIN_FRAMERESPONSE_TX, LinIf_ApplicativeCallback,
   LINIF_DELAY_US},
};

static LinIf_ScheduleTableEntryType entrysDiagRequest[] = {
  {0x3C, 8, LINIF_DIAG_MRF, LIN_ENHANCED_CS, LIN_FRAMERESPONSE_TX, LinIf_DiagMRFCallback, 5000},
};

static LinIf_ScheduleTableEntryType entrysDiagResponse[] = {
  {0x3D, 8, LINIF_DIAG_SRF, LIN_ENHANCED_CS, LIN_FRAMERESPONSE_RX, LinIf_DiagSRFCallback,
   LINIF_DELAY_US},
};

static const LinIf_ScheduleTableType scheduleTables[] = {
  {NULL, 0}, /* for NULL schedule */
  {entrysApplicative, ARRAY_SIZE(entrysApplicative)},
  {entrysDiagRequest, ARRAY_SIZE(entrysDiagRequest)},
  {entrysDiagResponse, ARRAY_SIZE(entrysDiagResponse)},
};

static LinIf_ChannelContextType LinIf_ChannelContexts[1];
static const LinIf_ChannelConfigType LinIf_ChannelCfgs[ARRAY_SIZE(LinIf_ChannelContexts)] = {
  {LINIF_MASTER, LINIF_TIMEOUT_US, 0},
};

const LinIf_ConfigType LinIf_Config = {
  scheduleTables,        ARRAY_SIZE(scheduleTables),        LinIf_ChannelCfgs,
  LinIf_ChannelContexts, ARRAY_SIZE(LinIf_ChannelContexts),
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void LinIf_ReConfig(uint8_t Channel, uint8_t ll_dl, uint32_t rxid, uint32_t txid) {
  if (Channel < ARRAY_SIZE(LinIf_ChannelCfgs)) {
    entrysDiagRequest->id = txid;
    entrysDiagRequest->dlc = ll_dl;
    entrysDiagResponse->id = rxid;
    entrysDiagResponse->dlc = ll_dl;
  }
}

uint32_t LinIf_CanTpGetTxId(uint8_t Channel) {
  return entrysDiagRequest->id;
}

uint32_t LinIf_CanTpGetRxId(uint8_t Channel) {
  return entrysDiagResponse->id;
}