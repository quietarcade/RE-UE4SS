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

#ifndef strncpy_s
    // strncpy_s(dest, src, count) - simplified version
    #define strncpy_s(dest, src, count) strncpy(dest, src, count)
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

// fopen_s replacement
#ifndef fopen_s
inline int fopen_s(FILE** pFile, const char* filename, const char* mode)
{
    *pFile = fopen(filename, mode);
    return (*pFile == nullptr) ? -1 : 0;
}
#endif

// __debugbreak replacement
#ifndef __debugbreak
    #define __debugbreak() __builtin_trap()
#endif

// __forceinline is MSVC-specific
#ifndef __forceinline
    #define __forceinline __attribute__((always_inline)) inline
#endif

// __int64 is MSVC-specific
#ifndef __int64
    #define __int64 long long
#endif

// TCHAR is in RC::Unreal namespace on Linux but Windows makes it global
// Provide a global typedef for compatibility
#include <cwchar>
using TCHAR = wchar_t;

// __declspec replacement - map to GCC visibility attribute
#ifndef __declspec
    #define __declspec(x)
#endif

// OutputDebugStringA/W - no-op on Linux
#ifndef OutputDebugStringA
    #define OutputDebugStringA(x) ((void)0)
#endif
#ifndef OutputDebugStringW
    #define OutputDebugStringW(x) ((void)0)
#endif

// GetCurrentProcess - returns pseudo-handle on Windows, just use nullptr on Linux
#ifndef GetCurrentProcess
    #define GetCurrentProcess() ((void*)(-1))
#endif

// MSVC stack allocation
#include <alloca.h>
#ifndef _malloca
    #define _malloca(size) alloca(size)
#endif
#ifndef _freea
    #define _freea(ptr) ((void)0)
#endif

// DLL export/import macros - on Linux shared libs, use visibility attribute or just empty
#ifndef RC_DYNOUT_API
    #define RC_DYNOUT_API
#endif
#ifndef RC_FILE_API
    #define RC_FILE_API
#endif
#ifndef RC_UE4SS_API
    #define RC_UE4SS_API
#endif
#ifndef RC_UE_API
    #define RC_UE_API
#endif
#ifndef RC_HELPERS_API
    #define RC_HELPERS_API
#endif
#ifndef RC_INI_API
    #define RC_INI_API
#endif
#ifndef RC_JSON_API
    #define RC_JSON_API
#endif
#ifndef RC_INPUT_API
    #define RC_INPUT_API
#endif
#ifndef RC_LMS_API
    #define RC_LMS_API
#endif
#ifndef RC_FUNCTION_API
    #define RC_FUNCTION_API
#endif
#ifndef RC_PARSER_BASE_API
    #define RC_PARSER_BASE_API
#endif
#ifndef RC_CONSTRUCTS_API
    #define RC_CONSTRUCTS_API
#endif
#ifndef RC_SCOPED_TIMER_API
    #define RC_SCOPED_TIMER_API
#endif
#ifndef RC_SINGLE_PASS_SIG_SCANNER_API
    #define RC_SINGLE_PASS_SIG_SCANNER_API
#endif
#ifndef RC_MPROGRAM_API
    #define RC_MPROGRAM_API
#endif
#ifndef RC_ASM_HELPER_API
    #define RC_ASM_HELPER_API
#endif

#endif // PLATFORM_LINUX
