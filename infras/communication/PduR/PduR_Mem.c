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
#include "Std_Debug.h"
#if defined(PDUR_USE_MEMPOOL)
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_PDUR 0
#define AS_LOG_PDURI 2
#define AS_LOG_PDURE 3
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void PduR_MemInit(void) {
  const PduR_ConfigType *config = PDUR_CONFIG;
  if (NULL != config->Mc) {
    mc_init(PDUR_CONFIG->Mc);
  }
}

uint8_t *PduR_MemAlloc(uint32_t size) {
  const PduR_ConfigType *config = PDUR_CONFIG;
  uint8_t *ptr = NULL;
  if (NULL != config->Mc) {
    ptr = mc_alloc(config->Mc, size);
    ASLOG(PDUR, ("PduR_MemAlloc %p size=%u\n", ptr, size));
  }
  return ptr;
}

uint8_t *PduR_MemGet(uint32_t *size) {
  const PduR_ConfigType *config = PDUR_CONFIG;
  uint8_t *ptr = NULL;
  if (NULL != config->Mc) {
    ptr = mc_get(config->Mc, size);
  }
  return ptr;
}

void PduR_MemFree(uint8_t *buffer) {
  const PduR_ConfigType *config = PDUR_CONFIG;
  if (NULL != config->Mc) {
    mc_free(config->Mc, buffer);
    ASLOG(PDUR, ("PduR_MemFree %p\n", buffer));
  }
}
#endif
