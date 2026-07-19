#include <Input/PlatformInputSource.hpp>

#if PLATFORM_WINDOWS
#include <Input/Platform/Win32AsyncInputSource.hpp>
#endif

#ifndef UE4SS_HEADLESS
#include <Input/Platform/GLFW3InputSource.hpp>
#endif

namespace RC::Input
{
    auto Handler::init() -> void
    {
#if PLATFORM_WINDOWS
        register_input_source(std::make_shared<Win32AsyncInputSource>(L"ConsoleWindowClass", L"UnrealWindow"));
#endif

#ifndef UE4SS_HEADLESS
        register_input_source(std::make_shared<GLFW3InputSource>());
#endif
        // On headless Linux, no input sources are registered (server doesn't need keyboard input)
    }
} // namespace RC::Input
