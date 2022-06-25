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
#if defined(_WIN32) || defined(linux)
#include <stdlib.h>
#include <unistd.h>
#endif
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_CANIF 0
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void BL_AliveIndicate(void);
/* ================================ [ DATAS     ] ============================================== */
static Std_TimerType timer10ms;
static Std_TimerType timer500ms;

static uint32_t lRxId = 0x731;
static uint32_t lTxId = 0x732;
static uint8_t lController = 0;
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
  Can_SetControllerMode(lController, CAN_CS_STARTED);
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

  if (lRxId == Mailbox->CanId) {
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
    canPdu.id = lTxId;
    ret = Can_Write(lController, &canPdu);
  }

  return ret;
}
#endif

#ifndef USE_PDUR
Std_ReturnType PduR_DcmTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
#ifdef USE_CANTP
  return CanTp_Transmit(TxPduId, PduInfoPtr);
#endif
#ifdef USE_LINTP
  return LinTp_Transmit(TxPduId, PduInfoPtr);
#endif
}
#endif

void BL_MainTask_500ms(void) {
  BL_AliveIndicate();
}

#if defined(_WIN32) || defined(linux)
void Can_ReConfig(uint8_t Controller, const char *device, int port, uint32_t baudrate);
#endif

int main(int argc, char *argv[]) {
  ASLOG(INFO, ("bootloader build @ %s %s\n", __DATE__, __TIME__));

#if defined(_WIN32) || defined(linux)
  {
    int ch;
    opterr = 0;
    while ((ch = getopt(argc, argv, "c:d:r:t:")) != -1) {
      switch (ch) {
      case 'c':
        lController = atoi(optarg);
        break;
      case 'd':
        Can_ReConfig(lController, optarg, 0, 500000);
        break;
      case 'r':
        lRxId = strtoul(optarg, NULL, 16);
        break;
      case 't':
        lTxId = strtoul(optarg, NULL, 16);
        break;
      default:
        printf("Usage: %s -c controller_id -r rx_id -t tx_id\n", argv[0]);
        return 0;
        break;
      }
    }
  }
#endif

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
#if defined(_WIN32) || defined(linux)
    Std_Sleep(1000);
#endif
  }
  return 0;
}
