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
bool Lua_ArgType::Get(std::string key, std::string &value) {
  bool r = false;
  if (LUA_ARG_TYPE_TABLE_STRING_MAP == Type) {
    auto it = StringMap.find(key);
    if ((it != StringMap.end()) && (LUA_ARG_TYPE_STRING == it->second.Type)) {
      value = it->second.String;
      r = true;
    }
  }
  return r;
}

bool Lua_ArgType::Get(std::string key, float &value) {
  bool r = false;
  if (LUA_ARG_TYPE_TABLE_STRING_MAP == Type) {
    auto it = StringMap.find(key);
    if ((it != StringMap.end()) && (LUA_ARG_TYPE_FLOAT == it->second.Type)) {
      value = it->second.Float;
      r = true;
    }
  }
  return r;
}

bool Lua_ArgType::Get(std::string key, bool &value) {
  bool r = false;
  if (LUA_ARG_TYPE_TABLE_STRING_MAP == Type) {
    auto it = StringMap.find(key);
    if ((it != StringMap.end()) && (LUA_ARG_TYPE_BOOL == it->second.Type)) {
      value = it->second.Float;
      r = true;
    }
  }
  return r;
}

bool Lua_ArgType::Get(std::string key, std::vector<Lua_ArgType> &args) {
  bool r = false;
  if (LUA_ARG_TYPE_TABLE_STRING_MAP == Type) {
    auto it = StringMap.find(key);
    if ((it != StringMap.end()) && (LUA_ARG_TYPE_TABLE_STRING_MAP == it->second.Type)) {
      for (size_t i = 0; i < it->second.StringMap.size(); i++) {
        auto it2 = it->second.StringMap.find(std::to_string(i + 1));
        if (it2 != it->second.StringMap.end()) {
          args.push_back(it2->second);
        }
      }
      if ((args.size() == it->second.StringMap.size()) && (args.size() > 0)) {
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
    Load(scripts.data(), libraries);
  }
}

AsLuaScript::AsLuaScript(const char *scripts, std::vector<Library> libraries) {
  Load(scripts, libraries);
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

void AsLuaScript::Load(const char *scripts, std::vector<Library> libraries) {
  lua_State *L;
  int r = 0;
  L = luaL_newstate();
  if (NULL != L) {
    luaL_openlibs(L);
    for (auto lib : libraries) {
      luaL_requiref(L, lib.name.c_str(), lib.CreateLibFnc, 1);
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

int AsLuaScript::Call(std::string api, std::vector<Lua_ArgType> &inputs,
                      std::vector<Lua_ArgType> &outputs) {
  return Call(api, inputs.data(), inputs.size(), outputs.data(), outputs.size());
}

bool AsLuaScript::HasApi(std::string api) {
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

int AsLuaScript::Call(std::string api, const Lua_ArgType *inputs, int numIn, Lua_ArgType *outputs,
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
    switch (inputs[i].Type) {
    case LUA_ARG_TYPE_STRING:
      ASLOG(LUAI, ("push arg %d: %s\n", (int)i, inputs[i].String.c_str()));
      s = lua_pushstring(L, inputs[i].String.c_str());
      if (NULL == s) {
        ASLOG(LUAE, ("failed to push arg %d: %s\n", (int)i, inputs[i].String.c_str()));
        r = -EIO;
      }
      break;

      PUSH_BOOL(i, inputs[i].Bool);

      PUSH_FLOAT(FLOAT, i, inputs[i].Float);
      PUSH_FLOAT(DOUBLE, i, inputs[i].Double);

      PUSH_BASIC(SINT8, i, inputs[i].S8);
      PUSH_BASIC(SINT16, i, inputs[i].S16);
      PUSH_BASIC(SINT32, i, inputs[i].S32);
      PUSH_BASIC(SINT64, i, inputs[i].S64);

      PUSH_BASIC(UINT8, i, inputs[i].U8);
      PUSH_BASIC(UINT16, i, inputs[i].U16);
      PUSH_BASIC(UINT32, i, inputs[i].U32);
      PUSH_BASIC(UINT64, i, inputs[i].U64);

      PUSH_ARRAY(SINT8_N, i, inputs[i].S8N);
      PUSH_ARRAY(SINT16_N, i, inputs[i].S16N);
      PUSH_ARRAY(SINT32_N, i, inputs[i].S32N);
      PUSH_ARRAY(SINT64_N, i, inputs[i].S64N);

      PUSH_ARRAY(UINT8_N, i, inputs[i].U8N);
      PUSH_ARRAY(UINT16_N, i, inputs[i].U16N);
      PUSH_ARRAY(UINT32_N, i, inputs[i].U32N);
      PUSH_ARRAY(UINT64_N, i, inputs[i].U64N);

      PUSH_ARRAY_FLOAT(FLOAT_N, i, inputs[i].FN);
      PUSH_ARRAY_FLOAT(DOUBLE_N, i, inputs[i].DN);

      PUSH_ARRAY_STRING(i, inputs[i].StringN);

      PUSH_TABLE_BASIC(STRING_SINT8, i, inputs[i].StringSint8Map);
      PUSH_TABLE_BASIC(STRING_SINT16, i, inputs[i].StringSint16Map);
      PUSH_TABLE_BASIC(STRING_SINT32, i, inputs[i].StringSint32Map);
      PUSH_TABLE_BASIC(STRING_SINT64, i, inputs[i].StringSint64Map);

      PUSH_TABLE_BASIC(STRING_UINT8, i, inputs[i].StringUint8Map);
      PUSH_TABLE_BASIC(STRING_UINT16, i, inputs[i].StringUint16Map);
      PUSH_TABLE_BASIC(STRING_UINT32, i, inputs[i].StringUint32Map);
      PUSH_TABLE_BASIC(STRING_UINT64, i, inputs[i].StringUint64Map);

      PUSH_TABLE_FLOAT(STRING_FLOAT, i, inputs[i].StringFloatMap);
      PUSH_TABLE_FLOAT(STRING_DOUBLE, i, inputs[i].StringDoubleMap);
    default:
      ASLOG(LUAE, ("invalid type %d for input arg %d\n", inputs[i].Type, (int)i));
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
    switch (outputs[i].Type) {
    case LUA_ARG_TYPE_STRING:
      s = lua_tostring(L, -1);
      if (NULL == s) {
        ASLOG(LUAE, ("failed to pop string arg %d\n", (int)i));
      } else {
        ASLOG(LUAI, ("pop arg %d: %s\n", (int)i, s));
        outputs[i].String = s;
      }
      lua_pop(L, 1);
      break;

      POP_BOOL(i, outputs[i].Bool);

      POP_FLOAT(FLOAT, i, outputs[i].Float, float);
      POP_FLOAT(DOUBLE, i, outputs[i].Double, double);

      POP_BASIC(SINT8, i, outputs[i].S8, int8_t);
      POP_BASIC(SINT16, i, outputs[i].S16, int16_t);
      POP_BASIC(SINT32, i, outputs[i].S32, int32_t);
      POP_BASIC(SINT64, i, outputs[i].S64, int64_t);

      POP_BASIC(UINT8, i, outputs[i].U8, uint8_t);
      POP_BASIC(UINT16, i, outputs[i].U16, uint16_t);
      POP_BASIC(UINT32, i, outputs[i].U32, uint32_t);
      POP_BASIC(UINT64, i, outputs[i].U64, uint64_t);

      POP_ARRAY(SINT8_N, i, outputs[i].S8N, int8_t);
      POP_ARRAY(SINT16_N, i, outputs[i].S16N, int16_t);
      POP_ARRAY(SINT32_N, i, outputs[i].S32N, int32_t);
      POP_ARRAY(SINT64_N, i, outputs[i].S64N, int64_t);

      POP_ARRAY(UINT8_N, i, outputs[i].U8N, uint8_t);
      POP_ARRAY(UINT16_N, i, outputs[i].U16N, uint16_t);
      POP_ARRAY(UINT32_N, i, outputs[i].U32N, uint32_t);
      POP_ARRAY(UINT64_N, i, outputs[i].U64N, uint64_t);

      POP_ARRAY_FLOAT(FLOAT_N, i, outputs[i].FN, float);
      POP_ARRAY_FLOAT(DOUBLE_N, i, outputs[i].DN, double);

      POP_ARRAY_STRING(i, outputs[i].StringN);

      POP_TABLE_BASIC(STRING_SINT8, i, outputs[i].StringSint8Map, int8_t);
      POP_TABLE_BASIC(STRING_SINT16, i, outputs[i].StringSint16Map, int16_t);
      POP_TABLE_BASIC(STRING_SINT32, i, outputs[i].StringSint32Map, int32_t);
      POP_TABLE_BASIC(STRING_SINT64, i, outputs[i].StringSint64Map, int64_t);

      POP_TABLE_BASIC(STRING_UINT8, i, outputs[i].StringUint8Map, uint8_t);
      POP_TABLE_BASIC(STRING_UINT16, i, outputs[i].StringUint16Map, uint16_t);
      POP_TABLE_BASIC(STRING_UINT32, i, outputs[i].StringUint32Map, uint32_t);
      POP_TABLE_BASIC(STRING_UINT64, i, outputs[i].StringUint64Map, uint64_t);

      POP_TABLE_FLOAT(STRING_FLOAT, i, outputs[i].StringFloatMap, float);
      POP_TABLE_FLOAT(STRING_DOUBLE, i, outputs[i].StringDoubleMap, double);
    default:
      ASLOG(LUAE, ("invalid type %d for output arg %d\n", outputs[i].Type, (int)i));
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
int AsLua_Call(const char *scripts, const char *api, const Lua_ArgType *inputs, int numIn,
               Lua_ArgType *outputs, int numOut) {
  try {
    AsLuaScript lua(scripts);
    return lua.Call(std::string(api), inputs, numIn, outputs, numOut);
  } catch (const std::exception &e) {
    return -1;
  }
}

int AsLua_Parse(lua_State *L, int index, Lua_ArgType &arg) {
  int r = 0;
  ASLOG(LUAI, ("parse at index = %d begin\n", index));
  switch (lua_type(L, index)) {
  case LUA_TBOOLEAN:
    arg.Type = LUA_ARG_TYPE_BOOL;
    arg.Bool = lua_toboolean(L, index);
    ASLOG(LUAI, ("  parse as bool %s\n", arg.Bool ? "true" : "false"));
    break;
  case LUA_TNUMBER:
    arg.Type = LUA_ARG_TYPE_FLOAT;
    arg.Float = lua_tonumber(L, index);
    ASLOG(LUAI, ("  parse as float %f\n", arg.Float));
    break;
  case LUA_TSTRING:
    arg.Type = LUA_ARG_TYPE_STRING;
    arg.String = lua_tostring(L, index);
    ASLOG(LUAI, ("  parse as string %s\n", arg.String.c_str()));
    break;
  case LUA_TTABLE:
    arg.Type = LUA_ARG_TYPE_TABLE_STRING_MAP;
    lua_pushvalue(L, index);
    lua_pushnil(L); /* push key */
    while ((0 == r) && (0 != lua_next(L, -2))) {
      /* stack: -1 -> value; -2 -> key */
      lua_pushvalue(L, -2); /* push key */
      /* stack: -1 -> key; -2 -> value; -3 -> key */
      if (lua_isstring(L, -1)) {
        const char *key = lua_tostring(L, -1);
        if (key != nullptr) {
          Lua_ArgType a;
          switch (lua_type(L, -2)) {
          case LUA_TBOOLEAN:
            a.Type = LUA_ARG_TYPE_BOOL;
            a.Bool = lua_toboolean(L, -2);
            ASLOG(LUAI, ("  parse table %s: %s\n", key, a.Bool ? "true" : "false"));
            break;
          case LUA_TNUMBER:
            a.Type = LUA_ARG_TYPE_FLOAT;
            a.Float = lua_tonumber(L, -2);
            ASLOG(LUAI, ("  parse table %s: %f\n", key, a.Float));
            break;
          case LUA_TSTRING:
            a.Type = LUA_ARG_TYPE_STRING;
            a.String = lua_tostring(L, -2);
            ASLOG(LUAI, ("  parse table %s: %s\n", key, a.String.c_str()));
            break;
          case LUA_TTABLE:
            ASLOG(LUAI, ("  parse nested table for %s\n", key));
            a.Type = LUA_ARG_TYPE_TABLE_STRING_MAP;
            r = AsLua_Parse(L, -2, a);
            break;
          default:
            ASLOG(LUAE, ("  type %d not supported\n", lua_type(L, -1)));
            r = -ENOTSUP;
            break;
          }
          if (0 == r) {
            arg.StringMap[std::string(key)] = a;
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
