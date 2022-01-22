/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Port Driver AUTOSAR CP Release 4.4.0
 */
#ifndef _PORT_H
#define _PORT_H
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#define PORT_PIN_IN ((Port_PinDirectionType)0x00)
#define PORT_PIN_OUT ((Port_PinDirectionType)0x01)
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
