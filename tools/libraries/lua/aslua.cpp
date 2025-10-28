/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "aslua_priv.hpp"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern int lua_create_can(lua_State *L);
extern int lua_create_crypto(lua_State *L);
extern int lua_create_x509(lua_State *L);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
bool lua_arg_s::get(std::string key, std::string &value) {
  bool r = false;
  if (LUA_ARG_TYPE_TABLE_STRING_MAP == type) {
    auto it = stringMap.find(key);
    if ((it != stringMap.end()) && (LUA_ARG_TYPE_STRING == it->second.type)) {
      value = it->second.string;
      r = true;
    }
  }
  return r;
}

bool lua_arg_s::get(std::string key, float &value) {
  bool r = false;
  if (LUA_ARG_TYPE_TABLE_STRING_MAP == type) {
    auto it = stringMap.find(key);
    if ((it != stringMap.end()) && (LUA_ARG_TYPE_FLOAT == it->second.type)) {
      value = it->second.f;
      r = true;
    }
  }
  return r;
}

bool lua_arg_s::get(std::string key, bool &value) {
  bool r = false;
  if (LUA_ARG_TYPE_TABLE_STRING_MAP == type) {
    auto it = stringMap.find(key);
    if ((it != stringMap.end()) && (LUA_ARG_TYPE_BOOL == it->second.type)) {
      value = it->second.f;
      r = true;
    }
  }
  return r;
}

bool lua_arg_s::get(std::string key, std::vector<lua_arg_t> &args) {
  bool r = false;
  if (LUA_ARG_TYPE_TABLE_STRING_MAP == type) {
    auto it = stringMap.find(key);
    if ((it != stringMap.end()) && (LUA_ARG_TYPE_TABLE_STRING_MAP == it->second.type)) {
      for (size_t i = 0; i < it->second.stringMap.size(); i++) {
        auto it2 = it->second.stringMap.find(std::to_string(i + 1));
        if (it2 != it->second.stringMap.end()) {
          args.push_back(it2->second);
        }
      }
      if ((args.size() == it->second.stringMap.size()) && (args.size() > 0)) {
        r = true;
      }
    }
  }
  return r;
}

AsLuaScript::AsLuaScript(std::string scriptFile, std::vector<Library> libraries) {
  std::vector<char> scripts;
  FILE *fp = fopen(scriptFile.c_str(), "rb");
  if (nullptr == fp) {
    std::string msg("lua script <" + scriptFile + "> not exists");
    throw std::runtime_error(msg);
  } else {
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    scripts.resize(size + 1);
    fread(scripts.data(), size, 1, fp);
    char *eol = (char *)scripts.data();
    eol[size] = 0;
    fclose(fp);
    load(scripts.data(), libraries);
  }
}

AsLuaScript::AsLuaScript(const char *scripts, std::vector<Library> libraries) {
  load(scripts, libraries);
}

extern "C" void luaL_openAsBuiltlibs(lua_State *L) {
/* load AS builtin lib */
#ifdef USE_CRYPTOIF
  luaL_requiref(L, "crypto", lua_create_crypto, 1);
  lua_pop(L, 1); /* remove lib */
#endif
#ifdef USE_ASN1X509
  luaL_requiref(L, "x509", lua_create_x509, 1);
  lua_pop(L, 1); /* remove lib */
#endif
  luaL_requiref(L, "can", lua_create_can, 1);
  lua_pop(L, 1); /* remove lib */
}

void AsLuaScript::load(const char *scripts, std::vector<Library> libraries) {
  lua_State *L;
  int r = 0;
  L = luaL_newstate();
  if (NULL != L) {
    luaL_openlibs(L);
    for (auto lib : libraries) {
      luaL_requiref(L, lib.name.c_str(), lib.createLibFnc, 1);
      lua_pop(L, 1); /* remove lib */
      ASLOG(LUAI, ("load library: %s\n", lib.name.c_str()));
    }
    luaL_openAsBuiltlibs(L);
  } else {
    r = -ENOMEM;
  }

  if (0 == r) {
    r = luaL_loadstring(L, scripts);
  }

  if (0 == r) {
    r = lua_pcall(L, 0, 0, 0);
    if (0 != r) {
      ASLOG(LUAE, ("load error: %s\n", lua_tostring(L, -1)));
    }
  }

  if (0 != r) {
    if (nullptr != L) {
      lua_close(L);
    }
    throw std::runtime_error("create lua state failed: error =" + std::to_string(r));
  }

  m_Context = L;
}

int AsLuaScript::call(std::string api, std::vector<lua_arg_t> &inputs,
                      std::vector<lua_arg_t> &outputs) {
  return call(api, inputs.data(), inputs.size(), outputs.data(), outputs.size());
}

bool AsLuaScript::hasApi(std::string api) {
  bool has = false;
  int r = 0;
  lua_State *L = (lua_State *)m_Context;

  r = lua_getglobal(L, api.c_str());
  if (LUA_TNIL != r) {
    lua_pop(L, 1);
  }
  if (LUA_TFUNCTION == r) {
    has = true;
  } else {
    has = false;
  }

  return has;
}

int AsLuaScript::call(std::string api, const lua_arg_t *inputs, int numIn, lua_arg_t *outputs,
                      int numOut) {
  int r = 0;
  int i, n;
  const char *s;
  lua_Integer len;
  lua_State *L = (lua_State *)m_Context;

  if (0 == r) {
    r = lua_getglobal(L, api.c_str());
    if (LUA_TFUNCTION == r) {
      ASLOG(LUAI, ("going to call API %s \n", api.c_str()));
      r = 0;
    } else {
      if (LUA_TNIL != r) {
        lua_pop(L, 1);
      }
      ASLOG(LUAE, ("no API %s\n", api.c_str()));
    }
  }

  for (i = 0; (0 == r) && (i < numIn); i++) {
    switch (inputs[i].type) {
    case LUA_ARG_TYPE_STRING:
      ASLOG(LUAI, ("push arg %d: %s\n", (int)i, inputs[i].string.c_str()));
      s = lua_pushstring(L, inputs[i].string.c_str());
      if (NULL == s) {
        ASLOG(LUAE, ("failed to push arg %d: %s\n", (int)i, inputs[i].string.c_str()));
        r = -EIO;
      }
      break;

      PUSH_BOOL(i, inputs[i].b);

      PUSH_FLOAT(FLOAT, i, inputs[i].f);
      PUSH_FLOAT(DOUBLE, i, inputs[i].d);

      PUSH_BASIC(SINT8, i, inputs[i].s8);
      PUSH_BASIC(SINT16, i, inputs[i].s16);
      PUSH_BASIC(SINT32, i, inputs[i].s32);
      PUSH_BASIC(SINT64, i, inputs[i].s64);

      PUSH_BASIC(UINT8, i, inputs[i].u8);
      PUSH_BASIC(UINT16, i, inputs[i].u16);
      PUSH_BASIC(UINT32, i, inputs[i].u32);
      PUSH_BASIC(UINT64, i, inputs[i].u64);

      PUSH_ARRAY(SINT8_N, i, inputs[i].s8N);
      PUSH_ARRAY(SINT16_N, i, inputs[i].s16N);
      PUSH_ARRAY(SINT32_N, i, inputs[i].s32N);
      PUSH_ARRAY(SINT64_N, i, inputs[i].s64N);

      PUSH_ARRAY(UINT8_N, i, inputs[i].u8N);
      PUSH_ARRAY(UINT16_N, i, inputs[i].u16N);
      PUSH_ARRAY(UINT32_N, i, inputs[i].u32N);
      PUSH_ARRAY(UINT64_N, i, inputs[i].u64N);

      PUSH_ARRAY_FLOAT(FLOAT_N, i, inputs[i].fN);
      PUSH_ARRAY_FLOAT(DOUBLE_N, i, inputs[i].dN);

      PUSH_ARRAY_STRING(i, inputs[i].stringN);

      PUSH_TABLE_BASIC(STRING_SINT8, i, inputs[i].stringSint8Map);
      PUSH_TABLE_BASIC(STRING_SINT16, i, inputs[i].stringSint16Map);
      PUSH_TABLE_BASIC(STRING_SINT32, i, inputs[i].stringSint32Map);
      PUSH_TABLE_BASIC(STRING_SINT64, i, inputs[i].stringSint64Map);

      PUSH_TABLE_BASIC(STRING_UINT8, i, inputs[i].stringUint8Map);
      PUSH_TABLE_BASIC(STRING_UINT16, i, inputs[i].stringUint16Map);
      PUSH_TABLE_BASIC(STRING_UINT32, i, inputs[i].stringUint32Map);
      PUSH_TABLE_BASIC(STRING_UINT64, i, inputs[i].stringUint64Map);

      PUSH_TABLE_FLOAT(STRING_FLOAT, i, inputs[i].stringFloatMap);
      PUSH_TABLE_FLOAT(STRING_DOUBLE, i, inputs[i].stringDoubleMap);
    default:
      ASLOG(LUAE, ("invalid type %d for input arg %d\n", inputs[i].type, (int)i));
      r = -EINVAL;
      break;
    }
  }

  if (0 == r) {
    r = lua_pcall(L, numIn, numOut, 0);
    if (0 != r) {
      ASLOG(LUAE, ("call error: %s\n", lua_tostring(L, -1)));
    }
  }

  for (i = numOut - 1; (0 == r) && (i >= 0); i--) {
    switch (outputs[i].type) {
    case LUA_ARG_TYPE_STRING:
      s = lua_tostring(L, -1);
      if (NULL == s) {
        ASLOG(LUAE, ("failed to pop string arg %d\n", (int)i));
      } else {
        ASLOG(LUAI, ("pop arg %d: %s\n", (int)i, s));
        outputs[i].string = s;
      }
      lua_pop(L, 1);
      break;

      POP_BOOL(i, outputs[i].b);

      POP_FLOAT(FLOAT, i, outputs[i].f, float);
      POP_FLOAT(DOUBLE, i, outputs[i].d, double);

      POP_BASIC(SINT8, i, outputs[i].s8, int8_t);
      POP_BASIC(SINT16, i, outputs[i].s16, int16_t);
      POP_BASIC(SINT32, i, outputs[i].s32, int32_t);
      POP_BASIC(SINT64, i, outputs[i].s64, int64_t);

      POP_BASIC(UINT8, i, outputs[i].u8, uint8_t);
      POP_BASIC(UINT16, i, outputs[i].u16, uint16_t);
      POP_BASIC(UINT32, i, outputs[i].u32, uint32_t);
      POP_BASIC(UINT64, i, outputs[i].u64, uint64_t);

      POP_ARRAY(SINT8_N, i, outputs[i].s8N, int8_t);
      POP_ARRAY(SINT16_N, i, outputs[i].s16N, int16_t);
      POP_ARRAY(SINT32_N, i, outputs[i].s32N, int32_t);
      POP_ARRAY(SINT64_N, i, outputs[i].s64N, int64_t);

      POP_ARRAY(UINT8_N, i, outputs[i].u8N, uint8_t);
      POP_ARRAY(UINT16_N, i, outputs[i].u16N, uint16_t);
      POP_ARRAY(UINT32_N, i, outputs[i].u32N, uint32_t);
      POP_ARRAY(UINT64_N, i, outputs[i].u64N, uint64_t);

      POP_ARRAY_FLOAT(FLOAT_N, i, outputs[i].fN, float);
      POP_ARRAY_FLOAT(DOUBLE_N, i, outputs[i].dN, double);

      POP_ARRAY_STRING(i, outputs[i].stringN);

      POP_TABLE_BASIC(STRING_SINT8, i, outputs[i].stringSint8Map, int8_t);
      POP_TABLE_BASIC(STRING_SINT16, i, outputs[i].stringSint16Map, int16_t);
      POP_TABLE_BASIC(STRING_SINT32, i, outputs[i].stringSint32Map, int32_t);
      POP_TABLE_BASIC(STRING_SINT64, i, outputs[i].stringSint64Map, int64_t);

      POP_TABLE_BASIC(STRING_UINT8, i, outputs[i].stringUint8Map, uint8_t);
      POP_TABLE_BASIC(STRING_UINT16, i, outputs[i].stringUint16Map, uint16_t);
      POP_TABLE_BASIC(STRING_UINT32, i, outputs[i].stringUint32Map, uint32_t);
      POP_TABLE_BASIC(STRING_UINT64, i, outputs[i].stringUint64Map, uint64_t);

      POP_TABLE_FLOAT(STRING_FLOAT, i, outputs[i].stringFloatMap, float);
      POP_TABLE_FLOAT(STRING_DOUBLE, i, outputs[i].stringDoubleMap, double);
    default:
      ASLOG(LUAE, ("invalid type %d for output arg %d\n", outputs[i].type, (int)i));
      r = -EINVAL;
      break;
    }
  }

  return r;
}

AsLuaScript::~AsLuaScript() {
  if (m_Context) {
    lua_close((lua_State *)m_Context);
  }
}

/* http://www.lua.org/pil/25.2.html */
int aslua_call(const char *scripts, const char *api, const lua_arg_t *inputs, int numIn,
               lua_arg_t *outputs, int numOut) {
  try {
    AsLuaScript lua(scripts);
    return lua.call(std::string(api), inputs, numIn, outputs, numOut);
  } catch (const std::exception &e) {
    return -1;
  }
}

int aslua_parse(lua_State *L, int index, lua_arg_t &arg) {
  int r = 0;
  ASLOG(LUAI, ("parse at index = %d begin\n", index));
  switch (lua_type(L, index)) {
  case LUA_TBOOLEAN:
    arg.type = LUA_ARG_TYPE_BOOL;
    arg.b = lua_toboolean(L, index);
    ASLOG(LUAI, ("  parse as bool %s\n", arg.b ? "true" : "false"));
    break;
  case LUA_TNUMBER:
    arg.type = LUA_ARG_TYPE_FLOAT;
    arg.f = lua_tonumber(L, index);
    ASLOG(LUAI, ("  parse as float %f\n", arg.f));
    break;
  case LUA_TSTRING:
    arg.type = LUA_ARG_TYPE_STRING;
    arg.string = lua_tostring(L, index);
    ASLOG(LUAI, ("  parse as string %s\n", arg.string.c_str()));
    break;
  case LUA_TTABLE:
    arg.type = LUA_ARG_TYPE_TABLE_STRING_MAP;
    lua_pushvalue(L, index);
    lua_pushnil(L); /* push key */
    while ((0 == r) && (0 != lua_next(L, -2))) {
      /* stack: -1 -> value; -2 -> key */
      lua_pushvalue(L, -2); /* push key */
      /* stack: -1 -> key; -2 -> value; -3 -> key */
      if (lua_isstring(L, -1)) {
        const char *key = lua_tostring(L, -1);
        if (key != nullptr) {
          lua_arg_t a;
          switch (lua_type(L, -2)) {
          case LUA_TBOOLEAN:
            a.type = LUA_ARG_TYPE_BOOL;
            a.b = lua_toboolean(L, -2);
            ASLOG(LUAI, ("  parse table %s: %s\n", key, a.b ? "true" : "false"));
            break;
          case LUA_TNUMBER:
            a.type = LUA_ARG_TYPE_FLOAT;
            a.f = lua_tonumber(L, -2);
            ASLOG(LUAI, ("  parse table %s: %f\n", key, a.f));
            break;
          case LUA_TSTRING:
            a.type = LUA_ARG_TYPE_STRING;
            a.string = lua_tostring(L, -2);
            ASLOG(LUAI, ("  parse table %s: %s\n", key, a.string.c_str()));
            break;
          case LUA_TTABLE:
            ASLOG(LUAI, ("  parse nested table for %s\n", key));
            a.type = LUA_ARG_TYPE_TABLE_STRING_MAP;
            r = aslua_parse(L, -2, a);
            break;
          default:
            ASLOG(LUAE, ("  type %d not supported\n", lua_type(L, -1)));
            r = -ENOTSUP;
            break;
          }
          if (0 == r) {
            arg.stringMap[std::string(key)] = a;
          }
        } else {
          ASLOG(LUAE, ("key is not string, OoM\n"));
          r = -EINVAL;
        }
      } else {
        ASLOG(LUAE, ("key is not string\n"));
        r = -EACCES;
      }
      lua_pop(L, 2); /* drop value and keep key */
    }
    lua_pop(L, 1);
    break;
  default:
    ASLOG(LUAE, ("type %d not supported\n", lua_type(L, index)));
    r = -ENOTSUP;
    break;
  }
  ASLOG(LUAI, ("parse at index = %d end\n", index));
  return r;
}
