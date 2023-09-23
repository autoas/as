/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "PduR.h"
#include "PduR_Priv.h"
#include "PduR_Cfg.h"
#include "CanTp.h"
#include "CanTp_Cfg.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef CANTP_MAX_CHANNELS
#define CANTP_MAX_CHANNELS 32
#endif

#define PDUR_DCM_TX_BASE_ID 0
#define PDUR_CANTP_RX_BASE_ID 32
#define PDUR_CANTP_TX_BASE_ID 0
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
BufReq_ReturnType IsoTp_CanTpStartOfReception(PduIdType id, const PduInfoType *info,
                                              PduLengthType TpSduLength,
                                              PduLengthType *bufferSizePtr);
BufReq_ReturnType IsoTp_CanCopyRxData(PduIdType id, const PduInfoType *info,
                                      PduLengthType *bufferSizePtr);
BufReq_ReturnType IsoTp_CanTpCopyTxData(PduIdType id, const PduInfoType *info,
                                        const RetryInfoType *retry,
                                        PduLengthType *availableDataPtr);
void IsoTp_CanTpRxIndication(PduIdType id, Std_ReturnType result);
void IsoTp_CanTpTxConfirmation(PduIdType id, Std_ReturnType result);
/* ================================ [ DATAS     ] ============================================== */
const PduR_ApiType PduR_IsoTpCanTpApi = {
  IsoTp_CanTpStartOfReception, IsoTp_CanCopyRxData,       {(void *)IsoTp_CanTpRxIndication}, NULL,
  IsoTp_CanTpCopyTxData,       IsoTp_CanTpTxConfirmation,
};

const PduR_ApiType PduR_CanTpApi = {
  NULL, NULL, {(void *)NULL}, CanTp_Transmit, NULL, NULL,
};

static PduR_PduType PduR_SrcPdu[CANTP_MAX_CHANNELS * 2];
static PduR_PduType PduR_DstPdu[CANTP_MAX_CHANNELS * 2][1];
static PduR_RoutingPathType PduR_RoutingPaths[CANTP_MAX_CHANNELS * 2];

const PduR_ConfigType PduR_Config = {
#if defined(PDUR_USE_MEMPOOL)
  NULL,
#endif
  PduR_RoutingPaths,
  ARRAY_SIZE(PduR_RoutingPaths),
  PDUR_DCM_TX_BASE_ID,
  -1,
  -1,
  PDUR_CANTP_RX_BASE_ID,
  PDUR_CANTP_TX_BASE_ID,
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void PduR_CanTpReConfig(uint8_t Channel) {
  if (Channel < CANTP_MAX_CHANNELS) {
    PduR_SrcPdu[Channel].Module = PDUR_MODULE_ISOTP;
    PduR_SrcPdu[Channel].PduHandleId = Channel;
    PduR_SrcPdu[Channel].api = &PduR_IsoTpCanTpApi;
    PduR_DstPdu[Channel][0].Module = PDUR_MODULE_CANTP;
    PduR_DstPdu[Channel][0].PduHandleId = Channel;
    PduR_DstPdu[Channel][0].api = &PduR_CanTpApi;
    PduR_RoutingPaths[Channel].SrcPduRef = &PduR_SrcPdu[Channel];
    PduR_RoutingPaths[Channel].DestPduRef = PduR_DstPdu[Channel];
    PduR_RoutingPaths[Channel].numOfDestPdus = 1;
    PduR_RoutingPaths[Channel].DestTxBufferRef = NULL;

    PduR_SrcPdu[CANTP_MAX_CHANNELS + Channel].Module = PDUR_MODULE_CANTP;
    PduR_SrcPdu[CANTP_MAX_CHANNELS + Channel].PduHandleId = Channel;
    PduR_SrcPdu[CANTP_MAX_CHANNELS + Channel].api = &PduR_CanTpApi;
    PduR_DstPdu[CANTP_MAX_CHANNELS + Channel][0].Module = PDUR_MODULE_ISOTP;
    PduR_DstPdu[CANTP_MAX_CHANNELS + Channel][0].PduHandleId = Channel;
    PduR_DstPdu[CANTP_MAX_CHANNELS + Channel][0].api = &PduR_IsoTpCanTpApi;
    PduR_RoutingPaths[CANTP_MAX_CHANNELS + Channel].SrcPduRef =
      &PduR_SrcPdu[CANTP_MAX_CHANNELS + Channel];
    PduR_RoutingPaths[CANTP_MAX_CHANNELS + Channel].DestPduRef =
      PduR_DstPdu[CANTP_MAX_CHANNELS + Channel];
    PduR_RoutingPaths[CANTP_MAX_CHANNELS + Channel].numOfDestPdus = 1;
    PduR_RoutingPaths[CANTP_MAX_CHANNELS + Channel].DestTxBufferRef = NULL;
  }
}

static void __attribute__((constructor)) _pdur_config_init(void) {
  uint8_t i;
  for (i = 0; i < CANTP_MAX_CHANNELS; i++) {
    PduR_CanTpReConfig(i);
  }
}