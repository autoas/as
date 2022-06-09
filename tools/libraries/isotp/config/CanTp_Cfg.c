/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "CanTp_Cfg.h"
#include "CanTp.h"
#include "CanTp_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef CANTP_CFG_N_As
#define CANTP_CFG_N_As 25
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

#ifndef CANTP_LL_DL
#define CANTP_LL_DL 8
#endif

#ifndef CANTP_CFG_PADDING
#define CANTP_CFG_PADDING 0x55
#endif

#ifndef CANTP_MAX_CHANNELS
#define CANTP_MAX_CHANNELS 32
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void CanIf_CanTpReconfig(uint8_t Channel, CanTp_ParamType *params);
extern void PduR_CanTpReConfig(uint8_t Channel);
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
void CanTp_ReConfig(uint8_t Channel, CanTp_ParamType *params) {
  if (Channel < ARRAY_SIZE(CanTpChannelConfigs)) {
    CanTpChannelConfigs[Channel].AddressingFormat = CANTP_STANDARD;
    CanTpChannelConfigs[Channel].CanIfTxPduId = Channel;
    CanTpChannelConfigs[Channel].PduR_RxPduId = Channel;
    CanTpChannelConfigs[Channel].PduR_TxPduId = Channel;
    CanTpChannelConfigs[Channel].N_As = CANTP_CONVERT_MS_TO_MAIN_CYCLES(CANTP_CFG_N_As);
    CanTpChannelConfigs[Channel].N_Bs = CANTP_CONVERT_MS_TO_MAIN_CYCLES(CANTP_CFG_N_Bs);
    CanTpChannelConfigs[Channel].N_Cr = CANTP_CONVERT_MS_TO_MAIN_CYCLES(CANTP_CFG_N_Cr);
    CanTpChannelConfigs[Channel].STmin = CANTP_CFG_STMIN;
    CanTpChannelConfigs[Channel].BS = CANTP_CFG_BS;
    CanTpChannelConfigs[Channel].N_TA = 0;
    CanTpChannelConfigs[Channel].CanTpRxWftMax = CANTP_CFG_RX_WFT_MAX;
    CanTpChannelConfigs[Channel].padding = CANTP_CFG_PADDING;
    CanTpChannelConfigs[Channel].data = u8P2PData[Channel];
    CanTpChannelConfigs[Channel].LL_DL = params->ll_dl;
    PduR_CanTpReConfig(Channel);
    CanIf_CanTpReconfig(Channel, params);
  }
}
