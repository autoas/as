/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2019 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "device.h"
#include <string.h>
#include "Std_Debug.h"
#ifdef USE_SHELL
#include "shell.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
#if defined(_WIN32) || defined(linux)
device_t __devtab_start[1024];
device_t *__devtab_end = &__devtab_start[0];
#else
extern const device_t __devtab_start[];
extern const device_t __devtab_end[];
#endif
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#if defined(_WIN32) || defined(linux)
void device_register(const device_t *device) {
  int number = __devtab_end - __devtab_start;
  if (number < ARRAY_SIZE(__devtab_start)) {
    __devtab_start[number] = *device;
    __devtab_end = &__devtab_start[number + 1];
  }
}
#endif

const device_t *device_find(const char *name) {
  const device_t *devIt;
  const device_t *device = NULL;
  for (devIt = __devtab_start; (devIt < __devtab_end) && (NULL == device); devIt++) {
    if (0 == strcmp(devIt->name, name)) {
      device = devIt;
    }
  }

  return device;
}

#ifdef USE_SHELL
int lsdevFunc(int argc, const char *argv[]) {
  static const char deviceTypeMap[] = {'b', 'n', 'c'};
  const device_t *devIt;
  size_t sz = 0;
  for (devIt = __devtab_start; devIt < __devtab_end; devIt++) {
    if (devIt->type == DEVICE_TYPE_BLOCK) {
      devIt->ops.ctrl(devIt, DEVICE_CTRL_GET_DISK_SIZE, &sz);
    }
    PRINTF("%crw-rw-rw- 1 as vfs %11u %s\r\n", deviceTypeMap[devIt->type], (uint32_t)sz,
           devIt->name);
  }
  return 0;
}

SHELL_REGISTER(lsdev, "ls - list devices\n", lsdevFunc);
#endif
