/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#define AS_LOG_DEFAULT AS_LOG_ERROR
#include "canlib.h"
#include "canlib_types.h"
#include <ctype.h>
#include "osal.h"
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
  uint32_t id;
  /* Length, max 8 bytes */
  uint8_t length;
  /* data ptr */
  uint8_t sdu[64];
} Can_PduType;
struct Can_Pdu_s {
  Can_PduType msg;
  STAILQ_ENTRY(Can_Pdu_s) entry;  /* entry for Can_PduQueue_s, sort by CANID queue*/
  STAILQ_ENTRY(Can_Pdu_s) entry2; /* entry for Can_Bus_s, sort by CANID queue*/
};

struct Can_PduQueue_s {
  uint32_t id; /* can_id of this list */
  uint32_t size;
  uint32_t warning;
  STAILQ_HEAD(, Can_Pdu_s) head;
  STAILQ_ENTRY(Can_PduQueue_s) entry;
};

struct Can_Bus_s {
  Can_DeviceType device;
  STAILQ_HEAD(, Can_PduQueue_s) head; /* sort message by CANID queue */
  STAILQ_HEAD(, Can_Pdu_s) head2;     /* for all the message received by this bus */
  uint32_t size2;

  STAILQ_HEAD(, Can_Pdu_s) headQ; /* for all the message RX or TX by this bus with single Queue */
  uint32_t sizeQ;
  bool warningQ;
  STAILQ_ENTRY(Can_Bus_s) entry;
  uint32_t ref;
  OSAL_MutexType q_lock;
};

struct Can_BusList_s {
  boolean initialized;
  OSAL_MutexType q_lock;
  uint32_t busidMask; /* I am going to support only 64 bus */
  STAILQ_HEAD(, Can_Bus_s) head;
};

/* ================================ [ DECLARES  ] ============================================== */
extern "C" const Can_DeviceOpsType can_simulator_ops;
extern "C" const Can_DeviceOpsType can_simulator_v2_ops;
extern "C" const Can_DeviceOpsType can_qs_ops;
#if defined(_WIN32) && !defined(ANDROID)
extern "C" const Can_DeviceOpsType can_peak_ops;
extern "C" const Can_DeviceOpsType can_peakfd_ops;
extern "C" const Can_DeviceOpsType can_vxl_ops;
extern "C" const Can_DeviceOpsType can_zlg_ops;
#endif

static void logCan(boolean isRx, int busid, uint32_t canid, uint8_t dlc, const uint8_t *data);
/* ================================ [ DATAS     ] ============================================== */
static struct Can_BusList_s canbusH = {
  .initialized = false,
  .q_lock = NULL,
  .busidMask = 0,
};
static const Can_DeviceOpsType *canOps[] = {
  &can_simulator_ops,
  &can_simulator_v2_ops,
  &can_qs_ops,
#if defined(_WIN32) && !defined(ANDROID)
  &can_peak_ops,
  &can_peakfd_ops,
  &can_vxl_ops,
  &can_zlg_ops,
#endif
  NULL,
};
static Logger *s_Logger = nullptr;
/* ================================ [ LOCALS    ] ============================================== */
static void freeQ(struct Can_PduQueue_s *l) {
  struct Can_Pdu_s *pdu;
  while (false == STAILQ_EMPTY(&l->head)) {
    pdu = STAILQ_FIRST(&l->head);
    STAILQ_REMOVE_HEAD(&l->head, entry);

    free(pdu);
  }
}
static void freeB(struct Can_Bus_s *b) {
  struct Can_PduQueue_s *l;
  while (false == STAILQ_EMPTY(&b->head)) {
    l = STAILQ_FIRST(&b->head);
    STAILQ_REMOVE_HEAD(&b->head, entry);
    freeQ(l);
    free(l);
  }
}
static void freeH(struct Can_BusList_s *h) {
  struct Can_Bus_s *b;

  OSAL_MutexLock(h->q_lock);
  while (false == STAILQ_EMPTY(&h->head)) {
    b = STAILQ_FIRST(&h->head);
    STAILQ_REMOVE_HEAD(&h->head, entry);
    freeB(b);
    free(b);
  }
  OSAL_MutexUnlock(h->q_lock);
}

static struct Can_Bus_s *getBus(int busid) {
  struct Can_Bus_s *handle, *h;
  handle = NULL;

  if (canbusH.initialized) {
    (void)OSAL_MutexLock(canbusH.q_lock);
    STAILQ_FOREACH(h, &canbusH.head, entry) {
      if (h->device.busid == busid) {
        handle = h;
        break;
      }
    }
    (void)OSAL_MutexUnlock(canbusH.q_lock);
  }

  return handle;
}

static struct Can_Bus_s *getBusByName(const char *device_name, uint32_t port) {
  struct Can_Bus_s *handle, *h;
  handle = NULL;
  if (canbusH.initialized) {
    (void)OSAL_MutexLock(canbusH.q_lock);
    STAILQ_FOREACH(h, &canbusH.head, entry) {
      if ((0 == strcmp(h->device.device_name, device_name)) && (h->device.port == port)) {
        handle = h;
        break;
      }
    }
    (void)OSAL_MutexUnlock(canbusH.q_lock);
  }
  return handle;
}

static void saveQ(struct Can_Bus_s *b, uint32_t canid, uint8_t dlc, const uint8_t *data) {
  struct Can_Pdu_s *pdu;
  if (b->sizeQ > CAN_BUS_Q_PDU_NUM) {
    if (false == b->warningQ) {
      b->warningQ = true;
      ASLOG(WARN, ("CAN BUSQ[id=%X] List is full with size %d\n", b->device.busid, b->sizeQ));
    }
    return;
  }
  pdu = (struct Can_Pdu_s *)malloc(sizeof(struct Can_Pdu_s));
  if (NULL != pdu) {
    pdu->msg.id = canid;
    pdu->msg.length = dlc;
    memcpy(&(pdu->msg.sdu), data, dlc);
    (void)OSAL_MutexLock(b->q_lock);
    STAILQ_INSERT_TAIL(&b->headQ, pdu, entry);
    b->sizeQ++;
    (void)OSAL_MutexUnlock(b->q_lock);
  }
}

static struct Can_Pdu_s *getQ(struct Can_Bus_s *b) {
  struct Can_Pdu_s *pdu = NULL;
  (void)OSAL_MutexLock(b->q_lock);
  if ((false == STAILQ_EMPTY(&b->headQ))) {
    pdu = STAILQ_FIRST(&b->headQ);
    STAILQ_REMOVE_HEAD(&b->headQ, entry);
    b->sizeQ--;
  }
  (void)OSAL_MutexUnlock(b->q_lock);
  return pdu;
}

static int allocBusId(void) {
  int i;
  int busid = -1;

  (void)OSAL_MutexLock(canbusH.q_lock);
  for (i = 0; i < CAN_BUS_NUM; i++) {
    if (0 == ((1 << i) & canbusH.busidMask)) {
      busid = i;
      canbusH.busidMask |= 1 << busid;
      break;
    }
  }
  (void)OSAL_MutexUnlock(canbusH.q_lock);

  return busid;
}

static void freeBusId(int busid) {
  (void)OSAL_MutexLock(canbusH.q_lock);
  canbusH.busidMask &= ~(1 << busid);
  (void)OSAL_MutexUnlock(canbusH.q_lock);
}

static struct Can_Pdu_s *getPdu(struct Can_Bus_s *b, uint32_t canid) {
  struct Can_PduQueue_s *L = NULL;
  struct Can_Pdu_s *pdu = NULL;
  struct Can_PduQueue_s *l;

  if ((uint32_t)-2 == canid) {
    return getQ(b);
  }

  (void)OSAL_MutexLock(b->q_lock);
  if ((uint32_t)-1 == canid) { /* id is -1, means get the first of queue from b->head2 */
    if (false == STAILQ_EMPTY(&b->head2)) {
      pdu = STAILQ_FIRST(&b->head2);
      /* get the first message canid, and then search CANID queue L */
      canid = pdu->msg.id;
    } else {
      /* no message all is empty */
    }
  }
  /* search queue specified by canid */
  STAILQ_FOREACH(l, &b->head, entry) {
    if ((l->id & (~CAN_ID_EXTENDED)) == (canid & (~CAN_ID_EXTENDED))) {
      L = l;
      break;
    }
  }
  if (L && (false == STAILQ_EMPTY(&L->head))) {
    pdu = STAILQ_FIRST(&L->head);
    /* when remove, should remove from the both queue */
    STAILQ_REMOVE_HEAD(&L->head, entry);
    STAILQ_REMOVE(&b->head2, pdu, Can_Pdu_s, entry2);
    b->size2--;
    L->size--;
  }
  (void)OSAL_MutexUnlock(b->q_lock);
  return pdu;
}

static void saveB(struct Can_Bus_s *b, struct Can_Pdu_s *pdu) {
  struct Can_PduQueue_s *L;
  struct Can_PduQueue_s *l;
  L = NULL;
  (void)OSAL_MutexLock(b->q_lock);
  STAILQ_FOREACH(l, &b->head, entry) {
    if (l->id == pdu->msg.id) {
      L = l;
      break;
    }
  }

  if (NULL == L) {
    L = (struct Can_PduQueue_s *)malloc(sizeof(struct Can_PduQueue_s));
    if (L) {
      L->id = pdu->msg.id;
      L->size = 0;
      L->warning = false;
      STAILQ_INIT(&L->head);
      STAILQ_INSERT_TAIL(&b->head, L, entry);
    } else {
      ASLOG(WARN, ("CAN Bus List malloc failed\n"));
    }
  }

  if (L) {
    /* limit by CANID queue is better than the whole bus one */
    if (L->size < CAN_BUS_PDU_NUM) {
      STAILQ_INSERT_TAIL(&L->head, pdu, entry);
      STAILQ_INSERT_TAIL(&b->head2, pdu, entry2);
      b->size2++;
      L->size++;
      L->warning = false;
    } else {
      if (L->warning == false) {
        ASLOG(WARN, ("CAN Q[id=%X] List is full with size %d\n", L->id, L->size));
        L->warning = true;
      }
      free(pdu);
    }
  }

  (void)OSAL_MutexUnlock(b->q_lock);
}

static void rx_notification(int busid, uint32_t canid, uint8_t dlc, uint8_t *data) {
  if ((busid < CAN_BUS_NUM) && ((uint32_t)-1 != canid)) {
    /* canid -1 reserved for can_read get the first received CAN message on bus */
    struct Can_Bus_s *b = getBus(busid);
    if (NULL != b) {
      struct Can_Pdu_s *pdu = (struct Can_Pdu_s *)malloc(sizeof(struct Can_Pdu_s));
      if (pdu) {
        pdu->msg.id = canid;
        pdu->msg.length = dlc;
        memcpy(&(pdu->msg.sdu), data, dlc);

        saveB(b, pdu);
        saveQ(b, canid, dlc, data);
        logCan(true, busid, canid, dlc, data);
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
static const Can_DeviceOpsType *search_ops(const char *name) {
  const Can_DeviceOpsType *ops, **o;
  o = canOps;
  ops = NULL;
  while (*o != NULL) {
    if (0 == strcmp((*o)->name, name)) {
      ops = *o;
      break;
    }
    o++;
  }

  return ops;
}

static void logCan(boolean isRx, int busid, uint32_t canid, uint8_t dlc, const uint8_t *data) {
  uint32_t i;
  char ts[64];

  STD_TOPIC_CAN(busid, isRx, canid, dlc, data);

  if (NULL != s_Logger) {
    Std_GetDateTime(ts, sizeof(ts));
    OSAL_MutexLock(canbusH.q_lock);
    s_Logger->print("busid=%d %s canid=%08X dlc=%d data=[", busid, isRx ? "rx" : "tx",
                    canid & (~CAN_ID_EXTENDED), dlc);
    if (dlc < 8) {
      dlc = 8;
    }
    for (i = 0; i < dlc; i++) {
      s_Logger->print("%02X,", data[i]);
    }

    s_Logger->print("] [");

    for (i = 0; i < dlc; i++) {
      if (isprint(data[i])) {
        s_Logger->print("%c", data[i]);
      } else {
        s_Logger->print(".");
      }
    }
    s_Logger->print("] @ %s\n", ts);
    s_Logger->check();
    OSAL_MutexUnlock(canbusH.q_lock);
  }
}

class CanLibInitializer {
public:
  CanLibInitializer() {
    char ts[64];
    OSAL_MutexAttrType attr;
    OSAL_MutexAttrInit(&attr);
    OSAL_MutexAttrSetType(&attr, OSAL_MUTEX_RECURSIVE);

    if (false == canbusH.initialized) {
      canbusH.initialized = true;
      canbusH.busidMask = 0;
      STAILQ_INIT(&canbusH.head);
      canbusH.q_lock = OSAL_MutexCreate(&attr);
      char *logName = getenv("CAN_LOG_NAME");
      if (logName != NULL) {
        s_Logger = new Logger(logName);
        if (NULL != s_Logger) {
          Std_GetDateTime(ts, sizeof(ts));
          ASLOG(INFO, ("can trace logger < %s >\n", logName));
          s_Logger->print("can log <%s> %s\n\n", logName, ts);
        } else {
          ASLOG(ERROR, ("canlib: failed to create logger %s\n", logName));
        }
      }
    }
  }

  ~CanLibInitializer() {
    if (canbusH.initialized) {
      struct Can_Bus_s *b;
      STAILQ_FOREACH(b, &canbusH.head, entry) {
        b->device.ops->close(b->device.port);
      }
      freeH(&canbusH);
      canbusH.initialized = false;

      if (NULL != s_Logger) {
        delete s_Logger;
      }
    }
  }
};

static CanLibInitializer g_CanLibInitializer;
/* ================================ [ FUNCTIONS ] ============================================== */
int can_open(const char *device_name, uint32_t port, uint32_t baudrate) {
  int rv;
  int busid = -1;
  const Can_DeviceOpsType *ops;
  OSAL_MutexAttrType attr;

  (void)&g_CanLibInitializer;

  (void)OSAL_MutexLock(canbusH.q_lock);
  ops = search_ops(device_name);
  struct Can_Bus_s *b = getBusByName(device_name, port);
  rv = false;
  if (NULL != b) {
    if (b->device.baudrate == baudrate) {
      b->ref++;
      busid = b->device.busid;
      if (s_Logger) {
        s_Logger->print("reopen %s:%d baudrate=%d, busid %d\n", b->device.device_name,
                        b->device.port, b->device.baudrate, b->device.busid);
      }
    } else {
      ASLOG(ERROR, ("can_open: device <%s:%d> already opened with baudrate %d!, can't reopen with "
                    "new baudrate %d\n",
                    device_name, port, b->device.baudrate, baudrate));
    }
  } else {
    if (NULL != ops) {
      busid = allocBusId();
      if (busid >= 0) {
        b = (struct Can_Bus_s *)malloc(sizeof(struct Can_Bus_s));
        b->device.busid = busid;
        b->device.ops = ops;
        b->device.busid = busid;
        b->device.port = port;
        strncpy(b->device.device_name, device_name, sizeof(b->device.device_name));
        b->device.baudrate = baudrate;
        b->ref = 1;

        rv = ops->probe(busid, port, baudrate, rx_notification);
      }

      if (rv) {
        OSAL_MutexAttrInit(&attr);
        OSAL_MutexAttrSetType(&attr, OSAL_MUTEX_RECURSIVE);
        b->q_lock = OSAL_MutexCreate(&attr);
        STAILQ_INIT(&b->head);
        STAILQ_INIT(&b->head2);
        STAILQ_INIT(&b->headQ);
        b->size2 = 0;
        b->sizeQ = 0;
        STAILQ_INSERT_TAIL(&canbusH.head, b, entry);
        /* result OK */
        if (s_Logger) {
          s_Logger->print("open %s:%d baudrate=%d as busid %d\n", b->device.device_name,
                          b->device.port, b->device.baudrate, b->device.busid);
        }
      } else {
        if (NULL != b) {
          free(b);
        }

        if (busid >= 0) {
          freeBusId(busid);
          busid = -1;
        }
        ASLOG(ERROR, ("can_open device <%s> failed!\n", device_name));
      }
    } else {
      ASLOG(ERROR, ("can_open device <%s> is not known!\n", device_name));
    }
  }
  (void)OSAL_MutexUnlock(canbusH.q_lock);
  return busid;
}

bool can_write(int busid, uint32_t canid, uint8_t dlc, const uint8_t *data) {
  bool rv = false;
  struct Can_Bus_s *b = getBus(busid);
  if (NULL == b) {
    ASLOG(ERROR, ("can bus(%d) is not on-line 'can_write'\n", (int)busid));
  } else if (dlc > 64) {
    ASLOG(ERROR, ("can bus(%d) 'can_write' with invalid dlc(%d>8)\n", (int)busid, (int)dlc));
  } else {
    if (b->device.ops->write) {
      logCan(false, busid, canid, dlc, data);
      rv = b->device.ops->write(b->device.port, canid, dlc, data);
      saveQ(b, canid, dlc, data);
      if (rv) {
        /* result OK */
      } else {
        ASLOG(ERROR, ("can_write bus(%d) failed!\n", (int)busid));
      }
    } else {
      ASLOG(ERROR, ("can bus(%d) is read-only 'can_write'\n", (int)busid));
    }
  }

  return rv;
}

bool can_read(int busid, uint32_t *canid, uint8_t *dlc, uint8_t *data) {
  bool rv = false;
  struct Can_Pdu_s *pdu;
  struct Can_Bus_s *b = getBus(busid);
  uint8_t len = *dlc;

  *dlc = 0;
  if (NULL == b) {
    ASLOG(ERROR, ("bus(%d) is not on-line 'can_read'\n", (int)busid));
  } else if (NULL == canid) {
    ASLOG(ERROR, ("bus(%d) 'can_read' with NULL canid\n", (int)busid));
  } else {
    pdu = getPdu(b, *canid);
    if (NULL == pdu) {
      /* no data */
    } else if ((data == NULL) || (len < pdu->msg.length)) {
      ASLOG(ERROR, ("bus(%d) 'can_read' with invalid args: canid=%x, dlc=%d\n", (int)busid,
                    (int)pdu->msg.id, (int)pdu->msg.length));
    } else {
      *canid = pdu->msg.id;
      *dlc = pdu->msg.length;
      memcpy(data, pdu->msg.sdu, *dlc);
      if (pdu->msg.length < len) {
        memset(&data[pdu->msg.length], 0x55, len - pdu->msg.length);
      }
      free(pdu);
      rv = true;
    }
  }

  return rv;
}

bool can_close(int busid) {
  bool rv;
  struct Can_Bus_s *b = getBus(busid);
  rv = false;
  if (NULL == b) {
    ASLOG(ERROR, ("can bus(%d) is not on-line 'can_close'\n", (int)busid));
  } else {
    (void)OSAL_MutexLock(canbusH.q_lock);
    b->ref--;
    if (0 == b->ref) {
      b->device.ops->close(b->device.port);
      STAILQ_REMOVE(&canbusH.head, b, Can_Bus_s, entry);
      freeB(b);
      OSAL_MutexDestory(b->q_lock);
      free(b);
      freeBusId(busid);
    }
    (void)OSAL_MutexUnlock(canbusH.q_lock);
    rv = true;
  }

  return rv;
}

bool can_reset(int busid) {
  bool rv;
  struct Can_Bus_s *b = getBus(busid);
  rv = false;
  if (NULL == b) {
    ASLOG(ERROR, ("can bus(%d) is not on-line 'can_reset'\n", (int)busid));
  } else {
    if (NULL != b->device.ops->reset) {
      rv = b->device.ops->reset(b->device.port);
    } else {
      rv = true;
    }
  }

  return rv;
}
