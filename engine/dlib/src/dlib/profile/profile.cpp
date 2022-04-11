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

#include "profile.h"

#include <stdio.h> // printf

#include "dlib.h"
#include "log.h"

#include "hash.h"

#include "remotery/Remotery.h"

namespace dmProfile
{

    static Remotery* g_Remotery = 0;
    static FSampleTreeCallback  g_SampleTreeCallback = 0;
    static void*                g_SampleTreeCallbackCtx = 0;

    static inline rmtSample* FromHandle(HSample sample)
    {
        return (rmtSample*)sample;
    }

    static inline HSample ToHandle(rmtSample* sample)
    {
        return (HSample)sample;
    }

    static void SampleTreeCallback(void* ctx, rmtSampleTree* sample_tree)
    {
        if (g_SampleTreeCallback)
        {
            const char* thread_name = rmt_SampleTreeGetThreadName(sample_tree);
            rmtSample* root = rmt_SampleTreeGetRootSample(sample_tree);

            g_SampleTreeCallback(g_SampleTreeCallbackCtx, thread_name, ToHandle(root));
        }
    }

    void Initialize(uint32_t port, uint32_t max_scopes, uint32_t max_samples, uint32_t max_counters)
    {
        if (!dLib::IsDebugMode())
            return;

        rmtSettings* settings = rmt_Settings();
        if (port != 0)
        {
            settings->port = (rmtU16)port;
        }


        settings->sampletree_handler = SampleTreeCallback;
        settings->sampletree_context = 0;

        rmtError result = rmt_CreateGlobalInstance(&g_Remotery);
        if (result != RMT_ERROR_NONE)
        {
            dmLogError("Failed to initialize profile library");
            return;
        }

        rmt_SetCurrentThreadName("Main");

        dmLogInfo("Initialized Remotery (ws://127.0.0.1:%d/rmt)", settings->port);
    }

    void Finalize()
    {
        if (g_Remotery)
            rmt_DestroyGlobalInstance(g_Remotery);
        g_Remotery = 0;
    }

    // static void CalculateScopeProfileThread(Profile* profile, const uint32_t* key, uint8_t* value)
    // {
    //     const uint32_t n_scopes = g_Scopes.Size();
    //     const uint32_t n_samples = profile->m_Samples.Size();
    //     const uint32_t thread_id = *key;

    //     for (uint32_t i = 0; i < n_scopes; ++i)
    //     {
    //         g_Scopes[i].m_Internal = 0;
    //     }

    //     g_DummyScope.m_Internal = 0;

    //     for (uint32_t i = 0; i < n_samples; ++i)
    //     {
    //         Sample* sample = &profile->m_Samples[i];

    //         if (g_StringTable.Get((uintptr_t)sample->m_Name) == 0)
    //         {
    //             if (g_StringTable.Full())
    //             {
    //                 dmLogWarning("String table full in profiler");
    //             }
    //             else
    //             {
    //                 g_StringTable.Put((uintptr_t)sample->m_Name, sample->m_Name);
    //             }
    //         }

    //         // Does this sample belong to current thread?
    //         if (sample->m_ThreadId != thread_id)
    //             continue;

    //         Scope* scope = sample->m_Scope;

    //         if (scope->m_Internal == 0)
    //         {
    //             // First sample for this scope found
    //             scope->m_Internal = sample;
    //         }
    //         else
    //         {
    //             // Check if sample is overlapping the last sample
    //             // If overlapping ignore the sample. We are only interested in the
    //             // total time spent in top scope
    //             Sample* last_sample = (Sample*)scope->m_Internal;
    //             uint32_t end_last = last_sample->m_Start + last_sample->m_Elapsed;
    //             if (sample->m_Start >= last_sample->m_Start && sample->m_Start < end_last)
    //             {
    //                 // New simple within, ignore
    //             }
    //             else
    //             {
    //                 // Close the last scope and set new sample to current
    //                 ScopeData* scope_data = &profile->m_ScopesData[scope->m_Index];
    //                 scope_data->m_Elapsed += last_sample->m_Elapsed;
    //                 scope_data->m_Count++;
    //                 scope->m_Internal = sample;
    //             }
    //         }
    //     }

    //     // Close non-closed scopes
    //     for (uint32_t i = 0; i < n_scopes; ++i)
    //     {
    //         Scope* scope = &g_Scopes[i];
    //         if (scope->m_Internal != 0)
    //         {
    //             Sample* last_sample = (Sample*)scope->m_Internal;
    //             // Does this sample belong to current thread?
    //             if (last_sample->m_ThreadId != thread_id)
    //                 continue;

    //             ScopeData* scope_data = &profile->m_ScopesData[scope->m_Index];
    //             scope_data->m_Elapsed += last_sample->m_Elapsed;
    //             scope_data->m_Count++;
    //             scope->m_Internal = 0;
    //         }
    //     }

    //     // Frame-time is defined as the maximum scope in frame 0, ie main frame
    //     if (thread_id == 0)
    //     {
    //         if (g_Scopes.Size() > 0)
    //         {
    //             float millisPerTick = (float)(1000.0 / g_TicksPerSecond);
    //             g_FrameTime = profile->m_ScopesData[0].m_Elapsed * millisPerTick;
    //             for (uint32_t i = 1; i < g_Scopes.Size(); ++i)
    //             {
    //                 float time = profile->m_ScopesData[i].m_Elapsed * millisPerTick;
    //                 g_FrameTime = dmMath::Select(g_FrameTime - time, g_FrameTime, time);
    //             }
    //             ++g_MaxFrameTimeCounter;
    //             if (g_MaxFrameTimeCounter > 60 || g_FrameTime > g_MaxFrameTime)
    //             {
    //                 g_MaxFrameTimeCounter = 0;
    //                 g_MaxFrameTime = g_FrameTime;
    //             }
    //         }
    //         else
    //         {
    //             g_FrameTime = 0.0f;
    //         }
    //     }
    // }

    // static void CalculateScopeProfile(Profile* profile)
    // {
    //     // First pass is to determine count of active threads
    //     const uint32_t table_size = 16;
    //     const uint32_t capacity = 64;
    //     typedef dmHashTable<uint32_t, uint8_t> ActiveThreadsT;
    //     char hash_buf[table_size * sizeof(uint32_t) + capacity * sizeof(ActiveThreadsT::Entry)];

    //     ActiveThreadsT active_threads(hash_buf, table_size, capacity);
    //     const uint32_t n_samples = profile->m_Samples.Size();
    //     for (uint32_t i = 0; i < n_samples; ++i)
    //     {
    //         Sample* sample = &profile->m_Samples[i];
    //         if (!active_threads.Get(sample->m_ThreadId))
    //         {
    //             if (active_threads.Full())
    //             {
    //                 dmLogError("Thread set exceeded in profiler!");
    //                 break;
    //             }
    //             active_threads.Put(sample->m_ThreadId, 1);
    //         }
    //     }

    //     active_threads.Iterate(&CalculateScopeProfileThread, profile);
    // }


    bool IsInitialized()
    {
        return g_Remotery != 0;
    }

    HProfile Begin()
    {
        if (!g_Remotery)
        {
            return 0;
        }

        rmt_BeginCPUSample(FrameBegin, 0);

        return g_Remotery;
    }

    void SetThreadName(const char* name)
    {
        rmt_SetCurrentThreadName(name);
    }

    void Release(HProfile profile)
    {
        if (!g_Remotery)
        {
            return;
        }

        if (!profile)
            return;

        rmt_EndCPUSample();
    }

    uint32_t AllocateScope(const char* name)
    {
        return 0xffffffffu;
    }

    uint32_t GetNameHash(const char* name, uint32_t string_length)
    {
        printf("MAWE GetNameHash %s\n", name);
        return 0;
        //return _rmt_HashName(name);
    }

    void AddCounter(const char* name, uint32_t amount)
    {

    }

    uint32_t AllocateCounter(const char* name)
    {
        return 0xFFFFFFFF;
    }

    void AddCounterIndex(uint32_t counter_index, uint32_t amount)
    {
    }

    float GetFrameTime()
    {
        return 0.0f;
    }

    float GetMaxFrameTime()
    {
        return 0.0f;
    }

    uint64_t GetTicksPerSecond()
    {
        return 0;
    }

    void SetSampleTreeCallback(void* ctx, FSampleTreeCallback callback)
    {
        g_SampleTreeCallback = callback;
        g_SampleTreeCallbackCtx = ctx;
    }

    void ProfileScope::StartScope(const char* name, uint32_t* name_hash)
    {
        _rmt_BeginCPUSample(name, RMTSF_Aggregate, name_hash);
    }

    void ProfileScope::EndScope()
    {
        rmt_EndCPUSample();
    }


    // *******************************************************************

    SampleIterator::SampleIterator()
    : m_Sample(0)
    , m_IteratorImpl(0)
    {
    }

    SampleIterator::~SampleIterator()
    {
        delete m_IteratorImpl;
    }

    SampleIterator IterateChildren(HSample sample)
    {
        SampleIterator iter;
        iter.m_Sample = 0;

        rmtSampleIterator* rmt_iter = new rmtSampleIterator;
        rmt_IterateChildren(rmt_iter, FromHandle(sample));

        iter.m_IteratorImpl = (void*)rmt_iter;
        return iter;
    }

    bool IterateNext(SampleIterator* iter)
    {
        rmtSampleIterator* rmt_iter = (rmtSampleIterator*)iter->m_IteratorImpl;
        bool result = _rmt_IterateNext(rmt_iter);
        iter->m_Sample = ToHandle(rmt_iter->sample);
        return result;
    }

    const char* SampleGetName(HSample sample)
    {
        return rmt_SampleGetName(FromHandle(sample));
    }

    uint64_t SampleGetTime(HSample sample)
    {
        return rmt_SampleGetTime(FromHandle(sample));
    }

    uint64_t SampleGetSelfTime(HSample sample)
    {
        return rmt_SampleGetSelfTime(FromHandle(sample));
    }

    uint32_t SampleGetCallCount(HSample sample)
    {
        return rmt_SampleGetCallCount(FromHandle(sample));
    }

    uint32_t SampleGetColor(HSample sample)
    {
        uint8_t r, g, b;
        rmt_SampleGetColour(FromHandle(sample), &r, &g, &b);
        return r << 24 | g << 16 | b;
    }

    // *******************************************************************


} // namespace dmProfile
