/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */

boolean CAN0_SECOC_MSG0_TX_TxIpduCallout(PduIdType PduId, const PduInfoType *PduInfoPtr) {
  return TRUE;
}

boolean CAN0_CanNmUserData_TX_TxIpduCallout(PduIdType PduId, const PduInfoType *PduInfoPtr) {
  return TRUE;
}

boolean CAN0_SECOC_MSG1_RX_RxIpduCallout(PduIdType PduId, const PduInfoType *PduInfoPtr) {
  return TRUE;
}
