/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef LIN_LCFG_H
#define LIN_LCFG_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Lin.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef LIN_CHANNEL_NUM
#define LIN_CHANNEL_NUM 4
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  char device[64];
  uint32_t baudrate;
} Lin_ChannelConfigType;

struct Lin_Config_s {
  char logPath[128];
  Lin_ChannelConfigType *channelConfigs;
  uint8_t numOfChannels;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */

#endif /* LIN_LCFG_H */