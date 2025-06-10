/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "aslua_priv.hpp"
#include "canlib.h"
#include "Std_Timer.h"
using namespace as;
/* ================================ [ MACROS    ] ============================================== */
#define LUA_CAN_HANDLE "CAN*"

#ifndef CAN_DEVICE_NAME_SIZE
#define CAN_DEVICE_NAME_SIZE 128
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  char device[CAN_DEVICE_NAME_SIZE];
  int port;
  int baudrate;
  int busid;
} LuaCan_t;

#define toLuaCan(L) ((LuaCan_t *)luaL_checkudata(L, 1, LUA_CAN_HANDLE))
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static int LuaCan_open(lua_State *L) {
  int n = lua_gettop(L); /* number of arguments */
  if (1 == n) {
    int isnum = FALSE;
    int busid = lua_tointegerx(L, 1, &isnum);
    if (FALSE == isnum) {
      return luaL_error(L, "invalid busid");
    }
    LuaCan_t *luaCan = (LuaCan_t *)lua_newuserdatauv(L, sizeof(LuaCan_t), 0);
    luaCan->device[0] = '\0';
    luaCan->port = 0;
    luaCan->baudrate = 0;
    luaCan->busid = busid;
    (void)luaCan;
    luaL_setmetatable(L, LUA_CAN_HANDLE);
  } else if (3 == n) {
    size_t len = 0;
    int isnum = FALSE;
    auto device = lua_tolstring(L, 1, &len);
    if (nullptr == device) {
      return luaL_error(L, "invalid device");
    }
    int port = lua_tointegerx(L, 2, &isnum);
    if (FALSE == isnum) {
      return luaL_error(L, "invalid port");
    }
    int baudrate = lua_tointegerx(L, 3, &isnum);
    if (FALSE == isnum) {
      return luaL_error(L, "invalid baudrate");
    }
    int busid = can_open(device, port, baudrate);
    if (busid < 0) {
      return luaL_error(L, "open(%s, %d, %d) failed\n", device, port, baudrate);
    }
    LuaCan_t *luaCan = (LuaCan_t *)lua_newuserdatauv(L, sizeof(LuaCan_t), 0);
    strncpy(luaCan->device, device, sizeof(luaCan->device));
    luaCan->port = port;
    luaCan->baudrate = baudrate;
    luaCan->busid = busid;
    (void)luaCan;
    luaL_setmetatable(L, LUA_CAN_HANDLE);
  } else {
    return luaL_error(L, "invalid number of arguments: %d", n);
  }

  return 1;
}

static const luaL_Reg s_LuaCanInferfaces[] = {
  {"open", LuaCan_open},
  {nullptr, nullptr},
};

static int LuaCan_gc(lua_State *L) {
  LuaCan_t *luaCan = toLuaCan(L);

  if ('\0' != luaCan->device[0]) {
    can_close(luaCan->busid);
  }

  luaCan->busid = -1;

  return 0;
}

static int LuaCan_tostring(lua_State *L) {
  LuaCan_t *luaCan = toLuaCan(L);
  if (luaCan->busid < 0)
    lua_pushliteral(L, "LuaCan (closed)");
  else
    lua_pushfstring(L, "LuaCan (%s, %d, %d) busid=%d", luaCan->device, luaCan->port,
                    luaCan->baudrate, luaCan->busid);
  return 1;
}
/*
** metamethods for LuaCan handles
*/
static const luaL_Reg LuaCan_metameth[] = {
  {"__index", NULL},               /* place holder */
  {"__gc", LuaCan_gc},             /* clean up */
  {"__close", LuaCan_gc},          /* close it */
  {"__tostring", LuaCan_tostring}, /* check state */
  {NULL, NULL},
};

static int LuaCan_write(lua_State *L) {
  LuaCan_t *luaCan = toLuaCan(L);
  uint32_t canid;
  int r = 0;
  int isnum = FALSE;
  lua_Integer len;

  int n = lua_gettop(L); /* number of arguments */
  if (3 == n) {
    canid = lua_tointegerx(L, 2, &isnum);
    if (!isnum) {
      return luaL_error(L, "invalid canid");
    }

    std::vector<uint8_t> data;
    lua_pushvalue(L, 3);
    switch (LUA_ARG_TYPE_UINT8_N) {
      POP_ARRAY(UINT8_N, 0, data, uint8_t);
    default:
      break;
    }

    if (0 != r) {
      return luaL_error(L, "invalid data");
    }

    r = can_write(luaCan->busid, canid, (uint8_t)data.size(), data.data());
    if (TRUE != r) {
      return luaL_error(L, "can write failed");
    } else {
      lua_pushboolean(L, TRUE);
    }
  } else {
    return luaL_error(L, "can write with invalid number of arguments: %d", n);
  }

  return 1;
}

static int LuaCan_read(lua_State *L) {
  LuaCan_t *luaCan = toLuaCan(L);
  uint32_t canid;
  uint8_t dlc;
  uint8_t data[64];
  int r = 0;
  int isnum = FALSE;
  std_time_t timeoutUs = 0;
  Std_TimerType timer;

  int n = lua_gettop(L); /* number of arguments */
  if ((2 == n) || (3 == n)) {
    canid = lua_tointegerx(L, 2, &isnum);
    if (!isnum) {
      return luaL_error(L, "invalid canid");
    }

    if (3 == n) {
      timeoutUs = lua_tointegerx(L, 3, &isnum);
      if (!isnum) {
        return luaL_error(L, "invalid timeout");
      }
      timeoutUs = timeoutUs * 1000;
    }

    Std_TimerStart(&timer);
    do {
      dlc = sizeof(data);
      r = can_read(luaCan->busid, &canid, &dlc, data);
      if (TRUE != r) {
        Std_Sleep(1000);
        dlc = sizeof(data);
        r = can_read(luaCan->busid, &canid, &dlc, data);
      }
      if (TRUE != r) {
        if (Std_GetTimerElapsedTime(&timer) > timeoutUs) {
          break;
        }
      }
    } while (TRUE != r);

    if (TRUE != r) {
      lua_pushboolean(L, FALSE);
      lua_pushnil(L);
      lua_pushnil(L);
    } else {
      lua_pushboolean(L, TRUE);
      lua_pushinteger(L, canid);
      lua_newtable(L);
      for (int i = 0; i < (int)dlc; i++) {
        lua_pushinteger(L, i + 1);   /* push key */
        lua_pushinteger(L, data[i]); /* push value */
        lua_settable(L, -3);         /* pop to the table */
      }
    }
  } else {
    return luaL_error(L, "can read with invalid number of arguments: %d", n);
  }

  return 3;
}

static int LuaCan_sleep(lua_State *L) {
  int isnum = FALSE;
  std_time_t timeoutUs = 0;

  int n = lua_gettop(L); /* number of arguments */
  if (2 == n) {
    timeoutUs = lua_tointegerx(L, 2, &isnum);
    if (!isnum) {
      return luaL_error(L, "invalid timeout");
    }
    timeoutUs = timeoutUs * 1000;
    Std_Sleep(timeoutUs);
    lua_pushboolean(L, TRUE);
  } else {
    return luaL_error(L, "can read with invalid number of arguments: %d", n);
  }

  return 1;
}

/*
** methods for LuaCan handles
*/
static const luaL_Reg LuaCan_meth[] = {
  {"write", LuaCan_write},
  {"read", LuaCan_read},
  {"sleep", LuaCan_sleep},
  {NULL, NULL},
};

static void LuaCan_createmeta(lua_State *L) {
  luaL_newmetatable(L, LUA_CAN_HANDLE); /* metatable for file handles */
  luaL_setfuncs(L, LuaCan_metameth, 0); /* add metamethods to new metatable */
  luaL_newlibtable(L, LuaCan_meth);     /* create method table */
  luaL_setfuncs(L, LuaCan_meth, 0);     /* add file methods to method table */
  lua_setfield(L, -2, "__index");       /* metatable.__index = method table */
  lua_pop(L, 1);                        /* pop metatable */
}
/* ================================ [ FUNCTIONS ] ============================================== */
int lua_create_can(lua_State *L) {
  luaL_newlib(L, s_LuaCanInferfaces);
  LuaCan_createmeta(L);
  return 1;
}
