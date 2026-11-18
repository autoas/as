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
Std_ReturnType SecOC_GetTxFreshness(uint16_t secOCFreshnessValueID, uint8_t *secOCFreshnessValue,
                                    uint32_t *secOCFreshnessValueLength) {
  Std_ReturnType ret;
  const SecOC_FreshnessValueType *FrVal;
  const SecOC_ConfigType *config = SecOC_GetConfig();
  DET_VALIDATE(NULL != config, 0x52, SECOC_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(secOCFreshnessValueID < config->NumFrVals, 0x52, SECOC_E_PARAM_POINTER,
               return E_NOT_OK);
  DET_VALIDATE((NULL != secOCFreshnessValue) && (NULL != secOCFreshnessValueLength), 0x52,
               SECOC_E_PARAM_POINTER, return E_NOT_OK);

  FrVal = &config->FrVals[secOCFreshnessValueID];
  DET_VALIDATE(*secOCFreshnessValueLength >= FrVal->Length, 0x52, SECOC_E_PARAM_POINTER,
               return E_NOT_OK);
  ret = FrVal->GetFreshnessFnc(secOCFreshnessValue, secOCFreshnessValueLength);
  return ret;
}
