/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "devlib.h"
#include "devlib_priv.hpp"
#ifdef USE_STD_PRINTF
#undef USE_STD_PRINTF
#endif
#include "Std_Debug.h"
#include "Std_Types.h"
#include <sys/queue.h>
#include <mutex>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DEV 0
#define AS_LOG_DEVI 0
#define AS_LOG_DEVE 2
/* ================================ [ TYPES     ] ============================================== */
typedef struct Dev_s Dev_DeviceType;

struct Dev_s {
  std::string Name;
  std::string Option;
  int Fd;
  void *Param; /* should be filled by device control module while open */
  const Dev_DeviceOpsType *Ops;
  uint32_t Size2;
  STAILQ_ENTRY(Dev_s) Entry;
  std::mutex Lock;
  uint32_t Ref;
};

typedef struct Dev_List_s Dev_ListType;

struct Dev_List_s {
  boolean IsInitialized;
  std::recursive_mutex QLock;
  STAILQ_HEAD(, Dev_s) Head;
};
/* ================================ [ DECLARES  ] ============================================== */
extern "C" const Dev_DeviceOpsType Dev_Rs232Ops;
extern "C" const Dev_DeviceOpsType Dev_LinOps;
#ifdef _WIN32
extern "C" const Dev_DeviceOpsType Dev_Ch341I2cOps;
extern "C" const Dev_DeviceOpsType Dev_Ch341SpiOps;
#endif
/* ================================ [ DATAS     ] ============================================== */
static const Dev_DeviceOpsType *devOps[] = {
  &Dev_Rs232Ops,
  &Dev_LinOps,
#ifdef _WIN32
  &Dev_Ch341I2cOps,
  &Dev_Ch341SpiOps,
#endif
  nullptr,
};
static int devFd = 0; /* file identifier start from 0 */
static Dev_ListType devListHead = {
  .IsInitialized = FALSE,
};
/* ================================ [ LOCALS    ] ============================================== */
static Dev_DeviceType *Dev_GetDevice(const char *name) {
  Dev_DeviceType *handle, *h;
  handle = nullptr;

  std::lock_guard<std::recursive_mutex> lg(devListHead.QLock);
  STAILQ_FOREACH(h, &devListHead.Head, Entry) {
    if (0u == strcmp(h->Name.c_str(), name)) {
      handle = h;
      break;
    }
  }

  return handle;
}

static Dev_DeviceType *Dev_GetDeviceByFd(int fd) {
  Dev_DeviceType *handle, *h;
  handle = nullptr;

  std::lock_guard<std::recursive_mutex> lg(devListHead.QLock);
  STAILQ_FOREACH(h, &devListHead.Head, Entry) {
    if (fd == h->Fd) {
      handle = h;
      break;
    }
  }

  return handle;
}
static const Dev_DeviceOpsType *Dev_SearchOps(const char *name) {
  const Dev_DeviceOpsType *ops, **o;
  o = devOps;
  ops = nullptr;
  while (*o != nullptr) {
    if (name == strstr(name, (*o)->Name.c_str())) {
      ops = *o;
      break;
    }
    o++;
  }

  return ops;
}
static void Dev_FreeDevice(Dev_ListType *h) {
  Dev_DeviceType *d;

  std::lock_guard<std::recursive_mutex> lg(h->QLock);
  while (FALSE == STAILQ_EMPTY(&h->Head)) {
    d = STAILQ_FIRST(&h->Head);
    STAILQ_REMOVE_HEAD(&h->Head, Entry);
    d->Ops->Close(d->Param);
    delete d;
  }
}

void Dev_CloseAll(void) {
  if (devListHead.IsInitialized) {
    devListHead.IsInitialized = FALSE;
    Dev_FreeDevice(&devListHead);
  }
}

INITIALIZER(Dev_Init) {
  STAILQ_INIT(&devListHead.Head);
  devListHead.IsInitialized = TRUE;
  atexit(Dev_CloseAll);
}
/* ================================ [ FUNCTIONS ] ============================================== */
int Dev_Open(const char *device_name, const char *option) {
  const Dev_DeviceOpsType *ops;
  Dev_DeviceType *d;
  int rv;

  std::lock_guard<std::recursive_mutex> lg(devListHead.QLock);
  d = Dev_GetDevice(device_name);
  if (nullptr != d) {
    if (option != nullptr) {
      if (0 == strcmp(option, d->Option.c_str())) {
        rv = d->Fd;
        d->Ref++;
      } else {
        ASLOG(DEVE, ("device(%s) reopen with different option %s != %s\n", device_name, option,
                     d->Option.c_str()));
        rv = -__LINE__;
      }
    } else {
      rv = d->Fd;
    }
  } else {
    ops = Dev_SearchOps(device_name);
    if (nullptr != ops) {
      d = new Dev_DeviceType;
      if (d != nullptr) {
        d->Name = device_name;
        if (option != nullptr) {
          d->Option = option;
        }

        rv = ops->Open(device_name, option, &d->Param);
      } else {
        rv = -__LINE__;
      }

      if (rv >= 0) {
        d->Fd = devFd++;
        d->Ops = ops;
        d->Ref = 1;
        STAILQ_INSERT_TAIL(&devListHead.Head, d, Entry);
        rv = d->Fd;
      } else {
        if (d) {
          delete d;
        }
        ASLOG(DEVE, ("%s device <%s> failed!\n", __func__, device_name));
      }
    } else {
      ASLOG(DEVE, ("%s device <%s> is not known by devlib!\n", __func__, device_name));
      rv = -__LINE__;
    }
  }

  if (rv >= 0) {
    ASLOG(DEVI,
          ("device(%s) open with option %s as fd=%d ref=%d\n", device_name, option, d->Fd, d->Ref));
  }

  return rv;
}

int Dev_Write(int fd, const uint8_t *data, size_t size) {
  int rv;
  Dev_DeviceType *d;
  d = Dev_GetDeviceByFd(fd);
  if (nullptr == d) {
    ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    rv = -__LINE__;
  } else if (d->Ops->Write != nullptr) {
    rv = d->Ops->Write(d->Param, data, size);

    if (rv < 0) {
      ASLOG(DEVE, ("%s write on device %s failed(%d)\n", __func__, d->Name.c_str(), rv));
    }
  } else {
    ASLOG(DEVE, ("%s for %s is not supported\n", __func__, d->Name.c_str()));
    rv = -__LINE__;
  }
  return rv;
}

int Dev_Read(int fd, uint8_t *data, size_t size) {
  int rv;
  Dev_DeviceType *d;

  d = Dev_GetDeviceByFd(fd);
  if (nullptr == d) {
    ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    rv = -__LINE__;
  } else if (d->Ops->Read != nullptr) {
    rv = d->Ops->Read(d->Param, data, size);
  } else {
    ASLOG(DEVE, ("%s for %s is not supported\n", __func__, d->Name.c_str()));
    rv = -__LINE__;
  }
  return rv;
}

int Dev_IoCtl(int fd, int type, const void *data, size_t size) {
  Dev_DeviceType *d;
  int rv;
  d = Dev_GetDeviceByFd(fd);
  if (nullptr == d) {
    ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    rv = -__LINE__;
  } else if (d->Ops->IoCtl != nullptr) {
    rv = d->Ops->IoCtl(d->Param, type, data, size);
    if (rv < 0) {
      ASLOG(DEVE, ("%s ioctl on device %s failed(%d)\n", __func__, d->Name.c_str(), rv));
    }
  } else {
    ASLOG(DEVE, ("%s for %s is not supported\n", __func__, d->Name.c_str()));
    rv = -__LINE__;
  }
  return rv;
}

int Dev_Close(int fd) {
  Dev_DeviceType *d;
  int rv;

  d = Dev_GetDeviceByFd(fd);
  if (nullptr == d) {
    if (devListHead.IsInitialized) {
      ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    }
    rv = -__LINE__;
  } else {
    std::lock_guard<std::recursive_mutex> lg(devListHead.QLock);
    d->Ref--;
    ASLOG(DEVI, ("device(%s) close with fd=%d ref=%d\n", d->Name.c_str(), d->Fd, d->Ref));
    if (0 == d->Ref) {
      if (d->Ops->Close != nullptr) {
        d->Ops->Close(d->Param);
      }
      STAILQ_REMOVE(&devListHead.Head, d, Dev_s, Entry);
      free(d);
    }
    rv = 0;
  }

  return rv;
}

int Dev_Lock(int fd) {
  Dev_DeviceType *d;
  int rv;
  d = Dev_GetDeviceByFd(fd);
  if (nullptr == d) {
    ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    rv = -__LINE__;
  } else {
    d->Lock.lock();
    rv = 0;
  }
  return rv;
}

int Dev_Unlock(int fd) {
  Dev_DeviceType *d;
  int rv;
  d = Dev_GetDeviceByFd(fd);
  if (nullptr == d) {
    ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    rv = -__LINE__;
  } else {
    d->Lock.unlock();
    rv = 0;
  }
  return rv;
}
