/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "devlib.h"
#include "devlib_priv.h"
#ifdef USE_STD_PRINTF
#undef USE_STD_PRINTF
#endif
#include "Std_Debug.h"
#include "Std_Types.h"
#include <sys/queue.h>
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DEV 0
#define AS_LOG_DEVI 0
#define AS_LOG_DEVE 2
#define DEVICE_OPTION_SIZE 128
/* ================================ [ TYPES     ] ============================================== */
struct Dev_s {
  char name[DEVICE_NAME_SIZE];
  char option[DEVICE_OPTION_SIZE];
  int fd;
  void *param; /* should be filled by device control module while open */
  const Dev_DeviceOpsType *ops;
  uint32_t size2;
  STAILQ_ENTRY(Dev_s) entry;
  pthread_mutex_t lock;
  uint32_t ref;
};

struct DevList_s {
  boolean initialized;
  pthread_mutex_t q_lock;
  STAILQ_HEAD(, Dev_s) head;
};
/* ================================ [ DECLARES  ] ============================================== */
extern const Dev_DeviceOpsType rs232_dev_ops;
extern const Dev_DeviceOpsType lin_dev_ops;
/* ================================ [ DATAS     ] ============================================== */
static const Dev_DeviceOpsType *devOps[] = {
  &rs232_dev_ops,
  &lin_dev_ops,
  NULL,
};
static int _fd = 0; /* file identifier start from 0 */
static struct DevList_s devListH = {
  .initialized = FALSE,
  .q_lock = PTHREAD_MUTEX_INITIALIZER,
};
/* ================================ [ LOCALS    ] ============================================== */
static struct Dev_s *getDev(const char *name) {
  struct Dev_s *handle, *h;
  handle = NULL;

  (void)pthread_mutex_lock(&devListH.q_lock);
  STAILQ_FOREACH(h, &devListH.head, entry) {
    if (0u == strcmp(h->name, name)) {
      handle = h;
      break;
    }
  }
  (void)pthread_mutex_unlock(&devListH.q_lock);

  return handle;
}

static struct Dev_s *getDev2(int fd) {
  struct Dev_s *handle, *h;
  handle = NULL;

  (void)pthread_mutex_lock(&devListH.q_lock);
  STAILQ_FOREACH(h, &devListH.head, entry) {
    if (fd == h->fd) {
      handle = h;
      break;
    }
  }
  (void)pthread_mutex_unlock(&devListH.q_lock);

  return handle;
}
static const Dev_DeviceOpsType *search_ops(const char *name) {
  const Dev_DeviceOpsType *ops, **o;
  o = devOps;
  ops = NULL;
  while (*o != NULL) {
    if (name == strstr(name, (*o)->name)) {
      ops = *o;
      break;
    }
    o++;
  }

  return ops;
}
static void freeDev(struct DevList_s *h) {
  struct Dev_s *d;

  pthread_mutex_lock(&h->q_lock);
  while (FALSE == STAILQ_EMPTY(&h->head)) {
    d = STAILQ_FIRST(&h->head);
    STAILQ_REMOVE_HEAD(&h->head, entry);
    d->ops->close(d->param);
    free(d);
  }
  pthread_mutex_unlock(&h->q_lock);
}

void devlib_close(void) {
  if (devListH.initialized) {
    devListH.initialized = FALSE;
    freeDev(&devListH);
  }
}

static void __attribute__((constructor)) devlib_open(void) {
  pthread_mutexattr_t attr;
  STAILQ_INIT(&devListH.head);
  devListH.initialized = TRUE;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&devListH.q_lock, &attr);
  atexit(devlib_close);
}
/* ================================ [ FUNCTIONS ] ============================================== */
int dev_open(const char *device_name, const char *option) {
  const Dev_DeviceOpsType *ops;
  struct Dev_s *d;
  int rv;

  pthread_mutex_lock(&devListH.q_lock);
  d = getDev(device_name);
  if (NULL != d) {
    if (option != NULL) {
      if (0 == strcmp(option, d->option)) {
        rv = d->fd;
        d->ref++;
      } else {
        ASLOG(DEVE, ("device(%s) reopen with different option %s != %s\n", device_name, option,
                     d->option));
        rv = -__LINE__;
      }
    } else {
      rv = d->fd;
    }
  } else {
    ops = search_ops(device_name);
    if (NULL != ops) {
      d = malloc(sizeof(struct Dev_s));
      if (d != NULL) {
        strcpy(d->name, device_name);
        if (option != NULL) {
          strcpy(d->option, option);
        } else {
          d->option[0] = '\0';
        }

        rv = ops->open(device_name, option, &d->param);
      } else {
        rv = -__LINE__;
      }

      if (rv >= 0) {
        d->fd = _fd++;
        d->ops = ops;
        d->ref = 1;
        pthread_mutex_init(&d->lock, NULL);
        STAILQ_INSERT_TAIL(&devListH.head, d, entry);
        rv = d->fd;
      } else {
        if (d) {
          free(d);
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
  pthread_mutex_unlock(&devListH.q_lock);

  return rv;
}

int dev_write(int fd, const uint8_t *data, size_t size) {
  int rv;
  struct Dev_s *d;
  d = getDev2(fd);
  if (NULL == d) {
    ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    rv = -__LINE__;
  } else if (d->ops->write != NULL) {
    rv = d->ops->write(d->param, data, size);

    if (rv < 0) {
      ASLOG(DEVE, ("%s write on device %s failed(%d)\n", __func__, d->name, rv));
    }
  } else {
    ASLOG(DEVE, ("%s for %s is not supported\n", __func__, d->name));
    rv = -__LINE__;
  }
  return rv;
}

int dev_read(int fd, uint8_t *data, size_t size) {
  int rv;
  struct Dev_s *d;

  d = getDev2(fd);
  if (NULL == d) {
    ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    rv = -__LINE__;
  } else if (d->ops->read != NULL) {
    rv = d->ops->read(d->param, data, size);
  } else {
    ASLOG(DEVE, ("%s for %s is not supported\n", __func__, d->name));
    rv = -__LINE__;
  }
  return rv;
}

int dev_ioctl(int fd, int type, const void *data, size_t size) {
  struct Dev_s *d;
  int rv;
  d = getDev2(fd);
  if (NULL == d) {
    ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    rv = -__LINE__;
  } else if (d->ops->ioctl != NULL) {
    rv = d->ops->ioctl(d->param, type, data, size);
    if (rv < 0) {
      ASLOG(DEVE, ("%s ioctl on device %s failed(%d)\n", __func__, d->name, rv));
    }
  } else {
    ASLOG(DEVE, ("%s for %s is not supported\n", __func__, d->name));
    rv = -__LINE__;
  }
  return rv;
}

int dev_close(int fd) {
  struct Dev_s *d;
  int rv;

  d = getDev2(fd);
  if (NULL == d) {
    if (devListH.initialized) {
      ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    }
    rv = -__LINE__;
  } else {
    pthread_mutex_lock(&devListH.q_lock);
    d->ref--;
    ASLOG(DEVI, ("device(%s) close with fd=%d ref=%d\n", d->name, d->fd, d->ref));
    if (0 == d->ref) {
      if (d->ops->close != NULL) {
        d->ops->close(d->param);
      }
      STAILQ_REMOVE(&devListH.head, d, Dev_s, entry);
      free(d);
    }
    pthread_mutex_unlock(&devListH.q_lock);
    rv = 0;
  }

  return rv;
}

int dev_lock(int fd) {
  struct Dev_s *d;
  int rv;
  d = getDev2(fd);
  if (NULL == d) {
    ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    rv = -__LINE__;
  } else {
    rv = pthread_mutex_lock(&d->lock);
  }
  return rv;
}

int dev_unlock(int fd) {
  struct Dev_s *d;
  int rv;
  d = getDev2(fd);
  if (NULL == d) {
    ASLOG(DEVE, ("fd(%d) is not existed '%s'\n", fd, __func__));
    rv = -__LINE__;
  } else {
    rv = pthread_mutex_unlock(&d->lock);
  }
  return rv;
}
