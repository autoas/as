/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of TCP/IP Stack AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <string.h>
#include <stdlib.h>

#include "TcpIp.h"
#include "Std_Debug.h"

#if defined(linux) && !defined(USE_LWIP)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#elif defined(_WIN32) && !defined(USE_LWIP)
#include <Ws2tcpip.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2def.h>
#include <errno.h>
#else
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/init.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/api.h"
#include "lwip/tcpip.h"
#if defined(_WIN32)
#include "pcapif.h"
#else
#include "netif/tapif.h"
#endif
#include "lwip/sockets.h"
#endif

/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_TCPIP 0
#define AS_LOG_TCPIPI 1
#define AS_LOG_TCPIPE 2
#define NETIF_ADDRS ipaddr, netmask, gw,

#ifndef TCPIP_MAX_DATA_SIZE
#define TCPIP_MAX_DATA_SIZE 1420
#endif

/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
#ifdef USE_LWIP
static struct netif netif;
#endif
/* ================================ [ LOCALS    ] ============================================== */
#ifdef USE_LWIP
static void init_default_netif(const ip4_addr_t *ipaddr, const ip4_addr_t *netmask,
                               const ip4_addr_t *gw) {
#if defined(_WIN32)
  netif_add(&netif, NETIF_ADDRS NULL, pcapif_init, tcpip_input);
#else
  netif_add(&netif, NETIF_ADDRS NULL, tapif_init, tcpip_input);
#endif
  netif_set_default(&netif);
}

static void default_netif_poll(void) {
#if defined(_WIN32)
#if !PCAPIF_RX_USE_THREAD
  pcapif_poll(&netif);
#endif
#else
#if NO_SYS
  tapif_poll(&netif);
#endif
#endif
}

static void tcpIpInit(void *arg) { /* remove compiler warning */
  sys_sem_t *init_sem;
  ip4_addr_t ipaddr, netmask, gw;

  init_sem = (sys_sem_t *)arg;

  LWIP_PORT_INIT_GW(&gw);
  LWIP_PORT_INIT_IPADDR(&ipaddr);
  LWIP_PORT_INIT_NETMASK(&netmask);
  ASLOG(TCPIPI, ("Starting lwIP, IP %s\n", ip4addr_ntoa(&ipaddr)));

  init_default_netif(&ipaddr, &netmask, &gw);

  netif_set_up(netif_default);

  sys_sem_signal(init_sem);
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
void TcpIp_Init(const TcpIp_ConfigType *ConfigPtr) {
#ifdef USE_LWIP
  sys_sem_t init_sem;
  sys_sem_new(&init_sem, 0);
  tcpip_init(tcpIpInit, &init_sem);
  sys_sem_wait(&init_sem);
  sys_sem_free(&init_sem);
#ifdef linux
  /* ref https://wiki.qemu.org/Documentation/Networking#Tap
   * route add -nv 224.224.224.245 dev tap0
   * route add -nv 224.224.224.245 dev enp0s3
   */
#endif
#elif defined(_WIN32)
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

void TcpIp_MainFunction(void) {
#ifdef USE_LWIP
  default_netif_poll();
#endif
}

TcpIp_SocketIdType TcpIp_Create(TcpIp_ProtocolType protocol) {
  TcpIp_SocketIdType sockId;
  int type;
#if defined(_WIN32) && !defined(USE_LWIP)
  u_long iMode = 1;
#endif
  int on = 1;

  if (TCPIP_IPPROTO_TCP == protocol) {
    type = SOCK_STREAM;
  } else {
    type = SOCK_DGRAM;
  }

  sockId = socket(AF_INET, type, 0);
  if (sockId >= 0) {
    setsockopt(sockId, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(int));
    if (TCPIP_IPPROTO_UDP == protocol) {
#if defined(_WIN32) && !defined(USE_LWIP)
      ioctlsocket(sockId, FIONBIO, &iMode);
#else
      ioctl(sockId, FIONBIO, &on);
#endif
    }
  }

  ASLOG(TCPIP, ("[%d] %s created\n", sockId, type == SOCK_STREAM ? "TCP" : "UDP"));

  return sockId;
}

Std_ReturnType TcpIp_Close(TcpIp_SocketIdType SocketId, boolean Abort) {
  int r;
  Std_ReturnType ret = E_OK;
  (void)Abort;
#if defined(_WIN32) && !defined(USE_LWIP)
  r = closesocket(SocketId);
#else
  r = close(SocketId);
#endif
  if (0 != r) {
    ret = E_NOT_OK;
  }

  ASLOG(TCPIP, ("[%d] close\n", SocketId));

  return ret;
}

Std_ReturnType TcpIp_Bind(TcpIp_SocketIdType SocketId, const char *LocalAddr, uint16_t Port) {
  Std_ReturnType ret = E_OK;

  int r;
#ifdef USE_LWIP
  ip_addr_t ipaddr;
#endif
  struct ip_mreq mreq;
  struct sockaddr_in sLocalAddr;
#if defined(_WIN32) && !defined(USE_LWIP)
  u_long iMode = 1;
  ioctlsocket(SocketId, FIONBIO, &iMode);
#else
  int on = 1;
  ioctl(SocketId, FIONBIO, &on);
#endif
#ifdef USE_LWIP
  setsockopt(SocketId, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(int)); /* Set socket to no delay */
#endif
  /*Source*/
  memset((char *)&sLocalAddr, 0, sizeof(sLocalAddr));
  sLocalAddr.sin_family = AF_INET;
#ifdef USE_LWIP
  sLocalAddr.sin_len = sizeof(sLocalAddr);
#endif
  sLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  sLocalAddr.sin_port = htons(Port);

  ASLOG(TCPIP, ("[%d] bind to :%d\n", SocketId, Port));
  r = bind(SocketId, (struct sockaddr *)&sLocalAddr, sizeof(sLocalAddr));
  if ((0 == r) && (LocalAddr != NULL)) {
#ifndef USE_LWIP
    if (0 == strncmp(LocalAddr, "224.", 4)) {
#else
    ipaddr.addr = ipaddr_addr(LocalAddr);
    if (ip_addr_ismulticast(&ipaddr)) {
#endif

#ifndef USE_LWIP
      mreq.imr_multiaddr.s_addr = inet_addr(LocalAddr);
      mreq.imr_interface.s_addr = htonl(INADDR_ANY);
#else
      mreq.imr_multiaddr.s_addr = ipaddr.addr;
      mreq.imr_interface.s_addr = netif.ip_addr.addr;
#endif
      r = setsockopt(SocketId, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq));
      ASLOG(TCPIP, ("[%d] multicast on %s\n", SocketId, LocalAddr));
    }
  }

  if (0 != r) {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType TcpIp_TcpKeepAlive(TcpIp_SocketIdType SocketId, uint32_t Idel, uint32_t Interval,
                                  uint32_t Count) {
  Std_ReturnType ret = E_OK;
#if defined(_WIN32) && !defined(USE_LWIP)
  struct tcp_keepalive keepin;
  struct tcp_keepalive keepout;
  DWORD bytesnum;
#else
  int keepalive = 1;
#ifndef USE_LWIP
  int keepInterval = Interval;
  int keepIdle = Idel;
  int keepCount = Count;
#endif
#endif
  int r = 0;

#if defined(_WIN32) && !defined(USE_LWIP)
  keepin.keepaliveinterval = Interval * 1000;
  keepin.keepalivetime = Idel * 1000;
  keepin.onoff = 1;
  r = WSAIoctl(SocketId, SIO_KEEPALIVE_VALS, &keepin, sizeof(keepin), &keepout, sizeof(keepout),
               &bytesnum, NULL, NULL);
#else
#ifndef USE_LWIP
  r = setsockopt(SocketId, SOL_TCP, TCP_KEEPIDLE, (void *)&keepIdle, sizeof(keepIdle));
  r |= setsockopt(SocketId, SOL_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval));
  r |= setsockopt(SocketId, SOL_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount));
#endif
  r |= setsockopt(SocketId, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive, sizeof(keepalive));
#endif

  if (0 != r) {
    ret = E_NOT_OK;
    ASLOG(TCPIP, ("[%d] keep alive fail\n", SocketId));
  }

  return ret;
}

Std_ReturnType TcpIp_TcpListen(TcpIp_SocketIdType SocketId, uint16_t MaxChannels) {
  Std_ReturnType ret = E_NOT_OK;
  int r;

  r = listen(SocketId, MaxChannels);
  if (0 == r) {
    ret = E_OK;
  }
  ASLOG(TCPIP, ("[%d] listen(%d) \n", SocketId, MaxChannels));

  return ret;
}

Std_ReturnType TcpIp_TcpAccept(TcpIp_SocketIdType SocketId, TcpIp_SocketIdType *AcceptSock,
                               TcpIp_SockAddrType *RemoteAddrPtr) {
  Std_ReturnType ret = E_OK;
  int clientFd;
  struct sockaddr_in client_addr;
  int addrlen = sizeof(client_addr);

  clientFd = accept(SocketId, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen);

  if (clientFd >= 0) {
    /* New connection established */
#if defined(_WIN32) && !defined(USE_LWIP)
    u_long iMode = 1;
    ioctlsocket(clientFd, FIONBIO, &iMode);
#else
    int on = 1;
    ioctl(clientFd, FIONBIO, &on);
#endif

    TcpIp_TcpKeepAlive(clientFd, 10, 1, 3);
    RemoteAddrPtr->port = htons(client_addr.sin_port);
    memcpy(RemoteAddrPtr->addr, &client_addr.sin_addr.s_addr, 4);
    ASLOG(TCPIP,
          ("[%d] accept %d.%d.%d.%d:%d\n", SocketId, RemoteAddrPtr->addr[0], RemoteAddrPtr->addr[1],
           RemoteAddrPtr->addr[2], RemoteAddrPtr->addr[3], RemoteAddrPtr->port));
    *AcceptSock = (TcpIp_SocketIdType)clientFd;
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType TcpIp_IsTcpStatusOK(TcpIp_SocketIdType SocketId) {
  Std_ReturnType ret = E_OK;
  int sockErr = 0, r;
  socklen_t sockErrLen = sizeof(sockErr);

  r = getsockopt(SocketId, SOL_SOCKET, SO_ERROR, (char *)&sockErr, &sockErrLen);
  if (r != 0) {
    ret = E_NOT_OK;
  } else {
    if ((sockErr != 0) && (sockErr != EWOULDBLOCK)) {
      ret = E_NOT_OK;
      ASLOG(TCPIP, ("[%d] status bad\n", SocketId));
    }
  }

  return ret;
}

Std_ReturnType TcpIp_Recv(TcpIp_SocketIdType SocketId, uint8_t *BufPtr,
                          uint16_t *Length /* InOut */) {
  Std_ReturnType ret = E_OK;
  int nbytes;

  nbytes = recv(SocketId, (char *)BufPtr, *Length, 0);

  *Length = 0;
  if (nbytes > 0) {
    *Length = nbytes;
    ASLOG(TCPIP, ("[%d] recv %d bytes\n", SocketId, nbytes));
  } else if (nbytes < -1) {
    ret = nbytes;
    ASLOG(TCPIPE, ("[%d] recv got error %d\n", nbytes));
  } else {
    /* got nothing */
  }

  return ret;
}

Std_ReturnType TcpIp_RecvFrom(TcpIp_SocketIdType SocketId, TcpIp_SockAddrType *RemoteAddrPtr,
                              uint8_t *BufPtr, uint16_t *Length /* InOut */) {
  Std_ReturnType ret = E_OK;
  struct sockaddr_in fromAddr;
  socklen_t fromAddrLen = sizeof(fromAddr);
  int nbytes;

  nbytes =
    recvfrom(SocketId, (char *)BufPtr, *Length, 0, (struct sockaddr *)&fromAddr, &fromAddrLen);

  *Length = 0;
  if (nbytes > 0) {
    RemoteAddrPtr->port = htons(fromAddr.sin_port);
    memcpy(RemoteAddrPtr->addr, &fromAddr.sin_addr.s_addr, 4);
    ASLOG(TCPIP, ("[%d] recv %d bytes from %d.%d.%d.%d:%d\n", SocketId, nbytes,
                  RemoteAddrPtr->addr[0], RemoteAddrPtr->addr[1], RemoteAddrPtr->addr[2],
                  RemoteAddrPtr->addr[3], RemoteAddrPtr->port));
    *Length = nbytes;
  } else if (nbytes < -1) {
    ret = nbytes;
    ASLOG(TCPIPE, ("[%d] recvfrom got error %d\n", nbytes));
  } else {
    /* got nothing */
  }

  return ret;
}

Std_ReturnType TcpIp_SendTo(TcpIp_SocketIdType SocketId, const TcpIp_SockAddrType *RemoteAddrPtr,
                            const uint8_t *BufPtr, uint16_t Length) {
  Std_ReturnType ret = E_OK;
  struct sockaddr_in toAddr;
  socklen_t toAddrLen = sizeof(toAddr);
  int nbytes;

  toAddr.sin_family = AF_INET;
#ifdef USE_LWIP
  toAddr.sin_len = sizeof(toAddr);
#endif

  memcpy(&toAddr.sin_addr.s_addr, RemoteAddrPtr->addr, 4);
  toAddr.sin_port = htons(RemoteAddrPtr->port);
  nbytes = sendto(SocketId, (char *)BufPtr, Length, 0, (struct sockaddr *)&toAddr, toAddrLen);

  ASLOG(TCPIP, ("[%d] send to %d.%d.%d.%d:%d %d/%d bytes\n", SocketId, RemoteAddrPtr->addr[0],
                RemoteAddrPtr->addr[1], RemoteAddrPtr->addr[2], RemoteAddrPtr->addr[3],
                RemoteAddrPtr->port, nbytes, Length));

  if (nbytes != Length) {
    ASLOG(TCPIPE, ("[%d] sendto(%d), error is %d\n", SocketId, Length, nbytes));
    if (nbytes >= 0) {
      ret = TCPIP_E_NOSPACE;
    } else {
      ret = E_NOT_OK;
    }
  }

  return ret;
}

Std_ReturnType TcpIp_Send(TcpIp_SocketIdType SocketId, const uint8_t *BufPtr, uint16_t Length) {
  Std_ReturnType ret = E_OK;
  int nbytes;

  nbytes = send(SocketId, (char *)BufPtr, Length, 0);
  ASLOG(TCPIP, ("[%d] send(%d/%d)\n", SocketId, nbytes, Length));

  if (nbytes != Length) {
    ASLOG(TCPIPE, ("[%d] send(%d), error is %d\n", SocketId, Length, nbytes));
    if (nbytes >= 0) {
      ret = TCPIP_E_NOSPACE;
    } else {
      ret = E_NOT_OK;
    }
  }

  return ret;
}

Std_ReturnType TcpIp_TcpConnect(TcpIp_SocketIdType SocketId,
                                const TcpIp_SockAddrType *RemoteAddrPtr) {
  Std_ReturnType ret = E_NOT_OK;
  int r;
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  memcpy(&addr.sin_addr.s_addr, RemoteAddrPtr->addr, 4);
  addr.sin_port = htons(RemoteAddrPtr->port);

  r = connect(SocketId, (struct sockaddr *)&addr, sizeof(struct sockaddr));
  if (0 == r) {
#if defined(_WIN32) && !defined(USE_LWIP)
    u_long iMode = 1;
    ioctlsocket(SocketId, FIONBIO, &iMode);
#else
    int on = 1;
    ioctl(SocketId, FIONBIO, &on);
#endif
    ret = E_OK;
  }
  ASLOG(TCPIP,
        ("[%d] connect %d.%d.%d.%d:%d\n", SocketId, RemoteAddrPtr->addr[0], RemoteAddrPtr->addr[1],
         RemoteAddrPtr->addr[2], RemoteAddrPtr->addr[3], RemoteAddrPtr->port));

  return ret;
}

Std_ReturnType TcpIp_SetupAddrFrom(TcpIp_SockAddrType *RemoteAddrPtr, const char *ip,
                                   uint16_t port) {
  Std_ReturnType ret = E_OK;
  uint32_t u32Addr = htonl(INADDR_ANY);

  if (NULL != RemoteAddrPtr) {
    if (NULL != ip) {
#ifndef USE_LWIP
      u32Addr = inet_addr(ip);
#else
      u32Addr = ipaddr_addr(ip);
#endif
    }
    memcpy(RemoteAddrPtr->addr, &u32Addr, 4);
    RemoteAddrPtr->port = port;
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType TcpIp_GetLocalIp(TcpIp_SockAddrType *addr) {
#ifdef USE_LWIP
  memcpy(addr->addr, &netif.ip_addr.addr, 4);
  return E_OK;
#else
  uint32_t u32Addr;
  char *ip;

  ip = getenv("AS_LOCAL_IP");
  if (NULL == ip) {
    ip = "172.18.0.1";
  }
  u32Addr = inet_addr(ip);
  memcpy(addr->addr, &u32Addr, 4);
  return E_OK;
#endif
}

uint16_t TcpIp_Tell(TcpIp_SocketIdType SocketId) {
#if defined(_WIN32) && !defined(USE_LWIP)
  u_long Length = 0;
  ioctlsocket(SocketId, FIONREAD, &Length);
#else
  int Length = 0;
  ioctl(SocketId, FIONREAD, &Length);
#endif

  if (Length > TCPIP_MAX_DATA_SIZE) {
    Length = TCPIP_MAX_DATA_SIZE;
  }

  return Length;
}