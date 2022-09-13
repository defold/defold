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

#ifndef DM_PROFILER_PROC_PRIVATE_H
#define DM_PROFILER_PROC_PRIVATE_H

#include <script/script.h>

namespace dmProfilerExt {
    /**
     * Call to sample CPU usage from proc in intevals.
     */
    void SampleProcCpuUsage();

    /**
     * Get current memory usage in bytes from proc (resident/working set) for the process, as reported by OS.
     */
    uint64_t GetProcMemoryUsage();

    /**
     * Get current CPU usage for process from proc, as reported by OS.
     */
    double GetProcCpuUsage();
}

#endif // #ifndef DM_PROFILER_PROC_PRIVATE_H
