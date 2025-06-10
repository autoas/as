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
static const Dio_ChannelConfigType Dio_ChannelConfigs[DIO_MAX_SIM_CHANNELS] = {{0}};
const Dio_ConfigType Dio_Config = {
  Dio_ChannelConfigs,
  ARRAY_SIZE(Dio_ChannelConfigs),
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
