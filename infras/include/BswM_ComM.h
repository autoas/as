/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: pecification of Basic Software Mode Manager AUTOSAR CP R23-11
 */
#ifndef BSWM_COMM_H
#define BSWM_COMM_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Rte_ComM_Type.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_BswM_00047 */
void BswM_ComM_CurrentMode(NetworkHandleType Network, ComM_ModeType RequestedMode);
#ifdef __cplusplus
}
#endif
#endif /* BSWM_COMM_H */
