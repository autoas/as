/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Driver AUTOSAR CP R23-11
 */
#ifndef CAN_PRIV_H
#define CAN_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"
#include "Dio.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define DET_THIS_MODULE_ID MODULE_ID_CAN

#define CAN_PIN_VALUE_NOT_USED 0xFF
/* ================================ [ TYPES     ] ============================================== */
/* void*: yes, can be use to hold either port register base address or a port ID.
 * Why not use Port and Dio, it was because of that if has Port or Dio, generally this was not
 * necessary here.  */
typedef void *Can_CtrlPinPortType;
typedef void *Can_CtrlPinIoType;

typedef struct {
  Can_CtrlPinPortType Port; /* this identify to a Port: through which to set Pin Mode */
  Can_CtrlPinIoType Pio; /* this identify to a GPIO PIN: through which to set Pin direction and get
                            input value or set output value  */
  uint16_t Pin;
  uint8_t Mode;
  /* Value: set to CAN_PIN_VALUE_NOT_USED if this is not a IO output PIN based on the Mode, else
   * STD_HIGH or STD_LOW */
  uint8_t Value;
} Can_CtrlPinType;

typedef struct {
  CanTrcv_TrcvModeType trcvMode;
  Can_ControllerStateType state;
#ifdef CAN_USE_CTRL_AC_CONTEXT_TYPE /* type Can_CtrlAcContextType defined in Can_Cfg.h */
  Can_CtrlAcContextType acCtx;      /* hardware controller access required context type */
#endif
} Can_ChannelContextType;

typedef struct {
  Can_ChannelContextType *context;
#ifndef USE_PORT
  const Can_CtrlPinType *CtrlPins;
#endif
#ifdef CAN_USE_CTRL_AC_CONFIG_TYPE /* type Can_CtrlAcConfigType defined in Can_Cfg.h */
  const Can_CtrlAcConfigType *acCfg;     /* hardware controller access required extra config type */
#endif
  uint32_t baudrate;
#ifdef USE_PORT
  Dio_ChannelType TrcvPinSTB;
#endif
  uint8_t samplePoint;  /* uint 1% */
  uint8_t hwInstanceId; /* the actual CAN hardware instance ID */
#ifndef USE_PORT
  uint8_t numOfCtrlPins;
  /* Note: For now, only support a Transciver has 1 control Pin, but it was true that there is a
   * case, 2 or more Pins to control the Tranciver, and this is not supported for now. */
  uint8_t TrcvPinSTB; /* the Can transciver Standby PIN id of CtrlPins */
#endif
  Dio_LevelType NormalValueOfTrcvPinSTB; /* the PIN value to put the TrcvPinSTB to Normal state:
                                      STD_HIGH or STD_LOW  */
#if defined(linux) || defined(_WIN32)
  char device[32];
#endif
} Can_ChannelConfigType;

struct Can_Config_s {
  const Can_ChannelConfigType *channelConfigs;
  /* see a ECU has 4 CAN controller CTRL0, CTRL1, CTRL2, CTRL3, but the product use CTRL2 as CAN0
   * and CTRL1 as CAN1, so the map is:
   * hwIns2ChlMap[4] = { CAN_CTRL_NOT_USED, CAN_CHL_CAN1, CAN_CHL_CAN0, CAN_CTRL_NOT_USED } */
  const uint8_t *hwIns2ChlMap;
  uint8_t numOfChannels;
  uint8_t sizeOfhwIns2ChlMap;
};
/* ================================ [ DECLARES  ] ============================================== */
Std_ReturnType CanAc_GlobalInit(const Can_ConfigType *Config);
Std_ReturnType CanAc_GlobalDeInit(const Can_ConfigType *Config);

Std_ReturnType CanAc_Init(uint8_t Controller, const Can_ChannelConfigType *config);
Std_ReturnType CanAc_DeInit(uint8_t Controller, const Can_ChannelConfigType *config);
Std_ReturnType CanAc_SetSleepMode(uint8_t Controller, const Can_ChannelConfigType *config);

Std_ReturnType CanAc_SetupPinMode(const Can_CtrlPinType *pin);

Std_ReturnType CanAc_WritePin(const Can_CtrlPinType *pin, uint8_t value);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ ALIAS     ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#ifdef __cplusplus
}
#endif
#endif /* CAN_PRIV_H */
