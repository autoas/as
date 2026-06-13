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
sys.path.append(os.path.abspath("%s/.." % (PKGDIR)))


class QACollapseWidget(QWidget):
    fix_applied = QtCore.pyqtSignal(list)

    def __init__(self, question, answer, fixed_config=None, parent=None):
        super(QACollapseWidget, self).__init__(parent)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        self.fixed_config = fixed_config

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        self.header = QWidget()
        self.header.setStyleSheet("background-color: #e8f4fc; padding: 8px; border-bottom: 1px solid #b3d9e8;")
        header_layout = QHBoxLayout(self.header)
        header_layout.setContentsMargins(0, 0, 0, 0)

        self.expand_icon = QLabel("-")
        self.expand_icon.setFont(QFont("Arial", 10))
        header_layout.addWidget(self.expand_icon)

        self.question_label = QLabel(f"Q: {question[:50]}..." if len(question) > 50 else f"Q: {question}")
        self.question_label.setFont(QFont("Arial", 10, QFont.Bold))
        self.question_label.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        self.question_label.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
        header_layout.addWidget(self.question_label)

        layout.addWidget(self.header)

        self.content = QWidget()
        self.content.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        content_layout = QVBoxLayout(self.content)
        content_layout.setContentsMargins(0, 0, 0, 0)
        content_layout.setSpacing(0)

        self.answer_text = QTextEdit()
        self.answer_text.setReadOnly(True)
        self.answer_text.setFont(QFont("Consolas", 10))
        self.answer_text.setText(answer)
        self.answer_text.setStyleSheet("background-color: #fafafa; border: none;")
        self.answer_text.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.answer_text.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        self.answer_text.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        content_layout.addWidget(self.answer_text)

        if self.fixed_config:
            button_bar = QWidget()
            button_bar.setStyleSheet("background-color: #f0f0f0; padding: 8px; border-top: 1px solid #ccc;")
            button_layout = QHBoxLayout(button_bar)

            self.fix_button = QPushButton("Apply Fix")
            self.fix_button.setStyleSheet(
                "background-color: #4CAF50; color: white; border: none; padding: 6px 12px; border-radius: 4px;"
            )
            self.fix_button.clicked.connect(self.apply_fix)
            button_layout.addWidget(self.fix_button)

            button_layout.addStretch()
            content_layout.addWidget(button_bar)

        layout.addWidget(self.content)

        self.header.mousePressEvent = self.toggle_expand
        self.expanded = True

    def toggle_expand(self, event):
        self.expanded = not self.expanded
        self.content.setVisible(self.expanded)
        self.expand_icon.setText("- " if self.expanded else "+ ")

    def apply_fix(self):
        if self.fixed_config:
            self.fix_applied.emit(self.fixed_config)
            if hasattr(self, "fix_button"):
                self.fix_button.setText("Fix Applied")
                self.fix_button.setEnabled(False)
                self.fix_button.setStyleSheet(
                    "background-color: #9E9E9E; color: white; border: none; padding: 6px 12px; border-radius: 4px;"
                )


class AIWorker(QtCore.QThread):
    finished = QtCore.pyqtSignal(str, str)
    error = QtCore.pyqtSignal(str, str)

    def __init__(self, agent, question):
        super(AIWorker, self).__init__()
        self.agent = agent
        self.question = question

    def run(self):
        try:
            answer = self.agent.chat(self.question)
            self.finished.emit(self.question, answer)
        except Exception as e:
            self.error.emit(self.question, str(e))


class AIValidationDialog(QMainWindow):
    fix_applied = QtCore.pyqtSignal(list)

    def __init__(self, parent=None, agent=None):
        super(AIValidationDialog, self).__init__(parent)
        self.setWindowTitle("AI Validation Assistant")
        self.setMinimumSize(800, 600)
        self.resize(900, 700)
        self.setWindowFlags(QtCore.Qt.Window)

        self.agent = agent

        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        self.toolbar = QWidget()
        self.toolbar.setStyleSheet("background-color: #4a90d9; padding: 8px;")
        toolbar_layout = QHBoxLayout(self.toolbar)

        model_label = QLabel("Model:")
        model_label.setStyleSheet("color: white; font-weight: bold;")
        toolbar_layout.addWidget(model_label)

        self.model_combo = QComboBox()
        self.model_combo.setStyleSheet("background-color: white; color: black; padding: 4px; border-radius: 4px;")
        if agent:
            models = agent.get_available_models()
            for model_id, display_name in models:
                self.model_combo.addItem(display_name, model_id)
                if model_id == agent.model:
                    self.model_combo.setCurrentText(display_name)
        self.model_combo.currentIndexChanged.connect(self.on_model_changed)
        toolbar_layout.addWidget(self.model_combo)

        toolbar_layout.addStretch()
        layout.addWidget(self.toolbar)

        self.scroll_area = QScrollArea()
        self.scroll_area.setWidgetResizable(True)
        self.scroll_content = QWidget()
        self.scroll_layout = QVBoxLayout(self.scroll_content)
        self.scroll_layout.setContentsMargins(10, 10, 10, 10)
        self.scroll_layout.setSpacing(10)
        self.scroll_area.setWidget(self.scroll_content)
        layout.addWidget(self.scroll_area)

        self.current_fixed_config = None

        self.input_bar = QWidget()
        self.input_bar.setStyleSheet("background-color: #f0f0f0; padding: 8px; border-top: 1px solid #ccc;")
        input_layout = QHBoxLayout(self.input_bar)

        self.input_field = QLineEdit()
        self.input_field.setPlaceholderText("Ask AI for help...")
        self.input_field.returnPressed.connect(self.send_message)
        input_layout.addWidget(self.input_field)

        self.send_button = QPushButton("Send")
        self.send_button.clicked.connect(self.send_message)
        input_layout.addWidget(self.send_button)

        self.fix_button = QPushButton("Apply Fix")
        self.fix_button.setStyleSheet(
            "background-color: #4CAF50; color: white; border: none; padding: 6px 12px; border-radius: 4px;"
        )
        self.fix_button.clicked.connect(self.apply_fix)
        self.fix_button.hide()
        input_layout.addWidget(self.fix_button)

        layout.addWidget(self.input_bar)

        self.setStyleSheet(
            """
            QDialog {
                border: 1px solid #ccc;
                border-radius: 4px;
            }
            QScrollArea {
                background-color: white;
            }
        """
        )

    def on_model_changed(self, index):
        if self.agent:
            model_id = self.model_combo.itemData(index)
            if model_id:
                self.agent.set_model(model_id)

    def add_qa(self, question, answer, fixed_config=None):
        qa_widget = QACollapseWidget(question, answer, fixed_config)
        qa_widget.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        qa_widget.fix_applied.connect(self.on_fix_applied)
        self.scroll_layout.addWidget(qa_widget)

        QApplication.processEvents()
        scroll_bar = self.scroll_area.verticalScrollBar()
        scroll_bar.setValue(scroll_bar.maximum())

    def on_fix_applied(self, fixed_config):
        self.fix_applied.emit(fixed_config)

    def send_message(self):
        question = self.input_field.text().strip()
        if not question or not self.agent:
            return

        self.input_field.clear()
        self.input_field.setEnabled(False)
        self.send_button.setEnabled(False)

        if hasattr(self.parent(), "toJSON"):
            cfgs = self.parent().toJSON()
            self.agent.load_config(cfgs)

        self.worker = AIWorker(self.agent, question)
        self.worker.finished.connect(self.on_worker_finished)
        self.worker.error.connect(self.on_worker_error)
        self.worker.start()

    def _parse_and_display_response(self, raw_text: str, question: str) -> None:
        """Parse AI JSON response and display it in the UI"""
        try:
            clean_text = self._clean_json_response(raw_text)
            response = json.loads(clean_text)

            success = response.get("success", False)
            message = response.get("message", "")
            explanation = response.get("explanation", "")
            issues = response.get("issues", [])
            fixed_config = response.get("fixed_config", None)

            full_text = f"**Status:** {'? Success' if success else '? Failed'}\n\n"
            if message:
                full_text += f"**Message:** {message}\n\n"

            if explanation:
                explanation = explanation.replace("\\n", "\n").replace("\\t", "\t")
                full_text += f"## Explanation\n\n{explanation}\n\n"

            if issues:
                full_text += "## Issues Found\n\n"
                for i, issue in enumerate(issues, 1):
                    severity = issue.get("severity", "INFO")
                    module = issue.get("module", "")
                    description = issue.get("description", "")
                    location = issue.get("location", "")
                    suggestion = issue.get("suggestion", "")

                    severity_icon = {"ERROR": "?", "WARNING": "?", "INFO": "?"}.get(severity, "?")
                    full_text += f"{severity_icon} **Issue {i}:** [{severity}] {module}\n\n"
                    if location:
                        full_text += f"  - **Location:** {location}\n"
                    if description:
                        full_text += f"  - **Description:** {description}\n"
                    if suggestion:
                        full_text += f"  - **Suggestion:** {suggestion}\n"
                    full_text += "\n"

            if fixed_config and isinstance(fixed_config, list) and len(fixed_config) > 0 and fixed_config != [{}]:
                full_text += "## Fixed Configuration\n\n"
                full_text += "```json\n"
                full_text += json.dumps(fixed_config, indent=2, ensure_ascii=False)
                full_text += "\n```\n"

            if fixed_config and not isinstance(fixed_config, list):
                fixed_config = [fixed_config]

            has_changes = False
            if fixed_config and len(fixed_config) > 0 and fixed_config != [{}]:
                if self.agent and self.agent.config:
                    has_changes = json.dumps(fixed_config, sort_keys=True) != json.dumps(
                        self.agent.config, sort_keys=True
                    )
                else:
                    has_changes = True

            self.current_fixed_config = fixed_config if (has_changes) else None
            self.add_qa(question, full_text, self.current_fixed_config)
            self.update_fix_button()
        except json.JSONDecodeError as e:
            cleaned_text = raw_text.replace("\\n", "\n").replace("\\t", "\t")
            full_text = f"**JSON Parse Error:** {str(e)}\n\n**Raw Response:**\n\n{cleaned_text}"
            self.add_qa(question, full_text)
            self.current_fixed_config = None
            self.update_fix_button()

    def on_worker_finished(self, question, answer):
        self._parse_and_display_response(answer, question)
        self.input_field.setEnabled(True)
        self.send_button.setEnabled(True)

    def on_worker_error(self, question, error):
        self.add_qa(question, f"Error: {error}")
        self.input_field.setEnabled(True)
        self.send_button.setEnabled(True)

    def _clean_json_response(self, text):
        text = text.strip()
        if text.startswith("```json"):
            text = text[7:]
        elif text.startswith("```"):
            text = text[3:]
        if text.endswith("```"):
            text = text[:-3]
        return text.strip()

    def set_results(self, text, question="help validate config"):
        self._parse_and_display_response(text, question)

    def set_agent(self, agent):
        self.agent = agent

    def update_fix_button(self):
        if hasattr(self, "fix_button"):
            if self.current_fixed_config:
                self.fix_button.show()
                self.fix_button.setEnabled(True)
                self.fix_button.setText("Apply Fix")
                self.fix_button.setStyleSheet(
                    "background-color: #4CAF50; color: white; border: none; padding: 6px 12px; border-radius: 4px;"
                )
            else:
                self.fix_button.hide()

    def apply_fix(self):
        if self.current_fixed_config:
            self.fix_applied.emit(self.current_fixed_config)
            self.fix_button.setText("Fix Applied")
            self.fix_button.setEnabled(False)
            self.fix_button.setStyleSheet(
                "background-color: #9E9E9E; color: white; border: none; padding: 6px 12px; border-radius: 4px;"
            )


class JsonDockWidget(QDockWidget):
    isClosed = False

    def __init__(self, title, parent=None):
        QDockWidget.__init__(self, title, parent)
        self.setAllowedAreas(QtCore.Qt.LeftDockWidgetArea | QtCore.Qt.RightDockWidgetArea)
        # self.setFeatures(QDockWidget.DockWidgetClosable|QDockWidget.DockWidgetMovable)

    def closeEvent(self, event):
        self.isClosed = True


class JsonEditor(QMainWindow):
    def __init__(self, args):
        self.modules = {}
        self.docks = {}
        self.actions = {}
        self.cfgMap = {}

        QMainWindow.__init__(self, None)
        self.setWindowTitle("JSON Editor")
        self.setMinimumSize(800, 400)

        self.schemaFile = args.schema
        self.jsonFile = args.input
        self.jsDir = None

        self.creStatusBar()
        self.load_schema()
        self.creMenu()
        self.loadPlugins()

        if self.jsonFile and os.path.isfile(self.jsonFile):
            self.mOpen(self.jsonFile)

        self.showMaximized()

    def loadPlugins(self):
        tMenu = self.menuBar().addMenu(self.tr("Plugin"))
        plgdir = os.path.abspath("%s/plugin" % (PKGDIR))
        for plg in glob.glob("%s/*.py" % (plgdir)):
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
        tMenu = self.menuBar().addMenu(self.tr("File"))
        # Open Ctrl+O
        sItem = QAction(self.tr("Open"), self)
        sItem.setShortcut("Ctrl+O")
        sItem.setStatusTip("Open a JSON configure file.")
        sItem.triggered.connect(self.mOpen)
        tMenu.addAction(sItem)
        # Load Ctrl+L
        sItem = QAction(self.tr("Load"), self)
        sItem.setShortcut("Ctrl+L")
        sItem.setStatusTip("Load a JSON module configure file.")
        sItem.triggered.connect(self.mLoad)
        tMenu.addAction(sItem)
        # Load Ctrl+D
        sItem = QAction(self.tr("Load Directory"), self)
        sItem.setShortcut("Ctrl+D")
        sItem.setStatusTip("Load all the JSON configure file under the directory.")
        sItem.triggered.connect(self.mLoadDir)
        tMenu.addAction(sItem)
        # Save Ctrl+S
        sItem = QAction(self.tr("Save"), self)
        sItem.setShortcut("Ctrl+S")
        sItem.setStatusTip("Save the JSON configure file.")
        sItem.triggered.connect(self.mSave)
        tMenu.addAction(sItem)
        # AI Validate Ctrl+V
        sItem = QAction(self.tr("AI Validate"), self)
        sItem.setShortcut("Ctrl+V")
        sItem.setStatusTip("Validate configuration using AI.")
        sItem.triggered.connect(self.mAIValidate)
        tMenu.addAction(sItem)
        # Save Ctrl+G
        sItem = QAction(self.tr("Generate"), self)
        sItem.setShortcut("Ctrl+G")
        sItem.setStatusTip("Convert the JSON configure file to C Code.")
        sItem.triggered.connect(self.mGen)
        tMenu.addAction(sItem)
        # Json Module
        tMenu = self.menuBar().addMenu(self.tr("Module"))
        for schema in self.schema:
            sItem = JsonAction(self.tr(schema["title"]), self)
            sItem.setStatusTip("Open %s console." % (schema["title"]))
            tMenu.addAction(sItem)

    def reload(self, title, cfg):
        if title in self.modules:
            del self.modules[title]
            self.removeDockWidget(self.docks[title])
            del self.docks[title]
        schema = self.find_schema(title)
        schema["__init__"] = cfg
        self.onAction(title)

    def find(self, url):
        urls = url.split("/")
        title = urls[0]
        if title in self.modules:
            module = self.modules[title]
        else:
            return None
        cfg = module.toJSON(bFind=True)
        for u in urls[1:]:
            if ":" in u:
                title, field = u.split(":")
                return [x[field] for x in cfg[title]]
            else:
                if type(cfg) == list:
                    c = None
                    for x in cfg:
                        if x["name"] == u:
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
            jsonFile, _ = QFileDialog.getOpenFileName(
                None, "Open JSON", "jse.json", "*.json", "*.json", QFileDialog.DontResolveSymlinks
            )
            if jsonFile == "":
                return
        if os.path.exists(jsonFile) == False:
            return

        with open(jsonFile) as f:
            cfg = json.load(f)
            for m in cfg:
                for schema in self.schema:
                    if m["class"] == schema["title"]:
                        schema["__init__"] = m
        self.jsonFile = jsonFile
        self.modules.clear()
        for _, dock in self.docks.items():
            self.removeDockWidget(dock)
        self.docks.clear()
        for schema in self.schema:
            if "__init__" in schema:
                self.onAction(schema["title"])

    def UpdateEcuCByCanIf(self, cfg):
        ecuc = self.find("EcuC")
        if ecuc == None:
            ecuc = {"class": "EcuC", "Pdus": []}
        pdusMap = [x["name"] for x in ecuc["Pdus"]]
        for network in cfg["networks"]:
            for pdu in network.get("RxPdus", []) + network.get("TxPdus", []):
                if pdu["name"] not in pdusMap:
                    pdusMap.append(pdu["name"])
                    ecuc["Pdus"].append({"name": pdu["name"], "size": pdu.get("dlc", 8) * 8})
        self.reload(ecuc["class"], ecuc)

    def UpdateEcuCByPduR(self, cfg):
        ecuc = self.find("EcuC")
        if ecuc == None:
            ecuc = {"class": "EcuC", "Pdus": []}
        pdusMap = [x["name"] for x in ecuc["Pdus"]]
        for rt in cfg["routines"]:
            if rt["name"] not in pdusMap:
                pdusMap.append(rt["name"])
                ecuc["Pdus"].append({"name": rt["name"], "size": 64})
        self.reload(ecuc["class"], ecuc)

    def mLoad(self, jsonFile=None):
        if type(jsonFile) is not str:
            jsonFile, _ = QFileDialog.getOpenFileName(
                None, "Load JSON", "jse.json", "*.json", "*.json", QFileDialog.DontResolveSymlinks
            )
        if jsonFile == "":
            return
        if os.path.exists(jsonFile) == False:
            return

        with open(jsonFile) as f:
            cfg = json.load(f)
            if type(cfg) == dict:
                self.cfgMap[cfg["class"]] = jsonFile
                cfg = [cfg]
            for m in cfg:
                for schema in self.schema:
                    if m["class"] == schema["title"]:
                        self.reload(m["class"], m)
                if m["class"] == "CanIf":
                    self.UpdateEcuCByCanIf(m)
                if m["class"] == "PduR":
                    self.UpdateEcuCByPduR(m)
            self.jsonFile = jsonFile

    def mLoadDir(self):
        jsDir = QFileDialog.getExistingDirectory(None, "Load JSON Directory", "")
        if jsDir == "":
            return
        self.jsDir = jsDir
        if os.path.isdir(jsDir) == False:
            return

        for jsonFile in glob.glob("%s/*.json" % (jsDir)):
            self.mLoad(jsonFile)

    def toJSON(self):
        cfgs = []
        for title, module in self.modules.items():
            cfgs.append(module.toJSON())
        return cfgs

    def mSave(self):
        bAllHasF = all(title in self.cfgMap for title, module in self.modules.items())
        if not bAllHasF:
            jsonFile = self.jsonFile
            if jsonFile == "" or jsonFile == None:
                jsonFile, _ = QFileDialog.getSaveFileName(
                    None, "Save JSON", "jse.json", "*.json", "*.json", QFileDialog.DontResolveSymlinks
                )
            if jsonFile == "":
                return
            self.jsonFile = jsonFile
            self.setWindowTitle("JSON Editor < %s >" % (jsonFile))
            cfgs = self.toJSON()
            with open(jsonFile, "w") as f:
                json.dump(cfgs, f, indent=2)
        else:
            jsonFile = ""
        for title, module in self.modules.items():
            if title in self.cfgMap:
                cfg = module.toJSON()
                jsonFile += " " + self.cfgMap[title]
                with open(self.cfgMap[title], "w") as f:
                    json.dump(cfg, f, indent=2)
        QMessageBox(
            QMessageBox.Information, "Info", "Save JSON Configuration < %s > Successfully !" % (jsonFile)
        ).exec_()

    def mAIValidate(self):
        if not self.modules:
            QMessageBox(
                QMessageBox.Warning,
                "Warning",
                "No configuration modules loaded. Please open or load a JSON configuration file first.",
            ).exec_()
            return

        ai_config_path = os.path.join(os.path.dirname(__file__), ".ai.config.json")
        if not os.path.exists(ai_config_path):
            QMessageBox(
                QMessageBox.Warning,
                "Warning",
                f"AI configuration file not found: {ai_config_path}\n\nPlease create a .ai.config.json file with your API key.",
            ).exec_()
            return

        try:
            from openai_agent import OpenAIAgent

            self.statusBar.showMessage(" AI validation in progress...", 0)
            QApplication.processEvents()

            agent = OpenAIAgent(self.schemaFile)
            cfgs = self.toJSON()
            agent.load_config(cfgs)

            result = agent.chat("help validate config")

            self.statusBar.showMessage(" AI validation completed", 5000)

            self.ai_validation_dialog = AIValidationDialog(self, agent=agent)
            self.ai_validation_dialog.fix_applied.connect(self.on_ai_fix_applied)
            self.ai_validation_dialog.set_results(result)
            self.ai_validation_dialog.show()

        except Exception as e:
            self.statusBar.showMessage(" AI validation failed", 5000)
            QMessageBox(QMessageBox.Critical, "Error", f"AI validation failed: {str(e)}").exec_()

    def on_ai_fix_applied(self, fixed_config):
        try:
            for cfg in fixed_config:
                module_class = cfg.get("class")
                if module_class:
                    self.reload(module_class, cfg)

            self.statusBar.showMessage(" Configuration updated by AI", 5000)
            msg_box = QMessageBox(self)
            msg_box.setIcon(QMessageBox.Information)
            msg_box.setWindowTitle("Success")
            msg_box.setText("Configuration has been updated successfully!")
            msg_box.exec_()
        except Exception as e:
            self.statusBar.showMessage(" Failed to apply fix", 5000)
            msg_box = QMessageBox(self)
            msg_box.setIcon(QMessageBox.Critical)
            msg_box.setWindowTitle("Error")
            msg_box.setText(f"Failed to apply fix: {str(e)}")
            msg_box.exec_()

    def mGen(self):
        if os.path.exists(self.jsonFile) == False and self.jsDir == None:
            return
        from generator import Generate

        if self.jsDir != None:
            gdir = self.jsDir
        else:
            gdir = os.path.dirname(self.jsonFile)
            gdir = os.path.abspath(gdir) + "/config"
        os.makedirs(gdir, exist_ok=True)
        cfgs = []
        for title, module in self.modules.items():
            cfg = module.toJSON()
            jsonFile = "%s/%s.json" % (gdir, title)
            with open(jsonFile, "w") as f:
                json.dump(cfg, f, indent=2)
            cfgs.append(jsonFile)
        Generate(cfgs, True)
        QMessageBox(
            QMessageBox.Information, "Info", "Generate ssas Configuration C Code Successfully !\n<%s>\n" % (gdir)
        ).exec_()

    def find_schema(self, title):
        for schema in self.schema:
            if schema["title"] == title:
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
            print("%s already started." % (title))
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
        self.statusBar.showMessage(" Json Editor for automotive", 0)


def main():
    import argparse

    parser = argparse.ArgumentParser(description="json editer")
    parser.add_argument("-s", "--schema", type=str, default="%s/schema.json" % (PKGDIR), help="input json schema file")
    parser.add_argument("-i", "--input", type=str, default=None, help="input json configuration file")
    args = parser.parse_args()

    qtApp = QApplication(sys.argv)
    if os.name == "nt":
        qtApp.setFont(QFont("Consolas"))
    elif os.name == "posix":
        qtApp.setFont(QFont("Monospace"))
    else:
        print("unKnown platform.")
    qtGui = JsonEditor(args)
    qtGui.show()
    qtApp.exec_()


if __name__ == "__main__":
    main()
