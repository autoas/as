/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of TCP/IP Stack AUTOSAR CP Release 4.4.0
 */
#ifndef _TCPIP_H
#define _TCPIP_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define TCPIP_AF_INET ((TcpIp_DomainType)0x02)
#define TCPIP_AF_INET6 ((TcpIp_DomainType)0x1c)

#define TCPIP_E_NOSPACE ((Std_ReturnType)0x10)

/* ================================ [ TYPES     ] ============================================== */
typedef int TcpIp_SocketIdType;

/* @SWS_TCPIP_00009 */
typedef uint16_t TcpIp_DomainType;

/* @SWS_TCPIP_00010 */
typedef enum
{
  TCPIP_IPPROTO_TCP = 0x06,
  TCPIP_IPPROTO_UDP = 0x11,
} TcpIp_ProtocolType;

/* @SWS_TCPIP_00012 */
typedef struct {
  uint16_t port;
  uint8_t addr[4];
} TcpIp_SockAddrType;

/* @SWS_TCPIP_00013 */
typedef struct {
  uint16_t port;
  uint32_t addr[1];
} TcpIp_SockAddrInetType;

/* @SWS_TCPIP_00014 */
typedef struct {
  uint16_t port;
  uint32_t addr[4];
} TcpIp_SockAddrInet6Type;

/* @SWS_TCPIP_00065 */
typedef enum
{
  TCPIP_IPADDR_ASSIGNMENT_STATIC,
  TCPIP_IPADDR_ASSIGNMENT_LINKLOCAL_DOIP,
  TCPIP_IPADDR_ASSIGNMENT_DHCP,
  TCPIP_IPADDR_ASSIGNMENT_LINKLOCAL,
  TCPIP_IPADDR_ASSIGNMENT_IPV6_ROUTER,
  TCPIP_IPADDR_ASSIGNMENT_ALL,
} TcpIp_IpAddrAssignmentType;

typedef struct TcpIp_Config_s TcpIp_ConfigType;

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_TCPIP_00002 */
void TcpIp_Init(const TcpIp_ConfigType *ConfigPtr);

Std_ReturnType TcpIp_SetupAddrFrom(TcpIp_SockAddrType *RemoteAddrPtr, const char *ip,
                                   uint16_t port);

Std_ReturnType TcpIp_GetLocalIp(TcpIp_SockAddrType *addr);

Std_ReturnType TcpIp_GetLocalAddr(TcpIp_SocketIdType SocketId, TcpIp_SockAddrType *addr);

/* @SWS_TCPIP_00026 */
void TcpIp_MainFunction(void);

TcpIp_SocketIdType TcpIp_Create(TcpIp_ProtocolType protocol);

Std_ReturnType TcpIp_SetNonBlock(TcpIp_SocketIdType SocketId, boolean nonBlocked);

Std_ReturnType TcpIp_SetTimeout(TcpIp_SocketIdType SocketId, uint32_t timeoutMs);

/* @SWS_TCPIP_00017 */
Std_ReturnType TcpIp_Close(TcpIp_SocketIdType SocketId, boolean Abort);

/* @SWS_TCPIP_00015 */
Std_ReturnType TcpIp_Bind(TcpIp_SocketIdType SocketId, const char *LocalAddr, uint16_t Port);

/* @SWS_TCPIP_00022 */
Std_ReturnType TcpIp_TcpConnect(TcpIp_SocketIdType SocketId,
                                const TcpIp_SockAddrType *RemoteAddrPtr);

/* @SWS_TCPIP_00023 */
Std_ReturnType TcpIp_TcpListen(TcpIp_SocketIdType SocketId, uint16_t MaxChannels);

Std_ReturnType TcpIp_TcpAccept(TcpIp_SocketIdType SocketId, TcpIp_SocketIdType *AcceptSock,
                               TcpIp_SockAddrType *RemoteAddrPtr);

Std_ReturnType TcpIp_IsTcpStatusOK(TcpIp_SocketIdType SocketId);

Std_ReturnType TcpIp_Recv(TcpIp_SocketIdType SocketId, uint8_t *BufPtr,
                          uint32_t *Length /* InOut */);

Std_ReturnType TcpIp_RecvFrom(TcpIp_SocketIdType SocketId, TcpIp_SockAddrType *RemoteAddrPtr,
                              uint8_t *BufPtr, uint32_t *Length /* InOut */);

Std_ReturnType TcpIp_SendTo(TcpIp_SocketIdType SocketId, const TcpIp_SockAddrType *RemoteAddrPtr,
                            const uint8_t *BufPtr, uint32_t Length);

Std_ReturnType TcpIp_Send(TcpIp_SocketIdType SocketId, const uint8_t *BufPtr, uint32_t Length);

/*
 * Idel: The time (in seconds) the connection needs to remain idle before TCP starts sending
 * keepalive probes,
 * Interval: The time (in seconds) between individual keepalive probes.
 * Count: The maximum number of keepalive probes TCP should send before dropping the connection.
 **/
Std_ReturnType TcpIp_TcpKeepAlive(TcpIp_SocketIdType SocketId, uint32_t Idel, uint32_t Interval,
                                  uint32_t Count);

uint16_t TcpIp_Tell(TcpIp_SocketIdType SocketId);
#ifdef __cplusplus
}
#endif
#endif /* _TCPIP_H */
