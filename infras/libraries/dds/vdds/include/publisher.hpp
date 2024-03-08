/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef _VRING_DDS_PUBLISHER_HPP_
#define _VRING_DDS_PUBLISHER_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include "vring.hpp"
#include <string>
#include <map>
#include <mutex>

#include "Std_Debug.h"

namespace as {
namespace vdds {
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_VPUBE 3
/* ================================ [ TYPES     ] ============================================== */
typedef struct PublisherOptions {
public:
  PublisherOptions(uint32_t maxSubscribers = 8, uint32_t queueDepth = 8)
    : maxSubscribers(maxSubscribers), queueDepth(queueDepth) {
  }

public:
  uint32_t maxSubscribers = 8;
  uint32_t queueDepth = 8;
} PublisherOptions_t;

template <typename T> class Publisher {
public:
  Publisher(std::string topicName, const PublisherOptions_t &publisherOptions = PublisherOptions());
  ~Publisher();

  int getLastError();

  T *load(uint32_t timeoutMs = 1000);
  void publish(T *sample);
  void publish(T *sample, size_t size);

  uint32_t idx(T *sample);

private:
  std::string m_TopicName;
  VRingWriter m_Writer;
  std::mutex m_Mutex;
  std::map<T *, uint32_t> m_IdxMap;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
template <typename T>
Publisher<T>::Publisher(std::string topicName, const PublisherOptions_t &publisherOptions)
  : m_TopicName(topicName),
    m_Writer(topicName, sizeof(T), publisherOptions.maxSubscribers, publisherOptions.queueDepth) {
}

template <typename T> Publisher<T>::~Publisher() {
}

template <typename T> int Publisher<T>::getLastError() {
  return m_Writer.getLastError();
}

template <typename T> T *Publisher<T>::load(uint32_t timeoutMs) {
  T *sample;
  uint32_t idx;
  uint32_t len;

  sample = (T *)m_Writer.get(&idx, &len, timeoutMs);
  if (nullptr != sample) {
    std::unique_lock<std::mutex> lck(m_Mutex);
    m_IdxMap[sample] = idx;
  }

  return sample;
}

template <typename T> void Publisher<T>::publish(T *sample) {
  uint32_t idx;
  std::unique_lock<std::mutex> lck(m_Mutex);
  auto it = m_IdxMap.find(sample);
  if (it != m_IdxMap.end()) {
    idx = it->second;
    m_Writer.put(idx, sizeof(T));
  } else {
    ASLOG(VPUBE, ("%s: invalid sample\n", m_TopicName.c_str()));
  }
}

template <typename T> void Publisher<T>::publish(T *sample, size_t size) {
  uint32_t idx;
  std::unique_lock<std::mutex> lck(m_Mutex);
  auto it = m_IdxMap.find(sample);
  if (it != m_IdxMap.end()) {
    idx = it->second;
    m_IdxMap.erase(it);
    m_Writer.put(idx, (uint32_t)size);
  } else {
    ASLOG(VPUBE, ("%s: invalid sample\n", m_TopicName.c_str()));
  }
}

template <typename T> uint32_t Publisher<T>::idx(T *sample) {
  uint32_t idx_ = -1;
  std::unique_lock<std::mutex> lck(m_Mutex);
  auto it = m_IdxMap.find(sample);
  if (it != m_IdxMap.end()) {
    idx_ = it->second;
  } else {
    ASLOG(VPUBE, ("%s: invalid sample\n", m_TopicName.c_str()));
  }

  return idx_;
}

} // namespace vdds
} // namespace as
#endif /* _VRING_DDS_PUBLISHER_HPP_ */
