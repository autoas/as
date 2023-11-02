/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "LinIf_Internal.h"
#include "LinTp.h"
/* ================================ [ MACROS    ] ============================================== */
#ifdef _WIN32
#define LINIF_TIMEOUT_US 1000000
#else
#define LINIF_TIMEOUT_US 5000
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
Std_ReturnType LinIf_DiagMRFCallback(uint8_t channel, Lin_PduType *frame,
                                     Std_ReturnType notifyResult);
Std_ReturnType LinIf_DiagSRFCallback(uint8_t channel, Lin_PduType *frame,
                                     Std_ReturnType notifyResult);
/* ================================ [ DATAS     ] ============================================== */

static LinIf_ScheduleTableEntryType entrysDiag[] = {
  {0x3C, 8, LINIF_DIAG_MRF, LIN_ENHANCED_CS, LIN_FRAMERESPONSE_RX, LinIf_DiagMRFCallback,
   LINIF_TIMEOUT_US},
  {0x3D, 8, LINIF_DIAG_SRF, LIN_ENHANCED_CS, LIN_FRAMERESPONSE_TX, LinIf_DiagSRFCallback,
   LINIF_TIMEOUT_US},
};

static const LinIf_ScheduleTableType scheduleTables[] = {
  {entrysDiag, ARRAY_SIZE(entrysDiag)},
};

static LinIf_ChannelContextType LinIf_ChannelContexts[1];
static const LinIf_ChannelConfigType LinIf_ChannelCfgs[ARRAY_SIZE(LinIf_ChannelContexts)] = {
  {LINIF_SLAVE, LINIF_TIMEOUT_US, 0},
};

const LinIf_ConfigType LinIf_Config = {
  scheduleTables,        ARRAY_SIZE(scheduleTables),        LinIf_ChannelCfgs,
  LinIf_ChannelContexts, ARRAY_SIZE(LinIf_ChannelContexts),
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType LinIf_DiagMRFCallback(uint8_t channel, Lin_PduType *frame,
                                     Std_ReturnType notifyResult) {
  Std_ReturnType r = LINIF_R_NOT_OK;
  PduInfoType pduInfo;

  if (LINIF_R_RECEIVED_OK == notifyResult) {
    pduInfo.SduDataPtr = frame->SduPtr;
    pduInfo.SduLength = frame->Dl;
    LinTp_RxIndication(channel, &pduInfo);
    r = LINIF_R_OK;
  }

  return r;
}

Std_ReturnType LinIf_DiagSRFCallback(uint8_t channel, Lin_PduType *frame,
                                     Std_ReturnType notifyResult) {
  Std_ReturnType r = LINIF_R_NOT_OK;
  Std_ReturnType ret;
  PduInfoType pduInfo;

  if (LINIF_R_TRIGGER_TRANSMIT == notifyResult) {
    pduInfo.SduDataPtr = frame->SduPtr;
    pduInfo.SduLength = frame->Dl;
    ret = LinTp_TriggerTransmit(0, &pduInfo);
    if (E_OK == ret) {
      r = LINIF_R_OK;
    }
  }

  return r;
}
