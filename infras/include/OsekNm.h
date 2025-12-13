/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: nm253
 */
#ifndef OSEK_NM_H
#define OSEK_NM_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "NmStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* @ nm253.pdf 4.4.3.1 P98 */
#define OSEKNM_BUS_SLEEP ((OsekNm_ModeNameType)1)
#define OSEKNM_AWAKE ((OsekNm_ModeNameType)2)
/* ================================ [ TYPES     ] ============================================== */
/* @ nm253.pdf 4.3 P89 */
typedef uint8_t OsekNm_NodeIdType;

/* @ nm253.pdf 4.4.5.3.1 P103 */
typedef uint8_t OsekNm_RingDataType[6];

typedef OsekNm_RingDataType *OsekNm_RingDataRefType;

typedef struct {
  uint8_t Source;
  uint8_t Destination;
  uint8_t OpCode;
  OsekNm_RingDataType RingData;
} OsekNm_PduType;

typedef uint8_t OsekNm_ModeNameType;

typedef enum {
  OSEKNM_ROUTINE_BUS_INIT,
  OSEKNM_ROUTINE_BUS_SHUTDOWN,
  OSEKNM_ROUTINE_BUS_RESTART,
  OSEKNM_ROUTINE_BUS_SLEEP,
  OSEKNM_ROUTINE_BUS_AWAKE
} OsekNm_RoutineRefType;

typedef struct OsekNm_Config_s OsekNm_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void OsekNm_Init(const OsekNm_ConfigType *ConfigPtr);
Std_ReturnType OsekNm_Start(NetworkHandleType NetId);
Std_ReturnType OsekNm_Silent(NetworkHandleType NetId);
Std_ReturnType OsekNm_Talk(NetworkHandleType NetId);
Std_ReturnType OsekNm_GotoMode(NetworkHandleType NetId, OsekNm_ModeNameType NewMode);
Std_ReturnType OsekNm_Stop(NetworkHandleType NetId);

Std_ReturnType OsekNm_NetworkRequest(NetworkHandleType nmChannelHandle);
Std_ReturnType OsekNm_NetworkRelease(NetworkHandleType nmChannelHandle);

Std_ReturnType OsekNm_GetState(NetworkHandleType NetId, Nm_ModeType *nmModePtr);

void OsekNm_TxConfirmation(PduIdType NetId, Std_ReturnType result);
void OsekNm_RxIndication(PduIdType NetId, const PduInfoType *PduInfoPtr);
void OsekNm_WakeupIndication(NetworkHandleType NetId);
void OsekNm_BusErrorIndication(NetworkHandleType NetId);

void OsekNm_MainFunction(void);

void OsekNm_D_Init(NetworkHandleType NetId, OsekNm_RoutineRefType Routine);
void OsekNm_D_Offline(NetworkHandleType NetId);
void OsekNm_D_Online(NetworkHandleType NetId);
Std_ReturnType OsekNm_D_WindowDataReq(NetworkHandleType NetId, OsekNm_PduType *nmPdu,
                                      uint8_t DataLengthTx);

void OsekNm_GetVersionInfo(Std_VersionInfoType *versionInfo);
#endif /* OSEK_NM_H */
