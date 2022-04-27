# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
from .helper import *

__all__ = ['Gen_DoIp']

def Gen_DoIp(cfg, dir):
    H = open('%s/DoIP_Cfg.h' % (dir), 'w')
    GenHeader(H)
    H.write('#ifndef _DOIP_CFG_H\n')
    H.write('#define _DOIP_CFG_H\n')
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    ID = 0
    H.write('#define DOIP_RX_PID_UDP %s\n' % (ID))
    ID += 1
    for i in range(cfg['max_connections']):
        H.write('#define DOIP_RX_PID_TCP%s %s\n' % (i, ID))
        ID += 1
    H.write('\n#define DOIP_MAX_TESTER_CONNECTIONS %s\n\n' %
            (cfg['max_connections']))
    H.write('#define DOIP_MAIN_FUNCTION_PERIOD 10\n')
    H.write('#define DOIP_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n')
    H.write('  ((x + DOIP_MAIN_FUNCTION_PERIOD - 1) / DOIP_MAIN_FUNCTION_PERIOD)\n\n')
    for i, target in enumerate(cfg['targets']):
        H.write('#define DOIP_%s_RX %s\n' %
                (toMacro(target['name']), i))
        H.write('#define DOIP_%s_TX %s\n' % (toMacro(target['name']), i))
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
    H.write('#endif /* _DOIP_CFG_H */\n')
    H.close()

    C = open('%s/DoIP_Cfg.c' % (dir), 'w')
    GenHeader(C)
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#include "DoIP.h"\n')
    C.write('#include "DoIP_Cfg.h"\n')
    C.write('#include "DoIP_Priv.h"\n')
    C.write('#include "SoAd_Cfg.h"\n')
    C.write('#include "Dcm.h"\n')
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    C.write('#define DOIP_INITIAL_INACTIVITY_TIME 5000\n')
    C.write('#define DOIP_GENERAL_INACTIVITY_TIME 5000\n')
    C.write('#define DOIP_ALIVE_CHECK_RESPONSE_TIMEOUT 50\n')
    C.write('\n')
    for i, target in enumerate(cfg['targets']):
        C.write('#define DOIP_TID_%s %s\n' % (target['name'], i))
    C.write('\n')
    for i, rt in enumerate(cfg['routines']):
        C.write('#define DOIP_RID_%s %s\n' % (rt['name'], i))
    C.write('\n')
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    for rt in cfg['routines']:
        C.write(
            'extern Std_ReturnType DoIP_%s_RoutingActivationAuthenticationCallback(\n' % (rt['name']))
        C.write('  boolean *Authentified, const uint8_t *AuthenticationReqData, uint8_t *AuthenticationResData);\n')
        C.write(
            'extern Std_ReturnType DoIP_%s_RoutingActivationConfirmationCallback(\n' % (rt['name']))
        C.write(
            '  boolean *Confirmed, const uint8_t *ConfirmationReqData, uint8_t *ConfirmationResData);\n')
    C.write('static const DoIP_TesterType DoIP_Testers[];\n')

    C.write('extern Std_ReturnType DoIP_UserGetEID(uint8_t *Data);\n')
    C.write('extern Std_ReturnType DoIP_UserGetGID(uint8_t *Data);\n')
    C.write('extern Std_ReturnType DoIP_UserGetPowerModeStatus(uint8_t *PowerState);\n')
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    C.write('static const DoIP_TargetAddressType DoIp_TargetAddress[] = {\n')
    for target in cfg['targets']:
        C.write('  {\n')
        C.write('    %s, /* TargetAddress */\n' % (target['address']))
        C.write('    DOIP_%s_RX, /* RxPduId */\n' % (toMacro(target['name'])))
        C.write('    DOIP_%s_TX, /* TxPduId */\n' % (toMacro(target['name'])))
        C.write('  },\n')
    C.write('};\n\n')

    for rt in cfg['routines']:
        C.write(
            'static const DoIP_TargetAddressType *const DoIP_%s_TargetAddressRefs[] = {\n' % (rt['name']))
        for ta in rt['targets']:
            C.write('  &DoIp_TargetAddress[DOIP_TID_%s],\n' % (ta))
        C.write('};\n\n')
    C.write(
        'static const DoIP_RoutingActivationType DoIP_RoutingActivations[] = {\n')
    for rt in cfg['routines']:
        C.write('  {\n')
        C.write('    %s, /* %s */\n' % (rt['number'], rt['name']))
        C.write('    0, /* OEMReqLen */\n')
        C.write('    0, /* OEMResLen */\n')
        C.write('    DoIP_%s_TargetAddressRefs, /* TargetAddressRefs */\n' %
                (rt['name']))
        C.write('    ARRAY_SIZE(DoIP_%s_TargetAddressRefs),\n' % (rt['name']))
        C.write('    DoIP_%s_RoutingActivationAuthenticationCallback,\n' %
                (rt['name']))
        C.write('    DoIP_%s_RoutingActivationConfirmationCallback,\n' %
                (rt['name']))
        C.write('  },\n')
        C.write('};\n\n')

    for tester in cfg['testers']:
        C.write(
            'static const DoIP_RoutingActivationType * const DoIP_%s_RoutingActivationRefs[] = {\n' % (tester['name']))
        for rt in tester['routines']:
            C.write('  &DoIP_RoutingActivations[DOIP_RID_%s],\n' % (rt))
        C.write('};\n\n')

    C.write('static const DoIP_TesterType DoIP_Testers[] = {\n')
    for tester in cfg['testers']:
        C.write('  {\n')
        C.write('    8, /* NumByteDiagAckNack */\n')
        C.write('    %s, /* TesterSA */\n' % (tester['address']))
        C.write('    DoIP_%s_RoutingActivationRefs,\n' % (tester['name']))
        C.write('    ARRAY_SIZE(DoIP_%s_RoutingActivationRefs),\n' %
                (tester['name']))
        C.write('  },\n')
    C.write('};\n\n')

    C.write(
        'static DoIP_TesterConnectionContextType DoIP_TesterConnectionContext[DOIP_MAX_TESTER_CONNECTIONS];\n\n')
    C.write(
        'static const DoIP_TesterConnectionType DoIP_TesterConnections[DOIP_MAX_TESTER_CONNECTIONS] = {\n')
    for i in range(cfg['max_connections']):
        C.write('  {\n')
        C.write('    &DoIP_TesterConnectionContext[%s],\n' % (i))
        C.write('    SOAD_SOCKID_DOIP_TCP_APT%s,\n' % (i))
        C.write('    SOAD_TX_PID_DOIP_TCP_APT%s,\n' % (i))
        C.write('  },\n')
    C.write('};\n\n')

    C.write('static const DoIP_TcpConnectionType DoIP_TcpConnections[] = {\n')
    C.write('  {\n')
    C.write('    SOAD_SOCKID_DOIP_TCP_SERVER, TRUE, /* RequestAddressAssignment */\n')
    C.write('  },\n')
    C.write('};\n\n')

    C.write('static const DoIP_UdpConnectionType DoIP_UdpConnections[] = {\n')
    C.write('  {\n')
    C.write('    SOAD_SOCKID_DOIP_UDP, SOAD_TX_PID_DOIP_UDP, TRUE, /* RequestAddressAssignment */\n')
    C.write('  },\n')
    C.write('};\n\n')

    C.write(
        'static DoIP_UdpVehicleAnnouncementConnectionContextType UdpVehicleAnnouncementConnectionContexts[1];\n\n')

    C.write(
        'static const DoIP_UdpVehicleAnnouncementConnectionType DoIP_UdpVehicleAnnouncementConnections[] = {\n')
    C.write('  {\n')
    C.write(
        '    &UdpVehicleAnnouncementConnectionContexts[0], /* context */\n')
    C.write('    SOAD_SOCKID_DOIP_UDP,                         /* SoConId */\n')
    C.write('    SOAD_TX_PID_DOIP_UDP,                         /* SoAdTxPdu */\n')
    C.write('    FALSE,                                        /* RequestAddressAssignment */\n')
    C.write('  },\n')
    C.write('};\n\n')

    C.write('static const uint16_t RxPduIdToConnectionMap[] = {\n')
    C.write('  DOIP_INVALID_INDEX, /* DOIP_RX_PID_UDP */\n')
    for i in range(cfg['max_connections']):
        C.write('  %s,                  /* DOIP_RX_PID_TCP%s */\n' % (i, i))
    C.write('};\n\n')

    C.write('const DoIP_ConfigType DoIP_Config = {\n')
    C.write('  DOIP_CONVERT_MS_TO_MAIN_CYCLES(DOIP_INITIAL_INACTIVITY_TIME),\n')
    C.write('  DOIP_CONVERT_MS_TO_MAIN_CYCLES(DOIP_GENERAL_INACTIVITY_TIME),\n')
    C.write('  DOIP_CONVERT_MS_TO_MAIN_CYCLES(DOIP_ALIVE_CHECK_RESPONSE_TIMEOUT),\n')
    C.write('  0xFF, /* VinInvalidityPattern */\n')
    C.write(
        '  DOIP_CONVERT_MS_TO_MAIN_CYCLES(50),  /* InitialVehicleAnnouncementTime */\n')
    C.write('  DOIP_CONVERT_MS_TO_MAIN_CYCLES(200), /* VehicleAnnouncementInterval */\n')
    C.write('  3,                                   /* VehicleAnnouncementCount */\n')
    C.write('  DOIP_GATEWAY, /* NodeType */\n')
    C.write('  TRUE, /* EntityStatusMaxByteFieldUse */\n')
    C.write('  Dcm_GetVin,\n')
    C.write('  DoIP_UserGetEID,\n')
    C.write('  DoIP_UserGetGID,\n')
    C.write('  DoIP_UserGetPowerModeStatus,\n')
    C.write('  0xdead, /* LogicalAddress */ \n')
    C.write('  DoIp_TargetAddress,\n')
    C.write('  ARRAY_SIZE(DoIp_TargetAddress),\n')
    C.write('  DoIP_TcpConnections,\n')
    C.write('  ARRAY_SIZE(DoIP_TcpConnections),\n')
    C.write('  DoIP_UdpConnections,\n')
    C.write('  ARRAY_SIZE(DoIP_UdpConnections),\n')
    C.write('  DoIP_UdpVehicleAnnouncementConnections,\n')
    C.write('  ARRAY_SIZE(DoIP_UdpVehicleAnnouncementConnections),\n')
    C.write('  DoIP_TesterConnections,\n')
    C.write('  ARRAY_SIZE(DoIP_TesterConnections),\n')
    C.write('  DoIP_RoutingActivations,\n')
    C.write('  ARRAY_SIZE(DoIP_RoutingActivations),\n')
    C.write('  DoIP_Testers,\n')
    C.write('  ARRAY_SIZE(DoIP_Testers),\n')
    C.write('  RxPduIdToConnectionMap,\n')
    C.write('  ARRAY_SIZE(RxPduIdToConnectionMap),\n')
    C.write('};\n')
    C.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    C.close()
