/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "devlib.h"
#include "devlib_priv.h"
#include "linlib.h"
#ifdef USE_STD_PRINTF
#undef USE_STD_PRINTF
#endif
#include "Std_Debug.h"
#include "Std_Types.h"
#include "Std_Timer.h"
#include <sys/queue.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include <errno.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_LIN 0
#define AS_LOG_LINE 2
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
static int dev_lin_open(const char *device, const char *option, void **param);
static int dev_lin_read(void *param, uint8_t *data, size_t size);
static int dev_lin_write(void *param, const uint8_t *data, size_t size);
static void dev_lin_close(void *param);
static int dev_lin_ioctl(void *param, int type, const void *data, size_t size);
static void *rx_daemon(void *param);

extern const Lin_DeviceOpsType lin_simulator_ops;

/* ================================ [ DATAS     ] ============================================== */
const Dev_DeviceOpsType lin_dev_ops = {
  .name = "lin/",
  .open = dev_lin_open,
  .read = dev_lin_read,
  .write = dev_lin_write,
  .close = dev_lin_close,
  .ioctl = dev_lin_ioctl,
};

static const Lin_DeviceOpsType *linOps[] = {
  &lin_simulator_ops,

  NULL,
};
/* ================================ [ LOCALS    ] ============================================== */
static const Lin_DeviceOpsType *search_ops(const char *name) {
  const Lin_DeviceOpsType *ops, **o;
  o = linOps;
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

static int dev_lin_open(const char *device, const char *option, void **param) {
  int r = 0;
  Lin_DeviceType *dev = NULL;
  const Lin_DeviceOpsType *ops = NULL;

  ops = search_ops(&device[4]);
  if (NULL == ops) {
    ASLOG(ERROR, ("LIN device(%s) is not found\n", device));
    r = -1;
  }

  if (0 == r) {
    dev = malloc(sizeof(Lin_DeviceType));
    if (NULL == dev) {
      r = -2;
    }
  }

  if (0 == r) {
    strcpy(dev->name, device);
    dev->ops = ops;
    dev->param = NULL;
    dev->size = 0;
    STAILQ_INIT(&dev->head);
    pthread_mutex_init(&dev->q_lock, NULL);
    dev->killed = FALSE;

    r = ops->open(dev, option);

    if (0 == r) {
      if (0 != pthread_create(&(dev->rx_thread), NULL, rx_daemon, (void *)dev)) {
        r = -3;
        ops->close(dev);
      }
    }

    if (0 == r) {
      *param = (void *)dev;
    } else {
      free(dev);
    }
  }

  return r;
}

static int dev_lin_read(void *param, uint8_t *data, size_t size) {
  int len = 0;
  Lin_DeviceType *dev = (Lin_DeviceType *)param;
  struct Lin_Frame_s *frame;
  (void)pthread_mutex_lock(&dev->q_lock);
  if (FALSE == STAILQ_EMPTY(&dev->head)) {
    frame = STAILQ_FIRST(&dev->head);
    if (size >= frame->size) {
      STAILQ_REMOVE_HEAD(&dev->head, entry);
      memcpy(data, frame->data, frame->size);
      len = frame->size;
    } else {
      ASLOG(LINE, ("lin read buffer size %d < %d\n", (int)size, (int)frame->size));
    }
  }
  (void)pthread_mutex_unlock(&dev->q_lock);
  return len;
}

static int dev_lin_write(void *param, const uint8_t *data, size_t size) {
  int len = LIN_MTU;
  Lin_DeviceType *dev = (Lin_DeviceType *)param;
  Lin_FrameType frame;

  memset(&frame, 0, LIN_MTU);
  if (((uint8_t)data[0] == LIN_TYPE_HEADER) && (size >= 2)) {
    frame.type = LIN_TYPE_HEADER;
    frame.pid = (lin_id_t)data[1];
    if (size > 2) {
      frame.dlc = data[2];
    } else {
      frame.dlc = 8;
    }
    ASLOG(LIN, ("TX Pid=%02X, DLC=%d @%u\n", frame.pid, (int)frame.dlc, (uint32_t)Std_GetTime()));
  } else if (((uint8_t)data[0] == LIN_TYPE_DATA) && (size > 2)) {
    frame.type = LIN_TYPE_DATA;
    frame.dlc = size - 2;
    memcpy(&frame.data, &data[1], frame.dlc);
    frame.checksum = data[size - 1];
    ASLOG(LIN, ("TX DLC=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X] checksum=%02X @%u\n",
                (int)frame.dlc, frame.data[0], frame.data[1], frame.data[2], frame.data[3],
                frame.data[4], frame.data[5], frame.data[6], frame.data[7], frame.checksum,
                (uint32_t)Std_GetTime()));
  } else if (((uint8_t)data[0] == LIN_TYPE_HEADER_AND_DATA) && (size > 3)) {
    frame.type = LIN_TYPE_HEADER_AND_DATA;
    frame.pid = (lin_id_t)data[1];
    frame.dlc = size - 3;
    memcpy(&frame.data, &data[2], frame.dlc);
    frame.checksum = data[size - 1];
    ASLOG(LIN,
          ("TX Pid=%02X, DLC=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X] checksum=%02X @%u\n",
           frame.pid, (int)frame.dlc, frame.data[0], frame.data[1], frame.data[2], frame.data[3],
           frame.data[4], frame.data[5], frame.data[6], frame.data[7], frame.checksum,
           (uint32_t)Std_GetTime()));
  } else if (((uint8_t)data[0] == LIN_TYPE_EXT_HEADER) && (size >= 5)) {
    frame.type = LIN_TYPE_EXT_HEADER;
    frame.pid =
      ((uint32_t)data[1] << 24) + ((uint32_t)data[2] << 16) + ((uint32_t)data[3] << 8) + data[4];
    if (size > 5) {
      frame.dlc = data[5];
    } else {
      frame.dlc = 8;
    }
    ASLOG(LIN, ("TX Pid=%02X, DLC=%d @%u\n", frame.pid, (int)frame.dlc, (uint32_t)Std_GetTime()));
  } else if (((uint8_t)data[0] == LIN_TYPE_EXT_HEADER_AND_DATA) && (size > 6)) {
    frame.type = LIN_TYPE_EXT_HEADER_AND_DATA;
    frame.pid =
      ((uint32_t)data[1] << 24) + ((uint32_t)data[2] << 16) + ((uint32_t)data[3] << 8) + data[4];
    frame.dlc = size - 6;
    memcpy(&frame.data, &data[5], frame.dlc);
    frame.checksum = data[size - 1];
    ASLOG(LIN,
          ("TX Pid=%02X, DLC=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X] checksum=%02X @%u\n",
           frame.pid, (int)frame.dlc, frame.data[0], frame.data[1], frame.data[2], frame.data[3],
           frame.data[4], frame.data[5], frame.data[6], frame.data[7], frame.checksum,
           (uint32_t)Std_GetTime()));
  } else {
    ASLOG(ERROR, ("Invalid data format for %s\n", dev->name));
    len = -EINVAL;
  }

  if (LIN_MTU == len) {
    len = dev->ops->write(dev, &frame);
  }

  if (LIN_MTU == len) {
    len = size;
  }

  return len;
}

static void dev_lin_close(void *param) {
  void *thread_return;
  Lin_DeviceType *dev = (Lin_DeviceType *)param;
  dev->killed = TRUE;
  pthread_join(dev->rx_thread, &thread_return);
  dev->ops->close(dev);
  free(dev);
}

static int dev_lin_ioctl(void *param, int type, const void *data, size_t size) {
  int r = -ENOTSUP;
  Lin_DeviceType *dev = (Lin_DeviceType *)param;

  if (dev->ops->ioctl) {
    r = dev->ops->ioctl(dev, type, data, size);
  }

  return r;
}

static void *rx_daemon(void *param) {
  Lin_DeviceType *dev = (Lin_DeviceType *)param;
  Lin_FrameType frame;
  struct Lin_Frame_s *pframe;
  int r;

  while (FALSE == dev->killed) {
    r = dev->ops->read(dev, &frame);
    if (LIN_MTU == r) {
      pframe = malloc(sizeof(struct Lin_Frame_s));
      if (NULL != pframe) {
        if (frame.type == LIN_TYPE_HEADER) {
          pframe->data[0] = LIN_TYPE_HEADER;
          pframe->data[1] = frame.pid;
          pframe->size = 2;
          ASLOG(LIN, ("RX Pid=%02X @%u\n", frame.pid, (uint32_t)Std_GetTime()));
        } else if (frame.type == LIN_TYPE_DATA) {
          pframe->data[0] = LIN_TYPE_DATA;
          memcpy(&pframe->data[1], frame.data, frame.dlc);
          pframe->data[1 + frame.dlc] = frame.checksum;
          pframe->size = 2 + frame.dlc;
          ASLOG(LIN, ("RX DLC=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X] "
                      "checksum=%02X @%u\n",
                      (int)frame.dlc, frame.data[0], frame.data[1], frame.data[2], frame.data[3],
                      frame.data[4], frame.data[5], frame.data[6], frame.data[7], frame.checksum,
                      (uint32_t)Std_GetTime()));
        } else if (frame.type == LIN_TYPE_HEADER_AND_DATA) {
          pframe->data[0] = LIN_TYPE_HEADER_AND_DATA;
          pframe->data[1] = frame.pid;
          memcpy(&pframe->data[2], frame.data, frame.dlc);
          pframe->data[2 + frame.dlc] = frame.checksum;
          pframe->size = 3 + frame.dlc;
          ASLOG(LIN, ("RX Pid=%02X, DLC=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X] "
                      "checksum=%02X @%u\n",
                      frame.pid, (int)frame.dlc, frame.data[0], frame.data[1], frame.data[2],
                      frame.data[3], frame.data[4], frame.data[5], frame.data[6], frame.data[7],
                      frame.checksum, (uint32_t)Std_GetTime()));
        } else if (frame.type == LIN_TYPE_EXT_HEADER) {
          pframe->data[0] = LIN_TYPE_EXT_HEADER;
          pframe->data[1] = (frame.pid >> 24) & 0xFF;
          pframe->data[2] = (frame.pid >> 16) & 0xFF;
          pframe->data[3] = (frame.pid >> 8) & 0xFF;
          pframe->data[4] = frame.pid & 0xFF;
          pframe->size = 5;
          ASLOG(LIN, ("RX Pid=%02X @%u\n", frame.pid, (uint32_t)Std_GetTime()));
        } else if (frame.type == LIN_TYPE_EXT_HEADER_AND_DATA) {
          pframe->data[0] = LIN_TYPE_EXT_HEADER_AND_DATA;
          pframe->data[1] = (frame.pid >> 24) & 0xFF;
          pframe->data[2] = (frame.pid >> 16) & 0xFF;
          pframe->data[3] = (frame.pid >> 8) & 0xFF;
          pframe->data[4] = frame.pid & 0xFF;
          memcpy(&pframe->data[5], frame.data, frame.dlc);
          pframe->data[5 + frame.dlc] = frame.checksum;
          pframe->size = 5 + frame.dlc;
          ASLOG(LIN, ("RX Pid=%02X, DLC=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X] "
                      "checksum=%02X @%u\n",
                      frame.pid, (int)frame.dlc, frame.data[0], frame.data[1], frame.data[2],
                      frame.data[3], frame.data[4], frame.data[5], frame.data[6], frame.data[7],
                      frame.checksum, (uint32_t)Std_GetTime()));
        } else {
          ASLOG(ERROR, ("invalid frame from %s\n", dev->name));
          continue;
        }
        (void)pthread_mutex_lock(&dev->q_lock);
        STAILQ_INSERT_TAIL(&dev->head, pframe, entry);
        (void)pthread_mutex_unlock(&dev->q_lock);
      }
    }
    usleep(1000);
  }

  return NULL;
}

/* ================================ [ FUNCTIONS ] ============================================== */
