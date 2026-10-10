/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * DemoTpRx - minimal SecOC receive demo application glue over CAN TP.
 *
 * The Secured I-PDU SECOC_MSG (CAN ID 0x99) is received frame by frame by
 * CanIf, reassembled by CanTp, passed to SecOC via PduR, verified in
 * SecOC_MainFunctionRx and, only when the MAC and freshness checks pass,
 * forwarded to Com. This file prints the message from the Com Rx IPdu callout,
 * so anything reaching the callout has already passed SecOC verification.
 *
 * This is the exact counterpart of the DemoTpTx application: run DemoTpTx on
 * one instance and DemoTpRx on another and they talk over CAN ID 0x99 using
 * ISO 15765-2 transport protocol segmentation.
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Com.h"
#include "Com_Cfg.h"
#include "ComStack_Types.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DEMO 1
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/**
 * @brief Com Rx IPdu callout of SECOC_MSG.
 *
 * Com invokes this after CanTp reassembled the N-PDUs and SecOC verified the
 * MAC and freshness value and forwarded the authentic I-PDU (SecOC -> PduR ->
 * Com). Returning TRUE accepts the PDU so its signals are updated; returning
 * FALSE would drop it. Here the raw authenticated payload is dumped to prove
 * the round trip.
 */
boolean CAN0_SECOC_MSG_RX_RxIpduCallout(PduIdType PduId, const PduInfoType *PduInfoPtr) {
  ASLOG(DEMO, ("[DemoTpRx] SECOC_MSG verified, len=%u data:\n", PduInfoPtr->SduLength));
  ASHEXDUMP(DEMO, (""), PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);

  return TRUE;
}

void User_Init(void) {
  ASLOG(INFO, ("[DemoTpRx] SecOC over CAN TP receive demo, SECOC_MSG on CAN ID 0x99\n"));
}

void User_MainTask10ms(void) {
}
