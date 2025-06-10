/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * ref: https://www.autosar.org/fileadmin/standards/R23-11/FO/AUTOSAR_FO_PRS_E2EProtocol.pdf
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "E2E.h"
#include "E2E_Cfg.h"
#include "E2E_Priv.h"
#include "Crc.h"
#include "Std_Debug.h"
#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#ifdef E2E_USE_PB_CONFIG
#define E2E_CONFIG cantpConfig
#else
#define E2E_CONFIG (&E2E_Config)
#endif

#define AS_LOG_E2E 0
#define AS_LOG_E2EE 2

#define E2E_CHECK_DONE_ONCE 0x80u
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const E2E_ConfigType E2E_Config;
/* ================================ [ DATAS     ] ============================================== */
#ifdef E2E_USE_PB_CONFIG
static const E2E_ConfigType *e2eConfig = NULL;
#endif
/* ================================ [ LOCALS    ] ============================================== */
#if defined(E2E_USE_PROTECT_P11) || defined(E2E_USE_CHECK_P11)
/* @PRS_E2E_00634 */
static uint8_t ComputeP11Crc(const E2E_Profile11ConfigType *config, uint8_t *data,
                             uint16_t length) {
  uint8_t tmp;
  uint8_t crc = 0xFFu;
  uint8_t Counter;

  /* @PRS_E2E_00513, @PRS_E2E_00163 */
  if (E2E_P11_DATAID_BOTH == config->DataIDMode) {
    /* both two bytes (double ID configuration) are included in the CRC, first low byte and then
     * high byte (see variant 1A - PRS_E2EProtocol_00227) */
    tmp = config->DataID & 0xFFu;
    crc = Crc_CalculateCRC8(&tmp, 1u, crc, FALSE);
    tmp = (config->DataID >> 8) & 0xFFu;
    crc = Crc_CalculateCRC8(&tmp, 1u, crc, FALSE);
  } else if (E2E_P11_DATAID_ALT == config->DataIDMode) {
    /* depending on parity of the counter (alternating ID configuration) the high and the low byte
     * is included (see variant 1B - PRS_E2EProtocol_00228). For even counter values the low byte is
     * included and for odd counter values the high byte is included. */
    Counter = (data[config->CounterOffset >> 3] >> (config->CounterOffset & 0x07u)) & 0x0Fu;
    if (0u != (Counter & 0x01u)) { /* even counter */
      tmp = config->DataID & 0xFFu;
      crc = Crc_CalculateCRC8(&tmp, 1u, crc, FALSE);
    } else {
      tmp = (config->DataID >> 8) & 0xFFu;
      crc = Crc_CalculateCRC8(&tmp, 1u, crc, FALSE);
    }
  } else if (E2E_P11_DATAID_LOW == config->DataIDMode) {
    tmp = config->DataID & 0xFFu;
    crc = Crc_CalculateCRC8(&tmp, 1u, crc, FALSE);
  } else { /* NIBBLE */
    tmp = config->DataID & 0xFFu;
    crc = Crc_CalculateCRC8(&tmp, 1u, crc, FALSE);
    tmp = 0;
    crc = Crc_CalculateCRC8(&tmp, 1u, crc, FALSE);
  }

  if (config->CRCOffset >= 8u) {
    crc = Crc_CalculateCRC8(data, config->CRCOffset >> 3, crc, FALSE);
  }

  if ((config->CRCOffset >> 3) < (length - 1u)) {
    crc = Crc_CalculateCRC8(&data[(config->CRCOffset >> 3) + 1u],
                            length - (config->CRCOffset >> 3) - 1u, crc, FALSE);
  }

  crc = crc ^ 0xFFu;

  return crc;
}
#endif

#if defined(E2E_USE_PROTECT_P22) || defined(E2E_USE_CHECK_P22)
/* @PRS_E2E_00531 */
static uint8_t ComputeP22Crc(const E2E_Profile22ConfigType *config, uint8_t *data,
                             uint16_t length) {
  uint8_t crc = 0x00u;
  uint8_t Counter;

  if (config->Offset >= 8u) {
    crc = Crc_CalculateCRC8H2F(data, config->Offset >> 3, crc, FALSE);
  }

  if ((config->Offset >> 3) < (length - 1u)) {
    crc = Crc_CalculateCRC8H2F(&data[(config->Offset >> 3) + 1u],
                               length - (config->Offset >> 3) - 1u, crc, FALSE);
  }

  Counter = data[(config->Offset >> 3) + 1] & 0x0Fu;
  crc = Crc_CalculateCRC8H2F(&config->DataIDList[Counter], 1, crc, FALSE);

  return crc;
}
#endif

#if defined(E2E_USE_PROTECT_P05) || defined(E2E_USE_CHECK_P05)
/* @PRS_E2E_00621 */
static uint16_t ComputeP05Crc(const E2E_Profile05ConfigType *config, uint8_t *data,
                              uint16_t length) {
  uint16_t crc = 0xFFFFu;
  uint8_t tmp;
  uint16_t offset = config->Offset >> 3;

  if (offset > 0) {
    crc = Crc_CalculateCRC16(data, offset, crc, FALSE);
  }
  if (length > (offset + 2u)) {
    crc = Crc_CalculateCRC16(&data[offset + 2u], length - 2u - offset, crc, FALSE);
  }

  tmp = config->DataID & 0xFFu;
  crc = Crc_CalculateCRC16(&tmp, 1, crc, FALSE);

  tmp = (config->DataID >> 8) & 0xFFu;
  crc = Crc_CalculateCRC16(&tmp, 1, crc, FALSE);

  return crc;
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
void E2E_Init(const E2E_ConfigType *config) {
  uint16_t i;
#ifdef E2E_USE_PB_CONFIG
  if (NULL != config) {
    E2E_CONFIG = config;
  } else {
    E2E_CONFIG = &E2E_Config;
  }
#else
  (void)config;
#endif
#ifdef E2E_USE_PROTECT_P11
  for (i = 0; i < E2E_CONFIG->numOfProtectP11; i++) { /* @PRS_E2E_00504 */
    E2E_CONFIG->ProtectP11Configs[i].context->Counter = 0u;
  }
#endif
#ifdef E2E_USE_CHECK_P11
  for (i = 0; i < E2E_CONFIG->numOfCheckP11; i++) { /* @PRS_E2E_00504 */
    E2E_CONFIG->CheckP11Configs[i].context->Counter = 0u;
  }
#endif
#ifdef E2E_USE_PROTECT_P22
  for (i = 0; i < E2E_CONFIG->numOfProtectP22; i++) { /* @PRS_E2E_00523 */
    E2E_CONFIG->ProtectP22Configs[i].context->Counter = 0u;
  }
#endif
#ifdef E2E_USE_CHECK_P22
  for (i = 0; i < E2E_CONFIG->numOfCheckP22; i++) {
    E2E_CONFIG->CheckP22Configs[i].context->Counter = 0u;
  }
#endif
#ifdef E2E_USE_PROTECT_P44
  for (i = 0; i < E2E_CONFIG->numOfProtectP44; i++) {
    E2E_CONFIG->ProtectP44Configs[i].context->Counter = 0u;
  }
#endif
#ifdef E2E_USE_CHECK_P44
  for (i = 0; i < E2E_CONFIG->numOfCheckP44; i++) {
    E2E_CONFIG->CheckP44Configs[i].context->Counter = 0u;
    E2E_CONFIG->CheckP44Configs[i].context->bSynced = FALSE;
  }
#endif
#ifdef E2E_USE_PROTECT_P05
  for (i = 0; i < E2E_CONFIG->numOfProtectP05; i++) {
    E2E_CONFIG->ProtectP05Configs[i].context->Counter = 0u;
  }
#endif
#ifdef E2E_USE_CHECK_P05
  for (i = 0; i < E2E_CONFIG->numOfCheckP05; i++) {
    E2E_CONFIG->CheckP05Configs[i].context->Counter = 0u;
    E2E_CONFIG->CheckP05Configs[i].context->bSynced = FALSE;
  }
#endif
}

#ifdef E2E_USE_PROTECT_P11
Std_ReturnType E2E_P11Protect(E2E_ProfileIdType profileId, uint8_t *data, uint16_t length) {
  Std_ReturnType ret = E_NOT_OK;
  const E2E_ProtectProfile11ConfigType *config;
  DET_VALIDATE(NULL != E2E_CONFIG, 0x11, E2E_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(profileId < E2E_CONFIG->numOfProtectP11, 0x11, E2E_E_PARAM_ID, return E_NOT_OK);
  DET_VALIDATE((NULL != data) && (length > 2u), 0x11, E2E_E_PARAM_POINTER, return E_NOT_OK);
  config = &E2E_CONFIG->ProtectP11Configs[profileId];

  if (E2E_P11_DATAID_NIBBLE == config->P11.DataIDMode) { /* @PRS_E2E_00511 */
    data[config->P11.DataIDNibbleOffset >> 3] &= ~(0xFu << (config->P11.DataIDNibbleOffset & 0x7u));
    data[config->P11.DataIDNibbleOffset >> 3] |= ((config->P11.DataID >> 8) & 0xFu)
                                                 << (config->P11.DataIDNibbleOffset & 0x7u);
  }

  /* @PRS_E2E_00512 */
  data[config->P11.CounterOffset >> 3] &= ~(0xFu << (config->P11.CounterOffset & 0x7u));
  data[config->P11.CounterOffset >> 3] |= (config->context->Counter & 0xFu)
                                          << (config->P11.CounterOffset & 0x7u);

  /* @PRS_E2E_00514 */
  data[config->P11.CRCOffset >> 3] = ComputeP11Crc(&config->P11, data, length);

  ASHEXDUMP(E2E,
            ("[%u]: P11 crc=0x%x DataId=0x%x Mode=%u Counter=%u", profileId,
             data[config->P11.CRCOffset >> 3], config->P11.DataID, config->P11.DataIDMode,
             config->context->Counter),
            data, length);

  /* @PRS_E2E_00515 */
  config->context->Counter++;
  if (config->context->Counter > 0x0E) {
    config->context->Counter = 0;
  }
  ret = E_OK;
  return ret;
}
#endif

#ifdef E2E_USE_CHECK_P11
Std_ReturnType E2E_P11Check(E2E_ProfileIdType profileId, uint8_t *data, uint16_t length) {
  Std_ReturnType ret = E_OK;
  uint8_t ReceivedNibble = 0;
  uint8_t ReceivedCounter = 0;
  uint8_t ReceivedCrc = 0;
  const E2E_CheckProfile11ConfigType *config;
  uint8_t crc;
  uint8_t deltaCounter;
  DET_VALIDATE(NULL != E2E_CONFIG, 0x12, E2E_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(profileId < E2E_CONFIG->numOfCheckP11, 0x12, E2E_E_PARAM_ID, return E_NOT_OK);
  DET_VALIDATE((NULL != data) && (length > 2u), 0x12, E2E_E_PARAM_POINTER, return E_NOT_OK);
  config = &E2E_CONFIG->CheckP11Configs[profileId];

  if (E2E_P11_DATAID_NIBBLE == config->P11.DataIDMode) { /* @PRS_E2E_00582 */
    ReceivedNibble =
      (data[config->P11.DataIDNibbleOffset >> 3] >> (config->P11.DataIDNibbleOffset & 0x07u)) &
      0x0Fu;
    if (ReceivedNibble != ((config->P11.DataID >> 8) & 0x0F)) {
      ret = E_NOT_OK;
      ASLOG(E2EE, ("[%u] P11 Nibble %x not correct: %x\n", profileId, ReceivedNibble,
                   config->P11.DataID));
    }
  }

  if (E_OK == ret) {
    /* @PRS_E2E_00519 */
    ReceivedCrc = data[config->P11.CRCOffset >> 3];
    crc = ComputeP11Crc(&config->P11, data, length);
    if (crc != ReceivedCrc) {
      ret = E2E_E_WRONG_CRC;
      ASLOG(E2EE, ("[%u] P11 Wrong Crc %x != %x\n", profileId, crc, ReceivedCrc));
    }
  }

  if (E_OK == ret) {
    ReceivedCounter =
      (data[config->P11.CounterOffset >> 3] >> (config->P11.CounterOffset & 0x07u)) & 0x0Fu;
    if (0u == (config->context->Counter & E2E_CHECK_DONE_ONCE)) {
      deltaCounter = 1;
    } else {
      /* @PRS_E2E_00518 */
      deltaCounter = config->context->Counter & 0xFu; /* Get Last Counter */
      if (ReceivedCounter > 0xEu) {
        ret = E_NOT_OK;
        ASLOG(E2EE, ("[%u] P11 Counter %u invalid\n", profileId, ReceivedCounter));
      } else if (ReceivedCounter >= deltaCounter) {
        deltaCounter = ReceivedCounter - deltaCounter;
      } else {
        deltaCounter = 0xF + ReceivedCounter - deltaCounter;
      }
    }
  }

  if (E_OK == ret) {
    if (deltaCounter <= config->MaxDeltaCounter) {
      if (deltaCounter > 0) {
        if (1 == deltaCounter) {
          ret = E_OK;
        } else {
          ret = E2E_E_OK_SOME_LOST;
          ASLOG(E2EE, ("[%u] P11 delta %u but OK\n", profileId, deltaCounter));
        }
      } else {
        ret = E2E_E_REPEATED;
        ASLOG(E2EE, ("[%u] P11 repeat\n", profileId));
      }
    } else {
      ret = E2E_E_WRONG_SEQUENCE;
      ASLOG(E2EE, ("[%u] P11 wrong sequence %u: %x %x\n", profileId, deltaCounter, ReceivedCounter,
                   config->context->Counter));
    }
    config->context->Counter = ReceivedCounter | E2E_CHECK_DONE_ONCE;
  }

  return ret;
}
#endif

#ifdef E2E_USE_PROTECT_P22
Std_ReturnType E2E_P22Protect(E2E_ProfileIdType profileId, uint8_t *data, uint16_t length) {
  Std_ReturnType ret = E_NOT_OK;
  const E2E_ProtectProfile22ConfigType *config;
  DET_VALIDATE(NULL != E2E_CONFIG, 0x22, E2E_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(profileId < E2E_CONFIG->numOfProtectP11, 0x22, E2E_E_PARAM_ID, return E_NOT_OK);
  DET_VALIDATE((NULL != data) && (length > 2u), 0x22, E2E_E_PARAM_POINTER, return E_NOT_OK);
  config = &E2E_CONFIG->ProtectP22Configs[profileId];

  /* @PRS_E2E_00533: intend no % 16 operation */
  config->context->Counter++;

  /* @PRS_E2E_00530 */
  data[(config->P22.Offset >> 3) + 1] &= 0xF0u;
  data[(config->P22.Offset >> 3) + 1] |= config->context->Counter & 0xFu;

  /* @PRS_E2E_00514 */
  data[config->P22.Offset >> 3] = ComputeP22Crc(&config->P22, data, length);

  ASHEXDUMP(E2E,
            ("[%u]: P22 crc=0x%x Counter=%u", profileId, data[config->P22.Offset >> 3],
             config->context->Counter),
            data, length);

  ret = E_OK;
  return ret;
}
#endif

#ifdef E2E_USE_CHECK_P22
Std_ReturnType E2E_P22Check(E2E_ProfileIdType profileId, uint8_t *data, uint16_t length) {
  Std_ReturnType ret = E_OK;
  uint8_t ReceivedCounter = 0;
  uint8_t ReceivedCrc = 0;
  const E2E_CheckProfile22ConfigType *config;
  uint8_t crc;
  uint8_t deltaCounter;
  DET_VALIDATE(NULL != E2E_CONFIG, 0x23, E2E_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(profileId < E2E_CONFIG->numOfCheckP22, 0x23, E2E_E_PARAM_ID, return E_NOT_OK);
  DET_VALIDATE((NULL != data) && (length > 2u), 0x23, E2E_E_PARAM_POINTER, return E_NOT_OK);
  config = &E2E_CONFIG->CheckP22Configs[profileId];

  /* @PRS_E2E_00539 */
  ReceivedCrc = data[config->P22.Offset >> 3];
  crc = ComputeP22Crc(&config->P22, data, length);
  if (crc != ReceivedCrc) {
    ret = E2E_E_WRONG_CRC;
    ASLOG(E2EE, ("[%u] P22 Wrong Crc %x != %x\n", profileId, crc, ReceivedCrc));
  }

  if (E_OK == ret) {
    ReceivedCounter = data[(config->P22.Offset >> 3) + 1] & 0x0Fu;
    if (0u == (config->context->Counter & E2E_CHECK_DONE_ONCE)) {
      deltaCounter = 1;
    } else {
      /* @PRS_E2E_00518 */
      deltaCounter = config->context->Counter & 0xFu; /* Get Last Counter */
      if (ReceivedCounter >= deltaCounter) {
        deltaCounter = ReceivedCounter - deltaCounter;
      } else {
        deltaCounter = 0x10u + ReceivedCounter - deltaCounter;
      }
    }
  }

  if (E_OK == ret) {
    if (deltaCounter <= config->MaxDeltaCounter) {
      if (deltaCounter > 0) {
        if (1 == deltaCounter) {
          ret = E_OK;
        } else {
          ret = E2E_E_OK_SOME_LOST;
          ASLOG(E2EE, ("[%u] P22 delta %u but OK\n", profileId, deltaCounter));
        }
      } else {
        ret = E2E_E_REPEATED;
        ASLOG(E2EE, ("[%u] P22 repeat\n", profileId));
      }
    } else {
      ret = E2E_E_WRONG_SEQUENCE;
      ASLOG(E2EE, ("[%u] P22 wrong sequence %u: %x %x\n", profileId, deltaCounter, ReceivedCounter,
                   config->context->Counter));
    }

    config->context->Counter = ReceivedCounter | E2E_CHECK_DONE_ONCE;
  }

  return ret;
}
#endif

#ifdef E2E_USE_PROTECT_P44
Std_ReturnType E2E_P44Protect(E2E_ProfileIdType profileId, uint8_t *data, uint16_t length) {
  Std_ReturnType ret = E_NOT_OK;
  const E2E_ProtectProfile44ConfigType *config;
  uint16_t offset;
  uint32_t crc = 0u;
  DET_VALIDATE(NULL != E2E_CONFIG, 0x44, E2E_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(profileId < E2E_CONFIG->numOfProtectP44, 0x44, E2E_E_PARAM_ID, return E_NOT_OK);
  DET_VALIDATE((NULL != data) && (length > 12u), 0x44, E2E_E_PARAM_POINTER, return E_NOT_OK);
  config = &E2E_CONFIG->ProtectP44Configs[profileId];

  offset = config->P44.Offset >> 3;

  data[offset + 0u] = (length >> 8) & 0xFFu;
  data[offset + 1u] = length & 0xFFu;

  data[offset + 2u] = (config->context->Counter >> 8) & 0xFFu;
  data[offset + 3u] = config->context->Counter & 0xFFu;

  data[offset + 4u] = (config->P44.DataID >> 24) & 0xFFu;
  data[offset + 5u] = (config->P44.DataID >> 16) & 0xFFu;
  data[offset + 6u] = (config->P44.DataID >> 8) & 0xFFu;
  data[offset + 7u] = config->P44.DataID & 0xFFu;

  crc = Crc_CalculateCRC32P4(data, offset + 8u, crc, FALSE);
  if (length > (offset + 12u)) {
    crc = Crc_CalculateCRC32P4(&data[offset + 12u], length - 12u - offset, crc, FALSE);
  }

  data[offset + 8u] = (crc >> 24) & 0xFFu;
  data[offset + 9u] = (crc >> 16) & 0xFFu;
  data[offset + 10u] = (crc >> 8) & 0xFFu;
  data[offset + 11u] = crc & 0xFFu;
  ASHEXDUMP(E2E, ("[%u]: P44 crc=0x%x Counter=%u", profileId, crc, config->context->Counter), data,
            length);

  config->context->Counter++;
  ret = E_OK;
  return ret;
}
#endif

#ifdef E2E_USE_CHECK_P44
Std_ReturnType E2E_P44Check(E2E_ProfileIdType profileId, uint8_t *data, uint16_t length) {
  Std_ReturnType ret = E_OK;
  uint16_t ReceivedCounter = 0;
  uint32_t ReceivedCrc = 0;
  uint32_t ReceivedDataID;
  uint16_t ReceivedLength;
  uint16_t offset;
  uint32_t crc = 0u;
  const E2E_CheckProfile44ConfigType *config;
  uint16_t deltaCounter;
  DET_VALIDATE(NULL != E2E_CONFIG, 0x45, E2E_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(profileId < E2E_CONFIG->numOfCheckP44, 0x45, E2E_E_PARAM_ID, return E_NOT_OK);
  DET_VALIDATE((NULL != data) && (length > 12u), 0x45, E2E_E_PARAM_POINTER, return E_NOT_OK);
  config = &E2E_CONFIG->CheckP44Configs[profileId];

  offset = config->P44.Offset;

  ReceivedLength = ((uint16_t)data[offset + 0u] << 8) + data[offset + 1u];
  if ((ReceivedLength < length) || ((offset + 12u) >= ReceivedLength)) {
    ASLOG(E2EE, ("[%u] P44 length %u < %u\n", profileId, ReceivedLength, length));
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    ReceivedCounter = ((uint16_t)data[offset + 2u] << 8) + data[offset + 3u];
    ReceivedDataID = ((uint32_t)data[offset + 4u] << 24) + ((uint32_t)data[offset + 5u] << 16) +
                     ((uint32_t)data[offset + 6u] << 8) + data[offset + 7u];
    if (ReceivedDataID == config->P44.DataID) {
      ReceivedCrc = ((uint32_t)data[offset + 8u] << 24) + ((uint32_t)data[offset + 9u] << 16) +
                    ((uint32_t)data[offset + 10u] << 8) + data[offset + 11u];
      crc = Crc_CalculateCRC32P4(data, offset + 8u, crc, FALSE);
      if (ReceivedLength > (offset + 12u)) {
        crc = Crc_CalculateCRC32P4(&data[offset + 12u], ReceivedLength - 12u - offset, crc, FALSE);
      }
      if (crc != ReceivedCrc) {
        ret = E2E_E_WRONG_CRC;
        ASLOG(E2EE, ("[%u] P44 Wrong Crc %x != %x\n", profileId, crc, ReceivedCrc));
      }
    } else {
      ret = E_NOT_OK;
      ASLOG(E2EE,
            ("[%u] P44 Wrong DataID %x != %x\n", profileId, ReceivedDataID, config->P44.DataID));
    }
  }

  if (E_OK == ret) {
    if (FALSE == config->context->bSynced) {
      deltaCounter = 1;
    } else {
      /* @PRS_E2E_00518 */
      deltaCounter = config->context->Counter; /* Get Last Counter */
      if (ReceivedCounter >= deltaCounter) {
        deltaCounter = ReceivedCounter - deltaCounter;
      } else {
        deltaCounter = 65536u - deltaCounter + ReceivedCounter;
      }
    }
  }

  if (E_OK == ret) {
    if (deltaCounter <= config->MaxDeltaCounter) {
      if (deltaCounter > 0) {
        if (1 == deltaCounter) {
          ret = E_OK;
        } else {
          ret = E2E_E_OK_SOME_LOST;
          ASLOG(E2EE, ("[%u] P44 delta %u but OK\n", profileId, deltaCounter));
        }
      } else {
        ret = E2E_E_REPEATED;
        ASLOG(E2EE, ("[%u] P44 repeat\n", profileId));
      }
    } else {
      ret = E2E_E_WRONG_SEQUENCE;
      ASLOG(E2EE, ("[%u] P44 wrong sequence %u: %x %x\n", profileId, deltaCounter, ReceivedCounter,
                   config->context->Counter));
    }
    config->context->Counter = ReceivedCounter;
    config->context->bSynced = TRUE;
  }

  return ret;
}
#endif

#ifdef E2E_USE_PROTECT_P05
Std_ReturnType E2E_P05Protect(E2E_ProfileIdType profileId, uint8_t *data, uint16_t length) {
  Std_ReturnType ret = E_NOT_OK;
  const E2E_ProtectProfile05ConfigType *config;
  uint16_t offset;
  uint16_t crc = 0u;
  DET_VALIDATE(NULL != E2E_CONFIG, 0x05, E2E_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(profileId < E2E_CONFIG->numOfProtectP05, 0x05, E2E_E_PARAM_ID, return E_NOT_OK);
  DET_VALIDATE((NULL != data) && (length > 3u), 0x05, E2E_E_PARAM_POINTER, return E_NOT_OK);
  config = &E2E_CONFIG->ProtectP05Configs[profileId];

  offset = config->P05.Offset >> 3;

  data[offset + 2u] = config->context->Counter;

  crc = ComputeP05Crc(&config->P05, data, length);
  data[offset + 0u] = (crc >> 8) & 0xFFu;
  data[offset + 1u] = crc & 0xFFu;
  ASHEXDUMP(E2E, ("[%u]: P05 crc=0x%x Counter=%u", profileId, crc, config->context->Counter), data,
            length);

  config->context->Counter++;
  ret = E_OK;
  return ret;
}
#endif

#ifdef E2E_USE_CHECK_P05
Std_ReturnType E2E_P05Check(E2E_ProfileIdType profileId, uint8_t *data, uint16_t length) {
  Std_ReturnType ret = E_OK;
  uint8_t ReceivedCounter = 0;
  uint16_t ReceivedCrc = 0;
  const E2E_CheckProfile05ConfigType *config;
  uint16_t crc;
  uint8_t deltaCounter;
  uint16_t offset;
  DET_VALIDATE(NULL != E2E_CONFIG, 0x15, E2E_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(profileId < E2E_CONFIG->numOfCheckP05, 0x15, E2E_E_PARAM_ID, return E_NOT_OK);
  DET_VALIDATE((NULL != data) && (length > 3u), 0x15, E2E_E_PARAM_POINTER, return E_NOT_OK);
  config = &E2E_CONFIG->CheckP05Configs[profileId];

  offset = config->P05.Offset >> 3;

  ReceivedCrc = ((uint16_t)data[offset] << 8) + data[offset + 1u];
  crc = ComputeP05Crc(&config->P05, data, length);
  if (crc != ReceivedCrc) {
    ret = E2E_E_WRONG_CRC;
    ASLOG(E2EE, ("[%u] P05 Wrong Crc %x != %x\n", profileId, crc, ReceivedCrc));
  }

  if (E_OK == ret) {
    ReceivedCounter = data[offset + 2u];
    if (FALSE == config->context->bSynced) {
      deltaCounter = 1u;
    } else {
      deltaCounter = config->context->Counter; /* Get Last Counter */
      if (ReceivedCounter >= deltaCounter) {
        deltaCounter = ReceivedCounter - deltaCounter;
      } else {
        deltaCounter = 0x100u - deltaCounter + ReceivedCounter;
      }
    }
  }

  if (E_OK == ret) {
    if (deltaCounter <= config->MaxDeltaCounter) {
      if (deltaCounter > 0u) {
        if (1u == deltaCounter) {
          ret = E_OK;
        } else {
          ret = E2E_E_OK_SOME_LOST;
          ASLOG(E2EE, ("[%u] P05 delta %u but OK\n", profileId, deltaCounter));
        }
      } else {
        ret = E2E_E_REPEATED;
        ASLOG(E2EE, ("[%u] P05 repeat\n", profileId));
      }
    } else {
      ret = E2E_E_WRONG_SEQUENCE;
      ASLOG(E2EE, ("[%u] P05 wrong sequence %u: %x %x\n", profileId, deltaCounter, ReceivedCounter,
                   config->context->Counter));
    }

    config->context->Counter = ReceivedCounter;
    config->context->bSynced = TRUE;
  }

  return ret;
}
#endif