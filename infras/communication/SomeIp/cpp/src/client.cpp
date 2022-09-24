/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "usomeip/usomeip.hpp"
#include "usomeip/client.hpp"
#include "./common.hpp"
namespace as {
namespace usomeip {
namespace client {
/* ================================ [ MACROS    ] ============================================== */
#define SOMEIP_SF_MAX 1396
/* ================================ [ TYPES     ] ============================================== */
class MethodClient : public CSHelper {
public:
  MethodClient(uint16_t methodId, client::Client *client, BufferPool *bp)
    : m_MethodId(methodId), m_Client(client), m_BufferPool(bp) {
  }

  ~MethodClient() {
  }

  Std_ReturnType copy_request(uint32_t requestId, SomeIp_TpMessageType *msg) {
    return copy_to(requestId & 0xFFFF, m_RequestMap, m_Lock, msg);
  }

  Std_ReturnType copy_response(uint32_t requestId, SomeIp_TpMessageType *msg) {
    Std_ReturnType ret = E_OK;
    auto buffer = get(requestId, m_BufferPool, m_ResponseMap);
    if (nullptr != buffer) {
      ret = copy_from(requestId, m_ResponseMap, m_Lock, msg);
    } else {
      ret = SOMEIP_E_NOMEM;
    }

    return ret;
  }

  void request(uint32_t requestId, std::shared_ptr<Buffer> buffer) {
    if (buffer->size > SOMEIP_SF_MAX) {
      std::unique_lock<std::mutex> lck(m_Lock);
      m_RequestMap[requestId & 0xFFFF] = {buffer, E_OK};
    }
    Std_ReturnType ret = SomeIp_Request(requestId, (uint8_t *)buffer->data, buffer->size);
    if (E_OK != ret) {
      if (buffer->size > SOMEIP_SF_MAX) {
        std::unique_lock<std::mutex> lck(m_Lock);
        m_RequestMap.erase(requestId & 0xFFFF);
      }
    }
  }

  void onResponse(uint32_t requestId, SomeIp_MessageType *res) {
    auto msg = transform(requestId, res, m_MethodId, MessageType::RESPONSE, m_ResponseMap);
    m_Client->onResponse(msg);
  }

  void onError(uint32_t requestId, Std_ReturnType ercd) {
    auto msg = std::make_shared<Message>(m_MethodId, requestId, MessageType::ERROR);
    m_Client->onError(msg);
  }

private:
  uint16_t m_MethodId;
  client::Client *m_Client;
  BufferPool *m_BufferPool;
  std::map<uint32_t, BufferInfo> m_RequestMap;
  std::map<uint32_t, BufferInfo> m_ResponseMap;
  std::mutex m_Lock;
};

class EventClient : public CSHelper {
public:
  EventClient(uint16_t eventId, client::Client *client, BufferPool *bp)
    : m_EventId(eventId), m_Client(client), m_BufferPool(bp) {
  }

  ~EventClient() {
  }

  Std_ReturnType copy_event(uint32_t requestId, SomeIp_TpMessageType *msg) {
    Std_ReturnType ret = E_OK;
    auto buffer = get(requestId, m_BufferPool, m_EventMap);
    if (nullptr != buffer) {
      ret = copy_from(requestId, m_EventMap, m_Lock, msg);
    } else {
      ret = SOMEIP_E_NOMEM;
    }

    return ret;
  }

  void onNotification(uint32_t requestId, SomeIp_MessageType *evt) {
    auto msg = transform(requestId, evt, m_EventId, MessageType::NOTIFICATION, m_EventMap);
    m_Client->onNotification(msg);
  }

private:
  uint16_t m_EventId;
  client::Client *m_Client;
  BufferPool *m_BufferPool;
  std::map<uint32_t, BufferInfo> m_EventMap;
  std::mutex m_Lock;
};

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static std::mutex s_Lock;
static std::map<uint16_t, std::shared_ptr<MethodClient>> s_MethodClientMap;
static std::map<uint16_t, std::shared_ptr<EventClient>> s_EventClientMap;
static std::map<uint16_t, client::Client *> s_IdentityMap;
/* ================================ [ LOCALS    ] ============================================== */
std::shared_ptr<MethodClient> get_mc(uint16_t methodId) {
  std::shared_ptr<MethodClient> mc = nullptr;
  std::unique_lock<std::mutex> lck(s_Lock);
  auto it = s_MethodClientMap.find(methodId);
  if (it != s_MethodClientMap.end()) {
    mc = it->second;
  }
  return mc;
}

std::shared_ptr<EventClient> get_ec(uint16_t eventId) {
  std::shared_ptr<EventClient> ec = nullptr;
  std::unique_lock<std::mutex> lck(s_Lock);
  auto it = s_EventClientMap.find(eventId);
  if (it != s_EventClientMap.end()) {
    ec = it->second;
  }
  return ec;
}
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType on_response(uint16_t methodId, uint32_t requestId, SomeIp_MessageType *res) {
  Std_ReturnType ret = E_NOT_OK;
  auto mc = get_mc(methodId);
  if (nullptr != mc) {
    mc->onResponse(requestId, res);
    ret = E_OK;
  } else {
    usLOG(ERROR, "method %u has no client for response %x\n", methodId, requestId);
  }

  return ret;
}

Std_ReturnType on_error(uint16_t methodId, uint32_t requestId, Std_ReturnType ercd) {
  Std_ReturnType ret = E_NOT_OK;
  auto mc = get_mc(methodId);
  if (nullptr != mc) {
    mc->onError(requestId, ercd);
    ret = E_OK;
  } else {
    usLOG(ERROR, "method %u has no client for response %x\n", methodId, requestId);
  }

  return ret;
}

Std_ReturnType on_method_tp_rx_data(uint16_t methodId, uint32_t requestId,
                                    SomeIp_TpMessageType *msg) {
  Std_ReturnType ret = E_OK;
  auto mc = get_mc(methodId);
  if (nullptr != mc) {
    ret = mc->copy_response(requestId, msg);
  } else {
    usLOG(ERROR, "method %u has no client for TP request %x\n", methodId, requestId);
    ret = E_NOT_OK;
  }
  return ret;
}

Std_ReturnType on_method_tp_tx_data(uint16_t methodId, uint32_t requestId,
                                    SomeIp_TpMessageType *msg) {
  Std_ReturnType ret = E_NOT_OK;
  auto ms = get_mc(methodId);
  if (nullptr != ms) {
    ret = ms->copy_request(requestId, msg);
  }
  return ret;
}

Std_ReturnType on_notification(uint16_t eventId, uint32_t requestId, SomeIp_MessageType *evt) {
  Std_ReturnType ret = E_NOT_OK;
  auto ec = get_ec(eventId);
  if (nullptr != ec) {
    ec->onNotification(requestId, evt);
    ret = E_OK;
  } else {
    usLOG(ERROR, "event %u has no client for notification %x\n", eventId, requestId);
  }

  return ret;
}

Std_ReturnType on_event_tp_rx_data(uint16_t eventId, uint32_t requestId,
                                   SomeIp_TpMessageType *msg) {
  Std_ReturnType ret = E_OK;
  auto ec = get_ec(eventId);
  if (nullptr != ec) {
    ret = ec->copy_event(requestId, msg);
  } else {
    usLOG(ERROR, "event %u has no client for TP request %x\n", eventId, requestId);
    ret = E_NOT_OK;
  }
  return ret;
}

void Client::bind(uint16_t methodId, BufferPool *bp) {
  std::unique_lock<std::mutex> lck(s_Lock);
  auto it = s_MethodClientMap.find(methodId);
  if (it == s_MethodClientMap.end()) {
    s_MethodClientMap[methodId] = std::make_shared<MethodClient>(methodId, this, bp);
  } else {
    throw std::runtime_error("method " + std::to_string(methodId) + " has already been used");
  }
}

void Client::listen(uint16_t eventId, BufferPool *bp) {
  std::unique_lock<std::mutex> lck(s_Lock);
  auto it = s_EventClientMap.find(eventId);
  if (it == s_EventClientMap.end()) {
    s_EventClientMap[eventId] = std::make_shared<EventClient>(eventId, this, bp);
  } else {
    throw std::runtime_error("event " + std::to_string(eventId) + " has already been used");
  }
}

void Client::require(uint16_t serviceId) {
  Sd_ClientServiceSetState(serviceId, SD_CLIENT_SERVICE_REQUESTED);
}

void Client::subscribe(uint16_t eventGroupId) {
  Sd_ConsumedEventGroupSetState(eventGroupId, SD_CONSUMED_EVENTGROUP_REQUESTED);
}

void on_availability(uint16_t clientId, boolean isAvailable) {
  std::unique_lock<std::mutex> lck(s_Lock);
  auto it = s_IdentityMap.find(clientId);
  if (it != s_IdentityMap.end()) {
    auto client = it->second;
    client->onAvailability(isAvailable);
  } else {
    usLOG(ERROR, "client %d not found\n", clientId);
  }
}

void Client::identity(uint16_t clientId) {
  std::unique_lock<std::mutex> lck(s_Lock);
  auto it = s_IdentityMap.find(clientId);
  if (it == s_IdentityMap.end()) {
    s_IdentityMap[clientId] = this;
  } else {
    throw std::runtime_error("client " + std::to_string(clientId) + " has already been used");
  }
}

void Client::request(uint32_t requestId, std::shared_ptr<Buffer> buffer) {
  uint16_t methodId = requestId >> 16;
  auto mc = get_mc(methodId);
  if (nullptr != mc) {
    mc->request(requestId, buffer);
  } else {
    usLOG(ERROR, "has no method client for request %x\n", requestId);
  }
}
} /* namespace client */
} /* namespace usomeip */
} /* namespace as */
