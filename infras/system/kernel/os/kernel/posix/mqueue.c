/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#if (OS_PTHREAD_NUM > 0)
#include "mqueue.h"
#include "OsMem.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define MQ_ALIGN(sz) ((sz + sizeof(void *) - 1) & (~(sizeof(void *) - 1)))

/* ================================ [ TYPES     ] ============================================== */
struct mqd_msg {
  TAILQ_ENTRY(mqd_msg) entry;
};

struct mqd {
  size_t msgsize;
  unsigned int refcount;
  unsigned int unlinked;
  char *name;
  TAILQ_HEAD(mqfree_list, mqd_msg) free;
  TAILQ_HEAD(mqavail_list, mqd_msg) avail;
  TAILQ_ENTRY(mqd) entry;
  TaskListType slist;
  TaskListType rlist;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static TAILQ_HEAD(mqueue_list, mqd) OsMqueueList = TAILQ_HEAD_INITIALIZER(OsMqueueList);
/* ================================ [ LOCALS    ] ============================================== */
static mqd_t mq_find(const char *name) {
  struct mqd *mq = NULL;

  TAILQ_FOREACH(mq, &OsMqueueList, entry) {
    if (0u == strcmp(mq->name, name)) {
      break;
    }
  }

  return mq;
}
/* ================================ [ FUNCTIONS ] ============================================== */
mqd_t mq_open(const char *name, int oflag, ...) {
  struct mqd *mq = NULL;
  struct mqd_msg *msg;
  va_list arg;
  mode_t mode;
  long sz, i;
  struct mq_attr *attr = NULL;

  asAssert(name != NULL);

  EnterCritical();
  mq = mq_find(name);
  ExitCritical();

  if ((NULL == mq) && (0 != (oflag & O_CREAT))) {
    va_start(arg, oflag);
    mode = (mode_t)va_arg(arg, mode_t);
    mode = mode;
    attr = (struct mq_attr *)va_arg(arg, struct mq_attr *);
    va_end(arg);
    asAssert(attr != NULL);
    asAssert(attr->mq_maxmsg > 0);
    asAssert(attr->mq_msgsize > 0);
    sz = MQ_ALIGN(attr->mq_msgsize) + sizeof(struct mqd_msg);
    mq = (mqd_t)Os_MemAlloc(sizeof(struct mqd) + sz * attr->mq_maxmsg + strlen(name) + 1);
    if (NULL != mq) {
      TAILQ_INIT(&mq->free);
      TAILQ_INIT(&mq->avail);
      msg = (struct mqd_msg *)&mq[1];
      for (i = 0; i < attr->mq_maxmsg; i++) {
        TAILQ_INSERT_TAIL(&mq->free, msg, entry);
        msg = (struct mqd_msg *)(((void *)msg) + sz);
      }
      mq->name = ((char *)&mq[1]) + sz * attr->mq_maxmsg;
      strcpy(mq->name, name);
      mq->msgsize = attr->mq_msgsize;
      mq->refcount = 0;
      mq->unlinked = 0;
      EnterCritical();
      /* no consideration of the mq_create race condition,
       * so just assert if such condition */
      asAssert(NULL == mq_find(name));
      TAILQ_INSERT_TAIL(&OsMqueueList, mq, entry);
      ExitCritical();
    }
  }

  if (NULL != mq) {
    EnterCritical();
    mq->refcount++;
    ExitCritical();
  }

  return mq;
}
ELF_EXPORT(mq_open);

int mq_close(mqd_t mq) {
  int ercd = 0;

  EnterCritical();
  if (mq->refcount > 0) {
    mq->refcount--;
    if ((0 == mq->refcount) && (mq->unlinked)) {
      TAILQ_REMOVE(&OsMqueueList, mq, entry);
      Os_MemFree((uint8_t *)mq);
    }
  } else {
    ercd = -EACCES;
  }
  ExitCritical();

  return ercd;
}
ELF_EXPORT(mq_close);

int mq_unlink(const char *name) {
  int ercd = 0;
  struct mqd *mq = NULL;

  EnterCritical();
  mq = mq_find(name);
  if (NULL != mq) {
    mq->unlinked = 1;
    if (0 == mq->refcount) {
      TAILQ_REMOVE(&OsMqueueList, mq, entry);
      Os_MemFree((uint8_t *)mq);
    }
  }
  ExitCritical();

  return ercd;
}
ELF_EXPORT(mq_unlink);

int mq_timedsend(mqd_t mq, const char *msg_ptr, size_t msg_len, unsigned msg_prio,
                 const struct timespec *abs_timeout) {
  int ercd = 0;
  struct mqd_msg *msg;

  asAssert(mq);
  asAssert(msg_ptr);
  asAssert(msg_len > 0);
  (void)msg_prio;

  /* greater than one message size */
  if (msg_len <= mq->msgsize) {
    EnterCritical();
    msg = TAILQ_FIRST(&mq->free);
    if (NULL == msg) {

      ercd = Os_ListWait(&mq->slist, abs_timeout);

      if (0 == ercd) {
        msg = TAILQ_FIRST(&mq->free);
      }
    }

    if (NULL != msg) {
      TAILQ_REMOVE(&mq->free, msg, entry);
      memcpy(&msg[1], msg_ptr, msg_len);
      TAILQ_INSERT_TAIL(&mq->avail, msg, entry);
      (void)Os_ListPost(&mq->rlist, TRUE);
    } else {
      ercd = -ENOSPC;
    }
    ExitCritical();
  } else {
    ercd = -EINVAL;
  }

  return ercd;
}
ELF_EXPORT(mq_timedsend);

int mq_send(mqd_t mq, const char *msg_ptr, size_t msg_len, unsigned msg_prio) {
  return mq_timedsend(mq, msg_ptr, msg_len, msg_prio, NULL);
}
ELF_EXPORT(mq_send);

ssize_t mq_timedreceive(mqd_t mq, char *msg_ptr, size_t msg_len, unsigned *msg_prio,
                        const struct timespec *abs_timeout) {
  int ercd = 0;
  struct mqd_msg *msg;

  asAssert(mq);
  asAssert(msg_ptr);
  asAssert(msg_len > 0);
  (void)msg_prio;

  /* greater than one message size */
  if (msg_len <= mq->msgsize) {
    EnterCritical();
    msg = TAILQ_FIRST(&mq->avail);
    if (NULL == msg) {
      ercd = Os_ListWait(&mq->rlist, abs_timeout);

      if (0 == ercd) {
        msg = TAILQ_FIRST(&mq->free);
      }
    }

    if (NULL != msg) {
      TAILQ_REMOVE(&mq->avail, msg, entry);
      memcpy(msg_ptr, &msg[1], msg_len > mq->msgsize ? mq->msgsize : msg_len);
      TAILQ_INSERT_TAIL(&mq->free, msg, entry);
      (void)Os_ListPost(&mq->slist, TRUE);
    } else {
      ercd = -ENOSPC;
    }
    ExitCritical();
  } else {
    ercd = -EINVAL;
  }

  return ercd;
}
ELF_EXPORT(mq_timedreceive);

ssize_t mq_receive(mqd_t mq, char *msg_ptr, size_t msg_len, unsigned *msg_prio) {
  return mq_timedreceive(mq, msg_ptr, msg_len, msg_prio, NULL);
}
ELF_EXPORT(mq_receive);

#endif /* OS_PTHREAD_NUM */
