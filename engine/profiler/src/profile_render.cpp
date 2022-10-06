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
 * The RenderProfile is created with a pre-defined number of sample/counters
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
    static const int INDENT_WIDTH = 2 * CHARACTER_WIDTH;

    static const int PORTRAIT_SAMPLE_FRAMES_NAME_LENGTH = 40;
    static const int LANDSCAPE_SAMPLE_FRAMES_NAME_LENGTH = 60;
    static const int PORTRAIT_SAMPLE_FRAMES_NAME_WIDTH  = PORTRAIT_SAMPLE_FRAMES_NAME_LENGTH * CHARACTER_WIDTH;
    static const int LANDSCAPE_SAMPLE_FRAMES_NAME_WIDTH  = LANDSCAPE_SAMPLE_FRAMES_NAME_LENGTH * CHARACTER_WIDTH;
    static const int SAMPLE_FRAMES_TIME_WIDTH  = 6 * CHARACTER_WIDTH;
    static const int SAMPLE_FRAMES_COUNT_WIDTH = 3 * CHARACTER_WIDTH;

    static const int COUNTERS_NAME_WIDTH  = 32 * CHARACTER_WIDTH;
    static const int COUNTERS_COUNT_WIDTH = 8 * CHARACTER_WIDTH;

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

    ProfilerThread::ProfilerThread()
    : m_NameHash(0)
    , m_Time(0)
    , m_SamplesTotalTime(0)
    {
    }

    // The RenderProfile contains the current "live" frame and
    // stats used to sort/purge sample items
    struct RenderProfile
    {
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
            ProfilerFrame* frame = render_profile->m_RecordBuffer[i];
            DeleteProfilerFrame(frame);
        }
        if (render_profile->m_RecordBuffer.Capacity() < capacity)
            render_profile->m_RecordBuffer.SetCapacity(capacity);
        render_profile->m_RecordBuffer.SetSize(0);
        render_profile->m_PlaybackFrame = -1;
    }

    //  float *r, *g, *b; /* red, green, blue in [0,1] */
    //  float h, s, l;    /* hue in [0,360]; saturation, light in [0,1] */
    static void hsl_to_rgb(float* r, float* g, float* b, float h, float s, float l)
    {
        float c      = (1.0f - dmMath::Abs(2.0f * l - 1.0f)) * s;
        float hp     = h / 60.0f;
        int hpi      = (int)hp;
        float hpmod2 = (hpi % 2) + (hp - hpi);
        float x      = c * (1.0f - dmMath::Abs(hpmod2 - 1.0f));
        switch (hpi)
        {
            case 0:
                *r = c;
                *g = x;
                *b = 0;
                break;
            case 1:
                *r = x;
                *g = c;
                *b = 0;
                break;
            case 2:
                *r = 0;
                *g = c;
                *b = x;
                break;
            case 3:
                *r = 0;
                *g = x;
                *b = c;
                break;
            case 4:
                *r = x;
                *g = 0;
                *b = c;
                break;
            case 5:
                *r = c;
                *g = 0;
                *b = x;
                break;
        }
        float m = l - 0.5f * c;
        *r += m;
        *g += m;
        *b += m;
    }

    static void HslToRgb2(float h, float s, float l, float* c)
    {
        hsl_to_rgb(&c[0], &c[1], &c[2], h * 360, s, l);
    }

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

    static Area GetPropertiesArea(DisplayMode display_mode, const Area& profiler_area, int counters_count)
    {
        Size s(COUNTERS_NAME_WIDTH + COUNTERS_COUNT_WIDTH, LINE_SPACING * counters_count);

        Position p(profiler_area.p.x + profiler_area.s.w - s.w, profiler_area.p.y + profiler_area.s.h  - s.h);
        return Area(p, s);
    }

    static Area GetSamplesArea(DisplayMode display_mode, const Area& details_area)
    {
        return details_area;
    }

    static Area GetSampleFramesArea(DisplayMode display_mode, int sample_frames_name_width, const Area& samples_area, const Area& properties_area)
    {
        const int offset_y = LINE_SPACING;
        const int offset_x = sample_frames_name_width + CHARACTER_WIDTH + SAMPLE_FRAMES_TIME_WIDTH + CHARACTER_WIDTH + SAMPLE_FRAMES_COUNT_WIDTH + CHARACTER_WIDTH;

        Position p(samples_area.p.x + offset_x, samples_area.p.y);
        Size s(samples_area.s.w - offset_x - properties_area.s.w - CHARACTER_WIDTH, samples_area.s.h - offset_y);

        return Area(p, s);
    }

    static void FillArea(dmRender::HRenderContext render_context, const Area& a, const Vector4& col)
    {
        dmRender::Square2d(render_context, a.p.x, a.p.y, a.p.x + a.s.w, a.p.y + a.s.h, col);
    }

    struct ThreadSortTimePred
    {
        ThreadSortTimePred() {}
        bool operator()(ProfilerThread* a, ProfilerThread* b) const
        {
            return a->m_SamplesTotalTime > b->m_SamplesTotalTime;
        }
    };


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

        const int SAMPLE_FRAMES_NAME_WIDTH = display_mode == DISPLAYMODE_LANDSCAPE ? LANDSCAPE_SAMPLE_FRAMES_NAME_WIDTH : PORTRAIT_SAMPLE_FRAMES_NAME_WIDTH;

        const Area profiler_area = GetProfilerArea(display_mode, display_size);
        FillArea(render_context, profiler_area, PROFILER_BG_COLOR);

        const Area header_area  = GetHeaderArea(display_mode, profiler_area);
        const Area details_area = GetDetailsArea(display_mode, profiler_area, header_area);

        ProfilerFrame* frame = render_profile->m_ActiveFrame ? render_profile->m_ActiveFrame : render_profile->m_CurrentFrame;
        std::sort(frame->m_Threads.Begin(), frame->m_Threads.End(), ThreadSortTimePred());

        const uint32_t properties_count  = frame->m_Properties.Size();
        const uint32_t extra_lines = 3; // a few extra lines for "Properties" word and offset in top

        const Area properties_area    = GetPropertiesArea(display_mode, profiler_area, properties_count + extra_lines);
        const Area samples_area       = GetSamplesArea(display_mode, details_area);
        const Area sample_frames_area = GetSampleFramesArea(display_mode, SAMPLE_FRAMES_NAME_WIDTH, samples_area, properties_area);

        char buffer[256];

        dmRender::DrawTextParams params;
        params.m_FaceColor   = TITLE_FACE_COLOR;
        params.m_ShadowColor = TITLE_SHADOW_COLOR;

        ProfilerThread* thread = GetSelectedThread(render_profile, frame);

        uint64_t ticks_per_second   = render_profile->m_TicksPerSecond;
        uint64_t max_frame_time     = render_profile->m_MaxFrameTime;
        uint64_t frame_time         = thread->m_SamplesTotalTime;
        uint32_t drawcalls          = 0;

        float frame_time_f = frame_time / (float)ticks_per_second;
        float max_frame_time_f = max_frame_time / (float)ticks_per_second;

        {
            // Properties
            int y = properties_area.p.y + properties_area.s.h - LINE_SPACING;

            int name_x  = properties_area.p.x;
            int count_x = name_x + COUNTERS_NAME_WIDTH + CHARACTER_WIDTH;

            params.m_FaceColor = TITLE_FACE_COLOR;
            params.m_Text      = "Properties:";
            params.m_WorldTransform.setElem(3, 0, name_x);
            params.m_WorldTransform.setElem(3, 1, y);
            dmRender::DrawText(render_context, font_map, 0, batch_key, params);

            for (uint32_t i = 0; i < frame->m_Properties.Size(); ++i)
            {
                const ProfilerProperty& property = frame->m_Properties[i];

                y -= LINE_SPACING;

                if (y < (properties_area.p.y + LINE_SPACING))
                {
                    break;
                }

                float col[3];
                uint32_t color_index = (property.m_NameHash >> 6) & 0x1f;
                HslToRgb2(color_index / 31.0f, 1.0f, 0.65f, col);

                if (property.m_Type == dmProfile::PROPERTY_TYPE_GROUP)
                    params.m_FaceColor = Vector4(col[0], col[1], col[2], 1.0f);
                else
                    params.m_FaceColor = Vector4(1.0f);

                float x = property.m_Indent * INDENT_WIDTH;
                params.m_WorldTransform.setElem(3, 0, name_x + x);
                params.m_WorldTransform.setElem(3, 1, y);

                const char* name = dmHashReverseSafe32(property.m_NameHash);
                if (strstr(name, "rmtp_") == name)
                {
                    name = name + 5;
                }
                params.m_Text = name;

                if (strstr(name, "DrawCalls") == name)
                {
                    drawcalls = property.m_Value.m_U32;
                }

                dmRender::DrawText(render_context, font_map, 0, batch_key, params);

                switch(property.m_Type)
                {
                case dmProfile::PROPERTY_TYPE_BOOL:  dmSnPrintf(buffer, sizeof(buffer), "%s", property.m_Value.m_Bool?"True":"False"); break;
                case dmProfile::PROPERTY_TYPE_S32:   dmSnPrintf(buffer, sizeof(buffer), "%d", property.m_Value.m_S32); break;
                case dmProfile::PROPERTY_TYPE_U32:   dmSnPrintf(buffer, sizeof(buffer), "%u", property.m_Value.m_U32); break;
                case dmProfile::PROPERTY_TYPE_F32:   dmSnPrintf(buffer, sizeof(buffer), "%f", property.m_Value.m_F32); break;
                case dmProfile::PROPERTY_TYPE_S64:   dmSnPrintf(buffer, sizeof(buffer), "%lld", (long long)property.m_Value.m_S64); break;
                case dmProfile::PROPERTY_TYPE_U64:   dmSnPrintf(buffer, sizeof(buffer), "%llx", (unsigned long long)property.m_Value.m_U64); break;
                case dmProfile::PROPERTY_TYPE_F64:   dmSnPrintf(buffer, sizeof(buffer), "%g", property.m_Value.m_F64); break;
                case dmProfile::PROPERTY_TYPE_GROUP: dmSnPrintf(buffer, sizeof(buffer), ""); break;
                default: break;
                }

                params.m_Text = buffer;
                params.m_WorldTransform.setElem(3, 0, count_x);
                dmRender::DrawText(render_context, font_map, 0, batch_key, params);
            }
        }

        int l = dmSnPrintf(buffer, sizeof(buffer), "Frame: %6.3f Max: %6.3f DrawCalls: %5u", frame_time_f, max_frame_time_f, drawcalls);

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

        FillArea(render_context, sample_frames_area, SAMPLES_BG_COLOR);

        {
            // Samples
            int y = samples_area.p.y + samples_area.s.h;

            params.m_WorldTransform.setElem(3, 1, y);

            int name_x   = samples_area.p.x;
            int time_x   = name_x + SAMPLE_FRAMES_NAME_WIDTH + CHARACTER_WIDTH;
            // tODO: Self time
            int count_x  = time_x + SAMPLE_FRAMES_TIME_WIDTH + CHARACTER_WIDTH;
            int frames_x = sample_frames_area.p.x;

            params.m_FaceColor = TITLE_FACE_COLOR;

            const char* name = dmHashReverseSafe32(thread->m_NameHash);
            dmSnPrintf(buffer, sizeof(buffer), "Thread: %s", name);
            params.m_Text = buffer;
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

            uint32_t num_samples = thread->m_Samples.Size();

            uint64_t frame_start = num_samples ? thread->m_Samples[0].m_StartTime : 0;
            for (uint32_t i = 0; i < num_samples; ++i)
            {
                ProfilerSample& sample = thread->m_Samples[i];
                sample.m_StartTime -= frame_start;
            }

            uint64_t frame_length = num_samples ? thread->m_Samples[0].m_Time : 1;

            for (uint32_t i = 0; i < num_samples; ++i)
            {
                const ProfilerSample& sample = thread->m_Samples[i];

                // There are so many samples and so little screen space, so we omit the smallest ones
                if (sample.m_Indent > 4)
                    continue;

                y -= LINE_SPACING;

                if (y < (samples_area.p.y + LINE_SPACING))
                {
                    break;
                }

                float t = (sample.m_Time * 1000) / (float)ticks_per_second; // in milliseconds

                float x = sample.m_Indent * INDENT_WIDTH;
                params.m_WorldTransform.setElem(3, 0, name_x + x);
                params.m_WorldTransform.setElem(3, 1, y);

                uint32_t color = sample.m_Color;
                float b = ((color >> 0 ) & 0xFF) / 255.0f;
                float g = ((color >> 8 ) & 0xFF) / 255.0f;
                float r = ((color >> 16) & 0xFF) / 255.0f;
                params.m_FaceColor = Vector4(r, g, b, 1.0f);

                const char* name = dmHashReverseSafe32(sample.m_NameHash);
                dmSnPrintf(buffer, sizeof(buffer), "%s", name);
                params.m_Text = buffer;
                dmRender::DrawText(render_context, font_map, 0, batch_key, params);

                dmSnPrintf(buffer, sizeof(buffer), "%6.3f", t);
                params.m_WorldTransform.setElem(3, 0, time_x);
                dmRender::DrawText(render_context, font_map, 0, batch_key, params);

                dmSnPrintf(buffer, sizeof(buffer), "%3u", sample.m_Count);
                params.m_WorldTransform.setElem(3, 0, count_x);
                dmRender::DrawText(render_context, font_map, 0, batch_key, params);

                float unit_start = sample.m_StartTime / (float)frame_length;
                float unit_length = sample.m_Time / (float)frame_length;
                x = frames_x + unit_start * sample_frame_width;
                float w = unit_length * sample_frame_width;

                if (w < 0.5f)
                {
                    w = 0.5f;
                }

                dmRender::Square2d(render_context, x, y - CHARACTER_HEIGHT, x + w, y, Vector4(r, g, b, 1.0f));

            }
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
        , m_Mode(PROFILER_MODE_RUN)
        , m_ViewMode(PROFILER_VIEW_MODE_FULL)
        , m_MaxFrameTime(0)
        , m_PlaybackFrame(0)
        , m_IncludeFrameWait(0)
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

        //uint64_t last_frame_time = last_thread ? last_thread->m_SamplesTotalTime : 0;
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
        assert(mode >= 0 && mode <= 4);
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

    ProfilerThread* FindOrCreateProfilerThread(ProfilerFrame* ctx, uint32_t name_hash)
    {
        ProfilerThread* thread = FindProfilerThread(ctx, name_hash);
        if (thread != 0)
            return thread;

        thread = new ProfilerThread;
        thread->m_NameHash = name_hash;
        thread->m_Samples.SetCapacity(1); //TODO: Increase!!

        if (ctx->m_Threads.Full())
            ctx->m_Threads.OffsetCapacity(8);
        ctx->m_Threads.Push(thread);
        return thread;
    }

    void ClearProfilerThreadSamples(ProfilerThread* thread)
    {
        thread->m_Samples.SetSize(0);
    }

    static void DeleteProfilerThread(ProfilerThread* thread)
    {
        ClearProfilerThreadSamples(thread);
        delete thread;
    }

    void DeleteProfilerFrame(ProfilerFrame* frame)
    {
        if (!frame)
            return;
        for (uint32_t i = 0; i < frame->m_Threads.Size(); ++i)
        {
            DeleteProfilerThread(frame->m_Threads[i]);
        }
        delete frame;
    }

    static ProfilerThread* DuplicateProfilerThread(ProfilerThread* thread)
    {
        ProfilerThread* cpy = new ProfilerThread;
        cpy->m_SamplesTotalTime = thread->m_SamplesTotalTime;
        cpy->m_NameHash = thread->m_NameHash;

        uint32_t num_samples = thread->m_Samples.Size();
        cpy->m_Samples.SetCapacity(num_samples);

        for (uint32_t i = 0; i < num_samples; ++i)
        {
            cpy->m_Samples.Push(thread->m_Samples[i]);
        }
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

        uint32_t num_properties = frame->m_Properties.Size();
        cpy->m_Properties.SetCapacity(num_properties);
        for (uint32_t i = 0; i < num_properties; ++i)
        {
            cpy->m_Properties.Push(frame->m_Properties[i]);
        }
        return cpy;
    }

    void PruneProfilerThreads(ProfilerFrame* frame, uint64_t time)
    {
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

    void AddProperty(ProfilerFrame* frame, uint32_t name_hash, dmProfile::PropertyType type, dmProfile::PropertyValue value, int indent)
    {
        ProfilerProperty prop;
        prop.m_NameHash = name_hash;
        prop.m_Value = value;
        prop.m_Type = type;
        prop.m_Indent = (uint8_t)indent;

        if (frame->m_Properties.Full())
            frame->m_Properties.OffsetCapacity(32);
        frame->m_Properties.Push(prop);
    }


} // namespace dmProfileRender
