/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 - 2022 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Interface AUTOSAR CP Release 4.4.0
 */
#ifndef _CANIF_PRIV_H
#define _CANIF_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"
/* ================================ [ MACROS    ] ============================================== */
#define DET_THIS_MODULE_ID MODULE_ID_CANIF

#ifndef CANIF_RX_PACKET_POOL_SIZE
#define CANIF_RX_PACKET_POOL_SIZE 0u
#endif

#ifndef CANIF_TX_PACKET_POOL_SIZE
#define CANIF_TX_PACKET_POOL_SIZE 0u
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef void (*CanIf_RxIndicationFncType)(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
typedef void (*CanIf_TxConfirmationFncType)(PduIdType TxPduId, Std_ReturnType result);

typedef struct {
  CanIf_RxIndicationFncType rxInd;
  PduIdType rxPduId;
  Can_IdType canid;
  Can_IdType mask; /* if RxCanId&mask == canid, then process */
  Can_HwHandleType hoh;
} CanIf_RxPduType;

typedef struct {
  CanIf_TxConfirmationFncType txConfirm;
  PduIdType txPduId;
  Can_IdType canid;
  Can_IdType *p_canid;
  Can_HwHandleType hoh;
  uint8_t ControllerId;
#if CANIF_TX_PACKET_POOL_SIZE > 0
  /** @param bUseTxPool for important message such as E2E message from Com, to ensure data atomic,
   * ensure bUseTxPool = true. */
  boolean bUseTxPool;
#endif
} CanIf_TxPduType;

typedef struct {
  CanIf_PduModeType PduMode;
#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
  uint16_t txTimeoutTimer;
#endif
#ifdef USE_MIRROR
  boolean bMirroringActive;
#endif
} CanIf_CtrlContextType;

typedef struct {
  const CanIf_RxPduType *rxPdus;
  uint16_t numOfRxPdus;
#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
  uint16_t txTimerout;
#endif
} CanIf_CtrlConfigType;

struct CanIf_Config_s {
  const CanIf_TxPduType *txPdus;
  CanIf_CtrlContextType *CtrlContexts;
  const CanIf_CtrlConfigType *CtrlConfigs;
  uint16_t numOfTxPdus;
  uint8_t numOfCtrls;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _CANIF_PRIV_H */
