/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail->com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "signal.hpp"
#include "Log.hpp"
#include "Std_Bit.h"
#include "linlib.h"
#include "canlib.h"
#include <sstream>
#include "Std_Timer.h"

namespace as {
/* ================================ [ MACROS    ] ============================================== */
template <typename T> T get(json &obj, std::string key, T dft) {
  T r = dft;
  if (obj.contains(key)) {
    r = obj[key].get<T>();
  }
  return r;
};
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern E2EExecutor *E2E_New(json &js, bool bProtect);
extern void E2E_Free(E2EExecutor *e2e);
extern bool E2E_Execute(E2EExecutor *e2e, uint8_t *data, uint16_t length);

static int Figure_Create(lua_State *L);
static int Figure_AddPoint(lua_State *L);
/* ================================ [ DATAS     ] ============================================== */
std::chrono::high_resolution_clock::time_point Signal::s_StartTimePoint =
  std::chrono::high_resolution_clock::now();

std::mutex Network::s_Lock;
std::map<std::string, Network *> Network::s_Networks;

const luaL_Reg Network::s_LuaInterfaces[] = {
  {"set", Network::Set},
  {"get", Network::Get},
  {"trigger", Network::Trigger},
  {nullptr, nullptr},
};

static const luaL_Reg s_FigureInterfaces[] = {
  {"create", Figure_Create},
  {"add_point", Figure_AddPoint},
  {nullptr, nullptr},
};

std::shared_ptr<AsLuaScript> Network::s_Script = nullptr;
std::chrono::time_point<std::chrono::high_resolution_clock> Network::s_LuaTimer;
std::string Network::s_LuaErrorMsg;
uint32_t Network::s_LuaNextCycle;
/* ================================ [ LOCALS    ] ============================================== */
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

static int Figure_Create(lua_State *L) {
  int n = lua_gettop(L); /* number of arguments */
  if (1 == n) {
    figure::FigureConfig figCfg;
    Lua_ArgType arg;
    auto r = AsLua_Parse(L, 1, arg);
    if ((0 == r) && (LUA_ARG_TYPE_TABLE_STRING_MAP == arg.Type)) {
      if (false == arg.Get("Name", figCfg.Name)) {
        return luaL_error(L, "figure name not provided");
      }
      arg.Get("titleX", figCfg.TitleX);
      arg.Get("titleY", figCfg.TitleY);
      arg.Get("minX", figCfg.MinX);
      arg.Get("maxX", figCfg.MaxX);
      arg.Get("minY", figCfg.MinY);
      arg.Get("maxY", figCfg.MaxY);
      std::vector<Lua_ArgType> lineArgs;
      if (false == arg.Get("lines", lineArgs)) {
        return luaL_error(L, "figure lines not provided");
      }
      for (auto &lineArg : lineArgs) {
        figure::LineConfig line;
        if (false == lineArg.Get("Name", line.Name)) {
          return luaL_error(L, "line name not provided for figure %s", figCfg.Name.c_str());
        }
        std::string ltype = "line";
        lineArg.Get("Type", ltype);
        if ("line" == ltype) {
          line.Type = figure::LineType::LINE;
        } else if ("spline" == ltype) {
          line.Type = figure::LineType::SPLINE;
        } else if ("scatter" == ltype) {
          line.Type = figure::LineType::SCATTER;
        } else if ("dot" == ltype) {
          line.Type = figure::LineType::DOT;
        } else {
          return luaL_error(L, "line type %s invalide for figure %s line %s", ltype.c_str(),
                            figCfg.Name.c_str(), line.Name.c_str());
        }
        figCfg.Lines.push_back(line);
      }

      auto topic = figure::Config();
      topic->Put(figCfg);
    } else {
      return luaL_error(L, "figure with invalid arguments");
    }
  } else {
    return luaL_error(L, "figure config with invalid number of arguments: %d", n);
  }
  return 1;
}

static int Figure_AddPoint(lua_State *L) {
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

    auto topic = figure::Line(figure, line);
    if (nullptr == topic) {
      return luaL_error(L, "%s %s not found", figure.c_str(), line.c_str());
    }

    figure::Point p = {x, y};
    topic->Put(p);
    if (topic->Size() >= 256) { /* NOTE: consume too slow or no consumer */
      topic->Get(p);
    }
  } else {
    return luaL_error(L, "figure config with invalid number of arguments: %d", n);
  }
  return 1;
}
/* ================================ [ FUNCTIONS ] ============================================== */
float Signal::Timing(void) {
  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed =
    std::chrono::duration_cast<std::chrono::milliseconds>(now - s_StartTimePoint).count();
  return (float)elapsed / 1000.0;
}

Signal::Signal(json &js, Message *msg) : m_Msg(msg), m_Lock(msg->GetLock()) {
  m_Name = js["Name"].get<std::string>();
  m_StartBit = js["Start"].get<uint32_t>();
  m_BitSize = js["Size"].get<uint32_t>();
  auto endian = js["Endian"].get<std::string>();
  if ("big" == endian) {
    m_Endian = Endian::BIG;
  } else {
    m_Endian = Endian::LITTLE;
  }
  m_Mask = (uint32_t)((1 << m_BitSize) - 1);
  LOG(INFO, "  signal %s start=%d size=%d %s endian\n", m_Name.c_str(), m_StartBit, m_BitSize,
      endian.c_str());
}

std::string Signal::Name(void) {
  return m_Name;
}

void Signal::Write(uint32_t value, bool isFromLua) {
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  m_Value = m_Mask & value;
  if (m_ViewMsgQ != nullptr) {
    if (m_LastValue != m_Value) { /* only need to push to the queue if there is a change */
      if (m_ViewMsgQ->Size() > 64) {
        m_ViewMsgQ->Clear();
      }
      ViewMsg vmsg = {Timing(), m_Value};
      m_ViewMsgQ->Put(vmsg);
    }
    m_LastValue = m_Value;
  }
  m_IsFromLua = isFromLua;
}

bool Signal::IsByLua(void) {
  bool ret;
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  ret = m_IsFromLua;
  m_IsFromLua = false;
  return ret;
}

uint32_t Signal::Read(void) {
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  return m_Value;
}

void Signal::CreateViewMsgQueue(void) {
  if (m_ViewMsgQ == nullptr) {
    m_ViewMsgQ = std::make_shared<MessageQueue<ViewMsg>>(Name());
  }
}

void Signal::DestroyViewMsgQueue(void) {
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  m_ViewMsgQ = nullptr;
}

std::vector<Signal::ViewMsg> Signal::View(void) {
  std::vector<ViewMsg> vmsgs;
  ViewMsg vmsg;

  bool r = true;
  while (r && m_ViewMsgQ) {
    r = m_ViewMsgQ->Get(vmsg, true, 0);
    if (r) {
      vmsgs.push_back(vmsg);
    }
  }

  if (0 == vmsgs.size()) {
    vmsg.Time = Timing();
    vmsg.Value = Read();
    vmsgs.push_back(vmsg);
  }

  return vmsgs;
}

uint32_t Signal::Start(void) {
  return m_StartBit;
}

uint32_t Signal::Size(void) {
  return m_BitSize;
}

Signal::Endian Signal::GetEndian(void) {
  return m_Endian;
}

Message *Signal::GetMessage(void) {
  return m_Msg;
}

Signal::~Signal() {
}

Message::Message(json &js, Network *network) {
  m_Network = network;
  m_Name = js["Name"].get<std::string>();
  m_Node = js["Node"].get<std::string>();
  m_Dlc = js["Dlc"].get<uint32_t>();
  m_Id = js["Id"].get<uint32_t>();
  m_Period = get<uint32_t>(js, "period", 1000);
  LOG(INFO, "messgae %s id=0x%x dlc=%d from node %s\n", m_Name.c_str(), m_Id, m_Dlc,
      m_Node.c_str());
  for (auto &cfg : js["Signals"]) {
    auto sig = std::make_shared<Signal>(cfg, this);
    m_Signals.push_back(sig);
  }

  if (Network::Type::CAN == network->GetType()) {
    /* for CAN, the me is the DUT(Device Under Test) */
    if (m_Node != network->Me()) {
      m_IsTx = true;
    }
  } else {
    /* for LIN, the me is the tester */
    if (m_Node == network->Me()) {
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

Network *Message::GetNetwork() {
  return m_Network;
}

std::string Message::Name(void) {
  return m_Name;
}

uint32_t Message::Id(void) {
  return m_Id;
}

uint32_t Message::Dlc(void) {
  return m_Dlc;
}

uint32_t Message::Length(void) {
  return sizeof(m_Buffer);
}

uint32_t Message::Period(void) {
  return m_Period;
}

void Message::SetPeriod(uint32_t period) {
  m_Period = period;
}

std::vector<std::shared_ptr<Signal>> &Message::Signals(void) {
  return m_Signals;
}

std::shared_ptr<Signal> Message::GetSignal(std::string name) {
  std::shared_ptr<Signal> sig = nullptr;

  for (auto s : m_Signals) {
    if (s->Name() == name) {
      sig = s;
      break;
    }
  }

  return sig;
}

std::recursive_mutex &Message::GetLock() {
  return m_Lock;
}

bool Message::IsTransmit(void) {
  return m_IsTx;
}

void Message::Run(void) {
  RunLua();

  if (m_IsTx) {
    if (m_Period > 0) {
      auto now = std::chrono::high_resolution_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_Timer);
      if (elapsed.count() >= m_Period) {
        m_Timer += std::chrono::milliseconds(m_Period);
        Trigger();
      }
    }
  } else {
    if (Network::Type::LIN == m_Network->GetType()) {
      if (m_Period > 0) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_Timer);
        if (elapsed.count() >= m_Period) {
          m_Timer += std::chrono::milliseconds(m_Period);
          Trigger();
        }
      }
    } else {
      Trigger();
    }
  }
}

bool Message::Trigger(void) {
  m_Requested.store(true);
  m_Network->Schedule();
  return true;
}

void Message::Call(std::string api, CallType callType) {
  std::vector<Lua_ArgType> outArgs;
  std::vector<Lua_ArgType> inArgs;
  if ((CALL_WITH_SIGNALS_RETURN_SIGNALS_AND_PERIOD == callType) ||
      (CALL_WITH_SIGNALS_RETURN_VOID == callType)) {
    inArgs.resize(1);
    inArgs[0].Type = LUA_ARG_TYPE_TABLE_STRING_UINT32;
    for (auto sig : m_Signals) {
      inArgs[0].StringUint32Map[sig->Name()] = sig->Read();
    }
  }

  if (CALL_WITH_SIGNALS_RETURN_SIGNALS_AND_PERIOD == callType) {
    outArgs.resize(2);
    outArgs[0].Type = LUA_ARG_TYPE_TABLE_STRING_UINT32; /* new values of the signals */
    outArgs[1].Type = LUA_ARG_TYPE_UINT32;              /* next period to call this exec again */
  }

  auto r = m_Script->Call(api, inArgs, outArgs);
  if (0 == r) {
    if (CALL_WITH_SIGNALS_RETURN_SIGNALS_AND_PERIOD == callType) {
      int index = 0;
      for (auto sig : m_Signals) {
        auto it = outArgs[0].StringUint32Map.find(sig->Name());
        if (it != outArgs[0].StringUint32Map.end()) {
          sig->Write(it->second, true);
        }
        index++;
      }
      m_LuaNextCycle = outArgs[1].U32;
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

void Message::CallbackToLua(std::string api, CallType callType) {
  if (nullptr != m_Script) {
    if (m_Script->HasApi(api)) {
      Call(api, callType);
    }
  }
}

void Message::StartLua(std::string scriptFile) {
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

void Message::RunLua(void) {
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  if (nullptr == m_Script) {
    return;
  }

  if (0 == m_LuaNextCycle) {
    m_LuaTimer = std::chrono::high_resolution_clock::now();
    Call("init");
  } else {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_LuaTimer);
    if (elapsed.count() >= m_LuaNextCycle) {
      m_LuaTimer += std::chrono::milliseconds(m_LuaNextCycle);
      Call("main");
    }
  }
}

std::string Message::LuaError(void) {
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  std::string err = m_LuaErrorMsg;
  m_LuaErrorMsg.clear();
  return err;
}

bool Message::IsLuaRunning(void) {
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  return m_Script != nullptr;
}

void Message::StopLua(void) {
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  m_Script = nullptr;
}

uint8_t *Message::Data() {
  return m_Buffer;
}

E2EExecutor *Message::E2E() {
  return m_E2E;
};

void Message::Backward(void) {
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  if (m_IsTx) {
    for (auto sig : m_Signals) {
      if (Signal::Endian::BIG == sig->GetEndian()) {
        Std_BitSetBigEndian(m_Buffer, sig->Read(), (uint16_t)sig->Start(), (uint8_t)sig->Size());
      } else {
        Std_BitSetLittleEndian(m_Buffer, sig->Read(), (uint16_t)sig->Start(), (uint8_t)sig->Size());
      }
    }
  }
}

std::shared_ptr<com::Message> Message::Trace(void) {
  std::shared_ptr<com::Message> msg = std::make_shared<com::Message>();
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  {
    char date[64];
    Std_GetDateTime(date, sizeof(date));
    msg->Timestamp = std::string(date);
  }

  msg->Name = Name();
  msg->Network = m_Network->Name();

  if (m_IsTx) {
    msg->Dir = "TX";
  } else {
    msg->Dir = "RX";
  }

  {
    std::stringstream ss;
    ss << "0x" << std::hex << (m_Id & (~CANL_ID_EXTENDED));
    msg->Id = ss.str();
  }

  {
    std::stringstream ss;
    ss << m_Dlc;
    msg->Dlc = ss.str();
  }

  {
    std::stringstream ss;
    for (uint32_t i = 0; i < m_Dlc; i++) {
      ss << std::setw(2) << std::setfill('0') << std::hex << (int)m_Buffer[i];
      if ((i + 1) < m_Dlc) {
        ss << " ";
      }
    }
    msg->Data = ss.str();
  }

  for (auto sig : m_Signals) {
    auto v = sig->Read();
    std::stringstream ss;
    ss << v << " ( 0x" << std::hex << v << " )";
    msg->Signals.push_back({sig->Name(), ss.str()});
  }

  return msg;
}

void Message::Forward(void) {
  uint32_t value;
  std::unique_lock<std::recursive_mutex> lck(m_Lock);
  if (false == m_IsTx) {
    for (auto sig : m_Signals) {
      if (Signal::Endian::BIG == sig->GetEndian()) {
        value = Std_BitGetBigEndian(m_Buffer, (uint16_t)sig->Start(), (uint8_t)sig->Size());
      } else {
        value = Std_BitGetLittleEndian(m_Buffer, (uint16_t)sig->Start(), (uint8_t)sig->Size());
      }
      sig->Write(value);
    }

    CallbackToLua("on_rx", CallType::CALL_WITH_SIGNALS_RETURN_VOID);
  }
}

Message::State Message::GetState() {
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
  m_Name = get<std::string>(js, "Name", "unknown");
  m_Device = get<std::string>(js, "Device", "simulator_v2");
  m_Me = get<std::string>(js, "Me", "AS");
  m_Port = get<uint32_t>(js, "Port", 0);
  m_Baudrate = get<uint32_t>(js, "Baudrate", 500000);
  auto type = get<std::string>(js, "Network", "CAN");
  if (type == "CAN") {
    m_Type = Type::CAN;
  } else if (type == "LIN") {
    m_Type = Type::LIN;
  } else {
    throw std::runtime_error("failed to create network with type " + type);
  }

  for (auto &cfg : js["Messages"]) {
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

std::shared_ptr<Signal> Network::Find(std::string name) {
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
    for (auto msg : it.second->Messages()) {
      if (false == msgName.empty()) {
        if (msg->Name() != msgName) {
          continue;
        }
      }
      sig = msg->GetSignal(sigName);
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

std::shared_ptr<Message> Network::FindMessage(std::string name) {
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
    msg = it.second->GetMessage(msgName);
    if (nullptr != msg) {
      break;
    }
  }

  return msg;
}

std::string Network::Name(void) {
  return m_Name;
}

std::string Network::Me(void) {
  return m_Me;
}

Network::Type Network::GetType(void) {
  return m_Type;
}

std::vector<std::shared_ptr<Message>> &Network::Messages(void) {
  return m_Messages;
}

std::shared_ptr<Message> Network::GetMessage(std::string name) {
  std::shared_ptr<Message> msg = nullptr;

  for (auto m : m_Messages) {
    if (m->Name() == name) {
      msg = m;
      break;
    }
  }

  return msg;
}

bool Network::Start(void) {
  bool ret = false;
  if (Type::CAN == m_Type) {
    m_Fd = CanL_Open(m_Device.c_str(), m_Port, m_Baudrate);
  } else {
    m_Fd = LinL_Open(m_Device.c_str(), m_Port, m_Baudrate);
  }

  if (m_Fd >= 0) {
    m_Stop = false;
    m_Thread = std::thread(&Network::Run, this);
    ret = true;
  }

  return ret;
}

void Network::Schedule(void) {
  m_CondVar.notify_one();
}

void Network::Run(void) {
  LOG(INFO, "network %s online: %s:%d:%d\n", m_Name.c_str(), m_Device.c_str(), m_Port, m_Baudrate);
  while (false == m_Stop) {
    for (auto msg : m_Messages) {
      msg->Run();
      auto state = msg->GetState();
      if (Message::State::TRANSMIT == state) {
        Transmit(msg);
      } else if (Message::State::RECEIVE == state) {
        Receive(msg);
      }
    }
    Network::RunLua();
    std::unique_lock<std::mutex> lck(m_Lock);
    m_CondVar.wait_for(lck, std::chrono::milliseconds(1));
  }
  LOG(INFO, "network %s offline\n", m_Name.c_str());
}

bool Network::Stop(void) {
  m_Stop = true;

  if (m_Thread.joinable()) {
    m_Thread.join();
  }

  if (m_Fd >= 0) {
    if (Type::CAN == m_Type) {
      CanL_Close(m_Fd);
    } else {
      LinL_Close(m_Fd);
    }
    m_Fd = -1;
  }

  std::unique_lock<std::mutex> lck(s_Lock);
  s_Networks.erase(m_Name);

  return true;
}

void Network::Transmit(std::shared_ptr<Message> msg) {
  bool r = true;
  auto &lock = msg->GetLock();
  std::unique_lock<std::recursive_mutex> lck(lock);
  msg->Backward();
  if (nullptr != msg->E2E()) {
    r = E2E_Execute(msg->E2E(), msg->Data(), (uint16_t)msg->Dlc());
  }
  if (true == r) {
    if (Type::CAN == m_Type) {
      r = CanL_Write(m_Fd, msg->Id(), (uint8_t)msg->Dlc(), msg->Data());
    } else {
      r = LinL_Write(m_Fd, msg->Id(), (uint8_t)msg->Dlc(), msg->Data(), true);
    }
  }

  if (false == r) {
    LOG(ERROR, "network %s transmit for message %s id=0x%x dlc=%d failed\n", m_Name.c_str(),
        msg->Name().c_str(), msg->Id(), msg->Dlc());
  } else {
    m_Pub->Push(msg->Trace());
    msg->CallbackToLua("on_tx", Message::CallType::CALL_VOID_RETURN_VOID);
    std::string api = "on_tx_" + m_Name + "_" + msg->Name();
    Network::CallbackToLua(api);
  }
}

void Network::Receive(std::shared_ptr<Message> msg) {
  bool r;
  if (Type::CAN == m_Type) {
    uint32_t canid = msg->Id();
    uint8_t dlc = msg->Length();
    r = CanL_Read(m_Fd, &canid, &dlc, msg->Data());
  } else {
    r = LinL_Read(m_Fd, msg->Id(), (uint8_t)msg->Dlc(), msg->Data(), true, 1000);
  }
  if (true == r) {
    if (nullptr != msg->E2E()) {
      r = E2E_Execute(msg->E2E(), msg->Data(), (uint16_t)msg->Dlc());
      if (false == r) {
        LOG(ERROR, "network %s receive message %s with invalid E2E\n", m_Name.c_str(),
            msg->Name().c_str());
      }
    }
  }

  if (true == r) {
    msg->Forward();
    m_Pub->Push(msg->Trace());
    std::string api = "on_rx_" + m_Name + "_" + msg->Name();
    Network::CallbackToLua(api);
  }
}

Network::~Network() {
  Stop();
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

    auto sig = Find(std::string(s));
    if (sig != nullptr) {
      sig->Write(value, true);
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

    auto sig = Find(std::string(s));
    if (sig != nullptr) {
      auto value = sig->Read();
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

    std::shared_ptr<Message> msg = FindMessage(std::string(s));
    if (msg != nullptr) {
      lua_pushboolean(L, msg->Trigger());
    } else {
      return luaL_error(L, "signal %s not found", s);
    }
  } else {
    return luaL_error(L, "set with invalid number of arguments: %d", n);
  }
  return 1;
}

int Network::CreateLuaInterface(lua_State *L) {
  luaL_newlib(L, s_LuaInterfaces);
  return 1;
}

void Network::StartLua(std::string scriptFile) {
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

void Network::CallbackToLua(std::string api) {
  std::unique_lock<std::mutex> lck(s_Lock);

  if (nullptr == s_Script) {
    return;
  }

  std::vector<Lua_ArgType> inArgs;
  std::vector<Lua_ArgType> outArgs;
  if (s_Script->HasApi(api)) {
    auto r = s_Script->Call(api, inArgs, outArgs);
    if (0 != r) {
      s_Script = nullptr;
      s_LuaErrorMsg = "callback '" + api + "' failed, error is " + std::to_string(r);
    }
  }
}

void Network::Call(std::string api) {
  std::vector<Lua_ArgType> inArgs;
  std::vector<Lua_ArgType> outArgs;
  outArgs.resize(1);
  outArgs[0].Type = LUA_ARG_TYPE_UINT32; /* next period to call this exec again */

  auto r = s_Script->Call(api, inArgs, outArgs);
  if (0 == r) {
    s_LuaNextCycle = outArgs[0].U32;
    if (0 == s_LuaNextCycle) {
      s_Script = nullptr;
    }
  } else {
    s_Script = nullptr;
    s_LuaErrorMsg = "call '" + api + "' failed, error is " + std::to_string(r);
  }
}

void Network::RunLua(void) {
  std::unique_lock<std::mutex> lck(s_Lock);

  if (nullptr == s_Script) {
    return;
  }
  if (0 == s_LuaNextCycle) {
    s_LuaTimer = std::chrono::high_resolution_clock::now();
    Call("init");
  } else {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_LuaTimer);
    if (elapsed.count() >= s_LuaNextCycle) {
      s_LuaTimer += std::chrono::milliseconds(s_LuaNextCycle);
      Call("main");
    }
  }
}

std::string Network::LuaError(void) {
  std::unique_lock<std::mutex> lck(s_Lock);
  std::string err = s_LuaErrorMsg;
  s_LuaErrorMsg.clear();
  return err;
}

bool Network::IsLuaRunning(void) {
  std::unique_lock<std::mutex> lck(s_Lock);
  return s_Script != nullptr;
}

void Network::StopLua(void) {
  std::unique_lock<std::mutex> lck(s_Lock);
  s_Script = nullptr;
}
} /* namespace as */
