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
#include <string>
#include <vector>
#include "Log.hpp"

namespace as {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
template <typename T> class MessageQueue {
public:
  MessageQueue(std::string name, uint32_t capability = 0) : m_Name(name), m_Capability(capability) {
    LOG(DEBUG, "%s: MessageQueue created with capability %d\n", m_Name.c_str(), capability);
  }
  ~MessageQueue() {
  }

  void put(T &msg) {
    std::unique_lock<std::mutex> lck(m_Lock);
    m_Queue.push(msg);
    m_CondVar.notify_one();
    if (m_Capability > 0) {
      if (m_Capability < m_Queue.size()) {
        m_Queue.pop(); /* drop the oldest */
      }
    }
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

  size_t size(void) {
    std::unique_lock<std::mutex> lck(m_Lock);
    return m_Queue.size();
  }

  void clear() {
    std::unique_lock<std::mutex> lck(m_Lock);
    while (false == m_Queue.empty()) {
      m_Queue.pop();
    }
  }

public:
  static std::shared_ptr<MessageQueue<T>> add(std::string name, uint32_t capability = 0);
  static std::shared_ptr<MessageQueue<T>> find(std::string name);
  static std::vector<std::string> topics(void);

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
  uint32_t m_Capability;

private:
  static std::mutex s_MapLock;
  static std::map<std::string, std::shared_ptr<MessageQueue<T>>> s_MsgQueueMap;
};

/* DDS like messgae publish & subscribe */
template <typename T> class MessageBroker {
public:
  MessageBroker(std::string topicName) : m_Name(topicName) {
    LOG(DEBUG, "%s: MessageBroker created\n", m_Name.c_str());
  }
  ~MessageBroker() {
  }

  std::shared_ptr<MessageQueue<T>> create_subscriber(std::string subscriberName,
                                                     uint32_t capability = 0) {
    std::shared_ptr<MessageQueue<T>> sub = nullptr;
    std::unique_lock<std::mutex> lck(m_Lock);
    auto it = m_MsgQueueMap.find(subscriberName);
    if (it == m_MsgQueueMap.end()) {
      sub = std::make_shared<MessageQueue<T>>(subscriberName, capability);
      m_MsgQueueMap[subscriberName] = sub;
      LOG(DEBUG, "%s: subscriber %s created\n", m_Name.c_str(), subscriberName.c_str());
    } else {
      LOG(ERROR, "%s: subscriber %s already exists\n", m_Name.c_str(), subscriberName.c_str());
    }
    return sub;
  }

  void remove_subscriber(std::string subscriberName) {
    std::unique_lock<std::mutex> lck(m_Lock);
    auto it = m_MsgQueueMap.find(subscriberName);
    if (it != m_MsgQueueMap.end()) {
      m_MsgQueueMap.erase(it);
    }
  }

  void put(T &msg) {
    std::unique_lock<std::mutex> lck(m_Lock);
    for (auto it : m_MsgQueueMap) {
      it.second->put(msg);
    }
  }

private:
  std::string m_Name;
  std::mutex m_Lock;
  std::map<std::string, std::shared_ptr<MessageQueue<T>>> m_MsgQueueMap;

public:
  static std::shared_ptr<MessageBroker<T>> add(std::string name);
  static std::vector<std::string> topics(void);

private:
  static std::mutex s_MapLock;
  static std::map<std::string, std::shared_ptr<MessageBroker<T>>> s_BrokerMap;
};

template <typename T> class MessagePublisher {
public:
  MessagePublisher(std::string name) : m_Name(name) {
  }

  ~MessagePublisher() {
  }

  bool create(std::string topicName) {
    bool ret = true;

    m_Broker = MessageBroker<T>::add(topicName);
    if (nullptr == m_Broker) {
      ret = false;
    }

    return ret;
  }

  void put(T &msg) {
    m_Broker->put(msg);
  }

private:
  std::string m_Name;
  std::shared_ptr<MessageBroker<T>> m_Broker = nullptr;
};

template <typename T> class MessageSubscriber {
public:
  MessageSubscriber(std::string name) : m_Name(name) {
  }

  ~MessageSubscriber() {
    if (m_Sub != nullptr) {
      m_Broker->remove_subscriber(m_Name);
    }
  }

  bool create(std::string topicName, uint32_t capability = 0) {
    bool ret = true;

    m_Broker = MessageBroker<T>::add(topicName);
    if (nullptr == m_Broker) {
      ret = false;
    } else {
      m_Sub = m_Broker->create_subscriber(m_Name, capability);
      if (nullptr == m_Sub) {
        ret = false;
      }
    }

    return ret;
  }

  bool get(T &out, bool FIFO = true, uint32_t timeoutMs = 1000) {
    bool ret = true;

    if (nullptr != m_Sub) {
      ret = m_Sub->get(out, FIFO, timeoutMs);
    } else {
      ret = false;
    }

    return ret;
  }

private:
  std::string m_Name;
  std::shared_ptr<MessageBroker<T>> m_Broker = nullptr;
  std::shared_ptr<MessageQueue<T>> m_Sub = nullptr;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
template <typename T> std::mutex MessageQueue<T>::s_MapLock;
template <typename T>
std::map<std::string, std::shared_ptr<MessageQueue<T>>> MessageQueue<T>::s_MsgQueueMap;

template <typename T> std::mutex MessageBroker<T>::s_MapLock;
template <typename T>
std::map<std::string, std::shared_ptr<MessageBroker<T>>> MessageBroker<T>::s_BrokerMap;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
template <typename T>
std::shared_ptr<MessageQueue<T>> MessageQueue<T>::add(std::string name, uint32_t capability) {
  std::shared_ptr<MessageQueue<T>> ptr = nullptr;
  std::unique_lock<std::mutex> lck(s_MapLock);
  auto it = s_MsgQueueMap.find(name);

  if (it == s_MsgQueueMap.end()) {
    ptr = std::make_shared<MessageQueue<T>>(name, capability);
    s_MsgQueueMap[name] = ptr;
  } else {
    ptr = it->second;
  }

  return ptr;
}

template <typename T> std::shared_ptr<MessageQueue<T>> MessageQueue<T>::find(std::string name) {
  std::shared_ptr<MessageQueue<T>> ptr = nullptr;
  std::unique_lock<std::mutex> lck(s_MapLock);
  auto it = s_MsgQueueMap.find(name);

  if (it != s_MsgQueueMap.end()) {
    ptr = it->second;
  }

  return ptr;
}

template <typename T> std::vector<std::string> MessageQueue<T>::topics() {
  std::vector<std::string> ts;
  std::unique_lock<std::mutex> lck(s_MapLock);
  for (auto it : s_MsgQueueMap) {
    ts.push_back(it.first);
  }
  return ts;
}

template <typename T>
std::shared_ptr<MessageBroker<T>> MessageBroker<T>::add(std::string topicName) {
  std::shared_ptr<MessageBroker<T>> ptr = nullptr;
  std::unique_lock<std::mutex> lck(s_MapLock);
  auto it = s_BrokerMap.find(topicName);

  if (it == s_BrokerMap.end()) {
    ptr = std::make_shared<MessageBroker<T>>(topicName);
    s_BrokerMap[topicName] = ptr;
  } else {
    ptr = it->second;
  }

  return ptr;
}

template <typename T> std::vector<std::string> MessageBroker<T>::topics() {
  std::vector<std::string> ts;
  std::unique_lock<std::mutex> lck(s_MapLock);
  for (auto it : s_BrokerMap) {
    ts.push_back(it.first);
  }
  return ts;
}
} /* namespace as */
#endif /* _MESSAGE_QUEUE_HPP_ */
