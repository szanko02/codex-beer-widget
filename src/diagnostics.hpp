#pragma once
#include "codex_source.hpp"
#include <psapi.h>

namespace beer {
inline Json process_resources() {
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory),
                              sizeof(memory)))
        throw std::runtime_error("Cannot measure process memory");
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        throw std::runtime_error("Cannot measure process CPU");
    const auto seconds = [](FILETIME time) {
        ULARGE_INTEGER value{};
        value.LowPart = time.dwLowDateTime;
        value.HighPart = time.dwHighDateTime;
        return static_cast<double>(value.QuadPart) / 10000000.;
    };
    return {{"workingSetMiB", static_cast<double>(memory.WorkingSetSize) / 1048576.},
            {"privateMiB", static_cast<double>(memory.PrivateUsage) / 1048576.},
            {"peakWorkingSetMiB", static_cast<double>(memory.PeakWorkingSetSize) / 1048576.},
            {"cpuSeconds", seconds(kernel) + seconds(user)}};
}
} // namespace beer
