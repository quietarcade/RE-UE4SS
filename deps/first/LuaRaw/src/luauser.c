#include "lua.h"

// Stub lock functions - no-op on Linux for now
void LuaLockInitial(lua_State* L) { (void)L; }
void LuaLockFinal(lua_State* L) { (void)L; }
void LuaLock(lua_State* L) { (void)L; }
void LuaUnlock(lua_State* L) { (void)L; }
