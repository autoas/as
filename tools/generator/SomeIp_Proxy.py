# SSAS - Simple Smart Automotive Software
# Copyright (C) 2026 Parai Wang <parai@foxmail.com>

import os
from .helper import *
from .SomeIpXf import *

__all__ = ["Gen_SomeIpProxy"]


def GetArgs(cfg, name):
    for args in cfg.get("args", []):
        if name == args["name"]:
            return args["args"]
    raise Exception(f"args {name} not found")


def toMethodTypeName(type_name, method):
    if type_name.startswith(method["name"]):
        return type_name[len(method["name"]) :]
    return type_name


def Gen_SomeIpProxy(cfg, service, dir, source):
    allStructs = GetStructs(cfg)
    service_name = service["name"]
    H = open("%s/%sProxy.hpp" % (dir, service_name), "w")
    source["%sProxy" % (service_name)] = ["%s/%sProxy.cpp" % (dir, service_name)]
    GenHeader(H)
    H.write("#ifndef ARA_SOMEIP_%s_PROXY_HPP\n" % (toMacro(service_name)))
    H.write("#define ARA_SOMEIP_%s_PROXY_HPP\n" % (toMacro(service_name)))
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write('#include "ara/com/proxy/proxy_base.h"\n')
    H.write('#include "SomeIpXf_Cfg.h"\n')
    H.write('#include "SomeIp.h"\n')
    H.write("namespace ara {\n")
    H.write("namespace com {\n")
    H.write(f"namespace {service_name} {{\n")
    H.write("using namespace ara::com;\n")
    H.write("using namespace ara::core;\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    for struct in cfg.get("structs", []):
        H.write(f"using {struct['name']} = {struct['name']}_Type;\n")
    H.write(f"class {service_name}Manager;\n")
    H.write("/* ================================ [ CLASS     ] ============================================== */\n")
    H.write("namespace events {\n")
    def_events = ""
    def_methods = ""
    for eg in service.get("event-groups", []):
        for event in eg["events"]:
            def_events += f"  events::{event['name']} {event['name']};\n"
            H.write(
                f"""
class {event['name']} {{
public:
  using SampleType = {event['type']}_Type;

  {event['name']}();
  ~{event['name']}();

  ara::core::Result<void> Subscribe(size_t maxSampleCount);

  ara::com::SubscriptionState GetSubscriptionState() const;

  void Unsubscribe();

  size_t GetFreeSampleCount() const noexcept;

  ara::core::Result<void> SetReceiveHandler(ara::com::EventReceiveHandler handler) {{
    m_evtRxHandler = handler;
    return ara::core::Result<void>(0);
  }}

  ara::core::Result<void> UnsetReceiveHandler() {{
    m_evtRxHandler = nullptr;
    return ara::core::Result<void>(0);
  }}

  ara::core::Result<void>
  SetSubscriptionStateChangeHandler(ara::com::SubscriptionStateChangeHandler handler) {{
    m_stateChgHandler = handler;
    return ara::core::Result<void>(0);
  }}

  void UnsetSubscriptionStateChangeHandler() {{
    m_stateChgHandler = nullptr;
  }}

  template <typename F>
  ara::core::Result<size_t>
  GetNewSamples(F &&f, size_t maxNumberOfSamples = std::numeric_limits<size_t>::max()) {{
    ara::core::Result<size_t> rslt(0);
    size_t count = 0;
    for (size_t i = 0; i < maxNumberOfSamples; i++) {{
      if (false == m_lastNSamples.empty()) {{
        SamplePtr<SampleType> samplePtr(std::move(m_lastNSamples.front()));
        m_lastNSamples.pop_front();
        f(std::move(samplePtr));
        count++;
      }} else {{
        break;
      }}
    }}

    rslt = ara::core::Result<size_t>(count);

    return rslt;
  }}

private:
  void OnEvent(const SampleType &sample);
  void OnSubscribeAck(bool isSubscribe);
  friend class {service_name}::{service_name}Manager;

private:
  std::deque<SamplePtr<SampleType>> m_lastNSamples;
  ara::com::EventReceiveHandler m_evtRxHandler = nullptr;
  ara::com::SubscriptionStateChangeHandler m_stateChgHandler = nullptr;
  size_t m_maxSampleCount = 0; 
}};\n\n"""
            )
    H.write("} // namespace events\n\n")
    H.write("namespace methods {\n")
    for method in service.get("methods", []):
        UsedTypes = []
        ReturnType = method.get("return", "void")
        if ReturnType != "void":
            UsedTypes.append(ReturnType)
        isTp = method.get("tp", False)
        defTpCopyFnc = ""
        defTpBuf = ""
        if isTp:
            defTpCopyFnc = f"""
  Std_ReturnType OnTpCopyRxData(uint32_t requestId, SomeIp_TpMessageType *msg);
  Std_ReturnType OnTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg);"""
            defTpBuf = f"  uint8_t m_response[{GetStructPayloadSize(allStructs[ReturnType], allStructs)}];\n  bool m_requestInUse = false;"
        payloadSize = 0
        Args = []
        if "args" in method:
            args = GetArgs(cfg, method["args"])
            for arg in args:
                Args.append(f"const {toMethodTypeName(arg['type'], method)} &{arg['name']}")
                if arg["type"] not in UsedTypes:
                    UsedTypes.append(arg["type"])
                payloadSize += GetStructPayloadSize(allStructs[arg["type"]], allStructs)
        def_methods += f"  methods::{method['name']} {method['name']};\n"
        used_types = ""
        for utype in UsedTypes:
            used_types += f"  using {toMethodTypeName(utype, method)} = {utype}_Type;\n"
        H.write(
            f"""
class {method['name']} {{
public:
{used_types}

  ara::core::Future<{toMethodTypeName(ReturnType, method)}> operator()({",".join(Args)});

  {method['name']}();
  ~{method['name']}();

private:
  Std_ReturnType OnError(uint32_t requestId, Std_ReturnType ercd);
  Std_ReturnType OnResponse(uint32_t requestId, SomeIp_MessageType *res);
{defTpCopyFnc}
  friend class {service_name}::{service_name}Manager;

private:
  std::shared_ptr<ara::core::Promise<{ReturnType}>> m_promise = nullptr;
  uint16_t m_sessionId = 0;
  uint8_t m_request[{payloadSize}];
  {toMethodTypeName(ReturnType, method)} m_{toMethodTypeName(ReturnType, method)};
{defTpBuf}
}};\n"""
        )
    H.write("} // namespace methods\n")
    H.write(
        f"""
class {service_name}Proxy {{
public:
  class HandleType {{
  public:
    inline bool operator==(const HandleType &other) const;
    const ara::com::InstanceIdentifier &GetInstanceId() const;
    HandleType();
    ~HandleType();

  private:
    const uint16_t m_clientId;
    const uint16_t m_serviceId;
  }};

  /** @SWS_CM_00123 */
  static ara::core::Result<ara::com::FindServiceHandle>
  StartFindService(ara::com::FindServiceHandler<HandleType> handler,
                   ara::com::InstanceIdentifier instanceId) noexcept;

  /** @SWS_CM_00623 */
  static ara::core::Result<ara::com::FindServiceHandle>
  StartFindService(ara::com::FindServiceHandler<HandleType> handler,
                   ara::core::InstanceSpecifier instanceSpec);

  /** @SWS_CM_11365 */
  template <typename ExecutorT>
  static ara::com::FindServiceHandle
  StartFindService(ara::com::FindServiceHandler<HandleType> handler,
                   ara::core::InstanceSpecifier instance, ExecutorT &&executor) noexcept;

  /** @SWS_CM_11352 */
  template <typename ExecutorT>
  static ara::com::FindServiceHandle
  StartFindService(ara::com::FindServiceHandler<HandleType> handler,
                   ara::com::InstanceIdentifier instance, ExecutorT &&executor) noexcept;

  /** @SWS_CM_00125 */
  static void StopFindService(ara::com::FindServiceHandle handle) noexcept;

  /** @SWS_CM_00122 */
  static ara::core::Result<ara::com::ServiceHandleContainer<HandleType>>
  FindService(ara::com::InstanceIdentifier instance) noexcept;

  /** @SWS_CM_00622 */
  static ara::core::Result<ara::com::ServiceHandleContainer<HandleType>>
  FindService(ara::core::InstanceSpecifier instanceSpec);

  explicit {service_name}Proxy(HandleType &handle);

  {service_name}Proxy({service_name}Proxy &other) = delete;

  {service_name}Proxy &operator=(const {service_name}Proxy &other) = delete;

public:
{def_events}

{def_methods}

private:
  HandleType &m_handle;
}};\n"""
    )
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write(f"}} // namespace {service_name}\n")
    H.write("} // namespace com\n")
    H.write("} // namespace ara\n")
    H.write("#endif /* ARA_SOMEIP_%s_PROXY_HPP */\n" % (toMacro(service_name)))
    H.close()
    C = open("%s/%sProxy.cpp" % (dir, service_name), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write(f'#include "{service_name}Proxy.hpp"\n')
    C.write('#include "Std_Types.h"\n')
    C.write('#include "Std_Debug.h"\n')
    C.write('#include "Sd.h"\n')
    C.write('#include "SomeIp_Cfg.h"\n')
    C.write('#include "Sd_Cfg.h"\n')
    C.write("namespace ara {\n")
    C.write("namespace com {\n")
    C.write(f"namespace {service_name} {{\n")
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write(f"#define AS_LOG_{toMacro(service_name)} 0\n")
    C.write(f"#define AS_LOG_{toMacro(service_name)}_E 2\n")
    C.write("#define SOMEIP_SF_MAX 1396\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ CLASS     ] ============================================== */\n")
    EventGroupInit = ""
    DecApis = ""
    DecVars = ""
    for eg in service.get("event-groups", []):
        EventGroupInit += f"""
    m_eventGroupInfoMap[SD_CONSUMED_EVENT_GROUP_{toMacro(service_name)}_{toMacro(eg['name'])}] = {{
      0,
      SubscriptionState::kNotSubscribed,
    }};\n"""
        for event in eg.get("events", []):
            DecVars += f"  events::{event['name']} *m_h{event['name']} = nullptr;\n"
            DecApis += f"""
  void Register(events::{event['name']} *h{event['name']}) {{
    std::lock_guard<std::mutex> lck(m_lock);
    if (nullptr == m_h{event['name']}) {{
      m_h{event['name']} = h{event['name']};
    }} else {{
      throw std::runtime_error("{event['name']} already registerred");
    }}
  }}

  void UnRegister(events::{event['name']} *h{event['name']}) {{
    std::lock_guard<std::mutex> lck(m_lock);
    if (h{event['name']} == m_h{event['name']}) {{
      m_h{event['name']} = nullptr;
    }} else {{
      throw std::runtime_error("incorrect {event['name']} when do unregister");
    }}
  }}
  
  void On{event['name']}(const events::{event['name']}::SampleType &sample) {{
    if (nullptr != m_h{event['name']}) {{
      m_h{event['name']}->OnEvent(sample);
    }}
  }}

  void On{event['name']}SubscribeAck(bool isSubscribe) {{
    if (nullptr != m_h{event['name']}) {{
      m_h{event['name']}->OnSubscribeAck(isSubscribe);
    }}
  }}\n"""
    for method in service.get("methods", []):
        defTpCopyFnc = ""
        if method.get("tp", False):
            defTpCopyFnc = f"""
Std_ReturnType On{method['name']}TpCopyRxData(uint32_t requestId, SomeIp_TpMessageType *msg){{
    Std_ReturnType ret = E_OK;

    if (nullptr != m_h{method['name']}) {{
      m_h{method['name']}->OnTpCopyRxData(requestId, msg);
    }} else {{
      ret = E_NOT_OK;
    }}

    return ret;
  }}

Std_ReturnType On{method['name']}TpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg){{
  Std_ReturnType ret = E_OK;

  if (nullptr != m_h{method['name']}) {{
    m_h{method['name']}->OnTpCopyTxData(requestId, msg);
  }} else {{
    ret = E_NOT_OK;
  }}

  return ret;
}}"""
        DecVars += f"  methods::{method['name']} *m_h{method['name']} = nullptr;\n"
        DecApis += f"""
  void Register(methods::{method['name']} *h{method['name']}) {{
    std::lock_guard<std::mutex> lck(m_lock);
    if (nullptr == m_h{method['name']}) {{
      m_h{method['name']} = h{method['name']};
    }} else {{
      throw std::runtime_error("{method['name']} already registerred");
    }}
  }}

  void UnRegister(methods::{method['name']} *h{method['name']}) {{
    std::lock_guard<std::mutex> lck(m_lock);
    if (h{method['name']} == m_h{method['name']}) {{
      m_h{method['name']} = nullptr;
    }} else {{
      throw std::runtime_error("incorrect {method['name']} when do unregister");
    }}
  }}

  Std_ReturnType On{method['name']}Error(uint32_t requestId, Std_ReturnType ercd) {{
    Std_ReturnType ret = E_OK;

    if (nullptr != m_h{method['name']}) {{
      m_h{method['name']}->OnError(requestId, ercd);
    }} else {{
      ret = E_NOT_OK;
    }}

    return ret;
  }}

{defTpCopyFnc}

  Std_ReturnType On{method['name']}Response(uint32_t requestId, SomeIp_MessageType *res) {{
    Std_ReturnType ret = E_OK;

    if (nullptr != m_h{method['name']}) {{
      m_h{method['name']}->OnResponse(requestId, res);
    }} else {{
      ret = E_NOT_OK;
    }}

    return ret;
  }}\n"""
    C.write(
        f"""
class {service_name}Manager {{
public:
  {service_name}Manager() {{
{EventGroupInit}
  }}

  ~{service_name}Manager() {{
  }}
{DecApis}

  Result<ServiceHandleContainer<{service_name}Proxy::HandleType>>
  FindService(uint16 ClientServiceHandleId) {{
    Result<ServiceHandleContainer<{service_name}Proxy::HandleType>> rslt({{}});
    Std_ReturnType ret;
    ret = Sd_ClientServiceSetState(ClientServiceHandleId, SD_CLIENT_SERVICE_REQUESTED);
    if (E_OK == ret) {{
      if (true == m_avaiable) {{
        ServiceHandleContainer<{service_name}Proxy::HandleType> shc;
        shc.push_back({service_name}Proxy::HandleType());
        rslt = Result<ServiceHandleContainer<{service_name}Proxy::HandleType>>(shc);
      }}
    }} else {{
      rslt = Result<ServiceHandleContainer<{service_name}Proxy::HandleType>>(ret);
    }}

    return rslt;
  }}

  Std_ReturnType Subscribe(uint16_t SdConsumedEventGroupHandleId) {{
    Std_ReturnType ret = E_OK;

    std::lock_guard<std::mutex> lck(m_lock);
    auto it = m_eventGroupInfoMap.find(SdConsumedEventGroupHandleId);
    if (m_eventGroupInfoMap.end() != it) {{
      if (0 == it->second.ref) {{
        ret = Sd_ConsumedEventGroupSetState(SdConsumedEventGroupHandleId,
                                            SD_CONSUMED_EVENTGROUP_REQUESTED);
      }}
      it->second.ref++;
    }} else {{
      ret = E_NOT_OK;
    }}
    return ret;
  }}

  Std_ReturnType Unsubscribe(uint16_t SdConsumedEventGroupHandleId) {{
    Std_ReturnType ret = E_OK;
    std::lock_guard<std::mutex> lck(m_lock);
    auto it = m_eventGroupInfoMap.find(SdConsumedEventGroupHandleId);
    if (m_eventGroupInfoMap.end() != it) {{
      if (it->second.ref > 0) {{
        it->second.ref--;
      }}
      if (0 == it->second.ref) {{
        ret = Sd_ConsumedEventGroupSetState(SdConsumedEventGroupHandleId,
                                            SD_CONSUMED_EVENTGROUP_RELEASED);
      }}
    }} else {{
      ret = E_NOT_OK;
    }}
    return ret;
  }}

  SubscriptionState GetSubscriptionState(uint16_t SdConsumedEventGroupHandleId) {{
    SubscriptionState state = SubscriptionState::kNotSubscribed;
    Sd_ConsumedEventGroupCurrentStateType ConsumedEventGroupCurrentState;
    Std_ReturnType ret;

    std::lock_guard<std::mutex> lck(m_lock);
    auto it = m_eventGroupInfoMap.find(SdConsumedEventGroupHandleId);
    if (m_eventGroupInfoMap.end() != it) {{
      ret = Sd_ConsumedEventGroupGetState(SdConsumedEventGroupHandleId,
                                          &ConsumedEventGroupCurrentState);
      if (E_OK == ret) {{
        if (SD_CONSUMED_EVENTGROUP_AVAILABLE == ConsumedEventGroupCurrentState) {{
          state = SubscriptionState::kSubscribed;
        }} else {{
          if (it->second.ref > 0) {{
            state = SubscriptionState::kSubscriptionPending;
          }}
        }}
      }}
    }}

    return state;
  }}

  void OnAvailability(boolean isAvailable) {{
    m_avaiable = isAvailable;
  }}

  static std::shared_ptr<{service_name}Manager> GetInstance();

private:
  struct EventGroupInfo {{
    int ref;
    SubscriptionState state;
  }};

private:
  bool m_avaiable = false;
  std::mutex m_lock;
  std::unordered_map<uint16_t, EventGroupInfo> m_eventGroupInfoMap;

{DecVars}

private:
  static std::mutex s_lock;
  static std::shared_ptr<{service_name}Manager> s_{service_name}Manager;
}};\n"""
    )
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    C.write(f"std::mutex {service_name}Manager::s_lock;\n")
    C.write(f"std::shared_ptr<{service_name}Manager> {service_name}Manager::s_{service_name}Manager;\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.write(
        f"""std::shared_ptr<{service_name}Manager> {service_name}Manager::GetInstance() {{

  std::lock_guard<std::mutex> lck(s_lock);
  if (nullptr == s_{service_name}Manager) {{
    s_{service_name}Manager = std::make_shared<{service_name}Manager>();
  }}

  return s_{service_name}Manager;
}}\n"""
    )
    C.write("namespace events {\n")
    for eg in service.get("event-groups", []):
        for event in eg.get("events", []):
            C.write(
                f"""
{event['name']}::{event['name']}() {{
  {service_name}Manager::GetInstance()->Register(this);
}}

{event['name']}::~{event['name']}() {{
  {service_name}Manager::GetInstance()->UnRegister(this);
}}

Result<void> {event['name']}::Subscribe(size_t maxSampleCount) {{
  Result<void> rslt(0);
  Std_ReturnType ret;

  ret = {service_name}Manager::GetInstance()->Subscribe(SD_CONSUMED_EVENT_GROUP_{toMacro(service_name)}_{toMacro(eg['name'])});
  if (E_OK == ret) {{
    m_maxSampleCount = maxSampleCount;
  }} else {{
    rslt = Result<void>(ret);
  }}
  return rslt;
}}

void {event['name']}::Unsubscribe() {{
  (void){service_name}Manager::GetInstance()->Unsubscribe(
    SD_CONSUMED_EVENT_GROUP_{toMacro(service_name)}_{toMacro(eg['name'])});
}}

SubscriptionState {event['name']}::GetSubscriptionState() const {{
  return {service_name}Manager::GetInstance()->GetSubscriptionState(
    SD_CONSUMED_EVENT_GROUP_{toMacro(service_name)}_{toMacro(eg['name'])});
}}

void {event['name']}::OnEvent(const SampleType &sample) {{
  SamplePtr<SampleType> samplePtr(new SampleType);
  *samplePtr = sample;
  m_lastNSamples.push_back(std::move(samplePtr));
  if (m_lastNSamples.size() > m_maxSampleCount) {{
    m_lastNSamples.pop_front();
  }}
  if (nullptr != m_evtRxHandler) {{
    m_evtRxHandler();
  }}
}}

void {event['name']}::OnSubscribeAck(bool isSubscribe) {{
  if (nullptr != m_stateChgHandler) {{
    if (true == isSubscribe) {{
      m_stateChgHandler(SubscriptionState::kSubscribed);
    }} else {{
      m_stateChgHandler(SubscriptionState::kNotSubscribed);
    }}
  }}
}}\n"""
            )
    C.write("} // namespace events\n")
    C.write("namespace methods {\n")
    for method in service.get("methods", []):
        ReturnType = method.get("return", "void")
        Args = []
        if "args" in method:
            args = GetArgs(cfg, method["args"])
            serArgs = ""
            for arg in args:
                Args.append(f"const {toMethodTypeName(arg['type'], method)} &{arg['name']}")
                serArgs += f"""
    if ((offset >= 0) && ((int32_t)sizeof(m_request) > offset)) {{
      serializedSize = SomeIpXf_EncodeStruct(m_request + offset, sizeof(m_request) - offset,
                                &{arg['name']}, &SomeIpXf_Struct{arg['type']}Def);
      if (serializedSize > 0) {{
        offset += serializedSize;
      }} else {{
        offset = -ENOSPC;
      }}
    }}
"""
        if method.get("tp", False):
            C.write(
                f"""
Std_ReturnType {method['name']}::OnTpCopyRxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  Std_ReturnType ret = E_OK;
  if ((NULL != msg) && ((msg->offset + msg->length)) <= sizeof(m_response)) {{
    memcpy(&m_response[msg->offset], msg->data, msg->length);
    if (false == msg->moreSegmentsFlag) {{
      msg->data = m_response;
    }}
  }} else {{
    ret = E_NOT_OK;
  }}
  return ret;
}}

Std_ReturnType {method['name']}::OnTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  Std_ReturnType ret = E_OK;
  if ((NULL != msg) && ((msg->offset + msg->length) <= sizeof(m_request))) {{
    memcpy(msg->data, &m_request[msg->offset], msg->length);
    if (false == msg->moreSegmentsFlag) {{
      m_requestInUse = false;
    }}
  }} else {{
    ret = E_NOT_OK;
    m_requestInUse = false;
  }}
  return ret;
}}

ara::core::Future<{method['name']}::{toMethodTypeName(ReturnType, method)}> {method['name']}::operator()({",".join(Args)}) {{
  Future<{method['name']}::{toMethodTypeName(ReturnType, method)}> future;
  int32_t offset = 0;
  int32_t serializedSize;
  if ((false == m_requestInUse) && (nullptr == m_promise)) {{
    m_promise = std::make_shared<Promise<{method['name']}::{toMethodTypeName(ReturnType, method)}>>();
    future = m_promise->get_future();
    uint32_t requestId = ((uint32_t)SOMEIP_TX_METHOD_{toMacro(service_name)}_{toMacro(method['name'])} << 16) + (++m_sessionId);
{serArgs}
    if (offset >= 0) {{
      Std_ReturnType ret = SomeIp_Request(requestId, m_request, offset);
      if (E_OK != ret) {{
        m_promise->SetError(ret);
        m_promise = nullptr;
      }} else {{
        if (offset > SOMEIP_SF_MAX) {{
          m_requestInUse = true;
        }}
      }}
    }} else {{
      m_promise->SetError(-EINVAL);
      m_promise = nullptr;
    }}
  }} else {{
    Promise<{method['name']}::{toMethodTypeName(ReturnType, method)}> promise;
    future = promise.get_future();
    promise.SetError(-EBUSY);
  }}

  return future;
}}\n"""
            )
        else:
            C.write(
                f"""
ara::core::Future<{method['name']}::{toMethodTypeName(ReturnType, method)}> {method['name']}::operator()({",".join(Args)}) {{
  m_promise = std::make_shared<Promise<{method['name']}::{toMethodTypeName(ReturnType, method)}>>();
  Future<{method['name']}::{toMethodTypeName(ReturnType, method)}> future = m_promise->get_future();
  int32_t offset = 0;
  int32_t serializedSize;

  if (nullptr == m_promise) {{
    m_promise = std::make_shared<Promise<{method['name']}::{toMethodTypeName(ReturnType, method)}>>();
    future = m_promise->get_future();
    uint32_t requestId = ((uint32_t)SOMEIP_TX_METHOD_{toMacro(service_name)}_{toMacro(method['name'])} << 16) + (++m_sessionId);
{serArgs}
    if (offset >= 0) {{
      Std_ReturnType ret = SomeIp_Request(requestId, m_request, offset);
      if (E_OK != ret) {{
        m_promise->SetError(ret);
        m_promise = nullptr;
      }}
    }} else {{
      m_promise->SetError(-EINVAL);
      m_promise = nullptr;
    }}
  }} else {{
    Promise<{method['name']}::{toMethodTypeName(ReturnType, method)}> promise;
    future = promise.get_future();
    promise.SetError(-EBUSY);
  }}

  return future;
}}\n"""
            )
        C.write(
            f"""
{method['name']}::{method['name']}() {{
  {service_name}Manager::GetInstance()->Register(this);
}}

{method['name']}::~{method['name']}() {{
  {service_name}Manager::GetInstance()->UnRegister(this);
}}

Std_ReturnType {method['name']}::OnError(uint32_t requestId, Std_ReturnType ercd) {{
  if (nullptr != m_promise) {{
    m_promise->SetError(ercd);
    m_promise = nullptr;
  }} else {{
    ASLOG({toMacro(service_name)}_E, ("no promise for {method['name']} error %d\\n", ercd));
  }}
  return E_OK;
}}

Std_ReturnType {method['name']}::OnResponse(uint32_t requestId, SomeIp_MessageType *res) {{
  Std_ReturnType ret = E_OK;
  if (nullptr != m_promise) {{
    int32_t serializedSize =
      SomeIpXf_DecodeStruct(res->data, res->length, &m_{toMethodTypeName(ReturnType, method)}, &SomeIpXf_Struct{ReturnType}Def);
    if (serializedSize > 0) {{
      m_promise->set_value(m_{toMethodTypeName(ReturnType, method)});
    }} else {{
      ASLOG({toMacro(service_name)}_E, ("malformed {method['name']} response {toMethodTypeName(ReturnType, method)}\\n"));
      m_promise->SetError(-EINVAL);
      ret = E_NOT_OK;
    }}
    m_promise = nullptr;
  }} else {{
    ASLOG({toMacro(service_name)}_E, ("no promise for {method['name']} response\\n"));
  }}

  return ret;
}}
\n"""
        )
    C.write("} // namespace methods\n")
    C.write(
        f"""
{service_name}Proxy::HandleType::HandleType()
  : m_clientId(SOMEIP_CSID_{toMacro(service_name)}), m_serviceId(SD_CLIENT_SERVICE_HANDLE_ID_{toMacro(service_name)}) {{
}}

{service_name}Proxy::HandleType::~HandleType() {{
}}

{service_name}Proxy::{service_name}Proxy(HandleType &handle) : m_handle(handle) {{
}}

Result<ServiceHandleContainer<{service_name}Proxy::HandleType>>
{service_name}Proxy::FindService(InstanceSpecifier instanceSpec) {{
  return {service_name}Manager::GetInstance()->FindService(SD_CLIENT_SERVICE_HANDLE_ID_{toMacro(service_name)});
}}\n"""
    )
    C.write(f"}} // namespace {service_name}\n")
    C.write("} // namespace com\n")
    C.write("} // namespace ara\n")
    C.write(f"using namespace ara::com::{service_name};\n")
    C.write('extern "C" {\n')
    C.write(
        f"""void SomeIp_{service_name}_OnAvailability(boolean isAvailable) {{
  ASLOG({toMacro(service_name)}, ("%s\\n", isAvailable ? "online" : "offline"));
  {service_name}Manager::GetInstance()->OnAvailability(isAvailable);
}}\n"""
    )
    for method in service.get("methods", []):
        C.write(
            f"""
Std_ReturnType SomeIp_{service_name}_{method['name']}_OnResponse(uint32_t requestId, SomeIp_MessageType *res) {{
  ASLOG({toMacro(service_name)}, ("{method['name']} OnResponse %X: len=%d, data=[%02X %02X %02X %02X ...]\\n", requestId,
                        res->length, res->data[0], res->data[1], res->data[2], res->data[3]));
  return {service_name}Manager::GetInstance()->On{method['name']}Response(requestId, res);
}}

Std_ReturnType SomeIp_{service_name}_{method['name']}_OnError(uint32_t requestId, Std_ReturnType ercd) {{
  ASLOG({toMacro(service_name)}, ("{method['name']} OnError %X: %d\\n", requestId, ercd));
  return {service_name}Manager::GetInstance()->On{method['name']}Error(requestId, ercd);
}}\n"""
        )
        if method.get("tp", False):
            C.write(
                f"""
Std_ReturnType SomeIp_{service_name}_{method['name']}_OnTpCopyRxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
    return {service_name}Manager::GetInstance()->On{method['name']}TpCopyRxData(requestId, msg);
}}

Std_ReturnType SomeIp_{service_name}_{method['name']}_OnTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  return {service_name}Manager::GetInstance()->On{method['name']}TpCopyTxData(requestId, msg);
}}\n"""
            )
    for eg in service.get("event-groups", []):
        for event in eg.get("events", []):
            isTp = event.get("tp", False)
            if isTp:
                C.write(
                    f"""
Std_ReturnType SomeIp_RadarService_Object_BrakeEvent_OnTpCopyRxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  Std_ReturnType ret = E_OK;
  static uint8_t payload[{GetStructPayloadSize(allStructs[event['type']], allStructs)}];
  if ((NULL != msg) && ((msg->offset + msg->length) <= sizeof(payload))) {{
    memcpy(&payload[msg->offset], msg->data, msg->length);
    if (false == msg->moreSegmentsFlag) {{
      msg->data = (uint8_t *)payload;
    }}
  }} else {{
    ret = E_NOT_OK;
  }}
  return ret;
}}\n"""
                )
            C.write(
                f"""
Std_ReturnType SomeIp_{service_name}_Object_{event['name']}_OnNotification(uint32_t requestId,
                                                                    SomeIp_MessageType *evt) {{

  Std_ReturnType ret = E_OK;
  int32_t serializedSize;
  static events::{event['name']}::SampleType sample;
  ASLOG(RADAR_SERVICE,
        ("{event['name']} OnNotification %X: len=%d, data=[%02X %02X %02X %02X ...]\\n", requestId,
         evt->length, evt->data[0], evt->data[1], evt->data[2], evt->data[3]));

  serializedSize =
    SomeIpXf_DecodeStruct(evt->data, evt->length, &sample, &SomeIpXf_Struct{event['type']}Def);
  if (serializedSize > 0) {{
    {service_name}Manager::GetInstance()->On{event['name']}(sample);
  }} else {{
    ASLOG(RADAR_SERVICE_E, ("malformed {event['name']}\\n"));
    ret = E_NOT_OK;
  }}

  return ret;
}}

void SomeIp_{service_name}_Object_OnSubscribeAck(boolean isSubscribe) {{
  ASLOG(RADAR_SERVICE, ("{eg['name']} %ssubscribed\\n", isSubscribe ? "" : "un"));
  {service_name}Manager::GetInstance()->On{event['name']}SubscribeAck(isSubscribe);
}}\n"""
            )
    C.write('} /* extern "C" */\n')
    C.close()
