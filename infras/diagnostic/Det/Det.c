/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Default Error Tracer AUTOSAR CP R21-11
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DET 3
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType Det_ReportError(uint16_t ModuleId, uint8_t InstanceId, uint8_t ApiId,
                               uint8_t ErrorId) {
  ASLOG(DET, ("Module %" PRIu16 " Instance %02" PRIx8 "h Api %02" PRIx8 "h Error %02" PRIx8 "h\n",
              ModuleId, InstanceId, ApiId, ErrorId));
  return E_OK;
}
