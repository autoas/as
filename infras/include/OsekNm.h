/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: nm253
 */
#ifndef _OSEK_NM_H
#define _OSEK_NM_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "NmStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* @ nm253.pdf 4.3 P89 */
typedef uint8_t NodeIdType;
typedef uint8_t NetIdType;

/* @ nm253.pdf 4.4.5.3.1 P103 */
typedef uint8_t RingDataType[6];
typedef RingDataType *RingDataRefType;

typedef struct {
  uint8_t Source;
  uint8_t Destination;
  union {
    uint8_t b;
    struct {
      uint8_t Alive : 1;
      uint8_t Ring : 1;
      uint8_t Limphome : 1;
      uint8_t reserved1 : 1;
      uint8_t SleepInd : 1;
      uint8_t SleepAck : 1;
      uint8_t reserved2 : 2;
    } B;
  } OpCode;
  RingDataType RingData;
} NMPduType;

/* @ nm253.pdf 4.4.3.1 P98 */
typedef enum
{
  NM_BusSleep = 1,
  NM_Awake
} NMModeName;

typedef enum
{
  BusInit,
  BusShutDown,
  BusRestart,
  BusSleep,
  BusAwake
} RoutineRefType;

typedef struct OsekNm_Config_s OsekNm_ConfigType;
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void OsekNm_Init(const OsekNm_ConfigType *ConfigPtr);
StatusType StartNM(NetIdType NetId);
StatusType SilentNM(NetIdType);
StatusType TalkNM(NetIdType);
StatusType GotoMode(NetIdType NetId, NMModeName NewMode);


StatusType OsekNm_GetState(NetIdType NetId, Nm_ModeType *nmModePtr);

void OsekNm_TxConfirmation(PduIdType NetId, Std_ReturnType result);
void OsekNm_RxIndication(PduIdType NetId, const PduInfoType *PduInfoPtr);
void OsekNm_WakeupIndication(NetIdType NetId);
void OsekNm_BusErrorIndication(NetIdType NetId);

void OsekNm_MainFunction(void);

void D_Init(NetIdType NetId, RoutineRefType Routine);
void D_Offline(NetIdType NetId);
void D_Online(NetIdType NetId);
StatusType D_WindowDataReq(NetIdType NetId, NMPduType *nmPdu, uint8_t DataLengthTx);
#endif /* _OSEK_NM_H */
