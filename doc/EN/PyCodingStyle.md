---
layout: post
title: Python Coding Style Guide
category: AUTOSAR
comments: true
---

# AS Python Coding Style Guide

## 1. Document Purpose

This document defines the coding style for Python code in the AS project. The style is derived from analyzing existing codebase patterns and best practices for PyQt5 development and general Python programming.

## 2. Naming Conventions

### 2.1 Classes

- Use **PascalCase** (CamelCase with uppercase first letter)
- Examples:
  ```python
  class JsonEditor(QMainWindow):  # Good
  class DataProcessor:  # Good
  class UIGroup(QScrollArea):  # Good
  class StringValidator:  # Good
  ```

### 2.2 Functions and Methods

- Use **camelCase** (lowercase first letter, uppercase subsequent words)
- Private methods start with single underscore `_` followed by camelCase
- Examples:
  ```python
  def isValid(self, value):  # Good
  def onTextChanged(self):  # Good
  def _updateTreeIcon(self):  # Good - private
  def _validateInput(self, data):  # Good - private
  ```

### 2.3 Variables

- Use **camelCase** (lowercase first letter, uppercase subsequent words)
- Private attributes start with single underscore `_` followed by camelCase
- Examples:
  ```python
  titleText = "<b>%s</b>" % module  # Good
  self._hasIssue = False  # Good - private attribute
  self._fieldAnnotations = {}  # Good
  userName = "admin"  # Good
  ```

### 2.4 Constants

- Use **ALL_UPPER_CASE** with underscores for word separation
- Place at module level before classes
- Examples:
  ```python
  BASIC_TYPES = ["string", "integer", "bool", "number"]  # Good
  C_CHAR_WIDTH = 14  # Good
  MAX_CONNECTIONS = 100  # Good
  THIS_IS_A_CONSTANT = 3  # Good
  ```

---

## 3. Code Layout

### 3.1 Indentation

- Use **4 spaces** for indentation
- Never use tabs
- Example:
  ```python
  def onTextChanged(self):
      text = str(self.text())
      self.obj.set(self.title, text)
      if text != vs:
          self.setText(vs)
  ```

### 3.2 Line Length

- Maximum **100 characters** per line
- Break long lines with parentheses
- Examples:
  ```python
  # Good - line break after opening parenthesis
  logging.error("invalid number: %s, value=%s, text=%s" % (e, value, text))
  
  # Good - multi-line with indentation
  QMessageBox(
      QMessageBox.Information,
      "Info",
      "invalid number: %s, value=%s, text=%s" % (e, value, text),
  ).exec_()
  ```

### 3.3 Blank Lines

- Two blank lines between classes and functions
- One blank line between methods within a class
- No blank line after function signature before first statement
- Example:
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

## 4. Imports

### 4.1 Order

1. Standard library imports
2. Third-party library imports (PyQt5, etc.)
3. Local module imports
4. Separate groups with blank lines
5. Use explicit imports over wildcard imports when possible
6. Example:
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

### 4.2 Wildcard Imports

- Acceptable for PyQt5 modules for brevity
- Example:
  ```python
  from PyQt5.QtWidgets import *  # Acceptable
  ```

---

## 5. Comments and Documentation

### 5.1 Comments

- Use `#` for single-line comments
- Comments should be in English
- Place comment above the code it describes
- Example:
  ```python
  # Update tree item icon based on issue severity
  def _updateTreeIcon(self):
      ...
  ```

### 5.2 Docstrings

- Use triple double quotes `"""`
- Provide brief description for public methods
- Include Args and Returns sections when appropriate
- Example:
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

## 6. Type Hints

### 6.1 Usage

- Use type hints for function parameters and return values
- Use `Optional`, `List`, `Dict` from typing module when needed
- Example:
  ```python
  def annotateIssue(self, issue: dict) -> None:
      ...
      
  def processData(data: list) -> dict:
      ...
  ```

---

## 7. Error Handling

### 7.1 Assertions

- Use `assert` for debugging and type checking
- Example:
  ```python
  assert isinstance(root, JsonModule)
  assert isinstance(obj, DataProcessor)
  ```

### 7.2 Exception Handling

- Use try/except for expected errors
- Log errors using `logging.error()`
- Example:
  ```python
  def onTextChanged(self, text):
      try:
          value = eval(text)
      except Exception as e:
          logging.error("invalid number: %s" % e)
          return
  ```

---

## 8. Tools

### 8.1 Linting

- Use `pylint` or `flake8` for static code analysis
- Check for:
  - Unused imports
  - Undefined variables
  - Code complexity

### 8.2 Formatting

- Use `autopep8` for automatic formatting
- Run `python -m py_compile` to check syntax errors

---

**Version**: 1.0  
**Date**: June 2024  
**Based on**: AS project codebase patterns