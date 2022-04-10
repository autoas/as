/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Mcu.h"
#include "Std_Debug.h"
#include "Std_Timer.h"
#include <string.h>
#include <assert.h>
#if defined(_WIN32)
#include <unistd.h>
#endif

#ifdef USE_CAN
#include "Can.h"
#include "CanIf.h"
#include "CanIf_Can.h"
#include "CanTp.h"
#include "PduR_CanTp.h"
#ifdef USE_OSEKNM
#include "OsekNm.h"
#endif
#ifdef USE_CANNM
#include "CanNm.h"
#endif
#endif

#ifdef USE_PDUR
#include "PduR.h"
#endif

#ifdef USE_COM
#include "Com.h"
#include "./config/Com/GEN/Com_Cfg.h"
#include "PduR_Com.h"
#endif

#ifdef USE_DLL
#include "Dll.h"
#endif

#ifdef USE_LINTP
#include "LinTp.h"
#endif

#include "Dcm.h"
#ifdef USE_DEM
#include "Dem.h"
#endif
#ifdef USE_FLS
#include "Fls.h"
#endif
#ifdef USE_FEE
#include "Fee.h"
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

#ifdef USE_TCPIP
#include "TcpIp.h"
#endif

#ifdef USE_SOAD
#include "SoAd.h"
#endif

#ifdef USE_DOIP
#include "DoIP.h"
#endif

#ifdef USE_SD
#include "Sd.h"
#endif

#ifdef USE_SOMEIP
#include "SomeIp.h"
#endif

#ifdef USE_PLUGIN
#include "plugin.h"
#endif

#include "app.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_CANIF 0
#define AS_LOG_OSEKNM 1

#ifdef USE_DOIP
#define CANID_P2P_RX 0x732
#define CANID_P2P_TX 0x731
#else
#define CANID_P2P_RX 0x731
#define CANID_P2P_TX 0x732
#endif
#define CANID_P2A_RX 0x7DF
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void App_AliveIndicate(void);
/* ================================ [ DATAS     ] ============================================== */
static Std_TimerType timer10ms;
static Std_TimerType timer100ms;
/* ================================ [ LOCALS    ] ============================================== */
static void MemoryTask(void) {
#ifdef USE_EEP
  Eep_MainFunction();
#endif
#ifdef USE_EA
  Ea_MainFunction();
#endif
#ifdef USE_FLS
  Fls_MainFunction();
#endif
#ifdef USE_FEE
  Fee_MainFunction();
#endif
#ifdef USE_NVM
  NvM_MainFunction();
#endif
}

static void MainTask_10ms(void) {
#ifdef USE_CAN
#ifdef USE_CANTP
  CanTp_MainFunction();
#endif
#ifdef USE_OSEKNM
  OsekNm_MainFunction();
#endif
#ifdef USE_CANNM
  CanNm_MainFunction();
#endif
#endif
#ifdef USE_LINTP
  LinTp_MainFunction();
#endif
#ifdef USE_COM
  Com_MainFunction();
#endif

  MemoryTask();
#ifdef USE_DCM
  Dcm_MainFunction();
#endif

#ifdef USE_DOIP
  DoIP_MainFunction();
#endif
#ifdef USE_SD
  Sd_MainFunction();
#endif
#ifdef USE_SOMEIP
  SomeIp_MainFunction();
#endif

#ifdef USE_PLUGIN
  plugin_main();
#endif
}

static void BSW_Init(void) {
#ifdef USE_CAN
  Can_Init(NULL);
  Can_SetControllerMode(0, CAN_CS_STARTED);
#ifdef USE_CANTP
  CanTp_Init(NULL);
#endif
#ifdef USE_OSEKNM
  OsekNm_Init(NULL);
  TalkNM(0);
  StartNM(0);
  GotoMode(0, NM_BusSleep);
#endif
#ifdef USE_CANNM
  CanNm_Init(NULL);
#endif
#endif
#ifdef USE_DLL
  DLL_Init(NULL);
  DLL_ScheduleRequest(0, 0);
#endif
#ifdef USE_LINTP
  LinTp_Init(NULL);
#endif

#ifdef USE_PDUR
  PduR_Init(NULL);
#endif

#ifdef USE_COM
  Com_Init(NULL);
#endif

#ifdef USE_EEP
  Eep_Init(NULL);
#endif
#ifdef USE_EA
  Ea_Init(NULL);
#endif
#ifdef USE_FLS
  Fls_Init(NULL);
#endif
#ifdef USE_FEE
  Fee_Init(NULL);
#endif
#ifdef USE_NVM
  NvM_Init(NULL);
  while (MEMIF_IDLE != NvM_GetStatus()) {
    MemoryTask();
  }
  NvM_ReadAll();
  while (MEMIF_IDLE != NvM_GetStatus()) {
    MemoryTask();
  }
#endif

#ifdef USE_DEM
  Dem_PreInit();
  Dem_Init(NULL);
#endif
#ifdef USE_DCM
  Dcm_Init(NULL);
#endif

#ifdef USE_TCPIP
  TcpIp_Init(NULL);
#endif
#ifdef USE_SOAD
  SoAd_Init(NULL);
#endif
#ifdef USE_DOIP
  DoIP_Init(NULL);
#endif
#ifdef USE_SD
  Sd_Init(NULL);
#endif
#ifdef USE_SOMEIP
  SomeIp_Init(NULL);
#endif
#ifdef USE_PLUGIN
  plugin_init();
#endif
}
/* ================================ [ FUNCTIONS ] ============================================== */
#ifdef USE_CAN
void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr) {
  ASLOG(CANIF, ("RX bus=%d, canid=%X, dlc=%d, data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n",
                Mailbox->ControllerId, Mailbox->CanId, PduInfoPtr->SduLength,
                PduInfoPtr->SduDataPtr[0], PduInfoPtr->SduDataPtr[1], PduInfoPtr->SduDataPtr[2],
                PduInfoPtr->SduDataPtr[3], PduInfoPtr->SduDataPtr[4], PduInfoPtr->SduDataPtr[5],
                PduInfoPtr->SduDataPtr[6], PduInfoPtr->SduDataPtr[7]));

  if (CANID_P2P_RX == Mailbox->CanId) {
#ifdef USE_CANTP
    CanTp_RxIndication((PduIdType)0, PduInfoPtr);
#endif
  } else if (CANID_P2A_RX == Mailbox->CanId) {
#ifdef USE_CANTP
    CanTp_RxIndication((PduIdType)1, PduInfoPtr);
#endif
  }
#ifdef USE_OSEKNM
  else if ((Mailbox->CanId >= 0x500) && ((Mailbox->CanId <= 0x5FF))) {
    NMPduType NMPDU;
    NMPDU.Source = Mailbox->CanId - 0x500;
    memcpy(&NMPDU.Destination, PduInfoPtr->SduDataPtr, 8);
    OsekNm_RxIndication(Mailbox->ControllerId, &NMPDU);
  }
#endif
#ifdef USE_CANNM
  else if ((Mailbox->CanId >= 0x400) && ((Mailbox->CanId <= 0x4FF))) {
    CanNm_RxIndication(Mailbox->ControllerId, PduInfoPtr);
  }
#endif
#ifdef USE_COM
  else
    COM_RX_FOR_CAN0(Mailbox->CanId, PduInfoPtr)
#endif
}
void CanIf_TxConfirmation(PduIdType CanTxPduId) {
  switch (CanTxPduId) {
#ifdef USE_CANTP
  case 0: /* P2P */
    CanTp_TxConfirmation(0, E_OK);
    break;
  case 1: /* P2A */
    CanTp_TxConfirmation(1, E_OK);
    break;
#endif
#ifdef USE_OSEKNM
  case 2:
    OsekNm_TxConformation((NetIdType)0);
    break;
#endif
#ifdef USE_CANNM
  case 3:
    CanNm_TxConfirmation(0, E_OK);
    break;
#endif
  default:
    break;
  }
#ifdef USE_COM
  if ((CanTxPduId >= COM_ECUC_CAN0_PDUID_MIN) && (CanTxPduId < COM_ECUC_CAN0_PDUID_MAX)) {
    Com_TxConfirmation(CanTxPduId - COM_ECUC_CAN0_PDUID_MIN, E_OK);
  }
#endif
}

Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  Can_PduType canPdu;

  canPdu.swPduHandle = TxPduId;
  canPdu.length = PduInfoPtr->SduLength;
  canPdu.sdu = PduInfoPtr->SduDataPtr;

  if ((0 == TxPduId) || (1 == TxPduId)) {
    canPdu.id = CANID_P2P_TX;
#ifdef USE_DOIP
    if (1 == TxPduId) {
      canPdu.id = CANID_P2A_RX;
    }
#endif
    ret = Can_Write(0, &canPdu);
  }
#ifdef USE_CANNM
  else if (3 == TxPduId) {
    uint8_t NodeId = 0;
    CanNm_GetLocalNodeIdentifier(0, &NodeId);
    canPdu.id = 0x400 + NodeId;
    ret = Can_Write(0, &canPdu);
  }
#endif
  else {
    ASLOG(ERROR, ("CanIf: Invalid TxPudId %d\n", TxPduId));
  }
  return ret;
}
#ifdef USE_OSEKNM
StatusType D_WindowDataReq(NetIdType NetId, NMPduType *NMPDU, uint8_t DataLengthTx) {
  StatusType ercd;
  Can_PduType canPdu;

  canPdu.swPduHandle = 2;
  canPdu.id = 0x500 + NMPDU->Source;
  canPdu.length = DataLengthTx;
  canPdu.sdu = &NMPDU->Destination;

  ercd = Can_Write(NetId, &canPdu);

  return ercd;
}
#endif

#ifdef USE_COM
Std_ReturnType PduR_ComTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  Can_PduType canPdu;

  if ((TxPduId >= COM_ECUC_CAN0_PDUID_MIN) && (TxPduId < COM_ECUC_CAN0_PDUID_MAX)) {
    canPdu.swPduHandle = TxPduId;
    canPdu.length = PduInfoPtr->SduLength;
    canPdu.sdu = PduInfoPtr->SduDataPtr;
    COM_TX_FOR_CAN0(TxPduId, canPdu, PduInfoPtr, ret)
  }

  return ret;
}
#endif
#endif /* USE_CAN */

#ifndef USE_PDUR
Std_ReturnType PduR_DcmTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
#ifdef USE_CANTP
  return CanTp_Transmit(TxPduId, PduInfoPtr);
#endif
#ifdef USE_LINTP
  return LinTp_Transmit(TxPduId, PduInfoPtr);
#endif
#ifdef USE_DOIP
  return DoIP_TpTransmit(TxPduId, PduInfoPtr);
#endif
  return E_NOT_OK;
}
#endif

int main(int argc, char *argv[]) {
  ASLOG(INFO, ("application build @ %s %s\n", __DATE__, __TIME__));

  Mcu_Init(NULL);

  BSW_Init();
  App_Init();
  Std_TimerStart(&timer10ms);
  Std_TimerStart(&timer100ms);
  for (;;) {
    if (Std_GetTimerElapsedTime(&timer10ms) >= 10000) {
      Std_TimerStart(&timer10ms);
      MainTask_10ms();
    }

    if (Std_GetTimerElapsedTime(&timer100ms) >= 100000) {
      Std_TimerStart(&timer100ms);
      App_AliveIndicate();
    }
#ifdef USE_DCM
    Dcm_MainFunction_Request();
#endif
#ifdef USE_CAN
    Can_MainFunction_Write();
    Can_MainFunction_Read();
#endif
#ifdef USE_DLL
    DLL_MainFunction();
    DLL_MainFunction_Read();
#endif
#ifdef USE_TCPIP
    TcpIp_MainFunction();
#endif
#ifdef USE_SOAD
    SoAd_MainFunction();
#endif
    App_MainFunction();
#if defined(_WIN32)
#if !defined(USE_OSEKNM) && !defined(USE_TCPIP)
    usleep(1000);
#endif
#endif
  }

  return 0;
}
