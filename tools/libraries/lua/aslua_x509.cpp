/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "aslua_priv.hpp"
#ifdef USE_ASN1X509
#include "x509.hpp"
#endif
using namespace as;
/* ================================ [ MACROS    ] ============================================== */
#define LUA_X509HANDLE "x509*"

#define tolx509(L) ((LX509 *)luaL_checkudata(L, 1, LUA_X509HANDLE))
/* ================================ [ TYPES     ] ============================================== */
#ifdef USE_ASN1X509
typedef struct {
  x509 *ca;
} LX509;
#endif
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
#ifdef USE_ASN1X509
static int x509_open(lua_State *L) {
  int n = lua_gettop(L); /* number of arguments */
  if (1 == n) {
    size_t len = 0;
    auto s = lua_tolstring(L, 1, &len);
    if (nullptr == s) {
      return luaL_error(L, "x509_open with invalid arguments");
    }
    auto ca = new x509(std::string(s));
    LX509 *lx509 = (LX509 *)lua_newuserdatauv(L, sizeof(LX509), 0);
    lx509->ca = ca;
    (void)lx509;
    luaL_setmetatable(L, LUA_X509HANDLE);
  } else {
    return luaL_error(L, "x509_open with invalid number of arguments: %d", n);
  }
  return 1;
}

static const luaL_Reg s_LuaX509Inferfaces[] = {
  {"open", x509_open},
  {nullptr, nullptr},
};

static int x509_gc(lua_State *L) {
  LX509 *lx509 = tolx509(L);
  if (nullptr != lx509->ca) {
    delete lx509->ca;
    lx509->ca = nullptr;
  }
  return 0;
}

static int x509_tostring(lua_State *L) {
  LX509 *lx509 = tolx509(L);
  if (nullptr == lx509->ca)
    lua_pushliteral(L, "x509 (closed)");
  else
    lua_pushfstring(L, "x509 (%p)", lx509->ca);
  return 1;
}
/*
** metamethods for x509 handles
*/
static const luaL_Reg x509_metameth[] = {
  {"__index", NULL},             /* place holder */
  {"__gc", x509_gc},             /* clean up */
  {"__close", x509_gc},          /* close it */
  {"__tostring", x509_tostring}, /* check state */
  {NULL, NULL},
};

static int x509_public_key(lua_State *L) {
  LX509 *lx509 = tolx509(L);
  auto key = lx509->ca->public_key();

  lua_newtable(L);
  for (int i = 0; i < (int)key.size(); i++) {
    lua_pushinteger(L, i + 1);  /* push key */
    lua_pushinteger(L, key[i]); /* push value */
    lua_settable(L, -3);        /* pop to the table */
  }

  return 1;
}

static int x509_signature(lua_State *L) {
  LX509 *lx509 = tolx509(L);
  auto sig = lx509->ca->signature();

  lua_newtable(L);
  for (int i = 0; i < (int)sig.size(); i++) {
    lua_pushinteger(L, i + 1);  /* push key */
    lua_pushinteger(L, sig[i]); /* push value */
    lua_settable(L, -3);        /* pop to the table */
  }

  return 1;
}

static int x509_tbs_certificate(lua_State *L) {
  LX509 *lx509 = tolx509(L);
  auto tbs = lx509->ca->tbs_certificate();

  lua_newtable(L);
  for (int i = 0; i < (int)tbs.size(); i++) {
    lua_pushinteger(L, i + 1);  /* push key */
    lua_pushinteger(L, tbs[i]); /* push value */
    lua_settable(L, -3);        /* pop to the table */
  }

  return 1;
}
/*
** methods for x509 handles
*/
static const luaL_Reg x509_meth[] = {
  {"public_key", x509_public_key},
  {"signature", x509_signature},
  {"tbs_certificate", x509_tbs_certificate},
  {NULL, NULL},
};

static void x509_createmeta(lua_State *L) {
  luaL_newmetatable(L, LUA_X509HANDLE); /* metatable for file handles */
  luaL_setfuncs(L, x509_metameth, 0);   /* add metamethods to new metatable */
  luaL_newlibtable(L, x509_meth);       /* create method table */
  luaL_setfuncs(L, x509_meth, 0);       /* add file methods to method table */
  lua_setfield(L, -2, "__index");       /* metatable.__index = method table */
  lua_pop(L, 1);                        /* pop metatable */
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
int lua_create_x509(lua_State *L) {
#ifdef USE_ASN1X509
  luaL_newlib(L, s_LuaX509Inferfaces);
  x509_createmeta(L);
#endif
  return 1;
}
