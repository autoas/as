/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#ifdef _WIN32
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <sys/types.h>
#include "devlib.h"
#include "devlib_priv.h"
#include "rs232.h"
#include <stdlib.h>
#include <string.h>
#ifdef USE_STD_PRINTF
#undef USE_STD_PRINTF
#endif
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define PPARAM(p) ((Dev_RS232ParamType *)p)
#define AS_LOG_DEV 0
/* QEMU TCP 127.0.0.1:1103 't' = 0x74, 'c' = 0x63, 'p' = 0x70 */
#define CAN_TCP_SERIAL_PORT 0x746370
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  int port;
  int s;
} Dev_RS232ParamType;
/* ================================ [ DECLARES  ] ============================================== */
static int dev_rs232_open(const char *device, const char *option, void **param);
static int dev_rs232_read(void *param, uint8_t *data, size_t size);
static int dev_rs232_write(void *param, const uint8_t *data, size_t size);
static void dev_rs232_close(void *param);
static int dev_rs232_ioctl(void *param, int type, const void *data, size_t size);
/* ================================ [ DATAS     ] ============================================== */
const Dev_DeviceOpsType rs232_dev_ops = {
  .name = "COM",
  .open = dev_rs232_open,
  .read = dev_rs232_read,
  .write = dev_rs232_write,
  .close = dev_rs232_close,
  .ioctl = dev_rs232_ioctl,
};
/* ================================ [ LOCALS    ] ============================================== */
static int dev_rs232_open(const char *device, const char *option, void **param) {
  const char *modes;
  int baudrate, port;
  int s, r = 0;

  if (0 == strcmp(device, "COMTCP")) {
    port = CAN_TCP_SERIAL_PORT;
  } else {
    port = atoi(&device[3]) - 1;
  }

  *param = malloc(sizeof(Dev_RS232ParamType));

  PPARAM(*param)->port = port;

  /* option format "baudrate\0modes", e.g. "115200\08N1" */
  baudrate = atoi(option);
  modes = option + strlen(option) + 1;
  ASLOG(DEV, ("rs232 open(%d, %d, %s)\n", port, baudrate, modes));

  if (CAN_TCP_SERIAL_PORT == port) {
    struct sockaddr_in addr;
#ifdef _WIN32
    {
      WSADATA wsaData;
      WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
#endif

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(1103);

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      ASLOG(ERROR, ("Serial TCP open failed!\n"));
      r = s;
    }

    if (0 == r) {
      /* Connect to serer. */
      if ((r = connect(s, (struct sockaddr *)&addr, sizeof(addr))) < 0) {
#ifdef _WIN32
        ASLOG(ERROR, ("Serial TCP connect failed: %d\n", WSAGetLastError()));
        closesocket(s);
#else
        ASLOG(ERROR, ("Serial TCP connect failed!\n"));
        close(s);
#endif
      }
    }

    if (0 == r) {
      PPARAM(*param)->s = s;
    }
  } else if (0 == RS232_OpenComport(port, baudrate, modes, 0)) {
    r = -__LINE__;
  } else {
    r = -__LINE__;
  }

  return r;
}

static int dev_rs232_read(void *param, uint8_t *data, size_t size) {
  int len;

  if (CAN_TCP_SERIAL_PORT == PPARAM(param)->port) {
    len = recv(PPARAM(param)->s, (char *)data, size, 0);
  } else {
    len = RS232_PollComport(PPARAM(param)->port, data, size);
  }

  ASLOG(DEV, ("rs232 %d = read(%d)\n", (int)len, (int)size));

  return len;
}
static int dev_rs232_write(void *param, const uint8_t *data, size_t size) {
  int r = 0;

  if (CAN_TCP_SERIAL_PORT == PPARAM(param)->port) {
    r = send(PPARAM(param)->s, (const char *)data, size, 0);
    if (size == (size_t)r) {
      /* send OK */
    } else {
      ASLOG(ERROR, ("Serial TCP send message failed!\n"));
    }
  } else {
    r = RS232_SendBuf(PPARAM(param)->port, (unsigned char *)data, size);
  }

  return r;
}

static void dev_rs232_close(void *param) {
  if (CAN_TCP_SERIAL_PORT == PPARAM(param)->port) {
#ifdef _WIN32
    closesocket(PPARAM(param)->s);
#else
    close(PPARAM(param)->s);
#endif
  } else {
    RS232_CloseComport(PPARAM(param)->port);
  }
  free(param);
}

static int dev_rs232_ioctl(void *param, int type, const void *data, size_t size) {
  (void)param;
  (void)type;
  (void)data;
  return 0;
}
/* ================================ [ FUNCTIONS ] ============================================== */
