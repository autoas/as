/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Generated at Sat Nov 27 10:33:30 2021
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "DoIP.h"
#include "DoIP_Cfg.h"
#include "DoIP_Priv.h"
#include "SoAd_Cfg.h"
#include "Dcm.h"
/* ================================ [ MACROS    ] ============================================== */
#define DOIP_INITIAL_INACTIVITY_TIME 5000
#define DOIP_GENERAL_INACTIVITY_TIME 5000
#define DOIP_ALIVE_CHECK_RESPONSE_TIMEOUT 50
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern Std_ReturnType DoIP_LocalRoutingActivationAuthenticationCallback(
  boolean *Authentified, const uint8_t *AuthenticationReqData, uint8_t *AuthenticationResData);
extern Std_ReturnType DoIP_LocalRoutingActivationConfirmationCallback(
  boolean *Confirmed, const uint8_t *ConfirmationReqData, uint8_t *ConfirmationResData);
static const DoIP_TesterType DoIP_Testers[];
extern Std_ReturnType DoIP_UserGetEID(uint8_t *Data);
extern Std_ReturnType DoIP_UserGetGID(uint8_t *Data);
/* ================================ [ DATAS     ] ============================================== */
static uint8_t DoIP_TxBuf[1400];
static const DoIP_TargetAddressType DoIp_TargetAddress0[] = {
  {
    0xdead,      /* TargetAddress */
    DCM_P2P_PDU, /* RxPduId */
    DCM_P2P_PDU, /* TxPduId */
  },
};

static const DoIP_RoutingActivationType DoIP_RoutingActivations[] = {
  {
    0xda,                /* Local DCM */
    0,                   /* OEMReqLen */
    0,                   /* OEMResLen */
    DoIp_TargetAddress0, /* TargetAddressRefs */
    ARRAY_SIZE(DoIp_TargetAddress0),
    DoIP_LocalRoutingActivationAuthenticationCallback,
    DoIP_LocalRoutingActivationConfirmationCallback,
    &DoIP_Testers[0],
  },
};

static const DoIP_RoutingActivationType *DoIP_RoutingActivationRefs[] = {
  &DoIP_RoutingActivations[0],
};

static const DoIP_TesterType DoIP_Testers[] = {
  {
    0xbeef, /* TesterSA */
    DoIP_RoutingActivationRefs,
    ARRAY_SIZE(DoIP_RoutingActivationRefs),
  },
};

static DoIP_TesterConnectionContextType DoIP_TesterConnectionContext[DOIP_MAX_TESTER_CONNECTIONS];

static const DoIP_TesterConnectionType DoIP_TesterConnections[DOIP_MAX_TESTER_CONNECTIONS] = {
  {
    &DoIP_TesterConnectionContext[0],
    SOAD_SOCKID_DOIP_TCP_APT0,
    SOAD_TX_PID_DOIP_TCP_APT0,
    DOIP_CONVERT_MS_TO_MAIN_CYCLES(DOIP_INITIAL_INACTIVITY_TIME),
    DOIP_CONVERT_MS_TO_MAIN_CYCLES(DOIP_GENERAL_INACTIVITY_TIME),
    DOIP_CONVERT_MS_TO_MAIN_CYCLES(DOIP_ALIVE_CHECK_RESPONSE_TIMEOUT),
},
  {
    &DoIP_TesterConnectionContext[1],
    SOAD_SOCKID_DOIP_TCP_APT1,
    SOAD_TX_PID_DOIP_TCP_APT1,
    DOIP_CONVERT_MS_TO_MAIN_CYCLES(DOIP_INITIAL_INACTIVITY_TIME),
    DOIP_CONVERT_MS_TO_MAIN_CYCLES(DOIP_GENERAL_INACTIVITY_TIME),
    DOIP_CONVERT_MS_TO_MAIN_CYCLES(DOIP_ALIVE_CHECK_RESPONSE_TIMEOUT),
},
  {
    &DoIP_TesterConnectionContext[2],
    SOAD_SOCKID_DOIP_TCP_APT2,
    SOAD_TX_PID_DOIP_TCP_APT2,
    DOIP_CONVERT_MS_TO_MAIN_CYCLES(DOIP_INITIAL_INACTIVITY_TIME),
    DOIP_CONVERT_MS_TO_MAIN_CYCLES(DOIP_GENERAL_INACTIVITY_TIME),
    DOIP_CONVERT_MS_TO_MAIN_CYCLES(DOIP_ALIVE_CHECK_RESPONSE_TIMEOUT),
},
};

static const DoIP_TcpConnectionType DoIP_TcpConnections[] = {
  {
    SOAD_SOCKID_DOIP_TCP_SERVER, TRUE, /* RequestAddressAssignment */
  },
};

static const DoIP_UdpConnectionType DoIP_UdpConnections[] = {
  {
    SOAD_SOCKID_DOIP_UDP, TRUE, /* RequestAddressAssignment */
  },
};

static DoIP_UdpVehicleAnnouncementConnectionContextType UdpVehicleAnnouncementConnectionContexts[1];

static const DoIP_UdpVehicleAnnouncementConnectionType DoIP_UdpVehicleAnnouncementConnections[] = {
  {
    &UdpVehicleAnnouncementConnectionContexts[0], /* context */
    SOAD_SOCKID_DOIP_UDP,                         /* SoConId */
    SOAD_TX_PID_DOIP_UDP,                         /* SoAdTxPdu */
    DOIP_CONVERT_MS_TO_MAIN_CYCLES(50),           /* InitialVehicleAnnouncementTime */
    DOIP_CONVERT_MS_TO_MAIN_CYCLES(200),          /* VehicleAnnouncementInterval */
    FALSE,                                        /* RequestAddressAssignment */
    3,                                            /* VehicleAnnouncementCount */
  },
};

static DoIP_ChannelContextType DoIP_ChannelContexts[1];

static const DoIP_ChannelConfigType DoIP_ChannelConfigs[1] = {

  {
    &DoIP_ChannelContexts[0], DoIP_TxBuf, sizeof(DoIP_TxBuf), DoIP_Testers,
    ARRAY_SIZE(DoIP_Testers), DoIP_TesterConnections, ARRAY_SIZE(DoIP_TesterConnections),
    DoIP_TcpConnections, ARRAY_SIZE(DoIP_TcpConnections), DoIP_UdpConnections,
   ARRAY_SIZE(DoIP_UdpConnections), DoIP_UdpVehicleAnnouncementConnections,
   ARRAY_SIZE(DoIP_UdpVehicleAnnouncementConnections), Dcm_GetVin, DoIP_UserGetEID,
    DoIP_UserGetGID, 0xdead, /* LogicalAddress */ 0xFF, /* VinInvalidityPattern */
  },
};

static const uint8_t RxPduIdToChannelMap[] = {
  0, /* DOIP_RX_PID_UDP */
  0, /* DOIP_RX_PID_TCP0 */
  0, /* DOIP_RX_PID_TCP1 */
  0, /* DOIP_RX_PID_TCP2 */
};

static const uint8_t RxPduIdToConnectionMap[] = {
  DOIP_INVALID_INDEX, /* DOIP_RX_PID_UDP */
  0,                  /* DOIP_RX_PID_TCP0 */
  1,                  /* DOIP_RX_PID_TCP1 */
  2,                  /* DOIP_RX_PID_TCP2 */
};

static const uint8_t TxPduIdToChannelMap[] = {0, 0, 0, 0, };
const DoIP_ConfigType DoIP_Config = {
  DoIP_ChannelConfigs,
  RxPduIdToChannelMap,
  RxPduIdToConnectionMap,
  TxPduIdToChannelMap,
  ARRAY_SIZE(DoIP_ChannelConfigs),
  ARRAY_SIZE(RxPduIdToChannelMap),
  ARRAY_SIZE(TxPduIdToChannelMap),
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
