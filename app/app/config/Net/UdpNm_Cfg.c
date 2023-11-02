/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "UdpNm.h"
#include "UdpNm_Cfg.h"
#include "UdpNm_Priv.h"
#include "Std_Debug.h"
#include "SoAd.h"
#ifdef _WIN32
#include <stdlib.h>
#endif
#include "GEN/SoAd_Cfg.h"
/* ================================ [ MACROS    ] ============================================== */
#define UDPNM_ImmediateNmCycleTime UDPNM_CONVERT_MS_TO_MAIN_CYCLES(100)
#define UDPNM_MsgCycleOffset UDPNM_CONVERT_MS_TO_MAIN_CYCLES(100)
#define UDPNM_MsgCycleTime UDPNM_CONVERT_MS_TO_MAIN_CYCLES(1000)
#define UDPNM_MsgTimeoutTime UDPNM_CONVERT_MS_TO_MAIN_CYCLES(10)
#define UDPNM_RepeatMessageTime UDPNM_CONVERT_MS_TO_MAIN_CYCLES(2000)
#define UDPNM_NmTimeoutTime UDPNM_CONVERT_MS_TO_MAIN_CYCLES(2000)
#define UDPNM_WaitBusSleepTime UDPNM_CONVERT_MS_TO_MAIN_CYCLES(5000)
#define UDPNM_RemoteSleepIndTime UDPNM_CONVERT_MS_TO_MAIN_CYCLES(1500)
#define UDPNM_NODE_ID 0
#define UDPNM_ImmediateNmTransmissions 10

#ifdef _WIN32
#define L_CONST
#else
#define L_CONST const
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
#ifdef UDPNM_GLOBAL_PN_SUPPORT
static const uint8_t nm0PnFilterMaskByte[2] = {0x55, 0xAA};
#endif
static L_CONST UdpNm_ChannelConfigType UdpNm_ChannelConfigs[] = {
  {
    UDPNM_ImmediateNmCycleTime,
    UDPNM_MsgCycleOffset,
    UDPNM_MsgCycleTime,
    UDPNM_MsgTimeoutTime,
    UDPNM_RepeatMessageTime,
    UDPNM_NmTimeoutTime,
    UDPNM_WaitBusSleepTime,
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED
    UDPNM_RemoteSleepIndTime,
#endif
    SOAD_TX_PID_UDPNM_CHL0, /* TxPdu */
    UDPNM_NODE_ID,
    0, /* nmNetworkHandle */
    UDPNM_ImmediateNmTransmissions,
    UDPNM_PDU_BYTE_1, /* PduCbvPosition */
    UDPNM_PDU_BYTE_0, /* PduNidPosition */
    TRUE,             /* ActiveWakeupBitEnabled */
    FALSE,            /* PassiveModeEnabled */
    TRUE,             /* RepeatMsgIndEnabled */
    TRUE,             /* NodeDetectionEnabled */
#ifdef UDPNM_GLOBAL_PN_SUPPORT
    TRUE,                        /* PnEnabled */
    TRUE,                        /* AllNmMessagesKeepAwake */
    4,                           /* PnInfoOffset */
    sizeof(nm0PnFilterMaskByte), /* PnInfoLength */
    nm0PnFilterMaskByte,
#endif
  },
};

static UdpNm_ChannelContextType UdpNm_ChannelContexts[ARRAY_SIZE(UdpNm_ChannelConfigs)];

const UdpNm_ConfigType UdpNm_Config = {
  UdpNm_ChannelConfigs,
  UdpNm_ChannelContexts,
  ARRAY_SIZE(UdpNm_ChannelConfigs),
};
/* ================================ [ LOCALS    ] ============================================== */
#ifdef _WIN32
static void __attribute__((constructor)) _udpnm_start(void) {
  char *nodeStr = getenv("UDPNM_NODE_ID");
  if (nodeStr != NULL) {
    UdpNm_ChannelConfigs[0].NodeId = atoi(nodeStr);
    ASLOG(INFO, ("UdpNm NodeId=%d\n", UdpNm_ChannelConfigs[0].NodeId));
  }
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */