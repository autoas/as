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
void Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level) {
  DET_VALIDATE(ChannelId < DIO_CONFIG->numOfChannels, 0x01, DIO_E_PARAM_INVALID_CHANNEL_ID, return);
  DET_VALIDATE((STD_LOW == Level) || (STD_HIGH == Level), 0x01, DIO_E_PARAM_POINTER, return);
  DioAc_WriteChannel(ChannelId, &DIO_CONFIG->channels[ChannelId], Level);
}

Dio_LevelType Dio_ReadChannel(Dio_ChannelType ChannelId) {
  DET_VALIDATE(ChannelId < DIO_CONFIG->numOfChannels, 0x00, DIO_E_PARAM_INVALID_CHANNEL_ID,
               return STD_LOW);
  return DioAc_ReadChannel(ChannelId, &DIO_CONFIG->channels[ChannelId]);
}
#endif
