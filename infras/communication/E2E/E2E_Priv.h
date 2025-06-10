/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * ref: https://www.autosar.org/fileadmin/standards/R23-11/FO/AUTOSAR_FO_PRS_E2EProtocol.pdf
 */
#ifndef E2E_PRIV_H
#define E2E_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifndef DET_THIS_MODULE_ID
#define DET_THIS_MODULE_ID MODULE_ID_E2E
#endif

#define E2E_E_PARAM_CONFIG 0x1
#define E2E_E_PARAM_ID 0x2
#define E2E_E_PARAM_POINTER 0x3
#define E2E_E_UNINIT 0x4

/* @PRS_E2E_00163 */
#define E2E_P11_DATAID_BOTH ((E2E_P11DataIDModeType)0)
#define E2E_P11_DATAID_ALT ((E2E_P11DataIDModeType)1)
#define E2E_P11_DATAID_LOW ((E2E_P11DataIDModeType)2)
#define E2E_P11_DATAID_NIBBLE ((E2E_P11DataIDModeType)3)
/* ================================ [ TYPES     ] ============================================== */
/* @PRS_E2E_00583  */
typedef uint8_t E2E_P11DataIDModeType;

typedef struct {
  uint8_t Counter;
} E2E_ProtectProfile11ContextType;

/* @PRS_E2E_00503 */
typedef struct { /* see 6.12.5 E2E Profile 11 Protocol Examples */
  /** @param DataID 16 bits or 12 bit, unique system-wide. (either implicitly sent (16 bits) or
     partly explicitly sent (12 bits; 4 bits explicitly and 8 bits implicitly sent))*/
  uint16_t DataID;
  uint16_t CRCOffset;          /* bit position for the 8 bits CRC-8-SAE J1850 */
  uint16_t CounterOffset;      /* bit position, default 8 */
  uint16_t DataIDNibbleOffset; /* bit position, default 12 */
  E2E_P11DataIDModeType DataIDMode;
} E2E_Profile11ConfigType;

/* @PRS_E2E_00503 */
typedef struct {
  E2E_ProtectProfile11ContextType *context;
  E2E_Profile11ConfigType P11;
} E2E_ProtectProfile11ConfigType;

typedef struct {
  uint8_t Counter;
} E2E_CheckProfile11ContextType;

typedef struct {
  E2E_CheckProfile11ContextType *context;
  E2E_Profile11ConfigType P11;
  uint8_t MaxDeltaCounter;
} E2E_CheckProfile11ConfigType;

/* @PRS_E2E_00666 */
typedef struct {
  uint16_t Offset;
  uint8_t DataIDList[16];
} E2E_Profile22ConfigType;

typedef struct {
  uint8_t Counter;
} E2E_ProtectProfile22ContextType;

typedef struct {
  E2E_ProtectProfile22ContextType *context;
  E2E_Profile22ConfigType P22;
} E2E_ProtectProfile22ConfigType;

typedef struct {
  uint8_t Counter;
} E2E_CheckProfile22ContextType;

typedef struct {
  E2E_CheckProfile22ContextType *context;
  E2E_Profile22ConfigType P22;
  uint8_t MaxDeltaCounter;
} E2E_CheckProfile22ConfigType;

/* @PRS_E2E_00735 */
typedef struct {
  uint32_t DataID;
  uint16_t Offset;
} E2E_Profile44ConfigType;

typedef struct {
  uint16_t Counter;
} E2E_ProtectProfile44ContextType;

typedef struct {
  E2E_ProtectProfile44ContextType *context;
  E2E_Profile44ConfigType P44;
} E2E_ProtectProfile44ConfigType;

typedef struct {
  uint16_t Counter;
  boolean bSynced;
} E2E_CheckProfile44ContextType;

typedef struct {
  E2E_CheckProfile44ContextType *context;
  E2E_Profile44ConfigType P44;
  uint8_t MaxDeltaCounter;
} E2E_CheckProfile44ConfigType;

/* @PRS_E2E_00394 */
typedef struct {
  uint16_t DataID;
  uint16_t Offset;
} E2E_Profile05ConfigType;

typedef struct {
  uint8_t Counter;
} E2E_ProtectProfile05ContextType;

typedef struct {
  E2E_ProtectProfile05ContextType *context;
  E2E_Profile05ConfigType P05;
} E2E_ProtectProfile05ConfigType;

typedef struct {
  uint8_t Counter;
  boolean bSynced;
} E2E_CheckProfile05ContextType;

typedef struct {
  E2E_CheckProfile05ContextType *context;
  E2E_Profile05ConfigType P05;
  uint8_t MaxDeltaCounter;
} E2E_CheckProfile05ConfigType;

struct E2E_Config_s {
#ifdef E2E_USE_PROTECT_P11
  const E2E_ProtectProfile11ConfigType *ProtectP11Configs;
#endif
#ifdef E2E_USE_CHECK_P11
  const E2E_CheckProfile11ConfigType *CheckP11Configs;
#endif
#ifdef E2E_USE_PROTECT_P22
  const E2E_ProtectProfile22ConfigType *ProtectP22Configs;
#endif
#ifdef E2E_USE_CHECK_P22
  const E2E_CheckProfile22ConfigType *CheckP22Configs;
#endif
#ifdef E2E_USE_PROTECT_P44
  const E2E_ProtectProfile44ConfigType *ProtectP44Configs;
#endif
#ifdef E2E_USE_CHECK_P44
  const E2E_CheckProfile44ConfigType *CheckP44Configs;
#endif
#ifdef E2E_USE_PROTECT_P05
  const E2E_ProtectProfile05ConfigType *ProtectP05Configs;
#endif
#ifdef E2E_USE_CHECK_P05
  const E2E_CheckProfile05ConfigType *CheckP05Configs;
#endif
#ifdef E2E_USE_PROTECT_P11
  uint16_t numOfProtectP11;
#endif
#ifdef E2E_USE_CHECK_P11
  uint16_t numOfCheckP11;
#endif
#ifdef E2E_USE_PROTECT_P22
  uint16_t numOfProtectP22;
#endif
#ifdef E2E_USE_CHECK_P22
  uint16_t numOfCheckP22;
#endif
#ifdef E2E_USE_PROTECT_P44
  uint16_t numOfProtectP44;
#endif
#ifdef E2E_USE_CHECK_P44
  uint16_t numOfCheckP44;
#endif
#ifdef E2E_USE_PROTECT_P05
  uint16_t numOfProtectP05;
#endif
#ifdef E2E_USE_CHECK_P05
  uint16_t numOfCheckP05;
#endif
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#ifdef __cplusplus
}
#endif
#endif /* E2E_PRIV_H */
