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
#include "PduR_Cfg.h"
#if defined(PDUR_USE_MEMPOOL)
#include "mempool.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#define PDUR_CONFIG (&PduR_Config)
/* ================================ [ TYPES     ] ============================================== */
typedef enum {
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
typedef enum {
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
  const PduIdType LINTP_RX_BASE_ID;
  const PduIdType LINTP_TX_BASE_ID;
};
/*  The above Base Ids looks very weird and it's really not a good design, and it can cover only some simple routines.
 * And the routines must be specially configured with correct sorted orders.
 * But the reason for this is that LinTp/CanTp and Dcm are channel based design to make it simple, and this issue can
 * be resolved that each module hold the right routine table Id when call PduR API, thus can configure all the base id as 0.
 *                                              `  To Make things simple, the PduR routines are grouped according to "from"/"to"
 *   +-------+                                  `   [x]: x is the routine path id.
 *   |  Dcm  |                                  `   [0] = { from: DoIp  0, to: Dcm   0 }  <- DOIP_RX_BASE_ID
 *   +---0---+                                  `   [1] = { from: DoIp  1, to: CanTp 0 }  <- CANTP_TX_BASE_ID
 *      ^ |                                     `   [2] = { from: DoIp  2, to: CanTp 1 }
 *      | v                                     `   [3] = { from: Dcm   0, to: DoIp  0 }  <- DOIP_TX_BASE_ID / DCM_TX_BASE_ID
 *  +---|-|----------------------------------+  `   [4] = { from: CanTp 0, to: DoIp  1 }  <- CANTP_RX_BASE_ID
 *  |   | |          PduR                    |  `   [5] = { from: CanTp 1, to: DoIp  2 }
 *  |   | |  +--------------------------+    |  `
 *  |   | |  | +----------------------+ |    |  `  The DoIP 1 get a request, thus forward it to CanTp 1, CanTp 1 will call PduR Tx
 *  |   | |  | |   +---------------+  | |    |  ` Related API with PduId 1, and the PduR will add the PduId with CANTP_TX_BASE_ID,
 *  |   | |  | |   | +-----------+ |  | |    |  ` thus in this case, get the correct routine path id 2.
 *  +---|-|--|-|---|-|-----------|-|--|-|----+  `  Thus, when CanTp 1 has a response, it calls PduR Rx related API with PduId 1,
 *      ^ |  ^ |   ^ |           ^ |  ^ |       ` and the PduR will add the PduId with CANTP_RX_BASE_ID, thus in this case, get
 *      | V  | V   | V           | V  | V       ` the correct routine path id 5.
 *  +----0----1-----2----+    +---0----1---+    `
 *  |       DoIP         |    |   CanTp    |    `
 *  +--------------------+    +------------+    `
 *
 * This design works for gateway cross TP, it will not works for routines in the same TP as below case.
 *
 *                                              `
 *   +-------+                                  `   [x]: x is the routine path id.
 *   |  Dcm  |                                  `   [0] = { from: CanTp 0, to: Dcm   0 }  <- CANTP_RX_BASE_ID
 *   +---0---+                                  `   [1] = { from: CanTp 1, to: CanTp 4 }  <- CANTP_TX_BASE_ID
 *      ^ |                                     `   [2] = { from: CanTp 2, to: CanTp 3 }
 *      | v                                     `   [3] = { from: CanTp 3, to: CanTp 2 }
 *  +---|-|----------------------------------+  `   [4] = { from: CanTp 4, to: CanTp 1 }
 *  |   | |          PduR                    |  `   [5] = { from: Dcm   0, to: CanTp 0 }  <- DCM_TX_BASE_ID
 *  |   | |  +--------------------------+    |  `
 *  |   | |  | +----------------------+ |    |  `  e.g: CanTp 3 call PduR Tx Api with PduId 3, thus routine path id is 3+1 which is 4,
 *  |   | |  | |   +---------------+  | |    |  ` it's totally wrong, the correct routine path id is 1 in this case.
 *  |   | |  | |   | +-----------+ |  | |    |  `
 *  +---|-|--|-|---|-|-----------|-|--|-|----+  `  So in thus case, all the PduR based id must be configured with 0, and each module
 *      ^ |  ^ |   ^ |           ^ |  ^ |       ` must call PduR API with the right routhine path id.
 *      | V  | V   | V           | V  | V       `
 *  +----0----1-----2---------- --3----4----+   `  And for the underlying Tp, for example for the TP pair 2&3, the TP configuration:
 *  |               CanTp                   |   `    TP Channel 2: { Rx: PDUR_ID_2_RX=[2], Tx: PDUR_ID_3_RX=[3] }
 *  +---------------------------------------+   `    TP Channel 3: { Rx: PDUR_ID_3_RX=[3], Tx: PDUR_ID_2_RX=[2] }
 *                                              `  Please note that the TP Channel Tx referece to the peer.
 */
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

BufReq_ReturnType PduR_LinTpGwCopyTxData(PduIdType id, const PduInfoType *info,
                                         const RetryInfoType *retry,
                                         PduLengthType *availableDataPtr);
void PduR_LinTpGwTxConfirmation(PduIdType id, Std_ReturnType result);

BufReq_ReturnType PduR_LinTpGwStartOfReception(PduIdType id, const PduInfoType *info,
                                               PduLengthType TpSduLength,
                                               PduLengthType *bufferSizePtr);
BufReq_ReturnType PduR_LinTpGwCopyRxData(PduIdType id, const PduInfoType *info,
                                         PduLengthType *bufferSizePtr);
void PduR_LinTpGwRxIndication(PduIdType id, Std_ReturnType result);

void PduR_MemInit(void);
uint8_t *PduR_MemAlloc(uint32_t size);
uint8_t *PduR_MemGet(uint32_t *size);
void PduR_MemFree(uint8_t *buffer);
#endif /* _PDUR_PRIV_H_ */
