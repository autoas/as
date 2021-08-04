/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Generated at Fri Jul 30 09:13:24 2021
 */
#ifndef COM_CFG_H
#define COM_CFG_H
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#ifndef COM_MAIN_FUNCTION_PERIOD
#define COM_MAIN_FUNCTION_PERIOD 10
#endif
#define COM_CONVERT_MS_TO_MAIN_CYCLES(x) \
  ((x + COM_MAIN_FUNCTION_PERIOD - 1) / COM_MAIN_FUNCTION_PERIOD)

#define COM_USE_CAN
//#define COM_USE_SIGNAL_CONFIG
#define COM_USE_SIGNAL_UPDATE_BIT

#define COM_RX_FOR_CAN0(id, PduInfoPtr) \
  if (0x102 == id) { \
    Com_RxIndication(COM_PID_RxMsgAbsInfo, PduInfoPtr); \
  }

#ifndef COM_ECUC_PDUID_OFFSET
#define COM_ECUC_PDUID_OFFSET 10
#endif

/* NOTE: manually modify to fix it to the right HTH */
#define COM_ECUC_CAN0_PDUID_MIN COM_ECUC_PDUID_OFFSET
#define COM_ECUC_CAN0_PDUID_MAX COM_ECUC_PDUID_OFFSET + 2
#define COM_TX_FOR_CAN0(TxPduId, dlPdu, PduInfoPtr, ret) \
  if ((COM_PID_TxMsgTime+COM_ECUC_PDUID_OFFSET) == TxPduId) { \
    dlPdu.id = 0x101; \
    ret = Can_Write(0, &dlPdu); \
  }

/* messages for network CAN0 */
#define COM_PID_TxMsgTime 0
#define COM_PID_RxMsgAbsInfo 1

/* signals for network CAN0 */
/* signals for message TxMsgTime: id=0x101 dlc=8, dir=TX */
#define COM_SID_year 0 /* big 16@7 */
#define COM_SID_month 1 /* big 8@23 */
#define COM_SID_day 2 /* big 8@31 */
#define COM_SID_hour 3 /* big 8@39 */
#define COM_SID_minute 4 /* big 8@47 */
#define COM_SID_second 5 /* big 8@55 */
#define COM_GID_SystemTime 6 /* big 56@7 */

/* signals for message RxMsgAbsInfo: id=0x102 dlc=8, dir=RX */
#define COM_SID_VehicleSpeed 7 /* big 16@7 */
#define COM_SID_TachoSpeed 8 /* big 16@23 */
#define COM_SID_Led1Sts 9 /* big 2@39 */
#define COM_SID_Led2Sts 10 /* big 2@37 */
#define COM_SID_Led3Sts 11 /* big 2@35 */


/* NOTE: manually modify to create more groups */
#define COM_GROUP_ID_CAN0 0
#define Com_IPduTxMsgTime_GroupRefMask (1<<COM_GROUP_ID_CAN0)
#define Com_IPduRxMsgAbsInfo_GroupRefMask (1<<COM_GROUP_ID_CAN0)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* COM_CFG_H */
