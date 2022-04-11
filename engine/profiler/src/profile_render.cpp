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

#include "profile_render.h"

#include <string.h>

#include <algorithm>

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/index_pool.h>
#include <dlib/math.h>
#include <dlib/profile.h>
#include <dmsdk/dlib/vmath.h>
#include <render/font_renderer.h>
#include <render/font_ddf.h>
#include "profile_render.h"

namespace dmProfileRender
{
/**
 * The on-screen profiler stores a copy of a profiled frame locally and tries
 * to be as efficitent as possible by only storing the necessary data.
 *
 * It also has a recording buffer where recorded frames are stored.
 *
 * A base frame (ProfileFrame) is a struct containing all the data related to
 * a sampled frame for all the threads.
 *
 * The profiler (RenderProfile) displays values from a ProfilerFrame
 *
 * Extra data such as filtered values for elapsed times and "last seen" information
 * is separated and only present in the RenderProfile, not in the actual frames.
 *
 * The lookup-by-hash and allocation information for data in the RenderProfile is
 * built on demand as a new frame is built from a frame sample on the go or when
 * picking up and displaying a frame from the recording buffer.
 *
 * The RenderProfile is created with a pre-defined number of sample/scopes/counters
 * available and no memory allocation is done when processing a frame.
 *
 * When recording profile frames only one malloc() call is done per recorded frame
 * to reduce overhead and keep the data compact.
 *
 * The profiler sorts the data on filtered values and keeps the coloring of sample
 * names consistent in an effort to reduce flickering and samples moving around to fast.
 *
 * It also automatically adjust the layout depending on the aspect ratio of the
 * profiler display area (which is directly derived from the window/screen size).
 *
 * Some terms:
            *  -   Scope           High level scope such as "Engine" and "Script", the first parameter to DM_PROFILE(Physics, <all samples>)
 *  -   Counter         A counter - ie DM_COUNTER("DrawCalls", 1)
 *  -   Sample          A single sample of a ProfilerScope, ie DM_PROFILE(Physics, "UpdateKinematic")
            *  -   SampleAggregate The aggregation of all the samples for a ProfilerScope, ie DM_PROFILE(Physics, "UpdateKinematic")
            *                      it also has a list of all the Sample instances for the DM_PROFILE scope
 *  -   ProfileFrame    Information (Scopes, Counters, Samples, SampleAggregates etc) of a single profile frame
 *  -   ProfileSnapshot A recorded profile frame, embedds a ProfileFrame
 *  -   RenderProfile   The on-screen profiler instance, embeds a ProfileFrame and holds lookup tables and allocation
 *                      buffers for sampled data. It also holds information of filtered values, sorting and a
 *                      recording buffer (array of ProfileSnapshot)
 */
    static const int CHARACTER_HEIGHT  = 16;
    static const int CHARACTER_WIDTH   = 8;
    static const int CHARACTER_BORDER  = 1;
    static const int LINE_SPACING = CHARACTER_HEIGHT + CHARACTER_BORDER * 2;
    static const int BORDER_SIZE  = 8;

    static const int SCOPES_NAME_WIDTH  = 17 * CHARACTER_WIDTH;
    static const int SCOPES_TIME_WIDTH  = 6 * CHARACTER_WIDTH;
    static const int SCOPES_COUNT_WIDTH = 3 * CHARACTER_WIDTH;

    static const int PORTRAIT_SAMPLE_FRAMES_NAME_LENGTH = 40;
    static const int LANDSCAPE_SAMPLE_FRAMES_NAME_LENGTH = 60;
    static const int PORTRAIT_SAMPLE_FRAMES_NAME_WIDTH  = PORTRAIT_SAMPLE_FRAMES_NAME_LENGTH * CHARACTER_WIDTH;
    static const int LANDSCAPE_SAMPLE_FRAMES_NAME_WIDTH  = LANDSCAPE_SAMPLE_FRAMES_NAME_LENGTH * CHARACTER_WIDTH;
    static const int SAMPLE_FRAMES_TIME_WIDTH  = 6 * CHARACTER_WIDTH;
    static const int SAMPLE_FRAMES_COUNT_WIDTH = 3 * CHARACTER_WIDTH;

    static const int COUNTERS_NAME_WIDTH  = 15 * CHARACTER_WIDTH;
    static const int COUNTERS_COUNT_WIDTH = 12 * CHARACTER_WIDTH;

    enum DisplayMode
    {
        DISPLAYMODE_MINIMIZED,
        DISPLAYMODE_LANDSCAPE,
        DISPLAYMODE_PORTRAIT
    };

    using namespace dmVMath;

    static const Vector4 PROFILER_BG_COLOR  = Vector4(0.1f, 0.1f, 0.1f, 0.6f);
    static const Vector4 TITLE_FACE_COLOR   = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    static const Vector4 TITLE_SHADOW_COLOR = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    static const Vector4 SAMPLES_BG_COLOR   = Vector4(0.15f, 0.15f, 0.15f, 0.2f);

    static ProfilerFrame* DuplicateProfilerFrame(const ProfilerFrame* frame);
    static ProfilerThread* GetSelectedThread(HRenderProfile render_profile, ProfilerFrame* frame);

    // // These hashes are used so we can filter out the time the engine is waiting for frame buffer flip
    // static const uint32_t VSYNC_WAIT_NAME_HASH   = 0123;//GetCombinedHash(dmProfile::GetNameHash("VSync", (uint32_t)strlen("VSync")), dmProfile::GetNameHash("Wait", (uint32_t)strlen("Wait")));
    // static const uint32_t ENGINE_FRAME_NAME_HASH = 4567;//GetCombinedHash(dmProfile::GetNameHash("Engine", (uint32_t)strlen("Engine")), dmProfile::GetNameHash("Frame", (uint32_t)strlen("Frame")));

    ProfilerThread::ProfilerThread()
    : m_NameHash(0)
    , m_Name(0)
    , m_Time(0)
    , m_SamplesTotalTime(0)
    {
    }

    // The RenderProfile contains the current "live" frame and
    // stats used to sort/purge sample items
    struct RenderProfile
    {
        static RenderProfile* New(float fps, uint32_t ticks_per_second, uint32_t lifetime_in_milliseconds);
        static void Delete(RenderProfile* render_profile);

        float m_FPS;
        uint64_t m_TicksPerSecond;
        uint64_t m_LifeTime;

        ProfilerFrame* m_CurrentFrame;
        ProfilerFrame* m_ActiveFrame;
        dmArray<ProfilerFrame*> m_RecordBuffer;

        ProfilerMode m_Mode;
        ProfilerViewMode m_ViewMode;

        ProfilerThread m_EmptyThread;

        uint64_t m_MaxFrameTime;
        int32_t m_PlaybackFrame;

        uint32_t m_IncludeFrameWait : 1;

        RenderProfile(float fps, uint64_t ticks_per_second, uint64_t lifetime_in_milliseconds);
    };

    static void FlushRecording(RenderProfile* render_profile, uint32_t capacity)
    {
        uint32_t c = render_profile->m_RecordBuffer.Size();
        for (uint32_t i = 0; i < c; ++i)
        {
            ProfilerFrame* frame = render_profile->m_RecordBuffer[c];
            DeleteProfilerFrame(frame);
            delete frame;
        }
        if (render_profile->m_RecordBuffer.Capacity() < capacity)
            render_profile->m_RecordBuffer.SetCapacity(capacity);
        render_profile->m_RecordBuffer.SetSize(0);
        render_profile->m_PlaybackFrame = -1;
    }

    // static void BuildScope(void* context, const dmProfile::ScopeData* scope_data)
    // {
    //     if (scope_data->m_Count == 0)
    //     {
    //         return;
    //     }
    //     RenderProfile* render_profile = (RenderProfile*)context;
    //     dmProfile::Scope* scope       = scope_data->m_Scope;
    //     if (!AddName(render_profile, scope->m_NameHash, scope->m_Name))
    //     {
    //         render_profile->m_ScopeOverflow = 1;
    //         return;
    //     }
    //     AddScope(render_profile, scope->m_NameHash, scope_data->m_Elapsed, scope_data->m_Count);
    // }

    // static void BuildSampleAggregate(void* context, const dmProfile::Sample* sample)
    // {
    //     if (sample->m_Elapsed == 0u)
    //     {
    //         return;
    //     }
    //     RenderProfile* render_profile = (RenderProfile*)context;
    //     if (!AddName(render_profile, sample->m_NameHash, sample->m_Name))
    //     {
    //         render_profile->m_SampleAggregateOverflow = 1;
    //         return;
    //     }
    //     AddSample(render_profile, sample->m_NameHash, sample->m_Scope->m_NameHash, sample->m_Start, sample->m_Elapsed);
    // }

    // static void BuildCounter(void* context, const dmProfile::CounterData* counter_data)
    // {
    //     if (counter_data->m_Value == 0)
    //     {
    //         return;
    //     }
    //     RenderProfile* render_profile = (RenderProfile*)context;
    //     dmProfile::Counter* counter   = counter_data->m_Counter;
    //     if (!AddName(render_profile, counter->m_NameHash, counter->m_Name))
    //     {
    //         render_profile->m_CounterOverflow = 1;
    //         return;
    //     }
    //     AddCounter(render_profile, counter->m_NameHash, counter_data->m_Value);
    // }

    // static uint32_t GetWaitTicks(HRenderProfile render_profile)
    // {
    //     // TIndex* wait_time_ptr = render_profile->m_SampleAggregateLookup.m_HashLookup.Get(VSYNC_WAIT_NAME_HASH);
    //     // if (wait_time_ptr != 0x0)
    //     // {
    //     //     SampleAggregate* sample_aggregate = &render_profile->m_ActiveFrame->m_SampleAggregates[*wait_time_ptr];
    //     //     return sample_aggregate->m_Elapsed;
    //     // }
    //     return 0;
    // }

    // static float GetWaitTime(HRenderProfile render_profile)
    // {
    //     uint32_t wait_ticks       = GetWaitTicks(render_profile);
    //     uint64_t ticks_per_second = render_profile->m_TicksPerSecond;
    //     double elapsed_s          = (double)(wait_ticks) / ticks_per_second;
    //     float elapsed_ms          = (float)(elapsed_s * 1000.0);
    //     return elapsed_ms;
    // }

    // static uint32_t GetFrameTicks(HRenderProfile render_profile)
    // {
    //     TIndex* frame_time_ptr = render_profile->m_SampleAggregateLookup.m_HashLookup.Get(ENGINE_FRAME_NAME_HASH);
    //     if (frame_time_ptr != 0x0)
    //     {
    //         SampleAggregate* sample_aggregate = &render_profile->m_ActiveFrame->m_SampleAggregates[*frame_time_ptr];
    //         return sample_aggregate->m_Elapsed;
    //     }
    //     return 0;
    // }

    // static uint32_t GetActiveFrameTicks(HRenderProfile render_profile, uint32_t frame_ticks)
    // {
    //     return frame_ticks - GetWaitTicks(render_profile);
    // }

    // static void BuildStructure(dmProfileRender::ProfilerFrame* frame_context, RenderProfile* render_profile)
    // {
    //     DM_MUTEX_SCOPED_LOCK(g_ProfilerFrameContext->m_Mutex);

    //     if (frame_context->m_Threads.Empty())
    //         return;

    //     // TODO: Find the main thread
    //     ProfilerThread* thread = frame_context->m_Threads[0]; // Hopefully the main thread

    //     if (thread->m_Samples.Empty())
    //         return;

    //     ProfilerSample* root_sample = thread->m_Samples[0];

    //     render_profile->m_NowTick                    = dmTime::GetTime();

    //     //render_profile->m_BuildFrame->m_WaitTime = GetWaitTime(render_profile);
    // }

    // struct CounterSortPred
    // {
    //     CounterSortPred(RenderProfile* render_profile)
    //         : m_RenderProfile(render_profile)
    //     {
    //     }

    //     bool operator()(TIndex a_index, TIndex b_index) const
    //     {
    //         const ProfilerFrame* frame         = m_RenderProfile->m_ActiveFrame;
    //         const Counter& counter_a          = frame->m_Counters[a_index];
    //         const char* const* counter_a_name = m_RenderProfile->m_NameLookupTable.Get(counter_a.m_NameHash);
    //         const Counter& counter_b          = frame->m_Counters[b_index];
    //         const char* const* counter_b_name = m_RenderProfile->m_NameLookupTable.Get(counter_b.m_NameHash);
    //         return dmStrCaseCmp(*counter_a_name, *counter_b_name) < 0;
    //     }

    //     const RenderProfile* m_RenderProfile;
    // };

    // static void SortStructure(RenderProfile* render_profile)
    // {
    //     uint32_t scope_count = render_profile->m_ScopeLookup.m_HashLookup.Size();
    //     std::stable_sort(&render_profile->m_ScopeLookup.m_SortOrder[0], &render_profile->m_ScopeLookup.m_SortOrder[scope_count], ScopeSortPred(render_profile));

    //     uint32_t sample_aggregate_count = render_profile->m_SampleAggregateLookup.m_HashLookup.Size();
    //     std::stable_sort(&render_profile->m_SampleAggregateLookup.m_SortOrder[0], &render_profile->m_SampleAggregateLookup.m_SortOrder[sample_aggregate_count], SampleAggregateSortPred(render_profile));

    //     uint32_t counter_count = render_profile->m_CounterLookup.m_HashLookup.Size();
    //     std::sort(&render_profile->m_CounterLookup.m_SortOrder[0], &render_profile->m_CounterLookup.m_SortOrder[counter_count], CounterSortPred(render_profile));
    // }

    static void GotoRecordedFrame(HRenderProfile render_profile, int recorded_frame_index)
    {
        if (render_profile->m_RecordBuffer.Size() == 0)
        {
            return;
        }
        if (recorded_frame_index >= (int)render_profile->m_RecordBuffer.Size())
        {
            recorded_frame_index = (int)render_profile->m_RecordBuffer.Size() - 1;
        }
        else if (recorded_frame_index < 0)
        {
            recorded_frame_index = 0;
        }

        ProfilerFrame* snapshot = render_profile->m_RecordBuffer[recorded_frame_index];
        if (snapshot == render_profile->m_ActiveFrame)
        {
            return;
        }

        render_profile->m_PlaybackFrame = recorded_frame_index;
        render_profile->m_ActiveFrame = snapshot;
    }

    struct Position
    {
        Position(int x, int y)
            : x(x)
            , y(y)
        {
        }
        int x;
        int y;
    };

    struct Size
    {
        Size(int w, int h)
            : w(w)
            , h(h)
        {
        }
        int w;
        int h;
    };

    struct Area
    {
        Area(Position p, Size s)
            : p(p)
            , s(s)
        {
        }
        Position p;
        Size s;
    };

    static Area GetProfilerArea(DisplayMode display_mode, const Size& display_size)
    {
        if (display_mode == DISPLAYMODE_MINIMIZED)
        {
            Position p(BORDER_SIZE, display_size.h - (BORDER_SIZE + CHARACTER_HEIGHT + CHARACTER_BORDER * 2));
            Size s(display_size.w - (BORDER_SIZE * 2), CHARACTER_HEIGHT + CHARACTER_BORDER * 2);
            return Area(p, s);
        }
        Position p(BORDER_SIZE, BORDER_SIZE);
        Size s(display_size.w - (BORDER_SIZE * 2), display_size.h - (BORDER_SIZE * 2));
        return Area(p, s);
    }

    static Area GetHeaderArea(DisplayMode display_mode, const Area& profiler_area)
    {
        Size s(profiler_area.s.w, CHARACTER_HEIGHT + CHARACTER_BORDER * 2);
        Position p(profiler_area.p.x, profiler_area.p.y + profiler_area.s.h - s.h);
        return Area(p, s);
    }

    static Area GetDetailsArea(DisplayMode display_mode, const Area& profiler_area, const Area& header_area)
    {
        Size s(profiler_area.s.w, profiler_area.s.h - header_area.s.h);
        Position p(profiler_area.p.x, profiler_area.p.y);
        return Area(p, s);
    }

    static Area GetScopesArea(DisplayMode display_mode, const Area& details_area, int scopes_count, int counters_count)
    {
        const int count = dmMath::Max(scopes_count, counters_count);

        Size s(SCOPES_NAME_WIDTH + CHARACTER_WIDTH + SCOPES_TIME_WIDTH + CHARACTER_WIDTH + SCOPES_COUNT_WIDTH, LINE_SPACING * (1 + count));
        if (display_mode == DISPLAYMODE_LANDSCAPE)
        {
            Position p(details_area.p.x, details_area.p.y + details_area.s.h - s.h);
            return Area(p, s);
        }
        else if (display_mode == DISPLAYMODE_PORTRAIT)
        {
            Position p(details_area.p.x + details_area.s.w - s.w, details_area.p.y);
            return Area(p, s);
        }
        return Area(Position(0, 0), Size(0, 0));
    }

    static Area GetCountersArea(DisplayMode display_mode, const Area& details_area, int scopes_count, int counters_count)
    {
        if (display_mode == DISPLAYMODE_LANDSCAPE || display_mode == DISPLAYMODE_PORTRAIT)
        {
            const int count = dmMath::Max(scopes_count, counters_count);
            Size s(COUNTERS_NAME_WIDTH + CHARACTER_WIDTH + COUNTERS_COUNT_WIDTH, LINE_SPACING * (1 + count));
            Position p(details_area.p.x, details_area.p.y);
            return Area(p, s);
        }
        return Area(Position(0, 0), Size(0, 0));
    }

    static Area GetSamplesArea(DisplayMode display_mode, const Area& details_area, const Area& scopes_area, const Area& counters_area)
    {
        if (display_mode == DISPLAYMODE_LANDSCAPE)
        {
            Size s(details_area.s.w - (scopes_area.s.w + CHARACTER_WIDTH), details_area.s.h);
            Position p(scopes_area.p.x + scopes_area.s.w + CHARACTER_WIDTH, details_area.p.y + details_area.s.h - s.h);
            return Area(p, s);
        }
        else if (display_mode == DISPLAYMODE_PORTRAIT)
        {
            int lower_clip = dmMath::Max(scopes_area.p.y + scopes_area.s.h, counters_area.p.y + counters_area.s.h);
            int max_height = details_area.p.y + details_area.s.h - lower_clip;
            Size s(details_area.s.w, max_height);
            Position p(details_area.p.x, details_area.p.y + details_area.s.h - s.h);
            return Area(p, s);
        }
        return Area(Position(0, 0), Size(0, 0));
    }

    static Area GetSampleFramesArea(DisplayMode display_mode, int sample_frames_name_width, const Area& samples_area)
    {
        const int offset_y = LINE_SPACING;
        const int offset_x = sample_frames_name_width + CHARACTER_WIDTH + SAMPLE_FRAMES_TIME_WIDTH + CHARACTER_WIDTH + SAMPLE_FRAMES_COUNT_WIDTH + CHARACTER_WIDTH;

        Position p(samples_area.p.x + offset_x, samples_area.p.y);
        Size s(samples_area.s.w - offset_x, samples_area.s.h - offset_y);

        return Area(p, s);
    }

    static void FillArea(dmRender::HRenderContext render_context, const Area& a, const Vector4& col)
    {
        dmRender::Square2d(render_context, a.p.x, a.p.y, a.p.x + a.s.w, a.p.y + a.s.h, col);
    }

    static void Draw(HRenderProfile render_profile, dmRender::HRenderContext render_context, dmRender::HFontMap font_map, const Size display_size, DisplayMode display_mode)
    {
        uint32_t batch_key = 0;
        {
            HashState64 key_state;
            dmHashInit64(&key_state, false);
            dmHashUpdateBuffer64(&key_state, &font_map, sizeof(font_map));
            uint16_t render_order = 0;
            dmHashUpdateBuffer64(&key_state, &render_order, sizeof(uint16_t));
            batch_key = dmHashFinal64(&key_state);
        }

        const int SAMPLE_FRAMES_NAME_LENGTH = display_mode == DISPLAYMODE_LANDSCAPE ? LANDSCAPE_SAMPLE_FRAMES_NAME_LENGTH : PORTRAIT_SAMPLE_FRAMES_NAME_LENGTH;
        const int SAMPLE_FRAMES_NAME_WIDTH = display_mode == DISPLAYMODE_LANDSCAPE ? LANDSCAPE_SAMPLE_FRAMES_NAME_WIDTH : PORTRAIT_SAMPLE_FRAMES_NAME_WIDTH;

        const Area profiler_area = GetProfilerArea(display_mode, display_size);
        FillArea(render_context, profiler_area, PROFILER_BG_COLOR);

        const Area header_area  = GetHeaderArea(display_mode, profiler_area);
        const Area details_area = GetDetailsArea(display_mode, profiler_area, header_area);

        char buffer[256];
        float col[3];

        dmRender::DrawTextParams params;
        params.m_FaceColor   = TITLE_FACE_COLOR;
        params.m_ShadowColor = TITLE_SHADOW_COLOR;

        ProfilerFrame* frame = render_profile->m_ActiveFrame;
        ProfilerThread* thread = GetSelectedThread(render_profile, frame);

        uint64_t ticks_per_second   = render_profile->m_TicksPerSecond;
        uint64_t max_frame_time     = render_profile->m_MaxFrameTime;
        uint64_t frame_time         = thread->m_SamplesTotalTime;

        float frame_time_f = frame_time / (float)ticks_per_second;
        float max_frame_time_f = max_frame_time / (float)ticks_per_second;

        int l = dmSnPrintf(buffer, sizeof(buffer), "Frame: %6.3f Max: %6.3f", frame_time_f, max_frame_time_f);

        switch (render_profile->m_Mode)
        {
            case PROFILER_MODE_RUN:
                break;
            case PROFILER_MODE_PAUSE:
                if (render_profile->m_PlaybackFrame < 0 || render_profile->m_PlaybackFrame == (int32_t)render_profile->m_RecordBuffer.Size())
                {
                    dmSnPrintf(&buffer[l], sizeof(buffer) - l, " (Paused)");
                }
                else
                {
                    dmSnPrintf(&buffer[l], sizeof(buffer) - l, " (Show: %d)", render_profile->m_PlaybackFrame);
                }
                break;
            case PROFILER_MODE_SHOW_PEAK_FRAME:
                dmSnPrintf(&buffer[l], sizeof(buffer) - l, " (Peak)");
                break;
            case PROFILER_MODE_RECORD:
                dmSnPrintf(&buffer[l], sizeof(buffer) - l, " (Rec: %d)", render_profile->m_PlaybackFrame);
                break;
        }

        params.m_Text = buffer;
        params.m_WorldTransform.setElem(3, 0, header_area.p.x);
        params.m_WorldTransform.setElem(3, 1, header_area.p.y + CHARACTER_HEIGHT);
        dmRender::DrawText(render_context, font_map, 0, batch_key, params);

        if (render_profile->m_ViewMode == PROFILER_VIEW_MODE_MINIMIZED)
        {
            return;
        }

        const uint32_t scope_count            = 0;//(uint32_t)render_profile->m_ScopeLookup.m_HashLookup.Size();
        const uint32_t counter_count          = 0;//(uint32_t)render_profile->m_CounterLookup.m_HashLookup.Size();
        //const uint32_t sample_aggregate_count = (uint32_t)render_profile->m_SampleAggregateLookup.m_HashLookup.Size();

        // We don't support scopes anymore
        const Area scopes_area        = GetScopesArea(display_mode, details_area, scope_count, counter_count);
        const Area counters_area      = GetCountersArea(display_mode, details_area, scope_count, counter_count);
        const Area samples_area       = GetSamplesArea(display_mode, details_area, scopes_area, counters_area);
        const Area sample_frames_area = GetSampleFramesArea(display_mode, SAMPLE_FRAMES_NAME_WIDTH, samples_area);

        FillArea(render_context, sample_frames_area, SAMPLES_BG_COLOR);

        // We don't support scopes anymore
        // {
        //     // Scopes
        //     int y = scopes_area.p.y + scopes_area.s.h;

        //     params.m_WorldTransform.setElem(3, 1, y);

        //     int name_x  = scopes_area.p.x;
        //     int time_x  = name_x + SCOPES_NAME_WIDTH + CHARACTER_WIDTH;
        //     int count_x = time_x + SCOPES_TIME_WIDTH + CHARACTER_WIDTH;

        //     params.m_FaceColor = TITLE_FACE_COLOR;
        //     params.m_Text      = render_profile->m_ScopeOverflow ? "*Scopes:" : "Scopes:";
        //     params.m_WorldTransform.setElem(3, 0, name_x);
        //     dmRender::DrawText(render_context, font_map, 0, batch_key, params);
        //     params.m_Text = "    ms";
        //     params.m_WorldTransform.setElem(3, 0, time_x);
        //     dmRender::DrawText(render_context, font_map, 0, batch_key, params);
        //     params.m_Text = "  #";
        //     params.m_WorldTransform.setElem(3, 0, count_x);
        //     dmRender::DrawText(render_context, font_map, 0, batch_key, params);

        //     HslToRgb2(4 / 16.0f, 1.0f, 0.65f, col);
        //     params.m_FaceColor = Vector4(col[0], col[1], col[2], 1.0f);

        //     for (TIndex sort_index = 0; sort_index < scope_count; ++sort_index)
        //     {
        //         TIndex index = render_profile->m_ScopeLookup.m_SortOrder[sort_index];
        //         Scope* scope = &frame->m_Scopes[index];

        //         y -= LINE_SPACING;

        //         if ((display_mode == DISPLAYMODE_LANDSCAPE) && (y < (counters_area.p.y + counters_area.s.h + LINE_SPACING)))
        //         {
        //             break;
        //         }

        //         double e = (double)(scope->m_Elapsed) / ticks_per_second;

        //         params.m_WorldTransform.setElem(3, 1, y);

        //         params.m_Text = *render_profile->m_NameLookupTable.Get(scope->m_NameHash);
        //         params.m_WorldTransform.setElem(3, 0, name_x);
        //         dmRender::DrawText(render_context, font_map, 0, batch_key, params);

        //         params.m_Text = buffer;
        //         dmSnPrintf(buffer, sizeof(buffer), "%6.3f", (float)(e * 1000.0));
        //         params.m_WorldTransform.setElem(3, 0, time_x);
        //         dmRender::DrawText(render_context, font_map, 0, batch_key, params);

        //         params.m_Text = buffer;
        //         dmSnPrintf(buffer, sizeof(buffer), "%3u", scope->m_Count);
        //         params.m_WorldTransform.setElem(3, 0, count_x);
        //         dmRender::DrawText(render_context, font_map, 0, batch_key, params);
        //     }
        // }
        // {
        //     // Counters
        //     int y = counters_area.p.y + counters_area.s.h;

        //     params.m_WorldTransform.setElem(3, 1, y);

        //     int name_x  = counters_area.p.x;
        //     int count_x = name_x + COUNTERS_NAME_WIDTH + CHARACTER_WIDTH;

        //     params.m_FaceColor = TITLE_FACE_COLOR;
        //     params.m_Text      = render_profile->m_CounterOverflow ? "*Counters:" : "Counters:";
        //     params.m_WorldTransform.setElem(3, 0, name_x);
        //     dmRender::DrawText(render_context, font_map, 0, batch_key, params);
        //     params.m_Text = "           #";
        //     params.m_WorldTransform.setElem(3, 0, count_x);
        //     dmRender::DrawText(render_context, font_map, 0, batch_key, params);

        //     HslToRgb2(4 / 16.0f, 1.0f, 0.65f, col);
        //     params.m_FaceColor = Vector4(col[0], col[1], col[2], 1.0f);
        //     for (TIndex sort_index = 0; sort_index < counter_count; ++sort_index)
        //     {
        //         TIndex index     = render_profile->m_CounterLookup.m_SortOrder[sort_index];
        //         Counter* counter = &frame->m_Counters[index];

        //         y -= LINE_SPACING;

        //         params.m_WorldTransform.setElem(3, 1, y);
        //         params.m_Text = *render_profile->m_NameLookupTable.Get(counter->m_NameHash);
        //         params.m_WorldTransform.setElem(3, 0, name_x);
        //         dmRender::DrawText(render_context, font_map, 0, batch_key, params);

        //         params.m_Text = buffer;
        //         dmSnPrintf(buffer, sizeof(buffer), "%12u", counter->m_Count);
        //         params.m_WorldTransform.setElem(3, 0, count_x);
        //         dmRender::DrawText(render_context, font_map, 0, batch_key, params);
        //     }
        // }
        {
            // Samples
            int y = samples_area.p.y + samples_area.s.h;

            params.m_WorldTransform.setElem(3, 1, y);

            int name_x   = samples_area.p.x;
            int time_x   = name_x + SAMPLE_FRAMES_NAME_WIDTH + CHARACTER_WIDTH;
            int count_x  = time_x + SAMPLE_FRAMES_TIME_WIDTH + CHARACTER_WIDTH;
            int frames_x = sample_frames_area.p.x;

            params.m_FaceColor = TITLE_FACE_COLOR;
            params.m_Text      = "Samples:";
            params.m_WorldTransform.setElem(3, 0, name_x);
            dmRender::DrawText(render_context, font_map, 0, batch_key, params);
            params.m_Text = "    ms";
            params.m_WorldTransform.setElem(3, 0, time_x);
            dmRender::DrawText(render_context, font_map, 0, batch_key, params);
            params.m_Text = "  #";
            params.m_WorldTransform.setElem(3, 0, count_x);
            dmRender::DrawText(render_context, font_map, 0, batch_key, params);
            params.m_Text = "Frame:";
            params.m_WorldTransform.setElem(3, 0, frames_x);
            dmRender::DrawText(render_context, font_map, 0, batch_key, params);

            uint32_t sample_frame_width         = sample_frames_area.s.w;

            // const uint32_t frame_ticks          = GetFrameTicks(render_profile);
            // const uint32_t active_frame_ticks   = GetActiveFrameTicks(render_profile, frame_ticks);
            // const uint32_t frame_time           = (frame_ticks == 0) ? (uint32_t)(ticks_per_second / render_profile->m_FPS) : render_profile->m_IncludeFrameWait ? frame_ticks : active_frame_ticks;

            float tick_length = (float)(sample_frame_width) / frame_time_f;
            uint32_t num_samples = thread->m_Samples.Size();

            // for (TIndex sort_index = 0; sort_index < sample_aggregate_count; ++sort_index)
            // {
            //     TIndex index          = render_profile->m_SampleAggregateLookup.m_SortOrder[sort_index];
            //     SampleAggregate* sample_aggregate = &frame->m_SampleAggregates[index];
            //     TNameHash name_hash   = GetCombinedHash(sample_aggregate->m_ScopeNameHash, sample_aggregate->m_SampleNameHash);
            //     if (render_profile->m_IncludeFrameWait == 0)
            //     {
            //         if (name_hash == VSYNC_WAIT_NAME_HASH ||
            //             name_hash == ENGINE_FRAME_NAME_HASH)
            //         {
            //             continue;
            //         }
            //     }

            //     y -= LINE_SPACING;

            //     if (y < (samples_area.p.y + LINE_SPACING))
            //     {
            //         break;
            //     }

            //     double e = ((double)(sample_aggregate->m_Elapsed)) / ticks_per_second;

            //     uint32_t color_index = (name_hash >> 6) & 0x1f;
            //     HslToRgb2(color_index / 31.0f, 1.0f, 0.65f, col);

            //     params.m_WorldTransform.setElem(3, 1, y);
            //     params.m_FaceColor = Vector4(col[0], col[1], col[2], 1.0f);

            //     params.m_Text = buffer;

            //     TIndex* scope_index_ptr          = render_profile->m_ScopeLookup.m_HashLookup.Get(sample_aggregate->m_ScopeNameHash);
            //     Scope* scope                     = &frame->m_Scopes[*scope_index_ptr];
            //     const char* scope_name           = *render_profile->m_NameLookupTable.Get(scope->m_NameHash);
            //     const char* sample_name          = *render_profile->m_NameLookupTable.Get(sample_aggregate->m_SampleNameHash);

            //     int buffer_offset = dmSnPrintf(buffer, SAMPLE_FRAMES_NAME_LENGTH, "%s.", scope_name);

            //     // Do truncation of sample name if needed, we add some knowledge on the sample name format for names that
            //     // we know tend to be long. The script call scopes tend to be long and we try to truncate the file path of
            //     // that string removing the beginnig of the file path and keeping as much as possible of the end.
            //     while (buffer_offset <= SAMPLE_FRAMES_NAME_LENGTH)
            //     {
            //         if (*sample_name == 0)
            //         {
            //             break;
            //         }
            //         else if (*sample_name == '@')
            //         {
            //             buffer[buffer_offset++] = *sample_name++;
            //             if (buffer_offset == (SAMPLE_FRAMES_NAME_LENGTH + 1))
            //             {
            //                 break;
            //             }
            //             const uint32_t remaining_sample_name_length = (uint32_t)strlen(sample_name);
            //             const uint32_t remaining_buffer_length = (SAMPLE_FRAMES_NAME_LENGTH + 1) - buffer_offset;
            //             if (remaining_sample_name_length > remaining_buffer_length)
            //             {
            //                 sample_name += (remaining_sample_name_length - remaining_buffer_length);
            //             }
            //             continue;
            //         }
            //         buffer[buffer_offset++] = *sample_name++;
            //     }
            //     buffer[buffer_offset] = 0;

            //     params.m_WorldTransform.setElem(3, 0, name_x);
            //     dmRender::DrawText(render_context, font_map, 0, batch_key, params);

            //     dmSnPrintf(buffer, sizeof(buffer), "%6.3f", (float)(e * 1000.0));
            //     params.m_WorldTransform.setElem(3, 0, time_x);
            //     dmRender::DrawText(render_context, font_map, 0, batch_key, params);

            //     dmSnPrintf(buffer, sizeof(buffer), "%3u", sample_aggregate->m_Count);
            //     params.m_WorldTransform.setElem(3, 0, count_x);
            //     dmRender::DrawText(render_context, font_map, 0, batch_key, params);

            //     TIndex sample_index = sample_aggregate->m_LastSampleIndex;
            //     while (sample_index != max_sample_count)
            //     {
            //         Sample* sample = &frame->m_Samples[sample_index];

            //         float x = frames_x + sample->m_StartTick * tick_length;
            //         float w = sample->m_Elapsed * tick_length;
            //         if (w < 0.5f)
            //         {
            //             w = 0.5f;
            //         }

            //         dmRender::Square2d(render_context, x, y - CHARACTER_HEIGHT, x + w, y, Vector4(col[0], col[1], col[2], 1.0f));

            //         sample_index = sample->m_PreviousSampleIndex;
            //     }
            // }
        }
    }

    RenderProfile::RenderProfile(
            float fps,
            uint64_t ticks_per_second,
            uint64_t lifetime_in_milliseconds)
        : m_FPS(fps)
        , m_TicksPerSecond(ticks_per_second)
        , m_LifeTime((uint32_t)((lifetime_in_milliseconds * ticks_per_second) / 1000))
        , m_CurrentFrame(0)
        , m_ActiveFrame(0)
    {
    }

    void RenderProfile::Delete(RenderProfile* render_profile)
    {
        FlushRecording(render_profile, 0);
        DeleteProfilerFrame(render_profile->m_CurrentFrame);
        delete render_profile;
    }

    HRenderProfile NewRenderProfile(float fps)
    {
        const uint32_t LIFETIME_IN_MILLISECONDS = 6000u;
        return new RenderProfile(fps, dmProfile::GetTicksPerSecond(), LIFETIME_IN_MILLISECONDS);
    }

    static ProfilerThread* GetSelectedThread(HRenderProfile render_profile, ProfilerFrame* frame)
    {
        if (frame == 0 || frame->m_Threads.Empty())
            return &render_profile->m_EmptyThread;
        return frame->m_Threads[0];
    }

    void UpdateRenderProfile(HRenderProfile render_profile, const dmProfileRender::ProfilerFrame* current_frame)
    {
        if (render_profile->m_Mode == PROFILER_MODE_PAUSE)
        {
            return;
        }

        if (render_profile->m_CurrentFrame)
        {
            DeleteProfilerFrame(render_profile->m_CurrentFrame);
            render_profile->m_CurrentFrame = 0;
        }

        if (!current_frame)
            return;

        ProfilerThread* last_thread = GetSelectedThread(render_profile, render_profile->m_CurrentFrame);

        render_profile->m_CurrentFrame = DuplicateProfilerFrame(current_frame);

        ProfilerThread* current_thread = GetSelectedThread(render_profile, render_profile->m_CurrentFrame);
        if (last_thread->m_NameHash != current_thread->m_NameHash)
        {
            last_thread = 0;
            render_profile->m_MaxFrameTime = 0;
        }

        uint64_t last_frame_time = last_thread ? last_thread->m_SamplesTotalTime : 0;
        uint64_t this_frame_time = current_thread->m_SamplesTotalTime;

        bool new_peak_frame = render_profile->m_MaxFrameTime < this_frame_time;
        render_profile->m_MaxFrameTime = dmMath::Max(render_profile->m_MaxFrameTime, this_frame_time);

        if (render_profile->m_Mode == PROFILER_MODE_SHOW_PEAK_FRAME)
        {
            if (new_peak_frame)
            {
                ProfilerFrame* snapshot = DuplicateProfilerFrame(render_profile->m_CurrentFrame);

                FlushRecording(render_profile, 1);
                render_profile->m_RecordBuffer.SetSize(1);
                render_profile->m_RecordBuffer[0] = snapshot;
            }

            GotoRecordedFrame(render_profile, 0);
            return;
        }

        if (render_profile->m_Mode == PROFILER_MODE_RECORD)
        {
            ProfilerFrame* snapshot = DuplicateProfilerFrame(render_profile->m_CurrentFrame);

            if (render_profile->m_RecordBuffer.Remaining() == 0)
            {
                render_profile->m_RecordBuffer.SetCapacity(render_profile->m_RecordBuffer.Size() + (uint32_t)render_profile->m_FPS);
            }
            render_profile->m_RecordBuffer.Push(snapshot);
            render_profile->m_PlaybackFrame = (int32_t)render_profile->m_RecordBuffer.Size();
        }

        if (render_profile->m_ViewMode != PROFILER_VIEW_MODE_MINIMIZED)
        {
            //
        }
    }

    void SetMode(HRenderProfile render_profile, ProfilerMode mode)
    {
        if ((render_profile->m_Mode != PROFILER_MODE_RECORD) && (mode == PROFILER_MODE_RECORD))
        {
            FlushRecording(render_profile, (uint32_t)render_profile->m_FPS);
        }
        if ((render_profile->m_Mode == PROFILER_MODE_PAUSE) && (mode == PROFILER_MODE_RUN))
        {
            render_profile->m_PlaybackFrame = (int32_t)render_profile->m_RecordBuffer.Size();
        }
        render_profile->m_Mode = mode;
    }

    void SetViewMode(HRenderProfile render_profile, ProfilerViewMode view_mode)
    {
        render_profile->m_ViewMode = view_mode;
    }

    void SetWaitTime(HRenderProfile render_profile, bool include_wait_time)
    {
        render_profile->m_IncludeFrameWait = include_wait_time ? 1 : 0;
    }

    void AdjustShownFrame(HRenderProfile render_profile, int distance)
    {
        render_profile->m_Mode = PROFILER_MODE_PAUSE;

        GotoRecordedFrame(render_profile, render_profile->m_PlaybackFrame + distance);
    }

    void ShowRecordedFrame(HRenderProfile render_profile, int frame)
    {
        render_profile->m_Mode = PROFILER_MODE_PAUSE;

        GotoRecordedFrame(render_profile, frame);
    }

    int GetRecordedFrameCount(HRenderProfile render_profile)
    {
        return render_profile->m_RecordBuffer.Size();
    }

    void DeleteRenderProfile(HRenderProfile render_profile)
    {
        RenderProfile::Delete(render_profile);
    }

    void Draw(HRenderProfile render_profile, dmRender::HRenderContext render_context, dmRender::HFontMap font_map)
    {
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        const Size display_size(dmGraphics::GetWindowWidth(graphics_context), dmGraphics::GetWindowHeight(graphics_context));

        const DisplayMode display_mode = render_profile->m_ViewMode == PROFILER_VIEW_MODE_MINIMIZED ?
        DISPLAYMODE_MINIMIZED :
        (display_size.w > display_size.h ? DISPLAYMODE_LANDSCAPE : DISPLAYMODE_PORTRAIT);

        Draw(render_profile, render_context, font_map, display_size, display_mode);
    }

    static ProfilerThread* FindProfilerThread(ProfilerFrame* ctx, dmhash_t name_hash)
    {
        for (uint32_t i = 0; i < ctx->m_Threads.Size(); ++i)
        {
            if (name_hash == ctx->m_Threads[i]->m_NameHash)
            {
                return ctx->m_Threads[i];
            }
        }
        return 0;
    }

    ProfilerThread* FindOrCreateProfilerThread(ProfilerFrame* ctx, const char* name)
    {
        dmhash_t name_hash = dmHashString64(name);
        ProfilerThread* thread = FindProfilerThread(ctx, name_hash);
        if (thread != 0)
            return thread;

        thread = new ProfilerThread;
        thread->m_NameHash = name_hash;
        thread->m_Name = strdup(name);
        thread->m_Samples.SetCapacity(1); //TODO: Increase!!

        if (ctx->m_Threads.Full())
            ctx->m_Threads.OffsetCapacity(8);
        ctx->m_Threads.Push(thread);
        return thread;
    }

    void ClearProfilerThreadSamples(ProfilerThread* thread)
    {
        uint32_t num_samples = thread->m_Samples.Size();
        for (uint32_t i = 0; i < num_samples; ++i)
        {
            free((void*)thread->m_Samples[i].m_Name);
        }
        thread->m_Samples.SetSize(0);
    }

    static void DeleteProfilerThread(ProfilerThread* thread)
    {
        ClearProfilerThreadSamples(thread);
        free((void*)thread->m_Name);
        delete thread;
    }

    void DeleteProfilerFrame(ProfilerFrame* frame)
    {
        for (uint32_t i = 0; i < frame->m_Threads.Size(); ++i)
        {
            DeleteProfilerThread(frame->m_Threads[i]);
        }
        frame->m_Threads.SetSize(0);
        delete frame;
    }

    static ProfilerThread* DuplicateProfilerThread(ProfilerThread* thread)
    {
        ProfilerThread* cpy = new ProfilerThread;
        cpy->m_SamplesTotalTime = thread->m_SamplesTotalTime;
        cpy->m_NameHash = thread->m_NameHash;
        cpy->m_Name = strdup(thread->m_Name);

        uint32_t num_samples = thread->m_Samples.Size();
        cpy->m_Samples.SetCapacity(num_samples);

        for (uint32_t i = 0; i < num_samples; ++i)
        {
            ProfilerSample sample;
            sample = thread->m_Samples[i];
            sample.m_Name = strdup(thread->m_Samples[i].m_Name);

            cpy->m_Samples.Push(sample);
        }
        dmhash_t                    m_NameHash;
        const char*                 m_Name;
        dmArray<ProfilerSample>     m_Samples;

        return cpy;
    }

    static ProfilerFrame* DuplicateProfilerFrame(const ProfilerFrame* frame)
    {
        ProfilerFrame* cpy = new ProfilerFrame;
        cpy->m_Time = frame->m_Time;

        uint32_t num_threads = frame->m_Threads.Size();
        cpy->m_Threads.SetCapacity(num_threads);
        for (uint32_t i = 0; i < num_threads; ++i)
        {
            cpy->m_Threads.Push(DuplicateProfilerThread(frame->m_Threads[i]));
        }
        return cpy;
    }

    void PruneProfilerThreads(ProfilerFrame* frame, uint64_t time)
    {
        uint32_t num_threads = frame->m_Threads.Size();
        for (uint32_t i = 0; i < frame->m_Threads.Size(); )
        {
            ProfilerThread* thread = frame->m_Threads[i];
            if (thread->m_Time < time)
            {
                DeleteProfilerThread(thread);
                frame->m_Threads.EraseSwap(i);
            }
            else
            {
                ++i;
            }
        }
    }

} // namespace dmProfileRender
