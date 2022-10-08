/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef _MESSAGE_QUEUE_HPP_
#define _MESSAGE_QUEUE_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include <memory>
#include <queue>
#include <mutex>
#include <map>
#include <condition_variable>
#include <chrono>
#include "Log.hpp"

namespace as {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
template <typename T> class MessageQueue {
public:
  MessageQueue(std::string name) : m_Name(name) {
  }
  ~MessageQueue() {
  }

  void put(T &msg) {
    std::unique_lock<std::mutex> lck(m_Lock);
    m_Queue.push(msg);
    m_CondVar.notify_one();
  }

  bool get(T &out, bool FIFO = true, uint32_t timeoutMs = 1000) {
    bool ret = true;
    std::unique_lock<std::mutex> lck(m_Lock);
    if (false == m_Queue.empty()) {
      pop(out, FIFO);
    } else {
      if (0 == timeoutMs) {
        ret = false;
      } else {
        auto status = m_CondVar.wait_for(lck, std::chrono::milliseconds(timeoutMs));
        if (std::cv_status::timeout != status) {
          if (false == m_Queue.empty()) {
            pop(out, FIFO);
          } else {
            ret = false;
            LOG(DEBUG, "%s: MessageQueue empty\n", m_Name.c_str());
          }
        } else {
          ret = false;
          LOG(DEBUG, "%s: MessageQueue timeout\n", m_Name.c_str());
        }
      }
    }

    return ret;
  }

  void clear() {
    std::unique_lock<std::mutex> lck(m_Lock);
    while (false == m_Queue.empty()) {
      m_Queue.pop();
    }
  }

public:
  static std::shared_ptr<MessageQueue<T>> add(std::string name);

private:
  void pop(T &out, bool FIFO) {
    if (FIFO) {
      out = m_Queue.front();
      m_Queue.pop();
    } else {
      out = m_Queue.back();
      while (false == m_Queue.empty()) {
        m_Queue.pop();
      }
    }
  }

private:
  std::mutex m_Lock;
  std::string m_Name;
  std::queue<T> m_Queue;
  std::condition_variable m_CondVar;

private:
  static std::mutex s_MapLock;
  static std::map<std::string, std::shared_ptr<MessageQueue<T>>> s_MsgQueueMap;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
template <typename T> std::mutex MessageQueue<T>::s_MapLock;
template <typename T>
std::map<std::string, std::shared_ptr<MessageQueue<T>>> MessageQueue<T>::s_MsgQueueMap;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
template <typename T> std::shared_ptr<MessageQueue<T>> MessageQueue<T>::add(std::string name) {
  std::shared_ptr<MessageQueue<T>> ptr = nullptr;
  std::unique_lock<std::mutex> lck(s_MapLock);
  auto it = s_MsgQueueMap.find(name);

  if (it == s_MsgQueueMap.end()) {
    ptr = std::make_shared<MessageQueue<T>>(name);
    s_MsgQueueMap[name] = ptr;
  } else {
    ptr = it->second;
  }

  return ptr;
}
} /* namespace as */
#endif /* _MESSAGE_QUEUE_HPP_ */
