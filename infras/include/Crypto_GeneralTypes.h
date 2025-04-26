/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Crypto Service Manager AUTOSAR CP R20-11
 */
#ifndef CRYPTO_GENERAL_TYPES_H
#define CRYPTO_GENERAL_TYPES_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#define CRYPTO_ALGOFAM_SHA2_256 ((Crypto_AlgorithmFamilyType)0x03)
#define CRYPTO_ALGOFAM_AES ((Crypto_AlgorithmFamilyType)0x14)

/* Algorithm key is not set */
#define CRYPTO_ALGOMODE_NOT_SET ((Crypto_AlgorithmModeType)0x00)
/* Blockmode: Electronic Code Book */
#define CRYPTO_ALGOMODE_ECB ((Crypto_AlgorithmModeType)0x01)
/* Blockmode: Cipher Block Chaining */
#define CRYPTO_ALGOMODE_CBC ((Crypto_AlgorithmModeType)0x02)
/* Blockmode: Cipher Feedback Mode */
#define CRYPTO_ALGOMODE_CFB ((Crypto_AlgorithmModeType)0x03)
/* Hashed-based MAC */
#define CRYPTO_ALGOMODE_HMAC ((Crypto_AlgorithmModeType)0x0f)
/* Cipher-based MAC */
#define CRYPTO_ALGOMODE_CMAC ((Crypto_AlgorithmModeType)0x10)
/* Galois MAC */
#define CRYPTO_ALGOMODE_GMAC ((Crypto_AlgorithmModeType)0x11)

#define CRYPTO_HASH ((Crypto_ServiceInfoType)0x00)
#define CRYPTO_MAC_GENERATE ((Crypto_ServiceInfoType)0x01)
#define CRYPTO_MAC_VERIFY ((Crypto_ServiceInfoType)0x02)
#define CRYPTO_ENCRYPT ((Crypto_ServiceInfoType)0x03)
#define CRYPTO_DECRYPT ((Crypto_ServiceInfoType)0x04)

#define CRYPTO_OPERATION_MODE_START ((Crypto_OperationModeType)0x01)
#define CRYPTO_OPERATION_MODE_UPDATE ((Crypto_OperationModeType)0x02)
#define CRYPTO_OPERATION_MODE_STREAM_START ((Crypto_OperationModeType)0x03)
#define CRYPTO_OPERATION_MODE_FINISH ((Crypto_OperationModeType)0x04)
#define CRYPTO_OPERATION_MODE_SINGLE_CALL ((Crypto_OperationModeType)0x07)
#define CRYPTO_OPERATION_MODE_SAVE_CONTEXT ((Crypto_OperationModeType)0x08)
#define CRYPTO_OPERATION_MODE_RESTORE_CONTEXT ((Crypto_OperationModeType)0x10)

#define CRYPTO_E_VER_OK ((Crypto_VerifyResultType)0x00)
#define CRYPTO_E_VER_NOT_OK ((Crypto_VerifyResultType)0x00)
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_Csm_01047 */
typedef uint8_t Crypto_AlgorithmFamilyType;

/* @SWS_Csm_01048 */
typedef uint8_t Crypto_AlgorithmModeType;

/* @SWS_Csm_01031 */
typedef uint8_t Crypto_ServiceInfoType;

/* @SWS_Csm_01029 */
typedef uint8_t Crypto_OperationModeType;

/* @SWS_Csm_01024 */
typedef uint8_t Crypto_VerifyResultType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* CRYPTO_GENERAL_TYPES_H */
