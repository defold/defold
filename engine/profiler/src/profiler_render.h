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

#ifndef DM_PROFILE_RENDER_H
#define DM_PROFILE_RENDER_H

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/profile.h>

namespace dmRender
{
    typedef struct FontMap*         HFontMap;
    typedef struct RenderContext*   HRenderContext;
}

namespace dmProfileRender
{
    typedef struct RenderProfile* HRenderProfile;

    enum ProfilerMode {
        PROFILER_MODE_RUN = 1,
        PROFILER_MODE_PAUSE = 2,
        PROFILER_MODE_SHOW_PEAK_FRAME = 3,
        PROFILER_MODE_RECORD = 4
    };

    enum ProfilerViewMode
    {
        PROFILER_VIEW_MODE_FULL = 1,
        PROFILER_VIEW_MODE_MINIMIZED = 2
    };

    struct ProfilerSample
    {
        uint32_t m_NameHash;
        uint64_t m_StartTime;
        uint64_t m_Time;
        uint64_t m_SelfTime;
        uint32_t m_Count;
        uint32_t m_Color;
        uint8_t  m_Indent; // The stack depth
    };

    struct ProfilerProperty
    {
        uint32_t                    m_NameHash;
        ProfilePropertyValue        m_Value;
        ProfilePropertyType         m_Type;
        uint8_t                     m_Indent; // The stack depth
    };

    struct ProfilerThread
    {
        dmArray<ProfilerSample>     m_Samples;
        uint32_t                    m_NameHash;
        uint64_t                    m_Time;             // The time of the last update for this thread
        uint64_t                    m_SamplesTotalTime; // The elapsed time of the samples in the thread

        ProfilerThread();
    };

    // The frame holds aggregated info about all threads
    struct ProfilerFrame
    {
        dmArray<ProfilerThread*>    m_Threads;
        dmArray<ProfilerProperty>   m_Properties;
        uint64_t                    m_Time;           // The time of the last update for this frame

        ProfilerFrame();
    };


    HRenderProfile NewRenderProfile(float fps);
    void UpdateRenderProfile(HRenderProfile render_profile, const ProfilerFrame* frame);
    void SetMode(HRenderProfile render_profile, ProfilerMode mode);
    void SetViewMode(HRenderProfile render_profile, ProfilerViewMode view_mode);
    void SetWaitTime(HRenderProfile render_profile, bool include_wait_time);
    void AdjustShownFrame(HRenderProfile render_profile, int distance);
    void ShowRecordedFrame(HRenderProfile render_profile, int frame);
    int GetRecordedFrameCount(HRenderProfile render_profile);
    void DeleteRenderProfile(HRenderProfile render_profile);

    void Draw(HRenderProfile render_profile, dmRender::HRenderContext render_context, dmRender::HFontMap font_map);

    void DumpFrame(ProfilerFrame* frame);

    //
    void ClearProfilerThreadSamples(ProfilerThread* thread);
    ProfilerThread* FindOrCreateProfilerThread(ProfilerFrame* ctx, uint32_t name_hash);
    void DeleteProfilerFrame(ProfilerFrame* frame);
    void PruneProfilerThreads(ProfilerFrame* ctx, uint64_t time);

    void AddProperty(ProfilerFrame* frame, uint32_t name_hash, ProfilePropertyType type, ProfilePropertyValue value, int indent);
}

#endif
