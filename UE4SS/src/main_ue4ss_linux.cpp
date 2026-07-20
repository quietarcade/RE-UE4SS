/**
 * UE4SS Linux Entry Point
 *
 * Replaces DllMain/proxy injection with LD_PRELOAD + __attribute__((constructor)).
 * When this shared library is loaded via LD_PRELOAD, the constructor runs before
 * the game's main() function, giving us a chance to initialize UE4SS.
 */

#ifdef PLATFORM_LINUX

#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <link.h>
#include <thread>

#include "UE4SSProgram.hpp"
#include <fenv.h>
#include <DynamicOutput/DynamicOutput.hpp>
#include <Helpers/String.hpp>

using namespace RC;

namespace fs = std::filesystem;

static UE4SSProgram* s_program = nullptr;

/**
 * Determine the path of this shared library (libUE4SS.so).
 * This is the equivalent of GetModuleFileName(hModule, ...) on Windows.
 */
static fs::path get_library_path()
{
    Dl_info info;
    if (dladdr(reinterpret_cast<void*>(&get_library_path), &info) && info.dli_fname)
    {
        return fs::canonical(info.dli_fname);
    }

    // Fallback: check /proc/self/maps for libUE4SS.so
    FILE* maps = fopen("/proc/self/maps", "r");
    if (!maps) return {};

    char line[512];
    fs::path result;
    while (fgets(line, sizeof(line), maps))
    {
        if (strstr(line, "libUE4SS.so"))
        {
            // Format: address perms offset dev inode pathname
            char* path_start = strchr(line, '/');
            if (path_start)
            {
                // Remove trailing newline
                char* nl = strchr(path_start, '\n');
                if (nl) *nl = '\0';
                result = path_start;
                break;
            }
        }
    }
    fclose(maps);
    return result;
}

/**
 * UE4SS initialization thread.
 * Runs in a separate thread to avoid blocking the game's startup.
 */
static void ue4ss_init_thread()
{
    fs::path lib_path = get_library_path();
    if (lib_path.empty())
    {
        fprintf(stderr, "[UE4SS-Linux] ERROR: Could not determine library path\n");
        return;
    }

    fprintf(stderr, "[UE4SS-Linux] Library path: %s\n", lib_path.c_str());

    try {
        fedisableexcept(FE_ALL_EXCEPT);
        fprintf(stderr, "[UE4SS-Linux] About to construct UE4SSProgram...\n");
        s_program = new UE4SSProgram(lib_path, {});
        fprintf(stderr, "[UE4SS-Linux] Created UE4SSProgram, calling init()...\n");
        s_program->init();
        fprintf(stderr, "[UE4SS-Linux] init() completed successfully\n");
    } catch (const std::exception& e) {
        fprintf(stderr, "[UE4SS-Linux] EXCEPTION during init: %s\n", e.what());
        return;
    } catch (...) {
        fprintf(stderr, "[UE4SS-Linux] UNKNOWN EXCEPTION during init\n");
        return;
    }




    fprintf(stderr, "[UE4SS-Linux] init() completed successfully\n");
    if (auto e = s_program->get_error_object(); e->has_error())
    {
        if (!Output::has_internal_error())
        {
            Output::send<LogLevel::Error>(STR("Fatal Error: {}\n"), ensure_str(e->get_message()));
        }
        else
        {
            fprintf(stderr, "[UE4SS-Linux] Fatal Error: %s\n", e->get_message());
        }
    }
}

/**
 * Constructor - called when the .so is loaded via LD_PRELOAD.
 * This runs before main() of the host process.
 */
__attribute__((constructor))
static void ue4ss_linux_entry()
{
    fprintf(stderr, "[UE4SS-Linux] Loaded via LD_PRELOAD, initializing...\n");

    // Launch init in a separate thread so we don't block the game's startup
    // Delay init to avoid static initialization order issues
    std::thread init_thread([]{
        std::this_thread::sleep_for(std::chrono::seconds(3));
        ue4ss_init_thread();
    });
    init_thread.detach();
}

/**
 * Destructor - called when the .so is unloaded (process exit).
 */
__attribute__((destructor))
static void ue4ss_linux_exit()
{
    if (s_program)
    {
        delete s_program;
        s_program = nullptr;
    }
}

#endif // PLATFORM_LINUX
