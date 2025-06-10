/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "LinTp_Cfg.h"
#include "PduR_LinTp.h"
#include "LinTp.h"
#include "LinTp_Priv.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define CANTP_CFG_H
#define CANTP_NO_FC
#define CANTP_USE_TRIGGER_TRANSMIT
#ifdef LINTP_USE_PB_CONFIG
#define CANTP_USE_PB_CONFIG
#define cantpConfig lintpConfig
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ ALIAS     ] ============================================== */
#include "CanTp.c"
/* ================================ [ FUNCTIONS ] ============================================== */
