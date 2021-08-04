/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of NVRAM Manager AUTOSAR CP Release 4.4.0
 */
#ifndef NVM_H
#define NVM_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "MemIf.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_NvM_00471 */
typedef uint16_t NvM_BlockIdType;

typedef struct NvM_Config_s NvM_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_NvM_00447 */
void NvM_Init(const NvM_ConfigType *ConfigPtr);

/* @SWS_NvM_00454 */
Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void *NvM_DstPtr);

/* @SWS_NvM_00455 */
Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void *NvM_SrcPtr);

/* @SWS_NvM_00456 */
Std_ReturnType NvM_RestoreBlockDefaults(NvM_BlockIdType BlockId, void *NvM_DestPtr);

/* @SWS_NvM_00457 */
Std_ReturnType NvM_EraseNvBlock(NvM_BlockIdType BlockId);

/* @SWS_NvM_00459 */
Std_ReturnType NvM_InvalidateNvBlock(NvM_BlockIdType BlockId);

/* @SWS_NvM_00460 */
void NvM_ReadAll(void);

/* @SWS_NvM_00461 */
void NvM_WriteAll(void);

/* @SWS_NvM_00458 */
void NvM_CancelWriteAll(void);

/* @SWS_NvM_00855 */
void NvM_ValidateAll(void);

/* @SWS_NvM_91001 */
void NvM_FirstInitAll(void);

/* @SWS_NvM_00462 */
void NvM_JobEndNotification(void);

/* @SWS_NvM_00463 */
void NvM_JobErrorNotification(void);

/* @SWS_NvM_00464 */
void NvM_MainFunction(void);

MemIf_StatusType NvM_GetStatus(void);
#endif /* NVM_H */
