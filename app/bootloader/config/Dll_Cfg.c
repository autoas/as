/**
 * DLL Config - Data Link Layer Configuration of LVDS project
 * Copyright (C) 2021  Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dll.h"
#include "Dll_Priv.h"
#include "LinTp.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef LINTP_LL_DL
#define LINTP_LL_DL 8
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern DLL_ResultType Net_TpReadIndication(uint8_t channel, DLL_FrameType *frame,
                                           DLL_ResultType notifyResult);
extern DLL_ResultType Net_TpWriteIndication(uint8_t channel, DLL_FrameType *frame,
                                            DLL_ResultType notifyResult);
/* ================================ [ DATAS     ] ============================================== */
#ifdef DLL_USE_RINGBUFFER
RB_DECLARE(dll0, char, DLL_MAX_DATA_LENGHT);
#endif
static const DLL_ScheduleTableEntryType entrys[] = {
  {0x3C, LINTP_LL_DL, DLL_FRAME_READ, Net_TpReadIndication},
  {0x3D, LINTP_LL_DL, DLL_FRAME_WRITE, Net_TpWriteIndication},
};

static const DLL_ScheduleTableType scheduleTables[] = {
  {entrys, sizeof(entrys) / sizeof(DLL_ScheduleTableEntryType)},
};

static const DLL_ChannelConfigType channelConfigs[] = {
  {
    500000,
    DLL_TIMIEOUT_US,
    0xA5,
    0xC3,
    0,
#ifdef DLL_USE_RINGBUFFER
    &rb_dll0,
#endif
  },
};

static DLL_ChannelContextType
  channelContexts[sizeof(channelConfigs) / sizeof(DLL_ChannelConfigType)];

const DLL_ConfigType DLL_Config = {
  scheduleTables,  sizeof(scheduleTables) / sizeof(DLL_ScheduleTableType), channelConfigs,
  channelContexts, sizeof(channelConfigs) / sizeof(DLL_ChannelConfigType),
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
DLL_ResultType Net_TpReadIndication(uint8_t channel, DLL_FrameType *frame,
                                    DLL_ResultType notifyResult) {
  DLL_ResultType r = DLL_R_NOT_OK;
  PduInfoType pduInfo;

  if (DLL_R_RECEIVED_OK == notifyResult) {
    if (0x3C == frame->H.id) {
      pduInfo.SduDataPtr = frame->D.data;
      /* @TM_Protocol.0103(3) */
      pduInfo.SduLength = frame->H.length;
      if (LINTP_LL_DL == pduInfo.SduLength) {
        LinTp_RxIndication(0, &pduInfo);
        r = DLL_R_OK;
      }
    }
  }

  return r;
}
DLL_ResultType Net_TpWriteIndication(uint8_t channel, DLL_FrameType *frame,
                                     DLL_ResultType notifyResult) {
  DLL_ResultType r = DLL_R_NOT_OK;
  Std_ReturnType ret;
  PduInfoType pduInfo;

  if (DLL_R_TRIGGER_TRANSMIT == notifyResult) {
    if (0x3D == frame->H.id) {
      pduInfo.SduDataPtr = frame->D.data;
      /* TM_Protocol.0104(3) */
      pduInfo.SduLength = frame->H.length;
      if (LINTP_LL_DL == pduInfo.SduLength) {
        ret = LinTp_TriggerTransmit(0, &pduInfo);
        if (E_OK == ret) {
          r = DLL_R_OK;
        }
      }
    }
  }
  return r;
}
