/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 - 2022 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Interface AUTOSAR CP Release 4.4.0
 */
#ifndef CANIF_PRIV_H
#define CANIF_PRIV_H
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

#ifndef CANIF_RX_PACKET_DATA_SIZE
#define CANIF_RX_PACKET_DATA_SIZE 64u
#endif

#ifndef CANIF_TX_PACKET_DATA_SIZE
#define CANIF_TX_PACKET_DATA_SIZE 64u
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef void (*CanIf_RxIndicationFncType)(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
typedef void (*CanIf_TxConfirmationFncType)(PduIdType TxPduId, Std_ReturnType result);

typedef struct {
  CanIf_RxIndicationFncType RxInd;
  PduIdType RxPduId;
  Can_IdType CanId;
  Can_IdType Mask; /* if RxCanId&mask == canid, then process */
  Can_HwHandleType Hoh;
} CanIf_RxPduType;

typedef struct {
  CanIf_TxConfirmationFncType TxConfirm;
  PduIdType TxPduId;
  Can_IdType CanId;
  Can_IdType *CanIdPtr;
  Can_HwHandleType Hoh;
  uint8_t ControllerId;
#if CANIF_TX_PACKET_POOL_SIZE > 0
  /** @param UseTxPool for important message such as E2E message from Com, to ensure data atomic,
   * ensure UseTxPool = true. */
  boolean UseTxPool;
#endif
} CanIf_TxPduType;

typedef struct {
  CanIf_PduModeType PduMode;
#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
  uint16_t TxTimeoutTimer;
#endif
#ifdef USE_MIRROR
  boolean MirroringActive;
#endif
} CanIf_CtrlContextType;

typedef struct {
  const CanIf_RxPduType *RxPdus;
  uint16_t NumOfRxPdus;
#if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)
  uint16_t TxTimeout;
#endif
} CanIf_CtrlConfigType;

struct CanIf_Config_s {
  const CanIf_TxPduType *TxPdus;
  CanIf_CtrlContextType *CtrlContexts;
  const CanIf_CtrlConfigType *CtrlConfigs;
  uint16_t NumOfTxPdus;
  uint8_t NumOfCtrls;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* CANIF_PRIV_H */
