/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "vfs.h"
#include <string.h>
#include "heap.h"
#include <stdarg.h>
#include <sys/queue.h>
#include "Std_Debug.h"
#include "Std_Critical.h"
#ifdef USE_SHELL
#include "shell.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifndef ELF_EXPORT_ALIAS
#define ELF_EXPORT_ALIAS(a, b)
#endif

#define AS_LOG_VFS 0
#define in_range(c, lo, up) (((uint8_t)c >= lo) && ((uint8_t)c <= up))
#define isprint(c) in_range(c, 0x20, 0x7f)
#ifndef VFS_FPRINTF_BUFFER_SIZE
#define VFS_FPRINTF_BUFFER_SIZE 512
#endif

#ifndef VFS_LOCK
#define VFS_LOCK() EnterCritical()
#endif

#ifndef VFS_UNLOCK
#define VFS_UNLOCK() ExitCritical()
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
#ifdef USE_FATFS
extern const struct vfs_filesystem_ops fatfs_ops;
#endif
#ifdef USE_LWEXT4
extern const struct vfs_filesystem_ops lwext_ops;
#endif
#if defined(_WIN32) || defined(linux)
extern const struct vfs_filesystem_ops hofs_ops;
#endif
/* ================================ [ DATAS     ] ============================================== */
static const struct vfs_filesystem_ops *vfs_ops[] = {
#ifdef USE_FATFS
  &fatfs_ops,
#endif
#ifdef USE_LWEXT4
  &lwext_ops,
#endif
#if defined(_WIN32) || defined(linux)
  &hofs_ops,
#endif
  NULL};

static TAILQ_HEAD(vfs_mount_head, vfs_mount_s) vfs_mount_list = TAILQ_HEAD_INITIALIZER(vfs_mount_list);

static char vfs_cwd[FILENAME_MAX] = "/";
/* ================================ [ LOCALS    ] ============================================== */
static char *serach(char *const p, const char *file) {
  VFS_DIR *dir;
  vfs_dirent_t *dirent;
  vfs_stat_t stat;
  size_t lenp;
  char *cs;

  lenp = strlen(p);
  if ('/' != p[lenp - 1]) {
    p[lenp] = '/';
    p[lenp + 1] = '\0';
    lenp += 1;
  }
  cs = &p[lenp];

  strcpy(cs, file);
  if ((0 == vfs_stat(p, &stat)) && VFS_ISREG(stat.st_mode)) {
    return p;
  }

  cs[0] = '\0';
  dir = vfs_opendir(p);
  if (NULL != dir) {
    dirent = vfs_readdir(dir);
    while (NULL != dirent) {
      strcpy(cs, dirent->d_name);
      if ((0 == vfs_stat(p, &stat)) && VFS_ISDIR(stat.st_mode)) {
        if ((0 != strcmp(dirent->d_name, ".")) && (0 != strcmp(dirent->d_name, ".."))) {
          char *r = serach(p, file);
          if (r) {
            vfs_closedir(dir);
            return r;
          }
        }
      }
      dirent = vfs_readdir(dir);
    }
    vfs_closedir(dir);
  }

  return NULL;
}

static const struct vfs_filesystem_ops *search_ops(const char *type) {
  const struct vfs_filesystem_ops *ops, **o;

  o = vfs_ops;
  ops = NULL;
  while (*o != NULL) {
    if (0 == strcmp((*o)->name, type)) {
      ops = *o;
      break;
    }
    o++;
  }

  ASLOG(VFS, ("search_ops(%s) = %s\n", type, (NULL != ops) ? ops->name : NULL));

  return ops;
}

static const vfs_mount_t *search_mnt(const char *filepath) {
  const vfs_mount_t *mnt, *m;
  int len;
  int best = 0;

  mnt = NULL;

  VFS_LOCK();
  TAILQ_FOREACH(m, &vfs_mount_list, entry) {
    len = strlen(m->mount_point);
    if ((len > 1) && ('/' == m->mount_point[len - 1])) {
      len = len - 1;
    }
    if (0 == strncmp(m->mount_point, filepath, len)) {
      if (best < len) {
        mnt = m;
        best = len;
      }
    }
  }
  VFS_UNLOCK();

  ASLOG(VFS, ("search_mnt(%s) = %s\n", filepath, (NULL != mnt) ? mnt->mount_point : NULL));

  return mnt;
}

static char *relpath(const char *path) {
  char *abspath;
  char *p;
  const char *s;

  abspath = (char *)heap_malloc(FILENAME_MAX);

  if (NULL != abspath) {
    p = abspath;

    memset(p, 0, FILENAME_MAX);

    if ('/' != path[0]) { /* relative path */
      s = vfs_cwd;
      while ('\0' != *s) {
        *p = *s;
        p++;
        s++;
      }

      if (*(p - 1) != '/') {
        *p = '/';
        p++;
      }

      s = path;
    } else {
      s = path;
    }

    while ('\0' != *s) {
      if (('.' == *s) && ('.' == *(s + 1))) {
        if (('/' == *(p - 1)) && ((p - abspath) >= 2)) {
          p = p - 2;
        }

        while (('/' != *p) && (p > abspath)) {
          p--;
        }

        s = s + 2;
      } else if ('.' == *s) {
        if (('/' == *(p - 1)) && (('/' == *(s + 1)) || ('\0' == *(s + 1)))) {
          p = p - 1;
        } else {
          *p = *s;
          p++;
        }
        s++;
      } else if (('/' == *s) && ('/' == *(p - 1))) {
        /* skip extra '/' */
        s++;
      } else {
        *p = *s;
        p++;
        s++;
      }
    }

    if (p == abspath) {
      *p = '/';
      p++;
    }

    *p = '\0';
  }

  ASLOG(VFS, ("relpath(%s) = %s\n", path, abspath));
  return abspath;
}
#ifdef USE_SHELL
static int lsFunc(int argc, const char *argv[]) {
  int r = 0;
  const char *path;
  VFS_DIR *dir;
  vfs_dirent_t *dirent;
  vfs_stat_t st;
  char *rpath = NULL;

  if (1 == argc) {
    path = vfs_cwd;
  } else {
    path = argv[1];
    rpath = (char *)heap_malloc(FILENAME_MAX);
  }

  dir = vfs_opendir(path);
  if (NULL != dir) {
    dirent = vfs_readdir(dir);
    while (NULL != dirent) {
      if ((0 == strcmp(dirent->d_name, "..")) || (0 == strcmp(dirent->d_name, "."))) {
        dirent = vfs_readdir(dir);
        continue;
      }
      if (rpath != NULL) {
        int r = strlen(path);
        strcpy(rpath, path);
        if (rpath[r - 1] != '/') {
          rpath[r] = '/';
          r += 1;
        }
        strcpy(&rpath[r], dirent->d_name);
        r = vfs_stat(rpath, &st);
      } else {
        r = vfs_stat(dirent->d_name, &st);
      }
      if (0 == r) {
        PRINTF("%srw-rw-rw- 1 as vfs %11u %s\r\n", VFS_ISDIR(st.st_mode) ? "d" : "-",
               (uint32_t)st.st_size, dirent->d_name);
        dirent = vfs_readdir(dir);
      } else {
        dirent = NULL; /* stat error, stop listing */
      }
    }
    vfs_closedir(dir);
  } else {
    r = vfs_stat(path, &st);
    if (0 == r) {
      PRINTF("-rw-rw-rw- 1 as vfs %11u %s\r\n", (uint32_t)st.st_size, path);
    }
  }

  if (NULL != rpath) {
    heap_free(rpath);
  }

  return r;
}
SHELL_REGISTER(ls,
               "ls [path]\n"
               "  list files of current directory or path directory\n",
               lsFunc);

static int chdirFunc(int argc, const char *argv[]) {
  int r;

  if (2 == argc) {
    r = vfs_chdir(argv[1]);
  } else {
    r = vfs_chdir("/");
  }

  return r;
}
SHELL_REGISTER(cd,
               "cd path\n"
               "  change current working directory\n",
               chdirFunc);

static int pwdFunc(int argc, const char *argv[]) {
  PRINTF("\n%s\n", vfs_cwd);
  return 0;
}
SHELL_REGISTER(pwd, "show full path of current working directory\n", pwdFunc);

static int mkdirFunc(int argc, const char *argv[]) {
  return vfs_mkdir(argv[1], 0);
}
SHELL_REGISTER(mkdir,
               "mkdir path\n"
               "  making a directory specified by path\n",
               mkdirFunc);

static int rmFunc(int argc, const char *argv[]) {
  int r;
  vfs_stat_t st;

  r = vfs_stat(argv[1], &st);

  if (0 == r) {
    if (VFS_ISDIR(st.st_mode)) {
      r = vfs_rmdir(argv[1]);
    } else {
      r = vfs_unlink(argv[1]);
    }
  }

  return r;
}
SHELL_REGISTER(rm,
               "rm path\n"
               "  remove a directory or file specified by path\n",
               rmFunc);

static int cpFunc(int argc, const char *argv[]) {
  int r = -1;
  int len;
  VFS_FILE *fps;
  VFS_FILE *fpt;
  char *buf = heap_malloc(512);

  if (NULL != buf) {
    fps = vfs_fopen(argv[1], "rb");
    if (NULL == fps) {
      PRINTF("open %s failed!\n", argv[1]);
    } else {
      fpt = vfs_fopen(argv[2], "wb");
      if (NULL == fpt) {
        PRINTF("create %s failed!\n", argv[2]);
        vfs_fclose(fps);
      } else {
        do {
          len = vfs_fread(buf, 1, 512, fps);
          if (len > 0) {
            r = vfs_fwrite(buf, 1, len, fpt);
            if (len != r) {
              PRINTF("write to %s failed!\n", argv[2]);
              r = -2;
              break;
            } else {
              r = 0;
            }
          }
        } while (len > 0);

        vfs_fclose(fps);
        vfs_fclose(fpt);
      }
    }
    heap_free(buf);
  } else {
    r = -ENOMEM;
  }

  return r;
}
SHELL_REGISTER(cp,
               "cp file path\n"
               "  copy a file to another path\n",
               cpFunc);

static int catFunc(int argc, const char *argv[]) {
  VFS_FILE *f;
  int r = 0;
  char *buf = heap_malloc(512 + 1);

  if (NULL != buf) {
    f = vfs_fopen(argv[1], "r");
    if (NULL != f) {
      do {
        r = vfs_fread(buf, 1, 512, f);
        if (r > 0) {
          buf[r] = '\0';
          PRINTF("%s", buf);
        }
      } while (r > 0);
      vfs_fclose(f);
    } else {
      r = -EEXIST;
    }
    heap_free(buf);
  } else {
    r = -ENOMEM;
  }
  return r;
}
SHELL_REGISTER(cat,
               "cat file\n"
               "  show file content in ascii mode\n",
               catFunc);

static int hexdumpFunc(int argc, const char *argv[]) {
  VFS_FILE *f;
  uint8_t buf[16];
  int r = 0;
  int i;
  unsigned long size = -1;
  unsigned long offset = 0;
  const char *file = NULL;

  for (i = 1; i < argc; i++) {
    if (0 == strcmp(argv[i], "-s")) {
      if (0 == strncmp(argv[i + 1], "0x", 2)) {
        offset = strtoul(argv[i + 1] + 2, NULL, 16);
      } else {
        offset = strtoul(argv[i + 1], NULL, 10);
      }
      i++;
    } else if (0 == strcmp(argv[i], "-n")) {
      size = strtoul(argv[i + 1], NULL, 10);
      i++;
    } else {
      file = argv[i];
    }
  }

  if (NULL == file) {
    r = -EINVAL;
  } else {
    f = vfs_fopen(argv[1], "rb");
    if (NULL != f) {
      r = vfs_fseek(f, (long int)offset, SEEK_SET);
      if (0 == r) {
        PRINTF("         :: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
        do {
          r = vfs_fread(buf, 1, 16, f);
          if (r > 0) {
            PRINTF("%08X ::", (uint32_t)offset);
            for (i = 0; i < 16; i++) {
              if (i < r) {
                PRINTF(" %02X", buf[i]);
              } else {
                PRINTF("   ");
              }
            }
            PRINTF("\t");
            for (i = 0; i < 16; i++) {
              if (i >= r)
                break;
              if (isprint(buf[i])) {
                PRINTF("%c", buf[i]);
              } else {
                PRINTF(".");
              }
            }
            PRINTF("\n");
          }
          offset += 16;
          size = (size > 16) ? (size - 16) : 0;
        } while ((r > 0) && (size > 0));
      }
      vfs_fclose(f);
      r = 0;
    } else {
      r = -EEXIST;
    }
  }

  return r;
}
SHELL_REGISTER(hexdump,
               "hexdump file [-s offset -n size]\n"
               "  show file content in hex mode\n",
               hexdumpFunc);

static int mountFunc(int argc, const char *argv[]) {
  int r = 0;
  const vfs_mount_t *m;
  const char *dname;
  const char *fmt;
  const char *mount_point;
  const device_t *device;
  if (argc == 1) {
    VFS_LOCK();
    TAILQ_FOREACH(m, &vfs_mount_list, entry) {
      PRINTF("mount device %s(%s) to %s\n", m->device->name, m->ops->name, m->mount_point);
    }
    VFS_UNLOCK();
  } else if (argc == 4) {
    dname = argv[1];
    fmt = argv[2];
    mount_point = argv[3];
    device = device_find(dname);
    if (NULL != device) {
      r = vfs_mount(device, fmt, mount_point);
      if (0 == r) {
        PRINTF("mount device %s(%s) to %s\n", dname, fmt, mount_point);
      } else {
        PRINTF("mount device %s(%s) to %s: error %d\n", dname, fmt, mount_point, r);
      }
    } else {
      r = -2;
    }
  } else {
    r = -1;
  }
  return r;
}
SHELL_REGISTER(mount, "mount device format dir\n", mountFunc);

static int mkfsFunc(int argc, const char *argv[]) {
  int r = 0;
  const struct vfs_filesystem_ops **o;
  const char *dname;
  const char *fmt;
  const device_t *device;
  if ((2 == argc) && (0 == strcmp(argv[1], "-h"))) {
    PRINTF("mkfs device format\n  format: ");
    o = vfs_ops;
    while (*o != NULL) {
      PRINTF(" %s", (*o)->name);
      o++;
    }
    PRINTF("\n");
  } else if (3 == argc) {
    dname = argv[1];
    fmt = argv[2];
    device = device_find(dname);
    if (NULL != device) {
      r = vfs_mkfs(device, fmt);
      if (0 == r) {
        PRINTF("mkfs device %s to %s\n", dname, fmt);
      } else {
        PRINTF("mkfs device %s to %s: error %d\n", dname, fmt, r);
      }
    } else {
      r = -1;
    }
  } else {
    r = -2;
  }

  return r;
}
SHELL_REGISTER(mkfs,
               "mkfs -h\n"
               "mkfs device format\n",
               mkfsFunc);
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
VFS_FILE *vfs_fopen(const char *filename, const char *opentype) {
  char *abspath;
  const vfs_mount_t *mnt;
  VFS_FILE *file = NULL;

  ASLOG(VFS, ("fopen(%s, %s)\n", filename, opentype));

  abspath = relpath(filename);

  if (NULL != abspath) {
    mnt = search_mnt(abspath);
    if (NULL != mnt) {
      file = mnt->ops->fopen(mnt, abspath, opentype);
    }
    heap_free(abspath);
  }

  return file;
}
ELF_EXPORT_ALIAS(vfs_fopen, "fopen");

int vfs_fclose(VFS_FILE *stream) {
  return stream->fops->fclose(stream);
}
ELF_EXPORT_ALIAS(vfs_fclose, "fclose");

int vfs_fread(void *data, size_t size, size_t count, VFS_FILE *stream) {
  return stream->fops->fread(data, size, count, stream);
}
ELF_EXPORT_ALIAS(vfs_fread, "fread");

int vfs_fwrite(const void *data, size_t size, size_t count, VFS_FILE *stream) {
  return stream->fops->fwrite(data, size, count, stream);
}
ELF_EXPORT_ALIAS(vfs_fwrite, "fwrite");

int vfs_fflush(VFS_FILE *stream) {
  return stream->fops->fflush(stream);
}
ELF_EXPORT_ALIAS(vfs_fflush, "fflush");

int vfs_fseek(VFS_FILE *stream, long int offset, int whence) {
  return stream->fops->fseek(stream, offset, whence);
}
ELF_EXPORT_ALIAS(vfs_fseek, "fseek");

size_t vfs_ftell(VFS_FILE *stream) {
  return stream->fops->ftell(stream);
}
ELF_EXPORT_ALIAS(vfs_ftell, "ftell");

int vfs_unlink(const char *filename) {
  char *abspath;
  int rc = EACCES;
  const vfs_mount_t *mnt;

  ASLOG(VFS, ("unlink(%s)\n", filename));

  abspath = relpath(filename);

  if (NULL != abspath) {
    mnt = search_mnt(abspath);
    if (NULL != mnt) {
      rc = mnt->ops->unlink(mnt, abspath);
    }
    heap_free(abspath);
  }

  return rc;
}
ELF_EXPORT_ALIAS(vfs_unlink, "unlink");

int vfs_stat(const char *filename, vfs_stat_t *buf) {
  char *abspath;
  int rc = EACCES;
  const vfs_mount_t *mnt;

  ASLOG(VFS, ("stat(%s)\n", filename));

  abspath = relpath(filename);

  if (NULL != abspath) {
    mnt = search_mnt(abspath);
    if (NULL != mnt) {
      rc = mnt->ops->stat(mnt, abspath, buf);
    }
    heap_free(abspath);
  }

  return rc;
}
ELF_EXPORT_ALIAS(vfs_stat, "stat");

VFS_DIR *vfs_opendir(const char *dirname) {
  char *abspath;
  const vfs_mount_t *mnt;
  VFS_DIR *dir = NULL;

  ASLOG(VFS, ("opendir(%s)\n", dirname));

  abspath = relpath(dirname);

  if (NULL != abspath) {
    mnt = search_mnt(abspath);
    if (NULL != mnt) {
      dir = mnt->ops->opendir(mnt, abspath);
    }
    heap_free(abspath);
  }

  return dir;
}
ELF_EXPORT_ALIAS(vfs_opendir, "opendir");

vfs_dirent_t *vfs_readdir(VFS_DIR *dirstream) {
  return dirstream->fops->readdir(dirstream);
}
ELF_EXPORT_ALIAS(vfs_readdir, "readdir");

int vfs_closedir(VFS_DIR *dirstream) {
  return dirstream->fops->closedir(dirstream);
}
ELF_EXPORT_ALIAS(vfs_closedir, "closedir");

int vfs_chdir(const char *filename) {
  char *abspath;
  int rc = EACCES;
  const vfs_mount_t *mnt;

  ASLOG(VFS, ("chdir(%s)\n", filename));

  abspath = relpath(filename);

  if (NULL != abspath) {
    mnt = search_mnt(abspath);
    if (NULL != mnt) {
      rc = mnt->ops->chdir(mnt, abspath);
      if (0 == rc) {
        strncpy(vfs_cwd, abspath, FILENAME_MAX);
      }
    }
    heap_free(abspath);
  }

  return rc;
}
ELF_EXPORT_ALIAS(vfs_chdir, "chdir");

char *vfs_getcwd(char *buffer, size_t size) {
  size_t rsize = strlen(vfs_cwd);

  ASLOG(VFS, ("getcwd(%s)\n", vfs_cwd));

  if (NULL == buffer) {
    size = rsize + 1;
    buffer = (char *)heap_malloc(size);
  }

  if (size < rsize) {
    buffer = NULL;
  }

  if (NULL != buffer) {
    strncpy(buffer, vfs_cwd, size);
  }

  return buffer;
}
ELF_EXPORT_ALIAS(vfs_getcwd, "getcwd");

int vfs_mkdir(const char *filename, uint32_t mode) {
  char *abspath;
  int rc = EACCES;
  const vfs_mount_t *mnt;

  ASLOG(VFS, ("mkdir(%s,  0x%x)\n", filename, mode));

  abspath = relpath(filename);

  if (NULL != abspath) {
    mnt = search_mnt(abspath);
    if (NULL != mnt) {
      rc = mnt->ops->mkdir(mnt, abspath, mode);
    }
    heap_free(abspath);
  }

  return rc;
}
ELF_EXPORT_ALIAS(vfs_mkdir, "mkdir");

int vfs_rmdir(const char *filename) {
  char *abspath;
  int rc = EACCES;
  const vfs_mount_t *mnt;

  ASLOG(VFS, ("rmdir(%s)\n", filename));

  abspath = relpath(filename);

  if (NULL != abspath) {
    mnt = search_mnt(abspath);
    if (NULL != mnt) {
      rc = mnt->ops->rmdir(mnt, abspath);
    }
    heap_free(abspath);
  }

  return rc;
}
ELF_EXPORT_ALIAS(vfs_rmdir, "rmdir");

int vfs_rename(const char *oldname, const char *newname) {
  char *abspath_old;
  char *abspath_new;
  int rc = EACCES;
  const vfs_mount_t *mnt;

  ASLOG(VFS, ("rename(%s, %s)\n", oldname, newname));

  abspath_old = relpath(oldname);
  abspath_new = relpath(newname);

  if (NULL != abspath_old) {
    if (NULL != abspath_new) {
      mnt = search_mnt(abspath_old);
      if (NULL != mnt) {
        rc = mnt->ops->rename(mnt, abspath_old, abspath_new);
      }
      heap_free(abspath_new);
    }
    heap_free(abspath_old);
  }

  return rc;
}
ELF_EXPORT_ALIAS(vfs_rename, "rename");

char *vfs_find(const char *file) {
  char *r = NULL;
  char *p;

  p = (char *)heap_malloc(FILENAME_MAX);
  if (p != NULL) {
    strcpy(p, "/");
    r = serach(p, file);
    if (NULL == r) {
      heap_free(p);
    }
  }

  return r;
}

int vfs_fprintf(VFS_FILE *fp, const char *format, ...) {
  int n;
  char *buf;

  va_list arg_ptr;

  buf = (char *)heap_malloc(VFS_FPRINTF_BUFFER_SIZE);
  if (buf != NULL) {
    va_start(arg_ptr, format);
    n = vsnprintf(buf, VFS_FPRINTF_BUFFER_SIZE, format, arg_ptr);
    va_end(arg_ptr);
    if (n >= VFS_FPRINTF_BUFFER_SIZE) {
      ASLOG(ERROR, ("VFS_FPRINTF_BUFFER_SIZE=%d is too small,  enlarge it please\n",
                    VFS_FPRINTF_BUFFER_SIZE));
    }

    n = vfs_fwrite(buf, 1, n, fp);

    heap_free(buf);
  } else {
    n = -ENOMEM;
  }

  return n;
}
ELF_EXPORT_ALIAS(vfs_fprintf, "fprintf");

void vfs_init(void) {
  TAILQ_INIT(&vfs_mount_list);
}

int vfs_mount(const device_t *device, const char *type, const char *mount_point) {
  int ercd = 0;
  vfs_mount_t *m;
  VFS_DIR *dir;
  const struct vfs_filesystem_ops *ops;

  VFS_LOCK();
  TAILQ_FOREACH(m, &vfs_mount_list, entry) {
    if (0 == strcmp(m->mount_point, mount_point)) {
      ercd = EEXIST;
      break;
    }
  }
  VFS_UNLOCK();

  if (0 == ercd) {
    if (0 != strcmp(mount_point, "/")) {
      dir = vfs_opendir(mount_point);
      if (dir != NULL) {
        vfs_closedir(dir);
      } else {
        ercd = ENOENT;
      }
    }
  }

  if (0 == ercd) {
    m = (vfs_mount_t *)heap_malloc(strlen(mount_point) + 1 + sizeof(vfs_mount_t));
    if (m != NULL) {
      ops = search_ops(type);
      if (NULL == ops) {
        ercd = EINVAL;
      } else {
        ercd = ops->mount(device, mount_point);
        if (0 == ercd) {
          m->mount_point = ((char *)m) + sizeof(vfs_mount_t);
          strcpy((char *)m->mount_point, mount_point);
          m->device = device;
          m->ops = ops;

          VFS_LOCK();
          TAILQ_INSERT_TAIL(&vfs_mount_list, m, entry);
          VFS_UNLOCK();
        } else {
          heap_free(m);
        }
      }
    } else {
      ercd = ENOMEM;
    }
  }

  ASLOG(VFS,
        ("mount device %s on %s %s\n", device->name, mount_point, (0 == ercd) ? "okay" : "failed"));

  return ercd;
}

int vfs_mkfs(const device_t *device, const char *type) {
  int ercd = 0;
  const struct vfs_filesystem_ops *ops;

  ops = search_ops(type);
  if (NULL == ops) {
    ercd = EINVAL;
  } else {
    ercd = ops->mkfs(device);
  }

  return ercd;
}

#if !defined(_WIN32) && !defined(linux) && defined(__GNUC__)
FILE *fopen(const char *filename, const char *opentype) __attribute__((weak, alias("vfs_fopen")));
int fclose(FILE *stream) __attribute__((weak, alias("vfs_fclose")));
size_t fread(void *data, size_t size, size_t count, FILE *stream)
  __attribute__((weak, alias("vfs_fread")));
size_t fwrite(const void *data, size_t size, size_t count, FILE *stream)
  __attribute__((weak, alias("vfs_fwrite")));
int fflush(FILE *stream) __attribute__((weak, alias("vfs_fflush")));
int fseek(FILE *stream, long int offset, int whence) __attribute__((weak, alias("vfs_fseek")));
long ftell(FILE *stream) __attribute__((weak, alias("vfs_ftell")));
#endif
