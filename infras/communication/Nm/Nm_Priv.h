/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef _NM_PRIV_H
#define _NM_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#define DET_THIS_MODULE_ID MODULE_ID_NM

#define NM_BUSNM_CANNM ((Nm_StandardBusTypeType)0)
#define NM_BUSNM_FRNM ((Nm_StandardBusTypeType)1)
#define NM_BUSNM_J1939NM ((Nm_StandardBusTypeType)2)
#define NM_BUSNM_LOCALNM ((Nm_StandardBusTypeType)3)
#define NM_BUSNM_UDPNM ((Nm_StandardBusTypeType)4)
#define NM_BUSNM_OSEKNM ((Nm_StandardBusTypeType)5)
/* ================================ [ TYPES     ] ============================================== */
/* @ECUC_Nm_00220 */
typedef uint8_t Nm_StandardBusTypeType;

typedef struct {
  Nm_StandardBusTypeType busType;
  NetworkHandleType handle;
  NetworkHandleType ComMHandle;
} Nm_ChannelConfigType;

struct Nm_Config_s {
  const Nm_ChannelConfigType *channelConfigs;
  uint8_t numOfChannels;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _NM_PRIV_H */
