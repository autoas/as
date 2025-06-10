/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * ref: https://www.autosar.org/fileadmin/standards/R23-11/FO/AUTOSAR_FO_PRS_E2EProtocol.pdf
 */
#ifndef E2E_H
#define E2E_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/** @PRS_E2E_00322, @PRS_E2E_00677
 * @note The application must treat `E2E_E_OK_SOME_LOST` the same as `E2E_E_OK`,
 * but with an acknowledgment that some message loss occurred (within tolerable limits).
 * This means the system should continue normal operation despite minor message loss.
 */
#define E2E_E_OK E_OK
#define E2E_E_OK_SOME_LOST ((Std_ReturnType)0xE0)

#define E2E_E_REPEATED ((Std_ReturnType)0xE1)
#define E2E_E_WRONG_CRC ((Std_ReturnType)0xE2)
#define E2E_E_WRONG_SEQUENCE ((Std_ReturnType)0xE3)
#define E2E_E_NOT_AVAILABLE ((Std_ReturnType)0xE4)
#define E2E_E_NO_NEWDATA ((Std_ReturnType)0xE5)

/* @PRS_E2E_00853, @PRS_E2E_00678 */
#define E2E_E_SM_VALID E_OK
#define E2E_E_SM_NODATA ((Std_ReturnType)0xE9)
#define E2E_E_SM_DEINIT ((Std_ReturnType)0xEA)
#define E2E_E_SM_INIT ((Std_ReturnType)0xEB)
#define E2E_E_SM_INVALID ((Std_ReturnType)0xEC)
/* ================================ [ TYPES     ] ============================================== */
typedef struct E2E_Config_s E2E_ConfigType;

typedef uint16_t E2E_ProfileIdType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void E2E_Init(const E2E_ConfigType *config);

/* @PRS_E2E_00509 */
Std_ReturnType E2E_P11Protect(E2E_ProfileIdType profileId, uint8_t *data, uint16_t length);

/* @PRS_E2E_00516 */
Std_ReturnType E2E_P11Check(E2E_ProfileIdType profileId, uint8_t *data, uint16_t length);

/* @PRS_E2E_00528 */
Std_ReturnType E2E_P22Protect(E2E_ProfileIdType profileId, uint8_t *data, uint16_t length);

/* @PRS_E2E_00534 */
Std_ReturnType E2E_P22Check(E2E_ProfileIdType profileId, uint8_t *data, uint16_t length);

/* @PRS_E2E_00707 */
Std_ReturnType E2E_P44Protect(E2E_ProfileIdType profileId, uint8_t *data, uint16_t length);
Std_ReturnType E2E_P44Check(E2E_ProfileIdType profileId, uint8_t *data, uint16_t length);

/* @PRS_E2E_00403 */
Std_ReturnType E2E_P05Protect(E2E_ProfileIdType profileId, uint8_t *data, uint16_t length);
/* @PRS_E2E_00411 */
Std_ReturnType E2E_P05Check(E2E_ProfileIdType profileId, uint8_t *data, uint16_t length);
#ifdef __cplusplus
}
#endif
#endif /* E2E_H */
