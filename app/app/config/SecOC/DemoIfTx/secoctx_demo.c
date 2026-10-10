/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * DemoIfTx - minimal SecOC transmit demo application glue.
 *
 * The whole BSW stack (Com/PduR/SecOC/Csm/CanIf) comes from the standard
 * SSAS libraries; this file only provides the small application hook that
 * feeds the protected signal into Com so that SecOC can secure and
 * transmit it. The dedicated configuration lives next to this file.
 *
 * The demo sends a single message SECOC_MSG on CAN ID 0x99, which is exactly
 * the frame that the DemoIfRx application receives and verifies.
 *
 * The authentic I-PDU is printed from the Com Tx IPdu callout, so the Tx log
 * is symmetric with the Rx side (which prints from its Com Rx IPdu callout).
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
void SecOC_DemoIfTx_Transmit(void);
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
 * PduR (which then routes it to SecOC for MAC generation and finally to
 * CanIf for transmission). Returning TRUE lets the transmission proceed;
 * returning FALSE cancels it. Printing here keeps the Tx log symmetric with
 * the Rx side, where the same style is used inside the Rx IPdu callout.
 */
boolean CAN0_SECOC_MSG_TX_TxIpduCallout(PduIdType PduId, const PduInfoType *PduInfoPtr) {
  ASLOG(DEMO, ("[DemoIfTx] send SECOC_MSG #%u, len=%u data:\n", counter, PduInfoPtr->SduLength));
  ASHEXDUMP(DEMO, (""), PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);

  return TRUE;
}

/**
 * @brief Kick off one transmit of the secured PDU.
 *
 * This mirrors the real data flow of CanApp: the application updates a Com
 * signal, then Com hands the authentic I-PDU to PduR -> SecOC, which computes
 * the MAC in SecOC_MainFunctionTx and sends the Secured I-PDU out via CanIf.
 */
void SecOC_DemoIfTx_Transmit(void) {
  uint8_t i;

  for (i = 0; i < sizeof(vehicleConfig); i++) {
    vehicleConfig[i] = (uint8_t)(i + counter);
  }

  (void)Com_SendSignal(COM_SID_VehicleConfig, vehicleConfig);

  counter++;
}

void User_Init(void) {
  memset(vehicleConfig, 0, sizeof(vehicleConfig));
  ASLOG(INFO, ("[DemoIfTx] SecOC transmit demo, SECOC_MSG on CAN ID 0x99\n"));
}

void User_MainTask10ms(void) {
  /* SECOC_MSG uses a 1000ms cycle, trigger it from here */
  static uint16_t tick = 0;
  tick++;
  if (0 == (tick % 100)) {
    SecOC_DemoIfTx_Transmit();
  }
}
