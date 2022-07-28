/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref:  Specification of NetworkManagement Interface AUTOSAR CP Release 4.4.0
 */
#ifndef _NM_STACK_TYPES_H
#define _NM_STACK_TYPES_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_Nm_00274 */
typedef enum
{
  NM_MODE_BUS_SLEEP,
  NM_MODE_PREPARE_BUS_SLEEP,
  NM_MODE_SYNCHRONIZE,
  NM_MODE_NETWORK,
} Nm_ModeType;

/* @SWS_Nm_00275 */
typedef enum
{
  NM_STATE_UNINIT,
  NM_STATE_BUS_SLEEP,
  NM_STATE_PREPARE_BUS_SLEEP,
  NM_STATE_READY_SLEEP,
  NM_STATE_NORMAL_OPERATION,
  NM_STATE_REPEAT_MESSAGE,
  NM_STATE_SYNCHRONIZE,
  NM_STATE_OFFLINE,
} Nm_StateType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#ifdef __cplusplus
}
#endif
#endif /* _NM_STACK_TYPES_H */
