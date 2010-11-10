#include "font_renderer.h"

#include <string.h>
#include <math.h>
#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include <graphics/graphics_device.h>

#include "render_private.h"
#include "render/render_ddf.h"

using namespace Vectormath::Aos;

namespace dmRender
{

    struct SFont
    {
        ~SFont()
        {
            dmGraphics::DestroyTexture(m_Texture);
        }

        dmRenderDDF::ImageFont*     m_Font;
        dmGraphics::HTexture        m_Texture;
        dmGraphics::HMaterial       m_Material;
    };

    struct SFontRenderer
    {
        dmRender::HRenderContext    m_RenderContext;
        SFont*                      m_Font;
        dmArray<SFontVertex>        m_Vertices;
        uint32_t                    m_MaxCharacters;
        dmRender::HRenderObject     m_RenderObject;
        dmGraphics::HVertexBuffer m_VertexBuffer;
        dmGraphics::HVertexDeclaration m_VertexDecl;
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

    HFont NewFont(HImageFont image_font)
    {
        SFont*ret = new SFont();
        ret->m_Material = 0;
        ret->m_Font = (dmRenderDDF::ImageFont*) image_font;
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

    dmGraphics::HTexture GetTexture(HFont font)
    {
        return font->m_Texture;
    }

    void SetMaterial(HFont font, dmGraphics::HMaterial material)
    {
        font->m_Material = material;
    }

    dmGraphics::HMaterial GetMaterial(HFont font)
    {
        return font->m_Material;
    }

    void DeleteFont(HFont font)
    {
        delete font;
    }

    HFontRenderer NewFontRenderer(HRenderContext render_context, HFont font,
                                  uint32_t width, uint32_t height,
                                  uint32_t max_characters)
    {
        SFontRenderer* fr = new SFontRenderer();
        fr->m_RenderContext = render_context;
        fr->m_Vertices.SetCapacity(max_characters*6);
        fr->m_Font = font;
        fr->m_MaxCharacters = max_characters;
        fr->m_RenderObject = dmRender::NewRenderObject(render_context->m_TextRenderType, font->m_Material, 0x0);
        dmRender::SetUserData(fr->m_RenderObject, (void*)fr);
        fr->m_VertexBuffer = dmGraphics::NewVertexbuffer(sizeof(SFontVertex), max_characters*6, dmGraphics::BUFFER_TYPE_DYNAMIC, dmGraphics::MEMORY_TYPE_MAIN,1, 0x0);

        dmGraphics::VertexElement ve[] =
        {
                {0, 3, dmGraphics::TYPE_FLOAT, 0, 0},
                {1, 3, dmGraphics::TYPE_FLOAT, 0, 0},
                {2, 2, dmGraphics::TYPE_FLOAT, 0, 0}
        };

        fr->m_VertexDecl = dmGraphics::NewVertexDeclaration(ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));
        return fr;
    }

    void DeleteFontRenderer(HFontRenderer renderer)
    {
        dmRender::DeleteRenderObject(renderer->m_RenderObject);
        dmGraphics::DeleteVertexBuffer(renderer->m_VertexBuffer);
        dmGraphics::DeleteVertexDeclaration(renderer->m_VertexDecl);
        delete renderer;
    }

    void FontRendererDrawString(HFontRenderer renderer, const char* string, uint16_t x0, uint16_t y0, float red, float green, float blue, float alpha)
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

            const dmRenderDDF::ImageFont::Glyph& g = renderer->m_Font->m_Font->m_Glyphs[c];

            if (g.m_Width != 0)
            {
                SFontVertex v1, v2, v3, v4;

                v1.m_Position[0] = x + g.m_LeftBearing;
                v1.m_Position[1] = y - g.m_Descent;
                v1.m_Position[2] = 0;

                v2.m_Position[0] = x + g.m_LeftBearing + g.m_Width;
                v2.m_Position[1] = y - g.m_Descent;
                v2.m_Position[2] = 0;

                v3.m_Position[0] = x + g.m_LeftBearing + g.m_Width;
                v3.m_Position[1] = y + g.m_Ascent;
                v3.m_Position[2] = 0;

                v4.m_Position[0] = x + g.m_LeftBearing;
                v4.m_Position[1] = y + g.m_Ascent;
                v4.m_Position[2] = 0;

                float im_recip = 1.0f / renderer->m_Font->m_Font->m_ImageWidth;
                float ih_recip = 1.0f / renderer->m_Font->m_Font->m_ImageHeight;

                v1.m_UV[0] = (g.m_X + g.m_LeftBearing) * im_recip;
                v1.m_UV[1] = (g.m_Y - g.m_Ascent) * ih_recip;

                v2.m_UV[0] = (g.m_X + g.m_LeftBearing + g.m_Width) * im_recip;
                v2.m_UV[1] = (g.m_Y - g.m_Ascent) * ih_recip;

                v3.m_UV[0] = (g.m_X + g.m_LeftBearing + g.m_Width) * im_recip;
                v3.m_UV[1] = (g.m_Y + g.m_Descent) * ih_recip;

                v4.m_UV[0] = (g.m_X + g.m_LeftBearing) * im_recip;
                v4.m_UV[1] = (g.m_Y + g.m_Descent) * ih_recip;


                v1.m_Colour[0] = red; v1.m_Colour[1] = green; v1.m_Colour[2] = blue; v1.m_Colour[3] = alpha;
                v2.m_Colour[0] = red; v2.m_Colour[1] = green; v2.m_Colour[2] = blue; v2.m_Colour[3] = alpha;
                v3.m_Colour[0] = red; v3.m_Colour[1] = green; v3.m_Colour[2] = blue; v3.m_Colour[3] = alpha;
                v4.m_Colour[0] = red; v4.m_Colour[1] = green; v4.m_Colour[2] = blue; v4.m_Colour[3] = alpha;


                renderer->m_Vertices.Push(v1);
                renderer->m_Vertices.Push(v2);
                renderer->m_Vertices.Push(v3);

                renderer->m_Vertices.Push(v3);
                renderer->m_Vertices.Push(v4);
                renderer->m_Vertices.Push(v1);
            }
            x += g.m_Advance;
        }
    }

    void FontRendererFlush(HFontRenderer renderer)
    {
        if (renderer->m_Vertices.Size() == 0)
            return;

        dmRender::AddToRender(renderer->m_RenderContext, renderer->m_RenderObject);
    }

    void FontRendererClear(HFontRenderer renderer)
    {
        renderer->m_Vertices.SetSize(0);
    }

    void RenderTypeTextBegin(HRenderContext render_context)
    {
        (void)render_context;
    }

    void RenderTypeTextDraw(HRenderContext rendercontext, HRenderObject ro, uint32_t count)
    {
        HFontRenderer renderer = (HFontRenderer)dmRender::GetUserData(ro);
        HFont font = renderer->m_Font;
        dmArray<SFontVertex>* vertex_data = &renderer->m_Vertices;

        if (!font || !vertex_data || vertex_data->Size() == 0)
            return;

        SFontVertex* data = &vertex_data->Front();
        float* pos = data->m_Position;
        float* colour = data->m_Colour;
        float* uv = data->m_UV;

        dmGraphics::HContext context = rendercontext->m_GFXContext;

        Matrix4 ident = Matrix4::identity();

        Matrix4 mat = Matrix4::orthographic( 0, 960, 540, 0, 10, -10 );

        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&mat, 0, 4);
        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&ident, 4, 4);

        dmGraphics::SetVertexStream(context, 0, 3, dmGraphics::TYPE_FLOAT,
                           sizeof(SFontVertex),
                           (void*) pos);

        dmGraphics::SetVertexStream(context, 1, 2, dmGraphics::TYPE_FLOAT,
                           sizeof(SFontVertex),
                           (void*) uv);

        dmGraphics::SetVertexStream(context, 2, 4, dmGraphics::TYPE_FLOAT,
                           sizeof(SFontVertex),
                           (void*) colour);

        dmGraphics::SetTexture(context, GetTexture(font));

        dmGraphics::Draw(context, dmGraphics::PRIMITIVE_TRIANGLES, 0, vertex_data->Size());

        dmGraphics::DisableVertexStream(context, 0);
        dmGraphics::DisableVertexStream(context, 1);
        dmGraphics::DisableVertexStream(context, 2);
    }
}
