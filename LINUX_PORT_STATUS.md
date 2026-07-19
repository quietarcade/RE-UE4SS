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
- Global defines at root xmake.lua level (ensures all targets see them)
- UVTD, cppmods, glad, patternsleuth excluded on Linux
- Rust/MSVC version checks skipped on Linux
- `LinuxCompat.hpp` force-included (printf_s, __declspec, strncpy_s, API macros)
- GCC flags: `-fpermissive`, `-fms-extensions`, `-Wno-error`, `-Wno-changes-meaning`, `-Wno-template-id-cdtor`

### Platform Headers (UEPseudo)
- `Linux/LinuxPlatform.hpp` - FPlatformTypes (wchar_t TCHAR, 4-byte), PLATFORM_DESKTOP, PLATFORM_64BITS, etc.
- `Linux/LinuxPlatformCompilerPreSetup.hpp` - __has_warning shim, pragma macros
- `Linux/LinuxPlatformCompilerSetup.hpp` - FORCEINLINE, DLLEXPORT, RESTRICT
- `Linux/LinuxPlatformAtomics.hpp` - FLinuxPlatformAtomics with __sync builtins (int32, int64, long long overloads, AtomicRead, And/Or/Xor)
- `Linux/LinuxPlatformMemory.hpp` - FLinuxPlatformMemory (memmove/memcpy/memswap/BigBlock/Streaming/Parallel)
- `Linux/LinuxPlatformMath.hpp` - typedef to FGenericPlatformMath (in RC::Unreal namespace)
- `Linux/LinuxPlatformMisc.hpp` - FLinuxPlatformMisc with MemoryBarrier (in RC::Unreal namespace)
- `Linux/LinuxPlatformString.hpp` - typedef to FGenericPlatformString (in RC::Unreal namespace)
- `Linux/LinuxPlatformProperties.hpp` - typedef (in RC::Unreal namespace)
- `Linux/LinuxPlatformFile.h` - empty stub
- `Windows/MinimalWindowsApi.hpp` - no-op on non-Windows instead of #error
- Template ordering fix in ContainerAllocationPolicies.hpp
- __FUNCDNAME__ -> __PRETTY_FUNCTION__ in VirtualFunctionHelper.hpp
- DECLARE_VIRTUAL_TYPE_BASE inlined to avoid TypeAccessor redeclaration
- ScanOverrides/FNameToStringMethod pragma suppress in UnrealInitializer.hpp

### UE4SS Source Guards
- `main_ue4ss_linux.cpp` - LD_PRELOAD entry point
- `CrashDumperLinux.cpp` - signal-based crash handler
- `CppModLinux.cpp` - dlopen/dlsym mod loader
- `SinglePassSigScannerLinux.cpp` - /proc/self/maps memory scanner
- `LinuxDetour.hpp` - mprotect-based inline x64 function hooking (namespace alias PLH = LinuxHook)
- Platform guards on: UE4SSProgram.cpp, CrashDumper.cpp, CppMod.cpp, main_ue4ss_rewritten.cpp, SinglePassSigScanner.cpp, Win32AsyncInputSource.cpp, PlatformInit.cpp, UEHeaderGenerator.cpp, LuaLibrary.cpp
- GUI/GUI.hpp and GUI/Console.hpp guarded with `#ifndef UE4SS_HEADLESS`
- `#if PLATFORM_WINDOWS` used everywhere (not `#ifdef`, since HAL/Platform.hpp defines it as 0)
- CppMod.hpp: Windows HMODULE guarded, uses void* on Linux

### String Type (RESOLVED)
- `CharType = wchar_t` on Linux (matches UE's TCHAR, avoids overload conflicts)
- `fmt` chrono issue fixed by formatting with narrow chars then widening
- `STR()` macro produces `L""` literals on Linux
- No more `StringViewType == std::string_view` collision

---

## Remaining Issues - Detailed Audit

### CRITICAL: SettingsManager.hpp (cascading - included by everything)

**Problem**: Lines 83-84 use `GUI::GfxBackend` and `GUI::RenderMode` enums which don't exist in headless builds (GUI/GUI.hpp is fully `#ifdef`'d out).

**Fix**: Provide stub enum definitions that exist in headless builds:
```cpp
#ifdef UE4SS_HEADLESS
namespace RC::GUI {
    enum class GfxBackend { DX11, GLFW3_OpenGL3 };
    enum class RenderMode { ExternalThread, EngineTick, GameViewportClientTick };
}
#endif
```
Add this to `GUI/GUI.hpp` BEFORE the `#ifndef UE4SS_HEADLESS` guard, or to a separate `GUI/GUIStubs.hpp`.

### CRITICAL: UE4SSProgram.cpp - unguarded GUI usage (many locations)

**Lines needing `#ifndef UE4SS_HEADLESS`:**
- 968-999: `gui_render_thread_tick()` entire function
- 1009-1013: `on_program_start()` GUI hook registrations
- 1042-1046: keydown handler GUI rendering
- 2258-2260: `stop_render_thread()` 
- 2269-2279: `add_gui_tab()` / `remove_gui_tab()`

### CRITICAL: SettingsManager.cpp - unguarded GUI enum usage

**Lines 144-162**: Uses `GUI::GfxBackend::DX11`, `GUI::RenderMode::ExternalThread` etc. in the settings deserializer.

**Fix**: If we provide stub enums, this will compile fine. The parsed values just won't be used at runtime.

### HIGH: UEHeaderGenerator.cpp - Windows API in determine_primary_game_module_name()

**Lines 4173-4175**: Uses `HMODULE`, `GetModuleHandleW`, `GetModuleFileNameW`, `ARRAYSIZE`.

**Fix**: Wrap with `#if PLATFORM_WINDOWS` and provide Linux fallback using `/proc/self/exe`:
```cpp
#if PLATFORM_WINDOWS
    HMODULE primary_executable_module = GetModuleHandleW(NULL);
    ...
#else
    char exe_buf[1024];
    ssize_t len = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf)-1);
    exe_buf[len > 0 ? len : 0] = '\0';
    FFilePath root_executable_path(ensure_str(std::string(exe_buf)));
    StringType filename = ensure_str(root_executable_path.filename().replace_extension());
#endif
```

### HIGH: LuaMod.cpp - GUI::Dumpers linker errors

**Lines 1863, 1868**: Calls `GUI::Dumpers::call_generate_static_mesh_file()` and `GUI::Dumpers::call_generate_all_actor_file()`.

**Fix**: Wrap with `#ifndef UE4SS_HEADLESS`. These Lua functions (`DumpStaticMeshes`, `DumpAllActors`) will just not be available in headless mode.

### MEDIUM: CppUserModBase.cpp - unguarded GUI references

**Lines 19-28**: Destructor iterates `GUITabs` (which doesn't exist in headless).
**Lines 56-59**: `register_tab()` function uses `GUI::GUITab`.

**Fix**: Both sections need `#ifndef UE4SS_HEADLESS` guards.

### LOW: UE4SSProgram.hpp - friend declarations for HookedLoadLibrary*

**Lines 374-377**: Friend declarations for Windows-only functions. Won't cause compile error (just declares friendship with non-existent functions) but will cause linker warnings.

**Fix**: Wrap with `#if PLATFORM_WINDOWS`.

---

## Dep Libraries That Need Compilation

These `.cpp` files in `deps/first/` will also need to compile. Known issues:

| Library | Files | Status |
|---------|-------|--------|
| Helpers | SysError.cpp, Casting.cpp, Time.cpp, Debug.cpp | DONE - all compile |
| ArgsParser | main.cpp | DONE - compiles |
| File | WinFile.cpp | Already `#ifdef _WIN32` guarded |
| DynamicOutput | OutputDevice.cpp, DebugConsoleDevice.cpp | Already `#if _WIN32` guarded |
| Unreal/src | UnrealInitializer.cpp, UObject.cpp, UnrealVersion.cpp | NEED `#ifdef _WIN32` guards on `Windows.h` includes. Has `K32GetModuleInformation`, `EnumProcessModules` - need Linux scanner wired in |
| IniParser | Likely cross-platform | Unknown |
| JSON | Likely cross-platform | Unknown |
| LuaMadeSimple/LuaRaw | Lua is cross-platform | Should be fine |
| Input | PlatformInit.cpp fixed, Win32 source guarded | DONE |
| SinglePassSigScanner | Windows version guarded, Linux version exists | DONE |
| MProgram | ErrorObject.hpp has strncpy_s (covered by LinuxCompat) | Should be fine |

---

## Build Command
```bash
source ~/.xmake/profile
cd /tmp && git clone -b feat/linux-headless-port --recurse-submodules git@github.com:quietarcade/RE-UE4SS.git
cd RE-UE4SS
xmake f -p linux -a x86_64 -m "Game__Shipping__Linux" --yes
xmake build -j2 2>&1 | tee /tmp/xmake-build.log
```

## Next Session Plan
1. Apply all fixes from the audit above in ONE batch (estimated 30 min)
2. Do a single test compile
3. Fix any remaining issues from the compile (estimated 1-2 rounds)
4. Begin linking phase
5. Wire up `UnrealInitializer.cpp` Linux module scanner

## Estimated Remaining Work
- Apply audit fixes (1 hour)
- Unreal/src .cpp file compilation (1-2 hours)
- Remaining compile errors (1 hour)
- Linking (30 min)
- Runtime testing on Palworld (1-2 hours)

Total: ~4-6 more hours to get a working .so
