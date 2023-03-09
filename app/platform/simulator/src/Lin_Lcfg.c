/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Lin_Lcfg.h"
#include <string.h>
#include <stdio.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Lin_ChannelConfigType Lin_ChannelConfigs[LIN_CHANNEL_NUM] = {
  {
    "lin/simulator/0",
    500000,
  },
  {
    "lin/simulator/1",
    500000,
  },
  {
    "lin/simulator/2",
    500000,
  },
  {
    "lin/simulator/3",
    500000,
  },
};

Lin_ConfigType Lin_Config = {
  ".lin.log",
  Lin_ChannelConfigs,
  ARRAY_SIZE(Lin_ChannelConfigs),
};

void Lin_ReConfig(uint8_t Controller, const char *device, int port, uint32_t baudrate) {
  Lin_ChannelConfigType *config;
  if (Controller < Lin_Config.numOfChannels) {
    config = &Lin_Config.channelConfigs[Controller];
    snprintf(config->device, sizeof(config->device), "lin/%s/%d", device, port);
    config->baudrate = baudrate;
  }
}
