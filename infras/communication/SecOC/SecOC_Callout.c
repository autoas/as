/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Secure Onboard Communication AUTOSAR CP R23-11
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "SecOC.h"
#include "SecOC_Priv.h"
#include "PduR_SecOC.h"
#include "SecOC_Cfg.h"
#include "Det.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType SecOC_GetTxFreshness(uint16_t SecOCFreshnessValueID, uint8_t *SecOCFreshnessValue,
                                    uint32_t *SecOCFreshnessValueLength) {
  Std_ReturnType ret;
  const SecOC_FreshnessValueType *FrVal;
  const SecOC_ConfigType *config = SecOC_GetConfig();
  DET_VALIDATE(NULL != config, 0x52, SECOC_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(SecOCFreshnessValueID < config->numFrVals, 0x52, SECOC_E_PARAM_POINTER,
               return E_NOT_OK);
  DET_VALIDATE((NULL != SecOCFreshnessValue) && (NULL != SecOCFreshnessValueLength), 0x52,
               SECOC_E_PARAM_POINTER, return E_NOT_OK);

  FrVal = &config->FrVals[SecOCFreshnessValueID];
  DET_VALIDATE(*SecOCFreshnessValueLength >= FrVal->length, 0x52, SECOC_E_PARAM_POINTER,
               return E_NOT_OK);
  ret = FrVal->GetFreshnessFnc(SecOCFreshnessValue, SecOCFreshnessValueLength);
  return ret;
}
