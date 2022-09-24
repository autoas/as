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
namespace client {
using namespace as::usomeip;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class Client {
public:
  Client() = default;
  ~Client() = default;

  virtual void onResponse(std::shared_ptr<Message> msg) = 0;
  virtual void onNotification(std::shared_ptr<Message> msg) = 0;
  virtual void onError(std::shared_ptr<Message> msg) = 0;
  virtual void onAvailability(bool isAvailable) = 0;

  void identity(uint16_t clientId);
  void require(uint16_t serviceId);
  void bind(uint16_t methodId, BufferPool *bp = nullptr);
  void listen(uint16_t eventId, BufferPool *bp = nullptr);
  void subscribe(uint16_t eventGroupId);

  void request(uint32_t requestId, std::shared_ptr<Buffer> buffer);
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* for client, the requestId is clientId + sessionId */
Std_ReturnType on_response(uint16_t methodId, uint32_t requestId, SomeIp_MessageType *res);

Std_ReturnType on_error(uint16_t methodId, uint32_t requestId, Std_ReturnType ercd);

Std_ReturnType on_method_tp_rx_data(uint16_t methodId, uint32_t requestId,
                                    SomeIp_TpMessageType *msg);

Std_ReturnType on_method_tp_tx_data(uint16_t methodId, uint32_t requestId,
                                    SomeIp_TpMessageType *msg);

Std_ReturnType on_event_tp_rx_data(uint16_t eventId, uint32_t requestId, SomeIp_TpMessageType *msg);

Std_ReturnType on_notification(uint16_t eventId, uint32_t requestId, SomeIp_MessageType *res);

void on_availability(uint16_t clientId, boolean isAvailable);
} // namespace client
} // namespace usomeip
} /* namespace as */
#endif /* _USOMEIP_SERVER_HPP_ */
