# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
from .helper import *


def Gen_MethodRxTxTp(C, service, method):
    C.write('Std_ReturnType SomeIp_%s_%s_OnTpCopyRxData(uint16_t conId, SomeIp_TpMessageType *msg) {\n' % (
        service['name'], method['name']))
    C.write('  Std_ReturnType ret = E_OK;\n')
    C.write('  if ((msg->offset + msg->length) < sizeof(%s_%sTpRxBuf)) {\n' % (
        service['name'], method['name']))
    C.write('    memcpy(&%s_%sTpRxBuf[msg->offset], msg->data, msg->length);\n' % (
        service['name'], method['name']))
    C.write('    if (FALSE == msg->moreSegmentsFlag) {\n')
    C.write('      msg->data = %s_%sTpRxBuf;\n' %
            (service['name'], method['name']))
    C.write('    }\n')
    C.write('  } else {\n')
    C.write('    ret = E_NOT_OK;\n')
    C.write('  }\n')
    C.write('  return ret;\n')
    C.write('}\n\n')
    C.write('Std_ReturnType SomeIp_%s_%s_OnTpCopyTxData(uint16_t conId, SomeIp_TpMessageType *msg) {\n' % (
        service['name'], method['name']))
    C.write('  Std_ReturnType ret = E_OK;\n')
    C.write('  if ((msg->offset + msg->length) < sizeof(%s_%sTpTxBuf)) {\n' % (
        service['name'], method['name']))
    C.write('    memcpy(msg->data, &%s_%sTpTxBuf[msg->offset], msg->length);\n' % (
        service['name'], method['name']))
    C.write('  } else {\n')
    C.write('    ret = E_NOT_OK;\n')
    C.write('  }\n')
    C.write('  return ret;\n')
    C.write('}\n\n')


def Gen_ServerService(service, dir):
    H = open('%s/SS_%s.h' % (dir, service['name']), 'w')
    GenHeader(H)
    H.write('#ifndef _SS_%s_H\n' % (service['name'].upper()))
    H.write('#define _SS_%s_H\n' % (service['name'].upper()))
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write('#include "SomeIp.h"\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    H.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    H.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    H.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    H.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    for method in service['methods']:
        H.write('Std_ReturnType SomeIp_%s_%s_OnRequest(uint16_t conId, SomeIp_MessageType* req, SomeIp_MessageType* res);\n' %
                (service['name'], method['name']))
        H.write('Std_ReturnType SomeIp_%s_%s_OnFireForgot(uint16_t conId, SomeIp_MessageType* res);\n' %
                (service['name'], method['name']))
        H.write('Std_ReturnType SomeIp_%s_%s_OnAsyncRequest(uint16_t conId, SomeIp_MessageType* res);\n' %
                (service['name'], method['name']))
        if method.get('tp', False):
            H.write('Std_ReturnType SomeIp_%s_%s_OnTpCopyRxData(uint16_t conId, SomeIp_TpMessageType *msg);\n' %
                    (service['name'], method['name']))
            H.write('Std_ReturnType SomeIp_%s_%s_OnTpCopyTxData(uint16_t conId, SomeIp_TpMessageType *msg);\n' %
                    (service['name'], method['name']))
    H.write('#endif /* _SS_%s_H */\n' % (service['name'].upper()))
    H.close()
    C = open('%s/SS_%s.c' % (dir, service['name']), 'w')
    GenHeader(C)
    C.write('/* TODO: This is default demo code */\n')
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#include "SS_%s.h"\n' % (service['name']))
    C.write('#include "Std_Debug.h"\n')
    C.write('#include <string.h>\n')
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    C.write('#define AS_LOG_%s 1\n' % (service['name'].upper()))
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    for method in service['methods']:
        if method.get('tp', False):
            C.write('static uint8_t %s_%sTpRxBuf[%d];\n' % (
                service['name'], method['name'], method.get('tpRxSize', 1*1024*1024)))
            C.write('static uint8_t %s_%sTpTxBuf[%d];\n' % (
                service['name'], method['name'], method.get('tpTxSize', 1*1024*1024)))
    C.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    for method in service['methods']:
        C.write('Std_ReturnType SomeIp_%s_%s_OnRequest(uint16_t conId, SomeIp_MessageType* req, SomeIp_MessageType* res) {\n' %
                (service['name'], method['name']))
        C.write('  uint32_t i;\n')
        if method.get('tp', False):
            C.write('  if (res->length < req->length) {\n')
            C.write('    res->data = %s_%sTpTxBuf;\n' %
                    (service['name'], method['name']))
            C.write('  }\n')
        C.write('  for (i = 0; i < req->length; i++) {\n')
        C.write('    res->data[req->length - 1 - i] = req->data[i];\n')
        C.write('  }\n')
        C.write('  res->length = req->length;\n')
        C.write(
            '  ASLOG(%s, ("%s OnRequest: len=%%d, data=[%%02X %%02X %%02X %%02X ...]\\n", req->length,\n' % (service['name'].upper(), method['name']))
        C.write(
            '                 req->data[0], req->data[1], req->data[2], req->data[3]));\n')
        C.write('  return E_OK;\n')
        C.write('}\n\n')
        C.write('Std_ReturnType SomeIp_%s_%s_OnFireForgot(uint16_t conId,SomeIp_MessageType* req) {\n' %
                (service['name'], method['name']))
        C.write(
            '  ASLOG(%s, ("%s OnFireForgot: len=%%d, data=[%%02X %%02X %%02X %%02X ...]\\n", req->length,\n' % (service['name'].upper(), method['name']))
        C.write(
            '                 req->data[0], req->data[1], req->data[2], req->data[3]));\n')
        C.write('  return E_OK;\n')
        C.write('}\n\n')
        C.write('Std_ReturnType SomeIp_%s_%s_OnAsyncRequest(uint16_t conId, SomeIp_MessageType* res) {\n' %
                (service['name'], method['name']))
        C.write('  return E_OK;\n')
        C.write('}\n\n')
        if method.get('tp', False):
            Gen_MethodRxTxTp(C, service, method)
    C.close()


def Gen_ClientService(service, dir):
    H = open('%s/CS_%s.h' % (dir, service['name']), 'w')
    GenHeader(H)
    H.write('#ifndef _CS_%s_H\n' % (service['name'].upper()))
    H.write('#define _CS_%s_H\n' % (service['name'].upper()))
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write('#include "SomeIp.h"\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    H.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    H.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    H.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    H.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    H.write('void SomeIp_%s_OnAvailability(boolean isAvailable);\n' %
            (service['name']))
    for method in service['methods']:
        H.write('Std_ReturnType SomeIp_%s_%s_OnResponse(SomeIp_MessageType* res);\n' %
                (service['name'], method['name']))
        if method.get('tp', False):
            H.write('Std_ReturnType SomeIp_%s_%s_OnTpCopyRxData(uint16_t conId, SomeIp_TpMessageType *msg);\n' %
                    (service['name'], method['name']))
            H.write('Std_ReturnType SomeIp_%s_%s_OnTpCopyTxData(uint16_t conId, SomeIp_TpMessageType *msg);\n' %
                    (service['name'], method['name']))
    for egroup in service['event-groups']:
        for event in egroup['events']:
            H.write('Std_ReturnType SomeIp_%s_%s_%s_OnNotification(SomeIp_MessageType* evt);\n' % (
                service['name'], egroup['name'], event['name']
            ))
    H.write('#endif /* _CS_%s_H */\n' % (service['name'].upper()))
    H.close()
    C = open('%s/CS_%s.c' % (dir, service['name']), 'w')
    GenHeader(C)
    C.write('/* TODO: This is default demo code */\n')
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#include "CS_%s.h"\n' % (service['name']))
    C.write('#include "Std_Debug.h"\n')
    C.write('#include "SomeIp_Cfg.h"\n')
    C.write('#include <string.h>\n')
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    C.write('#define AS_LOG_%s 1\n' % (service['name'].upper()))
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    for method in service['methods']:
        if method.get('tp', False):
            C.write('static uint8_t %s_%sTpRxBuf[%d];\n' % (
                service['name'], method['name'], method.get('tpRxSize', 1*1024*1024)))
            C.write('static uint8_t %s_%sTpTxBuf[%d];\n' % (
                service['name'], method['name'], method.get('tpTxSize', 1*1024*1024)))
    C.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    C.write('void SomeIp_%s_OnAvailability(boolean isAvailable) {\n' %
            (service['name']))
    C.write(
        '  ASLOG(%s, ("%%s\\n", isAvailable?"online":"offline"));\n' % (service['name'].upper()))
    C.write('}\n\n')
    for method in service['methods']:
        C.write('Std_ReturnType %s_%s_request(uint8_t *data, uint32_t length) {\n' % (
            service['name'], method['name']))
        if method.get('tp', False):
            C.write('  static uint8_t counter = 0;\n')
            C.write('  uint32_t i;\n')
            C.write('  counter++;\n')
            C.write(
                '  for (i = 0; i < sizeof(%s_%sTpTxBuf); i++) {\n' % (service['name'], method['name']))
            C.write('    %s_%sTpTxBuf[i] = (uint8_t)(counter + i);\n' %
                    (service['name'], method['name']))
            C.write('  }\n')
            C.write('  data = %s_%sTpTxBuf;\n' %
                    (service['name'], method['name']))
            C.write('  length = 5000;\n')
        C.write('  return SomeIp_Request(SOMEIP_TX_METHOD_%s_%s, data, length);\n' % (
            service['name'].upper(), method['name'].upper()))
        C.write('}\n\n')
        C.write('Std_ReturnType SomeIp_%s_%s_OnResponse(SomeIp_MessageType* res) {\n' %
                (service['name'], method['name']))
        C.write(
            '  ASLOG(%s, ("%s OnResponse: len=%%d, data=[%%02X %%02X %%02X %%02X ...]\\n",res->length,\n' % (service['name'].upper(), method['name']))
        C.write(
            '                 res->data[0], res->data[1], res->data[2], res->data[3]));\n')
        C.write('  return E_OK;\n')
        C.write('}\n\n')
        if method.get('tp', False):
            Gen_MethodRxTxTp(C, service, method)
    for egroup in service['event-groups']:
        for event in egroup['events']:
            C.write('Std_ReturnType SomeIp_%s_%s_%s_OnNotification(SomeIp_MessageType* evt) {\n' % (
                service['name'], egroup['name'], event['name']
            ))
            C.write(
                '  ASLOG(%s, ("%s %s OnNotification: len=%%d, data=[%%02X %%02X %%02X %%02X ...]\\n", evt->length,\n' % (service['name'].upper(), egroup['name'], event['name']))
            C.write(
                '                 evt->data[0], evt->data[1], evt->data[2], evt->data[3]));\n')
            C.write('  return E_OK;\n')
            C.write('}\n\n')
    C.close()


def Gen_SD(cfg, dir):
    H = open('%s/Sd_Cfg.h' % (dir), 'w')
    GenHeader(H)
    H.write('#ifndef _SD_CFG_H\n')
    H.write('#define _SD_CFG_H\n')
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    H.write('#define SD_RX_PID_MULTICAST 0\n')
    H.write('#define SD_RX_PID_UNICAST 0\n\n')
    for ID, service in enumerate(cfg['servers']):
        H.write('#define SD_SERVER_SERVICE_HANDLE_ID_%s %s\n' %
                (service['name'].upper(), ID))
    for ID, service in enumerate(cfg['clients']):
        H.write('#define SD_CLIENT_SERVICE_HANDLE_ID_%s %s\n' %
                (service['name'].upper(), ID))
    H.write('\n')
    ID = 0
    for service in cfg['servers']:
        if 'event-groups' not in service:
            continue
        for ge in service['event-groups']:
            H.write('#define SD_EVENT_HANDLER_%s_%s %s\n' %
                    (service['name'].upper(), ge['name'].upper(), ID))
            ID += 1
    ID = 0
    for service in cfg['clients']:
        if 'event-groups' not in service:
            continue
        for ge in service['event-groups']:
            H.write('#define SD_CONSUMED_EVENT_GROUP_%s_%s %s\n' %
                    (service['name'].upper(), ge['name'].upper(), ID))
            ID += 1
    H.write('\n#define SD_MAIN_FUNCTION_PERIOD 10\n')
    H.write('#define SD_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n')
    H.write('  ((x + SD_MAIN_FUNCTION_PERIOD - 1) / SD_MAIN_FUNCTION_PERIOD)\n')
    H.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    H.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    H.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    H.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    H.write('#endif /* _SD_CFG_H */\n')
    H.close()

    C = open('%s/Sd_Cfg.c' % (dir), 'w')
    GenHeader(C)
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#include "Sd.h"\n')
    C.write('#include "Sd_Cfg.h"\n')
    C.write('#include "Sd_Priv.h"\n')
    C.write('#include "SoAd_Cfg.h"\n')
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    C.write(
        'boolean Sd_ServerService0_CRMC(PduIdType pduID, uint8_t type, uint16_t serviceID,\n')
    C.write('                               uint16_t instanceID, uint8_t majorVersion, uint32_t minorVersion,\n')
    C.write('                               const Sd_ConfigOptionStringType *receivedConfigOptionPtrArray,\n')
    C.write('                               const Sd_ConfigOptionStringType *configuredConfigOptionPtrArray);\n')
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    C.write('static Sd_ServerTimerType Sd_ServerTimerDefault = {\n')
    C.write('  SD_CONVERT_MS_TO_MAIN_CYCLES(100),  /* InitialOfferDelayMax */\n')
    C.write('  SD_CONVERT_MS_TO_MAIN_CYCLES(10),   /* InitialOfferDelayMin */\n')
    C.write(
        '  SD_CONVERT_MS_TO_MAIN_CYCLES(200),  /* InitialOfferRepetitionBaseDelay */\n')
    C.write('  3,                                  /* InitialOfferRepetitionsMax */\n')
    C.write('  SD_CONVERT_MS_TO_MAIN_CYCLES(3000), /* OfferCyclicDelay */\n')
    C.write('  SD_CONVERT_MS_TO_MAIN_CYCLES(1500), /* RequestResponseMaxDelay */\n')
    C.write('  SD_CONVERT_MS_TO_MAIN_CYCLES(0),    /* RequestResponseMinDelay */\n')
    C.write('  DEFAULT_TTL,\n')
    C.write('};\n\n')

    C.write('static Sd_ClientTimerType Sd_ClientTimerDefault = {\n')
    C.write('  SD_CONVERT_MS_TO_MAIN_CYCLES(100),  /* InitialFindDelayMax */\n')
    C.write('  SD_CONVERT_MS_TO_MAIN_CYCLES(10),   /* InitialFindDelayMin */\n')
    C.write(
        '  SD_CONVERT_MS_TO_MAIN_CYCLES(200),  /* InitialFindRepetitionsBaseDelay */\n')
    C.write('  3,                                  /* InitialFindRepetitionsMax */\n')
    C.write('  SD_CONVERT_MS_TO_MAIN_CYCLES(1500), /* RequestResponseMaxDelay */\n')
    C.write('  SD_CONVERT_MS_TO_MAIN_CYCLES(0),    /* RequestResponseMinDelay */\n')
    C.write('  DEFAULT_TTL,\n')
    C.write('};\n\n')
    for service in cfg['servers']:
        if 'event-groups' not in service:
            continue
        C.write('static Sd_EventHandlerContextType Sd_EventHandlerContext_%s[%d];\n' % (
            service['name'], len(service['event-groups'])))
        for ge in service['event-groups']:
            C.write('static Sd_EventHandlerSubscriberType Sd_EventHandlerSubscriber_%s_%s[3];\n' % (
                service['name'], ge['name']))
        C.write('static const Sd_EventHandlerType Sd_EventHandlers_%s[] = {\n' % (
            service['name']))
        for ID, ge in enumerate(service['event-groups']):
            C.write('  {\n')
            C.write('    SD_EVENT_HANDLER_%s_%s, /* HandleId */\n' %
                    (service['name'].upper(), ge['name'].upper()))
            C.write('    %s, /* EventGroupId */\n' % (ge['groupId']))
            C.write('    0, /* MulticastThreshold */\n')
            C.write('    &Sd_EventHandlerContext_%s[%d],\n' % (
                service['name'], ID))
            C.write('    Sd_EventHandlerSubscriber_%s_%s,\n' % (
                service['name'], ge['name']))
            C.write('    ARRAY_SIZE(Sd_EventHandlerSubscriber_%s_%s),\n' % (
                service['name'], ge['name']))
            C.write('  },\n')
        C.write('};\n\n')
    for service in cfg['clients']:
        if 'event-groups' not in service:
            continue
        C.write('static Sd_ConsumedEventGroupContextType Sd_ConsumedEventGroupContext_%s[%d];\n' % (
            service['name'], len(service['event-groups'])))
        C.write('static const Sd_ConsumedEventGroupType Sd_ConsumedEventGroups_%s[] = {\n' % (
            service['name']))
        for ID, ge in enumerate(service['event-groups']):
            C.write('  {\n')
            C.write('    FALSE, /* AutoRequire */\n')
            C.write('    SD_CONSUMED_EVENT_GROUP_%s_%s, /* HandleId */\n' %
                    (service['name'].upper(), ge['name'].upper()))
            C.write('    %s, /* EventGroupId */\n' % (ge['groupId']))
            C.write('    &Sd_ConsumedEventGroupContext_%s[%d],\n' % (
                service['name'], ID))
            C.write('  },\n')
        C.write('};\n\n')
    C.write('static Sd_ServerServiceContextType Sd_ServerService_Contexts[%s];\n\n' % (
        len(cfg['servers'])))
    C.write('static const Sd_ServerServiceType Sd_ServerServices[] = {\n')
    for ID, service in enumerate(cfg['servers']):
        C.write('  {\n')
        C.write('    FALSE,                           /* AutoAvailable */\n')
        C.write('    SD_SERVER_SERVICE_HANDLE_ID_%s,  /* HandleId */\n' %
                (service['name'].upper()))
        C.write('    %s,                         /* ServiceId */\n' %
                (service['service']))
        C.write('    %s,                         /* InstanceId */\n' %
                (service['instance']))
        C.write('    0,                              /* MajorVersion */\n')
        C.write('    0,                              /* MinorVersion */\n')
        if ('unreliable' in service):
            C.write('    SOAD_SOCKID_SOMEIP_%s,     /* SoConId */\n' %
                    (service['name'].upper()))
            C.write('    TCPIP_IPPROTO_UDP,              /* ProtocolType */\n')
        else:
            C.write('    SOAD_SOCKID_SOMEIP_%s_SERVER,     /* SoConId */\n' %
                    (service['name'].upper()))
            C.write('    TCPIP_IPPROTO_TCP,              /* ProtocolType */\n')
        C.write(
            '    Sd_ServerService0_CRMC,         /* CapabilityRecordMatchCalloutRef */\n')
        C.write('    &Sd_ServerTimerDefault,\n')
        C.write('    &Sd_ServerService_Contexts[%s],\n' % (ID))
        C.write('    0, /* InstanceIndex */\n')
        if 'event-groups' not in service:
            C.write('    NULL,\n    0,\n')
        else:
            C.write('    Sd_EventHandlers_%s,\n' % (service['name']))
            C.write('    ARRAY_SIZE(Sd_EventHandlers_%s),\n' %
                    (service['name']))
        C.write('  },\n')
    C.write('};\n\n')
    C.write('static Sd_ClientServiceContextType Sd_ClientService_Contexts[%s];\n\n' % (
        len(cfg['clients'])))
    C.write('static const Sd_ClientServiceType Sd_ClientServices[] = {\n')
    for ID, service in enumerate(cfg['clients']):
        C.write('  {\n')
        C.write('    FALSE,                           /* AutoRequire */\n')
        C.write('    SD_CLIENT_SERVICE_HANDLE_ID_%s,  /* HandleId */\n' %
                (service['name'].upper()))
        C.write('    %s,                         /* ServiceId */\n' %
                (service['service']))
        C.write('    %s,                         /* InstanceId */\n' %
                (service['instance']))
        C.write('    0,                              /* MajorVersion */\n')
        C.write('    0,                              /* MinorVersion */\n')
        C.write('    SOAD_SOCKID_SOMEIP_%s, /* SoConId */\n' %
                (service['name'].upper()))
        if ('unreliable' in service):
            C.write('    TCPIP_IPPROTO_UDP,              /* ProtocolType */\n')
        else:
            C.write('    TCPIP_IPPROTO_TCP,              /* ProtocolType */\n')
        C.write(
            '    NULL,                           /* CapabilityRecordMatchCalloutRef */\n')
        C.write('    &Sd_ClientTimerDefault,\n')
        C.write('    &Sd_ClientService_Contexts[%s],\n' % (ID))
        C.write('    0, /* InstanceIndex */\n')
        if 'event-groups' not in service:
            C.write('    NULL,\n    0,\n')
        else:
            C.write('    Sd_ConsumedEventGroups_%s,\n' % (service['name']))
            C.write('    ARRAY_SIZE(Sd_ConsumedEventGroups_%s),\n' %
                    (service['name']))
        C.write('  },\n')
    C.write('};\n\n')
    C.write('static uint8_t sd_buffer[1400];\n')
    C.write('static Sd_InstanceContextType sd_context;\n')
    C.write('static const Sd_InstanceType Sd_Instances[] = {\n')
    C.write('  {\n')
    C.write('    "%s",                             /* Hostname */\n' %
            (cfg['SD']['hostname']))
    C.write(
        '    SD_CONVERT_MS_TO_MAIN_CYCLES(1000), /* SubscribeEventgroupRetryDelay */\n')
    C.write(
        '    3,                                  /* SubscribeEventgroupRetryMax */\n')
    C.write('    {\n')
    C.write('      SD_RX_PID_MULTICAST,      /* RxPduId */\n')
    C.write('      SOAD_SOCKID_SD_MULTICAST, /* SoConId */\n')
    C.write('    },                          /* MulticastRxPdu */\n')
    C.write('    {\n')
    C.write('      SD_RX_PID_UNICAST,      /* RxPduId */\n')
    C.write('      SOAD_SOCKID_SD_UNICAST, /* SoConId */\n')
    C.write('    },                        /* UnicastRxPdu */\n')
    C.write('    {\n')
    C.write('      SOAD_TX_PID_SD_MULTICAST,    /* MulticastTxPduId */\n')
    C.write('      SOAD_TX_PID_SD_UNICAST,      /* UnicastTxPduId */\n')
    C.write('    },                             /* TxPdu */\n')
    C.write('    Sd_ServerServices,             /* ServerServices */\n')
    C.write('    ARRAY_SIZE(Sd_ServerServices), /* numOfServerServices */\n')
    C.write('    Sd_ClientServices,             /* ClientServices */\n')
    C.write('    ARRAY_SIZE(Sd_ClientServices), /* numOfClientServices */\n')
    C.write('    sd_buffer,                     /*buffer */\n')
    C.write('    sizeof(sd_buffer),\n')
    C.write('    &sd_context,\n')
    C.write('  },\n')
    C.write('};\n\n')
    C.write('static const Sd_ServerServiceType* Sd_ServerServicesMap[] = {\n')
    for ID, service in enumerate(cfg['servers']):
        C.write('  &Sd_ServerServices[%s],\n' % (ID))
    C.write('};\n\n')
    C.write('static const Sd_ClientServiceType* Sd_ClientServicesMap[] = {\n')
    for ID, service in enumerate(cfg['clients']):
        C.write('  &Sd_ClientServices[%s],\n' % (ID))
    C.write('};\n\n')
    C.write('static const uint16_t Sd_EventHandlersMap[] = {\n')
    for service in cfg['servers']:
        if 'event-groups' not in service:
            continue
        for ge in service['event-groups']:
            C.write('  SD_SERVER_SERVICE_HANDLE_ID_%s,\n' %
                    (service['name'].upper()))
    C.write('  -1,\n};\n\n')
    C.write(
        'static const uint16_t Sd_PerServiceEventHandlerMap[] = {\n')
    for service in cfg['servers']:
        if 'event-groups' not in service:
            continue
        for id, ge in enumerate(service['event-groups']):
            C.write('  %s,\n' % (id))
    C.write('  -1,\n};\n\n')
    C.write('static const uint16_t Sd_ConsumedEventGroupsMap[] = {\n')
    for service in cfg['clients']:
        if 'event-groups' not in service:
            continue
        for ge in service['event-groups']:
            C.write('  SD_CLIENT_SERVICE_HANDLE_ID_%s,\n' %
                    (service['name'].upper()))
    C.write('  -1,\n};\n\n')
    C.write(
        'static const uint16_t Sd_PerServiceConsumedEventGroupsMap[] = {\n')
    for service in cfg['clients']:
        if 'event-groups' not in service:
            continue
        for id, ge in enumerate(service['event-groups']):
            C.write('  %s,\n' % (id))
    C.write('  -1,\n};\n\n')
    C.write('const Sd_ConfigType Sd_Config = {\n')
    C.write('  Sd_Instances,\n')
    C.write('  ARRAY_SIZE(Sd_Instances),\n')
    C.write('  Sd_ServerServicesMap,\n')
    C.write('  ARRAY_SIZE(Sd_ServerServicesMap),\n')
    C.write('  Sd_ClientServicesMap,\n')
    C.write('  ARRAY_SIZE(Sd_ClientServicesMap),\n')
    C.write('  Sd_EventHandlersMap,\n')
    C.write('  Sd_PerServiceEventHandlerMap,\n')
    C.write('  ARRAY_SIZE(Sd_EventHandlersMap)-1,\n')
    C.write('  Sd_ConsumedEventGroupsMap,\n')
    C.write('  Sd_PerServiceConsumedEventGroupsMap,\n')
    C.write('  ARRAY_SIZE(Sd_ConsumedEventGroupsMap)-1,\n')
    C.write('};\n')
    C.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    C.close()


def Gen_SOMEIP(cfg, dir):
    H = open('%s/SomeIp_Cfg.h' % (dir), 'w')
    GenHeader(H)
    H.write('#ifndef _SOMEIP_CFG_H\n')
    H.write('#define _SOMEIP_CFG_H\n')
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    ID = 0
    for service in cfg['servers']:
        H.write('#define SOMEIP_SSID_%s %s\n' % (service['name'].upper(), ID))
        ID += 1
    for service in cfg['clients']:
        H.write('#define SOMEIP_CSID_%s %s\n' % (service['name'].upper(), ID))
        ID += 1
    H.write('\n')
    ID = 0
    for service in cfg['servers']:
        if 'reliable' in service:
            listen = service['listen'] if 'listen' in service else 3
            for i in range(listen):
                H.write('#define SOMEIP_RX_PID_SOMEIP_%s%s %s\n' %
                        (service['name'].upper(), i, ID))
                H.write('#define SOMEIP_TX_PID_SOMEIP_%s%s %s\n' %
                        (service['name'].upper(), i, ID))
                ID += 1
        else:
            H.write('#define SOMEIP_RX_PID_SOMEIP_%s %s\n' %
                    (service['name'].upper(), ID))
            H.write('#define SOMEIP_TX_PID_SOMEIP_%s %s\n' %
                    (service['name'].upper(), ID))
            ID += 1
    for service in cfg['clients']:
        H.write('#define SOMEIP_RX_PID_SOMEIP_%s %s\n' %
                (service['name'].upper(), ID))
        H.write('#define SOMEIP_TX_PID_SOMEIP_%s %s\n' %
                (service['name'].upper(), ID))
        ID += 1
    H.write('\n')
    ID = 0
    for service in cfg['servers']:
        if 'methods' not in service:
            continue
        for method in service['methods']:
            H.write('#define SOMEIP_RX_METHOD_%s_%s %s\n' %
                    (service['name'].upper(), method['name'].upper(), ID))
    H.write('\n')
    ID = 0
    for service in cfg['clients']:
        if 'methods' not in service:
            continue
        for method in service['methods']:
            H.write('#define SOMEIP_TX_METHOD_%s_%s %s\n' %
                    (service['name'].upper(), method['name'].upper(), ID))
    H.write('\n')
    ID = 0
    for service in cfg['servers']:
        if 'event-groups' not in service:
            continue
        for egroup in service['event-groups']:
            for event in egroup['events']:
                H.write('#define SOMEIP_TX_EVT_%s_%s_%s %s\n' %
                        (service['name'].upper(), egroup['name'].upper(), event['name'].upper(), ID))
                ID += 1
    H.write('\n')
    ID = 0
    for service in cfg['clients']:
        if 'event-groups' not in service:
            continue
        for egroup in service['event-groups']:
            for event in egroup['events']:
                H.write('#define SOMEIP_RX_EVT_%s_%s_%s %s\n' %
                        (service['name'].upper(), egroup['name'].upper(), event['name'].upper(), ID))
                ID += 1
    H.write('\n#define SOMEIP_MAIN_FUNCTION_PERIOD 10\n')
    H.write('#define SOMEIP_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n')
    H.write('  ((x + SOMEIP_MAIN_FUNCTION_PERIOD - 1) / SOMEIP_MAIN_FUNCTION_PERIOD)\n')
    H.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    H.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    H.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    H.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    H.write('#endif /* _SOMEIP_CFG_H */\n')
    H.close()

    C = open('%s/SomeIp_Cfg.c' % (dir), 'w')
    GenHeader(C)
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#include "SomeIp.h"\n')
    C.write('#include "SomeIp_Cfg.h"\n')
    C.write('#include "SomeIp_Priv.h"\n')
    C.write('#include "SoAd_Cfg.h"\n')
    C.write('#include "Sd_Cfg.h"\n')
    for service in cfg['servers']:
        C.write('#include "SS_%s.h"\n' % (service['name']))
    for service in cfg['clients']:
        C.write('#include "CS_%s.h"\n' % (service['name']))
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    for service in cfg['servers']:
        Gen_ServerService(service, dir)
    for service in cfg['clients']:
        Gen_ClientService(service, dir)
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    for service in cfg['servers']:
        if 'methods' not in service:
            continue
        C.write('static const SomeIp_ServerMethodType someIpServerMethods_%s[] = {\n' % (
            service['name']))
        for method in service['methods']:
            C.write('  {\n')
            C.write('    %s, /* Method ID */\n' % (method['methodId']))
            C.write('    %s, /* interface version */\n' % (method['version']))
            C.write('    SomeIp_%s_%s_OnRequest,\n' %
                    (service['name'], method['name']))
            C.write('    SomeIp_%s_%s_OnFireForgot,\n' %
                    (service['name'], method['name']))
            C.write('    SomeIp_%s_%s_OnAsyncRequest,\n' %
                    (service['name'], method['name']))
            if method.get('tp', False):
                C.write('    SomeIp_%s_%s_OnTpCopyRxData,\n' %
                        (service['name'], method['name']))
                C.write('    SomeIp_%s_%s_OnTpCopyTxData,\n' %
                        (service['name'], method['name']))
            else:
                C.write('    NULL,\n')
                C.write('    NULL,\n')
            resMaxLen = method.get('resMaxLen', 512)
            if method.get('tp', False):
                resMaxLen = 1404
            C.write('    %s /* resMaxLen */\n' % (resMaxLen))
            C.write('  },\n')
        C.write("};\n\n")
    for service in cfg['servers']:
        if 'event-groups' not in service:
            continue
        C.write('static const SomeIp_ServerEventType someIpServerEvents_%s[] = {\n' % (
            service['name']))
        for egroup in service['event-groups']:
            for event in egroup['events']:
                C.write('  {\n')
                C.write('    SD_EVENT_HANDLER_%s_%s, /* SD EventGroup Handle ID */\n' %
                        (service['name'].upper(), egroup['name'].upper()))
                C.write('    %s, /* Event ID */\n' % (event['eventId']))
                C.write('    %s, /* interface version */\n' %
                        (event['version']))
                C.write('  },\n')
        C.write("};\n\n")
    for service in cfg['clients']:
        if 'methods' not in service:
            continue
        C.write('static const SomeIp_ClientMethodType someIpClientMethods_%s[] = {\n' % (
            service['name']))
        for method in service['methods']:
            C.write('  {\n')
            C.write('    %s, /* Method ID */\n' % (method['methodId']))
            C.write('    %s, /* interface version */\n' % (method['version']))
            C.write('    SomeIp_%s_%s_OnResponse,\n' %
                    (service['name'], method['name']))
            if method.get('tp', False):
                C.write('    SomeIp_%s_%s_OnTpCopyRxData,\n' %
                        (service['name'], method['name']))
                C.write('    SomeIp_%s_%s_OnTpCopyTxData,\n' %
                        (service['name'], method['name']))
            else:
                C.write('    NULL,\n')
                C.write('    NULL,\n')
            C.write('  },\n')
        C.write("};\n\n")
    for service in cfg['clients']:
        if 'event-groups' not in service:
            continue
        C.write('static const SomeIp_ClientEventType someIpClientEvents_%s[] = {\n' % (
            service['name']))
        for egroup in service['event-groups']:
            for event in egroup['events']:
                C.write('  {\n')
                C.write('    %s, /* Event ID */\n' % (event['eventId']))
                C.write('    %s, /* interface version */\n' %
                        (event['version']))
                C.write('    SomeIp_%s_%s_%s_OnNotification,\n' % (
                    service['name'], egroup['name'], event['name']))
                C.write('  },\n')
        C.write("};\n\n")
    for service in cfg['servers']:
        if 'reliable' in service:
            numOfConnections = service['listen'] if 'listen' in service else 3
            C.write('static SomeIp_TcpBufferType someIpTcpBuffer_%s[%s];\n\n' % (
                service['name'], numOfConnections))
        else:
            numOfConnections = 1
        C.write('static SomeIp_ServerServiceContextType someIpServerServiceContext_%s[%s];\n\n' % (
            service['name'], numOfConnections))
        C.write('static const SomeIp_ServerConnectionType someIpServerServiceConnections_%s[%s] = {\n' % (
            service['name'], numOfConnections))
        for i in range(numOfConnections):
            C.write('  {\n')
            C.write('    &someIpServerServiceContext_%s[%s],\n' %
                    (service['name'], i))
            if 'reliable' in service:
                C.write('    SOAD_TX_PID_SOMEIP_%s_APT%s,\n' %
                        (service['name'].upper(), i))
                C.write('    SOAD_SOCKID_SOMEIP_%s_APT%s,\n' %
                        (service['name'].upper(), i))
                C.write('    &someIpTcpBuffer_%s[%s],\n' % (
                    service['name'], i))
            else:
                C.write('    SOAD_TX_PID_SOMEIP_%s,\n' %
                        (service['name'].upper()))
                C.write('    SOAD_SOCKID_SOMEIP_%s,\n' %
                        (service['name'].upper()))
                C.write('    NULL\n')
            C.write('  },\n')
        C.write('};\n\n')
        C.write('static const SomeIp_ServerServiceType someIpServerService_%s = {\n' % (
            service['name']))
        C.write('  %s, /* serviceId */\n' % (service['service']))
        C.write('  %s, /* clientId */\n' % (service['clientId']))
        if 'methods' not in service:
            C.write('  NULL,\n  0,\n')
        else:
            C.write('  someIpServerMethods_%s,\n' % (service['name']))
            C.write('  ARRAY_SIZE(someIpServerMethods_%s),\n' %
                    (service['name']))
        if 'event-groups' not in service:
            C.write('  NULL,\n  0,\n')
        else:
            C.write('  someIpServerEvents_%s,\n' % (service['name']))
            C.write('  ARRAY_SIZE(someIpServerEvents_%s),\n' %
                    (service['name']))
        C.write('  someIpServerServiceConnections_%s,\n' % (service['name']))
        C.write('  ARRAY_SIZE(someIpServerServiceConnections_%s),\n' %
                (service['name']))
        C.write('};\n\n')
    for service in cfg['clients']:
        if 'reliable' in service:
            C.write('static SomeIp_TcpBufferType someIpTcpBuffer_%s;\n\n' % (
                    service['name']))
        C.write('static SomeIp_ClientServiceContextType someIpClientServiceContext_%s;\n' % (
            service['name']))
        C.write('static const SomeIp_ClientServiceType someIpClientService_%s = {\n' % (
            service['name']))
        C.write('  %s, /* serviceId */\n' % (service['service']))
        C.write('  %s, /* clientId */\n' % (service['clientId']))
        C.write('  SD_CLIENT_SERVICE_HANDLE_ID_%s, /* sdHandleID */\n' %
                (service['name'].upper()))
        if 'methods' not in service:
            C.write('  NULL,\n  0,\n')
        else:
            C.write('  someIpClientMethods_%s,\n' % (service['name']))
            C.write('  ARRAY_SIZE(someIpClientMethods_%s),\n' %
                    (service['name']))
        if 'event-groups' not in service:
            C.write('  NULL,\n  0,\n')
        else:
            C.write('  someIpClientEvents_%s,\n' % (service['name']))
            C.write('  ARRAY_SIZE(someIpClientEvents_%s),\n' %
                    (service['name']))
        C.write('  &someIpClientServiceContext_%s,\n' %
                (service['name']))
        C.write('  SOAD_TX_PID_SOMEIP_%s,\n' % (service['name'].upper()))
        C.write('  SomeIp_%s_OnAvailability,\n' % (service['name']))
        if 'reliable' in service:
            C.write('  &someIpTcpBuffer_%s,\n' % (service['name']))
        else:
            C.write('  NULL,\n')
        C.write('};\n\n')
    C.write('static const SomeIp_ServiceType SomeIp_Services[] = {\n')
    for service in cfg['servers']:
        C.write('  {\n')
        C.write('    TRUE,\n')
        if 'reliable' in service:
            C.write('    SOAD_SOCKID_SOMEIP_%s_SERVER,\n' %
                    (service['name'].upper()))
        else:
            C.write('    SOAD_SOCKID_SOMEIP_%s,\n' % (service['name'].upper()))
        C.write('    &someIpServerService_%s,\n' % (service['name']))
        C.write('  },\n')
    for service in cfg['clients']:
        C.write('  {\n')
        C.write('    FALSE,\n')
        C.write('    SOAD_SOCKID_SOMEIP_%s,\n' % (service['name'].upper()))
        C.write('    &someIpClientService_%s,\n' % (service['name']))
        C.write('  },\n')
    C.write('};\n\n')
    C.write('static const uint16_t Sd_PID2ServiceMap[] = {\n')
    for service in cfg['servers']:
        if 'reliable' in service:
            numOfConnections = service['listen'] if 'listen' in service else 3
        else:
            numOfConnections = 1
        for i in range(numOfConnections):
            C.write('  SOMEIP_SSID_%s,\n' % (service['name'].upper()))
    for service in cfg['clients']:
        C.write('  SOMEIP_CSID_%s,\n' % (service['name'].upper()))
    C.write('};\n\n')
    C.write('static const uint16_t Sd_PID2ServiceConnectionMap[] = {\n')
    for service in cfg['servers']:
        if 'reliable' in service:
            numOfConnections = service['listen'] if 'listen' in service else 3
        else:
            numOfConnections = 1
        for i in range(numOfConnections):
            C.write('  %s,\n' % (i))
    for service in cfg['clients']:
        C.write('  0,\n')
    C.write('};\n\n')
    C.write('static const uint16_t Sd_TxMethod2ServiceMap[] = {\n')
    for service in cfg['clients']:
        if 'methods' not in service:
            continue
        for method in service['methods']:
            C.write('  SOMEIP_CSID_%s,/* %s */\n' %
                    (service['name'].upper(), method['name']))
    C.write('  -1\n};\n\n')
    C.write('static const uint16_t Sd_TxMethod2PerServiceMap[] = {\n')
    for service in cfg['clients']:
        if 'methods' not in service:
            continue
        for i, method in enumerate(service['methods']):
            C.write('  %s, /* %s */\n' % (i, method['name']))
    C.write('  -1\n};\n\n')
    C.write('static const uint16_t Sd_TxEvent2ServiceMap[] = {\n')
    for service in cfg['servers']:
        if 'event-groups' not in service:
            continue
        for egroup in service['event-groups']:
            for event in egroup['events']:
                C.write('  SOMEIP_SSID_%s, /* %s %s */\n' %
                        (service['name'].upper(), egroup['name'], event['name']))
    C.write('  -1\n};\n\n')
    C.write('static const uint16_t Sd_TxEvent2PerServiceMap[] = {\n')
    for service in cfg['servers']:
        if 'event-groups' not in service:
            continue
        ID = 0
        for egroup in service['event-groups']:
            for event in egroup['events']:
                C.write('  %s, /* %s %s */\n' %
                        (ID, egroup['name'], event['name']))
                ID += 1
    C.write('  -1\n};\n\n')
    C.write('const SomeIp_ConfigType SomeIp_Config = {\n')
    C.write('  SOMEIP_CONVERT_MS_TO_MAIN_CYCLES(%s),\n' %
            (cfg.get('TpRxTimeoutTime', 100)))
    C.write('  SomeIp_Services,\n')
    C.write('  ARRAY_SIZE(SomeIp_Services),\n')
    C.write('  Sd_PID2ServiceMap,\n')
    C.write('  Sd_PID2ServiceConnectionMap,\n')
    C.write('  ARRAY_SIZE(Sd_PID2ServiceMap),\n')
    C.write('  Sd_TxMethod2ServiceMap,\n')
    C.write('  Sd_TxMethod2PerServiceMap,\n')
    C.write('  ARRAY_SIZE(Sd_TxMethod2ServiceMap)-1,\n')
    C.write('  Sd_TxEvent2ServiceMap,\n')
    C.write('  Sd_TxEvent2PerServiceMap,\n')
    C.write('  ARRAY_SIZE(Sd_TxEvent2ServiceMap)-1,\n')
    C.write('};\n\n')
    C.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    C.close()


def Gen_SomeIp(cfg, dir):
    Gen_SD(cfg, dir)
    Gen_SOMEIP(cfg, dir)
