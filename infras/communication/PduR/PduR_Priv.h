/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
#ifndef _PDUR_PRIV_H_
#define _PDUR_PRIV_H_
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */

typedef enum
{
  PDUR_MODULE_CANIF,
  PDUR_MODULE_CANTP,
  PDUR_MODULE_LINIF,
  PDUR_MODULE_LINTP,
  PDUR_MODULE_DOIP,
  PDUR_MODULE_CANNM,
  PDUR_MODULE_OSEKNM,
  /* ---- low / upper ---- */
  PDUR_MODULE_COM,
  PDUR_MODULE_DCM,
} PduR_ModuleType;

/* @ECUC_PduR_00289 */
typedef enum
{
  PDUR_DIRECT,
  PDUR_TRIGGERTRANSMIT
} PduR_DestPduDataProvisionType;

typedef struct {
  BufReq_ReturnType (*StartOfReception)(PduIdType id, const PduInfoType *info,
                                        PduLengthType TpSduLength, PduLengthType *bufferSizePtr);
  BufReq_ReturnType (*CopyRxData)(PduIdType id, const PduInfoType *info,
                                  PduLengthType *bufferSizePtr);
  union {
    void (*TpRxIndication)(PduIdType id, Std_ReturnType result);
    void (*RxIndication)(PduIdType id, const PduInfoType *PduInfoPtr);
  } Ind;
  Std_ReturnType (*Transmit)(PduIdType id, const PduInfoType *PduInfoPtr);
  BufReq_ReturnType (*CopyTxData)(PduIdType id, const PduInfoType *info, const RetryInfoType *retry,
                                  PduLengthType *availableDataPtr);
  void (*TxConfirmation)(PduIdType id, Std_ReturnType result);
} PduR_ApiType;

typedef struct {
  PduR_ModuleType Module;
  PduIdType PduHandleId;
  const PduR_ApiType *api;
} PduR_PduType;

typedef struct {
  uint8_t* data;
  PduLengthType size;
  PduLengthType index;
} PduR_BufferType;

/* @ECUC_PduR_00248 */
typedef struct {
  const PduR_PduType *SrcPduRef;
  const PduR_PduType *DestPduRef; /* @ECUC_PduR_00354 */
  uint16_t numOfDestPdus;
  PduR_BufferType *DestTxBufferRef; /* @ECUC_PduR_00304 */
} PduR_RoutingPathType;

struct PduR_Config_s {
  const PduR_RoutingPathType *RoutingPaths;
  uint16_t numOfRoutingPaths;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType PduR_TpTransmit(PduIdType pathId, const PduInfoType *PduInfoPtr);
BufReq_ReturnType PduR_CopyTxData(PduIdType pathId, const PduInfoType *info,
                                  const RetryInfoType *retry, PduLengthType *availableDataPtr);
void PduR_TxConfirmation(PduIdType pathId, Std_ReturnType result);

BufReq_ReturnType PduR_StartOfReception(PduIdType pathId, const PduInfoType *info,
                                        PduLengthType TpSduLength, PduLengthType *bufferSizePtr);
BufReq_ReturnType PduR_CopyRxData(PduIdType pathId, const PduInfoType *info,
                                  PduLengthType *bufferSizePtr);
void PduR_TpRxIndication(PduIdType pathId, Std_ReturnType result);

BufReq_ReturnType PduR_GwCopyTxData(PduIdType pathId, const PduInfoType *info,
                                    const RetryInfoType *retry, PduLengthType *availableDataPtr);
void PduR_GwTxConfirmation(PduIdType pathId, Std_ReturnType result);

BufReq_ReturnType PduR_GwStartOfReception(PduIdType pathId, const PduInfoType *info,
                                          PduLengthType TpSduLength, PduLengthType *bufferSizePtr);
BufReq_ReturnType PduR_GwCopyRxData(PduIdType pathId, const PduInfoType *info,
                                    PduLengthType *bufferSizePtr);
void PduR_GwRxIndication(PduIdType pathId, Std_ReturnType result);

BufReq_ReturnType PduR_CanTpGwCopyTxData(PduIdType id, const PduInfoType *info,
                                         const RetryInfoType *retry,
                                         PduLengthType *availableDataPtr);
void PduR_CanTpGwTxConfirmation(PduIdType id, Std_ReturnType result);

BufReq_ReturnType PduR_CanTpGwStartOfReception(PduIdType id, const PduInfoType *info,
                                               PduLengthType TpSduLength,
                                               PduLengthType *bufferSizePtr);
BufReq_ReturnType PduR_CanTpGwCopyRxData(PduIdType id, const PduInfoType *info,
                                         PduLengthType *bufferSizePtr);
void PduR_CanTpGwRxIndication(PduIdType id, Std_ReturnType result);


BufReq_ReturnType PduR_DoIPGwCopyTxData(PduIdType id, const PduInfoType *info,
                                         const RetryInfoType *retry,
                                         PduLengthType *availableDataPtr);
void PduR_DoIPGwTxConfirmation(PduIdType id, Std_ReturnType result);

BufReq_ReturnType PduR_DoIPGwStartOfReception(PduIdType id, const PduInfoType *info,
                                               PduLengthType TpSduLength,
                                               PduLengthType *bufferSizePtr);
BufReq_ReturnType PduR_DoIPGwCopyRxData(PduIdType id, const PduInfoType *info,
                                         PduLengthType *bufferSizePtr);
void PduR_DoIPGwRxIndication(PduIdType id, Std_ReturnType result);
#endif /* _PDUR_PRIV_H_ */
