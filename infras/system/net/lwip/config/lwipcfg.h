/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef _LWIP_H
#define _LWIP_H
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#ifndef LWIP_IPADDR_CFG_NR
#define LWIP_IPADDR_CFG_NR 0
#endif

#if LWIP_IPADDR_CFG_NR == 0
#define LWIP_MAC_ADDR_BASE                                                                         \
  { 0xde, 0xad, 0xbe, 0xef, 0xfe, 0xed }
#define LWIP_AS_LOCAL_IP_ADDR "172.18.0.200"
#elif LWIP_IPADDR_CFG_NR == 1
#define LWIP_MAC_ADDR_BASE                                                                         \
  { 0xfe, 0xed, 0xde, 0xad, 0xbe, 0xef }
#define LWIP_AS_LOCAL_IP_ADDR "172.18.0.201"
#else
#error invalid LWIP_IPADDR_CFG_NR
#endif

#define LWIP_AS_LOCAL_IP_NETMASK "255.255.255.0"
#define LWIP_AS_LOCAL_IP_GATEWAY "172.18.0.1"

#define LWIP_PORT_INIT_IPADDR(ip)                                                                  \
  do {                                                                                             \
    (ip)->addr = ipaddr_addr(LWIP_AS_LOCAL_IP_ADDR);                                               \
  } while (0)
#define LWIP_PORT_INIT_NETMASK(mask)                                                               \
  do {                                                                                             \
    (mask)->addr = ipaddr_addr(LWIP_AS_LOCAL_IP_NETMASK);                                          \
  } while (0)
#define LWIP_PORT_INIT_GW(gw)                                                                      \
  do {                                                                                             \
    (gw)->addr = ipaddr_addr(LWIP_AS_LOCAL_IP_GATEWAY);                                            \
  } while (0)

#define LWIP_SO_RCVBUF 1 /* enable ioctl FIONREAD */

#define TCPIP_THREAD_STACKSIZE 4096
#define TCPIP_MBOX_SIZE 128
#define TCPIP_THREAD_PRIO 5

#define DEFAULT_RAW_RECVMBOX_SIZE 32
#define DEFAULT_UDP_RECVMBOX_SIZE 128
#define DEFAULT_TCP_RECVMBOX_SIZE 128
#define DEFAULT_ACCEPTMBOX_SIZE 32

#define TCP_MSS 1412
#define TCP_SND_BUF 8096

#ifndef LWIP_DEBUG
#define LWIP_DEBUG 0
#endif

#ifdef USE_FREERTOS
#define LWIP_FREERTOS_CHECK_CORE_LOCKING 1
#endif

#define LWIP_SO_RCVTIMEO 1

#if LWIP_DEBUG == 1
#define LWIP_DBG_MIN_LEVEL LWIP_DBG_LEVEL_ALL
#define LWIP_DBG_TYPES_ON LWIP_DBG_ON
// #define ETHARP_DEBUG                    LWIP_DBG_ON
// #define NETIF_DEBUG                     LWIP_DBG_ON
// #define PBUF_DEBUG                      LWIP_DBG_OFF
// #define API_LIB_DEBUG                   LWIP_DBG_ON
// #define API_MSG_DEBUG                   LWIP_DBG_ON
// #define SOCKETS_DEBUG                   LWIP_DBG_ON
// #define ICMP_DEBUG                      LWIP_DBG_ON
// #define IGMP_DEBUG                      LWIP_DBG_ON
// #define INET_DEBUG                      LWIP_DBG_ON
#define IP_DEBUG LWIP_DBG_ON
// #define IP_REASS_DEBUG                  LWIP_DBG_ON
// #define RAW_DEBUG                       LWIP_DBG_ON
// #define MEM_DEBUG                       LWIP_DBG_ON
// #define MEMP_DEBUG                      LWIP_DBG_ON
// #define SYS_DEBUG                       LWIP_DBG_OFF
// #define TIMERS_DEBUG                    LWIP_DBG_OFF
#define TCP_DEBUG LWIP_DBG_ON
#define TCP_INPUT_DEBUG LWIP_DBG_ON
// #define TCP_FR_DEBUG                    LWIP_DBG_ON
// #define TCP_RTO_DEBUG                   LWIP_DBG_ON
// #define TCP_CWND_DEBUG                  LWIP_DBG_ON
// #define TCP_WND_DEBUG                   LWIP_DBG_ON
// #define TCP_OUTPUT_DEBUG                LWIP_DBG_ON
// #define TCP_RST_DEBUG                   LWIP_DBG_ON
// #define TCP_QLEN_DEBUG                  LWIP_DBG_ON
// #define UDP_DEBUG                       LWIP_DBG_ON
// #define TCPIP_DEBUG                     LWIP_DBG_ON
// #define PPP_DEBUG                       LWIP_DBG_ON
// #define SLIP_DEBUG                      LWIP_DBG_ON
// #define DHCP_DEBUG                      LWIP_DBG_ON
// #define AUTOIP_DEBUG                    LWIP_DBG_ON
// #define SNMP_MSG_DEBUG                  LWIP_DBG_ON
// #define SNMP_MIB_DEBUG                  LWIP_DBG_ON
// #define DNS_DEBUG                       LWIP_DBG_ON
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _LWIP_H */
