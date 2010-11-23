#ifndef DM_MEMPROFILE_H
#define DM_MEMPROFILE_H

#include "atomic.h"

struct dmMemProfileParams;

typedef void (*dmMemProfileBeginFrame)();
typedef void (*dmInitMemProfile)(dmMemProfileParams* params);

struct dmMemProfileParams
{
    dmMemProfileBeginFrame m_BeginFrame;
};

namespace dmMemProfile
{
    /**
     * Memory statistics
     */
    struct Stats
    {
        /// Total memory allocated
        uint32_atomic_t m_TotalAllocated;

        /// Total active memory
        uint32_atomic_t m_TotalActive;

        /// Total number of allocations
        uint32_atomic_t m_AllocationCount;
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
     * If memory profiling enabled
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
