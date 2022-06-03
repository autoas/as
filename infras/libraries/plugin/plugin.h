/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef _PLUGIN_H
#define _PLUGIN_H
/* ================================ [ INCLUDES  ] ============================================== */
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#if defined(_WIN32) || defined(linux)
#define REGISTER_PLUGIN(name)                                                                      \
  const plugin_t plugin_##name = {name##_init, name##_main, name##_deinit};                        \
  static void __attribute__((constructor)) _##name##_ctor(void) {                                  \
    plugin_register(&plugin_##name);                                                               \
  }
#else
#define REGISTER_PLUGIN(name)                                                                      \
  const plugin_t __attribute__((section("PluginTab")))                                             \
  plugin_##name = {name##_init, name##_main, name##_deinit};
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  void (*init)(void);
  void (*main)(void);
  void (*deinit)(void);
} plugin_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#if defined(_WIN32) || defined(linux)
void plugin_register(const plugin_t *plugin);
#endif
void plugin_init(void);
void plugin_deinit(void);
void plugin_main(void);
#ifdef __cplusplus
}
#endif
#endif /* _PLUGIN_H */
