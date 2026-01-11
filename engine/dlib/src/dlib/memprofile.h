// Copyright 2020-2026 The Defold Foundation
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

#ifndef DM_MEMPROFILE_H
#define DM_MEMPROFILE_H

#include "atomic.h"

namespace dmMemProfile
{
    /**
     * Memory statistics
     */
    struct Stats
    {
        /// Total memory allocated
        int32_atomic_t m_TotalAllocated;

        /// Total active memory
        int32_atomic_t m_TotalActive;

        /// Total number of allocations
        int32_atomic_t m_AllocationCount;
    };

    /**
     * Initialize memory profiler
     */
    void Initialize();

    /**
     * Finalize memory profiler
     */
    void Finalize();

    /**
     * Check if memory profiling enabled
     * @return True if enabled
     */
    bool IsEnabled();

    /**
     * Get memory allocation statistics
     * @param stats Pointer to memory stats struct
     */
    void GetStats(Stats* stats);
}

#endif // DM_MEMPROFILE_H
