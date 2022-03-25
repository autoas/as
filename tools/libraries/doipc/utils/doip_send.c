/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "doip_client.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static void usage(char *prog) {
  printf("usage: %s -v AABBCCDDEEFF.. [-i UDP_TEST_EQUIPMENT_REQUEST] "
         "[-p port] [-s sa] [-a activation type] [-t target address]\n",
         prog);
}

static void vlog(const char *prefix, uint8_t *data, int length) {
  int i;
  printf(prefix);
  for (i = 0; i < length; i++) {
    printf("%02X ", data[i]);
  }
  printf("\t");
  for (i = 0; i < length; i++) {
    if (isprint(data[i])) {
      printf("%c", data[i]);
    } else {
      printf(".");
    }
  }
  printf("\n");
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  int r = 0;
  int ch;
  char *ip = "224.244.224.245";
  int port = 13400;
  doip_client_t *client;
  doip_node_t *node;
  uint16_t sa = 0xbeef;
  uint8_t at = 0xda;
  uint16_t ta = 0xdead;
  int length = 0;
  uint8_t data[4095];
  char bytes[3] = {0, 0, 0};
  int i;

  opterr = 0;
  while ((ch = getopt(argc, argv, "a:hi:p:s:t:v:")) != -1) {
    switch (ch) {
    case 'a':
      at = strtoul(optarg, NULL, 16);
      break;
    case 'h':
      usage(argv[0]);
      return 0;
      break;
    case 'i':
      ip = optarg;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 's':
      sa = strtoul(optarg, NULL, 16);
      break;
    case 't':
      ta = strtoul(optarg, NULL, 16);
      break;
    case 'v':
      length = strlen(optarg) / 2;
      for (i = 0; (i < length) && (i < sizeof(data)); i++) {
        bytes[0] = optarg[2 * i];
        bytes[1] = optarg[2 * i + 1];
        data[i] = strtoul(bytes, NULL, 16);
      }
      break;
    default:
      break;
    }
  }
  if ((NULL == ip) || (port < 0) || (0 == length)) {
    usage(argv[0]);
    return -1;
  }

  client = doip_create_client(ip, port);

  if (NULL == client) {
    printf("Failed to clreate doip client <%s:%d>\n", ip, port);
    return -2;
  }

  r = doip_await_vehicle_announcement(client, &node, 1, 3000);

  if (r <= 0) {
    node = doip_request(client);
    if (NULL == node) {
      r = -1;
      printf("target is not reachable\n");
    } else {
      r = 0;
    }
  } else {
    r = 0;
  }

  if (0 == r) {
    printf("Found Vehicle:\n");
    vlog("  VIN: ", node->VIN, 17);
    vlog("  EID: ", node->EID, 6);
    vlog("  GID: ", node->GID, 6);
    printf("  LA: %X\n", node->LA);
    r = doip_connect(node);
    if (0 != r) {
      printf("connect failed\n");
    }
  }

  if (0 == r) {
    r = doip_activate(node, sa, at, NULL, 0);
    if (0 != r) {
      printf("activate failed\n");
    }
  }

  if (0 == r) {
    printf("TX: ");
    for (i = 0; i < length; i++) {
      printf("%02X ", data[i]);
    }
    printf("\n");
    r = doip_transmit(node, ta, data, length, data, sizeof(data));

    if (r > 0) {
      printf("RX: ");
      for (i = 0; i < r; i++) {
        printf("%02X ", data[i]);
      }
      printf("\n");
    } else {
      printf("failed with error %d\n", r);
    }
  }

  doip_destory_client(client);

  return r;
}
