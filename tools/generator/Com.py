# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *
from .dbc import dbc


def gen_rx_sig_cfg(sig, C):
    C.write('static const Com_SignalRxConfigType Com_SignalRxConfig_%s = {\n' % (
        sig['name']))
    InvalidNotification = sig.get('InvalidNotification', 'NULL')
    RxNotification = sig.get('RxNotification', 'NULL')
    FirstTimeout = sig.get('FirstTimeout', 0)
    DataInvalidAction = sig.get('DataInvalidAction', 'NOTIFY')
    RxDataTimeoutAction = sig.get('RxDataTimeoutAction', 'NOTIFY')
    C.write('  %s, /* InvalidNotification */\n' % (InvalidNotification))
    C.write('  %s, /* RxNotification */\n' % (RxNotification))
    C.write('  %s, /* FirstTimeout */\n' % (FirstTimeout))
    C.write('  %s, /* DataInvalidAction */\n' % (DataInvalidAction))
    C.write('  %s, /* RxDataTimeoutAction */\n' % (RxDataTimeoutAction))
    C.write('};\n\n')


def gen_tx_sig_cfg(sig, C):
    C.write('static const Com_SignalTxConfigType Com_SignalTxConfig_%s = {\n' % (
        sig['name']))
    ErrorNotification = sig.get('ErrorNotification', 'NULL')
    TxNotification = sig.get('TxNotification', 'NULL')
    C.write('  %s, /* ErrorNotification */\n' % (ErrorNotification))
    C.write('  %s, /* TxNotification */\n' % (TxNotification))
    C.write('};\n\n')


def get_signal_info(sig):
    if sig.get('isGroup', False):
        return 'UINT8N', 'uint8_t', int(sig['size']/8)
    if sig['size'] <= 8:
        t = 'INT8'
        t1 = 'int8_t'
    elif sig['size'] <= 16:
        t = 'INT16'
        t1 = 'int16_t'
    elif sig['size'] <= 32:
        t = 'INT32'
        t1 = 'int32_t'
    else:
        t = 'INT8N'
        t1 = 'int8_t'
    if sig.get('sign', '+') == '+':
        t = 'U%s' % (t)
        t1 = 'u%s' % (t1)
    else:
        t = 'S%s' % (t)
    return t, t1, int((sig['size']+7)/8)


def get_signal(msg, name):
    for sig in msg['signals']:
        if sig['name'] == name:
            return sig
    raise Exception('sinal %s not found in msg %s' % (name, msg['name']))


def gen_signal_init_value(sig, C):
    if sig.get('isGroup', False):
        return
    t0, t1, nBytes = get_signal_info(sig)

    if t0 in ['UINT8N', 'SINT8N']:
        InitialValue = sig.get('InitialValue', '[0]')
        cstr = ''
        for x in eval(InitialValue):
            cstr += '%x, ' % (x)
        C.write('static const %s %s_InitialValue[%s] = { %s };\n' % (
            t1, sig['name'], nBytes, cstr))
    else:
        InitialValue = sig.get('InitialValue', 0)
        C.write('static const %s %s_InitialValue = %s;\n' %
                (t1, sig['name'], InitialValue))


def gen_sig(sig, msg, C, isTx):
    C.write('  {\n')
    if 'group' in sig:
        gsig = get_signal(msg, sig['group'])
        offset = int(gsig['start']/8)
        C.write('    &Com_GrpsData_%s[%s], /* ptr */\n' %
                (sig['group'], int(sig['start']/8)-offset))
    else:
        C.write('    &Com_PduData_%s[%s], /* ptr */\n' %
                (msg['name'], int(sig['start']/8)))
    t0, t1, nBytes = get_signal_info(sig)
    if sig.get('isGroup', False):
        C.write('    Com_GrpsData_%s, /* shadowPtr */\n' % (sig['name']))
    else:
        C.write('    %s%s_InitialValue, /* initPtr */\n' %
                ('' if t0 in ['UINT8N', 'SINT8N'] else '&', sig['name']))
    C.write('    COM_%s, /* type */\n' % (t0))
    C.write('    COM_%sID_%s, /* HandleId */\n' % (
        'G' if sig.get('isGroup', False) else 'S',
        sig['name']))
    C.write('    %s, /* BitPosition */\n' % (sig['start'] & 7))
    C.write('    %s, /* BitSize */\n' % (sig['size']))
    C.write('#ifdef COM_USE_SIGNAL_UPDATE_BIT\n')
    UpdateBit = sig.get('UpdateBit', 'COM_UPDATE_BIT_NOT_USED')
    if type(UpdateBit) is int:
        assert(UpdateBit > sig['start'])
        UpdateBit = UpdateBit - int(sig['start']/8)*8
    C.write('    %s, /* UpdateBit */\n' % (UpdateBit))
    C.write('#endif\n')
    C.write('    %s, /* Endianness */\n' % (sig['endian'].upper()))
    C.write('#ifdef COM_USE_SIGNAL_CONFIG\n')
    if isTx:
        C.write('    NULL, /* rxConfig */\n')
        C.write('    &Com_SignalTxConfig_%s, /* txConfig */\n' % (sig['name']))
    else:
        C.write('    &Com_SignalRxConfig_%s, /* rxConfig */\n' % (sig['name']))
        C.write('    NULL, /* txConfig */\n')
    C.write('#endif\n')
    C.write('  },\n')


def gen_rx_msg_cfg(msg, C):
    C.write(
        'static const Com_IPduRxConfigType Com_IPduRxConfig_%s = {\n' % (msg['name']))
    RxNotification = msg.get('RxNotification', 'NULL')
    RxTOut = msg.get('RxTOut', 'NULL')
    FirstTimeout = msg.get('FirstTimeout', 0)
    Timeout = msg.get('Timeout', 0)
    C.write('  &Com_IPduRxContext_%s,\n' % (msg['name']))
    C.write('  %s, /* RxNotification */\n' % (RxNotification))
    C.write('  %s, /* RxTOut */\n' % (RxTOut))
    C.write('  COM_CONVERT_MS_TO_MAIN_CYCLES(%s), /* FirstTimeout */\n' %
            (FirstTimeout))
    C.write('  COM_CONVERT_MS_TO_MAIN_CYCLES(%s), /* Timeout */\n' % (Timeout))
    C.write('};\n\n')


def gen_tx_msg_cfg(msg, C):
    C.write(
        'static const Com_IPduTxConfigType Com_IPduTxConfig_%s = {\n' % (msg['name']))
    ErrorNotification = msg.get('ErrorNotification', 'NULL')
    TxNotification = msg.get('TxNotification', 'NULL')
    FirstTime = msg.get('FirstTime', 0)
    CycleTime = msg.get('CycleTime', 1000)
    C.write('  &Com_IPduTxContext_%s,\n' % (msg['name']))
    C.write('  %s, /* ErrorNotification */\n' % (ErrorNotification))
    C.write('  %s, /* TxNotification */\n' % (TxNotification))
    C.write('  COM_CONVERT_MS_TO_MAIN_CYCLES(%s), /* FirstTime */\n' %
            (FirstTime))
    C.write('  COM_CONVERT_MS_TO_MAIN_CYCLES(%s), /* CycleTime */\n' %
            (CycleTime))
    C.write('  COM_ECUC_PDUID_OFFSET + COM_PID_%s,\n' % (msg['name']))
    C.write('};\n\n')


def gen_msg(msg, C, network):
    isTx = msg['node'] == network['me']
    C.write('  {\n')
    C.write('    Com_PduData_%s, /* ptr */\n' % (msg['name']))
    C.write('    Com_IPduSignals_%s, /* signals */\n' % (msg['name']))
    if isTx:
        C.write('    NULL, /* rxConfig */\n')
        if network['network'] in ['LIN']:
            C.write('    NULL, /* txConfig */\n')
        else:
            C.write('    &Com_IPduTxConfig_%s, /* txConfig */\n' %
                    (msg['name']))
    else:
        C.write('    &Com_IPduRxConfig_%s, /* rxConfig */\n' % (msg['name']))
        C.write('    NULL, /* txConfig */\n')
    C.write('    Com_IPdu%s_GroupRefMask,\n' % (msg['name']))
    C.write('    sizeof(Com_PduData_%s), /* length */\n' % (msg['name']))
    C.write('    ARRAY_SIZE(Com_IPduSignals_%s), /* numOfSignals */\n' %
            (msg['name']))
    C.write('  },\n')


def Gen_Com(cfg, dir):
    H = open('%s/Com_Cfg.h' % (dir), 'w')
    GenHeader(H)
    H.write('#ifndef COM_CFG_H\n')
    H.write('#define COM_CFG_H\n')
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    H.write('#ifndef COM_MAIN_FUNCTION_PERIOD\n')
    H.write('#define COM_MAIN_FUNCTION_PERIOD 10\n')
    H.write('#endif\n')
    H.write('#define COM_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n')
    H.write('  ((x + COM_MAIN_FUNCTION_PERIOD - 1) / COM_MAIN_FUNCTION_PERIOD)\n\n')
    NTs = []
    for network in cfg['networks']:
        if network['network'] not in NTs:
            NTs.append(network['network'])
    for nt in NTs:
        H.write('#define COM_USE_%s\n' % (nt))
    H.write('//#define COM_USE_SIGNAL_CONFIG\n')
    H.write('#define COM_USE_SIGNAL_UPDATE_BIT\n')
    H.write('\n')
    for network in cfg['networks']:
        H.write('#define COM_RX_FOR_%s(id, PduInfoPtr) \\\n' %
                (network['name']))
        IF = 'if'
        for msg in network['messages']:
            if msg['node'] != network['me']:
                H.write('  %s (0x%X == id) { \\\n' % (IF, msg['id']))
                H.write(
                    '    Com_RxIndication(COM_PID_%s, PduInfoPtr); \\\n' % (msg['name']))
                H.write('  }')
                IF = 'else if'
        H.write('\n\n')
    H.write('#ifndef COM_ECUC_PDUID_OFFSET\n')
    H.write('#define COM_ECUC_PDUID_OFFSET 10\n')
    H.write('#endif\n\n')
    last_end = 'COM_ECUC_PDUID_OFFSET'
    for network in cfg['networks']:
        H.write('/* NOTE: manually modify to fix it to the right HTH */\n')
        H.write('#define COM_ECUC_%s_PDUID_MIN %s\n' %
                (network['name'], last_end))
        H.write('#define COM_ECUC_%s_PDUID_MAX %s + %s\n' %
                (network['name'], last_end, len(network['messages'])))
        last_end = 'COM_ECUC_%s_PDUID_MAX' % (network['name'])
        H.write('#define COM_TX_FOR_%s(TxPduId, dlPdu, PduInfoPtr, ret) \\\n' % (
            network['name']))
        IF = 'if'
        for msg in network['messages']:
            if msg['node'] == network['me']:
                H.write(
                    '  %s ((COM_PID_%s+COM_ECUC_PDUID_OFFSET) == TxPduId) { \\\n' % (IF, msg['name']))
                if network['network'] == 'CAN':
                    H.write('    dlPdu.id = 0x%X; \\\n' % (msg['id']))
                    H.write('    ret = Can_Write(0, &dlPdu); \\\n')
                elif network['network'] == 'LIN':
                    pass
                else:
                    raise
                H.write('  }')
                IF = 'else if'
        H.write('\n\n')
    PDU_ID = 0
    for network in cfg['networks']:
        H.write('/* messages for network %s */\n' % (network['name']))
        for msg in network['messages']:
            H.write('#define COM_PID_%s %s\n' % (msg['name'], PDU_ID))
            PDU_ID += 1
        H.write('\n')
    SIG_ID = 0
    for network in cfg['networks']:
        H.write('/* signals for network %s */\n' % (network['name']))
        for msg in network['messages']:
            H.write('/* signals for message %s: id=0x%X dlc=%d, dir=%s */\n' %
                    (msg['name'], msg['id'], msg['dlc'],
                     'TX' if msg['node'] == network['me'] else 'RX'))
            for sig in msg['signals']:
                H.write('#define COM_%sID_%s %s /* %s %s@%s */\n' % (
                    'G' if sig.get('isGroup', False) else 'S',
                    sig['name'], SIG_ID, sig['endian'], sig['size'], sig['start']))
                SIG_ID += 1
            H.write('\n')
        H.write('\n')
    H.write('/* NOTE: manually modify to create more groups */\n')
    for id, network in enumerate(cfg['networks']):
        H.write('#define COM_GROUP_ID_%s %s\n' % (network['name'], id))
        for msg in network['messages']:
            H.write('#define Com_IPdu%s_GroupRefMask (1<<COM_GROUP_ID_%s)\n' % (
                msg['name'], network['name']))
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
    H.write('#endif /* COM_CFG_H */\n')
    H.close()

    C = open('%s/Com_Cfg.c' % (dir), 'w')
    GenHeader(C)
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#include "Com_Cfg.h"\n')
    C.write('#include "Com.h"\n')
    C.write('#include "Com_Priv.h"\n')
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    for network in cfg['networks']:
        for msg in network['messages']:
            if (msg['node'] == network['me']):
                ErrorNotification = msg.get('ErrorNotification', 'NULL')
                TxNotification = msg.get('TxNotification', 'NULL')
                if ErrorNotification != 'NULL':
                    C.write('extern void %s(void);\n' % (ErrorNotification))
                if TxNotification != 'NULL':
                    C.write('extern void %s(void);\n' % (TxNotification))
                for sig in msg['signals']:
                    ErrorNotification = sig.get('ErrorNotification', 'NULL')
                    TxNotification = sig.get('TxNotification', 'NULL')
                    if ErrorNotification != 'NULL':
                        C.write('extern void %s(void);\n' %
                                (ErrorNotification))
                    if TxNotification != 'NULL':
                        C.write('extern void %s(void);\n' % (TxNotification))
            else:
                RxNotification = msg.get('RxNotification', 'NULL')
                RxTOut = msg.get('RxTOut', 'NULL')
                if RxNotification != 'NULL':
                    C.write('extern void %s(void);\n' % (RxNotification))
                if RxTOut != 'NULL':
                    C.write('extern void %s(void);\n' % (RxTOut))
                for sig in msg['signals']:
                    InvalidNotification = sig.get(
                        'InvalidNotification', 'NULL')
                    RxNotification = sig.get('RxNotification', 'NULL')
                    if RxNotification != 'NULL':
                        C.write('extern void %s(void);\n' %
                                (InvalidNotification))
                    if RxTOut != 'NULL':
                        C.write('extern void %s(void);\n' % (RxNotification))
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    for network in cfg['networks']:
        for msg in network['messages']:
            for sig in msg['signals']:
                gen_signal_init_value(sig, C)
    C.write('\n')
    for network in cfg['networks']:
        for msg in network['messages']:
            C.write('static uint8_t Com_PduData_%s[%s];\n' % (
                msg['name'], msg['dlc']))
            groups = []
            for sig in msg['signals']:
                if 'group' in sig:
                    if (sig['group'] not in groups):
                        groups.append(sig['group'])
            for sig in msg['signals']:
                if sig['name'] in groups:
                    C.write('static uint8_t Com_GrpsData_%s[%s];\n' % (
                        sig['name'], int(sig['size']/8)))

    C.write('\n')
    for network in cfg['networks']:
        for msg in network['messages']:
            if (msg['node'] == network['me']):
                if (network['network'] in ['LIN']):
                    continue
                C.write(
                    'static Com_IPduTxContextType Com_IPduTxContext_%s;\n' % (msg['name']))
            else:
                C.write(
                    'static Com_IPduRxContextType Com_IPduRxContext_%s;\n' % (msg['name']))
    C.write('\n')
    C.write('#ifdef COM_USE_SIGNAL_CONFIG\n')
    for network in cfg['networks']:
        for msg in network['messages']:
            gen_cfg = gen_rx_sig_cfg
            if (msg['node'] == network['me']):  # is Tx message
                gen_cfg = gen_tx_sig_cfg
            for sig in msg['signals']:
                gen_cfg(sig, C)
    C.write('#endif /* COM_USE_SIGNAL_CONFIG */\n')
    C.write('static const Com_SignalConfigType Com_SignalConfigs[] = {\n')
    for network in cfg['networks']:
        for msg in network['messages']:
            for sig in msg['signals']:
                gen_sig(sig, msg, C, msg['node'] == network['me'])
    C.write('};\n\n')
    for network in cfg['networks']:
        for msg in network['messages']:
            C.write(
                'static const Com_SignalConfigType* Com_IPduSignals_%s[] = {\n' % (msg['name']))
            for sig in msg['signals']:
                C.write('  &Com_SignalConfigs[COM_%sID_%s],\n' % (
                    'G' if sig.get('isGroup', False) else 'S',
                    sig['name']))
            C.write('};\n\n')
    for network in cfg['networks']:
        for msg in network['messages']:
            gen_cfg = gen_rx_msg_cfg
            if (msg['node'] == network['me']):  # is Tx message
                gen_cfg = gen_tx_msg_cfg
                if (network['network'] in ['LIN']):
                    continue
            gen_cfg(msg, C)
    C.write('static const Com_IPduConfigType Com_IPduConfigs[] = {\n')
    for network in cfg['networks']:
        for msg in network['messages']:
            gen_msg(msg, C, network)
    C.write('};\n\n')
    C.write('static Com_GlobalContextType Com_GlobalContext;\n')
    C.write('const Com_ConfigType Com_Config = {\n')
    C.write('  Com_IPduConfigs,\n')
    C.write('  Com_SignalConfigs,\n')
    C.write('  &Com_GlobalContext,\n')
    C.write('  ARRAY_SIZE(Com_IPduConfigs),\n')
    C.write('  ARRAY_SIZE(Com_SignalConfigs),\n')
    C.write('  %s /* numOfGroups */,\n' % (len(cfg['networks'])))
    C.write('};\n\n')
    C.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    C.close()


def get_messages(path):
    p = dbc(path)
    return p


def get_signals(signalNames, network):
    signals = []
    for msg in network['messages']:
        for sig in msg['signals']:
            if (sig['name'] in signalNames):
                signals.append(sig)
    return signals


def handle_groups(network):
    if 'groups' not in network:
        return
    for group in network['groups']:
        for groupName, signalNames in group.items():
            signals = get_signals(signalNames, network)
            for sig in signals:
                sig['group'] = groupName
    del network['groups']


def add_group_signal(msg):
    _bebm = []
    for i in range(msg['dlc']):
        for j in range(8):
            _bebm.append(i*8 + 7-j)
    group_signals = {}
    for sig in msg['signals']:
        if 'group' in sig:
            if sig['group'] not in group_signals:
                gsig = {'name': sig['group'], 'endian': sig['endian'],
                        'start': sig['start'], 'size': sig['size'], 'isGroup': True}
                group_signals[sig['group']] = gsig
            else:
                # group signal must be UINT8N
                gsig = group_signals[sig['group']]
                if gsig['endian'] == 'big':
                    index1 = _bebm.index(sig['start'])
                    index2 = _bebm.index(gsig['start'])
                    start = min(index1, index2)
                    lsbIndex1 = ((sig['start'] ^ 0x7) + sig['size'] - 1) ^ 7
                    lsbIndex2 = ((gsig['start'] ^ 0x7) + gsig['size'] - 1) ^ 7
                    index1 = _bebm.index(lsbIndex1)
                    index2 = _bebm.index(lsbIndex2)
                    end = max(index1, index2)
                    start = int(start/8)*8
                    end = end | 0x07
                    gsig['start'] = _bebm[start]
                    gsig['size'] = end - start + 1
                else:
                    start = min(sig['start'], gsig['start'])
                    end = max(gsig['start']+gsig['size'],
                              gsig['start']+gsig['size'])
                    start = int(start/8)*8
                    end = end | 0x07
                    gsig['start'] = start
                    gsig['size'] = end - start + 1
    msg['signals'].extend([sig for _, sig in group_signals.items()])


def post(cfg):
    for network in cfg['networks']:
        for msg in network['messages']:
            if any('group' in sig for sig in msg['signals']):
                add_group_signal(msg)


def extract(cfg, dir):
    cfg_ = {'class': 'Com', 'networks': []}
    bNew = False
    for network in cfg['networks']:
        if 'dbc' in network:
            network_ = dict(network)
            path = network['dbc']
            del network_['dbc']
            if not os.path.isfile(path):
                path = os.path.abspath(os.path.join(dir, '..', path))
            if not os.path.isfile(path):
                raise Exception('File %s not exists' % (path))
            network_['messages'] = get_messages(path)
            handle_groups(network_)
            cfg_['networks'].append(network_)
            bNew = True
        else:
            cfg_['networks'].append(network)
    if bNew:
        with open('%s/Com.json' % (dir), 'w') as f:
            json.dump(cfg_, f, indent=2)
    post(cfg_)
    return cfg_


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), 'GEN')
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    cfg_ = extract(cfg, dir)
    Gen_Com(cfg_, dir)
