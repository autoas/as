"""
SPARK Tools - Unified tool implementations.
"""

import os
from typing import Dict, Any


class SparkTools:
    @classmethod
    def read(cls, file_path: str) -> Dict[str, Any]:
        try:
            file_path = os.path.normpath(os.path.abspath(file_path))
            if not os.path.exists(file_path):
                return {"success": False, "error": f"File not found: {file_path}"}

            with open(file_path, "r", encoding="utf-8", errors="replace") as f:
                content = f.read()

            skill_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "prompts", "skill")
            if "{{CWD}}" in content and os.path.dirname(file_path) == skill_dir:
                cwd = os.path.normpath(os.path.dirname(os.path.abspath(__file__)))
                content = content.replace("{{CWD}}", cwd)

            return {
                "success": True,
                "message": f"Successfully read file: {file_path}",
                "content": content,
                "length": len(content),
            }

        except Exception as e:
            return {"success": False, "error": str(e)}

    @classmethod
    def executeTool(cls, tool_name: str, parameters: Dict) -> Dict[str, Any]:
        if tool_name == "read":
            return cls.read(parameters.get("file_path", ""))
        else:
            return {"success": False, "error": f"Unknown tool: {tool_name}"}
