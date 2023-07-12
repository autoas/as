# SSAS - Simple Smart Automotive Software
# Copyright (C) 2015 ~ 2023 Parai Wang <parai@foxmail.com>
from PyQt5 import QtCore, QtGui
from PyQt5.QtGui import *
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
import sys
import os
import json
from json_editor import *
import importlib.util
import glob

PKGDIR = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.path.abspath('%s/..' % (PKGDIR)))


class JsonDockWidget(QDockWidget):
    isClosed = False

    def __init__(self, title, parent=None):
        QDockWidget.__init__(self, title, parent)
        self.setAllowedAreas(QtCore.Qt.LeftDockWidgetArea |
                             QtCore.Qt.RightDockWidgetArea)
        # self.setFeatures(QDockWidget.DockWidgetClosable|QDockWidget.DockWidgetMovable)

    def closeEvent(self, event):
        self.isClosed = True


class JsonEditor(QMainWindow):
    def __init__(self, args):
        self.modules = {}
        self.docks = {}
        self.actions = {}

        QMainWindow.__init__(self, None)
        self.setWindowTitle('JSON Editor')
        self.setMinimumSize(800, 400)

        self.schemaFile = args.schema
        self.jsonFile = args.input

        self.creStatusBar()
        self.load_schema()
        self.creMenu()
        self.loadPlugins()

        if self.jsonFile and os.path.isfile(self.jsonFile):
            self.mOpen(self.jsonFile)

        self.showMaximized()

    def loadPlugins(self):
        tMenu = self.menuBar().addMenu(self.tr('Plugin'))
        plgdir = os.path.abspath('%s/plugin' % (PKGDIR))
        for plg in glob.glob('%s/*.py' % (plgdir)):
            plgName = os.path.basename(plg)[:-3]
            spec = importlib.util.spec_from_file_location(plgName, plg)
            plgM = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(plgM)
            sItem = plgM.Plugin(self)
            tMenu.addAction(sItem)

    def load_schema(self):
        with open(self.schemaFile) as f:
            self.schema = json.load(f)
        if type(self.schema) != list:
            self.schema = [self.schema]

    def creMenu(self):
        # File
        tMenu = self.menuBar().addMenu(self.tr('File'))
        # Open Ctrl+O
        sItem = QAction(self.tr('Open'), self)
        sItem.setShortcut('Ctrl+O')
        sItem.setStatusTip('Open a JSON configure file.')
        sItem.triggered.connect(self.mOpen)
        tMenu.addAction(sItem)
        # Load Ctrl+L
        sItem = QAction(self.tr('Load'), self)
        sItem.setShortcut('Ctrl+L')
        sItem.setStatusTip('Load a JSON configure file.')
        sItem.triggered.connect(self.mLoad)
        tMenu.addAction(sItem)
        # Save Ctrl+S
        sItem = QAction(self.tr('Save'), self)
        sItem.setShortcut('Ctrl+S')
        sItem.setStatusTip('Save the JSON configure file.')
        sItem.triggered.connect(self.mSave)
        tMenu.addAction(sItem)
        # Save Ctrl+G
        sItem = QAction(self.tr('Generate'), self)
        sItem.setShortcut('Ctrl+G')
        sItem.setStatusTip('Convert the JSON configure file to C Code.')
        sItem.triggered.connect(self.mGen)
        tMenu.addAction(sItem)
        # Json Module
        tMenu = self.menuBar().addMenu(self.tr('Module'))
        for schema in self.schema:
            sItem = JsonAction(self.tr(schema['title']), self)
            sItem.setStatusTip('Open %s console.' % (schema['title']))
            tMenu.addAction(sItem)

    def reload(self, title, cfg):
        if title in self.modules:
            del self.modules[title]
            self.removeDockWidget(self.docks[title])
            del self.docks[title]
        schema = self.find_schema(title)
        schema['__init__'] = cfg
        self.onAction(title)

    def find(self, url):
        urls = url.split('/')
        title = urls[0]
        if title in self.modules:
            module = self.modules[title]
        else:
            return None
        cfg = module.toJSON()
        for u in urls[1:]:
            if ':' in u:
                title, field = u.split(':')
                return [x[field] for x in cfg[title]]
            else:
                if type(cfg) == list:
                    c = None
                    for x in cfg:
                        if x['name'] == u:
                            c = x
                    if c != None:
                        cfg = c
                    else:
                        return None
                else:
                    cfg = cfg[u]
        return cfg

    def mOpen(self, jsonFile=None):
        if (jsonFile == None) or (type(jsonFile) == bool):
            jsonFile, _ = QFileDialog.getOpenFileName(None, 'Open JSON',
                                                      'jse.json', '*.json', '*.json',
                                                      QFileDialog.DontResolveSymlinks)
            if (jsonFile == ''):
                return
        if os.path.exists(jsonFile) == False:
            return

        with open(jsonFile) as f:
            cfg = json.load(f)
            if type(cfg) == dict:
                cfg = [cfg]
            for m in cfg:
                for schema in self.schema:
                    if m['class'] == schema['title']:
                        schema['__init__'] = m
        self.jsonFile = jsonFile
        self.modules.clear()
        for _, dock in self.docks.items():
            self.removeDockWidget(dock)
        self.docks.clear()
        for schema in self.schema:
            if '__init__' in schema:
                self.onAction(schema['title'])

    def mLoad(self):
        jsonFile, _ = QFileDialog.getOpenFileName(None, 'Load JSON',
                                                  'jse.json', '*.json', '*.json',
                                                  QFileDialog.DontResolveSymlinks)
        if (jsonFile == ''):
            return
        if os.path.exists(jsonFile) == False:
            return

        with open(jsonFile) as f:
            cfg = json.load(f)
            if type(cfg) == dict:
                cfg = [cfg]
            for m in cfg:
                for schema in self.schema:
                    if m['class'] == schema['title']:
                        self.reload(m['class'], m)

    def toJSON(self):
        cfgs = []
        for title, module in self.modules.items():
            cfgs.append(module.toJSON())
        return cfgs

    def mSave(self):
        jsonFile = self.jsonFile
        if (jsonFile == '' or jsonFile == None):
            jsonFile, _ = QFileDialog.getSaveFileName(None, 'Save JSON',
                                                      'jse.json', '*.json', '*.json',
                                                      QFileDialog.DontResolveSymlinks)
        if (jsonFile == ''):
            return
        self.jsonFile = jsonFile
        self.setWindowTitle('JSON Editor < %s >' % (jsonFile))
        cfgs = self.toJSON()
        with open(jsonFile, 'w') as f:
            json.dump(cfgs, f, indent=2)
        QMessageBox(QMessageBox.Information, 'Info',
                    'Save JSON Configuration < %s > Successfully !' % (jsonFile)).exec_()

    def mGen(self):
        if os.path.exists(self.jsonFile) == False:
            return
        from generator import Generate
        gdir = os.path.dirname(self.jsonFile)
        gdir = os.path.abspath(gdir) + '/config'
        os.makedirs(gdir, exist_ok=True)
        cfgs = []
        for title, module in self.modules.items():
            cfg = module.toJSON()
            jsonFile = '%s/%s.json' % (gdir, title)
            with open(jsonFile, 'w') as f:
                json.dump(cfg, f, indent=2)
            cfgs.append(jsonFile)
        Generate(cfgs, True)
        QMessageBox(QMessageBox.Information, 'Info',
                    'Generate ssas Configuration C Code Successfully !\n<%s>\n' % (gdir)).exec_()

    def find_schema(self, title):
        for schema in self.schema:
            if schema['title'] == title:
                return schema
        raise

    def onAction(self, title):
        if title not in self.modules:
            module = JsonModule(self.find_schema(title), self)
            self.modules[module.title] = module
        else:
            module = self.modules[title]
        if (title not in self.docks) or (self.docks[title].isClosed == True):
            self.docks[title] = JsonDockWidget(title, self)
            self.docks[title].setWidget(module)
            self.addDockWidget(QtCore.Qt.RightDockWidgetArea, self.docks[title])
        else:
            print('%s already started.' % (title))
        other = None
        for _, dock in self.docks.items():
            if other == None:
                if self.docks[title] != dock:
                    other = dock
                    break
        if other:
            self.tabifyDockWidget(other, self.docks[title])

    def creStatusBar(self):
        self.statusBar = QStatusBar()
        self.setStatusBar(self.statusBar)
        self.statusBar.showMessage(' Json Editor for automotive', 0)


def main():
    import argparse
    parser = argparse.ArgumentParser(description='json editer')
    parser.add_argument('-s', '--schema', type=str, default='%s/schema.json' % (PKGDIR),
                        help='input json schema file')
    parser.add_argument('-i', '--input', type=str, default='jse.json',
                        help='input json configuration file')
    args = parser.parse_args()

    qtApp = QApplication(sys.argv)
    if (os.name == 'nt'):
        qtApp.setFont(QFont('Consolas'))
    elif (os.name == 'posix'):
        qtApp.setFont(QFont('Monospace'))
    else:
        print('unKnown platform.')
    qtGui = JsonEditor(args)
    qtGui.show()
    qtApp.exec_()


if (__name__ == '__main__'):
    main()
