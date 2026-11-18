/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Communication AUTOSAR CP Release 4.4.0
 */
#ifndef COM_PRIV_H
#define COM_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#define DET_THIS_MODULE_ID MODULE_ID_COM

#define COM_ACTION_NONE ((Com_DataActionType)0x00)
#define COM_ACTION_NOTIFY ((Com_DataActionType)0x01)
#define COM_ACTION_REPLACE ((Com_DataActionType)0x02)
#define COM_ACTION_SUBSTITUTE ((Com_DataActionType)0x03)

#define COM_BIG_ENDIAN ((Com_SignalEndiannessType)0x00)
#define COM_LITTLE_ENDIAN ((Com_SignalEndiannessType)0x01)
#define COM_OPAQUE ((Com_SignalEndiannessType)0x02)

#define COM_SINT8N COM_UINT8N

#define COM_SINT8 ((Com_SignalTypeType)0)
#define COM_UINT8 ((Com_SignalTypeType)1)
#define COM_SINT16 ((Com_SignalTypeType)2)
#define COM_UINT16 ((Com_SignalTypeType)3)
#define COM_SINT32 ((Com_SignalTypeType)4)
#define COM_UINT32 ((Com_SignalTypeType)5)
#define COM_UINT8N ((Com_SignalTypeType)6)
#define COM_UINT8_DYN ((Com_SignalTypeType)7)

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

typedef uint16_t Com_DataLengthType;

typedef struct {
  uint16_t Timer;
} Com_SignalRxContextType;

typedef struct {
  Com_SignalRxContextType *Context;
#ifdef COM_USE_SIGNAL_RX_INVALID_NOTIFICATION
  Com_CbkInvFncType InvalidNotification;
#endif
#ifdef COM_USE_SIGNAL_RX_NOTIFICATION
  Com_CbkRxAckFncType RxNotification;
#endif
#ifdef COM_USE_SIGNAL_RX_TIMEOUT
  Com_CbkRxTOutFncType RxTOut;
#endif
  const uint8_t *TimeoutSubstitutionValue;
  uint16_t FirstTimeout;
  uint16_t Timeout;
  Com_DataActionType DataInvalidAction;   /* NOTIFY / REPLACE */
  Com_DataActionType RxDataTimeoutAction; /* NONE / REPLACE / SUBSTITUTE */
} Com_SignalRxConfigType;

typedef struct {
#ifdef COM_USE_SIGNAL_TX_ERROR_NOTIFICATION
  Com_CbkTxErrFncType ErrorNotification;
#endif
#ifdef COM_USE_SIGNAL_TX_NOTIFICATION
  Com_CbkTxAckFncType TxNotification;
#endif
#if !defined(COM_USE_SIGNAL_TX_ERROR_NOTIFICATION) && !defined(COM_USE_SIGNAL_TX_NOTIFICATION)
  uint8_t Dummy;
#endif
} Com_SignalTxConfigType;

/* @SWS_Com_00675 */
typedef uint8_t Com_SignalTypeType;

/* @ECUC_Com_00344 */
typedef struct {
#ifdef USE_SHELL
  char *Name;
#endif
  void *Ptr;
  const void *InitPtr; /* or shadowPtr for group signal */
#ifdef COM_USE_SIGNAL_CONFIG
  const Com_SignalRxConfigType *RxConfig;
  const Com_SignalTxConfigType *TxConfig;
#endif
  Com_SignalIdType HandleId;
  PduIdType PduId;
  uint16_t BitPosition;
  uint16_t BitSize;
#ifdef COM_USE_SIGNAL_UPDATE_BIT
  uint16_t UpdateBit;
#endif
  Com_SignalTypeType Type;
  Com_SignalEndiannessType Endianness;
  boolean IsGroupSignal;
} Com_SignalConfigType;

typedef struct {
  uint16_t Timer;
} Com_IPduRxContextType;

typedef struct {
  Com_IPduRxContextType *Context;
#ifdef COM_USE_RX_NOTIFICATION
  Com_CbkRxAckFncType RxNotification;
#endif
#ifdef COM_USE_RX_TIMEOUT
  Com_CbkRxTOutFncType RxTOut;
#endif
#ifdef COM_USE_RX_IPDU_CALLOUT
  Com_RxIpduCalloutFncType RxIpduCallout;
#endif
  uint16_t FirstTimeout;
  uint16_t Timeout;
} Com_IPduRxConfigType;

typedef struct {
  uint16_t Timer;
#ifdef COM_USE_MAIN_FAST
  boolean TxRetry;
#endif
} Com_IPduTxContextType;

typedef struct {
  /* For LIN, context is NULL */
  Com_IPduTxContextType *Context;
#ifdef COM_USE_TX_ERROR_NOTIFICATION
  Com_CbkTxErrFncType ErrorNotification;
#endif
#ifdef COM_USE_TX_NOTIFICATION
  Com_CbkTxAckFncType TxNotification;
#endif
#ifdef COM_USE_TX_IPDU_CALLOUT
  Com_TxIpduCalloutFncType TxIpduCallout;
#endif
  uint16_t FirstTime;
  uint16_t CycleTime;
  PduIdType TxPduId;
} Com_IPduTxConfigType;

typedef struct {
#ifdef USE_SHELL
  char *Name;
#endif
  void *Ptr;
  Com_DataLengthType *DynLen;
  const Com_SignalConfigType **Signals;
  const Com_IPduRxConfigType *RxConfig;
  const Com_IPduTxConfigType *TxConfig;
  Com_GroupMaskType GroupRefMask;
  Com_DataLengthType Length;
  uint8_t NumOfSignals;
} Com_IPduConfigType;

typedef struct {
  Com_GroupMaskType GroupStatus;
#ifdef USE_DCM
  Com_DcmComModeType DcmComMode;
#endif
} Com_GlobalContextType;

struct Com_Config_s {
  const Com_IPduConfigType *IPduConfigs;
  const Com_SignalConfigType *SignalConfigs;
  Com_GlobalContextType *Context;
  uint16_t NumOfIPdus;
  uint16_t NumOfSignals;
  uint8_t NumOfGroups;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* COM_PRIV_H */
