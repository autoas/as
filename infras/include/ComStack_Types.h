/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref:
 * https://www.autosar.org/fileadmin/user_upload/standards/classic/4-3/AUTOSAR_SWS_CommunicationStackTypes.pdf
 */
#ifndef COMSTACK_TYPES_H
#define COMSTACK_TYPES_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define PDU_LENGHT_MAX (0xFFFFUL)
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_COMTYPE_00005 */
typedef uint16_t PduIdType;

/* @SWS_COMTYPE_00008 */
typedef uint32_t PduLengthType;

/* @SWS_COMTYPE_00011 */
typedef struct {
  uint8_t *SduDataPtr;
  uint8_t *MetaDataPtr;
  PduLengthType SduLength;
} PduInfoType;

/* @SWS_COMTYPE_00036 */
typedef uint8_t PNCHandleType;

/* @SWS_COMTYPE_00031 */
typedef enum
{
  TP_STMIN,
  TP_BS,
  TP_BC
} TPParameterType;

/* @SWS_COMTYPE_00012 */
typedef enum
{
  BUFREQ_OK,
  BUFREQ_E_NOT_OK,
  BUFREQ_E_BUSY,
  BUFREQ_E_OVFL
} BufReq_ReturnType;

/* @SWS_COMTYPE_00027 */
typedef enum
{
  TP_DATACONF,
  TP_DATARETRY,
  TP_CONFPENDING
} TpDataStateType;

/* @SWS_COMTYPE_00037 */
typedef struct {
  TpDataStateType TpDataState;
  PduLengthType TxTpDataCnt;
} RetryInfoType;

/* @SWS_COMTYPE_00038 */
typedef uint8_t NetworkHandleType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#ifdef __cplusplus
}
#endif
#endif /* COMSTACK_TYPES_H */
