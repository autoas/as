/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ARA_COM_TYPES_HPP
#define ARA_COM_TYPES_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include <functional>
#include "ara/com/service/find_service_handle.h"
#include "ara/com/service/instance_identifier.h"
#include "ara/core/vector.h"

namespace ara {
namespace com {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ CLASS    ] ============================================== */
/** @SWS_CM_00309 @brief Callback function invoked if new event data arrives for an event. */
using EventReceiveHandler = std::function<void()>;

/** @SWS_CM_11615 @brief Callback function invoked if a new notification arrives for a field.  */
using FieldReceiveHandler = std::function<void()>;

/** @SWS_CM_00304 @brief Holds a list of service handles and is used as a return value of any of the
 * FindService() methods) */
template <typename T> using ServiceHandleContainer = ara::core::Vector<T>;

/** @SWS_CM_00383 @brief Callback function invoked if service availability changes.  */
template <typename T>
using FindServiceHandler = std::function<void(ServiceHandleContainer<T>, FindServiceHandle)>;

/** @SWS_CM_00310 */
enum class SubscriptionState : std::uint8_t {
  kSubscribed,
  kNotSubscribed,
  kSubscriptionPending
};

/** @SWS_CM_00311 */
using SubscriptionStateChangeHandler = std::function<void(SubscriptionState)>;

/** @SWS_CM_00301 */
enum class MethodCallProcessingMode : std::uint8_t {
  kPoll,
  kEvent,
  kEventSingleThread,
};

/** @SWS_CM_00319 */
using InstanceIdentifierContainer = ara::core::Vector<InstanceIdentifier>;

/** @SWS_CM_00308 */
template <typename T> using SampleAllocateePtr = std::unique_ptr<T>;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace com
} // namespace ara
#endif /* ARA_COM_TYPES_HPP */
