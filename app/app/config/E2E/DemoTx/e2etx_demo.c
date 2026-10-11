/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * DemoTx - minimal E2E transmit demo application glue.
 *
 * E2E is not a standalone BSW module in AS: it is called from inside Com, and
 * the Com generator writes the whole protection for you. This demo therefore
 * uses the automatic wiring:
 *
 *  - Com.json only lists the message in the network level "E2E" array
 *    ({ "name": "MSG_TIME", "profile": "P11" }) and enables the message tx
 *    callout. Com.py then generates
 *
 *      boolean CAN0_MSG_TIME_TX_TxIpduCallout(PduIdType, const PduInfoType*) {
 *        ret = E2E_P11Protect(E2E_PROTECT_P11_MSG_TIME, PduInfoPtr->SduDataPtr,
 *                             PduInfoPtr->SduLength);
 *        return (E_OK == ret);
 *      }
 *
 *    and E2E.json describes the profile. The application implements NO callout
 *    at all - it only updates the signal and logs its value.
 *
 * The demo sends a single 8-byte message MSG_TIME on CAN ID 0x123. With the
 * classic P11 NIBBLE layout (CRCOffset 0, CounterOffset 8, DataIDNibbleOffset
 * 12, DataID 0x1234) the wire format is:
 *
 *   byte 0    : CRC-8-SAE J1850 over the whole PDU
 *   byte 1    : low nibble  = 4-bit sequence counter (0..14, wraps)
 *               high nibble = high nibble of DataID (0x2)
 *   byte 2..3 : free
 *   byte 4..7 : payload, here the 32-bit "Seconds" signal
 *
 * The low byte of the DataID (0x34) never appears on the bus - with NIBBLE
 * mode only the high nibble is embedded in the PDU, the rest goes into the CRC
 * (see E2E_ComputeP11Crc).
 *
 * The counterpart is DemoRx, which receives ID 0x123 and runs E2E_P11Check
 * from its generated Rx IPdu callout.
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
void E2E_DemoTx_Transmit(void);
/* ================================ [ DATAS     ] ============================================== */
static uint32_t seconds = 604796;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/**
 * @brief Kick off one E2E protected transmit.
 *
 * The application only updates the Com signal and logs its value; everything
 * else is generated: Com assembles the PDU, the generated Tx IPdu callout runs
 * E2E_P11Protect on it, and PduR forwards the protected PDU to CanIf.
 */
void E2E_DemoTx_Transmit(void) {
  ASLOG(DEMO, ("[DemoTx] send MSG_TIME.Seconds=%u\n", (unsigned)seconds));

  (void)Com_SendSignal(COM_SID_Seconds, &seconds);

  seconds++;
}

void User_Init(void) {
  ASLOG(INFO, ("[DemoTx] E2E P11 transmit demo, MSG_TIME on CAN ID 0x123\n"));
}

void User_MainTask10ms(void) {
  /* MSG_TIME uses a 1000ms cycle, trigger it from here */
  static uint16_t tick = 0;
  tick++;
  if (0 == (tick % 100)) {
    E2E_DemoTx_Transmit();
  }
}
