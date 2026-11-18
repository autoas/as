---
layout: post
title: Python 编码风格指南
category: AUTOSAR
comments: true
---

# AS Python 编码风格指南

## 1. 文档目的

本文档定义 AS 项目中 Python 代码的编码风格。风格源自对现有代码库模式与最佳实践的分析，涵盖 PyQt5 开发与一般 Python 编程。

## 2. 命名约定

### 2.1 类

- 使用 **PascalCase**（大驼峰，首字母大写）
- 示例：
  ```python
  class JsonEditor(QMainWindow):  # 正确
  class DataProcessor:  # 正确
  class UIGroup(QScrollArea):  # 正确
  class StringValidator:  # 正确
  ```

### 2.2 函数与方法

- 使用 **camelCase**（小驼峰，首字母小写）
- 私有方法以单个下划线 `_` 开头，后接 camelCase
- 示例：
  ```python
  def isValid(self, value):  # 正确
  def onTextChanged(self):  # 正确
  def _updateTreeIcon(self):  # 正确 - 私有
  def _validateInput(self, data):  # 正确 - 私有
  ```

### 2.3 变量

- 使用 **camelCase**（小驼峰）
- 私有属性以单个下划线 `_` 开头，后接 camelCase
- 示例：
  ```python
  titleText = "<b>%s</b>" % module  # 正确
  self._hasIssue = False  # 正确 - 私有属性
  self._fieldAnnotations = {}  # 正确
  userName = "admin"  # 正确
  ```

### 2.4 常量

- 使用 **全大写字母加下划线**（ALL_UPPER_CASE）
- 放在模块级、类定义之前
- 示例：
  ```python
  BASIC_TYPES = ["string", "integer", "bool", "number"]  # 正确
  C_CHAR_WIDTH = 14  # 正确
  MAX_CONNECTIONS = 100  # 正确
  THIS_IS_A_CONSTANT = 3  # 正确
  ```

---

## 3. 代码布局

### 3.1 缩进

- 使用 **4 个空格** 缩进
- 禁止使用 Tab
- 示例：
  ```python
  def onTextChanged(self):
      text = str(self.text())
      self.obj.set(self.title, text)
      if text != vs:
          self.setText(vs)
  ```

### 3.2 行长度

- 每行最多 **100 个字符**
- 长行用括号折行
- 示例：
  ```python
  # 正确 - 开括号后折行
  logging.error("invalid number: %s, value=%s, text=%s" % (e, value, text))

  # 正确 - 多行带缩进
  QMessageBox(
      QMessageBox.Information,
      "Info",
      "invalid number: %s, value=%s, text=%s" % (e, value, text),
  ).exec_()
  ```

### 3.3 空行

- 类与函数之间空 **两行**
- 类内方法之间空 **一行**
- 函数签名与第一条语句之间不留空行
- 示例：
  ```python
  class DataProcessor:
      def __init__(self, config):
          self.config = config
          ...

      def process(self, data):
          result = self._validateInput(data)
          ...


  class StringValidator:
      def validate(self, value):
          ...
  ```

---

## 4. 导入

### 4.1 顺序

1. 标准库导入
2. 第三方库导入（PyQt5 等）
3. 本地模块导入
4. 各组之间用空行分隔
5. 尽量使用显式导入而非通配符导入
6. 示例：
  ```python
  import logging
  import re
  import sys

  from PyQt5 import QtCore, QtGui
  from PyQt5.QtGui import *
  from PyQt5.QtCore import *
  from PyQt5.QtWidgets import *

  __all__ = ["DataProcessor", "StringValidator", "logging"]
  ```

### 4.2 通配符导入

- 对 PyQt5 模块为求简洁可以使用通配符导入
- 示例：
  ```python
  from PyQt5.QtWidgets import *  # 可以接受
  ```

---

## 5. 注释与文档

### 5.1 注释

- 单行注释使用 `#`
- 注释使用英文
- 注释放在所描述代码的上方
- 示例：
  ```python
  # Update tree item icon based on issue severity
  def _updateTreeIcon(self):
      ...
  ```

### 5.2 Docstring

- 使用三重双引号 `"""`
- 公开方法提供简要说明
- 需要时包含 Args 与 Returns 段
- 示例：
  ```python
  def resolvePath(rootObj, path: list):
      """Resolve path to a tree node.

      Args:
          rootObj: Root of the tree to search
          path: List of path segments

      Returns:
          Tuple of (node, fieldName) where fieldName is None for tree items
      """
      ...
  ```

---

## 6. 类型注解

### 6.1 用法

- 为函数参数与返回值添加类型注解
- 需要时使用 typing 模块的 `Optional`、`List`、`Dict`
- 示例：
  ```python
  def annotateIssue(self, issue: dict) -> None:
      ...

  def processData(data: list) -> dict:
      ...
  ```

---

## 7. 错误处理

### 7.1 断言

- 使用 `assert` 做调试与类型检查
- 示例：
  ```python
  assert isinstance(root, JsonModule)
  assert isinstance(obj, DataProcessor)
  ```

### 7.2 异常处理

- 对预期内的错误使用 try/except
- 用 `logging.error()` 记录错误
- 示例：
  ```python
  def onTextChanged(self, text):
      try:
          value = eval(text)
      except Exception as e:
          logging.error("invalid number: %s" % e)
          return
  ```

---

## 8. 工具

### 8.1 静态检查

- 使用 `pylint` 或 `flake8` 做静态分析
- 检查项：
  - 未使用的导入
  - 未定义变量
  - 代码复杂度

### 8.2 格式化

- 使用 `autopep8` 自动格式化
- 运行 `python -m py_compile` 检查语法错误

---

**版本**：1.0
**日期**：2024 年 6 月
**依据**：AS 项目代码库既有模式
