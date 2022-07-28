/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Driver AUTOSAR CP Release 4.4.0
 */
#ifndef CAN_GENERAL_TYPES_H
#define CAN_GENERAL_TYPES_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* @SWS_Can_00039 */
#define CAN_BUSY 0x02
/* ================================ [ TYPES     ] ============================================== */
typedef enum
{
  CAN_CS_UNINIT,
  CAN_CS_STARTED,
  CAN_CS_STOPPED,
  CAN_CS_SLEEP
} Can_ControllerStateType;

/* @SWS_Can_00429 */
typedef uint16_t Can_HwHandleType;

/* @SWS_Can_00416 */
typedef uint32_t Can_IdType;

/* @SWS_Can_00415 */
typedef struct {
  PduIdType swPduHandle;
  uint8_t length;
  Can_IdType id;
  uint8_t *sdu;
} Can_PduType;

/* @SWS_CAN_00496 */
typedef struct {
  Can_IdType CanId;
  Can_HwHandleType Hoh;
  uint8_t ControllerId;
} Can_HwType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#ifdef __cplusplus
}
#endif
#endif /* CAN_GENERAL_TYPES_H */