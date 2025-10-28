/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 */
/* ================================ [ INCLUDES  ] ============================================== */
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

#include "TcpIp_Priv.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_TCPIP -10
#define AS_LOG_TCPIPI 1
#define AS_LOG_TCPIPE 2

#define NETIF_ADDRS ipaddr, netmask, gw,
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
static void TcpIp_AcInit(void);
static void TcpIp_AcMainFunction(void);
/* ================================ [ DATAS     ] ============================================== */
const struct TcpIp_Config_s TcpIp_Config = {
  TcpIp_AcInit,
  TcpIp_AcMainFunction,
};

#ifdef USE_LWIP
static struct netif netif;
#if LWIP_DHCP
/* dhcp struct for the ethernet netif */
struct dhcp netif_dhcp;
#endif /* LWIP_DHCP */
#endif
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
    OSAL_SleepUs(1000000);
  }

  ASLOG(TCPIPI, ("DHCP IP address: %s\n", ip4addr_ntoa(&netif_dhcp.offered_ip_addr)));
  ASLOG(TCPIPI, ("DHCP Subnet mask: %s\n", ip4addr_ntoa(&netif_dhcp.offered_sn_mask)));
  ASLOG(TCPIPI, ("DHCP Default gateway: %s\n", ip4addr_ntoa(&netif_dhcp.offered_gw_addr)));
#endif
#endif
  sys_sem_signal(init_sem);
}
#endif

static void TcpIp_AcInit(void) {
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
   * ufw allow proto udp from 224.224.224.245/4
   */
#endif
#elif defined(_WIN32)
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

static void TcpIp_AcMainFunction(void) {
#ifdef USE_LWIP
  default_netif_poll();
#endif
}
/* ================================ [ FUNCTIONS ] ============================================== */
