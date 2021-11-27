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
#include "DoIP.h"
#include "DoIP_Cfg.h"
#include "Sd.h"
#include "Sd_Cfg.h"
#include "SomeIp.h"
#include "SomeIp_Cfg.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static const SoAd_IfInterfaceType SoAd_DoIP_IF = {
  DoIP_SoAdIfRxIndication,
  NULL,
  DoIP_SoAdIfTxConfirmation,
};

static const SoAd_TpInterfaceType SoAd_DoIP_TP_IF = {
  DoIP_SoAdTpStartOfReception, DoIP_SoAdTpCopyRxData, NULL, NULL, NULL,
};

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
    DOIP_RX_PID_UDP,      /* RxPduId */
    SOAD_SOCKID_DOIP_UDP, /* SoConId */
    sizeof(SoAd_RxBuf),   /* rxBufLen */
    0,                    /* GID */
    TRUE,                 /* isGroup */
  },
  {
    SoAd_RxBuf,           /* rxBuf */
    -1,      /* RxPduId */
    SOAD_SOCKID_DOIP_TCP_SERVER, /* SoConId */
    sizeof(SoAd_RxBuf),   /* rxBufLen */
    1,                    /* GID */
    TRUE,                 /* isGroup */
  },
  {
    SoAd_RxBuf,           /* rxBuf */
    DOIP_RX_PID_TCP0,      /* RxPduId */
    SOAD_SOCKID_DOIP_TCP_APT0, /* SoConId */
    sizeof(SoAd_RxBuf),   /* rxBufLen */
    1,                    /* GID */
    FALSE,                 /* isGroup */
  },
  {
    SoAd_RxBuf,           /* rxBuf */
    DOIP_RX_PID_TCP1,      /* RxPduId */
    SOAD_SOCKID_DOIP_TCP_APT1, /* SoConId */
    sizeof(SoAd_RxBuf),   /* rxBufLen */
    1,                    /* GID */
    FALSE,                 /* isGroup */
  },
  {
    SoAd_RxBuf,           /* rxBuf */
    DOIP_RX_PID_TCP2,      /* RxPduId */
    SOAD_SOCKID_DOIP_TCP_APT2, /* SoConId */
    sizeof(SoAd_RxBuf),   /* rxBufLen */
    1,                    /* GID */
    FALSE,                 /* isGroup */
  },
  {
    SoAd_RxBuf,           /* rxBuf */
    SD_RX_PID_MULTICAST,      /* RxPduId */
    SOAD_SOCKID_SD_MULTICAST, /* SoConId */
    sizeof(SoAd_RxBuf),   /* rxBufLen */
    2,                    /* GID */
    TRUE,                 /* isGroup */
  },
  {
    SoAd_RxBuf,           /* rxBuf */
    SD_RX_PID_UNICAST,      /* RxPduId */
    SOAD_SOCKID_SD_UNICAST, /* SoConId */
    sizeof(SoAd_RxBuf),   /* rxBufLen */
    3,                    /* GID */
    TRUE,                 /* isGroup */
  },
  {
    SoAd_RxBuf,           /* rxBuf */
    SOMEIP_RX_PID_SOMEIP_SERVER0,      /* RxPduId */
    SOAD_SOCKID_SOMEIP_SERVER0, /* SoConId */
    sizeof(SoAd_RxBuf),   /* rxBufLen */
    4,                    /* GID */
    TRUE,                 /* isGroup */
  },
  {
    SoAd_RxBuf,           /* rxBuf */
    SOMEIP_RX_PID_SOMEIP_CLIENT0,      /* RxPduId */
    SOAD_SOCKID_SOMEIP_CLIENT0, /* SoConId */
    sizeof(SoAd_RxBuf),   /* rxBufLen */
    5,                    /* GID */
    TRUE,                 /* isGroup */
  },
};

static SoAd_SocketContextType SoAd_SocketContexts[ARRAY_SIZE(SoAd_SocketConnections)];

static const SoAd_SocketConnectionGroupType SoAd_SocketConnectionGroups[] = {
  {
    /* 0: DOIP_UDP */
    &SoAd_DoIP_IF,     /* Interface */
    "224.244.224.245", /* IpAddress */
    DoIP_SoConModeChg, /* SoConModeChgNotification */
    TCPIP_IPPROTO_UDP, /* ProtocolType */
    -1,                /* SoConId */
    13400,             /* Port */
    1,                 /* numOfConnections */
    FALSE,             /* AutomaticSoConSetup */
    FALSE,             /* IsTP */
    TRUE,              /* IsServer */
  },
  {
    /* 1: DOIP_TCP */
    &SoAd_DoIP_TP_IF,     /* Interface */
    NULL, /* IpAddress */
    DoIP_SoConModeChg, /* SoConModeChgNotification */
    TCPIP_IPPROTO_TCP, /* ProtocolType */
    SOAD_SOCKID_DOIP_TCP_APT0,                /* SoConId */
    13400,             /* Port */
    3,                 /* numOfConnections */
    FALSE,             /* AutomaticSoConSetup */
    TRUE,             /* IsTP */
    TRUE,              /* IsServer */
  },
  {
    /* 2: SD_MULTICAST */
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
    /* 3: SD_UNICAST */
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
    /* 4: SOMEIP_SERVER0 */
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
  {
    /* 5: SOMEIP_CLIENT0 */
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
};

static const SoAd_SoConIdType TxPduIdToSoCondIdMap[] = {
  SOAD_SOCKID_DOIP_UDP,      /* SOAD_TX_PID_DOIP_UDP */
  SOAD_SOCKID_DOIP_TCP_APT0,      /* SOAD_TX_PID_DOIP_TCP_APT0 */
  SOAD_SOCKID_DOIP_TCP_APT1,      /* SOAD_TX_PID_DOIP_TCP_APT1 */
  SOAD_SOCKID_DOIP_TCP_APT2,      /* SOAD_TX_PID_DOIP_TCP_APT2 */
  SOAD_SOCKID_SD_MULTICAST,      /* SOAD_TX_PID_SD_MULTICAST */
  SOAD_SOCKID_SD_UNICAST,      /* SOAD_TX_PID_SD_UNICAST */
  SOAD_SOCKID_SOMEIP_SERVER0,      /* SOAD_TX_PID_SOMEIP_SERVER0 */
  SOAD_SOCKID_SOMEIP_CLIENT0,      /* SOAD_TX_PID_SOMEIP_CLIENT0 */
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
