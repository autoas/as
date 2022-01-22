/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Flash Driver AUTOSAR CP Release 4.4.0
 */
#ifndef FLS_PRIV_H
#define FLS_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* @ECUC_Fls_00202 */
typedef struct {
  Fls_AddressType SectorStartAddress;
  Fls_AddressType SectorEndAddress;
  /* the minimum erase size */
  Fls_LengthType SectorSize;
  /* the minimum write size */
  Fls_LengthType PageSize;
  uint16_t NumberOfSectors;
} Fls_SectorType;

struct Fls_Config_s {
  void (*JobEndNotification)(void);
  void (*JobErrorNotification)(void);
  MemIf_ModeType defaultMode;
  Fls_LengthType MaxReadFastMode;
  Fls_LengthType MaxReadNormalMode;
  Fls_LengthType MaxWriteFastMode;
  Fls_LengthType MaxWriteNormalMode;
  Fls_LengthType MaxEraseFastMode;
  Fls_LengthType MaxEraseNormalMode;
  /* Sectors in List must be sorted by address from low to high */
  const Fls_SectorType *SectorList;
  uint8_t numOfSectors;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#ifdef __cplusplus
}
#endif
#endif /* FLS_PRIV_H */
