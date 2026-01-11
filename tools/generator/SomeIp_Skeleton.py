# SSAS - Simple Smart Automotive Software
# Copyright (C) 2026 Parai Wang <parai@foxmail.com>

import os
from .helper import *
from .SomeIpXf import *

__all__ = ["Gen_SomeIpSkeleton"]


def GetArgs(cfg, name):
    for args in cfg.get("args", []):
        if name == args["name"]:
            return args["args"]
    raise Exception(f"args {name} not found")


def Gen_SomeIpSkeleton(cfg, service, dir, source):
    allStructs = GetStructs(cfg)
    service_name = service["name"]
    H = open("%s/%sSkeleton.hpp" % (dir, service_name), "w")
    source["%sSkeleton" % (service_name)] = ["%s/%sSkeleton.cpp" % (dir, service_name)]
    GenHeader(H)
    H.write("#ifndef ARA_SOMEIP_%s_SKELETON_HPP\n" % (toMacro(service_name)))
    H.write("#define ARA_SOMEIP_%s_SKELETON_HPP\n" % (toMacro(service_name)))
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write('#include "ara/com/skeleton/skeleton_base.h"\n')
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
    H.write("/* ================================ [ CLASS     ] ============================================== */\n")
    H.write("namespace events {\n")
    def_events = ""
    def_methods = ""
    used_types = ""
    for eg in service.get("event-groups", []):
        for event in eg["events"]:
            def_events += f"  events::{event['name']} {event['name']};\n"
            isTp = event.get("tp", False)
            if isTp:
                defTpVars = f"  bool m_payloadInUse = false;"
                defTpCopyTxDataFnc = "  Std_ReturnType OnTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg);"
            else:
                defTpVars = ""
                defTpCopyTxDataFnc = ""
            struct = allStructs[event["type"]]
            payloadSize = GetStructPayloadSize(struct, allStructs)
            H.write(
                f"""
class {event['name']} {{
public:
  using SampleType = {event['type']}_Type;

  ara::core::Result<void> Send(const SampleType &data);

  ara::core::Result<ara::com::SampleAllocateePtr<SampleType>> Allocate();

  ara::core::Result<void> Send(ara::com::SampleAllocateePtr<SampleType> data);

  ara::com::SubscriptionState GetSubscriptionState () const noexcept;

public:
  /* stack internal use only public APIs */
{defTpCopyTxDataFnc}

private:
  uint16_t m_sessionId = 0;
  uint8_t m_payload[{payloadSize}];
{defTpVars}
}};\n\n"""
            )
    H.write("} // namespace events\n\n")
    for method in service.get("methods", []):
        UsedTypes = []
        ReturnType = method.get("return", "void")
        if ReturnType != "void":
            UsedTypes.append(ReturnType)
        Args = []
        if "args" in method:
            args = GetArgs(cfg, method["args"])
            for arg in args:
                Args.append(f"const {arg['type']} &{arg['name']}")
                if arg["type"] not in UsedTypes:
                    UsedTypes.append(arg["type"])
        def_methods += f"  virtual ara::core::Future<{ReturnType}> {method['name']}({','.join(Args)}) = 0;\n"
        for utype in UsedTypes:
            used_types += f"  using {utype} = {utype}_Type;\n"
    H.write(
        f"""
class {service_name}Skeleton {{
public:
{used_types}
  {service_name}Skeleton(
    const ara::com::InstanceIdentifier &instanceId,
    ara::com::MethodCallProcessingMode mode = ara::com::MethodCallProcessingMode::kEvent);

  static ara::core::Result<{service_name}Skeleton> Create(
    const ara::com::InstanceIdentifier &instanceID,
    ara::com::MethodCallProcessingMode mode = ara::com::MethodCallProcessingMode::kEvent) noexcept;

  {service_name}Skeleton(
    ara::com::InstanceIdentifierContainer instanceIds,
    ara::com::MethodCallProcessingMode mode = ara::com::MethodCallProcessingMode::kEvent);

  static ara::core::Result<{service_name}Skeleton> Create(
    const ara::com::InstanceIdentifierContainer &instanceIDs,
    ara::com::MethodCallProcessingMode mode = ara::com::MethodCallProcessingMode::kEvent) noexcept;

  {service_name}Skeleton(
    ara::core::InstanceSpecifier instanceSpec,
    ara::com::MethodCallProcessingMode mode = ara::com::MethodCallProcessingMode::kEvent);

  static ara::core::Result<{service_name}Skeleton> Create(
    const ara::core::InstanceSpecifier &instanceSpec,
    ara::com::MethodCallProcessingMode mode = ara::com::MethodCallProcessingMode::kEvent) noexcept;

  {service_name}Skeleton(const {service_name}Skeleton &other) = delete;

  {service_name}Skeleton &operator=(const {service_name}Skeleton &other) = delete;

  ~{service_name}Skeleton();

  ara::core::Result<void> OfferService();

  void StopOfferService();

  bool ProcessNextMethodCall();

public:
{def_events}

{def_methods}
}};\n"""
    )
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write(f"}} // namespace {service_name}\n")
    H.write("} // namespace com\n")
    H.write("} // namespace ara\n")
    H.write("#endif /* ARA_SOMEIP_%s_SKELETON_HPP */\n" % (toMacro(service_name)))
    H.close()
    C = open("%s/%sSkeleton.cpp" % (dir, service_name), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write(f'#include "{service_name}Skeleton.hpp"\n')
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
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write(f"static {service_name}Skeleton *s_h{service_name} = nullptr;\n")
    for eg in service.get("event-groups", []):
        C.write(f"static bool s_b{eg['name']}Subscribed = false;\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.write("namespace events {\n")
    for eg in service.get("event-groups", []):
        for event in eg.get("events", []):
            isTp = event.get("tp", False)
            if isTp:
                C.write(
                    f"""
Std_ReturnType {event['name']}::OnTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  Std_ReturnType ret = E_OK;
  if ((NULL != msg) && ((msg->offset + msg->length) <= sizeof(m_payload))) {{
    memcpy(msg->data, &m_payload[msg->offset], msg->length);
    if (false == msg->moreSegmentsFlag) {{
      m_payloadInUse = false;
    }}
  }} else {{
    m_payloadInUse = false;
    ret = E_NOT_OK;
  }}
  return ret;
}}

Result<void> {event['name']}::Send(const {event['name']}::SampleType &data) {{
  Result<void> rslt;

  if ( false == m_payloadInUse ) {{
    uint32_t requestId =
      ((uint32_t)SOMEIP_TX_EVT_{toMacro(service_name)}_{toMacro(eg['name'])}_{toMacro(event['name'])} << 16) + (++m_sessionId);
    int32_t serializedSize =
      SomeIpXf_EncodeStruct(m_payload, sizeof(m_payload), &data, &SomeIpXf_Struct{event['type']}Def);
    if (serializedSize > 0) {{
      Std_ReturnType ret = SomeIp_Notification(requestId, m_payload, serializedSize);
      if (E_OK != ret) {{
        rslt = Result<void>(ret);
      }} else {{
        if (serializedSize > SOMEIP_SF_MAX) {{
          m_payloadInUse = true;
        }}
      }}
    }} else {{
      ASLOG({toMacro(service_name)}_E, ("{event['name']} serialization failed!\\n"));
      rslt = Result<void>(-EINVAL);
    }}
  }} else {{
    rslt = Result<void>(-EBUSY);
  }}

  return rslt;
}}
\n"""
                )
            else:
                C.write(
                    f"""
Result<void> {event['name']}::Send(const {event['name']}::SampleType &data) {{
  Result<void> rslt;

  uint32_t requestId =
    ((uint32_t)SOMEIP_TX_EVT_{toMacro(service_name)}_{toMacro(eg['name'])}_{toMacro(event['name'])} << 16) + (++m_sessionId);
  int32_t serializedSize =
    SomeIpXf_EncodeStruct(m_payload, sizeof(m_payload), &data, &SomeIpXf_Struct{event['type']}Def);
  if (serializedSize > 0) {{
    Std_ReturnType ret = SomeIp_Notification(requestId, m_payload, serializedSize);
    if (E_OK != ret) {{
      rslt = Result<void>(ret);
    }}
  }} else {{
    ASLOG({toMacro(service_name)}_E, ("{event['name']} serialization failed!\\n"));
    rslt = Result<void>(-EINVAL);
  }}

  return rslt;
}}\n"""
                )
        C.write(
            f"""
Result<SampleAllocateePtr<{event['name']}::SampleType>> {event['name']}::Allocate() {{
  Result<SampleAllocateePtr<{event['name']}::SampleType>> rslt(ErrorCode(0));
  SampleAllocateePtr<{event['name']}::SampleType> SamplePtr = std::make_unique<{event['name']}::SampleType>();

  if (nullptr != SamplePtr) {{
    rslt = Result<SampleAllocateePtr<{event['name']}::SampleType>>(std::move(SamplePtr));
  }} else {{
    rslt = Result<SampleAllocateePtr<{event['name']}::SampleType>>(ErrorCode(-ENOMEM));
  }}

  return rslt;
}}

Result<void> {event['name']}::Send(SampleAllocateePtr<{event['name']}::SampleType> data) {{
  Result<void> rslt;

  if (nullptr != data) {{
    rslt = {event['name']}::Send(*data.get());
  }}

  return rslt;
}}

ara::com::SubscriptionState {event['name']}::GetSubscriptionState () const noexcept {{
  ara::com::SubscriptionState state = ara::com::SubscriptionState::kNotSubscribed;
  if (true == s_b{eg['name']}Subscribed) {{
    state = ara::com::SubscriptionState::kSubscribed;
  }}
  return state;
}}\n"""
        )
    C.write("} // namespace events\n")
    C.write(
        f"""
{service_name}Skeleton::{service_name}Skeleton(const InstanceIdentifier &instanceId,
                                           MethodCallProcessingMode mode) {{
  if (nullptr == s_h{service_name}) {{
    s_h{service_name} = this;
  }} else {{
    throw std::runtime_error("{service_name} already created");
  }}
}}

{service_name}Skeleton::~{service_name}Skeleton() {{
  if (this == s_h{service_name}) {{
    s_h{service_name} = nullptr;
  }}
}}

Result<void> {service_name}Skeleton::OfferService() {{
  Result<void> rslt;
  Std_ReturnType ret;

  ret = Sd_ServerServiceSetState(SD_SERVER_SERVICE_HANDLE_ID_{toMacro(service_name)},
                                 SD_SERVER_SERVICE_AVAILABLE);
  if (E_OK != ret) {{
    rslt = Result<void>(ret);
  }}

  return rslt;
}}\n"""
    )
    C.write(f"}} // namespace {service_name}\n")
    C.write("} // namespace com\n")
    C.write("} // namespace ara\n")
    C.write(f"using namespace ara::com::{service_name};\n")
    C.write('extern "C" {\n')
    C.write(
        f"""
boolean Sd_ServerService{service_name}_CRMC(PduIdType pduID, uint8_t type, uint16_t serviceID, uint16_t instanceID,
                       uint8_t majorVersion, uint32_t minorVersion,
                       const Sd_ConfigOptionStringType *receivedConfigOptionPtrArray,
                       const Sd_ConfigOptionStringType *configuredConfigOptionPtrArray) {{

  return true;
}}

void SomeIp_{service_name}_OnConnect(uint16_t conId, boolean isConnected) {{
  ASLOG({toMacro(service_name)}, ("{service_name} [%u] %sconnected\\n", conId, isConnected?"":"dis"));
}}
\n"""
    )
    for method in service.get("methods", []):
        ReturnType = method.get("return", "void")
        def_args = ""
        Args = []
        args = GetArgs(cfg, method["args"])
        decArgs = ""
        reqPayloadSize = 0
        for arg in args:
            def_args += f"  static {arg['type']} {arg['name']};\n"
            reqPayloadSize += GetStructPayloadSize(allStructs[arg["type"]], allStructs)
            Args.append(arg["name"])
            for arg in args:
                decArgs += f"""
    if ((offset >= 0) && (offset < (int32_t)req->length)) {{
      serializedSize = SomeIpXf_DecodeStruct(&req->data[offset], req->length - offset,
                          &{arg["name"]}, &SomeIpXf_Struct{arg['type']}Def);
      if (serializedSize > 0) {{
        offset += serializedSize;
      }} else {{
        offset = -ENOSPC;
      }}
    }}
"""
        C.write(f"static ara::core::Future<{ReturnType}> s_future{method['name']};")
        C.write(f"static bool s_{method['name']}InRequest = false;")
        if method.get("tp", False):
            C.write(f"static uint8_t s_{method['name']}RequestPayload[{reqPayloadSize}+1];\n")
            resPayloadSize = GetStructPayloadSize(allStructs[ReturnType], allStructs)
        C.write(f"static uint8_t s_{method['name']}ResponsePayload[{resPayloadSize}+16];\n")
        C.write(
            f"""
Std_ReturnType SomeIp_{service_name}_{method['name']}_OnRequest(uint32_t requestId, SomeIp_MessageType *req, SomeIp_MessageType *res) {{
  Std_ReturnType ret = E_OK;
  int32_t offset = 0;
  int32_t serializedSize;
{def_args}
  ASLOG({toMacro(service_name)}, ("{method['name']} OnRequest %X: len=%d, data=[%02X %02X %02X %02X ...]\\n", requestId,
                        req->length, req->data[0], req->data[1], req->data[2], req->data[3]));
  if (nullptr != s_hRadarService) {{
{decArgs}
    if (true == s_{method['name']}InRequest) {{
      ASLOG({toMacro(service_name)}_E, ("{method['name']} is busy in request!\\n"));
      ret = E_NOT_OK;
    }} else if (offset >= 0) {{
      s_{method['name']}InRequest = true;
      s_future{method['name']} = s_hRadarService->{method['name']}({",".join(Args)});
      if (true == s_future{method['name']}.is_ready()) {{
        ara::core::Result<{ReturnType}> result = s_future{method['name']}.GetResult();
        if (result.HasValue()) {{
          res->data = &s_{method['name']}ResponsePayload[16];
          serializedSize = SomeIpXf_EncodeStruct(res->data, sizeof(s_{method['name']}ResponsePayload)-16, &result.Value(), &SomeIpXf_Struct{ReturnType}Def);
          if (serializedSize > 0) {{
            res->length = serializedSize;
            if (serializedSize <= SOMEIP_SF_MAX) {{
              s_{method['name']}InRequest = false;
            }}
          }} else {{
            ASLOG({toMacro(service_name)}_E, ("{method['name']} encode response failed!\\n"));
            ret = E_NOT_OK;
            s_{method['name']}InRequest = false;
          }}
        }} else {{
          ASLOG({toMacro(service_name)}_E, ("{method['name']} execution failed!\\n"));
          ret = E_NOT_OK;
          s_{method['name']}InRequest = false;
        }}
      }} else {{
        ret = SOMEIP_E_PENDING;
      }}
    }} else {{
      ASLOG({toMacro(service_name)}_E, ("{method['name']} decode args failed!\\n"));
      ret = E_NOT_OK;
    }}
  }}
  return ret;
}}

Std_ReturnType SomeIp_{service_name}_{method['name']}_OnFireForgot(uint32_t requestId, SomeIp_MessageType *req) {{
  ASLOG(RADAR_SERVICE,
        ("{method['name']} OnFireForgot %X: len=%d, data=[%02X %02X %02X %02X ...]\\n", requestId, req->length,
         req->data[0], req->data[1], req->data[2], req->data[3]));
  return E_OK;
}}

Std_ReturnType SomeIp_{service_name}_{method['name']}_OnAsyncRequest(uint32_t requestId, SomeIp_MessageType *res) {{
  Std_ReturnType ret = E_OK;
  int32_t serializedSize;
  if (false == s_{method['name']}InRequest) {{
    ASLOG({toMacro(service_name)}_E, ("{method['name']} is not in busy request status!\\n"));
    ret = E_NOT_OK;
  }} else if (nullptr == res) {{
    ASLOG({toMacro(service_name)}_E, ("{method['name']} response transmission failed!\\n"));
    s_{method['name']}InRequest = false;
  }} else if (true == s_future{method['name']}.is_ready()) {{
    ara::core::Result<{ReturnType}> result = s_future{method['name']}.GetResult();
    if (result.HasValue()) {{
      res->data = &s_{method['name']}ResponsePayload[16];
      serializedSize = SomeIpXf_EncodeStruct(res->data, sizeof(s_{method['name']}ResponsePayload)-16, &result.Value(), &SomeIpXf_Struct{ReturnType}Def);
      if (serializedSize > 0) {{
        res->length = serializedSize;
        if (serializedSize <= SOMEIP_SF_MAX) {{
          s_{method['name']}InRequest = false;
        }}
      }} else {{
        ASLOG({toMacro(service_name)}_E, ("{method['name']} encode response failed!\\n"));
        ret = E_NOT_OK;
        s_{method['name']}InRequest = false;
      }}
    }} else {{
      ASLOG({toMacro(service_name)}_E, ("{method['name']} execution failed!\\n"));
      ret = E_NOT_OK;
      s_{method['name']}InRequest = false;
    }}
  }} else {{
    ret = SOMEIP_E_PENDING;
  }}
  return ret;
}}\n"""
        )
        if method.get("tp", False):
            C.write(
                f"""
Std_ReturnType SomeIp_{service_name}_{method['name']}_OnTpCopyRxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  Std_ReturnType ret = E_OK;
  if ((NULL != msg) && (false == s_{method['name']}InRequest) && 
      ((msg->offset + msg->length)) <= sizeof(s_{method['name']}RequestPayload)) {{
    memcpy(&s_{method['name']}RequestPayload[msg->offset], msg->data, msg->length);
    if (FALSE == msg->moreSegmentsFlag) {{
      msg->data = s_{method['name']}RequestPayload;
    }}
  }} else {{
    ret = E_NOT_OK;
  }}
  return ret;
}}

Std_ReturnType SomeIp_{service_name}_{method['name']}_OnTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  Std_ReturnType ret = E_OK;
  if ((NULL != msg) && ((msg->offset + msg->length + 16) <= sizeof(s_{method['name']}ResponsePayload))) {{
    memcpy(msg->data, &s_{method['name']}ResponsePayload[msg->offset + 16], msg->length);
    s_{method['name']}InRequest = false;
  }} else {{
    ret = E_NOT_OK;
  }}
  return ret;
}}\n"""
            )
    for eg in service.get("event-groups", []):
        C.write(
            f"""
void SomeIp_{service_name}_{eg['name']}_OnSubscribe(boolean isSubscribe, TcpIp_SockAddrType *RemoteAddr) {{
  ASLOG({toMacro(service_name)},
        ("{eg['name']} %ssubscribed by %d.%d.%d.%d:%d\\n", isSubscribe ? "" : "stop ", RemoteAddr->addr[0],
         RemoteAddr->addr[1], RemoteAddr->addr[2], RemoteAddr->addr[3], RemoteAddr->port));
  s_b{eg['name']}Subscribed = isSubscribe;
}}\n"""
        )
        for event in eg.get("events", []):
            isTp = event.get("tp", False)
            if isTp:
                beName = f"{service_name}_{eg['name']}_{event['name']}"
                C.write(
                    f"""
Std_ReturnType SomeIp_{service_name}_{eg['name']}_{event['name']}_OnTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  Std_ReturnType ret = E_OK;
  if (nullptr != s_h{service_name}) {{
    ret = s_h{service_name}->{event['name']}.OnTpCopyTxData(requestId, msg);
  }} else {{
    ret = E_NOT_OK;
  }}
  return ret;
}}\n"""
                )
    C.write('} /* extern "C" */\n')
    C.close()
