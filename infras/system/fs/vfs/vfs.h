/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
#ifndef _VFS_H
#define _VFS_H

/* ================================ [ INCLUDES  ] ============================================== */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32) || defined(linux)
#include <sys/stat.h>
#else
#include "stat.h"
#include <sys/dirent.h>
#endif
#include <time.h>
#include <errno.h>
#include "device.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef S_IRWXG
#define S_IRWXG 00070
#endif
#ifndef S_IRWXO
#define S_IRWXO 00007
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m)&S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m)&S_IFMT) == S_IFREG)
#endif
#define VFS_ISDIR(st_mode) S_ISDIR(st_mode)
#define VFS_ISREG(st_mode) S_ISREG(st_mode)
/* ================================ [ TYPES     ] ============================================== */
struct vfs_filesystem_ops;

typedef struct vfs_mount_s {
  const char *mount_point;
  const struct vfs_filesystem_ops *ops;
  const device_t *device;
  TAILQ_ENTRY(vfs_mount_s) entry;
} vfs_mount_t;

typedef struct {
  const struct vfs_filesystem_ops *fops;
  void *priv;
} VFS_DIR;

typedef struct {
  const struct vfs_filesystem_ops *fops;
  void *priv;
} VFS_FILE;

typedef struct {
  uint32_t st_mode; /* File mode */
  size_t st_size;   /* File size (regular files only) */
} vfs_stat_t;

typedef struct {
  unsigned short d_namlen;                        /* Length of name in d_name. */
  char d_name[FILENAME_MAX]; /* [FILENAME_MAX] */ /* File name. */
} vfs_dirent_t;

/* File system operations */
struct vfs_filesystem_ops {
  const char *name;

  /* operations for file */
  VFS_FILE *(*fopen)(const vfs_mount_t *mnt, const char *filename, const char *opentype);
  int (*fclose)(VFS_FILE *stream);
  int (*fread)(void *data, size_t size, size_t count, VFS_FILE *stream);
  int (*fwrite)(const void *data, size_t size, size_t count, VFS_FILE *stream);
  int (*fflush)(VFS_FILE *stream);
  int (*fseek)(VFS_FILE *stream, long int offset, int whence);
  size_t (*ftell)(VFS_FILE *stream);

  int (*unlink)(const vfs_mount_t *mnt, const char *filename);
  int (*stat)(const vfs_mount_t *mnt, const char *filename, vfs_stat_t *buf);

  VFS_DIR *(*opendir)(const vfs_mount_t *mnt, const char *dirname);
  vfs_dirent_t *(*readdir)(VFS_DIR *dirstream);
  int (*closedir)(VFS_DIR *dirstream);

  int (*chdir)(const vfs_mount_t *mnt, const char *filename);
  int (*mkdir)(const vfs_mount_t *mnt, const char *filename, uint32_t mode);
  int (*rmdir)(const vfs_mount_t *mnt, const char *filename);
  int (*rename)(const vfs_mount_t *mnt, const char *oldname, const char *newname);

  int (*mount)(const device_t *device, const char *mount_point);
  int (*mkfs)(const device_t *device);
};
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
VFS_FILE *vfs_fopen(const char *filename, const char *opentype);
int vfs_fclose(VFS_FILE *stream);
int vfs_fread(void *data, size_t size, size_t count, VFS_FILE *stream);
int vfs_fwrite(const void *data, size_t size, size_t count, VFS_FILE *stream);
int vfs_fflush(VFS_FILE *stream);
int vfs_fseek(VFS_FILE *stream, long int offset, int whence);
size_t vfs_ftell(VFS_FILE *stream);

int vfs_unlink(const char *filename);
int vfs_stat(const char *filename, vfs_stat_t *buf);

VFS_DIR *vfs_opendir(const char *dirname);
vfs_dirent_t *vfs_readdir(VFS_DIR *dirstream);
int vfs_closedir(VFS_DIR *dirstream);

int vfs_chdir(const char *filename);
char *vfs_getcwd(char *buffer, size_t size);
int vfs_mkdir(const char *filename, uint32_t mode);
int vfs_rmdir(const char *filename);
int vfs_rename(const char *oldname, const char *newname);

char *vfs_find(const char *file);

int vfs_fprintf(VFS_FILE *fp, const char *fmt, ...);

void vfs_init(void);
int vfs_mount(const device_t *device, const char *type, const char *mount_point);
int vfs_mkfs(const device_t *device, const char *type);
#endif /* _VFS_H */
