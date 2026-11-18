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

#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_CANTSYN 0
#define AS_LOG_CANTSYNI 0
#define AS_LOG_CANTSYNE 0

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
  CanTSyn_SlaveContextType *context = domain->U.S.Context;
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
    if (CANTSYN_SLAVE_IDLE != context->State) {
      ASLOG(CANTSYNE, ("Slave get SYNC when in state %d\n", context->State));
      context->State = CANTSYN_SLAVE_IDLE;
    }

    D = (payload[2] >> 4) & 0xF;
    SC = payload[2] & 0xF;
    if (CANTSYNC_SC_UNKNOWN == context->SC) {
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
    ret = StbM_GetCurrentVirtualLocalTime(domain->SynchronizedTimeBaseRef, &context->T2r);
    if (E_OK == ret) {
      context->St0r = ((uint32_t)payload[4] << 24) + ((uint32_t)payload[5] << 16) +
                      ((uint32_t)payload[6] << 8) + payload[7];
      /* @SWS_CanTSyn_00063 */
      context->Timer = domain->U.S.Slave->GlobalTimeFollowUpTimeout;
      context->State = CANTSYN_SLAVE_SYNC;
      ASLOG(CANTSYNI,
            ("t2r is 0x%08x%08x\n", context->T2r.nanosecondsHi, context->T2r.nanosecondsLo));
    }
  }
}

#if 0
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
#else
static Std_ReturnType
CanTSyn_CalcGlobalTimestamp(StbM_TimeStampType *timestamp, uint32_t st0r /* seconds */,
                            uint32_t t4r /*nanoseconds */, uint8_t ovs /* seconds */,
                            StbM_VirtualLocalTimeType *t2r, StbM_VirtualLocalTimeType *t3r) {
  Std_ReturnType ret = E_OK;
  uint32_t diff;
  uint32_t seconds = 0;

  /* Step 1: Compute (t3r - t2r) in nanoseconds.
   * Since t3r >= t2r (caller should ensure), and difference is small,
   * we can safely compute it in 32-bit if we handle high-word borrow.
   */
  if (t3r->nanosecondsLo >= t2r->nanosecondsLo) {
    /* No borrow from high word */
    diff = t3r->nanosecondsLo - t2r->nanosecondsLo;
    /* High part difference must be 0 */
    if (t3r->nanosecondsHi != t2r->nanosecondsHi) {
      /* If high part differs, ¦¤t >= 2^32 ns ¡Ö 4.29s ¡ú invalid for CanTSyn */
      ASLOG(CANTSYNE, ("t3r - t2r too large (>4.29s)\n"));
      ret = E_NOT_OK;
    }
  } else {
    /* Borrow needed: only valid if high parts differ by exactly 1 */
    if ((t3r->nanosecondsHi - t2r->nanosecondsHi) != 1u) {
      ret = E_NOT_OK; /* t3r < t2r or gap too big */
    } else {
      diff = (UINT32_MAX - t2r->nanosecondsLo) + 1u + t3r->nanosecondsLo;
    }
  }

  if (E_OK == ret) {
    timestamp->seconds = diff / CANTSYN_NANOSECONDS_PER_SECOND;
    diff = diff - timestamp->seconds * CANTSYN_NANOSECONDS_PER_SECOND;
    timestamp->seconds += ovs;

    seconds = t4r / CANTSYN_NANOSECONDS_PER_SECOND;
    timestamp->nanoseconds = t4r - seconds * CANTSYN_NANOSECONDS_PER_SECOND + diff;

    timestamp->seconds += ovs + seconds;
    timestamp->secondsHi = 0;

    if (st0r > (UINT32_MAX - timestamp->seconds)) {
      timestamp->secondsHi = 1u;
      timestamp->seconds = timestamp->seconds - (UINT32_MAX - st0r + 1);
    } else {
      timestamp->seconds += st0r;
    }
  }

  return ret;
}
#endif

static void CanTSyn_HandleMsgFup(const CanTSyn_GlobalTimeDomainType *domain, const uint8_t *payload,
                                 PduLengthType length) {
  CanTSyn_SlaveContextType *context = domain->U.S.Context;
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
    if (CANTSYN_SLAVE_SYNC != context->State) {
      ASLOG(CANTSYNE, ("Slave get FUP when in state %d\n", context->State));
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

      ret = CanTSyn_CalcGlobalTimestamp(&timestamp, context->St0r, t4r, ovs, &context->T2r, &t3r);
      if (E_OK == ret) {
        /* @CAN Time Synchronization (Time Slave)
         * *timeStampPtr = T3diff + (T0+T4);
         * *localTimePtr = T3VLT
         */
        (void)StbM_BusSetGlobalTime(domain->SynchronizedTimeBaseRef, &timestamp, NULL, NULL, &t3r);
      }
      context->Timer = 0;
      context->State = CANTSYN_SLAVE_IDLE;
    }
  }
}

static void CanTSyn_TransmitSYNC(const CanTSyn_GlobalTimeDomainType *domain) {
  Std_ReturnType ret = E_OK;
  uint8_t data[16];
  PduInfoType PduInfo;
  uint64_t t0rNs;
  uint32_t st0r;
  CanTSyn_MasterContextType *context = domain->U.M.Context;

  if (domain->U.M.Master->GlobalTimeTxCrcSecured) {
    data[0] = CANTSYN_SYNC_WITH_CRC;
  } else {
    data[0] = CANTSYN_SYNC_NO_CRC;
    data[1] = 0x00;
  }

  data[2] = (domain->GlobalTimeDomainId << 4) + context->SC;
  data[3] = 0x00;

  ret = StbM_GetCurrentVirtualLocalTime(domain->SynchronizedTimeBaseRef, &context->T0r);
  if (E_OK == ret) {
    t0rNs = ((uint64_t)context->T0r.nanosecondsHi << 32) + context->T0r.nanosecondsLo;
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
    context->State = CANTSYN_MASTER_SYNCED;
    context->SC++;
    if (context->SC > 15) {
      context->SC = 0;
    }

    /* @SWS_CanTSyn_00124 */
    context->DebounceCounter = domain->U.M.Master->GlobalTimeDebounceTime;
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
  CanTSyn_MasterContextType *context = domain->U.M.Context;

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

  t0rNs = ((uint64_t)context->T0r.nanosecondsHi << 32) + context->T0r.nanosecondsLo;
  t1rNs = ((uint64_t)context->T1r.nanosecondsHi << 32) + context->T1r.nanosecondsLo;
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
      context->State = CANTSYN_MASTER_IDLE;
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
    context->State = CANTSYN_MASTER_FUPED;
    /* @SWS_CanTSyn_00124 */
    context->DebounceCounter = domain->U.M.Master->GlobalTimeDebounceTime;
  }
}

static void CanTSyn_MainMaster(const CanTSyn_GlobalTimeDomainType *domain) {
  uint8_t cnt;
  CanTSyn_MasterContextType *context = domain->U.M.Context;

  if (context->DebounceCounter > 0) {
    context->DebounceCounter--;
  }

  if (context->Timer > 0) {
    context->Timer--;
    if (0 == context->Timer) {
      if (0 == context->DebounceCounter) {
        context->Timer = domain->U.M.Master->GlobalTimeTxPeriod;
        if (CANTSYN_MASTER_IDLE != context->State) {
          ASLOG(CANTSYNE, ("Master period enter SYNC from state %d\n", context->State));
        }
        context->State = CANTSYN_MASTER_SYNC;
      } else {
        context->Timer = 1; /* not yet ready now*/
      }
    }
  }

  /* @SWS_CanTSyn_00117 */
  if (domain->U.M.Master->ImmediateTimeSync) {
    cnt = StbM_GetTimeBaseUpdateCounter(domain->SynchronizedTimeBaseRef);
    /* @SWS_CanTSyn_00118 */
    if (cnt != context->TimeBaseUpdateCounter) {
      if (0 == context->DebounceCounter) {
        if (CANTSYN_MASTER_IDLE != context->State) {
          ASLOG(CANTSYNE, ("Master immediate enter SYNC from state %d\n", context->State));
        }
        context->State = CANTSYN_MASTER_SYNC;
        context->TimeBaseUpdateCounter = cnt;
        /* @SWS_CanTSyn_00120 */
        context->Timer = domain->U.M.Master->CyclicMsgResumeTime;
      }
    }
  }

  /* @SWS_CanTSyn_00125 */
  if (0 == context->DebounceCounter) {
    if (CANTSYN_MASTER_SYNC == context->State) {
      CanTSyn_TransmitSYNC(domain);
    } else if (CANTSYN_MASTER_FUP == context->State) {
      CanTSyn_TransmitFUP(domain);
    }
  }
}

static void CanTSyn_MainSlave(const CanTSyn_GlobalTimeDomainType *domain) {
  CanTSyn_SlaveContextType *context = domain->U.S.Context;

  if (CANTSYN_SLAVE_IDLE != context->State) {
    if (context->Timer > 0) {
      context->Timer--;
      if (0 == context->Timer) {
        ASLOG(CANTSYNE, ("Slave timeout in state %d\n", context->State));
        context->State = CANTSYN_SLAVE_IDLE;
      }
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void CanTSyn_Init(const CanTSyn_ConfigType *configPtr) {
  int i;
  const CanTSyn_GlobalTimeDomainType *domain;
  for (i = 0; i < CANTSYN_CONFIG->NumOfGlobalTimeDomains; i++) {
    domain = &CANTSYN_CONFIG->GlobalTimeDomains[i];
    if (CANTSYN_MASTER == domain->DomainType) {
      memset(domain->U.M.Context, 0, sizeof(CanTSyn_MasterContextType));
      domain->U.M.Context->Timer = domain->U.M.Master->GlobalTimeTxPeriod;
    } else {
      memset(domain->U.S.Context, 0, sizeof(CanTSyn_SlaveContextType));
      /* @SWS_CanTSyn_00079 */
      domain->U.S.Context->SC = CANTSYNC_SC_UNKNOWN;
    }
  }
}

void CanTSyn_SetTransmissionMode(uint8_t ctrlIdx, CanTSyn_TransmissionModeType mode) {
}

void CanTSyn_RxIndication(PduIdType rxPduId, const PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_OK;
  const CanTSyn_GlobalTimeDomainType *domain;
  ASLOG(CANTSYN, ("[%d]RX len=%d data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", rxPduId,
                  pduInfoPtr->SduLength, pduInfoPtr->SduDataPtr[0], pduInfoPtr->SduDataPtr[1],
                  pduInfoPtr->SduDataPtr[2], pduInfoPtr->SduDataPtr[3], pduInfoPtr->SduDataPtr[4],
                  pduInfoPtr->SduDataPtr[5], pduInfoPtr->SduDataPtr[6], pduInfoPtr->SduDataPtr[7]));
  if (rxPduId < CANTSYN_CONFIG->NumOfGlobalTimeDomains) {
    domain = &CANTSYN_CONFIG->GlobalTimeDomains[rxPduId];
    if (CANTSYN_SLAVE == domain->DomainType) {
      if (domain->UseExtendedMsgFormat) {
        if (16 != pduInfoPtr->SduLength) {
          ret = E_NOT_OK;
          ASLOG(CANTSYNE, ("[%d] message length is not 16\n", (int)rxPduId));
        }
      } else {
        if (8 != pduInfoPtr->SduLength) {
          ret = E_NOT_OK;
          ASLOG(CANTSYNE, ("[%d] message length is not 8\n", (int)rxPduId));
        }
      }
    } else {
      ret = E_NOT_OK;
      ASLOG(CANTSYNE, ("[%d] domain type not slave\n", (int)rxPduId));
    }
  } else {
    ASLOG(CANTSYNE, ("Invalid RxPduId=%d\n", (int)rxPduId));
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    switch (pduInfoPtr->SduDataPtr[0]) {
    case CANTSYN_SYNC_NO_CRC:
    case CANTSYN_SYNC_WITH_CRC:
      CanTSyn_HandleMsgSync(domain, &pduInfoPtr->SduDataPtr[0], pduInfoPtr->SduLength);
      break;
    case CANTSYN_FUP_NO_CRC:
    case CANTSYN_FUP_WITH_CRC:
      CanTSyn_HandleMsgFup(domain, &pduInfoPtr->SduDataPtr[0], pduInfoPtr->SduLength);
      break;
    default:
      ASLOG(CANTSYNE,
            ("[%d] invalid message type 0x%02x\n", (int)rxPduId, pduInfoPtr->SduDataPtr[0]));
      break;
    }
  }
}

void CanTSyn_TxConfirmation(PduIdType txPduId, Std_ReturnType result) {
  Std_ReturnType ret;
  const CanTSyn_GlobalTimeDomainType *domain;
  CanTSyn_MasterContextType *context;
  if (txPduId < CANTSYN_CONFIG->NumOfGlobalTimeDomains) {
    domain = &CANTSYN_CONFIG->GlobalTimeDomains[txPduId];
    if (CANTSYN_MASTER == domain->DomainType) {
      context = domain->U.M.Context;
      if (CANTSYN_MASTER_SYNCED == context->State) {
        ret = StbM_GetCurrentVirtualLocalTime(domain->SynchronizedTimeBaseRef, &context->T1r);
        if (E_OK == ret) {
          context->State = CANTSYN_MASTER_FUP;
        } else {
          context->State = CANTSYN_MASTER_IDLE;
        }
      } else if (CANTSYN_MASTER_FUPED == context->State) {
        context->State = CANTSYN_MASTER_IDLE;
      }
    }
  }
}

void CanTSyn_MainFunction(void) {
  int i;
  const CanTSyn_GlobalTimeDomainType *domain;
  for (i = 0; i < CANTSYN_CONFIG->NumOfGlobalTimeDomains; i++) {
    domain = &CANTSYN_CONFIG->GlobalTimeDomains[i];
    if (CANTSYN_MASTER == domain->DomainType) {
      CanTSyn_MainMaster(domain);
    } else {
      CanTSyn_MainSlave(domain);
    }
  }
}

void CanTSyn_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x02, CANTSYN_E_NULL_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_CANTSYN;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 1;
}

/** @brief release notes
 * - 4.0.1: Avoid to use uint64 to calculate time difference in CanTSyn_CalcGlobalTimestamp
 */
