/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "usomeip/usomeip.hpp"
#include "usomeip/server.hpp"
#include <atomic>
#include "./common.hpp"
namespace as {
namespace usomeip {
namespace server {
/* ================================ [ MACROS    ] ============================================== */
#define SOMEIP_SF_MAX 1396
/* ================================ [ TYPES     ] ============================================== */
class MethodServer : public CSHelper {
public:
  MethodServer(uint16_t methodId, server::Server *server, BufferPool *bp)
    : m_MethodId(methodId), m_Server(server), m_BufferPool(bp) {
  }

  ~MethodServer() {
  }

  Std_ReturnType copy_response(uint32_t requestId, SomeIp_TpMessageType *msg) {
    return copy_to(requestId, m_ResponseMap, m_Lock, msg);
  }

  Std_ReturnType onAsyncRequest(uint32_t requestId, SomeIp_MessageType *res) {
    Std_ReturnType ret = SOMEIP_E_PENDING;
    BufferInfo bu;
    bool responseReady = false;
    {
      std::unique_lock<std::mutex> lck(m_Lock);
      auto it = m_ResponseMap.find(requestId);
      if (it != m_ResponseMap.end()) {
        bu = it->second;
        responseReady = true;
      }
    }
    if (responseReady) {
      std::shared_ptr<Buffer> reply = bu.buffer;
      if (nullptr == reply) {
        res->length = 0;
        ret = bu.returnCode;
      } else if (reply->size <= SOMEIP_SF_MAX) {
        if (reply->size <= res->length) {
          memcpy(res->data, reply->data, reply->size);
          res->length = reply->size;
          ret = E_OK;
        } else {
          usLOG(ERROR, "response buffer too small for request %x\n", requestId);
          ret = SOMEIP_E_NOMEM;
        }
        m_ResponseMap.erase(requestId);
      } else {
        res->length = reply->size;
        res->data = (uint8_t *)reply->data;
        ret = E_OK;
      }
    }
    return ret;
  }

  Std_ReturnType onRequest(uint32_t requestId, SomeIp_MessageType *req, SomeIp_MessageType *res) {
    auto msg = transform(requestId, req, m_MethodId, MessageType::REQUEST, m_RequestMap);
    m_Server->onRequest(msg);
    return onAsyncRequest(requestId, res);
  }

  void onFireForgot(uint32_t requestId, SomeIp_MessageType *req) {
    auto msg = transform(requestId, req, m_MethodId, MessageType::REQUEST, m_RequestMap);
    m_Server->onFireForgot(msg);
  }

  Std_ReturnType copy_request(uint32_t requestId, SomeIp_TpMessageType *msg) {
    Std_ReturnType ret = E_OK;
    auto buffer = get(requestId, m_BufferPool, m_RequestMap);
    if (nullptr != buffer) {
      ret = copy_from(requestId, m_RequestMap, m_Lock, msg);
    } else {
      ret = SOMEIP_E_NOMEM;
    }

    return ret;
  }

  void reply(Std_ReturnType returnCode, uint32_t requestId, std::shared_ptr<Buffer> payload) {
    BufferInfo bu = {payload, returnCode};
    m_ResponseMap[requestId] = bu;
  }

private:
  uint16_t m_MethodId;
  server::Server *m_Server;
  BufferPool *m_BufferPool;
  std::map<uint32_t, BufferInfo> m_RequestMap;
  std::map<uint32_t, BufferInfo> m_ResponseMap;
  std::mutex m_Lock;
};

class EventGroupServer {
public:
  EventGroupServer(uint16_t eventGroupId, server::Server *server)
    : m_EventGroupId(eventGroupId), m_Server(server) {
  }

  ~EventGroupServer() {
  }

  void onSubscribe(boolean isSubscribe, TcpIp_SockAddrType *RemoteAddr) {
    if (true == isSubscribe) {
      if (0 == ref.fetch_add(1)) {
        m_Server->onSubscribe(m_EventGroupId, isSubscribe);
      }
    } else {
      if (1 == ref.fetch_sub(1)) {
        m_Server->onSubscribe(m_EventGroupId, isSubscribe);
      }
    }

    usLOG(DEBUG, "event group %d %s, ref %d\n", m_EventGroupId,
          isSubscribe ? "subscribed" : "unsubscribed", ref.load());
  }

private:
  uint16_t m_EventGroupId;
  server::Server *m_Server;
  std::atomic<int> ref = 0;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static std::mutex s_Lock;
static std::map<uint16_t, std::shared_ptr<MethodServer>> s_MethodServerMap;
static std::map<uint32_t, BufferInfo> s_EventMap;
static std::map<uint16_t, std::shared_ptr<EventGroupServer>> s_EventGroupServerMap;
/* ================================ [ LOCALS    ] ============================================== */
std::shared_ptr<MethodServer> get_ms(uint16_t methodId) {
  std::shared_ptr<MethodServer> ms = nullptr;
  std::unique_lock<std::mutex> lck(s_Lock);
  auto it = s_MethodServerMap.find(methodId);
  if (it != s_MethodServerMap.end()) {
    ms = it->second;
  }
  return ms;
}

std::shared_ptr<EventGroupServer> get_egs(uint16_t eventGroupId) {
  std::shared_ptr<EventGroupServer> egs = nullptr;
  std::unique_lock<std::mutex> lck(s_Lock);
  auto it = s_EventGroupServerMap.find(eventGroupId);
  if (it != s_EventGroupServerMap.end()) {
    egs = it->second;
  }
  return egs;
}
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType on_request(uint16_t methodId, uint32_t requestId, SomeIp_MessageType *req,
                          SomeIp_MessageType *res) {
  Std_ReturnType ret = E_NOT_OK;
  auto ms = get_ms(methodId);
  if (nullptr != ms) {
    ret = ms->onRequest(requestId, req, res);
  }

  return ret;
}

Std_ReturnType on_fire_forgot(uint16_t methodId, uint32_t requestId, SomeIp_MessageType *req) {
  Std_ReturnType ret = E_OK;
  auto ms = get_ms(methodId);
  if (nullptr != ms) {
    ms->onFireForgot(requestId, req);
  } else {
    usLOG(ERROR, "method %u has no server for request\n", methodId);
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType on_async_request(uint16_t methodId, uint32_t requestId, SomeIp_MessageType *res) {
  Std_ReturnType ret = E_NOT_OK;
  auto ms = get_ms(methodId);
  if (nullptr != ms) {
    ret = ms->onAsyncRequest(requestId, res);
  }
  return ret;
}

Std_ReturnType on_method_tp_rx_data(uint16_t methodId, uint32_t requestId,
                                    SomeIp_TpMessageType *msg) {
  Std_ReturnType ret = E_NOT_OK;
  auto ms = get_ms(methodId);
  if (nullptr != ms) {
    ret = ms->copy_request(requestId, msg);
  }
  return ret;
}

Std_ReturnType on_method_tp_tx_data(uint16_t methodId, uint32_t requestId,
                                    SomeIp_TpMessageType *msg) {
  Std_ReturnType ret = E_NOT_OK;
  auto ms = get_ms(methodId);
  if (nullptr != ms) {
    ret = ms->copy_response(requestId, msg);
  }
  return ret;
}

Std_ReturnType on_event_tp_tx_data(uint32_t requestId, SomeIp_TpMessageType *msg) {
  return copy_to(requestId, s_EventMap, s_Lock, msg);
}

void on_subscribe(uint16_t eventGroupId, boolean isSubscribe, TcpIp_SockAddrType *RemoteAddr) {
  auto egs = get_egs(eventGroupId);
  if (nullptr != egs) {
    egs->onSubscribe(isSubscribe, RemoteAddr);
  }
}

void Server::listen(uint16_t methodId, BufferPool *bp) {
  std::unique_lock<std::mutex> lck(s_Lock);
  auto it = s_MethodServerMap.find(methodId);
  if (it == s_MethodServerMap.end()) {
    s_MethodServerMap[methodId] = std::make_shared<MethodServer>(methodId, this, bp);
  } else {
    throw std::runtime_error("method " + std::to_string(methodId) + " has already been used");
  }
}

void Server::provide(uint16_t eventGroupId) {
  std::unique_lock<std::mutex> lck(s_Lock);
  auto it = s_EventGroupServerMap.find(eventGroupId);
  if (it == s_EventGroupServerMap.end()) {
    s_EventGroupServerMap[eventGroupId] = std::make_shared<EventGroupServer>(eventGroupId, this);
  } else {
    throw std::runtime_error("event group " + std::to_string(eventGroupId) +
                             " has already been used");
  }
}

void Server::notify(uint32_t requestId, std::shared_ptr<Buffer> buffer) {
  if (buffer->size > SOMEIP_SF_MAX) {
    std::unique_lock<std::mutex> lck(s_Lock);
    s_EventMap[requestId] = {buffer};
  }
  Std_ReturnType ret = SomeIp_Notification(requestId, (uint8_t *)buffer->data, buffer->size);
  if (E_OK != ret) {
    if (buffer->size > SOMEIP_SF_MAX) {
      std::unique_lock<std::mutex> lck(s_Lock);
      s_EventMap.erase(requestId);
    }
  }
}
} /* namespace server */

void Message::reply(Std_ReturnType ercd, std::shared_ptr<Buffer> payload) {
  if (type == MessageType::REQUEST) {
    uint32_t requestId = get_requestId();
    auto ms = server::get_ms(handleId);
    if (nullptr != ms) {
      ms->reply(ercd, requestId, payload);
    } else {
      usLOG(ERROR, "can't find ms for request %x\n", requestId);
    }
  } else {
    usLOG(ERROR, "reply is only allowed for request message\n");
  }
}

} /* namespace usomeip */
} /* namespace as */
