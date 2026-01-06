/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ARA_COM_SERVICE_FIND_SERVICE_HANDLE_HPP
#define ARA_COM_SERVICE_FIND_SERVICE_HANDLE_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include <cstdint>

namespace ara {
namespace com {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ CLASS    ] ============================================== */
/* @SWS_CM_00303 */
struct FindServiceHandle {
public:
  /** @SWS_CM_00353 */
  FindServiceHandle() = delete;

  /** @SWS_CM_11528 */
  FindServiceHandle &operator=(const FindServiceHandle &other);

  /** @SWS_CM_11527 */
  bool operator<(const FindServiceHandle &other) const;

  /** @SWS_CM_11526 */
  bool operator==(const FindServiceHandle &other) const;

private:
  uint64_t m_serviceId;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace com
} // namespace ara
#endif /* ARA_COM_SERVICE_FIND_SERVICE_HANDLE_HPP */
