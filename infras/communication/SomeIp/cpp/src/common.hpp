/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef _USOMEIP_COMMON_HPP_
#define _USOMEIP_COMMON_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include "usomeip/usomeip.hpp"
namespace as {
namespace usomeip {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
struct BufferInfo {
  std::shared_ptr<Buffer> buffer;
  Std_ReturnType returnCode;
};

class CSHelper {

public:
  std::shared_ptr<Buffer> get(uint32_t requestId, BufferPool *bufferPool,
                              std::map<uint32_t, BufferInfo> &bufferMap) {
    std::shared_ptr<Buffer> buffer = nullptr;
    std::unique_lock<std::mutex> lck(m_Lock);
    auto it = bufferMap.find(requestId);
    if (it == bufferMap.end()) {
      if (bufferPool != nullptr) {
        buffer = bufferPool->get();
        if (nullptr != buffer) {
          bufferMap[requestId] = {buffer, E_OK};
        } else {
          usLOG(ERROR, "all buffer is busy for %x\n", requestId);
        }
      } else {
        usLOG(ERROR, "no TP buffer provided for %x\n", requestId);
      }
    } else {
      auto &bu = it->second;
      buffer = bu.buffer;
    }
    return buffer;
  }

  std::shared_ptr<Buffer> poll(uint32_t requestId, std::map<uint32_t, BufferInfo> &bufferMap) {
    std::shared_ptr<Buffer> buffer = nullptr;
    std::unique_lock<std::mutex> lck(m_Lock);
    auto it = bufferMap.find(requestId);
    if (it != bufferMap.end()) {
      auto &bu = it->second;
      buffer = bu.buffer;
    }
    return buffer;
  }

  void put(uint32_t requestId, std::map<uint32_t, BufferInfo> &bufferMap) {
    std::unique_lock<std::mutex> lck(m_Lock);
    auto it = bufferMap.find(requestId);
    if (it != bufferMap.end()) {
      bufferMap.erase(it);
    }
  }

  std::shared_ptr<Message> transform(uint32_t requestId, SomeIp_MessageType *msg, uint16_t handleId,
                                     MessageType msgType,
                                     std::map<uint32_t, BufferInfo> &bufferMap) {
    std::shared_ptr<Message> pMsg = nullptr;
    auto buffer = poll(requestId, bufferMap);
    if (nullptr != buffer) {
      if (buffer->data != msg->data) {
        usLOG(ERROR, "request %x through TP, but buffer mismatch\n", requestId);
      } else {
        put(requestId, bufferMap);
        buffer->size = msg->length;
        pMsg = std::make_shared<Message>(handleId, requestId, buffer, msgType);
      }
    } else {
      pMsg = std::make_shared<Message>(handleId, requestId, msg->data, msg->length,
                                       MessageType::REQUEST);
    }
    return pMsg;
  }

protected:
  std::mutex m_Lock;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType copy_to(uint32_t requestId, std::map<uint32_t, BufferInfo> &bufferMap,
                       std::mutex &lock, SomeIp_TpMessageType *msg);
Std_ReturnType copy_from(uint32_t requestId, std::map<uint32_t, BufferInfo> &bufferMap,
                         std::mutex &lock, SomeIp_TpMessageType *msg);
} // namespace usomeip
} /* namespace as */
#endif /* _USOMEIP_COMMON_HPP_ */
