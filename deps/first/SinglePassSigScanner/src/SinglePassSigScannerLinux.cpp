/**
 * Linux implementation of memory scanning utilities.
 * Replaces Windows VirtualQuery / MODULEINFO / Psapi with /proc/self/maps and dl_iterate_phdr.
 */

#ifdef PLATFORM_LINUX

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <link.h>
#include <string>
#include <sys/mman.h>
#include <vector>

#include <SigScanner/SinglePassSigScanner.hpp>

namespace RC
{

/**
 * Represents a memory region parsed from /proc/self/maps
 */
struct MemoryRegion
{
    uintptr_t start;
    uintptr_t end;
    bool readable;
    bool writable;
    bool executable;
    std::string path;
};

/**
 * Parse /proc/self/maps into a vector of memory regions.
 */
static auto parse_proc_maps() -> std::vector<MemoryRegion>
{
    std::vector<MemoryRegion> regions;
    FILE* maps = fopen("/proc/self/maps", "r");
    if (!maps) return regions;

    char line[1024];
    while (fgets(line, sizeof(line), maps))
    {
        uintptr_t start, end;
        char perms[5];
        unsigned long offset;
        unsigned int dev_major, dev_minor;
        unsigned long inode;
        char path[512] = {0};

        int fields = sscanf(line, "%lx-%lx %4s %lx %x:%x %lu %511[^\n]",
                            &start, &end, perms, &offset, &dev_major, &dev_minor, &inode, path);

        if (fields >= 7)
        {
            MemoryRegion region;
            region.start = start;
            region.end = end;
            region.readable = (perms[0] == 'r');
            region.writable = (perms[1] == 'w');
            region.executable = (perms[2] == 'x');
            region.path = (fields >= 8) ? path : "";

            // Trim leading whitespace from path
            size_t path_start = region.path.find_first_not_of(' ');
            if (path_start != std::string::npos)
                region.path = region.path.substr(path_start);

            regions.push_back(region);
        }
    }
    fclose(maps);
    return regions;
}

/**
 * Linux equivalent of GetModuleFileName + GetModuleInformation.
 * Finds the base address and size of a loaded module by name.
 */
struct LinuxModuleInfo
{
    void* base_address;
    size_t size;
    std::string path;
};

static auto get_module_info(const char* module_name) -> LinuxModuleInfo
{
    LinuxModuleInfo info{nullptr, 0, ""};
    auto regions = parse_proc_maps();

    uintptr_t lowest_addr = UINTPTR_MAX;
    uintptr_t highest_addr = 0;

    for (const auto& region : regions)
    {
        if (region.path.find(module_name) != std::string::npos)
        {
            if (region.start < lowest_addr) lowest_addr = region.start;
            if (region.end > highest_addr) highest_addr = region.end;
            if (info.path.empty()) info.path = region.path;
        }
    }

    if (lowest_addr != UINTPTR_MAX)
    {
        info.base_address = reinterpret_cast<void*>(lowest_addr);
        info.size = highest_addr - lowest_addr;
    }

    return info;
}

/**
 * Linux equivalent of GetModuleHandle(NULL) - gets the main executable's base address.
 */
static auto get_main_module_info() -> LinuxModuleInfo
{
    LinuxModuleInfo info{nullptr, 0, ""};
    auto regions = parse_proc_maps();

    if (regions.empty()) return info;

    // The first executable region is typically the main binary
    // More reliably, read /proc/self/exe
    char exe_path[512];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len <= 0) return info;
    exe_path[len] = '\0';

    uintptr_t lowest_addr = UINTPTR_MAX;
    uintptr_t highest_addr = 0;

    for (const auto& region : regions)
    {
        if (region.path == exe_path)
        {
            if (region.start < lowest_addr) lowest_addr = region.start;
            if (region.end > highest_addr) highest_addr = region.end;
            if (info.path.empty()) info.path = region.path;
        }
    }

    if (lowest_addr != UINTPTR_MAX)
    {
        info.base_address = reinterpret_cast<void*>(lowest_addr);
        info.size = highest_addr - lowest_addr;
    }

    return info;
}

/**
 * Check if a memory address is readable (equivalent of VirtualQuery + MEM_COMMIT + !PAGE_NOACCESS)
 */
static auto is_memory_readable(void* addr, size_t size) -> bool
{
    auto regions = parse_proc_maps();
    uintptr_t target = reinterpret_cast<uintptr_t>(addr);

    for (const auto& region : regions)
    {
        if (target >= region.start && (target + size) <= region.end)
        {
            return region.readable;
        }
    }
    return false;
}

/**
 * Callback for dl_iterate_phdr - collects all loaded shared libraries.
 */
struct LoadedModule
{
    std::string name;
    void* base_address;
    size_t size;
};

static int dl_iterate_callback(struct dl_phdr_info* info, size_t size, void* data)
{
    auto* modules = static_cast<std::vector<LoadedModule>*>(data);

    LoadedModule mod;
    mod.name = info->dlpi_name ? info->dlpi_name : "";
    mod.base_address = reinterpret_cast<void*>(info->dlpi_addr);

    // Calculate total size from program headers
    size_t total_size = 0;
    for (int i = 0; i < info->dlpi_phnum; i++)
    {
        if (info->dlpi_phdr[i].p_type == PT_LOAD)
        {
            size_t segment_end = info->dlpi_phdr[i].p_vaddr + info->dlpi_phdr[i].p_memsz;
            if (segment_end > total_size) total_size = segment_end;
        }
    }
    mod.size = total_size;

    modules->push_back(mod);
    return 0;
}

/**
 * Get all loaded modules (equivalent of EnumProcessModules + GetModuleInformation)
 */
static auto get_all_loaded_modules() -> std::vector<LoadedModule>
{
    std::vector<LoadedModule> modules;
    dl_iterate_phdr(dl_iterate_callback, &modules);
    return modules;
}

} // namespace RC

#endif // PLATFORM_LINUX
