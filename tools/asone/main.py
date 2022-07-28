# SSAS - Simple Smart Automotive Software
# Copyright (C) 2015 Parai Wang <parai@foxmail.com>
import os
import sys
from PyQt5 import QtCore, QtGui
from PyQt5.QtGui import *
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
import importlib.util
import glob

CWD = os.path.abspath(os.path.dirname(__file__))
sys.path.append(CWD)


class AsAction(QAction):
    action = QtCore.pyqtSignal(str)

    def __init__(self, text, parent=None):
        super(QAction, self).__init__(text, parent)
        self.triggered.connect(self.onAction)

    def onAction(self):
        self.action.emit(self.text())


class Window(QMainWindow):
    def __init__(self, parent=None):
        super(QMainWindow, self).__init__(parent)
        self.UIList = {}
        self.loadUIs()
        self.creGui()
        self.setWindowTitle("AsOne")

    def loadUIs(self):
        uidir = os.path.abspath('%s/UIs' % (CWD))
        for ui in glob.glob('%s/UI*.py' % (uidir)):
            uiName = os.path.basename(ui)[:-3]
            spec = importlib.util.spec_from_file_location(uiName, ui)
            uiM = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(uiM)
            name, UIWidget = uiM.get()
            self.UIList[name] = UIWidget

    def closeEvent(self, Event):
        pass

    def creMenu(self):
        # easySAR Module
        tMenu = self.menuBar().addMenu(self.tr('MyApp'))
        for name, ui in self.UIList.items():
            sItem = AsAction(self.tr(name), self)
            sItem.action.connect(self.onAction)
            tMenu.addAction(sItem)

    def onAction(self, text):
        for i in range(self.tabWidget.count()):
            if(text == str(self.tabWidget.tabText(i))):
                return
        for name, ui in self.UIList.items():
            if(text == name):
                self.tabWidget.addTab(ui(), name)
                return

    def creGui(self):
        wid = QWidget()
        grid = QVBoxLayout()
        self.tabWidget = QTabWidget(self)
        grid.addWidget(self.tabWidget)
        wid.setLayout(grid)

        self.setCentralWidget(wid)
        self.creMenu()

        for name, _ in self.UIList.items():
            self.onAction(name)

        self.setMinimumSize(1200, 600)


def main():
    app = QApplication(sys.argv)
    if(os.name == 'nt'):
        app.setFont(QFont('Consolas'))
    elif(os.name == 'posix'):
        app.setFont(QFont('Monospace'))
    else:
        print('unKnown platform.')
    mWain = Window()
    mWain.show()
    sys.exit(app.exec_())


if(__name__ == '__main__'):
    main()
