/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of LIN Driver AUTOSAR CP Release 4.4.0
 */
#ifndef LIN_H
#define LIN_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#define LIN_CLASSIC_CS ((Lin_FrameCsModelType)0)
#define LIN_ENHANCED_CS ((Lin_FrameCsModelType)1)
#define LIN_DISABLE_CS ((Lin_FrameCsModelType)2)

#define LIN_FRAMERESPONSE_TX ((Lin_FrameResponseType)0)
#define LIN_FRAMERESPONSE_RX ((Lin_FrameResponseType)1)
#define LIN_FRAMERESPONSE_IGNORE ((Lin_FrameResponseType)2)
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_Lin_00228 */
#ifdef LIN_USE_EXT_ID
typedef uint32_t Lin_FramePidType;
#else
typedef uint8_t Lin_FramePidType;
#endif
/* @SWS_Lin_00229 */
typedef uint8_t Lin_FrameCsModelType;

/* @SWS_Lin_00230 */
typedef uint8_t Lin_FrameResponseType;

/* @SWS_Lin_00231 */
typedef uint8_t Lin_FrameDlType;

/* @SWS_Lin_00232 */
typedef struct {
  Lin_FramePidType Pid;
  Lin_FrameCsModelType Cs;
  Lin_FrameResponseType Drc;
  Lin_FrameDlType Dl;
  uint8_t *SduPtr;
} Lin_PduType;

/* @SWS_Lin_00233 */
typedef enum
{
  LIN_NOT_OK,
  LIN_TX_OK,
  LIN_TX_BUSY,
  LIN_TX_HEADER_ERROR,
  LIN_TX_ERROR,
  LIN_RX_OK,
  LIN_RX_BUSY,
  LIN_RX_ERROR,
  LIN_RX_NO_RESPONSE,
  LIN_OPERATIONAL,
  LIN_CH_SLEEP
} Lin_StatusType;

/* @SWS_Lin_91140 */
typedef enum
{
  LIN_ERR_HEADER,
  LIN_ERR_RESP_STOPBIT,
  LIN_ERR_RESP_CHKSUM,
  LIN_ERR_RESP_DATABIT,
  LIN_ERR_NO_RESP,
  LIN_ERR_INC_RESP,
  /* extended HW error */
  LIN_ERR_RECEIVED_IN_WRONG_STATE,
  LIN_ERR_RECEPTION_TIMEOUT,
  LIN_ERR_BIT_ERROR,
  LIN_ERR_BUFFER_OVERRUN,
} Lin_SlaveErrorType;

typedef enum
{
  LIN_CS_UNINIT,
  LIN_CS_STARTED,
  LIN_CS_STOPPED,
  LIN_CS_SLEEP
} Lin_ControllerStateType;

/* extended LIN error type */
typedef Lin_SlaveErrorType Lin_ErrorType;

typedef struct Lin_Config_s Lin_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_Lin_00006 */
void Lin_Init(const Lin_ConfigType *ConfigPtr);

Std_ReturnType Lin_SetControllerMode(uint8_t Channel, Lin_ControllerStateType Transition);

/* @SWS_Lin_00191 */
Std_ReturnType Lin_SendFrame(uint8_t Channel, const Lin_PduType *PduInfoPtr);
/* @SWS_Lin_00168 */
Lin_StatusType Lin_GetStatus(uint8_t Channel, uint8_t **Lin_SduPtr);

void Lin_RxIndication(uint8_t channel, uint8_t *data, uint8_t size);
void Lin_MainFunction(void);
void Lin_MainFunction_Write(uint8_t channel);
void Lin_MainFunction_Read(void);
void Lin_ErrorIntication(uint8_t channel, Lin_ErrorType error);
#endif /* LIN_H */
