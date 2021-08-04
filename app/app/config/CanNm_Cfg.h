/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef _CAN_NM_CFG_H
#define _CAN_NM_CFG_H
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#ifndef CANNM_MAIN_FUNCTION_PERIOD
#define CANNM_MAIN_FUNCTION_PERIOD 10
#endif
#define CANNM_CONVERT_MS_TO_MAIN_CYCLES(x)                                                         \
  ((x + CANNM_MAIN_FUNCTION_PERIOD - 1) / CANNM_MAIN_FUNCTION_PERIOD)

/* @ECUC_CanNm_00055 */
#define CANNM_REMOTE_SLEEP_IND_ENABLED

/* @ECUC_CanNm_00010 */
//#define CANNM_PASSIVE_MODE_ENABLED

/* @ECUC_CanNm_00080 */
#define CANNM_COORDINATOR_SYNC_SUPPORT

/* @ECUC_CanNm_00086 */
#define CANNM_GLOBAL_PN_SUPPORT
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _CAN_NM_CFG_H */
