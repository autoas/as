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
#define NVM_REQ_OK ((NvM_RequestResultType)0x00)
#define NVM_REQ_NOT_OK ((NvM_RequestResultType)0x01)
#define NVM_REQ_PENDING ((NvM_RequestResultType)0x02)
#define NVM_REQ_INTEGRITY_FAILED ((NvM_RequestResultType)0x03)
#define NVM_REQ_BLOCK_SKIPPED ((NvM_RequestResultType)0x04)
#define NVM_REQ_NV_INVALIDATED ((NvM_RequestResultType)0x05)
#define NVM_REQ_CANCELED ((NvM_RequestResultType)0x06)
#define NVM_REQ_RESTORED_DEFAULTS ((NvM_RequestResultType)0x07)

/* @SWS_NvM_91004 */
#define NVM_E_PARAM_BLOCK_ID 0x0A
#define NVM_E_PARAM_BLOCK_DATA_IDX 0x0C
#define NVM_E_PARAM_ADDRESS 0x0D
#define NVM_E_PARAM_DATA 0x0E
#define NVM_E_PARAM_POINTER 0x0F
#define NVM_E_BLOCK_WITHOUT_DEFAULTS 0x11
#define NVM_E_UNINIT 0x14
#define NVM_E_BLOCK_PENDING 0x15
#define NVM_E_BLOCK_CONFIG 0x18
#define NVM_E_WRITE_ONCE_STATUS_UNKNOWN 0x1A
#define NVM_E_BLOCK_CHIPHER_LENGTH_MISSMATCH 0x1B
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_NvM_00471 */
typedef uint16_t NvM_BlockIdType;

/* @SWS_NvM_00470 */
typedef uint8_t NvM_RequestResultType;

typedef struct NvM_Config_s NvM_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_NvM_00447 */
void NvM_Init(const NvM_ConfigType *ConfigPtr);

Std_ReturnType NvM_GetBlockDataPtrAndLength(NvM_BlockIdType BlockId, void **pDataPtr,
                                            uint16_t *pLength);

/* @SWS_NvM_00451 */
Std_ReturnType NvM_GetErrorStatus(NvM_BlockIdType BlockId, NvM_RequestResultType *RequestResultPtr);

/* @SWS_NvM_00453 */
Std_ReturnType NvM_SetRamBlockStatus(NvM_BlockIdType BlockId, boolean BlockChanged);

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

/* @SWS_NvM_00452 */
void NvM_GetVersionInfo(Std_VersionInfoType *versionInfo);
#endif /* NVM_H */
