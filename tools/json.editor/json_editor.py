# SSAS - Simple Smart Automotive Software
# Copyright (C) 2015 ~ 2023 Parai Wang <parai@foxmail.com>

from PyQt5 import QtCore, QtGui
from PyQt5.QtGui import *
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
import logging
import re
import sys

__all__ = ['JsonModule', 'JsonAction', 'logging']

logging.basicConfig(level=logging.INFO,
                    format='%(asctime)s - %(filename)s[line:%(lineno)d] - %(levelname)s: %(message)s')

cCharWidth = 14
cActionNumber = 8

BASIC_TYPES = ['string', 'integer', 'bool', 'number']

reSField = re.compile(r'\$\{([^\s\{\}]+)\}')


def prompt(msg):
    QMessageBox(QMessageBox.Information, 'Info', msg).exec_()


class JsonString(QLineEdit):
    def __init__(self, title, obj, root):
        assert (isinstance(root, JsonModule))
        self.title = title
        self.obj = obj
        self.root = root
        super(QLineEdit, self).__init__(str(self.obj.get(self.title)))
        self.textChanged.connect(self.onTextChanged)
        desc = obj.get_desc(title)
        if desc != None:
            self.setToolTip(desc)

    def onTextChanged(self):
        text = str(self.text())
        self.obj.set(self.title, text)
        vs = self.obj.get(self.title)
        if text != vs:
            self.setText(vs)


class JsonNumber(JsonString):
    def __init__(self, title, obj, root):
        JsonString.__init__(self, title, obj, root)
        prop = obj.get_prop(title)
        self.min = prop.get('minimum', -sys.maxsize-1)
        self.max = prop.get('maximum', sys.maxsize)
        self.format = prop.get('format', 'dec')
        self.fresh()

    def timerEvent(self, event):
        self.fresh()
        self.killTimer(event.timerId())

    def fresh(self):
        vs = self.obj.get(self.title)
        if self.format == 'hex':
            if type(vs) == str:
                vs = eval(vs)
            self.setText('0x%x' % (vs))
        else:
            self.setText(str(vs))

    def is_valid(self, value):
        if (value < self.min) or (value > self.max):
            logging.error('invalid range: %s < %s < %s' % (self.min, value, self.max))
            if value < self.min:
                self.obj.set(self.title, self.min)
            if value > self.max:
                self.obj.set(self.title, self.max)
            return False
        if type(value) not in [int, float]:
            logging.error(
                'invalid value type: %s type %s not in [int, float]' % (value, type(value)))
            return False
        return True

    def onTextChanged(self, value):
        try:
            value = eval(str(self.text()))
        except Exception as e:
            if str(self.text()) in ['0x', '']:
                return
            logging.error('invalid number: %s' % (e))
            self.fresh()
            return
        ret = self.is_valid(value)
        if True != ret:
            logging.error('invalid value: %s' % (value))
            self.startTimer(2000)
            return
        self.obj.set(self.title, value)
        if self.format == 'hex':
            self.setText('0x%x' % (self.obj.get(self.title)))


class JsonInteger(JsonNumber):
    def __init__(self, title, obj, root):
        JsonNumber.__init__(self, title, obj, root)

    def is_valid(self, value):
        valid = JsonNumber.is_valid(self, value)
        if True != valid:
            return valid
        if type(value) != int:
            logging.error('invalid value type: %s type %s != int' % (value, type(value)))
            return False
        return True


class JsonEnumRefString(QComboBox):
    def __init__(self, title, obj, root):
        assert (isinstance(root, JsonModule))
        prop = obj.get_prop(title)
        self.enumref = prop['enumref']
        self.enumitems = prop.get('enum', [])
        self.title = title
        self.obj = obj
        self.root = root
        super(QComboBox, self).__init__()
        self.initItems()
        self.currentTextChanged.connect(self.onTextChanged)
        desc = obj.get_desc(title)
        if desc != None:
            self.setToolTip(desc)
        self.timerEvent(None)
        self.startTimer(1000)

    def timerEvent(self, event):
        items = self.obj.find(self.enumref)
        if None != items:
            vs = str(self.obj.get(self.title))
            self.clear()
            self.addItems(self.enumitems + [str(i) for i in items])
            self.setCurrentIndex(self.findText(vs))

    def initItems(self):
        items = self.obj.find(self.enumref)
        if None != items:
            self.addItems([str(i) for i in items])
            vs = str(self.obj.get(self.title))
            self.setCurrentIndex(self.findText(vs))

    def onTextChanged(self, text):
        text = str(text)
        self.obj.set(self.title, text)


class JsonEnumRefInteger(JsonEnumRefString):
    def __init__(self, title, obj, root):
        ArgEnumRefString.__init__(self, title, obj, root)

    def onTextChanged(self, text):
        try:
            value = eval(str(self.text()))
        except Exception as e:
            logging.error('invalid int: %s' % (e))
            return
        self.obj.set(self.title, value)


class JsonEnumString(QComboBox):
    def __init__(self, title, obj, root):
        assert (isinstance(root, JsonModule))
        prop = obj.get_prop(title)
        self.items = prop['enum']
        self.title = title
        self.obj = obj
        self.root = root
        super(QComboBox, self).__init__()
        self.initItems()
        self.currentTextChanged.connect(self.onTextChanged)
        desc = obj.get_desc(title)
        if desc != None:
            self.setToolTip(desc)

    def initItems(self):
        self.addItems([str(i) for i in self.items])
        vs = str(self.obj.get(self.title))
        self.setCurrentIndex(self.findText(vs))

    def onTextChanged(self, text):
        text = str(text)
        self.obj.set(self.title, text)


class JsonEnumInteger(JsonEnumString):
    def __init__(self, title, obj, root):
        JsonEnumString.__init__(self, title, obj, root)

    def onTextChanged(self, text):
        try:
            value = eval(str(self.text()))
        except Exception as e:
            logging.error('invalid int: %s' % (e))
            return
        self.obj.set(self.title, value)


class JsonBool(QComboBox):
    def __init__(self, title, obj, root):
        assert (isinstance(root, JsonModule))
        self.title = title
        self.obj = obj
        self.root = root
        super(QComboBox, self).__init__()
        self.initItems()
        self.currentTextChanged.connect(self.onTextChanged)
        desc = obj.get_desc(title)
        if desc != None:
            self.setToolTip(desc)

    def initItems(self):
        self.addItems(['True', 'False'])
        vs = str(self.obj.get(self.title))
        self.setCurrentIndex(self.findText(vs))

    def onTextChanged(self, text):
        text = str(text)
        if text == 'True':
            text = True
        else:
            text = False
        self.obj.set(self.title, text)


class JsonMapString(QComboBox):
    def __init__(self, title, obj, root):
        assert (isinstance(root, JsonModule))
        assert (isinstance(obj, JsonObject))
        prop = obj.get_prop(title)
        self.map = prop['map']
        self.friends = prop.get('friends', [])
        self.title = title
        self.obj = obj
        self.root = root
        super(QComboBox, self).__init__()
        self.initItems()
        self.currentTextChanged.connect(self.onTextChanged)

    def initItems(self):
        items = self.map.keys()
        self.addItems([str(i) for i in items])
        vs = str(self.obj.get(self.title))
        self.setCurrentIndex(self.findText(vs))

    def onTextChanged(self, text):
        text = str(text)
        self.obj.set(self.title, text)
        mp = self.map[text]
        for f in self.friends:
            v2 = mp[f]
            self.obj.set(f, v2)
        if mp.get('extend', False):
            self.obj.extend(text)
        else:
            self.obj.rm_extend()


def JsonValue(title, obj, root):
    prop = obj.get_prop(title)
    logging.debug('create widget with property: %s', prop)
    if 'enumref' in prop:
        if prop['type'] == 'string':
            return JsonEnumRefString(title, obj, root)
        elif prop['type'] == 'integer':
            return JsonEnumRefInteger(title, obj, root)
    elif 'enum' in prop:
        if prop['type'] == 'string':
            return JsonEnumString(title, obj, root)
        elif prop['type'] == 'integer':
            return JsonEnumInteger(title, obj, root)
    elif 'map' in prop:
        if prop['type'] == 'string':
            return JsonMapString(title, obj, root)
    elif prop['type'] == 'string':
        return JsonString(title, obj, root)
    elif prop['type'] == 'integer':
        return JsonInteger(title, obj, root)
    elif prop['type'] == 'number':
        return JsonNumber(title, obj, root)
    elif prop['type'] == 'bool':
        return JsonBool(title, obj, root)
    return QLineEdit('Type %s not supported for %s' % (prop['type'], title))


class JsonAction(QAction):
    def __init__(self, text, parent):
        super(QAction, self).__init__(text, parent)
        self.root = parent
        self.triggered.connect(self.onAction)

    def onAction(self):
        self.root.onAction(str(self.text()))


class UIGroup(QScrollArea):
    def __init__(self, wd, parent=None):
        super(QScrollArea, self).__init__(parent)
        self.setWidget(wd)
        self.timerEvent(0)
        self.startTimer(1000)

    def timerEvent(self, event):
        frame = self.widget()
        grid = frame.layout()
        for row in range(grid.rowCount()):
            K = grid.itemAtPosition(row, 0).widget()
            V = grid.itemAtPosition(row, 1).widget()
            enabled = V.obj.is_enabled() and V.obj.is_enabled(V.title)
            K.setVisible(enabled)
            V.setVisible(enabled)
            V.setEnabled(enabled)


class JsonBase(QTreeWidgetItem):
    def __init__(self, title, schema, root, parent=None):
        assert (isinstance(root, JsonModule))
        QTreeWidgetItem.__init__(self, parent)
        self.root = root
        self.title = title
        self.schema = dict(schema)
        logging.debug('%s schema: %s', schema['type'], schema)
        self.reload()
        self.setText(0, self.name())

    def preproc(self, url):
        rsts = reSField.findall(url)
        if rsts:
            for field in rsts:
                v = self.find(field)
                logging.debug('%s: replace field: %s = %s', url, field, v)
                url = url.replace('${%s}' % (field), str(v))
        return url

    def is_enabled(self, attr=None):
        prop = self.get_prop(attr)
        es = prop.get('enabled', 'True')
        es = self.preproc(es)
        logging.debug('attr %s of prop %s condition: %s', attr, prop, es)
        return eval(es)

    def find(self, url):
        url = self.preproc(url)
        logging.debug('find url: %s', url)
        if url[0] == '/':
            return self.root.find(url[1:])
        which = self
        urls = url.split('/')
        for u in urls[:-1]:
            if u == '..':
                which = which.parent()
            elif u == './':
                pass
            else:
                raise
        attr = urls[-1]
        if ':' in attr:
            cfg = which.toJSON()
            title, field = attr.split(':')
            logging.debug('which cfg: %s, title: %s, field: %s', cfg, title, field)
            return [x[field] for x in cfg[title]]
        return which.get(attr)

    def reload(self):
        raise

    def reloadUI(self, wConfig):
        raise

    def toJSON(self):
        raise

    def isList(self):
        return False

    def isObj(self):
        return False

    def clear(self):
        for i in range(self.childCount()):
            self.takeChild(0)

    def get_desc(self, attr):
        prop = self.get_prop(attr)
        return prop.get('description', None)

    def get_prop(self, attr):
        if None == attr:
            return self.schema
        logging.debug('get attr %s prop from schema %s', attr, self.schema)
        if 'properties' in self.schema:
            prop = self.schema['properties'][attr]
        else:
            prop = self.schema
        return prop

    def auto_field(self, attr):
        updated = False
        value = self.get(attr)
        if type(value) == str:
            rsts = reSField.findall(value)
            if rsts:
                for field in rsts:
                    v = self.find(field)
                    logging.debug('auto field: %s = %s', field, v)
                    value = value.replace('${%s}' % (field), str(v))
                self.schema[attr] = value
                updated = True
        return updated

    def get(self, attr):
        logging.debug('get %s of schema: %s', attr, self.schema)
        if attr not in self.schema:
            try:
                prop = self.get_prop(attr)
            except KeyError:
                if attr == 'name':
                    prop = {'default': self.title}
                else:
                    raise Exception('no attribute %s for %s' % (attr, self.schema))
            if 'default' in prop:
                self.schema[attr] = prop['default']
            elif prop['type'] == 'string':
                self.schema[attr] = 'TBD'
            elif prop['type'] == 'integer':
                self.schema[attr] = 0
            else:
                self.schema[attr] = 'unknown'
        return self.schema[attr]

    def set(self, attr, value):
        ''' return True if has some automatical updates'''
        updated = self.auto_field(attr)
        if updated:
            return True
        self.schema[attr] = value
        if attr == 'name':
            self.setText(0, value)
        return False

    def name(self):
        if 'name' not in self.schema:
            if self.parent() != None:
                logging.debug('new %s for %s, idx=%s' %
                              (self.title, self.parent().title, self.parent().childCount()))
                vs = self.get('name')
                if vs in ['TBD', 0]:
                    vs = '%s%s' % (self.title, self.parent().childCount())
                self.schema['name'] = vs
            else:
                self.schema['name'] = self.title
        return self.schema['name']

    def onItemSelectionChanged(self):
        logging.debug('object %s ononItemSelectionChanged: isList=%s, schema: %s' %
                      (self.title, self.isList(), self.schema))
        Index = 0
        if self.isList():
            items = self.schema['items']
            if self.listMaxAllowed() > self.childCount():
                if self.is_enabled(self.title):
                    self.root.actions[Index].setText('Add %s' % (items['title']))
                    self.root.actions[Index].setDisabled(False)
                    Index += 1
                else:
                    self.clear()
        if (self.parent() != None and self.parent().isList()):  # if parent is None, then it is top obj, cann't be deleted
            self.root.actions[Index].setText('Delete %s' % (self.title))
            self.root.actions[Index].setDisabled(False)
            Index += 1
        for i in range(Index, cActionNumber):
            self.root.actions[i].setDisabled(True)
            self.root.actions[i].setText('')

        expand = True
        for i in range(self.childCount()):
            obj = self.child(i)
            if not obj.is_enabled():
                expand = False
        self.setExpanded(expand)
        self.root.showConfig(self)

    def addChildObj(self, obj):
        self.addChild(obj)
        self.onItemSelectionChanged()  # trigger refresh

    def reloadUI(self):
        raise


class JsonBasic(JsonBase):
    def __init__(self, title, schema, root, parent=None):
        JsonBase.__init__(self, title, schema, root, parent)
        self.display(self.get(self.title))

    def reload(self):
        if '__init__' in self.schema:
            self.schema[self.title] = self.schema['__init__']

    def display(self, value):
        if 'hex' == self.schema.get('format', None):
            if type(value) == int:
                value = '0x%x' % (value)
        self.setText(0, '%s: %s' % (self.title, str(value)))

    def set(self, attr, value):
        self.schema[attr] = value
        self.display(value)

    def toJSON(self):
        return self.get(self.title)

    def createUI(self):
        K = QLabel(self.title)
        V = JsonValue(self.title, self, self.root)
        return [(K, V)]

    def reloadUI(self):
        frame = QFrame()
        frame.setMinimumWidth(self.root.width()*3/5)
        grid = QGridLayout()
        frame.setLayout(grid)
        K = QLabel(self.title)
        V = JsonValue(self.title, self, self.root)
        grid.addWidget(K, 0, 0)
        grid.addWidget(V, 0, 1)
        return UIGroup(frame)


class JsonObject(JsonBase):
    def __init__(self, title, schema, root, parent=None):
        JsonBase.__init__(self, title, schema, root, parent)
        for title, schema in self.schema['properties'].items():
            if 'map' in schema:
                value = self.get(title)
                if schema['map'].get(value, {}).get('extend', False):
                    self.extend(value, True)

    def extend(self, ext, slient=False):
        if ext in self.schema.get('extends', {}):
            properties = dict(self.schema['extends'][ext])
            if not slient:
                self.schema['__init__'] = self.toJSON()
            logging.debug('extend schema %s with new properties "%s":%s' %
                          (self.schema, ext, properties))
            if '__properties__' not in self.schema:
                self.schema['__properties__'] = dict(self.schema['properties'])
            else:
                for title, schema in self.schema['properties'].items():
                    if title in properties:
                        if title in self.schema['__init__']:
                            del self.schema['__init__'][title]
                        if title in self.schema:
                            del self.schema[title]
            properties.update(self.schema['__properties__'])
            self.schema['properties'] = properties
            self.clear()
            self.reload()
            if not slient:
                self.root.showConfig(self)
        else:
            logging.error('extend %s properties not found for %s' % (ext, self.schema))

    def rm_extend(self):
        if '__properties__' in self.schema:
            self.schema['__init__'] = self.toJSON()
            self.schema['properties'] = self.schema['__properties__']
            self.clear()
            self.reload()
            self.root.showConfig(self)

    def reload(self):
        cfg = self.schema.get('__init__', {})
        for title, schema in self.schema['properties'].items():
            if schema['type'] in ['object', 'array']:
                schema = dict(schema)
                if title in cfg:
                    schema['__init__'] = cfg[title]
                schema['name'] = title
                self.addChild(JsonItem(title, schema, self.root, self))
            else:
                if title in cfg:
                    self.schema[title] = cfg[title]
                    logging.debug('reload object property:%s' % (self.schema))

    def isObj(self):
        return True

    def toJSON(self):
        cfg = {}
        for title, prop in self.schema['properties'].items():
            if prop['type'] not in ['object', 'array']:
                if self.is_enabled(title):
                    cfg[title] = self.get(title)
        for i in range(self.childCount()):
            obj = self.child(i)
            if obj.is_enabled():
                cfg[obj.name()] = obj.toJSON()
        return cfg

    def createUI(self, prefix=''):
        UIs = []
        orders = list(self.schema.get('orders', []))
        for title, schema in self.schema['properties'].items():
            if title not in orders:
                orders.append(title)
        logging.debug('create UI: orders = %s, schema = %s' % (orders, self.schema))
        for title in orders:
            schema = self.schema['properties'][title]
            if schema['type'] in BASIC_TYPES:
                K = QLabel('%s%s' % (prefix, title))
                V = JsonValue(title, self, self.root)
                UIs.append((K, V))
        return UIs

    def reloadUI(self):
        frame = QFrame()
        frame.setMinimumWidth(self.root.width()*3/5)
        grid = QGridLayout()
        frame.setLayout(grid)
        UIs = []
        UIs += self.createUI()
        for i in range(0, self.childCount()):
            obj1 = self.child(i)
            if obj1.isObj():
                UIs += obj1.createUI(prefix='%s: ' % (obj1.title))
            elif obj1.isList() and self.schema.get('display_with_list', True):
                num = obj1.childCount()
                if num > 8:
                    continue
                lUIs = obj1.createUI()
                if num > 0:
                    num = len(lUIs) // num
                else:
                    num = 0
                for i, (K, V) in enumerate(lUIs):
                    K.setText('%s %s: %s' % (obj1.title, i // num, K.text()))
                UIs += lUIs
        for column, (K, V) in enumerate(UIs):
            grid.addWidget(K, column, 0)
            grid.addWidget(V, column, 1)
        return UIGroup(frame)


class JsonArray(JsonBase):
    def __init__(self, title, schema, root, parent=None):
        JsonBase.__init__(self, title, schema, root, parent)

    def name(self):
        return self.title

    def isList(self):
        return True

    def reload(self):
        if '__init__' in self.schema:
            items = dict(self.schema['items'])
            for cfg in self.schema['__init__']:
                schema = dict(items)
                schema['__init__'] = cfg
                logging.debug('reload list %s:%s' % (schema['type'], schema))
                self.addChild(JsonItem(items['title'], schema, self.root, self))

    def listMaxAllowed(self):
        return self.schema.get('max', 65535)

    def toJSON(self):
        cfg = []
        for i in range(self.childCount()):
            obj = self.child(i)
            cfg.append(obj.toJSON())
        return cfg

    def onAction_Add(self, what):
        items = self.schema['items']
        if items['title'] == what:
            if self.isList():
                if self.listMaxAllowed() > self.childCount():
                    self.addChildObj(JsonItem(items['title'], items, self.root, self))
                else:
                    logging.error('Error:Maximum %s for %s is %s!' % (what, items['title'], mx))
            self.setExpanded(True)
        else:
            logging.error('add with what=%s != %s' % (what, items['title']))

    def createUI(self):
        UIs = []
        for i in range(0, self.childCount()):
            obj1 = self.child(i)
            if not obj1.isList():
                UIs += obj1.createUI()
        return UIs

    def reloadUI(self):
        table = QTableWidget()
        headers = []
        widths = []
        UIs = []
        for i in range(0, self.childCount()):
            obj1 = self.child(i)
            if not obj1.isList():
                uis = obj1.createUI()
                uis_ = []
                for pos, (K, V) in enumerate(uis):
                    if not V.obj.is_enabled(V.title):
                        continue
                    uis_.append([K, V])
                    title = str(K.text())
                    if title not in headers:
                        headers.insert(pos, title)
                        widths.insert(pos, len(title)*cCharWidth)
                UIs.append(uis_)
        table.setColumnCount(len(headers))
        table.setHorizontalHeaderLabels(headers)
        for uis in UIs:
            index = table.rowCount()
            table.setRowCount(index+1)
            for K, V in uis:
                title = str(K.text())
                column = headers.index(title)
                table.setCellWidget(index, column, V)
                if ((len(str(V.obj.get(title)))+1)*cCharWidth > widths[column]):
                    widths[column] = (len(str(V.obj.get(title)))+1)*cCharWidth
        for column in range(len(widths)):
            table.setColumnWidth(column, widths[column])
        table.setMinimumWidth(self.root.width()*3/4)
        return table


def JsonItem(title, schema, root, parent=None):
    if schema['type'] == 'object':
        return JsonObject(title, schema, root, parent)
    elif schema['type'] == 'array':
        return JsonArray(title, schema, root, parent)
    elif schema['type'] in BASIC_TYPES:
        return JsonBasic(title, schema, root, parent)
    else:
        raise


class JsonObjectTree(QTreeWidget):
    def __init__(self, schema, parent=None):
        assert (isinstance(parent, JsonModule))
        super(QTreeWidget, self).__init__(parent)
        self.root = parent
        self.title = schema['title']
        self.schema = schema
        self.objMap = {}
        logging.debug('tree schema: %s', schema)
        self.setHeaderLabel('%s' % (self.schema['title']))
        for title, schema in self.schema['properties'].items():
            if title in self.schema.get('__init__', {}):
                schema = dict(schema)
                schema['__init__'] = self.schema['__init__'][title]
            obj = JsonItem(title, schema, self.root)
            self.objMap[title] = obj
            self.addTopLevelItem(obj)
        self.itemSelectionChanged.connect(self.onItemSelectionChanged)
        self.setMaximumWidth(600)

    def toJSON(self):
        cfg = {'class': self.title}
        for title, schema in self.schema['properties'].items():
            jse = self.objMap[title].toJSON()
            logging.debug('tree toJSON %s %s: %s' % (schema['type'], title, jse))
            cfg[title] = jse
        return cfg

    def onItemSelectionChanged(self):
        obj = self.currentItem()
        if (isinstance(obj, JsonBase)):
            obj.onItemSelectionChanged()

    def onAction_Delete(self, what):
        obj = self.currentItem()
        assert (isinstance(obj, JsonBase))
        assert (obj.title == what)
        if (self.indexOfTopLevelItem(obj) != -1):
            self.takeTopLevelItem(self.indexOfTopLevelItem(obj))
        else:
            pObj = obj.parent()
            pObj.takeChild(pObj.indexOfChild(obj))

    def onAction(self, text):
        reAction = re.compile(r'(Add|Delete) ([^\s]+)')
        action = reAction.search(text).groups()
        if (action[0] == 'Add'):
            obj = self.currentItem()
            assert (isinstance(obj, JsonArray))
            obj.onAction_Add(action[1])
        elif (action[0] == 'Delete'):
            self.onAction_Delete(action[1])


class JsonModule(QMainWindow):
    def __init__(self, schema, main):
        self.actions = []
        super(QMainWindow, self).__init__()
        self.main = main
        self.title = schema['title']
        self.qSplitter = QSplitter(Qt.Horizontal, self)

        self.objTree = JsonObjectTree(schema, self)
        self.wConfig = QMainWindow()
        self.qSplitter.insertWidget(0, self.objTree)
        self.qSplitter.insertWidget(1, self.wConfig)

        self.setCentralWidget(self.qSplitter)
        self.creActions()

    def creActions(self):
        #  create cActionNumber action
        self.actionBar = QToolBar()
        self.addToolBar(Qt.TopToolBarArea, self.actionBar)
        for i in range(0, cActionNumber):
            qAction = JsonAction(self.tr(''), self)
            self.actionBar.addAction(qAction)
            qAction.setDisabled(True)
            self.actions.append(qAction)

    def toJSON(self):
        return self.objTree.toJSON()

    def find(self, url):
        return self.main.find(url)

    def onAction(self, text):
        self.objTree.onAction(text)

    def showConfig(self, obj):
        assert (isinstance(obj, JsonBase))
        self.wConfig.setCentralWidget(obj.reloadUI())
