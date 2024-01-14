# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import pprint
import os
import json
from .helper import *

from .Com import get_messages
from . import MemCluster as MC

__all__ = ['Gen']

HIGH_MODULES = ['Dcm']
TP_MODULES = ['DoIP', 'CanTp', 'LinTp']
LOW_MODULES = TP_MODULES + ['CanIf']


enable_base_id = True

def getBaseId(groups, modsFrom=[], modsTo=[]):
    index = 0
    for fr, grphls in groups.items():
        if fr in modsFrom and modsTo == []:
            return index
        for hl, grptos in grphls.items():
            for to, rts in grptos.items():
                if to in modsTo:
                    return index
                for rt in rts:
                    index += 1
    return -1


def Gen_PduR(cfg, dir):
    if 'memory' in cfg:
        mcfg = {'name': 'PduR', 'clusters': cfg['memory']}
    groups = {}
    modules = []
    hasGW = False
    for rt in cfg['routines']:
        fr = rt['from']
        to = rt['to']
        if fr in TP_MODULES and to in TP_MODULES:
            hasGW = True
        if fr not in groups:
            groups[fr] = {'high': {}, 'low': {}}
        if to in HIGH_MODULES:
            if to not in groups[fr]['high']:
                groups[fr]['high'][to] = []
            groups[fr]['high'][to].append(rt)
        else:
            if to not in groups[fr]['low']:
                groups[fr]['low'][to] = []
            groups[fr]['low'][to].append(rt)
        if fr not in modules:
            modules.append(fr)
        if to not in modules:
            modules.append(to)
    H = open('%s/PduR_Cfg.h' % (dir), 'w')
    GenHeader(H)
    H.write('#ifndef PDUR_CFG_H\n')
    H.write('#define PDUR_CFG_H\n')
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    if 'memory' in cfg:
        H.write('#define PDUR_USE_MEMPOOL\n')
    if(hasGW):
        H.write('#define PDUR_USE_TP_GATEWAY\n')
    H.write('#define PDUR_DCM_TX_BASE_ID %s\n' % (getBaseId(groups, modsFrom=['Dcm'])))
    if enable_base_id:
        H.write('#define PDUR_DOIP_RX_BASE_ID %s\n' % (getBaseId(groups, modsFrom=['DoIP'])))
        H.write('#define PDUR_DOIP_TX_BASE_ID %s\n' % (getBaseId(groups, modsTo=['DoIP'])))
        H.write('#define PDUR_CANTP_RX_BASE_ID %s\n' % (getBaseId(groups, modsFrom=['CanTp'])))
        H.write('#define PDUR_CANTP_TX_BASE_ID %s\n' % (getBaseId(groups, modsTo=['CanTp'])))
        H.write('#define PDUR_LINTP_RX_BASE_ID %s\n' % (getBaseId(groups, modsFrom=['LinTp'])))
        H.write('#define PDUR_LINTP_TX_BASE_ID %s\n' % (getBaseId(groups, modsTo=['LinTp'])))
    else:
        H.write('#define PDUR_DOIP_RX_BASE_ID 0\n')
        H.write('#define PDUR_DOIP_TX_BASE_ID 0\n')
        H.write('#define PDUR_CANTP_RX_BASE_ID 0\n')
        H.write('#define PDUR_CANTP_TX_BASE_ID 0\n')
        H.write('#define PDUR_LINTP_RX_BASE_ID 0\n')
        H.write('#define PDUR_LINTP_TX_BASE_ID 0\n')
    H.write('\n')
    index = 0
    for fr, grphls in groups.items():
        for hl, grptos in grphls.items():
            for to, rts in grptos.items():
                for rt in rts:
                    H.write('#define PDUR_%s %s\n' % (rt['name'], index))
                    index += 1
    if 'memory' in cfg:
        MC.Gen_Macros(mcfg, H)
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
    H.write('#endif /* PDUR_CFG_H */\n')
    H.close()

    C = open('%s/PduR_Cfg.c' % (dir), 'w')
    GenHeader(C)
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#include "PduR.h"\n')
    C.write('#include "PduR_Cfg.h"\n')
    C.write('#include "PduR_Priv.h"\n')
    for mod in modules:
        C.write('#include "%s.h"\n' % (mod))
        if mod not in ['Dcm']:
            C.write('#include "%s_Cfg.h"\n' % (mod))
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    if 'memory' in cfg:
        MC.Gen_Defs(mcfg, C)
    if 'Dcm' in modules:
        C.write('const PduR_ApiType PduR_DcmApi = {\n')
        C.write('  Dcm_StartOfReception,\n')
        C.write('  Dcm_CopyRxData,\n')
        C.write('  { (void_ptr_t)Dcm_TpRxIndication },\n')
        C.write('  NULL,\n')
        C.write('  Dcm_CopyTxData,\n')
        C.write('  Dcm_TpTxConfirmation,\n')
        C.write('};\n\n')
    if 'Com' in modules:
        C.write('const PduR_ApiType PduR_ComApi = {\n')
        C.write('  NULL,\n')
        C.write('  NULL,\n')
        C.write('  { (void*)Com_RxIndication },\n')
        C.write('  NULL,\n')
        C.write('  NULL,\n')
        C.write('  Com_TxConfirmation,\n')
        C.write('};\n\n')
    if 'DoIP' in modules:
        C.write('const PduR_ApiType PduR_DoIPApi = {\n')
        if(hasGW):
            C.write('  PduR_DoIPGwStartOfReception,\n')
            C.write('  PduR_DoIPGwCopyRxData,\n')
            C.write('  { (void*)PduR_DoIPGwRxIndication },\n')
        else:
            C.write('  NULL,\n')
            C.write('  NULL,\n')
            C.write('  { NULL },\n')
        C.write('  DoIP_TpTransmit,\n')
        if(hasGW):
            C.write('  PduR_DoIPGwCopyTxData,\n')
            C.write('  PduR_DoIPGwTxConfirmation,\n')
        else:
            C.write('  NULL,\n')
            C.write('  NULL,\n')
        C.write('};\n\n')
    if 'CanTp' in modules:
        C.write('const PduR_ApiType PduR_CanTpApi = {\n')
        if(hasGW):
            C.write('  PduR_CanTpGwStartOfReception,\n')
            C.write('  PduR_CanTpGwCopyRxData,\n')
            C.write('  { (void*)PduR_CanTpGwRxIndication },\n')
        else:
            C.write('  NULL,\n')
            C.write('  NULL,\n')
            C.write('  { NULL },\n')
        C.write('  CanTp_Transmit,\n')
        if(hasGW):
            C.write('  PduR_CanTpGwCopyTxData,\n')
            C.write('  PduR_CanTpGwTxConfirmation,\n')
        else:
            C.write('  NULL,\n')
            C.write('  NULL,\n')
        C.write('};\n\n')
    if 'LinTp' in modules:
        C.write('const PduR_ApiType PduR_LinTpApi = {\n')
        if(hasGW):
            C.write('  PduR_LinTpGwStartOfReception,\n')
            C.write('  PduR_LinTpGwCopyRxData,\n')
            C.write('  { (void*)PduR_LinTpGwRxIndication },\n')
        else:
            C.write('  NULL,\n')
            C.write('  NULL,\n')
            C.write('  { NULL },\n')
        C.write('  LinTp_Transmit,\n')
        if(hasGW):
            C.write('  PduR_LinTpGwCopyTxData,\n')
            C.write('  PduR_LinTpGwTxConfirmation,\n')
        else:
            C.write('  NULL,\n')
            C.write('  NULL,\n')
        C.write('};\n\n')
    if 'CanIf' in modules:
        C.write('const PduR_ApiType PduR_CanIfApi = {\n')
        C.write('  NULL,\n')
        C.write('  NULL,\n')
        C.write('  { NULL },\n')
        C.write('  CanIf_Transmit,\n')
        C.write('  NULL,\n')
        C.write('  NULL,\n')
        C.write('};\n\n')
    for rt in cfg['routines']:
        fr, to, name = rt['from'], rt['to'], rt['name']
        dest = rt.get('dest', name)
        C.write(
            'static const PduR_PduType PduR_SrcPdu_%s_%s_%s = {\n' % (fr, to, name))
        C.write('  PDUR_MODULE_%s,\n' % (fr.upper()))
        C.write('  %s_%s,\n' % (fr.upper(), name))
        C.write('  &PduR_%sApi,\n' % (fr))
        C.write('};\n\n')

        C.write(
            'static const PduR_PduType PduR_DstPdu_%s_%s_%s[]={\n' % (fr, to, name))
        C.write('  {\n')
        C.write('    PDUR_MODULE_%s,\n' % (to.upper()))
        C.write('    %s_%s,\n' % (to.upper(), dest))
        C.write('    &PduR_%sApi,\n' % (to))
        C.write('  },\n')
        C.write('};\n\n')
        if fr in TP_MODULES and to in TP_MODULES:
            C.write(
                'static PduR_BufferType PduR_Buffer_%s = { NULL, 0, 0 };\n' % (name))
    C.write('static const PduR_RoutingPathType PduR_RoutingPaths[] = {\n')
    index = 0
    for fr, grphls in groups.items():
        for hl, grptos in grphls.items():
            for to, rts in grptos.items():
                for rt in rts:
                    name = rt['name']
                    dest = rt.get('dest', name)
                    C.write('  { /* %s: PDU %s from %s to %s %s */\n' % (index, name, fr, to, dest))
                    C.write('    &PduR_SrcPdu_%s_%s_%s,\n' % (fr, to, name))
                    C.write('    PduR_DstPdu_%s_%s_%s,\n' % (fr, to, name))
                    C.write('    ARRAY_SIZE(PduR_DstPdu_%s_%s_%s),\n' % (fr, to, name))
                    if fr in TP_MODULES and to in TP_MODULES:
                        C.write('    &PduR_Buffer_%s,\n' % (name))
                    else:
                        C.write('    NULL,\n')
                    C.write('  },\n')
                    index += 1
    C.write('};\n\n')
    C.write('const PduR_ConfigType PduR_Config = {\n')
    C.write('#if defined(PDUR_USE_MEMPOOL)\n')
    if 'memory' in cfg:
        C.write('  &MC_PduR,\n')
    else:
        C.write('  NULL,\n')
    C.write('#endif\n')
    C.write('  PduR_RoutingPaths,\n')
    C.write('  ARRAY_SIZE(PduR_RoutingPaths),\n')
    C.write('  PDUR_DCM_TX_BASE_ID,\n')
    C.write('  PDUR_DOIP_RX_BASE_ID,\n')
    C.write('  PDUR_DOIP_TX_BASE_ID,\n')
    C.write('  PDUR_CANTP_RX_BASE_ID,\n')
    C.write('  PDUR_CANTP_TX_BASE_ID,\n')
    C.write('  PDUR_LINTP_RX_BASE_ID,\n')
    C.write('  PDUR_LINTP_TX_BASE_ID,\n')
    C.write('};\n')
    C.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    C.close()


def extract(cfg, dir):
    global enable_base_id
    enable_base_id = cfg.get('enable_base_id', True)
    cfg_ = {'class': 'PduR', 'routines': cfg.get('routines', [])}
    if 'memory' in cfg:
        cfg_['memory'] = cfg['memory']
    bNew = False
    for network in cfg.get('networks', []):
        if 'dbc' in network:
            path = network['dbc']
            me = network['me']
            name = network['name']
            ll = network['network']
            if ll.upper() == 'CAN':
                ll = 'CanIf'
            if ll.upper() == 'LIN':
                ll = 'LinIf'
            if not os.path.isfile(path):
                path = os.path.abspath(os.path.join(dir, '..', path))
            if not os.path.isfile(path):
                raise Exception('File %s not exists' % (path))
            messages = get_messages(path)
            for msg in messages:
                rn = toPduSymbol((name, msg['name']))
                if msg['node'] == me:
                    fr = 'Com'
                    to = ll
                else:
                    fr = ll
                    to = 'Com'
                routine = {'name': rn, 'from': fr, 'to': to, 'id': msg['id']}
                cfg_['routines'].append(routine)
            bNew = True
        else:
            raise
    if bNew:
        with open('%s/PduR.json' % (dir), 'w') as f:
            json.dump(cfg_, f, indent=2)
    return cfg_


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), 'GEN')
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    cfg_ = extract(cfg, dir)
    Gen_PduR(cfg_, dir)
