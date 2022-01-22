/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of LIN Interface AUTOSAR CP Release 4.4.0
 */
#ifndef LINIF_H
#define LINIF_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "Lin.h"
/* ================================ [ MACROS    ] ============================================== */
#define LINIF_NULL_SCHEDULE (LinIf_SchHandleType)0x00
#define LINIF_INVALD_SCHEDULE_TABLE (LinIf_SchHandleType)0xFF
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_LinIf_00197 */
typedef uint8_t LinIf_SchHandleType;

typedef enum
{
  LINIF_R_OK = 0,
  LINIF_R_NOT_OK,
  LINIF_R_RECEIVED_OK,
  LINIF_R_TRIGGER_TRANSMIT,
  LINIF_R_TX_COMPLETED,
} LinIf_ResultType;

typedef enum
{
  LINIF_UNCONDITIONAL,
  LINIF_EVENT,
  LINIF_SPORADIC,
  LINIF_DIAG_MRF,
  LINIF_DIAG_SRF,
} LinIf_FrameTypeType;

/* @SWS_LinIf_00629 */
typedef enum
{
  LINTP_APPLICATIVE_SCHEDULE,
  LINTP_DIAG_REQUEST,
  LINTP_DIAG_RESPONSE
} LinTp_Mode;

typedef LinIf_ResultType (*LinIf_NotificationCallbackType)(uint8_t channel, Lin_PduType *frame,
                                                           LinIf_ResultType notifyResult);

typedef struct {
  Lin_FramePidType id;
  Lin_FrameDlType dlc;
  LinIf_FrameTypeType type;
  Lin_FrameCsModelType Cs;
  Lin_FrameResponseType Drc;
  LinIf_NotificationCallbackType callback;
  uint32_t delay; /* delay in us */
} LinIf_ScheduleTableEntryType;

typedef struct {
  const LinIf_ScheduleTableEntryType *entrys;
  uint8_t numOfEntries;
} LinIf_ScheduleTableType;

typedef struct {
  NetworkHandleType linChannel;
} LinIf_ChannelConfigType;

typedef uint8_t LinIf_ChannelStatusType;

/* @SWS_LinIf_00668 */
typedef struct LinIf_Config_s LinIf_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_LinIf_00198 */
void LinIf_Init(const LinIf_ConfigType *ConfigPtr);

void LinIf_DeInit();

/* @SWS_LinIf_00201 */
Std_ReturnType LinIf_Transmit(PduIdType LinTxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_LinIf_00202 */
Std_ReturnType LinIf_ScheduleRequest(NetworkHandleType Channel, LinIf_SchHandleType Schedule);

/* @SWS_LinIf_00204 */
Std_ReturnType LinIf_GotoSleep(NetworkHandleType Channel);

/* @SWS_LinIf_00205 */
Std_ReturnType LinIf_WakeUp(NetworkHandleType Channel);

/* @SWS_LinIf_00384 */
void LinIf_MainFunction(void);
#endif /* LINIF_H */
