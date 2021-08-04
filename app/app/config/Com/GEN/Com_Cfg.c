/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Generated at Fri Jul 30 09:13:24 2021
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Com_Cfg.h"
#include "Com.h"
#include "Com_Priv.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static const uint16_t year_InitialValue = 0;
static const uint8_t month_InitialValue = 0;
static const uint8_t day_InitialValue = 0;
static const uint8_t hour_InitialValue = 0;
static const uint8_t minute_InitialValue = 0;
static const uint8_t second_InitialValue = 0;
static const uint16_t VehicleSpeed_InitialValue = 0;
static const uint16_t TachoSpeed_InitialValue = 0;
static const uint8_t Led1Sts_InitialValue = 0;
static const uint8_t Led2Sts_InitialValue = 0;
static const uint8_t Led3Sts_InitialValue = 0;

static uint8_t Com_PduData_TxMsgTime[8];
static uint8_t Com_GrpsData_SystemTime[7];
static uint8_t Com_PduData_RxMsgAbsInfo[8];

static Com_IPduTxContextType Com_IPduTxContext_TxMsgTime;
static Com_IPduRxContextType Com_IPduRxContext_RxMsgAbsInfo;

#ifdef COM_USE_SIGNAL_CONFIG
static const Com_SignalTxConfigType Com_SignalTxConfig_year = {
  NULL, /* ErrorNotification */
  NULL, /* TxNotification */
};

static const Com_SignalTxConfigType Com_SignalTxConfig_month = {
  NULL, /* ErrorNotification */
  NULL, /* TxNotification */
};

static const Com_SignalTxConfigType Com_SignalTxConfig_day = {
  NULL, /* ErrorNotification */
  NULL, /* TxNotification */
};

static const Com_SignalTxConfigType Com_SignalTxConfig_hour = {
  NULL, /* ErrorNotification */
  NULL, /* TxNotification */
};

static const Com_SignalTxConfigType Com_SignalTxConfig_minute = {
  NULL, /* ErrorNotification */
  NULL, /* TxNotification */
};

static const Com_SignalTxConfigType Com_SignalTxConfig_second = {
  NULL, /* ErrorNotification */
  NULL, /* TxNotification */
};

static const Com_SignalTxConfigType Com_SignalTxConfig_SystemTime = {
  NULL, /* ErrorNotification */
  NULL, /* TxNotification */
};

static const Com_SignalRxConfigType Com_SignalRxConfig_VehicleSpeed = {
  NULL, /* InvalidNotification */
  NULL, /* RxNotification */
  0, /* FirstTimeout */
  NOTIFY, /* DataInvalidAction */
  NOTIFY, /* RxDataTimeoutAction */
};

static const Com_SignalRxConfigType Com_SignalRxConfig_TachoSpeed = {
  NULL, /* InvalidNotification */
  NULL, /* RxNotification */
  0, /* FirstTimeout */
  NOTIFY, /* DataInvalidAction */
  NOTIFY, /* RxDataTimeoutAction */
};

static const Com_SignalRxConfigType Com_SignalRxConfig_Led1Sts = {
  NULL, /* InvalidNotification */
  NULL, /* RxNotification */
  0, /* FirstTimeout */
  NOTIFY, /* DataInvalidAction */
  NOTIFY, /* RxDataTimeoutAction */
};

static const Com_SignalRxConfigType Com_SignalRxConfig_Led2Sts = {
  NULL, /* InvalidNotification */
  NULL, /* RxNotification */
  0, /* FirstTimeout */
  NOTIFY, /* DataInvalidAction */
  NOTIFY, /* RxDataTimeoutAction */
};

static const Com_SignalRxConfigType Com_SignalRxConfig_Led3Sts = {
  NULL, /* InvalidNotification */
  NULL, /* RxNotification */
  0, /* FirstTimeout */
  NOTIFY, /* DataInvalidAction */
  NOTIFY, /* RxDataTimeoutAction */
};

#endif /* COM_USE_SIGNAL_CONFIG */
static const Com_SignalConfigType Com_SignalConfigs[] = {
  {
    &Com_GrpsData_SystemTime[0], /* ptr */
    &year_InitialValue, /* initPtr */
    COM_UINT16, /* type */
    COM_SID_year, /* HandleId */
    7, /* BitPosition */
    16, /* BitSize */
#ifdef COM_USE_SIGNAL_UPDATE_BIT
    COM_UPDATE_BIT_NOT_USED, /* UpdateBit */
#endif
    BIG, /* Endianness */
#ifdef COM_USE_SIGNAL_CONFIG
    NULL, /* rxConfig */
    &Com_SignalTxConfig_year, /* txConfig */
#endif
  },
  {
    &Com_GrpsData_SystemTime[2], /* ptr */
    &month_InitialValue, /* initPtr */
    COM_UINT8, /* type */
    COM_SID_month, /* HandleId */
    7, /* BitPosition */
    8, /* BitSize */
#ifdef COM_USE_SIGNAL_UPDATE_BIT
    COM_UPDATE_BIT_NOT_USED, /* UpdateBit */
#endif
    BIG, /* Endianness */
#ifdef COM_USE_SIGNAL_CONFIG
    NULL, /* rxConfig */
    &Com_SignalTxConfig_month, /* txConfig */
#endif
  },
  {
    &Com_GrpsData_SystemTime[3], /* ptr */
    &day_InitialValue, /* initPtr */
    COM_UINT8, /* type */
    COM_SID_day, /* HandleId */
    7, /* BitPosition */
    8, /* BitSize */
#ifdef COM_USE_SIGNAL_UPDATE_BIT
    COM_UPDATE_BIT_NOT_USED, /* UpdateBit */
#endif
    BIG, /* Endianness */
#ifdef COM_USE_SIGNAL_CONFIG
    NULL, /* rxConfig */
    &Com_SignalTxConfig_day, /* txConfig */
#endif
  },
  {
    &Com_GrpsData_SystemTime[4], /* ptr */
    &hour_InitialValue, /* initPtr */
    COM_UINT8, /* type */
    COM_SID_hour, /* HandleId */
    7, /* BitPosition */
    8, /* BitSize */
#ifdef COM_USE_SIGNAL_UPDATE_BIT
    COM_UPDATE_BIT_NOT_USED, /* UpdateBit */
#endif
    BIG, /* Endianness */
#ifdef COM_USE_SIGNAL_CONFIG
    NULL, /* rxConfig */
    &Com_SignalTxConfig_hour, /* txConfig */
#endif
  },
  {
    &Com_GrpsData_SystemTime[5], /* ptr */
    &minute_InitialValue, /* initPtr */
    COM_UINT8, /* type */
    COM_SID_minute, /* HandleId */
    7, /* BitPosition */
    8, /* BitSize */
#ifdef COM_USE_SIGNAL_UPDATE_BIT
    COM_UPDATE_BIT_NOT_USED, /* UpdateBit */
#endif
    BIG, /* Endianness */
#ifdef COM_USE_SIGNAL_CONFIG
    NULL, /* rxConfig */
    &Com_SignalTxConfig_minute, /* txConfig */
#endif
  },
  {
    &Com_GrpsData_SystemTime[6], /* ptr */
    &second_InitialValue, /* initPtr */
    COM_UINT8, /* type */
    COM_SID_second, /* HandleId */
    7, /* BitPosition */
    8, /* BitSize */
#ifdef COM_USE_SIGNAL_UPDATE_BIT
    COM_UPDATE_BIT_NOT_USED, /* UpdateBit */
#endif
    BIG, /* Endianness */
#ifdef COM_USE_SIGNAL_CONFIG
    NULL, /* rxConfig */
    &Com_SignalTxConfig_second, /* txConfig */
#endif
  },
  {
    &Com_PduData_TxMsgTime[0], /* ptr */
    Com_GrpsData_SystemTime, /* shadowPtr */
    COM_UINT8N, /* type */
    COM_GID_SystemTime, /* HandleId */
    7, /* BitPosition */
    56, /* BitSize */
#ifdef COM_USE_SIGNAL_UPDATE_BIT
    COM_UPDATE_BIT_NOT_USED, /* UpdateBit */
#endif
    BIG, /* Endianness */
#ifdef COM_USE_SIGNAL_CONFIG
    NULL, /* rxConfig */
    &Com_SignalTxConfig_SystemTime, /* txConfig */
#endif
  },
  {
    &Com_PduData_RxMsgAbsInfo[0], /* ptr */
    &VehicleSpeed_InitialValue, /* initPtr */
    COM_UINT16, /* type */
    COM_SID_VehicleSpeed, /* HandleId */
    7, /* BitPosition */
    16, /* BitSize */
#ifdef COM_USE_SIGNAL_UPDATE_BIT
    COM_UPDATE_BIT_NOT_USED, /* UpdateBit */
#endif
    BIG, /* Endianness */
#ifdef COM_USE_SIGNAL_CONFIG
    &Com_SignalRxConfig_VehicleSpeed, /* rxConfig */
    NULL, /* txConfig */
#endif
  },
  {
    &Com_PduData_RxMsgAbsInfo[2], /* ptr */
    &TachoSpeed_InitialValue, /* initPtr */
    COM_UINT16, /* type */
    COM_SID_TachoSpeed, /* HandleId */
    7, /* BitPosition */
    16, /* BitSize */
#ifdef COM_USE_SIGNAL_UPDATE_BIT
    COM_UPDATE_BIT_NOT_USED, /* UpdateBit */
#endif
    BIG, /* Endianness */
#ifdef COM_USE_SIGNAL_CONFIG
    &Com_SignalRxConfig_TachoSpeed, /* rxConfig */
    NULL, /* txConfig */
#endif
  },
  {
    &Com_PduData_RxMsgAbsInfo[4], /* ptr */
    &Led1Sts_InitialValue, /* initPtr */
    COM_UINT8, /* type */
    COM_SID_Led1Sts, /* HandleId */
    7, /* BitPosition */
    2, /* BitSize */
#ifdef COM_USE_SIGNAL_UPDATE_BIT
    COM_UPDATE_BIT_NOT_USED, /* UpdateBit */
#endif
    BIG, /* Endianness */
#ifdef COM_USE_SIGNAL_CONFIG
    &Com_SignalRxConfig_Led1Sts, /* rxConfig */
    NULL, /* txConfig */
#endif
  },
  {
    &Com_PduData_RxMsgAbsInfo[4], /* ptr */
    &Led2Sts_InitialValue, /* initPtr */
    COM_UINT8, /* type */
    COM_SID_Led2Sts, /* HandleId */
    5, /* BitPosition */
    2, /* BitSize */
#ifdef COM_USE_SIGNAL_UPDATE_BIT
    COM_UPDATE_BIT_NOT_USED, /* UpdateBit */
#endif
    BIG, /* Endianness */
#ifdef COM_USE_SIGNAL_CONFIG
    &Com_SignalRxConfig_Led2Sts, /* rxConfig */
    NULL, /* txConfig */
#endif
  },
  {
    &Com_PduData_RxMsgAbsInfo[4], /* ptr */
    &Led3Sts_InitialValue, /* initPtr */
    COM_UINT8, /* type */
    COM_SID_Led3Sts, /* HandleId */
    3, /* BitPosition */
    2, /* BitSize */
#ifdef COM_USE_SIGNAL_UPDATE_BIT
    COM_UPDATE_BIT_NOT_USED, /* UpdateBit */
#endif
    BIG, /* Endianness */
#ifdef COM_USE_SIGNAL_CONFIG
    &Com_SignalRxConfig_Led3Sts, /* rxConfig */
    NULL, /* txConfig */
#endif
  },
};

static const Com_SignalConfigType* Com_IPduSignals_TxMsgTime[] = {
  &Com_SignalConfigs[COM_SID_year],
  &Com_SignalConfigs[COM_SID_month],
  &Com_SignalConfigs[COM_SID_day],
  &Com_SignalConfigs[COM_SID_hour],
  &Com_SignalConfigs[COM_SID_minute],
  &Com_SignalConfigs[COM_SID_second],
  &Com_SignalConfigs[COM_GID_SystemTime],
};

static const Com_SignalConfigType* Com_IPduSignals_RxMsgAbsInfo[] = {
  &Com_SignalConfigs[COM_SID_VehicleSpeed],
  &Com_SignalConfigs[COM_SID_TachoSpeed],
  &Com_SignalConfigs[COM_SID_Led1Sts],
  &Com_SignalConfigs[COM_SID_Led2Sts],
  &Com_SignalConfigs[COM_SID_Led3Sts],
};

static const Com_IPduTxConfigType Com_IPduTxConfig_TxMsgTime = {
  &Com_IPduTxContext_TxMsgTime,
  NULL, /* ErrorNotification */
  NULL, /* TxNotification */
  COM_CONVERT_MS_TO_MAIN_CYCLES(0), /* FirstTime */
  COM_CONVERT_MS_TO_MAIN_CYCLES(1000), /* CycleTime */
  COM_ECUC_PDUID_OFFSET + COM_PID_TxMsgTime,
};

static const Com_IPduRxConfigType Com_IPduRxConfig_RxMsgAbsInfo = {
  &Com_IPduRxContext_RxMsgAbsInfo,
  NULL, /* RxNotification */
  NULL, /* RxTOut */
  COM_CONVERT_MS_TO_MAIN_CYCLES(0), /* FirstTimeout */
  COM_CONVERT_MS_TO_MAIN_CYCLES(0), /* Timeout */
};

static const Com_IPduConfigType Com_IPduConfigs[] = {
  {
    Com_PduData_TxMsgTime, /* ptr */
    Com_IPduSignals_TxMsgTime, /* signals */
    NULL, /* rxConfig */
    &Com_IPduTxConfig_TxMsgTime, /* txConfig */
    Com_IPduTxMsgTime_GroupRefMask,
    sizeof(Com_PduData_TxMsgTime), /* length */
    ARRAY_SIZE(Com_IPduSignals_TxMsgTime), /* numOfSignals */
  },
  {
    Com_PduData_RxMsgAbsInfo, /* ptr */
    Com_IPduSignals_RxMsgAbsInfo, /* signals */
    &Com_IPduRxConfig_RxMsgAbsInfo, /* rxConfig */
    NULL, /* txConfig */
    Com_IPduRxMsgAbsInfo_GroupRefMask,
    sizeof(Com_PduData_RxMsgAbsInfo), /* length */
    ARRAY_SIZE(Com_IPduSignals_RxMsgAbsInfo), /* numOfSignals */
  },
};

static Com_GlobalContextType Com_GlobalContext;
const Com_ConfigType Com_Config = {
  Com_IPduConfigs,
  Com_SignalConfigs,
  &Com_GlobalContext,
  ARRAY_SIZE(Com_IPduConfigs),
  ARRAY_SIZE(Com_SignalConfigs),
  1 /* numOfGroups */,
};

/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
