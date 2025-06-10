/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Port Driver AUTOSAR CP R22-11
 */
#ifndef PORT_PRIV_H
#define PORT_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define DET_THIS_MODULE_ID MODULE_ID_PORT

#define PORT_PIN_LEVEL_HIGH ((Port_PinLevelType)STD_HIGH)
#define PORT_PIN_LEVEL_LOW ((Port_PinLevelType)STD_LOW)

#define PORT_PIN_DEF(pinMode, pinDirection, pinLevel, port, pin, mode)                             \
  {                                                                                                \
    PORT_PIN_MODE_##pinMode, PORT_PIN_##pinDirection, PORT_PIN_LEVEL_##pinLevel, (uint8_t)port,    \
      (uint8_t)pin, (uint8_t)mode                                                                  \
  }
/* ================================ [ TYPES     ] ============================================== */
typedef uint8_t Port_PinLevelType;

typedef struct {
  Port_PinModeType pinMode; /* this may not used */
  Port_PinDirectionType pinDirection;
  Port_PinLevelType pinLevel;
  uint8_t port; /* the acutal Port Id */
  uint8_t pin;  /* the acutal Pin Id of the Port */
  uint8_t mode; /* the actual pin mode according to the Port MUX register definition */
} Port_PortPinConfigType;

struct Port_Config_s {
  const Port_PortPinConfigType *portPinConfigs;
  uint16_t numOfPortPins;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ ALIAS     ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void PortAc_PinInit(Port_PinType pinId, const Port_PortPinConfigType *config);
#ifdef __cplusplus
}
#endif
#endif /* PORT_PRIV_H */
