# SSAS - Simple Smart Automotive Software
# Copyright (C) 2015 - 2023 Parai Wang <parai@foxmail.com>
import os
from PyQt5 import QtCore, QtGui
from PyQt5.QtGui import *
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
import json
from bitarray import bitarray
from one.dcm import dcm

CWD = os.path.abspath(os.path.dirname(__file__))

__all__ = ['UIDcm']


def FormatMessage(res):
    ss = "["
    for v in res.toarray():
        ss += '%02X,' % (v)
    ss += ']'
    return ss


class dcmbits():
    def __init__(self):
        self.bits = bitarray()

    def append(self, d, num=1):
        for i in range(num):
            if ((d & (1 << (num-1-i))) != 0):
                self.bits.append(True)
            else:
                self.bits.append(False)

    def toint(self, pos, num):
        bits = self.bits[pos:pos+num]
        nbyte = int((num+7)/8)
        left = nbyte*8 - num
        v = 0
        for b in bits.tobytes():
            v = (v << 8) + b
        v = v >> left
        return v

    def toarray(self):
        return self.bits.tobytes()


class wDataUS(QComboBox):
    '''Data UxxSelect, 0<xx<=32'''

    def __init__(self, data, parent):
        super(QComboBox, self).__init__(parent)
        self.dcm = parent
        self.Data = data
        self.OptionInvalid = 0
        list = []
        for select in self.Data['Select']:
            list.append(select['name'])
            self.OptionInvalid += 1
        list.append('Invalid')
        self.addItems(list)

        if 'default' in self.Data:
            default = self.Data['default']
            self.setCurrentIndex(default)

    def getValue(self, data):
        index = self.currentIndex()

        si = 0
        svalue = None
        for select in self.Data['Select']:
            if (index == si):
                svalue = select['value']
                break
            else:
                si = si + 1
        assert (svalue)

        d = eval(svalue)
        a = dcmbits()
        num = eval(self.Data['type'][1:-6])
        data.append(d, num)

    def setValue(self, data, start):
        try:
            num = eval(self.Data['type'][1:-6])
            value = data.toint(start, num)
            start += num

        except IndexError:
            QMessageBox(QMessageBox.Critical, 'Error', 'Data record witn Invalid Length  %s.' % (
                self.dcm.get_response())).exec_()
            return
        index = 0
        for select in self.Data['Select']:
            if (eval(select['value']) == value):
                break
            else:
                index += 1
        self.setCurrentIndex(index)
        return start


class wDataU(QLineEdit):
    '''Data Uxx UxxArray 0<xx<=32'''

    def __init__(self, data, parent):
        super(QLineEdit, self).__init__(parent)
        self.dcm = parent
        self.Data = data
        if 'default' in data:
            self.setText('%s' % (data['default']))
        else:
            self.setText('0')

    def setValue(self, data, start):
        try:
            if (self.Data['type'][-5:] == 'Array'):
                num = eval(self.Data['type'][1:-5])
                value = '[ '
                if (self.Data['display'] == 'asc'):
                    value = 'text='
                size = self.Data['size']
                for i in range(0, size):
                    v = data.toint(start, num)
                    if (self.Data['display'] == 'hex'):
                        value += '0x%X,' % (v)
                    elif (self.Data['display'] == 'asc'):
                        value += '%c' % (v)
                    else:
                        value += '%d,' % (v)
                    start += num
                if (self.Data['display'] != 'asc'):
                    value = value[:-1] + ' ]'

            else:
                num = eval(self.Data['type'][1:])
                value = data.toint(start, num)
                start += num
        except IndexError:
            QMessageBox(QMessageBox.Critical, 'Error', 'Data record witn Invalid Length  %s.' % (
                self.dcm.get_response())).exec_()
            return
        if (self.Data['type'][-5:] == 'Array'):
            self.setText(value)
        else:
            if (self.Data['display'] == 'hex'):
                self.setText('0x%X' % (value))
            else:
                self.setText('%d' % (value))
        return start

    def getValue(self, data):
        stype = self.Data['type']
        if (stype[-5:] == 'Array'):
            num = eval(stype[1:-5])
            assert (num <= 32)
            size = self.Data['size']
            string = str(self.text())
            if (string[:5] == 'text='):
                va = []
                for c in string[5:]:
                    va.append(ord(c))
            else:
                string = string.replace('[', '').replace(']', '')
                grp = string.split(',')
                va = []
                for g in grp:
                    va.append(eval(g))
            for i in range(0, size):
                try:
                    data.append(va[i], num)
                except:
                    # print(traceback.format_exc())
                    data.append(0, num)
        else:
            d = eval(str(self.text()))
            num = eval(stype[1:])
            assert (num <= 32)
            data.append(d, num)


class UISessionControl(QGroupBox):
    def __init__(self, service, parent):
        super(QGroupBox, self).__init__("session control", parent)
        self.service = service
        self.dcm = parent
        self.cmbxSessions = QComboBox()
        for sesNmae, _ in service.items():
            self.service[sesNmae] = eval(self.service[sesNmae])
            self.cmbxSessions.addItem(sesNmae)
        grid = QGridLayout()
        grid.addWidget(QLabel('Session:'), 0, 0)
        self.btnEnter = QPushButton('Enter')
        grid.addWidget(self.cmbxSessions, 0, 1)
        grid.addWidget(self.btnEnter, 0, 2)
        self.setLayout(grid)
        self.btnEnter.clicked.connect(self.on_btnEnter_clicked)

    def on_btnEnter_clicked(self):
        data = dcmbits()
        data.append(0x10, 8)
        session = str(self.cmbxSessions.currentText())
        id = self.service[session]
        data.append(id, 8)
        res = self.dcm.transmit(data.toarray())
        if (res == None):
            return
        if (res.toarray()[0] != 0x50):
            QMessageBox(QMessageBox.Critical, 'Error', 'SessionControl Failed!  %s.' % (
                self.dcm.get_last_error())).exec_()
        else:
            QMessageBox(QMessageBox.Information, 'Info', 'SessionControl okay with response %s' % (
                FormatMessage(res))).exec_()


class UISecurityAccess(QGroupBox):
    def __init__(self, service, parent=None):
        super(QGroupBox, self).__init__('security access', parent)
        self.service = service
        self.dcm = parent
        self.cmbxSecurityLevels = QComboBox()
        for name, _ in service.items():
            self.cmbxSecurityLevels.addItem(name)
        grid = QGridLayout()
        grid.addWidget(QLabel('Security Level:'), 0, 0)
        self.btnUnlock = QPushButton('Unlock')
        grid.addWidget(self.cmbxSecurityLevels, 0, 1)
        grid.addWidget(self.btnUnlock, 0, 2)
        self.setLayout(grid)
        self.btnUnlock.clicked.connect(self.on_btnUnlock_clicked)

    def Seed2Key(self, Level, res):
        alg = Level['algorithm']
        if (type(alg) == list):
            alg = '\n'.join(alg)
        if (alg.startswith('def')):
            alg = alg.replace('\\n', '\n')
            alg = alg.replace('$LSL', '>>')
            alg = alg.replace('$LSR', '<<')
            alg = alg.replace('$AND', '&')
            fp = open('./SeedToKey.py', 'w')
            fp.write(alg)
            fp.close()
            import SeedToKey
            return SeedToKey.CalculateKey(res.toarray())
        else:
            assert (0)

    def on_btnUnlock_clicked(self):
        data = dcmbits()
        data.append(0x27, 8)
        levelName = str(self.cmbxSecurityLevels.currentText())
        Level = self.service[levelName]
        levelV = Level['level']
        data.append(levelV, 8)
        res = self.dcm.transmit(data.toarray())
        if (res == None):
            return
        if (res.toarray()[0] != 0x67):
            QMessageBox(QMessageBox.Critical, 'Error', 'SecurityAccess request seed Failed!  %s.' % (
                self.dcm.get_last_error())).exec_()
            return
        unlocked = True
        for v in res.toarray()[2:]:
            if (v != 0):
                unlocked = False
        if (unlocked):
            QMessageBox(QMessageBox.Information, 'Info',
                        'SecurityAccess okay with 0 seed, already unlocked!').exec_()
            return
        data = dcmbits()
        data.append(0x27, 8)
        data.append(levelV+1, 8)
        key = self.Seed2Key(Level, res)
        for v in key:
            data.append(v, 8)
        res = self.dcm.transmit(data.toarray())
        if (res == None):
            return
        if (res.toarray()[0] != 0x67):
            QMessageBox(QMessageBox.Critical, 'Error', 'SecurityAccess send key Failed!  %s.' % (
                self.dcm.get_last_error())).exec_()
        else:
            QMessageBox(QMessageBox.Information, 'Info', 'SecurityAccess okay with response %s' % (
                FormatMessage(res))).exec_()


class UIDataIdentifier(QGroupBox):
    def __init__(self, service, parent):
        super(QGroupBox, self).__init__(service['name'], parent)
        self.DID = service
        self.dcm = parent
        grid = QGridLayout()

        self.leDatas = []
        row = 0
        col = 0
        for data in self.DID['data']:
            if (data['type'][-6:] == 'Select'):
                leData = wDataUS(data, parent)
            else:
                leData = wDataU(data, parent)
            self.leDatas.append(leData)
            grid.addWidget(QLabel(data['name']), row, col+0)
            grid.addWidget(leData, row, col+1)
            col += 2

            if (col >= 8):
                row += 1
                col = 0
        row += 1
        self.btnRead = QPushButton('Read')
        self.btnWrite = QPushButton('Write')
        grid.addWidget(self.btnRead, row, 0)
        grid.addWidget(self.btnWrite, row, 1)
        self.btnRead.clicked.connect(self.on_btnRead_clicked)
        self.btnWrite.clicked.connect(self.on_btnWrite_clicked)

        if (self.DID['attribute'] == 'r'):  # read-only
            self.btnWrite.setDisabled(True)

        if (self.DID['attribute'] == 'w'):  # write-only
            self.btnRead.setDisabled(True)

        self.setLayout(grid)

    def on_btnRead_clicked(self):
        data = dcmbits()
        data.append(0x22, 8)
        did = eval(self.DID['ID'])
        data.append(did, 16)

        res = self.dcm.transmit(data.toarray())
        if (res == None):
            return
        start = 24
        if (res.toarray()[0] != 0x62):
            QMessageBox(QMessageBox.Critical, 'Error',
                        'DID Start Failed!  %s.' % (self.dcm.get_last_error())).exec_()
        else:
            for leData in self.leDatas:
                start = leData.setValue(res, start)

    def on_btnWrite_clicked(self):
        data = dcmbits()
        data.append(0x2E, 8)
        did = eval(self.DID['ID'])
        data.append(did, 16)
        for leData in self.leDatas:
            leData.getValue(data)
        res = self.dcm.transmit(data.toarray())
        if (res == None):
            return
        if (res.toarray()[0] != 0x6E):
            QMessageBox(QMessageBox.Critical, 'Error',
                        'DID Write Failed!  %s.' % (self.dcm.get_last_error())).exec_()


class UIRoutineControl(QGroupBox):
    def __init__(self, service, parent):
        super(QGroupBox, self).__init__(service['name'], parent)
        self.SRI = service
        self.dcm = parent
        grid = QGridLayout()

        self.leDatas = []
        row = 0
        col = 0
        for data in self.SRI['data']:
            if (data['type'][-6:] == 'Select'):
                leData = wDataUS(data, parent)
            else:
                leData = wDataU(data, parent)
            self.leDatas.append(leData)
            grid.addWidget(QLabel(data['name']), row, col+0)
            grid.addWidget(leData, row, col+1)
            col += 2

            if (col >= 8):
                row += 1
                col = 0
        row += 1

        grid.addWidget(QLabel('Result:'), row, 0)
        self.leResult = QLineEdit('No Result')
        self.leResult.setEnabled(False)
        grid.addWidget(self.leResult, row, 1)

        self.btnStart = QPushButton('Start')
        self.btnStop = QPushButton('Stop')
        self.btnResult = QPushButton('Result')
        grid.addWidget(self.btnStart, row+1, 0)
        grid.addWidget(self.btnStop, row+1, 1)
        grid.addWidget(self.btnResult, row+1, 2)

        self.btnStart.clicked.connect(self.on_btnStart_clicked)
        self.btnStop.clicked.connect(self.on_btnStop_clicked)
        self.btnResult.clicked.connect(self.on_btnResult_clicked)

        try:
            if self.SRI['stop']:
                self.btnStop.setDisabled(False)
            else:
                self.btnStop.setDisabled(True)
        except KeyError:
            self.btnStop.setDisabled(True)

        try:
            if self.SRI['result']:
                self.btnResult.setDisabled(False)
            else:
                self.btnResult.setDisabled(True)
        except KeyError:
            self.btnResult.setDisabled(True)

        self.setLayout(grid)

    def on_btnStart_clicked(self):
        data = dcmbits()
        data.append(0x3101, 16)
        did = eval(self.SRI['ID'])
        data.append(did, 16)

        for leData in self.leDatas:
            leData.getValue(data)

        res = self.dcm.transmit(data.toarray())
        if (res == None):
            return
        if (res.toarray()[0] != 0x71):
            QMessageBox(QMessageBox.Critical, 'Error',
                        'SRI Start Failed!  %s.' % (self.dcm.get_last_error())).exec_()
        else:
            self.leResult.setText(
                'Please Click Button \'Result\' to Read the Result.')

    def on_btnStop_clicked(self):
        data = dcmbits()
        data.append(0x3102, 16)
        did = eval(self.SRI['ID'])
        data.append(did, 16)
        res = self.dcm.transmit(data.toarray())
        if (res == None):
            return
        if (res.toarray()[0] != 0x71):
            QMessageBox(QMessageBox.Critical, 'Error',
                        'SRI Stop Failed!  %s.' % (self.dcm.get_last_error())).exec_()

    def on_btnResult_clicked(self):
        data = dcmbits()
        data.append(0x3103, 16)
        did = eval(self.SRI['ID'])
        data.append(did, 16)
        res = self.dcm.transmit(data.toarray())
        if (res == None):
            return
        if (res.toarray()[0] != 0x71):
            QMessageBox(QMessageBox.Critical, 'Error', 'SRI Request Result Failed!  %s.' % (
                self.dcm.get_last_error())).exec_()
        else:
            self.leResult.setText(
                'check it by the raw response: %s' % (FormatMessage(res)))


class UIInputOutputControl(QGroupBox):
    def __init__(self, service, parent):
        super(QGroupBox, self).__init__(service['name'], parent)
        self.IOC = service
        self.dcm = parent
        grid = QGridLayout()

        self.leDatas = []
        row = 0
        col = 0
        for data in self.IOC['data']:
            if (data['type'][-6:] == 'Select'):
                leData = wDataUS(data, parent)
            else:
                leData = wDataU(data, parent)
            self.leDatas.append(leData)
            grid.addWidget(QLabel(data['name']), row, col+0)
            grid.addWidget(leData, row, col+1)
            col += 2

            if (col >= 8):
                row += 1
                col = 0
        row += 1

        self.btnStart = QPushButton('Start')
        self.btnRtce = QPushButton('Return Ctrl to ECU')
        grid.addWidget(self.btnStart, 2, 0)
        grid.addWidget(self.btnRtce, 2, 1)
        self.btnStart.clicked.connect(self.on_btnStart_clicked)
        self.btnRtce.clicked.connect(self.on_btnRtce_clicked)

        self.setLayout(grid)

    def on_btnStart_clicked(self):
        data = dcmbits()
        data.append(0x2F, 8)
        did = eval(self.IOC['ID'])
        data.append(did, 16)
        data.append(0x03, 8)

        for leData in self.leDatas:
            leData.getValue(data)

        res = self.dcm.transmit(data.toarray())
        if (res == None):
            return
        if (res.toarray()[0] != 0x6F):
            QMessageBox(QMessageBox.Critical, 'Error',
                        'IOC Start Failed!  %s.' % (self.dcm.get_last_error())).exec_()

    def on_btnRtce_clicked(self):
        data = dcmbits()
        data.append(0x2F, 8)
        did = eval(self.IOC['ID'])
        data.append(did, 16)
        data.append(0x00, 8)
        res = self.dcm.transmit(data.toarray())
        if (res == None):
            return
        if (res.toarray()[0] != 0x6F):
            QMessageBox(QMessageBox.Critical, 'Error', 'IOC Return Control to ECU Failed!  %s.' % (
                self.dcm.get_last_error())).exec_()


class UIDTC(QGroupBox):
    def __init__(self, service, parent):
        super(QGroupBox, self).__init__("DTC", parent)
        if 'path' in service:
            with open(service['path']) as f:
                service = json.load(f)
        self.DTC = service
        self.dcm = parent

        self.ExtendedData = self.DTC['ExtendedDatas']
        self.Snapshot = {}
        self.DTCs = {}
        for one in self.DTC['DTCs']:
            self.DTCs[one['name']] = one
        for one in self.DTC['Environments']:
            self.Snapshot[eval(one['id'])] = one

        self.subfn = [self.reportNumberOfDTCByStatusMask,
                      self.reportNumberOfMirrorMemoryDTCByStatusMask,
                      self.reportDTCSnapshotIdentification,
                      self.reportDTCByStatusMask,
                      self.reportMirrorMemoryDTCByStatusMask,
                      self.reportDTCSnapshotRecordByDTCNumber,
                      self.reportDTCExtendedDataRecordByDTCNumber,
                      self.reportMirrorMemoryDTCExtendedDataRecordByDTCNumber,
                      ]
        self.cmbxSubfn = QComboBox()
        for fn in self.subfn:
            self.cmbxSubfn.addItem(fn.__name__)
        vbox = QVBoxLayout()
        grid = QGridLayout()
        grid.addWidget(QLabel('option:'), 0, 0)
        self.cmbxCtrlDTCSetting = QComboBox()
        self.cmbxCtrlDTCSetting.addItems(['on', 'off'])
        grid.addWidget(self.cmbxCtrlDTCSetting, 0, 1)
        self.btnCtrlDTCSetting = QPushButton('Control DTC Setting')
        grid.addWidget(self.btnCtrlDTCSetting, 0, 3)
        self.btnClearAll = QPushButton('Clear All DTC')
        grid.addWidget(self.btnClearAll, 0, 5)
        grid.addWidget(QLabel('Function:'), 1, 0)
        grid.addWidget(self.cmbxSubfn, 1, 1)
        self.cmbxDTC = QComboBox()
        for dtc in self.DTCs.keys():
            self.cmbxDTC.addItem(dtc)
        grid.addWidget(self.cmbxDTC, 1, 2)
        self.leParam = QLineEdit('0xFF')
        self.leParam.setToolTip('StatusMask or RecordNumber: 8 bits, eg 0xFF')
        grid.addWidget(self.leParam, 1, 3)
        self.btnRead = QPushButton('Read')
        grid.addWidget(self.btnRead, 1, 4)
        self.btnClear = QPushButton('Clear')
        grid.addWidget(self.btnClear, 1, 5)
        vbox.addLayout(grid)
        self.txtInfo = QTextEdit()
        self.txtInfo.setMinimumSize(1000, 600)
        self.txtInfo.setReadOnly(True)
        vbox.addWidget(self.txtInfo)
        self.setLayout(vbox)
        self.btnRead.clicked.connect(self.on_btnRead_clicked)
        self.btnClearAll.clicked.connect(self.on_btnClearAll_clicked)
        self.btnClear.clicked.connect(self.on_btnClear_clicked)
        self.btnCtrlDTCSetting.clicked.connect(
            self.on_btnCtrlDTCSetting_clicked)
        self.cmbxSubfn.currentIndexChanged.connect(
            self.on_cmbxSubfn_currentIndexChanged)

    def on_cmbxSubfn_currentIndexChanged(self, index):
        pass

    def addInfo(self, info):
        self.txtInfo.append(info)
        self.txtInfo.ensureCursorVisible()

    def reportDTCCommon(self, req=[]):
        data = dcmbits()
        data.append(0x19, 8)
        for v in req:
            data.append(v, 8)
        res = self.dcm.transmit(data.toarray())
        if (res == None):
            return None
        if (res.toarray()[0] != 0x59):
            QMessageBox(QMessageBox.Critical, 'Error',
                        'Read DTC Failed! %s.' % (self.dcm.get_last_error())).exec_()
            return None
        else:
            return res

    def showNumberOfDTCCommon(self, res):
        res = res.toarray()
        self.addInfo('\tStatusAvailabilityMask = 0x%02X' % (res[2]))
        self.addInfo('\tFormatIdentifier       = 0x%02X' % (res[3]))
        self.addInfo('\tNumber                 = %s' % ((res[4] << 8)+res[5]))

    def reportNumberOfMirrorMemoryDTCByStatusMask(self, fnid=0x11):
        statusMask = eval(str(self.leParam.text()))
        res = self.reportDTCCommon([fnid, statusMask])
        if (res == None):
            return
        self.showNumberOfDTCCommon(res)

    def reportNumberOfDTCByStatusMask(self, fnid=0x01):
        statusMask = eval(str(self.leParam.text()))
        res = self.reportDTCCommon([fnid, statusMask])
        if (res == None):
            return
        self.showNumberOfDTCCommon(res)

    def reportDTCSnapshotIdentification(self, fnid=0x03):
        res = self.reportDTCCommon([fnid])
        if (res == None):
            return
        record = res.toarray()[2:]
        while (len(record) > 0):
            idx = (record[0] << 16) + (record[1] << 8)+(record[2] << 0)
            number = record[3]
            self.addInfo('\tDTC ID=0x%06X (%s)' % (idx, self.strDtcName(idx)))
            self.addInfo('\tRecordNumber=0x%02X' % (number))
            record = record[4:]

    def decodeData(self, data, record):
        assert (data['type'] in ['uint8', 'uint16', 'uint32', 'uint64'])
        length = int(eval(data['type'][4:])/8)
        value = 0
        for j in range(length):
            value = (value << 8) + record[j]
        self.addInfo('\t    %s = %s (%s)' % (
            data['name'], value, [hex(s) for s in record[:length]]))
        return record[length:]

    def reportDTCSnapshotRecordByDTCNumber(self, fnid=0x04):
        recordNum = eval(str(self.leParam.text()))
        dtc = self.DTCs[str(self.cmbxDTC.currentText())]
        id = eval(dtc['number'])
        res = self.reportDTCCommon(
            [fnid, (id >> 16) & 0xFF, (id >> 8) & 0xFF, id & 0xFF, recordNum])
        if (res == None):
            return
        res = res.toarray()
        status = res[5]
        self.addInfo('\t%s status:' % (dtc['name']))
        self.addInfo('\tStatusMask = 0x%02X%s' %
                     (status, self.strStatusMask(status)))
        record = res[6:]
        while (len(record) > 0):
            self.addInfo('\tRecordNumber = %d' % (record[0]))
            self.addInfo('\tNumber Of Datas = %d' % (record[1]))
            num = record[1]
            record = record[2:]
            for i in range(num):
                id = (record[0] << 8) + record[1]
                record = record[2:]
                data = self.Snapshot[id]
                if data['type'] in ['uint8', 'uint16', 'uint32', 'uint64']:
                    record = self.decodeData(data, record)
                elif data['type'] == 'struct':
                    for sd in data['data']:
                        record = self.decodeData(sd, record)

    def reportDTCExtendedDataRecordByDTCNumber(self, fnid=0x06):
        recordNum = eval(str(self.leParam.text()))
        dtc = self.DTCs[str(self.cmbxDTC.currentText())]
        id = eval(dtc['number'])
        res = self.reportDTCCommon(
            [fnid, (id >> 16) & 0xFF, (id >> 8) & 0xFF, id & 0xFF, recordNum])
        if (res == None):
            return
        res = res.toarray()
        status = res[5]
        self.addInfo('\t%s status:' % (dtc['name']))
        self.addInfo('\tStatusMask = 0x%02X%s' %
                     (status, self.strStatusMask(status)))
        record = res[6:]
        while (len(record) > 0):
            self.addInfo('\tRecordNumber = %d' % (record[0]))
            num = len(self.ExtendedData)
            record = record[1:]
            for i in range(num):
                data = self.ExtendedData[i]
                record = self.decodeData(data, record)

    def reportMirrorMemoryDTCExtendedDataRecordByDTCNumber(self, fnid=0x10):
        self.reportDTCExtendedDataRecordByDTCNumber(fnid)

    def strStatusMask(self, mask):
        ss = ''
        if (mask & 0x01):
            ss += '\n\t\tTEST_FAILED'
        if (mask & 0x02):
            ss += '\n\t\tTEST_FAILED_THIS_OPERATION_CYCLE'
        if (mask & 0x04):
            ss += '\n\t\tPENDING_DTC'
        if (mask & 0x08):
            ss += '\n\t\tCONFIRMED_DTC'
        if (mask & 0x10):
            ss += '\n\t\tTEST_NOT_COMPLETED_SINCE_LAST_CLEAR'
        if (mask & 0x20):
            ss += '\n\t\tTEST_FAILED_SINCE_LAST_CLEAR'
        if (mask & 0x40):
            ss += '\n\t\tTEST_NOT_COMPLETED_THIS_OPERATION_CYCLE'
        if (mask & 0x80):
            ss += '\n\t\tWARNING_INDICATOR_REQUESTED'
        return ss

    def strDtcName(self, id):
        for _, dtc in self.DTCs.items():
            if (eval(dtc['number']) == id):
                return dtc['name']
        return 'unknown'

    def showDTCCommon(self, res):
        res = res.toarray()
        self.addInfo('\tStatusAvailabilityMask = 0x%02X' % (res[2]))
        for dtc in range(int((len(res)-3)/4)):
            id = (res[3+4*dtc] << 16) + \
                (res[3+4*dtc+1] << 8)+(res[3+4*dtc+2] << 0)
            status = res[3+4*dtc+3]
            self.addInfo('\tDTC ID=0x%06X (%s)' % (id, self.strDtcName(id)))
            self.addInfo('\tStatusMask = 0x%02X%s' %
                         (status, self.strStatusMask(status)))

    def reportDTCByStatusMask(self):
        statusMask = eval(str(self.leParam.text()))
        res = self.reportDTCCommon([0x02, statusMask])
        if (res == None):
            return
        self.showDTCCommon(res)

    def reportMirrorMemoryDTCByStatusMask(self):
        statusMask = eval(str(self.leParam.text()))
        res = self.reportDTCCommon([0x0F, statusMask])
        if (res == None):
            return
        self.showDTCCommon(res)

    def on_btnRead_clicked(self):
        fn = str(self.cmbxSubfn.currentText())
        for f in self.subfn:
            if (f.__name__ == fn):
                self.addInfo('%s:' % (fn))
                f()
                break

    def on_btnClearAll_clicked(self):
        data = dcmbits()
        data.append(0x14, 8)
        data.append(0xFFFFFF, 24)
        res = self.dcm.transmit(data.toarray())
        if (res == None):
            return
        if (res.toarray()[0] != 0x54):
            QMessageBox(QMessageBox.Critical, 'Error',
                        'Clear DTC Failed! %s.' % (self.dcm.get_last_error())).exec_()
        else:
            QMessageBox(QMessageBox.Information, 'Info',
                        'Clear ALL DTC done!').exec_()

    def on_btnClear_clicked(self):
        data = dcmbits()
        data.append(0x14, 8)
        dtc = self.DTCs[str(self.cmbxDTC.currentText())]
        id = eval(dtc['number'])
        data.append(id, 24)
        res = self.dcm.transmit(data.toarray())
        if (res == None):
            return
        if (res.toarray()[0] != 0x54):
            QMessageBox(QMessageBox.Critical, 'Error',
                        'Clear DTC Failed! %s.' % (self.dcm.get_last_error())).exec_()
        else:
            QMessageBox(QMessageBox.Information, 'Info',
                        'Clear DTC %s 0x%x done!' % (dtc['name'], id)).exec_()

    def on_btnCtrlDTCSetting_clicked(self):
        data = dcmbits()
        data.append(0x85, 8)
        dtcSetting = str(self.cmbxCtrlDTCSetting.currentText())
        if dtcSetting == 'on':
            data.append(0x01, 8)
        else:
            data.append(0x02, 8)
        res = self.dcm.transmit(data.toarray())
        if (res == None):
            return
        if (res.toarray()[0] != 0xC5):
            QMessageBox(QMessageBox.Critical, 'Error',
                        'Control DTC setting Failed! %s.' % (self.dcm.get_last_error())).exec_()
        else:
            QMessageBox(QMessageBox.Information, 'Info',
                        'Control DTC setting %s done!' % (dtcSetting)).exec_()


class UIGroup(QScrollArea):
    def __init__(self, group, parent):
        super(QScrollArea, self).__init__(parent)
        self.group = group
        wd = QWidget()
        vBox = QVBoxLayout()
        for name, service in group.items():
            if name == 'SessionControl':
                vBox.addWidget(UISessionControl(service, parent))
            elif name == "SecurityAccess":
                vBox.addWidget(UISecurityAccess(service, parent))
            elif name == "DataIdentifier":
                for sr in service:
                    vBox.addWidget(UIDataIdentifier(sr, parent))
            elif name == "RoutineControl":
                for sr in service:
                    vBox.addWidget(UIRoutineControl(sr, parent))
            elif name == "InputOutputControl":
                for sr in service:
                    vBox.addWidget(UIInputOutputControl(sr, parent))
            elif name == 'DTC':
                vBox.addWidget(UIDTC(service, parent))
        wd.setLayout(vBox)
        self.setWidget(wd)


class UIDcm(QWidget):
    def __init__(self, parent=None):
        super(QWidget, self).__init__(parent)
        self.vbox = QVBoxLayout()

        grid = QGridLayout()
        grid.addWidget(QLabel('json:'), 0, 0)
        self.leJson = QLineEdit()
        self.leJson.setReadOnly(True)
        grid.addWidget(self.leJson, 0, 1)
        self.btnOpenJson = QPushButton('...')
        grid.addWidget(self.btnOpenJson, 0, 2)

        grid.addWidget(QLabel('target:'), 1, 0)
        self.leTarget = QLineEdit()
        grid.addWidget(self.leTarget, 1, 1)
        self.btnStart = QPushButton('start')
        grid.addWidget(self.btnStart, 1, 2)
        self.cbxTesterPresent = QCheckBox("Tester Present")
        grid.addWidget(self.cbxTesterPresent, 2, 2)

        self.vbox.addLayout(grid)
        self.tabWidget = QTabWidget(self)
        self.vbox.addWidget(self.tabWidget)
        self.setLayout(self.vbox)

        self.btnOpenJson.clicked.connect(self.on_btnOpenJson_clicked)
        self.btnStart.clicked.connect(self.on_btnStart_clicked)
        self.cbxTesterPresent.stateChanged.connect(
            self.on_cbxTesterPresent_stateChanged)

        default_json = os.path.abspath(
            '%s/../examples/diagnostic.json' % (CWD))
        if os.path.isfile(default_json):
            self.loadJson(default_json)

    def on_cbxTesterPresent_stateChanged(self, state):
        if (state):
            self.TPtimer = self.startTimer(3000)
        else:
            self.killTimer(self.TPtimer)

    def timerEvent(self, event):
        self.dcm.transmit([0x3e, 0x80])

    def loadUI(self):
        self.djs['target'] = eval(str(self.leTarget.text()))
        if 'rxid' in self.djs['target']:
            self.djs['target']['rxid'] = eval(self.djs['target']['rxid'])
        if 'txid' in self.djs['target']:
            self.djs['target']['txid'] = eval(self.djs['target']['txid'])
        try:
            self.dcm = dcm(**self.djs['target'])
        except Exception as e:
            QMessageBox(QMessageBox.Critical, 'Error', '%s' % (e)).exec_()
            return
        self.tabWidget.clear()
        for name, group in self.djs['groups'].items():
            self.tabWidget.addTab(UIGroup(group, self), name)

    def on_btnStart_clicked(self):
        self.loadUI()

    def loadJson(self, djs):
        with open(djs) as f:
            self.djs = json.load(f)
        self.leTarget.setText('%s' % (self.djs['target']))
        self.leJson.setText(djs)

    def on_btnOpenJson_clicked(self):
        rv = QFileDialog.getOpenFileName(
            None, 'diagnostic description file', '', 'diagnostic description file (*.json)')
        if (rv[0] != ''):
            self.leJson.setText(rv[0])
            self.loadJson(rv[0])

    def transmit(self, req):
        ercd, res = self.dcm.transmit(req)
        if ((ercd == True) or (res is not None)):
            res2 = dcmbits()
            for d in res:
                res2.append(d, 8)
            return res2

        QMessageBox(QMessageBox.Critical, 'Error',
                    'Communication Error or Timeout').exec_()
        return None

    def get_last_error(self):
        return self.dcm.last_error

    def get_response(self):
        return self.dcm.last_reponse


def get():
    return 'Dcm', UIDcm
