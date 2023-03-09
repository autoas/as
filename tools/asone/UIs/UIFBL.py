# SSAS - Simple Smart Automotive Software
# Copyright (C) 2015 - 2023 Parai Wang <parai@foxmail.com>
import os
from PyQt5 import QtCore, QtGui
from PyQt5.QtGui import *
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *

from one.AsPy import loader

CWD = os.path.abspath(os.path.dirname(__file__))

__all__ = ['UIFBL']


class UIFBL(QWidget):
    def __init__(self, parent=None):
        super(QWidget, self).__init__(parent)
        vbox = QVBoxLayout()
        # device configuration
        grid = QGridLayout()
        grid.addWidget(QLabel('protocol:'), 0, 0)
        self.cmbxProtocol = QComboBox()
        self.cmbxProtocol.addItems(['CAN', 'CANFD'])
        self.cmbxProtocol.setEditable(True)
        grid.addWidget(self.cmbxProtocol, 0, 1)
        grid.addWidget(QLabel('device:'), 0, 2)
        self.cmbxDevice = QComboBox()
        self.cmbxDevice.addItems(['simulator_v2', 'qemu', 'peak', 'vxl', 'zlg'])
        self.cmbxDevice.setEditable(True)
        grid.addWidget(self.cmbxDevice, 0, 3)
        grid.addWidget(QLabel('port:'), 0, 4)
        self.cmbxDevicePort = QComboBox()
        self.cmbxDevicePort.addItems([str(i) for i in range(4)])
        self.cmbxDevicePort.setEditable(True)
        grid.addWidget(self.cmbxDevicePort, 0, 5)
        grid.addWidget(QLabel('baurdare:'), 0, 6)
        self.cmbxDeviceBaudrate = QComboBox()
        self.cmbxDeviceBaudrate.addItems(['125000', '250000', '500000', '1000000'])
        self.cmbxDeviceBaudrate.setCurrentIndex(2)
        self.cmbxDeviceBaudrate.setEditable(True)
        grid.addWidget(self.cmbxDeviceBaudrate, 0, 7)
        grid.addWidget(QLabel('TxId:'), 1, 0)
        self.leTxId = QLineEdit('0x731')
        grid.addWidget(self.leTxId, 1, 1)
        grid.addWidget(QLabel('RxId:'), 1, 2)
        self.leRxId = QLineEdit('0x732')
        grid.addWidget(self.leRxId, 1, 3)
        grid.addWidget(QLabel('FuncAddr:'), 1, 4);
        self.leFuncAddr = QLineEdit('0x7DF');
        grid.addWidget(self.leFuncAddr, 1, 5);
        self.cbxDebug = QCheckBox('debug')
        grid.addWidget(self.cbxDebug, 1, 7)

        # application and flash driver selection
        vbox.addLayout(grid)
        grid = QGridLayout()
        grid.addWidget(QLabel('Application'), 0, 0)
        self.leApplication = QLineEdit()
        grid.addWidget(self.leApplication, 0, 1)
        self.btnOpenApp = QPushButton('...')
        grid.addWidget(self.btnOpenApp, 0, 2)

        grid.addWidget(QLabel('Flash Driver'), 1, 0)
        self.leFlsDrv = QLineEdit()
        grid.addWidget(self.leFlsDrv, 1, 1)
        self.btnOpenFlsDrv = QPushButton('...')
        grid.addWidget(self.btnOpenFlsDrv, 1, 2)

        grid.addWidget(QLabel('Progress'), 2, 0)
        self.pgbProgress = QProgressBar()
        self.pgbProgress.setRange(0, 100)
        grid.addWidget(self.pgbProgress, 2, 1)
        self.btnStart = QPushButton('start')
        grid.addWidget(self.btnStart, 2, 2)
        vbox.addLayout(grid)
        self.leinfor = QTextEdit()
        self.leinfor.setReadOnly(True)
        vbox.addWidget(self.leinfor)
        self.setLayout(vbox)

        app = '%s/../../../build/%s/QemuVersatilepbGCC/VersatilepbCanApp/VersatilepbCanApp.s19.sign' % (
            CWD, os.name)
        fls = '%s/../../../build/%s/QemuVersatilepbGCC/VersatilepbFlashDriver/VersatilepbFlashDriver.s19.sign' % (
            CWD, os.name)
        app = os.path.abspath(app)
        fls = os.path.abspath(fls)
        if(os.path.exists(app)):
            self.leApplication.setText(app)
        if(os.path.exists(fls)):
            self.leFlsDrv.setText(fls)
        self.btnOpenApp.clicked.connect(self.on_btnOpenApp_clicked)
        self.btnOpenFlsDrv.clicked.connect(self.on_btnOpenFlsDrv_clicked)
        self.btnStart.clicked.connect(self.on_btnStart_clicked)

    def on_btnOpenApp_clicked(self):
        app = str(self.leApplication.text())
        apppath = os.path.dirname(app)
        rv = QFileDialog.getOpenFileName(
            None, 'application file', apppath, 'application (*.s19 *.s19.sign *.bin *.mot)')
        self.leApplication.setText(rv[0])

    def on_btnOpenFlsDrv_clicked(self):
        fls = str(self.leFlsDrv.text())
        flspath = os.path.dirname(fls)
        rv = QFileDialog.getOpenFileName(
            None, 'flash driver file', flspath, 'flash driver (*.s19 *.s19.sign *.bin *.mot)')
        self.leFlsDrv.setText(rv[0])

    def on_btnStart_clicked(self):
        if(os.path.exists(str(self.leApplication.text()))):
            self.pgbProgress.setValue(1)
            args = {}
            protocol = str(self.cmbxProtocol.currentText())
            if self.cbxDebug.isChecked():
                logLevel = 0
            else:
                logLevel = 1
            if 'CANFD' == protocol:
                protocol = 'CAN'
                LL_DL = 64
            else:
                LL_DL = 8
            args['protocol'] = protocol
            args['LL_DL'] = LL_DL
            args['device'] = str(self.cmbxDevice.currentText())
            args['port'] = eval(str(self.cmbxDevicePort.currentText()))
            args['baudrate'] = eval(str(self.cmbxDeviceBaudrate.currentText()))
            args['txid'] = eval(str(self.leTxId.text()))
            args['rxid'] = eval(str(self.leRxId.text()))
            args['app'] = str(self.leApplication.text())
            args['fls'] = str(self.leFlsDrv.text())
            args['logLevel'] = logLevel
            args['funcAddr'] = eval(str(self.leFuncAddr.text()))
            try:
                self.leinfor.clear()
                self.loader = loader(**args)
                self.loader.start()
                self.startTimer(10)
                self.btnStart.setDisabled(True)
            except Exception as e:
                QMessageBox(QMessageBox.Critical, 'Error',
                            'failed to lauch loader: %s' % (e)).exec_()
        else:
            QMessageBox.information(
                self, 'Tips', 'Please load a valid application first!')

    def timerEvent(self, ev):
        if getattr(self, 'loader', None):
            r, progress, msg = self.loader.poll()
            if msg:
                self.leinfor.insertPlainText(msg)
                self.leinfor.moveCursor(QTextCursor.End)
            self.pgbProgress.setValue(progress)
            if False == r:
                self.loader = None
                self.leinfor.append('>>> FAIL <<< ')
                QMessageBox(QMessageBox.Critical, 'Error',
                            'failed to upgrade app, check .AsPy.log for details').exec_()
                self.btnStart.setDisabled(False)
            if progress >= 100.0:
                self.loader = None
                self.leinfor.append('>>> SUCCESS <<< ')
                self.btnStart.setDisabled(False)


class UIFBLs(QWidget):
    def __init__(self, parent=None):
        super(QWidget, self).__init__(parent)
        sc = QScrollArea()
        sc.setWidgetResizable(True)
        wd = QWidget()
        grid = QGridLayout()
        for i in range(6):
            fbl = UIFBL(self)
            grid.addWidget(fbl, int(i/2), i % 2)
        wd.setLayout(grid)
        sc.setWidget(wd)
        vbox = QVBoxLayout()
        vbox.addWidget(sc)
        self.setLayout(vbox)


def get():
    return 'FBL', UIFBL
