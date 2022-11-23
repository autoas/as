/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Service Discovery AUTOSAR CP Release 4.4.0
 */
#ifndef _SD_H
#define _SD_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "SoAd.h"
#include "sys/queue.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_SD_00118 */
typedef enum
{
  SD_SERVER_SERVICE_DOWN,
  SD_SERVER_SERVICE_AVAILABLE,
} Sd_ServerServiceSetStateType;

/* @SWS_SD_00405 */
typedef enum
{
  SD_CLIENT_SERVICE_RELEASED,
  SD_CLIENT_SERVICE_REQUESTED,
} Sd_ClientServiceSetStateType;

/* @SWS_SD_00550 */
typedef enum
{
  SD_CONSUMED_EVENTGROUP_RELEASED,
  SD_CONSUMED_EVENTGROUP_REQUESTED,
} Sd_ConsumedEventGroupSetStateType;

/* @SWS_SD_00551 */
typedef enum
{
  SD_CLIENT_SERVICE_DOWN,
  SD_CLIENT_SERVICE_AVAILABLE,
} Sd_ClientServiceCurrentStateType;

/* @SWS_SD_00552 */
typedef enum
{
  SD_CONSUMED_EVENTGROUP_DOWN,
  SD_CONSUMED_EVENTGROUP_AVAILABLE,
} Sd_ConsumedEventGroupCurrentStateType;

/* @SWS_SD_00553 */
typedef enum
{
  SD_EVENT_HANDLER_RELEASED,
  SD_EVENT_HANDLER_REQUESTED,
} Sd_EventHandlerCurrentStateType;

typedef struct Sd_EventHandlerSubscriber_s {
  STAILQ_ENTRY(Sd_EventHandlerSubscriber_s) entry;
  /* remote subscriber address */
  TcpIp_SockAddrType RemoteAddr;
  uint32_t TTL;

  /* subscriber response port */
  uint16_t port;
  uint16_t TxPduId;
  uint8_t flags;
} Sd_EventHandlerSubscriberType;

/* @SWS_SD_91002 */
typedef uint8_t *Sd_ConfigOptionStringType;

typedef struct Sd_Config_s Sd_ConfigType;

typedef STAILQ_HEAD(Sd_EventSubHead, Sd_EventHandlerSubscriber_s) Sd_EventHandlerSubscriberListType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_SD_00119 */
void Sd_Init(const Sd_ConfigType *ConfigPtr);

/* @SWS_SD_00496 */
Std_ReturnType Sd_ServerServiceSetState(uint16_t SdServerServiceHandleId,
                                        Sd_ServerServiceSetStateType ServerServiceState);

/* @SWS_SD_00409 */
Std_ReturnType Sd_ClientServiceSetState(uint16_t ClientServiceHandleId,
                                        Sd_ClientServiceSetStateType ClientServiceState);

/* @SWS_SD_00560 */
Std_ReturnType
Sd_ConsumedEventGroupSetState(uint16_t SdConsumedEventGroupHandleId,
                              Sd_ConsumedEventGroupSetStateType ConsumedEventGroupState);

/* @SWS_SD_91002 */
void Sd_SoConModeChg(SoAd_SoConIdType SoConId, SoAd_SoConModeType Mode);

/* @SWS_SD_00129 */
void Sd_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_SD_00130 */
void Sd_MainFunction(void);

Std_ReturnType Sd_GetProviderAddr(uint16_t ClientServiceHandleId, TcpIp_SockAddrType *RemoteAddr);
Std_ReturnType Sd_GetSubscribers(uint16_t EventHandlerId, Sd_EventHandlerSubscriberListType **list);
void Sd_RemoveSubscriber(uint16_t EventHandlerId, PduIdType TxPduId);
#ifdef __cplusplus
}
#endif
#endif /* _SD_H */
