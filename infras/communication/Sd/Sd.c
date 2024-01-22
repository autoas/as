/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Service Discovery AUTOSAR CP Release 4.4.0
 * AUTOSAR_PRS_SOMEIPServiceDiscoveryProtocol.pdf
 * SOME/IP Service Discovery Protocol Specification AUTOSAR FO Release 1.3.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Sd.h"
#include "Sd_Priv.h"
#include "Std_Debug.h"
#include "Sd_Cfg.h"
#include "TcpIp.h"
#include "Std_Critical.h"
#include "mempool.h"

#include <stdlib.h>
#include <string.h>
#ifdef USE_PCAP
#include "pcap.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_SD 0
#define AS_LOG_SDI 2
#define AS_LOG_SDE 3

#ifdef USE_SD_CRITICAL
#define sdEnterCritical() EnterCritical()
#define sdExitCritical() ExitCritical();
#else
#define sdEnterCritical()
#define sdExitCritical()
#endif

#define SD_SET(flag, mask)                                                                         \
  do {                                                                                             \
    sdEnterCritical();                                                                             \
    flag |= (mask);                                                                                \
    sdExitCritical();                                                                              \
  } while (0)

#define SD_CLEAR(flag, mask)                                                                       \
  do {                                                                                             \
    sdEnterCritical();                                                                             \
    flag &= ~(mask);                                                                               \
    sdExitCritical();                                                                              \
  } while (0)

#define SD_SET_CLEAR(flag, maskSet, maskClear)                                                     \
  do {                                                                                             \
    sdEnterCritical();                                                                             \
    flag &= ~(maskClear);                                                                          \
    flag |= (maskSet);                                                                             \
    sdExitCritical();                                                                              \
  } while (0)

#define SD_FLG_STATE_REQUEST_ONLINE 0x01u
#define SD_FLG_STATE_REQUEST_ONCE 0x02u

/* For Service Service Offer */
#define SD_FLG_PENDING_OFFER 0x04u
#define SD_FLG_PENDING_STOP_OFFER 0x08u
/* For Client Service Find */
#define SD_FLG_PENDING_FIND 0x04u
#define SD_FLG_PENDING_STOP_FIND 0x08u

/* For Client Service Subscribe */
#define SD_FLG_PENDING_SUBSCRIBE 0x04u
#define SD_FLG_PENDING_STOP_SUBSCRIBE 0x08u

/* For Server Service Subscribe */
#define SD_FLG_PENDING_EVENT_GROUP_ACK 0x04u
#define SD_FLG_EVENT_GROUP_SUBSCRIBED 0x80u
#define SD_FLG_EVENT_GROUP_MULTICAST 0x10u
#define SD_FLG_EVENT_GROUP_UNSUBSCRIBED 0x00u

#define SD_FLG_LINK_UP 0x10u

/* @SWS_SD_00151 */
#define SD_REBOOT_FLAG 0x80u
/* @SWS_SD_00152 */
#define SD_UNICAST_FLAG 0x40u

#define SD_FLAG_MASK 0xC0u

#define SD_CONFIG (sdConfigPtr)

#define SD_FIND_SERVICE 0x00
#define SD_OFFER_SERVICE 0x01

#define SD_OPT_IP4_ENDPOINT 0x04
#define SD_OPT_IP4_MULTICAST 0x14

#define SD_SUBSCRIBE_EVENT_GROUP 0x06
#define SD_SUBSCRIBE_EVENT_GROUP_ACK 0x07
#define SD_SUBSCRIBE_EVENT_GROUP_NACK 0x07

#ifdef USE_PCAP
#define PCAP_TRACE PCap_SD
#else
#define PCAP_TRACE(data, length, RemoteAddr, isRx)
#endif

#ifndef SD_EVENT_HANDLER_SUBSCRIBER_POOL_SIZE
#define SD_EVENT_HANDLER_SUBSCRIBER_POOL_SIZE 32
#endif

/* SQP: SD Queue and Pool */
#define DEF_SQP(T, size)                                                                           \
  static Sd_##T##Type sd##T##Slots[size];                                                          \
  static mempool_t sd##T##Pool;

#define DEC_SQP(T)                                                                                 \
  Sd_##T##Type *var;                                                                               \
  Sd_##T##Type *next

#define SQP_INIT(T)                                                                                \
  do {                                                                                             \
    mp_init(&sd##T##Pool, (uint8_t *)&sd##T##Slots, sizeof(Sd_##T##Type),                          \
            ARRAY_SIZE(sd##T##Slots));                                                             \
  } while (0)

#define SQP_FIRST(T)                                                                               \
  do {                                                                                             \
    var = STAILQ_FIRST(&context->list##T##s);                                                      \
  } while (0)

#define SQP_NEXT()                                                                                 \
  do {                                                                                             \
    EnterCritical();                                                                               \
    next = STAILQ_NEXT(var, entry);                                                                \
    ExitCritical();                                                                                \
  } while (0)

#define SQP_WHILE(T)                                                                               \
  SQP_FIRST(T);                                                                                    \
  while (NULL != var) {                                                                            \
    SQP_NEXT();

#define SQP_WHILE_END()                                                                            \
  var = next;                                                                                      \
  }

/* CRM: context RM */
#define SQP_CRM_AND_FREE(T, var)                                                                   \
  do {                                                                                             \
    EnterCritical();                                                                               \
    STAILQ_REMOVE(&context->list##T##s, var, Sd_##T##_s, entry);                                   \
    ExitCritical();                                                                                \
    memset(var, 0, sizeof(*var));                                                                  \
    mp_free(&sd##T##Pool, (uint8_t *)var);                                                         \
  } while (0)

/* LRM: list RM */
#define SQP_LRM_AND_FREE(T, var)                                                                   \
  do {                                                                                             \
    EnterCritical();                                                                               \
    STAILQ_REMOVE(list##T##s, var, Sd_##T##_s, entry);                                             \
    ExitCritical();                                                                                \
    memset(var, 0, sizeof(*var));                                                                  \
    mp_free(&sd##T##Pool, (uint8_t *)var);                                                         \
  } while (0)

/* Context Append & Prepend */
#define SQP_CAPPEND(T, var)                                                                        \
  do {                                                                                             \
    EnterCritical();                                                                               \
    STAILQ_INSERT_TAIL(&context->list##T##s, var, entry);                                          \
    ExitCritical();                                                                                \
  } while (0)

#define SQP_CPREPEND(T, var)                                                                       \
  do {                                                                                             \
    EnterCritical();                                                                               \
    STAILQ_INSERT_HEAD(&context->list##T##s, var, entry);                                          \
    ExitCritical();                                                                                \
  } while (0)

/* List Append */
#define SQP_LAPPEND(T, var)                                                                        \
  do {                                                                                             \
    EnterCritical();                                                                               \
    STAILQ_INSERT_TAIL(list##T##s, var, entry);                                                    \
    ExitCritical();                                                                                \
  } while (0)

#define SQP_LPREPEND(T, var)                                                                       \
  do {                                                                                             \
    EnterCritical();                                                                               \
    STAILQ_INSERT_HEAD(list##T##s, var, entry);                                                    \
    ExitCritical();                                                                                \
  } while (0)

#define SQP_ALLOC(T) (Sd_##T##Type *)mp_alloc(&sd##T##Pool)

#define SQP_FREE(T, var)                                                                           \
  do {                                                                                             \
    memset(var, 0, sizeof(*var));                                                                  \
    mp_free(&sd##T##Pool, (uint8_t *)var);                                                         \
  } while (0)

#define SQP_CLEAR(T)                                                                               \
  do {                                                                                             \
    Sd_##T##Type *var;                                                                             \
    EnterCritical();                                                                               \
    var = STAILQ_FIRST(&context->list##T##s);                                                      \
    while (NULL != var) {                                                                          \
      STAILQ_REMOVE_HEAD(&context->list##T##s, entry);                                             \
      mp_free(&sd##T##Pool, (uint8_t *)var);                                                       \
      var = STAILQ_FIRST(&context->list##T##s);                                                    \
    }                                                                                              \
    ExitCritical();                                                                                \
  } while (0)
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint32_t length;
  uint32_t lengthOfEntries;
  uint32_t lengthOfOptions;
  uint16_t sessionId;
  uint8_t flags;
} Sd_HeaderType;

typedef struct {
  uint32_t TTL;
  uint32_t minor;
  uint16_t serviceId;
  uint16_t instanceId;
  uint8_t major;
} Sd_EntryType1Type;

typedef struct {
  uint32_t TTL;
  uint16_t serviceId;
  uint16_t instanceId;
  uint8_t major;
  uint8_t counter;
  uint16_t eventGroupId;
} Sd_EntryType2Type;

typedef struct {
  TcpIp_ProtocolType ProtocolType;
  TcpIp_SockAddrType Addr;
} Sd_OptionIPv4Type;
/* ================================ [ DECLARES  ] ============================================== */
extern const Sd_ConfigType Sd_Config;

static void Sd_InitClientServiceConsumedEventGroups(const Sd_ClientServiceType *config,
                                                    boolean soft);

extern Std_ReturnType SomeIp_ResolveSubscriber(uint16_t ServiceId,
                                               Sd_EventHandlerSubscriberType *sub);
/* ================================ [ DATAS     ] ============================================== */
DEF_SQP(EventHandlerSubscriber, SD_EVENT_HANDLER_SUBSCRIBER_POOL_SIZE)

static const Sd_ConfigType *sdConfigPtr = NULL;
/* ================================ [ LOCALS    ] ============================================== */
static uint16_t Sd_RandTime(uint16_t min, uint16_t max) {
  uint16_t ret;
  int range = max - min + 1;

  ret = min + ((uint16_t)rand()) % range;

  return ret;
}

static void Sd_BuildHeader(uint8_t *header, uint8_t flags, uint16_t sessionId,
                           uint32_t lengthOfEntries, uint32_t lengthOfOptions) {
  uint32_t length = 20 + lengthOfEntries + lengthOfOptions;

  header[0] = 0xFF;
  header[1] = 0xFF;
  header[2] = 0x81;
  header[3] = 0x00;

  header[4] = (length >> 24) & 0xFF;
  header[5] = (length >> 16) & 0xFF;
  header[6] = (length >> 8) & 0xFF;
  header[7] = length & 0xFF;

  /* @SWS_SD_00033 clientId = 0x0000 */
  header[8] = 0x00;
  header[9] = 0x00;
  header[10] = (sessionId >> 8) & 0xFF;
  header[11] = sessionId & 0xFF;
  header[12] = 0x01;                 /* Protocol Version */
  header[13] = 0x01;                 /* Interface Version */
  header[14] = 0x02;                 /* Message Type */
  header[15] = 0x00;                 /* Return Code */
  header[16] = flags & SD_FLAG_MASK; /* Flags */
  header[17] = 0x00;                 /* Reserved */
  header[18] = 0x00;
  header[19] = 0x00;

  header[20] = (lengthOfEntries >> 24) & 0xFF;
  header[21] = (lengthOfEntries >> 16) & 0xFF;
  header[22] = (lengthOfEntries >> 8) & 0xFF;
  header[23] = lengthOfEntries & 0xFF;

  header[24 + lengthOfEntries] = (lengthOfOptions >> 24) & 0xFF;
  header[25 + lengthOfEntries] = (lengthOfOptions >> 16) & 0xFF;
  header[26 + lengthOfEntries] = (lengthOfOptions >> 8) & 0xFF;
  header[27 + lengthOfEntries] = lengthOfOptions & 0xFF;
}

static Std_ReturnType Sd_DecodeHeader(const uint8_t *data, uint32_t length, Sd_HeaderType *header) {
  Std_ReturnType ret = E_OK;
  if (length < 28) {
    ASLOG(SDE, ("malformed SD message\n"));
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    if ((0xFF != data[0]) || (0xFF != data[1]) || (0x81 != data[2]) || (0x00 != data[3])) {
      ASLOG(SDE, ("invalid SD message ID\n"));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    if ((0x00 != data[8]) || (0x00 != data[9])) {
      ASLOG(SDE, ("invalid SD client ID\n"));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    if ((0x01 != data[12]) || (0x01 != data[13]) || (0x02 != data[14]) || (0x00 != data[15])) {
      ASLOG(SDE, ("invalid SD version or type\n"));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    if ((0x00 != data[17]) || (0x00 != data[18]) || (0x00 != data[19])) {
      ASLOG(SDE, ("invalid SD reserved\n"));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    header->sessionId = ((uint16_t)data[10] << 8) + data[11];
    if (0 == header->sessionId) {
      ASLOG(SDE, ("invalid session ID\n"));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    header->flags = data[16];
    if (0 != (header->flags & (~SD_FLAG_MASK))) {
      ASLOG(SDE, ("invalid flags\n"));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    header->length =
      ((uint32_t)data[4] << 24) + ((uint32_t)data[5] << 16) + ((uint32_t)data[6] << 8) + data[7];
    if ((header->length + 8) == length) {
      header->lengthOfEntries = ((uint32_t)data[20] << 24) + ((uint32_t)data[21] << 16) +
                                ((uint32_t)data[22] << 8) + data[23];
      if ((28 + header->lengthOfEntries) <= length) {
        header->lengthOfOptions = ((uint32_t)data[24 + header->lengthOfEntries] << 24) +
                                  ((uint32_t)data[25 + header->lengthOfEntries] << 16) +
                                  ((uint32_t)data[26 + header->lengthOfEntries] << 8) +
                                  data[27 + header->lengthOfEntries];
        if ((28 + header->lengthOfEntries + header->lengthOfOptions) != length) {
          ASLOG(SDE, ("invalid SD lengthOfOptions %d\n", header->lengthOfEntries));
          ret = E_NOT_OK;
        }
      } else {
        ASLOG(SDE, ("invalid SD lengthOfEntries %d\n", header->lengthOfEntries));
        ret = E_NOT_OK;
      }
    } else {
      ASLOG(SDE, ("invalid SD length %d\n", header->length));
      ret = E_NOT_OK;
    }
  }

  return ret;
}

static void Sd_BuildEntryType1(uint8_t *entry, uint8_t type, uint8_t indexOf1stOpt,
                               uint8_t indexOf2ndOpt, uint8_t numOf1stOpt, uint8_t numOf2ndOpt,
                               uint16_t serviceId, uint16_t instanceId, uint8_t majorVersion,
                               uint32_t minorVersion, uint32_t TTL) {
  /* @SWS_SD_00159 */
  entry[0] = type;
  entry[1] = indexOf1stOpt;
  entry[2] = indexOf2ndOpt;
  entry[3] = ((numOf1stOpt << 4) & 0xF0u) | (numOf2ndOpt & 0x0Fu);
  entry[4] = (serviceId >> 8) & 0xFF;
  entry[5] = serviceId & 0xFF;
  entry[6] = (instanceId >> 8) & 0xFF;
  entry[7] = instanceId & 0xFF;
  entry[8] = majorVersion;
  /* @SWS_SD_00180, @SWS_SD_00299: 0 for stop offer */
  entry[9] = (TTL >> 16) & 0xFF;
  entry[10] = (TTL >> 8) & 0xFF;
  entry[11] = TTL & 0xFF;
  entry[12] = (minorVersion >> 24) & 0xFF;
  entry[13] = (minorVersion >> 16) & 0xFF;
  entry[14] = (minorVersion >> 8) & 0xFF;
  entry[15] = minorVersion & 0xFF;
}

static void Sd_BuildEntryType2(uint8_t *entry, uint8_t type, uint8_t indexOf1stOpt,
                               uint8_t indexOf2ndOpt, uint8_t numOf1stOpt, uint8_t numOf2ndOpt,
                               uint16_t serviceId, uint16_t instanceId, uint8_t majorVersion,
                               uint8_t counter, uint16_t evnetGroupId, uint32_t TTL) {
  entry[0] = type;
  entry[1] = indexOf1stOpt;
  entry[2] = indexOf2ndOpt;
  entry[3] = ((numOf1stOpt << 4) & 0xF0u) | (numOf2ndOpt & 0x0Fu);
  entry[4] = (serviceId >> 8) & 0xFF;
  entry[5] = serviceId & 0xFF;
  entry[6] = (instanceId >> 8) & 0xFF;
  entry[7] = instanceId & 0xFF;
  entry[8] = majorVersion;
  entry[9] = (TTL >> 16) & 0xFF;
  entry[10] = (TTL >> 8) & 0xFF;
  entry[11] = TTL & 0xFF;
  entry[12] = 0;
  entry[13] = counter & 0x0F;
  entry[14] = (evnetGroupId >> 8) & 0xFF;
  entry[15] = evnetGroupId & 0xFF;
}

static void Sd_BuildOptionIPv4(uint8_t *option, uint8_t type,
                               const TcpIp_SockAddrType *LocalAddrPtr,
                               TcpIp_ProtocolType ProtocolType) {
  option[0] = 0x00; /* length = 9 */
  option[1] = 0x09;
  option[2] = type;                  /* type */
  option[3] = 0x00;                  /* reserved */
  option[4] = LocalAddrPtr->addr[0]; /* IPv4 addr */
  option[5] = LocalAddrPtr->addr[1];
  option[6] = LocalAddrPtr->addr[2];
  option[7] = LocalAddrPtr->addr[3];
  option[8] = 0x00;         /* reserved */
  option[9] = ProtocolType; /* L4 Proto */
  option[10] = (LocalAddrPtr->port >> 8) & 0xFF;
  option[11] = LocalAddrPtr->port & 0xFF;
}

static void Sd_BuildOptionIPv4Endpoint(uint8_t *option, const TcpIp_SockAddrType *LocalAddrPtr,
                                       TcpIp_ProtocolType ProtocolType) {
  Sd_BuildOptionIPv4(option, SD_OPT_IP4_ENDPOINT, LocalAddrPtr, ProtocolType);
}

static void Sd_BuildOptionIPv4Multicast(uint8_t *option, const TcpIp_SockAddrType *LocalAddrPtr,
                                        TcpIp_ProtocolType ProtocolType) {
  Sd_BuildOptionIPv4(option, SD_OPT_IP4_MULTICAST, LocalAddrPtr, ProtocolType);
}

static Std_ReturnType Sd_DecodeIpV4Option(const uint8_t *od, uint8_t type,
                                          Sd_OptionIPv4Type *ipv4Opt, uint8_t indexOf1stOpt,
                                          uint8_t numOf1stOpt) {
  Std_ReturnType ret = E_OK;
  const uint8_t *opt = od;
  uint16_t length;
  boolean optFound = FALSE;
  uint16_t i;

  for (i = 0; i < indexOf1stOpt; i++) { /* moving to the 1stOpt */
    length = ((uint16_t)opt[0] << 8) + opt[1];
    if ((SD_OPT_IP4_ENDPOINT == opt[2]) || (SD_OPT_IP4_MULTICAST == opt[2])) {
      opt += length + 3;
      if (length != 9) {
        ASLOG(SDE, ("Invalid option length for ipv4 endpoint\n"));
        ret = E_NOT_OK;
      }
    } else {
      ret = E_NOT_OK;
      ASLOG(SDE, ("TODO: unsupported option type %d\n", opt[2]));
    }
  }

  for (i = 0; (i < numOf1stOpt) && (E_OK == ret) && (FALSE == optFound); i++) {
    length = ((uint16_t)opt[0] << 8) + opt[1];
    if (type == opt[2]) {
      ipv4Opt->ProtocolType = opt[9];
      if ((TCPIP_IPPROTO_TCP == ipv4Opt->ProtocolType) ||
          (TCPIP_IPPROTO_UDP == ipv4Opt->ProtocolType)) {
        ipv4Opt->Addr.port = ((uint16_t)opt[10] << 8) + opt[11];
        memcpy(ipv4Opt->Addr.addr, &opt[4], 4);
        optFound = TRUE;
      } else {
        ret = E_NOT_OK;
        ASLOG(SDE, ("TODO: unsupported ipv4 endpoint protocol type %d\n", ipv4Opt->ProtocolType));
      }
    } else {
      ret = E_NOT_OK;
      ASLOG(SD, ("TODO: unsupported option type %d\n", opt[2]));
    }
  }

  if (FALSE == optFound) {
    ret = E_NOT_OK;
  }

  return ret;
}

static Std_ReturnType Sd_DecodeEntryType1OF(const uint8_t *ed, const uint8_t *od,
                                            const Sd_HeaderType *header, Sd_EntryType1Type *entry1,
                                            Sd_OptionIPv4Type *ipv4Opt) {
  Std_ReturnType ret = E_OK;

  entry1->serviceId = ((uint16_t)ed[4] << 8) + ed[5];
  entry1->instanceId = ((uint16_t)ed[6] << 8) + ed[7];
  entry1->major = ed[8];
  entry1->TTL = ((uint32_t)ed[9] << 16) + ((uint32_t)ed[10] << 8) + ed[11];
  entry1->minor =
    ((uint32_t)ed[12] << 24) + ((uint32_t)ed[13] << 16) + ((uint32_t)ed[14] << 8) + ed[15];
  if (ipv4Opt != NULL) { /* Offer Service, ipv4 endpoint must be provided */
    ret = Sd_DecodeIpV4Option(od, SD_OPT_IP4_ENDPOINT, ipv4Opt, ed[1], (ed[3] >> 4));
    if (E_OK != ret) {
      ASLOG(SDE, ("ipv4 endpoint option not found for %04x:%04x\n", entry1->serviceId,
                  entry1->instanceId));
    } else {
      ASLOG(SD, ("%sOffer Service %04x:%04x version %d.%d TTL %d s by %s %d.%d.%d.%d:%d\n",
                 (0 == entry1->TTL) ? "Stop " : "", entry1->serviceId, entry1->instanceId,
                 entry1->major, entry1->minor, entry1->TTL,
                 (ipv4Opt->ProtocolType == TCPIP_IPPROTO_TCP) ? "TCP" : "UDP",
                 ipv4Opt->Addr.addr[0], ipv4Opt->Addr.addr[1], ipv4Opt->Addr.addr[2],
                 ipv4Opt->Addr.addr[3], ipv4Opt->Addr.port));
    }
  } else {
    ASLOG(SD, ("Find Service %04x:%04x, version %d.%d\n", entry1->serviceId, entry1->instanceId,
               entry1->major, entry1->minor));
  }

  return ret;
}

static Std_ReturnType Sd_DecodeEntryType2SEG(const uint8_t *ed, const uint8_t *od,
                                             const Sd_HeaderType *header, Sd_EntryType2Type *entry2,
                                             Sd_OptionIPv4Type *ipv4Opt) {
  Std_ReturnType ret = E_OK;

  entry2->serviceId = ((uint16_t)ed[4] << 8) + ed[5];
  entry2->instanceId = ((uint16_t)ed[6] << 8) + ed[7];
  entry2->major = ed[8];
  entry2->TTL = ((uint32_t)ed[9] << 16) + ((uint32_t)ed[10] << 8) + ed[11];
  entry2->counter = ed[13] & 0x0F;
  entry2->eventGroupId = ((uint16_t)ed[14] << 8) + ed[15];
  ret = Sd_DecodeIpV4Option(od, SD_OPT_IP4_ENDPOINT, ipv4Opt, ed[1], (ed[3] >> 4));
  if (E_OK != ret) {
    ASLOG(SDE, ("ipv4 endpoint option not found for %04x:%04x:%04x\n", entry2->serviceId,
                entry2->instanceId, entry2->eventGroupId));
  } else {
    ASLOG(SD, ("%sSubscribe Event Group %04x:%04x:%04x version %d TTL %d s by %s %d.%d.%d.%d:%d\n",
               (0 == entry2->TTL) ? "Stop " : "", entry2->serviceId, entry2->instanceId,
               entry2->eventGroupId, entry2->major, entry2->TTL,
               (ipv4Opt->ProtocolType == TCPIP_IPPROTO_TCP) ? "TCP" : "UDP", ipv4Opt->Addr.addr[0],
               ipv4Opt->Addr.addr[1], ipv4Opt->Addr.addr[2], ipv4Opt->Addr.addr[3],
               ipv4Opt->Addr.port));
  }

  return ret;
}

static Std_ReturnType Sd_DecodeEntryType2SEGAck(const uint8_t *ed, const uint8_t *od,
                                                const Sd_HeaderType *header,
                                                Sd_EntryType2Type *entry2,
                                                Sd_OptionIPv4Type *ipv4Opt) {
  Std_ReturnType ret = E_OK;

  entry2->serviceId = ((uint16_t)ed[4] << 8) + ed[5];
  entry2->instanceId = ((uint16_t)ed[6] << 8) + ed[7];
  entry2->major = ed[8];
  entry2->TTL = ((uint32_t)ed[9] << 16) + ((uint32_t)ed[10] << 8) + ed[11];
  entry2->counter = ed[13] & 0x0F;
  entry2->eventGroupId = ((uint16_t)ed[14] << 8) + ed[15];
  ret = Sd_DecodeIpV4Option(od, SD_OPT_IP4_MULTICAST, ipv4Opt, ed[1], (ed[3] >> 4));
  if (E_OK != ret) {
    ret = E_OK;
    ASLOG(SD, ("Subscribe Event Group ACK %04x:%04x:%04x version %d TTL %d s\n", entry2->serviceId,
               entry2->instanceId, entry2->eventGroupId, entry2->major, entry2->TTL));
    memset(ipv4Opt, 0, sizeof(*ipv4Opt));
  } else {
    ASLOG(SD, ("Subscribe Event Group ACK %04x:%04x:%04x version %d TTL %d s with multicast %s "
               "%d.%d.%d.%d:%d\n",
               entry2->serviceId, entry2->instanceId, entry2->eventGroupId, entry2->major,
               entry2->TTL, (ipv4Opt->ProtocolType == TCPIP_IPPROTO_TCP) ? "TCP" : "UDP",
               ipv4Opt->Addr.addr[0], ipv4Opt->Addr.addr[1], ipv4Opt->Addr.addr[2],
               ipv4Opt->Addr.addr[3], ipv4Opt->Addr.port));
  }

  return ret;
}

Std_ReturnType Sd_Transmit(PduIdType TxPduId, uint8_t *data, uint32_t length,
                           const TcpIp_SockAddrType *RemoteAddr) {
  Std_ReturnType ret;
  PduInfoType pduInfo;

  pduInfo.MetaDataPtr = (uint8_t *)RemoteAddr;
  pduInfo.SduDataPtr = data;
  pduInfo.SduLength = length;
  ret = SoAd_IfTransmit(TxPduId, &pduInfo);
  if (E_OK != ret) {
    ASLOG(SDE, ("Tx Failed\n"));
  } else {
    PCAP_TRACE(data, length, RemoteAddr, FALSE);
  }

  return ret;
}

static Std_ReturnType Sd_HandleFindService(const Sd_InstanceType *Instance,
                                           const TcpIp_SockAddrType *RemoteAddr,
                                           const Sd_HeaderType *header,
                                           const Sd_EntryType1Type *entry1) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i;
  TcpIp_SockAddrType LocalAddr;
  const Sd_ServerServiceType *config;
  Sd_ServerServiceContextType *context;

  for (i = 0; i < Instance->numOfServerServices; i++) {
    config = &Instance->ServerServices[i];
    context = config->context;
    if ((config->ServiceId == entry1->serviceId) || (config->InstanceId == entry1->instanceId)) {
      ret = E_OK;
      break;
    }
  }

  if (E_OK == ret) {
    if ((config->MajorVersion != SD_ANY_MAJOR_VERSION) && (config->MajorVersion != entry1->major)) {
      ASLOG(SDE, ("major version not matched\n"));
      ret = E_NOT_OK;
    } else if ((config->MinorVersion != SD_ANY_MINOR_VERSION) &&
               (config->MinorVersion != entry1->minor)) {
      ASLOG(SDE, ("minor version not matched\n"));
      ret = E_NOT_OK;
    } else {
      /* version okay */
    }
  }

  if (E_OK == ret) {
    if (SD_PHASE_INITIAL_WAIT == context->phase) {
      /* @SWS_SD_00319 */
      ret = E_NOT_OK;
    }
  }

  if ((E_OK == ret) &&
      ((SD_PHASE_DOWN != context->phase) || (context->flags & SD_FLG_STATE_REQUEST_ONLINE))) {
    Sd_BuildEntryType1(&Instance->buffer[24], SD_OFFER_SERVICE, 0, 0, 1, 0, config->ServiceId,
                       config->InstanceId, config->MajorVersion, config->MinorVersion,
                       config->ServerTimer->TTL);
    (void)SoAd_GetLocalAddr(config->SoConId, &LocalAddr, NULL, NULL);
    Sd_BuildOptionIPv4Endpoint(&Instance->buffer[44], &LocalAddr, config->ProtocolType);
    Sd_BuildHeader(Instance->buffer, Instance->context->flags,
                   Instance->context->multicastSessionId, 16, 12);
    if (0 == Instance->context->multicastSessionId) {
      Instance->context->multicastSessionId = 1;
      Instance->context->flags &= ~SD_REBOOT_FLAG;
    }
    ret = Sd_Transmit(Instance->TxPdu.UnicastTxPduId, Instance->buffer, 56, RemoteAddr);
    if (E_OK != ret) {
      ASLOG(SDE, ("response to find service failed\n"));
    }
  }

  return ret;
}

static Std_ReturnType Sd_HandleOfferService(const Sd_InstanceType *Instance,
                                            const TcpIp_SockAddrType *RemoteAddr,
                                            const Sd_HeaderType *header,
                                            const Sd_EntryType1Type *entry1,
                                            const Sd_OptionIPv4Type *ipv4Opt) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i;

  const Sd_ClientServiceType *config;
  Sd_ClientServiceContextType *context;
  for (i = 0; i < Instance->numOfClientServices; i++) {
    config = &Instance->ClientServices[i];
    context = config->context;
    if ((config->ServiceId == entry1->serviceId) || (config->InstanceId == entry1->instanceId)) {
      ret = E_OK;
      break;
    }
  }

  if (E_OK == ret) {
    if ((config->MajorVersion != SD_ANY_MAJOR_VERSION) && (config->MajorVersion != entry1->major)) {
      ASLOG(SDE, ("major version not matched\n"));
      ret = E_NOT_OK;
    } else if ((config->MinorVersion != SD_ANY_MINOR_VERSION) &&
               (config->MinorVersion != entry1->minor)) {
      ASLOG(SDE, ("minor version not matched\n"));
      ret = E_NOT_OK;
    } else {
      /* version okay */
    }
  }

  if (E_OK == ret) {
    if (config->ProtocolType != ipv4Opt->ProtocolType) {
      ASLOG(SDE, ("protocol not matched, expect %X, provide %X\n", config->ProtocolType,
                  ipv4Opt->ProtocolType));
      ret = E_NOT_OK;
    }
  }

#if !defined(_WIN32)
  if (E_OK == ret) {
    if (0 != memcmp(ipv4Opt->Addr.addr, RemoteAddr->addr, sizeof(RemoteAddr->addr))) {
      ASLOG(SDE, ("offer: ipv4 addr not matched\n"));
      ret = E_NOT_OK;
    }
  }
#endif

  if (E_OK == ret) {
    if (context->isOffered) {
      if (0 != memcmp(&context->RemoteAddr, &ipv4Opt->Addr, sizeof(TcpIp_SockAddrType))) {
        ASLOG(SDE, ("Service %x:%x offer by %d.%d.%d.%d:%d again\n", config->ServiceId,
                    config->InstanceId, ipv4Opt->Addr.addr[0], ipv4Opt->Addr.addr[1],
                    ipv4Opt->Addr.addr[2], ipv4Opt->Addr.addr[3], ipv4Opt->Addr.port));
        ret = E_NOT_OK;
      }
    }
  }

  if (E_OK == ret) {
    if ((0 != context->sessionId) && (header->flags & SD_REBOOT_FLAG) &&
        (header->sessionId <= context->sessionId)) {
      ASLOG(SDE, ("%d.%d.%d.%d:%d reboot detected, session ID %d <= %d\n", ipv4Opt->Addr.addr[0],
                  ipv4Opt->Addr.addr[1], ipv4Opt->Addr.addr[2], ipv4Opt->Addr.addr[3],
                  ipv4Opt->Addr.port, header->sessionId, context->sessionId));
      if (SD_FLG_LINK_UP & context->flags) {
        SoAd_CloseSoCon(config->SoConId, TRUE);
        SD_CLEAR(context->flags, SD_FLG_LINK_UP);
      }
      Sd_InitClientServiceConsumedEventGroups(config, TRUE);
    }
    context->sessionId = header->sessionId;
  }
  if (E_OK == ret) {
    context->RemoteAddr = ipv4Opt->Addr;
    context->port = RemoteAddr->port;
    if (0 == entry1->TTL) {
      context->isOffered = FALSE;
      context->TTL = 0;

      Sd_InitClientServiceConsumedEventGroups(config, TRUE);
    } else {
      context->isOffered = TRUE;
      if (DEFAULT_TTL != entry1->TTL) { /* @SWS_SD_00514 */
        context->TTL = SD_CONVERT_MS_TO_MAIN_CYCLES(entry1->TTL * 1000);
      } else {
        context->TTL = 0; /* alive forever */
      }
    }
  }
  return ret;
}

static Std_ReturnType Sd_ResponseSubscribeEventGroup(const Sd_InstanceType *Instance,
                                                     const Sd_ServerServiceType *config,
                                                     const Sd_EventHandlerType *EventHandler,
                                                     Sd_EventHandlerSubscriberType *sub) {
  uint32_t lengthOfOptions = 0;
  Sd_BuildEntryType2(&Instance->buffer[24], SD_SUBSCRIBE_EVENT_GROUP_ACK, 0, 0, 1, 0,
                     config->ServiceId, config->InstanceId, config->MajorVersion, 0,
                     EventHandler->EventGroupId, config->ServerTimer->TTL);
  if (sub->TxPduId == EventHandler->MulticastTxPduId) {
    Sd_BuildOptionIPv4Multicast(&Instance->buffer[44], &EventHandler->MulticastEventAddr,
                                TCPIP_IPPROTO_UDP);
    lengthOfOptions = 12;
  }
  Sd_BuildHeader(Instance->buffer, Instance->context->flags, Instance->context->multicastSessionId,
                 16, lengthOfOptions);
  Instance->context->multicastSessionId++;
  if (0 == Instance->context->multicastSessionId) {
    Instance->context->multicastSessionId = 1;
    Instance->context->flags &= ~SD_REBOOT_FLAG;
  }
#if (defined(_WIN32) || defined(linux)) && !defined(USE_LWIP)
  return Sd_Transmit(Instance->TxPdu.MulticastTxPduId, Instance->buffer, 44 + lengthOfOptions,
                     NULL);
#else
  RemoteAddr = sub->RemoteAddr;
  RemoteAddr.port = sub->port;
  return Sd_Transmit(Instance->TxPdu.UnicastTxPduId, Instance->buffer, 44 + lengthOfOptions,
                     &RemoteAddr);
#endif
}

static uint16_t Sd_NumberOfSubscribes(const Sd_EventHandlerType *EventHandler) {
  uint16_t numberOfSubscribes = 0;
  Sd_EventHandlerContextType *context = EventHandler->context;
  DEC_SQP(EventHandlerSubscriber);
  SQP_WHILE(EventHandlerSubscriber) {
    numberOfSubscribes++;
  }
  SQP_WHILE_END()

  return numberOfSubscribes;
}

static Sd_EventHandlerSubscriberType *Sd_LookupSubscribe(const Sd_EventHandlerType *EventHandler,
                                                         const TcpIp_SockAddrType *RemoteAddr) {
  Sd_EventHandlerSubscriberType *sub = NULL;
  Sd_EventHandlerContextType *context = EventHandler->context;
  DEC_SQP(EventHandlerSubscriber);
  SQP_WHILE(EventHandlerSubscriber) {
    if (0 == memcmp(&var->RemoteAddr, RemoteAddr, sizeof(TcpIp_SockAddrType))) {
      sub = var;
      break;
    }
  }
  SQP_WHILE_END()

  if (NULL == sub) {
    sub = SQP_ALLOC(EventHandlerSubscriber);
    if (NULL != sub) {
      memset(sub, 0, sizeof(Sd_EventHandlerSubscriberType));
    }
  }

  return sub;
}

static Std_ReturnType Sd_HandleSubscribeEventGroup(const Sd_InstanceType *Instance,
                                                   const TcpIp_SockAddrType *RemoteAddr,
                                                   const Sd_EntryType2Type *entry2,
                                                   const Sd_OptionIPv4Type *ipv4Opt) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i;
  const Sd_ServerServiceType *config;
  const Sd_EventHandlerType *EventHandler;
  Sd_EventHandlerSubscriberType *sub = NULL;
  Sd_EventHandlerContextType *context = NULL;
  uint16_t numOfSubscribers;

  for (i = 0; i < Instance->numOfServerServices; i++) {
    config = &Instance->ServerServices[i];
    if ((config->ServiceId == entry2->serviceId) || (config->InstanceId == entry2->instanceId)) {
      ret = E_OK;
      break;
    }
  }

  if (E_OK == ret) {
    ret = E_NOT_OK;
    for (i = 0; i < config->numOfEventHandlers; i++) {
      EventHandler = &config->EventHandlers[i];
      if (EventHandler->EventGroupId == entry2->eventGroupId) {
        context = EventHandler->context;
        ret = E_OK;
        break;
      }
    }
  }

  if (E_OK == ret) {
    if (config->ProtocolType != ipv4Opt->ProtocolType) {
      ASLOG(SDE, ("protocol not matched"));
      ret = E_NOT_OK;
    }
  }
#if !defined(_WIN32)
  if (E_OK == ret) {
    if (0 != memcmp(ipv4Opt->Addr.addr, RemoteAddr->addr, sizeof(RemoteAddr->addr))) {
      ASLOG(SDE, ("sub: ipv4 addr not matched\n"));
      ret = E_NOT_OK;
    }
  }
#endif
  if (E_OK == ret) {
    sub = Sd_LookupSubscribe(EventHandler, &ipv4Opt->Addr);
    if (NULL == sub) {
      ASLOG(SDE, ("no free subscriber\n"));
      ret = E_NOT_OK;
    } else {
      /* OK */
    }
  }

  if (E_OK == ret) {
    if (entry2->TTL > 0) {
      sub->RemoteAddr = ipv4Opt->Addr;
      sub->port = RemoteAddr->port;
      numOfSubscribers = Sd_NumberOfSubscribes(EventHandler);
      /* @ECUC_SD_00097 */
      if ((0 == EventHandler->MulticastThreshold) ||
          ((numOfSubscribers + 1) < EventHandler->MulticastThreshold)) {
        ret = SomeIp_ResolveSubscriber(config->SomeIpServiceId, sub);
        if (E_OK != ret) {
          ASLOG(SDE, ("can't resolve subscriber %d.%d.%d.%d:%d\n", sub->RemoteAddr.addr[0],
                      sub->RemoteAddr.addr[1], sub->RemoteAddr.addr[2], sub->RemoteAddr.addr[3],
                      sub->RemoteAddr.port));
          if (SD_FLG_EVENT_GROUP_UNSUBSCRIBED != sub->flags) {
            EventHandler->onSubscribe(FALSE, &sub->RemoteAddr);
            SQP_CRM_AND_FREE(EventHandlerSubscriber, sub);
          } else {
            SQP_FREE(EventHandlerSubscriber, sub);
          }
        }
      } else {
        sub->TxPduId = EventHandler->MulticastTxPduId;
        if (FALSE == context->isMulticastOpened) {
          (void)SoAd_OpenSoCon(EventHandler->MulticastEventSoConRef);
          context->isMulticastOpened = TRUE;
        }
      }
    }
  }

  if (E_OK == ret) {
    if (entry2->TTL > 0) {
      if (SD_FLG_EVENT_GROUP_UNSUBSCRIBED == sub->flags) {
        sub->flags = SD_FLG_EVENT_GROUP_SUBSCRIBED;
        if (sub->TxPduId == EventHandler->MulticastTxPduId) {
          sub->flags |= SD_FLG_EVENT_GROUP_MULTICAST;
          SQP_CPREPEND(EventHandlerSubscriber, sub);
        } else {
          SQP_CAPPEND(EventHandlerSubscriber, sub);
        }
        EventHandler->onSubscribe(TRUE, &sub->RemoteAddr);
      }
      ret = Sd_ResponseSubscribeEventGroup(Instance, config, EventHandler, sub);
      if (E_OK != ret) { /* retry next time */
        SD_SET(sub->flags, SD_FLG_PENDING_EVENT_GROUP_ACK);
      }
      if (DEFAULT_TTL != entry2->TTL) {
        sub->TTL = SD_CONVERT_MS_TO_MAIN_CYCLES(entry2->TTL * 1000);
      }
      ret = E_OK;
    } else {
      ret = E_NOT_OK;
    }
  }

  if ((E_OK != ret) && (NULL != sub)) {
    if (SD_FLG_EVENT_GROUP_UNSUBSCRIBED != sub->flags) {
      EventHandler->onSubscribe(FALSE, &sub->RemoteAddr);
      SQP_CRM_AND_FREE(EventHandlerSubscriber, sub);
    } else {
      SQP_FREE(EventHandlerSubscriber, sub);
    }
  }

  return ret;
}

static Std_ReturnType Sd_HandleSubscribeEventGroupAck(const Sd_InstanceType *Instance,
                                                      const TcpIp_SockAddrType *RemoteAddr,
                                                      const Sd_EntryType2Type *entry2,
                                                      const Sd_OptionIPv4Type *ipv4Opt) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i;
  const Sd_ClientServiceType *config;
  const Sd_ConsumedEventGroupType *ConsumedEventGroup;

  for (i = 0; i < Instance->numOfClientServices; i++) {
    config = &Instance->ClientServices[i];
    if ((config->ServiceId == entry2->serviceId) || (config->InstanceId == entry2->instanceId)) {
      ret = E_OK;
      break;
    }
  }

  if (E_OK == ret) {
    ret = E_NOT_OK;
    for (i = 0; i < config->numOfConsumedEventGroups; i++) {
      ConsumedEventGroup = &config->ConsumedEventGroups[i];
      if (ConsumedEventGroup->EventGroupId == entry2->eventGroupId) {
        ret = E_OK;
        break;
      }
    }
  }

  if (E_OK == ret) {
    if (entry2->TTL > 0) {
      if (FALSE == ConsumedEventGroup->context->isSubscribed) {
        if ((NULL != ipv4Opt) && (ConsumedEventGroup->MulticastThreshold > 0)) {
          SoAd_SetRemoteAddr(ConsumedEventGroup->MulticastEventSoConRef, &ipv4Opt->Addr);
          SoAd_OpenSoCon(ConsumedEventGroup->MulticastEventSoConRef);
          ASLOG(SDI, ("0x%x:0x%x event group 0x%x multicast on %d.%d.%d.%d:%d\n", entry2->serviceId,
                      entry2->instanceId, entry2->eventGroupId, ipv4Opt->Addr.addr[0],
                      ipv4Opt->Addr.addr[1], ipv4Opt->Addr.addr[2], ipv4Opt->Addr.addr[3],
                      ipv4Opt->Addr.port));
        }
      }
      ConsumedEventGroup->context->isSubscribed = TRUE;
    } else {
      ASLOG(SDE, ("invalid TTL for subscribe group ack\n"));
      ConsumedEventGroup->context->isSubscribed = FALSE;
      if (ConsumedEventGroup->MulticastThreshold > 0) {
        SoAd_CloseSoCon(ConsumedEventGroup->MulticastEventSoConRef, TRUE);
      }
    }
  }

  return ret;
}

static void Sd_HandleMsg(const Sd_InstanceType *Instance, const PduInfoType *PduInfoPtr,
                         boolean isMulticast) {
  const TcpIp_SockAddrType *RemoteAddr = (TcpIp_SockAddrType *)PduInfoPtr->MetaDataPtr;
  uint8_t *data = PduInfoPtr->SduDataPtr;
  PduLengthType length = PduInfoPtr->SduLength;
  Sd_HeaderType header;
  Std_ReturnType ret;
  uint8_t type;
  union {
    Sd_EntryType1Type entry1;
    Sd_EntryType2Type entry2;
  } ET;
  union {
    Sd_OptionIPv4Type ipv4Opt;
  } OPT;
  uint16_t i;

  ASLOG(SD, ("[%s] Rx %s %d bytes from %d.%d.%d.%d:%d\n", Instance->Hostname,
             isMulticast ? "Multicast" : "Unicast", PduInfoPtr->SduLength, RemoteAddr->addr[0],
             RemoteAddr->addr[1], RemoteAddr->addr[2], RemoteAddr->addr[3], RemoteAddr->port));

  ret = Sd_DecodeHeader(data, length, &header);
  for (i = 0; (i < header.lengthOfEntries) && (E_OK == ret);) {
    type = data[24 + i];
    switch (type) {
    case SD_FIND_SERVICE:
      ret = Sd_DecodeEntryType1OF(&data[24 + i], NULL, &header, &ET.entry1, NULL);
      if (E_OK == ret) {
        (void)Sd_HandleFindService(Instance, RemoteAddr, &header, &ET.entry1);
      }
      i += 16;
      break;
    case SD_OFFER_SERVICE:
      ret = Sd_DecodeEntryType1OF(&data[24 + i], &data[28 + header.lengthOfEntries], &header,
                                  &ET.entry1, &OPT.ipv4Opt);
      if (E_OK == ret) {
        (void)Sd_HandleOfferService(Instance, RemoteAddr, &header, &ET.entry1, &OPT.ipv4Opt);
      }
      i += 16;
      break;
    case SD_SUBSCRIBE_EVENT_GROUP:
      ret = Sd_DecodeEntryType2SEG(&data[24 + i], &data[28 + header.lengthOfEntries], &header,
                                   &ET.entry2, &OPT.ipv4Opt);
      if (E_OK == ret) {
        (void)Sd_HandleSubscribeEventGroup(Instance, RemoteAddr, &ET.entry2, &OPT.ipv4Opt);
      }
      i += 16;
      break;
    case SD_SUBSCRIBE_EVENT_GROUP_ACK:
      ret = Sd_DecodeEntryType2SEGAck(&data[24 + i], &data[28 + header.lengthOfEntries], &header,
                                      &ET.entry2, &OPT.ipv4Opt);
      if (E_OK == ret) {
        (void)Sd_HandleSubscribeEventGroupAck(Instance, RemoteAddr, &ET.entry2, &OPT.ipv4Opt);
      }
      i += 16;
      break;
      break;
    default:
      ASLOG(SDE, ("TODO: unsupported SD entry type %d\n", type));
      ret = E_NOT_OK;
      break;
    }
  }
}

static void Sd_InitServerServiceEventHandlers(const Sd_ServerServiceType *config) {
  uint16_t i;
  const Sd_EventHandlerType *EventHandler;
  for (i = 0; i < config->numOfEventHandlers; i++) {
    EventHandler = &config->EventHandlers[i];
    memset(EventHandler->context, 0, sizeof(Sd_EventHandlerContextType));
    SQP_INIT(EventHandlerSubscriber);
    STAILQ_INIT(&EventHandler->context->listEventHandlerSubscribers);
  }
}

static void Sd_ReInitServerServiceEventHandlers(const Sd_ServerServiceType *config) {
  uint16_t i;
  const Sd_EventHandlerType *EventHandler;
  Sd_EventHandlerContextType *context;
  DEC_SQP(EventHandlerSubscriber);

  for (i = 0; i < config->numOfEventHandlers; i++) {
    EventHandler = &config->EventHandlers[i];
    context = EventHandler->context;
    SQP_WHILE(EventHandlerSubscriber) {
      if (SD_FLG_EVENT_GROUP_UNSUBSCRIBED != var->flags) {
        EventHandler->onSubscribe(FALSE, &var->RemoteAddr);
      }
      SQP_CRM_AND_FREE(EventHandlerSubscriber, var);
    }
    SQP_WHILE_END()
    if (context->isMulticastOpened) {
      SoAd_CloseSoCon(EventHandler->MulticastEventSoConRef, TRUE);
    }
    memset(EventHandler->context, 0, sizeof(Sd_EventHandlerContextType));
    STAILQ_INIT(&EventHandler->context->listEventHandlerSubscribers);
  }
}

static void Sd_InitServerService(const Sd_InstanceType *Instance) {
  uint16_t i;
  const Sd_ServerServiceType *config;
  Sd_ServerServiceContextType *context;

  for (i = 0; i < Instance->numOfServerServices; i++) {
    config = &Instance->ServerServices[i];
    context = config->context;
    memset(context, 0, sizeof(*context));
    Sd_InitServerServiceEventHandlers(config);
    if (config->AutoAvailable) {
      SD_SET(context->flags, SD_FLG_STATE_REQUEST_ONLINE);
    }
  }
}

static void Sd_InitClientServiceConsumedEventGroups(const Sd_ClientServiceType *config,
                                                    boolean soft) {
  uint16_t i;
  uint8_t flags;
  const Sd_ConsumedEventGroupType *ConsumedEventGroup;
  Sd_ConsumedEventGroupContextType *context;
  for (i = 0; i < config->numOfConsumedEventGroups; i++) {
    ConsumedEventGroup = &config->ConsumedEventGroups[i];
    context = ConsumedEventGroup->context;
    flags = ConsumedEventGroup->context->flags;
    if (soft && context->isSubscribed && (ConsumedEventGroup->MulticastThreshold > 0)) {
      SoAd_CloseSoCon(ConsumedEventGroup->MulticastEventSoConRef, TRUE);
    }
    memset(context, 0, sizeof(Sd_ConsumedEventGroupContextType));
    if ((ConsumedEventGroup->AutoRequire) || (soft && (flags & SD_FLG_STATE_REQUEST_ONLINE))) {
      SD_SET(context->flags, SD_FLG_STATE_REQUEST_ONLINE);
    }
  }
}

static void Sd_InitClientService(const Sd_InstanceType *Instance) {
  uint16_t i;
  const Sd_ClientServiceType *config;
  Sd_ClientServiceContextType *context;

  for (i = 0; i < Instance->numOfClientServices; i++) {
    config = &Instance->ClientServices[i];
    context = config->context;
    memset(context, 0, sizeof(*context));
    if (config->AutoRequire) {
      context->phase = SD_PHASE_INITIAL_WAIT;
      context->findTimer = Sd_RandTime(config->ClientTimer->InitialFindDelayMin,
                                       config->ClientTimer->InitialFindDelayMax);
      context->isOffered = FALSE;
      context->TTL = 0;
    }
    Sd_InitClientServiceConsumedEventGroups(config, FALSE);
  }
}

static void Sd_ServerServiceLinkControl(const Sd_ServerServiceType *config) {
  Sd_ServerServiceContextType *context = config->context;
  if (context->phase != SD_PHASE_DOWN) {
    if (0 == (SD_FLG_LINK_UP & context->flags)) {
      (void)SoAd_OpenSoCon(config->SoConId);
      SD_SET(context->flags, SD_FLG_LINK_UP);
    }
  } else {
    if (SD_FLG_LINK_UP & context->flags) {
      (void)SoAd_CloseSoCon(config->SoConId, TRUE);
      SD_CLEAR(context->flags, SD_FLG_LINK_UP);
    }
  }
}

static void Sd_ServerServiceMain_Down(const Sd_InstanceType *Instance,
                                      const Sd_ServerServiceType *config) {
  Sd_ServerServiceContextType *context = config->context;

  if (context->flags & SD_FLG_STATE_REQUEST_ONLINE) { /* @SWS_SD_00317 */
    Sd_InitServerServiceEventHandlers(config);
    context->phase = SD_PHASE_INITIAL_WAIT;
    /* @SWS_SD_00318 */
    context->offerTimer = Sd_RandTime(config->ServerTimer->InitialOfferDelayMin,
                                      config->ServerTimer->InitialOfferDelayMax);
    ASLOG(SD, ("Service %X:%X going up\n", config->ServiceId, config->InstanceId));
  }
}

static void Sd_ServerServiceMain_InitialWait(const Sd_InstanceType *Instance,
                                             const Sd_ServerServiceType *config) {
  Sd_ServerServiceContextType *context = config->context;
  if (0 == (context->flags & SD_FLG_STATE_REQUEST_ONLINE)) {
    /* @SWS_SD_00323 */
    context->offerTimer = 0;
    Sd_ReInitServerServiceEventHandlers(config);
    context->phase = SD_PHASE_DOWN;
  } else {
    if (context->offerTimer > 0) {
      context->offerTimer--;
      if (0 == context->offerTimer) {
        /* @SWS_SD_00321 */
        SD_SET(context->flags, SD_FLG_PENDING_OFFER);
        context->counter = 0;
        /* @SWS_SD_00434, @SWS_SD_00435 */
        if (config->ServerTimer->InitialOfferRepetitionsMax > 0) {
          ASLOG(SD, ("Service %X:%X enter repetition\n", config->ServiceId, config->InstanceId));
          context->phase = SD_PHASE_REPETITION;
          context->offerTimer = config->ServerTimer->InitialOfferRepetitionBaseDelay;
        } else {
          ASLOG(SD, ("Service %X:%X enter main\n", config->ServiceId, config->InstanceId));
          context->phase = SD_PHASE_MAIN;
          context->offerTimer = config->ServerTimer->OfferCyclicDelay;
        }
      }
    } else {
      ASLOG(SDE, ("Timer not started in phase INITIAL_WAIT\n"));
    }
  }
}

static void Sd_ServerServiceMain_Repetition(const Sd_InstanceType *Instance,
                                            const Sd_ServerServiceType *config) {
  Sd_ServerServiceContextType *context = config->context;
  if (0 == (context->flags & SD_FLG_STATE_REQUEST_ONLINE)) {
    SD_SET(context->flags, SD_FLG_PENDING_STOP_OFFER);
    Sd_ReInitServerServiceEventHandlers(config);
    context->offerTimer = 0;
    context->phase = SD_PHASE_DOWN;
  } else {
    if (context->offerTimer > 0) {
      context->offerTimer--;
      if (0 == context->offerTimer) {
        SD_SET(context->flags, SD_FLG_PENDING_OFFER);
        context->counter++;
        if (context->counter < config->ServerTimer->InitialOfferRepetitionsMax) {
          context->offerTimer =
            config->ServerTimer->InitialOfferRepetitionBaseDelay * (1 << context->counter);
        } else {
          ASLOG(SD, ("Service %X:%X enter main\n", config->ServiceId, config->InstanceId));
          context->phase = SD_PHASE_MAIN;
          context->offerTimer = config->ServerTimer->OfferCyclicDelay;
        }
      }
    } else {
      ASLOG(SDE, ("Timer not started in phase INITIAL_WAIT\n"));
    }
  }
}

static void Sd_ServerServiceMain_Main(const Sd_InstanceType *Instance,
                                      const Sd_ServerServiceType *config) {
  Sd_ServerServiceContextType *context = config->context;
  if (0 == (context->flags & SD_FLG_STATE_REQUEST_ONLINE)) {
    Sd_ReInitServerServiceEventHandlers(config);
    SD_SET(context->flags, SD_FLG_PENDING_STOP_OFFER);
    context->offerTimer = 0;
    context->phase = SD_PHASE_DOWN;
  } else {
    if (context->offerTimer > 0) {
      context->offerTimer--;
      if (0 == context->offerTimer) {
        SD_SET(context->flags, SD_FLG_PENDING_OFFER);
        if (context->counter < 0xFF) {
          context->counter++;
        }
        context->offerTimer = config->ServerTimer->OfferCyclicDelay;
      }
    } else {
      if ((0 == config->ServerTimer->OfferCyclicDelay) &&
          (DEFAULT_TTL == config->ServerTimer->TTL)) {
        /* @SWS_SD_00741 */
      } else {
        ASLOG(SDE, ("Timer not started in phase INITIAL_WAIT\n"));
      }
    }
  }
}

static void Sd_ClientService_GoToDown(const Sd_ClientServiceType *config) {
  uint16_t i;
  Sd_ClientServiceContextType *context = config->context;
  const Sd_ConsumedEventGroupType *ConsumedEventGroup;

  context->findTimer = 0;
  for (i = 0; i < config->numOfConsumedEventGroups; i++) {
    ConsumedEventGroup = &config->ConsumedEventGroups[i];
    if (ConsumedEventGroup->context->isSubscribed) {
      ConsumedEventGroup->context->isSubscribed = FALSE;
      SD_SET_CLEAR(ConsumedEventGroup->context->flags, SD_FLG_PENDING_STOP_SUBSCRIBE,
                   SD_FLG_PENDING_SUBSCRIBE | SD_FLG_STATE_REQUEST_ONCE);
    }
  }
  context->phase = SD_PHASE_DOWN;
}

static void Sd_ClientService_GoToInitialWait(const Sd_ClientServiceType *config) {
  uint16_t i;
  Sd_ClientServiceContextType *context = config->context;
  const Sd_ConsumedEventGroupType *ConsumedEventGroup;

  context->findTimer =
    Sd_RandTime(config->ClientTimer->InitialFindDelayMin, config->ClientTimer->InitialFindDelayMax);
  for (i = 0; i < config->numOfConsumedEventGroups; i++) {
    ConsumedEventGroup = &config->ConsumedEventGroups[i];
    ConsumedEventGroup->context->isSubscribed = FALSE;
    SD_CLEAR(ConsumedEventGroup->context->flags, SD_FLG_STATE_REQUEST_ONCE);
  }

  context->phase = SD_PHASE_INITIAL_WAIT; /* @SWS_SD_00600 */
}

static void Sd_ClientServiceMain_TTL(const Sd_ClientServiceType *config) {
  Sd_ClientServiceContextType *context = config->context;

  if (context->isOffered && (context->TTL > 0)) {
    context->TTL--;
    if (0 == context->TTL) {
      context->isOffered = FALSE;
      Sd_ClientService_GoToInitialWait(config);
    }
  }
}

static void Sd_ClientServiceMain_Down(const Sd_InstanceType *Instance,
                                      const Sd_ClientServiceType *config) {
  Sd_ClientServiceContextType *context = config->context;
  if (context->flags & SD_FLG_STATE_REQUEST_ONLINE) {
    if (context->isOffered) {
      ASLOG(SD, ("Client %x:%x enter main by offer\n", config->ServiceId, config->InstanceId));
      context->phase = SD_PHASE_MAIN;
      context->findTimer = 0;
    } else {
      ASLOG(SD, ("Client %x:%x going up\n", config->ServiceId, config->InstanceId));
      Sd_ClientService_GoToInitialWait(config);
    }
  }
}

static void Sd_ClientServiceMain_InitialWait(const Sd_InstanceType *Instance,
                                             const Sd_ClientServiceType *config) {
  Sd_ClientServiceContextType *context = config->context;
  if (0 == (context->flags & SD_FLG_STATE_REQUEST_ONLINE)) {
    Sd_ClientService_GoToDown(config);
  } else if (context->isOffered) {
    ASLOG(SD, ("Client %x:%x enter main by offer\n", config->ServiceId, config->InstanceId));
    context->phase = SD_PHASE_MAIN;
    context->findTimer = 0;
  } else {
    if (context->findTimer > 0) {
      context->findTimer--;
      if (0 == context->findTimer) {
        SD_SET(context->flags, SD_FLG_PENDING_FIND);
        if (config->ClientTimer->InitialFindRepetitionsMax > 0) {
          ASLOG(SD, ("Client %x:%x enter repetition\n", config->ServiceId, config->InstanceId));
          context->phase = SD_PHASE_REPETITION;
          context->counter = 0;
          context->findTimer = config->ClientTimer->InitialFindRepetitionsBaseDelay;
        } else {
          ASLOG(SD, ("Client %x:%x enter main\n", config->ServiceId, config->InstanceId));
          context->phase = SD_PHASE_MAIN;
        }
      }
    }
  }
}

static void Sd_ClientServiceMain_Repetition(const Sd_InstanceType *Instance,
                                            const Sd_ClientServiceType *config) {
  Sd_ClientServiceContextType *context = config->context;
  if (0 == (context->flags & SD_FLG_STATE_REQUEST_ONLINE)) {
    Sd_ClientService_GoToDown(config);
  } else if (context->isOffered) {
    ASLOG(SD, ("Client %x:%x enter main by offer\n", config->ServiceId, config->InstanceId));
    context->phase = SD_PHASE_MAIN;
    context->findTimer = 0;
  } else {
    if (context->findTimer > 0) {
      context->findTimer--;
      if (0 == context->findTimer) {
        SD_SET(context->flags, SD_FLG_PENDING_FIND);
        context->counter++;
        if (context->counter < config->ClientTimer->InitialFindRepetitionsMax) {
          context->findTimer =
            config->ClientTimer->InitialFindRepetitionsBaseDelay * (1 << context->counter);
        } else {
          ASLOG(SD, ("Client %x:%x enter main\n", config->ServiceId, config->InstanceId));
          context->phase = SD_PHASE_MAIN;
        }
      }
    }
  }
}

static void Sd_ConsumedEventGroupTTLStart(const Sd_ConsumedEventGroupType *ConsumedEventGroup,
                                          const Sd_ClientServiceType *config) {
  if (DEFAULT_TTL != config->ClientTimer->TTL) {
    ConsumedEventGroup->context->TTL =
      SD_CONVERT_MS_TO_MAIN_CYCLES(config->ClientTimer->TTL * 1000);
    if (ConsumedEventGroup->context->TTL > SD_CONVERT_MS_TO_MAIN_CYCLES(100)) {
      ConsumedEventGroup->context->TTL -= SD_CONVERT_MS_TO_MAIN_CYCLES(100);
    }
    ASLOG(SD, ("start ConsumedEventGroup %x:%x timer = %u\n", ConsumedEventGroup->EventGroupId,
               ConsumedEventGroup->HandleId, ConsumedEventGroup->context->TTL));
  }
}

static void Sd_ClientServiceMain_Main(const Sd_InstanceType *Instance,
                                      const Sd_ClientServiceType *config) {
  Sd_ClientServiceContextType *context = config->context;
  uint16_t i;
  const Sd_ConsumedEventGroupType *ConsumedEventGroup;
  if (0 == (context->flags & SD_FLG_STATE_REQUEST_ONLINE)) {
    Sd_ClientService_GoToDown(config);
  } else if (context->isOffered) {
    for (i = 0; i < config->numOfConsumedEventGroups; i++) {
      ConsumedEventGroup = &config->ConsumedEventGroups[i];
      if (ConsumedEventGroup->context->flags & SD_FLG_STATE_REQUEST_ONLINE) {
        if ((FALSE == ConsumedEventGroup->context->isSubscribed) &&
            (0 == (ConsumedEventGroup->context->flags & SD_FLG_STATE_REQUEST_ONCE))) {
          SD_SET_CLEAR(ConsumedEventGroup->context->flags,
                       SD_FLG_PENDING_SUBSCRIBE | SD_FLG_STATE_REQUEST_ONCE,
                       SD_FLG_PENDING_STOP_SUBSCRIBE);
          Sd_ConsumedEventGroupTTLStart(ConsumedEventGroup, config);
        } else {
          if (ConsumedEventGroup->context->TTL > 0) {
            ConsumedEventGroup->context->TTL--;
            if (0 == ConsumedEventGroup->context->TTL) {
              SD_SET(ConsumedEventGroup->context->flags, SD_FLG_PENDING_SUBSCRIBE);
              Sd_ConsumedEventGroupTTLStart(ConsumedEventGroup, config);
            }
          }
        }
      } else {
        if (ConsumedEventGroup->context->isSubscribed) {
          ConsumedEventGroup->context->isSubscribed = FALSE;
          SD_SET_CLEAR(ConsumedEventGroup->context->flags, SD_FLG_PENDING_STOP_SUBSCRIBE,
                       SD_FLG_PENDING_SUBSCRIBE | SD_FLG_STATE_REQUEST_ONCE);
        }
      }
    }
  } else {
    /* do nothing */
  }
}

static void Sd_ServerServiceOfferCheck(const Sd_InstanceType *Instance, uint32_t *lengthOfEntries,
                                       uint32_t *lengthOfOptions, uint8_t *numOfOptions) {
  uint16_t i;
  const Sd_ServerServiceType *config;
  Sd_ServerServiceContextType *context;

  for (i = 0; i < Instance->numOfServerServices; i++) {
    config = &Instance->ServerServices[i];
    context = config->context;
    if (context->flags & (SD_FLG_PENDING_OFFER | SD_FLG_PENDING_STOP_OFFER)) {
      if (((28 + *lengthOfEntries + *lengthOfOptions) < (Instance->bufLen - 28)) &&
          (*numOfOptions < 255)) {
        /* @SWS_SD_00160 */
        *lengthOfEntries += 16;
        *lengthOfOptions += 12;
        *numOfOptions += 1;
      } else {
        break;
      }
    }
  }
}

static void Sd_ClientServiceFindCheck(const Sd_InstanceType *Instance, uint32_t *lengthOfEntries,
                                      uint32_t lengthOfOptions) {
  uint16_t i;
  const Sd_ClientServiceType *config;
  Sd_ClientServiceContextType *context;

  for (i = 0; i < Instance->numOfClientServices; i++) {
    config = &Instance->ClientServices[i];
    context = config->context;
    if (context->flags & SD_FLG_PENDING_FIND) {
      if ((28 + *lengthOfEntries + lengthOfOptions) < (Instance->bufLen - 16)) {
        *lengthOfEntries += 16;
      } else {
        break;
      }
    }
  }
}

static void Sd_ServerServiceOfferBuild(const Sd_InstanceType *Instance, uint32_t *offsetOfEntries,
                                       uint32_t *offsetOfOptions, uint8_t *numOfOptions,
                                       uint32_t *freeSpace) {
  uint16_t i;
  const Sd_ServerServiceType *config;
  Sd_ServerServiceContextType *context;
  TcpIp_SockAddrType LocalAddr;
  uint32_t TTL;
  Std_ReturnType ret;

  for (i = 0; i < Instance->numOfServerServices; i++) {
    config = &Instance->ServerServices[i];
    context = config->context;
    if (context->flags & (SD_FLG_PENDING_OFFER | SD_FLG_PENDING_STOP_OFFER)) {
      if (*freeSpace >= 28) {
        /* @SWS_SD_00416 */
        ret = SoAd_GetLocalAddr(config->SoConId, &LocalAddr, NULL, NULL);
        if (E_OK == ret) {
          if (context->flags & SD_FLG_PENDING_STOP_OFFER) {
            TTL = 0;
          } else {
            TTL = config->ServerTimer->TTL;
          }
          SD_CLEAR(context->flags, SD_FLG_PENDING_OFFER | SD_FLG_PENDING_STOP_OFFER);
          Sd_BuildEntryType1(&Instance->buffer[*offsetOfEntries], SD_OFFER_SERVICE, *numOfOptions,
                             0, 1, 0, config->ServiceId, config->InstanceId, config->MajorVersion,
                             config->MinorVersion, TTL);
          Sd_BuildOptionIPv4Endpoint(&Instance->buffer[*offsetOfOptions], &LocalAddr,
                                     config->ProtocolType);
          *offsetOfEntries += 16;
          *offsetOfOptions += 12;
          *numOfOptions += 1;
          *freeSpace -= 28;
        }
      } else {
        break;
      }
    }
  }
}

static void Sd_ClientServiceFindBuild(const Sd_InstanceType *Instance, uint32_t *offsetOfEntries,
                                      uint32_t *freeSpace) {
  uint16_t i;
  const Sd_ClientServiceType *config;
  Sd_ClientServiceContextType *context;

  for (i = 0; i < Instance->numOfClientServices; i++) {
    config = &Instance->ClientServices[i];
    context = config->context;
    if (context->flags & SD_FLG_PENDING_FIND) {
      if (*freeSpace >= 16) {
        SD_CLEAR(context->flags, SD_FLG_PENDING_FIND);
        Sd_BuildEntryType1(&Instance->buffer[*offsetOfEntries], SD_FIND_SERVICE, 0, 0, 0, 0,
                           config->ServiceId, config->InstanceId, config->MajorVersion,
                           config->MinorVersion, config->ClientTimer->TTL);
        *offsetOfEntries += 16;
        *freeSpace -= 16;
      } else {
        break;
      }
    }
  }
}

static void Sd_ServerServiceMain_TTL(const Sd_ServerServiceType *config) {
  uint16_t i;
  uint16_t numOfSubscribers;
  const Sd_EventHandlerType *EventHandler;
  Sd_EventHandlerContextType *context;
  DEC_SQP(EventHandlerSubscriber);
  for (i = 0; i < config->numOfEventHandlers; i++) {
    EventHandler = &config->EventHandlers[i];
    context = EventHandler->context;
    SQP_WHILE(EventHandlerSubscriber) {
      if (SD_FLG_EVENT_GROUP_UNSUBSCRIBED != var->flags) {
        if (var->TTL > 0) {
          var->TTL--;
          if (0 == var->TTL) {
            EventHandler->onSubscribe(FALSE, &var->RemoteAddr);
            SQP_CRM_AND_FREE(EventHandlerSubscriber, var);
          }
        }
      }
    }
    SQP_WHILE_END()

    numOfSubscribers = Sd_NumberOfSubscribes(EventHandler);
    if (0 == numOfSubscribers) {
      if (context->isMulticastOpened) {
        context->isMulticastOpened = FALSE;
        (void)SoAd_CloseSoCon(EventHandler->MulticastEventSoConRef, TRUE);
      }
    }
  }
}

static void Sd_ServerServiceMain(const Sd_InstanceType *Instance) {
  uint16_t i;
  const Sd_ServerServiceType *config;
  Sd_ServerServiceContextType *context;
  for (i = 0; i < Instance->numOfServerServices; i++) {
    config = &Instance->ServerServices[i];
    context = config->context;
    Sd_ServerServiceMain_TTL(config);
    Sd_ServerServiceLinkControl(config);
    switch (context->phase) {
    case SD_PHASE_DOWN:
      Sd_ServerServiceMain_Down(Instance, config);
      break;
    case SD_PHASE_INITIAL_WAIT:
      Sd_ServerServiceMain_InitialWait(Instance, config);
      break;
    case SD_PHASE_REPETITION:
      Sd_ServerServiceMain_Repetition(Instance, config);
      break;
    case SD_PHASE_MAIN:
      Sd_ServerServiceMain_Main(Instance, config);
      break;
    default:
      break;
    }
  }
}

static void Sd_ClientServiceLinkControl(const Sd_ClientServiceType *config) {
  Sd_ClientServiceContextType *context = config->context;
  if (context->phase != SD_PHASE_DOWN) {
    if (context->isOffered) {
      if (0 == (SD_FLG_LINK_UP & context->flags)) {
        (void)SoAd_SetRemoteAddr(config->SoConId, &(context->RemoteAddr));
        (void)SoAd_OpenSoCon(config->SoConId);
        SD_SET(context->flags, SD_FLG_LINK_UP);
      }
    } else {
      if (SD_FLG_LINK_UP & context->flags) {
        (void)SoAd_CloseSoCon(config->SoConId, TRUE);
        SD_CLEAR(context->flags, SD_FLG_LINK_UP);
      }
    }
  } else {
    if (SD_FLG_LINK_UP & context->flags) {
      (void)SoAd_CloseSoCon(config->SoConId, TRUE);
      SD_CLEAR(context->flags, SD_FLG_LINK_UP);
    }
  }
}

static void Sd_ClientServiceMain(const Sd_InstanceType *Instance) {
  uint16_t i;
  const Sd_ClientServiceType *config;
  Sd_ClientServiceContextType *context;
  for (i = 0; i < Instance->numOfClientServices; i++) {
    config = &Instance->ClientServices[i];
    context = config->context;
    Sd_ClientServiceMain_TTL(config);
    Sd_ClientServiceLinkControl(config);
    switch (context->phase) {
    case SD_PHASE_DOWN:
      Sd_ClientServiceMain_Down(Instance, config);
      break;
    case SD_PHASE_INITIAL_WAIT:
      Sd_ClientServiceMain_InitialWait(Instance, config);
      break;
    case SD_PHASE_REPETITION:
      Sd_ClientServiceMain_Repetition(Instance, config);
      break;
    case SD_PHASE_MAIN:
      Sd_ClientServiceMain_Main(Instance, config);
      break;
    default:
      break;
    }
  }
}

static boolean Sd_ServerServiceEventGroupAckCheck(const Sd_InstanceType *Instance) {
  uint16_t i, j;
  const Sd_ServerServiceType *config;
  const Sd_EventHandlerType *EventHandler;
  Sd_EventHandlerContextType *context;
  DEC_SQP(EventHandlerSubscriber);
  boolean TxOne = FALSE;
  Std_ReturnType ret = E_OK;

  for (i = 0; (i < Instance->numOfServerServices) && (FALSE == TxOne); i++) {
    config = &Instance->ServerServices[i];
    for (j = 0; j < config->numOfEventHandlers; j++) {
      EventHandler = &config->EventHandlers[j];
      context = EventHandler->context;
      SQP_WHILE(EventHandlerSubscriber) {
        if (var->flags & SD_FLG_PENDING_EVENT_GROUP_ACK) {
          ret = Sd_ResponseSubscribeEventGroup(Instance, config, EventHandler, var);
          if (E_OK == ret) {
            SD_CLEAR(var->flags, SD_FLG_PENDING_EVENT_GROUP_ACK);
            TxOne = TRUE;
            break;
          } else {
            ASLOG(SDE, ("Sending Subscribe Event Group Ack Failed\n"));
          }
        }
      }
      SQP_WHILE_END()
    }
  }

  return TxOne;
}

static Std_ReturnType
Sd_SendSubscribeEventGroup(const Sd_InstanceType *Instance, const Sd_ClientServiceType *config,
                           const Sd_ConsumedEventGroupType *ConsumedEventGroup) {
  Std_ReturnType ret = E_OK;
  TcpIp_SockAddrType LocalAddr;
  uint32_t TTL = 0;

  if (0 == (ConsumedEventGroup->context->flags & SD_FLG_PENDING_STOP_SUBSCRIBE)) {
    TTL = config->ClientTimer->TTL;
  } else {
    /* send stop */
  }

  Sd_BuildEntryType2(&Instance->buffer[24], SD_SUBSCRIBE_EVENT_GROUP, 0, 0, 1, 0, config->ServiceId,
                     config->InstanceId, config->MajorVersion, 0, ConsumedEventGroup->EventGroupId,
                     TTL);
  ret = SoAd_GetLocalAddr(config->SoConId, &LocalAddr, NULL, NULL);
  if (E_OK == ret) {
    Sd_BuildOptionIPv4Endpoint(&Instance->buffer[44], &LocalAddr, config->ProtocolType);
    Sd_BuildHeader(Instance->buffer, Instance->context->flags,
                   Instance->context->multicastSessionId, 16, 12);
    Instance->context->multicastSessionId++;
    if (0 == Instance->context->multicastSessionId) {
      Instance->context->multicastSessionId = 1;
      Instance->context->flags &= ~SD_REBOOT_FLAG;
    }
#if (defined(_WIN32) || defined(linux)) && !defined(USE_LWIP)
    /* NOTE: this is a workaroud for case that server and client on the same host */
    ret = Sd_Transmit(Instance->TxPdu.MulticastTxPduId, Instance->buffer, 56, NULL);
#else
    memcpy(LocalAddr.addr, config->context->RemoteAddr.addr, 4);
    LocalAddr.port = config->context->port;
    ret = Sd_Transmit(Instance->TxPdu.UnicastTxPduId, Instance->buffer, 56, &LocalAddr);
#endif
    if (E_OK != ret) {
      ASLOG(SDE, ("Sending Subscribe Event Group Ack Failed\n"));
    }
  }

  return ret;
}

static boolean Sd_ClientServiceSubscribeEventGroupCheck(const Sd_InstanceType *Instance) {
  uint16_t i, j;
  const Sd_ClientServiceType *config;
  const Sd_ConsumedEventGroupType *ConsumedEventGroup;
  boolean TxOne = FALSE;
  Std_ReturnType ret;

  for (i = 0; (i < Instance->numOfClientServices) && (FALSE == TxOne); i++) {
    config = &Instance->ClientServices[i];
    for (j = 0; (j < config->numOfConsumedEventGroups) && (FALSE == TxOne); j++) {
      ConsumedEventGroup = &config->ConsumedEventGroups[j];
      if (ConsumedEventGroup->context->flags &
          (SD_FLG_PENDING_SUBSCRIBE | SD_FLG_PENDING_STOP_SUBSCRIBE)) {
        ret = Sd_SendSubscribeEventGroup(Instance, config, ConsumedEventGroup);
        if (E_OK == ret) {
          SD_CLEAR(ConsumedEventGroup->context->flags,
                   SD_FLG_PENDING_SUBSCRIBE | SD_FLG_PENDING_STOP_SUBSCRIBE);
          TxOne = TRUE;
        }
      }
    }
  }

  return TxOne;
}

static void Sd_ServerClientServiceMain(const Sd_InstanceType *Instance) {
  uint32_t lengthOfEntries = 0;
  uint32_t lengthOfOptions = 0;
  uint32_t offsetOfEntries = 24;
  uint8_t numOfOptions = 0;
  uint32_t offsetOfOptions;
  uint32_t freeSpace;
  boolean TxOne = FALSE;
  Std_ReturnType ret;

  Sd_ClientServiceFindCheck(Instance, &lengthOfEntries, lengthOfOptions);
  Sd_ServerServiceOfferCheck(Instance, &lengthOfEntries, &lengthOfOptions, &numOfOptions);

  if (lengthOfEntries > 0) {
    offsetOfOptions = offsetOfEntries + lengthOfEntries + 4;
    freeSpace = lengthOfEntries + lengthOfOptions;
    numOfOptions = 0;
    Sd_ClientServiceFindBuild(Instance, &offsetOfEntries, &freeSpace);
    Sd_ServerServiceOfferBuild(Instance, &offsetOfEntries, &offsetOfOptions, &numOfOptions,
                               &freeSpace);
    Sd_BuildHeader(Instance->buffer, Instance->context->flags,
                   Instance->context->multicastSessionId, lengthOfEntries, lengthOfOptions);
    Instance->context->multicastSessionId++;
    if (0 == Instance->context->multicastSessionId) {
      Instance->context->multicastSessionId = 1;
      Instance->context->flags &= ~SD_REBOOT_FLAG;
    }

    ret = Sd_Transmit(Instance->TxPdu.MulticastTxPduId, Instance->buffer,
                      28 + lengthOfEntries + lengthOfOptions, NULL);
    if (E_OK == ret) {
      TxOne = TRUE;
    }
  }
  if (FALSE == TxOne) {
    TxOne = Sd_ServerServiceEventGroupAckCheck(Instance);
  }
  if (FALSE == TxOne) {
    TxOne = Sd_ClientServiceSubscribeEventGroupCheck(Instance);
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Sd_Init(const Sd_ConfigType *ConfigPtr) {
  uint16_t i;
  const Sd_InstanceType *Instance;
  SoAd_SoConIdType SoConId;

  if (NULL != ConfigPtr) {
    sdConfigPtr = ConfigPtr;
  } else {
    sdConfigPtr = &Sd_Config;
  }

  for (i = 0; i < SD_CONFIG->numOfInstances; i++) {
    Instance = &SD_CONFIG->Instances[i];
    Instance->context->flags = SD_REBOOT_FLAG | SD_UNICAST_FLAG;
    Instance->context->multicastSessionId = 0x0001; /* @SWS_SD_00034 */
    (void)SoAd_GetSoConId(Instance->TxPdu.MulticastTxPduId, &SoConId);
    (void)SoAd_OpenSoCon(SoConId);
    (void)SoAd_GetSoConId(Instance->TxPdu.UnicastTxPduId, &SoConId);
    (void)SoAd_OpenSoCon(SoConId);
    Sd_InitServerService(Instance);
    Sd_InitClientService(Instance);
  }
}

Std_ReturnType Sd_ServerSoConModeChg(const Sd_InstanceType *Instance, SoAd_SoConIdType SoConId,
                                     SoAd_SoConModeType Mode) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i, j;
  const Sd_ServerServiceType *config;
  const Sd_EventHandlerType *EventHandler;
  for (i = 0; i < Instance->numOfServerServices; i++) {
    config = &Instance->ServerServices[i];
    for (j = 0; j < config->numOfEventHandlers; j++) {
      EventHandler = &config->EventHandlers[j];
      if ((EventHandler->MulticastThreshold > 0) &&
          (EventHandler->MulticastEventSoConRef == SoConId)) {
        if (SOAD_SOCON_OFFLINE == Mode) {
          if (EventHandler->context->isMulticastOpened) {
            EventHandler->context->isMulticastOpened = FALSE;
            ASLOG(SDE, ("Server Event Group %x multicast offline\n", EventHandler->EventGroupId));
          }
        } else {
          if (FALSE == EventHandler->context->isMulticastOpened) {
            EventHandler->context->isMulticastOpened = TRUE;
            ASLOG(SDE, ("Server Event Group %x multicast online\n", EventHandler->EventGroupId));
          }
        }
        ret = E_OK;
      }
    }
  }

  return ret;
}

Std_ReturnType Sd_ClientSoConModeChg(const Sd_InstanceType *Instance, SoAd_SoConIdType SoConId,
                                     SoAd_SoConModeType Mode) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i, j;
  const Sd_ClientServiceType *config;
  const Sd_ConsumedEventGroupType *ConsumedEventGroup;
  for (i = 0; i < Instance->numOfClientServices; i++) {
    config = &Instance->ClientServices[i];
    for (j = 0; j < config->numOfConsumedEventGroups; j++) {
      ConsumedEventGroup = &config->ConsumedEventGroups[j];
      if ((ConsumedEventGroup->MulticastThreshold > 0) &&
          (ConsumedEventGroup->MulticastEventSoConRef == SoConId)) {
        if (SOAD_SOCON_OFFLINE == Mode) {
          if (ConsumedEventGroup->context->isSubscribed) {
            ASLOG(SDE,
                  ("Client Event Group %x multicast offline\n", ConsumedEventGroup->EventGroupId));
          }
        } else {
          if (FALSE == ConsumedEventGroup->context->isSubscribed) {
            ASLOG(SDE,
                  ("Client Event Group %x multicast online\n", ConsumedEventGroup->EventGroupId));
          }
        }
        ret = E_OK;
      }
    }
  }
  return ret;
}

void Sd_SoConModeChg(SoAd_SoConIdType SoConId, SoAd_SoConModeType Mode) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i;
  const Sd_InstanceType *Instance;
  for (i = 0; i < SD_CONFIG->numOfInstances; i++) {
    Instance = &SD_CONFIG->Instances[i];
    if (SoConId == Instance->MulticastRxPdu.SoConId) {
      if (SOAD_SOCON_OFFLINE == Mode) {
        ASLOG(WARN, ("SD multicast socket offline\n"));
      } else {
        ret = E_OK;
      }
    } else if (SoConId == Instance->UnicastRxPdu.SoConId) {
      if (SOAD_SOCON_OFFLINE == Mode) {
        ASLOG(WARN, ("SD unicast socket offline\n"));
      } else {
        ret = E_OK;
      }
    } else {
      /* multicast event socket */
      ret = Sd_ServerSoConModeChg(Instance, SoConId, Mode);
      if (E_OK != ret) {
        ret = Sd_ClientSoConModeChg(Instance, SoConId, Mode);
      }
    }
  }
  if (E_OK != ret) {
    ASLOG(SDE, ("SoConId %d Mode %d\n", SoConId, Mode));
  }
}

/* This code logic looks weird, just check to ensure things in correct status */
void Sd_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  uint16_t i;
  boolean isMulticast = TRUE;
  const Sd_InstanceType *Instance = NULL;
  PCAP_TRACE(PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength,
             (TcpIp_SockAddrType *)PduInfoPtr->MetaDataPtr, TRUE);
  for (i = 0; i < SD_CONFIG->numOfInstances; i++) {
    if (RxPduId == SD_CONFIG->Instances[i].MulticastRxPdu.RxPduId) {
      Instance = &SD_CONFIG->Instances[i];
      break;
    }
    if (RxPduId == SD_CONFIG->Instances[i].UnicastRxPdu.RxPduId) {
      Instance = &SD_CONFIG->Instances[i];
      isMulticast = FALSE;
      break;
    }
  }

  if (NULL != Instance) {
    Sd_HandleMsg(Instance, PduInfoPtr, isMulticast);
  } else {
    ASLOG(SDE, ("Rx with unknown PDU ID %d\n", RxPduId));
  }
}

void Sd_MainFunction(void) {
  uint16_t i;
  for (i = 0; i < SD_CONFIG->numOfInstances; i++) {
    Sd_ServerServiceMain(&SD_CONFIG->Instances[i]);
    Sd_ClientServiceMain(&SD_CONFIG->Instances[i]);
    Sd_ServerClientServiceMain(&SD_CONFIG->Instances[i]);
  }
}

Std_ReturnType Sd_ServerServiceSetState(uint16_t SdServerServiceHandleId,
                                        Sd_ServerServiceSetStateType ServerServiceState) {
  const Sd_ServerServiceType *config;
  Sd_ServerServiceContextType *context;
  Std_ReturnType ret = E_NOT_OK;
  if (SdServerServiceHandleId < SD_CONFIG->numOfServerServices) {
    config = SD_CONFIG->ServerServicesMap[SdServerServiceHandleId];
    context = config->context;
    if (SD_SERVER_SERVICE_AVAILABLE == ServerServiceState) {
      SD_SET(context->flags, SD_FLG_STATE_REQUEST_ONLINE);
    } else {
      SD_CLEAR(context->flags, SD_FLG_STATE_REQUEST_ONLINE);
    }
    ret = E_OK;
  }

  return ret;
}
Std_ReturnType Sd_ClientServiceSetState(uint16_t ClientServiceHandleId,
                                        Sd_ClientServiceSetStateType ClientServiceState) {
  const Sd_ClientServiceType *config;
  Sd_ClientServiceContextType *context;
  Std_ReturnType ret = E_NOT_OK;
  if (ClientServiceHandleId < SD_CONFIG->numOfClientServices) {
    config = SD_CONFIG->ClientServicesMap[ClientServiceHandleId];
    context = config->context;
    if (SD_CLIENT_SERVICE_REQUESTED == ClientServiceState) {
      SD_SET(context->flags, SD_FLG_STATE_REQUEST_ONLINE);
    } else {
      SD_CLEAR(context->flags, SD_FLG_STATE_REQUEST_ONLINE);
    }
    ret = E_OK;
  }

  return ret;
}

Std_ReturnType
Sd_ConsumedEventGroupSetState(uint16_t SdConsumedEventGroupHandleId,
                              Sd_ConsumedEventGroupSetStateType ConsumedEventGroupState) {
  Std_ReturnType ret = E_OK;
  uint16_t index;
  const Sd_ClientServiceType *config;
  const Sd_ConsumedEventGroupType *ConsumedEventGroup;
  if (SdConsumedEventGroupHandleId < SD_CONFIG->numOfConsumedEventGroups) {
    index = SD_CONFIG->ConsumedEventGroupsMap[SdConsumedEventGroupHandleId];
    config = SD_CONFIG->ClientServicesMap[index];
    index = SD_CONFIG->PerServiceConsumedEventGroupsMap[SdConsumedEventGroupHandleId];
    ConsumedEventGroup = &config->ConsumedEventGroups[index];
    if (SD_CONSUMED_EVENTGROUP_REQUESTED == ConsumedEventGroupState) {
      SD_SET(ConsumedEventGroup->context->flags, SD_FLG_STATE_REQUEST_ONLINE);
    } else {
      SD_CLEAR(ConsumedEventGroup->context->flags, SD_FLG_STATE_REQUEST_ONLINE);
    }
  } else {
    ret = E_NOT_OK;
  }
  return ret;
}

Std_ReturnType Sd_GetSubscribers(uint16_t EventHandlerId,
                                 Sd_EventHandlerSubscriberListType **list) {
  Std_ReturnType ret = E_OK;
  uint16_t index;
  const Sd_ServerServiceType *config;
  const Sd_EventHandlerType *EventHandler;
  Sd_EventHandlerContextType *context;
  if (EventHandlerId < SD_CONFIG->numOfEventHandlers) {
    index = SD_CONFIG->EventHandlersMap[EventHandlerId];
    config = SD_CONFIG->ServerServicesMap[index];
    index = SD_CONFIG->PerServiceEventHandlerMap[EventHandlerId];
    EventHandler = &config->EventHandlers[index];
    context = EventHandler->context;
    if (STAILQ_EMPTY(&context->listEventHandlerSubscribers)) {
      ret = E_NOT_OK;
    } else {
      *list = &context->listEventHandlerSubscribers;
    }
  } else {
    ret = E_NOT_OK;
  }
  return ret;
}

void Sd_RemoveSubscriber(uint16_t EventHandlerId, PduIdType TxPduId) {
  uint16_t index;
  const Sd_ServerServiceType *config;
  const Sd_EventHandlerType *EventHandler;
  Sd_EventHandlerContextType *context;
  DEC_SQP(EventHandlerSubscriber);
  if (EventHandlerId < SD_CONFIG->numOfEventHandlers) {
    index = SD_CONFIG->EventHandlersMap[EventHandlerId];
    config = SD_CONFIG->ServerServicesMap[index];
    index = SD_CONFIG->PerServiceEventHandlerMap[EventHandlerId];
    EventHandler = &config->EventHandlers[index];
    context = EventHandler->context;
    SQP_WHILE(EventHandlerSubscriber) {
      if (var->TxPduId == TxPduId) {
        EventHandler->onSubscribe(FALSE, &var->RemoteAddr);
        SQP_CRM_AND_FREE(EventHandlerSubscriber, var);
      }
    }
    SQP_WHILE_END()
  }
}

Std_ReturnType Sd_GetProviderAddr(uint16_t ClientServiceHandleId, TcpIp_SockAddrType *RemoteAddr) {
  const Sd_ClientServiceType *config;
  Sd_ClientServiceContextType *context;
  Std_ReturnType ret = E_OK;
  if (ClientServiceHandleId < SD_CONFIG->numOfClientServices) {
    config = SD_CONFIG->ClientServicesMap[ClientServiceHandleId];
    context = config->context;
    if ((context->isOffered) && (context->flags & SD_FLG_LINK_UP)) {
      *RemoteAddr = context->RemoteAddr;
    } else {
      ret = E_NOT_OK;
    }
  } else {
    ret = E_NOT_OK;
  }
  return ret;
}
