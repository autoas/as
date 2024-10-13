/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Communication AUTOSAR CP Release 4.4.0
 */
#ifndef _COM_PRIV_H
#define _COM_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#define COM_ACTION_NONE ((Com_DataActionType)0x00)
#define COM_ACTION_NOTIFY ((Com_DataActionType)0x01)
#define COM_ACTION_REPLACE ((Com_DataActionType)0x02)
#define COM_ACTION_SUBSTITUTE ((Com_DataActionType)0x03)

#define BIG ((Com_SignalEndiannessType)0x00)
#define LITTLE ((Com_SignalEndiannessType)0x01)
#define OPAQUE ((Com_SignalEndiannessType)0x02)

#define COM_SINT8N COM_UINT8N

#define COM_UPDATE_BIT_NOT_USED ((uint16_t)0xFFFF)
/* ================================ [ TYPES     ] ============================================== */
/* maximum 16 groups supported by this implementataion */
typedef uint16_t Com_GroupMaskType;

/* @SWS_Com_00468 */
typedef void (*Com_CbkTxAckFncType)(void);

/* @SWS_Com_00491 */
typedef void (*Com_CbkTxErrFncType)(void);

/* @SWS_Com_00554 */
typedef void (*Com_CbkTxTOutFncType)(void);

/* @SWS_Com_00555 */
typedef void (*Com_CbkRxAckFncType)(void);

/* @SWS_Com_00556 */
typedef void (*Com_CbkRxTOutFncType)(void);

/* @SWS_Com_00536 */
typedef void (*Com_CbkInvFncType)(void);

/* @SWS_Com_00726 */
typedef void (*Com_CbkCounterErrFncType)(void);

/* @SWS_Com_00700 */
typedef boolean (*Com_RxIpduCalloutFncType)(PduIdType PduId, const PduInfoType *PduInfoPtr);

/* @SWS_Com_00346 */
typedef boolean (*Com_TxIpduCalloutFncType)(PduIdType PduId, const PduInfoType *PduInfoPtr);

typedef uint8_t Com_DataActionType;

typedef uint8_t Com_SignalEndiannessType;

typedef struct {
  uint16_t timer;
} Com_SignalRxContextType;

typedef struct {
  Com_SignalRxContextType *context;
  Com_CbkInvFncType InvalidNotification;
  Com_CbkRxAckFncType RxNotification;
  Com_CbkRxTOutFncType RxTOut;
  const uint8_t *TimeoutSubstitutionValue;
  uint16_t FirstTimeout;
  uint16_t Timeout;
  Com_DataActionType DataInvalidAction;   /* NOTIFY / REPLACE */
  Com_DataActionType RxDataTimeoutAction; /* NONE / REPLACE / SUBSTITUTE */
} Com_SignalRxConfigType;

typedef struct {
  Com_CbkTxErrFncType ErrorNotification;
  Com_CbkTxAckFncType TxNotification;
} Com_SignalTxConfigType;

/* @SWS_Com_00675 */
typedef enum {
  COM_SINT8,
  COM_UINT8,
  COM_SINT16,
  COM_UINT16,
  COM_SINT32,
  COM_UINT32,
  COM_UINT8N,
  COM_UINT8_DYN,
} Com_SignalTypeType;

/* @ECUC_Com_00344 */
typedef struct {
#ifdef USE_SHELL
  char *name;
#endif
  void *ptr;
  const void *initPtr; /* or shadowPtr for group signal */
  Com_SignalTypeType type;
  Com_SignalIdType HandleId;
  PduIdType PduId;
  uint16_t BitPosition;
  uint16_t BitSize;
#ifdef COM_USE_SIGNAL_UPDATE_BIT
  uint16_t UpdateBit;
#endif
  Com_SignalEndiannessType Endianness;
#ifdef COM_USE_SIGNAL_CONFIG
  const Com_SignalRxConfigType *rxConfig;
  const Com_SignalTxConfigType *txConfig;
#endif
  bool isGroupSignal;
} Com_SignalConfigType;

typedef struct {
  uint16_t timer;
} Com_IPduRxContextType;

typedef struct {
  Com_IPduRxContextType *context;
  Com_CbkRxAckFncType RxNotification;
  Com_CbkRxTOutFncType RxTOut;
  uint16_t FirstTimeout;
  uint16_t Timeout;
} Com_IPduRxConfigType;

typedef struct {
  uint16_t timer;
#ifdef COM_USE_MAIN_FAST
  boolean bTxRetry;
#endif
} Com_IPduTxContextType;

typedef struct {
  Com_IPduTxContextType *context;
  Com_CbkTxErrFncType ErrorNotification;
  Com_CbkTxAckFncType TxNotification;
  Com_TxIpduCalloutFncType TxIpduCallout;
  uint16_t FirstTime;
  uint16_t CycleTime;
  PduIdType TxPduId;
} Com_IPduTxConfigType;

typedef struct {
#ifdef USE_SHELL
  char *name;
#endif
  void *ptr;
  uint8_t *dynLen;
  const Com_SignalConfigType **signals;
  const Com_IPduRxConfigType *rxConfig;
  const Com_IPduTxConfigType *txConfig;
  Com_GroupMaskType GroupRefMask;
  uint8_t length;
  uint8_t numOfSignals;
} Com_IPduConfigType;

typedef struct {
  Com_GroupMaskType GroupStatus;
} Com_GlobalContextType;

struct Com_Config_s {
  const Com_IPduConfigType *IPduConfigs;
  const Com_SignalConfigType *SignalConfigs;
  Com_GlobalContextType *context;
  uint16_t numOfIPdus;
  uint16_t numOfSignals;
  uint8_t numOfGroups;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _COM_PRIV_H */
