# SSAS - Simple Smart Automotive Software
# Copyright (C) 2015 ~ 2023 Parai Wang <parai@foxmail.com>
import sys
import os
import json
import glob
import importlib.util

from PyQt5 import QtCore, QtGui
from PyQt5.QtCore import *
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *

from json_editor import *

PKGDIR = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.path.abspath("%s/.." % (PKGDIR)))


class QACollapseWidget(QWidget):
    def __init__(self, question, answer, parent=None):
        super(QACollapseWidget, self).__init__(parent)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        self.header = QWidget()
        self.header.setStyleSheet("background-color: #e8f4fc; padding: 8px; border-bottom: 1px solid #b3d9e8;")
        headerLayout = QHBoxLayout(self.header)
        headerLayout.setContentsMargins(0, 0, 0, 0)

        self.expandIcon = QLabel("-")
        self.expandIcon.setFont(QFont("Arial", 10))
        headerLayout.addWidget(self.expandIcon)

        self.questionLabel = QLabel(f"Q: {question[:50]}..." if len(question) > 50 else f"Q: {question}")
        self.questionLabel.setFont(QFont("Arial", 10, QFont.Bold))
        self.questionLabel.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        self.questionLabel.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
        headerLayout.addWidget(self.questionLabel)

        layout.addWidget(self.header)

        self.content = QWidget()
        self.content.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        contentLayout = QVBoxLayout(self.content)
        contentLayout.setContentsMargins(0, 0, 0, 0)
        contentLayout.setSpacing(0)

        self.answerText = QTextEdit()
        self.answerText.setReadOnly(True)
        self.answerText.setFont(QFont("Consolas", 10))
        self.answerText.setText(answer)
        self.answerText.setStyleSheet("background-color: #fafafa; border: none;")
        self.answerText.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.answerText.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        self.answerText.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        contentLayout.addWidget(self.answerText)

        layout.addWidget(self.content)

        self.header.mousePressEvent = self.toggleExpand
        self.expanded = True

    def toggleExpand(self, event):
        self.expanded = not self.expanded
        self.content.setVisible(self.expanded)
        self.expandIcon.setText("- " if self.expanded else "+ ")


class IssueWidget(QWidget):
    fixRequested = QtCore.pyqtSignal(int)
    showInEditor = QtCore.pyqtSignal(int)

    SEVERITY_STYLES = {
        "ERROR": {"bg": "#fef2f2", "border": "#fecaca", "badge_bg": "#dc2626"},
        "WARNING": {"bg": "#fffbeb", "border": "#fde68a", "badge_bg": "#d97706"},
        "INFO": {"bg": "#eff6ff", "border": "#bfdbfe", "badge_bg": "#2563eb"},
    }

    def __init__(self, issueIndex, issueData, parent=None):
        super(IssueWidget, self).__init__(parent)
        self.issueIndex = issueIndex
        self.issueData = issueData
        self.applied = False

        severity = issueData.get("severity", "INFO")
        s = self.SEVERITY_STYLES.get(severity, self.SEVERITY_STYLES["INFO"])

        self.setStyleSheet(
            "IssueWidget { background-color: %s; border: 1px solid %s; border-radius: 6px; }" % (s["bg"], s["border"])
        )

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 12, 10)
        layout.setSpacing(6)

        headerLayout = QHBoxLayout()
        headerLayout.setSpacing(8)

        badge = QLabel(severity)
        badge.setStyleSheet(
            "background-color: %s; color: white; padding: 2px 8px; border-radius: 3px; font-size: 14px; font-weight: bold;"
            % s["badge_bg"]
        )
        badge.setFixedHeight(24)
        headerLayout.addWidget(badge)

        module = issueData.get("module", "")
        issueId = issueData.get("id", "")
        titleText = "<b>%s</b>" % module
        if issueId:
            titleText += " <span style='color:#6b7280;'>(%s)</span>" % issueId
        titleLabel = QLabel(titleText)
        titleLabel.setTextFormat(QtCore.Qt.RichText)
        titleLabel.setStyleSheet("font-size: 17px; color: #1f2937;")
        titleLabel.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        headerLayout.addWidget(titleLabel)

        self.showButton = QPushButton("Show in Editor")
        self.showButton.setStyleSheet(
            "QPushButton { background-color: #2196F3; color: white; border: none; padding: 6px 14px; "
            "border-radius: 4px; font-weight: bold; font-size: 14px; }"
            "QPushButton:hover { background-color: #1976D2; }"
        )
        self.showButton.setCursor(QtCore.Qt.PointingHandCursor)
        self.showButton.clicked.connect(self._onShowClicked)
        headerLayout.addWidget(self.showButton)

        self.fixButton = QPushButton("Apply Fix")
        self.fixButton.setStyleSheet(
            "QPushButton { background-color: #4CAF50; color: white; border: none; padding: 6px 18px; "
            "border-radius: 4px; font-weight: bold; font-size: 15px; }"
            "QPushButton:hover { background-color: #45a049; }"
            "QPushButton:disabled { background-color: #9ca3af; }"
        )
        self.fixButton.setCursor(QtCore.Qt.PointingHandCursor)
        self.fixButton.clicked.connect(self._onFixClicked)
        headerLayout.addWidget(self.fixButton)

        layout.addLayout(headerLayout)

        detailsHtml = self._buildDetailsHtml(issueData)
        if detailsHtml:
            detailsLabel = QLabel(detailsHtml)
            detailsLabel.setTextFormat(QtCore.Qt.RichText)
            detailsLabel.setWordWrap(True)
            detailsLabel.setStyleSheet("color: #4b5563; font-size: 15px;")
            detailsLabel.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignTop)
            detailsLabel.setTextInteractionFlags(QtCore.Qt.TextSelectableByMouse)
            layout.addWidget(detailsLabel)

    def _buildDetailsHtml(self, issueData):
        location = issueData.get("location", "")
        description = issueData.get("description", "")
        suggestion = issueData.get("suggestion", "")
        changes = issueData.get("changes", {})

        html = ""  # Initialize before conditional appends
        if location:
            html += "<b>Location:</b> %s<br>" % location
        if description:
            html += "<b>Description:</b> %s<br>" % description
        if suggestion:
            html += "<b>Suggestion:</b> %s<br>" % suggestion

        if changes:
            deletions = changes.get("deletions", [])
            sets = changes.get("sets", [])
            if deletions or sets:
                html += "<b>Proposed Changes:</b><br>"
                for path in deletions:
                    html += "&nbsp;&nbsp;- Delete: <code>%s</code><br>" % ".".join(map(str, path))
                for setOp in sets:
                    path = setOp.get("path", [])
                    value = setOp.get("value")
                    html += "&nbsp;&nbsp;- Set: <code>%s</code> -&gt; <code>%s</code><br>" % (
                        ".".join(map(str, path)),
                        json.dumps(value),
                    )

        return html

    def _onShowClicked(self):
        self.showInEditor.emit(self.issueIndex)

    def _onFixClicked(self):
        if not self.applied:
            self.fixRequested.emit(self.issueIndex)

    def markApplied(self):
        self.applied = True
        self.fixButton.setEnabled(False)
        self.fixButton.setText("Applied")
        self.fixButton.setStyleSheet(
            "QPushButton { background-color: #9ca3af; color: white; border: none; padding: 6px 18px; "
            "border-radius: 4px; font-weight: bold; font-size: 15px; }"
        )


class ValidationResultWidget(QWidget):
    def __init__(self, question, summaryHtml, issueWidgets, parent=None):
        super(ValidationResultWidget, self).__init__(parent)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        self.header = QWidget()
        self.header.setStyleSheet("background-color: #e8f4fc; padding: 8px; border-bottom: 1px solid #b3d9e8;")
        headerLayout = QHBoxLayout(self.header)
        headerLayout.setContentsMargins(8, 0, 8, 0)

        self.expandIcon = QLabel("-")
        self.expandIcon.setFont(QFont("Arial", 15))
        headerLayout.addWidget(self.expandIcon)

        shortQ = "Q: %s..." % question[:50] if len(question) > 50 else "Q: %s" % question
        self.questionLabel = QLabel(shortQ)
        self.questionLabel.setFont(QFont("Arial", 15, QFont.Bold))
        self.questionLabel.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        self.questionLabel.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
        headerLayout.addWidget(self.questionLabel)

        if issueWidgets:
            countLabel = QLabel("(%d fixable)" % len(issueWidgets))
            countLabel.setStyleSheet("color: #666; font-size: 14px;")
            headerLayout.addWidget(countLabel)

        layout.addWidget(self.header)

        self.content = QWidget()
        self.content.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        contentLayout = QVBoxLayout(self.content)
        contentLayout.setContentsMargins(10, 10, 10, 10)
        contentLayout.setSpacing(8)

        if summaryHtml:
            summary = QLabel(summaryHtml)
            summary.setTextFormat(QtCore.Qt.RichText)
            summary.setWordWrap(True)
            summary.setTextInteractionFlags(QtCore.Qt.TextSelectableByMouse)
            summary.setStyleSheet(
                "background-color: #fafafa; padding: 8px; border-radius: 4px; color: #333; font-size: 15px;"
            )
            summary.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignTop)
            contentLayout.addWidget(summary)

        for iw in issueWidgets:
            contentLayout.addWidget(iw)

        layout.addWidget(self.content)

        self.header.mousePressEvent = self.toggleExpand
        self.expanded = True

    def toggleExpand(self, event):
        self.expanded = not self.expanded
        self.content.setVisible(self.expanded)
        self.expandIcon.setText("- " if self.expanded else "+ ")


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
    def __init__(self, parent=None, agent=None, config=None):
        super(AIValidationDialog, self).__init__(parent)
        self.mainWindow = parent  # store explicitly for reliable access
        self.setWindowTitle("AI Validation Assistant")
        self.setMinimumSize(800, 600)
        self.resize(900, 700)
        self.setWindowFlags(QtCore.Qt.Window)

        self.agent = agent
        self.issuesWithChanges = []
        self.issueWidgets = {}
        self.config = config

        self._initUi()

    def _initUi(self):
        centralWidget = QWidget()
        self.setCentralWidget(centralWidget)
        layout = QVBoxLayout(centralWidget)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        self._initToolbar(layout)
        self._initScrollArea(layout)
        self._initInputBar(layout)

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

    def _initToolbar(self, parentLayout):
        self.toolbar = QWidget()
        self.toolbar.setStyleSheet("background-color: #4a90d9; padding: 8px;")
        toolbarLayout = QHBoxLayout(self.toolbar)

        modelLabel = QLabel("Model:")
        modelLabel.setStyleSheet("color: white; font-weight: bold;")
        toolbarLayout.addWidget(modelLabel)

        self.modelCombo = QComboBox()
        self.modelCombo.setStyleSheet("background-color: white; color: black; padding: 4px; border-radius: 4px;")
        models = self.agent.getAvailableModels()
        if models:
            for modelId, displayName in models:
                self.modelCombo.addItem(displayName, modelId)
                if modelId == self.agent.model:
                    self.modelCombo.setCurrentText(displayName)
        self.modelCombo.currentIndexChanged.connect(self.onModelChanged)
        toolbarLayout.addWidget(self.modelCombo)

        toolbarLayout.addStretch()
        parentLayout.addWidget(self.toolbar)

    def _initScrollArea(self, parentLayout):
        self.scrollArea = QScrollArea()
        self.scrollArea.setWidgetResizable(True)
        self.scrollContent = QWidget()
        self.scrollLayout = QVBoxLayout(self.scrollContent)
        self.scrollLayout.setContentsMargins(10, 10, 10, 10)
        self.scrollLayout.setSpacing(10)
        self.scrollArea.setWidget(self.scrollContent)
        parentLayout.addWidget(self.scrollArea)

        self.loadingWidget = QWidget()
        loadingLayout = QHBoxLayout(self.loadingWidget)
        self.loadingLabel = QLabel("AI validation in progress...")
        self.loadingLabel.setFont(QFont("Arial", 12))
        self.loadingLabel.setStyleSheet("color: #888; padding: 40px;")
        self.loadingLabel.setAlignment(QtCore.Qt.AlignCenter)
        loadingLayout.addWidget(self.loadingLabel)
        self.scrollLayout.addWidget(self.loadingWidget)
        self.loadingWidget.hide()

    def _initInputBar(self, parentLayout):
        self.inputBar = QWidget()
        self.inputBar.setStyleSheet("background-color: #f0f0f0; padding: 8px; border-top: 1px solid #ccc;")
        inputLayout = QHBoxLayout(self.inputBar)

        self.inputField = QLineEdit()
        self.inputField.setPlaceholderText("Ask AI for help...")
        self.inputField.returnPressed.connect(self.sendMessage)
        inputLayout.addWidget(self.inputField)

        self.sendButton = QPushButton("Send")
        self.sendButton.clicked.connect(self.sendMessage)
        inputLayout.addWidget(self.sendButton)

        inputLayout.addStretch()

        self.applyAllButton = QPushButton("Apply All")
        self.applyAllButton.setStyleSheet(
            "background-color: #2196F3; color: white; border: none; padding: 6px 12px; border-radius: 4px;"
        )
        self.applyAllButton.clicked.connect(self.applyAllFixes)
        self.applyAllButton.hide()
        inputLayout.addWidget(self.applyAllButton)

        parentLayout.addWidget(self.inputBar)

    def onModelChanged(self, index):
        if self.agent:
            modelId = self.modelCombo.itemData(index)
            if modelId:
                self.agent.setModel(modelId)

    def addQA(self, question, answer):
        qaWidget = QACollapseWidget(question, answer)
        qaWidget.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        self.scrollLayout.addWidget(qaWidget)

        QApplication.processEvents()
        scrollBar = self.scrollArea.verticalScrollBar()
        scrollBar.setValue(scrollBar.maximum())

    def sendMessage(self):
        question = self.inputField.text().strip()
        if not question:
            return

        self.inputField.clear()
        self.inputField.setEnabled(False)
        self.sendButton.setEnabled(False)

        config = self.mainWindow.toJSON()
        
        if str(config) != str(self.config):
            question = f"{question}\nThe New SSAS Configuration:\n```json\n{config}````\n"
            self.config = config
        else:
            print("Configuration has not changed, skip config reload")

        self.worker = AIWorker(self.agent, question)
        self.worker.finished.connect(self.onWorkerFinished)
        self.worker.error.connect(self.onWorkerError)
        self.worker.start()

    def _parseAndDisplayResponse(self, question: str, rawText: str) -> None:
        try:
            cleanText = self._cleanJsonResponse(rawText)
            response = json.loads(cleanText)

            success = response.get("success", False)
            message = response.get("message", "")
            explanation = response.get("explanation", "")
            issues = response.get("issues", [])

            statusColor = "#16a34a" if success else "#dc2626"
            summaryHtml = "<h3 style='color:%s; margin:0;'>Status: %s</h3>" % (
                statusColor,
                "Success" if success else "Failed",
            )
            if message:
                summaryHtml += "<p><b>Message:</b> %s</p>" % message

            if explanation:
                explanation = explanation.replace("\\n", "\n").replace("\\t", "\t")
                summaryHtml += "<h4 style='margin:8px 0 2px 0;'>Explanation</h4><p>%s</p>" % (
                    explanation.replace("\n", "<br>")
                )

            self.issuesWithChanges = []
            self.issueWidgets = {}

            infoOnlyHtml = ""
            if issues:
                summaryHtml += "<h4 style='margin:8px 0 2px 0;'>Issues Found</h4>"
                for i, issue in enumerate(issues, 1):
                    severity = issue.get("severity", "INFO")
                    module = issue.get("module", "")
                    issueId = issue.get("id", "")
                    description = issue.get("description", "")
                    location = issue.get("location", "")
                    suggestion = issue.get("suggestion", "")
                    changes = issue.get("changes", {})

                    hasChanges = changes and (changes.get("deletions") or changes.get("sets"))

                    if not hasChanges:
                        infoOnlyHtml += "<b>Issue %d:</b> [%s] %s %s<br>" % (i, severity, module, issueId)
                        if location:
                            infoOnlyHtml += "&nbsp;&nbsp;- <b>Location:</b> %s<br>" % location
                        if description:
                            infoOnlyHtml += "&nbsp;&nbsp;- <b>Description:</b> %s<br>" % description
                        if suggestion:
                            infoOnlyHtml += "&nbsp;&nbsp;- <b>Suggestion:</b> %s<br>" % suggestion
                        infoOnlyHtml += "<br>"
                    else:
                        summaryHtml += (
                            "<p style='margin:2px 0;'><b>Issue %d:</b> [%s] %s %s <i style='color:#6b7280;'>(fixable below)</i></p>"
                            % (i, severity, module, issueId)
                        )
                        self.issuesWithChanges.append(issue)

                if infoOnlyHtml:
                    summaryHtml += "<h4 style='margin:8px 0 2px 0;'>Info-only Issues</h4><p>%s</p>" % infoOnlyHtml

            issueWidgets = []
            for idx, issue in enumerate(self.issuesWithChanges):
                iw = IssueWidget(idx, issue)
                iw.fixRequested.connect(self.applyFixInEditor)
                iw.showInEditor.connect(self.showIssueInEditor)
                self.issueWidgets[idx] = iw
                issueWidgets.append(iw)

            self._annotateIssuesInEditor()

            resultWidget = ValidationResultWidget(question, summaryHtml, issueWidgets)
            resultWidget.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
            self.scrollLayout.addWidget(resultWidget)

            QApplication.processEvents()
            scrollBar = self.scrollArea.verticalScrollBar()
            scrollBar.setValue(scrollBar.maximum())

            self.updateApplyAllButton()
        except json.JSONDecodeError as e:
            cleanedText = rawText.replace("\\n", "\n").replace("\\t", "\t")
            fullText = "JSON Parse Error: %s\n\nRaw Response:\n\n%s" % (str(e), cleanedText)
            self.addQA(question, fullText)
            self.issuesWithChanges = []
            self.issueWidgets = {}
            self.updateApplyAllButton()

    def onWorkerFinished(self, question, answer):
        self._parseAndDisplayResponse(question, answer)
        self.inputField.setEnabled(True)
        self.sendButton.setEnabled(True)

    def onWorkerError(self, question, error):
        self.addQA(question, f"Error: {error}")
        self.inputField.setEnabled(True)
        self.sendButton.setEnabled(True)

    def _cleanJsonResponse(self, text):
        text = text.strip()
        if text.startswith("```json"):
            text = text[7:]
        elif text.startswith("```"):
            text = text[3:]
        if text.endswith("```"):
            text = text[:-3]
        text = text.strip()

        firstBrace = text.find("{")
        lastBrace = text.rfind("}")
        if firstBrace != -1 and lastBrace != -1 and lastBrace > firstBrace:
            text = text[firstBrace : lastBrace + 1]

        return text.strip()

    def showLoading(self):
        self.loadingWidget.show()
        self.scrollArea.hide()
        QApplication.processEvents()

    def hideLoading(self):
        self.loadingWidget.hide()
        self.scrollArea.show()
        QApplication.processEvents()

    def setError(self, question, errorMsg):
        self.hideLoading()
        self.addQA(question, f"Error: {errorMsg}")

    def setResults(self, question, text):
        self.hideLoading()
        self._parseAndDisplayResponse(question, text)

    def setAgent(self, agent):
        self.agent = agent

    def updateApplyAllButton(self):
        if not self.issuesWithChanges:
            self.applyAllButton.hide()
            return

        remaining = 0
        for idx in range(len(self.issuesWithChanges)):
            iw = self.issueWidgets.get(idx)
            if iw is None or not iw.applied:
                remaining += 1

        if remaining > 0:
            self.applyAllButton.show()
            self.applyAllButton.setEnabled(True)
            self.applyAllButton.setText("Apply All (%d remaining)" % remaining)
        else:
            self.applyAllButton.hide()

    def applyAllFixes(self):
        if not self.agent or not self.issuesWithChanges:
            return

        mainWindow = self.mainWindow
        config = self.config
        if config is None and hasattr(mainWindow, "toJSON"):
            config = mainWindow.toJSON()

        if config is None:
            return

        for idx, issue in enumerate(self.issuesWithChanges):
            iw = self.issueWidgets.get(idx)
            if iw is not None and iw.applied:
                continue

            for deletion in issue.get("changes", {}).get("deletions", []):
                self._applyDeletion(config, deletion)

            for setOp in issue.get("changes", {}).get("sets", []):
                self._applySet(config, setOp)

            if iw is not None:
                iw.markApplied()

        if hasattr(mainWindow, "reload"):
            for cfg in config:
                module_class = cfg.get("class")
                if module_class:
                    mainWindow.reload(module_class, cfg)

        self.updateApplyAllButton()
        # Show the first issue location in editor after applying all fixes
        if self.issuesWithChanges:
            QTimer.singleShot(100, lambda: self.showIssueInEditor(0))

    def applyFixByIndex(self, index):
        if not self.agent or index >= len(self.issuesWithChanges):
            return

        iw = self.issueWidgets.get(index)
        if iw is None or iw.applied:
            return

        mainWindow = self.mainWindow
        config = self.config
        if config is None and hasattr(mainWindow, "toJSON"):
            config = mainWindow.toJSON()

        if config is None:
            return

        issue = self.issuesWithChanges[index]

        for deletion in issue.get("changes", {}).get("deletions", []):
            self._applyDeletion(config, deletion)

        for setOp in issue.get("changes", {}).get("sets", []):
            self._applySet(config, setOp)

        if hasattr(mainWindow, "reload"):
            for cfg in config:
                module_class = cfg.get("class")
                if module_class:
                    mainWindow.reload(module_class, cfg)

        iw.markApplied()
        self.updateApplyAllButton()
        # Show the issue location in editor after reload
        QTimer.singleShot(100, lambda: self.showIssueInEditor(index))

    @staticmethod
    def _isLeafFix(setOp: dict) -> bool:
        """A leaf fix changes a single value, not an entire module or object."""
        path = setOp.get("path", [])
        value = setOp.get("value")
        return len(path) >= 2 and isinstance(value, (str, int, float, bool))

    def applyFixInEditor(self, index):
        """Apply fix by updating the widget tree directly, NOT via config reload."""
        if index >= len(self.issuesWithChanges):
            return

        iw = self.issueWidgets.get(index)
        if iw and iw.applied:
            return

        issue = self.issuesWithChanges[index]
        moduleName = issue.get("module", "")

        mainWindow = self.mainWindow
        if not hasattr(mainWindow, "modules"):
            logging.warning(
                "applyFixInEditor: mainWindow has no 'modules' attribute (type=%s)", type(mainWindow).__name__
            )
            return

        jsonModule = mainWindow.modules.get(moduleName)
        if not jsonModule or not hasattr(jsonModule, "objTree"):
            logging.warning("applyFixInEditor: module '%s' not found in editor modules", moduleName)
            return

        changes = issue.get("changes", {})
        needsReload = False

        for setOp in changes.get("sets", []):
            if not self._isLeafFix(setOp):
                needsReload = True
                break

            path = setOp.get("path", [])
            value = setOp.get("value")

            try:
                node, fieldName = JsonBase.resolvePath(jsonModule.objTree, path)
            except Exception as e:
                logging.error("applyFixInEditor: resolvePath failed for %s: %s", path, e)
                needsReload = True
                break

            if not node:
                needsReload = True
                break

            if fieldName:
                if hasattr(node, "applyFieldFix"):
                    node.applyFieldFix(fieldName, value)
            else:
                if hasattr(node, "applySetFix"):
                    node.applySetFix(path, value)

        if not needsReload:
            for deletion in changes.get("deletions", []):
                needsReload = True  # structural change -> reload
                break

        if needsReload:
            # Fall back to old reload approach for structural changes
            self.applyFixByIndex(index)
            # After reload, show the issue location in editor
            QTimer.singleShot(100, lambda: self.showIssueInEditor(index))
            return

        if iw:
            iw.markApplied()
        self.updateApplyAllButton()
        # Show the issue location in editor after successful fix
        self.showIssueInEditor(index)

    def _annotateIssuesInEditor(self):
        mainWindow = self.mainWindow
        if not hasattr(mainWindow, "modules"):
            logging.warning("_annotateIssuesInEditor: mainWindow has no modules")
            return

        for issue in self.issuesWithChanges:
            moduleName = issue.get("module", "")
            jsonModule = mainWindow.modules.get(moduleName)
            if not jsonModule or not hasattr(jsonModule, "objTree"):
                logging.warning("_annotateIssuesInEditor: module '%s' not found", moduleName)
                continue

            changes = issue.get("changes", {})
            allPaths = []
            for setOp in changes.get("sets", []):
                path = setOp.get("path", [])
                if path:
                    allPaths.append(path)
            for deletion in changes.get("deletions", []):
                if deletion:
                    allPaths.append(deletion)

            for path in allPaths:
                try:
                    node, fieldName = JsonBase.resolvePath(jsonModule.objTree, path)
                    if node:
                        if fieldName:
                            node.annotateField(fieldName, issue)
                        else:
                            node.annotateIssue(issue)
                    else:
                        logging.warning("_annotateIssuesInEditor: could not resolve path %s", path)
                except Exception as e:
                    logging.error("_annotateIssuesInEditor: error resolving path %s: %s", path, e)

    def showIssueInEditor(self, index):
        if index >= len(self.issuesWithChanges):
            logging.warning("showIssueInEditor: index %d out of range", index)
            return

        issue = self.issuesWithChanges[index]
        moduleName = issue.get("module", "")

        mainWindow = self.mainWindow
        if not hasattr(mainWindow, "modules"):
            logging.warning("showIssueInEditor: mainWindow has no modules")
            return

        jsonModule = mainWindow.modules.get(moduleName)
        if not jsonModule or not hasattr(jsonModule, "objTree"):
            logging.warning("showIssueInEditor: module '%s' not found", moduleName)
            return

        changes = issue.get("changes", {})
        firstPath = None
        for setOp in changes.get("sets", []):
            firstPath = setOp.get("path", [])
            if firstPath:
                break
        if not firstPath:
            for deletion in changes.get("deletions", []):
                firstPath = deletion
                if firstPath:
                    break

        if not firstPath:
            logging.warning("showIssueInEditor: no path found in issue %d", index)
            return

        logging.debug("showIssueInEditor: resolving path %s", firstPath)
        try:
            node, fieldName = JsonBase.resolvePath(jsonModule.objTree, firstPath, partial_ok=True)
        except Exception as e:
            logging.error("showIssueInEditor: resolvePath failed for %s: %s", firstPath, e)
            return

        if node:
            logging.debug("showIssueInEditor: found node '%s' (type=%s)", node.title, type(node).__name__)

            # Expand parent chain so the item is visible in the tree
            p = node.parent()
            while p:
                if hasattr(p, "setExpanded"):
                    p.setExpanded(True)
                p = p.parent()

            # Raise the dock widget to bring it to front
            dock = mainWindow.docks.get(moduleName)
            if dock:
                dock.show()
                dock.raise_()

            jsonModule.objTree.setCurrentItem(node)
            QApplication.processEvents()
            node.onItemSelectionChanged()
        else:
            logging.warning("showIssueInEditor: could not resolve path %s in module %s", firstPath, moduleName)

    def _applyDeletion(self, config, path):
        try:
            if not path:
                return

            moduleName = path[0]
            module = next((m for m in config if m.get("class") == moduleName), None)
            if not module:
                return

            obj = module
            for key in path[1:-1]:
                if isinstance(obj, list) and isinstance(key, int):
                    if key < len(obj):
                        obj = obj[key]
                    else:
                        return
                elif isinstance(obj, dict) and key in obj:
                    obj = obj[key]
                else:
                    return

            last_key = path[-1]
            if isinstance(obj, list) and isinstance(last_key, int):
                if last_key < len(obj):
                    del obj[last_key]
            elif isinstance(obj, dict) and last_key in obj:
                del obj[last_key]
        except Exception:
            pass

    def _applySet(self, config, setOp):
        try:
            path = setOp.get("path", [])
            value = setOp.get("value")

            if not path or value is None:
                return

            moduleName = path[0]

            if len(path) == 1:
                existing_module = next((m for m in config if m.get("class") == moduleName), None)
                if existing_module:
                    existing_module.update(value)
                else:
                    if isinstance(value, dict) and "class" in value:
                        config.append(value)
            else:
                module = next((m for m in config if m.get("class") == moduleName), None)
                if not module:
                    return

                obj = module
                for key in path[1:-1]:
                    if isinstance(obj, list) and isinstance(key, int):
                        if key < len(obj):
                            obj = obj[key]
                        else:
                            return
                    elif isinstance(obj, dict) and key in obj:
                        obj = obj[key]
                    else:
                        return

                last_key = path[-1]
                if isinstance(obj, list) and isinstance(last_key, int):
                    if last_key < len(obj):
                        obj[last_key] = value
                elif isinstance(obj, dict):
                    obj[last_key] = value
        except Exception:
            pass


class JsonDockWidget(QDockWidget):
    isClosed = False

    def __init__(self, title, parent=None):
        QDockWidget.__init__(self, title, parent)
        self.setAllowedAreas(QtCore.Qt.LeftDockWidgetArea | QtCore.Qt.RightDockWidgetArea)

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
        self.loadSchema()
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

    def loadSchema(self):
        with open(self.schemaFile) as f:
            self.schema = json.load(f)
        if type(self.schema) != list:
            self.schema = [self.schema]

    def creMenu(self):
        tMenu = self.menuBar().addMenu(self.tr("File"))

        sItem = QAction(self.tr("Open"), self)
        sItem.setShortcut("Ctrl+O")
        sItem.setStatusTip("Open a JSON configure file.")
        sItem.triggered.connect(self.mOpen)
        tMenu.addAction(sItem)

        sItem = QAction(self.tr("Load"), self)
        sItem.setShortcut("Ctrl+L")
        sItem.setStatusTip("Load a JSON module configure file.")
        sItem.triggered.connect(self.mLoad)
        tMenu.addAction(sItem)

        sItem = QAction(self.tr("Load Directory"), self)
        sItem.setShortcut("Ctrl+D")
        sItem.setStatusTip("Load all the JSON configure file under the directory.")
        sItem.triggered.connect(self.mLoadDir)
        tMenu.addAction(sItem)

        sItem = QAction(self.tr("Save"), self)
        sItem.setShortcut("Ctrl+S")
        sItem.setStatusTip("Save the JSON configure file.")
        sItem.triggered.connect(self.mSave)
        tMenu.addAction(sItem)

        sItem = QAction(self.tr("AI Validate"), self)
        sItem.setShortcut("Ctrl+V")
        sItem.setStatusTip("Validate configuration using AI.")
        sItem.triggered.connect(self.mAIValidate)
        tMenu.addAction(sItem)

        sItem = QAction(self.tr("Generate"), self)
        sItem.setShortcut("Ctrl+G")
        sItem.setStatusTip("Convert the JSON configure file to C Code.")
        sItem.triggered.connect(self.mGen)
        tMenu.addAction(sItem)

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
        schema = self.findSchema(title)
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
        pDir = None
        for title, module in self.modules.items():
            if title in self.cfgMap:
                cfg = module.toJSON()
                jsonFile += " " + self.cfgMap[title]
                pDir = os.path.dirname(self.cfgMap[title])
                with open(self.cfgMap[title], "w") as f:
                    json.dump(cfg, f, indent=2)
        with open(os.path.join(pDir, "..", "jse.json"), "w") as f:
            json.dump(cfgs, f, indent=2)
        jsonFile += " " + os.path.join(pDir, "..", "jse.json")
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

        try:
            from spark import SparkAgent

            self.statusBar.showMessage(" AI validation in progress...", 0)
            QApplication.processEvents()

            agent = SparkAgent()
            cfgs = self.toJSON()

            self.aiValidationDialog = AIValidationDialog(self, agent=agent, config=cfgs)
            self.aiValidationDialog.showLoading()
            self.aiValidationDialog.show()

            self.aiWorker = AIWorker(
                agent, f"help validate below SSAS configuration:\n```json\n{json.dumps(cfgs, indent=2, ensure_ascii=False)}\n```"
            )
            self.aiWorker.finished.connect(self.aiValidationDialog.setResults)
            self.aiWorker.error.connect(self.aiValidationDialog.setError)
            self.aiWorker.finished.connect(lambda: self.statusBar.showMessage(" AI validation completed", 5000))
            self.aiWorker.error.connect(lambda: self.statusBar.showMessage(" AI validation failed", 5000))
            self.aiWorker.start()

        except Exception as e:
            self.statusBar.showMessage(" AI validation failed", 5000)
            QMessageBox(QMessageBox.Critical, "Error", f"AI validation failed: {str(e)}").exec_()

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

    def findSchema(self, title):
        for schema in self.schema:
            if schema["title"] == title:
                return schema
        raise

    def onAction(self, title):
        if title not in self.modules:
            module = JsonModule(self.findSchema(title), self)
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
