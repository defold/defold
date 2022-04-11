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

#include <stdio.h> // debug

#include "dlib/dlib.h"
#include "dlib/log.h"

#include "dlib/hash.h"

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

    void Initialize(const Options* options)
    {
        if (!dLib::IsDebugMode())
            return;

        rmtSettings* settings = rmt_Settings();

        if (options)
        {
            if (options->m_Port != 0)
            {
                settings->port = (rmtU16)options->m_Port;
            }
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

    bool IsInitialized()
    {
        return g_Remotery != 0;
    }

    HProfile BeginFrame()
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

    void Pause(bool pause)
    {
    }

    void EndFrame(HProfile profile)
    {
        if (!g_Remotery)
        {
            return;
        }

        if (!profile)
            return;

        rmt_EndCPUSample();
    }

    uint64_t GetTicksPerSecond()
    {
        return 1000000;
    }

    void AddCounter(const char* name, uint32_t amount)
    {

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
        delete (rmtSampleIterator*)m_IteratorImpl;
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
