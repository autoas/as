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
    def_fields = ""
    used_types = ""
    MaxPayloadSize = 32
    for eg in service.get("event-groups", []):
        for event in eg["events"]:
            if "for" in event:
                continue
            def_events += f"  events::{event['name']} {event['name']};\n"
            isTp = event.get("tp", False)
            if isTp:
                defTpVars = f"  bool m_payloadInUse = false;"
                defTpCopyTxDataFnc = "  Std_ReturnType OnTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg);"
            else:
                defTpVars = ""
                defTpCopyTxDataFnc = ""
            payloadSize = GetTypePayloadSize(event, allStructs)
            H.write(
                f"""
class {event['name']} {{
public:
  using SampleType = {GetXfCType(event, allStructs)};

  ara::core::Result<void> Send(const SampleType &data);

  ara::core::Result<ara::com::SampleAllocateePtr<SampleType>> Allocate();

  ara::core::Result<void> Send(ara::com::SampleAllocateePtr<SampleType> data);

  ara::com::SubscriptionState GetSubscriptionState () const noexcept;

public:
  /* stack internal use only public APIs */
{defTpCopyTxDataFnc}

private:
  uint16_t m_sessionId = 0;
  uint8_t m_payload[20 + {payloadSize}];
{defTpVars}
}};\n\n"""
            )
    H.write("} // namespace events\n\n")
    H.write("namespace fields {\n")
    for field in service.get("fields", []):
        def_fields += f"  fields::{field['name']} {field['name']};\n"
        fieldPayloadSize = GetTypePayloadSize(field, allStructs)
        if 'get' in field or 'set' in field:
          if MaxPayloadSize < fieldPayloadSize:
              MaxPayloadSize = fieldPayloadSize
        H.write(
            f"""
class {field['name']} {{
public:
  using FieldType = {GetXfCType(field, allStructs)};

{"  ara::core::Result<void> Update(const FieldType& data);" if 'event' in field or 'get' in field else ""}

{"  ara::core::Result<void> RegisterGetHandler(std::function<ara::core::Future<FieldType>()> getHandler);"
  if 'get' in field else ""}

{"  ara::core::Result<void> RegisterSetHandler(std::function<ara::core::Future<FieldType>(const FieldType& data)> setHandler);"
  if 'set' in field else ""}

public:
  /* stack internal use only public APIs */
  ara::com::ComErrc Validate();
{"  Std_ReturnType OnGetRequest(uint32_t requestId, SomeIp_MessageType* req, SomeIp_MessageType* res);" if 'get' in field else ""}
{"  Std_ReturnType OnGetAsyncRequest(uint32_t requestId, SomeIp_MessageType* res);" if 'get' in field else ""}
{"  Std_ReturnType OnGetTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg);" if 'get' in field and field.get("tp", False) else ""}

{"  Std_ReturnType OnSetRequest(uint32_t requestId, SomeIp_MessageType* req, SomeIp_MessageType* res);" if 'set' in field else ""}
{"  Std_ReturnType OnSetAsyncRequest(uint32_t requestId, SomeIp_MessageType* res);" if 'set' in field else ""}
{"  Std_ReturnType OnSetTpCopyRxData(uint32_t requestId, SomeIp_TpMessageType *msg);" if 'set' in field and field.get("tp", False) else ""}
{"  Std_ReturnType OnSetTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg);" if 'set' in field and field.get("tp", False) else ""}

{"  Std_ReturnType OnEventTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg);" if 'event' in field and field.get("tp", False) else ""}

private:
{"  std::function<ara::core::Future<FieldType>()> m_getHandler = nullptr;" if 'get' in field else ""}
{"  ara::core::Future<FieldType> m_futureGet;" if 'get' in field else ""}
{"  FieldType m_fieldGet;" if 'get' in field else ""}
{f"  uint8_t m_responseGet[20 + {fieldPayloadSize}];" if 'get' in field and field.get("tp", False) else ""}
{f"  bool m_responseGetInUse = false;" if 'get' in field else ""}
{f"  bool m_bValid = false;"}

{"  std::function<ara::core::Future<FieldType>(const FieldType& data)> m_setHandler = nullptr;" if 'set' in field else ""}
{"  ara::core::Future<FieldType> m_futureSet;" if 'get' in field else ""}
{"  FieldType m_fieldSet;" if 'set' in field else ""}
{f"  uint8_t m_requestSet[{fieldPayloadSize}];" if 'set' in field and field.get("tp", False) else ""}
{f"  uint8_t m_responseSet[20 + {fieldPayloadSize}];" if 'set' in field and field.get("tp", False) else ""}
{f"  bool m_responseSetInUse = false;" if 'set' in field else ""}

{f"  uint8_t m_payload[20 + {fieldPayloadSize}];" if 'event' in field else ""}
{f"  bool m_payloadInUse = false;" if 'event' in field else ""}
{f"  uint16_t m_sessionId = 0;" if 'event' in field else ""}
}};\n"""
        )
    H.write("} // namespace fields\n\n")
    for method in service.get("methods", []):
        listenNum = service.get("listen", 1)
        if "for" in method:
            continue
        UsedTypes = []
        ReturnType = method.get("return", "void")
        if ReturnType != "void":
            UsedTypes.append(ReturnType)
        Args = []
        if "args" in method:
            args = GetArgs(cfg, method["args"])
            argsPayloadSize = 0
            for arg in args:
                Args.append(f"const {arg['type']} &{arg['name']}")
                argsPayloadSize += GetTypePayloadSize(arg, allStructs)
                if arg["type"] not in UsedTypes:
                    UsedTypes.append(arg["type"])
            if MaxPayloadSize < argsPayloadSize:
                MaxPayloadSize = argsPayloadSize
        def_methods += f"  virtual ara::core::Future<{ReturnType}> {method['name']}({','.join(Args)}) = 0;\n"
        for utype in UsedTypes:
            used_types += f"  using {utype} = {GetXfCType(utype, allStructs)};\n"
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

{def_fields}

  void OnConnect(uint16_t conId, boolean isConnected);

private:
  void ThreadRxCtrl(uint16_t conId);
private:
  ara::com::MethodCallProcessingMode m_mode;
  bool m_stop[{listenNum}] = {{false}};
  std::thread m_thRxCtrl[{listenNum}];
  uint8_t m_rxBuf[{listenNum}][{MaxPayloadSize}+32];
  SomeIp_CtrlRxBufferType m_ctrlRxBuf[{listenNum}];
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
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    exprsValidateBeforeOffer = ""
    C.write("namespace events {\n")
    for eg in service.get("event-groups", []):
        for event in eg.get("events", []):
            if "for" in event:
                continue
            isTp = event.get("tp", False)
            if isTp:
                C.write(
                    f"""
Std_ReturnType {event['name']}::OnTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  Std_ReturnType ret = E_OK;
  if ((NULL != msg) && ((msg->offset + msg->length + 20) <= sizeof(m_payload))) {{
    msg->data = &m_payload[msg->offset + 20];
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
      {SomeIpXfEncode(event, allStructs, '&m_payload[20]', 'sizeof(m_payload) - 20', 'data')};
    if (serializedSize > 0) {{
      Std_ReturnType ret = SomeIp_Notification(requestId, &m_payload[20], serializedSize);
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
    {SomeIpXfEncode(event, allStructs, '&m_payload[20]', 'sizeof(m_payload) - 20', 'data')};
  if (serializedSize > 0) {{
    Std_ReturnType ret = SomeIp_Notification(requestId, &m_payload[20], serializedSize);
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
  Sd_EventHandlerSubscriberListType *list;
  Std_ReturnType ret = Sd_GetSubscribers(SD_EVENT_HANDLER_{toMacro(service_name)}_{toMacro(eg['name'])}, &list);
  if (E_OK == ret) {{
    state = ara::com::SubscriptionState::kSubscribed;
  }}
  return state;
}}\n"""
            )
    C.write("} // namespace events\n")
    C.write("namespace fields {\n")
    for field in service.get("fields", []):
        exprsValidateBeforeOffer += f"""
  if (ara::com::ComErrc::kOk == ercd) {{
    ercd = {field['name']}.Validate();
  }}"""
        exprsValidate = ""
        if "get" in field:
            C.write(
                f"""
ara::core::Result<void> {field['name']}::RegisterGetHandler(std::function<ara::core::Future<FieldType>()> getHandler) {{
  m_getHandler = getHandler;
  return ara::core::Result<void>(0);
}}

Std_ReturnType {field['name']}::OnGetRequest(uint32_t requestId, SomeIp_MessageType* req, SomeIp_MessageType* res) {{
  Std_ReturnType ret = E_OK;
  bool bValid = false;
  int32_t serializedSize;
  if (false != m_responseGetInUse) {{
    ret = E_NOT_OK;
  }} else if (nullptr != m_getHandler) {{
    m_responseGetInUse = true;
    m_futureGet = m_getHandler();
    if (true == m_futureGet.is_ready()) {{
      ara::core::Result<FieldType> result = m_futureGet.GetResult();
      if (result.HasValue()) {{
          m_fieldGet = result.Value();
          bValid = true;
      }} else {{
        ret = E_NOT_OK;
        m_responseGetInUse = false;
      }}
    }} else {{
      ret = SOMEIP_E_PENDING;
    }}
  }} else if (true == m_bValid) {{
    bValid = true;
  }} else {{
    ret = E_NOT_OK;
  }}

  if (true == bValid) {{
    res->data = &m_responseGet[20];
    serializedSize = {SomeIpXfEncode(field, allStructs, 'res->data', 'sizeof(m_responseGet) - 20', 'm_fieldGet')};
    if (serializedSize > 0) {{
      res->length = serializedSize;
      if (serializedSize <= SOMEIP_SF_MAX) {{
        m_responseGetInUse = false;
      }}
    }} else {{
      ret = E_NOT_OK;
      m_responseGetInUse = false;
    }}
  }}

  return ret;
}}

Std_ReturnType {field['name']}::OnGetAsyncRequest(uint32_t requestId, SomeIp_MessageType* res) {{
  Std_ReturnType ret = E_OK;
  int32_t serializedSize;
  if (false == m_responseGetInUse) {{
    ASLOG({toMacro(service_name)}_E, ("{field['name']} Get is not in busy request status!\\n"));
    ret = E_NOT_OK;
  }} else if (nullptr == res) {{
    ASLOG({toMacro(service_name)}_E, ("{field['name']} Get response transmission failed!\\n"));
    m_responseGetInUse = false;
  }} else if (true == m_futureGet.is_ready()) {{
    ara::core::Result<FieldType> result = m_futureGet.GetResult();
    if (result.HasValue()) {{
      res->data = &m_responseGet[20];
      serializedSize = {SomeIpXfEncode(field, allStructs, 'res->data', "sizeof(m_responseGet) - 20", "result.Value()")};
      if (serializedSize > 0) {{
        res->length = serializedSize;
        if (serializedSize <= SOMEIP_SF_MAX) {{
          m_responseGetInUse = false;
        }}
      }} else {{
        ASLOG({toMacro(service_name)}_E, ("{field['name']} Get encode response failed!\\n"));
        ret = E_NOT_OK;
        m_responseGetInUse = false;
      }}
    }} else {{
      ASLOG({toMacro(service_name)}_E, ("{field['name']} Get execution failed!\\n"));
      ret = E_NOT_OK;
      m_responseGetInUse = false;
    }}
  }} else {{
    ret = SOMEIP_E_PENDING;
  }}
  return ret;
}}\n"""
            )
        if field.get("tp", False):
            C.write(
                f"""
Std_ReturnType {field['name']}::OnGetTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  Std_ReturnType ret = E_OK;
  if ((NULL != msg) && ((msg->offset + msg->length + 20) <= sizeof(m_responseGet))) {{
    msg->data = &m_responseGet[msg->offset + 20];
    if (false == msg->moreSegmentsFlag) {{
      m_responseGetInUse = false;
    }}
  }} else {{
    ret = E_NOT_OK;
    m_responseGetInUse = false;
  }}
  return ret;
}}\n"""
            )
        if "set" in field:
            exprsValidate += f"""
  if (nullptr == m_setHandler) {{
    ercd = ara::com::ComErrc::kFieldSetHandlerNotSet;
  }}\n"""
            C.write(
                f"""
ara::core::Result<void> {field['name']}::RegisterSetHandler(std::function<ara::core::Future<FieldType>(const FieldType& data)> setHandler) {{
  m_setHandler = setHandler;
  return ara::core::Result<void>(0);
}}

Std_ReturnType {field['name']}::OnSetRequest(uint32_t requestId, SomeIp_MessageType* req, SomeIp_MessageType* res) {{
  Std_ReturnType ret = E_OK;
  int32_t serializedSize;
  if ((nullptr != m_setHandler) && (false == m_responseSetInUse)) {{
    m_responseSetInUse = true;
    serializedSize = {SomeIpXfDecode(field, allStructs, 'req->data', 'req->length', 'm_fieldSet')};
    if (serializedSize > 0) {{
      m_futureSet = m_setHandler(m_fieldSet);
      if (true == m_futureSet.is_ready()) {{
        ara::core::Result<FieldType> result = m_futureSet.GetResult();
        if (result.HasValue()) {{
          res->data = &m_responseSet[20];
          serializedSize = {SomeIpXfEncode(field, allStructs, 'res->data', 'sizeof(m_responseSet) - 20', 'result.Value()')};
          if (serializedSize > 0) {{
            res->length = serializedSize;
            if (serializedSize <= SOMEIP_SF_MAX) {{
              m_responseSetInUse = false;
            }}
          }} else {{
            ret = E_NOT_OK;
            m_responseSetInUse = false;
          }}
        }} else {{
          ret = E_NOT_OK;
          m_responseSetInUse = false;
        }}
      }} else {{
        ret = SOMEIP_E_PENDING;
      }}
    }} else {{
      ASLOG({toMacro(service_name)}_E, ("{field['name']} Set request malformed!\\n"));
      ret = E_NOT_OK;
      m_responseSetInUse = false;
    }}
  }} else {{
    ret = E_NOT_OK;
  }}
  return ret;
}}

Std_ReturnType {field['name']}::OnSetAsyncRequest(uint32_t requestId, SomeIp_MessageType* res) {{
  Std_ReturnType ret = E_OK;
  int32_t serializedSize;
  if (false == m_responseSetInUse) {{
    ASLOG({toMacro(service_name)}_E, ("{field['name']} Set is not in busy request status!\\n"));
    ret = E_NOT_OK;
  }} else if (nullptr == res) {{
    ASLOG({toMacro(service_name)}_E, ("{field['name']} Set response transmission failed!\\n"));
    m_responseSetInUse = false;
  }} else if (true == m_futureSet.is_ready()) {{
    ara::core::Result<FieldType> result = m_futureSet.GetResult();
    if (result.HasValue()) {{
      res->data = &m_responseSet[20];
      serializedSize = {SomeIpXfEncode(field, allStructs, 'res->data', "sizeof(m_responseSet) - 20", "result.Value()")};
      if (serializedSize > 0) {{
        res->length = serializedSize;
        if (serializedSize <= SOMEIP_SF_MAX) {{
          m_responseSetInUse = false;
        }}
      }} else {{
        ASLOG({toMacro(service_name)}_E, ("{field['name']} Set encode response failed!\\n"));
        ret = E_NOT_OK;
        m_responseSetInUse = false;
      }}
    }} else {{
      ASLOG({toMacro(service_name)}_E, ("{field['name']} Set execution failed!\\n"));
      ret = E_NOT_OK;
      m_responseSetInUse = false;
    }}
  }} else {{
    ret = SOMEIP_E_PENDING;
  }}
  return ret;
}}\n"""
            )
        if field.get("tp", False):
            C.write(
                f"""
Std_ReturnType {field['name']}::OnSetTpCopyRxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  Std_ReturnType ret = E_OK;
  return ret;
}}

Std_ReturnType {field['name']}::OnSetTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  Std_ReturnType ret = E_OK;
  if ((NULL != msg) && ((msg->offset + msg->length + 20) <= sizeof(m_responseSet))) {{
    msg->data = &m_responseSet[msg->offset + 20];
    if (false == msg->moreSegmentsFlag) {{
      m_responseSetInUse = false;
    }}
  }} else {{
    ret = E_NOT_OK;
    m_responseSetInUse = false;
  }}
  return ret;
}}\n"""
            )
        if "event" in field:
            if field.get("tp", False):
                C.write(
                    f"""
Std_ReturnType {field['name']}::OnEventTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  Std_ReturnType ret = E_OK;
  if ((NULL != msg) && ((msg->offset + msg->length + 20) <= sizeof(m_payload))) {{
    msg->data = &m_payload[msg->offset + 20];
    if (false == msg->moreSegmentsFlag) {{
      m_payloadInUse = false;
    }}
  }} else {{
    ret = E_NOT_OK;
    m_payloadInUse = false;
  }}
  return ret; 
}}\n"""
                )

        if "event" in field or "get" in field:
            exprs = ""
            if "get" in field:
                exprs += f"  m_fieldGet = data;\n"
                exprs += f"  m_bValid = true;\n"
                exprsValidate += f"""
  if ((nullptr == m_getHandler) && (false == m_bValid)) {{
    ercd = ara::com::ComErrc::kFieldValueNotInitialized;
  }}\n"""
            if "event" in field:
                exprs += f"""
  if ( false == m_payloadInUse ) {{
    uint32_t requestId =
      ((uint32_t)SOMEIP_TX_EVT_{toMacro(service_name)}_{toMacro(field['event']['groupName'])}_{toMacro(field['name'])} << 16) + (++m_sessionId);
    int32_t serializedSize =
      {SomeIpXfEncode(field, allStructs, '&m_payload[20]', 'sizeof(m_payload) - 20', 'data')};
    if (serializedSize > 0) {{
      Std_ReturnType ret = SomeIp_Notification(requestId, &m_payload[20], serializedSize);
      if (E_OK != ret) {{
        rslt = Result<void>(ret);
      }} else {{
        if (serializedSize > SOMEIP_SF_MAX) {{
          m_payloadInUse = true;
        }}
      }}
    }} else {{
      ASLOG({toMacro(service_name)}_E, ("{field['name']} serialization failed!\\n"));
      rslt = Result<void>(-EINVAL);
    }}
  }} else {{
    rslt = Result<void>(-EBUSY);
  }}"""
            C.write(
                f"""
Result<void> {field['name']}::Update(const FieldType& data) {{
  Result<void> rslt;

{exprs}
  return rslt;
}}\n"""
            )
        C.write(
            f"""
ara::com::ComErrc {field['name']}::Validate() {{
  ara::com::ComErrc ercd = ara::com::ComErrc::kOk;
{exprsValidate}
  return ercd;
}}\n"""
        )
    C.write("} // namespace fields\n")
    C.write(
        f"""
{service_name}Skeleton::{service_name}Skeleton(const InstanceIdentifier &instanceId,
                                           MethodCallProcessingMode mode) {{
  if (nullptr == s_h{service_name}) {{
    s_h{service_name} = this;
  }} else {{
    throw std::runtime_error("{service_name} already created");
  }}

  m_mode = mode;
}}

{service_name}Skeleton::~{service_name}Skeleton() {{
  if (this == s_h{service_name}) {{
    s_h{service_name} = nullptr;
  }}
}}

Result<void> {service_name}Skeleton::OfferService() {{
  Result<void> rslt;
  ara::com::ComErrc ercd = ara::com::ComErrc::kOk;
  Std_ReturnType ret;
{exprsValidateBeforeOffer}
  if (ara::com::ComErrc::kOk == ercd) {{
    ret = Sd_ServerServiceSetState(SD_SERVER_SERVICE_HANDLE_ID_{toMacro(service_name)},
                                  SD_SERVER_SERVICE_AVAILABLE);
    if (E_OK != ret) {{
      rslt = Result<void>(ret);
    }}
  }} else {{
    ASLOG({toMacro(service_name)}_E, ("Fail to offer: %d\\n", (int)ercd));
    rslt = Result<void>(ercd);
  }}

  return rslt;
}}

bool {service_name}Skeleton::ProcessNextMethodCall() {{
  bool pending = false;
  Std_ReturnType ret;

  for(uint16_t conId = 0; conId < {listenNum}; conId++) {{
    m_ctrlRxBuf[conId].data = m_rxBuf[conId];
    m_ctrlRxBuf[conId].size = sizeof(m_rxBuf[0]);
    do {{
      ret = SomeIp_ConnectionRxControl(SOMEIP_SSID_{toMacro(service_name)}, conId, &m_ctrlRxBuf[conId]);
      if (E_OK == ret) {{
        ASLOG(RADAR_SERVICE, ("a service message received in poll mode\\n"));
        pending = true;
      }}
    }} while (SOMEIP_E_PENDING == ret);
  }}

  return pending;
}}

void {service_name}Skeleton::ThreadRxCtrl(uint16_t conId) {{
  Std_ReturnType ret;
  m_ctrlRxBuf[conId].data = m_rxBuf[conId];
  m_ctrlRxBuf[conId].size = sizeof(m_rxBuf[0]);
  m_ctrlRxBuf[conId].offset = 0;
  m_ctrlRxBuf[conId].length = 0;
  ASLOG({toMacro(service_name)}, ("thread receive control online\\n"));
  while (false == m_stop[conId]) {{
    ret = SomeIp_ConnectionRxControl(SOMEIP_SSID_{toMacro(service_name)}, conId, &m_ctrlRxBuf[conId]);
    if (E_OK == ret) {{
      ASLOG({toMacro(service_name)}, ("a service message received in thread mode\\n"));
    }}
  }}
  ASLOG({toMacro(service_name)}, ("thread receive control offline\\n"));
}}

void {service_name}Skeleton::OnConnect(uint16_t conId, boolean isConnected) {{
  if (true == isConnected) {{
    m_ctrlRxBuf[conId].data = m_rxBuf[conId];
    m_ctrlRxBuf[conId].size = sizeof(m_rxBuf[0]);
    m_ctrlRxBuf[conId].offset = 0;
    m_ctrlRxBuf[conId].length = 0;
    if (MethodCallProcessingMode::kPoll == m_mode) {{
      SomeIp_ConnectionTakeControl(SOMEIP_SSID_{toMacro(service_name)}, conId);
    }} else if (MethodCallProcessingMode::kEvent == m_mode) {{
      SomeIp_ConnectionTakeControl(SOMEIP_SSID_{toMacro(service_name)}, conId);
      m_stop[conId] = false;
      m_thRxCtrl[conId] = std::thread(&{service_name}Skeleton::ThreadRxCtrl, this, conId);
    }} else {{  /* kEventSingleThread */
    }}
  }} else {{
    m_stop[conId] = true;
    if ( m_thRxCtrl[conId].joinable()) {{
      m_thRxCtrl[conId].join();
    }}
  }}
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
  if (nullptr != s_h{service_name}) {{
    s_h{service_name}->OnConnect(conId, isConnected);
  }}
}}
\n"""
    )
    for method in service.get("methods", []):
        if "for" in method:
            continue
        ReturnType = method.get("return", "void")
        def_args = ""
        Args = []
        args = GetArgs(cfg, method["args"])
        decArgs = ""
        reqPayloadSize = 0
        for arg in args:
            def_args += f"  static {arg['type']} {arg['name']};\n"
            reqPayloadSize += GetTypePayloadSize(arg, allStructs)
            Args.append(arg["name"])
            for arg in args:
                decArgs += f"""
    if ((offset >= 0) && (offset < (int32_t)req->length)) {{
      serializedSize = {SomeIpXfDecode(arg, allStructs, '&req->data[offset]', 'req->length - offset', arg["name"])};
      if (serializedSize > 0) {{
        offset += serializedSize;
      }} else {{
        offset = -ENOSPC;
      }}
    }}
"""
        C.write(f"static ara::core::Future<{ReturnType}> s_future{method['name']};\n")
        C.write(f"static bool s_{method['name']}InRequest = false;\n")
        if method.get("tp", False):
            C.write(f"static uint8_t s_{method['name']}RequestPayload[{reqPayloadSize}];\n")
            resPayloadSize = GetTypePayloadSize(ReturnType, allStructs)
        C.write(f"static uint8_t s_{method['name']}ResponsePayload[{resPayloadSize}+20];\n")
        C.write(
            f"""
Std_ReturnType SomeIp_{service_name}_{method['name']}_OnRequest(uint32_t requestId, SomeIp_MessageType *req, SomeIp_MessageType *res) {{
  Std_ReturnType ret = E_OK;
  int32_t offset = 0;
  int32_t serializedSize;
{def_args}
  ASLOG({toMacro(service_name)}, ("{method['name']} OnRequest %X: len=%d, data=[%02X %02X %02X %02X ...]\\n", requestId,
                        req->length, req->data[0], req->data[1], req->data[2], req->data[3]));
  if (nullptr != s_h{service_name}) {{
{decArgs}
    if (true == s_{method['name']}InRequest) {{
      ASLOG({toMacro(service_name)}_E, ("{method['name']} is busy in request!\\n"));
      ret = E_NOT_OK;
    }} else if (offset >= 0) {{
      s_{method['name']}InRequest = true;
      s_future{method['name']} = s_h{service_name}->{method['name']}({",".join(Args)});
      if (true == s_future{method['name']}.is_ready()) {{
        ara::core::Result<{ReturnType}> result = s_future{method['name']}.GetResult();
        if (result.HasValue()) {{
          res->data = &s_{method['name']}ResponsePayload[20];
          serializedSize = {SomeIpXfEncode(ReturnType, allStructs, 'res->data', f"sizeof(s_{method['name']}ResponsePayload)-20", "result.Value()")};
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
      res->data = &s_{method['name']}ResponsePayload[20];
      serializedSize = {SomeIpXfEncode(ReturnType, allStructs, 'res->data', f"sizeof(s_{method['name']}ResponsePayload) - 20", "result.Value()")};
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
  if ((NULL != msg) && ((msg->offset + msg->length + 20) <= sizeof(s_{method['name']}ResponsePayload))) {{
    msg->data = &s_{method['name']}ResponsePayload[msg->offset + 20];
    if (FALSE == msg->moreSegmentsFlag) {{
      s_{method['name']}InRequest = false;
    }}
  }} else {{
    ret = E_NOT_OK;
    s_{method['name']}InRequest = false;
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
}}\n"""
        )
        for event in eg.get("events", []):
            if "for" in event:
                continue
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
    for field in service.get("fields", []):
        if "get" in field:
            C.write(
                f"""
Std_ReturnType SomeIp_{service_name}_Get{field['name']}_OnRequest(uint32_t requestId, SomeIp_MessageType* req, SomeIp_MessageType* res) {{
  Std_ReturnType ret = E_OK;
  if (nullptr != s_h{service_name}) {{
    ret = s_h{service_name}->{field['name']}.OnGetRequest(requestId, req, res);
  }} else {{
    ret = E_NOT_OK;
  }}
  return ret;
}}

Std_ReturnType SomeIp_{service_name}_Get{field['name']}_OnFireForgot(uint32_t requestId, SomeIp_MessageType* res) {{
  Std_ReturnType ret = E_NOT_OK;
  /* Not used */
  return ret;
}}

Std_ReturnType SomeIp_{service_name}_Get{field['name']}_OnAsyncRequest(uint32_t requestId, SomeIp_MessageType* res) {{
  Std_ReturnType ret = E_OK;
  if (nullptr != s_h{service_name}) {{
    ret = s_h{service_name}->{field['name']}.OnGetAsyncRequest(requestId, res);
  }} else {{
    ret = E_NOT_OK;
  }}
  return ret;
}}\n"""
            )
        if field.get("tp", False):
            C.write(
                f"""
Std_ReturnType SomeIp_{service_name}_Get{field['name']}_OnTpCopyRxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  Std_ReturnType ret = E_NOT_OK;
  /* Not used */
  return ret;
}}

Std_ReturnType SomeIp_{service_name}_Get{field['name']}_OnTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  Std_ReturnType ret = E_OK;
  if (nullptr != s_h{service_name}) {{
    ret = s_h{service_name}->{field['name']}.OnGetTpCopyTxData(requestId, msg);
  }} else {{
    ret = E_NOT_OK;
  }}
  return ret;
}}\n"""
            )
        if "set" in field:
            C.write(
                f"""
Std_ReturnType SomeIp_{service_name}_Set{field['name']}_OnRequest(uint32_t requestId, SomeIp_MessageType* req, SomeIp_MessageType* res) {{
  Std_ReturnType ret = E_OK;
  if (nullptr != s_h{service_name}) {{
    ret = s_h{service_name}->{field['name']}.OnSetRequest(requestId, req, res);
  }} else {{
    ret = E_NOT_OK;
  }}
  return ret;
}}

Std_ReturnType SomeIp_{service_name}_Set{field['name']}_OnFireForgot(uint32_t requestId, SomeIp_MessageType* res) {{
  Std_ReturnType ret = E_NOT_OK;
  /* Not Used */
  return ret;
}}

Std_ReturnType SomeIp_{service_name}_Set{field['name']}_OnAsyncRequest(uint32_t requestId, SomeIp_MessageType* res) {{
  Std_ReturnType ret = E_OK;
  if (nullptr != s_h{service_name}) {{
    ret = s_h{service_name}->{field['name']}.OnSetAsyncRequest(requestId, res);
  }} else {{
    ret = E_NOT_OK;
  }}
  return ret;
}}\n"""
            )
        if field.get("tp", False):
            C.write(
                f"""
Std_ReturnType SomeIp_{service_name}_Set{field['name']}_OnTpCopyRxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  Std_ReturnType ret = E_OK;
  if (nullptr != s_h{service_name}) {{
    ret = s_h{service_name}->{field['name']}.OnSetTpCopyRxData(requestId, msg);
  }} else {{
    ret = E_NOT_OK;
  }}
  return ret;
}}

Std_ReturnType SomeIp_{service_name}_Set{field['name']}_OnTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  Std_ReturnType ret = E_OK;
  if (nullptr != s_h{service_name}) {{
    ret = s_h{service_name}->{field['name']}.OnSetTpCopyTxData(requestId, msg);
  }} else {{
    ret = E_NOT_OK;
  }}
  return ret;
}}\n"""
            )
        if "event" in field and field.get("tp", False):
            C.write(
                f"""
Std_ReturnType SomeIp_{service_name}_Fields_{field['name']}_OnTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg) {{
  Std_ReturnType ret = E_OK;
  if (nullptr != s_h{service_name}) {{
    ret = s_h{service_name}->{field['name']}.OnEventTpCopyTxData(requestId, msg);
  }} else {{
    ret = E_NOT_OK;
  }}
  return ret;    
}}\n"""
            )
    C.write('} /* extern "C" */\n')
    C.close()
