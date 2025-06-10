/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 */
#ifdef USE_LIN
/* ================================ [ INCLUDES  ] ============================================== */
#define STD_NO_ERRNO_H
#include "Lin.h"
#include "Lin_Cfg.h"
#include "Lin_Priv.h"
#include "Det.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_LIN 0
#define AS_LOG_LINI 0
#define AS_LOG_LINE 3

#define LIN_CONFIG (&Lin_Config)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const Lin_ConfigType Lin_Config;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ ALIAS     ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Lin_Init(const Lin_ConfigType *Config) {
  int i;
  const Lin_ChannelConfigType *config;
  for (i = 0; i < LIN_CONFIG->numOfChannels; i++) {
    config = &LIN_CONFIG->channelConfigs[i];
    config->context->state = LIN_CS_STOPPED;
  }
}

Std_ReturnType Lin_GoToSleep(uint8_t Channel) {
  Std_ReturnType ret;

  DET_VALIDATE(Channel < LIN_CONFIG->numOfChannels, 0x06, LIN_E_INVALID_CHANNEL, return E_NOT_OK);

  /* goto sleep command */
  static const uint8_t data[8] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  static const Lin_PduType PduInfo = {(uint8_t *)data, 0x3C, LIN_CLASSIC_CS, LIN_FRAMERESPONSE_TX,
                                      sizeof(data)};

  ret = Lin_SendFrame(Channel, &PduInfo);

  return ret;
}

Std_ReturnType Lin_Wakeup(uint8_t Channel) {
  Std_ReturnType ret = E_OK;
  const Lin_ChannelConfigType *config;
#ifndef USE_PORT
  int i = 0;
#endif

  DET_VALIDATE(Channel < LIN_CONFIG->numOfChannels, 0x07, LIN_E_INVALID_CHANNEL, return E_NOT_OK);

  config = &LIN_CONFIG->channelConfigs[Channel];
  if (LIN_CS_STARTED != config->context->state) {
#ifndef USE_PORT
    for (i = 0; i < config->numOfCtrlPins; i++) {
      ret = LinAc_SetupPinMode(&config->CtrlPins[i]);
      if (E_OK == ret) {
        if (config->CtrlPins[i].Value != LIN_PIN_VALUE_NOT_USED) {
          ret = LinAc_WritePin(&config->CtrlPins[i], config->CtrlPins[i].Value);
        }
      }
      if (E_OK != ret) {
        ASLOG(LINE, ("[%" PRIu8 "] Failed to set PIN %d mode\n", Channel, i));
        break;
      }
    }
    if (E_OK == ret) {
#endif
      ret = LinAc_Init(Channel, config);
#ifndef USE_PORT
    }
#endif
    if (E_OK == ret) {
      config->context->state = LIN_CS_STARTED;
    } else {
      ASLOG(LINE, ("[%" PRIu8 "] Failed to start LIN\n", Channel));
    }
  }

  return ret;
}

Std_ReturnType Lin_GoToSleepInternal(uint8_t Channel) {
  Std_ReturnType ret = E_OK;
  const Lin_ChannelConfigType *config;

  DET_VALIDATE(Channel < LIN_CONFIG->numOfChannels, 0x09, LIN_E_INVALID_CHANNEL, return E_NOT_OK);

  config = &LIN_CONFIG->channelConfigs[Channel];
  if (LIN_CS_SLEEP != config->context->state) {
    ret = LinAc_SetSleepMode(Channel, config);
    if (E_OK == ret) {
      config->context->state = LIN_CS_SLEEP;
    } else {
      ASLOG(LINE, ("[%" PRIu8 "] Failed to sleep LIN\n", Channel));
    }
  }

  return ret;
}

void LIN_DeInit(void) {
}
#endif
