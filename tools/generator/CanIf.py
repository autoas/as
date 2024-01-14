# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import pprint
import os
import json
from .helper import *

from .Com import get_messages

__all__ = ['Gen']


def Gen_CanIf(cfg, dir):
    modules = []
    for network in cfg['networks']:
        for pdu in network['RxPdus'] + network['TxPdus']:
            if pdu['up'] not in modules:
                modules.append(pdu['up'])
    H = open('%s/CanIf_Cfg.h' % (dir), 'w')
    GenHeader(H)
    H.write('#ifndef __CANIF_CFG_H\n')
    H.write('#define __CANIF_CFG_H\n')
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    ID = 0
    for network in cfg['networks']:
        for pdu in network['RxPdus']:
            H.write('#define CANIF_%s %s /* %s id=0x%x */\n' %
                    (pdu['name'], ID, network['name'], toNum(pdu['id'])))
            ID += 1
    H.write('\n')
    ID = 0
    for network in cfg['networks']:
        for pdu in network['TxPdus']:
            H.write('#define CANIF_%s %s /* %s id=0x%x */\n' %
                    (pdu['name'], ID, network['name'], toNum(pdu['id'])))
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
    H.write('#endif /* __CANIF_CFG_H */\n')
    H.close()

    C = open('%s/CanIf_Cfg.c' % (dir), 'w')
    GenHeader(C)
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#include "CanIf.h"\n')
    C.write('#include "CanIf_Priv.h"\n')
    for mod in modules:
        if mod == 'PduR':
            C.write('#include "PduR_CanIf.h"\n')
        else:
            C.write('#include "%s.h"\n' % (mod))
        if mod not in ['OsekNm', 'CanNm', 'CanTSyn']:
            C.write('#include "%s_Cfg.h"\n' % (mod))
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    C.write('static const CanIf_RxPduType CanIf_RxPdus[] = {\n')
    for netId, network in enumerate(cfg['networks']):
        for pdu in network['RxPdus']:
            C.write('  {\n')
            if pdu['up'] == 'PduR':
                C.write('    %s_CanIfRxIndication,\n' % (pdu['up']))
            else:
                C.write('    %s_RxIndication,\n' % (pdu['up']))
            if pdu['up'] in ['OsekNm', 'CanNm', 'CanTSyn']:
                C.write('    %s, /* NetId */\n' % (netId))
            else:
                C.write('    %s_%s,\n' % (pdu['up'].upper(), pdu['name']))
            C.write('    0x%x, /* canid */\n' % (toNum(pdu['id'])))
            C.write('    0x%x, /* mask */\n' % (toNum(pdu.get('mask', '0xFFFFFFFF'))))
            C.write('    %s, /* hoh */\n' % (pdu.get('hoh', 0)))
            C.write('  },\n')
    C.write('};\n\n')
    for netId, network in enumerate(cfg['networks']):
        for pdu in network['TxPdus']:
            if pdu.get('dynamic', False):
                C.write('static Can_IdType canidOf%s = %s;\n' %
                        (pdu['name'], pdu['id']))
    C.write('static const CanIf_TxPduType CanIf_TxPdus[] = {\n')
    for netId, network in enumerate(cfg['networks']):
        for pdu in network['TxPdus']:
            C.write('  {\n')
            if pdu['up'] == 'PduR':
                C.write('    %s_CanIfTxConfirmation,\n' % (pdu['up']))
            else:
                C.write('    %s_TxConfirmation,\n' % (pdu['up']))
            if pdu['up'] in ['OsekNm', 'CanNm', 'CanTSyn']:
                C.write('    %s, /* NetId */\n' % (netId))
            else:
                C.write('    %s_%s,\n' % (pdu['up'].upper(), pdu['name']))
            C.write('    0x%x, /* canid */\n' % (toNum(pdu['id'])))
            if pdu.get('dynamic', False):
                C.write('    &canidOf%s, /* p_canid */\n' % (pdu['name']))
            else:
                C.write('    NULL, /* p_canid */\n')
            C.write('    %s, /* hoh */\n' % (pdu.get('hoh', 0)))
            C.write('  },\n')
    C.write('};\n\n')
    C.write('const CanIf_ConfigType CanIf_Config = {\n')
    C.write('  CanIf_RxPdus,\n')
    C.write('  CanIf_TxPdus,\n')
    C.write('  ARRAY_SIZE(CanIf_RxPdus),\n')
    C.write('  ARRAY_SIZE(CanIf_TxPdus),\n')
    C.write('};\n\n')
    C.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    C.close()


def extract(cfg, dir):
    cfg_ = {'class': 'CanIf', 'networks': []}
    bNew = False
    hoh = -1
    for network in cfg['networks']:
        if 'dbc' in network:
            hoh += 1
            network_ = dict(network)
            path = network['dbc']
            del network_['dbc']
            me = network['me']
            name = network['name']
            if not os.path.isfile(path):
                path = os.path.abspath(os.path.join(dir, '..', path))
            if not os.path.isfile(path):
                raise Exception('File %s not exists' % (path))
            messages = get_messages(path)
            for msg in messages:
                rn = '%s_%s' % (name, toMacro(msg['name']))
                rn = rn.upper()
                if msg['node'] == me:
                    if 'TX' not in rn:
                        rn += '_TX'
                    kl = 'TxPdus'
                else:
                    if 'RX' not in rn:
                        rn += '_RX'
                    kl = 'RxPdus'
                pdu = {'name': rn, 'id': msg['id'], 'hoh': hoh, 'up': 'PduR'}
                if kl not in network_:
                    network_[kl] = []
                network_[kl].append(pdu)
            cfg_['networks'].append(network_)
            bNew = True
        else:
            cfg_['networks'].append(network)
    if bNew:
        with open('%s/CanIf.json' % (dir), 'w') as f:
            json.dump(cfg_, f, indent=2)
    for network in cfg_['networks']:
        network['RxPdus'].sort(key=lambda x: eval(str(x['id'])))
        network['TxPdus'].sort(key=lambda x: eval(str(x['id'])))
    return cfg_


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), 'GEN')
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    cfg_ = extract(cfg, dir)
    Gen_CanIf(cfg_, dir)
