"""
SPARK - Smart Platform for Automotive Rapid Kit
Configuration management module.
"""

import json
import os
import logging

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
    handlers=[logging.StreamHandler()],
)
logger = logging.getLogger(__name__)


class SettingsManager:
    _instance = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._loaded = False
            cls._instance._settings = {}
            cls._instance._configPath = ""
        return cls._instance

    @classmethod
    def getInstance(cls):
        return cls()

    def _load(self):
        if self._loaded:
            return

        configPaths = [
            os.path.join(os.getcwd(), ".spark", "settings.json"),
            os.path.join(os.path.expanduser("~"), ".spark", "settings.json"),
        ]

        for configPath in configPaths:
            if os.path.exists(configPath):
                try:
                    with open(configPath, "r", encoding="utf-8") as f:
                        self._settings = json.load(f)
                    self._configPath = configPath
                    logger.info(f"[Settings] Loaded from: {configPath}")
                    self._loaded = True
                    return
                except json.JSONDecodeError as e:
                    logger.warning(f"[Settings] Failed to parse {configPath}: {e}")
                except Exception as e:
                    logger.warning(f"[Settings] Failed to read {configPath}: {e}")

        logger.info(f"[Settings] No config found in: {', '.join(configPaths)}")
        self._loaded = True

    def get(self):
        self._load()
        return self._settings

    def getLLM(self):
        self._load()
        return self._settings.get("llm", {})

    def getEnvironment(self):
        self._load()
        return self._settings.get("environment", {})

    def getConfigPath(self):
        self._load()
        return self._configPath

    def set(self, key: str, value):
        self._load()
        keys = key.split(".")
        current = self._settings

        for i in range(len(keys) - 1):
            if keys[i] not in current:
                current[keys[i]] = {}
            current = current[keys[i]]

        current[keys[-1]] = value
        self._save()

    def getKey(self, key: str):
        self._load()
        keys = key.split(".")
        current = self._settings

        for k in keys:
            if current is None:
                return None
            current = current.get(k)

        return current

    def _save(self):
        if not self._configPath:
            self._configPath = os.path.join(os.getcwd(), ".spark", "settings.json")
            configDir = os.path.dirname(self._configPath)
            if not os.path.exists(configDir):
                os.makedirs(configDir, exist_ok=True)

        try:
            with open(self._configPath, "w", encoding="utf-8") as f:
                json.dump(self._settings, f, indent=2, ensure_ascii=False)
            logger.info(f"[Settings] Saved to: {self._configPath}")
        except Exception as e:
            logger.warning(f"[Settings] Failed to save: {e}")
