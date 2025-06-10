/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef __SIGNAL_HPP__
#define __SIGNAL_HPP__
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
    float time;
    uint32_t value;
  };

public:
  Signal(json &js, Message *msg);
  ~Signal();

public:
  std::string name(void);
  void write(uint32_t value, bool byLua = false);
  uint32_t read(void);
  uint32_t start(void);
  uint32_t size(void);
  Endian endian(void);

  bool bylua(void);

  Message *message(void);

  void createViewMsgQueue(void);
  void destoryViewMsgQueue(void);
  std::vector<ViewMsg> view(void);

  static float timimg(void);

private:
  Message *m_Msg;
  std::string m_Name;
  uint32_t m_StartBit;
  uint32_t m_BitSize;
  Endian m_Endian;
  bool m_byLua = false; /* is value last updated by lua */

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
  std::string name(void);
  uint32_t id(void);
  uint32_t dlc(void);
  uint32_t length(void);
  uint32_t period(void);
  void set_period(uint32_t period);
  bool IsTransmit(void);
  Network *network();
  void backward(void);
  void forward(void);
  std::shared_ptr<com::Message> trace(void);
  State state();
  uint8_t *data();
  E2EExecutor *E2E();
  void run(void);

  /* trigger the tranmist or receive of this message */
  bool trigger(void);

  void startLua(std::string scriptFile);
  bool isLuaRunning(void);
  std::string luaError(void);
  void stopLua(void);

  enum CallType {
    CALL_WITH_SIGNALS_RETURN_SIGNALS_AND_PERIOD,
    CALL_WITH_SIGNALS_RETURN_VOID,
    CALL_VOID_RETURN_VOID
  };
  void callbackToLua(std::string api, CallType callType);

  std::shared_ptr<Signal> signal(std::string name);

  std::recursive_mutex &getLock();

private:
  void runLua(void);

  void call(std::string api, CallType callType = CALL_WITH_SIGNALS_RETURN_SIGNALS_AND_PERIOD);

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
  std::string name(void);
  std::string me(void);
  Type type(void);
  std::vector<std::shared_ptr<Message>> &messages(void);

  bool start(void);
  void run(void);
  bool stop(void);

  void schedule(void);

  std::shared_ptr<Message> message(std::string name);

  static std::shared_ptr<Message> find_message(std::string name);
  static std::shared_ptr<Signal> find(std::string name);

  static void startLua(std::string scriptFile);
  static bool isLuaRunning(void);
  static std::string luaError(void);
  static void stopLua(void);
  static void runLua(void);
  static void callbackToLua(std::string api);

private:
  void transmit(std::shared_ptr<Message> msg);
  void receive(std::shared_ptr<Message> msg);

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
  static const luaL_Reg s_LuaInferfaces[];

  static std::shared_ptr<AsLuaScript> s_Script;
  static std::chrono::time_point<std::chrono::high_resolution_clock> s_LuaTimer;
  static std::string s_LuaErrorMsg;
  static uint32_t s_LuaNextCycle;

private:
  static int Set(lua_State *L);
  static int Get(lua_State *L);
  static int Trigger(lua_State *L);

  static void call(std::string api);

public:
  static int CreateLuaInterface(lua_State *L);
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace as */
#endif /* __SIGNAL_HPP__ */
