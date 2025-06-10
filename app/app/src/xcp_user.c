/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Xcp.h"
#include "Std_Timer.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static uint32_t lXcpSeed = 0xa1b2c3d4;
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType App_XcpGetConnectPermission(uint8_t mode, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  if ((0x00 == mode) || (0x01 == mode)) {
    /* support both normal and user defined mode */
  } else {
    ret = E_NOT_OK;
    *nrc = XCP_E_OUT_OF_RANGE;
  }
  return ret;
}

Std_ReturnType App_XcpDisconnecNotification(void) {
  return E_OK;
}

Std_ReturnType App_XcpGetSeed(uint8_t mode, uint8_t resource, uint8_t *seed, uint16_t *seedLen,
                              Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  uint32_t u32Seed; /* intentional not initialized to use the stack random value */
  uint32_t u32Time;

  if (0 == mode) {
    u32Time = Std_GetTime();
    lXcpSeed = lXcpSeed ^ u32Seed ^ u32Time ^ 0xfeedbeef;
    seed[0] = (uint8_t)(lXcpSeed >> 24);
    seed[1] = (uint8_t)(lXcpSeed >> 16);
    seed[2] = (uint8_t)(lXcpSeed >> 8);
    seed[3] = (uint8_t)(lXcpSeed);
    *seedLen = 4;
  } else {
    ret = E_NOT_OK;
    *nrc = XCP_E_OUT_OF_RANGE;
  }

  return ret;
}

Std_ReturnType App_CompareKey(uint8_t *key, uint16_t keyLen, Xcp_NegativeResponseCodeType *nrc) {
  Std_ReturnType ret = E_OK;
  uint32_t u32Key = ((uint32_t)key[0] << 24) + ((uint32_t)key[1] << 16) + ((uint32_t)key[2] << 8) +
                    ((uint32_t)key[3]);
  uint32_t u32KeyExpected = lXcpSeed ^ 0x78934673;

  if (4 == keyLen) {
    if (u32KeyExpected != u32Key) {
      ret = E_NOT_OK;
      *nrc = XCP_E_OUT_OF_RANGE;
    }
  } else {
    ret = E_NOT_OK;
    *nrc = XCP_E_OUT_OF_RANGE;
  }
  return ret;
}

Std_ReturnType Xcp_GetProgramResetPermission(Xcp_OpStatusType opStatus,
                                             Xcp_NegativeResponseCodeType *nrc) {
  return E_OK;
}

Std_ReturnType Xcp_GetProgramStartPermission(Xcp_OpStatusType opStatus,
                                             Xcp_NegativeResponseCodeType *nrc) {
  return E_OK;
}
