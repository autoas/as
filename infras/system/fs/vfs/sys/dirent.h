/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
#ifndef _VFS_SYS_DIRENT_H_
#define _VFS_SYS_DIRENT_H_
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#ifndef FILENAME_MAX
#define FILENAME_MAX 252
#endif
/* ================================ [ TYPES     ] ============================================== */
struct dirent
{
	unsigned short	d_namlen;	/* Length of name in d_name. */
	char		d_name[FILENAME_MAX]; /* [FILENAME_MAX] */ /* File name. */
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _VFS_SYS_DIRENT_H_ */
