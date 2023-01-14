/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
#ifdef USE_LWEXT4
/* ================================ [ INCLUDES  ] ============================================== */
#include "vfs.h"
#include "ext4.h"
#include "ext4_mkfs.h"
#include "Std_Debug.h"
#include "heap.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_LWEXT 0
#define AS_LOG_LWEXTE 1
#define TO_LWEXT_PATH(f) (&((f)[0]))
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const struct vfs_filesystem_ops lwext_ops;

static int blockdev_open(struct ext4_blockdev *bdev);
static int blockdev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt);
static int blockdev_bwrite(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id,
                           uint32_t blk_cnt);
static int blockdev_close(struct ext4_blockdev *bdev);
static int blockdev_lock(struct ext4_blockdev *bdev);
static int blockdev_unlock(struct ext4_blockdev *bdev);
/* ================================ [ DATAS     ] ============================================== */
static const device_t *lwext_device_table[CONFIG_EXT4_BLOCKDEVS_COUNT];

static size_t disk_sector_size[CONFIG_EXT4_BLOCKDEVS_COUNT] = {0};

EXT4_BLOCKDEV_STATIC_INSTANCE(ext4_blkdev0, 4096, 0, blockdev_open, blockdev_bread, blockdev_bwrite,
                              blockdev_close, blockdev_lock, blockdev_unlock);

#if CONFIG_EXT4_BLOCKDEVS_COUNT > 1
EXT4_BLOCKDEV_STATIC_INSTANCE(ext4_blkdev1, 4096, 0, blockdev_open, blockdev_bread, blockdev_bwrite,
                              blockdev_close, blockdev_lock, blockdev_unlock);
#endif
#if CONFIG_EXT4_BLOCKDEVS_COUNT > 2
EXT4_BLOCKDEV_STATIC_INSTANCE(ext4_blkdev2, 4096, 0, blockdev_open, blockdev_bread, blockdev_bwrite,
                              blockdev_close, blockdev_lock, blockdev_unlock);
#endif
#if CONFIG_EXT4_BLOCKDEVS_COUNT > 3
EXT4_BLOCKDEV_STATIC_INSTANCE(ext4_blkdev3, 4096, 0, blockdev_open, blockdev_bread, blockdev_bwrite,
                              blockdev_close, blockdev_lock, blockdev_unlock);
#endif

#if CONFIG_EXT4_BLOCKDEVS_COUNT > 4
#error vfs_lwext4 by default support only 4 partitions!
#endif

static struct ext4_blockdev *const ext4_blkdev_list[CONFIG_EXT4_BLOCKDEVS_COUNT] = {
  &ext4_blkdev0,
#if CONFIG_EXT4_BLOCKDEVS_COUNT > 1
  &ext4_blkdev1,
#endif
#if CONFIG_EXT4_BLOCKDEVS_COUNT > 2
  &ext4_blkdev2,
#endif
#if CONFIG_EXT4_BLOCKDEVS_COUNT > 3
  &ext4_blkdev3,
#endif
};
/* ================================ [ LOCALS    ] ============================================== */
static VFS_FILE *lwext_fopen(const vfs_mount_t *mnt, const char *filename, const char *opentype) {
  VFS_FILE *f;
  int r;

  ASLOG(LWEXT, ("fopen(%s, %s)\n", filename, opentype));

  f = heap_malloc(sizeof(VFS_FILE));
  if (NULL != f) {
    f->priv = heap_malloc(sizeof(ext4_file));
    if (NULL == f->priv) {
      heap_free(f);
      f = NULL;
    } else {
      r = ext4_fopen(f->priv, TO_LWEXT_PATH(filename), opentype);
      if (0 != r) {
        heap_free(f->priv);
        heap_free(f);
        f = NULL;
      } else {
        f->fops = &lwext_ops;
      }
    }
  }

  return f;
}

static int lwext_fclose(VFS_FILE *stream) {
  int r;

  r = ext4_fclose(stream->priv);

  if (0 == r) {
    heap_free(stream->priv);
    heap_free(stream);
  }

  return r;
}

static int lwext_fread(void *data, size_t size, size_t count, VFS_FILE *stream) {
  size_t bytesread = 0;
  int r;

  r = ext4_fread(stream->priv, data, size * count, &bytesread);
  if (0 != r) {
    bytesread = 0;
  }

  return (bytesread / size);
}

static int lwext_fwrite(const void *data, size_t size, size_t count, VFS_FILE *stream) {
  size_t byteswritten = 0;
  int r;

  r = ext4_fwrite(stream->priv, data, size * count, &byteswritten);
  if (0 != r) {
    byteswritten = 0;
  }

  return (byteswritten / size);
}

static int lwext_fflush(VFS_FILE *stream) {
  (void)stream;

  return ENOTSUP;
}

static int lwext_fseek(VFS_FILE *stream, long int offset, int whence) {
  int r;

  r = ext4_fseek(stream->priv, offset, whence);

  return r;
}

static size_t lwext_ftell(VFS_FILE *stream) {
  return ext4_ftell(stream->priv);
}

static int lwext_unlink(const vfs_mount_t *mnt, const char *filename) {
  int r;

  ASLOG(LWEXT, ("unlink(%s)\n", filename));

  r = ext4_fremove(TO_LWEXT_PATH(filename));

  return r;
}

static int lwext_stat(const vfs_mount_t *mnt, const char *filename, vfs_stat_t *buf) {
  int r = ENOENT;

  ASLOG(LWEXT, ("stat(%s)\n", filename));

  if (('\0' == TO_LWEXT_PATH(filename)[0]) ||
      (0 == strcmp(TO_LWEXT_PATH(filename), "/"))) { /* just the root */
    buf->st_mode = S_IFDIR;
    buf->st_size = 0;

    r = 0;
  } else {
    union {
      ext4_dir dir;
      ext4_file f;
    } var;

    r = ext4_dir_open(&(var.dir), TO_LWEXT_PATH(filename));

    if (0 == r) {
      (void)ext4_dir_close(&(var.dir));
      buf->st_mode = S_IFDIR;
      buf->st_size = 0;
    } else {
      r = ext4_fopen(&(var.f), TO_LWEXT_PATH(filename), "rb");
      if (0 == r) {
        buf->st_mode = S_IFREG;
        buf->st_size = ext4_fsize(&(var.f));
        (void)ext4_fclose(&(var.f));
      }
    }
  }

  return r;
}

static VFS_DIR *lwext_opendir(const vfs_mount_t *mnt, const char *dirname) {
  VFS_DIR *dir;

  ASLOG(LWEXT, ("opendir(%s)\n", dirname));

  dir = heap_malloc(sizeof(VFS_DIR));

  if (NULL != dir) {
    const char *p;
    dir->fops = &lwext_ops;
    dir->priv = heap_malloc(sizeof(ext4_dir));

    if (('\0' == TO_LWEXT_PATH(dirname)[0])) {
      p = "/";
    } else {
      p = TO_LWEXT_PATH(dirname);
    }

    if (NULL != dir->priv) {
      int r;
      r = ext4_dir_open(dir->priv, p);

      if (0 != r) {
        heap_free(dir->priv);
        heap_free(dir);
        dir = NULL;
        ASLOG(LWEXT, ("opendir(%s) failed!(%d)\n", p, r));
      }
    } else {
      heap_free(dir);
      dir = NULL;
    }
  }

  return dir;
}

static vfs_dirent_t *lwext_readdir(VFS_DIR *dirstream) {
  const ext4_direntry *rentry;
  vfs_dirent_t *rdirent;

  static vfs_dirent_t dirent;

  rentry = ext4_dir_entry_next(dirstream->priv);

  if (NULL != rentry) {
    dirent.d_namlen = rentry->name_length;

    strcpy(dirent.d_name, (char *)rentry->name);

    rdirent = &dirent;

  } else {
    rdirent = NULL;
  }

  return rdirent;
}

static int lwext_closedir(VFS_DIR *dirstream) {
  (void)ext4_fclose(dirstream->priv);

  heap_free(dirstream->priv);
  heap_free(dirstream);

  return 0;
}

static int lwext_chdir(const vfs_mount_t *mnt, const char *filename) {

  int r = ENOTDIR;

  ASLOG(LWEXT, ("chdir(%s)\n", filename));

  if (('\0' == TO_LWEXT_PATH(filename)[0])) {
    r = 0;
  } else {
    r = ext4_inode_exist(TO_LWEXT_PATH(filename), EXT4_DE_DIR);
  }

  return r;
}

static int lwext_mkdir(const vfs_mount_t *mnt, const char *filename, uint32_t mode) {
  int r;

  ASLOG(LWEXT, ("mkdir(%s,  0x%x)\n", filename, mode));

  r = ext4_dir_mk(TO_LWEXT_PATH(filename));

  return r;
}

static int lwext_rmdir(const vfs_mount_t *mnt, const char *filename) {
  int r;

  ASLOG(LWEXT, ("rmdir(%s)\n", filename));

  r = ext4_dir_rm(TO_LWEXT_PATH(filename));

  return r;
}

static int lwext_rename(const vfs_mount_t *mnt, const char *oldname, const char *newname) {
  int r;

  ASLOG(LWEXT, ("rename (%s,  %s)\n", oldname, newname));

  r = ext4_frename(TO_LWEXT_PATH(oldname), TO_LWEXT_PATH(newname));

  return r;
}

static int get_dev_index_or_alloc(const device_t *device) {
  int index;

  for (index = 0; index < CONFIG_EXT4_BLOCKDEVS_COUNT; index++) {
    if (lwext_device_table[index] == device) {
      break;
    } else if (NULL == lwext_device_table[index]) {
      lwext_device_table[index] = device;
      break;
    }
  }

  return index;
}

static int lwext_mount(const device_t *device, const char *mount_point) {
  int ercd = 0;
  int index;
  struct ext4_blockdev *bd;

  index = get_dev_index_or_alloc(device);

  if (index < CONFIG_EXT4_BLOCKDEVS_COUNT) {
    bd = ext4_blkdev_list[index];
    ercd = ext4_device_register(bd, device->name);
    if (0 == ercd) {
      ercd = ext4_mount(device->name, mount_point, false);
    }

    if (ercd != 0) {
      (void)ext4_device_unregister(device->name);
      lwext_device_table[index] = NULL;
    }
  } else {
    ercd = -1;
  }

  return ercd;
}

static int lwext_mkfs(const device_t *device) {
  int ercd = 0;
  int index;
  struct ext4_blockdev *bd;
  struct ext4_fs *fs;
  struct ext4_mkfs_info *info;

  index = get_dev_index_or_alloc(device);

  if (index < CONFIG_EXT4_BLOCKDEVS_COUNT) {
    bd = ext4_blkdev_list[index];
    ercd = ext4_device_register(bd, device->name);
    if (0 == ercd) {
      fs = heap_malloc(sizeof(struct ext4_fs) + sizeof(struct ext4_mkfs_info));
      if (fs != NULL) {
        info = (struct ext4_mkfs_info *)(fs + 1);
        info->block_size = 4096, info->journal = TRUE, ercd = ext4_mkfs(fs, bd, info, F_SET_EXT4);

        heap_free(fs);
      } else {
        ercd = ENOMEM;
      }

      (void)ext4_device_unregister(device->name);
      lwext_device_table[index] = NULL;
    }
  } else {
    ercd = -1;
  }

  return ercd;
}

static int get_bdev(struct ext4_blockdev *bdev) {
  int index;
  int ret = -1;

  for (index = 0; index < CONFIG_EXT4_BLOCKDEVS_COUNT; index++) {
    if (ext4_blkdev_list[index] == bdev) {
      ret = index;
      break;
    }
  }

  return ret;
}

static int blockdev_open(struct ext4_blockdev *bdev) {
  size_t size;
  int index;
  int ret = -1;
  const device_t *device;

  index = get_bdev(bdev);

  if (index >= 0) {
    device = lwext_device_table[index];
    if ((device != NULL) && (device->ops.open != NULL) && (device->ops.ctrl != NULL)) {
      ret = device->ops.open(device);
      ret += device->ops.ctrl(device, DEVICE_CTRL_GET_SECTOR_SIZE, &disk_sector_size[index]);
      ret += device->ops.ctrl(device, DEVICE_CTRL_GET_DISK_SIZE, &size);
    }
  }

  if (0 == ret) {
    bdev->part_offset = 0;
    bdev->part_size = size;
    bdev->bdif->ph_bcnt = bdev->part_size / bdev->bdif->ph_bsize;
  }

  return ret;
}

static int blockdev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
                          uint32_t blk_cnt) {
  int index;
  int ret = -1;
  const device_t *device;

  index = get_bdev(bdev);

  if (index >= 0) {
    device = lwext_device_table[index];
    if ((device != NULL) && (device->ops.read != NULL)) {
      ret = device->ops.read(device, blk_id * (bdev->bdif->ph_bsize / disk_sector_size[index]), buf,
                             blk_cnt * (bdev->bdif->ph_bsize / disk_sector_size[index]));
    }
  }

  return ret;
}

static int blockdev_bwrite(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id,
                           uint32_t blk_cnt) {
  int index;
  int ret = -1;
  const device_t *device;

  index = get_bdev(bdev);

  if (index >= 0) {
    device = lwext_device_table[index];
    if ((device != NULL) && (device->ops.write != NULL)) {
      ret = device->ops.write(device, blk_id * (bdev->bdif->ph_bsize / disk_sector_size[index]),
                              buf, blk_cnt * (bdev->bdif->ph_bsize / disk_sector_size[index]));
    }
  }

  return ret;
}

static int blockdev_close(struct ext4_blockdev *bdev) {
  return 0;
}

static int blockdev_lock(struct ext4_blockdev *bdev) {
  return 0;
}

static int blockdev_unlock(struct ext4_blockdev *bdev) {
  return 0;
}
/* ================================ [ FUNCTIONS ] ============================================== */
const struct vfs_filesystem_ops lwext_ops = {.name = "ext",
                                             .fopen = lwext_fopen,
                                             .fclose = lwext_fclose,
                                             .fread = lwext_fread,
                                             .fwrite = lwext_fwrite,
                                             .fflush = lwext_fflush,
                                             .fseek = lwext_fseek,
                                             .ftell = lwext_ftell,
                                             .unlink = lwext_unlink,
                                             .stat = lwext_stat,
                                             .opendir = lwext_opendir,
                                             .readdir = lwext_readdir,
                                             .closedir = lwext_closedir,
                                             .chdir = lwext_chdir,
                                             .mkdir = lwext_mkdir,
                                             .rmdir = lwext_rmdir,
                                             .rename = lwext_rename,
                                             .mount = lwext_mount,
                                             .mkfs = lwext_mkfs};
#endif
