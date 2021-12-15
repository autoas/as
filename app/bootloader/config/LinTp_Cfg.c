/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "LinTp_Cfg.h"
#include "LinTp.h"
#include "LinTp_Priv.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef LINTP_CFG_N_As
#define LINTP_CFG_N_As 1000
#endif
#ifndef LINTP_CFG_N_Bs
#define LINTP_CFG_N_Bs 1000
#endif
#ifndef LINTP_CFG_N_Cr
#define LINTP_CFG_N_Cr 200
#endif

#ifndef LINTP_CFG_STMIN
#define LINTP_CFG_STMIN 0
#endif

#define LINTP_CFG_BS 0

#ifndef LINTP_CFG_RX_WFT_MAX
#define LINTP_CFG_RX_WFT_MAX 8
#endif

#ifndef LINTP_LL_DL
#define LINTP_LL_DL 8
#endif

#ifndef LINTP_CFG_PADDING
#define LINTP_CFG_PADDING 0x55
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static uint8_t u8P2PData[LINTP_LL_DL];
static LinTp_ChannelConfigType LinTpChannelConfigs[] = {
  {
    /* P2P */
    CANTP_EXTENDED,
    0,
    0 /* PduR_RxPduId */,
    0 /* PduR_TxPduId */,
    LINTP_CONVERT_MS_TO_MAIN_CYCLES(LINTP_CFG_N_As),
    LINTP_CONVERT_MS_TO_MAIN_CYCLES(LINTP_CFG_N_Bs),
    LINTP_CONVERT_MS_TO_MAIN_CYCLES(LINTP_CFG_N_Cr),
    LINTP_CFG_STMIN,
    LINTP_CFG_BS,
    0 /* N_TA */,
    LINTP_CFG_RX_WFT_MAX,
    LINTP_LL_DL,
    LINTP_CFG_PADDING,
    u8P2PData,
  },
};

static LinTp_ChannelContextType LinTpChannelContexts[ARRAY_SIZE(LinTpChannelConfigs)];

const LinTp_ConfigType LinTp_Config = {
  LinTpChannelConfigs,
  LinTpChannelContexts,
  ARRAY_SIZE(LinTpChannelConfigs),
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
