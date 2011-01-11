#include "font_renderer.h"

#include <string.h>
#include <math.h>
#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include "render_private.h"
#include "render/font_ddf.h"

using namespace Vectormath::Aos;

namespace dmRender
{
    struct Font
    {
        ~Font()
        {
            dmGraphics::DeleteTexture(m_Texture);
        }

        dmRenderDDF::ImageFont* m_ImageFont;
        dmGraphics::HTexture    m_Texture;
        HMaterial               m_Material;
    };

    HImageFont NewImageFont(const void* font, uint32_t font_size)
    {
        dmRenderDDF::ImageFont* f;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::ImageFont>(font, font_size, &f);
        if (e != dmDDF::RESULT_OK)
        {
            return 0;
        }
        return f;
    }

    void DeleteImageFont(HImageFont font)
    {
        dmDDF::FreeMessage(font);
    }

    HFont NewFont(HRenderContext render_context, HImageFont image_font)
    {
        Font* ret = new Font();
        ret->m_Material = 0;
        ret->m_ImageFont = (dmRenderDDF::ImageFont*) image_font;
        dmGraphics::TextureParams params;
        params.m_Format = dmGraphics::TEXTURE_FORMAT_RGB;
        params.m_Data = &ret->m_ImageFont->m_ImageData[0];
        params.m_DataSize = ret->m_ImageFont->m_ImageData.m_Count;
        params.m_Width = ret->m_ImageFont->m_ImageWidth;
        params.m_Height = ret->m_ImageFont->m_ImageHeight;
        ret->m_Texture = dmGraphics::NewTexture(render_context->m_GraphicsContext, params);

        return ret;
    }

    HImageFont GetImageFont(HFont font)
    {
        return font->m_ImageFont;
    }

    dmGraphics::HTexture GetTexture(HFont font)
    {
        return font->m_Texture;
    }

    void SetMaterial(HFont font, HMaterial material)
    {
        font->m_Material = material;
    }

    HMaterial GetMaterial(HFont font)
    {
        return font->m_Material;
    }

    void DeleteFont(HFont font)
    {
        delete font;
    }

    void InitializeTextContext(HRenderContext render_context, uint32_t max_characters)
    {
        TextContext& text_context = render_context->m_TextContext;

        text_context.m_MaxVertexCount = max_characters * 4;
        text_context.m_VertexBuffer = dmGraphics::NewVertexBuffer(render_context->m_GraphicsContext, 4 * sizeof(float) * text_context.m_MaxVertexCount, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        text_context.m_VertexIndex = 0;

        dmGraphics::VertexElement ve[] =
        {
                {0, 4, dmGraphics::TYPE_FLOAT, 0, 0}
        };

        text_context.m_VertexDecl = dmGraphics::NewVertexDeclaration(render_context->m_GraphicsContext, ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));

        text_context.m_RenderObjects.SetCapacity(max_characters/8);
        for (uint32_t i = 0; i < text_context.m_RenderObjects.Capacity(); ++i)
        {
            RenderObject ro;
            ro.m_VertexBuffer = text_context.m_VertexBuffer;
            ro.m_VertexDeclaration = text_context.m_VertexDecl;
            ro.m_PrimitiveType = dmGraphics::PRIMITIVE_QUADS;
            text_context.m_RenderObjects.Push(ro);
        }
    }

    void FinalizeTextContext(HRenderContext render_context)
    {
        TextContext& text_context = render_context->m_TextContext;
        dmGraphics::DeleteVertexBuffer(text_context.m_VertexBuffer);
        dmGraphics::DeleteVertexDeclaration(text_context.m_VertexDecl);
    }

    DrawTextParams::DrawTextParams()
    : m_FaceColor(0.0f, 0.0f, 0.0f, 0.0f)
    , m_OutlineColor(0.0f, 0.0f, 0.0f, 0.0f)
    , m_ShadowColor(0.0f, 0.0f, 0.0f, 0.0f)
    , m_Text(0x0)
    , m_X(0)
    , m_Y(0)
    {}

    void DrawText(HRenderContext render_context, HFont font, const DrawTextParams& params)
    {
        TextContext& text_context = render_context->m_TextContext;
        if (text_context.m_VertexIndex + 4 >= text_context.m_MaxVertexCount || text_context.m_RenderObjectIndex >= text_context.m_RenderObjects.Size())
        {
            dmLogWarning("Fontrenderer: character buffer exceeded (size: %d)", text_context.m_VertexIndex / 4);
            return;
        }

        int n = strlen(params.m_Text);

        Vectormath::Aos::Vector4 colors[3] = {params.m_FaceColor, params.m_OutlineColor, params.m_ShadowColor};
        Vectormath::Aos::Vector4 clear_color(0.0f, 0.0f, 0.0f, 0.0f);

        float x_offsets[3] = {0.0f, 0.0f, font->m_ImageFont->m_ShadowX};
        float y_offsets[3] = {0.0f, 0.0f, font->m_ImageFont->m_ShadowY};

        struct TextVertex
        {
            float m_Position[2];
            float m_UV[2];
        };
        TextVertex* vertices = (TextVertex*)dmGraphics::MapVertexBuffer(text_context.m_VertexBuffer, dmGraphics::BUFFER_ACCESS_WRITE_ONLY);

        for (int i = 2; i >= 0; --i)
        {
            uint16_t x = params.m_X;
            uint16_t y = params.m_Y;
            RenderObject* ro = &text_context.m_RenderObjects[text_context.m_RenderObjectIndex++];
            ro->m_Material = font->m_Material;
            ro->m_Textures[0] = font->m_Texture;
            ro->m_VertexStart = text_context.m_VertexIndex;
            for (int j = 0; j < 3; ++j)
            {
                if (i == j)
                    SetRenderObjectFragmentConstant(ro, j, colors[j]);
                else
                    SetRenderObjectFragmentConstant(ro, j, clear_color);
            }
            for (int j = 0; j < n; ++j)
            {
                char c = params.m_Text[j];

                const dmRenderDDF::ImageFont::Glyph& g = font->m_ImageFont->m_Glyphs[c];

                if (g.m_Width > 0)
                {
                    TextVertex& v1 = vertices[text_context.m_VertexIndex];
                    TextVertex& v2 = *(&v1 + 1);
                    TextVertex& v3 = *(&v1 + 2);
                    TextVertex& v4 = *(&v1 + 3);
                    text_context.m_VertexIndex += 4;

                    v1.m_Position[0] = x + g.m_LeftBearing + x_offsets[i];
                    v1.m_Position[1] = y - g.m_Descent + y_offsets[i];

                    v2.m_Position[0] = x + g.m_LeftBearing + g.m_Width + x_offsets[i];
                    v2.m_Position[1] = y - g.m_Descent + y_offsets[i];

                    v3.m_Position[0] = x + g.m_LeftBearing + g.m_Width + x_offsets[i];
                    v3.m_Position[1] = y + g.m_Ascent + y_offsets[i];

                    v4.m_Position[0] = x + g.m_LeftBearing + x_offsets[i];
                    v4.m_Position[1] = y + g.m_Ascent + y_offsets[i];

                    float im_recip = 1.0f / font->m_ImageFont->m_ImageWidth;
                    float ih_recip = 1.0f / font->m_ImageFont->m_ImageHeight;

                    v1.m_UV[0] = (g.m_X + g.m_LeftBearing) * im_recip;
                    v1.m_UV[1] = (g.m_Y - g.m_Ascent) * ih_recip;

                    v2.m_UV[0] = (g.m_X + g.m_LeftBearing + g.m_Width) * im_recip;
                    v2.m_UV[1] = (g.m_Y - g.m_Ascent) * ih_recip;

                    v3.m_UV[0] = (g.m_X + g.m_LeftBearing + g.m_Width) * im_recip;
                    v3.m_UV[1] = (g.m_Y + g.m_Descent) * ih_recip;

                    v4.m_UV[0] = (g.m_X + g.m_LeftBearing) * im_recip;
                    v4.m_UV[1] = (g.m_Y + g.m_Descent) * ih_recip;
                }
                x += g.m_Advance;
            }
            ro->m_VertexCount = text_context.m_VertexIndex - ro->m_VertexStart;
            AddToRender(render_context, ro);
        }
        dmGraphics::UnmapVertexBuffer(text_context.m_VertexBuffer);
    }
}
