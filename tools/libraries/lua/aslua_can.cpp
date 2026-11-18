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
  char Device[CAN_DEVICE_NAME_SIZE];
  int Port;
  int Baudrate;
  int BusId;
} LuaCan_Type;

#define LuaCan_Check(L) ((LuaCan_Type *)luaL_checkudata(L, 1, LUA_CAN_HANDLE))
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static int LuaCan_Open(lua_State *L) {
  int n = lua_gettop(L); /* number of arguments */
  if (1 == n) {
    int isnum = FALSE;
    int busid = lua_tointegerx(L, 1, &isnum);
    if (FALSE == isnum) {
      return luaL_error(L, "invalid busid");
    }
    LuaCan_Type *luaCan = (LuaCan_Type *)lua_newuserdatauv(L, sizeof(LuaCan_Type), 0);
    luaCan->Device[0] = '\0';
    luaCan->Port = 0;
    luaCan->Baudrate = 0;
    luaCan->BusId = busid;
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
    int busid = CanL_Open(device, port, baudrate);
    if (busid < 0) {
      return luaL_error(L, "open(%s, %d, %d) failed\n", device, port, baudrate);
    }
    LuaCan_Type *luaCan = (LuaCan_Type *)lua_newuserdatauv(L, sizeof(LuaCan_Type), 0);
    strncpy(luaCan->Device, device, sizeof(luaCan->Device));
    luaCan->Port = port;
    luaCan->Baudrate = baudrate;
    luaCan->BusId = busid;
    (void)luaCan;
    luaL_setmetatable(L, LUA_CAN_HANDLE);
  } else {
    return luaL_error(L, "invalid number of arguments: %d", n);
  }

  return 1;
}

static const luaL_Reg s_LuaCanInterfaces[] = {
  {"open", LuaCan_Open},
  {nullptr, nullptr},
};

static int LuaCan_Gc(lua_State *L) {
  LuaCan_Type *luaCan = LuaCan_Check(L);

  if ('\0' != luaCan->Device[0]) {
    CanL_Close(luaCan->BusId);
  }

  luaCan->BusId = -1;

  return 0;
}

static int LuaCan_ToString(lua_State *L) {
  LuaCan_Type *luaCan = LuaCan_Check(L);
  if (luaCan->BusId < 0)
    lua_pushliteral(L, "LuaCan (closed)");
  else
    lua_pushfstring(L, "LuaCan (%s, %d, %d) busid=%d", luaCan->Device, luaCan->Port,
                    luaCan->Baudrate, luaCan->BusId);
  return 1;
}
/*
** metamethods for LuaCan handles
*/
static const luaL_Reg s_LuaCanMetaMeth[] = {
  {"__index", NULL},               /* place holder */
  {"__gc", LuaCan_Gc},             /* clean up */
  {"__close", LuaCan_Gc},          /* close it */
  {"__tostring", LuaCan_ToString}, /* check state */
  {NULL, NULL},
};

static int LuaCan_Write(lua_State *L) {
  LuaCan_Type *luaCan = LuaCan_Check(L);
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

    r = CanL_Write(luaCan->BusId, canid, (uint8_t)data.size(), data.data());
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

static int LuaCan_Read(lua_State *L) {
  LuaCan_Type *luaCan = LuaCan_Check(L);
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
      r = CanL_Read(luaCan->BusId, &canid, &dlc, data);
      if (TRUE != r) {
        Std_Sleep(1000);
        dlc = sizeof(data);
        r = CanL_Read(luaCan->BusId, &canid, &dlc, data);
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

static int LuaCan_Sleep(lua_State *L) {
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
static const luaL_Reg s_LuaCanMeth[] = {
  {"write", LuaCan_Write},
  {"read", LuaCan_Read},
  {"sleep", LuaCan_Sleep},
  {NULL, NULL},
};

static void LuaCan_CreateMeta(lua_State *L) {
  luaL_newmetatable(L, LUA_CAN_HANDLE);  /* metatable for file handles */
  luaL_setfuncs(L, s_LuaCanMetaMeth, 0); /* add metamethods to new metatable */
  luaL_newlibtable(L, s_LuaCanMeth);     /* create method table */
  luaL_setfuncs(L, s_LuaCanMeth, 0);     /* add file methods to method table */
  lua_setfield(L, -2, "__index");        /* metatable.__index = method table */
  lua_pop(L, 1);                         /* pop metatable */
}
/* ================================ [ FUNCTIONS ] ============================================== */
int lua_create_can(lua_State *L) {
  luaL_newlib(L, s_LuaCanInterfaces);
  LuaCan_CreateMeta(L);
  return 1;
}
