#include <Helpers/Casting.hpp>

#ifdef _WIN32
#define WINDOWS
#define NOMINMAX
#include "Windows.h"
#endif

namespace RC::Helper::Casting
{

#ifdef WINDOWS
    auto check_readable(void* handle, void* src_ptr) -> bool
    {
        uintptr_t is_valid_ptr_buffer;
        size_t bytes_read;

        return ReadProcessMemory(*reinterpret_cast<HANDLE*>(handle), src_ptr, &is_valid_ptr_buffer, 0x8, &bytes_read) != 0;
    }
#endif

} // namespace RC::Helper::Casting

#ifdef __linux__
#include <unistd.h>
#include <fcntl.h>

namespace RC::Helper::Casting
{
    auto check_readable(void* handle, void* src_ptr) -> bool
    {
        // Check if memory is readable by attempting to read via /proc/self/mem
        // A simpler approach: try to read from the pipe trick
        int fd[2];
        if (pipe(fd) != 0) return false;
        bool readable = (write(fd[1], src_ptr, sizeof(void*)) == sizeof(void*));
        close(fd[0]);
        close(fd[1]);
        return readable;
    }
}
#endif
