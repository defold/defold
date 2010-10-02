#include <string.h>
#include <dlib/dstrings.h>
#include "profile_render.h"
#include <render_debug/debugrenderer.h>
#include <dlib/profile.h>

namespace dmProfileRender
{
    struct Context
    {
        float    m_Y;
        uint32_t m_TextY;
        float    m_Barheight;
        float    m_Spacing;
        int32_t  m_Index;
        int32_t  m_SampleIndex;
        float    m_TicksPerSecond;
        float    m_FrameX;
        dmRender::HFontRenderer m_FontRenderer;
    };

//    float *r, *g, *b; /* red, green, blue in [0,1] */
  //  float h, s, v;    /* hue in [0,360]; saturation, value in [0,1] */
    void hsv_to_rgb(float* r, float* g, float* b, float h, float s, float v)
    {
      int i;
      float f, p, q, t;

      if(s==0.0) { /* achromatic, hue meaningless */
        (*r) = (*g) = (*b) = v;
      } else {
        if(h==360.0) h=0.0;    /* this is a circle */
        h /= 60.0;             /* h is mostly in one of 6 main hues */
        i=(int)floor(h);       /* i<=h, i is which hue of 0,1,2,3,4,5 */
        f=h-i;                 /* fractional part of h */
        p=v*(1.0-s);           /* purity of this hue */
        q=v*(1.0-(s*f));       /* farness from this hue */
        t=v*(1.0-(s*(1.0-f))); /* closeness to this hue */

        switch(i) {
          case 0: *r=v; *g=t; *b=p; break; /* mostly red */
          case 1: *r=q; *g=v; *b=p; break; /* mostly yellow */
          case 2: *r=p; *g=v; *b=t; break; /* mostly green */
          case 3: *r=p; *g=q; *b=v; break; /* mostly cyan */
          case 4: *r=t; *g=p; *b=v; break; /* mostly blue */
          case 5: *r=v; *g=p; *b=q; break; /* mostly magenta */
        }
      }
    }

    void HsvToRgb2(float h, float s, float v, float* c)
    {
        hsv_to_rgb(&c[0], &c[1], &c[2], h * 360, s, v);
    }

    const int g_Scope_x0 = 16;
    const int g_Scope_Time_x0 = 16 + 120;
    const int g_Scope_Count_x0 = 16 + 180;
    const int g_Sample_x0 = 250;
    const int g_Sample_Time_x0 = g_Sample_x0 + 260;
    const int g_Frame_x0 = g_Sample_Time_x0 + 65;
    const int g_TextSpacing = 20;

    void ProfileSampleCallback(void* context, const dmProfile::Sample* sample)
    {
        Context* c = (Context*) context;
        Matrix4 m = Matrix4::orthographic( -1, 1, 1, -1, 10, -10 );

        const float freq = 60.0f;

        //float y = c->m_Y + sample->m_Depth * c->m_Spacing;
        float y = c->m_Y + sample->m_Scope->m_Index * c->m_Spacing;

        float x = c->m_FrameX + (1.0f * freq * sample->m_Start) / c->m_TicksPerSecond;
        float w = (1.0f * freq * sample->m_Elapsed) / c->m_TicksPerSecond;

        float col[3];
        HsvToRgb2( (sample->m_Scope->m_Index % 16) / 16.0f, 0.99f, 0.99f, col);

        dmRenderDebug::Square(m, Point3(x + w/2, y + c->m_Barheight * 0.5f, 0),
                                 Vector3(w, c->m_Barheight, 0.0f),
                                 Vector4(col[0], col[1], col[2], 1));

        float e = sample->m_Elapsed / c->m_TicksPerSecond;
        if (e > 0.0002f)
        {
            int text_y = c->m_TextY + c->m_SampleIndex * g_TextSpacing;

            char buf[256];
            DM_SNPRINTF(buf, sizeof(buf), "%s.%s", sample->m_Scope->m_Name, sample->m_Name);
            dmRender::FontRendererDrawString(c->m_FontRenderer, buf, g_Sample_x0, text_y, col[0], col[1], col[2], 1);

            DM_SNPRINTF(buf, sizeof(buf), "%.1f", e * 1000.0f);
            dmRender::FontRendererDrawString(c->m_FontRenderer, buf, g_Sample_Time_x0, text_y, col[0], col[1], col[2], 1);

            c->m_SampleIndex++;
        }
        c->m_Index++;
    }

    void ProfileScopeCallback(void* context, const dmProfile::Scope* scope)
    {
        Context* c = (Context*) context;
        Matrix4 m = Matrix4::orthographic( -1, 1, 1, -1, 10, -10 );

        int y = c->m_TextY + c->m_Index * g_TextSpacing;

        float col[3];
        HsvToRgb2( (scope->m_Index % 16) / 16.0f, 0.8f, 0.90f, col);

        float e = scope->m_Elapsed / c->m_TicksPerSecond;

        //if (e > 0.0001f)
        {
            char buf[256];
            DM_SNPRINTF(buf, sizeof(buf), "%s", scope->m_Name);
            dmRender::FontRendererDrawString(c->m_FontRenderer, buf, g_Scope_x0, y, col[0], col[1], col[2], 1);

            DM_SNPRINTF(buf, sizeof(buf), "%.1f", e * 1000);
            dmRender::FontRendererDrawString(c->m_FontRenderer, buf, g_Scope_Time_x0, y, col[0], col[1], col[2], 1);

            DM_SNPRINTF(buf, sizeof(buf), "%d", scope->m_Samples);
            dmRender::FontRendererDrawString(c->m_FontRenderer, buf, g_Scope_Count_x0, y, col[0], col[1], col[2], 1);

            c->m_Index++;
        }
    }

    void Draw(dmRender::HFontRenderer font_renderer, uint32_t width, uint32_t height)
    {
        Matrix4 m = Matrix4::orthographic( -1, 1, 1, -1, 10, -10 );

        dmGraphics::HContext context = dmGraphics::GetContext();

        dmGraphics::SetBlendFunc(context, dmGraphics::BLEND_FACTOR_SRC_ALPHA, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        dmGraphics::DisableState(context, dmGraphics::DEPTH_TEST);
        dmGraphics::EnableState(context, dmGraphics::BLEND);

        dmRenderDebug::Square(m, Point3(0.0f,  0.0f, 0),
                                 Vector3(2.0f, 2.0f, 0),
                                 Vector4(0.1f, 0.1f, 0.1f, 0.4f));

        float frame_x0 = 2.0f * g_Frame_x0 / (float)width - 1.0f;
        dmRenderDebug::Square(m, Point3(frame_x0 + 0.5f * (1.0f - frame_x0), -0.35f, 0),
                                 Vector3(1.0f - frame_x0, 1.0f, 0),
                                 Vector4(0.1f, 0.1f, 0.15f, 0.4f));

        const int text_y0 = 50;
        dmRender::FontRendererDrawString(font_renderer, "Scopes:", g_Scope_x0, text_y0, 1, 1, 1, 1);
        dmRender::FontRendererDrawString(font_renderer, "ms", g_Scope_Time_x0, text_y0, 1, 1, 1, 1);
        dmRender::FontRendererDrawString(font_renderer, "#", g_Scope_Count_x0, text_y0, 1, 1, 1, 1);
        dmRender::FontRendererDrawString(font_renderer, "Samples:", g_Sample_x0, text_y0, 1, 1, 1, 1);
        dmRender::FontRendererDrawString(font_renderer, "ms", g_Sample_Time_x0, text_y0, 1, 1, 1, 1);
        dmRender::FontRendererDrawString(font_renderer, "Frame:", g_Frame_x0, text_y0, 1, 1, 1, 1);

        Context ctx;
        ctx.m_Y = -0.78f;
        ctx.m_TextY = text_y0 + g_TextSpacing;
        ctx.m_Barheight = 0.05f;
        ctx.m_Spacing = 0.074f;
        ctx.m_Index = 0;
        ctx.m_SampleIndex = 0;
        ctx.m_TicksPerSecond = dmProfile::GetTicksPerSecond();
        ctx.m_FrameX = frame_x0;
        ctx.m_FontRenderer = font_renderer;

        dmProfile::IterateSamples(&ctx, &ProfileSampleCallback);
        dmRender::FontRendererFlush(font_renderer);

        ctx.m_Index = 0;
        dmProfile::IterateScopes(&ctx, &ProfileScopeCallback);

        dmRender::FontRendererFlush(font_renderer);
    }
}
