/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 *
 * ref: https://www.autosar.org/fileadmin/standards/R20-11/CP/AUTOSAR_SWS_TimeSyncOverCAN.pdf
 */
#ifndef __CAN_TSYNC_PRIV_H__
#define __CAN_TSYNC_PRIV_H__
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "StbM.h"
/* ================================ [ MACROS    ] ============================================== */
#define CANTSYN_CRC_IGNORED ((CanTSyn_RxCrcValidatedType)0x00)
#define CANTSYN_CRC_NOT_VALIDATED ((CanTSyn_RxCrcValidatedType)0x01)
#define CANTSYN_CRC_OPTIONAL ((CanTSyn_RxCrcValidatedType)0x02)
#define CANTSYN_CRC_VALIDATED ((CanTSyn_RxCrcValidatedType)0x03)

#define CANTSYN_MASTER ((CanTSyn_DomainTypeType)0x00)
#define CANTSYN_SLAVE ((CanTSyn_DomainTypeType)0x01)

/* @SWS_CanTSyn_00015 */
#define CANTSYN_SYNC_NO_CRC 0x10
/* @SWS_CanTSyn_00016 */
#define CANTSYN_FUP_NO_CRC 0x18
/* @SWS_CanTSyn_00017 */
#define CANTSYN_SYNC_WITH_CRC 0x20
/* @SWS_CanTSyn_00018 */
#define CANTSYN_FUP_WITH_CRC 0x28
/* @SWS_CanTSyn_00126 */
#define CANTSYN_OFS_NO_CRC 0x34
/* @SWS_CanTSyn_00127 */
#define CANTSYN_OFNS_NO_CRC 0x3C
/* @SWS_CanTSyn_00128 */
#define CANTSYN_OFS_WITH_CRC 0x44
/* @SWS_CanTSyn_00129 */
#define CANTSYN_OFNS_WITH_CRC 0x4C
/* @SWS_CanTSyn_00111 */
#define CANTSYN_EXT_OFS_NO_CRC 0x54
/* @SWS_CanTSyn_00112 */
#define CANTSYN_EXT_OFS_WITH_CRC 0x64

#define CANTSYN_SLAVE_IDLE ((CanTSync_SlaveStateType)0x00)
#define CANTSYN_SLAVE_SYNC ((CanTSync_SlaveStateType)0x01)

#define CANTSYN_MASTER_IDLE ((CanTSync_MasterStateType)0x00)
#define CANTSYN_MASTER_SYNC ((CanTSync_MasterStateType)0x01)
#define CANTSYN_MASTER_SYNCED ((CanTSync_MasterStateType)0x02)
#define CANTSYN_MASTER_FUP ((CanTSync_MasterStateType)0x03)
#define CANTSYN_MASTER_FUPED ((CanTSync_MasterStateType)0x04)
/* ================================ [ TYPES     ] ============================================== */
typedef uint8_t CanTSyn_RxCrcValidatedType; /* @ECUC_CanTSyn_00021 */

typedef uint8_t CanTSyn_DomainTypeType;

/* @ECUC_CanTSyn_00009 */
typedef struct {
  PduIdType ConfirmationHandleId;
  PduIdType GlobalTimePduRef;
} CanTSyn_GlobalTimeMasterPduType;

/* @ECUC_CanTSyn_00007 */
typedef struct {
  uint16_t CyclicMsgResumeTime;    /* @ECUC_CanTSyn_00044: in ms */
  uint16_t GlobalTimeDebounceTime; /* @ECUC_CanTSyn_00045: in ms */
  uint16_t GlobalTimeTxPeriod;     /* @ECUC_CanTSyn_00017: in ms */
  PduIdType TxPduId;
  boolean GlobalTimeTxCrcSecured;  /* @ECUC_CanTSyn_00015: if true crc supported else not*/
  boolean ImmediateTimeSync;       /* @ECUC_CanTSyn_00043 */
  boolean TxTmacCalculated;        /* @ECUC_CanTSyn_00047: if true TMAC calculated else not */
} CanTSyn_GlobalTimeMasterType;

/* @ECUC_CanTSyn_00012 */
typedef struct {
  uint16_t GlobalTimeFollowUpTimeout;         /* @ECUC_CanTSyn_00006: in ms */
  uint16_t GlobalTimeMinMsgGap;               /* @ECUC_CanTSyn_00049: in ms */
  uint8_t GlobalTimeSequenceCounterJumpWidth; /* @ECUC_CanTSyn_00011: range 1 - 15 */
  CanTSyn_RxCrcValidatedType RxCrcValidated;
  boolean RxTmacValidated; /* @ECUC_CanTSyn_0000048: if true TMAC validated else not */
} CanTSyn_GlobalTimeSlaveType;

typedef uint8_t CanTSync_MasterStateType;

typedef struct {
  StbM_VirtualLocalTimeType t0r;
  StbM_VirtualLocalTimeType t1r;
  uint16_t timer;
  uint16_t debounceCounter;
  CanTSync_MasterStateType state;
  uint8_t timeBaseUpdateCounter;
  uint8_t SC;
} CanTSyn_MasterContextType;

typedef uint8_t CanTSync_SlaveStateType;

typedef struct {
  uint32_t st0r;
  StbM_VirtualLocalTimeType t2r;
  uint16_t timer;
  CanTSync_SlaveStateType state;
  uint8_t SC;
} CanTSyn_SlaveContextType;

/* @ECUC_CanTSyn_00004  */
typedef struct {
  union {
    struct {
      void *context;
      const void *config;
    } V;
    struct {
      CanTSyn_MasterContextType *context;
      const CanTSyn_GlobalTimeMasterType *Master;
    } M;
    struct {
      CanTSyn_SlaveContextType *context;
      const CanTSyn_GlobalTimeSlaveType *Slave;
    } S;
  } U;
  boolean EnableTimeValidation;                          /* @ECUC_CanTSyn_00051  */
  uint8_t GlobalTimeDomainId;                            /* @ECUC_CanTSyn_00005: range 0 .. 31 */
  uint8_t GlobalTimeNetworkSegmentId;                    /* @ECUC_CanTSyn_00052 */
  uint8_t GlobalTimeSecureTmacLength;                    /* @ECUC_CanTSyn_00046: range 0 .. 16 */
  boolean UseExtendedMsgFormat;                          /* @ECUC_CanTSyn_00042 */
  StbM_SynchronizedTimeBaseType SynchronizedTimeBaseRef; /* @ECUC_CanTSyn_00022 */
  CanTSyn_DomainTypeType DomainType;
} CanTSyn_GlobalTimeDomainType;

struct CanTSyn_Config_s {
  const CanTSyn_GlobalTimeDomainType *GlobalTimeDomains;
  uint8_t numOfGlobalTimeDomains;
};

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */

#endif /* __CAN_TSYNC_PRIV_H__ */
