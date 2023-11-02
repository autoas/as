/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef _UDP_NM_CFG_H
#define _UDP_NM_CFG_H
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#ifndef UDPNM_MAIN_FUNCTION_PERIOD
#define UDPNM_MAIN_FUNCTION_PERIOD 10
#endif
#define UDPNM_CONVERT_MS_TO_MAIN_CYCLES(x)                                                         \
  ((x + UDPNM_MAIN_FUNCTION_PERIOD - 1) / UDPNM_MAIN_FUNCTION_PERIOD)

#define UDPNM_RX_PID_CHL0 0

/* @ECUC_UdpNm_00005 */
#define UDPNM_REMOTE_SLEEP_IND_ENABLED

/* @ECUC_UdpNm_00010 */
//#define UDPNM_PASSIVE_MODE_ENABLED

/* @ECUC_UdpNm_00059 */
#define UDPNM_COORDINATOR_SYNC_SUPPORT

/* @ */
#define UDPNM_GLOBAL_PN_SUPPORT

#define UDPNM_PDU_LENGTH 8
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _UDP_NM_CFG_H */
