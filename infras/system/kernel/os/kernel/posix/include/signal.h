/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
#ifndef _ASKAR_SIGNAL_H_
#define _ASKAR_SIGNAL_H_
/* ================================ [ INCLUDES  ] ============================================== */
#include "pthread.h"
/* ================================ [ MACROS    ] ============================================== */
#define SIGHUP 1      /* hangup */
#define SIGINT 2      /* interrupt */
#define SIGQUIT 3     /* quit */
#define SIGILL 4      /* illegal instruction (not reset when caught) */
#define SIGTRAP 5     /* trace trap (not reset when caught) */
#define SIGIOT 6      /* IOT instruction */
#define SIGABRT 6     /* used by abort, replace SIGIOT in the future */
#define SIGEMT 7      /* EMT instruction */
#define SIGFPE 8      /* floating point exception */
#define SIGKILL 9     /* kill (cannot be caught or ignored) */
#define SIGBUS 10     /* bus error */
#define SIGSEGV 11    /* segmentation violation */
#define SIGSYS 12     /* bad argument to system call */
#define SIGPIPE 13    /* write on a pipe with no one to read it */
#define SIGALRM 14    /* alarm clock */
#define SIGTERM 15    /* software termination signal from kill */
#define SIGURG 16     /* urgent condition on IO channel */
#define SIGSTOP 17    /* sendable stop signal not from tty */
#define SIGTSTP 18    /* stop signal from tty */
#define SIGCONT 19    /* continue a stopped process */
#define SIGCHLD 20    /* to parent on child stop or exit */
#define SIGCLD 20     /* System V name for SIGCHLD */
#define SIGTTIN 21    /* to readers pgrp upon background tty read */
#define SIGTTOU 22    /* like TTIN for output if (tp->t_local&LTOSTOP) */
#define SIGIO 23      /* input/output possible signal */
#define SIGPOLL SIGIO /* System V name for SIGIO */
#define SIGXCPU 24    /* exceeded CPU time limit */
#define SIGXFSZ 25    /* exceeded file size limit */
#define SIGVTALRM 26  /* virtual time alarm */
#define SIGPROF 27    /* profiling time alarm */
#define SIGWINCH 28   /* window changed */
#define SIGLOST 29    /* resource lost (eg, record-lock lost) */
#define SIGUSR1 30    /* user defined signal 1 */
#define SIGUSR2 31    /* user defined signal 2 */

#define NSIG (8 * sizeof(sigset_t))

#define SIG_DFL (_sig_func_ptr)0
#define SIG_IGN (_sig_func_ptr)1
#define SIG_GET (_sig_func_ptr)2
#define SIG_SGE (_sig_func_ptr)3
#define SIG_ACK (_sig_func_ptr)4
#define SIG_ERR (_sig_func_ptr) - 1

#define SIG_SETMASK 0 /* set mask with sigprocmask() */
#define SIG_BLOCK 1   /* set of signals to block */
#define SIG_UNBLOCK 2 /* set of signals to, well, unblock */

/* ================================ [ TYPES     ] ============================================== */
typedef void (*_sig_func_ptr)(int);

typedef _sig_func_ptr sighandler_t;

struct sigaction {
  _sig_func_ptr sa_handler;
  sigset_t sa_mask;
  int sa_flags;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int pthread_sigmask(int how, const sigset_t *set, sigset_t *oset);
int pthread_kill(pthread_t tid, int sig);
int raise(int sig);

int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, const int signo);
int sigdelset(sigset_t *set, const int signo);
int sigismember(const sigset_t *set, int signo);
sighandler_t signal(int signum, sighandler_t action);
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

int sigwait(const sigset_t *set, int *sig);
#endif /* _ASKAR_SIGNAL_H_ */
