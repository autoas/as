/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ARA_COM_SKELETON_BASE_HPP
#define ARA_COM_SKELETON_BASE_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include <functional>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <deque>
#include <atomic>
#include "ara/core/result.h"
#include "ara/core/instance_specifier.h"
#include "ara/com/service/instance_identifier.h"
#include "ara/com/types.h"
#include "ara/core/future.h"

namespace ara {
namespace com {
namespace skeleton {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ CLASS    ] ============================================== */
class SkeletonBase {
public:
  SkeletonBase(
    ara::com::InstanceIdentifier instanceId,
    ara::com::MethodCallProcessingMode mode = ara::com::MethodCallProcessingMode::kEvent);

  static ara::core::Result<SkeletonBase> Create(
    const ara::com::InstanceIdentifier &instanceID,
    ara::com::MethodCallProcessingMode mode = ara::com::MethodCallProcessingMode::kEvent) noexcept;

  SkeletonBase(
    ara::com::InstanceIdentifierContainer instanceIds,
    ara::com::MethodCallProcessingMode mode = ara::com::MethodCallProcessingMode::kEvent);

  static ara::core::Result<SkeletonBase> Create(
    const ara::com::InstanceIdentifierContainer &instanceIDs,
    ara::com::MethodCallProcessingMode mode = ara::com::MethodCallProcessingMode::kEvent) noexcept;

  SkeletonBase(
    ara::core::InstanceSpecifier instanceSpec,
    ara::com::MethodCallProcessingMode mode = ara::com::MethodCallProcessingMode::kEvent);

  static ara::core::Result<SkeletonBase> Create(
    const ara::core::InstanceSpecifier &instanceSpec,
    ara::com::MethodCallProcessingMode mode = ara::com::MethodCallProcessingMode::kEvent) noexcept;

  SkeletonBase(const SkeletonBase &other) = delete;

  SkeletonBase &operator=(const SkeletonBase &other) = delete;

  ~SkeletonBase();

  /** @SWS_CM_00101 */
  ara::core::Result<void> OfferService() noexcept;

  /** @SWS_CM_00111 */
  void StopOfferService() noexcept;

  /** @SWS_CM_00199  */
  bool ProcessNextMethodCall() noexcept;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace skeleton
} // namespace com
} // namespace ara
#endif /* ARA_COM_SKELETON_BASE_HPP */
