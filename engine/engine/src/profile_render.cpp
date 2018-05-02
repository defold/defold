#include <string.h>

#include <algorithm>

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/math.h>
#include <dlib/profile.h>

#include "profile_render.h"

namespace dmProfileRender
{
    using namespace Vectormath::Aos;

    struct SampleStats
    {
        // Used to determine whether two samples are overlapping or not
        // It is also used to get the scope related to this sample
        const dmProfile::Sample* m_LastSample;
        uint32_t m_Elapsed;
        uint16_t m_Count;
    };

    bool CompareStats(const SampleStats& lhs, const SampleStats& rhs)
    {
        return lhs.m_Elapsed > rhs.m_Elapsed;
    }

    struct Context
    {
        uint32_t                    m_Y;
        uint32_t                    m_BarHeight;
        uint32_t                    m_Spacing;
        uint32_t                    m_Index;
        float                       m_TicksPerSecond;
        uint32_t                    m_FrameX;
        dmRender::HRenderContext    m_RenderContext;
        dmRender::HFontMap          m_FontMap;
        dmHashTable64<SampleStats>  m_SampleStats;
    };

//    float *r, *g, *b; /* red, green, blue in [0,1] */
  //  float h, s, l;    /* hue in [0,360]; saturation, light in [0,1] */
    void hsl_to_rgb(float* r, float* g, float* b, float h, float s, float l)
    {
        float c = (1.0f - dmMath::Abs(2.0f * l - 1.0f)) * s;
        float hp = h / 60.0f;
        int hpi = (int)hp;
        float hpmod2 = (hpi % 2) + (hp - hpi);
        float x = c * (1.0f - dmMath::Abs(hpmod2 - 1.0f));
        switch (hpi)
        {
            case 0: *r = c; *g = x; *b = 0; break;
            case 1: *r = x; *g = c; *b = 0; break;
            case 2: *r = 0; *g = c; *b = x; break;
            case 3: *r = 0; *g = x; *b = c; break;
            case 4: *r = x; *g = 0; *b = c; break;
            case 5: *r = c; *g = 0; *b = x; break;
        }
        float m = l - 0.5f * c;
        *r += m;
        *g += m;
        *b += m;
    }

    void HslToRgb2(float h, float s, float l, float* c)
    {
        hsl_to_rgb(&c[0], &c[1], &c[2], h * 360, s, l);
    }

    const int g_Scope_x0 = 16;
    const int g_Scope_Time_x0 = 16 + 120;
    const int g_Scope_Count_x0 = 16 + 180;
    const int g_Sample_x0 = 250;
    const int g_Sample_Time_x0 = g_Sample_x0 + 260;
    const int g_Sample_Count_x0 = g_Sample_Time_x0 + 60;
    const int g_Frame_x0 = g_Sample_Count_x0 + 65;
    const int g_Spacing = 20;
    const int g_Counter_x0 = 16;
    const int g_Counter_Amount_x0 = 16 + 200;


    void ProfileSampleCallback(void* context, const dmProfile::Sample* sample)
    {
        Context* c = (Context*) context;

        float y = c->m_Y - sample->m_Scope->m_Index * (c->m_Spacing + c->m_BarHeight);

        uint32_t frame_width = dmGraphics::GetWindowWidth(dmRender::GetGraphicsContext(c->m_RenderContext)) - c->m_FrameX;
        float frames_per_tick = 60.0f / c->m_TicksPerSecond;
        float x = c->m_FrameX + frame_width * sample->m_Start * frames_per_tick;
        float w = frame_width * sample->m_Elapsed * frames_per_tick;

        float col[3];
        HslToRgb2( (sample->m_Scope->m_Index % 16) / 16.0f, 1.0f, 0.65f, col);

        dmRender::Square2d(c->m_RenderContext, x, y, x + w, y + c->m_BarHeight, Vector4(col[0], col[1], col[2], 1));

        HashState64 hash_state;
        dmHashInit64(&hash_state, false);
        dmHashUpdateBuffer64(&hash_state, sample->m_Scope->m_Name, strlen(sample->m_Scope->m_Name));
        dmHashUpdateBuffer64(&hash_state, sample->m_Name, strlen(sample->m_Name));
        uint64_t hash = dmHashFinal64(&hash_state);

        SampleStats* stats = c->m_SampleStats.Get(hash);
        if (stats != 0x0)
        {

            const dmProfile::Sample* last_sample = stats->m_LastSample;
            uint32_t end_last = last_sample->m_Start + last_sample->m_Elapsed;
            if (sample->m_Start >= last_sample->m_Start && sample->m_Start < end_last)
            {
                // Probably recursion. The sample is overlapping the previous.
                // Ignore this sample.
            }
            else
            {
                ++stats->m_Count;
                stats->m_Elapsed += sample->m_Elapsed;
                stats->m_LastSample = sample;
            }
        }
        else
        {
            SampleStats stats;
            stats.m_LastSample = sample;
            stats.m_Elapsed = sample->m_Elapsed;
            stats.m_Count = 1;
            if (c->m_SampleStats.Full())
            {
                c->m_SampleStats.SetCapacity(64, c->m_SampleStats.Size()*2);
            }

            c->m_SampleStats.Put(hash, stats);
        }
    }

    void ProfileScopeCallback(void* context, const dmProfile::ScopeData* scope_data)
    {
        Context* c = (Context*) context;
        dmProfile::Scope* scope = scope_data->m_Scope;

        int y = c->m_Y - c->m_Index * g_Spacing;

        float col[3];
        HslToRgb2( (scope->m_Index % 16) / 16.0f, 1.0f, 0.65f, col);

        float e = scope_data->m_Elapsed / c->m_TicksPerSecond;

        char buf[256];

        dmRender::DrawTextParams params;
        params.m_Text = buf;
        params.m_WorldTransform.setElem(3, 1, y);
        params.m_FaceColor = Vectormath::Aos::Vector4(col[0], col[1], col[2], 1.0f);
        params.m_ShadowColor = Vectormath::Aos::Vector4(0.0f, 0.0f, 0.0f, 1.0f);

        DM_SNPRINTF(buf, sizeof(buf), "%s", scope->m_Name);
        params.m_WorldTransform.setElem(3, 0, g_Scope_x0);
        dmRender::DrawText(c->m_RenderContext, c->m_FontMap, 0, 0, params);

        DM_SNPRINTF(buf, sizeof(buf), "%.1f", e * 1000);
        params.m_WorldTransform.setElem(3, 0, g_Scope_Time_x0);
        dmRender::DrawText(c->m_RenderContext, c->m_FontMap, 0, 0, params);

        DM_SNPRINTF(buf, sizeof(buf), "%d", scope_data->m_Count);
        params.m_WorldTransform.setElem(3, 0, g_Scope_Count_x0);
        dmRender::DrawText(c->m_RenderContext, c->m_FontMap, 0, 0, params);

        c->m_Index++;
    }

    void ProfileCounterCallback(void* context, const dmProfile::CounterData* counter_data)
    {
        if (counter_data->m_Value == 0)
            return;

        Context* c = (Context*) context;

        int y = c->m_Y - c->m_Index * g_Spacing;

        float col[3];
        HslToRgb2( 4/ 16.0f, 1.0f, 0.65f, col);


        char buf[256];

        dmRender::DrawTextParams params;
        params.m_Text = buf;
        params.m_WorldTransform.setElem(3, 1, y);
        params.m_FaceColor = Vectormath::Aos::Vector4(col[0], col[1], col[2], 1.0f);
        params.m_ShadowColor = Vectormath::Aos::Vector4(0.0f, 0.0f, 0.0f, 1.0f);

        DM_SNPRINTF(buf, sizeof(buf), "%s", counter_data->m_Counter->m_Name);
        params.m_WorldTransform.setElem(3, 0, g_Counter_x0);
        dmRender::DrawText(c->m_RenderContext, c->m_FontMap, 0, 0, params);

        DM_SNPRINTF(buf, sizeof(buf), "%u", counter_data->m_Value);
        params.m_WorldTransform.setElem(3, 0, g_Counter_Amount_x0);
        dmRender::DrawText(c->m_RenderContext, c->m_FontMap, 0, 0, params);

        c->m_Index++;
    }

    void ProfileSampleStatsCallback(Context* context, const uint64_t* hash, SampleStats* stats)
    {
        float e = stats->m_Elapsed / context->m_TicksPerSecond;
        if (e < 0.0001f)
            return;
        int y = context->m_Y - context->m_Index * g_Spacing;

        float col[3];
        HslToRgb2( (stats->m_LastSample->m_Scope->m_Index % 16) / 16.0f, 1.0f, 0.65f, col);

        char buf[256];

        dmRender::DrawTextParams params;
        params.m_Text = buf;
        params.m_WorldTransform.setElem(3, 1, y);
        params.m_FaceColor = Vectormath::Aos::Vector4(col[0], col[1], col[2], 1.0f);
        params.m_ShadowColor = Vectormath::Aos::Vector4(0.0f, 0.0f, 0.0f, 1.0f);

        DM_SNPRINTF(buf, sizeof(buf), "%s.%s", stats->m_LastSample->m_Scope->m_Name, stats->m_LastSample->m_Name);
        params.m_WorldTransform.setElem(3, 0, g_Sample_x0);
        dmRender::DrawText(context->m_RenderContext, context->m_FontMap, 0, 0, params);

        DM_SNPRINTF(buf, sizeof(buf), "%.1f", e * 1000.0f);
        params.m_WorldTransform.setElem(3, 0, g_Sample_Time_x0);
        dmRender::DrawText(context->m_RenderContext, context->m_FontMap, 0, 0, params);

        DM_SNPRINTF(buf, sizeof(buf), "%d", stats->m_Count);
        params.m_WorldTransform.setElem(3, 0, g_Sample_Count_x0);
        dmRender::DrawText(context->m_RenderContext, context->m_FontMap, 0, 0, params);

        context->m_Index++;
    }

    void Draw(dmProfile::HProfile profile, dmRender::HRenderContext render_context, dmRender::HFontMap font_map)
    {
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        uint32_t display_width = dmGraphics::GetWindowWidth(graphics_context);
        uint32_t display_height = dmGraphics::GetWindowHeight(graphics_context);
        dmRender::Square2d(render_context, 0.0f, 0.0f, display_width, display_height, Vector4(0.1f, 0.1f, 0.1f, 0.4f));

        int y0 = display_height - g_Spacing;

        char buffer[256];

        dmRender::DrawTextParams params;
        params.m_Text = buffer;
        params.m_WorldTransform.setElem(3, 1, y0);
        params.m_FaceColor = Vectormath::Aos::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        params.m_ShadowColor = Vectormath::Aos::Vector4(0.0f, 0.0f, 0.0f, 1.0f);

        bool profile_valid = true;
        if (dmProfile::IsOutOfScopes())
        {
            profile_valid = false;
            params.m_WorldTransform.setElem(3, 0, g_Scope_x0);
            params.m_WorldTransform.setElem(3, 1, y0);
            params.m_Text = "Out of scopes!";
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            y0 -= g_Spacing;
        }
        if (dmProfile::IsOutOfSamples())
        {
            profile_valid = false;
            params.m_WorldTransform.setElem(3, 0, g_Scope_x0);
            params.m_WorldTransform.setElem(3, 1, y0);
            params.m_Text = "Out of samples!";
            dmRender::DrawText(render_context, font_map, 0, 0, params);
        }
        if (profile_valid)
        {
            DM_SNPRINTF(buffer, 256, "Frame: %.3f Max: %.3f", dmProfile::GetFrameTime(), dmProfile::GetMaxFrameTime());
            params.m_WorldTransform.setElem(3, 0, g_Scope_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);

            y0 -= g_Spacing;
            dmRender::Square2d(render_context, g_Frame_x0, y0, display_width, display_height, Vector4(0.1f, 0.1f, 0.15f, 0.4f));

            params.m_WorldTransform.setElem(3, 1, y0);

            params.m_Text = "Scopes:";
            params.m_WorldTransform.setElem(3, 0, g_Scope_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "ms";
            params.m_WorldTransform.setElem(3, 0, g_Scope_Time_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "#";
            params.m_WorldTransform.setElem(3, 0, g_Scope_Count_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "Samples:";
            params.m_WorldTransform.setElem(3, 0, g_Sample_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "ms";
            params.m_WorldTransform.setElem(3, 0, g_Sample_Time_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "#";
            params.m_WorldTransform.setElem(3, 0, g_Sample_Count_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "Frame:";
            params.m_WorldTransform.setElem(3, 0, g_Frame_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);

            Context ctx;
            ctx.m_Y = y0 - g_Spacing;
            ctx.m_BarHeight = 16.0f;
            ctx.m_Spacing = 4.0f;
            ctx.m_Index = 0;
            ctx.m_TicksPerSecond = dmProfile::GetTicksPerSecond();
            ctx.m_FrameX = g_Frame_x0;
            ctx.m_RenderContext = render_context;
            ctx.m_FontMap = font_map;
            ctx.m_SampleStats.SetCapacity(64, 256);

            dmProfile::IterateSamples(profile, &ctx, &ProfileSampleCallback);

            ctx.m_Index = 0;
            dmProfile::IterateScopeData(profile, &ctx, &ProfileScopeCallback);

            ctx.m_Index = 0;
            if (ctx.m_SampleStats.Size() > 0)
            {
                ctx.m_Y = y0 - g_Spacing;
                ctx.m_SampleStats.Iterate<Context>(ProfileSampleStatsCallback, &ctx);
            }

        }
        {
            y0 = 500;

            params.m_WorldTransform.setElem(3, 1, y0);

            params.m_Text = "Counters:";
            params.m_WorldTransform.setElem(3, 0, g_Counter_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "#";
            params.m_WorldTransform.setElem(3, 0, g_Counter_Amount_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);

            Context ctx;
            ctx.m_Y = y0 - g_Spacing;
            ctx.m_BarHeight = 16.0f;
            ctx.m_Spacing = 4.0f;
            ctx.m_Index = 0;
            ctx.m_TicksPerSecond = dmProfile::GetTicksPerSecond();
            ctx.m_RenderContext = render_context;
            ctx.m_FontMap = font_map;

            dmProfile::IterateCounterData(profile, &ctx, &ProfileCounterCallback);
        }
    }
}
