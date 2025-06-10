/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dio.h"
#include "Dio_Cfg.h"
#include "Dio_Priv.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static uint32_t lPortValue[DIO_MAX_SIM_CHANNELS / 32];
/* ================================ [ FUNCTIONS ] ============================================== */
Dio_LevelType DioAc_ReadChannel(Dio_ChannelType ChannelId, const Dio_ChannelConfigType *config) {
  int port = ChannelId / 32;
  int pin = ChannelId % 32;

  return (lPortValue[port] >> pin) & 0x01;
}

void DioAc_WriteChannel(Dio_ChannelType ChannelId, const Dio_ChannelConfigType *config,
                        Dio_LevelType Level) {
  int port = ChannelId / 32;
  int pin = ChannelId % 32;

  if (STD_HIGH == Level) {
    lPortValue[port] |= ((uint32_t)1 << pin);
  } else {
    lPortValue[port] &= ~((uint32_t)1 << pin);
  }
}
