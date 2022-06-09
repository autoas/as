/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef CAN_LCFG_H
#define CAN_LCFG_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Can.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  char device[32];
  int port;
  uint32_t baudrate;
} Can_ChannelConfigType;

struct Can_Config_s {
  Can_ChannelConfigType *channelConfigs;
  uint8_t numOfChannels;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */

#endif /* CAN_LCFG_H */