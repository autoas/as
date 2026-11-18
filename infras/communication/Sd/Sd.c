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

#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_SD 0
#define AS_LOG_SDI 0
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

#define SD_FIND_SERVICE 0x00u
#define SD_OFFER_SERVICE 0x01u

#define SD_OPT_IP4_ENDPOINT 0x04u
#define SD_OPT_IP4_MULTICAST 0x14u

#define SD_SUBSCRIBE_EVENT_GROUP 0x06u
#define SD_SUBSCRIBE_EVENT_GROUP_ACK 0x07u
#define SD_SUBSCRIBE_EVENT_GROUP_NACK 0x07u

#ifdef USE_PCAP
#define PCAP_TRACE PCap_SD
#else
#define PCAP_TRACE(data, length, RemoteAddr, isRx)
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
    var = STAILQ_FIRST(&context->List##T##s);                                                      \
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
    STAILQ_REMOVE(&context->List##T##s, var, Sd_##T##_s, entry);                                   \
    ExitCritical();                                                                                \
    (void)memset(var, 0u, sizeof(*var));                                                           \
    mp_free(&sd##T##Pool, (uint8_t *)var);                                                         \
  } while (0)

/* LRM: list RM */
#define SQP_LRM_AND_FREE(T, var)                                                                   \
  do {                                                                                             \
    EnterCritical();                                                                               \
    STAILQ_REMOVE(List##T##s, var, Sd_##T##_s, entry);                                             \
    ExitCritical();                                                                                \
    (void)memset(var, 0u, sizeof(*var));                                                           \
    mp_free(&sd##T##Pool, (uint8_t *)var);                                                         \
  } while (0)

/* Context Append & Prepend */
#define SQP_CAPPEND(T, var)                                                                        \
  do {                                                                                             \
    EnterCritical();                                                                               \
    STAILQ_INSERT_TAIL(&context->List##T##s, var, entry);                                          \
    ExitCritical();                                                                                \
  } while (0)

#define SQP_CPREPEND(T, var)                                                                       \
  do {                                                                                             \
    EnterCritical();                                                                               \
    STAILQ_INSERT_HEAD(&context->List##T##s, var, entry);                                          \
    ExitCritical();                                                                                \
  } while (0)

/* List Append */
#define SQP_LAPPEND(T, var)                                                                        \
  do {                                                                                             \
    EnterCritical();                                                                               \
    STAILQ_INSERT_TAIL(List##T##s, var, entry);                                                    \
    ExitCritical();                                                                                \
  } while (0)

#define SQP_LPREPEND(T, var)                                                                       \
  do {                                                                                             \
    EnterCritical();                                                                               \
    STAILQ_INSERT_HEAD(List##T##s, var, entry);                                                    \
    ExitCritical();                                                                                \
  } while (0)

#define SQP_ALLOC(T) (Sd_##T##Type *)mp_alloc(&sd##T##Pool)

#define SQP_FREE(T, var)                                                                           \
  do {                                                                                             \
    (void)memset(var, 0u, sizeof(*var));                                                           \
    mp_free(&sd##T##Pool, (uint8_t *)var);                                                         \
  } while (0)

#define SQP_CLEAR(T)                                                                               \
  do {                                                                                             \
    Sd_##T##Type *var;                                                                             \
    EnterCritical();                                                                               \
    var = STAILQ_FIRST(&context->List##T##s);                                                      \
    while (NULL != var) {                                                                          \
      STAILQ_REMOVE_HEAD(&context->List##T##s, entry);                                             \
      mp_free(&sd##T##Pool, (uint8_t *)var);                                                       \
      var = STAILQ_FIRST(&context->List##T##s);                                                    \
    }                                                                                              \
    ExitCritical();                                                                                \
  } while (0)
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint32_t Length;
  uint32_t LengthOfEntries;
  uint32_t LengthOfOptions;
  uint16_t SessionId;
  uint8_t Flags;
} Sd_HeaderType;

typedef struct {
  uint32_t TTL;
  uint32_t Minor;
  uint16_t ServiceId;
  uint16_t InstanceId;
  uint8_t Major;
} Sd_EntryType1Type;

typedef struct {
  uint32_t TTL;
  uint16_t ServiceId;
  uint16_t InstanceId;
  uint8_t Major;
  uint8_t Counter;
  uint16_t EventGroupId;
} Sd_EntryType2Type;

typedef struct {
  TcpIp_ProtocolType ProtocolType;
  TcpIp_SockAddrType Addr;
} Sd_OptionIPv4Type;
/* ================================ [ DECLARES  ] ============================================== */
extern const Sd_ConfigType Sd_Config;

static void Sd_InitClientServiceConsumedEventGroups(const Sd_ClientServiceType *config,
                                                    boolean soft);

extern Std_ReturnType SomeIp_ResolveSubscriber(uint16_t serviceId,
                                               Sd_EventHandlerSubscriberType *sub);
/* ================================ [ DATAS     ] ============================================== */
DEF_SQP(EventHandlerSubscriber, SD_EVENT_HANDLER_SUBSCRIBER_POOL_SIZE)

static const Sd_ConfigType *sdConfigPtr = NULL;
/* ================================ [ LOCALS    ] ============================================== */
static uint16_t Sd_RandTime(uint16_t min, uint16_t max) {
  uint16_t ret;
  uint16_t range = max - min + 1u;

  ret = min + ((uint16_t)rand()) % range;

  return ret;
}

static void Sd_BuildHeader(uint8_t *header, uint8_t flags, uint16_t sessionId,
                           uint32_t lengthOfEntries, uint32_t lengthOfOptions) {
  uint32_t length = 20u + lengthOfEntries + lengthOfOptions;

  header[0] = 0xFFu;
  header[1] = 0xFFu;
  header[2] = 0x81u;
  header[3] = 0x00u;

  header[4] = (length >> 24) & 0xFFu;
  header[5] = (length >> 16) & 0xFFu;
  header[6] = (length >> 8) & 0xFFu;
  header[7] = length & 0xFFu;

  /* @SWS_SD_00033 clientId = 0x0000 */
  header[8] = 0x00u;
  header[9] = 0x00u;
  header[10] = (sessionId >> 8) & 0xFFu;
  header[11] = sessionId & 0xFFu;
  header[12] = 0x01;                 /* Protocol Version */
  header[13] = 0x01;                 /* Interface Version */
  header[14] = 0x02;                 /* Message Type */
  header[15] = 0x00u;                /* Return Code */
  header[16] = flags & SD_FLAG_MASK; /* Flags */
  header[17] = 0x00u;                /* Reserved */
  header[18] = 0x00u;
  header[19] = 0x00u;

  header[20] = (lengthOfEntries >> 24) & 0xFFu;
  header[21] = (lengthOfEntries >> 16) & 0xFFu;
  header[22] = (lengthOfEntries >> 8) & 0xFFu;
  header[23] = lengthOfEntries & 0xFFu;

  header[24u + lengthOfEntries] = (lengthOfOptions >> 24) & 0xFFu;
  header[25u + lengthOfEntries] = (lengthOfOptions >> 16) & 0xFFu;
  header[26u + lengthOfEntries] = (lengthOfOptions >> 8) & 0xFFu;
  header[27u + lengthOfEntries] = lengthOfOptions & 0xFFu;
}

static Std_ReturnType Sd_DecodeHeader(const uint8_t *data, uint32_t length, Sd_HeaderType *header) {
  Std_ReturnType ret = E_OK;
  if (length < 28u) {
    ASLOG(SDE, ("malformed SD message\n"));
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    if ((0xFFu != data[0]) || (0xFFu != data[1]) || (0x81u != data[2]) || (0x00u != data[3])) {
      ASLOG(SDE, ("invalid SD message ID\n"));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    if ((0x00u != data[8]) || (0x00u != data[9])) {
      ASLOG(SDE, ("invalid SD client ID\n"));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    if ((0x01u != data[12]) || (0x01u != data[13]) || (0x02u != data[14]) || (0x00u != data[15])) {
      ASLOG(SDE, ("invalid SD version or type\n"));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    if ((0x00u != data[17]) || (0x00u != data[18]) || (0x00u != data[19])) {
      ASLOG(SDE, ("invalid SD reserved\n"));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    header->SessionId = ((uint16_t)data[10] << 8) + data[11];
    if (0u == header->SessionId) {
      ASLOG(SDE, ("invalid session ID\n"));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    header->Flags = data[16];
    if (0x0u != (header->Flags & (~SD_FLAG_MASK))) {
      ASLOG(SDE, ("invalid flags\n"));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    header->Length =
      ((uint32_t)data[4] << 24) + ((uint32_t)data[5] << 16) + ((uint32_t)data[6] << 8) + data[7];
    if ((header->Length + 8u) == length) {
      header->LengthOfEntries = ((uint32_t)data[20] << 24) + ((uint32_t)data[21] << 16) +
                                ((uint32_t)data[22] << 8) + data[23];
      if ((28u + header->LengthOfEntries) <= length) {
        header->LengthOfOptions = ((uint32_t)data[24u + header->LengthOfEntries] << 24) +
                                  ((uint32_t)data[25u + header->LengthOfEntries] << 16) +
                                  ((uint32_t)data[26u + header->LengthOfEntries] << 8) +
                                  data[27u + header->LengthOfEntries];
        if ((28u + header->LengthOfEntries + header->LengthOfOptions) != length) {
          ASLOG(SDE, ("invalid SD lengthOfOptions %d\n", header->LengthOfEntries));
          ret = E_NOT_OK;
        }
      } else {
        ASLOG(SDE, ("invalid SD lengthOfEntries %d\n", header->LengthOfEntries));
        ret = E_NOT_OK;
      }
    } else {
      ASLOG(SDE, ("invalid SD length %d\n", header->Length));
      ret = E_NOT_OK;
    }
  }

  return ret;
}

static void Sd_BuildEntryType1(uint8_t *entry, uint8_t type, uint8_t indexOf1stOpt,
                               uint8_t indexOf2ndOpt, uint8_t numOf1stOpt, uint8_t numOf2ndOpt,
                               uint16_t serviceId, uint16_t instanceId, uint8_t majorVersion,
                               uint32_t minorVersion, uint32_t tTL) {
  /* @SWS_SD_00159 */
  entry[0] = type;
  entry[1] = indexOf1stOpt;
  entry[2] = indexOf2ndOpt;
  entry[3] = ((numOf1stOpt << 4) & 0xF0u) | (numOf2ndOpt & 0x0Fu);
  entry[4] = (serviceId >> 8) & 0xFFu;
  entry[5] = serviceId & 0xFFu;
  entry[6] = (instanceId >> 8) & 0xFFu;
  entry[7] = instanceId & 0xFFu;
  entry[8] = majorVersion;
  /* @SWS_SD_00180u, @SWS_SD_00299: 0 for stop offer */
  entry[9] = (tTL >> 16) & 0xFFu;
  entry[10] = (tTL >> 8) & 0xFFu;
  entry[11] = tTL & 0xFFu;
  entry[12] = (minorVersion >> 24) & 0xFFu;
  entry[13] = (minorVersion >> 16) & 0xFFu;
  entry[14] = (minorVersion >> 8) & 0xFFu;
  entry[15] = minorVersion & 0xFFu;
}

static void Sd_BuildEntryType2(uint8_t *entry, uint8_t type, uint8_t indexOf1stOpt,
                               uint8_t indexOf2ndOpt, uint8_t numOf1stOpt, uint8_t numOf2ndOpt,
                               uint16_t serviceId, uint16_t instanceId, uint8_t majorVersion,
                               uint8_t counter, uint16_t evnetGroupId, uint32_t tTL) {
  entry[0] = type;
  entry[1] = indexOf1stOpt;
  entry[2] = indexOf2ndOpt;
  entry[3] = ((numOf1stOpt << 4) & 0xF0u) | (numOf2ndOpt & 0x0Fu);
  entry[4] = (serviceId >> 8) & 0xFFu;
  entry[5] = serviceId & 0xFFu;
  entry[6] = (instanceId >> 8) & 0xFFu;
  entry[7] = instanceId & 0xFFu;
  entry[8] = majorVersion;
  entry[9] = (tTL >> 16) & 0xFFu;
  entry[10] = (tTL >> 8) & 0xFFu;
  entry[11] = tTL & 0xFFu;
  entry[12] = 0u;
  entry[13] = counter & 0x0Fu;
  entry[14] = (evnetGroupId >> 8) & 0xFFu;
  entry[15] = evnetGroupId & 0xFFu;
}

static void Sd_BuildOptionIPv4(uint8_t *option, uint8_t type,
                               const TcpIp_SockAddrType *localAddrPtr,
                               TcpIp_ProtocolType protocolType) {
  option[0] = 0x00u; /* length = 9 */
  option[1] = 0x09;
  option[2] = type;                  /* type */
  option[3] = 0x00u;                 /* reserved */
  option[4] = localAddrPtr->Addr[0]; /* IPv4 addr */
  option[5] = localAddrPtr->Addr[1];
  option[6] = localAddrPtr->Addr[2];
  option[7] = localAddrPtr->Addr[3];
  option[8] = 0x00u;        /* reserved */
  option[9] = protocolType; /* L4 Proto */
  option[10] = (localAddrPtr->Port >> 8) & 0xFFu;
  option[11] = localAddrPtr->Port & 0xFFu;
}

static void Sd_BuildOptionIPv4Endpoint(uint8_t *option, const TcpIp_SockAddrType *localAddrPtr,
                                       TcpIp_ProtocolType protocolType) {
  Sd_BuildOptionIPv4(option, SD_OPT_IP4_ENDPOINT, localAddrPtr, protocolType);
}

static void Sd_BuildOptionIPv4Multicast(uint8_t *option, const TcpIp_SockAddrType *localAddrPtr,
                                        TcpIp_ProtocolType protocolType) {
  Sd_BuildOptionIPv4(option, SD_OPT_IP4_MULTICAST, localAddrPtr, protocolType);
}

static Std_ReturnType Sd_DecodeIpV4Option(const uint8_t *od, uint8_t type,
                                          Sd_OptionIPv4Type *ipv4Opt, uint8_t indexOf1stOpt,
                                          uint8_t numOf1stOpt) {
  Std_ReturnType ret = E_OK;
  const uint8_t *opt = od;
  uint16_t length;
  boolean optFound = FALSE;
  uint16_t i;

  for (i = 0u; i < indexOf1stOpt; i++) { /* moving to the 1stOpt */
    length = ((uint16_t)opt[0] << 8) + opt[1];
    if ((SD_OPT_IP4_ENDPOINT == opt[2]) || (SD_OPT_IP4_MULTICAST == opt[2])) {
      opt = &opt[length + 3u];
      if (length != 9u) {
        ASLOG(SDE, ("Invalid option length for ipv4 endpoint\n"));
        ret = E_NOT_OK;
      }
    } else {
      ret = E_NOT_OK;
      ASLOG(SDE, ("TODO: unsupported option type %d\n", opt[2]));
    }
  }

  for (i = 0u; (i < numOf1stOpt) && (E_OK == ret) && (FALSE == optFound); i++) {
    length = ((uint16_t)opt[0] << 8) + opt[1];
    if (type == opt[2]) {
      ipv4Opt->ProtocolType = opt[9];
      if ((TCPIP_IPPROTO_TCP == ipv4Opt->ProtocolType) ||
          (TCPIP_IPPROTO_UDP == ipv4Opt->ProtocolType)) {
        ipv4Opt->Addr.Port = ((uint16_t)opt[10] << 8) + opt[11];
        (void)memcpy(ipv4Opt->Addr.Addr, &opt[4], 4);
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
                                            Sd_EntryType1Type *entry1, Sd_OptionIPv4Type *ipv4Opt) {
  Std_ReturnType ret = E_OK;

  entry1->ServiceId = ((uint16_t)ed[4] << 8) + ed[5];
  entry1->InstanceId = ((uint16_t)ed[6] << 8) + ed[7];
  entry1->Major = ed[8];
  entry1->TTL = ((uint32_t)ed[9] << 16) + ((uint32_t)ed[10] << 8) + ed[11];
  entry1->Minor =
    ((uint32_t)ed[12] << 24) + ((uint32_t)ed[13] << 16) + ((uint32_t)ed[14] << 8) + ed[15];
  if (ipv4Opt != NULL) { /* Offer Service, ipv4 endpoint must be provided */
    ret = Sd_DecodeIpV4Option(od, SD_OPT_IP4_ENDPOINT, ipv4Opt, ed[1], (ed[3] >> 4));
    if (E_OK != ret) {
      ASLOG(SDE, ("ipv4 endpoint option not found for %04x:%04x\n", entry1->ServiceId,
                  entry1->InstanceId));
    } else {
      ASLOG(SD, ("%sOffer Service %04x:%04x version %d.%d TTL %d s by %s %d.%d.%d.%d:%d\n",
                 (0u == entry1->TTL) ? "Stop " : "", entry1->ServiceId, entry1->InstanceId,
                 entry1->Major, entry1->Minor, entry1->TTL,
                 (ipv4Opt->ProtocolType == TCPIP_IPPROTO_TCP) ? "TCP" : "UDP",
                 ipv4Opt->Addr.Addr[0], ipv4Opt->Addr.Addr[1], ipv4Opt->Addr.Addr[2],
                 ipv4Opt->Addr.Addr[3], ipv4Opt->Addr.Port));
    }
  } else {
    ASLOG(SD, ("Find Service %04x:%04x, version %d.%d\n", entry1->ServiceId, entry1->InstanceId,
               entry1->Major, entry1->Minor));
  }

  return ret;
}

static Std_ReturnType Sd_DecodeEntryType2SEG(const uint8_t *ed, const uint8_t *od,
                                             Sd_EntryType2Type *entry2,
                                             Sd_OptionIPv4Type *ipv4Opt) {
  Std_ReturnType ret = E_OK;

  entry2->ServiceId = ((uint16_t)ed[4] << 8) + ed[5];
  entry2->InstanceId = ((uint16_t)ed[6] << 8) + ed[7];
  entry2->Major = ed[8];
  entry2->TTL = ((uint32_t)ed[9] << 16) + ((uint32_t)ed[10] << 8) + ed[11];
  entry2->Counter = ed[13] & 0x0Fu;
  entry2->EventGroupId = ((uint16_t)ed[14] << 8) + ed[15];
  ret = Sd_DecodeIpV4Option(od, SD_OPT_IP4_ENDPOINT, ipv4Opt, ed[1], (ed[3] >> 4));
  if (E_OK != ret) {
    ASLOG(SDE, ("ipv4 endpoint option not found for %04x:%04x:%04x\n", entry2->ServiceId,
                entry2->InstanceId, entry2->EventGroupId));
  } else {
    ASLOG(SD, ("%sSubscribe Event Group %04x:%04x:%04x version %d TTL %d s by %s %d.%d.%d.%d:%d\n",
               (0u == entry2->TTL) ? "Stop " : "", entry2->ServiceId, entry2->InstanceId,
               entry2->EventGroupId, entry2->Major, entry2->TTL,
               (ipv4Opt->ProtocolType == TCPIP_IPPROTO_TCP) ? "TCP" : "UDP", ipv4Opt->Addr.Addr[0],
               ipv4Opt->Addr.Addr[1], ipv4Opt->Addr.Addr[2], ipv4Opt->Addr.Addr[3],
               ipv4Opt->Addr.Port));
  }

  return ret;
}

static Std_ReturnType Sd_DecodeEntryType2SEGAck(const uint8_t *ed, const uint8_t *od,
                                                Sd_EntryType2Type *entry2,
                                                Sd_OptionIPv4Type *ipv4Opt) {
  Std_ReturnType ret = E_OK;

  entry2->ServiceId = ((uint16_t)ed[4] << 8) + ed[5];
  entry2->InstanceId = ((uint16_t)ed[6] << 8) + ed[7];
  entry2->Major = ed[8];
  entry2->TTL = ((uint32_t)ed[9] << 16) + ((uint32_t)ed[10] << 8) + ed[11];
  entry2->Counter = ed[13] & 0x0Fu;
  entry2->EventGroupId = ((uint16_t)ed[14] << 8) + ed[15];
  ret = Sd_DecodeIpV4Option(od, SD_OPT_IP4_MULTICAST, ipv4Opt, ed[1], (ed[3] >> 4));
  if (E_OK != ret) {
    ret = E_OK;
    ASLOG(SD, ("Subscribe Event Group ACK %04x:%04x:%04x version %d TTL %d s\n", entry2->ServiceId,
               entry2->InstanceId, entry2->EventGroupId, entry2->Major, entry2->TTL));
    (void)memset(ipv4Opt, 0u, sizeof(*ipv4Opt));
  } else {
    ASLOG(SD, ("Subscribe Event Group ACK %04x:%04x:%04x version %d TTL %d s with multicast %s "
               "%d.%d.%d.%d:%d\n",
               entry2->ServiceId, entry2->InstanceId, entry2->EventGroupId, entry2->Major,
               entry2->TTL, (ipv4Opt->ProtocolType == TCPIP_IPPROTO_TCP) ? "TCP" : "UDP",
               ipv4Opt->Addr.Addr[0], ipv4Opt->Addr.Addr[1], ipv4Opt->Addr.Addr[2],
               ipv4Opt->Addr.Addr[3], ipv4Opt->Addr.Port));
  }

  return ret;
}

static Std_ReturnType Sd_Transmit(PduIdType txPduId, uint8_t *data, uint32_t length,
                                  const TcpIp_SockAddrType *remoteAddr) {
  Std_ReturnType ret;
  PduInfoType pduInfo;

  pduInfo.MetaDataPtr = (uint8_t *)remoteAddr;
  pduInfo.SduDataPtr = data;
  pduInfo.SduLength = length;
  ret = SoAd_IfTransmit(txPduId, &pduInfo);
  if (E_OK != ret) {
    ASLOG(SDE, ("Tx Failed\n"));
  } else {
    PCAP_TRACE(data, length, remoteAddr, FALSE);
  }

  return ret;
}

static Std_ReturnType Sd_HandleFindService(const Sd_InstanceType *instance,
                                           const TcpIp_SockAddrType *remoteAddr,
                                           const Sd_EntryType1Type *entry1) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i;
  TcpIp_SockAddrType LocalAddr;
  const Sd_ServerServiceType *config;
  Sd_ServerServiceContextType *context;

  for (i = 0u; i < instance->NumOfServerServices; i++) {
    config = &instance->ServerServices[i];
    context = config->Context;
    if ((config->ServiceId == entry1->ServiceId) || (config->InstanceId == entry1->InstanceId)) {
      ret = E_OK;
      break;
    }
  }

  if (E_OK == ret) {
    if ((config->MajorVersion != SD_ANY_MAJOR_VERSION) && (entry1->Major != SD_ANY_MAJOR_VERSION) &&
        (config->MajorVersion != entry1->Major)) {
      ASLOG(SDE, ("major version not matched: %u != %u\n", config->MajorVersion, entry1->Major));
      ret = E_NOT_OK;
    } else if ((config->MinorVersion != SD_ANY_MINOR_VERSION) &&
               (entry1->Minor != SD_ANY_MINOR_VERSION) && (config->MinorVersion != entry1->Minor)) {
      ASLOG(SDE, ("minor version not matched: %du != %u\n", config->MinorVersion, entry1->Minor));
      ret = E_NOT_OK;
    } else {
      /* version okay */
    }
  }

  if (E_OK == ret) {
    if (SD_PHASE_INITIAL_WAIT == context->Phase) {
      /* @SWS_SD_00319 */
      ret = E_NOT_OK;
    }
  }

  if ((E_OK == ret) &&
      ((SD_PHASE_DOWN != context->Phase) || (context->Flags & SD_FLG_STATE_REQUEST_ONLINE))) {
    Sd_BuildEntryType1(&instance->Buffer[24u], SD_OFFER_SERVICE, 0u, 0u, 1u, 0u, config->ServiceId,
                       config->InstanceId, config->MajorVersion, config->MinorVersion,
                       config->ServerTimer->TTL);
    (void)SoAd_GetLocalAddr(config->SoConId, &LocalAddr, NULL, NULL);
    Sd_BuildOptionIPv4Endpoint(&instance->Buffer[44u], &LocalAddr, config->ProtocolType);
    Sd_BuildHeader(instance->Buffer, instance->Context->Flags,
                   instance->Context->MulticastSessionId, 16u, 12u);
    instance->Context->MulticastSessionId++;
    if (0u == instance->Context->MulticastSessionId) {
      instance->Context->MulticastSessionId = 1u;
      instance->Context->Flags &= ~SD_REBOOT_FLAG;
    }
    ret = Sd_Transmit(instance->TxPdu.UnicastTxPduId, instance->Buffer, 56u, remoteAddr);
    if (E_OK != ret) {
      ASLOG(SDE, ("response to find service failed\n"));
    }
  }

  return ret;
}

static Std_ReturnType Sd_HandleOfferService(const Sd_InstanceType *instance,
                                            const TcpIp_SockAddrType *remoteAddr,
                                            Sd_HeaderType *header, const Sd_EntryType1Type *entry1,
                                            const Sd_OptionIPv4Type *ipv4Opt) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i;
  int result;

  const Sd_ClientServiceType *config;
  Sd_ClientServiceContextType *context;
  for (i = 0u; i < instance->NumOfClientServices; i++) {
    config = &instance->ClientServices[i];
    context = config->Context;
    if ((config->ServiceId == entry1->ServiceId) || (config->InstanceId == entry1->InstanceId)) {
      ret = E_OK;
      break;
    }
  }

  if (E_OK == ret) {
    if ((config->MajorVersion != SD_ANY_MAJOR_VERSION) && (config->MajorVersion != entry1->Major)) {
      ASLOG(SDE, ("major version not matched\n"));
      ret = E_NOT_OK;
    } else if ((config->MinorVersion != SD_ANY_MINOR_VERSION) &&
               (config->MinorVersion != entry1->Minor)) {
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
    result = memcmp(ipv4Opt->Addr.Addr, remoteAddr->Addr, sizeof(remoteAddr->Addr));
    if (0 != result) {
      ASLOG(SDE, ("offer: ipv4 addr not matched\n"));
      ret = E_NOT_OK;
    }
  }
#endif

  if (E_OK == ret) {
    if (TRUE == context->IsOffered) {
      result = memcmp(&context->RemoteAddr, &ipv4Opt->Addr, sizeof(TcpIp_SockAddrType));
      if (0 != result) {
        ASLOG(SDE, ("Service %x:%x offer by %d.%d.%d.%d:%d again\n", config->ServiceId,
                    config->InstanceId, ipv4Opt->Addr.Addr[0], ipv4Opt->Addr.Addr[1],
                    ipv4Opt->Addr.Addr[2], ipv4Opt->Addr.Addr[3], ipv4Opt->Addr.Port));
        ret = E_NOT_OK;
      }
    }
  }

  if (E_OK == ret) {
    if ((0x0u != context->SessionId) && (header->Flags & SD_REBOOT_FLAG) &&
        (header->SessionId <= context->SessionId)) {
      ASLOG(SDE, ("%d.%d.%d.%d:%d reboot detected, session ID %d <= %d\n", ipv4Opt->Addr.Addr[0],
                  ipv4Opt->Addr.Addr[1], ipv4Opt->Addr.Addr[2], ipv4Opt->Addr.Addr[3],
                  ipv4Opt->Addr.Port, header->SessionId, context->SessionId));
      if (0u != (SD_FLG_LINK_UP & context->Flags)) {
        (void)SoAd_CloseSoCon(config->SoConId, TRUE);
        SD_CLEAR(context->Flags, SD_FLG_LINK_UP);
        ASLOG(SDI, ("SOCK[%u] client service %x:-:%x link down\n", config->SoConId,
                    config->ServiceId, config->InstanceId));
      }
      Sd_InitClientServiceConsumedEventGroups(config, TRUE);
    }
    context->SessionId = header->SessionId;
  }
  if (E_OK == ret) {
    context->RemoteAddr = ipv4Opt->Addr;
    context->Port = remoteAddr->Port;
    if (0u == entry1->TTL) {
      context->IsOffered = FALSE;
      context->TTL = 0u;

      Sd_InitClientServiceConsumedEventGroups(config, TRUE);
    } else {
      context->IsOffered = TRUE;
      if (DEFAULT_TTL != entry1->TTL) { /* @SWS_SD_00514 */
        context->TTL = SD_CONVERT_MS_TO_MAIN_CYCLES(entry1->TTL * 1000u);
      } else {
        context->TTL = 0u; /* alive forever */
      }
    }
  }
  return ret;
}

static Std_ReturnType Sd_ResponseSubscribeEventGroup(const Sd_InstanceType *instance,
                                                     const Sd_ServerServiceType *config,
                                                     const Sd_EventHandlerType *eventHandler,
                                                     Sd_EventHandlerSubscriberType *sub) {
#if (defined(_WIN32) || defined(linux)) && !defined(USE_LWIP)
#else
  TcpIp_SockAddrType RemoteAddr;
#endif
  uint32_t lengthOfOptions = 0u;
  uint8_t numOf1stOpt = 0u;
  if (sub->TxPduId == eventHandler->MulticastTxPduId) {
    Sd_BuildOptionIPv4Multicast(&instance->Buffer[44], &eventHandler->MulticastEventAddr,
                                TCPIP_IPPROTO_UDP);
    lengthOfOptions = 12u;
    numOf1stOpt = 1u;
  }
  Sd_BuildEntryType2(&instance->Buffer[24], SD_SUBSCRIBE_EVENT_GROUP_ACK, 0u, 0u, numOf1stOpt, 0u,
                     config->ServiceId, config->InstanceId, config->MajorVersion, 0u,
                     eventHandler->EventGroupId, config->ServerTimer->TTL);
  Sd_BuildHeader(instance->Buffer, instance->Context->Flags, instance->Context->MulticastSessionId,
                 16u, lengthOfOptions);
  instance->Context->MulticastSessionId++;
  if (0u == instance->Context->MulticastSessionId) {
    instance->Context->MulticastSessionId = 1;
    instance->Context->Flags &= ~SD_REBOOT_FLAG;
  }
#if (defined(_WIN32) || defined(linux)) && !defined(USE_LWIP)
  return Sd_Transmit(instance->TxPdu.MulticastTxPduId, instance->Buffer, 44u + lengthOfOptions,
                     NULL);
#else
  RemoteAddr = sub->RemoteAddr;
  RemoteAddr.Port = sub->Port;
  return Sd_Transmit(instance->TxPdu.UnicastTxPduId, instance->Buffer, 44u + lengthOfOptions,
                     &RemoteAddr);
#endif
}

static uint16_t Sd_NumberOfSubscribes(const Sd_EventHandlerType *eventHandler) {
  uint16_t numberOfSubscribes = 0u;
  Sd_EventHandlerContextType *context = eventHandler->Context;
  DEC_SQP(EventHandlerSubscriber);
  SQP_WHILE(EventHandlerSubscriber) {
    numberOfSubscribes++;
  }
  SQP_WHILE_END()

  return numberOfSubscribes;
}

static Sd_EventHandlerSubscriberType *Sd_LookupSubscribe(const Sd_EventHandlerType *eventHandler,
                                                         const TcpIp_SockAddrType *remoteAddr) {
  Sd_EventHandlerSubscriberType *sub = NULL;
  Sd_EventHandlerContextType *context = eventHandler->Context;
  int result;
  DEC_SQP(EventHandlerSubscriber);
  SQP_WHILE(EventHandlerSubscriber) {
    ASLOG(SDI, ("Exist subscriber(%p) %d.%d.%d.%d:%d for event group %x\n", var,
                var->RemoteAddr.Addr[0], var->RemoteAddr.Addr[1], var->RemoteAddr.Addr[2],
                var->RemoteAddr.Addr[3], var->RemoteAddr.Port, eventHandler->EventGroupId));
    result = memcmp(&var->RemoteAddr, remoteAddr, sizeof(TcpIp_SockAddrType));
    if (0 == result) {
      sub = var;
      break;
    }
  }
  SQP_WHILE_END()

  if (NULL == sub) {
    sub = SQP_ALLOC(EventHandlerSubscriber);
    if (NULL != sub) {
      (void)memset(sub, 0u, sizeof(Sd_EventHandlerSubscriberType));
      ASLOG(SDI, ("New subscriber(%p) %d.%d.%d.%d:%d for event group %x\n", sub,
                  remoteAddr->Addr[0], remoteAddr->Addr[1], remoteAddr->Addr[2],
                  remoteAddr->Addr[3], remoteAddr->Port, eventHandler->EventGroupId));
    }
  }

  return sub;
}

static Std_ReturnType Sd_HandleSubscribeEventGroup(const Sd_InstanceType *instance,
                                                   const TcpIp_SockAddrType *remoteAddr,
                                                   const Sd_EntryType2Type *entry2,
                                                   const Sd_OptionIPv4Type *ipv4Opt) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i;
  const Sd_ServerServiceType *config;
  const Sd_EventHandlerType *EventHandler;
  Sd_EventHandlerSubscriberType *sub = NULL;
  Sd_EventHandlerContextType *context = NULL;
  uint16_t numOfSubscribers;
#if !defined(_WIN32)
  int result;
#endif

  for (i = 0u; i < instance->NumOfServerServices; i++) {
    config = &instance->ServerServices[i];
    if ((config->ServiceId == entry2->ServiceId) || (config->InstanceId == entry2->InstanceId)) {
      ret = E_OK;
      break;
    }
  }

  if (E_OK == ret) {
    ret = E_NOT_OK;
    for (i = 0u; i < config->NumOfEventHandlers; i++) {
      EventHandler = &config->EventHandlers[i];
      if (EventHandler->EventGroupId == entry2->EventGroupId) {
        context = EventHandler->Context;
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
    result = memcmp(ipv4Opt->Addr.Addr, remoteAddr->Addr, sizeof(remoteAddr->Addr));
    if (0 != result) {
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
    if (entry2->TTL > 0u) {
      sub->RemoteAddr = ipv4Opt->Addr;
      sub->Port = remoteAddr->Port;
      numOfSubscribers = Sd_NumberOfSubscribes(EventHandler);
      /* @ECUC_SD_00097 */
      if ((0u == EventHandler->MulticastThreshold) ||
          ((numOfSubscribers + 1u) < EventHandler->MulticastThreshold)) {
        ret = SomeIp_ResolveSubscriber(config->SomeIpServiceId, sub);
        if (E_OK != ret) {
          ASLOG(SDE, ("can't resolve subscriber %d.%d.%d.%d:%d\n", sub->RemoteAddr.Addr[0],
                      sub->RemoteAddr.Addr[1], sub->RemoteAddr.Addr[2], sub->RemoteAddr.Addr[3],
                      sub->RemoteAddr.Port));
          if (SD_FLG_EVENT_GROUP_UNSUBSCRIBED != sub->Flags) {
            EventHandler->OnSubscribe(FALSE, &sub->RemoteAddr);
            SQP_CRM_AND_FREE(EventHandlerSubscriber, sub);
          } else {
            SQP_FREE(EventHandlerSubscriber, sub);
          }
        }
      } else {
        sub->TxPduId = EventHandler->MulticastTxPduId;
        if (FALSE == context->IsMulticastOpened) {
          (void)SoAd_OpenSoCon(EventHandler->MulticastEventSoConRef);
          context->IsMulticastOpened = TRUE;
          ASLOG(SDI, ("SOCK[%u] consumed event group %x:%x:%x multicast on %d.%d.%d.%d:%d\n",
                      EventHandler->MulticastEventSoConRef, entry2->ServiceId, entry2->EventGroupId,
                      entry2->InstanceId, ipv4Opt->Addr.Addr[0], ipv4Opt->Addr.Addr[1],
                      ipv4Opt->Addr.Addr[2], ipv4Opt->Addr.Addr[3], ipv4Opt->Addr.Port));
        }
      }
    }
  }

  if (E_OK == ret) {
    if (entry2->TTL > 0u) {
      if (SD_FLG_EVENT_GROUP_UNSUBSCRIBED == sub->Flags) {
        sub->Flags = SD_FLG_EVENT_GROUP_SUBSCRIBED;
        if (sub->TxPduId == EventHandler->MulticastTxPduId) {
          sub->Flags |= SD_FLG_EVENT_GROUP_MULTICAST;
          SQP_CPREPEND(EventHandlerSubscriber, sub);
        } else {
          SQP_CAPPEND(EventHandlerSubscriber, sub);
        }
        EventHandler->OnSubscribe(TRUE, &sub->RemoteAddr);
      }
      ret = Sd_ResponseSubscribeEventGroup(instance, config, EventHandler, sub);
      if (E_OK != ret) { /* retry next time */
        SD_SET(sub->Flags, SD_FLG_PENDING_EVENT_GROUP_ACK);
      }
      if (DEFAULT_TTL != entry2->TTL) {
        sub->TTL = SD_CONVERT_MS_TO_MAIN_CYCLES(entry2->TTL * 1000u);
      }
      ret = E_OK;
    } else {
      ret = E_NOT_OK;
    }
  }

  if ((E_OK != ret) && (NULL != sub)) {
    if (SD_FLG_EVENT_GROUP_UNSUBSCRIBED != sub->Flags) {
      EventHandler->OnSubscribe(FALSE, &sub->RemoteAddr);
      SQP_CRM_AND_FREE(EventHandlerSubscriber, sub);
    } else {
      SQP_FREE(EventHandlerSubscriber, sub);
    }
  }

  return ret;
}

static Std_ReturnType Sd_HandleSubscribeEventGroupAck(const Sd_InstanceType *instance,
                                                      const Sd_EntryType2Type *entry2,
                                                      const Sd_OptionIPv4Type *ipv4Opt) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i;
  const Sd_ClientServiceType *config;
  const Sd_ConsumedEventGroupType *ConsumedEventGroup;

  for (i = 0u; i < instance->NumOfClientServices; i++) {
    config = &instance->ClientServices[i];
    if ((config->ServiceId == entry2->ServiceId) || (config->InstanceId == entry2->InstanceId)) {
      ret = E_OK;
      break;
    }
  }

  if (E_OK == ret) {
    ret = E_NOT_OK;
    for (i = 0u; i < config->NumOfConsumedEventGroups; i++) {
      ConsumedEventGroup = &config->ConsumedEventGroups[i];
      if (ConsumedEventGroup->EventGroupId == entry2->EventGroupId) {
        ret = E_OK;
        break;
      }
    }
  }

  if (E_OK == ret) {
    if (entry2->TTL > 0u) {
      if (FALSE == ConsumedEventGroup->Context->IsSubscribed) {
        if ((NULL != ipv4Opt) && (ConsumedEventGroup->MulticastThreshold > 0u)) {
          (void)SoAd_SetRemoteAddr(ConsumedEventGroup->MulticastEventSoConRef, &ipv4Opt->Addr);
          (void)SoAd_OpenSoCon(ConsumedEventGroup->MulticastEventSoConRef);
          ASLOG(SDI, ("SOCK[%u] consumed event group %x:%x:%x multicast on %d.%d.%d.%d:%d\n",
                      ConsumedEventGroup->MulticastEventSoConRef, entry2->ServiceId,
                      entry2->EventGroupId, entry2->InstanceId, ipv4Opt->Addr.Addr[0],
                      ipv4Opt->Addr.Addr[1], ipv4Opt->Addr.Addr[2], ipv4Opt->Addr.Addr[3],
                      ipv4Opt->Addr.Port));
        }
      }
      ConsumedEventGroup->Context->IsSubscribed = TRUE;
      ConsumedEventGroup->OnSubscribe(TRUE);
    } else {
      ASLOG(SDE, ("invalid TTL for subscribe group ack\n"));
      ConsumedEventGroup->Context->IsSubscribed = FALSE;
      if (ConsumedEventGroup->MulticastThreshold > 0u) {
        (void)SoAd_CloseSoCon(ConsumedEventGroup->MulticastEventSoConRef, TRUE);
        ASLOG(SDI, ("SOCK[%u] consumed event group %x:%x:%x multicast off\n",
                    ConsumedEventGroup->MulticastEventSoConRef, entry2->ServiceId,
                    entry2->EventGroupId, entry2->InstanceId));
      }
      ConsumedEventGroup->OnSubscribe(FALSE);
    }
  }

  return ret;
}

static void Sd_HandleMsg(const Sd_InstanceType *instance, const PduInfoType *pduInfoPtr,
                         boolean isMulticast) {
  const TcpIp_SockAddrType *RemoteAddr = (TcpIp_SockAddrType *)pduInfoPtr->MetaDataPtr;
  uint8_t *data = pduInfoPtr->SduDataPtr;
  PduLengthType length = pduInfoPtr->SduLength;
  Sd_HeaderType header;
  Std_ReturnType ret;
  uint8_t type;
  union {
    Sd_EntryType1Type Entry1;
    Sd_EntryType2Type Entry2;
  } ET;
  union {
    Sd_OptionIPv4Type Ipv4Opt;
  } OPT;
  uint16_t i;
  (void)isMulticast;

  ASLOG(SD, ("[%s] Rx %s %d bytes from %d.%d.%d.%d:%d\n", instance->Hostname,
             isMulticast ? "Multicast" : "Unicast", pduInfoPtr->SduLength, RemoteAddr->Addr[0],
             RemoteAddr->Addr[1], RemoteAddr->Addr[2], RemoteAddr->Addr[3], RemoteAddr->Port));

  ret = Sd_DecodeHeader(data, length, &header);
  for (i = 0u; (i < header.LengthOfEntries) && (E_OK == ret);) {
    type = data[24u + i];
    switch (type) {
    case SD_FIND_SERVICE:
      ret = Sd_DecodeEntryType1OF(&data[24u + i], NULL, &ET.Entry1, NULL);
      if (E_OK == ret) {
        (void)Sd_HandleFindService(instance, RemoteAddr, &ET.Entry1);
      }
      i += 16u;
      break;
    case SD_OFFER_SERVICE:
      ret = Sd_DecodeEntryType1OF(&data[24u + i], &data[28u + header.LengthOfEntries], &ET.Entry1,
                                  &OPT.Ipv4Opt);
      if (E_OK == ret) {
        (void)Sd_HandleOfferService(instance, RemoteAddr, &header, &ET.Entry1, &OPT.Ipv4Opt);
      }
      i += 16u;
      break;
    case SD_SUBSCRIBE_EVENT_GROUP:
      ret = Sd_DecodeEntryType2SEG(&data[24u + i], &data[28u + header.LengthOfEntries], &ET.Entry2,
                                   &OPT.Ipv4Opt);
      if (E_OK == ret) {
        (void)Sd_HandleSubscribeEventGroup(instance, RemoteAddr, &ET.Entry2, &OPT.Ipv4Opt);
      }
      i += 16u;
      break;
    case SD_SUBSCRIBE_EVENT_GROUP_ACK:
      ret = Sd_DecodeEntryType2SEGAck(&data[24u + i], &data[28u + header.LengthOfEntries],
                                      &ET.Entry2, &OPT.Ipv4Opt);
      if (E_OK == ret) {
        (void)Sd_HandleSubscribeEventGroupAck(instance, &ET.Entry2, &OPT.Ipv4Opt);
      }
      i += 16u;
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
  for (i = 0u; i < config->NumOfEventHandlers; i++) {
    EventHandler = &config->EventHandlers[i];
    (void)memset(EventHandler->Context, 0u, sizeof(Sd_EventHandlerContextType));
    SQP_INIT(EventHandlerSubscriber);
    STAILQ_INIT(&EventHandler->Context->ListEventHandlerSubscribers);
  }
}

static void Sd_ReInitServerServiceEventHandlers(const Sd_ServerServiceType *config) {
  uint16_t i;
  const Sd_EventHandlerType *EventHandler;
  Sd_EventHandlerContextType *context;
  DEC_SQP(EventHandlerSubscriber);

  for (i = 0u; i < config->NumOfEventHandlers; i++) {
    EventHandler = &config->EventHandlers[i];
    context = EventHandler->Context;
    SQP_WHILE(EventHandlerSubscriber) {
      if (SD_FLG_EVENT_GROUP_UNSUBSCRIBED != var->Flags) {
        EventHandler->OnSubscribe(FALSE, &var->RemoteAddr);
      }
      SQP_CRM_AND_FREE(EventHandlerSubscriber, var);
    }
    SQP_WHILE_END() if (TRUE == context->IsMulticastOpened) {
      (void)SoAd_CloseSoCon(EventHandler->MulticastEventSoConRef, TRUE);
      ASLOG(SDI, ("SOCK[%u] event group %x:-:%x multicast off\n",
                  EventHandler->MulticastEventSoConRef, config->ServiceId, config->InstanceId));
    }
    (void)memset(EventHandler->Context, 0u, sizeof(Sd_EventHandlerContextType));
    STAILQ_INIT(&EventHandler->Context->ListEventHandlerSubscribers);
  }
}

static void Sd_InitServerService(const Sd_InstanceType *instance) {
  uint16_t i;
  const Sd_ServerServiceType *config;
  Sd_ServerServiceContextType *context;

  for (i = 0u; i < instance->NumOfServerServices; i++) {
    config = &instance->ServerServices[i];
    context = config->Context;
    (void)memset(context, 0u, sizeof(*context));
    Sd_InitServerServiceEventHandlers(config);
    if (TRUE == config->AutoAvailable) {
      SD_SET(context->Flags, SD_FLG_STATE_REQUEST_ONLINE);
    }
  }
}

static void Sd_InitClientServiceConsumedEventGroups(const Sd_ClientServiceType *config,
                                                    boolean soft) {
  uint16_t i;
  uint8_t flags;
  const Sd_ConsumedEventGroupType *ConsumedEventGroup;
  Sd_ConsumedEventGroupContextType *context;
  for (i = 0u; i < config->NumOfConsumedEventGroups; i++) {
    ConsumedEventGroup = &config->ConsumedEventGroups[i];
    context = ConsumedEventGroup->Context;
    flags = ConsumedEventGroup->Context->Flags;
    if ((TRUE == soft) && (TRUE == context->IsSubscribed) &&
        (ConsumedEventGroup->MulticastThreshold > 0u)) {
      (void)SoAd_CloseSoCon(ConsumedEventGroup->MulticastEventSoConRef, TRUE);
      ASLOG(SDI,
            ("SOCK[%u] consumed event group %x:-:%x multicast off\n",
             ConsumedEventGroup->MulticastEventSoConRef, config->ServiceId, config->InstanceId));
    }
    (void)memset(context, 0u, sizeof(Sd_ConsumedEventGroupContextType));
    if ((TRUE == ConsumedEventGroup->AutoRequire) ||
        ((TRUE == soft) && (0u != (flags & SD_FLG_STATE_REQUEST_ONLINE)))) {
      SD_SET(context->Flags, SD_FLG_STATE_REQUEST_ONLINE);
    }
  }
}

static void Sd_InitClientService(const Sd_InstanceType *instance) {
  uint16_t i;
  const Sd_ClientServiceType *config;
  Sd_ClientServiceContextType *context;

  for (i = 0u; i < instance->NumOfClientServices; i++) {
    config = &instance->ClientServices[i];
    context = config->Context;
    (void)memset(context, 0u, sizeof(*context));
    if (TRUE == config->AutoRequire) {
      context->Phase = SD_PHASE_INITIAL_WAIT;
      context->FindTimer = Sd_RandTime(config->ClientTimer->InitialFindDelayMin,
                                       config->ClientTimer->InitialFindDelayMax);
      context->IsOffered = FALSE;
      context->TTL = 0u;
    }
    Sd_InitClientServiceConsumedEventGroups(config, FALSE);
  }
}

static void Sd_ServerServiceLinkControl(const Sd_ServerServiceType *config) {
  Sd_ServerServiceContextType *context = config->Context;
  if (context->Phase != SD_PHASE_DOWN) {
    if (0u == (SD_FLG_LINK_UP & context->Flags)) {
      (void)SoAd_OpenSoCon(config->SoConId);
      ASLOG(SDI, ("SOCK[%u] server service %x:-:%x link up with %s\n", config->SoConId,
                  config->ServiceId, config->InstanceId,
                  (config->ProtocolType == TCPIP_IPPROTO_TCP) ? "TCP" : "UDP"));
      SD_SET(context->Flags, SD_FLG_LINK_UP);
    }
  } else {
    if (0u != (SD_FLG_LINK_UP & context->Flags)) {
      (void)SoAd_CloseSoCon(config->SoConId, TRUE);
      SD_CLEAR(context->Flags, SD_FLG_LINK_UP);
      ASLOG(SDI, ("SOCK[%u] server service %x:-:%x link down\n", config->SoConId, config->ServiceId,
                  config->InstanceId));
    }
  }
}

static void Sd_ServerServiceMain_Down(const Sd_ServerServiceType *config) {
  Sd_ServerServiceContextType *context = config->Context;

  if (0u != (context->Flags & SD_FLG_STATE_REQUEST_ONLINE)) { /* @SWS_SD_00317 */
    Sd_InitServerServiceEventHandlers(config);
    context->Phase = SD_PHASE_INITIAL_WAIT;
    /* @SWS_SD_00318 */
    context->OfferTimer = Sd_RandTime(config->ServerTimer->InitialOfferDelayMin,
                                      config->ServerTimer->InitialOfferDelayMax);
    ASLOG(SD, ("Service %X:%X going up\n", config->ServiceId, config->InstanceId));
  }
}

static void Sd_ServerServiceMain_InitialWait(const Sd_ServerServiceType *config) {
  Sd_ServerServiceContextType *context = config->Context;
  if (0u == (context->Flags & SD_FLG_STATE_REQUEST_ONLINE)) {
    /* @SWS_SD_00323 */
    context->OfferTimer = 0u;
    Sd_ReInitServerServiceEventHandlers(config);
    context->Phase = SD_PHASE_DOWN;
  } else {
    if (context->OfferTimer > 0u) {
      context->OfferTimer--;
      if (0u == context->OfferTimer) {
        /* @SWS_SD_00321 */
        SD_SET(context->Flags, SD_FLG_PENDING_OFFER);
        context->Counter = 0u;
        /* @SWS_SD_00434, @SWS_SD_00435 */
        if (config->ServerTimer->InitialOfferRepetitionsMax > 0u) {
          ASLOG(SD, ("Service %X:%X enter repetition\n", config->ServiceId, config->InstanceId));
          context->Phase = SD_PHASE_REPETITION;
          context->OfferTimer = config->ServerTimer->InitialOfferRepetitionBaseDelay;
        } else {
          ASLOG(SD, ("Service %X:%X enter main\n", config->ServiceId, config->InstanceId));
          context->Phase = SD_PHASE_MAIN;
          context->OfferTimer = config->ServerTimer->OfferCyclicDelay;
        }
      }
    } else {
      ASLOG(SDE, ("Timer not started in phase INITIAL_WAIT\n"));
    }
  }
}

static void Sd_ServerServiceMain_Repetition(const Sd_ServerServiceType *config) {
  Sd_ServerServiceContextType *context = config->Context;
  if (0u == (context->Flags & SD_FLG_STATE_REQUEST_ONLINE)) {
    SD_SET(context->Flags, SD_FLG_PENDING_STOP_OFFER);
    Sd_ReInitServerServiceEventHandlers(config);
    context->OfferTimer = 0u;
    context->Phase = SD_PHASE_DOWN;
  } else {
    if (context->OfferTimer > 0u) {
      context->OfferTimer--;
      if (0u == context->OfferTimer) {
        SD_SET(context->Flags, SD_FLG_PENDING_OFFER);
        context->Counter++;
        if (context->Counter < config->ServerTimer->InitialOfferRepetitionsMax) {
          context->OfferTimer = config->ServerTimer->InitialOfferRepetitionBaseDelay
                                << context->Counter;
        } else {
          ASLOG(SD, ("Service %X:%X enter main\n", config->ServiceId, config->InstanceId));
          context->Phase = SD_PHASE_MAIN;
          context->OfferTimer = config->ServerTimer->OfferCyclicDelay;
        }
      }
    } else {
      ASLOG(SDE, ("Timer not started in phase INITIAL_WAIT\n"));
    }
  }
}

static void Sd_ServerServiceMain_Main(const Sd_ServerServiceType *config) {
  Sd_ServerServiceContextType *context = config->Context;
  if (0u == (context->Flags & SD_FLG_STATE_REQUEST_ONLINE)) {
    Sd_ReInitServerServiceEventHandlers(config);
    SD_SET(context->Flags, SD_FLG_PENDING_STOP_OFFER);
    context->OfferTimer = 0u;
    context->Phase = SD_PHASE_DOWN;
  } else {
    if (context->OfferTimer > 0u) {
      context->OfferTimer--;
      if (0u == context->OfferTimer) {
        SD_SET(context->Flags, SD_FLG_PENDING_OFFER);
        if (context->Counter < 0xFFu) {
          context->Counter++;
        }
        context->OfferTimer = config->ServerTimer->OfferCyclicDelay;
      }
    } else {
      if ((0u == config->ServerTimer->OfferCyclicDelay) &&
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
  Sd_ClientServiceContextType *context = config->Context;
  const Sd_ConsumedEventGroupType *ConsumedEventGroup;

  context->FindTimer = 0u;
  for (i = 0u; i < config->NumOfConsumedEventGroups; i++) {
    ConsumedEventGroup = &config->ConsumedEventGroups[i];
    if (TRUE == ConsumedEventGroup->Context->IsSubscribed) {
      ConsumedEventGroup->Context->IsSubscribed = FALSE;
      SD_SET_CLEAR(ConsumedEventGroup->Context->Flags, SD_FLG_PENDING_STOP_SUBSCRIBE,
                   SD_FLG_PENDING_SUBSCRIBE | SD_FLG_STATE_REQUEST_ONCE);
    }
  }
  context->Phase = SD_PHASE_DOWN;
}

static void Sd_ClientService_GoToInitialWait(const Sd_ClientServiceType *config) {
  uint16_t i;
  Sd_ClientServiceContextType *context = config->Context;
  const Sd_ConsumedEventGroupType *ConsumedEventGroup;

  context->FindTimer =
    Sd_RandTime(config->ClientTimer->InitialFindDelayMin, config->ClientTimer->InitialFindDelayMax);
  for (i = 0u; i < config->NumOfConsumedEventGroups; i++) {
    ConsumedEventGroup = &config->ConsumedEventGroups[i];
    if (TRUE == ConsumedEventGroup->Context->IsSubscribed) {
      ConsumedEventGroup->OnSubscribe(FALSE);
    }
    ConsumedEventGroup->Context->IsSubscribed = FALSE;
    SD_CLEAR(ConsumedEventGroup->Context->Flags, SD_FLG_STATE_REQUEST_ONCE);
  }

  context->Phase = SD_PHASE_INITIAL_WAIT; /* @SWS_SD_00600 */
}

static void Sd_ClientServiceMain_TTL(const Sd_ClientServiceType *config) {
  Sd_ClientServiceContextType *context = config->Context;

  if (context->IsOffered && (context->TTL > 0u)) {
    context->TTL--;
    if (0u == context->TTL) {
      context->IsOffered = FALSE;
      Sd_ClientService_GoToInitialWait(config);
    }
  }
}

static void Sd_ClientServiceMain_Down(const Sd_ClientServiceType *config) {
  Sd_ClientServiceContextType *context = config->Context;
  if (0u != (context->Flags & SD_FLG_STATE_REQUEST_ONLINE)) {
    if (TRUE == context->IsOffered) {
      ASLOG(SD, ("Client %x:%x enter main by offer\n", config->ServiceId, config->InstanceId));
      context->Phase = SD_PHASE_MAIN;
      context->FindTimer = 0u;
    } else {
      ASLOG(SD, ("Client %x:%x going up\n", config->ServiceId, config->InstanceId));
      Sd_ClientService_GoToInitialWait(config);
    }
  }
}

static void Sd_ClientServiceMain_InitialWait(const Sd_ClientServiceType *config) {
  Sd_ClientServiceContextType *context = config->Context;
  if (0u == (context->Flags & SD_FLG_STATE_REQUEST_ONLINE)) {
    Sd_ClientService_GoToDown(config);
  } else if (TRUE == context->IsOffered) {
    ASLOG(SD, ("Client %x:%x enter main by offer\n", config->ServiceId, config->InstanceId));
    context->Phase = SD_PHASE_MAIN;
    context->FindTimer = 0u;
  } else {
    if (context->FindTimer > 0u) {
      context->FindTimer--;
      if (0u == context->FindTimer) {
        SD_SET(context->Flags, SD_FLG_PENDING_FIND);
        if (config->ClientTimer->InitialFindRepetitionsMax > 0u) {
          ASLOG(SD, ("Client %x:%x enter repetition\n", config->ServiceId, config->InstanceId));
          context->Phase = SD_PHASE_REPETITION;
          context->Counter = 0u;
          context->FindTimer = config->ClientTimer->InitialFindRepetitionsBaseDelay;
        } else {
          ASLOG(SD, ("Client %x:%x enter main\n", config->ServiceId, config->InstanceId));
          context->Phase = SD_PHASE_MAIN;
        }
      }
    }
  }
}

static void Sd_ClientServiceMain_Repetition(const Sd_ClientServiceType *config) {
  Sd_ClientServiceContextType *context = config->Context;
  if (0u == (context->Flags & SD_FLG_STATE_REQUEST_ONLINE)) {
    Sd_ClientService_GoToDown(config);
  } else if (TRUE == context->IsOffered) {
    ASLOG(SD, ("Client %x:%x enter main by offer\n", config->ServiceId, config->InstanceId));
    context->Phase = SD_PHASE_MAIN;
    context->FindTimer = 0u;
  } else {
    if (context->FindTimer > 0u) {
      context->FindTimer--;
      if (0u == context->FindTimer) {
        SD_SET(context->Flags, SD_FLG_PENDING_FIND);
        context->Counter++;
        if (context->Counter < config->ClientTimer->InitialFindRepetitionsMax) {
          context->FindTimer = config->ClientTimer->InitialFindRepetitionsBaseDelay
                               << context->Counter;
        } else {
          ASLOG(SD, ("Client %x:%x enter main\n", config->ServiceId, config->InstanceId));
          context->Phase = SD_PHASE_MAIN;
        }
      }
    }
  }
}

static void Sd_ConsumedEventGroupTTLStart(const Sd_ConsumedEventGroupType *consumedEventGroup,
                                          const Sd_ClientServiceType *config) {
  if (DEFAULT_TTL != config->ClientTimer->TTL) {
    consumedEventGroup->Context->TTL =
      SD_CONVERT_MS_TO_MAIN_CYCLES(config->ClientTimer->TTL * 1000u);
    if (consumedEventGroup->Context->TTL > SD_CONVERT_MS_TO_MAIN_CYCLES(100u)) {
      consumedEventGroup->Context->TTL -= SD_CONVERT_MS_TO_MAIN_CYCLES(100u);
    }
    ASLOG(SD, ("start consumedEventGroup %x:%x timer = %u\n", consumedEventGroup->EventGroupId,
               consumedEventGroup->HandleId, consumedEventGroup->Context->TTL));
  }
}

static void Sd_ClientServiceMain_Main(const Sd_ClientServiceType *config) {
  Sd_ClientServiceContextType *context = config->Context;
  uint16_t i;
  const Sd_ConsumedEventGroupType *ConsumedEventGroup;
  if (0u == (context->Flags & SD_FLG_STATE_REQUEST_ONLINE)) {
    Sd_ClientService_GoToDown(config);
  } else if (TRUE == context->IsOffered) {
    for (i = 0u; i < config->NumOfConsumedEventGroups; i++) {
      ConsumedEventGroup = &config->ConsumedEventGroups[i];
      if (0u != (ConsumedEventGroup->Context->Flags & SD_FLG_STATE_REQUEST_ONLINE)) {
        if ((FALSE == ConsumedEventGroup->Context->IsSubscribed) &&
            (0u == (ConsumedEventGroup->Context->Flags & SD_FLG_STATE_REQUEST_ONCE))) {
          SD_SET_CLEAR(ConsumedEventGroup->Context->Flags,
                       SD_FLG_PENDING_SUBSCRIBE | SD_FLG_STATE_REQUEST_ONCE,
                       SD_FLG_PENDING_STOP_SUBSCRIBE);
          Sd_ConsumedEventGroupTTLStart(ConsumedEventGroup, config);
        } else {
          if (ConsumedEventGroup->Context->TTL > 0u) {
            ConsumedEventGroup->Context->TTL--;
            if (0u == ConsumedEventGroup->Context->TTL) {
              SD_SET(ConsumedEventGroup->Context->Flags, SD_FLG_PENDING_SUBSCRIBE);
              Sd_ConsumedEventGroupTTLStart(ConsumedEventGroup, config);
            }
          }
        }
      } else {
        if (TRUE == ConsumedEventGroup->Context->IsSubscribed) {
          ConsumedEventGroup->Context->IsSubscribed = FALSE;
          SD_SET_CLEAR(ConsumedEventGroup->Context->Flags, SD_FLG_PENDING_STOP_SUBSCRIBE,
                       SD_FLG_PENDING_SUBSCRIBE | SD_FLG_STATE_REQUEST_ONCE);
        }
      }
    }
  } else {
    /* do nothing */
  }
}

static void Sd_ServerServiceOfferCheck(const Sd_InstanceType *instance, uint32_t *lengthOfEntries,
                                       uint32_t *lengthOfOptions, uint8_t *numOfOptions) {
  uint16_t i;
  const Sd_ServerServiceType *config;
  Sd_ServerServiceContextType *context;

  for (i = 0u; i < instance->NumOfServerServices; i++) {
    config = &instance->ServerServices[i];
    context = config->Context;
    if (0u != (context->Flags & (SD_FLG_PENDING_OFFER | SD_FLG_PENDING_STOP_OFFER))) {
      if (((28u + *lengthOfEntries + *lengthOfOptions) < (instance->BufLen - 28u)) &&
          (*numOfOptions < 255u)) {
        /* @SWS_SD_00160 */
        *lengthOfEntries += 16u;
        *lengthOfOptions += 12u;
        *numOfOptions += 1u;
      } else {
        break;
      }
    }
  }
}

static void Sd_ClientServiceFindCheck(const Sd_InstanceType *instance, uint32_t *lengthOfEntries,
                                      uint32_t lengthOfOptions) {
  uint16_t i;
  const Sd_ClientServiceType *config;
  Sd_ClientServiceContextType *context;

  for (i = 0u; i < instance->NumOfClientServices; i++) {
    config = &instance->ClientServices[i];
    context = config->Context;
    if (0u != (context->Flags & SD_FLG_PENDING_FIND)) {
      if ((28u + *lengthOfEntries + lengthOfOptions) < (instance->BufLen - 16u)) {
        *lengthOfEntries += 16u;
      } else {
        break;
      }
    }
  }
}

static void Sd_ServerServiceOfferBuild(const Sd_InstanceType *instance, uint32_t *offsetOfEntries,
                                       uint32_t *offsetOfOptions, uint8_t *numOfOptions,
                                       uint32_t *freeSpace) {
  uint16_t i;
  const Sd_ServerServiceType *config;
  Sd_ServerServiceContextType *context;
  TcpIp_SockAddrType LocalAddr;
  uint32_t TTL;
  Std_ReturnType ret;

  for (i = 0u; i < instance->NumOfServerServices; i++) {
    config = &instance->ServerServices[i];
    context = config->Context;
    if (0u != (context->Flags & (SD_FLG_PENDING_OFFER | SD_FLG_PENDING_STOP_OFFER))) {
      if (*freeSpace >= 28u) {
        /* @SWS_SD_00416 */
        ret = SoAd_GetLocalAddr(config->SoConId, &LocalAddr, NULL, NULL);
        if (E_OK == ret) {
          if (0u != (context->Flags & SD_FLG_PENDING_STOP_OFFER)) {
            TTL = 0u;
          } else {
            TTL = config->ServerTimer->TTL;
          }
          SD_CLEAR(context->Flags, SD_FLG_PENDING_OFFER | SD_FLG_PENDING_STOP_OFFER);
          Sd_BuildEntryType1(&instance->Buffer[*offsetOfEntries], SD_OFFER_SERVICE, *numOfOptions,
                             0u, 1u, 0u, config->ServiceId, config->InstanceId,
                             config->MajorVersion, config->MinorVersion, TTL);
          Sd_BuildOptionIPv4Endpoint(&instance->Buffer[*offsetOfOptions], &LocalAddr,
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

static void Sd_ClientServiceFindBuild(const Sd_InstanceType *instance, uint32_t *offsetOfEntries,
                                      uint32_t *freeSpace) {
  uint16_t i;
  const Sd_ClientServiceType *config;
  Sd_ClientServiceContextType *context;

  for (i = 0u; i < instance->NumOfClientServices; i++) {
    config = &instance->ClientServices[i];
    context = config->Context;
    if (0u != (context->Flags & SD_FLG_PENDING_FIND)) {
      if (*freeSpace >= 16) {
        SD_CLEAR(context->Flags, SD_FLG_PENDING_FIND);
        Sd_BuildEntryType1(&instance->Buffer[*offsetOfEntries], SD_FIND_SERVICE, 0u, 0u, 0u, 0u,
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
  for (i = 0u; i < config->NumOfEventHandlers; i++) {
    EventHandler = &config->EventHandlers[i];
    context = EventHandler->Context;
    SQP_WHILE(EventHandlerSubscriber) {
      if (SD_FLG_EVENT_GROUP_UNSUBSCRIBED != var->Flags) {
        if (var->TTL > 0u) {
          var->TTL--;
          if (0u == var->TTL) {
            ASLOG(SDI, ("Delete subscriber(%p) %d.%d.%d.%d:%d for event group %x\n", var,
                        var->RemoteAddr.Addr[0], var->RemoteAddr.Addr[1], var->RemoteAddr.Addr[2],
                        var->RemoteAddr.Addr[3], var->RemoteAddr.Port, EventHandler->EventGroupId));
            EventHandler->OnSubscribe(FALSE, &var->RemoteAddr);
            SQP_CRM_AND_FREE(EventHandlerSubscriber, var);
          }
        }
      }
    }
    SQP_WHILE_END()

    numOfSubscribers = Sd_NumberOfSubscribes(EventHandler);
    if (0u == numOfSubscribers) {
      if (TRUE == context->IsMulticastOpened) {
        context->IsMulticastOpened = FALSE;
        (void)SoAd_CloseSoCon(EventHandler->MulticastEventSoConRef, TRUE);
        ASLOG(SDI, ("SOCK[%u] event group %x:-:%x multicast off\n",
                    EventHandler->MulticastEventSoConRef, config->ServiceId, config->InstanceId));
      }
    }
  }
}

static void Sd_ServerServiceMain(const Sd_InstanceType *instance) {
  uint16_t i;
  const Sd_ServerServiceType *config;
  Sd_ServerServiceContextType *context;
  for (i = 0u; i < instance->NumOfServerServices; i++) {
    config = &instance->ServerServices[i];
    context = config->Context;
    Sd_ServerServiceMain_TTL(config);
    Sd_ServerServiceLinkControl(config);
    switch (context->Phase) {
    case SD_PHASE_DOWN:
      Sd_ServerServiceMain_Down(config);
      break;
    case SD_PHASE_INITIAL_WAIT:
      Sd_ServerServiceMain_InitialWait(config);
      break;
    case SD_PHASE_REPETITION:
      Sd_ServerServiceMain_Repetition(config);
      break;
    case SD_PHASE_MAIN:
      Sd_ServerServiceMain_Main(config);
      break;
    default:
      break;
    }
  }
}

static void Sd_ClientServiceLinkControl(const Sd_ClientServiceType *config) {
  Sd_ClientServiceContextType *context = config->Context;
  if (context->Phase != SD_PHASE_DOWN) {
    if (TRUE == context->IsOffered) {
      if (0u == (SD_FLG_LINK_UP & context->Flags)) {
        (void)SoAd_SetRemoteAddr(config->SoConId, &(context->RemoteAddr));
        (void)SoAd_OpenSoCon(config->SoConId);
        ASLOG(SDI,
              ("SOCK[%u] client service %x:-:%x link up with %s:%d.%d.%d.%d:%d\n", config->SoConId,
               config->ServiceId, config->InstanceId,
               (config->ProtocolType == TCPIP_IPPROTO_TCP) ? "TCP" : "UDP",
               context->RemoteAddr.Addr[0], context->RemoteAddr.Addr[1],
               context->RemoteAddr.Addr[2], context->RemoteAddr.Addr[3], context->RemoteAddr.Port));
        SD_SET(context->Flags, SD_FLG_LINK_UP);
      }
    } else {
      if (0u != (SD_FLG_LINK_UP & context->Flags)) {
        (void)SoAd_CloseSoCon(config->SoConId, TRUE);
        SD_CLEAR(context->Flags, SD_FLG_LINK_UP);
        ASLOG(SDI, ("SOCK[%u] client service %x:-:%x link down\n", config->SoConId,
                    config->ServiceId, config->InstanceId));
      }
    }
  } else {
    if (0u != (SD_FLG_LINK_UP & context->Flags)) {
      (void)SoAd_CloseSoCon(config->SoConId, TRUE);
      SD_CLEAR(context->Flags, SD_FLG_LINK_UP);
      ASLOG(SDI, ("SOCK[%u] client service %x:-:%x link down\n", config->SoConId, config->ServiceId,
                  config->InstanceId));
    }
  }
}

static void Sd_ClientServiceMain(const Sd_InstanceType *instance) {
  uint16_t i;
  const Sd_ClientServiceType *config;
  Sd_ClientServiceContextType *context;
  for (i = 0u; i < instance->NumOfClientServices; i++) {
    config = &instance->ClientServices[i];
    context = config->Context;
    Sd_ClientServiceMain_TTL(config);
    Sd_ClientServiceLinkControl(config);
    switch (context->Phase) {
    case SD_PHASE_DOWN:
      Sd_ClientServiceMain_Down(config);
      break;
    case SD_PHASE_INITIAL_WAIT:
      Sd_ClientServiceMain_InitialWait(config);
      break;
    case SD_PHASE_REPETITION:
      Sd_ClientServiceMain_Repetition(config);
      break;
    case SD_PHASE_MAIN:
      Sd_ClientServiceMain_Main(config);
      break;
    default:
      break;
    }
  }
}

static boolean Sd_ServerServiceEventGroupAckCheck(const Sd_InstanceType *instance) {
  uint16_t i;
  uint16_t j;
  const Sd_ServerServiceType *config;
  const Sd_EventHandlerType *EventHandler;
  Sd_EventHandlerContextType *context;
  DEC_SQP(EventHandlerSubscriber);
  boolean TxOne = FALSE;
  Std_ReturnType ret = E_OK;

  for (i = 0u; (i < instance->NumOfServerServices) && (FALSE == TxOne); i++) {
    config = &instance->ServerServices[i];
    for (j = 0u; j < config->NumOfEventHandlers; j++) {
      EventHandler = &config->EventHandlers[j];
      context = EventHandler->Context;
      SQP_WHILE(EventHandlerSubscriber) {
        if (0u != (var->Flags & SD_FLG_PENDING_EVENT_GROUP_ACK)) {
          ret = Sd_ResponseSubscribeEventGroup(instance, config, EventHandler, var);
          if (E_OK == ret) {
            SD_CLEAR(var->Flags, SD_FLG_PENDING_EVENT_GROUP_ACK);
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
Sd_SendSubscribeEventGroup(const Sd_InstanceType *instance, const Sd_ClientServiceType *config,
                           const Sd_ConsumedEventGroupType *consumedEventGroup) {
  Std_ReturnType ret = E_OK;
  TcpIp_SockAddrType LocalAddr;
  uint32_t TTL = 0u;

  if (0u == (consumedEventGroup->Context->Flags & SD_FLG_PENDING_STOP_SUBSCRIBE)) {
    TTL = config->ClientTimer->TTL;
  } else {
    /* send stop */
  }

  Sd_BuildEntryType2(&instance->Buffer[24u], SD_SUBSCRIBE_EVENT_GROUP, 0u, 0u, 1u, 0u,
                     config->ServiceId, config->InstanceId, config->MajorVersion, 0u,
                     consumedEventGroup->EventGroupId, TTL);
  ret = SoAd_GetLocalAddr(config->SoConId, &LocalAddr, NULL, NULL);
  if (E_OK == ret) {
    ASLOG(SDI, ("SOCK[%u] local addr %d.%d.%d.%d:%d subscribe event group %x\n", config->SoConId,
                LocalAddr.Addr[0], LocalAddr.Addr[1], LocalAddr.Addr[2], LocalAddr.Addr[3],
                LocalAddr.Port, consumedEventGroup->EventGroupId));
    Sd_BuildOptionIPv4Endpoint(&instance->Buffer[44], &LocalAddr, config->ProtocolType);
    Sd_BuildHeader(instance->Buffer, instance->Context->Flags,
                   instance->Context->MulticastSessionId, 16, 12);
    instance->Context->MulticastSessionId++;
    if (0u == instance->Context->MulticastSessionId) {
      instance->Context->MulticastSessionId = 1;
      instance->Context->Flags &= ~SD_REBOOT_FLAG;
    }
#if (defined(_WIN32) || defined(linux)) && !defined(USE_LWIP)
    /* NOTE: this is a workaroud for case that server and client on the same host */
    ret = Sd_Transmit(instance->TxPdu.MulticastTxPduId, instance->Buffer, 56, NULL);
#else
    (void)memcpy(LocalAddr.Addr, config->Context->RemoteAddr.Addr, 4);
    LocalAddr.Port = config->Context->Port;
    ret = Sd_Transmit(instance->TxPdu.UnicastTxPduId, instance->Buffer, 56, &LocalAddr);
#endif
    if (E_OK != ret) {
      ASLOG(SDE, ("Sending Subscribe Event Group Ack Failed\n"));
    }
  }

  return ret;
}

static boolean Sd_ClientServiceSubscribeEventGroupCheck(const Sd_InstanceType *instance) {
  uint16_t i;
  uint16_t j;
  const Sd_ClientServiceType *config;
  const Sd_ConsumedEventGroupType *ConsumedEventGroup;
  boolean TxOne = FALSE;
  Std_ReturnType ret;

  for (i = 0u; (i < instance->NumOfClientServices) && (FALSE == TxOne); i++) {
    config = &instance->ClientServices[i];
    for (j = 0u; (j < config->NumOfConsumedEventGroups) && (FALSE == TxOne); j++) {
      ConsumedEventGroup = &config->ConsumedEventGroups[j];
      if (0u != (ConsumedEventGroup->Context->Flags &
                 (SD_FLG_PENDING_SUBSCRIBE | SD_FLG_PENDING_STOP_SUBSCRIBE))) {
        ret = Sd_SendSubscribeEventGroup(instance, config, ConsumedEventGroup);
        if (E_OK == ret) {
          SD_CLEAR(ConsumedEventGroup->Context->Flags,
                   SD_FLG_PENDING_SUBSCRIBE | SD_FLG_PENDING_STOP_SUBSCRIBE);
          TxOne = TRUE;
        }
      }
    }
  }

  return TxOne;
}

static void Sd_ServerClientServiceMain(const Sd_InstanceType *instance) {
  uint32_t lengthOfEntries = 0u;
  uint32_t lengthOfOptions = 0u;
  uint32_t offsetOfEntries = 24;
  uint8_t numOfOptions = 0u;
  uint32_t offsetOfOptions;
  uint32_t freeSpace;
  boolean TxOne = FALSE;
  Std_ReturnType ret;

  Sd_ClientServiceFindCheck(instance, &lengthOfEntries, lengthOfOptions);
  Sd_ServerServiceOfferCheck(instance, &lengthOfEntries, &lengthOfOptions, &numOfOptions);

  if (lengthOfEntries > 0u) {
    offsetOfOptions = offsetOfEntries + lengthOfEntries + 4u;
    freeSpace = lengthOfEntries + lengthOfOptions;
    numOfOptions = 0u;
    Sd_ClientServiceFindBuild(instance, &offsetOfEntries, &freeSpace);
    Sd_ServerServiceOfferBuild(instance, &offsetOfEntries, &offsetOfOptions, &numOfOptions,
                               &freeSpace);
    Sd_BuildHeader(instance->Buffer, instance->Context->Flags,
                   instance->Context->MulticastSessionId, lengthOfEntries, lengthOfOptions);
    instance->Context->MulticastSessionId++;
    if (0u == instance->Context->MulticastSessionId) {
      instance->Context->MulticastSessionId = 1u;
      instance->Context->Flags &= ~SD_REBOOT_FLAG;
    }

    ret = Sd_Transmit(instance->TxPdu.MulticastTxPduId, instance->Buffer,
                      28u + lengthOfEntries + lengthOfOptions, NULL);
    if (E_OK == ret) {
      TxOne = TRUE;
    }
  }
  if (FALSE == TxOne) {
    TxOne = Sd_ServerServiceEventGroupAckCheck(instance);
  }
  if (FALSE == TxOne) {
    TxOne = Sd_ClientServiceSubscribeEventGroupCheck(instance);
  }
}

static Std_ReturnType Sd_ServerSoConModeChg(const Sd_InstanceType *instance,
                                            SoAd_SoConIdType soConId, SoAd_SoConModeType mode) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i;
  uint16_t j;
  const Sd_ServerServiceType *config;
  const Sd_EventHandlerType *EventHandler;
  for (i = 0u; i < instance->NumOfServerServices; i++) {
    config = &instance->ServerServices[i];
    for (j = 0u; j < config->NumOfEventHandlers; j++) {
      EventHandler = &config->EventHandlers[j];
      if ((EventHandler->MulticastThreshold > 0u) &&
          (EventHandler->MulticastEventSoConRef == soConId)) {
        if (SOAD_SOCON_OFFLINE == mode) {
          if (TRUE == EventHandler->Context->IsMulticastOpened) {
            EventHandler->Context->IsMulticastOpened = FALSE;
            ASLOG(SDE, ("Server Event Group %x multicast offline\n", EventHandler->EventGroupId));
          }
        } else {
          if (FALSE == EventHandler->Context->IsMulticastOpened) {
            EventHandler->Context->IsMulticastOpened = TRUE;
            ASLOG(SDE, ("Server Event Group %x multicast online\n", EventHandler->EventGroupId));
          }
        }
        ret = E_OK;
      }
    }
  }

  return ret;
}

static Std_ReturnType Sd_ClientSoConModeChg(const Sd_InstanceType *instance,
                                            SoAd_SoConIdType soConId, SoAd_SoConModeType mode) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i;
  uint16_t j;
  const Sd_ClientServiceType *config;
  const Sd_ConsumedEventGroupType *ConsumedEventGroup;
  for (i = 0u; i < instance->NumOfClientServices; i++) {
    config = &instance->ClientServices[i];
    for (j = 0u; j < config->NumOfConsumedEventGroups; j++) {
      ConsumedEventGroup = &config->ConsumedEventGroups[j];
      if ((ConsumedEventGroup->MulticastThreshold > 0u) &&
          (ConsumedEventGroup->MulticastEventSoConRef == soConId)) {
        if (SOAD_SOCON_OFFLINE == mode) {
          if (TRUE == ConsumedEventGroup->Context->IsSubscribed) {
            ASLOG(SDE,
                  ("Client Event Group %x multicast offline\n", ConsumedEventGroup->EventGroupId));
          }
        } else {
          if (FALSE == ConsumedEventGroup->Context->IsSubscribed) {
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
/* ================================ [ FUNCTIONS ] ============================================== */
void Sd_Init(const Sd_ConfigType *configPtr) {
  uint16_t i;
  const Sd_InstanceType *Instance;
  SoAd_SoConIdType SoConId;

  if (NULL != configPtr) {
    sdConfigPtr = configPtr;
  } else {
    sdConfigPtr = &Sd_Config;
  }

  for (i = 0u; i < SD_CONFIG->NumOfInstances; i++) {
    Instance = &SD_CONFIG->Instances[i];
    Instance->Context->Flags = SD_REBOOT_FLAG | SD_UNICAST_FLAG;
    Instance->Context->MulticastSessionId = 0x0001; /* @SWS_SD_00034 */
    (void)SoAd_GetSoConId(Instance->TxPdu.MulticastTxPduId, &SoConId);
    (void)SoAd_OpenSoCon(SoConId);
    (void)SoAd_GetSoConId(Instance->TxPdu.UnicastTxPduId, &SoConId);
    (void)SoAd_OpenSoCon(SoConId);
    Sd_InitServerService(Instance);
    Sd_InitClientService(Instance);
  }
}

void Sd_SoConModeChg(SoAd_SoConIdType soConId, SoAd_SoConModeType mode) {
  Std_ReturnType ret = E_NOT_OK;
  uint16_t i;
  const Sd_InstanceType *Instance;
  for (i = 0u; i < SD_CONFIG->NumOfInstances; i++) {
    Instance = &SD_CONFIG->Instances[i];
    if (soConId == Instance->MulticastRxPdu.SoConId) {
      if (SOAD_SOCON_OFFLINE == mode) {
        ASLOG(WARN, ("SD multicast socket offline\n"));
      } else {
        ret = E_OK;
      }
    } else if (soConId == Instance->UnicastRxPdu.SoConId) {
      if (SOAD_SOCON_OFFLINE == mode) {
        ASLOG(WARN, ("SD unicast socket offline\n"));
      } else {
        ret = E_OK;
      }
    } else {
      /* multicast event socket */
      ret = Sd_ServerSoConModeChg(Instance, soConId, mode);
      if (E_OK != ret) {
        ret = Sd_ClientSoConModeChg(Instance, soConId, mode);
      }
    }
  }
  if (E_OK != ret) {
    ASLOG(SDE, ("SoConId %d mode %d\n", soConId, mode));
  }
}

/* This code logic looks weird, just check to ensure things in correct status */
void Sd_RxIndication(PduIdType rxPduId, const PduInfoType *pduInfoPtr) {
  uint16_t i;
  boolean isMulticast = TRUE;
  const Sd_InstanceType *Instance = NULL;
  PCAP_TRACE(pduInfoPtr->SduDataPtr, pduInfoPtr->SduLength,
             (TcpIp_SockAddrType *)pduInfoPtr->MetaDataPtr, TRUE);
  for (i = 0u; i < SD_CONFIG->NumOfInstances; i++) {
    if (rxPduId == SD_CONFIG->Instances[i].MulticastRxPdu.RxPduId) {
      Instance = &SD_CONFIG->Instances[i];
    } else if (rxPduId == SD_CONFIG->Instances[i].UnicastRxPdu.RxPduId) {
      Instance = &SD_CONFIG->Instances[i];
      isMulticast = FALSE;
    } else {
      continue;
    }

    if (NULL != Instance) {
      break;
    }
  }

  if (NULL != Instance) {
    Sd_HandleMsg(Instance, pduInfoPtr, isMulticast);
  } else {
    ASLOG(SDE, ("Rx with unknown PDU ID %d\n", rxPduId));
  }
}

void Sd_MainFunction(void) {
  uint16_t i;
  boolean bLinkedUp = TcpIp_IsLinkedUp();
  for (i = 0u; (TRUE == bLinkedUp) && (i < SD_CONFIG->NumOfInstances); i++) {
    Sd_ServerServiceMain(&SD_CONFIG->Instances[i]);
    Sd_ClientServiceMain(&SD_CONFIG->Instances[i]);
    Sd_ServerClientServiceMain(&SD_CONFIG->Instances[i]);
  }
}

Std_ReturnType Sd_ServerServiceSetState(uint16_t sdServerServiceHandleId,
                                        Sd_ServerServiceSetStateType serverServiceState) {
  const Sd_ServerServiceType *config;
  Sd_ServerServiceContextType *context;
  Std_ReturnType ret = E_NOT_OK;
  if (sdServerServiceHandleId < SD_CONFIG->NumOfServerServices) {
    config = SD_CONFIG->ServerServicesMap[sdServerServiceHandleId];
    context = config->Context;
    if (SD_SERVER_SERVICE_AVAILABLE == serverServiceState) {
      SD_SET(context->Flags, SD_FLG_STATE_REQUEST_ONLINE);
    } else {
      SD_CLEAR(context->Flags, SD_FLG_STATE_REQUEST_ONLINE);
    }
    ret = E_OK;
  }

  return ret;
}
Std_ReturnType Sd_ClientServiceSetState(uint16_t clientServiceHandleId,
                                        Sd_ClientServiceSetStateType clientServiceState) {
  const Sd_ClientServiceType *config;
  Sd_ClientServiceContextType *context;
  Std_ReturnType ret = E_NOT_OK;
  if (clientServiceHandleId < SD_CONFIG->NumOfClientServices) {
    config = SD_CONFIG->ClientServicesMap[clientServiceHandleId];
    context = config->Context;
    if (SD_CLIENT_SERVICE_REQUESTED == clientServiceState) {
      SD_SET(context->Flags, SD_FLG_STATE_REQUEST_ONLINE);
    } else {
      SD_CLEAR(context->Flags, SD_FLG_STATE_REQUEST_ONLINE);
    }
    ret = E_OK;
  }

  return ret;
}

Std_ReturnType
Sd_ConsumedEventGroupSetState(uint16_t sdConsumedEventGroupHandleId,
                              Sd_ConsumedEventGroupSetStateType consumedEventGroupState) {
  Std_ReturnType ret = E_OK;
  uint16_t index;
  const Sd_ClientServiceType *config;
  const Sd_ConsumedEventGroupType *ConsumedEventGroup;
  if (sdConsumedEventGroupHandleId < SD_CONFIG->NumOfConsumedEventGroups) {
    index = SD_CONFIG->ConsumedEventGroupsMap[sdConsumedEventGroupHandleId];
    config = SD_CONFIG->ClientServicesMap[index];
    index = SD_CONFIG->PerServiceConsumedEventGroupsMap[sdConsumedEventGroupHandleId];
    ConsumedEventGroup = &config->ConsumedEventGroups[index];
    if (SD_CONSUMED_EVENTGROUP_REQUESTED == consumedEventGroupState) {
      SD_SET(ConsumedEventGroup->Context->Flags, SD_FLG_STATE_REQUEST_ONLINE);
    } else {
      SD_CLEAR(ConsumedEventGroup->Context->Flags, SD_FLG_STATE_REQUEST_ONLINE);
    }
  } else {
    ret = E_NOT_OK;
  }
  return ret;
}

Std_ReturnType Sd_ConsumedEventGroupGetState(
  uint16_t sdConsumedEventGroupHandleId,
  Sd_ConsumedEventGroupCurrentStateType *consumedEventGroupCurrentState) {
  Std_ReturnType ret = E_OK;
  uint16_t index;
  const Sd_ClientServiceType *config;
  const Sd_ConsumedEventGroupType *ConsumedEventGroup;
  if (sdConsumedEventGroupHandleId < SD_CONFIG->NumOfConsumedEventGroups) {
    index = SD_CONFIG->ConsumedEventGroupsMap[sdConsumedEventGroupHandleId];
    config = SD_CONFIG->ClientServicesMap[index];
    index = SD_CONFIG->PerServiceConsumedEventGroupsMap[sdConsumedEventGroupHandleId];
    ConsumedEventGroup = &config->ConsumedEventGroups[index];
    if ((0u != (ConsumedEventGroup->Context->Flags & SD_FLG_STATE_REQUEST_ONLINE)) &&
        (TRUE == ConsumedEventGroup->Context->IsSubscribed)) {
      *consumedEventGroupCurrentState = SD_CONSUMED_EVENTGROUP_AVAILABLE;
    } else {
      *consumedEventGroupCurrentState = SD_CONSUMED_EVENTGROUP_DOWN;
    }
  } else {
    ret = E_NOT_OK;
  }
  return ret;
}

Std_ReturnType Sd_GetSubscribers(uint16_t eventHandlerId,
                                 Sd_EventHandlerSubscriberListType **list) {
  Std_ReturnType ret = E_OK;
  uint16_t index;
  const Sd_ServerServiceType *config;
  const Sd_EventHandlerType *EventHandler;
  Sd_EventHandlerContextType *context;
  if (eventHandlerId < SD_CONFIG->NumOfEventHandlers) {
    index = SD_CONFIG->EventHandlersMap[eventHandlerId];
    config = SD_CONFIG->ServerServicesMap[index];
    index = SD_CONFIG->PerServiceEventHandlerMap[eventHandlerId];
    EventHandler = &config->EventHandlers[index];
    context = EventHandler->Context;
    if (STAILQ_EMPTY(&context->ListEventHandlerSubscribers)) {
      ret = E_NOT_OK;
    } else {
      *list = &context->ListEventHandlerSubscribers;
    }
  } else {
    ret = E_NOT_OK;
  }
  return ret;
}

void Sd_RemoveSubscriber(uint16_t eventHandlerId, PduIdType txPduId) {
  uint16_t index;
  const Sd_ServerServiceType *config;
  const Sd_EventHandlerType *EventHandler;
  Sd_EventHandlerContextType *context;
  DEC_SQP(EventHandlerSubscriber);
  if (eventHandlerId < SD_CONFIG->NumOfEventHandlers) {
    index = SD_CONFIG->EventHandlersMap[eventHandlerId];
    config = SD_CONFIG->ServerServicesMap[index];
    index = SD_CONFIG->PerServiceEventHandlerMap[eventHandlerId];
    EventHandler = &config->EventHandlers[index];
    context = EventHandler->Context;
    SQP_WHILE(EventHandlerSubscriber) {
      if (var->TxPduId == txPduId) {
        EventHandler->OnSubscribe(FALSE, &var->RemoteAddr);
        SQP_CRM_AND_FREE(EventHandlerSubscriber, var);
      }
    }
    SQP_WHILE_END()
  }
}

void Sd_NotifyServiceOffline(uint16_t sdServerServiceHandleId) {
  const Sd_ServerServiceType *config;
  Sd_ServerServiceContextType *context;
  if (sdServerServiceHandleId < SD_CONFIG->NumOfServerServices) {
    config = SD_CONFIG->ServerServicesMap[sdServerServiceHandleId];
    context = config->Context;
    if (0 != (SD_FLG_LINK_UP & context->Flags)) {
      ASLOG(SDE, ("SOCK[%u] server service %x:-:%x offline unexpected\n", config->SoConId,
                  config->ServiceId, config->InstanceId));
      SD_CLEAR(context->Flags, SD_FLG_LINK_UP);
      Sd_ServerServiceLinkControl(config);
    }
  }
}

Std_ReturnType Sd_GetProviderAddr(uint16_t clientServiceHandleId, TcpIp_SockAddrType *remoteAddr) {
  const Sd_ClientServiceType *config;
  Sd_ClientServiceContextType *context;
  Std_ReturnType ret = E_OK;
  if (clientServiceHandleId < SD_CONFIG->NumOfClientServices) {
    config = SD_CONFIG->ClientServicesMap[clientServiceHandleId];
    context = config->Context;
    if ((context->IsOffered) && (context->Flags & SD_FLG_LINK_UP)) {
      *remoteAddr = context->RemoteAddr;
    } else {
      ret = E_NOT_OK;
    }
  } else {
    ret = E_NOT_OK;
  }
  return ret;
}

Std_ReturnType Sd_HeaderIndication(PduIdType rxPduId, const PduInfoType *info,
                                   uint32_t *payloadLength) {
  Std_ReturnType ret = E_OK;
  uint8_t *data = info->SduDataPtr;
  (void)rxPduId;

  if (info->SduLength < 8u) {
    ASLOG(SDE, ("malformed SD message\n"));
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    if ((0xFFu != data[0]) || (0xFFu != data[1]) || (0x81u != data[2]) || (0x00u != data[3])) {
      ASLOG(SDE, ("invalid SD message ID\n"));
      ret = E_NOT_OK;
    } else {
      *payloadLength =
        ((uint32_t)data[4] << 24) + ((uint32_t)data[5] << 16) + ((uint32_t)data[6] << 8) + data[7];
    }
  }

  return ret;
}

void Sd_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x02, SD_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_SD;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 0;
}
