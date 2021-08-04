/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Mcu.h"
#include "Std_Debug.h"
#include "Std_Timer.h"

#ifdef USE_CAN
#include "Can.h"
#include "CanIf.h"
#include "CanIf_Can.h"
#include "CanTp.h"
#include "PduR_CanTp.h"
#endif

#ifdef USE_DLL
#include "Dll.h"
#endif

#ifdef USE_LINTP
#include "LinTp.h"
#endif

#include "Dcm.h"
#include "bl.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_CANIF 0
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void BL_AliveIndicate(void);
/* ================================ [ DATAS     ] ============================================== */
static Std_TimerType timer10ms;
static Std_TimerType timer500ms;
/* ================================ [ LOCALS    ] ============================================== */
static void MainTask_10ms(void) {
#ifdef USE_CAN
  CanTp_MainFunction();
#endif
#ifdef USE_LINTP
  LinTp_MainFunction();
#endif
  Dcm_MainFunction();
}

static void Init(void) {
#ifdef USE_CAN
  Can_Init(NULL);
  Can_SetControllerMode(0, CAN_CS_STARTED);
  CanTp_Init(NULL);
#endif
#ifdef USE_DLL
  DLL_Init(NULL);
  DLL_ScheduleRequest(0, 0);
#endif
#ifdef USE_LINTP
  LinTp_Init(NULL);
#endif

  Dcm_Init(NULL);
  BL_Init();
}
/* ================================ [ FUNCTIONS ] ============================================== */
#ifdef USE_CAN
void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr) {
  ASLOG(CANIF, ("RX bus=%d, canid=%X, dlc=%d, data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n",
                Mailbox->ControllerId, Mailbox->CanId, PduInfoPtr->SduLength,
                PduInfoPtr->SduDataPtr[0], PduInfoPtr->SduDataPtr[1], PduInfoPtr->SduDataPtr[2],
                PduInfoPtr->SduDataPtr[3], PduInfoPtr->SduDataPtr[4], PduInfoPtr->SduDataPtr[5],
                PduInfoPtr->SduDataPtr[6], PduInfoPtr->SduDataPtr[7]));

  if (0x731 == Mailbox->CanId) {
    CanTp_RxIndication((PduIdType)0, PduInfoPtr);
  } else if (0x7DF == Mailbox->CanId) {
    CanTp_RxIndication((PduIdType)1, PduInfoPtr);
  }
}
void CanIf_TxConfirmation(PduIdType CanTxPduId) {
  if (0 == CanTxPduId) {
    CanTp_TxConfirmation(0, E_OK);
  } else if (1 == CanTxPduId) {
    CanTp_TxConfirmation(1, E_OK);
  }
}

Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  Can_PduType canPdu;

  canPdu.swPduHandle = TxPduId;
  canPdu.length = PduInfoPtr->SduLength;
  canPdu.sdu = PduInfoPtr->SduDataPtr;

  if ((0 == TxPduId) || (1 == TxPduId)) {
    canPdu.id = 0x732;
    ret = Can_Write(0, &canPdu);
  }

  return ret;
}
#endif

Std_ReturnType PduR_DcmTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
#ifdef USE_CANTP
  return CanTp_Transmit(TxPduId, PduInfoPtr);
#endif
#ifdef USE_LINTP
  return LinTp_Transmit(TxPduId, PduInfoPtr);
#endif
}

void BL_MainTask_500ms(void) {
  BL_AliveIndicate();
}

int main(int argc, char *argv[]) {
  ASLOG(INFO, ("bootloader build @ %s %s\n", __DATE__, __TIME__));

  Mcu_Init(NULL);

  BL_CheckAndJump();

  Init();
  Std_TimerStart(&timer10ms);
  Std_TimerStart(&timer500ms);
  for (;;) {
    if (Std_GetTimerElapsedTime(&timer10ms) >= 10000) {
      MainTask_10ms();
      Std_TimerStart(&timer10ms);
    }
    if (Std_GetTimerElapsedTime(&timer500ms) >= 500000) {
      BL_MainTask_500ms();
      Std_TimerStart(&timer500ms);
    }

    Dcm_MainFunction_Request();
#ifdef USE_CAN
    Can_MainFunction_Write();
    Can_MainFunction_Read();
#endif
#ifdef USE_DLL
    DLL_MainFunction();
    DLL_MainFunction_Read();
#endif
  }
  return 0;
}
