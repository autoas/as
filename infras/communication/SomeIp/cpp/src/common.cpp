/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "./common.hpp"
namespace as {
namespace usomeip {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType copy_to(uint32_t requestId, std::map<uint32_t, BufferInfo> &bufferMap,
                       std::mutex &lock, SomeIp_TpMessageType *msg) {
  Std_ReturnType ret = E_OK;
  BufferInfo bu;
  bool ready = false;
  {
    std::unique_lock<std::mutex> lck(lock);
    auto it = bufferMap.find(requestId);
    if (it != bufferMap.end()) {
      bu = it->second;
      ready = true;
    }
  }
  if (ready) {
    std::shared_ptr<Buffer> buffer = bu.buffer;
    if (nullptr == msg) {
      std::unique_lock<std::mutex> lck(lock);
      bufferMap.erase(requestId);
      usLOG(ERROR, "request %x TP copy to abort\n", requestId);
    } else if (nullptr != buffer) {
      if ((msg->offset + msg->length) <= buffer->size) {
        memcpy(msg->data, &((uint8_t *)buffer->data)[msg->offset], msg->length);
        if (false == msg->moreSegmentsFlag) {
          std::unique_lock<std::mutex> lck(lock);
          bufferMap.erase(requestId);
        }
      } else {
        usLOG(ERROR, "copy to for request %x but buffer overflow\n", requestId);
        ret = E_NOT_OK;
        std::unique_lock<std::mutex> lck(lock);
        bufferMap.erase(requestId);
      }
    } else {
      usLOG(ERROR, "copy to for request %x but null buffer\n", requestId);
      ret = E_NOT_OK;
      std::unique_lock<std::mutex> lck(lock);
      bufferMap.erase(requestId);
    }
  } else {
    usLOG(ERROR, "copy to for request %x but no buffer\n", requestId);
    ret = E_NOT_OK;
  }
  return ret;
}

Std_ReturnType copy_from(uint32_t requestId, std::map<uint32_t, BufferInfo> &bufferMap,
                         std::mutex &lock, SomeIp_TpMessageType *msg) {
  Std_ReturnType ret = E_OK;
  BufferInfo bu;
  bool ready = false;
  {
    std::unique_lock<std::mutex> lck(lock);
    auto it = bufferMap.find(requestId);
    if (it != bufferMap.end()) {
      bu = it->second;
      ready = true;
    }
  }
  if (ready) {
    std::shared_ptr<Buffer> buffer = bu.buffer;
    if (nullptr == msg) {
      std::unique_lock<std::mutex> lck(lock);
      bufferMap.erase(requestId);
      usLOG(ERROR, "request %x TP copy from abort\n", requestId);
    } else if (nullptr != buffer) {
      if ((msg->offset + msg->length) <= buffer->size) {
        memcpy(&((uint8_t *)buffer->data)[msg->offset], msg->data, msg->length);
        if (false == msg->moreSegmentsFlag) {
          msg->data = (uint8_t *)buffer->data;
        }
      } else {
        usLOG(ERROR, "copy from for request %x but buffer overflow\n", requestId);
        ret = E_NOT_OK;
        std::unique_lock<std::mutex> lck(lock);
        bufferMap.erase(requestId);
      }
    } else {
      usLOG(ERROR, "copy from for request %x but null buffer\n", requestId);
      ret = E_NOT_OK;
      std::unique_lock<std::mutex> lck(lock);
      bufferMap.erase(requestId);
    }
  } else {
    usLOG(ERROR, "copy from for request %x but no buffer\n", requestId);
    ret = E_NOT_OK;
  }
  return ret;
}
} // namespace usomeip
} /* namespace as */
