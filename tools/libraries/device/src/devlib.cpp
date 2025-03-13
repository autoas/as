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
struct Dev_s {
  std::string name;
  std::string option;
  int fd;
  void *param; /* should be filled by device control module while open */
  const Dev_DeviceOpsType *ops;
  uint32_t size2;
  STAILQ_ENTRY(Dev_s) entry;
  std::mutex lock;
  uint32_t ref;
};

struct DevList_s {
  boolean initialized;
  std::recursive_mutex q_lock;
  STAILQ_HEAD(, Dev_s) head;
};
/* ================================ [ DECLARES  ] ============================================== */
extern "C" const Dev_DeviceOpsType rs232_dev_ops;
extern "C" const Dev_DeviceOpsType lin_dev_ops;
/* ================================ [ DATAS     ] ============================================== */
static const Dev_DeviceOpsType *devOps[] = {
  &rs232_dev_ops,
  &lin_dev_ops,
  nullptr,
};
static int _fd = 0; /* file identifier start from 0 */
static struct DevList_s devListH = {
  .initialized = FALSE,
};
/* ================================ [ LOCALS    ] ============================================== */
static struct Dev_s *getDev(const char *name) {
  struct Dev_s *handle, *h;
  handle = nullptr;

  std::lock_guard<std::recursive_mutex> lg(devListH.q_lock);
  STAILQ_FOREACH(h, &devListH.head, entry) {
    if (0u == strcmp(h->name.c_str(), name)) {
      handle = h;
      break;
    }
  }

  return handle;
}

static struct Dev_s *getDev2(int fd) {
  struct Dev_s *handle, *h;
  handle = nullptr;

  std::lock_guard<std::recursive_mutex> lg(devListH.q_lock);
  STAILQ_FOREACH(h, &devListH.head, entry) {
    if (fd == h->fd) {
      handle = h;
      break;
    }
  }

  return handle;
}
static const Dev_DeviceOpsType *search_ops(const char *name) {
  const Dev_DeviceOpsType *ops, **o;
  o = devOps;
  ops = nullptr;
  while (*o != nullptr) {
    if (name == strstr(name, (*o)->name.c_str())) {
      ops = *o;
      break;
    }
    o++;
  }

  return ops;
}
static void freeDev(struct DevList_s *h) {
  struct Dev_s *d;

  std::lock_guard<std::recursive_mutex> lg(h->q_lock);
  while (FALSE == STAILQ_EMPTY(&h->head)) {
    d = STAILQ_FIRST(&h->head);
    STAILQ_REMOVE_HEAD(&h->head, entry);
    d->ops->close(d->param);
    delete d;
  }
}

void devlib_close(void) {
  if (devListH.initialized) {
    devListH.initialized = FALSE;
    freeDev(&devListH);
  }
}

INITIALIZER(devlib_open) {
  STAILQ_INIT(&devListH.head);
  devListH.initialized = TRUE;
  atexit(devlib_close);
}
/* ================================ [ FUNCTIONS ] ============================================== */
int dev_open(const char *device_name, const char *option) {
  const Dev_DeviceOpsType *ops;
  struct Dev_s *d;
  int rv;

  std::lock_guard<std::recursive_mutex> lg(devListH.q_lock);
  d = getDev(device_name);
  if (nullptr != d) {
    if (option != nullptr) {
      if (0 == strcmp(option, d->option.c_str())) {
        rv = d->fd;
        d->ref++;
      } else {
        ASLOG(DEVE, ("device(%s) reopen with different option %s != %s\n", device_name, option,
                     d->option.c_str()));
        rv = -__LINE__;
      }
    } else {
      rv = d->fd;
    }
  } else {
    ops = search_ops(device_name);
    if (nullptr != ops) {
      d = new struct Dev_s;
      if (d != nullptr) {
        d->name = device_name;
        if (option != nullptr) {
          d->option = option;
        }

        rv = ops->open(device_name, option, &d->param);
      } else {
        rv = -__LINE__;
      }

      if (rv >= 0) {
        d->fd = _fd++;
        d->ops = ops;
        d->ref = 1;
        STAILQ_INSERT_TAIL(&devListH.head, d, entry);
        rv = d->fd;
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
          ("device(%s) open with option %s as fd=%d ref=%d\n", device_name, option, d->fd, d->ref));
  }

  return rv;
}

int dev_write(int fd, const uint8_t *data, size_t size) {
  int rv;
  struct Dev_s *d;
  d = getDev2(fd);
  if (nullptr == d) {
    ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    rv = -__LINE__;
  } else if (d->ops->write != nullptr) {
    rv = d->ops->write(d->param, data, size);

    if (rv < 0) {
      ASLOG(DEVE, ("%s write on device %s failed(%d)\n", __func__, d->name.c_str(), rv));
    }
  } else {
    ASLOG(DEVE, ("%s for %s is not supported\n", __func__, d->name.c_str()));
    rv = -__LINE__;
  }
  return rv;
}

int dev_read(int fd, uint8_t *data, size_t size) {
  int rv;
  struct Dev_s *d;

  d = getDev2(fd);
  if (nullptr == d) {
    ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    rv = -__LINE__;
  } else if (d->ops->read != nullptr) {
    rv = d->ops->read(d->param, data, size);
  } else {
    ASLOG(DEVE, ("%s for %s is not supported\n", __func__, d->name.c_str()));
    rv = -__LINE__;
  }
  return rv;
}

int dev_ioctl(int fd, int type, const void *data, size_t size) {
  struct Dev_s *d;
  int rv;
  d = getDev2(fd);
  if (nullptr == d) {
    ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    rv = -__LINE__;
  } else if (d->ops->ioctl != nullptr) {
    rv = d->ops->ioctl(d->param, type, data, size);
    if (rv < 0) {
      ASLOG(DEVE, ("%s ioctl on device %s failed(%d)\n", __func__, d->name.c_str(), rv));
    }
  } else {
    ASLOG(DEVE, ("%s for %s is not supported\n", __func__, d->name.c_str()));
    rv = -__LINE__;
  }
  return rv;
}

int dev_close(int fd) {
  struct Dev_s *d;
  int rv;

  d = getDev2(fd);
  if (nullptr == d) {
    if (devListH.initialized) {
      ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    }
    rv = -__LINE__;
  } else {
    std::lock_guard<std::recursive_mutex> lg(devListH.q_lock);
    d->ref--;
    ASLOG(DEVI, ("device(%s) close with fd=%d ref=%d\n", d->name.c_str(), d->fd, d->ref));
    if (0 == d->ref) {
      if (d->ops->close != nullptr) {
        d->ops->close(d->param);
      }
      STAILQ_REMOVE(&devListH.head, d, Dev_s, entry);
      free(d);
    }
    rv = 0;
  }

  return rv;
}

int dev_lock(int fd) {
  struct Dev_s *d;
  int rv;
  d = getDev2(fd);
  if (nullptr == d) {
    ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    rv = -__LINE__;
  } else {
    d->lock.lock();
    rv = 0;
  }
  return rv;
}

int dev_unlock(int fd) {
  struct Dev_s *d;
  int rv;
  d = getDev2(fd);
  if (nullptr == d) {
    ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    rv = -__LINE__;
  } else {
    d->lock.unlock();
    rv = 0;
  }
  return rv;
}
