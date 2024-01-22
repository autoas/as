/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Service Discovery AUTOSAR CP Release 4.4.0
 */
#ifndef _SD_PRIV_H
#define _SD_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "SoAd.h"
#include "Sd.h"
/* ================================ [ MACROS    ] ============================================== */
#define DEFAULT_TTL 0xFFFFFF /* @SWS_SD_00514, until next reboot */

#define SD_ANY_MAJOR_VERSION 0xFF
#define SD_ANY_MINOR_VERSION 0xFFFFFFFF
/* ================================ [ TYPES     ] ============================================== */
typedef enum
{
  SD_PHASE_DOWN,
  SD_PHASE_INITIAL_WAIT,
  SD_PHASE_REPETITION,
  SD_PHASE_MAIN,
} Sd_PhaseType;

/* @ECUC_SD_00043*/
typedef struct {
  uint16_t InitialFindDelayMax;
  uint16_t InitialFindDelayMin;
  uint16_t InitialFindRepetitionsBaseDelay;
  uint8_t InitialFindRepetitionsMax;
  uint16_t RequestResponseMaxDelay;
  uint16_t RequestResponseMinDelay;
  uint32_t TTL; /* Time to live for find and subscribe messages */
} Sd_ClientTimerType;

/* @ECUC_SD_00035 */
typedef struct {
  uint16_t InitialOfferDelayMax;
  uint16_t InitialOfferDelayMin;
  uint16_t InitialOfferRepetitionBaseDelay;
  uint8_t InitialOfferRepetitionsMax;
  uint16_t OfferCyclicDelay;
  uint16_t RequestResponseMaxDelay;
  uint16_t RequestResponseMinDelay;
  uint32_t TTL; /* Time to live for offer service */
} Sd_ServerTimerType;

/* @SWS_SD_91001 */
typedef boolean (*Sd_CapabilityRecordMatchCalloutFncType)(
  PduIdType pduID, uint8_t type, uint16_t serviceID, uint16_t instanceID, uint8_t majorVersion,
  uint32_t minorVersion, const Sd_ConfigOptionStringType *receivedConfigOptionPtrArray,
  const Sd_ConfigOptionStringType *configuredConfigOptionPtrArray);

typedef struct {
  uint32_t TTL; /* TTL to do resubscribe before timeout */
  boolean isSubscribed;
  uint8_t flags;
} Sd_ConsumedEventGroupContextType;

/* @ECUC_SD_00056 */
typedef struct {
  boolean AutoRequire;
  uint16_t HandleId;
  uint16_t EventGroupId;
  SoAd_SoConIdType MulticastEventSoConRef;
  uint8_t MulticastThreshold;
  Sd_ConsumedEventGroupContextType *context;
} Sd_ConsumedEventGroupType;

typedef struct {
  Sd_EventHandlerSubscriberListType listEventHandlerSubscribers;
  bool isMulticastOpened;
} Sd_EventHandlerContextType;

typedef void (*SomeIp_EventGroupOnSubscribeFncType)(boolean isSubscribe,
                                                    TcpIp_SockAddrType *RemoteAddr);

/* @ECUC_SD_00055 */
typedef struct {
  uint16_t HandleId;
  uint16_t EventGroupId;
  SoAd_SoConIdType MulticastEventSoConRef;
  TcpIp_SockAddrType MulticastEventAddr;
  PduIdType MulticastTxPduId;
  uint8_t MulticastThreshold;
  Sd_EventHandlerContextType *context;
  SomeIp_EventGroupOnSubscribeFncType onSubscribe;
} Sd_EventHandlerType;

typedef struct {
  uint32_t TTL;
  Sd_PhaseType phase;
  uint16_t offerTimer;
  uint8_t flags;
  uint8_t counter;
} Sd_ServerServiceContextType;

/* @ECUC_SD_00004 */
typedef struct {
  boolean AutoAvailable;
  uint16_t HandleId;
  uint16_t ServiceId;
  uint16_t InstanceId;
#if 0
  uint16_t LoadBalancingPriority;
  uint16_t LoadBalancingWeight;
#endif
  uint8_t MajorVersion;
  uint32_t MinorVersion;
  SoAd_SoConIdType SoConId;
  TcpIp_ProtocolType ProtocolType;
  Sd_CapabilityRecordMatchCalloutFncType CapabilityRecordMatchCalloutRef;
  const Sd_ServerTimerType *ServerTimer;
  Sd_ServerServiceContextType *context;
  uint8_t InstanceIndex;
  const Sd_EventHandlerType *EventHandlers;
  uint16_t numOfEventHandlers;
  uint16_t SomeIpServiceId;
} Sd_ServerServiceType;

typedef struct {
  uint32_t TTL;
  Sd_PhaseType phase;
  uint16_t findTimer;
  uint8_t flags;
  uint8_t counter;
  boolean isOffered;
  /* remote server ip:port which provide this service */
  TcpIp_SockAddrType RemoteAddr;
  uint16_t port; /* subscribe port */
  uint16_t sessionId;
} Sd_ClientServiceContextType;

/* @ECUC_SD_00005 */
typedef struct {
  boolean AutoRequire;
  uint16_t HandleId;
  uint16_t ServiceId;
  uint16_t InstanceId;
  uint8_t MajorVersion;
  uint32_t MinorVersion;
  SoAd_SoConIdType SoConId;
  TcpIp_ProtocolType ProtocolType;
  Sd_CapabilityRecordMatchCalloutFncType SdClientCapabilityRecordMatchCalloutRef;
  const Sd_ClientTimerType *ClientTimer;
  Sd_ClientServiceContextType *context;
  uint8_t InstanceIndex;
  const Sd_ConsumedEventGroupType *ConsumedEventGroups;
  uint16_t numOfConsumedEventGroups;
} Sd_ClientServiceType;

typedef struct {
  PduIdType RxPduId;
  SoAd_SoConIdType SoConId;
} Sd_InstanceMulticastRxPduType;

typedef struct {
  PduIdType MulticastTxPduId;
  PduIdType UnicastTxPduId;
} Sd_InstanceTxPduType;

typedef struct {
  PduIdType RxPduId;
  SoAd_SoConIdType SoConId;
} Sd_InstanceUnicastRxPduType;

typedef struct {
  uint16_t multicastSessionId;
  uint8_t flags;
} Sd_InstanceContextType;

/* @ECUC_SD_00084 */
typedef struct {
  const char *Hostname;
  uint16_t SubscribeEventgroupRetryDelay;
  uint8_t SubscribeEventgroupRetryMax;
  const Sd_InstanceMulticastRxPduType MulticastRxPdu;
  const Sd_InstanceUnicastRxPduType UnicastRxPdu;
  const Sd_InstanceTxPduType TxPdu;

  const Sd_ServerServiceType *ServerServices;
  uint16_t numOfServerServices;
  const Sd_ClientServiceType *ClientServices;
  uint16_t numOfClientServices;
  uint8_t *buffer;
  PduLengthType bufLen;
  Sd_InstanceContextType *context;
} Sd_InstanceType;

struct Sd_Config_s {
  const Sd_InstanceType *Instances;
  uint8_t numOfInstances;
  const Sd_ServerServiceType *const *ServerServicesMap;
  uint16_t numOfServerServices;
  const Sd_ClientServiceType *const *ClientServicesMap;
  uint16_t numOfClientServices;
  const uint16_t *EventHandlersMap;
  const uint16_t *PerServiceEventHandlerMap;
  uint16_t numOfEventHandlers;
  const uint16_t *ConsumedEventGroupsMap;
  const uint16_t *PerServiceConsumedEventGroupsMap;
  uint16_t numOfConsumedEventGroups;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _SD_PRIV_H */
