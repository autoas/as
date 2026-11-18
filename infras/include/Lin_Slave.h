/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef LIN_SLAVE_H
#define LIN_SLAVE_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Lin.h"
/* ================================ [ MACROS    ] ============================================== */
/* for Lin Slave read write callback */
#define LIN_E_SLAVE_TRIGGER_TRANSMIT ((Std_ReturnType)0x10)
#define LIN_E_SLAVE_RECEIVED_OK ((Std_ReturnType)0x11)
#define LIN_E_SLAVE_TRANSMIT_OK ((Std_ReturnType)0x12)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Lin_Slave_Init(const Lin_ConfigType *config);
void Lin_Slave_MainFunction(void);
void Lin_Slave_MainFunction_Read(void);
#endif /* LIN_SLAVE_H */
