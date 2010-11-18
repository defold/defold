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
    struct Font
    {
        ~Font()
        {
            dmGraphics::DeleteTexture(m_Texture);
        }

        dmRenderDDF::ImageFont*     m_Font;
        dmGraphics::HTexture        m_Texture;
        dmGraphics::HMaterial       m_Material;
    };

    struct FontRenderer
    {
        dmRender::HRenderContext            m_RenderContext;
        Font*                               m_Font;
        dmArray<SFontVertex>                m_Vertices;
        dmArray<dmRender::HRenderObject>    m_RenderObjects;
        uint32_t                            m_RenderObjectIndex;
        dmGraphics::HVertexBuffer           m_VertexBuffer;
        dmGraphics::HVertexDeclaration      m_VertexDecl;
    };

    struct FontUserData
    {
        Vectormath::Aos::Vector4    m_FaceColor;
        Vectormath::Aos::Vector4    m_OutlineColor;
        Vectormath::Aos::Vector4    m_ShadowColor;
        FontRenderer*               m_Renderer;
        uint32_t                    m_VertexStart;
        uint32_t                    m_VertexCount;
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
        Font* ret = new Font();
        ret->m_Material = 0;
        ret->m_Font = (dmRenderDDF::ImageFont*) image_font;
        ret->m_Texture = dmGraphics::NewTexture(ret->m_Font->m_ImageWidth,
                                                   ret->m_Font->m_ImageHeight,
                                                   dmGraphics::TEXTURE_FORMAT_RGB);

        // TODO: Texture data is duplicated in memory. (m_Texture + m_Font->m_ImageData)
        dmGraphics::SetTextureData(ret->m_Texture, 0, ret->m_Font->m_ImageWidth, ret->m_Font->m_ImageHeight, 0,
                                   dmGraphics::TEXTURE_FORMAT_RGB, &ret->m_Font->m_ImageData[0], ret->m_Font->m_ImageData.m_Count);

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
        uint32_t max_vertex_count = max_characters * 4;
        FontRenderer* fr = new FontRenderer();
        fr->m_RenderContext = render_context;
        fr->m_Vertices.SetCapacity(max_vertex_count);
        fr->m_Font = font;
        fr->m_RenderObjects.SetCapacity(max_characters/8);
        for (uint32_t i = 0; i < fr->m_RenderObjects.Capacity(); ++i)
        {
            HRenderObject ro = dmRender::NewRenderObject(render_context->m_TextRenderType, font->m_Material, 0x0);
            FontUserData* font_user_data = new FontUserData();
            font_user_data->m_Renderer = fr;
            dmRender::SetUserData(ro, (void*)font_user_data);
            fr->m_RenderObjects.Push(ro);
        }
        fr->m_VertexBuffer = dmGraphics::NewVertexbuffer(sizeof(SFontVertex), max_vertex_count, dmGraphics::BUFFER_TYPE_DYNAMIC, dmGraphics::MEMORY_TYPE_MAIN,1, 0x0);

        dmGraphics::VertexElement ve[] =
        {
                {0, 4, dmGraphics::TYPE_FLOAT, 0, 0}
        };

        fr->m_VertexDecl = dmGraphics::NewVertexDeclaration(ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));
        return fr;
    }

    void DeleteFontRenderer(HFontRenderer renderer)
    {
        for (uint32_t i = 0; i < renderer->m_RenderObjects.Size(); ++i)
        {
            HRenderObject ro = renderer->m_RenderObjects[i];
            FontUserData* font_user_data = (FontUserData*)dmRender::GetUserData(ro);
            delete font_user_data;
            dmRender::DeleteRenderObject(ro);
        }
        dmGraphics::DeleteVertexBuffer(renderer->m_VertexBuffer);
        dmGraphics::DeleteVertexDeclaration(renderer->m_VertexDecl);
        delete renderer;
    }

    DrawStringParams::DrawStringParams()
    : m_FaceColor(0.0f, 0.0f, 0.0f, 0.0f)
    , m_OutlineColor(0.0f, 0.0f, 0.0f, 0.0f)
    , m_ShadowColor(0.0f, 0.0f, 0.0f, 0.0f)
    , m_String(0x0)
    , m_X(0)
    , m_Y(0)
    {}

    void FontRendererDrawString(HFontRenderer renderer, const DrawStringParams& params)
    {
        if (renderer->m_Vertices.Size() + 4 >= renderer->m_Vertices.Capacity() || renderer->m_RenderObjectIndex >= renderer->m_RenderObjects.Size())
        {
            dmLogWarning("Fontrenderer: character buffer exceeded (size: %d)", renderer->m_Vertices.Capacity() / 4);
            return;
        }

        int n = strlen(params.m_String);
        uint16_t x = params.m_X;
        uint16_t y = params.m_Y;

        HRenderObject ro = renderer->m_RenderObjects[renderer->m_RenderObjectIndex++];
        FontUserData* font_user_data = (FontUserData*)dmRender::GetUserData(ro);
        font_user_data->m_FaceColor = params.m_FaceColor;
        font_user_data->m_OutlineColor = params.m_OutlineColor;
        font_user_data->m_ShadowColor = params.m_ShadowColor;
        font_user_data->m_VertexStart = renderer->m_Vertices.Size();
        font_user_data->m_VertexCount = n * 4;

        AddToRender(renderer->m_RenderContext, ro);

        for (int i = 0; i < n; ++i)
        {
            char c = params.m_String[i];

            const dmRenderDDF::ImageFont::Glyph& g = renderer->m_Font->m_Font->m_Glyphs[c];

            if (g.m_Width > 0)
            {
                SFontVertex v1, v2, v3, v4;

                v1.m_Position[0] = x + g.m_LeftBearing;
                v1.m_Position[1] = y - g.m_Descent;

                v2.m_Position[0] = x + g.m_LeftBearing + g.m_Width;
                v2.m_Position[1] = y - g.m_Descent;

                v3.m_Position[0] = x + g.m_LeftBearing + g.m_Width;
                v3.m_Position[1] = y + g.m_Ascent;

                v4.m_Position[0] = x + g.m_LeftBearing;
                v4.m_Position[1] = y + g.m_Ascent;

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

                renderer->m_Vertices.Push(v1);
                renderer->m_Vertices.Push(v2);
                renderer->m_Vertices.Push(v3);
                renderer->m_Vertices.Push(v4);
            }
            x += g.m_Advance;
        }
    }

    void FontRendererClear(HFontRenderer renderer)
    {
        renderer->m_Vertices.SetSize(0);
        renderer->m_RenderObjectIndex = 0;
    }

    void RenderTypeTextBegin(HRenderContext render_context)
    {
        (void)render_context;
    }

    void RenderTypeTextDraw(HRenderContext rendercontext, HRenderObject ro, uint32_t count)
    {
        FontUserData* font_user_data = (FontUserData*)dmRender::GetUserData(ro);
        HFontRenderer renderer = font_user_data->m_Renderer;
        HFont font = renderer->m_Font;
        dmArray<SFontVertex>* vertex_data = &renderer->m_Vertices;

        if (font_user_data->m_VertexCount == 0)
            return;

        void* data = (void*)&vertex_data->Front();

        dmGraphics::HContext context = rendercontext->m_GFXContext;

        dmGraphics::SetTexture(context, GetTexture(font));

        dmGraphics::SetVertexStream(context, 0, 4, dmGraphics::TYPE_FLOAT, sizeof(SFontVertex), data);
        dmGraphics::DisableVertexStream(context, 1);
        dmGraphics::DisableVertexStream(context, 2);

        Matrix4 mat = Matrix4::orthographic(0, GetDisplayWidth(rendercontext), GetDisplayHeight(rendercontext), 0, 1, -1);
        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&mat, 0, 4);

        Vector4 face_color(1.0f, 0.0f, 0.0f, 1.0f);
        Vector4 outline_color(0.0f, 1.0f, 0.0f, 1.0f);
        Vector4 shadow_color(0.0f, 0.0f, 1.0f, 1.0f);
        Vector4 clear(0.0f, 0.0f, 0.0f, 0.0f);

        if (font_user_data->m_ShadowColor.getW() > 0.0f)
        {
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&clear, 0);
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&clear, 1);
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&font_user_data->m_ShadowColor, 2);

            Vector4 offset(font->m_Font->m_ShadowX, font->m_Font->m_ShadowY, 0.0f, 0.0f);
            dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&offset, 4, 1);

            dmGraphics::Draw(context, dmGraphics::PRIMITIVE_QUADS, font_user_data->m_VertexStart, font_user_data->m_VertexCount);
        }

        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&clear, 4, 1);

        if (font_user_data->m_FaceColor.getW() > 0.0f)
        {
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&font_user_data->m_FaceColor, 0);
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&clear, 1);
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&clear, 2);

            dmGraphics::Draw(context, dmGraphics::PRIMITIVE_QUADS, font_user_data->m_VertexStart, font_user_data->m_VertexCount);
        }

        if (font_user_data->m_OutlineColor.getW() > 0.0f)
        {
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&clear, 0);
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&font_user_data->m_OutlineColor, 1);
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&clear, 2);

            dmGraphics::Draw(context, dmGraphics::PRIMITIVE_QUADS, font_user_data->m_VertexStart, font_user_data->m_VertexCount);
        }

        dmGraphics::DisableVertexStream(context, 0);
    }
}
