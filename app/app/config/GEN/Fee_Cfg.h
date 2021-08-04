/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Generated at Fri Jul 30 09:13:24 2021
 */
#ifndef FEE_CFG_H
#define FEE_CFG_H
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#ifndef FEE_PAGE_SIZE
#define FEE_PAGE_SIZE 8
#endif

#define FEE_MAX_DATA_SIZE 28

#define FEE_NUMBER_Dem_NvmEventStatusRecord0 1
#define FEE_NUMBER_Dem_NvmEventStatusRecord1 2
#define FEE_NUMBER_Dem_NvmEventStatusRecord2 3
#define FEE_NUMBER_Dem_NvmEventStatusRecord3 4
#define FEE_NUMBER_Dem_NvmEventStatusRecord4 5
#define FEE_NUMBER_Dem_NvmFreezeFrameRecord0 6
#define FEE_NUMBER_Dem_NvmFreezeFrameRecord1 7
#define FEE_NUMBER_Dem_NvmFreezeFrameRecord2 8
#define FEE_NUMBER_Dem_NvmFreezeFrameRecord3 9
#define FEE_NUMBER_Dem_NvmFreezeFrameRecord4 10
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint8_t status;
  uint8_t testFailedCounter;
  uint8_t faultOccuranceCounter;
  uint8_t agingCounter;
  uint8_t agedCounter;
} Dem_NvmEventStatusRecordType;

typedef struct {
  uint16_t EventId;
  uint8_t record[2][13];
} Dem_NvmFreezeFrameRecordType;

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* FEE_CFG_H */
