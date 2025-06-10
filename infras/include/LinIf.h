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

#define LINIF_R_OK E_OK
#define LINIF_R_NOT_OK E_NOT_OK
#define LINIF_R_RECEIVED_OK ((Std_ReturnType)0x10)
#define LINIF_R_TRIGGER_TRANSMIT ((Std_ReturnType)0x12)
#define LINIF_R_TX_COMPLETED ((Std_ReturnType)0x13)

/* @SWS_LinIf_00376 */
#define LINIF_E_UNINIT 0x00
#define LINIF_E_INIT_FAILED 0x10
#define LINIF_E_NONEXISTENT_CHANNEL 0x20
#define LINIF_E_PARAMETER 0x30
#define LINIF_E_PARAM_POINTER 0x40
#define LINIF_E_SCHEDULE_REQUEST_ERROR 0x51
#define LINIF_E_TRCV_INV_MODE 0x53
#define LINIF_E_TRCV_NOT_NORMAL 0x54
#define LINIF_E_PARAM_WAKEUPSOURCE 0x55

#define LINIF_VARIANT_MASTER 0x01
#define LINIF_VARIANT_SLAVE 0x02
#define LINIF_VARIANT_BOTH (LINIF_VARIANT_MASTER | LINIF_VARIANT_SLAVE)
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_LinIf_00197 */
typedef uint8_t LinIf_SchHandleType;

typedef enum {
  LINIF_UNCONDITIONAL,
  LINIF_EVENT,
  LINIF_SPORADIC,
  LINIF_DIAG_MRF,
  LINIF_DIAG_SRF,
} LinIf_FrameTypeType;

/* @SWS_LinIf_00629 */
typedef enum {
  LINTP_APPLICATIVE_SCHEDULE,
  LINTP_DIAG_REQUEST,
  LINTP_DIAG_RESPONSE
} LinTp_ModeType;

typedef Std_ReturnType (*LinIf_NotificationCallbackType)(uint8_t channel, Lin_PduType *frame,
                                                         Std_ReturnType notifyResult);
/* @SWS_LinIf_00668 */
typedef struct LinIf_Config_s LinIf_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_LinIf_00198 */
void LinIf_Init(const LinIf_ConfigType *ConfigPtr);

void LinIf_DeInit(void);

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

void LinIf_MainFunction_Read(void);

/* for slave */
/* @SWS_LinIf_91004 */
Std_ReturnType LinIf_HeaderIndication(NetworkHandleType Channel, Lin_PduType *PduPtr);

/* @SWS_LinIf_91005 */
void LinIf_RxIndication(NetworkHandleType Channel, uint8 *Lin_SduPtr);

/* @SWS_LinIf_91006 */
void LinIf_TxConfirmation(NetworkHandleType Channel);

/* @SWS_LinIf_91007 */
void LinIf_LinErrorIndication(NetworkHandleType Channel, Lin_SlaveErrorType ErrorStatus);
#endif /* LINIF_H */
