/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef SIGNAL_HPP
#define SIGNAL_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <chrono>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include "MessageQueue.hpp"
#include "aslua.hpp"
#include "topic.hpp"

using json = nlohmann::ordered_json;
using namespace as::topic;
namespace as {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class E2EExecutor;

class Message;
class Signal {
public:
  enum Endian {
    BIG,
    LITTLE,
  };

  struct ViewMsg {
    float Time;
    uint32_t Value;
  };

public:
  Signal(json &js, Message *msg);
  ~Signal();

public:
  std::string Name(void);
  void Write(uint32_t value, bool isFromLua = false);
  uint32_t Read(void);
  uint32_t Start(void);
  uint32_t Size(void);
  Endian GetEndian(void);

  bool IsByLua(void);

  Message *GetMessage(void);

  void CreateViewMsgQueue(void);
  void DestroyViewMsgQueue(void);
  std::vector<ViewMsg> View(void);

  static float Timing(void);

private:
  Message *m_Msg;
  std::string m_Name;
  uint32_t m_StartBit;
  uint32_t m_BitSize;
  Endian m_Endian;
  bool m_IsFromLua = false; /* is value last updated by lua */

  std::recursive_mutex
    &m_Lock; /* to protect when to do destory of this queue, also protect the value */
  std::shared_ptr<MessageQueue<ViewMsg>> m_ViewMsgQ = nullptr;

  /* this tool only support basic types */
  uint32_t m_Value = 0;
  uint32_t m_LastValue = 0;
  uint32_t m_Mask;

  static std::chrono::high_resolution_clock::time_point s_StartTimePoint;
};

class Network;

class Message {
public:
  enum State {
    IDLE,
    TRANSMIT,
    RECEIVE
  };

public:
  Message(json &js, Network *network);
  ~Message();

public:
  std::vector<std::shared_ptr<Signal>> &Signals(void);
  std::string Name(void);
  uint32_t Id(void);
  uint32_t Dlc(void);
  uint32_t Length(void);
  uint32_t Period(void);
  void SetPeriod(uint32_t period);
  bool IsTransmit(void);
  Network *GetNetwork();
  void Backward(void);
  void Forward(void);
  std::shared_ptr<com::Message> Trace(void);
  State GetState();
  uint8_t *Data();
  E2EExecutor *E2E();
  void Run(void);

  /* trigger the tranmist or receive of this message */
  bool Trigger(void);

  void StartLua(std::string scriptFile);
  bool IsLuaRunning(void);
  std::string LuaError(void);
  void StopLua(void);

  enum CallType {
    CALL_WITH_SIGNALS_RETURN_SIGNALS_AND_PERIOD,
    CALL_WITH_SIGNALS_RETURN_VOID,
    CALL_VOID_RETURN_VOID
  };
  void CallbackToLua(std::string api, CallType callType);

  std::shared_ptr<Signal> GetSignal(std::string name);

  std::recursive_mutex &GetLock();

private:
  void RunLua(void);

  void Call(std::string api, CallType callType = CALL_WITH_SIGNALS_RETURN_SIGNALS_AND_PERIOD);

private:
  Network *m_Network;
  std::vector<std::shared_ptr<Signal>> m_Signals;

  std::string m_Name;
  std::string m_Node;
  uint32_t m_Id;
  uint32_t m_Dlc;
  uint32_t m_Period;

  uint8_t m_Buffer[64];
  bool m_IsTx = false;
  std::atomic<bool> m_Requested;

  E2EExecutor *m_E2E = nullptr;

  std::chrono::time_point<std::chrono::high_resolution_clock> m_Timer;

  std::recursive_mutex m_Lock;
  std::shared_ptr<AsLuaScript> m_Script = nullptr;
  std::chrono::time_point<std::chrono::high_resolution_clock> m_LuaTimer;
  std::string m_LuaErrorMsg;
  uint32_t m_LuaNextCycle;
};

class Network {

public:
  enum Type {
    CAN,
    LIN
  };

public:
  Network(json &js);
  ~Network();

public:
  std::string Name(void);
  std::string Me(void);
  Type GetType(void);
  std::vector<std::shared_ptr<Message>> &Messages(void);

  bool Start(void);
  void Run(void);
  bool Stop(void);

  void Schedule(void);

  std::shared_ptr<Message> GetMessage(std::string name);

  static std::shared_ptr<Message> FindMessage(std::string name);
  static std::shared_ptr<Signal> Find(std::string name);

  static void StartLua(std::string scriptFile);
  static bool IsLuaRunning(void);
  static std::string LuaError(void);
  static void StopLua(void);
  static void RunLua(void);
  static void CallbackToLua(std::string api);

private:
  void Transmit(std::shared_ptr<Message> msg);
  void Receive(std::shared_ptr<Message> msg);

private:
  std::string m_Name;
  std::string m_Device;
  std::string m_Me;
  uint32_t m_Port;
  uint32_t m_Baudrate;
  Type m_Type;
  int m_Fd = -1;
  bool m_Stop;
  std::thread m_Thread;
  std::vector<std::shared_ptr<Message>> m_Messages;

private:
  std::shared_ptr<com::Publisher> m_Pub = nullptr;

  std::mutex m_Lock;
  std::condition_variable m_CondVar;

  static std::mutex s_Lock;
  static std::map<std::string, Network *> s_Networks;
  static const luaL_Reg s_LuaInterfaces[];

  static std::shared_ptr<AsLuaScript> s_Script;
  static std::chrono::time_point<std::chrono::high_resolution_clock> s_LuaTimer;
  static std::string s_LuaErrorMsg;
  static uint32_t s_LuaNextCycle;

private:
  static int Set(lua_State *L);
  static int Get(lua_State *L);
  static int Trigger(lua_State *L);

  static void Call(std::string api);

public:
  static int CreateLuaInterface(lua_State *L);
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace as */
#endif /* SIGNAL_HPP */
