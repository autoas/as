/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Generated at Sat Nov 27 10:33:30 2021
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "SoAd.h"
#include "SoAd_Cfg.h"
#include "SoAd_Priv.h"
#include "Sd.h"
#include "Sd_Cfg.h"
#include "SomeIp.h"
#include "SomeIp_Cfg.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static const SoAd_IfInterfaceType SoAd_SD_IF = {
  Sd_RxIndication,
  NULL,
  NULL,
};

static const SoAd_IfInterfaceType SoAd_SOMEIP_IF = {
  SomeIp_RxIndication,
  NULL,
  NULL,
};

static uint8_t SoAd_RxBuf[1400];

static const SoAd_SocketConnectionType SoAd_SocketConnections[] = {
  {
    SoAd_RxBuf,           /* rxBuf */
    SD_RX_PID_MULTICAST,      /* RxPduId */
    SOAD_SOCKID_SD_MULTICAST, /* SoConId */
    sizeof(SoAd_RxBuf),   /* rxBufLen */
    0,                    /* GID */
    TRUE,                 /* isGroup */
  },
  {
    SoAd_RxBuf,           /* rxBuf */
    SD_RX_PID_UNICAST,      /* RxPduId */
    SOAD_SOCKID_SD_UNICAST, /* SoConId */
    sizeof(SoAd_RxBuf),   /* rxBufLen */
    1,                    /* GID */
    TRUE,                 /* isGroup */
  },
  {
    SoAd_RxBuf,           /* rxBuf */
    SOMEIP_RX_PID_SOMEIP_CLIENT0,      /* RxPduId */
    SOAD_SOCKID_SOMEIP_CLIENT0, /* SoConId */
    sizeof(SoAd_RxBuf),   /* rxBufLen */
    2,                    /* GID */
    TRUE,                 /* isGroup */
  },
  {
    SoAd_RxBuf,           /* rxBuf */
    SOMEIP_RX_PID_SOMEIP_SERVER0,      /* RxPduId */
    SOAD_SOCKID_SOMEIP_SERVER0, /* SoConId */
    sizeof(SoAd_RxBuf),   /* rxBufLen */
    3,                    /* GID */
    TRUE,                 /* isGroup */
  },
};

static SoAd_SocketContextType SoAd_SocketContexts[ARRAY_SIZE(SoAd_SocketConnections)];

static const SoAd_SocketConnectionGroupType SoAd_SocketConnectionGroups[] = {
  {
    /* 0: SD_MULTICAST */
    &SoAd_SD_IF,     /* Interface */
    "224.244.224.245", /* IpAddress */
    Sd_SoConModeChg, /* SoConModeChgNotification */
    TCPIP_IPPROTO_UDP, /* ProtocolType */
    -1,                /* SoConId */
    30490,             /* Port */
    1,                 /* numOfConnections */
    FALSE,             /* AutomaticSoConSetup */
    FALSE,             /* IsTP */
    TRUE,              /* IsServer */
  },
  {
    /* 1: SD_UNICAST */
    &SoAd_SD_IF,     /* Interface */
    NULL, /* IpAddress */
    Sd_SoConModeChg, /* SoConModeChgNotification */
    TCPIP_IPPROTO_UDP, /* ProtocolType */
    -1,                /* SoConId */
    30490,             /* Port */
    1,                 /* numOfConnections */
    FALSE,             /* AutomaticSoConSetup */
    FALSE,             /* IsTP */
    TRUE,              /* IsServer */
  },
  {
    /* 2: SOMEIP_CLIENT0 */
    &SoAd_SOMEIP_IF,     /* Interface */
    NULL, /* IpAddress */
    SomeIp_SoConModeChg, /* SoConModeChgNotification */
    TCPIP_IPPROTO_UDP, /* ProtocolType */
    -1,                /* SoConId */
    30568,             /* Port */
    1,                 /* numOfConnections */
    FALSE,             /* AutomaticSoConSetup */
    FALSE,             /* IsTP */
    TRUE,              /* IsServer */
  },
  {
    /* 3: SOMEIP_SERVER0 */
    &SoAd_SOMEIP_IF,     /* Interface */
    NULL, /* IpAddress */
    SomeIp_SoConModeChg, /* SoConModeChgNotification */
    TCPIP_IPPROTO_UDP, /* ProtocolType */
    -1,                /* SoConId */
    30560,             /* Port */
    1,                 /* numOfConnections */
    FALSE,             /* AutomaticSoConSetup */
    FALSE,             /* IsTP */
    TRUE,              /* IsServer */
  },
};

static const SoAd_SoConIdType TxPduIdToSoCondIdMap[] = {
  SOAD_SOCKID_SD_MULTICAST,      /* SOAD_TX_PID_SD_MULTICAST */
  SOAD_SOCKID_SD_UNICAST,      /* SOAD_TX_PID_SD_UNICAST */
  SOAD_SOCKID_SOMEIP_CLIENT0,      /* SOAD_TX_PID_SOMEIP_CLIENT0 */
  SOAD_SOCKID_SOMEIP_SERVER0,      /* SOAD_TX_PID_SOMEIP_SERVER0 */
};

const SoAd_ConfigType SoAd_Config = {
  SoAd_SocketConnections,
  SoAd_SocketContexts,
  ARRAY_SIZE(SoAd_SocketConnections),
  TxPduIdToSoCondIdMap,
  ARRAY_SIZE(TxPduIdToSoCondIdMap),
  SoAd_SocketConnectionGroups,
  ARRAY_SIZE(SoAd_SocketConnectionGroups),
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
