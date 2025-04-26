/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Crypto Service Manager AUTOSAR CP R20-11
 */
#ifndef CSM_PRIV_H
#define CSM_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Crypto_GeneralTypes.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef DET_THIS_MODULE_ID
#define DET_THIS_MODULE_ID MODULE_ID_CSM
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef Std_ReturnType (*Csm_MacGenerateInitFncType)(void *AlgorithmContext,
                                                     const uint8_t *AlgorithmKey,
                                                     uint32_t AlgorithmKeyLength);

typedef Std_ReturnType (*Csm_MacGenerateStartFncType)(void *AlgorithmContext);

typedef Std_ReturnType (*Csm_MacGenerateUpdateFncType)(void *AlgorithmContext, const uint8_t *data,
                                                       uint32_t len);

typedef Std_ReturnType (*Csm_MacGenerateFinishFncType)(void *AlgorithmContext, uint8_t *mac,
                                                       uint32_t *len);

typedef void (*Csm_MacGenerateDeinitFncType)(void *AlgorithmContext);

typedef struct {
  Csm_MacGenerateInitFncType InitFnc;
  Csm_MacGenerateStartFncType StartFnc;
  Csm_MacGenerateUpdateFncType UpdateFnc;
  Csm_MacGenerateFinishFncType FinishFnc;
  Csm_MacGenerateDeinitFncType DeinitFnc;
} Csm_MacGenPrimitiveType;

typedef struct {
  void *AlgorithmContext;
  const uint8_t *AlgorithmKey;
  const Csm_MacGenPrimitiveType *Primitive;
  uint8_t* MacBuf;
  uint16_t AlgorithmKeyLength; /* Size of the MAC key in bytes */
  Crypto_AlgorithmFamilyType AlgorithmFamily;
  Crypto_AlgorithmModeType AlgorithmMode;
} Csm_MacGenerateConfigType;

typedef struct {
  uint16_t AlgoRef;
  Crypto_ServiceInfoType serviceType;
} Csm_JobConfigType;

struct Csm_Config_s {
  const Csm_JobConfigType *JobConfigs;
  const Csm_MacGenerateConfigType *MacGenerateConfigs;
  uint16_t numOfJobs;
  uint16_t numOfMacGen;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* CSM_PRIV_H */
