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
#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_PDUR 0
#define AS_LOG_PDURI 2
#define AS_LOG_PDURE 3
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void PduR_Init(const PduR_ConfigType *configPtr) {
  (void)configPtr;
#if defined(PDUR_USE_MEMPOOL)
  PduR_MemInit();
#endif
}

Std_ReturnType PduR_TpTransmit(PduIdType pathId, const PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  const PduR_ConfigType *config = PDUR_CONFIG;
  const PduR_PduType *DestPduRef;
  PduIdType PduHandleId;

  DET_VALIDATE(pathId < config->NumOfRoutingPaths, 0xF1, PDUR_E_PDU_ID_INVALID, return E_NOT_OK);

  ASLOG(PDUR, ("PduR_TpTransmit %d\n", pathId));
  DestPduRef = &config->RoutingPaths[pathId].DestPduRef[0];
  if (NULL != config->RoutingPaths[pathId].DestTxBufferRef) {
    PduHandleId = pathId;
  } else {
    PduHandleId = DestPduRef->PduHandleId;
  }
  if (NULL != DestPduRef->Api->Transmit) {
    ret = DestPduRef->Api->Transmit(PduHandleId, pduInfoPtr);
  } else {
    ASLOG(PDURE, ("null Transmit\n"));
  }

  return ret;
}

BufReq_ReturnType PduR_CopyTxData(PduIdType pathId, const PduInfoType *info,
                                  const RetryInfoType *retry, PduLengthType *availableDataPtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  const PduR_ConfigType *config = PDUR_CONFIG;
  PduIdType PduHandleId;
  const PduR_RoutingPathType *RoutingPath;

  DET_VALIDATE(pathId < config->NumOfRoutingPaths, 0x43, PDUR_E_PDU_ID_INVALID,
               return BUFREQ_E_NOT_OK);

  ASLOG(PDUR, ("PduR_CopyTxData %d\n", pathId));
  RoutingPath = &config->RoutingPaths[pathId];
  if (NULL != RoutingPath->DestTxBufferRef) {
    PduHandleId = pathId;
  } else {
    PduHandleId = RoutingPath->SrcPduRef->PduHandleId;
  }

  if (NULL != RoutingPath->SrcPduRef->Api->CopyTxData) {
    ret = RoutingPath->SrcPduRef->Api->CopyTxData(PduHandleId, info, retry, availableDataPtr);
  } else {
    ASLOG(PDURE, ("null CopyTxData\n"));
  }

  return ret;
}

void PduR_RxIndication(PduIdType pathId, const PduInfoType *pduInfoPtr) {
  const PduR_ConfigType *config = PDUR_CONFIG;
  const PduR_PduType *DestPduRef;
  PduIdType PduHandleId;
  uint16_t i;

  DET_VALIDATE(pathId < config->NumOfRoutingPaths, 0x42, PDUR_E_PDU_ID_INVALID, return);

  ASLOG(PDUR, ("PduR_RxIndication %d\n", pathId));
  for (i = 0; i < config->RoutingPaths[pathId].NumOfDestPdus; i++) {
    DestPduRef = &config->RoutingPaths[pathId].DestPduRef[i];
    PduHandleId = DestPduRef->PduHandleId;
    if (NULL != DestPduRef->Api->RxIndication) {
      DestPduRef->Api->RxIndication(PduHandleId, pduInfoPtr);
    } else {
      ASLOG(PDURE, ("null RxIndication\n"));
    }
  }
}

Std_ReturnType PduR_Transmit(PduIdType pathId, const PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_OK;
  Std_ReturnType ret2;
  const PduR_ConfigType *config = PDUR_CONFIG;
  const PduR_PduType *DestPduRef;
  PduIdType PduHandleId;
  uint16_t i;

  DET_VALIDATE(pathId < config->NumOfRoutingPaths, 0x49, PDUR_E_PDU_ID_INVALID, return E_NOT_OK);
  DET_VALIDATE((NULL != pduInfoPtr) && (NULL != pduInfoPtr->SduDataPtr), 0x49, PDUR_E_PARAM_POINTER,
               return E_NOT_OK);

  ASLOG(PDUR, ("PduR_Transmit %d\n", pathId));

  for (i = 0; i < config->RoutingPaths[pathId].NumOfDestPdus; i++) {
    DestPduRef = &config->RoutingPaths[pathId].DestPduRef[i];
    PduHandleId = DestPduRef->PduHandleId;
    if (NULL != DestPduRef->Api->Transmit) {
      ret2 = DestPduRef->Api->Transmit(PduHandleId, pduInfoPtr);
      if (E_OK != ret2) {
        ret = ret2;
        ASLOG(PDUR, ("Transmit(%d) to dest %d failed\n", pathId, i));
      }
    } else {
      ASLOG(PDURE, ("null Transmit\n"));
    }
  }

  return ret;
}

void PduR_TxConfirmation(PduIdType pathId, Std_ReturnType result) {
  const PduR_ConfigType *config = PDUR_CONFIG;
  PduIdType PduHandleId;
  const PduR_RoutingPathType *RoutingPath;

  DET_VALIDATE(pathId < config->NumOfRoutingPaths, 0x40, PDUR_E_PDU_ID_INVALID, return);

  ASLOG(PDUR, ("PduR_TxConfirmation %d\n", pathId));
  RoutingPath = &config->RoutingPaths[pathId];
  if (NULL != RoutingPath->DestTxBufferRef) {
    PduHandleId = pathId;
  } else {
    PduHandleId = RoutingPath->SrcPduRef->PduHandleId;
  }

  if (NULL != RoutingPath->SrcPduRef->Api->TxConfirmation) {
    RoutingPath->SrcPduRef->Api->TxConfirmation(PduHandleId, result);
  } else {
    ASLOG(PDURE, ("null TxConfirmation\n"));
  }
}

BufReq_ReturnType PduR_StartOfReception(PduIdType pathId, const PduInfoType *info,
                                        PduLengthType tpSduLength, PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  const PduR_ConfigType *config = PDUR_CONFIG;
  const PduR_PduType *DestPduRef;
  PduIdType PduHandleId;

  DET_VALIDATE(pathId < config->NumOfRoutingPaths, 0x46, PDUR_E_PDU_ID_INVALID,
               return BUFREQ_E_NOT_OK);

  ASLOG(PDUR, ("PduR_StartOfReception %d\n", pathId));
  DestPduRef = &config->RoutingPaths[pathId].DestPduRef[0];
  if (NULL != config->RoutingPaths[pathId].DestTxBufferRef) {
    PduHandleId = pathId;
  } else {
    PduHandleId = DestPduRef->PduHandleId;
  }
  if (NULL != DestPduRef->Api->StartOfReception) {
    ret = DestPduRef->Api->StartOfReception(PduHandleId, info, tpSduLength, bufferSizePtr);
  } else {
    ASLOG(PDURE, ("null StartOfReception\n"));
  }

  return ret;
}

BufReq_ReturnType PduR_CopyRxData(PduIdType pathId, const PduInfoType *info,
                                  PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  const PduR_ConfigType *config = PDUR_CONFIG;
  const PduR_PduType *DestPduRef;
  PduIdType PduHandleId;

  DET_VALIDATE(pathId < config->NumOfRoutingPaths, 0x44, PDUR_E_PDU_ID_INVALID,
               return BUFREQ_E_NOT_OK);

  ASLOG(PDUR, ("PduR_CopyRxData %d\n", pathId));

  DestPduRef = &config->RoutingPaths[pathId].DestPduRef[0];
  if (NULL != config->RoutingPaths[pathId].DestTxBufferRef) {
    PduHandleId = pathId;
  } else {
    PduHandleId = DestPduRef->PduHandleId;
  }
  if (NULL != DestPduRef->Api->CopyRxData) {
    ret = DestPduRef->Api->CopyRxData(PduHandleId, info, bufferSizePtr);
  } else {
    ASLOG(PDURE, ("null CopyRxData\n"));
  }

  return ret;
}

void PduR_TpRxIndication(PduIdType pathId, Std_ReturnType result) {
  const PduR_ConfigType *config = PDUR_CONFIG;
  const PduR_PduType *DestPduRef;
  PduIdType PduHandleId;

  DET_VALIDATE(pathId < config->NumOfRoutingPaths, 0x45, PDUR_E_PDU_ID_INVALID, return);

  ASLOG(PDUR, ("PduR_TpRxIndication %d\n", pathId));
  DestPduRef = &config->RoutingPaths[pathId].DestPduRef[0];
  if (NULL != config->RoutingPaths[pathId].DestTxBufferRef) {
    PduHandleId = pathId;
  } else {
    PduHandleId = DestPduRef->PduHandleId;
  }
  if (NULL != DestPduRef->Api->TpRxIndication) {
    DestPduRef->Api->TpRxIndication(PduHandleId, result);
  } else {
    ASLOG(PDURE, ("null TpRxIndication\n"));
  }
}

BufReq_ReturnType PduR_GwCopyTxData(PduIdType pathId, const PduInfoType *info,
                                    const RetryInfoType *retry, PduLengthType *availableDataPtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  const PduR_ConfigType *config = PDUR_CONFIG;
  PduR_BufferType *buffer;
  PduLengthType offset = *((PduLengthType *)info->MetaDataPtr);

  (void)retry;

  ASLOG(PDUR, ("PduR_GwCopyTxData %d\n", pathId));
  if ((pathId < config->NumOfRoutingPaths) &&
      (NULL != config->RoutingPaths[pathId].DestTxBufferRef)) {
    buffer = config->RoutingPaths[pathId].DestTxBufferRef;
    if (NULL != buffer->Data) {
      (void)memcpy(info->SduDataPtr, &buffer->Data[offset], info->SduLength);
      offset += info->SduLength;
      *availableDataPtr = buffer->Size - offset;
      ret = BUFREQ_OK;
    }
  }
  return ret;
}

void PduR_GwTxConfirmation(PduIdType pathId, Std_ReturnType result) {
  const PduR_ConfigType *config = PDUR_CONFIG;
  PduR_BufferType *buffer;
  const PduR_RoutingPathType *RoutingPath;

  (void)result;

  DET_VALIDATE(pathId < config->NumOfRoutingPaths, 0xF2, PDUR_E_PDU_ID_INVALID, return);

  RoutingPath = &config->RoutingPaths[pathId];

  ASLOG(PDUR, ("PduR_GwTxConfirmation %d\n", pathId));
  if (NULL != RoutingPath->DestTxBufferRef) {
    buffer = RoutingPath->DestTxBufferRef;
    if (NULL != buffer->Data) {
#if defined(PDUR_USE_MEMPOOL)
      if (buffer->Data != RoutingPath->GwBuffer) {
        PduR_MemFree(buffer->Data);
      }
#endif
      buffer->Data = NULL;
    }
  }
}

BufReq_ReturnType PduR_GwStartOfReception(PduIdType pathId, const PduInfoType *info,
                                          PduLengthType tpSduLength, PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  const PduR_ConfigType *config = PDUR_CONFIG;
  const PduR_RoutingPathType *RoutingPath;
  PduR_BufferType *buffer;

  (void)info;

  DET_VALIDATE(pathId < config->NumOfRoutingPaths, 0xF3, PDUR_E_PDU_ID_INVALID,
               return BUFREQ_E_NOT_OK);

  RoutingPath = &config->RoutingPaths[pathId];

  ASLOG(PDUR, ("PduR_GwStartOfReception %d\n", pathId));
  if (NULL != RoutingPath->DestTxBufferRef) {
    buffer = RoutingPath->DestTxBufferRef;
    if ((NULL != RoutingPath->GwBuffer) && (RoutingPath->GwBufferSize >= tpSduLength)) {
      buffer->Data = RoutingPath->GwBuffer;
      buffer->Size = tpSduLength;
      buffer->Index = 0;
      *bufferSizePtr = tpSduLength;
      ret = BUFREQ_OK;
    } else { /* no static buffer or size too small */
#if defined(PDUR_USE_MEMPOOL)
      if ((buffer->Data != NULL) && (buffer->Data != RoutingPath->GwBuffer)) {
        PduR_MemFree(buffer->Data);
        ASLOG(PDURE, ("[%u] Free old buffer\n", pathId));
      }
      buffer->Data = PduR_MemAlloc(tpSduLength);
      if (NULL != buffer->Data) {
        buffer->Size = tpSduLength;
        buffer->Index = 0;
        *bufferSizePtr = tpSduLength;
        ret = BUFREQ_OK;
      }
#endif
    }
  }
  return ret;
}

BufReq_ReturnType PduR_GwCopyRxData(PduIdType pathId, const PduInfoType *info,
                                    PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_E_NOT_OK;
  const PduR_ConfigType *config = PDUR_CONFIG;
  PduR_BufferType *buffer;

  DET_VALIDATE(pathId < config->NumOfRoutingPaths, 0xF4, PDUR_E_PDU_ID_INVALID,
               return BUFREQ_E_NOT_OK);

  ASLOG(PDUR, ("PduR_GwCopyRxData %d\n", pathId));
  if (NULL != config->RoutingPaths[pathId].DestTxBufferRef) {
    buffer = config->RoutingPaths[pathId].DestTxBufferRef;
    if (NULL != buffer->Data) {
      if ((buffer->Index < buffer->Size) && (info->SduLength <= (buffer->Size - buffer->Index))) {
        (void)memcpy(&buffer->Data[buffer->Index], info->SduDataPtr, info->SduLength);
        buffer->Index += info->SduLength;
        *bufferSizePtr = buffer->Size - buffer->Index;
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
  uint16_t i;
  PduLengthType bufferSize;
  Std_ReturnType ret = result;
  const PduR_RoutingPathType *RoutingPath;

  DET_VALIDATE(pathId < config->NumOfRoutingPaths, 0xF5, PDUR_E_PDU_ID_INVALID, return);

  RoutingPath = &config->RoutingPaths[pathId];

  ASLOG(PDUR, ("PduR_GwRxIndication %d\n", pathId));
  if (NULL != RoutingPath->DestTxBufferRef) {
    for (i = 0; (i < RoutingPath->NumOfDestPdus) && (E_OK == ret); i++) {
      DestPduRef = &RoutingPath->DestPduRef[i];
      buffer = RoutingPath->DestTxBufferRef;
      if ((E_OK == result) && (NULL != buffer->Data)) {
        PduInfo.SduDataPtr = buffer->Data;
        PduInfo.SduLength = buffer->Size;
        PduInfo.MetaDataPtr = NULL;
        if (DestPduRef->Module > PDUR_MODULE_ISOTP) { /* further forward rx data */
          if ((NULL != DestPduRef->Api->StartOfReception) &&
              (NULL != DestPduRef->Api->CopyRxData) && (NULL != DestPduRef->Api->TpRxIndication)) {
            ret = DestPduRef->Api->StartOfReception(DestPduRef->PduHandleId, &PduInfo,
                                                    PduInfo.SduLength, &bufferSize);
            if (E_OK == ret) {
              ret = DestPduRef->Api->CopyRxData(DestPduRef->PduHandleId, &PduInfo, &bufferSize);
            }
            if (E_OK == ret) {
              DestPduRef->Api->TpRxIndication(DestPduRef->PduHandleId, E_OK);
            }
          } else {
            ASLOG(PDURE, ("null ISOTP RX API\n"));
            ret = E_NOT_OK;
          }
        } else {
          if (NULL != DestPduRef->Api->Transmit) {
            buffer->Index = 0;
            ret = DestPduRef->Api->Transmit(DestPduRef->PduHandleId, &PduInfo);
          } else {
            ASLOG(PDURE, ("null Transmit\n"));
            ret = E_NOT_OK;
          }
        }
      }
    }
    if (E_NOT_OK == ret) {
#if defined(PDUR_USE_MEMPOOL)
      if ((buffer->Data != NULL) && (buffer->Data != RoutingPath->GwBuffer)) {
        PduR_MemFree(buffer->Data);
        buffer->Data = NULL;
      }
#endif
    }
  }
}

void PduR_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0xf1, PDUR_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_PDUR;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 0;
}
