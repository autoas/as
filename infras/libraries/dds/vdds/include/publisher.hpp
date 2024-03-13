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
  PublisherOptions(uint32_t queueDepth = 8) : queueDepth(queueDepth) {
  }

public:
  uint32_t queueDepth = 8;
} PublisherOptions_t;

template <typename T> class Publisher {
public:
  Publisher(std::string topicName, const PublisherOptions_t &publisherOptions = PublisherOptions());
  ~Publisher();

  int init();

  int load(T *&sample, uint32_t timeoutMs = 1000);
  int publish(T *sample);
  int publish(T *sample, size_t size);

  // API for debug purpose
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
  : m_TopicName(topicName), m_Writer(topicName, sizeof(T), publisherOptions.queueDepth) {
}

template <typename T> Publisher<T>::~Publisher() {
}

template <typename T> int Publisher<T>::init() {
  return m_Writer.init();
}

template <typename T> int Publisher<T>::load(T *&sample, uint32_t timeoutMs) {
  uint32_t idx;
  uint32_t len;
  int ret = 0;

  ret = m_Writer.get((void *&)sample, idx, len, timeoutMs);
  if (0 == ret) {
    std::unique_lock<std::mutex> lck(m_Mutex);
    m_IdxMap[sample] = idx;
  }

  return ret;
}

template <typename T> int Publisher<T>::publish(T *sample) {
  int ret = 0;
  uint32_t idx;

  std::unique_lock<std::mutex> lck(m_Mutex);
  auto it = m_IdxMap.find(sample);
  if (it != m_IdxMap.end()) {
    idx = it->second;
    ret = m_Writer.put(idx, sizeof(T));
  } else {
    ASLOG(VPUBE, ("%s: invalid sample\n", m_TopicName.c_str()));
    ret = EINVAL;
  }

  return ret;
}

template <typename T> int Publisher<T>::publish(T *sample, size_t size) {
  int ret = 0;
  uint32_t idx;

  std::unique_lock<std::mutex> lck(m_Mutex);
  auto it = m_IdxMap.find(sample);
  if (it != m_IdxMap.end()) {
    idx = it->second;
    m_IdxMap.erase(it);
    m_Writer.put(idx, (uint32_t)size);
  } else {
    ASLOG(VPUBE, ("%s: invalid sample\n", m_TopicName.c_str()));
    ret = EINVAL;
  }

  return ret;
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
