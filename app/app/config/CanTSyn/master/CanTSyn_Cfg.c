/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "CanTSyn_Cfg.h"
#include "CanTSyn_Priv.h"
#include "CanTSyn.h"
#include "../../Com/GEN/CanIf_Cfg.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static CanTSyn_MasterContextType CanTSyn_MasterContext0;
static const CanTSyn_GlobalTimeMasterType CanTSyn_GlobalTimeMaster0 = {
  CANTSYN_CONVERT_MS_TO_MAIN_CYCLES(2000), /* CyclicMsgResumeTime */
  CANTSYN_CONVERT_MS_TO_MAIN_CYCLES(100),  /* GlobalTimeDebounceTime */
  CANTSYN_CONVERT_MS_TO_MAIN_CYCLES(5000), /* GlobalTimeTxPeriod */
  CANIF_CANTSYN_TX,                        /* TxPduId */
  TRUE,                                    /* GlobalTimeTxCrcSecured */
  TRUE,                                    /* ImmediateTimeSync */
  TRUE,                                    /* TxTmacCalculated */
};

static const CanTSyn_GlobalTimeDomainType CanTSyn_GlobalTimeDomains[1] = {
  {
    {{&CanTSyn_MasterContext0, &CanTSyn_GlobalTimeMaster0}},
    FALSE, /* EnableTimeValidation */
    0,     /* GlobalTimeDomainId */
    0,     /* GlobalTimeNetworkSegmentId */
    0,     /* GlobalTimeSecureTmacLength */
    FALSE, /* UseExtendedMsgFormat */
    0,     /* SynchronizedTimeBaseRef */
    CANTSYN_MASTER,
  },
};

const CanTSyn_ConfigType CanTSyn_Config = {
  CanTSyn_GlobalTimeDomains,
  ARRAY_SIZE(CanTSyn_GlobalTimeDomains),
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
