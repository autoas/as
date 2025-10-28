/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 */
#ifndef MIRROR_PRIV_H
#define MIRROR_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "Std_Timer.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef DET_THIS_MODULE_ID
#define DET_THIS_MODULE_ID MODULE_ID_MIRROR
#endif

#define MIRROR_STATUS_CANID_INVALID 0xFFFFFFFFul

#define MIRROR_SOURCE_CAN_FILTER_RANGE ((Mirror_SourceCanFilterTypeType)0)
#define MIRROR_SOURCE_CAN_FILTER_MASK ((Mirror_SourceCanFilterTypeType)1)

#define MIRROR_SOURCE_LIN_FILTER_RANGE ((Mirror_SourceLinFilterTypeType)0)
#define MIRROR_SOURCE_LIN_FILTER_MASK ((Mirror_SourceLinFilterTypeType)1)

#define MIRROR_NT_DEST ((Mirror_NetworkType)0x80)

#define MIRROR_NT_TYPE_MASK ((Mirror_NetworkType)0x0F)
/* ================================ [ TYPES     ] ============================================== */
typedef struct { /* @ECUC_Mirror_00018 */
  uint32_t Lower;
  uint32_t Upper;
} Mirror_SourceCanFilterRangeType;

typedef struct { /* @ECUC_Mirror_00019 */
  uint32_t Code;
  uint32_t Mask;
} Mirror_SourceCanFilterMaskType;

typedef uint8_t Mirror_SourceCanFilterTypeType;

typedef struct { /* @ECUC_Mirror_00031 */
  uint8_t Lower;
  uint8_t Upper;
} Mirror_SourceLinFilterRangeType;

typedef struct { /* @ECUC_Mirror_00035 */
  uint8_t Code;
  uint8_t Mask;
} Mirror_SourceLinFilterMaskType;

typedef uint8_t Mirror_SourceLinFilterTypeType;

typedef struct { /* ECUC_Mirror_00025 */
  uint32_t DestBaseId;
  uint32_t SourceCanIdCode;
  uint32_t SourceCanIdMask;
} Mirror_SourceCanMaskBasedIdMappingType;

typedef struct { /* @ECUC_Mirror_00022 */
  uint32_t SourceCanId;
  uint32_t DestCanId;
} Mirror_SourceCanSingleIdMappingType;

typedef struct { /* @ECUC_Mirror_00038 */
  uint32_t CanId;
  uint8_t LinId;
} Mirror_SourceLinToCanIdMappingType;

typedef struct {
  union {
    Mirror_SourceCanFilterRangeType R;
    Mirror_SourceCanFilterMaskType M;
  } U;
  Mirror_SourceCanFilterTypeType FilterType;
} Mirror_SourceCanFilterType;

typedef struct {
  union {
    Mirror_SourceLinFilterRangeType R;
    Mirror_SourceLinFilterMaskType M;
  } U;
  Mirror_SourceLinFilterTypeType FilterType;
} Mirror_SourceLinFilterType;

typedef struct {
  uint8_t MaxActiveFilterId; /* The max active filter Id + 1 */
  boolean bStarted;
} Mirror_SourceNetworkCanContextType;

typedef struct { /* @ECUC_Mirror_00010 */
  const Mirror_SourceCanFilterType *StaticFilters;
  Mirror_SourceCanFilterType *DynamicFilters;
  Mirror_SourceNetworkCanContextType *context;
  const Mirror_SourceCanMaskBasedIdMappingType *MaskBasedIdMappings;
  const Mirror_SourceCanSingleIdMappingType *SingleIdMappings;
  boolean *CanFiltersStatus; /* size = MaxDynamicFilters + NumStaticFilters */
  uint16_t NumMaskBasedIdMappings;
  uint16_t NumSingleIdMappings;
  uint8_t NumStaticFilters;
  uint8_t MaxDynamicFilters;
  uint8_t ControllerId;
} Mirror_SourceNetworkCanType;

typedef struct {
  uint8_t MaxActiveFilterId;
  boolean bStarted;
} Mirror_SourceNetworkLinContextType;

typedef struct { /* @ECUC_Mirror_00029 */
  const Mirror_SourceLinFilterType *StaticFilters;
  Mirror_SourceLinFilterType *DynamicFilters;
  Mirror_SourceNetworkLinContextType *context;
  const Mirror_SourceLinToCanIdMappingType *SourceLinToCanIdMappings;
  boolean *LinFiltersStatus; /* size = MaxDynamicFilters + NumStaticFilters */
  uint32_t SourceLinToCanBaseId;
  uint16_t NumSourceLinToCanIdMappings;
  uint8_t NumStaticFilters;
  uint8_t MaxDynamicFilters;
  NetworkHandleType Channel;
} Mirror_SourceNetworkLinType;

typedef struct {              /* @ECUC_Mirror_00055 */
  PduIdType MirrorDestPduId;  /* I-PDU identifier used for TxConfirmation from PduR */
  PduIdType MirrorDestPduRef; /* Reference to the Pdu object representing the I-PDU. */
  boolean UsesTriggerTransmit;
} MirrorDestPduType;

typedef struct {
  uint8_t SequenceNumber;
} Mirror_DestNetworkCanContextType;

/**
 * @brief General Ring Buffer Element for queuing CAN or LIN frames.
 *
 * Structure of the buffer elements:
 * - The **first** DataElement (pointed to by `in`) acts as the **header**:
 *   - **Byte 0**: FrameType (indicates CAN/LIN frame type)
 *   - **Byte 1**: CAN/LIN source network ID.
 *   - **Bytes 2-3**: Actual data length (in bytes).
 *   - **Bytes 4-5**: CAN/LIN frame ID.
 * - **Consecutive** DataElements (after `in`) contain raw payload data.
 *
 * @note The buffer follows a ring (circular) structure for efficient FIFO operations.
 */
typedef struct {
  uint8_t data[8];
} Mirror_DataElementType;

typedef struct {
  uint16_t in;
  uint16_t out;
} Mirror_RingContextType;

typedef struct {
  Mirror_RingContextType *context;
  Mirror_DataElementType *DataElements; /* size must be power of 2 */
  uint16_t NumOfDataElements;
} Mirror_RingBufferType;

typedef struct { /* @ECUC_Mirror_00052 */
  Mirror_DestNetworkCanContextType *context;
  const Mirror_RingBufferType *RingBuffer;
  uint32_t MirrorStatusCanId;
  PduIdType TxPduId;
} Mirror_DestNetworkCanType;

typedef struct {
  Std_TimerType timer;
  uint16_t TxDeadlineTimer;
  uint8_t in;
  uint8_t out;
  uint8_t SequenceNumber;
  boolean bFrameLost;
} Mirror_DestNetworkIpContextType;

typedef struct {
  uint8_t *data;
  uint16_t *offset;
  uint16_t size;
} Mirror_DestBufferType;

typedef struct { /* @ECUC_Mirror_00060 */
  Mirror_DestNetworkIpContextType *context;
  const Mirror_DestBufferType *DestBuffers;
  uint16_t MirrorDestTransmissionDeadline; /* maximum 655 ms */
  PduIdType TxPduId;
  uint8_t NumDestBuffers; /* must be power of 2 */
} Mirror_DestNetworkIpType;

typedef struct {
  NetworkHandleType NetworkId;
  Mirror_NetworkType NetworkType;
} Mirror_ComMChannelMapType;

struct Mirror_Config_s {
  const Mirror_SourceNetworkCanType *SourceNetworkCans;
  const Mirror_SourceNetworkLinType *SourceNetworkLins;
  const Mirror_DestNetworkCanType *DestNetworkCans;
  const Mirror_DestNetworkIpType *DestNetworkIps;
  const NetworkHandleType *CanCtrlIdToNetworkMaps;
  const NetworkHandleType *LinCtrlIdToNetworkMaps;
  const Mirror_ComMChannelMapType *ComChannelMaps;
  uint8_t NumOfSourceNetworkCans;
  uint8_t NumOfSourceNetworkLins;
  uint8_t NumOfDestNetworkCans;
  uint8_t NumOfDestNetworkIps;
  uint8_t SizeOfCanCtrlIdToNetworkMaps;
  uint8_t SizeOfLinCtrlIdToNetworkMaps;
  uint8_t SizeOfComChannelMaps;
};

typedef struct {
  NetworkHandleType ActiveDestNetwork;
} Mirror_ContextType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* MIRROR_PRIV_H */
