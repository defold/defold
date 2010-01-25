#include <string.h>
#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/log.h>
#include <dlib/array.h>
#include <graphics/graphics_device.h>

#include "fontrenderer.h"
#include "render/render_ddf.h"

using namespace Vectormath::Aos;

namespace dmRender
{
    struct SFontVertex
    {
        float m_Position[3];
        float m_UV[2];
        float m_Colour[3];
    };

    struct SFont
    {
        ~SFont()
        {
            dmDDF::FreeMessage(m_Font);
            dmGraphics::DestroyTexture(m_Texture);
        }

        ImageFont*                   m_Font;
        dmGraphics::HTexture         m_Texture;
        dmGraphics::HVertexProgram   m_VertexProgram;
        dmGraphics::HFragmentProgram m_FragmentProgram;
    };

    struct SFontRenderer
    {
        ~SFontRenderer()
        {
        }

        SFont*                    m_Font;
        dmArray<SFontVertex>      m_Vertices;
        Matrix4                   m_Projection;
        uint32_t                  m_MaxCharacters;
    };

    HImageFont NewImageFont(const void* font, uint32_t font_size)
    {
        ImageFont* f;
        dmDDF::Result e = dmDDF::LoadMessage<ImageFont>(font, font_size, &f);
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

    HFont NewFont(HImageFont image_font)
    {
        SFont*ret = new SFont();
        ret->m_VertexProgram = 0;
        ret->m_FragmentProgram = 0;
        ret->m_Font = (dmRender::ImageFont*) image_font;
        ret->m_Texture = dmGraphics::CreateTexture(ret->m_Font->m_ImageWidth,
                                                   ret->m_Font->m_ImageHeight,
                                                   dmGraphics::TEXTURE_FORMAT_LUMINANCE);

        // TODO: Texture data is duplicated in memory. (m_Texture + m_Font->m_ImageData)
        dmGraphics::SetTextureData(ret->m_Texture, 0, ret->m_Font->m_ImageWidth, ret->m_Font->m_ImageHeight, 0,
                                   dmGraphics::TEXTURE_FORMAT_LUMINANCE, &ret->m_Font->m_ImageData[0], ret->m_Font->m_ImageData.m_Count);

        return ret;
    }

    HImageFont GetImageFont(HFont font)
    {
        return font->m_Font;
    }

    void SetVertexProgram(HFont font, dmGraphics::HVertexProgram program)
    {
        font->m_VertexProgram = program;
    }

    dmGraphics::HVertexProgram GetVertexProgram(HFont font)
    {
        return font->m_VertexProgram;
    }

    void SetFragmentProgram(HFont font, dmGraphics::HFragmentProgram program)
    {
        font->m_FragmentProgram = program;
    }

    dmGraphics::HFragmentProgram GetFragmentProgram(HFont font)
    {
        return font->m_FragmentProgram;
    }

    void DeleteFont(HFont font)
    {
        delete font;
    }

    HFontRenderer NewFontRenderer(HFont font,
                                  uint32_t width, uint32_t height,
                                  uint32_t max_characters)
    {
        SFontRenderer* fr = new SFontRenderer();
        fr->m_Vertices.SetCapacity(max_characters*6);
        fr->m_Font = font;
        fr->m_Projection = Matrix4::orthographic( 0, width, height, 0, 10, -10 );
        fr->m_MaxCharacters = max_characters;
        return fr;
    }

    void DeleteFontRenderer(HFontRenderer renderer)
    {
        delete renderer;
    }

    void FontRendererDrawString(HFontRenderer renderer, const char* string, uint16_t x0, uint16_t y0, float red, float green, float blue)
    {
        if (renderer->m_Vertices.Size() >= renderer->m_MaxCharacters)
        {
            dmLogWarning("Fontrenderer: character buffer exceeded (size: %d)", renderer->m_MaxCharacters);
            return;
        }

        int n = strlen(string);
        uint16_t x = x0;
        uint16_t y = y0;

        for (int i = 0; i < n; ++i)
        {
            char c = string[i];

            ImageFont::Glyph g = renderer->m_Font->m_Font->m_Glyphs[c];

            SFontVertex v1, v2, v3, v4;

            v1.m_Position[0] = x;
            v1.m_Position[1] = y;
            v1.m_Position[2] = 0;

            v2.m_Position[0] = x + g.m_Width;
            v2.m_Position[1] = y;
            v2.m_Position[2] = 0;

            v3.m_Position[0] = x + g.m_Width;
            v3.m_Position[1] = y + g.m_Height;
            v3.m_Position[2] = 0;

            v4.m_Position[0] = x;
            v4.m_Position[1] = y + g.m_Height;
            v4.m_Position[2] = 0;

            float im_recip = 1.0f / renderer->m_Font->m_Font->m_ImageWidth;
            float ih_recip = 1.0f / renderer->m_Font->m_Font->m_ImageHeight;

            v1.m_UV[0] = (g.m_X) * im_recip;
            v1.m_UV[1] = (g.m_Y) * ih_recip;

            v2.m_UV[0] = (g.m_X + g.m_Width) * im_recip;
            v2.m_UV[1] = (g.m_Y) * ih_recip;

            v3.m_UV[0] = (g.m_X + g.m_Width) * im_recip;
            v3.m_UV[1] = (g.m_Y + g.m_Height) * ih_recip;

            v4.m_UV[0] = (g.m_X) * im_recip;
            v4.m_UV[1] = (g.m_Y + g.m_Height) * ih_recip;


            v1.m_Colour[0] = red; v1.m_Colour[1] = green; v1.m_Colour[2] = blue;
            v2.m_Colour[0] = red; v2.m_Colour[1] = green; v2.m_Colour[2] = blue;
            v3.m_Colour[0] = red; v3.m_Colour[1] = green; v3.m_Colour[2] = blue;
            v4.m_Colour[0] = red; v4.m_Colour[1] = green; v4.m_Colour[2] = blue;


            renderer->m_Vertices.Push(v1);
            renderer->m_Vertices.Push(v2);
            renderer->m_Vertices.Push(v3);

            renderer->m_Vertices.Push(v3);
            renderer->m_Vertices.Push(v4);
            renderer->m_Vertices.Push(v1);

            x += g.m_Width;
        }
    }

    void FontRendererFlush(HFontRenderer renderer)
    {
        if (renderer->m_Vertices.Size() == 0)
            return;

        dmGraphics::HContext context = dmGraphics::GetContext();
        Matrix4 ident = Matrix4::identity();

        if (renderer->m_Font->m_VertexProgram)
            dmGraphics::SetVertexProgram(context, renderer->m_Font->m_VertexProgram);
        if (renderer->m_Font->m_FragmentProgram)
            dmGraphics::SetFragmentProgram(context, renderer->m_Font->m_FragmentProgram);

        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&renderer->m_Projection, 0, 4);
        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&ident, 4, 4);

        dmGraphics::SetVertexStream(context, 0, 3, dmGraphics::TYPE_FLOAT,
                           sizeof(SFontVertex),
                           (void*) &renderer->m_Vertices[0].m_Position[0]);

        dmGraphics::SetVertexStream(context, 1, 2, dmGraphics::TYPE_FLOAT,
                           sizeof(SFontVertex),
                           (void*) &renderer->m_Vertices[0].m_UV[0]);

        dmGraphics::SetVertexStream(context, 2, 3, dmGraphics::TYPE_FLOAT,
                           sizeof(SFontVertex),
                           (void*) &renderer->m_Vertices[0].m_Colour[0]);

        dmGraphics::SetTexture(context, renderer->m_Font->m_Texture);

        dmGraphics::SetBlendFunc(context, dmGraphics::BLEND_FACTOR_SRC_ALPHA, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        dmGraphics::DisableState(context, dmGraphics::DEPTH_TEST);
        dmGraphics::EnableState(context, dmGraphics::BLEND);

        dmGraphics::Draw(context, dmGraphics::PRIMITIVE_TRIANGLES, 0, renderer->m_Vertices.Size());

        dmGraphics::EnableState(context, dmGraphics::DEPTH_TEST);
        dmGraphics::DisableState(context, dmGraphics::BLEND);

        dmGraphics::DisableVertexStream(context, 0);
        dmGraphics::DisableVertexStream(context, 1);
        dmGraphics::DisableVertexStream(context, 2);

        renderer->m_Vertices.SetSize(0);
    }

}
