/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail->com>
 */
/* ================================ { INCLUDES  ] ============================================== */
#include "signal.hpp"
#include "Log.hpp"
#include "Std_Bit.h"
#include "linlib.h"
#include "canlib.h"
#include <sstream>
#include "Std_Timer.h"

namespace as {
/* ================================ { MACROS    ] ============================================== */
template <typename T> T get(json &obj, std::string key, T dft) {
  T r = dft;
  if (obj.contains(key)) {
    r = obj[key].get<T>();
  }
  return r;
};
/* ================================ { TYPES     ] ============================================== */
/* ================================ { DECLARES  ] ============================================== */
extern E2EExecutor *E2E_New(json &js, bool bProtect);
extern void E2E_Free(E2EExecutor *e2e);
extern bool E2E_Execute(E2EExecutor *e2e, uint8_t *data, uint16_t length);

static int figure_create(lua_State *L);
static int figure_add_point(lua_State *L);
/* ================================ { DATAS     ] ============================================== */
std::chrono::high_resolution_clock::time_point Signal::s_StartTimePoint =
  std::chrono::high_resolution_clock::now();

std::mutex Network::s_Lock;
std::map<std::string, Network *> Network::s_Networks;

const luaL_Reg Network::s_LuaInferfaces[] = {
  {"set", Network::Set},
  {"get", Network::Get},
  {"trigger", Network::Trigger},
  {nullptr, nullptr},
};

static const luaL_Reg s_FigureInterfaces[] = {
  {"create", figure_create},
  {"add_point", figure_add_point},
  {nullptr, nullptr},
};

std::shared_ptr<AsLuaScript> Network::s_Script = nullptr;
std::chrono::time_point<std::chrono::high_resolution_clock> Network::s_LuaTimer;
std::string Network::s_LuaErrorMsg;
uint32_t Network::s_LuaNextCycle;
/* ================================ { LOCALS    ] ============================================== */
std::vector<std::string> split(std::string s, char del) {
  std::vector<std::string> results;
  std::stringstream ss(s);
  std::string word;
  while (!ss.eof()) {
    std::getline(ss, word, del);
    results.push_back(word);
  }
  return results;
}

static int CreateFigureInterface(lua_State *L) {
  luaL_newlib(L, s_FigureInterfaces);
  return 1;
}

static int figure_create(lua_State *L) {
  int n = lua_gettop(L); /* number of arguments */
  if (1 == n) {
    figure::FigureConfig figCfg;
    lua_arg_t arg;
    auto r = aslua_parse(L, 1, arg);
    if ((0 == r) && (LUA_ARG_TYPE_TABLE_STRING_MAP == arg.type)) {
      if (false == arg.get("name", figCfg.name)) {
        return luaL_error(L, "figure name not provided");
      }
      arg.get("titleX", figCfg.titleX);
      arg.get("titleY", figCfg.titleY);
      arg.get("minX", figCfg.minX);
      arg.get("maxX", figCfg.maxX);
      arg.get("minY", figCfg.minY);
      arg.get("maxY", figCfg.maxY);
      std::vector<lua_arg_t> lineArgs;
      if (false == arg.get("lines", lineArgs)) {
        return luaL_error(L, "figure lines not provided");
      }
      for (auto &lineArg : lineArgs) {
        figure::LineConfig line;
        if (false == lineArg.get("name", line.name)) {
          return luaL_error(L, "line name not provided for figure %s", figCfg.name.c_str());
        }
        std::string ltype = "line";
        lineArg.get("type", ltype);
        if ("line" == ltype) {
          line.type = figure::LineType::LINE;
        } else if ("spline" == ltype) {
          line.type = figure::LineType::SPLINE;
        } else if ("scatter" == ltype) {
          line.type = figure::LineType::SCATTER;
        } else if ("dot" == ltype) {
          line.type = figure::LineType::DOT;
        } else {
          return luaL_error(L, "line type %s invalide for figure %s line %s", ltype.c_str(),
                            figCfg.name.c_str(), line.name.c_str());
        }
        figCfg.lines.push_back(line);
      }

      auto topic = figure::config();
      topic->put(figCfg);
    } else {
      return luaL_error(L, "figure with invalid arguments");
    }
  } else {
    return luaL_error(L, "figure config with invalid number of arguments: %d", n);
  }
  return 1;
}

static int figure_add_point(lua_State *L) {
  int n = lua_gettop(L); /* number of arguments */
  if (4 == n) {
    std::string figure = lua_tostring(L, 1);
    if (figure.empty()) {
      return luaL_error(L, "figure name not given");
    }

    std::string line = lua_tostring(L, 2);
    if (line.empty()) {
      return luaL_error(L, "line name not given");
    }

    int isnum = 0;
    float x = lua_tonumberx(L, 3, &isnum);
    if (0 == isnum) {
      return luaL_error(L, "x not given");
    }

    isnum = 0;
    float y = lua_tonumberx(L, 4, &isnum);
    if (0 == isnum) {
      return luaL_error(L, "y not given");
    }

    auto topic = figure::line(figure, line);
    if (nullptr == topic) {
      return luaL_error(L, "%s %s not found", figure.c_str(), line.c_str());
    }

    figure::Point p = {x, y};
    topic->put(p);
    if (topic->size() >= 256) { /* NOTE: consume too slow or no consumer */
      topic->get(p);
    }
  } else {
    return luaL_error(L, "figure config with invalid number of arguments: %d", n);
  }
  return 1;
}
/* ================================ { FUNCTIONS ] ============================================== */
float Signal::timimg(void) {
  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed =
    std::chrono::duration_cast<std::chrono::milliseconds>(now - s_StartTimePoint).count();
  return (float)elapsed / 1000.0;
}

Signal::Signal(json &js, Message *msg) : m_Msg(msg), m_Lock(msg->getLock()) {
  m_Name = js["name"].get<std::string>();
  m_StartBit = js["start"].get<uint32_t>();
  m_BitSize = js["size"].get<uint32_t>();
  auto endian = js["endian"].get<std::string>();
  if ("big" == endian) {
    m_Endian = Endian::BIG;
  } else {
    m_Endian = Endian::LITTLE;
  }
  m_Mask = (uint32_t)((1 << m_BitSize) - 1);
  LOG(INFO, "  signal %s start=%d size=%d %s endian\n", m_Name.c_str(), m_StartBit, m_BitSize,
      endian.c_str());
}

std::string Signal::name(void) {
  return m_Name;
}

void Signal::write(uint32_t value, bool byLua) {
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  m_Value = m_Mask & value;
  if (m_ViewMsgQ != nullptr) {
    if (m_LastValue != m_Value) { /* only need to push to the queue if there is a change */
      if (m_ViewMsgQ->size() > 64) {
        m_ViewMsgQ->clear();
      }
      ViewMsg vmsg = {timimg(), m_Value};
      m_ViewMsgQ->put(vmsg);
    }
    m_LastValue = m_Value;
  }
  m_byLua = byLua;
}

bool Signal::bylua(void) {
  bool ret;
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  ret = m_byLua;
  m_byLua = false;
  return ret;
}

uint32_t Signal::read(void) {
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  return m_Value;
}

void Signal::createViewMsgQueue(void) {
  if (m_ViewMsgQ == nullptr) {
    m_ViewMsgQ = std::make_shared<MessageQueue<ViewMsg>>(name());
  }
}

void Signal::destoryViewMsgQueue(void) {
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  m_ViewMsgQ = nullptr;
}

std::vector<Signal::ViewMsg> Signal::view(void) {
  std::vector<ViewMsg> vmsgs;
  ViewMsg vmsg;

  bool r = true;
  while (r && m_ViewMsgQ) {
    r = m_ViewMsgQ->get(vmsg, true, 0);
    if (r) {
      vmsgs.push_back(vmsg);
    }
  }

  if (0 == vmsgs.size()) {
    vmsg.time = timimg();
    vmsg.value = read();
    vmsgs.push_back(vmsg);
  }

  return vmsgs;
}

uint32_t Signal::start(void) {
  return m_StartBit;
}

uint32_t Signal::size(void) {
  return m_BitSize;
}

Signal::Endian Signal::endian(void) {
  return m_Endian;
}

Message *Signal::message(void) {
  return m_Msg;
}

Signal::~Signal() {
}

Message::Message(json &js, Network *network) {
  m_Network = network;
  m_Name = js["name"].get<std::string>();
  m_Node = js["node"].get<std::string>();
  m_Dlc = js["dlc"].get<uint32_t>();
  m_Id = js["id"].get<uint32_t>();
  m_Period = get<uint32_t>(js, "period", 1000);
  LOG(INFO, "messgae %s id=0x%x dlc=%d from node %s\n", m_Name.c_str(), m_Id, m_Dlc,
      m_Node.c_str());
  for (auto &cfg : js["signals"]) {
    auto sig = std::make_shared<Signal>(cfg, this);
    m_Signals.push_back(sig);
  }

  if (Network::Type::CAN == network->type()) {
    /* for CAN, the me is the DUT(Device Under Test) */
    if (m_Node != network->me()) {
      m_IsTx = true;
    }
  } else {
    /* for LIN, the me is the tester */
    if (m_Node == network->me()) {
      m_IsTx = true;
    }
  }

  if (js.contains("E2E")) {
    m_E2E = E2E_New(js["E2E"], m_IsTx);
  }

  memset(m_Buffer, 0, sizeof(m_Buffer));

  m_Timer = std::chrono::high_resolution_clock::now();

  m_Requested.store(false);
}

Network *Message::network() {
  return m_Network;
}

std::string Message::name(void) {
  return m_Name;
}

uint32_t Message::id(void) {
  return m_Id;
}

uint32_t Message::dlc(void) {
  return m_Dlc;
}

uint32_t Message::length(void) {
  return sizeof(m_Buffer);
}

uint32_t Message::period(void) {
  return m_Period;
}

void Message::set_period(uint32_t period) {
  m_Period = period;
}

std::vector<std::shared_ptr<Signal>> &Message::Signals(void) {
  return m_Signals;
}

std::shared_ptr<Signal> Message::signal(std::string name) {
  std::shared_ptr<Signal> sig = nullptr;

  for (auto s : m_Signals) {
    if (s->name() == name) {
      sig = s;
      break;
    }
  }

  return sig;
}

std::recursive_mutex &Message::getLock() {
  return m_Lock;
}

bool Message::IsTransmit(void) {
  return m_IsTx;
}

void Message::run(void) {
  runLua();

  if (m_IsTx) {
    if (m_Period > 0) {
      auto now = std::chrono::high_resolution_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_Timer);
      if (elapsed.count() >= m_Period) {
        m_Timer += std::chrono::milliseconds(m_Period);
        trigger();
      }
    }
  } else {
    if (Network::Type::LIN == m_Network->type()) {
      if (m_Period > 0) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_Timer);
        if (elapsed.count() >= m_Period) {
          m_Timer += std::chrono::milliseconds(m_Period);
          trigger();
        }
      }
    } else {
      trigger();
    }
  }
}

bool Message::trigger(void) {
  m_Requested.store(true);
  m_Network->schedule();
  return true;
}

void Message::call(std::string api, CallType callType) {
  std::vector<lua_arg_t> outArgs;
  std::vector<lua_arg_t> inArgs;
  if ((CALL_WITH_SIGNALS_RETURN_SIGNALS_AND_PERIOD == callType) ||
      (CALL_WITH_SIGNALS_RETURN_VOID == callType)) {
    inArgs.resize(1);
    inArgs[0].type = LUA_ARG_TYPE_TABLE_STRING_UINT32;
    for (auto sig : m_Signals) {
      inArgs[0].stringUint32Map[sig->name()] = sig->read();
    }
  }

  if (CALL_WITH_SIGNALS_RETURN_SIGNALS_AND_PERIOD == callType) {
    outArgs.resize(2);
    outArgs[0].type = LUA_ARG_TYPE_TABLE_STRING_UINT32; /* new values of the signals */
    outArgs[1].type = LUA_ARG_TYPE_UINT32;              /* next period to call this exec again */
  }

  auto r = m_Script->call(api, inArgs, outArgs);
  if (0 == r) {
    if (CALL_WITH_SIGNALS_RETURN_SIGNALS_AND_PERIOD == callType) {
      int index = 0;
      for (auto sig : m_Signals) {
        auto it = outArgs[0].stringUint32Map.find(sig->name());
        if (it != outArgs[0].stringUint32Map.end()) {
          sig->write(it->second, true);
        }
        index++;
      }
      m_LuaNextCycle = outArgs[1].u32;
      if (0 == m_LuaNextCycle) {
        m_Script = nullptr;
      }
    }
  } else {
    m_Script = nullptr;
    m_LuaErrorMsg =
      "call '" + api + "' failed for message <" + m_Name + ">, error is " + std::to_string(r);
  }
}

void Message::callbackToLua(std::string api, CallType callType) {
  if (nullptr != m_Script) {
    if (m_Script->hasApi(api)) {
      call(api, callType);
    }
  }
}

void Message::startLua(std::string scriptFile) {
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  std::vector<AsLuaScript::Library> libs = {{"com", Network::CreateLuaInterface},
                                            {"figure", CreateFigureInterface}};
  try {
    m_Script = std::make_shared<AsLuaScript>(scriptFile, libs);
  } catch (const std::exception &e) {
    m_LuaErrorMsg = e.what();
    return;
  }
  m_LuaNextCycle = 0;
}

void Message::runLua(void) {
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  if (nullptr == m_Script) {
    return;
  }

  if (0 == m_LuaNextCycle) {
    m_LuaTimer = std::chrono::high_resolution_clock::now();
    call("init");
  } else {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_LuaTimer);
    if (elapsed.count() >= m_LuaNextCycle) {
      m_LuaTimer += std::chrono::milliseconds(m_LuaNextCycle);
      call("main");
    }
  }
}

std::string Message::luaError(void) {
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  std::string err = m_LuaErrorMsg;
  m_LuaErrorMsg.clear();
  return err;
}

bool Message::isLuaRunning(void) {
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  return m_Script != nullptr;
}

void Message::stopLua(void) {
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  m_Script = nullptr;
}

uint8_t *Message::data() {
  return m_Buffer;
}

E2EExecutor *Message::E2E() {
  return m_E2E;
};

void Message::backward(void) {
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  if (m_IsTx) {
    for (auto sig : m_Signals) {
      if (Signal::Endian::BIG == sig->endian()) {
        Std_BitSetBigEndian(m_Buffer, sig->read(), (uint16_t)sig->start(), (uint8_t)sig->size());
      } else {
        Std_BitSetLittleEndian(m_Buffer, sig->read(), (uint16_t)sig->start(), (uint8_t)sig->size());
      }
    }
  }
}

std::shared_ptr<com::Message> Message::trace(void) {
  std::shared_ptr<com::Message> msg = std::make_shared<com::Message>();
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  {
    char date[64];
    Std_GetDateTime(date, sizeof(date));
    msg->timestamp = std::string(date);
  }

  msg->name = name();
  msg->network = m_Network->name();

  if (m_IsTx) {
    msg->dir = "TX";
  } else {
    msg->dir = "RX";
  }

  {
    std::stringstream ss;
    ss << "0x" << std::hex << (m_Id & (~CAN_ID_EXTENDED));
    msg->id = ss.str();
  }

  {
    std::stringstream ss;
    ss << m_Dlc;
    msg->dlc = ss.str();
  }

  {
    std::stringstream ss;
    for (uint32_t i = 0; i < m_Dlc; i++) {
      ss << std::setw(2) << std::setfill('0') << std::hex << (int)m_Buffer[i];
      if ((i + 1) < m_Dlc) {
        ss << " ";
      }
    }
    msg->data = ss.str();
  }

  for (auto sig : m_Signals) {
    auto v = sig->read();
    std::stringstream ss;
    ss << v << " ( 0x" << std::hex << v << " )";
    msg->Signals.push_back({sig->name(), ss.str()});
  }

  return msg;
}

void Message::forward(void) {
  uint32_t value;
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  if (false == m_IsTx) {
    for (auto sig : m_Signals) {
      if (Signal::Endian::BIG == sig->endian()) {
        value = Std_BitGetBigEndian(m_Buffer, (uint16_t)sig->start(), (uint8_t)sig->size());
      } else {
        value = Std_BitGetLittleEndian(m_Buffer, (uint16_t)sig->start(), (uint8_t)sig->size());
      }
      sig->write(value);
    }

    callbackToLua("on_rx", CallType::CALL_WITH_SIGNALS_RETURN_VOID);
  }
}

Message::State Message::state() {
  Message::State st = Message::State::IDLE;
  if (m_Requested.load()) {
    m_Requested.store(false);
    if (m_IsTx) {
      st = Message::State::TRANSMIT;
    } else {
      st = Message::State::RECEIVE;
    }
  }

  return st;
}

Message::~Message() {
  if (nullptr != m_E2E) {
    E2E_Free(m_E2E);
  }
}

Network::Network(json &js) {
  m_Name = get<std::string>(js, "name", "unknown");
  m_Device = get<std::string>(js, "device", "simulator_v2");
  m_Me = get<std::string>(js, "me", "AS");
  m_Port = get<uint32_t>(js, "port", 0);
  m_Baudrate = get<uint32_t>(js, "baudrate", 500000);
  auto type = get<std::string>(js, "network", "CAN");
  if (type == "CAN") {
    m_Type = Type::CAN;
  } else if (type == "LIN") {
    m_Type = Type::LIN;
  } else {
    throw std::runtime_error("failed to create network with type " + type);
  }

  for (auto &cfg : js["messages"]) {
    auto msg = std::make_shared<Message>(cfg, this);
    m_Messages.push_back(msg);
  }

  std::unique_lock<std::mutex> lck(s_Lock);
  auto it = s_Networks.find(m_Name);
  if (it == s_Networks.end()) {
    s_Networks[m_Name] = this;
  } else {
    throw std::runtime_error("network already exists: " + m_Name);
  }

  m_Pub = std::make_shared<com::Publisher>(m_Name);
}

std::shared_ptr<Signal> Network::find(std::string name) {
  std::shared_ptr<Signal> sig = nullptr;

  std::string netName;
  std::string msgName;
  std::string sigName;
  std::vector<std::string> names = split(name, '.');
  if (1 == names.size()) {
    sigName = names[0];
  } else if (2 == names.size()) {
    msgName = names[0];
    sigName = names[1];
  } else if (3 == names.size()) {
    netName = names[0];
    msgName = names[1];
    sigName = names[2];
  } else {
    return nullptr;
  }

  std::unique_lock<std::mutex> lck(s_Lock);
  for (auto it : s_Networks) {
    if (false == netName.empty()) {
      if (it.first != netName) {
        continue;
      }
    }
    for (auto msg : it.second->messages()) {
      if (false == msgName.empty()) {
        if (msg->name() != msgName) {
          continue;
        }
      }
      sig = msg->signal(sigName);
      if (nullptr != sig) {
        break;
      }
    }
    if (nullptr != sig) {
      break;
    }
  }

  return sig;
}

std::shared_ptr<Message> Network::find_message(std::string name) {
  std::shared_ptr<Message> msg = nullptr;

  std::string netName;
  std::string msgName;
  std::vector<std::string> names = split(name, '.');
  if (1 == names.size()) {
    msgName = names[0];
  } else if (2 == names.size()) {
    netName = names[0];
    msgName = names[1];
  } else {
    return nullptr;
  }

  std::unique_lock<std::mutex> lck(s_Lock);
  for (auto it : s_Networks) {
    if (false == netName.empty()) {
      if (it.first != netName) {
        continue;
      }
    }
    msg = it.second->message(msgName);
    if (nullptr != msg) {
      break;
    }
  }

  return msg;
}

std::string Network::name(void) {
  return m_Name;
}

std::string Network::me(void) {
  return m_Me;
}

Network::Type Network::type(void) {
  return m_Type;
}

std::vector<std::shared_ptr<Message>> &Network::messages(void) {
  return m_Messages;
}

std::shared_ptr<Message> Network::message(std::string name) {
  std::shared_ptr<Message> msg = nullptr;

  for (auto m : m_Messages) {
    if (m->name() == name) {
      msg = m;
      break;
    }
  }

  return msg;
}

bool Network::start(void) {
  bool ret = false;
  if (Type::CAN == m_Type) {
    m_Fd = can_open(m_Device.c_str(), m_Port, m_Baudrate);
  } else {
    m_Fd = lin_open(m_Device.c_str(), m_Port, m_Baudrate);
  }

  if (m_Fd >= 0) {
    m_Stop = false;
    m_Thread = std::thread(&Network::run, this);
    ret = true;
  }

  return ret;
}

void Network::schedule(void) {
  m_CondVar.notify_one();
}

void Network::run(void) {
  LOG(INFO, "network %s online: %s:%d:%d\n", m_Name.c_str(), m_Device.c_str(), m_Port, m_Baudrate);
  while (false == m_Stop) {
    for (auto msg : m_Messages) {
      msg->run();
      auto state = msg->state();
      if (Message::State::TRANSMIT == state) {
        transmit(msg);
      } else if (Message::State::RECEIVE == state) {
        receive(msg);
      }
    }
    Network::runLua();
    std::unique_lock<std::mutex> lck(m_Lock);
    m_CondVar.wait_for(lck, std::chrono::milliseconds(1));
  }
  LOG(INFO, "network %s offline\n", m_Name.c_str());
}

bool Network::stop(void) {
  m_Stop = true;

  if (m_Thread.joinable()) {
    m_Thread.join();
  }

  if (m_Fd >= 0) {
    if (Type::CAN == m_Type) {
      can_close(m_Fd);
    } else {
      lin_close(m_Fd);
    }
    m_Fd = -1;
  }

  std::unique_lock<std::mutex> lck(s_Lock);
  s_Networks.erase(m_Name);

  return true;
}

void Network::transmit(std::shared_ptr<Message> msg) {
  bool r = true;
  auto &lock = msg->getLock();
  std::unique_lock<std::recursive_mutex> lck(lock);
  msg->backward();
  if (nullptr != msg->E2E()) {
    r = E2E_Execute(msg->E2E(), msg->data(), (uint16_t)msg->dlc());
  }
  if (true == r) {
    if (Type::CAN == m_Type) {
      r = can_write(m_Fd, msg->id(), (uint8_t)msg->dlc(), msg->data());
    } else {
      r = lin_write(m_Fd, msg->id(), (uint8_t)msg->dlc(), msg->data(), true);
    }
  }

  if (false == r) {
    LOG(ERROR, "network %s transmit for message %s id=0x%x dlc=%d failed\n", m_Name.c_str(),
        msg->name().c_str(), msg->id(), msg->dlc());
  } else {
    m_Pub->push(msg->trace());
    msg->callbackToLua("on_tx", Message::CallType::CALL_VOID_RETURN_VOID);
    std::string api = "on_tx_" + m_Name + "_" + msg->name();
    Network::callbackToLua(api);
  }
}

void Network::receive(std::shared_ptr<Message> msg) {
  bool r;
  if (Type::CAN == m_Type) {
    uint32_t canid = msg->id();
    uint8_t dlc = msg->length();
    r = can_read(m_Fd, &canid, &dlc, msg->data());
  } else {
    r = lin_read(m_Fd, msg->id(), (uint8_t)msg->dlc(), msg->data(), true, 1000);
  }
  if (true == r) {
    if (nullptr != msg->E2E()) {
      r = E2E_Execute(msg->E2E(), msg->data(), (uint16_t)msg->dlc());
      if (false == r) {
        LOG(ERROR, "network %s receive message %s with invalid E2E\n", m_Name.c_str(),
            msg->name().c_str());
      }
    }
  }

  if (true == r) {
    msg->forward();
    m_Pub->push(msg->trace());
    std::string api = "on_rx_" + m_Name + "_" + msg->name();
    Network::callbackToLua(api);
  }
}

Network::~Network() {
  stop();
}

int Network::Set(lua_State *L) {
  int n = lua_gettop(L); /* number of arguments */
  if (2 == n) {
    int isnum;
    auto s = lua_tostring(L, 1);
    auto value = lua_tointegerx(L, 2, &isnum);
    if ((nullptr == s) || (0 == isnum)) {
      return luaL_error(L, "set with invalid arguments");
    }

    auto sig = find(std::string(s));
    if (sig != nullptr) {
      sig->write(value, true);
    } else {
      return luaL_error(L, "signal %s not found", s);
    }
  } else {
    return luaL_error(L, "set with invalid number of arguments: %d", n);
  }
  return 1;
}

int Network::Get(lua_State *L) {
  int n = lua_gettop(L); /* number of arguments */
  if (1 == n) {
    auto s = lua_tostring(L, 1);
    if (nullptr == s) {
      return luaL_error(L, "get with invalid arguments");
    }

    auto sig = find(std::string(s));
    if (sig != nullptr) {
      auto value = sig->read();
      lua_pushinteger(L, value);
    } else {
      return luaL_error(L, "signal %s not found", s);
    }
  } else {
    return luaL_error(L, "set with invalid number of arguments: %d", n);
  }
  return 1;
}

int Network::Trigger(lua_State *L) {
  int n = lua_gettop(L); /* number of arguments */
  if (1 == n) {
    auto s = lua_tostring(L, 1);
    if (nullptr == s) {
      return luaL_error(L, "get with invalid arguments");
    }

    std::shared_ptr<Message> msg = find_message(std::string(s));
    if (msg != nullptr) {
      lua_pushboolean(L, msg->trigger());
    } else {
      return luaL_error(L, "signal %s not found", s);
    }
  } else {
    return luaL_error(L, "set with invalid number of arguments: %d", n);
  }
  return 1;
}

int Network::CreateLuaInterface(lua_State *L) {
  luaL_newlib(L, s_LuaInferfaces);
  return 1;
}

void Network::startLua(std::string scriptFile) {
  std::unique_lock<std::mutex> lck(s_Lock);
  std::vector<AsLuaScript::Library> libs = {{"com", Network::CreateLuaInterface},
                                            {"figure", CreateFigureInterface}};
  try {
    s_Script = std::make_shared<AsLuaScript>(scriptFile, libs);
  } catch (const std::exception &e) {
    s_LuaErrorMsg = e.what();
    return;
  }
  s_LuaNextCycle = 0;
}

void Network::callbackToLua(std::string api) {
  std::unique_lock<std::mutex> lck(s_Lock);

  if (nullptr == s_Script) {
    return;
  }

  std::vector<lua_arg_t> inArgs;
  std::vector<lua_arg_t> outArgs;
  if (s_Script->hasApi(api)) {
    auto r = s_Script->call(api, inArgs, outArgs);
    if (0 != r) {
      s_Script = nullptr;
      s_LuaErrorMsg = "callback '" + api + "' failed, error is " + std::to_string(r);
    }
  }
}

void Network::call(std::string api) {
  std::vector<lua_arg_t> inArgs;
  std::vector<lua_arg_t> outArgs;
  outArgs.resize(1);
  outArgs[0].type = LUA_ARG_TYPE_UINT32; /* next period to call this exec again */

  auto r = s_Script->call(api, inArgs, outArgs);
  if (0 == r) {
    s_LuaNextCycle = outArgs[0].u32;
    if (0 == s_LuaNextCycle) {
      s_Script = nullptr;
    }
  } else {
    s_Script = nullptr;
    s_LuaErrorMsg = "call '" + api + "' failed, error is " + std::to_string(r);
  }
}

void Network::runLua(void) {
  std::unique_lock<std::mutex> lck(s_Lock);

  if (nullptr == s_Script) {
    return;
  }
  if (0 == s_LuaNextCycle) {
    s_LuaTimer = std::chrono::high_resolution_clock::now();
    call("init");
  } else {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_LuaTimer);
    if (elapsed.count() >= s_LuaNextCycle) {
      s_LuaTimer += std::chrono::milliseconds(s_LuaNextCycle);
      call("main");
    }
  }
}

std::string Network::luaError(void) {
  std::unique_lock<std::mutex> lck(s_Lock);
  std::string err = s_LuaErrorMsg;
  s_LuaErrorMsg.clear();
  return err;
}

bool Network::isLuaRunning(void) {
  std::unique_lock<std::mutex> lck(s_Lock);
  return s_Script != nullptr;
}

void Network::stopLua(void) {
  std::unique_lock<std::mutex> lck(s_Lock);
  s_Script = nullptr;
}
} /* namespace as */
