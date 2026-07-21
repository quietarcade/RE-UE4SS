# UE4SS Linux Port - Status & Next Steps

## Branch
- **RE-UE4SS**: `feat/linux-headless-port` on `quietarcade/RE-UE4SS`
- **UEPseudo**: `feat/linux-hooks` on `quietarcade/UEPseudo`

## Goal
Build `libUE4SS.so` that can be loaded via `LD_PRELOAD` into a native Linux Palworld dedicated server to enable Lua mod support.

## Current Status: LUA MODS LOADING AND EXECUTING

`libUE4SS.so` (15MB stripped) successfully loads into Palworld, initializes, discovers mods, and executes Lua scripts. Both the TestMod and BetterBaseRange mod load and run their initialization code. UE4SS API stubs allow mods to call `NotifyOnNewObject`, `RegisterHook`, etc. without crashing (they just don't have actual UE engine interaction yet).

### What works end-to-end:
- .so loads via LD_PRELOAD alongside the game server
- UE4SS initializes (paths, logging, file I/O, console output)
- Mods discovered from `Mods/` directory via `mods.txt`
- Lua states created per mod with standard libraries
- `require()` resolves modules from mod's scripts directory
- `main.lua` scripts execute successfully
- Game server runs concurrently without interference
- BetterBaseRange mod loads config, registers hooks (stubs), prints loaded message

### What doesn't work yet:
- UE4SS API functions are stubs (NotifyOnNewObject, RegisterHook, FindObject, etc.)
- No actual UE engine interaction (no sig scanning, no hooking)
- Mods that depend on UE objects/properties won't function at runtime

---

## Architecture

### Loading Flow
1. `LD_PRELOAD=libUE4SS.so` - .so loaded before main()
2. `__attribute__((constructor))` fires, starts a 3-second delayed init thread
3. After delay (ensures static initialization is complete):
   - `UE4SSProgram` constructor: setup_paths, setup_mods (creates LuaMod objects)
   - `init()`: skips UE scanning, installs + executes Lua mods in basic mode
4. Each mod gets: Lua state, open_all_libs, package.path configured, main.lua executed
5. Init thread enters idle loop (mods are loaded, game runs normally)

### Key Design Decisions
- **3-second delayed init**: Required to avoid static initialization order issues (SIGFPE from uninitialized std::unordered_map in LuaMadeSimple)
- **Skip UE scanning on Linux**: `ps_scan` returns false, `setup_unreal()` skipped entirely
- **Basic Lua mode**: Mods execute main.lua directly via `luaL_dofile` instead of through UE4SS's full `prepare_mod`/`start_mod` flow (which requires UE type registration)
- **Stub API functions**: Key UE4SS globals registered as Lua functions that log/no-op
- **Skip C++ mods**: `start_cpp_mods()` disabled on Linux (Lua-only focus)

---

## Build System
- xmake, platform `linux`, arch `x86_64`, mode `Game__Shipping__Linux`
- Global flags: `-fpermissive -fno-char8_t -fPIC -Wno-error`
- Force-include: `LinuxCompat.hpp` (maps MSVC functions to POSIX)
- Global defines: `PLATFORM_LINUX PLATFORM_UNIX UE4SS_HEADLESS`
- Excluded on Linux: UVTD, cppmods, glad, patternsleuth

## Build Command
```bash
source ~/.xmake/profile
cd /tmp/RE-UE4SS
xmake f -p linux -a x86_64 -m "Game__Shipping__Linux" -c --ccache=n --yes
xmake build -j1
strip --strip-debug Binaries/Game__Shipping__Linux/UE4SS/libUE4SS.so
```

Full rebuild: ~10 min with `-j1`. Incremental: ~30s.

## Deploy & Test
```bash
cp Binaries/Game__Shipping__Linux/UE4SS/libUE4SS.so /srv/games/palworld-modded/
cd /srv/games/palworld-modded
LD_PRELOAD=/srv/games/palworld-modded/libUE4SS.so ./Pal/Binaries/Linux/PalServer-Linux-Shipping Pal -log
```

---

## Files Changed (Key)

### UE4SS Core
- `UE4SS/src/main_ue4ss_linux.cpp` - LD_PRELOAD entry point, delayed init thread, crash handler
- `UE4SS/src/UE4SSProgram.cpp` - Linux init path (skip UE scanning, basic Lua mode, stub APIs)
- `UE4SS/src/Mod/CppModLinux.cpp` - dlopen/dlsym mod loader + all virtual method implementations
- `UE4SS/include/FilesystemWatcher_Linux.cpp_impl` - no-op filesystem watcher stubs

### Dependencies
- `deps/first/File/src/FileType/LinuxFile.cpp` - POSIX file operations (fopen/fwrite/fread)
- `deps/first/LuaRaw/src/luauser.c` - no-op lock functions (pthread not needed for single-threaded)
- `deps/first/Helpers/src/Casting.cpp` - Linux `check_readable` via pipe trick
- `deps/first/SinglePassSigScanner/src/SinglePassSigScannerLinux.cpp` - static member defs + stub scanner
- `deps/first/Unreal/src/UnrealInitializer.cpp` - `ps_scan` stub returning false
- `deps/first/JSON/include/JSON/Number.hpp` - `long long` overloads for LP64
- `deps/first/Unreal/include/Unreal/Common.hpp` - `#ifndef` guards for FORCEINLINE/FORCENOINLINE

---

## Next Steps: Implementing UE Interaction

To make mods like BetterBaseRange actually functional (modifying game properties at runtime):

### Vtable Discovery (completed this session)

We found that PalServer-Linux-Shipping has **dynamic symbol exports** for UE vtables despite being stripped. Key findings:

```
_ZTV7UObject  at vaddr 0x1a5c980  (712 bytes, 87 vfuncs)
_ZTV11UObjectBase at vaddr 0x1a5c948
_ZTV6UWorld   at vaddr 0x22507f0
_ZTV6UClass   (also exported)
_ZTV11UGameEngine at vaddr 0x20e6558
```

**UObject vtable analysis** (vptr = vtable + 16 = `0x1a5c990`):
- Top candidates for `ProcessEvent`: **vfunc[62] at `0x7aedbb0`** (13696 bytes, prologue `push rbp; push r15; push r14` - complex dispatch function)
- Trivial stubs (just `ret`): vfunc[63], vfunc[64]
- Virtual dispatch thunks: vfunc[67] (`mov rax,[rdi]; jmp [rax+0x210]`)
- The 0x440ce** range functions are small shims (likely default base class overrides)

**Binary characteristics:**
- Type: EXEC (not PIE), so addresses are absolute
- Stripped (no debug symbols via `nm`)
- Has RTTI strings (`_ZTV7UObject`, `_ZTI7UObject` in `.dynstr`)
- `.text` section: `0x43b3000` to `0xbc8d330` (120MB)
- `.data.rel.ro`: contains vtable data at `0xbc8e330`
- 58786 dynamic symbols (mostly ICU, OpenSSL, libstdc++)
- No UE game functions exported (all static-linked)
- `GUObjectArray`, `ProcessEvent`, `FName::ToString` NOT in dynsym

**What this means for the implementation:**
- Option B (hardcoded offsets from exports) is PARTIALLY viable - we have vtable addresses
- We can hook `ProcessEvent` by patching the vtable entry (no inline hook needed!)
- vtable hooking: `mprotect` the vtable page, replace vfunc[62] with our trampoline
- Still need to find `GUObjectArray` and `FName::ToString` for full UE4SS support
- Pattern scanning still needed for non-vtable globals

### Implementation Plan (revised)

**Phase 1: Vtable-based ProcessEvent hook** (2-3 hours)
1. At runtime, resolve `_ZTV7UObject` via `dlsym` on the main executable handle
2. Calculate vfunc[62] address (vtable + 16 + 62*8)
3. `mprotect` the vtable page to RW
4. Replace the function pointer with our hook function
5. Our hook: check if the called UFunction matches registered callbacks, then call original
6. This gives us `RegisterHook` and `NotifyOnNewObject` (via hooking PostInitProperties)

**Phase 2: GUObjectArray discovery** (2-3 hours)
1. Scan `.text` for patterns that reference GUObjectArray (e.g., `FUObjectArray::AllocateUObjectIndex`)
2. Or: at runtime, find any UObject instance (from ProcessEvent hook), read its index, trace back to the array
3. Once found, enables `FindObject`, `FindFirstOf`, `StaticFindObject`

**Phase 3: Property access** (3-4 hours)
1. With GUObjectArray + ProcessEvent, can iterate objects and read UProperty metadata
2. Implement Lua object wrappers that allow `base_model.AreaRange = value`
3. This makes BetterBaseRange fully functional

### Option C: Config File Modification (no hooking needed)
- `bBuildAreaLimit=False` is already set (build area unlimited)
- But `AreaRange` (Pal work range) is NOT a server config option - requires UE property modification
- Option C is not viable for this specific mod

---

## Known Warnings (non-blocking)
- FORCEINLINE/FORCENOINLINE redefinition between Common.hpp and LinuxPlatform.hpp
- `friend declaration ... declares a non-template function` in HandleTemplate.hpp
- Deprecated enum-enum-conversion in UnrealType.hpp
- UE4SS loads into subprocess too (crashpad_handler) - harmless, just extra log lines
