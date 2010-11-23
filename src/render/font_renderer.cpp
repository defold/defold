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

        dmRenderDDF::ImageFont* m_ImageFont;
        dmGraphics::HTexture    m_Texture;
        HMaterial               m_Material;
    };

    struct TextUserData
    {
        Vectormath::Aos::Vector4    m_FaceColor;
        Vectormath::Aos::Vector4    m_OutlineColor;
        Vectormath::Aos::Vector4    m_ShadowColor;
        Font*                       m_Font;
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
        ret->m_ImageFont = (dmRenderDDF::ImageFont*) image_font;
        ret->m_Texture = dmGraphics::NewTexture(ret->m_ImageFont->m_ImageWidth,
                                                   ret->m_ImageFont->m_ImageHeight,
                                                   dmGraphics::TEXTURE_FORMAT_RGB);

        // TODO: Texture data is duplicated in memory. (m_Texture + m_Font->m_ImageData)
        dmGraphics::SetTextureData(ret->m_Texture, 0, ret->m_ImageFont->m_ImageWidth, ret->m_ImageFont->m_ImageHeight, 0,
                                   dmGraphics::TEXTURE_FORMAT_RGB, &ret->m_ImageFont->m_ImageData[0], ret->m_ImageFont->m_ImageData.m_Count);

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

        RenderType text_render_type;
        text_render_type.m_BeginCallback = RenderTypeTextBegin;
        text_render_type.m_DrawCallback = RenderTypeTextDraw;
        text_render_type.m_EndCallback = 0x0;
        RegisterRenderType(render_context, text_render_type, &text_context.m_TextRenderType);

        uint32_t max_vertex_count = max_characters * 4;
        text_context.m_Vertices.SetCapacity(max_vertex_count);
        text_context.m_RenderObjects.SetCapacity(max_characters/8);
        for (uint32_t i = 0; i < text_context.m_RenderObjects.Capacity(); ++i)
        {
            HRenderObject ro = dmRender::NewRenderObject(text_context.m_TextRenderType, 0x0, 0x0);
            TextUserData* text_user_data = new TextUserData();
            dmRender::SetUserData(ro, (void*)text_user_data);
            text_context.m_RenderObjects.Push(ro);
        }
        text_context.m_VertexBuffer = dmGraphics::NewVertexBuffer(sizeof(TextVertex) * max_vertex_count, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

        dmGraphics::VertexElement ve[] =
        {
                {0, 4, dmGraphics::TYPE_FLOAT, 0, 0}
        };

        text_context.m_VertexDecl = dmGraphics::NewVertexDeclaration(ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));
    }

    void FinalizeTextContext(HRenderContext render_context)
    {
        TextContext& text_context = render_context->m_TextContext;
        for (uint32_t i = 0; i < text_context.m_RenderObjects.Size(); ++i)
        {
            HRenderObject ro = text_context.m_RenderObjects[i];
            TextUserData* text_user_data = (TextUserData*)dmRender::GetUserData(ro);
            delete text_user_data;
            dmRender::DeleteRenderObject(ro);
        }
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
        if (text_context.m_Vertices.Size() + 4 >= text_context.m_Vertices.Capacity() || text_context.m_RenderObjectIndex >= text_context.m_RenderObjects.Size())
        {
            dmLogWarning("Fontrenderer: character buffer exceeded (size: %d)", text_context.m_Vertices.Capacity() / 4);
            return;
        }

        int n = strlen(params.m_Text);
        uint16_t x = params.m_X;
        uint16_t y = params.m_Y;

        HRenderObject ro = text_context.m_RenderObjects[text_context.m_RenderObjectIndex++];
        SetMaterial(ro, font->m_Material);
        TextUserData* text_user_data = (TextUserData*)dmRender::GetUserData(ro);
        text_user_data->m_Font = font;
        text_user_data->m_FaceColor = params.m_FaceColor;
        text_user_data->m_OutlineColor = params.m_OutlineColor;
        text_user_data->m_ShadowColor = params.m_ShadowColor;
        text_user_data->m_VertexStart = text_context.m_Vertices.Size();

        for (int i = 0; i < n; ++i)
        {
            char c = params.m_Text[i];

            const dmRenderDDF::ImageFont::Glyph& g = font->m_ImageFont->m_Glyphs[c];

            if (g.m_Width > 0)
            {
                TextVertex v1, v2, v3, v4;

                v1.m_Position[0] = x + g.m_LeftBearing;
                v1.m_Position[1] = y - g.m_Descent;

                v2.m_Position[0] = x + g.m_LeftBearing + g.m_Width;
                v2.m_Position[1] = y - g.m_Descent;

                v3.m_Position[0] = x + g.m_LeftBearing + g.m_Width;
                v3.m_Position[1] = y + g.m_Ascent;

                v4.m_Position[0] = x + g.m_LeftBearing;
                v4.m_Position[1] = y + g.m_Ascent;

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

                text_context.m_Vertices.Push(v1);
                text_context.m_Vertices.Push(v2);
                text_context.m_Vertices.Push(v3);
                text_context.m_Vertices.Push(v4);
            }
            x += g.m_Advance;
        }
        text_user_data->m_VertexCount = text_context.m_Vertices.Size() - text_user_data->m_VertexStart;
        AddToRender(render_context, ro);
    }

    void RenderTypeTextBegin(HRenderContext render_context, void* user_context)
    {
        (void)render_context;
    }

    void RenderTypeTextDraw(HRenderContext render_context, void* user_context, HRenderObject ro, uint32_t count)
    {
        TextUserData* text_user_data = (TextUserData*)dmRender::GetUserData(ro);
        HFont font = text_user_data->m_Font;
        dmArray<TextVertex>* vertex_data = &render_context->m_TextContext.m_Vertices;

        if (text_user_data->m_VertexCount == 0)
            return;

        void* data = (void*)&vertex_data->Front();

        dmGraphics::HContext context = render_context->m_GFXContext;

        dmGraphics::SetTexture(context, GetTexture(font));

        dmGraphics::SetVertexStream(context, 0, 4, dmGraphics::TYPE_FLOAT, sizeof(TextVertex), data);
        dmGraphics::DisableVertexStream(context, 1);
        dmGraphics::DisableVertexStream(context, 2);

        Matrix4 mat = Matrix4::orthographic(0, GetDisplayWidth(render_context), GetDisplayHeight(render_context), 0, 1, -1);
        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&mat, 0, 4);

        Vector4 clear(0.0f, 0.0f, 0.0f, 0.0f);

        if (text_user_data->m_ShadowColor.getW() > 0.0f)
        {
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&clear, 0);
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&clear, 1);
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&text_user_data->m_ShadowColor, 2);

            Vector4 offset(font->m_ImageFont->m_ShadowX, font->m_ImageFont->m_ShadowY, 0.0f, 0.0f);
            dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&offset, 4, 1);

            dmGraphics::Draw(context, dmGraphics::PRIMITIVE_QUADS, text_user_data->m_VertexStart, text_user_data->m_VertexCount);
        }

        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&clear, 4, 1);

        if (text_user_data->m_FaceColor.getW() > 0.0f)
        {
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&text_user_data->m_FaceColor, 0);
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&clear, 1);
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&clear, 2);

            dmGraphics::Draw(context, dmGraphics::PRIMITIVE_QUADS, text_user_data->m_VertexStart, text_user_data->m_VertexCount);
        }

        if (text_user_data->m_OutlineColor.getW() > 0.0f)
        {
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&clear, 0);
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&text_user_data->m_OutlineColor, 1);
            dmGraphics::SetFragmentConstant(context, (const Vector4*)&clear, 2);

            dmGraphics::Draw(context, dmGraphics::PRIMITIVE_QUADS, text_user_data->m_VertexStart, text_user_data->m_VertexCount);
        }

        dmGraphics::DisableVertexStream(context, 0);
    }
}
