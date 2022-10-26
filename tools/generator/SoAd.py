# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
from .helper import *


__all__ = ['Gen_SoAd']

def Gen_Sock(C, RxPduId, SoConId, GID, SoConType):
    C.write('  {\n')
    C.write('    %s, /* RxPduId */\n' % (RxPduId))
    C.write('    %s, /* SoConId */\n' % (SoConId))
    C.write('    %s, /* GID */\n' % (GID))
    C.write('    SOAD_SOCON_%s, /* SoConType */\n' % (SoConType))
    C.write('  },\n')


def Gen_SoAd(cfg, dir):
    H = open('%s/SoAd_Cfg.h' % (dir), 'w')
    GenHeader(H)
    H.write('#ifndef _SOAD_CFG_H\n')
    H.write('#define _SOAD_CFG_H\n')
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    ID = 0
    for sock in cfg['sockets']:
        if sock['protocol'] == 'UDP':
            H.write('#define SOAD_SOCKID_%s %s\n' % (sock['name'], ID))
            ID += 1
        elif 'server' in sock:
            H.write('#define SOAD_SOCKID_%s_SERVER %s\n' % (sock['name'], ID))
            ID += 1
            for i in range(sock['listen']):
                H.write('#define SOAD_SOCKID_%s_APT%s %s\n' %
                        (sock['name'], i, ID))
                ID += 1
        elif 'client' in sock:
            H.write('#define SOAD_SOCKID_%s %s\n' % (sock['name'], ID))
            ID += 1
        else:
            raise Exception('wrong config for %s' % (sock))
    H.write('\n')
    ID = 0
    for sock in cfg['sockets']:
        if sock['protocol'] == 'UDP':
            H.write('#define SOAD_TX_PID_%s %s\n' % (sock['name'], ID))
            ID += 1
        elif 'server' in sock:
            for i in range(sock['listen']):
                H.write('#define SOAD_TX_PID_%s_APT%s %s\n' %
                        (sock['name'], i, ID))
                ID += 1
        elif 'client' in sock:
            H.write('#define SOAD_TX_PID_%s %s\n' % (sock['name'], ID))
            ID += 1
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
    H.write('#endif /* _SOAD_CFG_H */\n')
    H.close()

    C = open('%s/SoAd_Cfg.c' % (dir), 'w')
    GenHeader(C)
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#include "SoAd.h"\n')
    C.write('#include "SoAd_Cfg.h"\n')
    C.write('#include "SoAd_Priv.h"\n')
    if any(sock['up'] == 'DoIP' for sock in cfg['sockets']):
        C.write('#include "DoIP.h"\n')
        C.write('#include "DoIP_Cfg.h"\n')
    if any(sock['up'] == 'SD' for sock in cfg['sockets']):
        C.write('#include "Sd.h"\n')
        C.write('#include "Sd_Cfg.h"\n')
    if any(sock['up'] == 'SOMEIP' for sock in cfg['sockets']):
        C.write('#include "SomeIp.h"\n')
        C.write('#include "SomeIp_Cfg.h"\n')
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    if any(sock['up'] == 'DoIP' for sock in cfg['sockets']):
        C.write('static const SoAd_IfInterfaceType SoAd_DoIP_IF = {\n')
        C.write('  DoIP_SoAdIfRxIndication,\n')
        C.write('  NULL,\n')
        C.write('  DoIP_SoAdIfTxConfirmation,\n')
        C.write('};\n\n')

        C.write('static const SoAd_TpInterfaceType SoAd_DoIP_TP_IF = {\n')
        C.write('  DoIP_SoAdTpStartOfReception,\n')
        C.write('  DoIP_SoAdTpCopyRxData,\n')
        C.write('  NULL,\n')
        C.write('  NULL,\n')
        C.write('  NULL,\n')
        C.write('};\n\n')

    if any(sock['up'] == 'SD' for sock in cfg['sockets']):
        C.write('static const SoAd_IfInterfaceType SoAd_SD_IF = {\n')
        C.write('  Sd_RxIndication,\n')
        C.write('  NULL,\n')
        C.write('  NULL,\n')
        C.write('};\n\n')

    if any(sock['up'] == 'SOMEIP' and sock['protocol'] == 'UDP' for sock in cfg['sockets']):
        C.write('static const SoAd_IfInterfaceType SoAd_SOMEIP_IF = {\n')
        C.write('  SomeIp_RxIndication,\n')
        C.write('  NULL,\n')
        C.write('  NULL,\n')
        C.write('};\n\n')
    if any(sock['up'] == 'SOMEIP' and sock['protocol'] == 'TCP' for sock in cfg['sockets']):
        C.write('static const SoAd_TpInterfaceType SoAd_SOMEIP_TP_IF = {\n')
        C.write('  SomeIp_SoAdTpStartOfReception,\n')
        C.write('  SomeIp_SoAdTpCopyRxData,\n')
        C.write('  NULL,\n')
        C.write('  NULL,\n')
        C.write('  NULL,\n')
        C.write('};\n\n')

    C.write(
        'static const SoAd_SocketConnectionType SoAd_SocketConnections[] = {\n')
    for GID, sock in enumerate(cfg['sockets']):
        RxPduId = sock['RxPduId']
        SoConType = '%s_%s'%(sock['protocol'], 'SERVER' if 'server' in sock else 'CLIENT')
        if sock['protocol'] == 'UDP':
            SoConId = 'SOAD_SOCKID_%s' % (sock['name'])
        elif 'server' in sock:
            SoConId = 'SOAD_SOCKID_%s_SERVER' % (sock['name'])
            RxPduId = -1
        elif 'client' in sock:
            SoConId = 'SOAD_SOCKID_%s' % (sock['name'])
        Gen_Sock(C, RxPduId, SoConId, GID, SoConType)
        if ('server' in sock) and (sock['protocol'] == 'TCP'):
            for i in range(sock['listen']):
                RxPduId = '%s%s' % (sock['RxPduId'], i)
                SoConId = 'SOAD_SOCKID_%s_APT%s' % (sock['name'], i)
                Gen_Sock(C, RxPduId, SoConId, GID, 'TCP_ACCEPT')
    C.write('};\n\n')

    C.write(
        'static SoAd_SocketContextType SoAd_SocketContexts[ARRAY_SIZE(SoAd_SocketConnections)];\n\n')

    C.write(
        'static const SoAd_SocketConnectionGroupType SoAd_SocketConnectionGroups[] = {\n')
    for GID, sock in enumerate(cfg['sockets']):
        IF = 'TODO'
        SoConModeChgNotification = 'TODO'
        SoConId = -1
        numOfConnections = 1
        IsTP = 'FALSE'
        if sock['protocol'] == 'UDP':
            pass
        elif 'server' in sock:
            numOfConnections = sock['listen']
            SoConId = 'SOAD_SOCKID_%s_APT0' % (sock['name'])
        elif 'client' in sock:
            pass
        if sock['up'] == 'DoIP':
            SoConModeChgNotification = 'DoIP_SoConModeChg'
            if sock['protocol'] == 'UDP':
                IF = 'SoAd_DoIP_IF'
            else:
                IF = 'SoAd_DoIP_TP_IF'
                IsTP = 'TRUE'
        elif sock['up'] == 'SD':
            SoConModeChgNotification = 'Sd_SoConModeChg'
            IF = 'SoAd_SD_IF'
        elif sock['up'] == 'SOMEIP':
            SoConModeChgNotification = 'SomeIp_SoConModeChg'
            if sock['protocol'] == 'UDP':
                IF = 'SoAd_SOMEIP_IF'
            else:
                IF = 'SoAd_SOMEIP_TP_IF'
                IsTP = 'TRUE'
        else:
            raise
        if 'server' in sock:
            IpAddress, Port = sock['server'].split(':')
        else:
            IpAddress, Port = sock['client'].split(':')
        if IpAddress != 'NULL':
            IpAddress = '"%s"' % (IpAddress)
        C.write('  {\n')
        C.write('    /* %s: %s */\n' % (GID, sock['name']))
        C.write('    &%s, /* Interface */\n' % (IF))
        C.write('    %s, /* IpAddress */\n' % (IpAddress))
        C.write('    %s, /* SoConModeChgNotification */\n' %
                (SoConModeChgNotification))
        C.write('    TCPIP_IPPROTO_%s, /* ProtocolType */\n' %
                (sock['protocol']))
        C.write('    %s, /* SoConId */\n' % (SoConId))
        C.write('    %s, /* Port */\n' % (Port))
        C.write('    %s, /* numOfConnections */\n' % (numOfConnections))
        C.write('    FALSE, /* AutomaticSoConSetup */\n')
        C.write('    %s, /* IsTP */\n' % (IsTP))
        C.write('  },\n')
    C.write('};\n\n')

    C.write('static const SoAd_SoConIdType TxPduIdToSoCondIdMap[] = {\n')
    for sock in cfg['sockets']:
        if sock['protocol'] == 'UDP':
            C.write('  SOAD_SOCKID_%s, /* SOAD_TX_PID_%s */\n' %
                    (sock['name'], sock['name']))
        elif 'server' in sock:
            for i in range(sock['listen']):
                C.write('  SOAD_SOCKID_%s_APT%s, /* SOAD_TX_PID_%s_APT%s */\n' %
                        (sock['name'], i, sock['name'], i))
        else:
            C.write('  SOAD_SOCKID_%s, /* SOAD_TX_PID_%s */\n' %
                    (sock['name'], sock['name']))
    C.write('};\n\n')
    C.write('const SoAd_ConfigType SoAd_Config = {\n')
    C.write('  SoAd_SocketConnections,\n')
    C.write('  SoAd_SocketContexts,\n')
    C.write('  ARRAY_SIZE(SoAd_SocketConnections),\n')
    C.write('  TxPduIdToSoCondIdMap,\n')
    C.write('  ARRAY_SIZE(TxPduIdToSoCondIdMap),\n')
    C.write('  SoAd_SocketConnectionGroups,\n')
    C.write('  ARRAY_SIZE(SoAd_SocketConnectionGroups),\n')
    C.write('};\n')
    C.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    C.close()
