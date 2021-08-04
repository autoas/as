/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "OsekNm_Priv.h"
#ifdef _WIN32
#include <stdlib.h>
#endif
/* ================================ [ MACROS    ] ============================================== */
/* NM Main Task Tick = 10 ms */
#ifndef OSEKNM_MAIN_FUNCTION_PERIOD
#define OSEKNM_MAIN_FUNCTION_PERIOD 10
#endif
#define OSEK_CONVERT_MS_TO_MAIN_CYCLES(x)                                                          \
  ((x + OSEKNM_MAIN_FUNCTION_PERIOD - 1) / OSEKNM_MAIN_FUNCTION_PERIOD)
#ifdef _WIN32
#define tTyp OSEK_CONVERT_MS_TO_MAIN_CYCLES(1000)
#define tMax OSEK_CONVERT_MS_TO_MAIN_CYCLES(1500)
#else
#define tTyp OSEK_CONVERT_MS_TO_MAIN_CYCLES(100)
#define tMax OSEK_CONVERT_MS_TO_MAIN_CYCLES(260)
#endif
#define tError OSEK_CONVERT_MS_TO_MAIN_CYCLES(1000)
#define tWBS OSEK_CONVERT_MS_TO_MAIN_CYCLES(5000)
#define tTx OSEK_CONVERT_MS_TO_MAIN_CYCLES(20)

#define OSEKNM_NODE_ID 0

#define rx_limit 4
#define tx_limit 8

#ifdef _WIN32
#define L_CONST
#else
#define L_CONST const
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static L_CONST OsekNm_ChannelConfigType OsekNm_ChannelConfigs[] = {
  {
    tTx,
    tTyp,
    tMax,
    tError,
    tWBS,
    OSEKNM_NODE_ID,
    rx_limit,
    tx_limit,
  },
};
static OsekNm_ChannelContextType OsekNm_ChannelContexts[ARRAY_SIZE(OsekNm_ChannelConfigs)];

OsekNm_ConfigType OsekNm_Config = {
  OsekNm_ChannelConfigs,
  OsekNm_ChannelContexts,
  ARRAY_SIZE(OsekNm_ChannelConfigs),
};
/* ================================ [ LOCALS    ] ============================================== */
#ifdef _WIN32
static void __attribute__((constructor)) _oseknm_start(void) {
  char* nodeStr = getenv("OSEKNM_NODE_ID");
  if (nodeStr != NULL)
  {
    OsekNm_ChannelConfigs[0].NodeId = atoi(nodeStr);
  }
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */

void D_Init(NetIdType NetId, RoutineRefType Routine)
{

}

void D_Offline(NetIdType NetId)
{

}

void D_Online(NetIdType NetId)
{

}
