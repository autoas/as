/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "CanTSyn_Cfg.h"
#include "CanTSyn_Priv.h"
#include "CanTSyn.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static CanTSyn_SlaveContextType CanTSyn_SlaveContext0;
static const CanTSyn_GlobalTimeSlaveType CanTSyn_GlobalTimeSlave0 = {
  CANTSYN_CONVERT_MS_TO_MAIN_CYCLES(100), /* GlobalTimeFollowUpTimeout */
  CANTSYN_CONVERT_MS_TO_MAIN_CYCLES(10),  /* GlobalTimeMinMsgGap */
  3,                                      /* GlobalTimeSequenceCounterJumpWidth */
  CANTSYN_CRC_IGNORED,                    /* RxCrcValidated */
  FALSE,                                  /* RxTmacValidated */
};

static const CanTSyn_GlobalTimeDomainType CanTSyn_GlobalTimeDomains[1] = {
  {
    {{&CanTSyn_SlaveContext0, &CanTSyn_GlobalTimeSlave0}},
    FALSE, /* EnableTimeValidation */
    0,     /* GlobalTimeDomainId */
    0,     /* GlobalTimeNetworkSegmentId */
    0,     /* GlobalTimeSecureTmacLength */
    FALSE, /* UseExtendedMsgFormat */
    0,     /* SynchronizedTimeBaseRef */
    CANTSYN_SLAVE,
  },
};

const CanTSyn_ConfigType CanTSyn_Config = {
  CanTSyn_GlobalTimeDomains,
  ARRAY_SIZE(CanTSyn_GlobalTimeDomains),
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
