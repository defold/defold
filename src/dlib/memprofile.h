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
    struct Stats
    {
        uint32_atomic_t m_TotalAllocated;
        uint32_atomic_t m_TotalActive;
        uint32_atomic_t m_AllocationCount;
    };

    bool IsEnabled();

    void GetStats(Stats* stats);
}

#endif // DM_MEMPROFILE_H
