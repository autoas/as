/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "PAL.h"

#if defined(_WIN32)
#include <windows.h>
#include <shlwapi.h>
#else
#include <unistd.h>
#include <stdlib.h>
#include <dlfcn.h>
#endif

#include <chrono>
#include <cstdio>
namespace as {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#if defined(_WIN32)
extern "C" bool PAL_FileExists(const char *file) {
  return PathFileExistsA(file);
}

extern "C" void *PAL_DlOpen(const char *path) {
  return (void *)LoadLibrary(path);
}

extern "C" void *PAL_DlSym(void *dll, const char *symbol) {
  return (void *)GetProcAddress((HMODULE)dll, (LPCSTR)symbol);
}

extern "C" const char *PAL_DlErr(void) {
  static char err[256] = {0};
  DWORD ercd = GetLastError();
  snprintf(err, sizeof(err), "%lu", ercd);
  return err;
}

extern "C" void PAL_DlClose(void *dll) {
  FreeLibrary((HMODULE)dll);
}
#else
extern "C" bool PAL_FileExists(const char *file) {
  bool ret = false;
  if (0 != access(file, F_OK | R_OK)) {
    ret = true;
  }
  return ret;
}

extern "C" void *PAL_DlOpen(const char *path) {
  return dlopen(path, RTLD_NOW);
}

extern "C" void *PAL_DlSym(void *dll, const char *symbol) {
  return dlsym(dll, symbol);
}

extern "C" const char *PAL_DlErr(void) {
  return dlerror();
}

extern "C" void PAL_DlClose(void *dll) {
  dlclose(dll);
}
#endif

extern "C" uint64_t PAL_Timestamp(void) {
  uint64_t timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
  return timestamp;
}

} /* namespace as */
