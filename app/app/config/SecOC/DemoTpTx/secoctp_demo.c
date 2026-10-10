/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * DemoTpTx - minimal SecOC transmit demo application glue over CAN TP.
 *
 * Same idea as DemoIfTx, but the Secured I-PDU is handed to CanTp instead of
 * CanIf: Com -> PduR -> SecOC -> PduR -> CanTp -> CanIf. Because the Secured
 * I-PDU (header + authentic I-PDU + truncated FV + truncated MAC = 48 bytes)
 * is larger than one CAN frame, CanTp segments it into a First Frame plus
 * Consecutive Frames (and the receiver answers with a Flow Control frame).
 *
 * The authentic I-PDU is printed from the Com Tx IPdu callout, so the Tx log
 * matches the Rx side.
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Com.h"
#include "Com_Cfg.h"
#include "ComStack_Types.h"
#include "Std_Debug.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DEMO 1
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
void SecOC_DemoTpTx_Transmit(void);
/* ================================ [ DATAS     ] ============================================== */
/* SECOC_MSG.VehicleConfig is a 288 bit (36 bytes) big-endian signal */
static uint8_t vehicleConfig[36];
static uint8_t counter = 0;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/**
 * @brief Com Tx IPdu callout of SECOC_MSG.
 *
 * Com invokes this each time the authentic I-PDU is about to be handed to
 * PduR (which routes it to SecOC for MAC generation and then to CanTp for
 * segmentation). Returning TRUE lets the transmission proceed.
 */
boolean CAN0_SECOC_MSG_TX_TxIpduCallout(PduIdType PduId, const PduInfoType *PduInfoPtr) {
  ASLOG(DEMO, ("[DemoTpTx] send SECOC_MSG #%u, len=%u data:\n", counter, PduInfoPtr->SduLength));
  ASHEXDUMP(DEMO, (""), PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);

  return TRUE;
}

/**
 * @brief Kick off one transmit of the secured PDU.
 *
 * The application updates a Com signal; Com then hands the authentic I-PDU to
 * PduR -> SecOC, which secures it, and finally to CanTp, which segments it and
 * transmits the N-PDUs via CanIf.
 */
void SecOC_DemoTpTx_Transmit(void) {
  uint8_t i;

  for (i = 0; i < sizeof(vehicleConfig); i++) {
    vehicleConfig[i] = (uint8_t)(i + counter);
  }

  (void)Com_SendSignal(COM_SID_VehicleConfig, vehicleConfig);

  counter++;
}

void User_Init(void) {
  memset(vehicleConfig, 0, sizeof(vehicleConfig));
  ASLOG(INFO, ("[DemoTpTx] SecOC over CAN TP transmit demo, SECOC_MSG on CAN ID 0x99\n"));
}

void User_MainTask10ms(void) {
  /* SECOC_MSG uses a 1000ms cycle, trigger it from here */
  static uint16_t tick = 0;
  tick++;
  if (0 == (tick % 100)) {
    SecOC_DemoTpTx_Transmit();
  }
}
