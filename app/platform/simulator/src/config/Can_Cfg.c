/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Can.h"
#include "Can_Cfg.h"
#include "Can_Priv.h"
#include "string.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static Can_ChannelContextType Can_ChannelContexts[64];
static Can_ChannelConfigType Can_ChannelConfigs[64] = {
  {
    &Can_ChannelContexts[0],
#ifndef USE_PORT
    /* CtrlPins */ NULL,
#endif
    /* baudrate */ 500000,
#ifdef USE_PORT
    DIO_INVALID_CHANNEL,
#endif
    /* samplePoint % */ 75,
    /* hwInstanceId */ 0,
#ifndef USE_PORT
    /* numOfCtrlPins */ 0,
    /* TrcvPinSTB */ 0,
#endif
    /* NormalValueOfTrcvPinSTB */ STD_LOW,
    /* device */ "simulator_v2",
  },
  {
    &Can_ChannelContexts[1],
#ifndef USE_PORT
    /* CtrlPins */ NULL,
#endif
    /* baudrate */ 500000,
#ifdef USE_PORT
    DIO_INVALID_CHANNEL,
#endif
    /* samplePoint % */ 75,
    /* hwInstanceId */ 1,
#ifndef USE_PORT
    /* numOfCtrlPins */ 0,
    /* TrcvPinSTB */ 0,
#endif
    /* NormalValueOfTrcvPinSTB */ STD_LOW,
    /* device */ "simulator_v2",
  },
  {
    &Can_ChannelContexts[2],
#ifndef USE_PORT
    /* CtrlPins */ NULL,
#endif
    /* baudrate */ 500000,
#ifdef USE_PORT
    DIO_INVALID_CHANNEL,
#endif
    /* samplePoint % */ 75,
    /* hwInstanceId */ 2,
#ifndef USE_PORT
    /* numOfCtrlPins */ 0,
    /* TrcvPinSTB */ 0,
#endif
    /* NormalValueOfTrcvPinSTB */ STD_LOW,
    /* device */ "simulator_v2",
  },
  {
    &Can_ChannelContexts[3],
#ifndef USE_PORT
    /* CtrlPins */ NULL,
#endif
    /* baudrate */ 500000,
#ifdef USE_PORT
    DIO_INVALID_CHANNEL,
#endif
    /* samplePoint % */ 75,
    /* hwInstanceId */ 3,
#ifndef USE_PORT
    /* numOfCtrlPins */ 0,
    /* TrcvPinSTB */ 0,
#endif
    /* NormalValueOfTrcvPinSTB */ STD_LOW,
    /* device */ "simulator_v2",
  },
};

static uint8_t Can_hwIns2ChlMap[64] = {
  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
  22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
  44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
};

Can_ConfigType Can_Config = {
  Can_ChannelConfigs,
  /* hwIns2ChlMap */ Can_hwIns2ChlMap,
  ARRAY_SIZE(Can_ChannelConfigs),
  ARRAY_SIZE(Can_hwIns2ChlMap),
};
/* ================================ [ LOCALS    ] ============================================== */
INITIALIZER(init_can_cfg) {
  int i;
  Can_ChannelConfigType *config;
  for (i = 0; i < Can_Config.numOfChannels; i++) {
    config = (Can_ChannelConfigType *)&Can_Config.channelConfigs[i];
    config->context = &Can_ChannelContexts[i];
    config->hwInstanceId = i;
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Can_ReConfig(uint8_t Controller, const char *device, int port, uint32_t baudrate) {
  Can_ChannelConfigType *config;
  if (Controller < Can_Config.numOfChannels) {
    config = (Can_ChannelConfigType *)&Can_Config.channelConfigs[Controller];
    strncpy(config->device, device, sizeof(config->device));
    config->hwInstanceId = port;
    config->baudrate = baudrate;
    config->context = &Can_ChannelContexts[Controller];
    Can_hwIns2ChlMap[port] = Controller;
  }
}
