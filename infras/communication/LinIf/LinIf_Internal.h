/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef LINIF_INTERNAL_H
#define LINIF_INTERNAL_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "LinIf.h"
#include "Std_Timer.h"
#include "LinIf_Cfg.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef LINIF_MAX_DATA_LENGHT
#define LINIF_MAX_DATA_LENGHT 8
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  Lin_FramePidType id;
  Lin_FrameDlType dlc;
  LinIf_FrameTypeType type;
  Lin_FrameCsModelType Cs;
  Lin_FrameResponseType Drc;
  LinIf_NotificationCallbackType callback;
  uint32_t delay; /* delay in us */
} LinIf_ScheduleTableEntryType;

typedef struct {
  const LinIf_ScheduleTableEntryType *entrys;
  uint8_t numOfEntries;
} LinIf_ScheduleTableType;

typedef struct {
  Lin_PduType frame;
  LinIf_ChannelStatusType status;
  const LinIf_ScheduleTableType *scheduleTable;
  LinIf_SchHandleType curSch;
  Std_TimerType timer;
  LinIf_SchHandleType scheduleRequested;
  uint8_t data[LINIF_MAX_DATA_LENGHT];
} LinIf_ChannelContextType;

/* @ECUC_LinIf_00654 */
typedef enum
{
  LINIF_MASTER,
  LINIF_SLAVE
} LinIf_NodeTypeType;

typedef struct {
  LinIf_NodeTypeType nodeType;
  uint32_t timeout; /* us */
  NetworkHandleType linChannel;
} LinIf_ChannelConfigType;

struct LinIf_Config_s {
  const LinIf_ScheduleTableType *scheduleTables;
  uint8_t numOfSchTbls;
  const LinIf_ChannelConfigType *channelConfigs;
  LinIf_ChannelContextType *channelContexts;
  uint8_t numOfChannels;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* LINIF_INTERNAL_H */
