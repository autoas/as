/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "PduR.h"
#include "PduR_DoIP.h"
#include "PduR_Cfg.h"
#include "PduR_Priv.h"
#if defined(PDUR_USE_MEMPOOL)
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void PduR_MemInit(void) {
  const PduR_ConfigType *config = PDUR_CONFIG;
  if (NULL != config->mc) {
    mc_init(PDUR_CONFIG->mc);
  }
}

uint8_t *PduR_MemAlloc(uint32_t size) {
  const PduR_ConfigType *config = PDUR_CONFIG;
  uint8_t *ptr = NULL;
  if (NULL != config->mc) {
    ptr = mc_alloc(config->mc, size);
  }
  return ptr;
}

uint8_t *PduR_MemGet(uint32_t *size) {
  const PduR_ConfigType *config = PDUR_CONFIG;
  uint8_t *ptr = NULL;
  if (NULL != config->mc) {
    ptr = mc_get(config->mc, size);
  }
  return ptr;
}

void PduR_MemFree(uint8_t *buffer) {
  const PduR_ConfigType *config = PDUR_CONFIG;
  if (NULL != config->mc) {
    mc_free(config->mc, buffer);
  }
}
#endif