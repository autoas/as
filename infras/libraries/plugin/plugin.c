/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "plugin.h"
#include <Std_Types.h>
/* ================================ [ MACROS    ] ============================================== */
#if defined(_WIN32) || defined(linux)
#else
#define lPluginList __plugintab_start
#define lPluginNum (__plugintab_end - __plugintab_start)
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
#if defined(_WIN32) || defined(linux)
static plugin_t lPluginList[1024];
static uint32_t lPluginNum = 0;
#else
extern const plugin_t __plugintab_start[];
extern const plugin_t __plugintab_end[];
#endif
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#if defined(_WIN32) || defined(linux)
void plugin_register(const plugin_t *plugin) {
  if (lPluginNum < ARRAY_SIZE(lPluginList)) {
    lPluginList[lPluginNum] = *plugin;
    lPluginNum++;
  }
}
#endif

void plugin_init(void) {
  uint32_t i;
  for (i = 0; i < lPluginNum; i++) {
    lPluginList[i].init();
  }
}

void plugin_deinit(void) {
  uint32_t i;
  for (i = 0; i < lPluginNum; i++) {
    lPluginList[i].deinit();
  }
}

void plugin_main(void) {
  uint32_t i;
  for (i = 0; i < lPluginNum; i++) {
    lPluginList[i].main();
  }
}
