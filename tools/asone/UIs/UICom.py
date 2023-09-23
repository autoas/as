# SSAS - Simple Smart Automotive Software
# Copyright (C) 2015 - 2023 Parai Wang <parai@foxmail.com>
from PyQt5 import QtCore, QtGui
from PyQt5.QtGui import *
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from one.assignal import *
import re
import time
import math
import random
import json
import os
import sys

CWD = os.path.dirname(__file__)
if (CWD == ''):
    CWD = os.path.abspath('.')
if os.path.exists('%s/AniView' % (CWD)):
    sys.path.append(CWD)
    from AniView import ANI_VIEWS
else:
    ANI_VIEWS = {}

__all__ = ['UICom']


class SignalGenerator():
    def __init__(self, sig):
        self.sig = sig
        self.leData = QLineEdit('0')
        self.cbxLabel = QCheckBox(sig['name'])
        self.cmbxGenerator = QComboBox()
        self.min = self.sig.get_min()
        self.max = self.sig.get_max()
        self.generator = self.nan
        self.reGen = re.compile(
            r'(\w+)\s+from\s+(\w+)\s+to\s+(\w+)\s+period\s+(\w+)\s+s')
        pfix = ' from %s to %s period 5 s' % (self.min, self.max)
        self.cmbxGenerator.addItems(['NAN', 'sin'+pfix, 'rand'+pfix])
        self.cmbxGenerator.setEditable(True)
        self.cmbxGenerator.setVisible(False)

        self.cbxLabel.stateChanged.connect(self.on_cbxLabel_stateChanged)
        self.cmbxGenerator.currentIndexChanged.connect(
            self.on_cmbxGenerator_currentIndexChanged)

        self.value = 0

    def on_cbxLabel_stateChanged(self, state):
        if (state):
            self.leData.setVisible(False)
            self.cmbxGenerator.setVisible(True)
        else:
            self.leData.setVisible(True)
            self.cmbxGenerator.setVisible(False)

    def nan(self):
        return 0

    def sin(self):
        now = time.time()
        if (now-self.preTime >= self.period):
            self.preTime = now
        v = math.sin((now-self.preTime)/self.period*math.pi) * \
            (self.max-self.min)+self.min
        return int(v)

    def rand(self):
        v = random.random()*(self.max-self.min)+self.min
        return int(v)

    def on_cmbxGenerator_currentIndexChanged(self, index):
        text = str(self.cmbxGenerator.currentText())
        if (self.reGen.search(text)):
            grp = self.reGen.search(text).groups()
            if (grp[0] == 'sin'):
                self.generator = self.sin
            elif (grp[0] == 'rand'):
                self.generator = self.rand
            else:
                self.generator = self.nan
                print('invalid generator', text)
            self.min = float(grp[1])
            self.max = float(grp[2])
            self.period = float(grp[3])
            self.preTime = time.time()

    def text(self):
        if (self.cbxLabel.isChecked()):
            return str(self.generator())
        else:
            return self.leData.text()


class UIMsg(QScrollArea):
    def __init__(self, msg, parent=None):
        super(QScrollArea, self).__init__(parent)
        self.msg = msg
        wd = QWidget()
        grid = QGridLayout()
        self.leData = {}
        row = col = 0
        for sig in self.msg:
            if (self.msg.IsTransmit()):
                sigGen = SignalGenerator(sig)
                grid.addWidget(sigGen.cbxLabel, row, col)
                grid.addWidget(sigGen.leData, row, col+1)
                grid.addWidget(sigGen.cmbxGenerator, row, col+1)
                self.leData[sig['name']] = sigGen
            else:
                grid.addWidget(QLabel(sig['name']), row, col)
                self.leData[sig['name']] = QLineEdit('0')
                grid.addWidget(self.leData[sig['name']], row, col+1)
            col += 2
            if (col > 3):
                col = 0
                row += 1
        row += 1

        grid.addWidget(QLabel('period(ms):'), row, 0)
        self.lePeriod = QLineEdit('%s' % (self.msg.get_period()))
        grid.addWidget(self.lePeriod, row, 1)
        self.btnUpdate = QPushButton('update')
        self.btnUpdate.setToolTip('if period is 0, will update message and send or read it once\n'
                                  'if period is not 0, will only update the period and message value')
        grid.addWidget(self.btnUpdate, row, 2)
        self.btnUpdate.clicked.connect(self.on_btnUpdate_clicked)
        if not self.msg.IsTransmit():
            for sig in self.msg:
                self.leData[sig['name']].setReadOnly(True)
        wd.setLayout(grid)
        self.setWidget(wd)

    def toInteger(self, strnum):
        if (strnum.find('0x') != -1 or strnum.find('0X') != -1):
            return int(strnum, 16)
        else:
            return int(strnum, 10)

    def updateMsg(self):
        for sig in self.msg:
            try:
                sig.set_value(self.toInteger(
                    str(self.leData[sig['name']].text())))
            except ValueError:
                pass  # as just type '0x' maybe
        period = self.toInteger(str(self.lePeriod.text()))
        self.msg.set_period(period)

    def on_btnUpdate_clicked(self):
        self.updateMsg()
        if (self.msg.get_period() == 0):
            if self.msg.IsTransmit():
                self.msg.transmit()

    def Period(self):
        if (self.msg.IsTransmit()):
            self.updateMsg()
        else:
            for sig in self.msg:
                self.leData[sig['name']].setText(
                    '%s(%s)' % (hex(sig.get_value()), sig.get_value()))


class UICom(QWidget):
    def __init__(self, parent=None):
        super(QWidget, self).__init__(parent)
        self.vbox = QVBoxLayout()

        hbox = QHBoxLayout()
        hbox.addWidget(QLabel('load COM json:'))
        self.leComJs = QLineEdit()
        hbox.addWidget(self.leComJs)
        self.btnOpenComJs = QPushButton('...')
        hbox.addWidget(self.btnOpenComJs)
        self.btnStart = QPushButton('start')
        hbox.addWidget(self.btnStart)
        self.vbox.addLayout(hbox)

        hbox = QHBoxLayout()
        hbox.addWidget(QLabel('AniView:'))
        self.cmbxSignals = QComboBox()
        self.cmbxSignals.setEditable(True)
        self.cmbxSignals.setMinimumWidth(300)
        hbox.addWidget(self.cmbxSignals)
        self.cmbxSignals.currentIndexChanged.connect(
            self.on_cmbxSignals_currentIndexChanged)
        hbox.addWidget(QLabel('scale:'))
        self.leScale = QLineEdit()
        self.leScale.setText('1')
        hbox.addWidget(self.leScale)
        hbox.addWidget(QLabel('offset:'))
        self.leOffset = QLineEdit()
        self.leOffset.setText('0')
        hbox.addWidget(self.leOffset)
        self.btnView = QPushButton('View')
        hbox.addWidget(self.btnView)
        self.btnView.clicked.connect(self.on_btnView_clicked)
        self.vbox.addLayout(hbox)

        self.tabWidget = QTabWidget(self)
        self.vbox.addWidget(self.tabWidget)

        self.setLayout(self.vbox)

        self.networks = []
        self.btnOpenComJs.clicked.connect(self.on_btnOpenComJs_clicked)
        self.btnStart.clicked.connect(self.on_btnStart_clicked)

        defJs = os.path.abspath('%s/../../../app/app/config/Com/GEN/Com.json' % (CWD))
        if os.path.isfile(defJs):
            self.leComJs.setText(defJs)
        self.qviews = {}

    def __del__(self):
        for network in self.networks:
            network.stop()

    def timerEvent(self, ev):
        for key, msg in self.msgs.items():
            msg.Period()

    def on_cmbxSignals_currentIndexChanged(self, index):
        self.leScale.setText('1')
        self.leOffset.setText('0')

    def on_btnView_clicked(self):
        if (len(self.networks) == 0):
            return
        signal = str(self.cmbxSignals.currentText())
        scale = float(self.leScale.text())
        offset = float(self.leOffset.text())
        for network in self.networks:
            sig = network.lookup(signal)
            if (sig != None):
                print('view of signal:', signal)
                self.qviews[sig.name] = QView(sig, scale, offset)
                return
        for network in self.networks:
            for msg in network:
                if msg.name == signal:
                    self.qviews[msg.name] = ANI_VIEWS[msg.name](msg)
                    return

        print('can\'t find signal:', signal)

    def on_btnOpenComJs_clicked(self):
        rv = QFileDialog.getOpenFileName(
            None, 'COM configuration', '', 'COM configuration (*.json)')
        if (rv[0] != ''):
            self.leComJs.setText(defJs)

    def on_btnStart_clicked(self):
        if self.btnStart.text() == 'start':
            self.loadComJs()
            self.btnStart.setText('stop')
        else:
            self.tabWidget.clear()
            for network in self.networks:
                network.stop()
            self.networks = []
            self.btnStart.setText('start')

    def loadComJs(self):
        comjs = self.leComJs.text()
        self.tabWidget.clear()
        for network in self.networks:
            network.stop()
        with open(comjs) as f:
            cfg = json.load(f)
        self.msgs = {}
        signals = []
        for net in cfg['networks']:
            network = Network(net)
            for msg in network:
                if msg.name in ANI_VIEWS:
                    signals.append(msg.name)
            for msg in network:
                self.msgs[msg.name] = UIMsg(msg)
                self.tabWidget.addTab(self.msgs[msg.name], msg.name)
                for sig in msg:
                    signals.append(sig['name'])
            self.networks.append(network)
        self.cmbxSignals.clear()
        self.cmbxSignals.addItems(signals)
        self.startTimer(100)


def get():
    return 'Com', UICom
