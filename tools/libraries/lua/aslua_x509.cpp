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

#define LuaX509_Check(L) ((LuaX509_Type *)luaL_checkudata(L, 1, LUA_X509HANDLE))
/* ================================ [ TYPES     ] ============================================== */
#ifdef USE_ASN1X509
typedef struct {
  X509 *Ca;
} LuaX509_Type;
#endif
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
#ifdef USE_ASN1X509
static int LuaX509_Open(lua_State *L) {
  int n = lua_gettop(L); /* number of arguments */
  if (1 == n) {
    size_t len = 0;
    auto s = lua_tolstring(L, 1, &len);
    if (nullptr == s) {
      return luaL_error(L, "X509_Open with invalid arguments");
    }
    auto ca = new X509(std::string(s));
    LuaX509_Type *lx509 = (LuaX509_Type *)lua_newuserdatauv(L, sizeof(LuaX509_Type), 0);
    lx509->Ca = ca;
    (void)lx509;
    luaL_setmetatable(L, LUA_X509HANDLE);
  } else {
    return luaL_error(L, "X509_Open with invalid number of arguments: %d", n);
  }
  return 1;
}

static const luaL_Reg s_LuaX509Interfaces[] = {
  {"open", LuaX509_Open},
  {nullptr, nullptr},
};

static int LuaX509_Gc(lua_State *L) {
  LuaX509_Type *lx509 = LuaX509_Check(L);
  if (nullptr != lx509->Ca) {
    delete lx509->Ca;
    lx509->Ca = nullptr;
  }
  return 0;
}

static int LuaX509_ToString(lua_State *L) {
  LuaX509_Type *lx509 = LuaX509_Check(L);
  if (nullptr == lx509->Ca)
    lua_pushliteral(L, "x509 (closed)");
  else
    lua_pushfstring(L, "x509 (%p)", lx509->Ca);
  return 1;
}
/*
** metamethods for x509 handles
*/
static const luaL_Reg s_X509MetaMeth[] = {
  {"__index", NULL},                /* place holder */
  {"__gc", LuaX509_Gc},             /* clean up */
  {"__close", LuaX509_Gc},          /* close it */
  {"__tostring", LuaX509_ToString}, /* check state */
  {NULL, NULL},
};

static int LuaX509_PublicKey(lua_State *L) {
  LuaX509_Type *lx509 = LuaX509_Check(L);
  auto key = lx509->Ca->PublicKey();

  lua_newtable(L);
  for (int i = 0; i < (int)key.size(); i++) {
    lua_pushinteger(L, i + 1);  /* push key */
    lua_pushinteger(L, key[i]); /* push value */
    lua_settable(L, -3);        /* pop to the table */
  }

  return 1;
}

static int LuaX509_Signature(lua_State *L) {
  LuaX509_Type *lx509 = LuaX509_Check(L);
  auto sig = lx509->Ca->Signature();

  lua_newtable(L);
  for (int i = 0; i < (int)sig.size(); i++) {
    lua_pushinteger(L, i + 1);  /* push key */
    lua_pushinteger(L, sig[i]); /* push value */
    lua_settable(L, -3);        /* pop to the table */
  }

  return 1;
}

static int LuaX509_TbsCertificate(lua_State *L) {
  LuaX509_Type *lx509 = LuaX509_Check(L);
  auto tbs = lx509->Ca->TbsCertificate();

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
static const luaL_Reg s_X509Meth[] = {
  {"public_key", LuaX509_PublicKey},
  {"signature", LuaX509_Signature},
  {"tbs_certificate", LuaX509_TbsCertificate},
  {NULL, NULL},
};

static void X509_CreateMeta(lua_State *L) {
  luaL_newmetatable(L, LUA_X509HANDLE); /* metatable for file handles */
  luaL_setfuncs(L, s_X509MetaMeth, 0);  /* add metamethods to new metatable */
  luaL_newlibtable(L, s_X509Meth);      /* create method table */
  luaL_setfuncs(L, s_X509Meth, 0);      /* add file methods to method table */
  lua_setfield(L, -2, "__index");       /* metatable.__index = method table */
  lua_pop(L, 1);                        /* pop metatable */
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
int lua_create_x509(lua_State *L) {
#ifdef USE_ASN1X509
  luaL_newlib(L, s_LuaX509Interfaces);
  X509_CreateMeta(L);
#endif
  return 1;
}
