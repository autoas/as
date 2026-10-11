/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * DemoRx - minimal E2E receive demo application glue.
 *
 * MSG_TIME (CAN ID 0x123) is received by CanIf and passed to Com through PduR.
 * As on the Tx side, the protection is generated: Com.json lists the message
 * in the network level "E2E" array and enables the message rx callout, so
 * Com.py emits
 *
 *   boolean CAN0_MSG_TIME_RX_RxIpduCallout(PduIdType, const PduInfoType*) {
 *     ret = E2E_P11Check(E2E_CHECK_P11_MSG_TIME, PduInfoPtr->SduDataPtr,
 *                        PduInfoPtr->SduLength);
 *     return ( (E_OK == ret) || (E2E_E_OK_SOME_LOST == ret));
 *   }
 *
 * and Com runs it as soon as the PDU is received - before the signals of the
 * PDU are updated - so returning TRUE is what accepts the PDU.
 *
 * The application does NOT implement that callout. Instead it uses the signal
 * level callback: with "enable_signal_rx_notification" the generator wires
 *   CAN0_MSG_TIME_RX_Seconds_RxNotification
 * for the Seconds signal (network + generated msg name which already carries
 * the _RX suffix + signal name), and Com calls it only for a PDU that the E2E
 * check callout has accepted (see Com_RxIndication: IPduCallout first, then the
 * signal notifications). Logging from there therefore proves that the data
 * passed E2E verification.
 *
 * This is the exact counterpart of the DemoTx application: run DemoTx on one
 * instance and DemoRx on another and they talk over CAN ID 0x123.
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
 * @brief Com signal RxNotification of MSG_TIME.Seconds.
 *
 * Com invokes this after the received PDU has passed the generated E2E check
 * callout, so the signal is guaranteed to be E2E protected and valid. The
 * value is read back through Com_ReceiveSignal, which is the normal way for an
 * application to consume a signal.
 */
void CAN0_MSG_TIME_RX_Seconds_RxNotification(void) {
  uint32_t seconds = 0;

  (void)Com_ReceiveSignal(COM_SID_Seconds, &seconds);

  ASLOG(DEMO, ("[DemoRx] MSG_TIME checked OK, Seconds=%u\n", (unsigned)seconds));
}

void User_Init(void) {
  ASLOG(INFO, ("[DemoRx] E2E P11 receive demo, MSG_TIME on CAN ID 0x123\n"));
}

void User_MainTask10ms(void) {
}
