"""
SPARK - Smart Platform for Automotive Rapid Kit
Prompt Manager for loading and composing system prompts.
"""

import os
import json
from typing import Dict, List


class PromptManager:
    def __init__(self, promptsDir: str = None):
        self.promptsDir = promptsDir or os.path.join(os.path.dirname(__file__), "prompts")
        self.cwd = os.path.dirname(__file__)
        self.systemPrompt = self._loadSystemPrompt()

    def _loadSystemPrompt(self) -> str:
        path = os.path.join(self.promptsDir, "system.md")
        if os.path.exists(path):
            with open(path, "r", encoding="utf-8") as f:
                content = f.read()
                content = content.replace("{{CWD}}", self.cwd)
                return content
        raise FileNotFoundError(f"System prompt not found: {path}")

    @property
    def skillsDir(self) -> str:
        return os.path.join(self.promptsDir, "skill")

    def isSkillPrompt(self, file_path: str) -> bool:
        """Check if a file path points to a skill prompt file."""
        if not file_path:
            return False
        abs_path = os.path.normpath(os.path.abspath(file_path))
        skill_dir = os.path.normpath(self.skillsDir)
        return os.path.dirname(abs_path) == skill_dir

    def getSystemPrompt(self) -> str:
        return self.systemPrompt
