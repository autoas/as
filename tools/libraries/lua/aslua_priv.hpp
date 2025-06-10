/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023-2024 Parai Wang <parai@foxmail.com>
 */
#ifndef ASLUA_PRIV_H
#define ASLUA_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aslua.hpp"
#include "Log.hpp"
#include "Std_Debug.h"
#include "Std_Types.h"
using namespace as;
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_LUAI 0
#define AS_LOG_LUAE 3

#define PUSH_BOOL(i, value)                                                                        \
  case LUA_ARG_TYPE_BOOL:                                                                          \
    ASLOG(LUAI, ("push arg %d: bool=%s\n", (int)(i), (value) ? "true" : "false"));                 \
    lua_pushboolean(L, (int)(value));                                                              \
    break

#define PUSH_FLOAT(type, i, value)                                                                 \
  case LUA_ARG_TYPE_##type:                                                                        \
    ASLOG(LUAI, ("push arg %d: " #type "=%f\n", (int)(i), (float)(value)));                        \
    lua_pushnumber(L, (lua_Number)(value));                                                        \
    break

#define PUSH_BASIC(type, i, value)                                                                 \
  case LUA_ARG_TYPE_##type:                                                                        \
    ASLOG(LUAI, ("push arg %d: " #type "=0x%X\n", (int)(i), (uint32_t)(value)));                   \
    lua_pushinteger(L, (lua_Integer)(value));                                                      \
    break

#define PUSH_ARRAY(type, i, data)                                                                  \
  case LUA_ARG_TYPE_##type:                                                                        \
    ASLOG(LUAI, ("push arg %d: " #type "=%d\n", (int)(i), (int)((data).size())));                  \
    lua_newtable(L); /* new a table as array */                                                    \
    for (n = 0; n < (int)(data).size(); n++) {                                                     \
      ASLOG(LUAI, ("  push array %d: 0x%x\n", (int)(n), (uint32_t)(data)[n]));                     \
      lua_pushinteger(L, n + 1);     /* push key */                                                \
      lua_pushinteger(L, (data)[n]); /* push value */                                              \
      lua_settable(L, -3);           /* pop to the table */                                        \
    }                                                                                              \
    break

#define PUSH_ARRAY_STRING(i, data)                                                                 \
  case LUA_ARG_TYPE_STRING_N:                                                                      \
    ASLOG(LUAI, ("push arg %d: string array =%d\n", (int)(i), (int)((data).size())));              \
    lua_newtable(L); /* new a table as array */                                                    \
    for (n = 0; n < (int)(data).size(); n++) {                                                     \
      ASLOG(LUAI, ("  push array %d: %s\n", (int)(n), (data)[n].c_str()));                         \
      lua_pushinteger(L, n + 1); /* push key */                                                    \
      s = lua_pushstring(L, (data)[n].c_str());                                                    \
      if (NULL == s) {                                                                             \
        ASLOG(LUAE, ("failed to push arg %d: array[%d]=%s\n", (int)i, (int)n, (data)[n].c_str())); \
        r = -EIO;                                                                                  \
      }                                                                                            \
      lua_settable(L, -3); /* pop to the table */                                                  \
    }                                                                                              \
    break

#define PUSH_ARRAY_FLOAT(type, i, data)                                                            \
  case LUA_ARG_TYPE_##type:                                                                        \
    ASLOG(LUAI, ("push arg %d: " #type "=%d\n", (int)(i), (int)((data).size())));                  \
    lua_newtable(L); /* new a table as array */                                                    \
    for (n = 0; n < (int)(data).size(); n++) {                                                     \
      ASLOG(LUAI, ("  push array %d: %f\n", (int)(n), (float)(data)[n]));                          \
      lua_pushinteger(L, n + 1);                /* push key */                                     \
      lua_pushnumber(L, (lua_Number)(data)[n]); /* push value */                                   \
      lua_settable(L, -3);                      /* pop to the table */                             \
    }                                                                                              \
    break

#define PUSH_TABLE_BASIC(type, i, data)                                                            \
  case LUA_ARG_TYPE_TABLE_##type:                                                                  \
    ASLOG(LUAI, ("push arg %d: " #type " map=%d\n", (int)(i), (int)(data).size()));                \
    lua_newtable(L); /* new a table as array */                                                    \
    for (auto it : (data)) {                                                                       \
      ASLOG(LUAI, ("  push map %s: 0x%x\n", it.first.c_str(), it.second));                         \
      s = lua_pushstring(L, it.first.c_str()); /* push key */                                      \
      if (NULL == s) {                                                                             \
        ASLOG(LUAE, ("failed to push map%s: 0x%x\n", it.first.c_str(), it.second));                \
        r = -EIO;                                                                                  \
        break;                                                                                     \
      }                                                                                            \
      lua_pushinteger(L, (lua_Integer)it.second); /* push value */                                 \
      lua_settable(L, -3);                        /* pop to the table */                           \
    }                                                                                              \
    break;

#define PUSH_TABLE_FLOAT(type, i, data)                                                            \
  case LUA_ARG_TYPE_TABLE_##type:                                                                  \
    ASLOG(LUAI, ("push arg %d: " #type " map=%d\n", (int)(i), (int)(data).size()));                \
    lua_newtable(L); /* new a table as array */                                                    \
    for (auto it : (data)) {                                                                       \
      ASLOG(LUAI, ("  push map %s: %f\n", it.first.c_str(), (float)it.second));                    \
      s = lua_pushstring(L, it.first.c_str()); /* push key */                                      \
      if (NULL == s) {                                                                             \
        ASLOG(LUAE, ("failed to push map%s: %f\n", it.first.c_str(), (float)it.second));           \
        r = -EIO;                                                                                  \
        break;                                                                                     \
      }                                                                                            \
      lua_pushnumber(L, (lua_Number)it.second); /* push value */                                   \
      lua_settable(L, -3);                      /* pop to the table */                             \
    }                                                                                              \
    break;

#define POP_BOOL(i, value)                                                                         \
  case LUA_ARG_TYPE_BOOL:                                                                          \
    r = lua_isboolean(L, -1);                                                                      \
    if (r) {                                                                                       \
      (value) = (bool)lua_toboolean(L, -1);                                                        \
      lua_pop(L, 1);                                                                               \
      ASLOG(LUAI, ("pop arg %d: bool=%s\n", (int)i, (value) ? "true" : "false"));                  \
      r = 0;                                                                                       \
    } else {                                                                                       \
      ASLOG(LUAE, ("pop arg %d: not bool, dtype=%d\n", (int)i, lua_type(L, -1)));                  \
      r = -EACCES;                                                                                 \
    }                                                                                              \
    break

#define POP_FLOAT(type, i, value, dtype)                                                           \
  case LUA_ARG_TYPE_##type:                                                                        \
    r = lua_isnumber(L, -1);                                                                       \
    if (r) {                                                                                       \
      (value) = (dtype)lua_tonumber(L, -1);                                                        \
      lua_pop(L, 1);                                                                               \
      ASLOG(LUAI, ("pop arg %d: " #type "=%f\n", (int)i, (float)(value)));                         \
      r = 0;                                                                                       \
    } else {                                                                                       \
      ASLOG(LUAE, ("pop arg %d: not number, dtype=%d\n", (int)i, lua_type(L, -1)));                \
      r = -EACCES;                                                                                 \
    }                                                                                              \
    break

#define POP_BASIC(type, i, value, dtype)                                                           \
  case LUA_ARG_TYPE_##type:                                                                        \
    r = lua_isinteger(L, -1);                                                                      \
    if (r) {                                                                                       \
      (value) = (dtype)lua_tointeger(L, -1);                                                       \
      lua_pop(L, 1);                                                                               \
      ASLOG(LUAI, ("pop arg %d: " #type "=0x%x\n", (int)i, (uint32_t)(value)));                    \
      r = 0;                                                                                       \
    } else {                                                                                       \
      ASLOG(LUAE, ("pop arg %d: not integer, dtype=%d\n", (int)i, lua_type(L, -1)));               \
      r = -EACCES;                                                                                 \
    }                                                                                              \
    break

#define POP_ARRAY(type, i, data, dtype)                                                            \
  case LUA_ARG_TYPE_##type:                                                                        \
    ASLOG(LUAI, ("pop arg %d: " #type "\n", (int)(i)));                                            \
    r = lua_istable(L, -1);                                                                        \
    if (r) {                                                                                       \
      r = 0;                                                                                       \
      len = luaL_len(L, -1);                                                                       \
      if (len > 0) {                                                                               \
        (data).reserve(len);                                                                       \
      }                                                                                            \
      for (n = 0; (n < len) && (0 == r); n++) {                                                    \
        lua_pushinteger(L, n + 1); /* push key */                                                  \
        r = lua_gettable(L, -2);   /* pop key,push value */                                        \
        if ((LUA_TNUMBER == r) && (lua_isinteger(L, -1))) {                                        \
          (data).push_back((dtype)lua_tointeger(L, -1));                                           \
          ASLOG(LUAI, ("  pop array %d: 0x%x\n", (int)(n), (uint32_t)(data)[n]));                  \
          r = 0;                                                                                   \
        } else {                                                                                   \
          ASLOG(LUAE, ("  pop array %d: not integer, dtype=%d\n", (int)n, r));                     \
          r = -EACCES;                                                                             \
        }                                                                                          \
        lua_pop(L, 1);                                                                             \
      }                                                                                            \
    } else {                                                                                       \
      ASLOG(LUAE, ("pop arg %d: not table, dtype=%d\n", (int)i, lua_type(L, -1)));                 \
      r = -EACCES;                                                                                 \
    }                                                                                              \
    lua_pop(L, 1);                                                                                 \
    break

#define POP_ARRAY_FLOAT(type, i, data, dtype)                                                      \
  case LUA_ARG_TYPE_##type:                                                                        \
    r = lua_istable(L, -1);                                                                        \
    ASLOG(LUAI, ("pop arg %d: " #type "\n", (int)(i)));                                            \
    if (r) {                                                                                       \
      r = 0;                                                                                       \
      len = luaL_len(L, -1);                                                                       \
      if (len > 0) {                                                                               \
        (data).reserve(len);                                                                       \
      }                                                                                            \
      for (n = 0; (n < len) && (0 == r); n++) {                                                    \
        lua_pushinteger(L, n + 1); /* push key */                                                  \
        r = lua_gettable(L, -2);   /* pop key,push value */                                        \
        if (LUA_TNUMBER == r) {                                                                    \
          (data).push_back((dtype)lua_tointeger(L, -1));                                           \
          ASLOG(LUAI, ("  pop array %d: %f\n", (int)(n), (float)(data)[n]));                       \
          r = 0;                                                                                   \
        } else {                                                                                   \
          ASLOG(LUAE, ("  pop array %d: not float, dtype=%d\n", (int)n, r));                       \
          r = -EACCES;                                                                             \
        }                                                                                          \
        lua_pop(L, 1);                                                                             \
      }                                                                                            \
    } else {                                                                                       \
      ASLOG(LUAE, ("pop arg %d: not table, dtype=%d\n", (int)i, lua_type(L, -1)));                 \
      r = -EACCES;                                                                                 \
    }                                                                                              \
    lua_pop(L, 1);                                                                                 \
    break

#define POP_ARRAY_STRING(i, data)                                                                  \
  case LUA_ARG_TYPE_STRING_N:                                                                      \
    r = lua_istable(L, -1);                                                                        \
    ASLOG(LUAI, ("pop arg %d: string array\n", (int)(i)));                                         \
    if (r) {                                                                                       \
      r = 0;                                                                                       \
      len = luaL_len(L, -1);                                                                       \
      if (len > 0) {                                                                               \
        (data).reserve(len);                                                                       \
      }                                                                                            \
      for (n = 0; (n < len) && (0 == r); n++) {                                                    \
        lua_pushinteger(L, n + 1); /* push key */                                                  \
        r = lua_gettable(L, -2);   /* pop key,push value */                                        \
        if (LUA_TSTRING == r) {                                                                    \
          (data).push_back(std::string(lua_tostring(L, -1)));                                      \
          ASLOG(LUAI, ("  pop array %d: %s\n", (int)(n), (data)[n].c_str()));                      \
          r = 0;                                                                                   \
        } else {                                                                                   \
          ASLOG(LUAE, ("  pop array %d: not string, dtype=%d\n", (int)n, r));                      \
          r = -EACCES;                                                                             \
        }                                                                                          \
        lua_pop(L, 1);                                                                             \
      }                                                                                            \
    } else {                                                                                       \
      ASLOG(LUAE, ("pop arg %d: not table, dtype=%d\n", (int)i, lua_type(L, -1)));                 \
      r = -EACCES;                                                                                 \
    }                                                                                              \
    lua_pop(L, 1);                                                                                 \
    break

#define POP_TABLE_BASIC(type, i, data, dtype)                                                      \
  case LUA_ARG_TYPE_TABLE_##type:                                                                  \
    ASLOG(LUAI, ("pop arg %d: " #type " map\n", (int)(i)));                                        \
    r = lua_istable(L, -1);                                                                        \
    if (r) {                                                                                       \
      r = 0;                                                                                       \
      lua_pushnil(L); /* push key */                                                               \
      while ((0 == r) && (0 != lua_next(L, -2))) {                                                 \
        if (lua_isstring(L, -2) && lua_isinteger(L, -1)) {                                         \
          const char *key = lua_tostring(L, -2);                                                   \
          if (key != nullptr) {                                                                    \
            dtype value = (dtype)lua_tointeger(L, -1);                                             \
            data[std::string(key)] = value;                                                        \
            ASLOG(LUAI, ("  pop map %s: 0x%x\n", key, value));                                     \
            r = 0;                                                                                 \
          } else {                                                                                 \
            ASLOG(LUAE, ("  pop " #type " map %d: key type invalid\n", (int)n));                   \
            r = -EINVAL;                                                                           \
          }                                                                                        \
        } else {                                                                                   \
          ASLOG(LUAE, ("  pop " #type " map %d: type invalid\n", (int)n));                         \
          r = -EACCES;                                                                             \
        }                                                                                          \
        lua_pop(L, 1); /* drop value and keep key */                                               \
      }                                                                                            \
    } else {                                                                                       \
      ASLOG(LUAE, ("pop arg %d: not table, dtype=%d\n", (int)i, lua_type(L, -1)));                 \
      r = -EACCES;                                                                                 \
    }                                                                                              \
    lua_pop(L, 1);                                                                                 \
    break;

#define POP_TABLE_FLOAT(type, i, data, dtype)                                                      \
  case LUA_ARG_TYPE_TABLE_##type:                                                                  \
    ASLOG(LUAI, ("pop arg %d: " #type " map\n", (int)(i)));                                        \
    r = lua_istable(L, -1);                                                                        \
    if (r) {                                                                                       \
      r = 0;                                                                                       \
      lua_pushnil(L); /* push key */                                                               \
      while ((0 == r) && (0 != lua_next(L, -2))) {                                                 \
        if (lua_isstring(L, -2) && lua_isnumber(L, -1)) {                                          \
          const char *key = lua_tostring(L, -2);                                                   \
          if (key != nullptr) {                                                                    \
            dtype value = (dtype)lua_tonumber(L, -1);                                              \
            data[std::string(key)] = value;                                                        \
            ASLOG(LUAI, ("  pop map %s: %f\n", key, (float)value));                                \
            r = 0;                                                                                 \
          } else {                                                                                 \
            ASLOG(LUAE, ("  pop " #type " map %d: key type invalid\n", (int)n));                   \
            r = -EINVAL;                                                                           \
          }                                                                                        \
        } else {                                                                                   \
          ASLOG(LUAE, ("  pop " #type " map %d: type invalid\n", (int)n));                         \
          r = -EACCES;                                                                             \
        }                                                                                          \
        lua_pop(L, 1); /* drop value and keep key */                                               \
      }                                                                                            \
    } else {                                                                                       \
      ASLOG(LUAE, ("pop arg %d: not table, dtype=%d\n", (int)i, lua_type(L, -1)));                 \
      r = -EACCES;                                                                                 \
    }                                                                                              \
    lua_pop(L, 1);                                                                                 \
    break;
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* ASLUA_PRIV_H */
