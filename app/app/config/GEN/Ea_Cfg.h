/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Generated at Sat Nov 27 10:33:30 2021
 */
#ifndef _EA_CFG_H
#define _EA_CFG_H
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#ifndef EA_PAGE_SIZE
#define EA_PAGE_SIZE 1
#endif

#ifndef EA_SECTOR_SIZE
#define EA_SECTOR_SIZE 4
#endif

#define EA_MAX_DATA_SIZE 28

#define EA_NUMBER_Dem_NvmEventStatusRecord0 1
#define EA_NUMBER_Dem_NvmEventStatusRecord1 2
#define EA_NUMBER_Dem_NvmEventStatusRecord2 3
#define EA_NUMBER_Dem_NvmEventStatusRecord3 4
#define EA_NUMBER_Dem_NvmEventStatusRecord4 5
#define EA_NUMBER_Dem_NvmFreezeFrameRecord0 6
#define EA_NUMBER_Dem_NvmFreezeFrameRecord1 7
#define EA_NUMBER_Dem_NvmFreezeFrameRecord2 8
#define EA_NUMBER_Dem_NvmFreezeFrameRecord3 9
#define EA_NUMBER_Dem_NvmFreezeFrameRecord4 10
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
#endif /* _EA_CFG_H */
