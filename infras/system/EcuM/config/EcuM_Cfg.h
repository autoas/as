/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef ECUM_CONFIG_H
#define ECUM_CONFIG_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Mcu.h"
#include "Can.h"
#include "CanIf.h"
#include "CanTp.h"
#include "OsekNm.h"
#include "CanNm.h"
#include "CanSM.h"
#include "ComM.h"
#include "Nm.h"
#include "CanTSyn.h"
#include "Lin.h"
#include "LinIf.h"
#include "Xcp.h"
#include "Com.h"
#include "PduR.h"
#include "Fls.h"
#include "Eep.h"
#include "NvM.h"
#include "Ea.h"
#include "Fee.h"
#include "Dem.h"
#include "Dcm.h"

#include "Port.h"

#include "TcpIp.h"
#include "SoAd.h"
#include "DoIP.h"
#include "Sd.h"
#include "SomeIp.h"
#include "UdpNm.h"

#include "J1939Tp.h"

#include "Csm.h"
#include "SecOC.h"
#include "E2E.h"
/* ================================ [ MACROS    ] ============================================== */
#define ECUM_MAIN_FUNCTION_PERIOD 10
#define ECUM_CONVERT_MS_TO_MAIN_CYCLES(x)                                                          \
  ((x + ECUM_MAIN_FUNCTION_PERIOD - 1) / ECUM_MAIN_FUNCTION_PERIOD)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* ECUM_CONFIG_H */
