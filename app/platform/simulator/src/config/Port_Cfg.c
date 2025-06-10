/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Port.h"
#include "Port_Cfg.h"
#include "Port_Priv.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static const Port_PortPinConfigType Port_PortPinConfigs[] = {
  PORT_PIN_DEF(CAN, IN, LOW, PORT_PORT_A, 0, 0),
  PORT_PIN_DEF(CAN, OUT, HIGH, PORT_PORT_A, 1, 0),
  PORT_PIN_DEF(CAN, OUT, HIGH, PORT_PORT_A, 2, 0),
  PORT_PIN_DEF(DIO, OUT, HIGH, PORT_PORT_A, 3, 0),
};

const Port_ConfigType Port_Config = {
  Port_PortPinConfigs,
  ARRAY_SIZE(Port_PortPinConfigs),
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
