/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2025 Parai Wang <parai@foxmail.com>
 *
 * Generated at Wed Apr 30 14:48:38 2025
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "E2E.h"
#include "E2E_Cfg.h"
#include "E2E_Priv.h"
#include "Det.h"

#include <vector>
#include <mutex>
#include <queue>

#include <nlohmann/json.hpp>

using json = nlohmann::ordered_json;
namespace as {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef Std_ReturnType (*E2E_ExecuteFncType)(E2E_ProfileIdType profileId, uint8_t *data,
                                             uint16_t length);
typedef enum {
  E2E_PROTECT_P11,
  E2E_CHECK_P11,
  E2E_PROTECT_P22,
  E2E_CHECK_P22,
  E2E_PROTECT_P44,
  E2E_CHECK_P44,
  E2E_PROTECT_P05,
  E2E_CHECK_P05,
  E2E_TYPE_MAX,
} E2EType_e;

class E2EExecutor {
public:
  E2EExecutor(E2E_ProfileIdType profileId, E2E_ExecuteFncType fnc, bool bProtect, E2EType_e type)
    : m_ExecuteFnc(fnc), m_ProfileId(profileId), m_bProtect(bProtect), m_Type(type) {
  }

  ~E2EExecutor();

  bool Execute(uint8_t *data, uint16_t length) {
    bool rv = false;
    Std_ReturnType ret;
    ret = m_ExecuteFnc(m_ProfileId, data, length);
    if (m_bProtect) {
      rv = (E_OK == ret);
    } else {
      rv = (E_OK == ret) || (E2E_E_OK_SOME_LOST == ret);
    }
    return rv;
  }

private:
  E2E_ExecuteFncType m_ExecuteFnc;
  E2E_ProfileIdType m_ProfileId;
  bool m_bProtect;
  E2EType_e m_Type;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static std::queue<E2E_ProfileIdType> E2E_RecycleQueue[E2E_TYPE_MAX];
static std::vector<E2E_ProtectProfile11ContextType> E2E_ProtectP11_Contexts;
static std::vector<E2E_ProtectProfile11ConfigType> E2E_ProtectProfile11Configs;
static std::vector<E2E_CheckProfile11ContextType> E2E_CheckP11_Contexts;
static std::vector<E2E_CheckProfile11ConfigType> E2E_CheckProfile11Configs;

static std::vector<E2E_ProtectProfile22ContextType> E2E_ProtectP22_Contexts;
static std::vector<E2E_ProtectProfile22ConfigType> E2E_ProtectProfile22Configs;
static std::vector<E2E_CheckProfile22ContextType> E2E_CheckP22_Contexts;
static std::vector<E2E_CheckProfile22ConfigType> E2E_CheckProfile22Configs;

static std::vector<E2E_ProtectProfile44ContextType> E2E_ProtectP44_Contexts;
static std::vector<E2E_ProtectProfile44ConfigType> E2E_ProtectProfile44Configs;
static std::vector<E2E_CheckProfile44ContextType> E2E_CheckP44_Contexts;
static std::vector<E2E_CheckProfile44ConfigType> E2E_CheckProfile44Configs;

static std::vector<E2E_ProtectProfile05ContextType> E2E_ProtectP05_Contexts;
static std::vector<E2E_ProtectProfile05ConfigType> E2E_ProtectProfile05Configs;
static std::vector<E2E_CheckProfile05ContextType> E2E_CheckP05_Contexts;
static std::vector<E2E_CheckProfile05ConfigType> E2E_CheckProfile05Configs;

static std::mutex m_E2ELock;

extern "C" {
E2E_ConfigType E2E_Config;
}

template <typename T> T get(json &obj, std::string key, T dft) {
  T r = dft;
  if (obj.contains(key)) {
    r = obj[key].get<T>();
  }
  return r;
};

static E2EExecutor *E2E_CFGAddProtectP11(json &js) {
  E2EExecutor *e2e = nullptr;
  bool bRecycle = false;
  E2E_ProfileIdType profileId = -1;
  E2E_ProtectProfile11ConfigType *config;
  std::unique_lock<std::mutex> lck(m_E2ELock);

  if (false == E2E_RecycleQueue[E2E_PROTECT_P11].empty()) {
    profileId = E2E_RecycleQueue[E2E_PROTECT_P11].front();
    E2E_RecycleQueue[E2E_PROTECT_P11].pop();
    bRecycle = true;
  } else {
    profileId = E2E_Config.numOfProtectP11;
    E2E_Config.numOfProtectP11++;
    E2E_ProtectP11_Contexts.resize(profileId + 1);
    E2E_ProtectProfile11Configs.resize(profileId + 1);
  }

  E2E_Config.ProtectP11Configs = E2E_ProtectProfile11Configs.data();
  E2E_ProtectP11_Contexts[profileId].Counter = 0;
  config = &E2E_ProtectProfile11Configs[profileId];
  config->context = &E2E_ProtectP11_Contexts[profileId];
  config->P11.DataID = js["DataID"].get<uint16_t>();
  config->P11.CRCOffset = get<uint16_t>(js, "CRCOffset", 0);
  config->P11.CounterOffset = get<uint16_t>(js, "CounterOffset", 8);
  config->P11.DataIDNibbleOffset = get<uint16_t>(js, "DataIDNibbleOffset", 12);
  std::string DataIDMode = get<std::string>(js, "DataIDMode", "NIBBLE");
  if ("NIBBLE" == DataIDMode) {
    config->P11.DataIDMode = E2E_P11_DATAID_NIBBLE;
  } else if ("BOTH" == DataIDMode) {
    config->P11.DataIDMode = E2E_P11_DATAID_BOTH;
  } else if ("ALT" == DataIDMode) {
    config->P11.DataIDMode = E2E_P11_DATAID_ALT;
  } else if ("LOW" == DataIDMode) {
    config->P11.DataIDMode = E2E_P11_DATAID_LOW;
  } else {
    profileId = -1;
  }

  if (-1 != profileId) {
    e2e = new E2EExecutor(profileId, E2E_P11Protect, true, E2E_PROTECT_P11);
  } else if (false == bRecycle) {
    E2E_Config.numOfProtectP11--;
  }
  return e2e;
}

static E2EExecutor *E2E_CFGAddCheckP11(json &js) {
  E2EExecutor *e2e = nullptr;
  bool bRecycle = false;
  E2E_ProfileIdType profileId = -1;
  E2E_CheckProfile11ConfigType *config;
  std::unique_lock<std::mutex> lck(m_E2ELock);
  if (false == E2E_RecycleQueue[E2E_CHECK_P11].empty()) {
    profileId = E2E_RecycleQueue[E2E_CHECK_P11].front();
    E2E_RecycleQueue[E2E_CHECK_P11].pop();
    bRecycle = true;
  } else {
    profileId = E2E_Config.numOfCheckP11;
    E2E_Config.numOfCheckP11++;
    E2E_CheckP11_Contexts.resize(profileId + 1);
    E2E_CheckProfile11Configs.resize(profileId + 1);
  }
  E2E_Config.CheckP11Configs = E2E_CheckProfile11Configs.data();
  E2E_CheckP11_Contexts[profileId].Counter = 0;
  config = &E2E_CheckProfile11Configs[profileId];
  config->context = &E2E_CheckP11_Contexts[profileId];
  config->P11.DataID = js["DataID"].get<uint16_t>();
  config->P11.CRCOffset = get<uint16_t>(js, "CRCOffset", 0);
  config->P11.CounterOffset = get<uint16_t>(js, "CounterOffset", 8);
  config->P11.DataIDNibbleOffset = get<uint16_t>(js, "DataIDNibbleOffset", 12);
  config->MaxDeltaCounter = get<uint16_t>(js, "MaxDeltaCounter", 1);
  std::string DataIDMode = get<std::string>(js, "DataIDMode", "NIBBLE");
  if ("NIBBLE" == DataIDMode) {
    config->P11.DataIDMode = E2E_P11_DATAID_NIBBLE;
  } else if ("BOTH" == DataIDMode) {
    config->P11.DataIDMode = E2E_P11_DATAID_BOTH;
  } else if ("ALT" == DataIDMode) {
    config->P11.DataIDMode = E2E_P11_DATAID_ALT;
  } else if ("LOW" == DataIDMode) {
    config->P11.DataIDMode = E2E_P11_DATAID_LOW;
  } else {
    profileId = -1;
  }

  if (-1 != profileId) {
    e2e = new E2EExecutor(profileId, E2E_P11Check, false, E2E_CHECK_P11);
  } else if (false == bRecycle) {
    E2E_Config.numOfCheckP11--;
  }
  return e2e;
}

static E2EExecutor *E2E_CFGAddProtectP22(json &js) {
  E2EExecutor *e2e = nullptr;
  bool bRecycle = false;
  E2E_ProfileIdType profileId = -1;
  E2E_ProtectProfile22ConfigType *config;
  std::unique_lock<std::mutex> lck(m_E2ELock);
  if (false == E2E_RecycleQueue[E2E_PROTECT_P22].empty()) {
    profileId = E2E_RecycleQueue[E2E_PROTECT_P22].front();
    E2E_RecycleQueue[E2E_PROTECT_P22].pop();
    bRecycle = true;
  } else {
    profileId = E2E_Config.numOfProtectP22;
    E2E_Config.numOfProtectP22++;
    E2E_ProtectP22_Contexts.resize(profileId + 1);
    E2E_ProtectProfile22Configs.resize(profileId + 1);
  }
  E2E_Config.ProtectP22Configs = E2E_ProtectProfile22Configs.data();
  E2E_ProtectP22_Contexts[profileId].Counter = 0;
  config = &E2E_ProtectProfile22Configs[profileId];
  config->context = &E2E_ProtectP22_Contexts[profileId];
  config->P22.Offset = get<uint16_t>(js, "Offset", 0);
  std::vector<uint8_t> didList = js["DataIDList"].get<std::vector<uint8_t>>();
  memcpy(config->P22.DataIDList, didList.data(), 16);

  if (-1 != profileId) {
    e2e = new E2EExecutor(profileId, E2E_P22Protect, true, E2E_PROTECT_P22);
  } else if (false == bRecycle) {
    E2E_Config.numOfProtectP22--;
  }
  return e2e;
}

static E2EExecutor *E2E_CFGAddCheckP22(json &js) {
  E2EExecutor *e2e = nullptr;
  bool bRecycle = false;
  E2E_ProfileIdType profileId = -1;
  E2E_CheckProfile22ConfigType *config;
  std::unique_lock<std::mutex> lck(m_E2ELock);
  if (false == E2E_RecycleQueue[E2E_CHECK_P22].empty()) {
    profileId = E2E_RecycleQueue[E2E_CHECK_P22].front();
    E2E_RecycleQueue[E2E_CHECK_P22].pop();
    bRecycle = true;
  } else {
    profileId = E2E_Config.numOfCheckP22;
    E2E_Config.numOfCheckP22++;
    E2E_CheckP22_Contexts.resize(profileId + 1);
    E2E_CheckProfile22Configs.resize(profileId + 1);
  }
  E2E_Config.CheckP22Configs = E2E_CheckProfile22Configs.data();
  E2E_CheckP22_Contexts[profileId].Counter = 0;
  config = &E2E_CheckProfile22Configs[profileId];
  config->context = &E2E_CheckP22_Contexts[profileId];
  config->P22.Offset = get<uint16_t>(js, "Offset", 0);
  std::vector<uint8_t> didList = js["DataIDList"].get<std::vector<uint8_t>>();
  memcpy(config->P22.DataIDList, didList.data(), 16);
  config->MaxDeltaCounter = get<uint16_t>(js, "MaxDeltaCounter", 1);

  if (-1 != profileId) {
    e2e = new E2EExecutor(profileId, E2E_P22Check, true, E2E_CHECK_P22);
  } else if (false == bRecycle) {
    E2E_Config.numOfCheckP22--;
  }
  return e2e;
}

static E2EExecutor *E2E_CFGAddProtectP44(json &js) {
  E2EExecutor *e2e = nullptr;
  bool bRecycle = false;
  E2E_ProfileIdType profileId = -1;
  E2E_ProtectProfile44ConfigType *config;
  std::unique_lock<std::mutex> lck(m_E2ELock);
  if (false == E2E_RecycleQueue[E2E_PROTECT_P44].empty()) {
    profileId = E2E_RecycleQueue[E2E_PROTECT_P44].front();
    E2E_RecycleQueue[E2E_PROTECT_P44].pop();
    bRecycle = true;
  } else {
    profileId = E2E_Config.numOfProtectP44;
    E2E_Config.numOfProtectP44++;
    E2E_ProtectP44_Contexts.resize(profileId + 1);
    E2E_ProtectProfile44Configs.resize(profileId + 1);
  }
  E2E_Config.ProtectP44Configs = E2E_ProtectProfile44Configs.data();
  E2E_ProtectP44_Contexts[profileId].Counter = 0;
  config = &E2E_ProtectProfile44Configs[profileId];
  config->context = &E2E_ProtectP44_Contexts[profileId];
  config->P44.DataID = get<uint32_t>(js, "DataID", 0);
  config->P44.Offset = get<uint16_t>(js, "Offset", 0);

  if (-1 != profileId) {
    e2e = new E2EExecutor(profileId, E2E_P44Protect, true, E2E_PROTECT_P44);
  } else if (false == bRecycle) {
    E2E_Config.numOfProtectP44--;
  }
  return e2e;
}

static E2EExecutor *E2E_CFGAddCheckP44(json &js) {
  E2EExecutor *e2e = nullptr;
  bool bRecycle = false;
  E2E_ProfileIdType profileId = -1;
  E2E_CheckProfile44ConfigType *config;
  std::unique_lock<std::mutex> lck(m_E2ELock);
  if (false == E2E_RecycleQueue[E2E_CHECK_P44].empty()) {
    profileId = E2E_RecycleQueue[E2E_CHECK_P44].front();
    E2E_RecycleQueue[E2E_CHECK_P44].pop();
    bRecycle = true;
  } else {
    profileId = E2E_Config.numOfCheckP44;
    E2E_Config.numOfCheckP44++;
    E2E_CheckP44_Contexts.resize(profileId + 1);
    E2E_CheckProfile44Configs.resize(profileId + 1);
  }
  E2E_Config.CheckP44Configs = E2E_CheckProfile44Configs.data();
  E2E_CheckP44_Contexts[profileId].Counter = 0;
  config = &E2E_CheckProfile44Configs[profileId];
  config->context = &E2E_CheckP44_Contexts[profileId];
  config->P44.DataID = get<uint32_t>(js, "DataID", 0);
  config->P44.Offset = get<uint16_t>(js, "Offset", 0);
  config->MaxDeltaCounter = get<uint16_t>(js, "MaxDeltaCounter", 1);

  if (-1 != profileId) {
    e2e = new E2EExecutor(profileId, E2E_P44Check, true, E2E_CHECK_P44);
  } else if (false == bRecycle) {
    E2E_Config.numOfCheckP44--;
  }
  return e2e;
}

static E2EExecutor *E2E_CFGAddProtectP05(json &js) {
  E2EExecutor *e2e = nullptr;
  bool bRecycle = false;
  E2E_ProfileIdType profileId = -1;
  E2E_ProtectProfile05ConfigType *config;
  std::unique_lock<std::mutex> lck(m_E2ELock);
  if (false == E2E_RecycleQueue[E2E_PROTECT_P05].empty()) {
    profileId = E2E_RecycleQueue[E2E_PROTECT_P05].front();
    E2E_RecycleQueue[E2E_PROTECT_P05].pop();
    bRecycle = true;
  } else {
    profileId = E2E_Config.numOfProtectP05;
    E2E_Config.numOfProtectP05++;
    E2E_ProtectP05_Contexts.resize(profileId + 1);
    E2E_ProtectProfile05Configs.resize(profileId + 1);
  }
  E2E_Config.ProtectP05Configs = E2E_ProtectProfile05Configs.data();
  E2E_ProtectP05_Contexts[profileId].Counter = 0;
  config = &E2E_ProtectProfile05Configs[profileId];
  config->context = &E2E_ProtectP05_Contexts[profileId];
  config->P05.DataID = get<uint16_t>(js, "DataID", 0);
  config->P05.Offset = get<uint16_t>(js, "Offset", 0);

  if (-1 != profileId) {
    e2e = new E2EExecutor(profileId, E2E_P05Protect, true, E2E_PROTECT_P05);
  } else if (false == bRecycle) {
    E2E_Config.numOfProtectP05--;
  }
  return e2e;
}

static E2EExecutor *E2E_CFGAddCheckP05(json &js) {
  E2EExecutor *e2e = nullptr;
  bool bRecycle = false;
  E2E_ProfileIdType profileId = -1;
  E2E_CheckProfile05ConfigType *config;
  std::unique_lock<std::mutex> lck(m_E2ELock);
  if (false == E2E_RecycleQueue[E2E_CHECK_P05].empty()) {
    profileId = E2E_RecycleQueue[E2E_CHECK_P05].front();
    E2E_RecycleQueue[E2E_CHECK_P05].pop();
    bRecycle = true;
  } else {
    profileId = E2E_Config.numOfCheckP05;
    E2E_Config.numOfCheckP05++;
    E2E_CheckP05_Contexts.resize(profileId + 1);
    E2E_CheckProfile05Configs.resize(profileId + 1);
  }
  E2E_Config.CheckP05Configs = E2E_CheckProfile05Configs.data();
  E2E_CheckP05_Contexts[profileId].Counter = 0;
  config = &E2E_CheckProfile05Configs[profileId];
  config->context = &E2E_CheckP05_Contexts[profileId];
  config->P05.DataID = get<uint16_t>(js, "DataID", 0);
  config->P05.Offset = get<uint16_t>(js, "Offset", 0);
  config->MaxDeltaCounter = get<uint16_t>(js, "MaxDeltaCounter", 1);

  if (-1 != profileId) {
    e2e = new E2EExecutor(profileId, E2E_P05Check, true, E2E_CHECK_P05);
  } else if (false == bRecycle) {
    E2E_Config.numOfCheckP05--;
  }
  return e2e;
}
/* ================================ [ FUNCTIONS ] ============================================== */
INITIALIZER(E2E_CFGInit) {
  uint32_t maxNum = 32;
  E2E_ProtectP11_Contexts.reserve(maxNum);
  E2E_ProtectProfile11Configs.reserve(maxNum);
  E2E_CheckP11_Contexts.reserve(maxNum);
  E2E_CheckProfile11Configs.reserve(maxNum);
  E2E_Config.ProtectP11Configs = E2E_ProtectProfile11Configs.data();
  E2E_Config.numOfProtectP11 = 0;
  E2E_Config.CheckP11Configs = E2E_CheckProfile11Configs.data();
  E2E_Config.numOfCheckP11 = 0;
  E2E_Init(&E2E_Config);
}

E2EExecutor *E2E_New(json &js, bool bProtect) {
  E2EExecutor *e2e = nullptr;
  if (bProtect) {
    if ("P11" == js["profile"]) {
      e2e = E2E_CFGAddProtectP11(js);
    } else if ("P22" == js["profile"]) {
      e2e = E2E_CFGAddProtectP22(js);
    } else if ("P44" == js["profile"]) {
      e2e = E2E_CFGAddProtectP44(js);
    } else if ("P05" == js["profile"]) {
      e2e = E2E_CFGAddProtectP05(js);
    }
  } else {
    if ("P11" == js["profile"]) {
      e2e = E2E_CFGAddCheckP11(js);
    } else if ("P22" == js["profile"]) {
      e2e = E2E_CFGAddCheckP22(js);
    } else if ("P44" == js["profile"]) {
      e2e = E2E_CFGAddCheckP44(js);
    } else if ("P05" == js["profile"]) {
      e2e = E2E_CFGAddCheckP05(js);
    }
  }

  if (nullptr == e2e) {
    throw std::runtime_error("E2E_CFG: profile not supported: " + js.dump());
  }
  return e2e;
}

bool E2E_Execute(E2EExecutor *e2e, uint8_t *data, uint16_t length) {
  return e2e->Execute(data, length);
}

E2EExecutor::~E2EExecutor() {
  std::unique_lock<std::mutex> lck(m_E2ELock);
  E2E_RecycleQueue[m_Type].push(m_ProfileId);
}

void E2E_Free(E2EExecutor *e2e) {
  delete e2e;
}

} // namespace as
