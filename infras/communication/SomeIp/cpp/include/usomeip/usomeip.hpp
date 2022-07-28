/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef _USOMEIP_HPP_
#define _USOMEIP_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include "SomeIp.h"
#include "Sd.h"
#include "Log.hpp"
#include "Buffer.hpp"
#include "BufferPool.hpp"
#include <stdint.h>
#include <string.h>
#include <memory>
#include <vector>
#include <map>
#include <functional>
#include <thread>
#include <chrono>
#include <mutex>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "osal.h"

namespace as {
namespace usomeip {
using namespace as;
/* ================================ [ MACROS    ] ============================================== */
#define usLOG(level, ...) LOG(level, "usomeip: " __VA_ARGS__)
/* ================================ [ TYPES     ] ============================================== */
typedef enum
{
  REQUEST,
  REQUEST_NO_RETURN,
  RESPONSE,
  NOTIFICATION,
  ERROR
} MessageType;

struct Message {
public:
  uint16_t handleId;
  uint16_t clientId;
  uint16_t sessionId;
  std::shared_ptr<Buffer> payload = nullptr;
  MessageType type;

public:
  Message(uint16_t handleId, uint32_t requestId, uint8_t *data, uint32_t len, MessageType type)
    : handleId(handleId), type(type) {
    clientId = (uint16_t)(requestId >> 16);
    sessionId = (uint16_t)(requestId & 0xFFFF);
    payload = std::make_shared<Buffer>(len);
    memcpy(payload->data, data, len);
  }

  Message(uint16_t handleId, uint32_t requestId, std::shared_ptr<Buffer> payload, MessageType type)
    : handleId(handleId), payload(payload), type(type) {
    clientId = (uint16_t)(requestId >> 16);
    sessionId = (uint16_t)(requestId & 0xFFFF);
  }

  Message(uint16_t handleId, uint32_t requestId, MessageType type)
    : handleId(handleId), payload(nullptr), type(type) {
    clientId = (uint16_t)(requestId >> 16);
    sessionId = (uint16_t)(requestId & 0xFFFF);
  }

  uint32_t get_requestId() {
    return ((uint32_t)clientId << 16) + sessionId;
  }

  std::string str(size_t max = 16) {
    std::stringstream ss;
    ss << "message with Handle/Client/Session [";
    ss << std::setw(4) << std::setfill('0') << std::hex << handleId << "/";
    ss << std::setw(4) << std::setfill('0') << std::hex << clientId << "/";
    ss << std::setw(4) << std::setfill('0') << std::hex << sessionId << "] ";
    if (nullptr != payload) {
      size_t len = payload->size;
      uint8_t *data = (uint8_t *)payload->data;
      ss << "payload " << std::dec << len << ": ";
      if (len > max) {
        len = max;
      }
      for (size_t i = 0; i < len; i++) {
        ss << std::setw(2) << std::setfill('0') << std::hex << (int)data[i];
      }
    }
    return ss.str();
  }

  void reply(Std_ReturnType returnCode, std::shared_ptr<Buffer> payload = nullptr);
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */

} // namespace usomeip
} /* namespace as */
#endif /* _USOMEIP_HPP_ */
