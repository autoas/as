/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef _DOIP_CLIENT_H
#define _DOIP_CLIENT_H
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
typedef struct doip_client_s doip_client_t;
typedef struct {
  uint8_t VIN[17];
  uint16_t LA;
  uint8_t EID[6];
  uint8_t GID[6];
} doip_node_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
doip_client_t *doip_create_client(const char *ip, int port);
int doip_await_vehicle_announcement(doip_client_t *client, doip_node_t **nodes, int numOfNodes,
                                    uint32_t timeout /* unit ms */);
doip_node_t *doip_request(doip_client_t *client);
int doip_connect(doip_node_t *node);
int doip_activate(doip_node_t *node, uint16_t sa, uint8_t at, uint8_t *oem, uint8_t oem_len);
int doip_transmit(doip_node_t *node, uint16_t ta, const uint8_t *txBuffer, size_t txSize, uint8_t *rxBuffer,
                  size_t rxSize);
int doip_receive(doip_node_t *node, uint8_t *rxBuffer, size_t rxSize);
void doip_destory_client(doip_client_t *client);
#ifdef __cplusplus
}
#endif
#endif /* _DOIP_CLIENT_H */
