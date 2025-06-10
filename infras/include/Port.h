/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Port Driver AUTOSAR CP Release 4.4.0
 */
#ifndef _PORT_H
#define _PORT_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#define PORT_PIN_IN ((Port_PinDirectionType)0x00)
#define PORT_PIN_OUT ((Port_PinDirectionType)0x01)
#define PORT_PIN_UNUSED ((Port_PinDirectionType)0x02) /* A mode that don't set the pin direction */

#define PORT_PIN_MODE_ADC ((Port_PinModeType)0)
#define PORT_PIN_MODE_CAN ((Port_PinModeType)1)
#define PORT_PIN_MODE_DIO ((Port_PinModeType)2)
#define PORT_PIN_MODE_DIO_GPT ((Port_PinModeType)3)
#define PORT_PIN_MODE_DIO_WDG ((Port_PinModeType)4)
#define PORT_PIN_MODE_FLEXRAY ((Port_PinModeType)5)
#define PORT_PIN_MODE_ICU ((Port_PinModeType)6)
#define PORT_PIN_MODE_LIN ((Port_PinModeType)7)
#define PORT_PIN_MODE_MEM ((Port_PinModeType)8)
#define PORT_PIN_MODE_PWM ((Port_PinModeType)9)
#define PORT_PIN_MODE_SPI ((Port_PinModeType)10)

/* @SWS_Port_00051 */
#define PORT_E_PARAM_PIN 0x0A
#define PORT_E_DIRECTION_UNCHANGEABLE 0x0B
#define PORT_E_INIT_FAILED 0x0C
#define PORT_E_PARAM_INVALID_MODE 0x0D
#define PORT_E_MODE_UNCHANGEABLE 0x0E
#define PORT_E_UNINIT 0x0F
#define PORT_E_PARAM_POINTER 0x10
/* ================================ [ TYPES     ] ============================================== */
typedef struct Port_Config_s Port_ConfigType;

/* @SWS_Port_00229 */
typedef uint16_t Port_PinType;

/* @SWS_Port_00230 */
typedef uint8_t Port_PinDirectionType;

/* @SWS_Port_00231 */
typedef uint8_t Port_PinModeType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_Port_00140 */
void Port_Init(const Port_ConfigType *ConfigPtr);

/* @SWS_Port_00141 */
void Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction);

/* @SWS_Port_00142 */
void Port_RefreshPortDirection(void);

/* @SWS_Port_00145 */
void Port_SetPinMode(Port_PinType Pin, Port_PinModeType Mode);
#endif /* _PORT_H */
