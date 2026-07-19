/**
 * Linux implementation of C++ mod loading.
 * Replaces LoadLibrary/GetProcAddress with dlopen/dlsym.
 */

#ifdef PLATFORM_LINUX

#include <dlfcn.h>
#include <filesystem>

#include <DynamicOutput/DynamicOutput.hpp>
#include <Helpers/String.hpp>
#include <Mod/CppMod.hpp>

namespace RC
{
    CppMod::CppMod(UE4SSProgram& program, StringType&& mod_name, StringType&& mod_path) : Mod(program, std::move(mod_name), std::move(mod_path))
    {
        m_dlls_path = m_mod_path / STR("dlls");

        if (!std::filesystem::exists(m_dlls_path))
        {
            Output::send<LogLevel::Warning>(STR("Could not find the dlls folder for mod {}\n"), m_mod_name);
            set_installable(false);
            return;
        }

        // On Linux, look for main.so instead of main.dll
        auto so_path = m_dlls_path / STR("main.so");
        if (!std::filesystem::exists(so_path))
        {
            so_path = m_dlls_path / fmt::format(STR("{}.so"), mod_name);

            if (!std::filesystem::exists(so_path))
            {
                Output::send<LogLevel::Warning>(STR("Failed to load C++ mod {}, dlls folder must contain either main.so or {}\n"),
                                                m_mod_name, ensure_str(so_path.filename()));
                set_installable(false);
                return;
            }
        }

        m_dll_filename = ensure_str(so_path.filename());

        // Load the shared library
        std::string path_str = so_path.string();
        m_main_dll_module = dlopen(path_str.c_str(), RTLD_NOW | RTLD_LOCAL);

        if (!m_main_dll_module)
        {
            Output::send<LogLevel::Warning>(STR("Failed to load shared library <{}> for mod {}, error: {}\n"),
                                            ensure_str(so_path), m_mod_name, ensure_str(dlerror()));
            set_installable(false);
            return;
        }

        m_start_mod_func = reinterpret_cast<start_type>(dlsym(m_main_dll_module, "start_mod"));
        m_uninstall_mod_func = reinterpret_cast<uninstall_type>(dlsym(m_main_dll_module, "uninstall_mod"));

        if (!m_start_mod_func || !m_uninstall_mod_func)
        {
            Output::send<LogLevel::Warning>(STR("Failed to find exported mod lifecycle functions for mod {}\n"), m_mod_name);

            dlclose(m_main_dll_module);
            m_main_dll_module = nullptr;

            set_installable(false);
            return;
        }
    }

    CppMod::~CppMod()
    {
        if (m_main_dll_module)
        {
            dlclose(m_main_dll_module);
            m_main_dll_module = nullptr;
        }
    }

} // namespace RC

#endif // PLATFORM_LINUX
