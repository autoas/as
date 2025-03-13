/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef USE_THIRD_PARTY_CAN
/* ================================ [ INCLUDES  ] ============================================== */
#include "Can.h"
#include "Can_Cfg.h"
#include "Can_Priv.h"
#include "Det.h"
#include "Std_Debug.h"
#include "shell.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_CAN 0
#define AS_LOG_CANI 0
#define AS_LOG_CANE 3

#define CAN_CONFIG (&Can_Config)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const Can_ConfigType Can_Config;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
#ifdef USE_SHELL
static int LsCanFunc(int argc, const char *argv[]) {
  int i;
  const Can_ChannelConfigType *config;
  for (i = 0; i < CAN_CONFIG->numOfChannels; i++) {
    config = &CAN_CONFIG->channelConfigs[i];
    if (CAN_CS_STARTED == config->context->state) {
      PRINTF("  CAN %d: online\n", i);
    }
  }
  return 0;
}
SHELL_REGISTER(lscan, "list can status\n", LsCanFunc);
#endif
/* ================================ [ ALIAS     ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Can_Init(const Can_ConfigType *Config) {
  int i;
  const Can_ChannelConfigType *config;
  for (i = 0; i < CAN_CONFIG->numOfChannels; i++) {
    config = &CAN_CONFIG->channelConfigs[i];
    config->context->state = CAN_CS_STOPPED;
    config->context->trcvMode = CANTRCV_TRCVMODE_SLEEP;
  }
#ifdef CAN_USE_CTRL_AC_GLOBAL
  CanAc_GlobalInit(CAN_CONFIG);
#endif
}

Std_ReturnType Can_SetControllerMode(uint8_t Controller, Can_ControllerStateType Transition) {
  Std_ReturnType ret = E_OK;
  const Can_ChannelConfigType *config;
#ifndef USE_PORT
  int i = 0;
#endif
  DET_VALIDATE(Controller < CAN_CONFIG->numOfChannels, 0x03, CAN_E_PARAM_CONTROLLER,
               return E_NOT_OK);
  DET_VALIDATE((Transition >= CAN_CS_STARTED) && (Transition <= CAN_CS_SLEEP), 0x03,
               CAN_E_TRANSITION, return E_NOT_OK);

  config = &CAN_CONFIG->channelConfigs[Controller];
  switch (Transition) {
  case CAN_CS_STARTED:
    if (CAN_CS_STARTED != config->context->state) {
#ifndef USE_PORT
      for (i = 0; i < config->numOfCtrlPins; i++) {
        ret = CanAc_SetupPinMode(&config->CtrlPins[i]);
        if (E_OK == ret) {
          if (config->CtrlPins[i].Value != CAN_PIN_VALUE_NOT_USED) {
            ret = CanAc_WritePin(&config->CtrlPins[i], config->CtrlPins[i].Value);
          }
        }
        if (E_OK != ret) {
          ASLOG(CANE, ("[%" PRIu8 "] Failed to set PIN %d mode\n", Controller, i));
          break;
        }
      }
      if (E_OK == ret) {
#endif
        ret = CanAc_Init(Controller, config);
#ifndef USE_PORT
      }
#endif
      if (E_OK == ret) {
        config->context->state = CAN_CS_STARTED;
      } else {
        ASLOG(CANE, ("[%" PRIu8 "] Failed to start CAN\n", Controller));
      }
    }
    break;
  case CAN_CS_STOPPED:
    if (CAN_CS_STOPPED != config->context->state) {
      ret = CanAc_DeInit(Controller, config);
      if (E_OK == ret) {
        config->context->state = CAN_CS_STOPPED;
      } else {
        ASLOG(CANE, ("[%" PRIu8 "] Failed to stop CAN\n", Controller));
      }
    }
    break;
  case CAN_CS_SLEEP:
    if (CAN_CS_SLEEP != config->context->state) {
      ret = CanAc_SetSleepMode(Controller, config);
      if (E_OK == ret) {
        config->context->state = CAN_CS_SLEEP;
      } else {
        ASLOG(CANE, ("[%" PRIu8 "] Failed to sleep CAN\n", Controller));
      }
    }
    break;
  default:
    ret = E_NOT_OK;
    break;
  }

  return ret;
}

Std_ReturnType CanIf_SetTrcvMode(uint8_t TransceiverId, CanTrcv_TrcvModeType TransceiverMode) {
  Std_ReturnType ret = E_OK;
  const Can_ChannelConfigType *config;

  DET_VALIDATE(TransceiverId < CAN_CONFIG->numOfChannels, 0xF1, CAN_E_PARAM_CONTROLLER,
               return E_NOT_OK);
  DET_VALIDATE(TransceiverMode <= CANTRCV_TRCVMODE_NORMAL, 0xF1, CAN_E_TRANSITION, return E_NOT_OK);

  config = &CAN_CONFIG->channelConfigs[TransceiverId];
#ifndef USE_PORT
  if (config->TrcvPinSTB < config->numOfCtrlPins) {
    if (CANTRCV_TRCVMODE_NORMAL == TransceiverMode) {
      ret = CanAc_WritePin(&config->CtrlPins[config->TrcvPinSTB], config->NormalValueOfTrcvPinSTB);
    } else {
      ret = CanAc_WritePin(&config->CtrlPins[config->TrcvPinSTB],
                           (~config->NormalValueOfTrcvPinSTB) & STD_HIGH);
    }
  }
#else
  if (DIO_INVALID_CHANNEL != config->TrcvPinSTB) {
    if (CANTRCV_TRCVMODE_NORMAL == TransceiverMode) {
      Dio_WriteChannel(config->TrcvPinSTB, config->NormalValueOfTrcvPinSTB);
    } else {
      Dio_WriteChannel(config->TrcvPinSTB, (~config->NormalValueOfTrcvPinSTB) & STD_HIGH);
    }
  }
#endif
  else {
    /* no standby pin configued, assume OK */
  }

  if (E_OK == ret) {
    config->context->trcvMode = TransceiverMode;
  }

  return ret;
}

Std_ReturnType CanIf_GetTrcvMode(uint8_t TransceiverId, CanTrcv_TrcvModeType *TransceiverModePtr) {
  Std_ReturnType ret = E_OK;
  const Can_ChannelConfigType *config;

  DET_VALIDATE(TransceiverId < CAN_CONFIG->numOfChannels, 0xF2, CAN_E_PARAM_CONTROLLER,
               return E_NOT_OK);
  DET_VALIDATE(NULL != TransceiverModePtr, 0xF2, CAN_E_PARAM_POINTER, return E_NOT_OK);

  config = &CAN_CONFIG->channelConfigs[TransceiverId];
  *TransceiverModePtr = config->context->trcvMode;

  return ret;
}

Std_ReturnType Can_GetControllerMode(uint8_t Controller,
                                     Can_ControllerStateType *ControllerModePtr) {
  Std_ReturnType ret = E_OK;
  const Can_ChannelConfigType *config;

  DET_VALIDATE(Controller < CAN_CONFIG->numOfChannels, 0x12, CAN_E_PARAM_CONTROLLER,
               return E_NOT_OK);
  DET_VALIDATE(NULL != ControllerModePtr, 0x12, CAN_E_PARAM_POINTER, return E_NOT_OK);

  config = &CAN_CONFIG->channelConfigs[Controller];
  *ControllerModePtr = config->context->state;

  return ret;
}

void Can_DeInit(void) {
#ifdef CAN_USE_CTRL_AC_GLOBAL
  CanAc_GlobalDeInit(CAN_CONFIG);
#endif
}
#else
void CanAc_ExtraInit(void) {
}
#endif
