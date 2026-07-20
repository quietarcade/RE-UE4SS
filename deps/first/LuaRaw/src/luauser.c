#include "lua.h"

#ifdef _WIN32
#include <windows.h>

static struct {
    CRITICAL_SECTION LockSct;
    BOOL Init;
} Gl;

void LuaLockInitial(lua_State* L)
{
    if (!Gl.Init)
    {
        InitializeCriticalSection(&Gl.LockSct);
        Gl.Init = TRUE;
    }
}

void LuaLockFinal(lua_State* L)
{
    if (Gl.Init)
    {
        DeleteCriticalSection(&Gl.LockSct);
        Gl.Init = FALSE;
    }
}

void LuaLock(lua_State* L)
{
    LuaLockInitial(L);
    EnterCriticalSection(&Gl.LockSct);
}

void LuaUnlock(lua_State* L)
{
    LeaveCriticalSection(&Gl.LockSct);
}

#else
#include <pthread.h>

static struct {
    pthread_mutex_t mutex;
    int init;
} Gl;

void LuaLockInitial(lua_State* L)
{
    if (!Gl.init)
    {
        pthread_mutex_init(&Gl.mutex, NULL);
        Gl.init = 1;
    }
}

void LuaLockFinal(lua_State* L)
{
    if (Gl.init)
    {
        pthread_mutex_destroy(&Gl.mutex);
        Gl.init = 0;
    }
}

void LuaLock(lua_State* L)
{
    LuaLockInitial(L);
    pthread_mutex_lock(&Gl.mutex);
}

void LuaUnlock(lua_State* L)
{
    pthread_mutex_unlock(&Gl.mutex);
}

#endif
