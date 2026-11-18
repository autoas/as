/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Socket Adaptor AUTOSAR CP Release 4.4.0
 */
#ifndef SOAD_H
#define SOAD_H
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

/* @SWS_SoAd_00101 */
#define SOAD_E_UNINIT 0x01
#define SOAD_E_PARAM_POINTER 0x02
#define SOAD_E_INV_ARG 0x03
#define SOAD_E_INV_PDUID 0x06
#define SOAD_E_INV_SOCKETID 0x07
#define SOAD_E_INIT_FAILED 0x08
#define SOAD_E_INV_METADATA 0x09
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_SoAd_00518 */
typedef uint16_t SoAd_SoConIdType;

/* @SWS_SoAd_00512 */
typedef enum {
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
void SoAd_Init(const SoAd_ConfigType *configPtr);

/* @SWS_SoAd_00091 */
Std_ReturnType SoAd_IfTransmit(PduIdType txPduId, const PduInfoType *pduInfoPtr);

/* @SWS_SoAd_00656 */
Std_ReturnType SoAd_IfRoutingGroupTransmit(SoAd_RoutingGroupIdType id);

/* @SWS_SoAd_00711 */
Std_ReturnType SoAd_IfSpecificRoutingGroupTransmit(SoAd_RoutingGroupIdType id,
                                                   SoAd_SoConIdType soConId);

/* @SWS_SoAd_00105 */
Std_ReturnType SoAd_TpTransmit(PduIdType txPduId, const PduInfoType *pduInfoPtr);

/* @SWS_SoAd_00522 */
Std_ReturnType SoAd_TpCancelTransmit(PduIdType txPduId);

/* @SWS_SoAd_00521 */
Std_ReturnType SoAd_TpCancelReceive(PduIdType rxPduId);

/* @SWS_SoAd_00509 */
Std_ReturnType SoAd_GetSoConId(PduIdType txPduId, SoAd_SoConIdType *soConIdPtr);

/* @SWS_SoAd_00510 */
Std_ReturnType SoAd_OpenSoCon(SoAd_SoConIdType soConId);

/* @SWS_SoAd_00511 */
Std_ReturnType SoAd_CloseSoCon(SoAd_SoConIdType soConId, boolean abort);

/* @SWS_SoAd_91001 */
void SoAd_GetSoConMode(SoAd_SoConIdType soConId, SoAd_SoConModeType *modePtr);

/* @SWS_SoAd_00520 */
Std_ReturnType SoAd_RequestIpAddrAssignment(SoAd_SoConIdType soConId,
                                            TcpIp_IpAddrAssignmentType type,
                                            const TcpIp_SockAddrType *localIpAddrPtr,
                                            uint8_t netmask,
                                            const TcpIp_SockAddrType *defaultRouterPtr);

/* @SWS_SoAd_00536 */
Std_ReturnType SoAd_ReleaseIpAddrAssignment(SoAd_SoConIdType soConId);

/* @SWS_SoAd_00506 */
Std_ReturnType SoAd_GetLocalAddr(SoAd_SoConIdType soConId, TcpIp_SockAddrType *localAddrPtr,
                                 uint8_t *netmaskPtr, TcpIp_SockAddrType *defaultRouterPtr);

/* @SWS_SoAd_00507 */
Std_ReturnType SoAd_GetPhysAddr(SoAd_SoConIdType soConId, uint8_t *physAddrPtr);

/* @SWS_SoAd_00655 */
Std_ReturnType SoAd_GetRemoteAddr(SoAd_SoConIdType soConId, TcpIp_SockAddrType *ipAddrPtr);

/* @SWS_SoAd_00515 */
Std_ReturnType SoAd_SetRemoteAddr(SoAd_SoConIdType soConId,
                                  const TcpIp_SockAddrType *remoteAddrPtr);

/* @SWS_SoAd_00121 */
void SoAd_MainFunction(void);

Std_ReturnType SoAd_TakeControl(SoAd_SoConIdType soConId);
Std_ReturnType SoAd_SetNonBlock(SoAd_SoConIdType soConId, boolean nonBlocked);
Std_ReturnType SoAd_SetTimeout(SoAd_SoConIdType soConId, uint32_t timeoutMs);
Std_ReturnType SoAd_ControlRecv(SoAd_SoConIdType soConId, uint8_t *data, uint32_t *length);

/* @SWS_SoAd_00096 */
void SoAd_GetVersionInfo(Std_VersionInfoType *versionInfo);
#ifdef __cplusplus
}
#endif
#endif /* SOAD_H */
