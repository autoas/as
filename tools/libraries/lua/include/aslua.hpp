/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef _AS_LUA_HPP_
#define _AS_LUA_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>

extern "C" {
#include "lprefix.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef enum {
  LUA_ARG_TYPE_STRING,
  LUA_ARG_TYPE_BOOL,
  LUA_ARG_TYPE_FLOAT,
  LUA_ARG_TYPE_DOUBLE,

  LUA_ARG_TYPE_SINT8,
  LUA_ARG_TYPE_SINT16,
  LUA_ARG_TYPE_SINT32,
  LUA_ARG_TYPE_SINT64,

  LUA_ARG_TYPE_UINT8,
  LUA_ARG_TYPE_UINT16,
  LUA_ARG_TYPE_UINT32,
  LUA_ARG_TYPE_UINT64,

  LUA_ARG_TYPE_SINT8_N,
  LUA_ARG_TYPE_SINT16_N,
  LUA_ARG_TYPE_SINT32_N,
  LUA_ARG_TYPE_SINT64_N,

  LUA_ARG_TYPE_UINT8_N,
  LUA_ARG_TYPE_UINT16_N,
  LUA_ARG_TYPE_UINT32_N,
  LUA_ARG_TYPE_UINT64_N,

  LUA_ARG_TYPE_FLOAT_N,
  LUA_ARG_TYPE_DOUBLE_N,

  LUA_ARG_TYPE_STRING_N,

  LUA_ARG_TYPE_TABLE_STRING_SINT8,
  LUA_ARG_TYPE_TABLE_STRING_SINT16,
  LUA_ARG_TYPE_TABLE_STRING_SINT32,
  LUA_ARG_TYPE_TABLE_STRING_SINT64,

  LUA_ARG_TYPE_TABLE_STRING_UINT8,
  LUA_ARG_TYPE_TABLE_STRING_UINT16,
  LUA_ARG_TYPE_TABLE_STRING_UINT32,
  LUA_ARG_TYPE_TABLE_STRING_UINT64,

  LUA_ARG_TYPE_TABLE_STRING_FLOAT,
  LUA_ARG_TYPE_TABLE_STRING_DOUBLE,

  LUA_ARG_TYPE_TABLE_STRING_MAP,
  LUA_ARG_TYPE_NIL,
} lua_arg_type_t;

typedef struct lua_arg_s lua_arg_t;

struct lua_arg_s {
  lua_arg_type_t type = LUA_ARG_TYPE_NIL;

  std::string string;

  bool b;
  float f;
  double d;

  int8_t s8;
  int16_t s16;
  int32_t s32;
  int64_t s64;

  uint8_t u8;
  uint16_t u16;
  uint32_t u32;
  uint64_t u64;

  std::vector<int8_t> s8N;
  std::vector<int16_t> s16N;
  std::vector<int32_t> s32N;
  std::vector<int64_t> s64N;

  std::vector<uint8_t> u8N;
  std::vector<uint16_t> u16N;
  std::vector<uint32_t> u32N;
  std::vector<uint64_t> u64N;

  std::vector<float> fN;
  std::vector<double> dN;

  std::vector<std::string> stringN;

  std::map<std::string, int8_t> stringSint8Map;
  std::map<std::string, int16_t> stringSint16Map;
  std::map<std::string, int32_t> stringSint32Map;
  std::map<std::string, int64_t> stringSint64Map;

  std::map<std::string, uint8_t> stringUint8Map;
  std::map<std::string, uint16_t> stringUint16Map;
  std::map<std::string, uint32_t> stringUint32Map;
  std::map<std::string, uint64_t> stringUint64Map;

  std::map<std::string, float> stringFloatMap;
  std::map<std::string, double> stringDoubleMap;

  std::map<std::string, lua_arg_t> stringMap;

  bool get(std::string key, std::string &value);
  bool get(std::string key, float &value);
  bool get(std::string key, bool &value);
  bool get(std::string key, std::vector<lua_arg_t> &args);
};

class AsLuaScript {

public:
  struct Library {
    std::string name;
    lua_CFunction createLibFnc;
  };

public:
  AsLuaScript(std::string scriptFile, std::vector<Library> libraries = {});
  AsLuaScript(const char *scripts, std::vector<Library> libraries = {});
  ~AsLuaScript();
  bool hasApi(std::string api);
  int call(std::string api, std::vector<lua_arg_t> &inputs, std::vector<lua_arg_t> &outputs);
  int call(std::string api, const lua_arg_t *inputs, int numIn, lua_arg_t *outputs, int numOut);

private:
  void load(const char *scripts, std::vector<Library> libraries);

private:
  void *m_Context = nullptr;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int aslua_call(const char *scripts, const char *api, const lua_arg_t *inputs, int numIn,
               lua_arg_t *outputs, int numOut);
int aslua_parse(lua_State *L, int index, lua_arg_t &arg);
#endif /* _AS_LUA_HPP_ */
