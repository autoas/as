/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ARA_COM_PROXY_BASE_HPP
#define ARA_COM_PROXY_BASE_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include <functional>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <deque>
#include <atomic>
#include "ara/core/result.h"
#include "ara/core/future.h"
#include "ara/core/instance_specifier.h"
#include "ara/com/types.h"
#include "ara/com/service/instance_identifier.h"
#include "ara/com/service/sample_ptr.h"

namespace ara {
namespace com {
namespace proxy {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ CLASS    ] ============================================== */
class ProxyEventBase {
public:
  /** @SWS_CM_00141 */
  ara::core::Result<void> Subscribe(std::size_t maxSampleCount) noexcept;

  /** @SWS_CM_00316 */
  ara::com::SubscriptionState GetSubscriptionState() const noexcept;

  /** @SWS_CM_00151 */
  void Unsubscribe() noexcept;

  /** @SWS_CM_11603 */
  std::size_t GetFreeSampleCount() const noexcept;

  /** @SWS_CM_11605 */
  ara::core::Result<void> SetReceiveHandler(ara::com::EventReceiveHandler handler) noexcept;

  /** @SWS_CM_11606 */
  ara::core::Result<void> UnsetReceiveHandler() noexcept;

  /** @SWS_CM_12008 */
  ara::core::Result<void>
  SetSubscriptionStateChangeHandler(ara::com::SubscriptionStateChangeHandler handler) noexcept;

  /** @SWS_CM_12010 */
  void UnsetSubscriptionStateChangeHandler() noexcept;

  /** @SWS_CM_11610 */
  template <typename F>
  ara::core::Result<std::size_t>
  GetNewSamples(F &&f, std::size_t maxNumberOfSamples = std::numeric_limits<std::size_t>::max());
};

class ProxyMethodBase {
public:
  /** One-Way aka Fire-and-Forget Methods */
  ara::core::Result<void> operator()();

  /** Request with return */
  template <typename OutputT> ara::core::Future<OutputT> operator()(...);
};

class HandleTypeBase { /** @SWS_CM_00312 */
public:
  /** @SWS_CM_00317 */
  HandleTypeBase(const HandleTypeBase &other);

  /** @SWS_CM_00318 */
  HandleTypeBase(HandleTypeBase &&other);

  /** @SWS_CM_00349 */
  HandleTypeBase() = delete;

  /** @SWS_CM_11532 */
  HandleTypeBase &operator=(const HandleTypeBase &other);

  /** @SWS_CM_11533 */
  HandleTypeBase &operator=(HandleTypeBase &&other);

  /** @SWS_CM_11371 */
  ~HandleTypeBase() noexcept;

  /** @SWS_CM_11531 */
  const ara::com::InstanceIdentifier &GetInstanceId() const;

  /** @SWS_CM_11530 */
  bool operator<(const HandleTypeBase &other) const;

  /** @SWS_CM_11529 */
  bool operator==(const HandleTypeBase &other) const;
};

template <typename HandleType> class ProxyBase {
public:
  /** @SWS_CM_00123 */
  static ara::core::Result<ara::com::FindServiceHandle>
  StartFindService(ara::com::FindServiceHandler<HandleType> handler,
                   ara::com::InstanceIdentifier instanceId) noexcept;

  /** @SWS_CM_00623 */
  static ara::core::Result<ara::com::FindServiceHandle>
  StartFindService(ara::com::FindServiceHandler<HandleType> handler,
                   ara::core::InstanceSpecifier instanceSpec);

  /** @SWS_CM_11365 */
  template <typename ExecutorT>
  static ara::com::FindServiceHandle
  StartFindService(ara::com::FindServiceHandler<HandleType> handler,
                   ara::core::InstanceSpecifier instance, ExecutorT &&executor) noexcept;

  /** @SWS_CM_11352 */
  template <typename ExecutorT>
  static ara::com::FindServiceHandle
  StartFindService(ara::com::FindServiceHandler<HandleType> handler,
                   ara::com::InstanceIdentifier instance, ExecutorT &&executor) noexcept;

  /** @SWS_CM_00125 */
  static void StopFindService(ara::com::FindServiceHandle handle) noexcept;

  /** @SWS_CM_00122 */
  static ara::core::Result<ara::com::ServiceHandleContainer<HandleType>>
  FindService(ara::com::InstanceIdentifier instance) noexcept;

  /** @SWS_CM_00622 */
  static ara::core::Result<ara::com::ServiceHandleContainer<HandleType>>
  FindService(ara::core::InstanceSpecifier instanceSpec);

  explicit ProxyBase(HandleType &handle);

  ProxyBase(ProxyBase &other) = delete;

  ProxyBase &operator=(const ProxyBase &other) = delete;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace proxy
} // namespace com
} // namespace ara
#endif /* ARA_COM_PROXY_BASE_HPP */
