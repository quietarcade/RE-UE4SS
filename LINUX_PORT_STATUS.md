# UE4SS Linux Port - Status & Next Steps

## Branch
- **RE-UE4SS**: `feat/linux-headless-port` on `quietarcade/RE-UE4SS`
- **UEPseudo**: `feat/linux-hooks` on `quietarcade/UEPseudo`

## Goal
Build `libUE4SS.so` that can be loaded via `LD_PRELOAD` into a native Linux Palworld dedicated server to enable Lua mod support.

## Current Status: COMPILED AND LINKED SUCCESSFULLY

`libUE4SS.so` (248MB, ELF 64-bit x86_64 shared object) builds with exit code 0.

Output: `/tmp/RE-UE4SS/Binaries/Game__Shipping__Linux/UE4SS/libUE4SS.so`

---

## What's Done

### Build System
- xmake allows `linux` platform with `x86_64` arch
- Linux platform type with correct defines (`PLATFORM_LINUX`, `PLATFORM_UNIX`, `UE4SS_HEADLESS`)
- Global defines at root xmake.lua level (ensures all targets see them)
- UVTD, cppmods, glad, patternsleuth excluded on Linux
- Rust/MSVC version checks skipped on Linux
- `LinuxCompat.hpp` force-included globally (printf_s, __declspec, strncpy_s, API macros)
- GCC flags: `-fpermissive`, `-fms-extensions`, `-fno-char8_t`, `-fPIC`, `-Wno-error`
- `-fPIC` added globally for all C and C++ targets (required for linking static libs into .so)

### Platform Headers (UEPseudo)
- `Linux/LinuxPlatform.hpp` - FPlatformTypes (wchar_t TCHAR, 4-byte), PLATFORM_DESKTOP, PLATFORM_64BITS, etc.
- `Linux/LinuxPlatformCompilerPreSetup.hpp` - __has_warning shim, pragma macros
- `Linux/LinuxPlatformCompilerSetup.hpp` - FORCEINLINE, DLLEXPORT, RESTRICT
- `Linux/LinuxPlatformAtomics.hpp` - FLinuxPlatformAtomics with __sync builtins
- `Linux/LinuxPlatformMemory.hpp` - FLinuxPlatformMemory (memmove/memcpy/memswap)
- `Linux/LinuxPlatformMath.hpp` - typedef to FGenericPlatformMath
- `Linux/LinuxPlatformMisc.hpp` - FLinuxPlatformMisc with MemoryBarrier
- `Linux/LinuxPlatformString.hpp` - typedef to FGenericPlatformString
- `Linux/LinuxPlatformProperties.hpp` - typedef
- `Linux/LinuxPlatformFile.h` - empty stub
- `Windows/MinimalWindowsApi.hpp` - no-op on non-Windows
- Template ordering fix in ContainerAllocationPolicies.hpp
- __FUNCDNAME__ -> __PRETTY_FUNCTION__ in VirtualFunctionHelper.hpp
- DECLARE_VIRTUAL_TYPE_BASE inlined to avoid TypeAccessor redeclaration
- ScanOverrides/FNameToStringMethod pragma suppress in UnrealInitializer.hpp
- Common.hpp: FORCEINLINE/FORCENOINLINE guarded with `#ifndef` and `#ifdef __linux__`
- LinuxPlatform.hpp: FORCEINLINE/FORCENOINLINE guarded with `#ifndef`
- UAssetRegistry.cpp: Windows includes (`psapi.h`, `WindowsHWrapper.hpp`) guarded with `#ifndef __linux__`
- ByteSwap.hpp: uses `PLATFORM_TCHAR_IS_4_BYTES` (set to 1 in LinuxPlatform.hpp) for 4-byte TCHAR support

### UE4SS Source Guards
- `main_ue4ss_linux.cpp` - LD_PRELOAD entry point
- `CrashDumperLinux.cpp` - signal-based crash handler
- `CppModLinux.cpp` - dlopen/dlsym mod loader
- `SinglePassSigScannerLinux.cpp` - /proc/self/maps memory scanner
- `LinuxDetour.hpp` - mprotect-based inline x64 function hooking (namespace alias PLH = LinuxHook)
- Platform guards on all Windows-specific sources
- GUI/GUI.hpp and GUI/Console.hpp guarded with `#ifndef UE4SS_HEADLESS`
- CppMod.hpp: Windows HMODULE guarded, uses void* on Linux

### Dependency Fixes
- `luauser.c`: Windows CRITICAL_SECTION replaced with pthread_mutex_t on Linux
- `JSON/Number.hpp`: Added `long long` / `unsigned long long` constructor overloads for LP64 Linux (where `int64_t = long` but `long long` is distinct)
- `FMemory.cpp`: `_BitScanForward` replaced with `__builtin_ctz` via `#ifdef _MSC_VER`

### String Type
- `CharType = wchar_t` on Linux (matches UE's TCHAR, 4 bytes)
- `fmt` chrono issue fixed by formatting with narrow chars then widening
- `STR()` macro produces `L""` literals on Linux

---

## Next Steps: Runtime Testing

1. **Strip debug symbols** to reduce .so size (248MB -> ~20-30MB expected)
   ```bash
   strip --strip-debug /tmp/RE-UE4SS/Binaries/Game__Shipping__Linux/UE4SS/libUE4SS.so
   ```

2. **Deploy to Palworld server**
   ```bash
   cp libUE4SS.so /srv/games/palworld-modded/
   # Update start.sh to use LD_PRELOAD=./libUE4SS.so
   ```

3. **Create UE4SS config** - `UE4SS-settings.ini` in the game directory

4. **Test basic loading** - verify UE4SS initializes (check logs for startup messages)

5. **Test Lua mod loading** - create a simple test mod that writes to a file on load

6. **Test BetterBaseRange mod** - the actual target mod for extended base building range

---

## Build Command
```bash
source ~/.xmake/profile
cd /tmp/RE-UE4SS
xmake f -p linux -a x86_64 -m "Game__Shipping__Linux" -c --ccache=n --yes
xmake build -j1
```

Note: `-j1` is recommended. With `-j2` the build may get killed on systems with other running services. The full build takes ~8-10 minutes with `-j1`.

---

## Known Warnings (non-blocking)
- FORCEINLINE/FORCENOINLINE redefinition warnings between Common.hpp and LinuxPlatform.hpp (harmless, same value on both sides when `__linux__` is defined)
- `friend declaration ... declares a non-template function` in HandleTemplate.hpp
- Deprecated enum-enum-conversion in UnrealType.hpp
- `-Wno-gnu-line-marker` unrecognized (GCC doesn't support it, only Clang)

## Architecture Notes
- The .so is loaded via `LD_PRELOAD` - constructor function runs `UE4SSProgram::setup()`
- Module scanning uses `/proc/self/maps` to find UE binary segments
- Function hooking uses mprotect-based inline x64 patching (LinuxDetour.hpp)
- Lua mods loaded from `Mods/` directory relative to the game binary
- No GUI - all output goes to stdout/log files (headless mode)
