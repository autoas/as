/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef DOIP_CLIENT_H
#define DOIP_CLIENT_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define DOIP_E_OK 0
#define DOIP_E_NOT_OK -1
#define DOIP_E_NEGATIVE -2
#define DOIP_E_INVAL -3
#define DOIP_E_TOO_LONG -4
#define DOIP_E_TIMEOUT -5
#define DOIP_E_AGAIN -6
#define DOIP_E_NODEV -7
#define DOIP_E_ACCES -8
#define DOIP_E_NOSPC -9
#define DOIP_E_NOMEM -10

#define DOIP_E_OK_SILENT -999
/* ================================ [ TYPES     ] ============================================== */
typedef struct DoIp_Client_s DoIp_ClientType;
typedef struct {
  uint8_t VIN[17];
  uint16_t LA;
  uint8_t EID[6];
  uint8_t GID[6];
} DoIp_NodeType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
DoIp_ClientType *DoIp_CreateClient(const char *ip, int port, const char *casPem);
int DoIp_AwaitVehicleAnnouncement(DoIp_ClientType *client, DoIp_NodeType **nodes, int numOfNodes,
                                  uint32_t timeoutMs);
DoIp_NodeType *DoIp_Request(DoIp_ClientType *client);
int DoIp_Connect(DoIp_NodeType *node);
int DoIp_Activate(DoIp_NodeType *node, uint16_t sa, uint8_t at, uint8_t *oem, uint8_t oemLen);
int DoIp_Transmit(DoIp_NodeType *node, uint16_t ta, const uint8_t *txBuffer, size_t txSize,
                  uint8_t *rxBuffer, size_t rxSize);
int DoIp_Receive(DoIp_NodeType *node, uint8_t *rxBuffer, size_t rxSize);
void DoIp_DestroyClient(DoIp_ClientType *client);
#ifdef __cplusplus
}
#endif
#endif /* DOIP_CLIENT_H */
