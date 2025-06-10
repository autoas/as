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

#ifdef USE_LIN_SLAVE
#include "Lin_Slave.h"
#endif

#ifdef USE_LINIF
#include "LinIf.h"
#endif

#ifdef USE_LINTP
#include "LinTp.h"
#endif

#ifdef USE_EEP
#include "Eep.h"
#endif
#ifdef USE_EA
#include "Ea.h"
#endif
#ifdef USE_NVM
#include "NvM.h"
#endif

#include "Dcm.h"
#include "bl.h"
#if defined(_WIN32) || defined(linux)
#include <stdlib.h>
#include <unistd.h>
#endif

#ifdef USE_OSAL
#include "osal.h"
#endif

#ifdef USE_SHELL
#include "shell.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_CANIF 0

#ifndef CAN_DIAG_P2P_RX
#define CAN_DIAG_P2P_RX 0x731
#endif

#ifndef CAN_DIAG_P2P_TX
#define CAN_DIAG_P2P_TX 0x732
#endif

#ifndef CAN_DIAG_P2A_RX
#define CAN_DIAG_P2A_RX 0x7DF
#endif

#ifndef BL_NVM_TRY_FLUSH_MAX
#define BL_NVM_TRY_FLUSH_MAX 100
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void BL_AliveIndicate(void);
#ifdef USE_STDIO_CAN
extern void stdio_main_function(void);
#endif

#if defined(_WIN32) || defined(linux)
void Can_ReConfig(uint8_t Controller, const char *device, int port, uint32_t baudrate);
#endif
/* ================================ [ DATAS     ] ============================================== */
static Std_TimerType timer10ms;
static Std_TimerType timer500ms;

#ifdef USE_CAN
#if defined(_WIN32) || defined(linux)
static uint32_t lP2PRxId = CAN_DIAG_P2P_RX;
static uint32_t lP2PTxId = CAN_DIAG_P2P_TX;
static uint32_t lP2ARxId = CAN_DIAG_P2A_RX;
static uint8_t lController = 0;
#else
#define lP2PRxId CAN_DIAG_P2P_RX
#define lP2PTxId CAN_DIAG_P2P_TX
#define lP2ARxId CAN_DIAG_P2A_RX
#define lController 0
#endif
#endif
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
#ifdef USE_LIN_SLAVE
  Lin_Slave_Init(NULL);
#endif
#ifdef USE_LINIF
  Lin_Init(NULL);
  LinIf_Init(NULL);
#endif
#ifdef USE_LINTP
  LinTp_Init(NULL);
#endif

  Dcm_Init(NULL);
  BL_Init();

#ifdef USE_SHELL
  Shell_Init();
#endif
}

#ifdef USE_NVM
static void MemoryTask(void) {
#ifdef USE_EEP
  Eep_MainFunction();
#endif
#ifdef USE_EA
  Ea_MainFunction();
#endif
#ifdef USE_NVM
  NvM_MainFunction();
#endif
}
#endif

static void Memory_Init(void) {
#ifdef USE_EEP
  Eep_Init(NULL);
#endif
#ifdef USE_EA
  Ea_Init(NULL);
#endif
#ifdef USE_NVM
  NvM_Init(NULL);
  BL_FlushNvM();
  NvM_ReadAll();
  BL_FlushNvM();
#endif
}

/* ================================ [ FUNCTIONS ] ============================================== */
void BL_FlushNvM(void) {
#ifdef USE_NVM
  uint16_t tryCnt = BL_NVM_TRY_FLUSH_MAX;
  MemoryTask();
  while ((MEMIF_IDLE != NvM_GetStatus()) && (tryCnt > 0)) {
    MemoryTask();
    tryCnt--;
  }

  if (MEMIF_IDLE != NvM_GetStatus()) {
    ASLOG(BLE, ("NvM flush failed\n"));
  }
#endif
}
#ifdef USE_CAN
void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr) {
  ASLOG(CANIF, ("RX bus=%d, canid=%X, dlc=%d, data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n",
                Mailbox->ControllerId, Mailbox->CanId, PduInfoPtr->SduLength,
                PduInfoPtr->SduDataPtr[0], PduInfoPtr->SduDataPtr[1], PduInfoPtr->SduDataPtr[2],
                PduInfoPtr->SduDataPtr[3], PduInfoPtr->SduDataPtr[4], PduInfoPtr->SduDataPtr[5],
                PduInfoPtr->SduDataPtr[6], PduInfoPtr->SduDataPtr[7]));

  if (lP2PRxId == (Mailbox->CanId & 0x1FFFFFFFul)) {
    CanTp_RxIndication((PduIdType)0, PduInfoPtr);
  } else if (lP2ARxId == (Mailbox->CanId & 0x1FFFFFFFul)) {
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
    canPdu.id = lP2PTxId;
    ret = Can_Write(lController, &canPdu);
  }

  return ret;
}

void CanIf_ControllerBusOff(uint8_t ControllerId) {
  Can_Init(NULL);
  Can_SetControllerMode(ControllerId, CAN_CS_STARTED);
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

void Task_MainLoop(void) {
#ifdef USE_LATE_MCU_INIT
  Mcu_Init(NULL);
#endif
  Init();
  Std_TimerSet(&timer10ms, 10000);
  Std_TimerSet(&timer500ms, 500000);
  for (;;) {
    if (TRUE == Std_IsTimerTimeout(&timer10ms)) {
      Std_TimerSet(&timer10ms, 10000);
      MainTask_10ms();
    }
    if (TRUE == Std_IsTimerTimeout(&timer500ms)) {
      Std_TimerSet(&timer500ms, 500000);
      BL_MainTask_500ms();
    }

    Dcm_MainFunction_Request();
#ifdef USE_CAN
    CanTp_MainFunction_Fast();
    Can_MainFunction_Write();
    Can_MainFunction_Read();
#endif
#ifdef USE_DLL
    DLL_MainFunction();
    DLL_MainFunction_Read();
#endif
#ifdef USE_LIN_SLAVE
    Lin_Slave_MainFunction();
    Lin_Slave_MainFunction_Read();
#endif
#ifdef USE_LINIF
    Lin_MainFunction();
    Lin_MainFunction_Read();
    LinIf_MainFunction();
#endif
#ifdef USE_SHELL
    Shell_MainFunction();
#endif
#if defined(USE_STDIO_CAN) || defined(USE_STDIO_OUT)
    stdio_main_function();
#endif
  }
}

#ifdef USE_OSAL
void TaskMainTaskIdle(void) {
  while (1)
    ;
}
void StartupHook(void) {
  OSAL_ThreadCreate((OSAL_ThreadEntryType)Task_MainLoop, NULL);
}
#endif

int main(int argc, char *argv[]) {
  ASLOG(INFO, ("bootloader build @ %s %s\n", __DATE__, __TIME__));

#if defined(_WIN32) || defined(linux)
  {
    int ch;
    opterr = 0;
    while ((ch = getopt(argc, argv, "c:d:r:t:F:v:")) != -1) {
      switch (ch) {
#ifdef USE_CAN
      case 'c':
        lController = atoi(optarg);
        break;
      case 'd':
        Can_ReConfig(lController, optarg, 0, 500000);
        break;

      case 'r':
        lP2PRxId = strtoul(optarg, NULL, 16);
        break;
      case 't':
        lP2PTxId = strtoul(optarg, NULL, 16);
        break;
      case 'F':
        lP2ARxId = strtoul(optarg, NULL, 16);
        break;
#endif
      case 'v':
        std_set_log_level(atoi(optarg));
        break;
      default:
        printf("Usage: %s -c controller_id -r rx_id -t tx_id -v level\n", argv[0]);
        return 0;
        break;
      }
    }
  }
#endif

#ifndef USE_LATE_MCU_INIT
  Mcu_Init(NULL);
#endif

  Memory_Init();

  BL_CheckAndJump();

#ifdef USE_OSAL
  osal_start();
#else
  Task_MainLoop();
#endif
  return 0;
}
