/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: https://www.autosar.org/fileadmin/user_upload/standards/classic/4-3/AUTOSAR_SWS_MCUDriver.pdf
 */
#ifndef MCU_H
#define MCU_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef struct Mcu_Config_s Mcu_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_Mcu_00153 */
void Mcu_Init(const Mcu_ConfigType *ConfigPtr);
#endif /* MCU_H */
