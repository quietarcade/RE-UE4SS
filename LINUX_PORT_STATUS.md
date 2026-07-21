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

### Option A: Full Sig Scanner Implementation (hard, complete)
1. Parse `/proc/self/maps` to find game binary's memory regions
2. Port patternsleuth's pattern matching to scan for UE function signatures
3. Find: GUObjectArray, FName::ToString, ProcessEvent, ProcessInternal, etc.
4. Hook ProcessEvent via mprotect-based inline patching (LinuxDetour.hpp exists)
5. Register real UE4SS Lua API functions (NotifyOnNewObject uses ProcessEvent hook)
6. Estimated: 8-15 hours of work

### Option B: Hardcoded Offsets (quick, fragile)
1. Use a tool like `objdump`/`nm`/`readelf` on PalServer-Linux-Shipping to find symbols
2. Linux UE5 server binaries often have debug symbols or exported names
3. If GUObjectArray etc. are exported, just read their addresses directly
4. Estimated: 2-4 hours, breaks on every game update

### Option C: Config File Modification (no hooking needed)
1. BetterBaseRange's goal is to increase AreaRange on base camps
2. If this is a config value, might be achievable by modifying game .ini files or DefaultPalWorldSettings
3. Check if Palworld exposes base camp range as a server config option
4. Estimated: 30 min if possible, but may not be

### Recommended: Start with Option B
Linux UE5 dedicated servers typically export many symbols. Check:
```bash
nm -D /srv/games/palworld-modded/Pal/Binaries/Linux/PalServer-Linux-Shipping | grep -i "GUObjectArray\|FName\|ProcessEvent"
```
If symbols are found, we can skip the sig scanner entirely and use direct addresses.

---

## Known Warnings (non-blocking)
- FORCEINLINE/FORCENOINLINE redefinition between Common.hpp and LinuxPlatform.hpp
- `friend declaration ... declares a non-template function` in HandleTemplate.hpp
- Deprecated enum-enum-conversion in UnrealType.hpp
- UE4SS loads into subprocess too (crashpad_handler) - harmless, just extra log lines
