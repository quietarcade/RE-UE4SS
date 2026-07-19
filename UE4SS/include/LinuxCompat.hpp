/**
 * LinuxCompat.hpp - MSVC compatibility shims for Linux builds
 * 
 * Maps MSVC-specific functions to standard POSIX/C equivalents.
 * Included globally via forced include on Linux builds.
 */

#pragma once

#ifdef PLATFORM_LINUX

#include <cstdio>
#include <cstring>
#include <cstdarg>

// MSVC "safe" printf variants - just use standard printf on Linux
#ifndef printf_s
    #define printf_s printf
#endif

#ifndef fprintf_s
    #define fprintf_s fprintf
#endif

#ifndef sprintf_s
    #define sprintf_s snprintf
#endif

#ifndef sscanf_s
    #define sscanf_s sscanf
#endif

#ifndef _stricmp
    #define _stricmp strcasecmp
#endif

#ifndef _wcsicmp
    #define _wcsicmp wcscasecmp
#endif

// freopen_s replacement
#ifndef freopen_s
inline int freopen_s(FILE** pFile, const char* filename, const char* mode, FILE* stream)
{
    *pFile = freopen(filename, mode, stream);
    return (*pFile == nullptr) ? -1 : 0;
}
#endif

// __debugbreak replacement
#ifndef __debugbreak
    #define __debugbreak() __builtin_trap()
#endif

// OutputDebugStringA/W - no-op on Linux
#ifndef OutputDebugStringA
    #define OutputDebugStringA(x) ((void)0)
#endif
#ifndef OutputDebugStringW
    #define OutputDebugStringW(x) ((void)0)
#endif

#endif // PLATFORM_LINUX
