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
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define LIN_CLASSIC_CS ((Lin_FrameCsModelType)0)
#define LIN_ENHANCED_CS ((Lin_FrameCsModelType)1)
#define LIN_DISABLE_CS ((Lin_FrameCsModelType)2)

#define LIN_FRAMERESPONSE_TX ((Lin_FrameResponseType)0)
#define LIN_FRAMERESPONSE_RX ((Lin_FrameResponseType)1)
#define LIN_FRAMERESPONSE_IGNORE ((Lin_FrameResponseType)2)

#define LIN_CS_UNINIT ((Lin_ControllerStateType)0)
#define LIN_CS_STARTED ((Lin_ControllerStateType)1)
#define LIN_CS_STOPPED ((Lin_ControllerStateType)2)
#define LIN_CS_SLEEP ((Lin_ControllerStateType)3)

/* @SWS_Lin_00048 */
#define LIN_E_UNINIT 0x00
#define LIN_E_INVALID_CHANNEL 0x02
#define LIN_E_INVALID_POINTER 0x03
#define LIN_E_STATE_TRANSITION 0x04
#define LIN_E_PARAM_POINTER 0x05

#define LIN_NOT_OK ((Lin_StatusType)0)
#define LIN_TX_OK ((Lin_StatusType)1)
#define LIN_TX_BUSY ((Lin_StatusType)2)
#define LIN_TX_HEADER_ERROR ((Lin_StatusType)3)
#define LIN_TX_ERROR ((Lin_StatusType)4)
#define LIN_RX_OK ((Lin_StatusType)5)
#define LIN_RX_BUSY ((Lin_StatusType)6)
#define LIN_RX_ERROR ((Lin_StatusType)7)
#define LIN_RX_NO_RESPONSE ((Lin_StatusType)8)
#define LIN_OPERATIONAL ((Lin_StatusType)9)
#define LIN_CH_SLEEP ((Lin_StatusType)10)
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
  uint8_t *SduPtr;
  Lin_FramePidType Pid;
  Lin_FrameCsModelType Cs;
  Lin_FrameResponseType Drc;
  Lin_FrameDlType Dl;
} Lin_PduType;

/* @SWS_Lin_00233 */
typedef uint8_t Lin_StatusType;

/* @SWS_Lin_91140 */
typedef enum {
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

typedef uint8_t Lin_ControllerStateType;

/* extended LIN error type */
typedef Lin_SlaveErrorType Lin_ErrorType;

typedef struct Lin_Config_s Lin_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_Lin_00006 */
void Lin_Init(const Lin_ConfigType *ConfigPtr);

/* @SWS_Lin_00166: This service is only applicable for LIN master node  */
Std_ReturnType Lin_GoToSleep(uint8_t Channel);

/* @SWS_Lin_00167 */
Std_ReturnType Lin_GoToSleepInternal(uint8_t Channel);

/* @SWS_Lin_00169 */
Std_ReturnType Lin_Wakeup(uint8_t Channel);

/* @SWS_Lin_00256 */
Std_ReturnType Lin_WakeupInternal(uint8_t Channel);

/* deprecated, don't use this API any more */
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

#ifdef __cplusplus
}
#endif
#endif /* LIN_H */
