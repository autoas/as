/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Communication AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Com.h"
#include "Com_Cfg.h"
#include "Com_Priv.h"
#include "PduR_Com.h"
#include "Std_Bit.h"
#include <string.h>
#ifdef USE_SHELL
#include "Std_Debug.h"
#include "shell.h"
#endif

#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#ifdef COM_USE_PB_CONFIG
#define COM_CONFIG comConfig
#else
#define COM_CONFIG (&Com_Config)
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const Com_ConfigType Com_Config;
/* ================================ [ DATAS     ] ============================================== */
#ifdef COM_USE_PB_CONFIG
static const Com_ConfigType *comConfig = NULL;
#endif
/* ================================ [ LOCALS    ] ============================================== */
static Std_ReturnType Com_StoreSignalValue(const Com_SignalConfigType *signal, uint32_t sigV,
                                           void *signalDataPtr) {
  Std_ReturnType ret = E_OK;
  uint32_t mask;
  uint32_t signmask;
  uint32_t u32V = sigV;
  mask = 0xFFFFFFFFul >> (32u - signal->BitSize); /* calculate mask for SigVal */
  u32V &= mask;                                   /* clear bit out of range */
  signmask = ~(mask >> 1);                        /* @SWS_Com_00008 */
  switch (signal->Type) {
  case COM_SINT8:
    if (0u != (u32V & signmask)) {
      u32V |= signmask; /* add sign bits */
    }
    *(int8_t *)signalDataPtr = (int8_t)u32V;
    break;
  case COM_UINT8:
    *(uint8_t *)signalDataPtr = (uint8_t)u32V;
    break;
  case COM_SINT16:
    if (0u != (u32V & signmask)) {
      u32V |= signmask; /* add sign bits */
    }
    *(int16_t *)signalDataPtr = (int16_t)u32V;
    break;
  case COM_UINT16:
    *(uint16_t *)signalDataPtr = (uint16_t)u32V;
    break;
  case COM_SINT32:
    if (0u != (u32V & signmask)) {
      u32V |= signmask; /* add sign bits */
    }
    *(int32_t *)signalDataPtr = (int32_t)u32V;
    break;
  case COM_UINT32:
    *(uint32_t *)signalDataPtr = (uint32_t)u32V;
    break;
  default:
    ret = E_NOT_OK;
    break;
  }
  return ret;
}

static Std_ReturnType Com_GetSignalValue(const Com_SignalConfigType *signal, uint32_t *sigV,
                                         const void *signalDataPtr) {
  Std_ReturnType ret = E_OK;
  switch (signal->Type) {
  case COM_SINT8:
  case COM_UINT8:
    *sigV = *(const uint8_t *)signalDataPtr;
    break;
  case COM_SINT16:
  case COM_UINT16:
    *sigV = *(const uint16_t *)signalDataPtr;
    break;
  case COM_SINT32:
  case COM_UINT32:
    *sigV = *(const uint32_t *)signalDataPtr;
    break;
  default:
    ret = E_NOT_OK;
    break;
  }

  return ret;
}

static Std_ReturnType Com_ReceiveSignalBig(const Com_SignalConfigType *signal,
                                           void *signalDataPtr) {
  uint32_t sigV = Std_BitGetBigEndian(signal->Ptr, signal->BitPosition, signal->BitSize);
  return Com_StoreSignalValue(signal, sigV, signalDataPtr);
}

static Std_ReturnType Com_SendSignalBig(const Com_SignalConfigType *signal,
                                        const void *signalDataPtr) {
  uint32_t sigV;
  Std_ReturnType ret = Com_GetSignalValue(signal, &sigV, signalDataPtr);

  if (E_OK == ret) {
    Std_BitSetBigEndian(signal->Ptr, sigV, signal->BitPosition, signal->BitSize);
  }

  return ret;
}

static Std_ReturnType Com_ReceiveSignalLittle(const Com_SignalConfigType *signal,
                                              void *signalDataPtr) {
  uint32_t sigV = Std_BitGetLittleEndian(signal->Ptr, signal->BitPosition, signal->BitSize);
  return Com_StoreSignalValue(signal, sigV, signalDataPtr);
}

static Std_ReturnType Com_SendSignalLittle(const Com_SignalConfigType *signal,
                                           const void *signalDataPtr) {
  uint32_t sigV;
  Std_ReturnType ret = Com_GetSignalValue(signal, &sigV, signalDataPtr);

  if (E_OK == ret) {
    Std_BitSetLittleEndian(signal->Ptr, sigV, signal->BitPosition, signal->BitSize);
  }

  return ret;
}

static Std_ReturnType Com_ReceiveSignalByConfig(const Com_SignalConfigType *signal,
                                                void *signalDataPtr) {
  Std_ReturnType ret = E_OK;
#ifdef COM_USE_SIGNAL_UPDATE_BIT
  boolean isUpdated;
  if (signal->UpdateBit != COM_UPDATE_BIT_NOT_USED) {
    isUpdated = Std_BitGet(signal->Ptr, signal->UpdateBit);
    if (FALSE == isUpdated) {
      ret = E_NOT_OK;
    } else {
      Std_BitClear(signal->Ptr, signal->UpdateBit);
    }
  }
  if (E_OK == ret) {
#endif
    if ((COM_UINT8N == signal->Type) || (COM_OPAQUE == signal->Endianness)) {
      /* @SWS_Com_00472 */
      (void)memcpy(signalDataPtr, signal->Ptr, (signal->BitSize >> 3));
      ret = E_OK;
    } else {
      switch (signal->Endianness) {
      case COM_BIG_ENDIAN:
        ret = Com_ReceiveSignalBig(signal, signalDataPtr);
        break;
      case COM_LITTLE_ENDIAN:
        ret = Com_ReceiveSignalLittle(signal, signalDataPtr);
        break;
      default:
        ret = E_NOT_OK;
        break;
      }
    }
#ifdef COM_USE_SIGNAL_UPDATE_BIT
  }
#endif
  return ret;
}

static Std_ReturnType Com_SendSignalByConfig(const Com_SignalConfigType *signal,
                                             const void *signalDataPtr) {
  Std_ReturnType ret = E_NOT_OK;
  if ((COM_UINT8N == signal->Type) || (COM_OPAQUE == signal->Endianness)) {
    /* @SWS_Com_00472 */
    (void)memcpy(signal->Ptr, signalDataPtr, (signal->BitSize >> 3));
    ret = E_OK;
  } else {
    switch (signal->Endianness) {
    case COM_BIG_ENDIAN:
      ret = Com_SendSignalBig(signal, signalDataPtr);
      break;
    case COM_LITTLE_ENDIAN:
      ret = Com_SendSignalLittle(signal, signalDataPtr);
      break;
    default:
      break;
    }
  }
#ifdef COM_USE_SIGNAL_UPDATE_BIT
  if (signal->UpdateBit != COM_UPDATE_BIT_NOT_USED) {
    Std_BitSet(signal->Ptr, signal->UpdateBit);
  }
#endif
  return ret;
}

static void Com_IPduDataInit(const Com_IPduConfigType *iPduConfig) {
  const Com_SignalConfigType *signal;
  uint16_t i;
  for (i = 0; i < iPduConfig->NumOfSignals; i++) {
    signal = iPduConfig->Signals[i];
    (void)Com_SendSignalByConfig(signal, signal->InitPtr);
  }
}

#ifdef COM_USE_SIGNAL_UPDATE_BIT
static void Com_TxClearUpdateBit(const Com_IPduConfigType *iPduConfig) {
  const Com_SignalConfigType *signal;
  uint16_t i;
  for (i = 0; i < iPduConfig->NumOfSignals; i++) {
    signal = iPduConfig->Signals[i];
    if (signal->UpdateBit != COM_UPDATE_BIT_NOT_USED) {
      Std_BitClear(signal->Ptr, signal->UpdateBit);
    }
  }
}
#endif

static Com_DataLengthType Com_GetMinimumLength(const Com_IPduConfigType *iPduConfig) {
  const Com_SignalConfigType *signal;
  Com_DataLengthType minLen = iPduConfig->Length;

  if (NULL != iPduConfig->DynLen) { /* only the last signal can be dyn */
    signal = iPduConfig->Signals[iPduConfig->NumOfSignals - 1u];
    minLen -= (signal->BitSize >> 3); /* the last dyn signal size can be 0 */
  }

  return minLen;
}

#ifdef USE_SHELL
static int Com_CmdLsSgFunc(int argc, const char *argv[]) {
  union {
    uint32_t U32V;
    uint16_t U16V;
    uint8_t U8V;
  } uV;
  const Com_IPduConfigType *IPdu;
  const Com_SignalConfigType *signal;
  int i, j;

  if (2 == argc) {
    i = (int)strtoul(argv[1], NULL, 10);
  } else {
    for (i = 0; i < COM_CONFIG->NumOfIPdus; i++) {
      IPdu = &COM_CONFIG->IPduConfigs[i];
      PRINTF("%d %s\n", i, IPdu->Name);
    }
    return 0;
  }
  if (i < COM_CONFIG->NumOfIPdus) {
    IPdu = &COM_CONFIG->IPduConfigs[i];
    if (IPdu->RxConfig) {
      for (j = 0; j < IPdu->NumOfSignals; j++) {
        signal = IPdu->Signals[j];
        if (signal->IsGroupSignal) {
          Com_ReceiveSignalGroup(signal->HandleId);
        }
      }
    }
    for (j = 0; j < IPdu->NumOfSignals; j++) {
      signal = IPdu->Signals[j];

      PRINTF("%s ", (IPdu->TxConfig) ? "T" : "R");
      if (signal->IsGroupSignal) {
        PRINTF("%s.%s(GID=%d) is group signal\n", IPdu->Name, signal->Name, signal->HandleId);
        continue;
      }
      switch (signal->Type) {
      case COM_UINT8:
      case COM_SINT8:
        (void)Com_ReceiveSignal(signal->HandleId, &uV.U8V);
        PRINTF("%s.%s(SID=%d): V = 0x%02X(%d)\n", IPdu->Name, signal->Name, signal->HandleId,
               uV.U8V, uV.U8V);
        break;
      case COM_UINT16:
      case COM_SINT16:
        (void)Com_ReceiveSignal(signal->HandleId, &uV.U16V);
        PRINTF("%s.%s(SID=%d): V = 0x%04X(%d)\n", IPdu->Name, signal->Name, signal->HandleId,
               uV.U16V, uV.U16V);
        break;
      case COM_UINT32:
      case COM_SINT32:
        (void)Com_ReceiveSignal(signal->HandleId, &uV.U32V);
        PRINTF("%s.%s(SID=%d): V = 0x%08X(%d)\n", IPdu->Name, signal->Name, signal->HandleId,
               uV.U32V, uV.U32V);
        break;
      default:
        PRINTF("%s.%s(SID=%d): unsupported type %d\n", IPdu->Name, signal->Name, signal->HandleId,
               signal->Type);
        break;
      }
    }
  }
  return 0;
}
static int Com_CmdWrSgFunc(int argc, const char *argv[]) {
  Com_SignalIdType sid, gid = (Com_SignalIdType)-1;
  uint32_t u32V;
  union {
    uint32_t U32V;
    uint16_t U16V;
    uint8_t U8V;
  } uV;
  const Com_SignalConfigType *signal;

  if (argc < 3) {
    return -1;
  }

  sid = strtoul(argv[1], NULL, 10);
  if (sid >= COM_CONFIG->NumOfSignals) {
    return -2;
  }
  if (0 == strncmp("0x", argv[2], 2)) {
    u32V = strtoul(argv[2], NULL, 16);
  } else {
    u32V = strtoul(argv[2], NULL, 10);
  }
  if (argc >= 4) {
    gid = strtoul(argv[3], NULL, 10);
    if (gid >= COM_CONFIG->NumOfSignals) {
      return -3;
    }
  }

  signal = &COM_CONFIG->SignalConfigs[sid];
  switch (signal->Type) {
  case COM_UINT8:
  case COM_SINT8:
    uV.U8V = (uint8)u32V;
    (void)Com_SendSignal(signal->HandleId, &uV.U8V);
    break;
  case COM_UINT16:
  case COM_SINT16:
    uV.U16V = (uint16)u32V;
    (void)Com_SendSignal(signal->HandleId, &uV.U16V);
    break;
  case COM_UINT32:
  case COM_SINT32:
    uV.U32V = (uint32)u32V;
    (void)Com_SendSignal(signal->HandleId, &uV.U32V);
    break;
  default:
    break;
  }

  if (((Com_SignalIdType)-1) != gid) {
    Com_SendSignalGroup(gid);
  }

  return 0;
}
SHELL_REGISTER(lssg, "list all the value of com signals\n", Com_CmdLsSgFunc);
SHELL_REGISTER(wrsg,
              "wrsg sid value [gid]\n"
              "  write signal, if sid is group signals, need the gid\n",
              Com_CmdWrSgFunc);
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
void Com_Init(const Com_ConfigType *config) {
#ifdef COM_USE_PB_CONFIG
  if (NULL != config) {
    COM_CONFIG = config;
  } else {
    COM_CONFIG = &Com_Config;
  }
#else
  (void)config;
#endif
  COM_CONFIG->Context->GroupStatus = 0u;
#ifdef USE_DCM
  COM_CONFIG->Context->DcmComMode = 0x00u;
#endif
}

void Com_IpduGroupStart(Com_IpduGroupIdType ipduGroupId, boolean initialize) {
  const Com_IPduConfigType *IPduConfig;
  uint16_t i;
#ifdef COM_USE_SIGNAL_CONFIG
  const Com_SignalConfigType *signal;
  uint16_t j;
#endif
  DET_VALIDATE(NULL != COM_CONFIG, 0x03, COM_E_UNINIT, return);
  DET_VALIDATE(ipduGroupId < COM_CONFIG->NumOfGroups, 0x03, COM_E_PARAM, return);

  COM_CONFIG->Context->GroupStatus |= (1u << ipduGroupId);
  for (i = 0; i < COM_CONFIG->NumOfIPdus; i++) {
    IPduConfig = &COM_CONFIG->IPduConfigs[i];
    if (0u != (IPduConfig->GroupRefMask & (1u << ipduGroupId))) {
      if (TRUE == initialize) {
        Com_IPduDataInit(IPduConfig);
      }
      if (NULL != IPduConfig->RxConfig) {
        if (IPduConfig->RxConfig->FirstTimeout > 0u) {
          IPduConfig->RxConfig->Context->Timer = IPduConfig->RxConfig->FirstTimeout;
        } else {
          IPduConfig->RxConfig->Context->Timer = IPduConfig->RxConfig->Timeout;
        }

#ifdef COM_USE_SIGNAL_CONFIG
        for (j = 0; j < IPduConfig->NumOfSignals; j++) {
          signal = IPduConfig->Signals[j];
          if (NULL != signal->RxConfig) {
            if (signal->RxConfig->FirstTimeout > 0u) {
              signal->RxConfig->Context->Timer = signal->RxConfig->FirstTimeout;
            } else {
              signal->RxConfig->Context->Timer = signal->RxConfig->Timeout;
            }
          }
        }
#endif
      } else if ((NULL != IPduConfig->TxConfig) && (NULL != IPduConfig->TxConfig->Context)) {
        /* For LIN, as trigger transmit by LinIf, it has no context */
        if (IPduConfig->TxConfig->FirstTime > 0u) {
          IPduConfig->TxConfig->Context->Timer = IPduConfig->TxConfig->FirstTime;
        } else {
          IPduConfig->TxConfig->Context->Timer = IPduConfig->TxConfig->CycleTime;
        }
#ifdef COM_USE_MAIN_FAST
        IPduConfig->TxConfig->Context->TxRetry = FALSE;
#endif
      } else {
        /* do nothing */
      }
    }
  }
}

void Com_IpduGroupStop(Com_IpduGroupIdType ipduGroupId) {

  DET_VALIDATE(NULL != COM_CONFIG, 0x04, COM_E_UNINIT, return);
  DET_VALIDATE(ipduGroupId < COM_CONFIG->NumOfGroups, 0x04, COM_E_PARAM, return);

  if (ipduGroupId < COM_CONFIG->NumOfGroups) {
    COM_CONFIG->Context->GroupStatus &= ~(1u << ipduGroupId);
  }
}

Std_ReturnType Com_ReceiveSignal(Com_SignalIdType signalId, void *signalDataPtr) {
  Std_ReturnType ret = E_NOT_OK;
  const Com_SignalConfigType *signal;

  DET_VALIDATE(NULL != COM_CONFIG, 0x0B, COM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(signalId < COM_CONFIG->NumOfSignals, 0x0B, COM_E_PARAM, return E_NOT_OK);
  DET_VALIDATE(NULL != signalDataPtr, 0x0B, COM_E_PARAM_POINTER, return E_NOT_OK);

  signal = &COM_CONFIG->SignalConfigs[signalId];
  ret = Com_ReceiveSignalByConfig(signal, signalDataPtr);

  return ret;
}

Std_ReturnType Com_SendSignal(Com_SignalIdType signalId, const void *signalDataPtr) {
  Std_ReturnType ret = E_NOT_OK;
  const Com_SignalConfigType *signal;

  DET_VALIDATE(NULL != COM_CONFIG, 0x0A, COM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(signalId < COM_CONFIG->NumOfSignals, 0x0A, COM_E_PARAM, return E_NOT_OK);
  DET_VALIDATE(NULL != signalDataPtr, 0x0A, COM_E_PARAM_POINTER, return E_NOT_OK);

  signal = &COM_CONFIG->SignalConfigs[signalId];
  ret = Com_SendSignalByConfig(signal, signalDataPtr);

  return ret;
}

Std_ReturnType Com_SendDynSignal(Com_SignalIdType signalId, const void *signalDataPtr,
                                 uint16_t length) {
  Std_ReturnType ret = E_NOT_OK;
  const Com_SignalConfigType *signal;
  const Com_IPduConfigType *IPduConfig;

  DET_VALIDATE(NULL != COM_CONFIG, 0x21, COM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(signalId < COM_CONFIG->NumOfSignals, 0x21, COM_E_PARAM, return E_NOT_OK);
  DET_VALIDATE(NULL != signalDataPtr, 0x21, COM_E_PARAM_POINTER, return E_NOT_OK);

  signal = &COM_CONFIG->SignalConfigs[signalId];
  DET_VALIDATE(COM_UINT8_DYN == signal->Type, 0x21, COM_E_PARAM, return E_NOT_OK);
  IPduConfig = &COM_CONFIG->IPduConfigs[signal->PduId];
  DET_VALIDATE(IPduConfig->Signals[IPduConfig->NumOfSignals - 1] == signal, 0x21, COM_E_PARAM,
               return E_NOT_OK); /* only the last signal can be dyn type */
  if (length <= (signal->BitSize >> 3)) {
    (void)memcpy(signal->Ptr, signalDataPtr, length);
    *IPduConfig->DynLen = IPduConfig->Length - (signal->BitSize >> 3) + length;
    ret = E_OK;
  }

  return ret;
}

Std_ReturnType Com_ReceiveDynSignal(Com_SignalIdType signalId, void *signalDataPtr,
                                    uint16_t *length) {
  Std_ReturnType ret = E_NOT_OK;
  const Com_SignalConfigType *signal;
  const Com_IPduConfigType *IPduConfig;
  Com_DataLengthType dynLen;

  DET_VALIDATE(NULL != COM_CONFIG, 0x22, COM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(signalId < COM_CONFIG->NumOfSignals, 0x22, COM_E_PARAM, return E_NOT_OK);
  DET_VALIDATE(NULL != signalDataPtr, 0x22, COM_E_PARAM_POINTER, return E_NOT_OK);
  DET_VALIDATE(NULL != length, 0x22, COM_E_PARAM_POINTER, return E_NOT_OK);

  signal = &COM_CONFIG->SignalConfigs[signalId];
  DET_VALIDATE(COM_UINT8_DYN == signal->Type, 0x22, COM_E_PARAM, return E_NOT_OK);
  IPduConfig = &COM_CONFIG->IPduConfigs[signal->PduId];
  DET_VALIDATE(IPduConfig->Signals[IPduConfig->NumOfSignals - 1] == signal, 0x22, COM_E_PARAM,
               return E_NOT_OK); /* only the last signal can be dyn type */
  dynLen = Com_GetMinimumLength(IPduConfig);
  dynLen = *IPduConfig->DynLen - dynLen;

  if (*length >= dynLen) {
    (void)memcpy(signalDataPtr, signal->Ptr, dynLen);
    *length = dynLen;
    ret = E_OK;
  }

  return ret;
}

Std_ReturnType Com_SendSignalGroup(Com_SignalGroupIdType signalGroupId) {
  Std_ReturnType ret = E_OK;
  const Com_SignalConfigType *signal;

  DET_VALIDATE(NULL != COM_CONFIG, 0x0D, COM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(signalGroupId < COM_CONFIG->NumOfSignals, 0x0D, COM_E_PARAM, return E_NOT_OK);
  signal = &COM_CONFIG->SignalConfigs[signalGroupId];
  DET_VALIDATE(COM_UINT8N == signal->Type, 0x0D, COM_E_PARAM, return E_NOT_OK);

  (void)memcpy(signal->Ptr, signal->InitPtr, (signal->BitSize >> 3));

  return ret;
}

Std_ReturnType Com_ReceiveSignalGroup(Com_SignalGroupIdType signalGroupId) {
  Std_ReturnType ret = E_OK;
  const Com_SignalConfigType *signal;

  DET_VALIDATE(NULL != COM_CONFIG, 0x0E, COM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(signalGroupId < COM_CONFIG->NumOfSignals, 0x0E, COM_E_PARAM, return E_NOT_OK);
  signal = &COM_CONFIG->SignalConfigs[signalGroupId];
  DET_VALIDATE(COM_UINT8N == signal->Type, 0x0E, COM_E_PARAM, return E_NOT_OK);

  (void)memcpy((void *)signal->InitPtr, signal->Ptr, (signal->BitSize >> 3));

  return ret;
}

#if defined(COM_USE_CAN)
Std_ReturnType Com_TriggerIPDUSend(PduIdType pduId) {
  Std_ReturnType ret = E_NOT_OK;
  const Com_IPduConfigType *IPduConfig;
  PduInfoType PduInfo;
#ifdef COM_USE_TX_IPDU_CALLOUT
  boolean bProcess;
#endif

  DET_VALIDATE(NULL != COM_CONFIG, 0x17, COM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(pduId < COM_CONFIG->NumOfIPdus, 0x17, COM_E_INVALID_TXPDUID, return E_NOT_OK);
  IPduConfig = &COM_CONFIG->IPduConfigs[pduId];
  DET_VALIDATE(NULL != IPduConfig->TxConfig, 0x17, COM_E_PARAM, return E_NOT_OK);
  if ((0u != (COM_CONFIG->Context->GroupStatus & IPduConfig->GroupRefMask))
#ifdef USE_DCM
      && (0u == (COM_CONFIG->Context->DcmComMode & COM_DCM_COM_MODE_TX_DISABLED))
#endif
  ) {
    PduInfo.SduDataPtr = IPduConfig->Ptr;
    PduInfo.SduLength = IPduConfig->Length;
    if (NULL != IPduConfig->DynLen) {
      PduInfo.SduLength = *IPduConfig->DynLen;
    }
#ifdef COM_USE_TX_IPDU_CALLOUT
    if (NULL != IPduConfig->TxConfig->TxIpduCallout) {
      bProcess = IPduConfig->TxConfig->TxIpduCallout((PduIdType)pduId, &PduInfo);
    } else {
      bProcess = TRUE;
    }
    if (TRUE == bProcess) { /* @SWS_Com_00492 */
#endif
      ret = PduR_ComTransmit(IPduConfig->TxConfig->TxPduId, &PduInfo);
      if (E_OK == ret) {
        IPduConfig->TxConfig->Context->Timer = IPduConfig->TxConfig->CycleTime;
      } else {
        IPduConfig->TxConfig->Context->Timer = 1;
        ret = E_OK;
      }
#ifdef COM_USE_TX_IPDU_CALLOUT
    }
#endif
  }

  return ret;
}
#endif

void Com_RxIndication(PduIdType rxPduId, const PduInfoType *pduInfoPtr) {
  const Com_IPduConfigType *IPduConfig;
#ifdef COM_USE_RX_IPDU_CALLOUT
  boolean bProcess;
#endif
#ifdef COM_USE_SIGNAL_CONFIG
  const Com_SignalConfigType *signal;
  uint16_t i;
#endif
  Com_DataLengthType dynLen;
  DET_VALIDATE(NULL != COM_CONFIG, 0x42, COM_E_UNINIT, return);
  DET_VALIDATE(rxPduId < COM_CONFIG->NumOfIPdus, 0x42, COM_E_INVALID_RXPDUID, return);
  IPduConfig = &COM_CONFIG->IPduConfigs[rxPduId];
  DET_VALIDATE(NULL != IPduConfig->RxConfig, 0x42, COM_E_PARAM, return);
  dynLen = Com_GetMinimumLength(IPduConfig);
  DET_VALIDATE(dynLen <= pduInfoPtr->SduLength, 0x42, COM_E_PARAM, return);
  if ((0u != (COM_CONFIG->Context->GroupStatus & IPduConfig->GroupRefMask))
#ifdef USE_DCM
      && (0u == (COM_CONFIG->Context->DcmComMode & COM_DCM_COM_MODE_RX_DISABLED))
#endif
  ) {
#ifdef COM_USE_RX_IPDU_CALLOUT
    if (NULL != IPduConfig->RxConfig->RxIpduCallout) {
      bProcess = IPduConfig->RxConfig->RxIpduCallout(rxPduId, pduInfoPtr);
    } else {
      bProcess = TRUE;
    }
    if (TRUE == bProcess) {
#endif
      if (NULL != IPduConfig->DynLen) {
        dynLen = pduInfoPtr->SduLength;
        if (dynLen > IPduConfig->Length) {
          dynLen = IPduConfig->Length;
        }
        *IPduConfig->DynLen = dynLen;
      }
      (void)memcpy(IPduConfig->Ptr, pduInfoPtr->SduDataPtr, dynLen);
      IPduConfig->RxConfig->Context->Timer = IPduConfig->RxConfig->Timeout;
#ifdef COM_USE_RX_NOTIFICATION
      if (IPduConfig->RxConfig->RxNotification) {
        IPduConfig->RxConfig->RxNotification();
      }
#endif
#ifdef COM_USE_SIGNAL_CONFIG
      for (i = 0; i < IPduConfig->NumOfSignals; i++) {
        signal = IPduConfig->Signals[i];
        if (NULL != signal->RxConfig) {
          signal->RxConfig->Context->Timer = signal->RxConfig->Timeout;
#ifdef COM_USE_SIGNAL_RX_NOTIFICATION
          if (NULL != signal->RxConfig->RxNotification) {
            signal->RxConfig->RxNotification();
          }
#endif
        }
      }
#endif
#ifdef COM_USE_RX_IPDU_CALLOUT
    }
#endif
  }
}

Std_ReturnType Com_TriggerTransmit(PduIdType txPduId, PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_OK;
  const Com_IPduConfigType *IPduConfig;
#ifdef COM_USE_TX_IPDU_CALLOUT
  boolean bProcess;
  PduInfoType PduInfo;
#endif

  DET_VALIDATE(NULL != COM_CONFIG, 0x41, COM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(txPduId < COM_CONFIG->NumOfIPdus, 0x41, COM_E_INVALID_TXPDUID, return E_NOT_OK);
  DET_VALIDATE((NULL != pduInfoPtr) && (NULL != pduInfoPtr->SduDataPtr), 0x41, COM_E_PARAM_POINTER,
               return E_NOT_OK);
  IPduConfig = &COM_CONFIG->IPduConfigs[txPduId];
  DET_VALIDATE(IPduConfig->Length <= pduInfoPtr->SduLength, 0x41, COM_E_PARAM, return E_NOT_OK);

#ifdef COM_USE_TX_IPDU_CALLOUT
  if (NULL != IPduConfig->TxConfig->TxIpduCallout) {
    PduInfo.SduDataPtr = IPduConfig->Ptr;
    PduInfo.SduLength = IPduConfig->Length;
    bProcess = IPduConfig->TxConfig->TxIpduCallout(txPduId, &PduInfo);
  } else {
    bProcess = TRUE;
  }
  if (TRUE == bProcess) {
#endif
    (void)memcpy(pduInfoPtr->SduDataPtr, IPduConfig->Ptr, IPduConfig->Length);
    pduInfoPtr->SduLength = IPduConfig->Length;
#ifdef COM_USE_TX_IPDU_CALLOUT
  } else {
    ret = E_NOT_OK; /* reject by APP */
  }
#endif

  return ret;
}

BufReq_ReturnType Com_CopyTxData(PduIdType id, const PduInfoType *info, const RetryInfoType *retry,
                                 PduLengthType *availableDataPtr) {
  BufReq_ReturnType bufRet = BUFREQ_E_NOT_OK;
  const Com_IPduConfigType *IPduConfig;
  PduLengthType offset = 0;

  DET_VALIDATE(NULL != COM_CONFIG, 0x43, COM_E_UNINIT, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(id < COM_CONFIG->NumOfIPdus, 0x43, COM_E_INVALID_TXPDUID, return BUFREQ_E_NOT_OK);
  IPduConfig = &COM_CONFIG->IPduConfigs[id];
  DET_VALIDATE(NULL != IPduConfig->TxConfig, 0x43, COM_E_PARAM, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(info->MetaDataPtr != NULL, 0x43, COM_E_PARAM, return BUFREQ_E_NOT_OK);
  (void)retry;
  if (0u != (COM_CONFIG->Context->GroupStatus & IPduConfig->GroupRefMask)) {
    offset = *(PduLengthType *)info->MetaDataPtr;

    if ((offset + info->SduLength) <= IPduConfig->Length) {
      (void)memcpy(info->SduDataPtr, ((uint8_t *)IPduConfig->Ptr) + offset, info->SduLength);
      *availableDataPtr = IPduConfig->Length - (offset + info->SduLength);
      bufRet = BUFREQ_OK;
    }
  }

  return bufRet;
}

void Com_TxConfirmation(PduIdType txPduId, Std_ReturnType result) {
  const Com_IPduConfigType *IPduConfig;
#if defined(COM_USE_SIGNAL_CONFIG) &&                                                              \
  (defined(COM_USE_SIGNAL_TX_NOTIFICATION) || defined(COM_USE_SIGNAL_TX_ERROR_NOTIFICATION))
  const Com_SignalConfigType *signal;
  uint16_t i;
#endif

  DET_VALIDATE(NULL != COM_CONFIG, 0x40, COM_E_UNINIT, return);
  DET_VALIDATE(txPduId < COM_CONFIG->NumOfIPdus, 0x40, COM_E_INVALID_TXPDUID, return);
  IPduConfig = &COM_CONFIG->IPduConfigs[txPduId];
  DET_VALIDATE(NULL != IPduConfig->TxConfig, 0x40, COM_E_PARAM, return);
  if (0u != (COM_CONFIG->Context->GroupStatus & IPduConfig->GroupRefMask)) {
    if (E_OK == result) {
#ifdef COM_USE_TX_NOTIFICATION
      if (IPduConfig->TxConfig->TxNotification) {
        IPduConfig->TxConfig->TxNotification();
      }
#endif
#if defined(COM_USE_SIGNAL_CONFIG) && defined(COM_USE_SIGNAL_TX_NOTIFICATION)
      for (i = 0; i < IPduConfig->NumOfSignals; i++) {
        signal = IPduConfig->Signals[i];
        if ((NULL != signal->TxConfig) && (NULL != signal->TxConfig->TxNotification)) {
          signal->TxConfig->TxNotification();
        }
      }
#endif
    } else {
#ifdef COM_USE_TX_ERROR_NOTIFICATION
      if (IPduConfig->TxConfig->ErrorNotification) {
        IPduConfig->TxConfig->ErrorNotification();
      }
#endif
#if defined(COM_USE_SIGNAL_CONFIG) && defined(COM_USE_SIGNAL_TX_ERROR_NOTIFICATION)
      for (i = 0; i < IPduConfig->NumOfSignals; i++) {
        signal = IPduConfig->Signals[i];
        if ((NULL != signal->TxConfig) && (NULL != signal->TxConfig->ErrorNotification)) {
          signal->TxConfig->ErrorNotification();
        }
      }
#endif
    }
  }
}

void Com_MainFunctionRx(void) {
  const Com_IPduConfigType *IPduConfig;
  uint16_t i;
#ifdef COM_USE_SIGNAL_CONFIG
  const Com_SignalConfigType *signal;
  uint16_t j;
#endif

  DET_VALIDATE(NULL != COM_CONFIG, 0x18, COM_E_UNINIT, return);

  for (i = 0; i < COM_CONFIG->NumOfIPdus; i++) {
    IPduConfig = &COM_CONFIG->IPduConfigs[i];
    if ((NULL != IPduConfig->RxConfig) &&
#ifdef USE_DCM
        (0u == (COM_CONFIG->Context->DcmComMode & COM_DCM_COM_MODE_RX_DISABLED)) &&
#endif
        (0u != (COM_CONFIG->Context->GroupStatus & IPduConfig->GroupRefMask))) {
      if (IPduConfig->RxConfig->Context->Timer > 0u) {
        IPduConfig->RxConfig->Context->Timer--;
        if (0u == IPduConfig->RxConfig->Context->Timer) {
          IPduConfig->RxConfig->Context->Timer = IPduConfig->RxConfig->Timeout;
#ifdef COM_USE_RX_TIMEOUT
          if (IPduConfig->RxConfig->RxTOut) {
            IPduConfig->RxConfig->RxTOut();
          }
#endif
        }
      }
#ifdef COM_USE_SIGNAL_CONFIG
      for (j = 0; j < IPduConfig->NumOfSignals; j++) {
        signal = IPduConfig->Signals[j];
        if (NULL != signal->RxConfig) {
          if (signal->RxConfig->Context->Timer > 0u) {
            signal->RxConfig->Context->Timer--;
            if (0u == signal->RxConfig->Context->Timer) {
              signal->RxConfig->Context->Timer = signal->RxConfig->Timeout;
              switch (signal->RxConfig->RxDataTimeoutAction) {
              case COM_ACTION_REPLACE:
                (void)Com_SendSignalByConfig(signal, signal->InitPtr);
                break;
              case COM_ACTION_SUBSTITUTE:
                (void)Com_SendSignalByConfig(signal, signal->RxConfig->TimeoutSubstitutionValue);
                break;
              default:
                break;
              }
#ifdef COM_USE_SIGNAL_RX_TIMEOUT
              if (NULL != signal->RxConfig->RxTOut) {
                signal->RxConfig->RxTOut();
              }
#endif
            }
          }
        }
      }
#endif
    }
  }
}

void Com_MainFunctionTx(void) {
#if defined(COM_USE_CAN)
  const Com_IPduConfigType *IPduConfig;
  Std_ReturnType ret;
  PduInfoType PduInfo;
#ifdef COM_USE_TX_IPDU_CALLOUT
  boolean bProcess;
#endif
  uint16_t i;

  DET_VALIDATE(NULL != COM_CONFIG, 0x19, COM_E_UNINIT, return);

  for (i = 0; i < COM_CONFIG->NumOfIPdus; i++) {
    IPduConfig = &COM_CONFIG->IPduConfigs[i];
    if ((0u != (COM_CONFIG->Context->GroupStatus & IPduConfig->GroupRefMask) &&
#ifdef USE_DCM
         (0u == (COM_CONFIG->Context->DcmComMode & COM_DCM_COM_MODE_TX_DISABLED)) &&
#endif
         (NULL != IPduConfig->TxConfig) && (NULL != IPduConfig->TxConfig->Context))) {
      if (IPduConfig->TxConfig->Context->Timer > 0u) {
        IPduConfig->TxConfig->Context->Timer--;
        if (0u == IPduConfig->TxConfig->Context->Timer) {
          PduInfo.MetaDataPtr = NULL;
          PduInfo.SduDataPtr = IPduConfig->Ptr;
          PduInfo.SduLength = IPduConfig->Length;
          if (NULL != IPduConfig->DynLen) {
            PduInfo.SduLength = *IPduConfig->DynLen;
          }
#ifdef COM_USE_TX_IPDU_CALLOUT
          if (NULL != IPduConfig->TxConfig->TxIpduCallout) {
            bProcess = IPduConfig->TxConfig->TxIpduCallout((PduIdType)i, &PduInfo);
          } else {
            bProcess = TRUE;
          }
          if (TRUE == bProcess) {
#endif
            ret = PduR_ComTransmit(IPduConfig->TxConfig->TxPduId, &PduInfo);
            if (E_OK == ret) {
              IPduConfig->TxConfig->Context->Timer = IPduConfig->TxConfig->CycleTime;
#ifdef COM_USE_SIGNAL_UPDATE_BIT
              Com_TxClearUpdateBit(IPduConfig);
#endif
            } else {
#ifdef COM_USE_MAIN_FAST
              IPduConfig->TxConfig->Context->Timer = IPduConfig->TxConfig->CycleTime;
              IPduConfig->TxConfig->Context->TxRetry = TRUE;
#else
            IPduConfig->TxConfig->Context->Timer = 1;
#endif
            }
#ifdef COM_USE_TX_IPDU_CALLOUT
          } else { /* cancelled by APP, restart the timer only */
            IPduConfig->TxConfig->Context->Timer = IPduConfig->TxConfig->CycleTime;
          }
#endif
        }
      }
    }
  }
#endif
}

#ifdef COM_USE_MAIN_FAST
static void Com_MainFunctionTx_Fast(void) {
#if defined(COM_USE_CAN)
  const Com_IPduConfigType *IPduConfig;
  Std_ReturnType ret;
  PduInfoType PduInfo;
  uint16_t i;

  DET_VALIDATE(NULL != COM_CONFIG, 0x19, COM_E_UNINIT, return);

  for (i = 0; i < COM_CONFIG->NumOfIPdus; i++) {
    IPduConfig = &COM_CONFIG->IPduConfigs[i];
    if ((0u != (COM_CONFIG->Context->GroupStatus & IPduConfig->GroupRefMask) &&
         (NULL != IPduConfig->TxConfig) && (NULL != IPduConfig->TxConfig->Context))) {
      if (TRUE == IPduConfig->TxConfig->Context->TxRetry) {
        PduInfo.SduDataPtr = IPduConfig->Ptr;
        PduInfo.SduLength = IPduConfig->Length;
        if (NULL != IPduConfig->DynLen) {
          PduInfo.SduLength = *IPduConfig->DynLen;
        }
        ret = PduR_ComTransmit(IPduConfig->TxConfig->TxPduId, &PduInfo);
        if (E_OK == ret) {
#ifdef COM_USE_SIGNAL_UPDATE_BIT
          Com_TxClearUpdateBit(IPduConfig);
#endif
          IPduConfig->TxConfig->Context->TxRetry = FALSE;
        }
      }
    }
  }
#endif
}
#endif

void Com_MainFunction(void) {
  Com_MainFunctionRx();
  Com_MainFunctionTx();
}

void Com_MainFunction_Fast(void) {
#ifdef COM_USE_MAIN_FAST
  Com_MainFunctionTx_Fast();
#endif
}

BufReq_ReturnType Com_StartOfReception(PduIdType id, const PduInfoType *info,
                                       PduLengthType tpSduLength, PduLengthType *bufferSizePtr) {
  BufReq_ReturnType bufRet = BUFREQ_E_NOT_OK;
  const Com_IPduConfigType *IPduConfig;
  Com_DataLengthType dynLen;
  DET_VALIDATE(NULL != COM_CONFIG, 0x43, COM_E_UNINIT, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(id < COM_CONFIG->NumOfIPdus, 0x43, COM_E_INVALID_RXPDUID, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != bufferSizePtr, 0x43, COM_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);
  IPduConfig = &COM_CONFIG->IPduConfigs[id];
  DET_VALIDATE(NULL != IPduConfig->RxConfig, 0x43, COM_E_PARAM, return BUFREQ_E_NOT_OK);
  (void)info;
  if (0u != (COM_CONFIG->Context->GroupStatus & IPduConfig->GroupRefMask)) {
    dynLen = Com_GetMinimumLength(IPduConfig);
    if (NULL != IPduConfig->DynLen) {
      dynLen = (Com_DataLengthType)tpSduLength;
      if (dynLen > IPduConfig->Length) {
        dynLen = IPduConfig->Length;
      }
      *IPduConfig->DynLen = dynLen;
    }

    if (dynLen == tpSduLength) {
      *bufferSizePtr = IPduConfig->Length;
      bufRet = BUFREQ_OK;
    } else {
      /* message too short or too long , reject */
    }
  }

  return bufRet;
}

BufReq_ReturnType Com_CopyRxData(PduIdType id, const PduInfoType *info,
                                 PduLengthType *bufferSizePtr) {
  BufReq_ReturnType bufRet = BUFREQ_E_NOT_OK;
  const Com_IPduConfigType *IPduConfig;
  PduLengthType offset = 0;

  DET_VALIDATE(NULL != COM_CONFIG, 0x44, COM_E_UNINIT, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(id < COM_CONFIG->NumOfIPdus, 0x44, COM_E_INVALID_RXPDUID, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != bufferSizePtr, 0x44, COM_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != info, 0x44, COM_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);
  DET_VALIDATE(NULL != info->MetaDataPtr, 0x44, COM_E_PARAM_POINTER, return BUFREQ_E_NOT_OK);
  IPduConfig = &COM_CONFIG->IPduConfigs[id];
  DET_VALIDATE(NULL != IPduConfig->RxConfig, 0x44, COM_E_PARAM, return BUFREQ_E_NOT_OK);
  if (0u != (COM_CONFIG->Context->GroupStatus & IPduConfig->GroupRefMask)) {
    offset = *(PduLengthType *)info->MetaDataPtr;

    if ((offset + info->SduLength) <= IPduConfig->Length) {
      (void)memcpy(((uint8_t *)IPduConfig->Ptr) + offset, info->SduDataPtr, info->SduLength);
      if (NULL == IPduConfig->DynLen) {
        *bufferSizePtr = IPduConfig->Length - (offset + info->SduLength);
      } else {
        *bufferSizePtr = *IPduConfig->DynLen - (offset + info->SduLength);
      }
      bufRet = BUFREQ_OK;
    }
  }

  return bufRet;
}

void Com_TpRxIndication(PduIdType id, Std_ReturnType result) {
  const Com_IPduConfigType *IPduConfig;
#ifdef COM_USE_RX_IPDU_CALLOUT
  PduInfoType PduInfo;
  boolean bProcess;
#endif
#ifdef COM_USE_SIGNAL_CONFIG
  const Com_SignalConfigType *signal;
  uint16_t i;
#endif
  DET_VALIDATE(NULL != COM_CONFIG, 0x45, COM_E_UNINIT, return);
  DET_VALIDATE(id < COM_CONFIG->NumOfIPdus, 0x45, COM_E_INVALID_RXPDUID, return);
  IPduConfig = &COM_CONFIG->IPduConfigs[id];
  DET_VALIDATE(NULL != IPduConfig->RxConfig, 0x45, COM_E_PARAM, return);
  if ((COM_CONFIG->Context->GroupStatus & IPduConfig->GroupRefMask) && (E_OK == result)) {
#ifdef COM_USE_RX_IPDU_CALLOUT
    if (NULL != IPduConfig->RxConfig->RxIpduCallout) {
      if (NULL == IPduConfig->DynLen) {
        PduInfo.SduLength = IPduConfig->Length;
      } else {
        PduInfo.SduLength = *IPduConfig->DynLen;
      }
      PduInfo.SduDataPtr = (uint8_t *)IPduConfig->Ptr;
      bProcess = IPduConfig->RxConfig->RxIpduCallout(id, &PduInfo);
    } else {
      bProcess = TRUE;
    }
    if (TRUE == bProcess) {
#endif
      IPduConfig->RxConfig->Context->Timer = IPduConfig->RxConfig->Timeout;
#ifdef COM_USE_RX_NOTIFICATION
      if (IPduConfig->RxConfig->RxNotification) {
        IPduConfig->RxConfig->RxNotification();
      }
#endif
#ifdef COM_USE_SIGNAL_CONFIG
      for (i = 0; i < IPduConfig->NumOfSignals; i++) {
        signal = IPduConfig->Signals[i];
        if (NULL != signal->RxConfig) {
          signal->RxConfig->Context->Timer = signal->RxConfig->Timeout;
#ifdef COM_USE_SIGNAL_RX_NOTIFICATION
          if (NULL != signal->RxConfig->RxNotification) {
            signal->RxConfig->RxNotification();
          }
#endif
        }
      }
#endif
#ifdef COM_USE_RX_IPDU_CALLOUT
    }
#endif
  }
}

void Com_EnableReceptionDM(Com_IpduGroupIdType ipduGroupId) {
  const Com_IPduConfigType *IPduConfig;
  uint16_t i;

  DET_VALIDATE(NULL != COM_CONFIG, 0x06, COM_E_UNINIT, return);
  DET_VALIDATE(ipduGroupId < COM_CONFIG->NumOfGroups, 0x06, COM_E_PARAM, return);

  for (i = 0; i < COM_CONFIG->NumOfIPdus; i++) {
    IPduConfig = &COM_CONFIG->IPduConfigs[i];
    if ((NULL != IPduConfig->RxConfig) &&
        (0u != ((1u << ipduGroupId) & IPduConfig->GroupRefMask))) {
      if (0u == IPduConfig->RxConfig->Context->Timer) {
        if (IPduConfig->RxConfig->FirstTimeout > 0u) {
          IPduConfig->RxConfig->Context->Timer = IPduConfig->RxConfig->FirstTimeout;
        } else {
          IPduConfig->RxConfig->Context->Timer = IPduConfig->RxConfig->Timeout;
        }
      }
    }
  }
}

void Com_DisableReceptionDM(Com_IpduGroupIdType ipduGroupId) {
  const Com_IPduConfigType *IPduConfig;
  uint16_t i;

  DET_VALIDATE(NULL != COM_CONFIG, 0x05, COM_E_UNINIT, return);
  DET_VALIDATE(ipduGroupId < COM_CONFIG->NumOfGroups, 0x05, COM_E_PARAM, return);

  for (i = 0; i < COM_CONFIG->NumOfIPdus; i++) {
    IPduConfig = &COM_CONFIG->IPduConfigs[i];
    if ((NULL != IPduConfig->RxConfig) &&
        (0u != ((1u << ipduGroupId) & IPduConfig->GroupRefMask))) {
      IPduConfig->RxConfig->Context->Timer = 0;
    }
  }
}

#ifdef USE_DCM
void Com_DcmCommunicationControl(Com_DcmComCtrlType comCtrlMode) {
  uint16_t i;
  DET_VALIDATE(NULL != COM_CONFIG, 0xF0, COM_E_UNINIT, return);
  switch (comCtrlMode) {
  case COM_DCM_COM_CTRL_ENABLE_BOTH:
    COM_CONFIG->Context->DcmComMode = 0x00u;
    for (i = 0; i < COM_CONFIG->NumOfGroups; i++) {
      Com_EnableReceptionDM(i);
    }
    break;
  case COM_DCM_COM_CTRL_ENABLE_RX_DISABLE_TX:
    COM_CONFIG->Context->DcmComMode = COM_DCM_COM_MODE_TX_DISABLED;
    for (i = 0; i < COM_CONFIG->NumOfGroups; i++) {
      Com_EnableReceptionDM(i);
    }
    break;
  case COM_DCM_COM_CTRL_DISABLE_RX_ENABLE_TX:
    COM_CONFIG->Context->DcmComMode = COM_DCM_COM_MODE_RX_DISABLED;
    for (i = 0; i < COM_CONFIG->NumOfGroups; i++) {
      Com_DisableReceptionDM(i);
    }
    break;
  case COM_DCM_COM_CTRL_DISABLE_BOTH:
    COM_CONFIG->Context->DcmComMode = COM_DCM_COM_MODE_RX_DISABLED | COM_DCM_COM_MODE_TX_DISABLED;
    for (i = 0; i < COM_CONFIG->NumOfGroups; i++) {
      Com_DisableReceptionDM(i);
    }
    break;
  default:
    break;
  }
}
#endif

void Com_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x09, COM_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_COM;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 1;
  versionInfo->SwPatchVersion = 0;
}

/** @brief release notes
 * - 4.1.0: Add Reception Deadline Monitor and Dcm Comm Ctrl support.
 */
