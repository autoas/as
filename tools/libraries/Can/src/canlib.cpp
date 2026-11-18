/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#define AS_LOG_DEFAULT AS_LOG_ERROR
#include "canlib.h"
#include "canlib_types.hpp"
#include <ctype.h>
#include <mutex>
#include <map>
#include <chrono>
#include <memory>
#include <condition_variable>
#include "Std_Timer.h"
#include "Std_Topic.h"
#include "Log.hpp"

using namespace as;
/* ================================ [ MACROS    ] ============================================== */
#define CAN_BUS_NUM 32
#define CAN_BUS_PDU_NUM 128
#define CAN_BUS_Q_PDU_NUM 1024

#define AS_LOG_CAN 0
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  /* the CAN ID, 29 or 11-bit */
  uint32_t Id;
  /* Length, max 8 bytes */
  uint8_t Length;
  /* data ptr */
  uint8_t Sdu[CANL_MAX_MTU];
  uint64_t Timestamp; /* timestamp */
} CanL_PduType;

struct CanL_PduNodeType {
  CanL_PduType msg;
  STAILQ_ENTRY(CanL_PduNodeType) entry;  /* entry for CanL_PduQueueType, sort by CANID queue*/
  STAILQ_ENTRY(CanL_PduNodeType) entry2; /* entry for CanL_BusType, sort by CANID queue*/
};

struct CanL_PduQueueType {
  uint32_t Id; /* can_id of this list */
  uint32_t Size;
  uint32_t Warning;
  STAILQ_HEAD(, CanL_PduNodeType) head;
  STAILQ_ENTRY(CanL_PduQueueType) entry;
};

struct CanL_BusType {
  CanL_DeviceType device;
  STAILQ_HEAD(, CanL_PduQueueType) head; /* sort message by CANID queue */
  STAILQ_HEAD(, CanL_PduNodeType) head2; /* for all the message received by this bus */
  uint32_t Size2;

  STAILQ_HEAD(, CanL_PduNodeType)
  headQ; /* for all the message RX or TX by this bus with single Queue */
  uint32_t SizeQ;
  bool WarningQ;
  STAILQ_ENTRY(CanL_BusType) entry;
  uint32_t Ref;

  std::condition_variable condVar; /* for any message received by this bus */
  std::recursive_mutex QLock;
};

struct CanL_BusListType {
  boolean Initialized;
  std::recursive_mutex QLock;
  uint32_t BusIdMask; /* I am going to support only 64 bus */
  STAILQ_HEAD(, CanL_BusType) head;
};

/* ================================ [ DECLARES  ] ============================================== */
extern "C" const CanL_DeviceOpsType CanL_SimulatorOps;
extern "C" const CanL_DeviceOpsType CanL_SimulatorV2Ops;
extern "C" const CanL_DeviceOpsType CanL_QemuOps;
#if defined(_WIN32) && !defined(ANDROID)
extern "C" const CanL_DeviceOpsType CanL_PeakOps;
extern "C" const CanL_DeviceOpsType CanL_PeakFdOps;
extern "C" const CanL_DeviceOpsType CanL_VxlOps;
extern "C" const CanL_DeviceOpsType CanL_ZlgOps;
#endif

static void CanL_LogCan(boolean isRx, int busid, uint32_t canid, uint8_t dlc, const uint8_t *data,
                        uint64_t timestamp);
/* ================================ [ DATAS     ] ============================================== */
static struct CanL_BusListType canlBusList = {
  .Initialized = false,
  .BusIdMask = 0,
};
static const CanL_DeviceOpsType *canOps[] = {
  &CanL_SimulatorOps,
  &CanL_SimulatorV2Ops,
  &CanL_QemuOps,
#if defined(_WIN32) && !defined(ANDROID)
  &CanL_PeakOps,
  &CanL_PeakFdOps,
  &CanL_VxlOps,
  &CanL_ZlgOps,
#endif
  NULL,
};
static Logger *canlLogger = nullptr;
static bool canlPerfMode = false;
/* ================================ [ LOCALS    ] ============================================== */
static void CanL_FreeQueue(struct CanL_PduQueueType *l) {
  struct CanL_PduNodeType *pdu;
  while (false == STAILQ_EMPTY(&l->head)) {
    pdu = STAILQ_FIRST(&l->head);
    STAILQ_REMOVE_HEAD(&l->head, entry);

    free(pdu);
  }
}
static void CanL_FreeBus(struct CanL_BusType *b) {
  struct CanL_PduQueueType *l;
  while (false == STAILQ_EMPTY(&b->head)) {
    l = STAILQ_FIRST(&b->head);
    STAILQ_REMOVE_HEAD(&b->head, entry);
    CanL_FreeQueue(l);
    free(l);
  }
}
static void CanL_FreeHead(struct CanL_BusListType *h) {
  struct CanL_BusType *b;

  std::lock_guard<std::recursive_mutex> lg(canlBusList.QLock);
  while (false == STAILQ_EMPTY(&h->head)) {
    b = STAILQ_FIRST(&h->head);
    STAILQ_REMOVE_HEAD(&h->head, entry);
    CanL_FreeBus(b);
    free(b);
  }
}

static struct CanL_BusType *CanL_GetBus(int busid) {
  struct CanL_BusType *handle, *h;
  handle = NULL;

  if (canlBusList.Initialized) {
    std::lock_guard<std::recursive_mutex> lg(canlBusList.QLock);
    STAILQ_FOREACH(h, &canlBusList.head, entry) {
      if (h->device.BusId == busid) {
        handle = h;
        break;
      }
    }
  }

  return handle;
}

static struct CanL_BusType *CanL_GetBusByName(const char *device_name, uint32_t port) {
  struct CanL_BusType *handle, *h;
  handle = NULL;
  if (canlBusList.Initialized) {
    std::lock_guard<std::recursive_mutex> lg(canlBusList.QLock);
    STAILQ_FOREACH(h, &canlBusList.head, entry) {
      if ((h->device.DeviceName == device_name) && (h->device.Port == port)) {
        handle = h;
        break;
      }
    }
  }
  return handle;
}

static void CanL_SaveQueue(struct CanL_BusType *b, uint32_t canid, uint8_t dlc, const uint8_t *data,
                           uint64_t timestamp) {
  struct CanL_PduNodeType *pdu;
  if (b->SizeQ > CAN_BUS_Q_PDU_NUM) {
    if (false == b->WarningQ) {
      b->WarningQ = true;
      ASLOG(WARN, ("CAN BUSQ[id=%X] List is full with size %d\n", b->device.BusId, b->SizeQ));
    }
    return;
  }
  pdu = (struct CanL_PduNodeType *)malloc(sizeof(struct CanL_PduNodeType));
  if (NULL != pdu) {
    pdu->msg.Id = canid;
    pdu->msg.Length = dlc;
    pdu->msg.Timestamp = timestamp;
    memcpy(&(pdu->msg.Sdu), data, dlc);
    std::lock_guard<std::recursive_mutex> lg(b->QLock);
    STAILQ_INSERT_TAIL(&b->headQ, pdu, entry);
    b->SizeQ++;
  }
}

static struct CanL_PduNodeType *CanL_GetQueue(struct CanL_BusType *b) {
  struct CanL_PduNodeType *pdu = NULL;
  std::lock_guard<std::recursive_mutex> lg(b->QLock);
  if ((false == STAILQ_EMPTY(&b->headQ))) {
    pdu = STAILQ_FIRST(&b->headQ);
    STAILQ_REMOVE_HEAD(&b->headQ, entry);
    b->SizeQ--;
  }
  return pdu;
}

static int CanL_AllocBusId(void) {
  int i;
  int busid = -1;

  std::lock_guard<std::recursive_mutex> lg(canlBusList.QLock);
  for (i = 0; i < CAN_BUS_NUM; i++) {
    if (0 == ((1 << i) & canlBusList.BusIdMask)) {
      busid = i;
      canlBusList.BusIdMask |= 1 << busid;
      break;
    }
  }

  return busid;
}

static void CanL_FreeBusId(int busid) {
  std::lock_guard<std::recursive_mutex> lg(canlBusList.QLock);
  canlBusList.BusIdMask &= ~(1 << busid);
}

static struct CanL_PduNodeType *CanL_GetPdu(struct CanL_BusType *b, uint32_t canid) {
  struct CanL_PduQueueType *L = NULL;
  struct CanL_PduNodeType *pdu = NULL;
  struct CanL_PduQueueType *l;

  if ((uint32_t)-2 == canid) {
    return CanL_GetQueue(b);
  }

  std::lock_guard<std::recursive_mutex> lg(b->QLock);
  if ((uint32_t)-1 == canid) { /* id is -1, means get the first of queue from b->head2 */
    if (false == STAILQ_EMPTY(&b->head2)) {
      pdu = STAILQ_FIRST(&b->head2);
      /* get the first message canid, and then search CANID queue L */
      canid = pdu->msg.Id;
    } else {
      /* no message all is empty */
    }
  }
  /* search queue specified by canid */
  STAILQ_FOREACH(l, &b->head, entry) {
    if ((l->Id & (~CANL_ID_EXTENDED)) == (canid & (~CANL_ID_EXTENDED))) {
      L = l;
      break;
    }
  }
  if (L && (false == STAILQ_EMPTY(&L->head))) {
    pdu = STAILQ_FIRST(&L->head);
    /* when remove, should remove from the both queue */
    STAILQ_REMOVE_HEAD(&L->head, entry);
    STAILQ_REMOVE(&b->head2, pdu, CanL_PduNodeType, entry2);
    b->Size2--;
    L->Size--;
  }

  return pdu;
}

static bool CanL_HasPdu(struct CanL_BusType *b, uint32_t canid) {
  struct CanL_PduQueueType *L = NULL;
  struct CanL_PduNodeType *pdu = NULL;
  struct CanL_PduQueueType *l;
  bool bHasPdu = false;

  std::lock_guard<std::recursive_mutex> lg(b->QLock);
  if ((uint32_t)-2 == canid) {
    if ((false == STAILQ_EMPTY(&b->headQ))) {
      return true;
    }
  }

  if ((uint32_t)-1 == canid) { /* id is -1, means get the first of queue from b->head2 */
    if (false == STAILQ_EMPTY(&b->head2)) {
      pdu = STAILQ_FIRST(&b->head2);
      /* get the first message canid, and then search CANID queue L */
      canid = pdu->msg.Id;
    } else {
      /* no message all is empty */
    }
  }
  /* search queue specified by canid */
  STAILQ_FOREACH(l, &b->head, entry) {
    if ((l->Id & (~CANL_ID_EXTENDED)) == (canid & (~CANL_ID_EXTENDED))) {
      L = l;
      break;
    }
  }
  if (L && (false == STAILQ_EMPTY(&L->head))) {
    bHasPdu = true;
  }

  return bHasPdu;
}

static void CanL_SaveBus(struct CanL_BusType *b, struct CanL_PduNodeType *pdu) {
  struct CanL_PduQueueType *L;
  struct CanL_PduQueueType *l;
  L = NULL;
  std::lock_guard<std::recursive_mutex> lg(b->QLock);
  STAILQ_FOREACH(l, &b->head, entry) {
    if (l->Id == pdu->msg.Id) {
      L = l;
      break;
    }
  }

  if (NULL == L) {
    L = (struct CanL_PduQueueType *)malloc(sizeof(struct CanL_PduQueueType));
    if (L) {
      L->Id = pdu->msg.Id;
      L->Size = 0;
      L->Warning = false;
      STAILQ_INIT(&L->head);
      STAILQ_INSERT_TAIL(&b->head, L, entry);
    } else {
      ASLOG(WARN, ("CAN Bus List malloc failed\n"));
    }
  }

  if (L) {
    /* limit by CANID queue is better than the whole bus one */
    if (L->Size < CAN_BUS_PDU_NUM) {
      STAILQ_INSERT_TAIL(&L->head, pdu, entry);
      STAILQ_INSERT_TAIL(&b->head2, pdu, entry2);
      b->Size2++;
      L->Size++;
      L->Warning = false;
    } else {
      if (L->Warning == false) {
        ASLOG(WARN, ("CAN Q[id=%X] List is full with size %d\n", L->Id, L->Size));
        L->Warning = true;
      }
      free(pdu);
    }
  }
}

static void CanL_RxIndication(int busid, uint32_t canid, uint8_t dlc, uint8_t *data,
                              uint64_t timestamp) {
  if ((busid < CAN_BUS_NUM) && ((uint32_t)-1 != canid)) {
    /* canid -1 reserved for CanL_Read get the first received CAN message on bus */
    struct CanL_BusType *b = CanL_GetBus(busid);
    if (NULL != b) {
      struct CanL_PduNodeType *pdu =
        (struct CanL_PduNodeType *)malloc(sizeof(struct CanL_PduNodeType));
      if (pdu) {
        if (0 == timestamp) {
          pdu->msg.Timestamp = PAL_Timestamp();
        } else {
          pdu->msg.Timestamp = timestamp;
        }
        pdu->msg.Id = canid;
        pdu->msg.Length = dlc;
        memcpy(&(pdu->msg.Sdu), data, dlc);

        CanL_SaveBus(b, pdu);
        CanL_SaveQueue(b, canid, dlc, data, timestamp);
        CanL_LogCan(true, busid, canid, dlc, data, timestamp);

        b->condVar.notify_all();
      } else {
        ASLOG(WARN, ("CAN RX malloc failed\n"));
      }
    } else {
      /* not on-line */
      ASLOG(CAN, ("CAN is not on-line now!\n"));
    }
  } else {
    ASLOG(WARN, ("CAN RX bus <%d> out of range, busid < %d is support only\n", busid, CAN_BUS_NUM));
  }

  ASLOG(CAN, ("RX CAN ID=0x%08X LEN=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X]\n", canid,
              dlc, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
}
static const CanL_DeviceOpsType *CanL_SearchOps(const char *name) {
  const CanL_DeviceOpsType *ops, **o;
  o = canOps;
  ops = NULL;
  while (*o != NULL) {
    if (0 == strcmp((*o)->Name.c_str(), name)) {
      ops = *o;
      break;
    }
    o++;
  }

  return ops;
}

static void CanL_LogCan(boolean isRx, int busid, uint32_t canid, uint8_t dlc, const uint8_t *data,
                        uint64_t timestamp) {
  uint32_t i;
  char ts[64];

  STD_TOPIC_CAN(busid, isRx, canid, dlc, data);

  if (NULL != canlLogger) {
    Std_GetDateTime(ts, sizeof(ts));
    std::lock_guard<std::recursive_mutex> lg(canlBusList.QLock);
    canlLogger->Print("busid=%d %s canid=%08X dlc=%d data=[", busid, isRx ? "Rx" : "Tx",
                      canid & (~CANL_ID_EXTENDED), dlc);
    if (dlc < 8) {
      dlc = 8;
    }
    for (i = 0; i < dlc; i++) {
      canlLogger->Print("%02X,", data[i]);
    }

    canlLogger->Print("] [");

    for (i = 0; i < dlc; i++) {
      if (isprint(data[i])) {
        canlLogger->Print("%c", data[i]);
      } else {
        canlLogger->Print(".");
      }
    }
    canlLogger->Print("] @ %s timestamp=%" PRIu64 "\n", ts, timestamp);
    canlLogger->Check();
  }
}

class CanLInitializer {
public:
  CanLInitializer() {
    char ts[64];

    if (false == canlBusList.Initialized) {
      canlBusList.Initialized = true;
      canlBusList.BusIdMask = 0;
      STAILQ_INIT(&canlBusList.head);
      char *logName = getenv("CAN_LOG_NAME");
      if (logName != NULL) {
        canlLogger = new Logger(logName);
        if (NULL != canlLogger) {
          Std_GetDateTime(ts, sizeof(ts));
          ASLOG(INFO, ("can trace logger < %s >\n", logName));
          canlLogger->Print("can log <%s> %s\n\n", logName, ts);
        } else {
          ASLOG(ERROR, ("CanL: failed to create logger %s\n", logName));
        }
      }
      char *perfMode = getenv("CAN_PERF_MODE");
      if ((nullptr != perfMode) && (perfMode == std::string("YES"))) {
        canlPerfMode = true;
      }
    }
  }

  ~CanLInitializer() {
    if (canlBusList.Initialized) {
      struct CanL_BusType *b;
      STAILQ_FOREACH(b, &canlBusList.head, entry) {
        b->device.Ops->Close(b->device.Port);
      }
      CanL_FreeHead(&canlBusList);
      canlBusList.Initialized = false;

      if (NULL != canlLogger) {
        delete canlLogger;
      }
    }
  }
};

static std::shared_ptr<CanLInitializer> canlInitializer = nullptr;
/* ================================ [ FUNCTIONS ] ============================================== */
bool CanL_IsPerfMode(void) {
  return canlPerfMode;
}

int CanL_Open(const char *device_name, uint32_t port, uint32_t baudrate) {
  int rv;
  int busid = -1;
  const CanL_DeviceOpsType *ops;

  std::lock_guard<std::recursive_mutex> lg(canlBusList.QLock);
  if (nullptr == canlInitializer) {
    canlInitializer = std::make_shared<CanLInitializer>();
  }
  ops = CanL_SearchOps(device_name);
  struct CanL_BusType *b = CanL_GetBusByName(device_name, port);
  rv = false;
  if (NULL != b) {
    if (b->device.Baudrate == baudrate) {
      b->Ref++;
      busid = b->device.BusId;
      if (canlLogger) {
        canlLogger->Print("reopen %s:%d baudrate=%d, busid %d\n", b->device.DeviceName.c_str(),
                          b->device.Port, b->device.Baudrate, b->device.BusId);
      }
    } else {
      ASLOG(ERROR, ("CanL_Open: device <%s:%d> already opened with baudrate %d!, can't reopen with "
                    "new baudrate %d\n",
                    device_name, port, b->device.Baudrate, baudrate));
    }
  } else {
    if (NULL != ops) {
      busid = CanL_AllocBusId();
      if (busid >= 0) {
        b = new struct CanL_BusType;
        b->device.BusId = busid;
        b->device.Ops = ops;
        b->device.BusId = busid;
        b->device.Port = port;
        b->device.DeviceName = device_name;
        b->device.Baudrate = baudrate;
        b->Ref = 1;

        rv = ops->Probe(busid, port, baudrate, CanL_RxIndication);
      }

      if (rv) {
        STAILQ_INIT(&b->head);
        STAILQ_INIT(&b->head2);
        STAILQ_INIT(&b->headQ);
        b->Size2 = 0;
        b->SizeQ = 0;
        STAILQ_INSERT_TAIL(&canlBusList.head, b, entry);
        /* result OK */
        if (canlLogger) {
          canlLogger->Print("open %s:%d baudrate=%d as busid %d\n", b->device.DeviceName.c_str(),
                            b->device.Port, b->device.Baudrate, b->device.BusId);
        }
      } else {
        if (NULL != b) {
          delete b;
        }

        if (busid >= 0) {
          CanL_FreeBusId(busid);
          busid = -1;
        }
        ASLOG(ERROR, ("CanL_Open device <%s> failed!\n", device_name));
      }
    } else {
      ASLOG(ERROR, ("CanL_Open device <%s> is not known!\n", device_name));
    }
  }

  return busid;
}

bool CanL_Write(int busid, uint32_t canid, uint8_t dlc, const uint8_t *data) {
  bool rv = false;
  struct CanL_BusType *b = CanL_GetBus(busid);
  if (NULL == b) {
    ASLOG(ERROR, ("can bus(%d) is not on-line 'CanL_Write'\n", (int)busid));
  } else if (dlc > 64) {
    ASLOG(ERROR, ("can bus(%d) 'CanL_Write' with invalid dlc(%d>8)\n", (int)busid, (int)dlc));
  } else {
    if (b->device.Ops->Write) {
      uint64_t timestamp = PAL_Timestamp();
      CanL_LogCan(false, busid, canid, dlc, data, timestamp);
      rv = b->device.Ops->Write(b->device.Port, canid, dlc, data, timestamp);
      CanL_SaveQueue(b, canid, dlc, data, timestamp);
      if (rv) {
        /* result OK */
      } else {
        ASLOG(ERROR, ("CanL_Write bus(%d) failed!\n", (int)busid));
      }
    } else {
      ASLOG(ERROR, ("can bus(%d) is read-only 'CanL_Write'\n", (int)busid));
    }
  }

  return rv;
}

bool CanL_WriteV2(int busid, CanL_FrameType *can_frame) {
  bool rv = false;
  struct CanL_BusType *b = CanL_GetBus(busid);
  if (NULL == b) {
    ASLOG(ERROR, ("can bus(%d) is not on-line 'CanL_Write'\n", (int)busid));
  } else if (can_frame->Dlc > 64) {
    ASLOG(ERROR,
          ("can bus(%d) 'CanL_Write' with invalid dlc(%d>8)\n", (int)busid, (int)can_frame->Dlc));
  } else {
    if (b->device.Ops->Write) {
      CanL_LogCan(false, busid, can_frame->CanId, can_frame->Dlc, can_frame->Data,
                  can_frame->Timestamp);
      rv = b->device.Ops->Write(b->device.Port, can_frame->CanId, can_frame->Dlc, can_frame->Data,
                                can_frame->Timestamp);
      CanL_SaveQueue(b, can_frame->CanId, can_frame->Dlc, can_frame->Data, can_frame->Timestamp);
      if (rv) {
        /* result OK */
      } else {
        ASLOG(ERROR, ("CanL_Write bus(%d) failed!\n", (int)busid));
      }
    } else {
      ASLOG(ERROR, ("can bus(%d) is read-only 'CanL_Write'\n", (int)busid));
    }
  }

  return rv;
}

bool CanL_Read(int busid, uint32_t *canid, uint8_t *dlc, uint8_t *data) {
  bool rv = false;
  struct CanL_PduNodeType *pdu;
  struct CanL_BusType *b = CanL_GetBus(busid);
  uint8_t len = *dlc;

  *dlc = 0;
  if (NULL == b) {
    ASLOG(ERROR, ("bus(%d) is not on-line 'CanL_Read'\n", (int)busid));
  } else if (NULL == canid) {
    ASLOG(ERROR, ("bus(%d) 'CanL_Read' with NULL canid\n", (int)busid));
  } else {
    pdu = CanL_GetPdu(b, *canid);
    if (NULL == pdu) {
      /* no data */
    } else if ((data == NULL) || (len < pdu->msg.Length)) {
      ASLOG(ERROR, ("bus(%d) 'CanL_Read' with invalid args: canid=%x, dlc=%d\n", (int)busid,
                    (int)pdu->msg.Id, (int)pdu->msg.Length));
    } else {
      *canid = pdu->msg.Id;
      *dlc = pdu->msg.Length;
      memcpy(data, pdu->msg.Sdu, *dlc);
      if (pdu->msg.Length < len) {
        memset(&data[pdu->msg.Length], 0x55, len - pdu->msg.Length);
      }
      free(pdu);
      rv = true;
    }
  }

  return rv;
}

bool CanL_ReadV2(int busid, CanL_FrameType *can_frame) {
  bool rv = false;
  struct CanL_PduNodeType *pdu;
  struct CanL_BusType *b = CanL_GetBus(busid);
  uint8_t len = sizeof(can_frame->Data);

  if (NULL == b) {
    ASLOG(ERROR, ("bus(%d) is not on-line 'CanL_ReadV2'\n", (int)busid));
  } else {
    pdu = CanL_GetPdu(b, can_frame->CanId);
    if (NULL == pdu) {
      /* no data */
    } else {
      can_frame->CanId = pdu->msg.Id;
      can_frame->Dlc = pdu->msg.Length;
      memcpy(can_frame->Data, pdu->msg.Sdu, can_frame->Dlc);
      if (pdu->msg.Length < len) {
        memset(&can_frame->Data[pdu->msg.Length], 0x55, len - pdu->msg.Length);
      }
      can_frame->Timestamp = pdu->msg.Timestamp;
      free(pdu);
      rv = true;
    }
  }

  return rv;
}

bool CanL_Close(int busid) {
  bool rv;
  struct CanL_BusType *b = CanL_GetBus(busid);
  rv = false;
  if (NULL == b) {
    ASLOG(ERROR, ("can bus(%d) is not on-line 'CanL_Close'\n", (int)busid));
  } else {
    std::unique_lock<std::recursive_mutex> lg(canlBusList.QLock);
    b->Ref--;
    if (0 == b->Ref) {
      /* unlock to avoid under layer rxNotification dead lock */
      lg.unlock();
      b->device.Ops->Close(b->device.Port);
      lg.lock();
      STAILQ_REMOVE(&canlBusList.head, b, CanL_BusType, entry);
      CanL_FreeBus(b);
      delete b;
      CanL_FreeBusId(busid);
    }
    rv = true;
  }

  return rv;
}

bool CanL_Reset(int busid) {
  bool rv;
  struct CanL_BusType *b = CanL_GetBus(busid);
  rv = false;
  if (NULL == b) {
    ASLOG(ERROR, ("can bus(%d) is not on-line 'CanL_Reset'\n", (int)busid));
  } else {
    if (NULL != b->device.Ops->Reset) {
      rv = b->device.Ops->Reset(b->device.Port);
    } else {
      rv = true;
    }
  }

  return rv;
}

bool CanL_Wait(int busid, uint32_t canid, uint32_t timeoutMs) {
  bool rv;
  struct CanL_BusType *b = CanL_GetBus(busid);
  rv = false;
  auto begin = std::chrono::high_resolution_clock::now();
  uint32_t leftMs = timeoutMs;
  if (NULL == b) {
    ASLOG(ERROR, ("can bus(%d) is not on-line 'CanL_Wait'\n", (int)busid));
  } else {
    rv = CanL_HasPdu(b, canid);
    while (false == rv) {
      std::mutex lock;
      std::unique_lock<std::mutex> lck(lock);
      auto now = std::chrono::high_resolution_clock::now();
      uint32_t elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - begin).count();
      if (elapsedMs >= timeoutMs) {
        break;
      }
      leftMs = timeoutMs - elapsedMs;
      auto status = b->condVar.wait_for(lck, std::chrono::milliseconds(leftMs));
      if (std::cv_status::timeout != status) {
        rv = CanL_HasPdu(b, canid);
        if (true == rv) {
          break;
        }
      } else {
        rv = false;
        break;
      }
    }
  }

  return rv;
}

bool CanL_WaitV2(int busid, uint32_t canid, uint32_t timeoutMs) {
  bool rv;
  struct CanL_BusType *b = CanL_GetBus(busid);
  rv = false;
  auto begin = std::chrono::high_resolution_clock::now();
  if (NULL == b) {
    ASLOG(ERROR, ("can bus(%d) is not on-line 'CanL_Wait'\n", (int)busid));
  } else {
    rv = CanL_HasPdu(b, canid);
    while (false == rv) {
      auto now = std::chrono::high_resolution_clock::now();
      uint32_t elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - begin).count();
      if (elapsedMs >= timeoutMs) {
        break;
      }
      b->device.Ops->Read(b->device.Port);
      rv = CanL_HasPdu(b, canid);
      if (true == rv) {
        break;
      }
    }
  }

  return rv;
}
