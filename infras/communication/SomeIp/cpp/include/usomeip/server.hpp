/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef _USOMEIP_SERVER_HPP_
#define _USOMEIP_SERVER_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include "usomeip/usomeip.hpp"
namespace as {
namespace usomeip {
namespace server {
using namespace as::usomeip;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class Server {
public:
  Server() = default;
  ~Server() = default;

  virtual void onRequest(std::shared_ptr<Message> msg) = 0;
  virtual void onFireForgot(std::shared_ptr<Message> msg) = 0;
  virtual void onSubscribe(uint16_t eventGroupId, bool isSubscribe) = 0;

  void identity(uint16_t serviceId);
  void offer(uint16_t serviceId);
  void listen(uint16_t methodId, BufferPool *bp = nullptr);
  void provide(uint16_t eventGroupId);

  // requestId is eventId + sessionId
  void notify(uint32_t requestId, std::shared_ptr<Buffer> buffer);

  void on_connect(uint16_t conId, bool isConnected);

public:
  struct Connection {
    Server *self;
    uint16_t conId;
    osal_thread_t thread;
    bool online;
  };
  void run_rx(Connection *con);

private:
  uint16_t m_Identity = -1;
  std::map<uint16_t, Connection *> m_ConnectionMap;
  std::mutex m_Lock;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* for server, the requestId is clientId + sessionId */
Std_ReturnType on_request(uint16_t methodId, uint32_t requestId, SomeIp_MessageType *req,
                          SomeIp_MessageType *res);

Std_ReturnType on_fire_forgot(uint16_t methodId, uint32_t requestId, SomeIp_MessageType *req);

Std_ReturnType on_async_request(uint16_t methodId, uint32_t requestId, SomeIp_MessageType *res);

Std_ReturnType on_method_tp_rx_data(uint16_t methodId, uint32_t requestId,
                                    SomeIp_TpMessageType *msg);

Std_ReturnType on_method_tp_tx_data(uint16_t methodId, uint32_t requestId,
                                    SomeIp_TpMessageType *msg);

Std_ReturnType on_event_tp_tx_data(uint32_t requestId, SomeIp_TpMessageType *msg);

void on_connect(uint16_t serviceId, uint16_t conId, boolean isConnected);

void on_subscribe(uint16_t eventGroupId, boolean isSubscribe, TcpIp_SockAddrType *RemoteAddr);
} // namespace server
} // namespace usomeip
} /* namespace as */
#endif /* _USOMEIP_SERVER_HPP_ */
