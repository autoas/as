/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 *
 * ref: https://www.autosar.org/fileadmin/standards/R20-11/CP/AUTOSAR_SWS_TimeSyncOverCAN.pdf
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "CanTSyn_Priv.h"
#include "CanTSyn.h"
#include "CanIf.h"
#include "Crc.h"
#include <string.h>
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_CANTSYN 0
#define AS_LOG_CANTSYNI 0
#define AS_LOG_CANTSYNE 2

#define CANTSYNC_SC_UNKNOWN 0xFF

#define CANTSYN_CONFIG (&CanTSyn_Config)

#define CANTSYN_NANOSECONDS_PER_SECOND 1000000000

/* 0x12a05f200 */
#define CANTSYN_NANOSECONDS_MAX_U32 (0xFFFFFFFF)
#define CANTSYN_NANOSECONDS_MAX_WITH_OVS ((uint64_t)3 * CANTSYN_NANOSECONDS_PER_SECOND + 0xFFFFFFFF)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const CanTSyn_ConfigType CanTSyn_Config;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/*               TM                            TS
 *               |                             |
 *       t0r --> | \   SYNC(s(t0r))            |
 *               |  \---------------------\    |
 *       t1r --> |                        \--> | <-- t2r: this must be in interrupt
 *               |                             |
 *               |                             |
 *       FUP --> | \  FUP(t4r=t1r-s(t0r))      |
 *               |  \---------------------\    |
 *               |                        \--> |
 *               |                             |
 *               |                             | <-- t3r: this can be in interrupt or pooling
 *               |                             |
 * t0r is the time that shall be transmitted. Second portion of t0r is put in CAN message.
 * Capture actual time of transmission time t1r through transmit confirmation in the interrupt.
 * Capture actual time of reception time t2r through receive indication in the interrupt.
 *   computed rel.time = (t3r - t2r) + s(t0r) + t4r
 *                                     `--- t1r --`
 */
static void CanTSyn_HandleMsgSync(const CanTSyn_GlobalTimeDomainType *domain,
                                  const uint8_t *payload, PduLengthType length) {
  CanTSyn_SlaveContextType *context = domain->U.S.context;
  uint8_t D, SC;
  uint8_t crc;
  Std_ReturnType ret = E_OK;

  if (CANTSYN_SYNC_NO_CRC == payload[0]) {
    /* @SWS_CanTSyn_00058, @SWS_CanTSyn_00059, @SWS_CanTSyn_00109 */
    if (CANTSYN_CRC_VALIDATED == domain->U.S.Slave->RxCrcValidated) {
      ASLOG(CANTSYNE, ("Slave get SYNC non CRC but configured wiht crc validated\n"));
      ret = E_NOT_OK;
    }
  } else {
    /* @ECUC_CanTSyn_00021 */
    if ((CANTSYN_CRC_VALIDATED == domain->U.S.Slave->RxCrcValidated) ||
        (CANTSYN_CRC_OPTIONAL == domain->U.S.Slave->RxCrcValidated)) {
      /* @SWS_CanTSyn_00080 */
      crc = Crc_CalculateCRC8H2F(&payload[2], length - 2, 0, TRUE);
      if (crc != payload[1]) {
        ASLOG(CANTSYNE, ("Slave get SYNC with invalid CRC %x != %x\n", crc, payload[1]));
        ret = E_NOT_OK;
      }
    } else if (CANTSYN_CRC_NOT_VALIDATED == domain->U.S.Slave->RxCrcValidated) {
      ASLOG(CANTSYNE, ("Slave get SYNC with CRC but configured wiht crc not validated\n"));
      ret = E_NOT_OK;
    } else {
      /* don't check crc as ignored */
    }
  }

  if (E_OK == ret) {
    if (CANTSYN_SLAVE_IDLE != context->state) {
      ASLOG(CANTSYNE, ("Slave get SYNC when in state %d\n", context->state));
      context->state = CANTSYN_SLAVE_IDLE;
    }

    D = (payload[2] >> 4) & 0xF;
    SC = payload[2] & 0xF;
    if (CANTSYNC_SC_UNKNOWN != context->SC) {
      context->SC = SC; /* @SWS_CanTSyn_00079 */
    } else if ((SC > context->SC) &&
               ((SC - context->SC) <= domain->U.S.Slave->GlobalTimeSequenceCounterJumpWidth)) {
      context->SC = SC; /* @SWS_CanTSyn_00078 */
    } else if ((SC < context->SC) &&
               (16 + SC - context->SC) <= domain->U.S.Slave->GlobalTimeSequenceCounterJumpWidth) {
      context->SC = SC; /* @SWS_CanTSyn_00078 */
    } else {
      ASLOG(CANTSYNE, ("Slave get SYNC with invalid SC %d, current SC %d\n", SC, context->SC));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    /* @SWS_CanTSyn_00086 */
    if (D != domain->GlobalTimeDomainId) {
      ASLOG(CANTSYNE,
            ("Slave get SYNC with invalid D %d, expect %d\n", D, domain->GlobalTimeDomainId));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    /* @SWS_CanTSyn_00073 */
    ret = StbM_GetCurrentVirtualLocalTime(domain->SynchronizedTimeBaseRef, &context->t2r);
    if (E_OK == ret) {
      context->st0r = ((uint32_t)payload[4] << 24) + ((uint32_t)payload[5] << 16) +
                      ((uint32_t)payload[6] << 8) + payload[7];
      /* @SWS_CanTSyn_00063 */
      context->timer = domain->U.S.Slave->GlobalTimeFollowUpTimeout;
      context->state = CANTSYN_SLAVE_SYNC;
      ASLOG(CANTSYNI,
            ("t2r is 0x%08x%08x\n", context->t2r.nanosecondsHi, context->t2r.nanosecondsLo));
    }
  }
}

static Std_ReturnType
CanTSyn_CalcGlobalTimestamp(StbM_TimeStampType *timestamp, uint32_t st0r /* seconds */,
                            uint32_t t4r /*nanoseconds */, uint8_t ovs /* seconds */,
                            StbM_VirtualLocalTimeType *t2r, StbM_VirtualLocalTimeType *t3r) {
  Std_ReturnType ret = E_OK;
  uint64_t diff;
  diff = (((uint64_t)t3r->nanosecondsHi << 32) + t3r->nanosecondsLo) -
         (((uint64_t)t2r->nanosecondsHi << 32) + t2r->nanosecondsLo) + t4r;

  timestamp->seconds = diff / CANTSYN_NANOSECONDS_PER_SECOND; /* get seconds portion */
  timestamp->nanoseconds = diff - timestamp->seconds * CANTSYN_NANOSECONDS_PER_SECOND;
  timestamp->seconds += ovs;

  if (st0r > (UINT32_MAX - timestamp->seconds)) {
    timestamp->secondsHi += 1;
    timestamp->seconds = timestamp->seconds - (UINT32_MAX - st0r + 1);
  } else {
    timestamp->seconds += st0r;
  }

  return ret;
}

static void CanTSyn_HandleMsgFup(const CanTSyn_GlobalTimeDomainType *domain, const uint8_t *payload,
                                 PduLengthType length) {
  CanTSyn_SlaveContextType *context = domain->U.S.context;
  uint8_t D, SC;
  uint8_t crc;
  Std_ReturnType ret = E_OK;
  StbM_VirtualLocalTimeType t3r;
  uint32_t t4r;
  uint8_t sgw;
  uint8_t ovs;
  StbM_TimeStampType timestamp = {0, 0, 0, 0};

  if (CANTSYN_FUP_NO_CRC == payload[0]) {
    /* @SWS_CanTSyn_00058, @SWS_CanTSyn_00059, @SWS_CanTSyn_00109 */
    if (CANTSYN_CRC_VALIDATED == domain->U.S.Slave->RxCrcValidated) {
      ASLOG(CANTSYNE, ("Slave get FUP non CRC but configured wiht crc validated\n"));
      ret = E_NOT_OK;
    }
  } else {
    /* @ECUC_CanTSyn_00021 */
    if ((CANTSYN_CRC_VALIDATED == domain->U.S.Slave->RxCrcValidated) ||
        (CANTSYN_CRC_OPTIONAL == domain->U.S.Slave->RxCrcValidated)) {
      /* @SWS_CanTSyn_00080 */
      crc = Crc_CalculateCRC8H2F(&payload[2], length - 2, 0, TRUE);
      if (crc != payload[1]) {
        ASLOG(CANTSYNE, ("Slave get SYNC with invalid CRC %x != %x\n", crc, payload[1]));
        ret = E_NOT_OK;
      }
    } else if (CANTSYN_CRC_NOT_VALIDATED == domain->U.S.Slave->RxCrcValidated) {
      ASLOG(CANTSYNE, ("Slave get SYNC with CRC but configured wiht crc not validated\n"));
      ret = E_NOT_OK;
    } else {
      /* don't check crc as ignored */
    }
  }

  if (E_OK == ret) {
    if (CANTSYN_SLAVE_SYNC != context->state) {
      ASLOG(CANTSYNE, ("Slave get FUP when in state %d\n", context->state));
      ret = E_NOT_OK;
    } else {
      D = (payload[2] >> 4) & 0xF;
      SC = payload[2] & 0xF;
    }
  }

  if (E_OK == ret) {
    /* @SWS_CanTSyn_00076 */
    if (SC != context->SC) {
      ASLOG(CANTSYNE, ("Slave get FUP with invalid SC %d, current SC %d\n", SC, context->SC));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    /* @SWS_CanTSyn_00086 */
    if (D != domain->GlobalTimeDomainId) {
      ASLOG(CANTSYNE,
            ("Slave get SYNC with invalid D %d, expect %d\n", D, domain->GlobalTimeDomainId));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    /* @SWS_CanTSyn_00073 */
    ret = StbM_GetCurrentVirtualLocalTime(domain->SynchronizedTimeBaseRef, &t3r);
    if (E_OK == ret) {
      sgw = (payload[3] >> 2) & 0x01; /* SyncToGTM = 0, SyncToSubDomain = 1 */
      ovs = payload[3] & 0x03;
      t4r = ((uint32_t)payload[4] << 24) + ((uint32_t)payload[5] << 16) +
            ((uint32_t)payload[6] << 8) + payload[7];

      if (sgw) {
        timestamp.timeBaseStatus |= STBM_STATUS_SYNC_TO_GATEWAY;
      }

      ret = CanTSyn_CalcGlobalTimestamp(&timestamp, context->st0r, t4r, ovs, &context->t2r, &t3r);
      if (E_OK == ret) {
        /* @CAN Time Synchronization (Time Slave)
         * *timeStampPtr = T3diff + (T0+T4);
         * *localTimePtr = T3VLT
         */
        (void)StbM_BusSetGlobalTime(domain->SynchronizedTimeBaseRef, &timestamp, NULL, NULL, &t3r);
      }
      context->timer = 0;
      context->state = CANTSYN_SLAVE_IDLE;
    }
  }
}

static void CanTSyn_TransmitSYNC(const CanTSyn_GlobalTimeDomainType *domain) {
  Std_ReturnType ret = E_OK;
  uint8_t data[16];
  PduInfoType PduInfo;
  uint64_t t0rNs;
  uint32_t st0r;
  CanTSyn_MasterContextType *context = domain->U.M.context;

  if (domain->U.M.Master->GlobalTimeTxCrcSecured) {
    data[0] = CANTSYN_SYNC_WITH_CRC;
  } else {
    data[0] = CANTSYN_SYNC_NO_CRC;
    data[1] = 0x00;
  }

  data[2] = (domain->GlobalTimeDomainId << 4) + context->SC;
  data[3] = 0x00;

  ret = StbM_GetCurrentVirtualLocalTime(domain->SynchronizedTimeBaseRef, &context->t0r);
  if (E_OK == ret) {
    t0rNs = ((uint64_t)context->t0r.nanosecondsHi << 32) + context->t0r.nanosecondsLo;
    st0r = t0rNs / CANTSYN_NANOSECONDS_PER_SECOND;
    data[4] = (st0r >> 24) & 0xFF;
    data[5] = (st0r >> 16) & 0xFF;
    data[6] = (st0r >> 8) & 0xFF;
    data[7] = st0r & 0xFF;
    if (domain->UseExtendedMsgFormat) {
      memset(&data[8], 0, 8);
      PduInfo.SduLength = 16;
    } else {
      PduInfo.SduLength = 8;
    }
    if (domain->U.M.Master->GlobalTimeTxCrcSecured) {
      data[1] = Crc_CalculateCRC8H2F(&data[2], 6, 0xFF, TRUE);
    }
  }

  if (E_OK == ret) {
    PduInfo.MetaDataPtr = NULL;
    PduInfo.SduDataPtr = data;
    ret = CanIf_Transmit(domain->U.M.Master->TxPduId, &PduInfo);
  }

  if (E_OK == ret) {
    context->state = CANTSYN_MASTER_SYNCED;
    context->SC++;
    if (context->SC > 15) {
      context->SC = 0;
    }

    /* @SWS_CanTSyn_00124 */
    context->debounceCounter = domain->U.M.Master->GlobalTimeDebounceTime;
  }
}

static void CanTSyn_TransmitFUP(const CanTSyn_GlobalTimeDomainType *domain) {
  Std_ReturnType ret = E_OK;
  uint8_t data[16];
  PduInfoType PduInfo;
  uint32_t st0r;
  uint64_t t4r;
  uint64_t t0rNs;
  uint64_t t1rNs;
  uint8_t ovs = 0;
  CanTSyn_MasterContextType *context = domain->U.M.context;

  if (domain->U.M.Master->GlobalTimeTxCrcSecured) {
    data[0] = CANTSYN_FUP_WITH_CRC;
  } else {
    data[0] = CANTSYN_FUP_NO_CRC;
    data[1] = 0x00;
  }

  /* @SWS_CanTSyn_00076 */
  if (context->SC > 0) {
    data[2] = (domain->GlobalTimeDomainId << 4) + (context->SC - 1);
  } else {
    data[2] = (domain->GlobalTimeDomainId << 4) + 15;
  }

  t0rNs = ((uint64_t)context->t0r.nanosecondsHi << 32) + context->t0r.nanosecondsLo;
  t1rNs = ((uint64_t)context->t1r.nanosecondsHi << 32) + context->t1r.nanosecondsLo;
  st0r = t0rNs / CANTSYN_NANOSECONDS_PER_SECOND;
  t4r = t1rNs - (uint64_t)st0r * CANTSYN_NANOSECONDS_PER_SECOND;

  if (t4r >= CANTSYN_NANOSECONDS_MAX_U32) {
    if (t4r <= CANTSYN_NANOSECONDS_MAX_WITH_OVS) {
      ovs = t4r / CANTSYN_NANOSECONDS_PER_SECOND;
      if (ovs > 3) {
        ovs = 3;
      }
      t4r = t4r - (uint64_t)ovs * CANTSYN_NANOSECONDS_PER_SECOND;
    } else {
      ASLOG(CANTSYNE, ("master FUP overflow\n"));
      ret = E_NOT_OK;
      context->state = CANTSYN_MASTER_IDLE;
    }
  }

  if (E_OK == ret) {
    data[3] = ovs;
    data[4] = (t4r >> 24) & 0xFF;
    data[5] = (t4r >> 16) & 0xFF;
    data[6] = (t4r >> 8) & 0xFF;
    data[7] = t4r & 0xFF;
    if (domain->UseExtendedMsgFormat) {
      memset(&data[8], 0, 8);
      PduInfo.SduLength = 16;
    } else {
      PduInfo.SduLength = 8;
    }
    if (domain->U.M.Master->GlobalTimeTxCrcSecured) {
      data[1] = Crc_CalculateCRC8H2F(&data[2], 6, 0xFF, TRUE);
    }

    PduInfo.MetaDataPtr = NULL;
    PduInfo.SduDataPtr = data;
    ret = CanIf_Transmit(domain->U.M.Master->TxPduId, &PduInfo);
  }

  if (E_OK == ret) {
    context->state = CANTSYN_MASTER_FUPED;
    /* @SWS_CanTSyn_00124 */
    context->debounceCounter = domain->U.M.Master->GlobalTimeDebounceTime;
  }
}

static void CanTSyn_MainMaster(const CanTSyn_GlobalTimeDomainType *domain) {
  uint8_t cnt;
  CanTSyn_MasterContextType *context = domain->U.M.context;

  if (context->debounceCounter > 0) {
    context->debounceCounter--;
  }

  if (context->timer > 0) {
    context->timer--;
    if (0 == context->timer) {
      if (0 == context->debounceCounter) {
        context->timer = domain->U.M.Master->GlobalTimeTxPeriod;
        if (CANTSYN_MASTER_IDLE != context->state) {
          ASLOG(CANTSYNE, ("Master period enter SYNC from state %d\n", context->state));
        }
        context->state = CANTSYN_MASTER_SYNC;
      } else {
        context->timer = 1; /* not yet ready now*/
      }
    }
  }

  /* @SWS_CanTSyn_00117 */
  if (domain->U.M.Master->ImmediateTimeSync) {
    cnt = StbM_GetTimeBaseUpdateCounter(domain->SynchronizedTimeBaseRef);
    /* @SWS_CanTSyn_00118 */
    if (cnt != context->timeBaseUpdateCounter) {
      if (0 == context->debounceCounter) {
        if (CANTSYN_MASTER_IDLE != context->state) {
          ASLOG(CANTSYNE, ("Master immediate enter SYNC from state %d\n", context->state));
        }
        context->state = CANTSYN_MASTER_SYNC;
        context->timeBaseUpdateCounter = cnt;
        /* @SWS_CanTSyn_00120 */
        context->timer = domain->U.M.Master->CyclicMsgResumeTime;
      }
    }
  }

  /* @SWS_CanTSyn_00125 */
  if (0 == context->debounceCounter) {
    if (CANTSYN_MASTER_SYNC == context->state) {
      CanTSyn_TransmitSYNC(domain);
    } else if (CANTSYN_MASTER_FUP == context->state) {
      CanTSyn_TransmitFUP(domain);
    }
  }
}

static void CanTSyn_MainSlave(const CanTSyn_GlobalTimeDomainType *domain) {
  CanTSyn_SlaveContextType *context = domain->U.S.context;

  if (CANTSYN_SLAVE_IDLE != context->state) {
    if (context->timer > 0) {
      context->timer--;
      if (0 == context->timer) {
        ASLOG(CANTSYNE, ("Slave timeout in state %d\n", context->state));
        context->state = CANTSYN_SLAVE_IDLE;
      }
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void CanTSyn_Init(const CanTSyn_ConfigType *ConfigPtr) {
  int i;
  const CanTSyn_GlobalTimeDomainType *domain;
  for (i = 0; i < CANTSYN_CONFIG->numOfGlobalTimeDomains; i++) {
    domain = &CANTSYN_CONFIG->GlobalTimeDomains[i];
    if (CANTSYN_MASTER == domain->DomainType) {
      memset(domain->U.M.context, 0, sizeof(CanTSyn_MasterContextType));
      domain->U.M.context->timer = domain->U.M.Master->GlobalTimeTxPeriod;
    } else {
      memset(domain->U.S.context, 0, sizeof(CanTSyn_SlaveContextType));
      /* @SWS_CanTSyn_00079 */
      domain->U.S.context->SC = CANTSYNC_SC_UNKNOWN;
    }
  }
}

void CanTSyn_SetTransmissionMode(uint8_t CtrlIdx, CanTSyn_TransmissionModeType Mode) {
}

void CanTSyn_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_OK;
  const CanTSyn_GlobalTimeDomainType *domain;
  ASLOG(CANTSYN, ("[%d]RX len=%d data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", RxPduId,
                  PduInfoPtr->SduLength, PduInfoPtr->SduDataPtr[0], PduInfoPtr->SduDataPtr[1],
                  PduInfoPtr->SduDataPtr[2], PduInfoPtr->SduDataPtr[3], PduInfoPtr->SduDataPtr[4],
                  PduInfoPtr->SduDataPtr[5], PduInfoPtr->SduDataPtr[6], PduInfoPtr->SduDataPtr[7]));
  if (RxPduId < CANTSYN_CONFIG->numOfGlobalTimeDomains) {
    domain = &CANTSYN_CONFIG->GlobalTimeDomains[RxPduId];
    if (CANTSYN_SLAVE == domain->DomainType) {
      if (domain->UseExtendedMsgFormat) {
        if (16 != PduInfoPtr->SduLength) {
          ret = E_NOT_OK;
          ASLOG(CANTSYNE, ("[%d] message length is not 16\n", (int)RxPduId));
        }
      } else {
        if (8 != PduInfoPtr->SduLength) {
          ret = E_NOT_OK;
          ASLOG(CANTSYNE, ("[%d] message length is not 8\n", (int)RxPduId));
        }
      }
    } else {
      ret = E_NOT_OK;
      ASLOG(CANTSYNE, ("[%d] domain type not slave\n", (int)RxPduId));
    }
  } else {
    ASLOG(CANTSYNE, ("Invalid RxPduId=%d\n", (int)RxPduId));
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    switch (PduInfoPtr->SduDataPtr[0]) {
    case CANTSYN_SYNC_NO_CRC:
    case CANTSYN_SYNC_WITH_CRC:
      CanTSyn_HandleMsgSync(domain, &PduInfoPtr->SduDataPtr[0], PduInfoPtr->SduLength);
      break;
    case CANTSYN_FUP_NO_CRC:
    case CANTSYN_FUP_WITH_CRC:
      CanTSyn_HandleMsgFup(domain, &PduInfoPtr->SduDataPtr[0], PduInfoPtr->SduLength);
      break;
    default:
      ASLOG(CANTSYNE,
            ("[%d] invalid message type 0x%02x\n", (int)RxPduId, PduInfoPtr->SduDataPtr[0]));
      break;
    }
  }
}

void CanTSyn_TxConfirmation(PduIdType TxPduId, Std_ReturnType result) {
  Std_ReturnType ret;
  const CanTSyn_GlobalTimeDomainType *domain;
  CanTSyn_MasterContextType *context;
  if (TxPduId < CANTSYN_CONFIG->numOfGlobalTimeDomains) {
    domain = &CANTSYN_CONFIG->GlobalTimeDomains[TxPduId];
    if (CANTSYN_MASTER == domain->DomainType) {
      context = domain->U.M.context;
      if (CANTSYN_MASTER_SYNCED == context->state) {
        ret = StbM_GetCurrentVirtualLocalTime(domain->SynchronizedTimeBaseRef, &context->t1r);
        if (E_OK == ret) {
          context->state = CANTSYN_MASTER_FUP;
        } else {
          context->state = CANTSYN_MASTER_IDLE;
        }
      } else if (CANTSYN_MASTER_FUPED == context->state) {
        context->state = CANTSYN_MASTER_IDLE;
      }
    }
  }
}

void CanTSyn_MainFunction(void) {
  int i;
  const CanTSyn_GlobalTimeDomainType *domain;
  for (i = 0; i < CANTSYN_CONFIG->numOfGlobalTimeDomains; i++) {
    domain = &CANTSYN_CONFIG->GlobalTimeDomains[i];
    if (CANTSYN_MASTER == domain->DomainType) {
      CanTSyn_MainMaster(domain);
    } else {
      CanTSyn_MainSlave(domain);
    }
  }
}
