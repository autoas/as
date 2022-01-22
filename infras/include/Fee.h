/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Flash EEPROM Emulation AUTOSAR CP Release 4.4.0
 */
#ifndef FEE_H
#define FEE_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "MemIf.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef struct Fee_Config_s Fee_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Fee_Init(const Fee_ConfigType *ConfigPtr);
void Fee_SetMode(MemIf_ModeType Mode);
Std_ReturnType Fee_Read(uint16_t BlockNumber, uint16_t BlockOffset, uint8_t *DataBufferPtr,
                        uint16_t Length);
Std_ReturnType Fee_Write(uint16_t BlockNumber, const uint8_t *DataBufferPtr);
void Fee_Cancel(void);
MemIf_StatusType Fee_GetStatus(void);
MemIf_JobResultType Fee_GetJobResult(void);
Std_ReturnType Fee_InvalidateBlock(uint16_t BlockNumber);
Std_ReturnType Fee_EraseImmediateBlock(uint16_t BlockNumber);

void Fee_JobEndNotification(void);
void Fee_JobErrorNotification(void);

void Fee_MainFunction(void);
#ifdef __cplusplus
}
#endif
#endif /* FEE_H */
