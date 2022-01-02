/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "plugin.h"
#include <Std_Types.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
#if defined(_WIN32) || defined(linux)
static const plugin_t *lPluginList[1024];
static uint32_t lPluginNum = 0;
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
void plugin_register(const plugin_t *plugin) {
  if (lPluginNum < ARRAY_SIZE(lPluginList)) {
    lPluginList[lPluginNum] = plugin;
    lPluginNum++;
  }
}

void plugin_init(void) {
  uint32_t i;
  for (i = 0; i < lPluginNum; i++) {
    lPluginList[i]->init();
  }
}

void plugin_deinit(void) {
  uint32_t i;
  for (i = 0; i < lPluginNum; i++) {
    lPluginList[i]->deinit();
  }
}

void plugin_main(void) {
  uint32_t i;
  for (i = 0; i < lPluginNum; i++) {
    lPluginList[i]->main();
  }
}