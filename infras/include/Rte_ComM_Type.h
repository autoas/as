/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Communication Manager AUTOSAR CP R23-11
 */
#ifndef RET_COMM_TYPES_H
#define RET_COMM_TYPES_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#define COMM_NO_COMMUNICATION ((ComM_ModeType)0)
#define COMM_SILENT_COMMUNICATION ((ComM_ModeType)1)
#define COMM_FULL_COMMUNICATION ((ComM_ModeType)2)
#define COMM_FULL_COMMUNICATION_WITH_WAKEUP_REQUEST ((ComM_ModeType)3)
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_ComM_00672 */
typedef uint8_t ComM_ModeType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* RET_COMM_TYPES_H */
