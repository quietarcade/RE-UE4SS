/**
 * Linux crash handler - replaces Windows MiniDumpWriteDump.
 * Uses signal handlers to catch crashes and log useful info.
 */

#ifdef PLATFORM_LINUX

#include <CrashDumper.hpp>
#include <UE4SSProgram.hpp>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <execinfo.h>
#include <unistd.h>

namespace RC
{
    static void crash_signal_handler(int signum)
    {
        const char* signal_name = "UNKNOWN";
        switch (signum)
        {
        case SIGSEGV: signal_name = "SIGSEGV (Segmentation Fault)"; break;
        case SIGABRT: signal_name = "SIGABRT (Abort)"; break;
        case SIGFPE:  signal_name = "SIGFPE (Floating Point Exception)"; break;
        case SIGILL:  signal_name = "SIGILL (Illegal Instruction)"; break;
        case SIGBUS:  signal_name = "SIGBUS (Bus Error)"; break;
        }

        fprintf(stderr, "\n[UE4SS-Linux] CRASH: %s (signal %d)\n", signal_name, signum);
        fprintf(stderr, "[UE4SS-Linux] Backtrace:\n");

        void* frames[64];
        int frame_count = backtrace(frames, 64);
        backtrace_symbols_fd(frames, frame_count, STDERR_FILENO);

        fprintf(stderr, "\n[UE4SS-Linux] End of backtrace. Core dump may be available.\n");

        // Re-raise with default handler to generate core dump
        signal(signum, SIG_DFL);
        raise(signum);
    }

    CrashDumper::CrashDumper()
    {
    }

    CrashDumper::~CrashDumper()
    {
        if (enabled)
        {
            signal(SIGSEGV, SIG_DFL);
            signal(SIGABRT, SIG_DFL);
            signal(SIGFPE, SIG_DFL);
            signal(SIGILL, SIG_DFL);
            signal(SIGBUS, SIG_DFL);
        }
    }

    void CrashDumper::enable()
    {
        signal(SIGSEGV, crash_signal_handler);
        signal(SIGABRT, crash_signal_handler);
        signal(SIGFPE, crash_signal_handler);
        signal(SIGILL, crash_signal_handler);
        signal(SIGBUS, crash_signal_handler);
        enabled = true;
    }

    void CrashDumper::set_full_memory_dump(bool enabled_param)
    {
        // On Linux, core dump behavior is controlled by ulimit -c and /proc/sys/kernel/core_pattern
        // We don't need to do anything here
        (void)enabled_param;
    }

} // namespace RC

#endif // PLATFORM_LINUX
