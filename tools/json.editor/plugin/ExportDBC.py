# SSAS - Simple Smart Automotive Software
# Copyright (C) 2015 ~ 2023 Parai Wang <parai@foxmail.com>
import sys
import os
from PyQt5 import QtCore, QtGui
from PyQt5.QtGui import *
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *

_dbc_header = '''VERSION "HNPBNNNYYNYNNYYNNNNNNNNNNNNNNNYNNNNYNNNNNN/4/%%%/4/'%**4NNN///"


NS_ :
    NS_DESC_
    CM_
    BA_DEF_
    BA_
    VAL_
    CAT_DEF_
    CAT_
    FILTER
    BA_DEF_DEF_
    EV_DATA_
    ENVVAR_DATA_
    SGTYPE_
    SGTYPE_VAL_
    BA_DEF_SGTYPE_
    BA_SGTYPE_
    SIG_TYPE_REF_
    VAL_TABLE_
    SIG_GROUP_
    SIG_VALTYPE_
    SIGTYPE_VALTYPE_
    BO_TX_BU_
    BA_DEF_REL_
    BA_REL_
    BA_DEF_DEF_REL_
    BU_SG_REL_
    BU_EV_REL_
    BU_BO_REL_
    SG_MUL_VAL_

BS_:

BU_: '''


class Plugin(QAction):
    def __init__(self, root):
        self.root = root
        super(QAction, self).__init__(root.tr('Export as Vector CAN DBC'), root)
        self.setStatusTip('Export COM signals as Vector CAN DBC')
        self.triggered.connect(self.onAction)

    def GenBO(self, fp, message):
        fp.write('\nBO_ %s %s: %s %s\n' %
                 (message['id'], message['name'], message['dlc'], message['node']))
        for sig in message.get('signals', []):
            endian = 0 if sig['endian'] == 'big' else 1
            fp.write('SG_ %s : %s|%s@%s+ (1,0) [0|4294967295] "" %s\n' % (
                sig['name'], sig['start'], sig['size'], endian, message['node']))

    def onAction(self):
        com = self.root.find('Com')
        dir = QFileDialog.getExistingDirectory(
            None, 'Save as Vector CAN DBC', '.', QFileDialog.ShowDirsOnly)
        if (not os.path.exists(dir)):
            return
        for network in com.get('networks', []):
            fp = open('%s/%s.dbc' % (dir, network['name']), 'w')
            fp.write(_dbc_header)
            nodes = []
            for message in network.get('messages', []):
                n = message['node']
                if n not in nodes:
                    nodes.append(n)
            fp.write('%s\n' % (' '.join(nodes)))
            for message in network.get('messages', []):
                self.GenBO(fp, message)
            fp.write('\n\n\n\n')
            fp.close()
        QMessageBox(QMessageBox.Information, 'Info',
                    'Export COM signals as Vector CAN DBC Successfully !').exec_()
