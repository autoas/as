/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of EEPROM Abstraction AUTOSAR CP Release 4.4.0
 */
#ifndef EA_H
#define EA_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "MemIf.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* @SWS_Ea_00196 */
#define EA_E_UNINIT 0x01
#define EA_E_INVALID_BLOCK_NO 0x02
#define EA_E_INVALID_BLOCK_OFS 0x03
#define EA_E_PARAM_POINTER 0x04
#define EA_E_INVALID_BLOCK_LEN 0x05
#define EA_E_INIT_FAILED 0x09
/* ================================ [ TYPES     ] ============================================== */
typedef struct Ea_Config_s Ea_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Ea_Init(const Ea_ConfigType *ConfigPtr);
void Ea_SetMode(MemIf_ModeType Mode);
Std_ReturnType Ea_Read(uint16_t BlockNumber, uint16_t BlockOffset, uint8_t *DataBufferPtr,
                       uint16_t Length);
Std_ReturnType Ea_Write(uint16_t BlockNumber, const uint8_t *DataBufferPtr);
void Ea_Cancel(void);
MemIf_StatusType Ea_GetStatus(void);
MemIf_JobResultType Ea_GetJobResult(void);
Std_ReturnType Ea_InvalidateBlock(uint16_t BlockNumber);
Std_ReturnType Ea_EraseImmediateBlock(uint16_t BlockNumber);

void Ea_JobEndNotification(void);
void Ea_JobErrorNotification(void);

void Ea_MainFunction(void);

/* @SWS_Ea_00092 */
void Ea_GetVersionInfo(Std_VersionInfoType *versionInfo);
#ifdef __cplusplus
}
#endif
#endif /* EA_H */
