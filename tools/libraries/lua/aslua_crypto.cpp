/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef _MSC_VER
/* ================================ [ INCLUDES  ] ============================================== */
#include "aslua_priv.hpp"
#include "crypto.h"

/* ================================ [ MACROS    ] ============================================== */
#define MAX_RSA_SIZE 4096
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static int LuaCrypto_Keygen(lua_State *L) {
  int n = lua_gettop(L); /* number of arguments */
  if (1 == n) {
    int isnum;
    auto bitsize = lua_tointegerx(L, 1, &isnum);
    if ((0 == isnum) || (bitsize <= 0)) {
      return luaL_error(L, "keygen with invalid arguments");
    }

    if (bitsize > (MAX_RSA_SIZE * 8)) {
      return luaL_error(L, "keygen with bitsize too big");
    }

    std::vector<uint8_t> public_key;
    public_key.resize(bitsize * 5 / 8);
    uint32_t public_key_len = public_key.size();

    std::vector<uint8_t> private_key;
    private_key.resize(bitsize * 5 / 8);
    uint32_t private_key_len = private_key.size();

    auto r = crypto_rsa_keygen(bitsize, public_key.data(), &public_key_len, private_key.data(),
                               &private_key_len);
    if (r != 0) {
      return luaL_error(L, "keygen failed: %d", r);
    }

    /* return private key. */
    lua_newtable(L);
    for (uint32_t i = 0; i < private_key_len; i++) {
      lua_pushinteger(L, i + 1);          /* push key */
      lua_pushinteger(L, private_key[i]); /* push value */
      lua_settable(L, -3);                /* pop to the table */
    }

    /* return public key. */
    lua_newtable(L);
    for (uint32_t i = 0; i < public_key_len; i++) {
      lua_pushinteger(L, i + 1);         /* push key */
      lua_pushinteger(L, public_key[i]); /* push value */
      lua_settable(L, -3);               /* pop to the table */
    }
  } else {
    return luaL_error(L, "keygen with invalid number of arguments: %d", n);
  }

  return 2;
}

static int LuaCrypto_Sign(lua_State *L) {
  int n = lua_gettop(L); /* number of arguments */
  if (2 == n) {
    int r = 0;
    int len = 0;
    std::vector<uint8_t> private_key;
    lua_pushvalue(L, 1);
    switch (LUA_ARG_TYPE_UINT8_N) {
      POP_ARRAY(UINT8_N, 0, private_key, uint8_t);
    default:
      break;
    }
    if (0 != r) {
      return luaL_error(L, "sign with invalid arguments key");
    }

    std::vector<uint8_t> data;
    lua_pushvalue(L, 2);
    Lua_ArgTypeType dtype = LUA_ARG_TYPE_UINT8_N;
    if (lua_isstring(L, -1)) {
      dtype = LUA_ARG_TYPE_STRING;
    }
    switch (dtype) {
    case LUA_ARG_TYPE_STRING: {
      size_t sz = 0;
      auto s = lua_tolstring(L, -1, &sz);
      if (s != nullptr) {
        for (size_t i = 0; i < sz; i++) {
          data.push_back((uint8_t)s[i]);
        }
      } else {
        r = -EINVAL;
      }
      lua_pop(L, 1);
      break;
    }
      POP_ARRAY(UINT8_N, 0, data, uint8_t);
    default:
      break;
    }
    if (0 != r) {
      return luaL_error(L, "sign with invalid arguments data");
    }

    uint8_t sig[MAX_RSA_SIZE / 8];
    uint32_t siglen = sizeof(sig);

    r = crypto_rsa_sign(data.data(), data.size(), sig, &siglen, private_key.data(),
                        private_key.size());
    if (r != 0) {
      return luaL_error(L, "sign failed: %d", r);
    }
    /* return signature. */
    lua_newtable(L);
    for (int i = 0; i < (int)siglen; i++) {
      lua_pushinteger(L, i + 1);  /* push key */
      lua_pushinteger(L, sig[i]); /* push value */
      lua_settable(L, -3);        /* pop to the table */
    }
  } else {
    return luaL_error(L, "sign with invalid number of arguments: %d", n);
  }
  return 1;
}

static int LuaCrypto_Verify(lua_State *L) {
  int n = lua_gettop(L); /* number of arguments */
  if (3 == n) {
    int r = 0;
    int len = 0;
    std::vector<uint8_t> public_key;
    lua_pushvalue(L, 1);
    switch (LUA_ARG_TYPE_UINT8_N) {
      POP_ARRAY(UINT8_N, 0, public_key, uint8_t);
    default:
      break;
    }
    if (0 != r) {
      return luaL_error(L, "verify with invalid arguments key");
    }

    std::vector<uint8_t> data;
    lua_pushvalue(L, 2);
    Lua_ArgTypeType dtype = LUA_ARG_TYPE_UINT8_N;
    if (lua_isstring(L, -1)) {
      dtype = LUA_ARG_TYPE_STRING;
    }
    switch (dtype) {
    case LUA_ARG_TYPE_STRING: {
      size_t sz = 0;
      auto s = lua_tolstring(L, -1, &sz);
      if (s != nullptr) {
        for (size_t i = 0; i < sz; i++) {
          data.push_back((uint8_t)s[i]);
        }
      } else {
        r = -EINVAL;
      }
      lua_pop(L, 1);
      break;
    }
      POP_ARRAY(UINT8_N, 0, data, uint8_t);
    default:
      break;
    }
    if (0 != r) {
      return luaL_error(L, "verify with invalid arguments data");
    }

    std::vector<uint8_t> sig;
    lua_pushvalue(L, 3);
    switch (LUA_ARG_TYPE_UINT8_N) {
      POP_ARRAY(UINT8_N, 0, sig, uint8_t);
    default:
      break;
    }
    if (0 != r) {
      return luaL_error(L, "verify with invalid arguments signature");
    }

    r = crypto_rsa_verify(data.data(), data.size(), sig.data(), sig.size(), public_key.data(),
                          public_key.size());
    if (0 == r) {
      lua_pushboolean(L, true);
    } else if (E_INVALID_SIGNATURE == r) {
      lua_pushboolean(L, false);
    } else {
      return luaL_error(L, "verify failed: %d", r);
    }
  } else {
    return luaL_error(L, "verify with invalid number of arguments: %d", n);
  }
  return 1;
}

static int LuaCrypto_Encrypt(lua_State *L) {
  int n = lua_gettop(L); /* number of arguments */
  if (2 == n) {
    int r = 0;
    int len = 0;
    std::vector<uint8_t> public_key;
    lua_pushvalue(L, 1);
    switch (LUA_ARG_TYPE_UINT8_N) {
      POP_ARRAY(UINT8_N, 0, public_key, uint8_t);
    default:
      break;
    }
    if (0 != r) {
      return luaL_error(L, "encrypt with invalid arguments key");
    }

    std::vector<uint8_t> data;
    lua_pushvalue(L, 2);
    Lua_ArgTypeType dtype = LUA_ARG_TYPE_UINT8_N;
    if (lua_isstring(L, -1)) {
      dtype = LUA_ARG_TYPE_STRING;
    }
    switch (dtype) {
    case LUA_ARG_TYPE_STRING: {
      size_t sz = 0;
      auto s = lua_tolstring(L, -1, &sz);
      if (s != nullptr) {
        for (size_t i = 0; i < sz; i++) {
          data.push_back((uint8_t)s[i]);
        }
      } else {
        r = -EINVAL;
      }
      lua_pop(L, 1);
      break;
    }
      POP_ARRAY(UINT8_N, 0, data, uint8_t);
    default:
      break;
    }
    if (0 != r) {
      return luaL_error(L, "encrypt with invalid arguments data");
    }

    uint32_t outlen = MAX_RSA_SIZE;
    std::vector<uint8_t> out;
    out.resize(outlen);

    r = crypto_rsa_encrypt(data.data(), data.size(), out.data(), &outlen, public_key.data(),
                           public_key.size());
    if (r != 0) {
      return luaL_error(L, "encrypt failed: %d", r);
    }
    /* return encryption. */
    lua_newtable(L);
    for (int i = 0; i < (int)outlen; i++) {
      lua_pushinteger(L, i + 1);  /* push key */
      lua_pushinteger(L, out[i]); /* push value */
      lua_settable(L, -3);        /* pop to the table */
    }
  } else {
    return luaL_error(L, "encrypt with invalid number of arguments: %d", n);
  }
  return 1;
}

static int LuaCrypto_Decrypt(lua_State *L) {
  int n = lua_gettop(L); /* number of arguments */
  if (2 == n) {
    int r = 0;
    int len = 0;
    std::vector<uint8_t> private_key;
    lua_pushvalue(L, 1);
    switch (LUA_ARG_TYPE_UINT8_N) {
      POP_ARRAY(UINT8_N, 0, private_key, uint8_t);
    default:
      break;
    }
    if (0 != r) {
      return luaL_error(L, "decrypt with invalid arguments key");
    }

    std::vector<uint8_t> data;
    lua_pushvalue(L, 2);
    Lua_ArgTypeType dtype = LUA_ARG_TYPE_UINT8_N;
    if (lua_isstring(L, -1)) {
      dtype = LUA_ARG_TYPE_STRING;
    }
    switch (dtype) {
    case LUA_ARG_TYPE_STRING: {
      size_t sz = 0;
      auto s = lua_tolstring(L, -1, &sz);
      if (s != nullptr) {
        for (size_t i = 0; i < sz; i++) {
          data.push_back((uint8_t)s[i]);
        }
      } else {
        r = -EINVAL;
      }
      lua_pop(L, 1);
      break;
    }
      POP_ARRAY(UINT8_N, 0, data, uint8_t);
    default:
      break;
    }
    if (0 != r) {
      return luaL_error(L, "decrypt with invalid arguments data");
    }

    uint32_t outlen = data.size();
    std::vector<uint8_t> out;
    out.resize(outlen);

    r = crypto_rsa_decrypt(data.data(), data.size(), out.data(), &outlen, private_key.data(),
                           private_key.size());
    if (r != 0) {
      return luaL_error(L, "decrypt failed: %d", r);
    }
    /* return encryption. */
    lua_newtable(L);
    for (int i = 0; i < (int)outlen; i++) {
      lua_pushinteger(L, i + 1);  /* push key */
      lua_pushinteger(L, out[i]); /* push value */
      lua_settable(L, -3);        /* pop to the table */
    }
  } else {
    return luaL_error(L, "encrypt with invalid number of arguments: %d", n);
  }
  return 1;
}

static int LuaCrypto_Base64Encode(lua_State *L) {
  int n = lua_gettop(L); /* number of arguments */
  if (1 == n) {
    int r = 0;
    int len = 0;
    std::vector<uint8_t> data;
    lua_pushvalue(L, 1);
    switch (LUA_ARG_TYPE_UINT8_N) {
      POP_ARRAY(UINT8_N, 0, data, uint8_t);
      break;
    default:
      break;
    }
    if (0 != r) {
      return luaL_error(L, "base64_encode with invalid arguments");
    }
#ifdef USE_LTC
    ltc_mp = ltm_desc;
#endif
    std::vector<uint8_t> asc;
    asc.resize(data.size() * 2);
    uint32_t outlen = asc.size();
    r = crypto_base64_encode(data.data(), data.size(), asc.data(), &outlen);
    if (0 == r) {
      lua_pushstring(L, (char *)asc.data());
    } else {
      return luaL_error(L, "base64_encode failed: %d", r);
    }
  } else {
    return luaL_error(L, "base64_encode with invalid number of arguments: %d", n);
  }
  return 1;
}

static int LuaCrypto_Base64Decode(lua_State *L) {
  int n = lua_gettop(L); /* number of arguments */
  if (1 == n) {
    size_t len = 0;
    auto s = lua_tolstring(L, 1, &len);
    if (nullptr == s) {
      return luaL_error(L, "base64_decode with invalid arguments");
    }
#ifdef USE_LTC
    ltc_mp = ltm_desc;
#endif
    std::vector<uint8_t> bin;
    bin.resize(len);
    uint32_t outlen = bin.size();
    int r = crypto_base64_decode((uint8_t *)s, len, bin.data(), &outlen);
    if (0 == r) {
      lua_newtable(L);
      for (int i = 0; i < (int)outlen; i++) {
        lua_pushinteger(L, i + 1);  /* push key */
        lua_pushinteger(L, bin[i]); /* push value */
        lua_settable(L, -3);        /* pop to the table */
      }
    } else {
      return luaL_error(L, "base64_decode failed: %d", r);
    }
  } else {
    return luaL_error(L, "base64_decode with invalid number of arguments: %d", n);
  }
  return 1;
}

static const luaL_Reg s_LuaCryptoInterfaces[] = {
  {"keygen", LuaCrypto_Keygen},
  {"sign", LuaCrypto_Sign},
  {"verify", LuaCrypto_Verify},
  {"encrypt", LuaCrypto_Encrypt},
  {"decrypt", LuaCrypto_Decrypt},
  {"base64_encode", LuaCrypto_Base64Encode},
  {"base64_decode", LuaCrypto_Base64Decode},
  {nullptr, nullptr},
};
/* ================================ [ FUNCTIONS ] ============================================== */
int lua_create_crypto(lua_State *L) {
  luaL_newlib(L, s_LuaCryptoInterfaces);
  return 1;
}
#endif
