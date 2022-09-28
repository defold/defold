// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

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

    /**
     * Call update in platforms implementations to collect platform specific data.
     */
    void UpdatePlatformProfiler();
}

#endif // #ifndef DM_PROFILER_PRIVATE_H
