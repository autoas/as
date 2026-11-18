/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef ASLUA_HPP
#define ASLUA_HPP
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
} Lua_ArgTypeType;

typedef struct Lua_Arg_s Lua_ArgType;

struct Lua_Arg_s {
  Lua_ArgTypeType Type = LUA_ARG_TYPE_NIL;

  std::string String;

  bool Bool;
  float Float;
  double Double;

  int8_t S8;
  int16_t S16;
  int32_t S32;
  int64_t S64;

  uint8_t U8;
  uint16_t U16;
  uint32_t U32;
  uint64_t U64;

  std::vector<int8_t> S8N;
  std::vector<int16_t> S16N;
  std::vector<int32_t> S32N;
  std::vector<int64_t> S64N;

  std::vector<uint8_t> U8N;
  std::vector<uint16_t> U16N;
  std::vector<uint32_t> U32N;
  std::vector<uint64_t> U64N;

  std::vector<float> FN;
  std::vector<double> DN;

  std::vector<std::string> StringN;

  std::map<std::string, int8_t> StringSint8Map;
  std::map<std::string, int16_t> StringSint16Map;
  std::map<std::string, int32_t> StringSint32Map;
  std::map<std::string, int64_t> StringSint64Map;

  std::map<std::string, uint8_t> StringUint8Map;
  std::map<std::string, uint16_t> StringUint16Map;
  std::map<std::string, uint32_t> StringUint32Map;
  std::map<std::string, uint64_t> StringUint64Map;

  std::map<std::string, float> StringFloatMap;
  std::map<std::string, double> StringDoubleMap;

  std::map<std::string, Lua_ArgType> StringMap;

  bool Get(std::string key, std::string &value);
  bool Get(std::string key, float &value);
  bool Get(std::string key, bool &value);
  bool Get(std::string key, std::vector<Lua_ArgType> &args);
};

class AsLuaScript {

public:
  struct Library {
    std::string name;
    lua_CFunction CreateLibFnc;
  };

public:
  AsLuaScript(std::string scriptFile, std::vector<Library> libraries = {});
  AsLuaScript(const char *scripts, std::vector<Library> libraries = {});
  ~AsLuaScript();
  bool HasApi(std::string api);
  int Call(std::string api, std::vector<Lua_ArgType> &inputs, std::vector<Lua_ArgType> &outputs);
  int Call(std::string api, const Lua_ArgType *inputs, int numIn, Lua_ArgType *outputs, int numOut);

private:
  void Load(const char *scripts, std::vector<Library> libraries);

private:
  void *m_Context = nullptr;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int AsLua_Call(const char *scripts, const char *api, const Lua_ArgType *inputs, int numIn,
               Lua_ArgType *outputs, int numOut);
int AsLua_Parse(lua_State *L, int index, Lua_ArgType &arg);
#endif /* ASLUA_HPP */
