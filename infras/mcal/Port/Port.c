/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Port Driver AUTOSAR CP R22-11
 */
#ifdef USE_PORT
/* ================================ [ INCLUDES  ] ============================================== */
#include "Port.h"
#include "Port_Cfg.h"
#include "Port_Priv.h"
#include "Det.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_PORT 0
#define AS_LOG_PORTI 0
#define AS_LOG_PORTE 3

#define PORT_CONFIG (&Port_Config)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const Port_ConfigType Port_Config;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ ALIAS     ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Port_Init(const Port_ConfigType *Config) {
  uint16_t i;
  for (i = 0; i < PORT_CONFIG->numOfPortPins; i++) {
    PortAc_PinInit(i, &PORT_CONFIG->portPinConfigs[i]);
  }
}
#endif
