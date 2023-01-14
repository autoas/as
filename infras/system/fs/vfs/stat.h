/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
#ifndef _SYS_STAT_H_
#define _SYS_STAT_H_
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#define S_IFMT   0xF000
#define S_IFDIR  0x4000
#define S_IFCHR  0x2000
#define S_IFIFO  0x1000
#define S_IFREG  0x8000
#define S_IREAD  0x0100
#define S_IWRITE 0x0080
#define S_IEXEC  0x0040

#define S_IRWXU (S_IREAD | S_IWRITE | S_IEXEC)
#define S_IXUSR S_IEXEC
#define S_IWUSR S_IWRITE
#define S_IRUSR S_IREAD

#define S_IRGRP    (S_IRUSR >> 3)
#define S_IWGRP    (S_IWUSR >> 3)
#define S_IXGRP    (S_IXUSR >> 3)
#define S_IRWXG    (S_IRWXU >> 3)

#define S_IROTH    (S_IRGRP >> 3)
#define S_IWOTH    (S_IWGRP >> 3)
#define S_IXOTH    (S_IXGRP >> 3)
#define S_IRWXO    (S_IRWXG >> 3)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _SYS_STAT_H_ */
