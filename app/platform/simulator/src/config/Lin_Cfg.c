/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Lin.h"
#include "Lin_Cfg.h"
#include "Lin_Priv.h"
#include <string.h>
#include <stdio.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static Lin_ChannelContextType Lin_ChannelContexts[LIN_CHANNEL_NUM];
static Lin_ChannelConfigType Lin_ChannelConfigs[LIN_CHANNEL_NUM] = {
  {
    &Lin_ChannelContexts[0],
#ifndef USE_PORT
    /* CtrlPins */ NULL,
#endif
    /* baudrate */ 500000,
    /* hwInstanceId */ 0,
#ifndef USE_PORT
    /* numOfCtrlPins */ 0,
#endif
    "lin/simulator_v2/0",
  },
  {
    &Lin_ChannelContexts[0],
#ifndef USE_PORT
    /* CtrlPins */ NULL,
#endif
    /* baudrate */ 500000,
    /* hwInstanceId */ 1,
#ifndef USE_PORT
    /* numOfCtrlPins */ 0,
#endif
    "lin/simulator_v2/1",
  },
  {
    &Lin_ChannelContexts[2],
#ifndef USE_PORT
    /* CtrlPins */ NULL,
#endif
    /* baudrate */ 500000,
    /* hwInstanceId */ 2,
#ifndef USE_PORT
    /* numOfCtrlPins */ 0,
#endif
    "lin/simulator_v2/2",
  },
  {
    &Lin_ChannelContexts[3],
#ifndef USE_PORT
    /* CtrlPins */ NULL,
#endif
    /* baudrate */ 500000,
    /* hwInstanceId */ 3,
#ifndef USE_PORT
    /* numOfCtrlPins */ 0,
#endif
    "lin/simulator_v2/3",
  },
};

static uint8_t Lin_hwIns2ChlMap[LIN_CHANNEL_NUM] = {0, 1, 2, 3};

Lin_ConfigType Lin_Config = {
  Lin_ChannelConfigs,
  Lin_hwIns2ChlMap,
  ARRAY_SIZE(Lin_ChannelConfigs),
  ARRAY_SIZE(Lin_hwIns2ChlMap),
};

/* ================================ [ LOCALS    ] ============================================== */
INITIALIZER(init_lin_cfg) {
  int i;
  Lin_ChannelConfigType *config;
  for (i = 0; i < Lin_Config.numOfChannels; i++) {
    config = (Lin_ChannelConfigType *)&Lin_Config.channelConfigs[i];
    config->context = &Lin_ChannelContexts[i];
    config->hwInstanceId = i;
    config->context->acCtx.fd = -1;
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Lin_ReConfig(uint8_t Controller, const char *device, int port, uint32_t baudrate) {
  Lin_ChannelConfigType *config;
  if (Controller < Lin_Config.numOfChannels) {
    config = (Lin_ChannelConfigType *)&Lin_Config.channelConfigs[Controller];
    snprintf(config->device, sizeof(config->device), "lin/%s/%d", device, port);
    config->baudrate = baudrate;
  }
}
