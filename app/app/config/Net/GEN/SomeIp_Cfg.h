/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Generated at Sat Nov 27 10:33:30 2021
 */
#ifndef _SOMEIP_CFG_H
#define _SOMEIP_CFG_H
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#define SOMEIP_SSID_SERVER0 0
#define SOMEIP_CSID_CLIENT0 1

#define SOMEIP_RX_PID_SOMEIP_SERVER0 0
#define SOMEIP_TX_PID_SOMEIP_SERVER0 0
#define SOMEIP_RX_PID_SOMEIP_CLIENT0 1
#define SOMEIP_TX_PID_SOMEIP_CLIENT0 1

#define SOMEIP_RX_METHOD_SERVER0_METHOD1 0

#define SOMEIP_TX_METHOD_CLIENT0_METHOD2 0

#define SOMEIP_TX_EVT_SERVER0_EVENT_GROUP1_EVENT0 0

#define SOMEIP_RX_EVT_CLIENT0_EVENT_GROUP2_EVENT0 0

#define SOMEIP_MAIN_FUNCTION_PERIOD 10
#define SOMEIP_CONVERT_MS_TO_MAIN_CYCLES(x) \
  ((x + SOMEIP_MAIN_FUNCTION_PERIOD - 1) / SOMEIP_MAIN_FUNCTION_PERIOD)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _SOMEIP_CFG_H */
