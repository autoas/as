# SSAS - Simple Smart Automotive Software
# Copyright (C) 2015 - 2023 Parai Wang <parai@foxmail.com>

from PyQt5 import QtCore, QtGui
from PyQt5.QtGui import *
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from one.AsPy import can as AsCan
import sys
import time

__all__ = ['UICan']


class UICan(QWidget):
    bus_num = 4

    def __init__(self, parent=None):
        super(QWidget, self).__init__(parent)
        vbox = QVBoxLayout()
        grid = QGridLayout()
        self.cmbxCanDevice = []
        self.cmbxCanPort = []
        self.cmbxCanBaud = []
        self.btnOpen = []
        self.online = []
        self.nodes = []

        opens = [self.on_btnOpenClicked_0, self.on_btnOpenClicked_1,
                 self.on_btnOpenClicked_2, self.on_btnOpenClicked_3]
        for i in range(self.bus_num):
            self.cmbxCanDevice.append(QComboBox())
            self.cmbxCanPort.append(QComboBox())
            self.cmbxCanBaud.append(QComboBox())
            self.btnOpen.append(QPushButton('Open'))
            self.cmbxCanDevice[i].addItems(
                ['simulator_v2', 'peak', 'vxl', 'zlg', 'qemu'])
            self.cmbxCanPort[i].addItems(
                ['port 0', 'port 1', 'port 2', 'port 3', 'port 4', 'port 5', 'port 6', 'port 7'])
            self.cmbxCanBaud[i].addItems(
                ['125000', '250000', '500000', '1000000'])
            self.cmbxCanBaud[i].setCurrentIndex(2)
            self.cmbxCanDevice[i].setEditable(True)
            self.cmbxCanPort[i].setEditable(True)
            self.cmbxCanBaud[i].setEditable(True)

            grid.addWidget(QLabel('CAN%s:' % (i)), i, 0)
            grid.addWidget(self.cmbxCanDevice[i], i, 1)
            grid.addWidget(self.cmbxCanPort[i], i, 2)
            grid.addWidget(self.cmbxCanBaud[i], i, 3)
            grid.addWidget(self.btnOpen[i], i, 4)

            self.btnOpen[i].clicked.connect(opens[i])
            self.online.append(False)
            self.nodes.append(None)
        hbox = QHBoxLayout()
        hbox.addWidget(QLabel('BUS ID:'))
        self.cmbxBusID = QComboBox()
        self.cmbxBusID.addItems(['CAN0', 'CAN1', 'CAN2', 'CAN3'])
        hbox.addWidget(self.cmbxBusID)
        hbox.addWidget(QLabel('CAN ID:'))
        self.leCanID = QLineEdit()
        self.leCanID.setMaximumWidth(120)
        self.leCanID.setText('0')
        self.leCanID.setStatusTip('hex value')
        hbox.addWidget(self.leCanID)
        hbox.addWidget(QLabel('DATA:'))
        self.leCanData = QLineEdit()
        self.leCanData.setText('1122334455667788')
        hbox.addWidget(self.leCanData)
        hbox.addWidget(QLabel('PERIOD:'))
        self.leCanPeriod = QLineEdit()
        self.leCanPeriod.setText('0')
        hbox.addWidget(self.leCanPeriod)
        self.leCanPeriod.setMaximumWidth(120)
        self.btnSend = QPushButton('Send')
        hbox.addWidget(self.btnSend)
        self.btnSend.clicked.connect(self.on_btnSend_clicked)

        self.canTrace = QTextEdit()
        self.canTrace.setReadOnly(True)
        self.canTraceEnable = QCheckBox("CAN trace log enable")
        self.canTraceEnable.setChecked(False)
        self.canTraceClear = QPushButton('Clear')

        vbox.addLayout(grid)
        vbox.addLayout(hbox)
        hbox2 = QHBoxLayout()
        hbox2.addWidget(self.canTraceEnable)
        hbox2.addWidget(self.canTraceClear)
        vbox.addLayout(hbox2)
        vbox.addWidget(self.canTrace)
        self.canTraceEnable.stateChanged.connect(
            self.on_canTraceEnable_stateChanged)
        self.canTraceClear.clicked.connect(self.on_canTraceClear_clicked)
        self.setLayout(vbox)

        self.canSendTimer = None
        self.traceTimer = None

    def canSend(self):
        busid = self.cmbxBusID.currentIndex()
        if self.nodes[busid] != None:
            canid = int(str(self.leCanID.text()), 16)
            data = str(self.leCanData.text())
            rdata = []
            for i in range(int(len(data)/2)):
                rdata.append(int(data[2*i:2*i+2], 16))
            self.nodes[busid].write(canid, bytes(rdata))

    def on_btnSend_clicked(self):
        period = int(str(self.leCanPeriod.text()), 10)
        if(period > 0):
            if(self.canSendTimer != None):
                self.killTimer(self.canSendTimer)
            self.canSendTimer = self.startTimer(period)
        self.canSend()

    def on_canTraceClear_clicked(self):
        self.canTrace.clear()

    def on_canTraceEnable_stateChanged(self, state):
        if(state):
            self.traceTimer = self.startTimer(1)
            self.timeTick = time.time()
        else:
            self.killTimer(self.traceTimer)
            self.traceTimer = None

    def timerEvent(self, ev):
        if(ev.timerId() == self.traceTimer):
            self.onTraceTimerEvent()
        elif(ev.timerId() == self.canSendTimer):
            self.canSend()
            period = int(str(self.leCanPeriod.text()), 10)
            if(period == 0):
                self.killTimer(self.canSendTimer)
                self.canSendTimer = None
        else:
            assert(0)

    def onTraceTimerEvent(self):
        for bus in range(self.bus_num):
            if(self.online[bus]):
                ercd, canid, data = self.nodes[bus].read(-2)
                if(ercd):
                    cstr = 'bus=%s canid=%03X data=[' % (bus, canid)
                    for d in data:
                        cstr += ' %02X' % (d)
                    cstr += '] @ %.4f s' % (time.time() - self.timeTick)
                    self.canTrace.append(cstr)

    def on_btnOpen(self, id):
        if(str(self.btnOpen[id].text()) == 'Open'):
            device = str(self.cmbxCanDevice[id].currentText())
            port = int(
                str(self.cmbxCanPort[id].currentText()).replace('port', ''))
            baud = int(str(self.cmbxCanBaud[id].currentText()))
            self.nodes[id] = AsCan(device, port, baud)
            self.btnOpen[id].setText('Close')
            self.online[id] = True
        else:
            self.btnOpen[id].setText('Open')
            self.online[id] = False
            self.nodes[id] = None

    def on_btnOpenClicked_0(self):
        self.on_btnOpen(0)

    def on_btnOpenClicked_1(self):
        self.on_btnOpen(1)

    def on_btnOpenClicked_2(self):
        self.on_btnOpen(2)

    def on_btnOpenClicked_3(self):
        self.on_btnOpen(3)


def get():
    return 'CAN', UICan
