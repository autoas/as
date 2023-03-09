/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#ifdef USE_DOIP
#include "DoIP.h"
#include "Dcm.h"
#include "Std_Timer.h"
#include "Std_Debug.h"
#if defined(_WIN32)
#include <time.h>
#endif
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

#ifndef USE_PDUR
BufReq_ReturnType PduR_DoIPStartOfReception(PduIdType id, const PduInfoType *info,
                                            PduLengthType TpSduLength,
                                            PduLengthType *bufferSizePtr) {
  return Dcm_StartOfReception(id, info, TpSduLength, bufferSizePtr);
}

BufReq_ReturnType PduR_DoIPCopyRxData(PduIdType id, const PduInfoType *info,
                                      PduLengthType *bufferSizePtr) {
  return Dcm_CopyRxData(id, info, bufferSizePtr);
}

BufReq_ReturnType PduR_DoIPCopyTxData(PduIdType id, const PduInfoType *info,
                                      const RetryInfoType *retry, PduLengthType *availableDataPtr) {
  return Dcm_CopyTxData(id, info, retry, availableDataPtr);
}

void PduR_DoIPTxConfirmation(PduIdType id, Std_ReturnType result) {
  Dcm_TpTxConfirmation(id, result);
}

void PduR_DoIPRxIndication(PduIdType id, Std_ReturnType result) {
  Dcm_TpRxIndication(id, result);
}
#endif
Std_ReturnType Dcm_GetVin(uint8_t *Data) {
  static const char *vin = "VIN20210822-PARAI";
  memcpy(Data, vin, 17);
  return E_OK;
}

Std_ReturnType DoIP_UserGetEID(uint8_t *Data) {
  static const char *EID = "EID123";
  memcpy(Data, EID, 6);
  return E_OK;
}

Std_ReturnType DoIP_UserGetGID(uint8_t *Data) {
  static const char *GID = "GID123";
  memcpy(Data, GID, 6);
  return E_OK;
}

Std_ReturnType DoIP_UserGetPowerModeStatus(uint8_t *PowerState) {
  *PowerState = 1;
  return E_OK;
}
#endif /* USE_DOIP */
