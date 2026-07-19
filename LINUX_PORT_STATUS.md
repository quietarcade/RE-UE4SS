# UE4SS Linux Port - Status & Next Steps

## Branch
- **RE-UE4SS**: `feat/linux-headless-port` on `quietarcade/RE-UE4SS`
- **UEPseudo**: `feat/linux-hooks` on `quietarcade/UEPseudo`

## Goal
Build `libUE4SS.so` that can be loaded via `LD_PRELOAD` into a native Linux Palworld dedicated server to enable Lua mod support.

## What's Done

### Build System
- xmake allows `linux` platform with `x86_64` arch
- Linux platform type with correct defines (`PLATFORM_LINUX`, `PLATFORM_UNIX`, `UE4SS_HEADLESS`)
- UVTD, cppmods, glad, patternsleuth excluded on Linux
- Rust/MSVC version checks skipped on Linux
- `LinuxCompat.hpp` force-included (printf_s, __declspec, strncpy_s, API macros)
- GCC flags: `-fpermissive`, `-fms-extensions`, `-Wno-error`, `-Wno-changes-meaning`

### Platform Headers (UEPseudo)
- `Linux/LinuxPlatform.hpp` - FPlatformTypes (wchar_t TCHAR, 4-byte), PLATFORM_DESKTOP, PLATFORM_64BITS, etc
- `Linux/LinuxPlatformCompilerPreSetup.hpp` - __has_warning shim, pragma macros
- `Linux/LinuxPlatformCompilerSetup.hpp` - FORCEINLINE, DLLEXPORT, RESTRICT
- `Linux/LinuxPlatformAtomics.hpp` - FLinuxPlatformAtomics with __sync builtins (all overloads)
- `Linux/LinuxPlatformMemory.hpp` - FLinuxPlatformMemory (memmove/memcpy/memswap etc)
- `Linux/LinuxPlatformMath.hpp` - typedef to FGenericPlatformMath
- `Linux/LinuxPlatformMisc.hpp` - FLinuxPlatformMisc with MemoryBarrier
- `Linux/LinuxPlatformString.hpp` - typedef to FGenericPlatformString
- `Linux/LinuxPlatformProperties.hpp` - typedef to FGenericPlatformProperties
- `Linux/LinuxPlatformFile.h` - empty stub
- `Windows/MinimalWindowsApi.hpp` - no-op on non-Windows instead of #error
- Template ordering fix in ContainerAllocationPolicies.hpp
- __FUNCDNAME__ -> __PRETTY_FUNCTION__ in VirtualFunctionHelper.hpp
- DECLARE_VIRTUAL_TYPE_BASE inlined to avoid TypeAccessor redeclaration
- ScanOverrides/FNameToStringMethod pragma suppress

### UE4SS Source Guards
- `main_ue4ss_linux.cpp` - LD_PRELOAD entry point
- `CrashDumperLinux.cpp` - signal-based crash handler
- `CppModLinux.cpp` - dlopen/dlsym mod loader
- `SinglePassSigScannerLinux.cpp` - /proc/self/maps memory scanner
- `LinuxDetour.hpp` - mprotect-based inline x64 function hooking
- Platform guards on: UE4SSProgram.cpp, CrashDumper.cpp, CppMod.cpp, main_ue4ss_rewritten.cpp, SinglePassSigScanner.cpp, Win32AsyncInputSource.cpp, PlatformInit.cpp, UEHeaderGenerator.cpp, LuaLibrary.cpp
- GUI headers guarded with `#ifndef UE4SS_HEADLESS`
- `#if PLATFORM_WINDOWS` used everywhere (not `#ifdef`, since it's defined as 0)
- CppMod.hpp: Windows HMODULE guarded, uses void* on Linux

### String Type Decision
Currently: `CharType = char` for headless builds (in StringType.hpp). UE's TCHAR is `wchar_t` (4 bytes on Linux).

## Remaining Issues (Next Session)

### Critical: String Type Conflict
**Problem**: With `CharType = char`, `StringViewType = std::string_view`. But `UE4SSProgram.hpp` has two overloads of `find_mod_by_name` - one taking `std::string_view` and one taking `StringViewType`. When they're the same type, GCC rejects the duplicate.

**Options**:
1. Use `wchar_t` as CharType on Linux (matches TCHAR, avoids conflicts) and fix `fmt` chrono formatting with wchar_t
2. Use `char16_t` as CharType (like the FORCE_U16 path) - needs fmt support for char16_t
3. Keep `char` but add `#if` guards around the conflicting overloads
4. Best option: Use `wchar_t` for CharType on Linux. For the `fmt` chrono issue, narrow-convert the time string (format with `char` fmt, then widen to wchar_t)

### GUI References in SettingsManager.hpp
Line 83-84 reference `GUI::RenderMode` type. Needs `#ifndef UE4SS_HEADLESS` guard.

### Template Specialization in Class Body (UE4SSProgram.hpp line 339)
`find_mod_by_name<LuaMod>` explicit specialization inside class - GCC rejects. Move outside the class.

### Files Not Yet Compiled (will reveal more errors)
The build currently fails at `UE4SSProgram.cpp` compilation. Once that passes, we'll hit errors in:
- `LuaMod.cpp` (the Lua mod loader - critical for our goal)
- `Mod.cpp`
- `FilesystemWatcher.cpp`
- `LuaType/*.cpp` files
- Deps: `Unreal/src/UnrealInitializer.cpp`, `Unreal/src/UObject.cpp` (have `#include <Windows.h>` that need `#if _WIN32` guards)

### Linking Phase
After all .cpp files compile, the linker will need:
- All dependency libraries linked (LuaMadeSimple, LuaRaw, File, DynamicOutput, etc)
- The deps' own .cpp files to compile on Linux (some have Windows.h includes)
- Missing symbols resolved (any Windows APIs we missed)

## Build Command
```bash
source ~/.xmake/profile
cd /tmp/RE-UE4SS
git clone -b feat/linux-headless-port --recurse-submodules git@github.com:quietarcade/RE-UE4SS.git
cd RE-UE4SS
xmake f -p linux -a x86_64 -m "Game__Shipping__Linux" --yes
xmake build -j2
```

## Estimated Remaining Work
- Fix string type (1-2 hours - most impactful decision)
- Guard remaining GUI/Windows refs (30 min)
- Fix template specialization issues (30 min)
- Compile Unreal submodule .cpp files (1-2 hours of iteration)
- Compile remaining UE4SS .cpp files (1-2 hours)
- Linking (30 min - mostly dep resolution)
- Testing on Palworld server (1 hour)

Total: ~6-8 more hours of iteration to get a compilable .so, then testing.
