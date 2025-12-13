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
/* @SWS_Fee_00010 */
#define FEE_E_UNINIT 0x01
#define FEE_E_INVALID_BLOCK_NO 0x02
#define FEE_E_INVALID_BLOCK_OFS 0x03
#define FEE_E_PARAM_POINTER 0x04
#define FEE_E_INVALID_BLOCK_LEN 0x05
#define FEE_E_INIT_FAILED 0x09
/* ================================ [ TYPES     ] ============================================== */
typedef struct Fee_Config_s Fee_ConfigType;

typedef struct {
  uint32_t erasedNumber;
  uint32_t adminFreeAddr;
  uint32_t dataFreeAddr;
  uint8_t curWrokingBank;
} Fee_AdminInfoType;
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

void Fee_GetAdminInfo(Fee_AdminInfoType *pAdminInfo);

void Fee_PanicUserAction(uint8_t fault);

/* @SWS_Fee_00093 */
void Fee_GetVersionInfo(Std_VersionInfoType *versionInfo);
#ifdef __cplusplus
}
#endif
#endif /* FEE_H */
