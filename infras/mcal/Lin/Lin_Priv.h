/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of LIN Driver AUTOSAR CP R23-11
 */
#ifndef LIN_PRIV_H
#define LIN_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "Lin.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define DET_THIS_MODULE_ID MODULE_ID_LIN

#define LIN_PIN_VALUE_NOT_USED 0xFF
/* ================================ [ TYPES     ] ============================================== */
/* void*: yes, Lin be use to hold either port register base address or a port ID.
 * Why not use Port and Dio, it was because of that if has Port or Dio, generally this was not
 * necessary here.  */
typedef void *Lin_CtrlPinPortType;
typedef void *Lin_CtrlPinIoType;

typedef struct {
  Lin_CtrlPinPortType Port; /* this identify to a Port: through which to set Pin Mode */
  Lin_CtrlPinIoType Pio; /* this identify to a GPIO PIN: through which to set Pin direction and get
                            input value or set output value  */
  uint16_t Pin;
  uint8_t Mode;
  /* Value: set to LIN_PIN_VALUE_NOT_USED if this is not a IO output PIN based on the Mode, else
   * STD_HIGH or STD_LOW */
  uint8_t Value;
} Lin_CtrlPinType;

typedef struct {
  Lin_ControllerStateType state;
#ifdef LIN_USE_CTRL_AC_CONTEXT_TYPE /* type Lin_CtrlAcContextType defined in Lin_Cfg.h */
  Lin_CtrlAcContextType acCtx;      /* hardware controller access required context type */
#endif
} Lin_ChannelContextType;

typedef struct {
  Lin_ChannelContextType *context;
#ifndef USE_PORT
  const Lin_CtrlPinType *CtrlPins;
#endif
#ifdef LIN_USE_CTRL_AC_CONFIG_TYPE /* type Lin_CtrlAcConfigType defined in Lin_Cfg.h */
  Lin_CtrlAcConfigType acCfg;      /* hardware controller access required extra config type */
#endif
  uint32_t baudrate;
  uint8_t hwInstanceId; /* the actual Lin hardware instance ID */
#ifndef USE_PORT
  uint8_t numOfCtrlPins;
#endif
#if defined(linux) || defined(_WIN32)
  char device[32];
#endif
} Lin_ChannelConfigType;

struct Lin_Config_s {
  const Lin_ChannelConfigType *channelConfigs;
  /* see a ECU has 4 Lin controller CTRL0, CTRL1, CTRL2, CTRL3, but the product use CTRL2 as LIN0
   * and CTRL1 as LIN1, so the map is:
   * hwIns2ChlMap[4] = { LIN_CTRL_NOT_USED, LIN_CHL_LIN1, LIN_CHL_LIN0, LIN_CTRL_NOT_USED } */
  const uint8_t *hwIns2ChlMap;
  uint8_t numOfChannels;
  uint8_t sizeOfhwIns2ChlMap;
};
/* ================================ [ DECLARES  ] ============================================== */
Std_ReturnType LinAc_Init(uint8_t Controller, const Lin_ChannelConfigType *config);
Std_ReturnType LinAc_DeInit(uint8_t Controller, const Lin_ChannelConfigType *config);
Std_ReturnType LinAc_SetSleepMode(uint8_t Controller, const Lin_ChannelConfigType *config);

Std_ReturnType LinAc_SetupPinMode(const Lin_CtrlPinType *pin);

Std_ReturnType LinAc_WritePin(const Lin_CtrlPinType *pin, uint8_t value);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ ALIAS     ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#ifdef __cplusplus
}
#endif
#endif /* LIN_PRIV_H */
