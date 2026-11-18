/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "CanTp_Cfg.h"
#include "CanTp.h"
#include "CanTp_Priv.h"
#include "CanIf_Cfg.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef CANTP_CFG_N_As
#define CANTP_CFG_N_As 100
#endif
#ifndef CANTP_CFG_N_Bs
#define CANTP_CFG_N_Bs 1000
#endif
#ifndef CANTP_CFG_N_Cr
#define CANTP_CFG_N_Cr 1000
#endif

#ifndef CANTP_CFG_STMIN
#define CANTP_CFG_STMIN 0
#endif

#ifndef CANTP_CFG_BS
#define CANTP_CFG_BS 8
#endif

#ifndef CANTP_CFG_RX_WFT_MAX
#define CANTP_CFG_RX_WFT_MAX 8
#endif

#ifndef CANTP_LINK_LAYER_DATA_LENGTH
#define CANTP_LINK_LAYER_DATA_LENGTH 8
#endif

#ifndef CANTP_CFG_PADDING
#define CANTP_CFG_PADDING 0x55
#endif

#ifndef CANTP_MAX_CHANNELS
#define CANTP_MAX_CHANNELS 32
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void CanIf_CanTpReconfig(uint8_t channel, CanTp_ParamType *params);
extern void PduR_CanTpReConfig(uint8_t channel);
/* ================================ [ DATAS     ] ============================================== */
static uint8_t u8P2PData[CANTP_MAX_CHANNELS][64];
static CanTp_ChannelConfigType CanTpChannelConfigs[CANTP_MAX_CHANNELS];
static CanTp_ChannelContextType CanTpChannelContexts[ARRAY_SIZE(CanTpChannelConfigs)];
const CanTp_ConfigType CanTp_Config = {
  CanTpChannelConfigs,
  CanTpChannelContexts,
  ARRAY_SIZE(CanTpChannelConfigs),
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void CanTp_ReConfig(uint8_t channel, CanTp_ParamType *params) {
  if (channel < ARRAY_SIZE(CanTpChannelConfigs)) {
    CanTpChannelConfigs[channel].AddressingFormat = CANTP_STANDARD;
    CanTpChannelConfigs[channel].CanIfTxPduId = channel;
    CanTpChannelConfigs[channel].PduR_RxPduId = CANTP_MAX_CHANNELS + channel;
    CanTpChannelConfigs[channel].PduR_TxPduId = channel;
    CanTpChannelConfigs[channel].N_As = CANTP_CONVERT_MS_TO_MAIN_CYCLES(CANTP_CFG_N_As);
    CanTpChannelConfigs[channel].N_Bs = CANTP_CONVERT_MS_TO_MAIN_CYCLES(CANTP_CFG_N_Bs);
    CanTpChannelConfigs[channel].N_Cr = CANTP_CONVERT_MS_TO_MAIN_CYCLES(CANTP_CFG_N_Cr);
    CanTpChannelConfigs[channel].STmin = params->STmin;
    CanTpChannelConfigs[channel].BS = CANTP_CFG_BS;
    CanTpChannelConfigs[channel].N_TA = params->N_TA;
    if (0xFFFF != params->N_TA) {
      CanTpChannelConfigs[channel].AddressingFormat = CANTP_EXTENDED;
    }
    CanTpChannelConfigs[channel].CanTpRxWftMax = CANTP_CFG_RX_WFT_MAX;
    CanTpChannelConfigs[channel].Padding = CANTP_CFG_PADDING;
    CanTpChannelConfigs[channel].Data = u8P2PData[channel];
    CanTpChannelConfigs[channel].LinkLayerDataLength = params->LinkLayerDataLength;
    PduR_CanTpReConfig(channel);
    CanIf_CanTpReconfig(channel, params);
  }
}
