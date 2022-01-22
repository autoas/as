/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Memory Abstraction Interface AUTOSAR CP Release 4.4.0
 */
#ifndef MEMIF_H
#define MEMIF_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifdef MEMIF_ZERO_COST_FEE
#define MemIf_Read(DeviceIndex, BlockNumber, BlockOffset, DataBufferPtr, Length)                   \
  Fee_Read(BlockNumber, BlockOffset, DataBufferPtr, Length)
#define MemIf_Write(DeviceIndex, BlockNumber, DataBufferPtr) Fee_Write(BlockNumber, DataBufferPtr)
#define MemIf_GetStatus(DeviceIndex) Fee_GetStatus()
#define MemIf_Cancel(DeviceIndex) Fee_Cancel()
#endif

#ifdef MEMIF_ZERO_COST_EA
#define MemIf_Read(DeviceIndex, BlockNumber, BlockOffset, DataBufferPtr, Length)                   \
  Ea_Read(BlockNumber, BlockOffset, DataBufferPtr, Length)
#define MemIf_Write(DeviceIndex, BlockNumber, DataBufferPtr) Ea_Write(BlockNumber, DataBufferPtr)
#define MemIf_GetStatus(DeviceIndex) Ea_GetStatus()
#define MemIf_Cancel(DeviceIndex) Ea_Cancel()
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* @SWS_MemIf_00064 */
typedef enum
{
  MEMIF_UNINIT,
  MEMIF_IDLE,
  MEMIF_BUSY,
  MEMIF_BUSY_INTERNAL
} MemIf_StatusType;

/* @SWS_MemIf_00065 */
typedef enum
{
  MEMIF_JOB_OK,
  MEMIF_JOB_FAILED,
  MEMIF_JOB_PENDING,
  MEMIF_JOB_CANCELED,
  MEMIF_BLOCK_INCONSISTENT,
  MEMIF_BLOCK_INVALID
} MemIf_JobResultType;

/* @SWS_MemIf_00066 */
typedef enum
{
  MEMIF_MODE_SLOW,
  MEMIF_MODE_FAST
} MemIf_ModeType;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#ifdef MEMIF_ZERO_COST_FEE
#include "Fee.h"
#elif defined(MEMIF_ZERO_COST_EA)
#include "Ea.h"
#else
Std_ReturnType MemIf_Read(uint8_t DeviceIndex, uint16_t BlockNumber, uint16_t BlockOffset,
                          uint8_t *DataBufferPtr, uint16_t Length);
Std_ReturnType MemIf_Write(uint8_t DeviceIndex, uint16_t BlockNumber, const uint8_t *DataBufferPtr);
void MemIf_Cancel(uint8_t DeviceIndex);
MemIf_StatusType MemIf_GetStatus(uint8_t DeviceIndex);
MemIf_JobResultType MemIf_GetJobResult(uint8_t DeviceIndex);
Std_ReturnType MemIf_InvalidateBlock(uint8_t DeviceIndex, uint16_t BlockNumber);
Std_ReturnType MemIf_EraseImmediateBlock(uint8_t DeviceIndex, uint16_t BlockNumber);
#endif
#ifdef __cplusplus
}
#endif
#endif /* MEMIF_H */
