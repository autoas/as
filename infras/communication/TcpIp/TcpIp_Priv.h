/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef TCPIP_PRIV_H
#define TCPIP_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#define DET_THIS_MODULE_ID MODULE_ID_TCPIP
/* ================================ [ TYPES     ] ============================================== */
typedef void (*TcpIp_InitFncType)(void);
typedef void (*TcpIp_MainFncType)(void);

struct TcpIp_Config_s {
  TcpIp_InitFncType InitFnc;
  TcpIp_MainFncType MainFnc;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* TCPIP_PRIV_H */
