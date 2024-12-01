// Copyright 2020-2024 The Defold Foundation
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

#include "profile.h"

#define TRACY_ENABLE
#include <tracy/tracy/Tracy.hpp>
#include <tracy/client/TracyProfiler.hpp>
#include <tracy/common/TracyAlign.hpp>
#include <tracy/common/TracyQueue.hpp>
#include <tracy/common/TracySystem.hpp>

#include <stdio.h> // vsnprintf
#include <stdarg.h> // va_start et al

#include "dlib/dlib.h"
#include "dlib/log.h"
#include "dlib/atomic.h"
#include "dlib/spinlock.h"

#include "dlib/hash.h"

// #include "dmsdk/external/remotery/Remotery.h"

namespace dmProfile
{
    //static Remotery*                g_Remotery = 0;
    static FSampleTreeCallback      g_SampleTreeCallback = 0;
    static void*                    g_SampleTreeCallbackCtx = 0;
    static FPropertyTreeCallback    g_PropertyTreeCallback = 0;
    static void*                    g_PropertyTreeCallbackCtx = 0;
    static int32_atomic_t           g_ProfilerInitialized = 0;
    static dmSpinlock::Spinlock     g_ProfilerLock;

    // static inline rmtSample* SampleFromHandle(HSample sample)
    // {
    //     return (rmtSample*)sample;
    // }
    // static inline HSample SampleToHandle(rmtSample* sample)
    // {
    //     return (HSample)sample;
    // }
    // static inline rmtProperty* PropertyFromHandle(HProperty property)
    // {
    //     return (rmtProperty*)property;
    // }
    // static inline HProperty PropertyToHandle(rmtProperty* property)
    // {
    //     return (HProperty)property;
    // }

    bool IsInitialized()
    {
        return dmAtomicGet32(&g_ProfilerInitialized) != 0;
    }

    // static void SampleTreeCallback(void* ctx, rmtSampleTree* sample_tree)
    // {
    //     if (!IsInitialized())
    //         return;

    //     DM_SPINLOCK_SCOPED_LOCK(g_ProfilerLock);

    //     if (g_SampleTreeCallback)
    //     {
    //         const char* thread_name = rmt_SampleTreeGetThreadName(sample_tree);
    //         rmtSample* root = rmt_SampleTreeGetRootSample(sample_tree);

    //         g_SampleTreeCallback(g_SampleTreeCallbackCtx, thread_name, SampleToHandle(root));
    //     }
    // }

    // static void PropertyTreeCallback(void* ctx, rmtProperty* root)
    // {
    //     if (!IsInitialized())
    //         return;

    //     DM_SPINLOCK_SCOPED_LOCK(g_ProfilerLock);

    //     if (g_PropertyTreeCallback)
    //     {
    //         g_PropertyTreeCallback(g_PropertyTreeCallbackCtx, PropertyToHandle(root));
    //     }
    // }

    void Initialize(const Options* options)
    {
        if (!dLib::IsDebugMode())
            return;

        // rmtSettings* settings = rmt_Settings();

        // if (options)
        // {
        //     if (options->m_Port != 0)
        //     {
        //         settings->port = (rmtU16)options->m_Port;
        //     }
        //     if (options->m_SleepBetweenServerUpdates > 0)
        //     {
        //         settings->msSleepBetweenServerUpdates = options->m_SleepBetweenServerUpdates;
        //     }
        // }
        // settings->sampletree_handler = SampleTreeCallback;
        // settings->sampletree_context = 0;
        // settings->snapshot_callback = PropertyTreeCallback;
        // settings->snapshot_context = 0;
        // settings->reuse_open_port = true;
        // settings->enableThreadSampler = false;

        // rmtError result = rmt_CreateGlobalInstance(&g_Remotery);
        // if (result != RMT_ERROR_NONE)
        // {
        //     dmLogError("Failed to initialize profile library %d", result);
        //     return;
        // }

        // rmt_SetCurrentThreadName("Main");

        dmSpinlock::Create(&g_ProfilerLock);
        dmAtomicStore32(&g_ProfilerInitialized, 1);

        //dmLogInfo("Initialized Remotery (ws://127.0.0.1:%d/rmt)", settings->port);
    }

    static inline void WaitForFlag(int32_atomic_t* lock)
    {
        while (dmAtomicGet32(lock) == 0) {
        }
    }

    static inline void AtomicUnlock(int32_atomic_t* lock)
    {
        dmAtomicStore32(lock, 0);
    }

    void Finalize()
    {
        {
            DM_SPINLOCK_SCOPED_LOCK(g_ProfilerLock);
            dmAtomicStore32(&g_ProfilerInitialized, 0);

            // g_SampleTreeCallback = 0;
            // g_SampleTreeCallbackCtx = 0;
            // g_PropertyTreeCallback = 0;
            // g_PropertyTreeCallbackCtx = 0;

            // if (g_Remotery)
            //     rmt_DestroyGlobalInstance(g_Remotery);
            // g_Remotery = 0;
        }

        dmSpinlock::Destroy(&g_ProfilerLock);
    }

    static const char* g_FrameMarker = "MainFrame";

    HProfile BeginFrame()
    {
        if (!IsInitialized())
        {
            return 0;
        }

        tracy::Profiler::SendFrameMark( g_FrameMarker, tracy::QueueType::FrameMarkMsgStart );
        return (HProfile)1;
    }

    void SetThreadName(const char* name)
    {
        if (!IsInitialized())
            return;

        tracy::SetThreadName(name);
    }

    void EndFrame(HProfile profile)
    {
        if (!IsInitialized())
        {
            return;
        }

        if (!profile)
            return;

        tracy::Profiler::SendFrameMark( g_FrameMarker, tracy::QueueType::FrameMarkMsgEnd );
    }

    void AddCounter(const char* name, uint32_t amount)
    {
        // Not supported currently.
        // Used by mem profiler to dynamically insert a property at runtime
    }

    uint64_t GetTicksPerSecond()
    {
        return 1000000;
    }

    void LogText(const char* format, ...)
    {
        if (!IsInitialized())
        {
            return;
        }

        char buffer[DM_PROFILE_TEXT_LENGTH];

        va_list lst;
        va_start(lst, format);
        int n = vsnprintf(buffer, DM_PROFILE_TEXT_LENGTH, format, lst);
        if (n < 0)
        {
            buffer[DM_PROFILE_TEXT_LENGTH-1] = '\0';
        }
        va_end(lst);

        TracyMessage(buffer, n);
    }

    void SetSampleTreeCallback(void* ctx, FSampleTreeCallback callback)
    {
        // This function is called both before the profiler is initialized, and during
        if (IsInitialized())
        {
            dmSpinlock::Lock(&g_ProfilerLock);
        }

        g_SampleTreeCallback = callback;
        g_SampleTreeCallbackCtx = ctx;

        if (IsInitialized())
        {
            dmSpinlock::Unlock(&g_ProfilerLock);
        }
    }

    void SetPropertyTreeCallback(void* ctx, FPropertyTreeCallback callback)
    {
        // This function is called both before the profiler is initialized, and during
        if (IsInitialized())
        {
            dmSpinlock::Lock(&g_ProfilerLock);
        }

        g_PropertyTreeCallback = callback;
        g_PropertyTreeCallbackCtx = ctx;

        if (IsInitialized())
        {
            dmSpinlock::Unlock(&g_ProfilerLock);
        }
    }

    void ProfileScope::StartScope(const char* source, const char* function, int line, const char* name, uint64_t* name_hash)
    {
        (void)name_hash;
        if (!IsInitialized()) {
            return;
        }
        if (name != 0)
        {
            valid = 1;
            if (name[0] == 0)
                name = "<empty>";

            TracyQueuePrepare( tracy::QueueType::ZoneBeginAllocSrcLocCallstack );
            const auto srcloc = tracy::Profiler::AllocSourceLocation( line, source, strlen(source), function, strlen(function), name, strlen(name), 0 );
            tracy::MemWrite( &item->zoneBegin.time, tracy::Profiler::GetTime() );
            tracy::MemWrite( &item->zoneBegin.srcloc, srcloc );
            TracyQueueCommit( zoneBeginThread );
        }
    }

    void ProfileScope::EndScope()
    {
        if (valid)
        {
            TracyQueuePrepare( tracy::QueueType::ZoneEnd );
            tracy::MemWrite( &item->zoneEnd.time, tracy::Profiler::GetTime() );
            TracyQueueCommit( zoneEndThread );
        }
    }


    void ScopeBegin(const char* name, uint64_t* name_hash)
    {
        (void)name_hash;
        if (!IsInitialized()) {
            return;
        }

        TracyQueuePrepare( tracy::QueueType::ZoneBeginAllocSrcLocCallstack );
        const auto srcloc = tracy::Profiler::AllocSourceLocation( 0, 0, 0, 0, 0, name, strlen(name), 0 );
        tracy::MemWrite( &item->zoneBegin.time, tracy::Profiler::GetTime() );
        tracy::MemWrite( &item->zoneBegin.srcloc, srcloc );
        TracyQueueCommit( zoneBeginThread );
    }

    void ScopeEnd()
    {
        if (!IsInitialized()) {
            return;
        }
        TracyQueuePrepare( tracy::QueueType::ZoneEnd );
        tracy::MemWrite( &item->zoneEnd.time, tracy::Profiler::GetTime() );
        TracyQueueCommit( zoneEndThread );
    }

    // *******************************************************************

    SampleIterator::SampleIterator()
    : m_Sample(0)
    , m_IteratorImpl(0)
    {
    }

    SampleIterator::~SampleIterator()
    {
    }

    SampleIterator* SampleIterateChildren(HSample sample, SampleIterator* iter)
    {
        return iter;
    }

    bool SampleIterateNext(SampleIterator* iter)
    {
        return false;
    }

    uint32_t SampleGetNameHash(HSample sample)
    {
        return 0;
    }

    const char* SampleGetName(HSample sample)
    {
        return 0;
    }

    uint64_t SampleGetStart(HSample sample)
    {
        return 0;
    }

    uint64_t SampleGetTime(HSample sample)
    {
        return 0;
    }

    uint64_t SampleGetSelfTime(HSample sample)
    {
        return 0;
    }

    uint32_t SampleGetCallCount(HSample sample)
    {
        return 0;
    }

    uint32_t SampleGetColor(HSample sample)
    {
        return 0;
    }


    // *******************************************************************

    PropertyIterator::PropertyIterator()
    : m_Property(0)
    , m_IteratorImpl(0)
    {
    }

    PropertyIterator::~PropertyIterator()
    {
    }

    PropertyIterator* PropertyIterateChildren(HProperty property, PropertyIterator* iter)
    {
        return iter;
    }

    bool PropertyIterateNext(PropertyIterator* iter)
    {
        return false;
    }

    // Property accessors

    uint32_t PropertyGetNameHash(HProperty hproperty)
    {
        return 0;
    }

    const char* PropertyGetName(HProperty hproperty)
    {
        return 0;
    }

    const char* PropertyGetDesc(HProperty hproperty)
    {
        return 0;
    }

    PropertyType PropertyGetType(HProperty hproperty)
    {
        return PROPERTY_TYPE_GROUP;
    }

    PropertyValue PropertyGetValue(HProperty hproperty)
    {
        PropertyValue out = {};
        return out;
    }


} // namespace dmProfile
