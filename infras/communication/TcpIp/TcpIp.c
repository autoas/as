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
#include <sys/time.h>
#include <time.h>
#elif defined(_WIN32) && !defined(USE_LWIP)
#include <Ws2tcpip.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2def.h>
#include <errno.h>
#include <iphlpapi.h>
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
#include "lwip/dhcp.h"
#include "lwip/apps/netbiosns.h"
#if defined(_WIN32)
#include "pcapif.h"
#else
#include "netif/tapif.h"
#endif
#include "lwip/sockets.h"
#endif

/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_TCPIP -10
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
#if LWIP_DHCP
/* dhcp struct for the ethernet netif */
struct dhcp netif_dhcp;
#endif /* LWIP_DHCP */
#endif
static boolean lInitialized = FALSE;
/* ================================ [ LOCALS    ] ============================================== */
#ifdef USE_LWIP
static void init_default_netif(const ip4_addr_t *ipaddr, const ip4_addr_t *netmask,
                               const ip4_addr_t *gw) {
  netif.name[0] = 'a';
  netif.name[1] = 's';
#if LWIP_NETIF_HOSTNAME
  netif.hostname = "as";
#endif
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

#if LWIP_DHCP
  ipaddr.addr = 0;
  netmask.addr = 0;
  gw.addr = 0;
#else
  LWIP_PORT_INIT_GW(&gw);
  LWIP_PORT_INIT_IPADDR(&ipaddr);
  LWIP_PORT_INIT_NETMASK(&netmask);
  ASLOG(TCPIPI, ("Starting lwIP, IP %s\n", ip4addr_ntoa(&ipaddr)));
#endif
  init_default_netif(&ipaddr, &netmask, &gw);

#if LWIP_DHCP
  dhcp_set_struct(netif_default, &netif_dhcp);
#endif
  netif_set_up(netif_default);
#if LWIP_DHCP
  /* start dhcp search */
  dhcp_start(netif_default);
  netbiosns_init();
  netbiosns_set_name(netif_default->hostname);
#if 0 // defined(_WIN32) || defined(linux)
  uint32_t over_time = 0;
  while (!dhcp_supplied_address(netif_default)) {
    over_time++;
    ASLOG(TCPIPI, ("dhcp_connect: DHCP discovering... for %d times\n", over_time));
    if (over_time > 10) {
      ASLOG(TCPIPE, ("dhcp_connect: overtime, not doing dhcp\n"));
      break;
    }
    usleep(1000000);
  }

  ASLOG(TCPIPI, ("DHCP IP address: %s\n", ip4addr_ntoa(&netif_dhcp.offered_ip_addr)));
  ASLOG(TCPIPI, ("DHCP Subnet mask: %s\n", ip4addr_ntoa(&netif_dhcp.offered_sn_mask)));
  ASLOG(TCPIPI, ("DHCP Default gateway: %s\n", ip4addr_ntoa(&netif_dhcp.offered_gw_addr)));
#endif
#endif
  sys_sem_signal(init_sem);
}
#endif

#if defined(_WIN32) && !defined(USE_LWIP)
#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
Std_ReturnType TcpIp_GetAdapterAddress(uint32_t number, TcpIp_SockAddrType *addr) {
  Std_ReturnType ret = E_OK;
  DWORD dwRetVal = 0;
  int i = 0;
  ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
  ULONG family = AF_INET;
  PIP_ADAPTER_ADDRESSES pAddresses = NULL;
  ULONG outBufLen = 0;
  ULONG Iterations = 0;

  PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
  PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;

  // Allocate a 15 KB buffer to start with.
  outBufLen = 15000;

  do {
    pAddresses = (IP_ADAPTER_ADDRESSES *)MALLOC(outBufLen);
    if (pAddresses == NULL) {
      printf("Memory allocation failed for IP_ADAPTER_ADDRESSES struct\n");
      ret = E_NOT_OK;
    }

    dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
      FREE(pAddresses);
      pAddresses = NULL;
      ret = E_NOT_OK;
    } else {
      break;
    }

    if (number == Iterations) {
      break;
    }

    Iterations++;

  } while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < 3));

  Iterations = 0;
  ret = E_NOT_OK;
  if (dwRetVal == NO_ERROR) {
    // If successful, output some information from the data we received
    pCurrAddresses = pAddresses;
    while (pCurrAddresses) {
      if (IfOperStatusUp == pCurrAddresses->OperStatus) {
        if (pCurrAddresses->PhysicalAddressLength != 0) {
          printf("Adapter[%d]: ", (int)Iterations);
          for (i = 0; i < (int)pCurrAddresses->PhysicalAddressLength; i++) {
            if (i == (pCurrAddresses->PhysicalAddressLength - 1))
              printf("%.2X\n", (int)pCurrAddresses->PhysicalAddress[i]);
            else
              printf("%.2X-", (int)pCurrAddresses->PhysicalAddress[i]);
          }
        } else {
          printf("Adapter[%d]: localhost\n", (int)Iterations);
        }
        pUnicast = pCurrAddresses->FirstUnicastAddress;
        if (pUnicast != NULL) {
          for (i = 0; pUnicast != NULL; i++) {
            uint8_t *ipaddr = (uint8_t *)&pUnicast->Address.lpSockaddr->sa_data[2];
            printf("\tUnicast[%d]: %u.%u.%u.%u\n", i, ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]);
            if ((i == 0) && (Iterations == number)) {
              memcpy(addr->addr, ipaddr, 4);
              ret = E_OK;
            }
            pUnicast = pUnicast->Next;
          }
        }
        Iterations++;
      }
      pCurrAddresses = pCurrAddresses->Next;
    }
  }

  if (pAddresses) {
    FREE(pAddresses);
  }
  return ret;
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
void TcpIp_Init(const TcpIp_ConfigType *ConfigPtr) {
  if (FALSE == lInitialized) {
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
    lInitialized = TRUE;
  }
}

void TcpIp_MainFunction(void) {
#ifdef USE_LWIP
  default_netif_poll();
#endif
}

TcpIp_SocketIdType TcpIp_Create(TcpIp_ProtocolType protocol) {
  TcpIp_SocketIdType sockId;
  int type;
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
      TcpIp_SetNonBlock(sockId, TRUE);
    }
  }

  ASLOG(TCPIP, ("[%d] %s created\n", sockId, type == SOCK_STREAM ? "TCP" : "UDP"));

  return sockId;
}

Std_ReturnType TcpIp_SetNonBlock(TcpIp_SocketIdType SocketId, boolean nonBlocked) {
  Std_ReturnType ret = E_OK;
  int r;
#if defined(_WIN32) && !defined(USE_LWIP)
  u_long iMode = (u_long)nonBlocked;
  r = ioctlsocket(SocketId, FIONBIO, &iMode);
#else
  int on = (int)nonBlocked;
  r = ioctl(SocketId, FIONBIO, &on);
#endif

  if (0 != r) {
    ASLOG(TCPIPE, ("[%d] set non block falied: %d\n", SocketId, r));
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType TcpIp_SetTimeout(TcpIp_SocketIdType SocketId, uint32_t timeoutMs) {
  Std_ReturnType ret = E_OK;
  int r;
#if defined(linux) || defined(USE_LWIP)
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = timeoutMs * 1000;
  r = setsockopt(SocketId, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
#elif defined(_WIN32)
  DWORD timeout = timeoutMs;
  r = setsockopt(SocketId, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof timeout);
#endif

  if (0 != r) {
    ASLOG(TCPIPE, ("[%d] set timeout falied: %d\n", SocketId, r));
    ret = E_NOT_OK;
  }

  return ret;
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

Std_ReturnType TcpIp_Bind(TcpIp_SocketIdType SocketId, TcpIp_LocalAddrIdType LocalAddrId,
                          uint16_t *PortPtr) {
  Std_ReturnType ret = E_OK;

  int r;
#ifdef USE_LWIP
  int on = 1;
#endif
  struct sockaddr_in sLocalAddr;
#if (defined(_WIN32) || defined(linux)) && !defined(USE_LWIP)
  TcpIp_SockAddrType sAddr;
#endif
#ifdef USE_LWIP
  setsockopt(SocketId, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(int)); /* Set socket to no delay */
#endif
  memset((char *)&sLocalAddr, 0, sizeof(sLocalAddr));
  sLocalAddr.sin_family = AF_INET;
#ifdef USE_LWIP
  sLocalAddr.sin_len = sizeof(sLocalAddr);
#endif
#if (defined(_WIN32) || defined(linux)) && !defined(USE_LWIP)
  if (LocalAddrId == TCPIP_LOCALADDRID_ANY) {
    sLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  } else if (LocalAddrId == TCPIP_LOCALADDRID_LOCALHOST) {
    sLocalAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  } else {
    TcpIp_GetIpAddr(LocalAddrId, &sAddr, NULL, NULL);
    memcpy(&sLocalAddr.sin_addr.s_addr, sAddr.addr, 4);
  }
#else
  sLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
  sLocalAddr.sin_port = htons(*PortPtr);

  ASLOG(TCPIP, ("[%d] bind to adapter %d port %d\n", SocketId, LocalAddrId, *PortPtr));
  r = bind(SocketId, (struct sockaddr *)&sLocalAddr, sizeof(sLocalAddr));
  if (0 != r) {
    ret = E_NOT_OK;
    ASLOG(TCPIPE,
          ("[%d] bind to adapter %d port %d failed: %d\n", SocketId, LocalAddrId, *PortPtr, r));
  } else {
    TcpIp_SetNonBlock(SocketId, TRUE);
  }

  return ret;
}

Std_ReturnType TcpIp_AddToMulticast(TcpIp_SocketIdType SocketId, TcpIp_SockAddrType *ipv4Addr) {
  int r;
  Std_ReturnType ret = E_NOT_OK;
#ifdef USE_LWIP
  ip_addr_t ipaddr;
#endif
  struct ip_mreq mreq;

#ifdef USE_LWIP
  memcpy(&ipaddr.addr, ipv4Addr->addr, 4);
  if (ip_addr_ismulticast(&ipaddr)) {
#endif

#ifndef USE_LWIP
    memcpy(&mreq.imr_multiaddr.s_addr, ipv4Addr->addr, 4);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
#else
  mreq.imr_multiaddr.s_addr = ipaddr.addr;
  mreq.imr_interface.s_addr = netif.ip_addr.addr;
#endif
    r = setsockopt(SocketId, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq));
    ASLOG(TCPIP, ("[%d] multicast on %d.%d.%d.%d:%d\n", SocketId, ipv4Addr->addr[0],
                  ipv4Addr->addr[1], ipv4Addr->addr[2], ipv4Addr->addr[3], ipv4Addr->port));
    if (0 == r) {
      ret = E_OK;
    } else {
      ASLOG(TCPIPE, ("[%d] multicast on %d.%d.%d.%d:%d error: %d\n", SocketId, ipv4Addr->addr[0],
                     ipv4Addr->addr[1], ipv4Addr->addr[2], ipv4Addr->addr[3], ipv4Addr->port, r));
    }
#ifdef USE_LWIP
  }
#endif
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
  } else {
    ASLOG(TCPIPE, ("[%d] listen failed: %d\n", SocketId, r));
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
    TcpIp_SetNonBlock(clientFd, TRUE);

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
                          uint32_t *Length /* InOut */) {
  Std_ReturnType ret = E_OK;
  int nbytes;

  nbytes = recv(SocketId, (char *)BufPtr, *Length, 0);

  *Length = 0;
  if (nbytes > 0) {
    *Length = nbytes;
    ASLOG(TCPIP, ("[%d] recv %d bytes\n", SocketId, nbytes));
  } else if (nbytes < -1) {
    ret = nbytes;
    ASLOG(TCPIPE, ("[%d] recv got error %d\n", SocketId, nbytes));

  } else if (-1 == nbytes) {
#ifndef USE_LWIP
#ifdef _WIN32
    if (10035 != WSAGetLastError())
#else
    if (EAGAIN != errno)
#endif
    {
      ret = E_NOT_OK;
    } else {
      /* Resource temporarily unavailable. */
    }
#endif
  } else {
    /* got nothing */
  }

  return ret;
}

Std_ReturnType TcpIp_RecvFrom(TcpIp_SocketIdType SocketId, TcpIp_SockAddrType *RemoteAddrPtr,
                              uint8_t *BufPtr, uint32_t *Length /* InOut */) {
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
    ASLOG(TCPIPE, ("[%d] recvfrom got error %d\n", SocketId, nbytes));
  } else {
    /* got nothing */
  }

  return ret;
}

Std_ReturnType TcpIp_SendTo(TcpIp_SocketIdType SocketId, const TcpIp_SockAddrType *RemoteAddrPtr,
                            const uint8_t *BufPtr, uint32_t Length) {
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
    ASLOG(TCPIPE, ("[%d] sendto(%d.%d.%d.%d:%d, %d), error is %d\n", SocketId,
                   RemoteAddrPtr->addr[0], RemoteAddrPtr->addr[1], RemoteAddrPtr->addr[2],
                   RemoteAddrPtr->addr[3], RemoteAddrPtr->port, Length, nbytes));
    if (nbytes >= 0) {
      ret = TCPIP_E_NOSPACE;
    } else {
      ret = E_NOT_OK;
    }
  }

  return ret;
}

Std_ReturnType TcpIp_Send(TcpIp_SocketIdType SocketId, const uint8_t *BufPtr, uint32_t Length) {
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
    TcpIp_SetNonBlock(SocketId, TRUE);
    ret = E_OK;
  } else {
    ASLOG(TCPIPE, ("[%d] connect %d.%d.%d.%d:%d failed: %d\n", SocketId, RemoteAddrPtr->addr[0],
                   RemoteAddrPtr->addr[1], RemoteAddrPtr->addr[2], RemoteAddrPtr->addr[3],
                   RemoteAddrPtr->port, r));
  }

  ASLOG(TCPIP,
        ("[%d] connect %d.%d.%d.%d:%d\n", SocketId, RemoteAddrPtr->addr[0], RemoteAddrPtr->addr[1],
         RemoteAddrPtr->addr[2], RemoteAddrPtr->addr[3], RemoteAddrPtr->port));

  return ret;
}

Std_ReturnType TcpIp_SetupAddrFrom(TcpIp_SockAddrType *RemoteAddrPtr, uint32_t ipv4Addr,
                                   uint16_t port) {
  uint32_t u32Addr = htonl(ipv4Addr);
  memcpy(RemoteAddrPtr->addr, &u32Addr, 4);
  RemoteAddrPtr->port = port;
  return E_OK;
}

uint32_t TcpIp_InetAddr(const char *ip) {
  uint32_t u32Addr;
#ifndef USE_LWIP
  u32Addr = inet_addr(ip);
#else
  u32Addr = ipaddr_addr(ip);
#endif
  u32Addr = ntohl(u32Addr);
  return u32Addr;
}

Std_ReturnType TcpIp_GetIpAddr(TcpIp_LocalAddrIdType LocalAddrId, TcpIp_SockAddrType *IpAddrPtr,
                               uint8 *NetmaskPtr, TcpIp_SockAddrType *DefaultRouterPtr) {
#ifdef USE_LWIP
  memcpy(IpAddrPtr->addr, &netif.ip_addr.addr, 4);
  return E_OK;
#else
  uint32_t u32Addr;
  char *ip;

  ip = getenv("AS_LOCAL_IP");
  if (NULL == ip) {
#ifdef _WIN32
    TcpIp_SockAddrType addr;
    static char lIPStr[64];
    static bool lGeted = FALSE;
    static bool lAvaiable = FALSE;
    if (FALSE == lGeted) {
      if (E_OK == TcpIp_GetAdapterAddress(LocalAddrId, &addr)) {
        snprintf(lIPStr, sizeof(lIPStr), "%d.%d.%d.%d", addr.addr[0], addr.addr[1], addr.addr[2],
                 addr.addr[3]);
        ASLOG(WARN, ("env AS_LOCAL_IP not set, using adapter 0, IP is %s\n", lIPStr));
        lAvaiable = TRUE;
      }
      lGeted = TRUE;
    }
    if (lAvaiable) {
      ip = lIPStr;
    } else {
#endif
      ip = "172.18.0.1";
      ASLOG(WARN, ("env AS_LOCAL_IP not set, default %s\n", ip));
#ifdef _WIN32
    }
#endif
  }
  u32Addr = inet_addr(ip);
  memcpy(IpAddrPtr->addr, &u32Addr, 4);
  return E_OK;
#endif
}

Std_ReturnType TcpIp_GetLocalAddr(TcpIp_SocketIdType SocketId, TcpIp_SockAddrType *addr) {
  Std_ReturnType ret = E_NOT_OK;
  int r;
  struct sockaddr_in name;
  socklen_t namelen = sizeof(name);

  r = getsockname(SocketId, (struct sockaddr *)&name, &namelen);
  if (0 == r) {
    addr->port = htons(name.sin_port);
    memcpy(addr->addr, &name.sin_addr, 4);
    ret = E_OK;

    ASLOG(TCPIP, ("[%d] sockname %d.%d.%d.%d:%d\n", SocketId, addr->addr[0], addr->addr[1],
                  addr->addr[2], addr->addr[3], addr->port));
  }

  return ret;
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