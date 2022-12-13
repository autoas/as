/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */

#ifndef _ASKAR_MQUEUE_H_
#define _ASKAR_MQUEUE_H_
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include <fcntl.h>
#include <sys/types.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
struct mq_attr {
  long mq_flags;   /* message queue flags */
  long mq_maxmsg;  /* maximum number of messages */
  long mq_msgsize; /* maximum message size */
  long mq_curmsgs; /* number of messages currently queued */
};

struct mqd;
/* message queue types */
typedef struct mqd *mqd_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int mq_close(mqd_t mq);
int mq_getattr(mqd_t mq, struct mq_attr *mqstat);
mqd_t mq_open(const char *name, int oflag, ...);
ssize_t mq_receive(mqd_t mq, char *msg_ptr, size_t msg_len, unsigned *msg_prio);
int mq_send(mqd_t mq, const char *msg_ptr, size_t msg_len, unsigned msg_prio);
int mq_setattr(mqd_t mq, const struct mq_attr *mqstat, struct mq_attr *omqstat);
ssize_t mq_timedreceive(mqd_t mq, char *msg_ptr, size_t msg_len, unsigned *msg_prio,
                        const struct timespec *abs_timeout);
int mq_timedsend(mqd_t mq, const char *msg_ptr, size_t msg_len, unsigned msg_prio,
                 const struct timespec *abs_timeout);

int mq_unlink(const char *name);

#endif /* _ASKAR_MQUEUE_H_ */
