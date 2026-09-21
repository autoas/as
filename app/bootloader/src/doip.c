/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#ifdef USE_DOIP
#include "DoIP.h"
#include "Dcm.h"
#include "Std_Debug.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType DoIP_default_RoutingActivationAuthenticationCallback(
  boolean *Authentified, const uint8_t *AuthenticationReqData, uint8_t *AuthenticationResData) {
  *Authentified = TRUE;
  return E_OK;
}

Std_ReturnType DoIP_default_RoutingActivationConfirmationCallback(
  boolean *Confirmed, const uint8_t *ConfirmationReqData, uint8_t *ConfirmationResData) {
  ASLOG(INFO, ("DOIP default activated\n"));
  *Confirmed = TRUE;
  return E_OK;
}

Std_ReturnType DoIP_CANBL_RoutingActivationAuthenticationCallback(
  boolean *Authentified, const uint8_t *AuthenticationReqData, uint8_t *AuthenticationResData) {
  *Authentified = TRUE;
  return E_OK;
}

Std_ReturnType DoIP_CANBL_RoutingActivationConfirmationCallback(
  boolean *Confirmed, const uint8_t *ConfirmationReqData, uint8_t *ConfirmationResData) {
  ASLOG(INFO, ("DOIP CANBL activated\n"));
  *Confirmed = TRUE;
  return E_OK;
}

Std_ReturnType Dcm_GetVin(uint8_t *Data) {
  static const char *vin = "ASBL0000000000001";
  memcpy(Data, vin, 17);
  return E_OK;
}

Std_ReturnType DoIP_UserGetEID(uint8_t *Data) {
  static const char *EID = "ASBL01";
  memcpy(Data, EID, 6);
  return E_OK;
}

Std_ReturnType DoIP_UserGetGID(uint8_t *Data) {
  static const char *GID = "ASBL01";
  memcpy(Data, GID, 6);
  return E_OK;
}

Std_ReturnType DoIP_UserGetRoutingActivationResponseOem(uint8_t *Data) {
  return E_NOT_OK; /* no OEM data */
}

Std_ReturnType DoIP_UserGetPowerModeStatus(uint8_t *PowerState) {
  *PowerState = 1;
  return E_OK;
}
#endif /* USE_DOIP */
