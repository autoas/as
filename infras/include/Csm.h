/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Crypto Service Manager AUTOSAR CP R20-11
 */
#ifndef CSM_H
#define CSM_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "Crypto_GeneralTypes.h"
/* ================================ [ MACROS    ] ============================================== */
/* @SWS_Csm_91004 */
#define CSM_E_PARAM_POINTER 0x01u
#define CSM_E_PARAM_HANDLE 0x04u
#define CSM_E_UNINIT 0x05u
#define CSM_E_INIT_FAILED 0x07
#define CSM_E_PROCESSING_MODE 0x08u
#define CSM_E_SERVICE_TYPE 0x09
/* ================================ [ TYPES     ] ============================================== */
typedef struct Csm_Config_s Csm_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_Csm_00646 */
void Csm_Init(const Csm_ConfigType *configPtr);

/* @SWS_Csm_00982 */
Std_ReturnType Csm_MacGenerate(uint32_t jobId, Crypto_OperationModeType mode,
                               const uint8_t *dataPtr, uint32_t dataLength, uint8_t *macPtr,
                               uint32_t *macLengthPtr);

/* @SWS_Csm_01050 */
Std_ReturnType Csm_MacVerify(uint32_t jobId, Crypto_OperationModeType mode, const uint8_t *dataPtr,
                             uint32_t dataLength, const uint8_t *macPtr, const uint32_t macLength,
                             Crypto_VerifyResultType *verifyPtr);
#endif /* CSM_H */
