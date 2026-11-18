/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 * ref https://wiki.wireshark.org/Development/LibpcapFileFormat
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "pcap.h"
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
/* ================================ [ MACROS    ] ============================================== */
#define SOMEIP_MSG_REQUEST 0x00
#define SOMEIP_MSG_REQUEST_NO_RETURN 0x01
#define SOMEIP_MSG_NOTIFICATION 0x02
#define SOMEIP_MSG_RESPONSE 0x80
#define SOMEIP_MSG_ERROR 0x81

#define PCAP_SOMEIP_TP_FLAG 0x20

#define PCAP_SD_FIND_SERVICE 0x00
#define PCAP_SD_OFFER_SERVICE 0x01
#define PCAP_SD_SUBSCRIBE_EVENT_GROUP 0x06
#define PCAP_SD_SUBSCRIBE_EVENT_GROUP_ACK 0x07

#define PCAP_SD_OPT_IP4_ENDPOINT 0x04
#define PCAP_SD_OPT_IP4_MULTICAST 0x14

#define PCAP_SD_FLAG_MASK 0xC0u
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static FILE *pcapLogFile = NULL;
static FILE *pcapWireFile = NULL;
/* ================================ [ LOCALS    ] ============================================== */
static void PCap_Stop(void) {
  if (NULL != pcapLogFile) {
    fclose(pcapLogFile);
  }
  if (NULL != pcapWireFile) {
    fclose(pcapWireFile);
  }
}

static void __attribute__((constructor)) PCap_Start(void) {
  char path[256];
  char *name = getenv("PCAP_PATH");

  if (NULL == name) {
    snprintf(path, sizeof(path), "net.log");
  } else {
    snprintf(path, sizeof(path), "%s.log", name);
  }
  pcapLogFile = fopen(path, "wb");

  if (NULL == name) {
    snprintf(path, sizeof(path), "wireshark.pcap");
  } else {
    snprintf(path, sizeof(path), "%s.pcap", name);
  }
  pcapWireFile = fopen(path, "wb");
  if (NULL != pcapWireFile) {
    static const uint8_t globalHeader[] = {
      0xD4, 0xC3, 0xB2, 0xA1, /* magic number */
      0x02, 0x00, 0x04, 0x00, /* major/minor version number: 2.4 */
      0x00, 0x00, 0x00, 0x00, /* GMT to local correction */
      0x00, 0x00, 0x00, 0x00, /* accuracy of timestamps */
      0xFF, 0xFF, 0x00, 0x00, /* max length of captured packets, in octets */
      0x01, 0x00, 0x00, 0x00, /* data link type: LINKTYPE_ETHERNET */
    };
    fwrite(globalHeader, sizeof(globalHeader), 1, pcapWireFile);
  }

  if ((NULL != pcapLogFile) || (NULL != pcapWireFile)) {
    atexit(PCap_Stop);
  }
}

static float PCap_GetRelTime(void) {
  static struct timeval m0 = {-1, -1};
  struct timeval m1;
  float rtim;

  if ((-1 == m0.tv_sec) && (-1 == m0.tv_usec)) {
    gettimeofday(&m0, NULL);
  }
  gettimeofday(&m1, NULL);
  rtim = m1.tv_sec - m0.tv_sec;
  if (m1.tv_usec > m0.tv_usec) {
    rtim += (float)(m1.tv_usec - m0.tv_usec) / 1000000.0;
  } else {
    rtim = rtim - 1 + (float)(1000000.0 + m1.tv_usec - m0.tv_usec) / 1000000.0;
  }

  return rtim;
}

static boolean PCap_InvalidSd(char *errMsg, uint8_t *data, uint32_t length,
                              const TcpIp_SockAddrType *remoteAddr, boolean isRx) {
  uint32_t i;
  float rtim = PCap_GetRelTime();
  fprintf(pcapLogFile, "\n%.4f: invalid(%s) SD %s %d.%d.%d.%d:%d\n  data(%u) = [", rtim,
          isRx ? "from" : "to", errMsg, remoteAddr->Addr[0], remoteAddr->Addr[1],
          remoteAddr->Addr[2], remoteAddr->Addr[3], remoteAddr->Port, length);
  for (i = 0; i < length; i++) {
    fprintf(pcapLogFile, "%02x", data[i]);
  }
  fprintf(pcapLogFile, "]\n");

  return FALSE;
}

static boolean PCap_ValidateSd(uint8_t *data, uint32_t length, const TcpIp_SockAddrType *remoteAddr,
                               boolean isRx) {
  boolean good = TRUE;
  if (length < 28) {
    good = PCap_InvalidSd("length", data, length, remoteAddr, isRx);
  }

  if (good) {
    if ((0xFF != data[0]) || (0xFF != data[1]) || (0x81 != data[2]) || (0x00 != data[3])) {
      good = PCap_InvalidSd("message id", data, length, remoteAddr, isRx);
    }
  }

  if (good) {
    if ((0x00 != data[8]) || (0x00 != data[9])) {
      good = PCap_InvalidSd("client id", data, length, remoteAddr, isRx);
    }
  }

  if (good) {
    if ((0x01 != data[12]) || (0x01 != data[13]) || (0x02 != data[14]) || (0x00 != data[15])) {
      good = PCap_InvalidSd("version type", data, length, remoteAddr, isRx);
    }
  }

  if (good) {
    if ((0x00 != data[17]) || (0x00 != data[18]) || (0x00 != data[19])) {
      good = PCap_InvalidSd("reservdata", data, length, remoteAddr, isRx);
    }
  }
  if (good) {
    if (0 != (data[16] & (~PCAP_SD_FLAG_MASK))) {
      good = PCap_InvalidSd("flags", data, length, remoteAddr, isRx);
    }
  }

  return good;
}

uint32_t PCap_SdEntry1(char *ts, uint8_t *data) {
  uint8_t indexOf1st = data[1];
  uint8_t indexOf2nd = data[2];
  uint8_t numOfOpt1 = (data[3] >> 4) & 0xF;
  uint8_t numOfOpt2 = data[3] & 0xF;
  uint16_t serviceId = ((uint16_t)data[4] << 8) + data[5];
  uint16_t instanceId = ((uint16_t)data[6] << 8) + data[7];
  uint8_t major = data[8];
  uint32_t TTL = ((uint32_t)data[9] << 16) + ((uint32_t)data[10] << 8) + data[11];
  uint32_t minor =
    ((uint32_t)data[12] << 24) + ((uint32_t)data[13] << 16) + ((uint32_t)data[14] << 8) + data[15];

  if ((PCAP_SD_OFFER_SERVICE == data[0]) && (0 == TTL)) {
    ts = "stop offer";
  }

  if ((PCAP_SD_SUBSCRIBE_EVENT_GROUP == data[0]) && (0 == TTL)) {
    ts = "stop subscribe";
  }

  fprintf(pcapLogFile,
          "  %s: service:instance %x:%x version %d.%u TTL %u opt1<@%d, #%d> opt2<@%d, #%d>\n", ts,
          serviceId, instanceId, major, minor, TTL, indexOf1st, numOfOpt1, indexOf2nd, numOfOpt2);
  return 16;
}

uint32_t PCap_SdEntry2(char *ts, uint8_t *data) {
  uint8_t indexOf1st = data[1];
  uint8_t indexOf2nd = data[2];
  uint8_t numOfOpt1 = (data[3] >> 4) & 0xF;
  uint8_t numOfOpt2 = data[3] & 0xF;
  uint16_t serviceId = ((uint16_t)data[4] << 8) + data[5];
  uint16_t instanceId = ((uint16_t)data[6] << 8) + data[7];
  uint8_t major = data[8];
  uint32_t TTL = ((uint32_t)data[9] << 16) + ((uint32_t)data[10] << 8) + data[11];
  uint8_t counter = data[13] & 0x0F;
  uint16_t eventGroupId = ((uint16_t)data[14] << 8) + data[15];
  if (0 == TTL) {
    if (PCAP_SD_SUBSCRIBE_EVENT_GROUP_ACK == data[0]) {
      ts = "subscribe nack";
    } else if (PCAP_SD_SUBSCRIBE_EVENT_GROUP == data[0]) {
      ts = "stop subscribe";
    } else {
    }
  }
  fprintf(pcapLogFile,
          "  %s: service:instance:group %x:%x:%x version %d TTL %u counter %d opt1<@%d, #%d> "
          "opt2<@%d, #%d>\n",
          ts, serviceId, instanceId, eventGroupId, major, TTL, counter, indexOf1st, numOfOpt1,
          indexOf2nd, numOfOpt2);
  return 16;
}

uint32_t PCap_SdOptionIpv4(char *ts, uint8_t *data) {
  char tmpStr[32];
  const char *proStr;
  uint16_t length = ((uint16_t)data[0] << 8) + data[1];
  uint16_t port = ((uint16_t)data[10] << 8) + data[11];
  uint8_t protocol = data[9];
  if (TCPIP_IPPROTO_TCP == protocol) {
    proStr = "TCP";
  } else if (TCPIP_IPPROTO_UDP == protocol) {
    proStr = "UDP";
  } else {
    snprintf(tmpStr, sizeof(tmpStr), "protocol(%x)", protocol);
    proStr = tmpStr;
  }

  fprintf(pcapLogFile, "  %s IPv4 %s %d.%d.%d.%d:%d\n", ts, proStr, data[4], data[5], data[6],
          data[7], port);
  if (9 != length) {
    fprintf(pcapLogFile, "    Error: length %d != 9\n", length);
  }
  return 12;
}

/* ================================ [ FUNCTIONS ] ============================================== */
void PCap_SomeIp(uint16_t serviceId, uint16_t methodId, uint8_t interfaceVersion,
                 uint8_t messageType, uint8_t returnCode, uint8_t *payload, uint32_t payloadLength,
                 uint16_t clientId, uint16_t sessionId, const TcpIp_SockAddrType *remoteAddr,
                 boolean isTp, uint32_t offset, boolean more, boolean isRx) {
  char typeStr[32];
  const char *type;
  uint32_t i;
  uint32_t length;
  float rtim;

  if (pcapLogFile) {
    rtim = PCap_GetRelTime();
    if (messageType & PCAP_SOMEIP_TP_FLAG) {
      messageType &= ~PCAP_SOMEIP_TP_FLAG;
      isTp = TRUE;
      length = ((uint32_t)payload[0] << 24) + ((uint32_t)payload[1] << 16) +
               ((uint32_t)payload[2] << 8) + payload[3];
      offset = length & 0xFFFFFFF0;
      more = length & 1;
      payload = &payload[4];
      payloadLength = payloadLength - 4;
    }
    switch (messageType) {
    case SOMEIP_MSG_REQUEST:
      type = "request";
      break;
    case SOMEIP_MSG_REQUEST_NO_RETURN:
      type = "fire&forgot";
      break;
    case SOMEIP_MSG_NOTIFICATION:
      type = "notification";
      break;
    case SOMEIP_MSG_RESPONSE:
      type = "response";
      break;
    case SOMEIP_MSG_ERROR:
      type = "error";
      break;
    default:
      snprintf(typeStr, sizeof(typeStr), "unknown<0x%x>", messageType);
      type = typeStr;
      break;
    }

    if (remoteAddr) {
      fprintf(pcapLogFile, "\n%.4f: SOMEIP %s %d.%d.%d.%d:%d\n", rtim, isRx ? "from" : "to",
              remoteAddr->Addr[0], remoteAddr->Addr[1], remoteAddr->Addr[2], remoteAddr->Addr[3],
              remoteAddr->Port);
    } else {
      fprintf(pcapLogFile, "\n%.4f: SOMEIP %s null\n", rtim, isRx ? "from" : "to");
    }
    fprintf(
      pcapLogFile,
      "  %s service:method:version %x:%x:%x session %d client %x return code %d payload %u bytes",
      type, serviceId, methodId, interfaceVersion, sessionId, clientId, returnCode, payloadLength);
    if (isTp) {
      fprintf(pcapLogFile, " TP offset=%d more=%d", offset, more);
    }
    fprintf(pcapLogFile, "\n  payload = [");
    length = 32;
    if (length > payloadLength) {
      length = payloadLength;
    }
    for (i = 0; i < length; i++) {
      fprintf(pcapLogFile, "%02x", payload[i]);
    }
    fprintf(pcapLogFile, "]\n");
  }
}

void PCap_SD(uint8_t *data, uint32_t length, const TcpIp_SockAddrType *remoteAddr, boolean isRx) {
  boolean good = TRUE;
  uint16_t sessionId;
  uint32_t i;
  uint32_t payloadLength;
  uint32_t lengthOfEntries;
  uint32_t lengthOfOptions;
  uint8_t flags;
  uint8_t *entries;
  uint8_t *options;
  float rtim;
  static const TcpIp_SockAddrType broadcast = {30490, {224, 244, 224, 245}};
  if (NULL == remoteAddr) {
    remoteAddr = &broadcast;
  }

  good = PCap_ValidateSd(data, length, remoteAddr, isRx);

  if (good) {
    sessionId = ((uint16_t)data[10] << 8) + data[11];
    flags = data[16];
    payloadLength =
      ((uint32_t)data[4] << 24) + ((uint32_t)data[5] << 16) + ((uint32_t)data[6] << 8) + data[7];
    if ((payloadLength + 8) != length) {
      good = PCap_InvalidSd("payload length", data, length, remoteAddr, isRx);
    } else {
      lengthOfEntries = ((uint32_t)data[20] << 24) + ((uint32_t)data[21] << 16) +
                        ((uint32_t)data[22] << 8) + data[23];
      if ((28 + lengthOfEntries) <= length) {
        lengthOfOptions = ((uint32_t)data[24 + lengthOfEntries] << 24) +
                          ((uint32_t)data[25 + lengthOfEntries] << 16) +
                          ((uint32_t)data[26 + lengthOfEntries] << 8) + data[27 + lengthOfEntries];
        if ((28 + lengthOfEntries + lengthOfOptions) != length) {
          good = PCap_InvalidSd("options length", data, length, remoteAddr, isRx);
        }
      } else {
        good = PCap_InvalidSd("entries length", data, length, remoteAddr, isRx);
      }
    }
  }

  if (good) {
    rtim = PCap_GetRelTime();
    fprintf(
      pcapLogFile, "\n%.4f: SD %s %d.%d.%d.%d:%d session %d flags %x entries(%u) options(%u)\n",
      rtim, isRx ? "from" : "to", remoteAddr->Addr[0], remoteAddr->Addr[1], remoteAddr->Addr[2],
      remoteAddr->Addr[3], remoteAddr->Port, sessionId, flags, lengthOfEntries, lengthOfOptions);
    entries = &data[24];
    for (i = 0; (i < lengthOfEntries) && good;) {
      switch (entries[i]) {
      case PCAP_SD_FIND_SERVICE:
        i += PCap_SdEntry1("find", &entries[i]);
        break;
      case PCAP_SD_OFFER_SERVICE:
        i += PCap_SdEntry1("offer", &entries[i]);
        break;
      case PCAP_SD_SUBSCRIBE_EVENT_GROUP:
        i += PCap_SdEntry2("subscribe", &entries[i]);
        break;
      case PCAP_SD_SUBSCRIBE_EVENT_GROUP_ACK:
        i += PCap_SdEntry2("subscribe ack", &entries[i]);
        break;
      default:
        fprintf(pcapLogFile, "  unknown entry type %x\n", entries[i]);
        good = FALSE;
        break;
      }
    }

    options = &data[28 + lengthOfEntries];
    for (i = 0; (i < lengthOfOptions) && good;) {
      switch (options[i + 2]) {
      case PCAP_SD_OPT_IP4_ENDPOINT:
        i += PCap_SdOptionIpv4("endpoint", &options[i]);
        break;
      case PCAP_SD_OPT_IP4_MULTICAST:
        i += PCap_SdOptionIpv4("multicast", &options[i]);
        break;
      default:
        fprintf(pcapLogFile, "  unknown option type %x\n", options[i + 2]);
        good = FALSE;
        break;
      }
    }
  }
}

void PCap_Packet(const void *packet, uint32_t length) {
  uint8_t pcaprecHdr[16];
  float rtim = PCap_GetRelTime();
  uint32_t ts_sec = (uint32_t)rtim;
  uint32_t ts_usec = (uint32_t)((rtim - ts_sec) * 1000000);

  pcaprecHdr[0] = ts_sec & 0xFF;
  pcaprecHdr[1] = (ts_sec >> 8) & 0xFF;
  pcaprecHdr[2] = (ts_sec >> 16) & 0xFF;
  pcaprecHdr[3] = (ts_sec >> 24) & 0xFF;

  pcaprecHdr[4] = ts_usec & 0xFF;
  pcaprecHdr[5] = (ts_usec >> 8) & 0xFF;
  pcaprecHdr[6] = (ts_usec >> 16) & 0xFF;
  pcaprecHdr[7] = (ts_usec >> 24) & 0xFF;

  pcaprecHdr[8] = length & 0xFF;
  pcaprecHdr[9] = (length >> 8) & 0xFF;
  pcaprecHdr[10] = (length >> 16) & 0xFF;
  pcaprecHdr[11] = (length >> 24) & 0xFF;

  pcaprecHdr[12] = length & 0xFF;
  pcaprecHdr[13] = (length >> 8) & 0xFF;
  pcaprecHdr[14] = (length >> 16) & 0xFF;
  pcaprecHdr[15] = (length >> 24) & 0xFF;

  fwrite(pcaprecHdr, sizeof(pcaprecHdr), 1, pcapWireFile);
  fwrite(packet, length, 1, pcapWireFile);
}
