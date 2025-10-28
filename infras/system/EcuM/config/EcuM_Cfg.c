/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "EcuM_Priv.h"
#include "EcuM_Externals.h"
#include "EcuM_Cfg.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef ECUM_MIN_WAKEUP_TIME
#define ECUM_MIN_WAKEUP_TIME 2000
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
typedef struct LinTp_Config_s LinTp_ConfigType;
extern void LinTp_Init(const LinTp_ConfigType *CfgPtr);
extern void LinTp_MainFunction(void);
/* ================================ [ DATAS     ] ============================================== */
static void Dummy_Init(const void *config) {
}
static void Dummy_MainFunction(void) {
}
static const EcuM_DriverInitItemType EcuM_DriverInitListZero[] = {
#ifdef USE_MCU
  {(EcuM_DriverInitFncType)Mcu_Init, NULL},
#endif
#ifdef USE_PORT
  {(EcuM_DriverInitFncType)Port_Init, NULL},
#endif
#ifdef USE_CAN
  {(EcuM_DriverInitFncType)Can_Init, NULL},
#endif
#ifdef USE_LINIF
  {(EcuM_DriverInitFncType)Lin_Init, NULL},
#endif
#ifdef USE_EEP
  {(EcuM_DriverInitFncType)Eep_Init, NULL},
#endif
#ifdef USE_FLS
  {(EcuM_DriverInitFncType)Fls_Init, NULL},
#endif
  {Dummy_Init, NULL},
};

static const EcuM_DriverInitItemType EcuM_DriverInitListOne[] = {
#ifdef USE_CANIF
  {(EcuM_DriverInitFncType)CanIf_Init, NULL},
#endif
#ifdef USE_CANTP
  {(EcuM_DriverInitFncType)CanTp_Init, NULL},
#endif
#ifdef USE_OSEKNM
  {(EcuM_DriverInitFncType)OsekNm_Init, NULL},
#endif
#ifdef USE_CANNM
  {(EcuM_DriverInitFncType)CanNm_Init, NULL},
#endif
#if defined(USE_NM) && defined(USE_COMM)
  {(EcuM_DriverInitFncType)Nm_Init, NULL},
#endif
#ifdef USE_CANTSYN
  {(EcuM_DriverInitFncType)CanTSyn_Init, NULL},
#endif
#ifdef USE_CANSM
  {(EcuM_DriverInitFncType)CanSM_Init, NULL},
#endif
#ifdef USE_XCP
  {(EcuM_DriverInitFncType)Xcp_Init, NULL},
#endif
#ifdef USE_J1939TP
  {(EcuM_DriverInitFncType)J1939Tp_Init, NULL},
#endif
#ifdef USE_LINIF
  {(EcuM_DriverInitFncType)LinIf_Init, NULL},
#endif
#ifdef USE_LINTP
  {(EcuM_DriverInitFncType)LinTp_Init, NULL},
#endif
#ifdef USE_PDUR
  {(EcuM_DriverInitFncType)PduR_Init, NULL},
#endif
#ifdef USE_COM
  {(EcuM_DriverInitFncType)Com_Init, NULL},
#endif
#ifdef USE_COMM
  {(EcuM_DriverInitFncType)ComM_Init, NULL},
#endif
#ifdef USE_EA
  {(EcuM_DriverInitFncType)Ea_Init, NULL},
#endif
#ifdef USE_FEE
  {(EcuM_DriverInitFncType)Fee_Init, NULL},
#endif
#ifdef USE_NVM
  {(EcuM_DriverInitFncType)NvM_Init, NULL},
#endif
  {Dummy_Init, NULL},
};

static const EcuM_DriverInitItemType EcuM_DriverInitListTwo[] = {
/* DEM and DCM use NvM, so must be initialized after NvM */
#ifdef USE_DEM
  {(EcuM_DriverInitFncType)Dem_Init, NULL},
#endif
#ifdef USE_DCM
  {(EcuM_DriverInitFncType)Dcm_Init, NULL},
#endif
#if defined(USE_TCPIP) || defined(USE_SOAD)
  {(EcuM_DriverInitFncType)TcpIp_Init, NULL},
#endif
#ifdef USE_SOAD
  {(EcuM_DriverInitFncType)SoAd_Init, NULL},
#endif
#ifdef USE_DOIP
  {(EcuM_DriverInitFncType)DoIP_Init, NULL},
#endif
#ifdef USE_SD
  {(EcuM_DriverInitFncType)Sd_Init, NULL},
#endif
#ifdef USE_SOMEIP
  {(EcuM_DriverInitFncType)SomeIp_Init, NULL},
#endif
#ifdef USE_UDPNM
  {(EcuM_DriverInitFncType)UdpNm_Init, NULL},
#endif
#ifdef USE_CSM
  {(EcuM_DriverInitFncType)Csm_Init, NULL},
#endif
#ifdef USE_SECOC
  {(EcuM_DriverInitFncType)SecOC_Init, NULL},
#endif
#ifdef USE_E2E
  {(EcuM_DriverInitFncType)E2E_Init, NULL},
#endif
#ifdef USE_TLS
  {(EcuM_DriverInitFncType)TLS_Init, NULL},
#endif
#ifdef USE_MIRROR
  {(EcuM_DriverInitFncType)Mirror_Init, NULL},
#endif
  {Dummy_Init, NULL},
};

static const EcuM_DriverMainFncType EcuM_DriverMainList[] = {
#ifdef USE_CAN
#ifdef USE_CANIF
  CanIf_MainFunction,
#endif
#ifdef USE_CANTP
  CanTp_MainFunction,
#endif
#ifdef USE_OSEKNM
  OsekNm_MainFunction,
#endif
#ifdef USE_CANNM
  CanNm_MainFunction,
#endif
#if defined(USE_NM) && defined(USE_COMM)
  Nm_MainFunction,
#endif
#ifdef USE_CANTSYN
  CanTSyn_MainFunction,
#endif
#endif
#ifdef USE_LINTP
  LinTp_MainFunction,
#endif
#ifdef USE_LINIF
  LinIf_MainFunction,
#endif
#ifdef USE_DCM
  Dcm_MainFunction,     Dcm_MainFunction_Request,
#endif
#ifdef USE_XCP
  Xcp_MainFunction,     Xcp_MainFunction_Write,
#endif
#ifdef USE_COM
  Com_MainFunction,
#endif
#ifdef USE_CANSM
  CanSM_MainFunction,
#endif
#ifdef USE_COMM
  ComM_MainFunction,
#endif
#ifdef USE_J1939TP
  J1939Tp_MainFunction,
#endif
#ifdef USE_DEM
  Dem_MainFunction,
#endif
#ifdef USE_DOIP
  DoIP_MainFunction,
#endif
#ifdef USE_SD
  Sd_MainFunction,
#endif
#ifdef USE_SOMEIP
  SomeIp_MainFunction,
#endif
#ifdef USE_UDPNM
  UdpNm_MainFunction,
#endif
#ifdef USE_TLS
  TLS_MainFunction,
#endif
  Dummy_MainFunction,
};

static const EcuM_DriverMainFncType EcuM_DriverMainMemList[] = {
#ifdef USE_EEP
  Eep_MainFunction,
#endif
#ifdef USE_EA
  Ea_MainFunction,
#endif
#ifdef USE_FLS
  Fls_MainFunction,
#endif
#ifdef USE_FEE
  Fee_MainFunction,
#endif
#ifdef USE_NVM
  NvM_MainFunction,
#endif
  Dummy_MainFunction,
};

static const EcuM_DriverMainFncType EcuM_DriverMainFastList[] = {
#ifdef USE_CANIF
  CanIf_MainFunction_Fast,
#endif
#ifdef USE_COM
  Com_MainFunction_Fast,
#endif
#ifdef USE_CANTP
  CanTp_MainFunction_Fast,
#endif
#ifdef USE_CAN
  Can_MainFunction_Write,   Can_MainFunction_Read,
#endif
#ifdef USE_J1939TP
  J1939Tp_MainFunctionFast,
#endif
#ifdef USE_DLL
  DLL_MainFunction,         DLL_MainFunction_Read,
#endif
#ifdef USE_LIN_SLAVE
  Lin_Slave_MainFunction,   Lin_Slave_MainFunction_Read,
#endif
#ifdef USE_LINIF
  Lin_MainFunction,         Lin_MainFunction_Read,       LinIf_MainFunction_Read,
#endif
#ifdef USE_TCPIP
  TcpIp_MainFunction,
#endif
#ifdef USE_SOAD
  SoAd_MainFunction,
#endif
#ifdef USE_SECOC
  SecOC_MainFunctionTx,     SecOC_MainFunctionRx,
#endif
#ifdef USE_MIRROR
  Mirror_MainFunction,
#endif
  Dummy_MainFunction,
};

const EcuM_ConfigType EcuM_Config = {
  EcuM_DriverInitListZero,
  EcuM_DriverInitListOne,
  EcuM_DriverInitListTwo,
  EcuM_DriverMainList,
  EcuM_DriverMainMemList,
  EcuM_DriverMainFastList,
  ARRAY_SIZE(EcuM_DriverInitListZero),
  ARRAY_SIZE(EcuM_DriverInitListOne),
  ARRAY_SIZE(EcuM_DriverInitListTwo),
  ARRAY_SIZE(EcuM_DriverMainList),
  ARRAY_SIZE(EcuM_DriverMainMemList),
  ARRAY_SIZE(EcuM_DriverMainFastList),
  ECUM_CONVERT_MS_TO_MAIN_CYCLES(ECUM_MIN_WAKEUP_TIME),
};

/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
