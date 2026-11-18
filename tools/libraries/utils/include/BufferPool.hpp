/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef BUFFERPOOL_HPP
#define BUFFERPOOL_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include "Log.hpp"
#include "Buffer.hpp"
#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <mutex>

namespace as {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class BufferPool {
  struct BufferInfo {
    Buffer *BufferPtr = nullptr;
    bool IsBusy = false;
  };

public:
  BufferPool() {
  }
  ~BufferPool() {
    for (size_t i = 0; i < m_BufferInfos.size(); i++) {
      BufferInfo &bufferInfo = m_BufferInfos[i];
      if (nullptr != bufferInfo.BufferPtr) {
        delete bufferInfo.BufferPtr;
      }
    }
  }

  bool Create(std::string name, size_t num, size_t size) {
    bool ret = true;
    m_Name = name;
    m_BufferInfos.resize(num);
    m_Size = size;

    for (size_t i = 0; (i < num) && (true == ret); i++) {
      BufferInfo &bufferInfo = m_BufferInfos[i];
      Buffer *buffer = new Buffer(size, i);
      if ((nullptr == buffer) || (nullptr == buffer->Data)) {
        LOG(ERROR, "%s: OoM for buffer pool\n", m_Name.c_str());
        ret = false;
      } else {
        bufferInfo.BufferPtr = buffer;
      }
    }

    return ret;
  }

  std::shared_ptr<Buffer> Get(void) {
    std::shared_ptr<Buffer> buffer = nullptr;
    std::unique_lock<std::mutex> lck(m_Lock);
    for (auto &bufferInfo : m_BufferInfos) {
      if (false == bufferInfo.IsBusy) {
        bufferInfo.IsBusy = true;
        buffer = std::shared_ptr<Buffer>(bufferInfo.BufferPtr, [&](Buffer *bufferPtr) {
          Deleter(bufferPtr);
        });
        buffer->Size = m_Size;
        break;
      }
    }
    if (nullptr == buffer) {
      LOG(DEBUG, "%s: all buffer is busy\n", m_Name.c_str());
    }
    return buffer;
  }

private:
  void Deleter(Buffer *buffer) {
    if (nullptr != buffer) {
      if (buffer->Idx < m_BufferInfos.size()) {
        auto &bufferInfo = m_BufferInfos[buffer->Idx];
        bufferInfo.IsBusy = false;
      } else {
        LOG(ERROR, "%s: index over flow\n", m_Name.c_str());
      }
    } else {
      LOG(ERROR, "%s: free nullptr\n", m_Name.c_str());
    }
  }

private:
  std::string m_Name;
  std::vector<BufferInfo> m_BufferInfos;
  size_t m_Size;
  std::mutex m_Lock;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace as */
#endif /* BUFFERPOOL_HPP */
