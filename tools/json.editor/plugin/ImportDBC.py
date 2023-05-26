# SSAS - Simple Smart Automotive Software
# Copyright (C) 2015 ~ 2023 Parai Wang <parai@foxmail.com>
import sys
import os
from PyQt5 import QtCore, QtGui
from PyQt5.QtGui import *
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from generator.dbc import dbc


class Plugin(QAction):
    def __init__(self, root):
        self.root = root
        super(QAction, self).__init__(root.tr('Import Vector CAN DBC Sinals'), root)
        self.setStatusTip('Import Vector CAN DBC Sinals.')
        self.triggered.connect(self.onAction)

    def onAction(self):
        self.mImportVectorCANDBCSignals()

    def msg_name(self, bus, msg):
        name = msg['name']
        if not name.startswith(bus+'_'):
            name = '_'.join([bus, name])
            msg['name'] = name
        return name

    def updateEcuC(self, bus, messages):
        updated = False
        cfg = self.root.find('EcuC')
        if None == cfg:
            cfg = {'class': 'EcuC', 'Pdus': []}
            updated = True
        names = [x['name'] for x in cfg.get('Pdus', [])]
        for msg in messages:
            name = self.msg_name(bus, msg)
            if name not in names:
                names.append(name)
                cfg['Pdus'].append({'name': name, 'size': 8*msg['dlc']})
                updated = True
        if updated:
            self.root.reload('EcuC', cfg)

    def updateCanIf(self, bus, messages):
        updated = False
        cfg = self.root.find('CanIf')
        if None == cfg:
            cfg = {'class': 'CanIf', 'networks': []}
            updated = True
        network_names = [x['name'] for x in cfg.get('networks', [])]
        if bus not in network_names:
            network = {'name': bus, 'me': self.CAN_SELF_DBC_NODE, 'RxPdus': [], 'TxPdus': []}
            cfg['networks'].append(network)
            updated = True
        else:
            for x in cfg['networks']:
                if x['name'] == bus:
                    network = x
                    break
        rx_names = [x['name'] for x in network.get('RxPdus', [])]
        tx_names = [x['name'] for x in network.get('TxPdus', [])]
        for msg in messages:
            name = self.msg_name(bus, msg)
            if msg['node'] == self.CAN_SELF_DBC_NODE:
                if name not in tx_names:
                    txPdu = {'name': name, 'id': msg['id'], 'hoh': 0, 'dynamic': False, 'up': 'PduR'}
                    network['TxPdus'].append(txPdu)
                    updated = True
            else:
                if name not in rx_names:
                    rxPdu = {'name': name, 'id': msg['id'],
                             'hoh': 0, 'use_mask': False, 'up': 'PduR'}
                    network['RxPdus'].append(rxPdu)
                    updated = True
        if updated:
            self.root.reload('CanIf', cfg)

    def updatePduR(self, bus, messages):
        updated = False
        cfg = self.root.find('PduR')
        if None == cfg:
            cfg = {'class': 'PduR', 'routines': []}
            updated = True
        names = [x['name'] for x in cfg.get('routines', [])]
        for msg in messages:
            name = self.msg_name(bus, msg)
            if name not in names:
                names.append(name)
                if msg['node'] == self.CAN_SELF_DBC_NODE:
                    fr, to = 'Com', 'CanIf'
                else:
                    fr, to = 'CanIf', 'Com'
                cfg['routines'].append({'name': name, 'from': fr, 'to': to})
                updated = True
        if updated:
            self.root.reload('PduR', cfg)

    def updateCom(self, bus, messages):
        updated = False
        cfg = self.root.find('Com')
        if None == cfg:
            cfg = {'class': 'Com', 'nodes': [], 'group_signals': [], 'networks': []}
            updated = True
        network_names = [x['name'] for x in cfg.get('networks', [])]
        if bus not in network_names:
            network = {'name': bus, 'network': 'CAN', 'me': self.CAN_SELF_DBC_NODE, 'messages': []}
            cfg['networks'].append(network)
            updated = True
        else:
            for x in cfg['networks']:
                if x['name'] == bus:
                    network = x
                    break
        message_names = [x['name'] for x in network.get('messages', [])]
        for msg in messages:
            if msg['node'] not in cfg['nodes']:
                cfg['nodes'].append(msg['node'])
                updated = True
            signals = list(msg['signals'])
            name = self.msg_name(bus, msg)
            if name not in message_names:
                bo = dict(msg)
                network['messages'].append(bo)
            else:
                for x in network.get('messages', []):
                    if x['name'] == name:
                        bo = x
                        break
            signal_names = [x['name'] for x in bo.get('signals', [])]
            for signal in signals:
                if signal['name'] not in signal_names:
                    bo['signals'].append(signal)
                    updated = True
        if updated:
            self.root.reload('Com', cfg)

    def mImportVectorCANDBCSignals(self):
        self.CAN_SELF_DBC_NODE = os.getenv('CAN_SELF_DBC_NODE', 'AS')
        dbcFile, _ = QFileDialog.getOpenFileName(None, 'Open Vector CAN DBC',
                                                 '', '*.dbc', '*.dbc',
                                                 QFileDialog.DontResolveSymlinks)
        if (os.path.exists(dbcFile)):
            bus = os.path.basename(dbcFile)[:-4]
            messages = dbc(dbcFile)
            self.updateEcuC(bus, messages)
            self.updateCanIf(bus, messages)
            self.updatePduR(bus, messages)
            self.updateCom(bus, messages)
