/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Generated at Sat Nov 27 10:33:30 2021
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Sd.h"
#include "Sd_Cfg.h"
#include "Sd_Priv.h"
#include "SoAd_Cfg.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
boolean Sd_ServerService0_CRMC(PduIdType pduID, uint8_t type, uint16_t serviceID,
                               uint16_t instanceID, uint8_t majorVersion, uint32_t minorVersion,
                               const Sd_ConfigOptionStringType *receivedConfigOptionPtrArray,
                               const Sd_ConfigOptionStringType *configuredConfigOptionPtrArray);
/* ================================ [ DATAS     ] ============================================== */
static Sd_ServerTimerType Sd_ServerTimerDefault = {
  SD_CONVERT_MS_TO_MAIN_CYCLES(100),  /* InitialOfferDelayMax */
  SD_CONVERT_MS_TO_MAIN_CYCLES(10),   /* InitialOfferDelayMin */
  SD_CONVERT_MS_TO_MAIN_CYCLES(200),  /* InitialOfferRepetitionBaseDelay */
  3,                                  /* InitialOfferRepetitionsMax */
  SD_CONVERT_MS_TO_MAIN_CYCLES(3000), /* OfferCyclicDelay */
  SD_CONVERT_MS_TO_MAIN_CYCLES(1500), /* RequestResponseMaxDelay */
  SD_CONVERT_MS_TO_MAIN_CYCLES(0),    /* RequestResponseMinDelay */
  DEFAULT_TTL,
};

static Sd_ClientTimerType Sd_ClientTimerDefault = {
  SD_CONVERT_MS_TO_MAIN_CYCLES(100),  /* InitialFindDelayMax */
  SD_CONVERT_MS_TO_MAIN_CYCLES(10),   /* InitialFindDelayMin */
  SD_CONVERT_MS_TO_MAIN_CYCLES(200),  /* InitialFindRepetitionsBaseDelay */
  3,                                  /* InitialFindRepetitionsMax */
  SD_CONVERT_MS_TO_MAIN_CYCLES(1500), /* RequestResponseMaxDelay */
  SD_CONVERT_MS_TO_MAIN_CYCLES(0),    /* RequestResponseMinDelay */
  DEFAULT_TTL,
};

static Sd_EventHandlerContextType Sd_EventHandlerContext_server0[1];
static Sd_EventHandlerSubscriberType Sd_EventHandlerSubscriber_server0_event_group1[3];
static const Sd_EventHandlerType Sd_EventHandlers_server0[] = {
  {
    SD_EVENT_HANDLER_SERVER0_EVENT_GROUP1, /* HandleId */
    0x8001, /* EventGroupId */
    0, /* MulticastThreshold */
    &Sd_EventHandlerContext_server0[0],
    Sd_EventHandlerSubscriber_server0_event_group1,
    ARRAY_SIZE(Sd_EventHandlerSubscriber_server0_event_group1),
  },
};

static Sd_ConsumedEventGroupContextType Sd_ConsumedEventGroupContext_client0[1];
static const Sd_ConsumedEventGroupType Sd_ConsumedEventGroups_client0[] = {
  {
    FALSE, /* AutoRequire */
    SD_CONSUMED_EVENT_GROUP_CLIENT0_EVENT_GROUP2, /* HandleId */
    0x8002, /* EventGroupId */
    &Sd_ConsumedEventGroupContext_client0[0],
  },
};

static Sd_ServerServiceContextType Sd_ServerService_Contexts[1];

static const Sd_ServerServiceType Sd_ServerServices[] = {
  {
    FALSE,                           /* AutoAvailable */
    SD_SERVER_SERVICE_HANDLE_ID_SERVER0,  /* HandleId */
    0x1234,                         /* ServiceId */
    0x5678,                         /* InstanceId */
    0,                              /* MajorVersion */
    0,                              /* MinorVersion */
    SOAD_SOCKID_SOMEIP_SERVER0,     /* SoConId */
    TCPIP_IPPROTO_UDP,              /* ProtocolType */
    Sd_ServerService0_CRMC,         /* CapabilityRecordMatchCalloutRef */
    &Sd_ServerTimerDefault,
    &Sd_ServerService_Contexts[0],
    0, /* InstanceIndex */
    Sd_EventHandlers_server0,
    ARRAY_SIZE(Sd_EventHandlers_server0),
  },
};

static Sd_ClientServiceContextType Sd_ClientService_Contexts[1];

static const Sd_ClientServiceType Sd_ClientServices[] = {
  {
    FALSE,                           /* AutoRequire */
    SD_CLIENT_SERVICE_HANDLE_ID_CLIENT0,  /* HandleId */
    0xabcd,                         /* ServiceId */
    0xbeef,                         /* InstanceId */
    0,                              /* MajorVersion */
    0,                              /* MinorVersion */
    SOAD_SOCKID_SOMEIP_CLIENT0, /* SoConId */
    TCPIP_IPPROTO_UDP,              /* ProtocolType */
    NULL,                           /* CapabilityRecordMatchCalloutRef */
    &Sd_ClientTimerDefault,
    &Sd_ClientService_Contexts[0],
    0, /* InstanceIndex */
    Sd_ConsumedEventGroups_client0,
    ARRAY_SIZE(Sd_ConsumedEventGroups_client0),
  },
};

static uint8_t sd_buffer[1400];
static Sd_InstanceContextType sd_context;
static const Sd_InstanceType Sd_Instances[] = {
  {
    "ssas",                             /* Hostname */
    SD_CONVERT_MS_TO_MAIN_CYCLES(1000), /* SubscribeEventgroupRetryDelay */
    3,                                  /* SubscribeEventgroupRetryMax */
    {
      SD_RX_PID_MULTICAST,      /* RxPduId */
      SOAD_SOCKID_SD_MULTICAST, /* SoConId */
    },                          /* MulticastRxPdu */
    {
      SD_RX_PID_UNICAST,      /* RxPduId */
      SOAD_SOCKID_SD_UNICAST, /* SoConId */
    },                        /* UnicastRxPdu */
    {
      SOAD_TX_PID_SD_MULTICAST,    /* MulticastTxPduId */
      SOAD_TX_PID_SD_UNICAST,      /* UnicastTxPduId */
    },                             /* TxPdu */
    Sd_ServerServices,             /* ServerServices */
    ARRAY_SIZE(Sd_ServerServices), /* numOfServerServices */
    Sd_ClientServices,             /* ClientServices */
    ARRAY_SIZE(Sd_ClientServices), /* numOfClientServices */
    sd_buffer,                     /*buffer */
    sizeof(sd_buffer),
    &sd_context,
  },
};

static const Sd_ServerServiceType* Sd_ServerServicesMap[] = {
  &Sd_ServerServices[0],
};

static const Sd_ClientServiceType* Sd_ClientServicesMap[] = {
  &Sd_ClientServices[0],
};

static const uint16_t Sd_EventHandlersMap[] = {
  SD_SERVER_SERVICE_HANDLE_ID_SERVER0,
  -1,
};

static const uint16_t Sd_PerServiceEventHandlerMap[] = {
  0,
  -1,
};

static const uint16_t Sd_ConsumedEventGroupsMap[] = {
  SD_CLIENT_SERVICE_HANDLE_ID_CLIENT0,
  -1,
};

static const uint16_t Sd_PerServiceConsumedEventGroupsMap[] = {
  0,
  -1,
};

const Sd_ConfigType Sd_Config = {
  Sd_Instances,
  ARRAY_SIZE(Sd_Instances),
  Sd_ServerServicesMap,
  ARRAY_SIZE(Sd_ServerServicesMap),
  Sd_ClientServicesMap,
  ARRAY_SIZE(Sd_ClientServicesMap),
  Sd_EventHandlersMap,
  Sd_PerServiceEventHandlerMap,
  ARRAY_SIZE(Sd_EventHandlersMap)-1,
  Sd_ConsumedEventGroupsMap,
  Sd_PerServiceConsumedEventGroupsMap,
  ARRAY_SIZE(Sd_ConsumedEventGroupsMap)-1,
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
