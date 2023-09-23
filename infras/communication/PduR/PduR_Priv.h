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
#if defined(PDUR_USE_MEMPOOL)
#include "mempool.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#define PDUR_CONFIG (&PduR_Config)
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
  PDUR_MODULE_ISOTP,
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
    void_ptr_t ptr;
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
  uint8_t *data;
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
#if defined(PDUR_USE_MEMPOOL)
  const mem_cluster_t *mc;
#endif
  const PduR_RoutingPathType *RoutingPaths;
  uint16_t numOfRoutingPaths;
  const PduIdType DCM_TX_BASE_ID;
  const PduIdType DOIP_RX_BASE_ID;
  const PduIdType DOIP_TX_BASE_ID;
  const PduIdType CANTP_RX_BASE_ID;
  const PduIdType CANTP_TX_BASE_ID;
};
/* ================================ [ DECLARES  ] ============================================== */
extern const PduR_ConfigType PduR_Config;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType PduR_TpTransmit(PduIdType pathId, const PduInfoType *PduInfoPtr);
BufReq_ReturnType PduR_CopyTxData(PduIdType pathId, const PduInfoType *info,
                                  const RetryInfoType *retry, PduLengthType *availableDataPtr);
void PduR_TxConfirmation(PduIdType pathId, Std_ReturnType result);
void PduR_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
Std_ReturnType PduR_Transmit(PduIdType pathId, const PduInfoType *PduInfoPtr);

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

void PduR_MemInit(void);
uint8_t *PduR_MemAlloc(uint32_t size);
uint8_t *PduR_MemGet(uint32_t *size);
void PduR_MemFree(uint8_t *buffer);
#endif /* _PDUR_PRIV_H_ */
