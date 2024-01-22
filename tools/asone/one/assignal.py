# SSAS - Simple Smart Automotive Software
# Copyright (C) 2015 Parai Wang <parai@foxmail.com>

import os
import sys
import time
import threading
import queue
from bitarray import bitarray
import json
from .AsPy import can, lin

__all__ = ['Network', 'QView']

# big endian bits map
_bebm = []

# cstr = 'COM big endian bits map:\n'
for i in range(64):
    #    cstr += '\n\tB%2d '%(i)
    for j in range(8):
        _bebm.append(i*8 + 7-j)
#        cstr += '%3d '%(i*8 + 7-j)
# print(cstr)


class Sdu():
    def __init__(self, length):
        self.data = []
        for i in range(0, length):
            self.data.append(0x5A)

    def __iter__(self):
        for v in self.data:
            yield v

    def __len__(self):
        return len(self.data)

    def beset(self, start, size, value):
        rBit = size-1
        nBit = _bebm.index(start)
        wByte = 0
        wBit = 0
        for i in range(size):
            wBit = _bebm[nBit]
            wByte = int(wBit/8)
            wBit = wBit % 8
            if (value & (1 << rBit) != 0):
                self.data[wByte] |= 1 << wBit
            else:
                self.data[wByte] &= ~(1 << wBit)
            nBit += 1
            rBit -= 1

    def leset(self, start, size, value):
        rBit = size-1
        nBit = start+size-1
        wByte = 0
        wBit = 0
        for i in range(size):
            wBit = nBit
            wByte = int(wBit/8)
            wBit = wBit % 8
            if (value & (1 << rBit) != 0):
                self.data[wByte] |= 1 << wBit
            else:
                self.data[wByte] &= ~(1 << wBit)
            nBit -= 1
            rBit -= 1

    def set(self, sig, value):
        start = sig['start']
        size = sig['size']
        endian = sig['endian']
        if (endian == 'big'):
            self.beset(start, size, value)
        else:
            self.leset(start, size, value)

    def beget(self, start, size):
        nBit = _bebm.index(start)
        value = 0
        for i in range(size):
            rBit = _bebm[nBit]
            rByte = int(rBit/8)
            if (self.data[rByte] & (1 << (rBit % 8)) != 0):
                value = (value << 1)+1
            else:
                value = (value << 1)+0
            nBit += 1
        return value

    def leget(self, start, size):
        nBit = start+size-1
        value = 0
        for i in range(size):
            rBit = nBit
            rByte = int(rBit/8)
            if (self.data[rByte] & (1 << (rBit % 8)) != 0):
                value = (value << 1)+1
            else:
                value = (value << 1)+0
            nBit -= 1
        return value

    def get(self, sig):
        # for big endian only
        start = sig['start']
        size = sig['size']
        endian = sig['endian']
        if (endian == 'big'):
            return self.beget(start, size)
        else:
            return self.leget(start, size)

    def __str__(self):
        cstr = ''
        for b in self.data:
            cstr += '%02X' % (b)
        return cstr


class Signal():
    def __init__(self, sg):
        self.sg = sg
        self.name = sg['name']
        self.mask = (1 << sg['size'])-1
        self.notifyMQ = []
        self.set_value(0)

    def register_mq(self, mq):
        self.notifyMQ.append(mq)

    def unregister_mq(self, mq):
        self.notifyMQ.remove(mq)

    def get_max(self):
        return 0xFFFFFFFF & self.mask

    def get_min(self):
        return 0

    def set_value(self, v):
        self.value = v & self.mask
        for mq in self.notifyMQ:
            mq.put(self.value)

    def get_value(self):
        for mq in self.notifyMQ:
            mq.put(self.value)
        return self.value & self.mask

    def __str__(self):
        return str(self.sg)

    def __getitem__(self, key):
        return self.sg[key]


class Message():
    def __init__(self, msg, network, isTx):
        self.name = msg['name']
        self.msg = msg
        self.network = network
        self.isTx = isTx
        self.sgs = {}
        self.sdu = Sdu(msg['dlc'])
        if ('period' in msg):
            self.period = msg['period']
        else:
            self.period = 1000
        self.timer = time.time()
        for sg in msg['signals']:
            self.sgs[sg['name']] = Signal(sg)
        self.notifyMQ = []

    def register_mq(self, mq):
        self.notifyMQ.append(mq)

    def unregister_mq(self, mq):
        self.notifyMQ.remove(mq)

    def notify_mq(self):
        if len(self.notifyMQ):
            msg = {}
            for sig in self:
                msg[sig['name']] = sig.value
            for mq in self.notifyMQ:
                mq.put(msg)

    def attrib(self, key):
        return self.msg[key]

    def set_period(self, period):
        self.period = period

    def get_period(self):
        return self.period

    def transmit(self):
        for sig in self:
            self.sdu.set(sig, sig.value)
        self.network.write(self.msg['id'], self.sdu)

    def ProcessTX(self):
        if (self.period <= 0):
            return
        elapsed = time.time() - self.timer
        if (self.period <= elapsed*1000):
            self.timer = time.time()
            self.transmit()
            self.notify_mq()

    def ProcessRX(self):
        result, idx, data = self.network.read(self.msg['id'], self.msg['dlc'])
        if (result):
            self.sdu.data = [d for d in data]
            for sig in self:
                sig.value = self.sdu.get(sig)
            self.notify_mq()

    def IsTransmit(self):
        return self.isTx

    def Process(self):
        if self.isTx:
            self.ProcessTX()
        self.ProcessRX()

    def __str__(self):
        return str(self.msg)

    def __iter__(self):
        for key, sig in self.sgs.items():
            yield sig

    def __getitem__(self, key):
        self.sgs[key].value = self.sdu.get(self.sgs[key])
        return self.sgs[key].value

    def __setitem__(self, key, value):
        self.sgs[key].set_value(value)

    def __getattr__(self, key):
        return self.__getitem__(key)

    def __setattr__(self, key, value):
        if (key in ['msg', 'name', 'network', 'isTx', 'sgs', 'sdu', 'period', 'timer', 'notifyMQ']):
            self.__dict__[key] = value
        else:
            self.__setitem__(key, value)


class Network(threading.Thread):
    def __init__(self, network):
        threading.Thread.__init__(self)
        device = network.get('device', 'simulator')
        port = network.get('port', 0)
        baudrate = network.get('baudrate', 1000000)
        if network['network'] == 'CAN':
            self.bus = can(device, port, baudrate)
        else:
            self.bus = lin(**network)
        self.msgs = {}
        for msg in network['messages']:
            self.msgs[msg['name']] = Message(
                msg, self, isTx=False if msg['node'] == network['me'] else True)
        self.start()

    def lookup(self, name):
        for msg in self:
            for sig in msg:
                if (sig['name'] == name):
                    return sig
        return None

    def write(self, id, sdu):
        self.bus.write(id, bytes(sdu))

    def read(self, id, dlc):
        if isinstance(self.bus, can):
            return self.bus.read(id)
        else:
            return self.bus.read(id, dlc)

    def stop(self):
        self.is_running = False

    def run(self):
        self.is_running = True
        while (self.is_running):
            for msg in self:
                msg.Process()
            time.sleep(0.001)

    def __iter__(self):
        for key, msg in self.msgs.items():
            yield msg

    def __getitem__(self, key):
        return self.msgs[key]

    def __getattr__(self, key):
        return self.__getitem__(key)


def QView(sig, scale=1, offset=0):
    from PyQt5 import QtCore
    from PyQt5.QtWidgets import QWidget, QPushButton, QVBoxLayout, QGridLayout
    import matplotlib
    matplotlib.use('Qt5Agg')
    from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
    from matplotlib.backends.backend_qt5agg import NavigationToolbar2QT as NavigationToolbar
    from matplotlib.figure import Figure
    import matplotlib.pyplot as plt

    class MplCanvas(FigureCanvas):
        def __init__(self, name, width=10, height=6, dpi=100):
            fig = Figure(figsize=(width, height), dpi=dpi)
            fig.suptitle(name)
            self.axes = fig.add_subplot(111)
            super(MplCanvas, self).__init__(fig)

    class QView_(QWidget):
        def __init__(self, sig, scale=1, offset=0):
            self.mq = queue.Queue()
            QWidget.__init__(self)
            self.sig = sig
            self.scale = scale
            self.offset = offset
            self.lastValue = 0
            self.sig.register_mq(self.mq)
            self.setWindowTitle('AsView')
            self.canvas = MplCanvas(self.sig.name)
            self.toolbar = NavigationToolbar(self.canvas, self)
            vbox = QVBoxLayout()
            vbox.addWidget(self.toolbar)
            vbox.addWidget(self.canvas)
            grid = QGridLayout()
            self.btnClear = QPushButton('clear')
            self.btnClear.setMaximumWidth(100)
            self.btnClear.clicked.connect(self.on_btnClear_clicked)
            grid.addWidget(self.btnClear, 0, 5)
            vbox.addLayout(grid)
            self.setLayout(vbox)

            self.ax = self.canvas.axes
            self.ax.set_ylim(self.get_min(), self.get_max())
            self.ax.set_xlim(0, 10)
            self.show()

            self.t = []
            self.y = []
            self.pre = time.time()
            self.lastValue = 0
            self.timer = QtCore.QTimer()
            self.timer.setInterval(100)
            self.timer.timeout.connect(self.render)
            self.timer.start()

        def on_btnClear_clicked(self):
            self.t = []
            self.y = []

        def render(self):
            empty = False
            while (not empty):
                try:
                    v = self.mq.get_nowait()
                    self.lastValue = v
                except:
                    v = self.lastValue
                    empty = True
                v = v*self.scale+self.offset
                self.t.append(time.time() - self.pre)
                self.y.append(v)
            self.ax.clear()
            self.ax.plot(self.t, self.y)
            self.ax.grid()
            self.canvas.draw()

        def __del__(self):
            self.sig.unregister_mq(self.mq)

        def get_value(self):
            try:
                v = self.mq.get_nowait()
                self.lastValue = v
            except:
                v = self.lastValue
            return v*self.scale+self.offset

        def get_max(self):
            if (self.scale > 0):
                return self.sig.get_max()*self.scale+self.offset
            else:
                return self.sig.get_min()*self.scale+self.offset

        def get_min(self):
            if (self.scale < 0):
                return self.sig.get_max()*self.scale+self.offset
            else:
                return self.sig.get_min()*self.scale+self.offset

    return QView_(sig, scale, offset)
