# SSAS - Simple Smart Automotive Software
# Copyright (C) 2015 ~ 2023 Parai Wang <parai@foxmail.com>
import os

from PyQt5 import QtCore, QtGui
from PyQt5.QtCore import *
from PyQt5.QtGui import *
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
        name = msg['Name']
        if not name.startswith(bus+'_'):
            name = '_'.join([bus, name])
            msg['Name'] = name
        return name

    def updateEcuC(self, bus, messages):
        updated = False
        cfg = self.root.find('EcuC')
        if None == cfg:
            cfg = {'Class': 'EcuC', 'Pdus': []}
            updated = True
        names = [x['Name'] for x in cfg.get('Pdus', [])]
        for msg in messages:
            name = self.msg_name(bus, msg)
            if name not in names:
                names.append(name)
                cfg['Pdus'].append({'Name': name, 'Size': 8*msg['Dlc']})
                updated = True
        if updated:
            self.root.reload('EcuC', cfg)

    def updateCanIf(self, bus, messages):
        updated = False
        cfg = self.root.find('CanIf')
        if None == cfg:
            cfg = {'Class': 'CanIf', 'Networks': []}
            updated = True
        network_names = [x['Name'] for x in cfg.get('Networks', [])]
        if bus not in network_names:
            network = {'Name': bus, 'Me': self.CAN_SELF_DBC_NODE, 'RxPdus': [], 'TxPdus': []}
            cfg['Networks'].append(network)
            updated = True
        else:
            for x in cfg['Networks']:
                if x['Name'] == bus:
                    network = x
                    break
        rx_names = [x['Name'] for x in network.get('RxPdus', [])]
        tx_names = [x['Name'] for x in network.get('TxPdus', [])]
        for msg in messages:
            name = self.msg_name(bus, msg)
            if msg['Node'] == self.CAN_SELF_DBC_NODE:
                if name not in tx_names:
                    txPdu = {'Name': name, 'Id': msg['Id'], 'Hoh': 0, 'Dynamic': False, 'Up': 'PduR'}
                    network['TxPdus'].append(txPdu)
                    updated = True
            else:
                if name not in rx_names:
                    rxPdu = {'Name': name, 'Id': msg['Id'],
                             'Hoh': 0, 'UseMask': False, 'Up': 'PduR'}
                    network['RxPdus'].append(rxPdu)
                    updated = True
        if updated:
            self.root.reload('CanIf', cfg)

    def updatePduR(self, bus, messages):
        updated = False
        cfg = self.root.find('PduR')
        if None == cfg:
            cfg = {'Class': 'PduR', 'Routines': []}
            updated = True
        names = [x['Name'] for x in cfg.get('Routines', [])]
        for msg in messages:
            name = self.msg_name(bus, msg)
            if name not in names:
                names.append(name)
                if msg['Node'] == self.CAN_SELF_DBC_NODE:
                    fr, to = 'Com', 'CanIf'
                else:
                    fr, to = 'CanIf', 'Com'
                cfg['Routines'].append({'Name': name, 'From': fr, 'To': to})
                updated = True
        if updated:
            self.root.reload('PduR', cfg)

    def updateCom(self, bus, messages):
        updated = False
        cfg = self.root.find('Com')
        if None == cfg:
            cfg = {'Class': 'Com', 'Nodes': [], 'GroupSignals': [], 'Networks': []}
            updated = True
        network_names = [x['Name'] for x in cfg.get('Networks', [])]
        if bus not in network_names:
            network = {'Name': bus, 'Network': 'CAN', 'Me': self.CAN_SELF_DBC_NODE, 'Messages': []}
            cfg['Networks'].append(network)
            updated = True
        else:
            for x in cfg['Networks']:
                if x['Name'] == bus:
                    network = x
                    break
        message_names = [x['Name'] for x in network.get('Messages', [])]
        for msg in messages:
            if msg['Node'] not in cfg['Nodes']:
                cfg['Nodes'].append(msg['Node'])
                updated = True
            signals = list(msg['Signals'])
            name = self.msg_name(bus, msg)
            if name not in message_names:
                bo = dict(msg)
                network['Messages'].append(bo)
            else:
                for x in network.get('Messages', []):
                    if x['Name'] == name:
                        bo = x
                        break
            signal_names = [x['Name'] for x in bo.get('Signals', [])]
            for signal in signals:
                if signal['Name'] not in signal_names:
                    bo['Signals'].append(signal)
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
