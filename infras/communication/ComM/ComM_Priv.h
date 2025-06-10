/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef _COMM_PRIV_H
#define _COMM_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "Com.h"
/* ================================ [ MACROS    ] ============================================== */
#define DET_THIS_MODULE_ID MODULE_ID_COMM

#define COMM_BUS_TYPE_CAN ((ComM_BusTypeType)0)
#define COMM_BUS_TYPE_CDD ((ComM_BusTypeType)1)
#define COMM_BUS_TYPE_ETH ((ComM_BusTypeType)2)
#define COMM_BUS_TYPE_FR ((ComM_BusTypeType)3)
#define COMM_BUS_TYPE_CINTERNAL ((ComM_BusTypeType)4)
#define COMM_BUS_TYPE_LIN ((ComM_BusTypeType)5)

#define COMM_NM_VARIANT_FULL ((ComM_NmVariantType)0)
#define COMM_NM_VARIANT_LIGHT ((ComM_NmVariantType)1)
#define COMM_NM_VARIANT_NONE ((ComM_NmVariantType)2)
#define COMM_NM_VARIANT_PASSIVE ((ComM_NmVariantType)3)

#define COMM_NM_FLAG_NETWORK_START ((ComM_NmFlagType)0x01)
#define COMM_NM_FLAG_NETWORK_MODE ((ComM_NmFlagType)0x02)
#define COMM_NM_FLAG_PREPARE_BUS_SLEEP ((ComM_NmFlagType)0x04)
#define COMM_NM_FLAG_BUS_SLEEP ((ComM_NmFlagType)0x04)
/* ================================ [ TYPES     ] ============================================== */
/* @ECUC_ComM_00567 */
typedef uint8_t ComM_BusTypeType;

typedef struct {
  ComM_ModeType requestMode;
} ComM_UserContextType;

typedef uint8_t ComM_NmFlagType;

typedef struct {
#ifdef COMM_USE_VARIANT_LIGHT
  uint16_t NmLightTimer;
#endif
  ComM_ModeType requestMode;
  ComM_StateType state;
  ComM_NmFlagType nmFlag;
  boolean bCommunicationAllowed;
} ComM_ChannelContextType;

/* @ECUC_ComM_00568 */
typedef uint8_t ComM_NmVariantType;

/* @ECUC_ComM_00565 */
typedef struct {
  const uint8_t *users;
  const Com_IpduGroupIdType* ComIpduGroupIds;
#ifdef COMM_USE_VARIANT_LIGHT
  uint16_t NmLightTimeout;
#endif
  NetworkHandleType smHandle;
  ComM_BusTypeType busType;
  uint8_t numOfUsers;
  uint8_t numOfComIpduGroups;
  ComM_NmVariantType NmVariant;
  NetworkHandleType nmHandle;
} ComM_ChannelConfigType;

/* @ECUC_ComM_00653 */
typedef struct {
  const uint8_t *channels; /* A channel can be hold by multiple users */
  uint8_t numOfChannels;
} ComM_UserConfigType;

struct ComM_Config_s {
  const ComM_ChannelConfigType *channelConfigs;
  ComM_ChannelContextType *channelContexts;
  const ComM_UserConfigType *userConfigs;
  ComM_UserContextType *userContexts;
  uint8_t numOfChannels;
  uint8_t numOfUsers;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _COMM_PRIV_H */
