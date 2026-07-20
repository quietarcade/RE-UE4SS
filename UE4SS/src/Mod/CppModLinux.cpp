/**
 * Linux implementation of C++ mod loading.
 * Replaces LoadLibrary/GetProcAddress with dlopen/dlsym.
 */

#ifdef PLATFORM_LINUX

#include <dlfcn.h>
#include <filesystem>

#include <DynamicOutput/DynamicOutput.hpp>
#include <Helpers/String.hpp>
#include <LuaMadeSimple/LuaMadeSimple.hpp>
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

        auto so_path = m_dlls_path / STR("main.so");
        if (!std::filesystem::exists(so_path))
        {
            so_path = m_dlls_path / fmt::format(STR("{}.so"), m_mod_name);

            if (!std::filesystem::exists(so_path))
            {
                Output::send<LogLevel::Warning>(STR("Failed to load C++ mod {}, dlls folder must contain either main.so or {}\n"),
                                                m_mod_name, ensure_str(so_path.filename()));
                set_installable(false);
                return;
            }
        }

        m_dll_filename = ensure_str(so_path.filename());

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

    auto CppMod::start_mod() -> void
    {
        try
        {
            m_mod = m_start_mod_func();
            m_is_started = m_mod != nullptr;
        }
        catch (std::exception& e)
        {
            if (!Output::has_internal_error())
            {
                Output::send<LogLevel::Warning>(STR("Failed to load shared library <{}> for mod {}, because: {}\n"),
                                                ensure_str((m_dlls_path / m_dll_filename)),
                                                m_mod_name,
                                                ensure_str(e.what()));
            }
            else
            {
                printf_s("Internal Error: %s\n", e.what());
            }
        }
    }

    auto CppMod::uninstall() -> void
    {
        Output::send(STR("Stopping C++ mod '{}' for uninstall\n"), m_mod_name);
        if (m_mod && m_uninstall_mod_func)
        {
            m_uninstall_mod_func(m_mod);
        }
    }

    auto CppMod::fire_on_lua_start(StringViewType mod_name,
                                   LuaMadeSimple::Lua& lua,
                                   LuaMadeSimple::Lua& main_lua,
                                   LuaMadeSimple::Lua& async_lua,
                                   LuaMadeSimple::Lua* hook_lua) -> void
    {
        if (m_mod)
        {
            m_mod->on_lua_start(mod_name, lua, main_lua, async_lua, hook_lua);
            std::vector<LuaMadeSimple::Lua*> hook_luas;
            if (hook_lua) { hook_luas.push_back(hook_lua); }
            m_mod->on_lua_start(mod_name, lua, main_lua, async_lua, hook_luas);
        }
    }

    auto CppMod::fire_on_lua_start(LuaMadeSimple::Lua& lua,
                                   LuaMadeSimple::Lua& main_lua,
                                   LuaMadeSimple::Lua& async_lua,
                                   LuaMadeSimple::Lua* hook_lua) -> void
    {
        if (m_mod)
        {
            m_mod->on_lua_start(lua, main_lua, async_lua, hook_lua);
            std::vector<LuaMadeSimple::Lua*> hook_luas;
            if (hook_lua) { hook_luas.push_back(hook_lua); }
            m_mod->on_lua_start(lua, main_lua, async_lua, hook_luas);
        }
    }

    auto CppMod::fire_on_lua_stop(StringViewType mod_name,
                                  LuaMadeSimple::Lua& lua,
                                  LuaMadeSimple::Lua& main_lua,
                                  LuaMadeSimple::Lua& async_lua,
                                  LuaMadeSimple::Lua* hook_lua) -> void
    {
        if (m_mod)
        {
            m_mod->on_lua_stop(mod_name, lua, main_lua, async_lua, hook_lua);
            std::vector<LuaMadeSimple::Lua*> hook_luas;
            if (hook_lua) { hook_luas.push_back(hook_lua); }
            m_mod->on_lua_stop(mod_name, lua, main_lua, async_lua, hook_luas);
        }
    }

    auto CppMod::fire_on_lua_stop(LuaMadeSimple::Lua& lua, LuaMadeSimple::Lua& main_lua, LuaMadeSimple::Lua& async_lua, LuaMadeSimple::Lua* hook_lua) -> void
    {
        if (m_mod)
        {
            m_mod->on_lua_stop(lua, main_lua, async_lua, hook_lua);
            std::vector<LuaMadeSimple::Lua*> hook_luas;
            if (hook_lua) { hook_luas.push_back(hook_lua); }
            m_mod->on_lua_stop(lua, main_lua, async_lua, hook_luas);
        }
    }

    auto CppMod::fire_unreal_init() -> void
    {
        if (m_mod) { m_mod->on_unreal_init(); }
    }

    auto CppMod::fire_ui_init() -> void
    {
        if (m_mod) { m_mod->on_ui_init(); }
    }

    auto CppMod::fire_program_start() -> void
    {
        if (m_mod) { m_mod->on_program_start(); }
    }

    auto CppMod::fire_update() -> void
    {
        if (m_mod) { m_mod->on_update(); }
    }

    auto CppMod::fire_dll_load(StringViewType dll_name) -> void
    {
        if (m_mod) { m_mod->on_dll_load(dll_name); }
    }

    auto CppMod::fire_on_cpp_mods_loaded() -> void
    {
        if (m_mod) { m_mod->on_cpp_mods_loaded(); }
    }

} // namespace RC

#endif // PLATFORM_LINUX
