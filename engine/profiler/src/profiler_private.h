#ifndef DM_PROFILER_PRIVATE_H
#define DM_PROFILER_PRIVATE_H

#include <script/script.h>

#define SAMPLE_CPU_INTERVAL 0.25

namespace dmProfilerExt {
    /**
     * Call to sample CPU usage in intevals. (Needed for Linux, Android and Win32 platforms.)
     */
    void SampleCpuUsage();

    /**
     * Get current memory usage in bytes (resident/working set) for the process, as reported by OS.
     */
    uint64_t GetMemoryUsage();

    /**
     * Get current CPU usage for process, as reported by OS.
     */
    double GetCpuUsage();
}

#endif // #ifndef DM_PROFILER_PRIVATE_H
