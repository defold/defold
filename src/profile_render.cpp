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
    struct SampleStats
    {
        const dmProfile::Sample* m_Sample;
        uint32_t m_Elapsed;
        uint16_t m_Count;
    };

    bool CompareStats(const SampleStats& lhs, const SampleStats& rhs)
    {
        return lhs.m_Elapsed > rhs.m_Elapsed;
    }

    struct Context
    {
        float                       m_Y;
        uint32_t                    m_TextY;
        float                       m_Barheight;
        float                       m_Spacing;
        uint32_t                    m_Index;
        float                       m_TicksPerSecond;
        float                       m_FrameX;
        dmRender::HRenderContext    m_RenderContext;
        dmRender::HFont             m_Font;
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
    const int g_TextSpacing = 20;
    const int g_Counter_x0 = 16;
    const int g_Counter_Amount_x0 = 16 + 160;


    void ProfileSampleCallback(void* context, const dmProfile::Sample* sample)
    {
        Context* c = (Context*) context;
        Matrix4 m = Matrix4::orthographic( -1, 1, 1, -1, 10, -10 );

        const float scale = 30.0f;

        //float y = c->m_Y + sample->m_Depth * c->m_Spacing;
        float y = c->m_Y + sample->m_Scope->m_Index * c->m_Spacing;

        float x = c->m_FrameX + (1.0f * scale * sample->m_Start) / c->m_TicksPerSecond;
        float w = (1.0f * scale * sample->m_Elapsed) / c->m_TicksPerSecond;

        float col[3];
        HslToRgb2( (sample->m_Scope->m_Index % 16) / 16.0f, 1.0f, 0.65f, col);

        dmRender::Square2d(c->m_RenderContext, x, y, x + w, y + c->m_Barheight, Vector4(col[0], col[1], col[2], 1));

        uint64_t hash = dmHashString64(sample->m_Name);
        SampleStats* stats = c->m_SampleStats.Get(hash);
        if (stats != 0x0)
        {
            ++stats->m_Count;
            stats->m_Elapsed += sample->m_Elapsed;
        }
        else
        {
            SampleStats stats;
            stats.m_Sample = sample;
            stats.m_Elapsed = sample->m_Elapsed;
            stats.m_Count = 1;
            if (c->m_SampleStats.Full())
            {
                c->m_SampleStats.SetCapacity(64, c->m_SampleStats.Size()*2);
            }

            c->m_SampleStats.Put(hash, stats);
        }
    }

    void ProfileScopeCallback(void* context, const dmProfile::Scope* scope)
    {
        Context* c = (Context*) context;
        Matrix4 m = Matrix4::orthographic( -1, 1, 1, -1, 10, -10 );

        int y = c->m_TextY + c->m_Index * g_TextSpacing;

        float col[3];
        HslToRgb2( (scope->m_Index % 16) / 16.0f, 1.0f, 0.65f, col);

        float e = scope->m_Elapsed / c->m_TicksPerSecond;

        char buf[256];

        dmRender::DrawTextParams params;
        params.m_Text = buf;
        params.m_Y = y;
        params.m_FaceColor = Vectormath::Aos::Vector4(col[0], col[1], col[2], 1.0f);
        params.m_ShadowColor = Vectormath::Aos::Vector4(0.0f, 0.0f, 0.0f, 1.0f);

        DM_SNPRINTF(buf, sizeof(buf), "%s", scope->m_Name);
        params.m_X = g_Scope_x0;
        dmRender::DrawText(c->m_RenderContext, c->m_Font, params);

        DM_SNPRINTF(buf, sizeof(buf), "%.1f", e * 1000);
        params.m_X = g_Scope_Time_x0;
        dmRender::DrawText(c->m_RenderContext, c->m_Font, params);

        DM_SNPRINTF(buf, sizeof(buf), "%d", scope->m_Count);
        params.m_X = g_Scope_Count_x0;
        dmRender::DrawText(c->m_RenderContext, c->m_Font, params);

        c->m_Index++;
    }

    void ProfileCounterCallback(void* context, const dmProfile::Counter* counter)
    {
        if (counter->m_Counter == 0)
            return;

        Context* c = (Context*) context;
        Matrix4 m = Matrix4::orthographic( -1, 1, 1, -1, 10, -10 );

        int y = c->m_TextY + c->m_Index * g_TextSpacing;

        float col[3];
        HslToRgb2( 4/ 16.0f, 1.0f, 0.65f, col);


        char buf[256];

        dmRender::DrawTextParams params;
        params.m_Text = buf;
        params.m_Y = y;
        params.m_FaceColor = Vectormath::Aos::Vector4(col[0], col[1], col[2], 1.0f);
        params.m_ShadowColor = Vectormath::Aos::Vector4(0.0f, 0.0f, 0.0f, 1.0f);

        DM_SNPRINTF(buf, sizeof(buf), "%s", counter->m_Name);
        params.m_X = g_Counter_x0;
        dmRender::DrawText(c->m_RenderContext, c->m_Font, params);

        DM_SNPRINTF(buf, sizeof(buf), "%u", counter->m_Counter);
        params.m_X = g_Counter_Amount_x0;
        dmRender::DrawText(c->m_RenderContext, c->m_Font, params);

        c->m_Index++;
    }

    void ProfileSampleStatsCallback(Context* context, const uint64_t* hash, SampleStats* stats)
    {
        float e = stats->m_Elapsed / context->m_TicksPerSecond;
        if (e < 0.0001f)
            return;
        int text_y = context->m_TextY + context->m_Index * g_TextSpacing;

        float col[3];
        HslToRgb2( (stats->m_Sample->m_Scope->m_Index % 16) / 16.0f, 1.0f, 0.65f, col);

        char buf[256];

        dmRender::DrawTextParams params;
        params.m_Text = buf;
        params.m_Y = text_y;
        params.m_FaceColor = Vectormath::Aos::Vector4(col[0], col[1], col[2], 1.0f);
        params.m_ShadowColor = Vectormath::Aos::Vector4(0.0f, 0.0f, 0.0f, 1.0f);

        DM_SNPRINTF(buf, sizeof(buf), "%s.%s", stats->m_Sample->m_Scope->m_Name, stats->m_Sample->m_Name);
        params.m_X = g_Sample_x0;
        dmRender::DrawText(context->m_RenderContext, context->m_Font, params);

        DM_SNPRINTF(buf, sizeof(buf), "%.1f", e * 1000.0f);
        params.m_X = g_Sample_Time_x0;
        dmRender::DrawText(context->m_RenderContext, context->m_Font, params);

        DM_SNPRINTF(buf, sizeof(buf), "%d", stats->m_Count);
        params.m_X = g_Sample_Count_x0;
        dmRender::DrawText(context->m_RenderContext, context->m_Font, params);

        context->m_Index++;
    }

    void Draw(dmRender::HRenderContext render_context, dmRender::HFont font)
    {
        Matrix4 m = Matrix4::orthographic(-1, 1, 1, -1, 1, -1);

        dmGraphics::HContext context = dmGraphics::GetContext();

        dmGraphics::SetBlendFunc(context, dmGraphics::BLEND_FACTOR_SRC_ALPHA, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        dmGraphics::DisableState(context, dmGraphics::DEPTH_TEST);
        dmGraphics::EnableState(context, dmGraphics::BLEND);

        dmRender::Square2d(render_context, -1.0f, -1.0f, 1.0f, 1.0f, Vector4(0.1f, 0.1f, 0.1f, 0.4f));

        int text_y0 = 50 - g_TextSpacing;

        char buffer[256];

        dmRender::DrawTextParams params;
        params.m_Text = buffer;
        params.m_Y = text_y0;
        params.m_FaceColor = Vectormath::Aos::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        params.m_ShadowColor = Vectormath::Aos::Vector4(0.0f, 0.0f, 0.0f, 1.0f);

        bool profile_valid = true;
        if (dmProfile::IsOutOfScopes())
        {
            profile_valid = false;
            params.m_X = g_Scope_x0;
            params.m_Y = text_y0;
            params.m_Text = "Out of scopes!";
            dmRender::DrawText(render_context, font, params);
            text_y0 += g_TextSpacing;
        }
        if (dmProfile::IsOutOfSamples())
        {
            profile_valid = false;
            params.m_X = g_Scope_x0;
            params.m_Y = text_y0;
            params.m_Text = "Out of samples!";
            dmRender::DrawText(render_context, font, params);
        }
        if (profile_valid)
        {
            DM_SNPRINTF(buffer, 256, "Frame: %.3f Max: %.3f", dmProfile::GetFrameTime(), dmProfile::GetMaxFrameTime());
            params.m_X = g_Scope_x0;
            dmRender::DrawText(render_context, font, params);

            text_y0 += g_TextSpacing;
            float frame_x0 = 2.0f * g_Frame_x0 / (float)dmRender::GetDisplayWidth(render_context) - 1.0f;
            dmRender::Square2d(render_context, frame_x0, -0.85f, 1.0f, 0.15f, Vector4(0.1f, 0.1f, 0.15f, 0.4f));

            params.m_Y = text_y0;

            params.m_Text = "Scopes:";
            params.m_X = g_Scope_x0;
            dmRender::DrawText(render_context, font, params);
            params.m_Text = "ms";
            params.m_X = g_Scope_Time_x0;
            dmRender::DrawText(render_context, font, params);
            params.m_Text = "#";
            params.m_X = g_Scope_Count_x0;
            dmRender::DrawText(render_context, font, params);
            params.m_Text = "Samples:";
            params.m_X = g_Sample_x0;
            dmRender::DrawText(render_context, font, params);
            params.m_Text = "ms";
            params.m_X = g_Sample_Time_x0;
            dmRender::DrawText(render_context, font, params);
            params.m_Text = "#";
            params.m_X = g_Sample_Count_x0;
            dmRender::DrawText(render_context, font, params);
            params.m_Text = "Frame:";
            params.m_X = g_Frame_x0;
            dmRender::DrawText(render_context, font, params);

            Context ctx;
            ctx.m_Y = -0.78f;
            ctx.m_TextY = text_y0 + g_TextSpacing;
            ctx.m_Barheight = 0.05f;
            ctx.m_Spacing = 0.074f;
            ctx.m_Index = 0;
            ctx.m_TicksPerSecond = dmProfile::GetTicksPerSecond();
            ctx.m_FrameX = frame_x0;
            ctx.m_RenderContext = render_context;
            ctx.m_Font = font;
            ctx.m_SampleStats.SetCapacity(64, 256);

            dmProfile::IterateSamples(&ctx, &ProfileSampleCallback);

            ctx.m_Index = 0;
            dmProfile::IterateScopes(&ctx, &ProfileScopeCallback);

            ctx.m_Index = 0;
            if (ctx.m_SampleStats.Size() > 0)
            {
                ctx.m_TextY = text_y0 + g_TextSpacing;
                ctx.m_SampleStats.Iterate<Context>(ProfileSampleStatsCallback, &ctx);
            }

        }
        {
            text_y0 = 300;

            params.m_Y = text_y0;

            params.m_Text = "Counters:";
            params.m_X = g_Counter_x0;
            dmRender::DrawText(render_context, font, params);
            params.m_Text = "#";
            params.m_X = g_Counter_Amount_x0;
            dmRender::DrawText(render_context, font, params);

            Context ctx;
            ctx.m_Y = -0.78f;
            ctx.m_TextY = text_y0 + g_TextSpacing;
            ctx.m_Barheight = 0.05f;
            ctx.m_Spacing = 0.074f;
            ctx.m_Index = 0;
            ctx.m_TicksPerSecond = dmProfile::GetTicksPerSecond();
            ctx.m_RenderContext = render_context;
            ctx.m_Font = font;

            dmProfile::IterateCounters(&ctx, &ProfileCounterCallback);
        }
    }
}
