/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Socket Adaptor AUTOSAR CP Release 4.4.0
 */
#ifndef _SOAD_H
#define _SOAD_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "TcpIp.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define SOAD_MEAS_DROP_TCP ((SoAd_MeasurementIdxType)0x01)
#define SOAD_MEAS_DROP_UDP ((SoAd_MeasurementIdxType)0x02)
#define SOAD_MEAS_ALL ((SoAd_MeasurementIdxType)0x0xFF)
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_SoAd_00518 */
typedef uint16_t SoAd_SoConIdType;

/* @SWS_SoAd_00512 */
typedef enum
{
  SOAD_SOCON_ONLINE,
  SOAD_SOCON_RECONNECT,
  SOAD_SOCON_OFFLINE
} SoAd_SoConModeType;

/* @SWS_SoAd_00519 */
typedef uint16_t SoAd_RoutingGroupIdType;

/* @SWS_SoAd_91010 */
typedef uint8_t SoAd_MeasurementIdxType;

typedef struct SoAd_Config_s SoAd_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_SoAd_00093 */
void SoAd_Init(const SoAd_ConfigType *ConfigPtr);

/* @SWS_SoAd_00091 */
Std_ReturnType SoAd_IfTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_SoAd_00656 */
Std_ReturnType SoAd_IfRoutingGroupTransmit(SoAd_RoutingGroupIdType id);

/* @SWS_SoAd_00711 */
Std_ReturnType SoAd_IfSpecificRoutingGroupTransmit(SoAd_RoutingGroupIdType id,
                                                   SoAd_SoConIdType SoConId);

/* @SWS_SoAd_00105 */
Std_ReturnType SoAd_TpTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_SoAd_00522 */
Std_ReturnType SoAd_TpCancelTransmit(PduIdType TxPduId);

/* @SWS_SoAd_00521 */
Std_ReturnType SoAd_TpCancelReceive(PduIdType RxPduId);

/* @SWS_SoAd_00509 */
Std_ReturnType SoAd_GetSoConId(PduIdType TxPduId, SoAd_SoConIdType *SoConIdPtr);

/* @SWS_SoAd_00510 */
Std_ReturnType SoAd_OpenSoCon(SoAd_SoConIdType SoConId);

/* @SWS_SoAd_00511 */
Std_ReturnType SoAd_CloseSoCon(SoAd_SoConIdType SoConId, boolean abort);

/* @SWS_SoAd_91001 */
void SoAd_GetSoConMode(SoAd_SoConIdType SoConId, SoAd_SoConModeType *ModePtr);

/* @SWS_SoAd_00520 */
Std_ReturnType SoAd_RequestIpAddrAssignment(SoAd_SoConIdType SoConId,
                                            TcpIp_IpAddrAssignmentType Type,
                                            const TcpIp_SockAddrType *LocalIpAddrPtr,
                                            uint8_t Netmask,
                                            const TcpIp_SockAddrType *DefaultRouterPtr);

/* @SWS_SoAd_00536 */
Std_ReturnType SoAd_ReleaseIpAddrAssignment(SoAd_SoConIdType SoConId);

/* @SWS_SoAd_00506 */
Std_ReturnType SoAd_GetLocalAddr(SoAd_SoConIdType SoConId, TcpIp_SockAddrType *LocalAddrPtr,
                                 uint8_t *NetmaskPtr, TcpIp_SockAddrType *DefaultRouterPtr);

/* @SWS_SoAd_00507 */
Std_ReturnType SoAd_GetPhysAddr(SoAd_SoConIdType SoConId, uint8_t *PhysAddrPtr);

/* @SWS_SoAd_00655 */
Std_ReturnType SoAd_GetRemoteAddr(SoAd_SoConIdType SoConId, TcpIp_SockAddrType *IpAddrPtr);

/* @SWS_SoAd_00515 */
Std_ReturnType SoAd_SetRemoteAddr(SoAd_SoConIdType SoConId,
                                  const TcpIp_SockAddrType *RemoteAddrPtr);

/* @SWS_SoAd_00121 */
void SoAd_MainFunction(void);

Std_ReturnType SoAd_TakeControl(SoAd_SoConIdType SoConId);
Std_ReturnType SoAd_SetNonBlock(SoAd_SoConIdType SoConId, boolean nonBlocked);
Std_ReturnType SoAd_SetTimeout(SoAd_SoConIdType SoConId, uint32_t timeoutMs);
Std_ReturnType SoAd_ControlRx(SoAd_SoConIdType SoConId, uint8_t* data, uint32_t length);
#ifdef __cplusplus
}
#endif
#endif /* _SOAD_H */
