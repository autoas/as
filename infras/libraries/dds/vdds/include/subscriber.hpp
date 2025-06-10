/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef _VRING_DDS_SUBSCRIBER_HPP_
#define _VRING_DDS_SUBSCRIBER_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include "vring/spmc/reader.hpp"
#include "vring/spsc/reader.hpp"
#include <map>
#include <mutex>
#include <string>

#include "Std_Debug.h"

using namespace as::vdds::vring;

namespace as {
namespace vdds {
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_VSUBE 3
/* ================================ [ TYPES     ] ============================================== */
typedef struct SubscriberOptions {
public:
  SubscriberOptions(uint32_t queueDepth = 8) : queueDepth(queueDepth) {
  }

public:
  uint32_t queueDepth = 8;
} SubscriberOptions_t;

template <typename T, typename VRingReader = vring::spmc::Reader> class Subscriber {
public:
  Subscriber(std::string topicName,
             const SubscriberOptions_t &subscriberOptions = SubscriberOptions());
  ~Subscriber();

  int init();

  int receive(T *&sample, uint32_t timeoutMs = 1000);
  int receive(T *&sample, size_t &size, uint32_t timeoutMs = 1000);

  int release(T *sample);

  uint32_t idx(T *sample);

private:
  std::string m_TopicName;
  VRingReader m_Reader;
  std::mutex m_Mutex;
  std::map<T *, uint32_t> m_IdxMap;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
template <typename T, typename VRingReader>
Subscriber<T, VRingReader>::Subscriber(std::string topicName,
                                       const SubscriberOptions_t &subscriberOptions)
  : m_TopicName(topicName), m_Reader(topicName, subscriberOptions.queueDepth) {
}

template <typename T, typename VRingReader> Subscriber<T, VRingReader>::~Subscriber() {
}

template <typename T, typename VRingReader> int Subscriber<T, VRingReader>::init() {
  return m_Reader.init();
}

template <typename T, typename VRingReader>
int Subscriber<T, VRingReader>::receive(T *&sample, uint32_t timeoutMs) {
  int ret = 0;
  uint32_t idx;
  uint32_t len;

  ret = m_Reader.get(sample, idx, len, timeoutMs);
  if (0 == ret) {
    std::unique_lock<std::mutex> lck(m_Mutex);
    m_IdxMap[sample] = idx;
  }

  return ret;
}

template <typename T, typename VRingReader>
int Subscriber<T, VRingReader>::receive(T *&sample, size_t &size, uint32_t timeoutMs) {
  int ret = 0;
  uint32_t idx = -1;
  uint32_t len = 0;

  ret = m_Reader.get((void *&)sample, idx, len, timeoutMs);
  if (0 == ret) {
    std::unique_lock<std::mutex> lck(m_Mutex);
    m_IdxMap[sample] = idx;
    size = len;
  }

  return ret;
}

template <typename T, typename VRingReader> int Subscriber<T, VRingReader>::release(T *sample) {
  int ret = 0;
  uint32_t idx;

  std::unique_lock<std::mutex> lck(m_Mutex);
  auto it = m_IdxMap.find(sample);
  if (it != m_IdxMap.end()) {
    idx = it->second;
    m_Reader.put(idx);
  } else {
    ASLOG(VSUBE, ("%s: invalid sample\n", m_TopicName.c_str()));
    ret = EINVAL;
  }

  return ret;
}

template <typename T, typename VRingReader> uint32_t Subscriber<T, VRingReader>::idx(T *sample) {
  uint32_t idx_ = -1;
  std::unique_lock<std::mutex> lck(m_Mutex);
  auto it = m_IdxMap.find(sample);
  if (it != m_IdxMap.end()) {
    idx_ = it->second;
  } else {
    ASLOG(VSUBE, ("%s: invalid sample\n", m_TopicName.c_str()));
  }

  return idx_;
}

} // namespace vdds
} // namespace as
#endif /* _VRING_DDS_SUBSCRIBER_HPP_ */
