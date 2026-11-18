/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of DIO Driver AUTOSAR CP R19-11
 */
#ifdef USE_DIO
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dio.h"
#include "Dio_Cfg.h"
#include "Dio_Priv.h"
#include "Det.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DIO 0
#define AS_LOG_DIOI 0
#define AS_LOG_DIOE 3

#define DIO_CONFIG (&Dio_Config)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const Dio_ConfigType Dio_Config;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ ALIAS     ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Dio_WriteChannel(Dio_ChannelType channelId, Dio_LevelType level) {
  DET_VALIDATE(channelId < DIO_CONFIG->numOfChannels, 0x01, DIO_E_PARAM_INVALID_CHANNEL_ID, return);
  DET_VALIDATE((STD_LOW == level) || (STD_HIGH == level), 0x01, DIO_E_PARAM_POINTER, return);
  DioAc_WriteChannel(channelId, &DIO_CONFIG->channels[channelId], level);
}

Dio_LevelType Dio_ReadChannel(Dio_ChannelType channelId) {
  DET_VALIDATE(channelId < DIO_CONFIG->numOfChannels, 0x00, DIO_E_PARAM_INVALID_CHANNEL_ID,
               return STD_LOW);
  return DioAc_ReadChannel(channelId, &DIO_CONFIG->channels[channelId]);
}
#endif
