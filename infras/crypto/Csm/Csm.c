/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Crypto Service Manager AUTOSAR CP R20-11
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Csm.h"
#include "Csm_Priv.h"
#include "Det.h"

#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#ifdef CSM_USE_PB_CONFIG
#define CSM_CONFIG csmConfig
#else
#define CSM_CONFIG (&Csm_Config)
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const Csm_ConfigType Csm_Config;
/* ================================ [ DATAS     ] ============================================== */
#ifdef CSM_USE_PB_CONFIG
static const Csm_ConfigType *csmConfig = NULL;
#endif
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Csm_Init(const Csm_ConfigType *configPtr) {
  uint16_t i;
  Std_ReturnType ret;
  const Csm_MacGenerateConfigType *mg;

#ifdef CSM_USE_PB_CONFIG
  if (NULL != configPtr) {
    CSM_CONFIG = configPtr;
  } else {
    CSM_CONFIG = &Csm_Config;
  }
#else
  (void)configPtr;
#endif

  for (i = 0; i < CSM_CONFIG->numOfMacGen; i++) {
    mg = &CSM_CONFIG->MacGenerateConfigs[i];
    mg->Primitive->DeinitFnc(mg->AlgorithmContext); /* Safe to do Deinit before Init */
    ret = mg->Primitive->InitFnc(mg->AlgorithmContext, mg->AlgorithmKey, mg->AlgorithmKeyLength);
    DET_VALIDATE(E_OK == ret, 0x00, CSM_E_INIT_FAILED, (void)ret);
    (void)ret;
  }
}

Std_ReturnType Csm_MacGenerate(uint32_t jobId, Crypto_OperationModeType mode,
                               const uint8_t *dataPtr, uint32_t dataLength, uint8_t *macPtr,
                               uint32_t *macLengthPtr) {
  Std_ReturnType ret = E_OK;
  const Csm_JobConfigType *jobCfg;
  const Csm_MacGenerateConfigType *mg;

  DET_VALIDATE(NULL != CSM_CONFIG, 0x60, CSM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE((NULL != dataPtr) && (dataLength > 0), 0x60, CSM_E_PARAM_POINTER, return E_NOT_OK);
  DET_VALIDATE((NULL != macPtr) && (NULL != macLengthPtr) && (*macLengthPtr > 0), 0x60,
               CSM_E_PARAM_POINTER, return E_NOT_OK);
  DET_VALIDATE(jobId < CSM_CONFIG->numOfJobs, 0x60, CSM_E_PARAM_HANDLE, return E_NOT_OK);
  jobCfg = &CSM_CONFIG->JobConfigs[jobId];
  DET_VALIDATE(CRYPTO_MAC_GENERATE == jobCfg->serviceType, 0x60, CSM_E_SERVICE_TYPE,
               return E_NOT_OK);
  DET_VALIDATE(jobCfg->AlgoRef < CSM_CONFIG->numOfMacGen, 0x60, CSM_E_SERVICE_TYPE,
               return E_NOT_OK);
  mg = &CSM_CONFIG->MacGenerateConfigs[jobCfg->AlgoRef];
  switch (mode) {
  case CRYPTO_OPERATION_MODE_START:
    ret = mg->Primitive->StartFnc(mg->AlgorithmContext);
    break;
  case CRYPTO_OPERATION_MODE_UPDATE:
    ret = mg->Primitive->UpdateFnc(mg->AlgorithmContext, dataPtr, dataLength);
    break;
  case CRYPTO_OPERATION_MODE_FINISH:
    ret = mg->Primitive->FinishFnc(mg->AlgorithmContext, macPtr, macLengthPtr);
    break;
  default:
    DET_VALIDATE(FALSE, 0x60, CSM_E_PARAM_HANDLE, return E_NOT_OK);
    ret = E_NOT_OK;
    break;
  }
  return ret;
}

Std_ReturnType Csm_MacVerify(uint32_t jobId, Crypto_OperationModeType mode, const uint8_t *dataPtr,
                             uint32_t dataLength, const uint8_t *macPtr, const uint32_t macLength,
                             Crypto_VerifyResultType *verifyPtr) {
  Std_ReturnType ret = E_OK;
  const Csm_JobConfigType *jobCfg;
  const Csm_MacGenerateConfigType *mg;
  uint32_t macLen;

  DET_VALIDATE(NULL != CSM_CONFIG, 0x61, CSM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE((NULL != dataPtr) && (dataLength > 0), 0x61, CSM_E_PARAM_POINTER, return E_NOT_OK);
  DET_VALIDATE((NULL != macPtr) && (macLength > 0), 0x61, CSM_E_PARAM_POINTER, return E_NOT_OK);
  DET_VALIDATE(jobId < CSM_CONFIG->numOfJobs, 0x61, CSM_E_PARAM_HANDLE, return E_NOT_OK);
  jobCfg = &CSM_CONFIG->JobConfigs[jobId];
  DET_VALIDATE(CRYPTO_MAC_GENERATE == jobCfg->serviceType, 0x61, CSM_E_SERVICE_TYPE,
               return E_NOT_OK);
  DET_VALIDATE(jobCfg->AlgoRef < CSM_CONFIG->numOfMacGen, 0x61, CSM_E_SERVICE_TYPE,
               return E_NOT_OK);
  mg = &CSM_CONFIG->MacGenerateConfigs[jobCfg->AlgoRef];
  DET_VALIDATE((NULL != mg->MacBuf) && (macLength == mg->AlgorithmKeyLength), 0x61,
               CSM_E_PARAM_HANDLE, return E_NOT_OK);
  macLen = mg->AlgorithmKeyLength;
  switch (mode) {
  case CRYPTO_OPERATION_MODE_START:
    ret = mg->Primitive->StartFnc(mg->AlgorithmContext);
    break;
  case CRYPTO_OPERATION_MODE_UPDATE:
    ret = mg->Primitive->UpdateFnc(mg->AlgorithmContext, dataPtr, dataLength);
    break;
  case CRYPTO_OPERATION_MODE_FINISH:
    ret = mg->Primitive->FinishFnc(mg->AlgorithmContext, mg->MacBuf, &macLen);
    if (E_OK == ret) {
      if (0 != memcmp(mg->MacBuf, macPtr, macLen)) {
        ret = E_NOT_OK;
      }
    }
    break;
  default:
    DET_VALIDATE(FALSE, 0x61, CSM_E_PARAM_HANDLE, return E_NOT_OK);
    ret = E_NOT_OK;
    break;
  }
  return ret;
}