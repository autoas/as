/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef LINIF_INTERNAL_H
#define LINIF_INTERNAL_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "LinIf.h"
#include "LinIf_Cfg.h"
/* ================================ [ MACROS    ] ============================================== */
#define DET_THIS_MODULE_ID MODULE_ID_LINIF
#ifndef LINIF_MAX_DATA_LENGHT
#define LINIF_MAX_DATA_LENGHT 8
#endif

#if !defined(LINIF_SCHED_MODE_POLLING) && !defined(LINIF_SCHED_MODE_INTERRUPT)
#define LINIF_SCHED_MODE_POLLING
#endif

#define LINIF_CHANNEL_UNINIT ((LinIf_ChannelStateType)0)
#define LINIF_CHANNEL_OPERATIONAL ((LinIf_ChannelStateType)1)
#define LINIF_CHANNEL_SLEEP ((LinIf_ChannelStateType)2)
#define LINIF_CHANNEL_SLEEP_PENDING ((LinIf_ChannelStateType)3)
#define LINIF_CHANNEL_SLEEP_COMMAND ((LinIf_ChannelStateType)4)

#define LINIF_MASTER ((LinIf_NodeTypeType)0)
#define LINIF_SLAVE ((LinIf_NodeTypeType)1)
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  Lin_FramePidType id;
  Lin_FrameDlType dlc;
  LinIf_FrameTypeType type;
  Lin_FrameCsModelType Cs;
  Lin_FrameResponseType Drc;
  LinIf_NotificationCallbackType callback;
  uint16_t delay;
} LinIf_ScheduleTableEntryType;

typedef struct {
  const LinIf_ScheduleTableEntryType *entrys;
  uint8_t numOfEntries;
} LinIf_ScheduleTableType;

typedef uint8_t LinIf_ChannelStateType; /* @SWS_LinIf_00290 */

typedef uint8_t LinIf_ChannelStatusType;

typedef struct {
#if (LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER
  const LinIf_ScheduleTableType *scheduleTable;
#endif
  Lin_PduType frame;
  uint16_t timer; /* timer in ms */
  LinIf_ChannelStateType state;
  LinIf_ChannelStatusType status;
  LinIf_SchHandleType curSch;
#if (LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER
  LinIf_SchHandleType scheduleRequested;
#endif
  uint8_t data[LINIF_MAX_DATA_LENGHT];
#ifdef USE_MIRROR
  boolean bMirroringActive;
#endif
} LinIf_ChannelContextType;

/* @ECUC_LinIf_00654 */
typedef uint8_t LinIf_NodeTypeType;

typedef struct {
#if (LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE
  const LinIf_ScheduleTableType *scheduleTable;
#endif
  uint16_t timeout;
  NetworkHandleType linChannel;
#if LINIF_VARIANT == LINIF_VARIANT_BOTH
  LinIf_NodeTypeType nodeType;
#endif
} LinIf_ChannelConfigType;

struct LinIf_Config_s {
#if (LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER
  const LinIf_ScheduleTableType *scheduleTables;
  uint8_t numOfSchTbls;
#endif
  const LinIf_ChannelConfigType *channelConfigs;
  LinIf_ChannelContextType *channelContexts;
  uint8_t numOfChannels;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* LINIF_PRIV_H */
