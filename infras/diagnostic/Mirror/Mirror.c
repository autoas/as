/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Mirror.h"
#include "Mirror_Cfg.h"
#include "Mirror_Priv.h"
#include "CanIf.h"
#include "LinIf.h"
#include "Std_Critical.h"
#include "Std_Debug.h"
#include "Det.h"
#include "StbM.h"
#include "PduR_Mirror.h"
#ifdef USE_SOAD
#include "SoAd.h"
#endif

#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_MIRROR 0
#define AS_LOG_MIRRORI 0
#define AS_LOG_MIRRORE 3

#ifdef MIRROW_USE_PB_CONFIG
#define MIRROR_CONFIG mirrorConfig
#else
#define MIRROR_CONFIG (&Mirror_Config)
#endif

#define MIRROR_NT_STATE_AVAIABLE 0x80u
#define MIRROR_NT_FRAME_ID_AVAIABLE 0x40u
#define MIRROR_NT_PAYLOAD_AVAIABLE 0x20u

#define LIN_BIT(v, pos) (((v) >> (pos)) & 0x01)

#ifndef Mirror_EnterCritical
#define Mirror_EnterCritical EnterCritical
#endif

#ifndef Mirror_ExitCritical
#define Mirror_ExitCritical ExitCritical
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const Mirror_ConfigType Mirror_Config;
/* ================================ [ DATAS     ] ============================================== */
#ifdef MIRROW_USE_PB_CONFIG
static const Mirror_ConfigType *mirrorConfig = NULL;
#endif
static Mirror_ContextType Mirror_Context;
/* ================================ [ LOCALS    ] ============================================== */
static void Mirror_InitSourceCan(NetworkHandleType network) {
  uint16_t i;
  const Mirror_SourceNetworkCanType *config = &MIRROR_CONFIG->SourceNetworkCans[network];
  config->context->bStarted = FALSE;
  config->context->MaxActiveFilterId = 0;
  for (i = 0; i < ((uint16_t)config->NumStaticFilters + config->MaxDynamicFilters); i++) {
    config->CanFiltersStatus[i] = FALSE;
  }
}

static void Mirror_InitSourceLin(NetworkHandleType network) {
  uint16_t i;
  const Mirror_SourceNetworkLinType *config = &MIRROR_CONFIG->SourceNetworkLins[network];
  config->context->bStarted = FALSE;
  config->context->MaxActiveFilterId = 0;
  for (i = 0; i < ((uint16_t)config->NumStaticFilters + config->MaxDynamicFilters); i++) {
    config->LinFiltersStatus[i] = FALSE;
  }
}

static void Mirror_InitDestCan(NetworkHandleType network) {
  const Mirror_DestNetworkCanType *config = &MIRROR_CONFIG->DestNetworkCans[network];
  config->context->SequenceNumber = 0;
  config->RingBuffer->context->in = 0;
  config->RingBuffer->context->out = 0;
}

static void Mirror_InitDestIp(NetworkHandleType network) {
  uint16_t i;
  const Mirror_DestNetworkIpType *config = &MIRROR_CONFIG->DestNetworkIps[network];
#ifdef USE_SOAD
  SoAd_SoConIdType SoConId = (SoAd_SoConIdType)-1;
#endif

  config->context->SequenceNumber = 0;
  config->context->in = 0;
  config->context->out = 0;
  config->context->TxDeadlineTimer = 0;
  for (i = 0; i < config->NumDestBuffers; i++) {
    *config->DestBuffers[i].offset = 0;
  }
#ifdef USE_SOAD
  (void)SoAd_GetSoConId(config->TxPduId, &SoConId);
  (void)SoAd_CloseSoCon(SoConId, TRUE);
#endif
}

static boolean Mirror_IsSourceNetworkCanStarted(NetworkHandleType network) {
  const Mirror_SourceNetworkCanType *config = &MIRROR_CONFIG->SourceNetworkCans[network];
  return config->context->bStarted;
}

static Std_ReturnType Mirror_StartSourceNetworkCan(NetworkHandleType network) {
  Std_ReturnType ret = E_OK;
  const Mirror_SourceNetworkCanType *config = &MIRROR_CONFIG->SourceNetworkCans[network];
/* @SWS_Mirror_00019 */
#ifdef USE_CANIF
  ret = CanIf_EnableBusMirroring(config->ControllerId, TRUE);
  if (E_OK == ret) {
#endif
    config->context->bStarted = TRUE;
    ASLOG(MIRRORI, ("Source CAN %u start\n", network));
#ifdef USE_CANIF
  }
#endif
  return ret;
}

static Std_ReturnType Mirror_StopSourceNetworkCan(NetworkHandleType network) {
  Std_ReturnType ret = E_OK;
  const Mirror_SourceNetworkCanType *config = &MIRROR_CONFIG->SourceNetworkCans[network];
/* @SWS_Mirror_00019 */
#ifdef USE_CANIF
  ret = CanIf_EnableBusMirroring(config->ControllerId, FALSE);
  if (E_OK == ret) {
#endif
    config->context->bStarted = FALSE;
    ASLOG(MIRRORI, ("Source CAN %u stop\n", network));
#ifdef USE_CANIF
  }
#endif
  return ret;
}

static boolean Mirror_IsSourceNetworkLinStarted(NetworkHandleType network) {
  const Mirror_SourceNetworkLinType *config = &MIRROR_CONFIG->SourceNetworkLins[network];
  return config->context->bStarted;
}

static Std_ReturnType Mirror_StartSourceNetworkLin(NetworkHandleType network) {
  Std_ReturnType ret = E_OK;
  const Mirror_SourceNetworkLinType *config = &MIRROR_CONFIG->SourceNetworkLins[network];
/* @SWS_Mirror_00027 */
#ifdef USE_LINIF
  ret = LinIf_EnableBusMirroring(config->Channel, TRUE);
  if (E_OK == ret) {
#endif
    config->context->bStarted = TRUE;
    ASLOG(MIRRORI, ("Source LIN %u start\n", network));
#ifdef USE_LINIF
  }
#endif
  return ret;
}

static Std_ReturnType Mirror_StopSourceNetworkLin(NetworkHandleType network) {
  Std_ReturnType ret = E_OK;
  const Mirror_SourceNetworkLinType *config = &MIRROR_CONFIG->SourceNetworkLins[network];
/* @SWS_Mirror_00027 */
#ifdef USE_LINIF
  ret = LinIf_EnableBusMirroring(config->Channel, FALSE);
  if (E_OK == ret) {
#endif
    config->context->bStarted = FALSE;
    ASLOG(MIRRORI, ("Source LIN %u stop\n", network));
#ifdef USE_LINIF
  }
#endif
  return ret;
}

static boolean Mirror_ValidateCanFilter(const Mirror_SourceCanFilterType *filter, uint32_t canId) {
  boolean bAcceptIt = FALSE;

  if (MIRROR_SOURCE_CAN_FILTER_RANGE == filter->FilterType) { /* @SWS_Mirror_00024 */
    if ((canId >= filter->U.R.Lower) && (canId <= filter->U.R.Upper)) {
      bAcceptIt = TRUE;
    }
  } else { /* @SWS_Mirror_00022 */
    if ((canId & filter->U.M.Mask) == filter->U.M.Code) {
      bAcceptIt = TRUE;
    }
  }

  return bAcceptIt;
}

static boolean Mirror_ValidateLinFilter(const Mirror_SourceLinFilterType *filter,
                                        Lin_FramePidType pid) {
  boolean bAcceptIt = FALSE;

  if (MIRROR_SOURCE_LIN_FILTER_RANGE == filter->FilterType) { /* @SWS_Mirror_00032 */
    if ((pid >= filter->U.R.Lower) && (pid <= filter->U.R.Upper)) {
      bAcceptIt = TRUE;
    }
  } else { /* @SWS_Mirror_00030 */
    if ((pid & filter->U.M.Mask) == filter->U.M.Code) {
      bAcceptIt = TRUE;
    }
  }

  return bAcceptIt;
}

static Std_ReturnType Mirror_GetNetworkCanStaticFilterState(NetworkHandleType network,
                                                            uint8_t filterId, boolean *isActive) {
  Std_ReturnType ret = E_OK;
  const Mirror_SourceNetworkCanType *config = &MIRROR_CONFIG->SourceNetworkCans[network];
  DET_VALIDATE(filterId < config->NumStaticFilters, 0x23, MIRROR_E_PARAM_POINTER, return E_NOT_OK);
  *isActive = config->CanFiltersStatus[filterId];
  return ret;
}

static void Mirror_UpdateCanMaxActiveFilterId(const Mirror_SourceNetworkCanType *config,
                                              uint8_t filterId, boolean isActive) {
  int16_t i;
  uint8_t MaxActiveFilterId;
  if (TRUE == isActive) {
    if ((filterId + 1u) >= config->context->MaxActiveFilterId) {
      config->context->MaxActiveFilterId = filterId + 1u;
    }
  } else {
    if ((filterId + 1u) >= config->context->MaxActiveFilterId) {
      /* search the max active filter Id */
      MaxActiveFilterId = 0;
      for (i = (int16_t)config->context->MaxActiveFilterId - 2; i >= 0; i--) {
        if (TRUE == config->CanFiltersStatus[i]) {
          MaxActiveFilterId = i + 1;
          break;
        }
      }
      config->context->MaxActiveFilterId = MaxActiveFilterId;
    }
  }
  ASLOG(MIRRORI, ("CAN MaxActiveFilterId = %u\n", config->context->MaxActiveFilterId));
}

static void Mirror_UpdateLinMaxActiveFilterId(const Mirror_SourceNetworkLinType *config,
                                              uint8_t filterId, boolean isActive) {
  int16_t i;
  uint8_t MaxActiveFilterId;
  if (TRUE == isActive) {
    if ((filterId + 1u) >= config->context->MaxActiveFilterId) {
      config->context->MaxActiveFilterId = filterId + 1u;
    }
  } else {
    if ((filterId + 1u) >= config->context->MaxActiveFilterId) {
      /* search the max active filter Id */
      MaxActiveFilterId = 0;
      for (i = (int16_t)config->context->MaxActiveFilterId - 2; i >= 0; i--) {
        if (TRUE == config->LinFiltersStatus[i]) {
          MaxActiveFilterId = i + 1;
          break;
        }
      }
      config->context->MaxActiveFilterId = MaxActiveFilterId;
    }
  }
  ASLOG(MIRRORI, ("LIN MaxActiveFilterId = %u\n", config->context->MaxActiveFilterId));
}

static Std_ReturnType Mirror_SetNetworkCanStaticFilterState(NetworkHandleType network,
                                                            uint8_t filterId, boolean isActive) {
  Std_ReturnType ret = E_OK;
  const Mirror_SourceNetworkCanType *config = &MIRROR_CONFIG->SourceNetworkCans[network];
  DET_VALIDATE(filterId < config->NumStaticFilters, 0x14, MIRROR_E_PARAM_POINTER, return E_NOT_OK);
  config->CanFiltersStatus[filterId] = isActive;
  Mirror_UpdateCanMaxActiveFilterId(config, filterId, isActive);
  return ret;
}

static Std_ReturnType Mirror_GetNetworkLinStaticFilterState(NetworkHandleType network,
                                                            uint8_t filterId, boolean *isActive) {
  Std_ReturnType ret = E_OK;
  const Mirror_SourceNetworkLinType *config = &MIRROR_CONFIG->SourceNetworkLins[network];
  DET_VALIDATE(filterId < config->NumStaticFilters, 0x23, MIRROR_E_PARAM_POINTER, return E_NOT_OK);
  *isActive = config->LinFiltersStatus[filterId];
  return ret;
}

static Std_ReturnType Mirror_SetNetworkLinStaticFilterState(NetworkHandleType network,
                                                            uint8_t filterId, boolean isActive) {
  Std_ReturnType ret = E_OK;
  const Mirror_SourceNetworkLinType *config = &MIRROR_CONFIG->SourceNetworkLins[network];
  DET_VALIDATE(filterId < config->NumStaticFilters, 0x14, MIRROR_E_PARAM_POINTER, return E_NOT_OK);
  config->LinFiltersStatus[filterId] = isActive;
  Mirror_UpdateLinMaxActiveFilterId(config, filterId, isActive);
  return ret;
}

Std_ReturnType Mirror_RemoveCanFilter(NetworkHandleType network, uint8 filterId) {
  Std_ReturnType ret = E_OK;
  const Mirror_SourceNetworkCanType *config = &MIRROR_CONFIG->SourceNetworkCans[network];
  DET_VALIDATE((filterId >= config->NumStaticFilters) &&
                 (filterId < ((uint16_t)config->NumStaticFilters + config->MaxDynamicFilters)),
               0x1a, MIRROR_E_PARAM_POINTER, return E_NOT_OK);
  if (TRUE == config->CanFiltersStatus[filterId]) {
    config->CanFiltersStatus[filterId] = FALSE;
    Mirror_UpdateCanMaxActiveFilterId(config, filterId, FALSE);
  } else {
    ret = E_NOT_OK;
  }
  return ret;
}

Std_ReturnType Mirror_RemoveLinFilter(NetworkHandleType network, uint8 filterId) {
  Std_ReturnType ret = E_OK;
  const Mirror_SourceNetworkLinType *config = &MIRROR_CONFIG->SourceNetworkLins[network];
  DET_VALIDATE((filterId >= config->NumStaticFilters) &&
                 (filterId < ((uint16_t)config->NumStaticFilters + config->MaxDynamicFilters)),
               0x1a, MIRROR_E_PARAM_POINTER, return E_NOT_OK);
  if (TRUE == config->LinFiltersStatus[filterId]) {
    config->LinFiltersStatus[filterId] = FALSE;
    Mirror_UpdateLinMaxActiveFilterId(config, filterId, FALSE);
  } else {
    ret = E_NOT_OK;
  }
  return ret;
}

#ifdef MIRROR_USE_DEST_CAN
static void Mirror_EnqueCanFrame(const Mirror_RingBufferType *RingBuffer, uint8_t NetworkId,
                                 Can_IdType canId, uint8_t length, const uint8_t *payload) {
  uint8_t numPackets = (length + 15) >> 3;
  uint16_t capability;
  Mirror_DataElementType *dataElement;
  uint8_t offset = 0;
  uint8_t doSz;

  Mirror_EnterCritical();
  if (RingBuffer->context->in >= RingBuffer->context->out) {
    capability =
      RingBuffer->NumOfDataElements - (RingBuffer->context->in - RingBuffer->context->out);
  } else {
    capability = RingBuffer->NumOfDataElements -
                 (0xFFFFul - RingBuffer->context->out + 1u + RingBuffer->context->in);
  }
  if (capability > numPackets) {
    dataElement =
      &RingBuffer->DataElements[RingBuffer->context->in % RingBuffer->NumOfDataElements];
    RingBuffer->context->in++;
    dataElement->data[0] = MIRROR_NT_CAN;
    dataElement->data[1] = NetworkId;
    dataElement->data[2] = 0u; /* reserved */
    dataElement->data[3] = length;
    dataElement->data[4] = (canId >> 24) & 0x3Fu;
    if ((canId & 0x3FFFFFFFu) > 0x7FF) { /* @SWS_Mirror_00098 */
      dataElement->data[4] |= 0x80u;     /* Extended CAN ID */
    }
    if (length > 8u) {               /* @SWS_Mirror_00099 */
      dataElement->data[4] |= 0x40u; /* CAN FD Frame */
    }
    dataElement->data[5] = (canId >> 16) & 0xFFu;
    dataElement->data[6] = (canId >> 8) & 0xFFu;
    dataElement->data[7] = canId & 0xFFu;
    while (offset < length) {
      dataElement =
        &RingBuffer->DataElements[RingBuffer->context->in % RingBuffer->NumOfDataElements];
      RingBuffer->context->in++;
      doSz = length - offset;
      if (doSz > 8) {
        doSz = 8;
      }
      (void)memcpy(dataElement->data, &payload[offset], doSz);
      offset += doSz;
    }
    ASLOG(MIRROR, ("in = %u out = %u\n", RingBuffer->context->in, RingBuffer->context->out));
  } else {
    ASLOG(MIRRORE, ("No free space\n"));
  }
  Mirror_ExitCritical();
}

static void Mirror_ReportCanFrameToDestCan(NetworkHandleType network, uint8_t NetworkId,
                                           Can_IdType canId, uint8_t length,
                                           const uint8_t *payload) {
  const Mirror_DestNetworkCanType *config = &MIRROR_CONFIG->DestNetworkCans[network];
  Mirror_EnqueCanFrame(config->RingBuffer, NetworkId, canId, length, payload);
}
#endif

#ifdef MIRROR_USE_DEST_CAN
static void Mirror_EnqueLinFrame(const Mirror_RingBufferType *RingBuffer, uint8_t controllerId,
                                 Lin_FramePidType pid, uint8_t length, const uint8_t *payload,
                                 Lin_StatusType status) {
  uint8_t numPackets;
  uint16_t capability;
  Mirror_DataElementType *dataElement;
  uint8_t offset = 0;
  uint8_t doSz;

  if ((LIN_TX_OK == status) || (LIN_RX_OK == status)) {
    doSz = length;
  } else {
    doSz = 0;
  }

  numPackets = (doSz + 15) >> 3;

  Mirror_EnterCritical();
  if (RingBuffer->context->in >= RingBuffer->context->out) {
    capability =
      RingBuffer->NumOfDataElements - (RingBuffer->context->in - RingBuffer->context->out);
  } else {
    capability = RingBuffer->NumOfDataElements -
                 (0xFFFFul - RingBuffer->context->out + 1u + RingBuffer->context->in);
  }
  if (capability > numPackets) {
    dataElement =
      &RingBuffer->DataElements[RingBuffer->context->in % RingBuffer->NumOfDataElements];
    RingBuffer->context->in++;
    dataElement->data[0] = MIRROR_NT_LIN;
    dataElement->data[1] = controllerId;
    dataElement->data[2] = 0u; /* reserved */
    dataElement->data[3] = length;
    dataElement->data[4] = pid;
    dataElement->data[5] = status;
    if ((LIN_TX_OK == status) || (LIN_RX_OK == status)) {
      while (offset < length) {
        dataElement =
          &RingBuffer->DataElements[RingBuffer->context->in % RingBuffer->NumOfDataElements];
        RingBuffer->context->in++;
        doSz = length - offset;
        if (doSz > 8) {
          doSz = 8;
        }
        (void)memcpy(dataElement->data, &payload[offset], doSz);
        offset += doSz;
      }
    }
    ASLOG(MIRROR, ("in = %u out = %u\n", RingBuffer->context->in, RingBuffer->context->out));
  } else {
    ASLOG(MIRRORE, ("No free space\n"));
  }
  Mirror_ExitCritical();
}

static void Mirror_ReportLinFrameToDestCan(NetworkHandleType network, uint8_t controllerId,
                                           Lin_FramePidType pid, uint8_t length,
                                           const uint8_t *payload, Lin_StatusType status) {
  const Mirror_DestNetworkCanType *config = &MIRROR_CONFIG->DestNetworkCans[network];
  (void)config;
  Mirror_EnqueLinFrame(config->RingBuffer, controllerId, pid, length, payload, status);
}
#endif

static void Mirror_GetHeaderTimestamp(uint8_t *Seconds, uint8_t *Nanoseconds) {

  StbM_TimeTupleType timeTuple;
  StbM_UserDataType userData;

  (void)StbM_GetCurrentTime(0, &timeTuple, &userData);

  Seconds[0] = (timeTuple.globalTime.secondsHi >> 8) & 0xFFu;
  Seconds[1] = timeTuple.globalTime.secondsHi & 0xFFu;
  Seconds[2] = (timeTuple.globalTime.seconds >> 24) & 0xFFu;
  Seconds[3] = (timeTuple.globalTime.seconds >> 16) & 0xFFu;
  Seconds[4] = (timeTuple.globalTime.seconds >> 8) & 0xFFu;
  Seconds[5] = timeTuple.globalTime.seconds & 0xFFu;

  Nanoseconds[0] = (timeTuple.globalTime.nanoseconds >> 24) & 0xFFu;
  Nanoseconds[1] = (timeTuple.globalTime.nanoseconds >> 16) & 0xFFu;
  Nanoseconds[2] = (timeTuple.globalTime.nanoseconds >> 8) & 0xFFu;
  Nanoseconds[3] = timeTuple.globalTime.nanoseconds & 0xFFu;
}

static void Mirror_IpAddCanFrameToDestBuf(const Mirror_DestNetworkIpType *config,
                                          const Mirror_DestBufferType *DestBuffer,
                                          uint8_t NetworkId, Can_IdType canId, uint8_t length,
                                          const uint8_t *payload) {
  boolean bHasState = FALSE;
  uint8_t NetworkState = 0;
  uint16_t offset = *DestBuffer->offset;
  std_time_t timestamp;

  timestamp = Std_GetTimerElapsedTime(&config->context->timer);
  timestamp = timestamp / 10;

  /* timestamp 2 Byte */
  DestBuffer->data[offset + 0u] = (timestamp >> 8) & 0xFFu;
  DestBuffer->data[offset + 1u] = timestamp & 0xFFu;
  DestBuffer->data[offset + 2u] =
    MIRROR_NT_FRAME_ID_AVAIABLE | MIRROR_NT_PAYLOAD_AVAIABLE | MIRROR_NT_CAN;
  DestBuffer->data[offset + 3u] = NetworkId; /* NetworkID */
  if (TRUE == config->context->bFrameLost) {
    DestBuffer->data[offset + 2u] |= MIRROR_NT_STATE_AVAIABLE;
    config->context->bFrameLost = FALSE;
    NetworkState |= MIRROR_NS_FRAME_LOST; /* Frame Lost, SWS_Mirror_00079 */
    bHasState = TRUE;
  }
  offset += 4;
  if (TRUE == bHasState) {
    DestBuffer->data[offset + 0u] = NetworkState;
    offset += 1;
  }
  /* FrameID */
  /* @SWS_Mirror_00097 */
  DestBuffer->data[offset + 0u] = (canId >> 24) & 0x3Fu;
  DestBuffer->data[offset + 1u] = (canId >> 16) & 0xFFu;
  DestBuffer->data[offset + 2u] = (canId >> 8) & 0xFFu;
  DestBuffer->data[offset + 3u] = canId & 0xFFu;
  if ((canId & 0x3FFFFFFFu) > 0x7FF) {      /* @SWS_Mirror_00098 */
    DestBuffer->data[offset + 0u] |= 0x80u; /* Extended CAN ID */
  }
  if (length > 8u) {                        /* @SWS_Mirror_00099 */
    DestBuffer->data[offset + 0u] |= 0x40u; /* CAN FD Frame */
  }
  offset += 4;

  /* PayloadLength */
  DestBuffer->data[offset + 0u] = length;
  offset += 1;

  (void)memcpy(&DestBuffer->data[offset], payload, length);
  offset += length;

  *DestBuffer->offset = offset;
}

static void Mirror_ReportCanFrameToDestIp(NetworkHandleType network, uint8_t NetworkId,
                                          Can_IdType canId, uint8_t length,
                                          const uint8_t *payload) {
  boolean bMoveToNextDestBuf = FALSE;
  const Mirror_DestNetworkIpType *config = &MIRROR_CONFIG->DestNetworkIps[network];
  const Mirror_DestBufferType *DestBuffer;
  uint8_t used;
  do {
    bMoveToNextDestBuf = FALSE;
    Mirror_EnterCritical();
    DestBuffer = &config->DestBuffers[config->context->in % config->NumDestBuffers];
    if (0 == *DestBuffer->offset) { /* start of the dest buffer */
      /* @SWS_Mirror_00055 */
      DestBuffer->data[0] = 1u; /* ProtocolVersion */
      DestBuffer->data[1] = config->context->SequenceNumber;
      Std_TimerStart(&config->context->timer);
      Mirror_GetHeaderTimestamp(&DestBuffer->data[2], &DestBuffer->data[8]);
      *DestBuffer->offset = 14;
      config->context->SequenceNumber++;
      config->context->TxDeadlineTimer = config->MirrorDestTransmissionDeadline;
    }

    if ((*DestBuffer->offset + 10u + length) <= DestBuffer->size) { /* space enough */
      Mirror_IpAddCanFrameToDestBuf(config, DestBuffer, NetworkId, canId, length, payload);
    } else {
      ASLOG(MIRROR, ("DestBuffer %u full\n", config->context->in));
      config->context->in++;
      if (config->context->in >= config->context->out) {
        used = config->context->in - config->context->out;
      } else {
        used = (uint16_t)256u - config->context->out + config->context->in;
      }
      if (used < config->NumDestBuffers) {
        bMoveToNextDestBuf = TRUE;
      } else {
        ASLOG(MIRRORE, ("No Free DestBuffer\n"));
        config->context->bFrameLost = TRUE; /* SWS_Mirror_00113 */
        config->context->in--;              /* rollback */
      }
    }
    Mirror_ExitCritical();
  } while (TRUE == bMoveToNextDestBuf);
}

static void Mirror_IpAddCanStateToDestBuf(const Mirror_DestNetworkIpType *config,
                                          const Mirror_DestBufferType *DestBuffer,
                                          uint8_t NetworkId,
                                          Mirror_CanNetworkStateType NetworkState) {
  uint16_t offset = *DestBuffer->offset;
  std_time_t timestamp;

  timestamp = Std_GetTimerElapsedTime(&config->context->timer);
  timestamp = timestamp / 10;

  /* timestamp 2 Byte */
  DestBuffer->data[offset + 0u] = (timestamp >> 8) & 0xFFu;
  DestBuffer->data[offset + 1u] = timestamp & 0xFFu;
  DestBuffer->data[offset + 2u] = MIRROR_NT_STATE_AVAIABLE | MIRROR_NT_CAN;
  DestBuffer->data[offset + 3u] = NetworkId; /* NetworkID */
  DestBuffer->data[offset + 4u] = NetworkState;
  offset += 5;

  *DestBuffer->offset = offset;
}

static void Mirror_ReportCanStateToDestIp(NetworkHandleType network, uint8_t NetworkId,
                                          Mirror_CanNetworkStateType NetworkState) {
  boolean bMoveToNextDestBuf = FALSE;
  const Mirror_DestNetworkIpType *config = &MIRROR_CONFIG->DestNetworkIps[network];
  const Mirror_DestBufferType *DestBuffer;
  uint8_t used;
  do {
    bMoveToNextDestBuf = FALSE;
    Mirror_EnterCritical();
    DestBuffer = &config->DestBuffers[config->context->in % config->NumDestBuffers];
    if (0 == *DestBuffer->offset) { /* start of the dest buffer */
      /* @SWS_Mirror_00055 */
      DestBuffer->data[0] = 1u; /* ProtocolVersion */
      DestBuffer->data[1] = config->context->SequenceNumber;
      Std_TimerStart(&config->context->timer);
      Mirror_GetHeaderTimestamp(&DestBuffer->data[2], &DestBuffer->data[8]);
      *DestBuffer->offset = 14;
      config->context->SequenceNumber++;
      config->context->TxDeadlineTimer = config->MirrorDestTransmissionDeadline;
    }

    if ((*DestBuffer->offset + 5u) <= DestBuffer->size) { /* space enough */
      Mirror_IpAddCanStateToDestBuf(config, DestBuffer, NetworkId, NetworkState);
    } else {
      ASLOG(MIRROR, ("DestBuffer %u full\n", config->context->in));
      config->context->in++;
      if (config->context->in >= config->context->out) {
        used = config->context->in - config->context->out;
      } else {
        used = (uint16_t)256u - config->context->out + config->context->in;
      }
      if (used < config->NumDestBuffers) {
        bMoveToNextDestBuf = TRUE;
      } else {
        ASLOG(MIRRORE, ("No Free DestBuffer\n"));
        config->context->bFrameLost = TRUE; /* SWS_Mirror_00113 */
        config->context->in--;              /* rollback */
      }
    }
    Mirror_ExitCritical();
  } while (TRUE == bMoveToNextDestBuf);
}

static Lin_FramePidType Mirror_CalcLinPid(uint8_t id) {
  uint8_t pid;
  uint8_t p0;
  uint8_t p1;

  /* calculate the pid for standard LIN id case */
  pid = id & 0x3F;
  p0 = LIN_BIT(pid, 0) ^ LIN_BIT(pid, 1) ^ LIN_BIT(pid, 2) ^ LIN_BIT(pid, 4);
  p1 = ~(LIN_BIT(pid, 1) ^ LIN_BIT(pid, 3) ^ LIN_BIT(pid, 4) ^ LIN_BIT(pid, 5));
  pid = pid | (p0 << 6) | (p1 << 7);

  return pid;
}

static void Mirror_IpAddLinFrameToDestBuf(const Mirror_DestNetworkIpType *config,
                                          const Mirror_DestBufferType *DestBuffer,
                                          uint8_t controllerId, Lin_FramePidType pid,
                                          uint8_t length, const uint8_t *payload,
                                          Lin_StatusType status) {
  uint16_t offset = *DestBuffer->offset;
  boolean bHasState = FALSE;
  boolean bHasPayload = FALSE;
  uint8_t NetworkState = 0;
  std_time_t timestamp;
  timestamp = Std_GetTimerElapsedTime(&config->context->timer);
  timestamp = timestamp / 10;
  /* timestamp 2 Byte */
  DestBuffer->data[offset + 0u] = (timestamp >> 8) & 0xFFu;
  DestBuffer->data[offset + 1u] = timestamp & 0xFFu;
  DestBuffer->data[offset + 2u] = MIRROR_NT_FRAME_ID_AVAIABLE | MIRROR_NT_LIN;
  DestBuffer->data[offset + 3u] = controllerId; /* NetworkID */

  /* @SWS_Mirror_00034 */
  switch (status) {
  case LIN_TX_OK:
    bHasPayload = TRUE;
    break;
  case LIN_TX_HEADER_ERROR:
    NetworkState |= 0x08u; /* Header Tx Error */
    bHasState = TRUE;
    break;
  case LIN_TX_ERROR:
    NetworkState |= 0x04u; /* Tx Error */
    bHasState = TRUE;
    bHasPayload = TRUE;
    break;
  case LIN_RX_OK:
    bHasPayload = TRUE;
    break;
  case LIN_RX_ERROR:
    NetworkState |= 0x02u; /* Rx Error */
    bHasState = TRUE;
    break;
  case LIN_RX_NO_RESPONSE:
    NetworkState |= 0x01u; /* Rx No Response */
    bHasState = TRUE;
    break;
  case LIN_OPERATIONAL:
    NetworkState |= 0x40u; /* Bus Online */
    bHasState = TRUE;
    break;
  case LIN_CH_SLEEP:
    /* Bus Offline */
    bHasState = TRUE;
    break;
  default:
    break;
  }
  if (TRUE == bHasPayload) {
    DestBuffer->data[offset + 2u] |= MIRROR_NT_PAYLOAD_AVAIABLE;
  }
  if (TRUE == config->context->bFrameLost) {
    config->context->bFrameLost = FALSE;
    NetworkState |= MIRROR_NS_FRAME_LOST; /* Frame Lost, SWS_Mirror_00079 */
    bHasState = TRUE;
  }
  if (TRUE == bHasState) {
    DestBuffer->data[offset + 2u] |= MIRROR_NT_STATE_AVAIABLE;
    DestBuffer->data[offset + 4u] = NetworkState;
    offset += 5;
  } else {
    offset += 4;
  }
  /* FrameID */
  /* @SWS_Mirror_00102, @SWS_Mirror_00029 */
  DestBuffer->data[offset + 0u] = Mirror_CalcLinPid(pid);
  offset += 1;

  if (TRUE == bHasPayload) {
    /* PayloadLength */
    DestBuffer->data[offset + 0u] = length;
    offset += 1;
    (void)memcpy(&DestBuffer->data[offset], payload, length);
    offset += length;
  }

  *DestBuffer->offset = offset;
}

static void Mirror_ReportLinFrameToDestIp(NetworkHandleType network, uint8_t controllerId,
                                          Lin_FramePidType pid, uint8_t length,
                                          const uint8_t *payload, Lin_StatusType status) {
  boolean bMoveToNextDestBuf = FALSE;
  const Mirror_DestNetworkIpType *config = &MIRROR_CONFIG->DestNetworkIps[network];
  const Mirror_DestBufferType *DestBuffer;
  uint8_t used;

  do {
    bMoveToNextDestBuf = FALSE;
    Mirror_EnterCritical();
    DestBuffer = &config->DestBuffers[config->context->in % config->NumDestBuffers];
    if (0 == *DestBuffer->offset) { /* start of the dest buffer */
      /* @SWS_Mirror_00055 */
      DestBuffer->data[0] = 1u; /* ProtocolVersion */
      DestBuffer->data[1] = config->context->SequenceNumber;
      Std_TimerStart(&config->context->timer);
      Mirror_GetHeaderTimestamp(&DestBuffer->data[2], &DestBuffer->data[8]);
      *DestBuffer->offset = 14;
      config->context->SequenceNumber++;
      config->context->TxDeadlineTimer = config->MirrorDestTransmissionDeadline;
    }

    if ((*DestBuffer->offset + 7u + length) <= DestBuffer->size) { /* space enough */
      Mirror_IpAddLinFrameToDestBuf(config, DestBuffer, controllerId, pid, length, payload, status);
    } else {
      ASLOG(MIRROR, ("DestBuffer %u full\n", config->context->in));
      config->context->in++;
      if (config->context->in >= config->context->out) {
        used = config->context->in - config->context->out;
      } else {
        used = (uint16_t)256u - config->context->out + config->context->in;
      }
      if (used < config->NumDestBuffers) {
        bMoveToNextDestBuf = TRUE;
      } else {
        ASLOG(MIRRORE, ("No Free DestBuffer\n"));
        config->context->bFrameLost = TRUE; /* SWS_Mirror_00113 */
        config->context->in--;              /* rollback */
      }
    }
    Mirror_ExitCritical();
  } while (TRUE == bMoveToNextDestBuf);
}

#ifdef MIRROR_USE_DEST_CAN
static uint32_t Mirror_GetCanIdFromCanIdMap(uint8_t controllerId, uint8_t *data) {
  uint32_t canid;
  NetworkHandleType network;
  const Mirror_SourceNetworkCanType *config;
  uint16_t i;
  boolean bMapped = FALSE;

  network = MIRROR_CONFIG->CanCtrlIdToNetworkMaps[controllerId];
  config = &MIRROR_CONFIG->SourceNetworkCans[network];
  canid =
    ((uint32_t)data[0] << 24) + ((uint32_t)data[1] << 16) + ((uint32_t)data[2] << 8) + data[3];
  for (i = 0; (i < config->NumSingleIdMappings) && (FALSE == bMapped); i++) {
    if (canid == config->SingleIdMappings[i].SourceCanId) { /* @SWS_Mirror_00114 */
      canid = config->SingleIdMappings[i].DestCanId;
      bMapped = TRUE;
    }
  }

  for (i = 0; (i < config->NumMaskBasedIdMappings) && (FALSE == bMapped); i++) {
    if (config->MaskBasedIdMappings[i].SourceCanIdCode ==
        (canid & config->MaskBasedIdMappings[i].SourceCanIdMask)) { /* @SWS_Mirror_00115 */
      canid = config->MaskBasedIdMappings[i].DestBaseId +
              (canid & config->MaskBasedIdMappings[i].SourceCanIdMask);
      bMapped = TRUE;
    }
  }
  return canid;
}

static uint32_t Mirror_GetCanIdFromLinIdMap(uint8_t controllerId, uint8_t pid) {
  uint32_t canid = pid & 0x3F;
  NetworkHandleType network;
  const Mirror_SourceNetworkLinType *config;
  uint16_t i;
  boolean bMapped = FALSE;

  network = MIRROR_CONFIG->LinCtrlIdToNetworkMaps[controllerId];
  config = &MIRROR_CONFIG->SourceNetworkLins[network];
  for (i = 0; (i < config->NumSourceLinToCanIdMappings) && (FALSE == bMapped); i++) {
    if (canid == config->SourceLinToCanIdMappings[i].LinId) { /* @SWS_Mirror_00117 */
      canid = config->SourceLinToCanIdMappings[i].CanId;
      bMapped = TRUE;
    }
  }

  if (FALSE == bMapped) { /* @SWS_Mirror_00118 */
    canid = config->SourceLinToCanBaseId + (pid & 0x3F);
  }
  return canid;
}

static void Mirror_MainFunctionDestCan(NetworkHandleType network) {
  const Mirror_DestNetworkCanType *config = &MIRROR_CONFIG->DestNetworkCans[network];
  Std_ReturnType ret;
  Mirror_DataElementType *DataElement;
  PduInfoType PduInfo;
  uint32_t canid;
  uint8_t data[64]; /* 64 for CanFd */
  const Mirror_RingBufferType *RingBuffer = config->RingBuffer;
  uint8_t offset = 0;
  uint8_t doSz;
  boolean bStatusFrame = FALSE;
  uint16_t out = RingBuffer->context->out;

  PduInfo.MetaDataPtr = (uint8_t *)&canid; /* @SWS_Mirror_00121 */
  PduInfo.SduDataPtr = data;
  PduInfo.SduLength = 0;

  if (RingBuffer->context->in != out) {
    DataElement = &RingBuffer->DataElements[out % RingBuffer->NumOfDataElements];
    out++;
    if (MIRROR_NT_CAN == DataElement->data[0]) {
      canid = Mirror_GetCanIdFromCanIdMap(DataElement->data[1], &DataElement->data[4]);
      PduInfo.SduLength = DataElement->data[3];

    } else {
      canid = Mirror_GetCanIdFromLinIdMap(DataElement->data[1], DataElement->data[4]);
      if ((LIN_TX_OK == DataElement->data[5]) || (LIN_RX_OK == DataElement->data[5])) {
        PduInfo.SduLength = DataElement->data[3];
      } else if (config->MirrorStatusCanId != MIRROR_STATUS_CANID_INVALID) {
        bStatusFrame = TRUE;
        data[0] = 1;
        data[1] = 0;
        /* @SWS_Mirror_00034 */
        switch (DataElement->data[5]) {
        case LIN_TX_HEADER_ERROR:
          data[1] |= 0x08u; /* Header Tx Error */
          break;
        case LIN_TX_ERROR:
          data[1] |= 0x04u; /* Tx Error */
          break;
        case LIN_RX_ERROR:
          data[1] |= 0x02u; /* Rx Error */
          break;
        case LIN_RX_NO_RESPONSE:
          data[1] |= 0x01u; /* Rx No Response */
          break;
        case LIN_OPERATIONAL:
          data[1] |= 0x40u; /* Bus Online */
          break;
        default:
          break;
        }
        PduInfo.SduLength = 2;
      } else {
        RingBuffer->context->out = out;
        ASLOG(MIRRIR, ("Status Frame not enabled, drop!\n"));
      }
    }
  }

  if ((PduInfo.SduLength > 0) && (FALSE != bStatusFrame)) {
    while (offset < PduInfo.SduLength) {
      doSz = PduInfo.SduLength - offset;
      if (doSz > 8) {
        doSz = 8;
      }
      DataElement = &RingBuffer->DataElements[out % RingBuffer->NumOfDataElements];
      out++;
      (void)memcpy(&data[offset], DataElement->data, doSz);
      offset += doSz;
    }
  }

  if (PduInfo.SduLength > 0) {
    ret = PduR_MirrorTransmit(config->TxPduId, &PduInfo);
    if (E_OK == ret) {
      RingBuffer->context->out = out;
    } else {
      ASLOG(MIRRIR, ("Failed to send CAN packet, retry\n"));
    }
  }
}
#endif /* MIRROR_USE_DEST_CAN */

static void Mirror_MainFunctionDestIp(NetworkHandleType network) {
  const Mirror_DestNetworkIpType *config = &MIRROR_CONFIG->DestNetworkIps[network];
#ifdef USE_SOAD
  Std_ReturnType ret;
#endif
  PduInfoType PduInfo;
  const Mirror_DestBufferType *DestBuffer = NULL;
  boolean bLinkedUp = TcpIp_IsLinkedUp();

  PduInfo.SduLength = 0;

  if ((config->context->TxDeadlineTimer > 0) && (TRUE == bLinkedUp)) {
    config->context->TxDeadlineTimer--;
    if (0 == config->context->TxDeadlineTimer) {
      Mirror_EnterCritical();
      DestBuffer = &config->DestBuffers[config->context->in % config->NumDestBuffers];
      if (*DestBuffer->offset > 0) {
        config->context->in++; /* trigger transmit */
        ASLOG(MIRROR, ("Tx Deadline timeout, tx %u\n", config->context->out));
      }
      Mirror_ExitCritical();
    }
  }

  if ((config->context->in != config->context->out) &&
      (TRUE == bLinkedUp)) { /* @SWS_Mirror_00055 */
    DestBuffer = &config->DestBuffers[config->context->out % config->NumDestBuffers];
    PduInfo.MetaDataPtr = NULL;
    PduInfo.SduDataPtr = DestBuffer->data;
    PduInfo.SduLength = *DestBuffer->offset;
    PduInfo.SduDataPtr[12] = ((PduInfo.SduLength - 14u) >> 8) & 0xFFu;
    PduInfo.SduDataPtr[13] = (PduInfo.SduLength - 14u) & 0xFFu;
  }

  if (PduInfo.SduLength > 0) {
    ASHEXDUMP(MIRROR, ("UDP Tx:"), PduInfo.SduDataPtr, PduInfo.SduLength);
#ifdef USE_SOAD
    ret = SoAd_IfTransmit(config->TxPduId, &PduInfo);
    if (E_OK != ret) {
#if 0
      /* SWS_Mirror_00150 */
      config->context->bFrameLost = TRUE;
      *DestBuffer->offset = 0;
      config->context->out++;
#else
      /* just cancel it according to SWS_Mirror_00150 maybe not good, do re-try next times */
#endif
      ASLOG(MIRRORE, ("Failed to send ip packet\n"));
    } else {
      *DestBuffer->offset = 0;
      config->context->out++;
    }
#endif
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Mirror_Init(const Mirror_ConfigType *configPtr) {
  NetworkHandleType network;
#ifdef MIRROW_USE_PB_CONFIG
  if (NULL != CfgPtr) {
    MIRROR_CONFIG = configPtr;
  } else {
    MIRROR_CONFIG = &Mirror_Config;
  }
#else
  (void)configPtr;
#endif
  for (network = 0u; network < MIRROR_CONFIG->NumOfSourceNetworkCans; network++) {
    Mirror_InitSourceCan(network);
  }
  for (network = 0u; network < MIRROR_CONFIG->NumOfSourceNetworkLins; network++) {
    Mirror_InitSourceLin(network);
  }
  for (network = 0u; network < MIRROR_CONFIG->NumOfDestNetworkCans; network++) {
    Mirror_InitDestCan(network);
  }
  for (network = 0u; network < MIRROR_CONFIG->NumOfDestNetworkIps; network++) {
    Mirror_InitDestIp(network);
  }

  Mirror_Context.ActiveDestNetwork = MIRROR_INVALID_NETWORK;
}

boolean Mirror_IsMirrorActive(void) {
  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x20, MIRROR_E_UNINIT, return FALSE);
  return (Mirror_Context.ActiveDestNetwork != MIRROR_INVALID_NETWORK);
}

void Mirror_Offline(void) {
  Mirror_Init(MIRROR_CONFIG);
}

NetworkHandleType Mirror_GetDestNetwork(void) {
  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x21, MIRROR_E_UNINIT, return MIRROR_INVALID_NETWORK);

  return Mirror_Context.ActiveDestNetwork;
}

Std_ReturnType Mirror_SwitchDestNetwork(NetworkHandleType network) {
  Std_ReturnType ret = E_OK;
#ifdef USE_SOAD
  SoAd_SoConIdType SoConId;
#endif

  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x12, MIRROR_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(network < MIRROR_CONFIG->SizeOfComChannelMaps, 0x12, MIRROR_E_INVALID_NETWORK_ID,
               return E_NOT_OK);
  DET_VALIDATE(0u != (MIRROR_CONFIG->ComChannelMaps[network].NetworkType & MIRROR_NT_DEST), 0x12,
               MIRROR_E_INVALID_NETWORK_ID, return E_NOT_OK);

  if (Mirror_Context.ActiveDestNetwork != network) {
    Mirror_Init(MIRROR_CONFIG);
  }
#ifdef USE_SOAD
  if (MIRROR_CONFIG->ComChannelMaps[network].NetworkType == (MIRROR_NT_ETHERNET | MIRROR_NT_DEST)) {
    ret = SoAd_GetSoConId(
      MIRROR_CONFIG->DestNetworkIps[MIRROR_CONFIG->ComChannelMaps[network].NetworkId].TxPduId,
      &SoConId);
    if (E_OK == ret) {
      ret = SoAd_OpenSoCon(SoConId);
    }
  }
#endif

  if (E_OK == ret) {
    Mirror_Context.ActiveDestNetwork = network;
    ASLOG(MIRRORI, ("Dest %u start\n", network));
  }
  return ret;
}

void Mirror_DeInit(void) {
  Mirror_Init(MIRROR_CONFIG);
}

boolean Mirror_IsSourceNetworkStarted(NetworkHandleType network) {
  Std_ReturnType ret = E_OK;

  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x22, MIRROR_E_UNINIT, return FALSE);

  DET_VALIDATE(network < MIRROR_CONFIG->SizeOfComChannelMaps, 0x22, MIRROR_E_INVALID_NETWORK_ID,
               return FALSE);
  DET_VALIDATE(0u == (MIRROR_CONFIG->ComChannelMaps[network].NetworkType & MIRROR_NT_DEST), 0x22,
               MIRROR_E_INVALID_NETWORK_ID, return E_NOT_OK);
  if (MIRROR_CONFIG->ComChannelMaps[network].NetworkType == MIRROR_NT_CAN) {
    ret = Mirror_IsSourceNetworkCanStarted(MIRROR_CONFIG->ComChannelMaps[network].NetworkId);
  } else {
    ret = Mirror_IsSourceNetworkLinStarted(MIRROR_CONFIG->ComChannelMaps[network].NetworkId);
  }

  return ret;
}

Std_ReturnType Mirror_StartSourceNetwork(NetworkHandleType network) {
  Std_ReturnType ret = E_OK;

  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x10, MIRROR_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(network < MIRROR_CONFIG->SizeOfComChannelMaps, 0x10, MIRROR_E_INVALID_NETWORK_ID,
               return E_NOT_OK);
  DET_VALIDATE(0u == (MIRROR_CONFIG->ComChannelMaps[network].NetworkType & MIRROR_NT_DEST), 0x10,
               MIRROR_E_INVALID_NETWORK_ID, return E_NOT_OK);
  if (MIRROR_CONFIG->ComChannelMaps[network].NetworkType == MIRROR_NT_CAN) {
    ret = Mirror_StartSourceNetworkCan(MIRROR_CONFIG->ComChannelMaps[network].NetworkId);
  } else {
    ret = Mirror_StartSourceNetworkLin(MIRROR_CONFIG->ComChannelMaps[network].NetworkId);
  }

  return ret;
}

Std_ReturnType Mirror_StopSourceNetwork(NetworkHandleType network) {
  Std_ReturnType ret = E_OK;

  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x11, MIRROR_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(network < MIRROR_CONFIG->SizeOfComChannelMaps, 0x11, MIRROR_E_INVALID_NETWORK_ID,
               return E_NOT_OK);
  DET_VALIDATE(0u == (MIRROR_CONFIG->ComChannelMaps[network].NetworkType & MIRROR_NT_DEST), 0x11,
               MIRROR_E_INVALID_NETWORK_ID, return E_NOT_OK);
  if (MIRROR_CONFIG->ComChannelMaps[network].NetworkType == MIRROR_NT_CAN) {
    ret = Mirror_StopSourceNetworkCan(MIRROR_CONFIG->ComChannelMaps[network].NetworkId);
  } else {
    ret = Mirror_StopSourceNetworkLin(MIRROR_CONFIG->ComChannelMaps[network].NetworkId);
  }

  return ret;
}

Mirror_NetworkType Mirror_GetNetworkType(NetworkHandleType network) {
  Mirror_NetworkType NetworkType = MIRROR_NT_INVALID;
  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x24, MIRROR_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(network < MIRROR_CONFIG->SizeOfComChannelMaps, 0x24, MIRROR_E_INVALID_NETWORK_ID,
               return E_NOT_OK);
  NetworkType = MIRROR_CONFIG->ComChannelMaps[network].NetworkType & MIRROR_NT_TYPE_MASK;
  return NetworkType;
}

uint8_t Mirror_GetNetworkId(NetworkHandleType network) {
  uint8_t NetworkId = MIRROR_INVALID_NETWORK;
  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x25, MIRROR_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(network < MIRROR_CONFIG->SizeOfComChannelMaps, 0x25, MIRROR_E_INVALID_NETWORK_ID,
               return E_NOT_OK);
  NetworkId = MIRROR_CONFIG->ComChannelMaps[network].NetworkId;
  return NetworkId;
}

NetworkHandleType Mirror_GetNetworkHandle(Mirror_NetworkType networkType, uint8_t networkId) {
  NetworkHandleType network = MIRROR_INVALID_NETWORK;
  NetworkHandleType i;
  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x26, MIRROR_E_UNINIT, return E_NOT_OK);
  for (i = 0; i < MIRROR_CONFIG->SizeOfComChannelMaps; i++) {
    if ((MIRROR_CONFIG->ComChannelMaps[network].NetworkType == networkType) &&
        (MIRROR_CONFIG->ComChannelMaps[network].NetworkId == networkId)) {
      network = i;
      break;
    }
  }
  return network;
}

void Mirror_ReportCanFrame(uint8_t controllerId, Can_IdType canId, uint8_t length,
                           const uint8_t *payload) {
  NetworkHandleType network;
  uint16_t i;
  boolean bAcceptIt = FALSE;
  const Mirror_SourceNetworkCanType *config;
  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x50, MIRROR_E_UNINIT, return);
  DET_VALIDATE(controllerId < MIRROR_CONFIG->SizeOfCanCtrlIdToNetworkMaps, 0x50,
               MIRROR_E_INVALID_NETWORK_ID, return);
  DET_VALIDATE(NULL != payload, 0x50, MIRROR_E_PARAM_POINTER, return);
  DET_VALIDATE(0 != length, 0x50, MIRROR_E_PARAM_POINTER, return);

  ASLOG(MIRROR, ("CAN%u ID=0x%08X LEN=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X]\n",
                 controllerId, canId, length, payload[0], payload[1], payload[2], payload[3],
                 payload[4], payload[5], payload[6], payload[7]));
  network = MIRROR_CONFIG->CanCtrlIdToNetworkMaps[controllerId];
  config = &MIRROR_CONFIG->SourceNetworkCans[network];

  if (Mirror_Context.ActiveDestNetwork >= MIRROR_CONFIG->SizeOfComChannelMaps) {
    /* no dest is active */
  } else if (TRUE == config->context->bStarted) { /* @SWS_Mirror_00021 */
    if (0u == config->context->MaxActiveFilterId) {
      bAcceptIt = TRUE;
    }
    for (i = 0; (FALSE == bAcceptIt) && (i < config->context->MaxActiveFilterId) &&
                (i < config->NumStaticFilters);
         i++) {
      if (TRUE == config->CanFiltersStatus[i]) {
        bAcceptIt = Mirror_ValidateCanFilter(&config->StaticFilters[i], canId);
      }
    }

    for (i = config->NumStaticFilters;
         (FALSE == bAcceptIt) && (i < config->context->MaxActiveFilterId) &&
         (i < ((uint16_t)config->NumStaticFilters + config->MaxDynamicFilters));
         i++) {
      if (TRUE == config->CanFiltersStatus[i]) {
        bAcceptIt =
          Mirror_ValidateCanFilter(&config->DynamicFilters[i - config->NumStaticFilters], canId);
      }
    }
  } else {
    ASLOG(MIRROR, ("network %u not started.\n", network));
  }

  if (TRUE == bAcceptIt) {
#ifdef MIRROR_USE_DEST_CAN
    if (MIRROR_CONFIG->ComChannelMaps[Mirror_Context.ActiveDestNetwork].NetworkType ==
        (MIRROR_NT_CAN | MIRROR_NT_DEST)) {
      Mirror_ReportCanFrameToDestCan(
        MIRROR_CONFIG->ComChannelMaps[Mirror_Context.ActiveDestNetwork].NetworkId,
        config->NetworkId, canId, length, payload);
    } else
#endif
      if (MIRROR_CONFIG->ComChannelMaps[Mirror_Context.ActiveDestNetwork].NetworkType ==
          (MIRROR_NT_ETHERNET | MIRROR_NT_DEST)) {
      Mirror_ReportCanFrameToDestIp(
        MIRROR_CONFIG->ComChannelMaps[Mirror_Context.ActiveDestNetwork].NetworkId,
        config->NetworkId, canId, length, payload);
    } else {
      /* do nothing */
      ASLOG(MIRROR, ("drop as ActiveDestNetwork=%u\n", Mirror_Context.ActiveDestNetwork));
    }
  } else {
    ASLOG(MIRROR, ("filter it out\n"));
  }
}

void Mirror_ReportCanState(uint8_t controllerId, Mirror_CanNetworkStateType NetworkState) {
  NetworkHandleType network;
  boolean bAcceptIt = FALSE;
  const Mirror_SourceNetworkCanType *config;
  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x55, MIRROR_E_UNINIT, return);
  DET_VALIDATE(controllerId < MIRROR_CONFIG->SizeOfCanCtrlIdToNetworkMaps, 0x55,
               MIRROR_E_INVALID_NETWORK_ID, return);
  DET_VALIDATE(0 != NetworkState, 0x55, MIRROR_E_PARAM_POINTER, return);

  ASLOG(MIRROR, ("CAN%u NetworkState=%02X\n", controllerId, NetworkState));
  network = MIRROR_CONFIG->CanCtrlIdToNetworkMaps[controllerId];
  config = &MIRROR_CONFIG->SourceNetworkCans[network];

  if (Mirror_Context.ActiveDestNetwork >= MIRROR_CONFIG->SizeOfComChannelMaps) {
    /* no dest is active */
  } else if (TRUE == config->context->bStarted) { /* @SWS_Mirror_00021 */
    bAcceptIt = TRUE;
  } else {
    ASLOG(MIRROR, ("network %u not started.\n", network));
  }

  if (TRUE == bAcceptIt) {
#ifdef MIRROR_USE_DEST_CAN
    if (MIRROR_CONFIG->ComChannelMaps[Mirror_Context.ActiveDestNetwork].NetworkType ==
        (MIRROR_NT_CAN | MIRROR_NT_DEST)) {
      /* TODO: */
    } else
#endif
      if (MIRROR_CONFIG->ComChannelMaps[Mirror_Context.ActiveDestNetwork].NetworkType ==
          (MIRROR_NT_ETHERNET | MIRROR_NT_DEST)) {
      Mirror_ReportCanStateToDestIp(
        MIRROR_CONFIG->ComChannelMaps[Mirror_Context.ActiveDestNetwork].NetworkId,
        config->NetworkId, NetworkState);
    } else {
      /* do nothing */
      ASLOG(MIRROR, ("drop as ActiveDestNetwork=%u\n", Mirror_Context.ActiveDestNetwork));
    }
  }
}

void Mirror_ReportLinFrame(NetworkHandleType controllerId, Lin_FramePidType pid,
                           const PduInfoType *pdu, Lin_StatusType status) {
  NetworkHandleType network;
  uint16_t i;
  boolean bAcceptIt = FALSE;
  const Mirror_SourceNetworkLinType *config;
  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x51, MIRROR_E_UNINIT, return);
  DET_VALIDATE(controllerId < MIRROR_CONFIG->SizeOfLinCtrlIdToNetworkMaps, 0x51,
               MIRROR_E_INVALID_NETWORK_ID, return);
  DET_VALIDATE((NULL != pdu) && (NULL != pdu->SduDataPtr), 0x51, MIRROR_E_PARAM_POINTER, return);

  ASLOG(MIRROR,
        ("LIN%u ID=0x%02X LEN=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X] status=%d\n",
         controllerId, pid, pdu->SduLength, pdu->SduDataPtr[0], pdu->SduDataPtr[1],
         pdu->SduDataPtr[2], pdu->SduDataPtr[3], pdu->SduDataPtr[4], pdu->SduDataPtr[5],
         pdu->SduDataPtr[6], pdu->SduDataPtr[7], status));
  network = MIRROR_CONFIG->LinCtrlIdToNetworkMaps[controllerId];
  config = &MIRROR_CONFIG->SourceNetworkLins[network];

  if (Mirror_Context.ActiveDestNetwork >= MIRROR_CONFIG->SizeOfComChannelMaps) {
    /* no dest is active */
  } else if (TRUE == config->context->bStarted) { /* @SWS_Mirror_00021 */
    if (0u == config->context->MaxActiveFilterId) {
      bAcceptIt = TRUE;
    }
    for (i = 0; (FALSE == bAcceptIt) && (i < config->context->MaxActiveFilterId) &&
                (i < config->NumStaticFilters);
         i++) {
      if (TRUE == config->LinFiltersStatus[i]) {
        bAcceptIt = Mirror_ValidateLinFilter(&config->StaticFilters[i], pid);
      }
    }

    for (i = config->NumStaticFilters;
         (FALSE == bAcceptIt) && (i < config->context->MaxActiveFilterId) &&
         (i < ((uint16_t)config->NumStaticFilters + config->MaxDynamicFilters));
         i++) {
      if (TRUE == config->LinFiltersStatus[i]) {
        bAcceptIt =
          Mirror_ValidateLinFilter(&config->DynamicFilters[i - config->NumStaticFilters], pid);
      }
    }
  } else {
    ASLOG(MIRROR, ("network %u not started.\n", network));
  }

  if (TRUE == bAcceptIt) {
#ifdef MIRROR_USE_DEST_CAN
    if (MIRROR_CONFIG->ComChannelMaps[Mirror_Context.ActiveDestNetwork].NetworkType ==
        (MIRROR_NT_CAN | MIRROR_NT_DEST)) {
      Mirror_ReportLinFrameToDestCan(
        MIRROR_CONFIG->ComChannelMaps[Mirror_Context.ActiveDestNetwork].NetworkId, controllerId,
        pid, pdu->SduLength, pdu->SduDataPtr, status);
    } else
#endif
      if (MIRROR_CONFIG->ComChannelMaps[Mirror_Context.ActiveDestNetwork].NetworkType ==
          (MIRROR_NT_ETHERNET | MIRROR_NT_DEST)) {
      Mirror_ReportLinFrameToDestIp(
        MIRROR_CONFIG->ComChannelMaps[Mirror_Context.ActiveDestNetwork].NetworkId, controllerId,
        pid, pdu->SduLength, pdu->SduDataPtr, status);
    } else {
      /* do nothing */
      ASLOG(MIRROR, ("drop as ActiveDestNetwork=%u\n", Mirror_Context.ActiveDestNetwork));
    }
  } else {
    ASLOG(MIRROR, ("filter it out\n"));
  }
}

Std_ReturnType Mirror_GetStaticFilterState(NetworkHandleType network, uint8_t filterId,
                                           boolean *isActive) {
  Std_ReturnType ret = E_OK;
  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x23, MIRROR_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(network < MIRROR_CONFIG->SizeOfComChannelMaps, 0x23, MIRROR_E_INVALID_NETWORK_ID,
               return E_NOT_OK);
  DET_VALIDATE(0u == (MIRROR_CONFIG->ComChannelMaps[network].NetworkType & MIRROR_NT_DEST), 0x23,
               MIRROR_E_INVALID_NETWORK_ID, return E_NOT_OK);
  DET_VALIDATE(NULL != isActive, 0x23, MIRROR_E_PARAM_POINTER, return E_NOT_OK);
  if (MIRROR_NT_CAN == MIRROR_CONFIG->ComChannelMaps[network].NetworkType) {
    ret = Mirror_GetNetworkCanStaticFilterState(MIRROR_CONFIG->ComChannelMaps[network].NetworkId,
                                                filterId, isActive);
  } else {
    ret = Mirror_GetNetworkLinStaticFilterState(MIRROR_CONFIG->ComChannelMaps[network].NetworkId,
                                                filterId, isActive);
  }
  return ret;
}

Std_ReturnType Mirror_SetStaticFilterState(NetworkHandleType network, uint8_t filterId,
                                           boolean isActive) {
  Std_ReturnType ret = E_OK;
  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x14, MIRROR_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(network < MIRROR_CONFIG->SizeOfComChannelMaps, 0x14, MIRROR_E_INVALID_NETWORK_ID,
               return E_NOT_OK);
  DET_VALIDATE(0u == (MIRROR_CONFIG->ComChannelMaps[network].NetworkType & MIRROR_NT_DEST), 0x14,
               MIRROR_E_INVALID_NETWORK_ID, return E_NOT_OK);

  if (MIRROR_NT_CAN == MIRROR_CONFIG->ComChannelMaps[network].NetworkType) {
    ret = Mirror_SetNetworkCanStaticFilterState(MIRROR_CONFIG->ComChannelMaps[network].NetworkId,
                                                filterId, isActive);
  } else {
    ret = Mirror_SetNetworkLinStaticFilterState(MIRROR_CONFIG->ComChannelMaps[network].NetworkId,
                                                filterId, isActive);
  }
  return ret;
}

Std_ReturnType Mirror_AddCanRangeFilter(NetworkHandleType network, uint8_t *filterId,
                                        Can_IdType lowerId, Can_IdType upperId) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i;
  const Mirror_SourceNetworkCanType *config;
  Mirror_SourceCanFilterType *filter;
  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x15, MIRROR_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(NULL != filterId, 0x15, MIRROR_E_PARAM_POINTER, return E_NOT_OK);
  DET_VALIDATE(network < MIRROR_CONFIG->SizeOfComChannelMaps, 0x15, MIRROR_E_INVALID_NETWORK_ID,
               return E_NOT_OK);
  DET_VALIDATE(MIRROR_NT_CAN == MIRROR_CONFIG->ComChannelMaps[network].NetworkType, 0x15,
               MIRROR_E_INVALID_NETWORK_ID, return E_NOT_OK);
  DET_VALIDATE(lowerId <= upperId, 0x15, MIRROR_E_PARAM_POINTER, return E_NOT_OK);
  config = &MIRROR_CONFIG->SourceNetworkCans[MIRROR_CONFIG->ComChannelMaps[network].NetworkId];
  for (i = config->NumStaticFilters;
       i < ((uint16_t)config->NumStaticFilters + config->MaxDynamicFilters); i++) {
    if (FALSE == config->CanFiltersStatus[i]) {
      filter = &config->DynamicFilters[i - config->NumStaticFilters];
      filter->FilterType = MIRROR_SOURCE_CAN_FILTER_RANGE;
      filter->U.R.Lower = lowerId;
      filter->U.R.Upper = upperId;
      if ((i + 1u) >= config->context->MaxActiveFilterId) {
        config->context->MaxActiveFilterId = i + 1u;
      }
      *filterId = i;
      config->CanFiltersStatus[i] = TRUE;
      ASLOG(MIRRORI, ("Can add filter lower=%x upper=%x -> filterId=%u\n", lowerId, upperId, i));
      ret = E_OK;
      break;
    }
  }
  return ret;
}

Std_ReturnType Mirror_AddCanMaskFilter(NetworkHandleType network, uint8_t *filterId, Can_IdType id,
                                       Can_IdType mask) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i;
  const Mirror_SourceNetworkCanType *config;
  Mirror_SourceCanFilterType *filter;
  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x16, MIRROR_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(NULL != filterId, 0x16, MIRROR_E_PARAM_POINTER, return E_NOT_OK);
  DET_VALIDATE(network < MIRROR_CONFIG->SizeOfComChannelMaps, 0x16, MIRROR_E_INVALID_NETWORK_ID,
               return E_NOT_OK);
  DET_VALIDATE(MIRROR_NT_CAN == MIRROR_CONFIG->ComChannelMaps[network].NetworkType, 0x16,
               MIRROR_E_INVALID_NETWORK_ID, return E_NOT_OK);
  config = &MIRROR_CONFIG->SourceNetworkCans[MIRROR_CONFIG->ComChannelMaps[network].NetworkId];
  for (i = config->NumStaticFilters;
       i < ((uint16_t)config->NumStaticFilters + config->MaxDynamicFilters); i++) {
    if (FALSE == config->CanFiltersStatus[i]) {
      filter = &config->DynamicFilters[i - config->NumStaticFilters];
      filter->FilterType = MIRROR_SOURCE_CAN_FILTER_MASK;
      filter->U.M.Code = id;
      filter->U.M.Mask = mask;
      if ((i + 1u) >= config->context->MaxActiveFilterId) {
        config->context->MaxActiveFilterId = i + 1u;
      }
      *filterId = i;
      config->CanFiltersStatus[i] = TRUE;
      ASLOG(MIRRORI, ("Can add filter code=%x mask=%x -> filterId=%u\n", id, mask, i));
      ret = E_OK;
      break;
    }
  }
  return ret;
}

Std_ReturnType Mirror_AddLinRangeFilter(NetworkHandleType network, uint8_t *filterId,
                                        uint8_t lowerId, uint8_t upperId) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i;
  const Mirror_SourceNetworkLinType *config;
  Mirror_SourceLinFilterType *filter;
  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x17, MIRROR_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(NULL != filterId, 0x17, MIRROR_E_PARAM_POINTER, return E_NOT_OK);
  DET_VALIDATE(network < MIRROR_CONFIG->SizeOfComChannelMaps, 0x17, MIRROR_E_INVALID_NETWORK_ID,
               return E_NOT_OK);
  DET_VALIDATE(MIRROR_NT_LIN == MIRROR_CONFIG->ComChannelMaps[network].NetworkType, 0x17,
               MIRROR_E_INVALID_NETWORK_ID, return E_NOT_OK);
  DET_VALIDATE(lowerId <= upperId, 0x17, MIRROR_E_PARAM_POINTER, return E_NOT_OK);
  config = &MIRROR_CONFIG->SourceNetworkLins[MIRROR_CONFIG->ComChannelMaps[network].NetworkId];
  for (i = config->NumStaticFilters;
       i < ((uint16_t)config->NumStaticFilters + config->MaxDynamicFilters); i++) {
    if (FALSE == config->LinFiltersStatus[i]) {
      filter = &config->DynamicFilters[i - config->NumStaticFilters];
      filter->FilterType = MIRROR_SOURCE_LIN_FILTER_RANGE;
      filter->U.R.Lower = lowerId;
      filter->U.R.Upper = upperId;
      if ((i + 1u) >= config->context->MaxActiveFilterId) {
        config->context->MaxActiveFilterId = i + 1u;
      }
      *filterId = i;
      config->LinFiltersStatus[i] = TRUE;
      ASLOG(MIRRORI, ("Lin add filter lower=%x upper=%x -> filterId=%u\n", lowerId, upperId, i));
      ret = E_OK;
      break;
    }
  }
  return ret;
}

Std_ReturnType Mirror_AddLinMaskFilter(NetworkHandleType network, uint8_t *filterId, uint8_t id,
                                       uint8_t mask) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i;
  const Mirror_SourceNetworkLinType *config;
  Mirror_SourceLinFilterType *filter;
  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x18, MIRROR_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(NULL != filterId, 0x18, MIRROR_E_PARAM_POINTER, return E_NOT_OK);
  DET_VALIDATE(network < MIRROR_CONFIG->SizeOfComChannelMaps, 0x18, MIRROR_E_INVALID_NETWORK_ID,
               return E_NOT_OK);
  DET_VALIDATE(MIRROR_NT_LIN == MIRROR_CONFIG->ComChannelMaps[network].NetworkType, 0x18,
               MIRROR_E_INVALID_NETWORK_ID, return E_NOT_OK);
  config = &MIRROR_CONFIG->SourceNetworkLins[MIRROR_CONFIG->ComChannelMaps[network].NetworkId];
  for (i = config->NumStaticFilters;
       i < ((uint16_t)config->NumStaticFilters + config->MaxDynamicFilters); i++) {
    if (FALSE == config->LinFiltersStatus[i]) {
      filter = &config->DynamicFilters[i - config->NumStaticFilters];
      filter->FilterType = MIRROR_SOURCE_LIN_FILTER_MASK;
      filter->U.M.Code = id;
      filter->U.M.Mask = mask;
      if ((i + 1u) >= config->context->MaxActiveFilterId) {
        config->context->MaxActiveFilterId = i + 1u;
      }
      *filterId = i;
      config->LinFiltersStatus[i] = TRUE;
      ASLOG(MIRRORI, ("Lin add filter code=%x mask=%x -> filterId=%u\n", id, mask, i));
      ret = E_OK;
      break;
    }
  }
  return ret;
}

Std_ReturnType Mirror_RemoveFilter(NetworkHandleType network, uint8 filterId) {
  Std_ReturnType ret = E_OK;
  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x1a, MIRROR_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(network < MIRROR_CONFIG->SizeOfComChannelMaps, 0x14, MIRROR_E_INVALID_NETWORK_ID,
               return E_NOT_OK);
  DET_VALIDATE(0u == (MIRROR_CONFIG->ComChannelMaps[network].NetworkType & MIRROR_NT_DEST), 0x14,
               MIRROR_E_INVALID_NETWORK_ID, return E_NOT_OK);
  if (MIRROR_NT_CAN == MIRROR_CONFIG->ComChannelMaps[network].NetworkType) {
    ret = Mirror_RemoveCanFilter(MIRROR_CONFIG->ComChannelMaps[network].NetworkId, filterId);
  } else {
    ret = Mirror_RemoveLinFilter(MIRROR_CONFIG->ComChannelMaps[network].NetworkId, filterId);
  }
  return ret;
}

void Mirror_MainFunction(void) {
  DET_VALIDATE(NULL != MIRROR_CONFIG, 0x04, MIRROR_E_UNINIT, return);
  if (Mirror_Context.ActiveDestNetwork >= MIRROR_CONFIG->SizeOfComChannelMaps) {
    /* do nothing as not active */
  }
#ifdef MIRROR_USE_DEST_CAN
  else if (MIRROR_CONFIG->ComChannelMaps[Mirror_Context.ActiveDestNetwork].NetworkType ==
           (MIRROR_NT_CAN | MIRROR_NT_DEST)) {
    Mirror_MainFunctionDestCan(
      MIRROR_CONFIG->ComChannelMaps[Mirror_Context.ActiveDestNetwork].NetworkId);
  } else
#endif
    if (MIRROR_CONFIG->ComChannelMaps[Mirror_Context.ActiveDestNetwork].NetworkType ==
        (MIRROR_NT_ETHERNET | MIRROR_NT_DEST)) {
    Mirror_MainFunctionDestIp(
      MIRROR_CONFIG->ComChannelMaps[Mirror_Context.ActiveDestNetwork].NetworkId);
  } else {
    /* do nothing */
  }
}

void Mirror_TxConfirmation(PduIdType TxPduId, Std_ReturnType result) {
}

#ifndef USE_PDUR
Std_ReturnType __weak PduR_MirrorTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  /* TODO: implement this if PduR not used */
  return E_OK;
}
#endif
