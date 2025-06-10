/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Module XCP AUTOSAR CP R23-11
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Xcp.h"
#include "Xcp_Priv.h"
#include "Std_Debug.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_XCP 0
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType Xcp_MtaRead(uint8_t extension, uint32_t address, uint8_t *data, Xcp_MsgLenType len,
                           Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;

  ASLOG(XCP, ("MTA Read(%d, %x, %d)\n", extension, (unsigned int)address, len));
#if defined(linux) || defined(_WIN32)
#else
  switch (extension) {
  case XCP_MTA_EXT_MEMORY:
  case XCP_MTA_EXT_FLASH:
    memcpy(data, (void *)address, len);
    break;
  default:
    ret = E_NOT_OK;
    *nrc = XCP_E_OUT_OF_RANGE;
    break;
  }
#endif
  return ret;
}

Std_ReturnType Xcp_MtaWrite(uint8_t extension, uint32_t address, uint8_t *data, Xcp_MsgLenType len,
                            Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;

  ASLOG(XCP, ("MTA Write(%d, %x, %d)\n", extension, (unsigned int)address, len));
#if defined(linux) || defined(_WIN32)
#else
  switch (extension) {
  case XCP_MTA_EXT_MEMORY:
    memcpy((void *)address, data, len);
    break;
  default:
    ret = E_NOT_OK;
    *nrc = XCP_E_OUT_OF_RANGE;
    break;
  }
#endif
  return ret;
}
