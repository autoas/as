/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef DCM_CFG_H
#define DCM_CFG_H
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#define DCM_MAIN_FUNCTION_PERIOD 10
#define DCM_CONVERT_MS_TO_MAIN_CYCLES(x)                                                           \
  ((x + DCM_MAIN_FUNCTION_PERIOD - 1) / DCM_MAIN_FUNCTION_PERIOD)

#define DCM_USE_SERVICE_SECURITY_ACCESS
#define DCM_USE_SERVICE_REQUEST_DOWNLOAD
#define DCM_USE_SERVICE_REQUEST_UPLOAD
#define DCM_USE_SERVICE_TRANSFER_DATA
#define DCM_USE_SERVICE_REQUEST_TRANSFER_EXIT
#define DCM_USE_SERVICE_ROUTINE_CONTROL
#define DCM_USE_SERVICE_ECU_RESET
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* DCM_CFG_H */