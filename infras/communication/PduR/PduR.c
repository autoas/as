/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "PduR.h"
#include "PduR_Priv.h"
#include "Std_Debug.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_PDUR 0
#define AS_LOG_PDURI 2
#define AS_LOG_PDURE 3
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void PduR_Init(const PduR_ConfigType *ConfigPtr) {
#if defined(PDUR_USE_MEMPOOL)
  PduR_MemInit();
#endif
}

Std_ReturnType PduR_TpTransmit(PduIdType pathId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  const PduR_ConfigType *config = PDUR_CONFIG;
  const PduR_PduType *DestPduRef;
  PduIdType PduHandleId;

  ASLOG(PDUR, ("%s %d\n", __func__, pathId));
  if (pathId < config->numOfRoutingPaths) {
    DestPduRef = &config->RoutingPaths[pathId].DestPduRef[0];
    if (NULL != config->RoutingPaths[pathId].DestTxBufferRef) {
      PduHandleId = pathId;
    } else {
      PduHandleId = DestPduRef->PduHandleId;
    }
    if (NULL != DestPduRef->api->Transmit) {
      ret = DestPduRef->api->Transmit(PduHandleId, PduInfoPtr);
    } else {
      ASLOG(PDURE, ("null Transmit\n"));
    }
  }

  return ret;
}

BufReq_ReturnType PduR_CopyTxData(PduIdType pathId, const PduInfoType *info,
                                  const RetryInfoType *retry, PduLengthType *availableDataPtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  const PduR_ConfigType *config = PDUR_CONFIG;
  PduIdType PduHandleId;
  const PduR_RoutingPathType *RoutingPath;

  ASLOG(PDUR, ("%s %d\n", __func__, pathId));
  if (pathId < config->numOfRoutingPaths) {
    RoutingPath = &config->RoutingPaths[pathId];
    if (NULL != RoutingPath->DestTxBufferRef) {
      PduHandleId = pathId;
    } else {
      PduHandleId = RoutingPath->SrcPduRef->PduHandleId;
    }

    if (NULL != RoutingPath->SrcPduRef->api->CopyTxData) {
      ret = RoutingPath->SrcPduRef->api->CopyTxData(PduHandleId, info, retry, availableDataPtr);
    } else {
      ASLOG(PDURE, ("null CopyTxData\n"));
    }
  }

  return ret;
}

void PduR_RxIndication(PduIdType pathId, const PduInfoType *PduInfoPtr) {
  const PduR_ConfigType *config = PDUR_CONFIG;
  const PduR_PduType *DestPduRef;
  PduIdType PduHandleId;
  uint16_t i;

  ASLOG(PDUR, ("%s %d\n", __func__, pathId));
  if (pathId < config->numOfRoutingPaths) {
    for (i = 0; i < config->RoutingPaths[pathId].numOfDestPdus; i++) {
      DestPduRef = &config->RoutingPaths[pathId].DestPduRef[i];
      PduHandleId = DestPduRef->PduHandleId;
      if (NULL != DestPduRef->api->Ind.RxIndication) {
        DestPduRef->api->Ind.RxIndication(PduHandleId, PduInfoPtr);
      } else {
        ASLOG(PDURE, ("null RxIndication\n"));
      }
    }
  }
}

Std_ReturnType PduR_Transmit(PduIdType pathId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_OK;
  Std_ReturnType ret2;
  const PduR_ConfigType *config = PDUR_CONFIG;
  const PduR_PduType *DestPduRef;
  PduIdType PduHandleId;
  uint16_t i;

  ASLOG(PDUR, ("%s %d\n", __func__, pathId));
  if (pathId < config->numOfRoutingPaths) {
    for (i = 0; i < config->RoutingPaths[pathId].numOfDestPdus; i++) {
      DestPduRef = &config->RoutingPaths[pathId].DestPduRef[i];
      PduHandleId = DestPduRef->PduHandleId;
      if (NULL != DestPduRef->api->Transmit) {
        ret2 = DestPduRef->api->Transmit(PduHandleId, PduInfoPtr);
        if (E_OK != ret2) {
          ret = ret2;
          ASLOG(PDURE, ("Transmit(%d) to dest %d failed\n", pathId, i));
        }
      } else {
        ASLOG(PDURE, ("null Transmit\n"));
      }
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

void PduR_TxConfirmation(PduIdType pathId, Std_ReturnType result) {
  const PduR_ConfigType *config = PDUR_CONFIG;
  PduIdType PduHandleId;
  const PduR_RoutingPathType *RoutingPath;

  ASLOG(PDUR, ("%s %d\n", __func__, pathId));
  if (pathId < config->numOfRoutingPaths) {
    RoutingPath = &config->RoutingPaths[pathId];
    if (NULL != RoutingPath->DestTxBufferRef) {
      PduHandleId = pathId;
    } else {
      PduHandleId = RoutingPath->SrcPduRef->PduHandleId;
    }

    if (NULL != RoutingPath->SrcPduRef->api->TxConfirmation) {
      RoutingPath->SrcPduRef->api->TxConfirmation(PduHandleId, result);
    } else {
      ASLOG(PDURE, ("null TxConfirmation\n"));
    }
  }
}

BufReq_ReturnType PduR_StartOfReception(PduIdType pathId, const PduInfoType *info,
                                        PduLengthType TpSduLength, PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  const PduR_ConfigType *config = PDUR_CONFIG;
  const PduR_PduType *DestPduRef;
  PduIdType PduHandleId;

  ASLOG(PDUR, ("%s %d\n", __func__, pathId));
  if (pathId < config->numOfRoutingPaths) {
    DestPduRef = &config->RoutingPaths[pathId].DestPduRef[0];
    if (NULL != config->RoutingPaths[pathId].DestTxBufferRef) {
      PduHandleId = pathId;
    } else {
      PduHandleId = DestPduRef->PduHandleId;
    }
    if (NULL != DestPduRef->api->StartOfReception) {
      ret = DestPduRef->api->StartOfReception(PduHandleId, info, TpSduLength, bufferSizePtr);
    } else {
      ASLOG(PDURE, ("null StartOfReception\n"));
    }
  }

  return ret;
}

BufReq_ReturnType PduR_CopyRxData(PduIdType pathId, const PduInfoType *info,
                                  PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  const PduR_ConfigType *config = PDUR_CONFIG;
  const PduR_PduType *DestPduRef;
  PduIdType PduHandleId;

  ASLOG(PDUR, ("%s %d\n", __func__, pathId));
  if (pathId < config->numOfRoutingPaths) {
    DestPduRef = &config->RoutingPaths[pathId].DestPduRef[0];
    if (NULL != config->RoutingPaths[pathId].DestTxBufferRef) {
      PduHandleId = pathId;
    } else {
      PduHandleId = DestPduRef->PduHandleId;
    }
    if (NULL != DestPduRef->api->CopyRxData) {
      ret = DestPduRef->api->CopyRxData(PduHandleId, info, bufferSizePtr);
    } else {
      ASLOG(PDURE, ("null CopyRxData\n"));
    }
  }

  return ret;
}

void PduR_TpRxIndication(PduIdType pathId, Std_ReturnType result) {
  const PduR_ConfigType *config = PDUR_CONFIG;
  const PduR_PduType *DestPduRef;
  PduIdType PduHandleId;

  ASLOG(PDUR, ("%s %d\n", __func__, pathId));
  if (pathId < config->numOfRoutingPaths) {
    DestPduRef = &config->RoutingPaths[pathId].DestPduRef[0];
    if (NULL != config->RoutingPaths[pathId].DestTxBufferRef) {
      PduHandleId = pathId;
    } else {
      PduHandleId = DestPduRef->PduHandleId;
    }
    if (NULL != DestPduRef->api->Ind.TpRxIndication) {
      DestPduRef->api->Ind.TpRxIndication(PduHandleId, result);
    } else {
      ASLOG(PDURE, ("null TpRxIndication\n"));
    }
  }
}

BufReq_ReturnType PduR_GwCopyTxData(PduIdType pathId, const PduInfoType *info,
                                    const RetryInfoType *retry, PduLengthType *availableDataPtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  const PduR_ConfigType *config = PDUR_CONFIG;
  PduR_BufferType *buffer;

  ASLOG(PDUR, ("%s %d\n", __func__, pathId));
  if ((pathId < config->numOfRoutingPaths) &&
      (NULL != config->RoutingPaths[pathId].DestTxBufferRef)) {
    buffer = config->RoutingPaths[pathId].DestTxBufferRef;
    if (NULL != buffer->data) {
      memcpy(info->SduDataPtr, &buffer->data[buffer->index], info->SduLength);
      buffer->index += info->SduLength;
      *availableDataPtr = buffer->size - buffer->index;
      ret = BUFREQ_OK;
    }
  }
  return ret;
}

#if defined(PDUR_USE_MEMPOOL)
void PduR_GwTxConfirmation(PduIdType pathId, Std_ReturnType result) {
  const PduR_ConfigType *config = PDUR_CONFIG;
  PduR_BufferType *buffer;

  ASLOG(PDUR, ("%s %d\n", __func__, pathId));
  if ((pathId < config->numOfRoutingPaths) &&
      (NULL != config->RoutingPaths[pathId].DestTxBufferRef)) {
    buffer = config->RoutingPaths[pathId].DestTxBufferRef;
    if (NULL != buffer->data) {
      PduR_MemFree(buffer->data);
      buffer->data = NULL;
    }
  }
}

BufReq_ReturnType PduR_GwStartOfReception(PduIdType pathId, const PduInfoType *info,
                                          PduLengthType TpSduLength, PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  const PduR_ConfigType *config = PDUR_CONFIG;
  PduR_BufferType *buffer;

  ASLOG(PDUR, ("%s %d\n", __func__, pathId));
  if ((pathId < config->numOfRoutingPaths) &&
      (NULL != config->RoutingPaths[pathId].DestTxBufferRef)) {
    buffer = config->RoutingPaths[pathId].DestTxBufferRef;
    if (buffer->data != NULL) {
      PduR_MemFree(buffer->data);
    }
    buffer->data = PduR_MemAlloc(TpSduLength);
    if (NULL != buffer->data) {
      buffer->size = TpSduLength;
      buffer->index = 0;
      *bufferSizePtr = TpSduLength;
      ret = BUFREQ_OK;
    }
  }
  return ret;
}

BufReq_ReturnType PduR_GwCopyRxData(PduIdType pathId, const PduInfoType *info,
                                    PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  const PduR_ConfigType *config = PDUR_CONFIG;
  PduR_BufferType *buffer;

  ASLOG(PDUR, ("%s %d\n", __func__, pathId));
  if ((pathId < config->numOfRoutingPaths) &&
      (NULL != config->RoutingPaths[pathId].DestTxBufferRef)) {
    buffer = config->RoutingPaths[pathId].DestTxBufferRef;
    if (NULL != buffer->data) {
      if ((buffer->index < buffer->size) && (info->SduLength <= (buffer->size - buffer->index))) {
        memcpy(&buffer->data[buffer->index], info->SduDataPtr, info->SduLength);
        buffer->index += info->SduLength;
        *bufferSizePtr = buffer->size - buffer->index;
        ret = BUFREQ_OK;
      } else {
        ASLOG(PDURE, ("Buffer Overflow\n"));
        ret = BUFREQ_E_OVFL;
      }
    }
  }
  return ret;
}

void PduR_GwRxIndication(PduIdType pathId, Std_ReturnType result) {
  const PduR_ConfigType *config = PDUR_CONFIG;
  PduR_BufferType *buffer;
  const PduR_PduType *DestPduRef;
  PduInfoType PduInfo;
  Std_ReturnType ret = E_NOT_OK;

  ASLOG(PDUR, ("%s %d\n", __func__, pathId));
  if ((pathId < config->numOfRoutingPaths) &&
      (NULL != config->RoutingPaths[pathId].DestTxBufferRef)) {
    DestPduRef = &config->RoutingPaths[pathId].DestPduRef[0];
    buffer = config->RoutingPaths[pathId].DestTxBufferRef;
    if ((E_OK == result) && (NULL != buffer->data)) {
      if (NULL != DestPduRef->api->Transmit) {
        PduInfo.SduDataPtr = buffer->data;
        PduInfo.SduLength = buffer->size;
        PduInfo.MetaDataPtr = NULL;
        buffer->index = 0;
        ret = DestPduRef->api->Transmit(DestPduRef->PduHandleId, &PduInfo);
      } else {
        ASLOG(PDURE, ("null Transmit\n"));
      }
    }
    if (E_NOT_OK == ret) {
      if (NULL != buffer->data) {
        PduR_MemFree(buffer->data);
        buffer->data = NULL;
      }
    }
  }
}
#endif /* defined(PDUR_USE_MEMPOOL) */