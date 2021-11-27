/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Generated at Sat Nov 27 10:33:30 2021
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "SomeIp.h"
#include "SomeIp_Cfg.h"
#include "SomeIp_Priv.h"
#include "SoAd_Cfg.h"
#include "Sd_Cfg.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
Std_ReturnType SomeIp_RequestFnc_client0_method2(
        const uint8_t *reqData, uint32_t reqLen,
        uint8_t *resData, uint32_t *resLen);
Std_ReturnType SomeIp_ResponseFnc_server0_method1(
        const uint8_t *resData, uint32_t resLen);
Std_ReturnType SomeIp_NotifyFnc_server0_event_group1_event0(const uint8_t *evtData, uint32_t evtLen);
/* ================================ [ DATAS     ] ============================================== */
static uint8_t someipBuf[1400];
static const SomeIp_ServerMethodType someIpServerMethods_client0[] = {
  {
    0x424, /* Method ID */
    0, /* interface version */
    SomeIp_RequestFnc_client0_method2, /* requestFnc */
  },
};

static const SomeIp_ServerEventType someIpServerEvents_client0[] = {
  {
    SD_EVENT_HANDLER_CLIENT0_EVENT_GROUP2, /* SD EventGroup Handle ID */
    0xabcd, /* Event ID */
    0, /* interface version */
  },
};

static const SomeIp_ClientMethodType someIpClientMethods_server0[] = {
  {
    0x421, /* Method ID */
    0, /* interface version */
    SomeIp_ResponseFnc_server0_method1, /* responseFnc */
  },
};

static const SomeIp_ClientEventType someIpClientEvents_server0[] = {
  {
    0xbeef, /* Event ID */
    0, /* interface version */
    SomeIp_NotifyFnc_server0_event_group1_event0,
  },
};

static SomeIp_ServerServiceContextType someIpServerServiceContext_client0[1];

static const SomeIp_ServerConnectionType someIpServerServiceConnections_client0[1] = {
  {
    &someIpServerServiceContext_client0[0],
    someipBuf,
    sizeof(someipBuf),
    SOAD_TX_PID_SOMEIP_CLIENT0,
  },
};

static const SomeIp_ServerServiceType someIpServerService_client0 = {
  0xabcd, /* serviceId */
  0x5555, /* clientId */
  someIpServerMethods_client0,
  ARRAY_SIZE(someIpServerMethods_client0),
  someIpServerEvents_client0,
  ARRAY_SIZE(someIpServerEvents_client0),
  someIpServerServiceConnections_client0,
  ARRAY_SIZE(someIpServerServiceConnections_client0),
};

static SomeIp_ClientServiceContextType someIpClientServiceContext_server0;
static const SomeIp_ClientServiceType someIpClientService_server0 = {
  0x1234, /* serviceId */
  0x4444, /* clientId */
  SD_CLIENT_SERVICE_HANDLE_ID_SERVER0, /* sdHandleID */
  someIpClientMethods_server0,
  ARRAY_SIZE(someIpClientMethods_server0),
  someIpClientEvents_server0,
  ARRAY_SIZE(someIpClientEvents_server0),
  &someIpClientServiceContext_server0,
  someipBuf,
  sizeof(someipBuf),
  SOAD_TX_PID_SOMEIP_SERVER0,
};

static const SomeIp_ServiceType SomeIp_Services[] = {
  {
    TRUE,
    &someIpServerService_client0,
  },
  {
    FALSE,
    &someIpClientService_server0,
  },
};

static const uint16_t Sd_PID2ServiceMap[] = {
  SOMEIP_SSID_CLIENT0,
  SOMEIP_CSID_SERVER0,
};

static const uint16_t Sd_PID2ServiceConnectionMap[] = {
  0,
  0,
};

static const uint16_t Sd_TxMethod2ServiceMap[] = {
  SOMEIP_CSID_SERVER0,/* method1 */
  -1
};

static const uint16_t Sd_TxMethod2PerServiceMap[] = {
  0, /* method1 */
  -1
};

static const uint16_t Sd_TxEvent2ServiceMap[] = {
  SOMEIP_SSID_CLIENT0, /* event_group2 event0 */
  -1
};

static const uint16_t Sd_TxEvent2PerServiceMap[] = {
  0, /* event_group2 event0 */
  -1
};

const SomeIp_ConfigType SomeIp_Config = {
  SomeIp_Services,
  ARRAY_SIZE(SomeIp_Services),
  Sd_PID2ServiceMap,
  Sd_PID2ServiceConnectionMap,
  ARRAY_SIZE(Sd_PID2ServiceMap),
  Sd_TxMethod2ServiceMap,
  Sd_TxMethod2PerServiceMap,
  ARRAY_SIZE(Sd_TxMethod2ServiceMap)-1,
  Sd_TxEvent2ServiceMap,
  Sd_TxEvent2PerServiceMap,
  ARRAY_SIZE(Sd_TxEvent2ServiceMap)-1,
};

/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
