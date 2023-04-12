/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
#ifdef USE_FATFS
/* ================================ [ INCLUDES  ] ============================================== */
#include "ff.h"
#include "diskio.h"
#include "vfs.h"
#include <string.h>
#include "heap.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_FATFS 0
#define AS_LOG_FATFSE 3

#define TO_FATFS_PATH(f) fatfsP
#define BEGIN_FATFS_PATH(f)                                                                        \
  do {                                                                                             \
    char cache[2];                                                                                 \
  char *fatfsP = to_fatfs_path(mnt, f, cache)
#define ENDOF_FATFS_PATH(f)                                                                        \
  restore_fatfs_path(mnt, f, cache);                                                               \
  }                                                                                                \
  while (0)

#define TO_FATFS_PATHA(f) fatfsPA
#define TO_FATFS_PATHB(f) fatfsPB
#define BEGIN_FATFS_PATHAB(a, b)                                                                   \
  do {                                                                                             \
    char cachea[2], cacheb[2];                                                                     \
    char *fatfsPA = to_fatfs_path(mnt, a, cachea);                                                 \
  char *fatfsPB = to_fatfs_path(mnt, b, cacheb)
#define ENDOF_FATFS_PATHAB(a, b)                                                                   \
  restore_fatfs_path(mnt, a, cachea);                                                              \
  restore_fatfs_path(mnt, b, cacheb);                                                              \
  }                                                                                                \
  while (0)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const struct vfs_filesystem_ops fatfs_ops;
/* ================================ [ DATAS     ] ============================================== */
/* dirent that will be given to callers;
 * note: both APIs assume that only one dirent ever exists
 */
static vfs_dirent_t dir_ent;

static FATFS fatfs_FatFS[FF_VOLUMES];
static const device_t *fatfs_device_table[FF_VOLUMES];
/* ================================ [ LOCALS    ] ============================================== */
static int get_dev_index(const device_t *device) {
  int index;

  for (index = 0; index < FF_VOLUMES; index++) {
    if (fatfs_device_table[index] == device) {
      break;
    }
  }

  return index;
}

static char *to_fatfs_path(const vfs_mount_t *mnt, const char *filename, char *cache) {
  int index;
  int len;
  char *p;

  len = strlen(mnt->mount_point);
  index = get_dev_index(mnt->device);

  if (len >= 2) {
    p = (char *)&filename[len - 2];
    cache[0] = p[0];
    cache[1] = p[1];
    p[0] = '0' + index;
    p[1] = ':';
  } else if ((0 == index) && (len == 1)) {
    p = (char *)&filename[len];
  } else {
    asAssert(0);
  }

  return p;
}

static char *restore_fatfs_path(const vfs_mount_t *mnt, const char *filename, char *cache) {
  int len;
  char *p = (char *)filename;

  len = strlen(mnt->mount_point);
  if (len >= 2) {
    p = (char *)&filename[len - 2];
    p[0] = cache[0];
    p[1] = cache[1];
  }

  return p;
}

int fatfs_is_root(const char *path) {
  int ret = FALSE;
  int len = strlen(path);

  if ((1 == len) && ('/' == path[0])) {
    ret = TRUE;
  } else if ((2 == len) && (':' == path[1])) {
    ret = TRUE;
  }

  return ret;
}

static VFS_FILE *fatfs_fopen(const vfs_mount_t *mnt, const char *filename, const char *opentype) {
  VFS_FILE *f;
  BYTE flags = 0;
  FRESULT r;

  ASLOG(FATFS, ("fopen(%s, %s)\n", filename, opentype));

  f = heap_malloc(sizeof(VFS_FILE));
  if (NULL == f) {
    return NULL;
  }

  f->priv = heap_malloc(sizeof(FIL));
  if (NULL == f->priv) {
    heap_free(f);
    return NULL;
  }

  while (*opentype != '\0') {
    if (*opentype == 'r') {
      flags |= FA_READ;
    } else if (*opentype == 'w') {
      flags |= FA_WRITE | FA_CREATE_ALWAYS;
    } else if (*opentype == 'a') {
      flags |= FA_OPEN_APPEND;
    } else if (*opentype == '+') {
      flags |= FA_WRITE;
    }
    opentype++;
  }

  BEGIN_FATFS_PATH(filename);
  r = f_open(f->priv, TO_FATFS_PATH(filename), flags);
  ENDOF_FATFS_PATH(filename);
  if (FR_OK != r) {
    heap_free(f->priv);
    heap_free(f);
    return NULL;
  } else {
    f->fops = &fatfs_ops;
  }

  return f;
}

static int fatfs_fclose(VFS_FILE *stream) {
  FRESULT r;

  r = f_close(stream->priv);
  if (FR_OK != r) {
    ASLOG(FATFS, ("fclose failed!(%d)\n", r));
    return EOF;
  }

  heap_free(stream->priv);
  heap_free(stream);

  return 0;
}

static int fatfs_fread(void *data, size_t size, size_t count, VFS_FILE *stream) {
  UINT bytesread;
  FRESULT r;

  ASLOG(FATFS, ("fread(%p, %u, %u, %p)\n", data, (uint32_t)size, (uint32_t)count, stream));

  r = f_read(stream->priv, data, size * count, &bytesread);
  if (FR_OK != r) {
    ASLOG(FATFS, ("fread failed!(%d)\n", r));
    return 0;
  }

  return (bytesread / size);
}

static int fatfs_fwrite(const void *data, size_t size, size_t count, VFS_FILE *stream) {
  UINT byteswritten;
  FRESULT r;

  ASLOG(FATFS, ("fwrite(%p, %u, %u, %p)\n", data, (uint32_t)size, (uint32_t)count, stream));

  r = f_write(stream->priv, data, size * count, &byteswritten);
  if (FR_OK != r) {
    ASLOG(FATFS, ("fwrite failed!(%d)\n", r));
    return 0;
  }

  return (byteswritten / size);
}

static int fatfs_fflush(VFS_FILE *stream) {
  FRESULT r;

  r = f_sync(stream->priv);
  if (FR_OK == r) {
    return 0;
  }

  return EOF;
}

static int fatfs_fseek(VFS_FILE *stream, long int offset, int whence) {
  FRESULT r;

  if (SEEK_SET != whence)
    return EOF;

  r = f_lseek(stream->priv, offset);
  if (FR_OK == r) {
    return 0;
  }

  ASLOG(FATFS, ("fseek failed!(%d)\n", r));

  return EOF;
}

static size_t fatfs_ftell(VFS_FILE *stream) {
  return ((FIL *)(stream->priv))->fptr;
}

static int fatfs_unlink(const vfs_mount_t *mnt, const char *filename) {
  FRESULT r;

  ASLOG(FATFS, ("unlink(%s)\n", filename));

  BEGIN_FATFS_PATH(filename);
  r = f_unlink(TO_FATFS_PATH(filename));
  ENDOF_FATFS_PATH(filename);
  if (FR_OK == r) {
    return 0;
  }

  return EOF;
}

static int fatfs_stat(const vfs_mount_t *mnt, const char *filename, vfs_stat_t *buf) {
  FILINFO f;
  FRESULT r;

  ASLOG(FATFS, ("stat(%s)\n", filename));
  BEGIN_FATFS_PATH(filename);
  if (fatfs_is_root(TO_FATFS_PATH(filename))) { /* just the root */
    f.fsize = 0;
    f.fattrib = AM_DIR;
  } else {
    r = f_stat(TO_FATFS_PATH(filename), &f);

    if (FR_OK != r) {
      ASLOG(FATFS, ("stat failed!(%d)\n", r));
      return ENOENT;
    }
  }
  ENDOF_FATFS_PATH(filename);
  buf->st_size = f.fsize;

  buf->st_mode = 0;

  if (f.fattrib & AM_DIR) {
    buf->st_mode |= S_IFDIR;
  } else {
    buf->st_mode |= S_IFREG;
  }

  if (f.fattrib & AM_RDO) {
    buf->st_mode |= S_IEXEC | S_IREAD;
  } else {
    buf->st_mode |= S_IEXEC | S_IREAD | S_IWRITE;
  }

  return 0;
}

static VFS_DIR *fatfs_opendir(const vfs_mount_t *mnt, const char *dirname) {
  VFS_DIR *dir;
  FRESULT r;

  ASLOG(FATFS, ("opendir(%s)\n", dirname));

  dir = heap_malloc(sizeof(VFS_DIR));

  if (NULL == dir) {
    return NULL;
  }

  dir->priv = heap_malloc(sizeof(DIR));
  if (NULL == dir->priv) {
    heap_free(dir);
    return NULL;
  }
  BEGIN_FATFS_PATH(dirname);
  r = f_opendir(dir->priv, TO_FATFS_PATH(dirname));
  ENDOF_FATFS_PATH(dirname);
  if (FR_OK != r) {
    ASLOG(FATFS, ("opendir(%s) failed!(%d)\n", dirname, r));
    heap_free(dir->priv);
    heap_free(dir);
    return NULL;
  } else {
    dir->fops = &fatfs_ops;
  }

  return dir;
}
static vfs_dirent_t *fatfs_readdir(VFS_DIR *dir) {
  FILINFO fi;
  FRESULT r;

  r = f_readdir(dir->priv, &fi);

  if (FR_OK != r) {
    return NULL;
  }

  if (0 == fi.fname[0]) {
    return NULL;
  }

  dir_ent.d_namlen = strnlen(fi.fname, sizeof(fi.fname));

  strcpy(dir_ent.d_name, fi.fname);

  return &dir_ent;
}

static int fatfs_closedir(VFS_DIR *dir) {
  FRESULT r;

  r = f_closedir(dir->priv);

  if (FR_OK == r) {
    heap_free(dir->priv);
    heap_free(dir);
    return 0;
  }

  return EBADF;
}

static int fatfs_chdir(const vfs_mount_t *mnt, const char *filename) {
  FRESULT r;

  ASLOG(FATFS, ("chdir(%s)\n", filename));
  BEGIN_FATFS_PATH(filename);
  if (('\0' == TO_FATFS_PATH(filename)[0])) {
    r = f_chdir("/");
  } else {
    r = f_chdir(TO_FATFS_PATH(filename));
  }
  ENDOF_FATFS_PATH(filename);
  if (FR_OK == r) {
    return 0;
  }

  ASLOG(FATFS, ("chdir(%s) failed!(%d)\n", filename, r));

  return ENOTDIR;
}

static int fatfs_mkdir(const vfs_mount_t *mnt, const char *filename, uint32_t mode) {
  FRESULT r;

  ASLOG(FATFS, ("mkdir(%s,  0x%x)\n", filename, mode));
  BEGIN_FATFS_PATH(filename);
  r = f_mkdir(TO_FATFS_PATH(filename));
  ENDOF_FATFS_PATH(filename);

  if (FR_OK == r) {
    return 0;
  }

  ASLOG(FATFS, ("mkdir failed!(%d)\n", r));
  if (FR_EXIST == r) {
    return EEXIST;
  }

  return EACCES;
}

static int fatfs_rmdir(const vfs_mount_t *mnt, const char *filename) {
  FRESULT r;

  ASLOG(FATFS, ("rmdir(%s)\n", filename));

  BEGIN_FATFS_PATH(filename);
  r = f_rmdir(TO_FATFS_PATH(filename));
  ENDOF_FATFS_PATH(filename);

  if (FR_OK == r) {
    return 0;
  }

  return EACCES;
}

static int fatfs_rename(const vfs_mount_t *mnt, const char *oldname, const char *newname) {
  FRESULT r;

  ASLOG(FATFS, ("rename(%s, %s)\n", oldname, newname));

  BEGIN_FATFS_PATHAB(oldname, newname);
  r = f_rename(TO_FATFS_PATHA(oldname), TO_FATFS_PATHB(newname));
  ENDOF_FATFS_PATHAB(oldname, newname);

  if (FR_OK == r) {
    return 0;
  }

  return EACCES;
}

static int get_dev_index_or_alloc(const device_t *device) {
  int index;

  for (index = 0; index < FF_VOLUMES; index++) {
    if (fatfs_device_table[index] == device) {
      break;
    } else if (NULL == fatfs_device_table[index]) {
      fatfs_device_table[index] = device;
      break;
    }
  }

  return index;
}

static int fatfs_mount(const device_t *device, const char *mount_point) {
  int ercd;
  int index;
  char mp[3] = "0:";

  index = get_dev_index_or_alloc(device);

  if (index < FF_VOLUMES) {
    mp[0] += index;
    ercd = f_mount(&fatfs_FatFS[index], mp, 1);

    if (0 != ercd) {
      fatfs_device_table[index] = NULL;
    }
  } else {
    ercd = -1;
  }

  return ercd;
}

static int fatfs_mkfs(const device_t *device) {
  int ercd;
  int index;
  char mp[3] = "0:";
  MKFS_PARM opt;

  index = get_dev_index_or_alloc(device);

  if (index < FF_VOLUMES) {
    mp[0] += index;
    opt.fmt = FM_ANY;
    opt.n_fat = 0;
    opt.align = 0;
    opt.au_size = 0;
    opt.n_root = 0;
    ercd = f_mkfs(mp, &opt, fatfs_FatFS[index].win, sizeof(fatfs_FatFS[index].win));
    if (FR_OK != ercd) {
      fatfs_device_table[index] = NULL;
      ASLOG(FATFSE, ("mkfs error %d\n", ercd));
    }
  } else {
    ercd = -1;
  }

  return ercd;
}
/* ================================ [ FUNCTIONS ] ============================================== */
const struct vfs_filesystem_ops fatfs_ops = {
  /*.name =*/"vfat",
  /*.fopen =*/fatfs_fopen,
  /*.fclose =*/fatfs_fclose,
  /*.fread =*/fatfs_fread,
  /*.fwrite =*/fatfs_fwrite,
  /*.fflush =*/fatfs_fflush,
  /*.fseek =*/fatfs_fseek,
  /*.ftell =*/fatfs_ftell,
  /*.unlink =*/fatfs_unlink,
  /*.stat =*/fatfs_stat,
  /*.opendir =*/fatfs_opendir,
  /*.readdir =*/fatfs_readdir,
  /*.closedir =*/fatfs_closedir,
  /*.chdir =*/fatfs_chdir,
  /*.mkdir =*/fatfs_mkdir,
  /*.rmdir =*/fatfs_rmdir,
  /*.rename =*/fatfs_rename,
  /*.mount =*/fatfs_mount,
  /*.mkfs =*/fatfs_mkfs};

DSTATUS disk_status(BYTE pdrv /* Physical drive nmuber to identify the drive */
) {
  DSTATUS stat = STA_NOINIT;
  const device_t *device;

  if (pdrv < FF_VOLUMES) {
    device = fatfs_device_table[pdrv];
    if (device != NULL) {
      stat = RES_OK;
    }
  }
  return stat;
}

DSTATUS disk_initialize(BYTE pdrv /* Physical drive nmuber to identify the drive */
) {
  DSTATUS stat = STA_NOINIT;
  const device_t *device;

  ASLOG(FATFS, ("disk_initialize %d\n", (uint32_t)pdrv));

  if (pdrv < FF_VOLUMES) {
    device = fatfs_device_table[pdrv];
    if ((device != NULL) && (device->ops.open != NULL)) {
      stat = device->ops.open(device);
    }
  }
  return stat;
}

DRESULT disk_read(BYTE pdrv,    /* Physical drive nmuber to identify the drive */
                  BYTE *buff,   /* Data buffer to store read data */
                  DWORD sector, /* Start sector in LBA */
                  UINT count    /* Number of sectors to read */
) {
  DRESULT res = RES_PARERR;
  const device_t *device;
  ASLOG(FATFS, ("disk_read %d %d %d\n", (uint32_t)pdrv, (uint32_t)sector, (uint32_t)count));

  if (pdrv < FF_VOLUMES) {
    device = fatfs_device_table[pdrv];
    if ((device != NULL) && (device->ops.read != NULL)) {
      res = (DRESULT)device->ops.read(device, sector, buff, count);
    }
  }

  return res;
}

DRESULT disk_write(BYTE pdrv,        /* Physical drive nmuber to identify the drive */
                   const BYTE *buff, /* Data to be written */
                   DWORD sector,     /* Start sector in LBA */
                   UINT count        /* Number of sectors to write */
) {
  DRESULT res = RES_PARERR;
  const device_t *device;
  ASLOG(FATFS, ("disk_write %d %d %d\n", (uint32_t)pdrv, (uint32_t)sector, (uint32_t)count));

  if (pdrv < FF_VOLUMES) {
    device = fatfs_device_table[pdrv];
    if ((device != NULL) && (device->ops.write != NULL)) {
      res = (DRESULT)device->ops.write(device, sector, buff, count);
    }
  }

  return res;
}

DRESULT disk_ioctl(BYTE pdrv, /* Physical drive nmuber (0..) */
                   BYTE cmd,  /* Control code */
                   void *buff /* Buffer to send/receive control data */
) {
  DRESULT res = RES_PARERR;
  const device_t *device;
  size_t sz;
  ASLOG(FATFS, ("disk_ioctl %d %d\n", (uint32_t)pdrv, (uint32_t)cmd));

  if (pdrv < FF_VOLUMES) {
    device = fatfs_device_table[pdrv];
    if ((device != NULL) && (device->ops.ctrl != NULL)) {
      switch (cmd) {
      case CTRL_SYNC:
        res = RES_OK;
        break;

      case GET_SECTOR_COUNT: {
        res = (DRESULT)device->ops.ctrl(device, DEVICE_CTRL_GET_SECTOR_COUNT, &sz);
        *(DWORD *)buff = sz;
        break;
      }
      case GET_SECTOR_SIZE:
        res = (DRESULT)device->ops.ctrl(device, DEVICE_CTRL_GET_SECTOR_SIZE, &sz);
        *(DWORD *)buff = sz;
        break;

      case GET_BLOCK_SIZE:
        res = (DRESULT)device->ops.ctrl(device, DEVICE_CTRL_GET_BLOCK_SIZE, &sz);
        *(DWORD *)buff = sz;
        break;
      }
    }
  }

  return res;
}

DWORD get_fattime(void) {
  return 0;
}
#endif
