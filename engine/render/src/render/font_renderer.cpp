
#include "font_renderer.h"

#include <string.h>
#include <math.h>
#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include "render_private.h"
#include "render/font_ddf.h"
#include "render.h"

using namespace Vectormath::Aos;

namespace dmRender
{
    FontMapParams::FontMapParams()
    : m_Glyphs()
    , m_TextureWidth(0)
    , m_TextureHeight(0)
    , m_TextureData(0x0)
    , m_TextureDataSize(0)
    , m_ShadowX(0.0f)
    , m_ShadowY(0.0f)
    {

    }

    struct FontMap
    {
        FontMap()
        : m_Texture(0)
        , m_Material(0)
        , m_Glyphs()
        , m_TextureWidth(0)
        , m_TextureHeight(0)
        , m_ShadowX(0.0f)
        , m_ShadowY(0.0f)
        {

        }

        ~FontMap()
        {
            dmGraphics::DeleteTexture(m_Texture);
        }

        dmGraphics::HTexture    m_Texture;
        HMaterial               m_Material;
        dmArray<Glyph>          m_Glyphs;
        uint32_t                m_TextureWidth;
        uint32_t                m_TextureHeight;
        float                   m_ShadowX;
        float                   m_ShadowY;
        float                   m_MaxAscent;
        float                   m_MaxDescent;
    };

    HFontMap NewFontMap(dmGraphics::HContext graphics_context, FontMapParams& params)
    {
        FontMap* font_map = new FontMap();
        font_map->m_Material = 0;

        font_map->m_Glyphs.Swap(params.m_Glyphs);
        font_map->m_TextureWidth = params.m_TextureWidth;
        font_map->m_TextureHeight = params.m_TextureHeight;
        font_map->m_ShadowX = params.m_ShadowX;
        font_map->m_ShadowY = params.m_ShadowY;
        font_map->m_MaxAscent = params.m_MaxAscent;
        font_map->m_MaxDescent = params.m_MaxDescent;
        dmGraphics::TextureParams tex_params;
        tex_params.m_Format = dmGraphics::TEXTURE_FORMAT_RGB;
        tex_params.m_Data = params.m_TextureData;
        tex_params.m_DataSize = params.m_TextureDataSize;
        tex_params.m_Width = params.m_TextureWidth;
        tex_params.m_Height = params.m_TextureHeight;
        tex_params.m_OriginalWidth = params.m_TextureWidth;
        tex_params.m_OriginalHeight = params.m_TextureHeight;
        // NOTE: No mipmap support in fonts yet therefore TEXTURE_FILTER_LINEAR
        tex_params.m_MinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        tex_params.m_MagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        font_map->m_Texture = dmGraphics::NewTexture(graphics_context, tex_params);

        return font_map;
    }

    void DeleteFontMap(HFontMap font_map)
    {
        delete font_map;
    }

    void SetFontMap(HFontMap font_map, FontMapParams& params)
    {
        font_map->m_Glyphs.Swap(params.m_Glyphs);
        font_map->m_TextureWidth = params.m_TextureWidth;
        font_map->m_TextureHeight = params.m_TextureHeight;
        font_map->m_ShadowX = params.m_ShadowX;
        font_map->m_ShadowY = params.m_ShadowY;
        font_map->m_MaxAscent = params.m_MaxAscent;
        font_map->m_MaxDescent = params.m_MaxDescent;
        dmGraphics::TextureParams tex_params;
        tex_params.m_Format = dmGraphics::TEXTURE_FORMAT_RGB;
        tex_params.m_Data = params.m_TextureData;
        tex_params.m_DataSize = params.m_TextureDataSize;
        tex_params.m_Width = params.m_TextureWidth;
        tex_params.m_Height = params.m_TextureHeight;
        dmGraphics::SetTexture(font_map->m_Texture, tex_params);
    }

    dmGraphics::HTexture GetFontMapTexture(HFontMap font_map)
    {
        return font_map->m_Texture;
    }

    void SetFontMapMaterial(HFontMap font_map, HMaterial material)
    {
        font_map->m_Material = material;
    }

    HMaterial GetFontMapMaterial(HFontMap font_map)
    {
        return font_map->m_Material;
    }

    void InitializeTextContext(HRenderContext render_context, uint32_t max_characters)
    {
        TextContext& text_context = render_context->m_TextContext;

        text_context.m_MaxVertexCount = max_characters * 6 * 3; // 6 vertices per character and 3 passes
        uint32_t buffer_size = 4 * sizeof(float) * text_context.m_MaxVertexCount;
        text_context.m_VertexBuffer = dmGraphics::NewVertexBuffer(render_context->m_GraphicsContext, buffer_size, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        text_context.m_ClientBuffer = new char[buffer_size];
        text_context.m_VertexIndex = 0;

        dmGraphics::VertexElement ve[] =
        {
                {"position", 0, 4, dmGraphics::TYPE_FLOAT }
        };

        text_context.m_VertexDecl = dmGraphics::NewVertexDeclaration(render_context->m_GraphicsContext, ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));

        text_context.m_RenderObjects.SetCapacity(max_characters/8);
        text_context.m_RenderObjectIndex = 0;
        for (uint32_t i = 0; i < text_context.m_RenderObjects.Capacity(); ++i)
        {
            RenderObject ro;
            ro.m_VertexBuffer = text_context.m_VertexBuffer;
            ro.m_VertexDeclaration = text_context.m_VertexDecl;
            ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
            text_context.m_RenderObjects.Push(ro);
        }
    }

    void FinalizeTextContext(HRenderContext render_context)
    {
        TextContext& text_context = render_context->m_TextContext;
        delete [] (char*)text_context.m_ClientBuffer;
        dmGraphics::DeleteVertexBuffer(text_context.m_VertexBuffer);
        dmGraphics::DeleteVertexDeclaration(text_context.m_VertexDecl);
    }

    DrawTextParams::DrawTextParams()
    : m_WorldTransform(Matrix4::identity())
    , m_FaceColor(0.0f, 0.0f, 0.0f, -1.0f)
    , m_OutlineColor(0.0f, 0.0f, 0.0f, -1.0f)
    , m_ShadowColor(0.0f, 0.0f, 0.0f, -1.0f)
    , m_Text(0x0)
    , m_Depth(0)
    {
    }

    static dmhash_t g_ConstantNameHashes[] = { dmHashString64("face_color"), dmHashString64("outline_color"), dmHashString64("shadow_color") };

    void DrawText(HRenderContext render_context, HFontMap font_map, const DrawTextParams& params)
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

        float x_offsets[3] = {0.0f, 0.0f, font_map->m_ShadowX};
        float y_offsets[3] = {0.0f, 0.0f, font_map->m_ShadowY};

        struct TextVertex
        {
            float m_Position[2];
            float m_UV[2];
        };
        TextVertex* vertices = (TextVertex*)text_context.m_ClientBuffer;

        for (int i = 2; i >= 0; --i)
        {
            RenderObject* ro = &text_context.m_RenderObjects[text_context.m_RenderObjectIndex++];
            ro->m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_SRC_ALPHA;
            ro->m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            ro->m_SetBlendFactors = 1;
            ro->m_RenderKey.m_Depth = params.m_Depth;
            ro->m_RenderKey.m_Order = 2 - i;
            ro->m_Material = font_map->m_Material;
            ro->m_Textures[0] = font_map->m_Texture;
            ro->m_VertexStart = text_context.m_VertexIndex;
            ro->m_WorldTransform = params.m_WorldTransform;
            for (int j = 0; j < 3; ++j)
            {
                if (i == j)
                {
                    if (colors[j].getW() >= 0.0f)
                        EnableRenderObjectConstant(ro, g_ConstantNameHashes[j], colors[j]);
                }
                else
                    EnableRenderObjectConstant(ro, g_ConstantNameHashes[j], clear_color);
            }
            int16_t x = 0;
            int16_t y = 0;
            for (int j = 0; j < n; ++j)
            {
                char c = params.m_Text[j];

                const Glyph& g = font_map->m_Glyphs[c];

                if (g.m_Width > 0)
                {
                    TextVertex& v1 = vertices[text_context.m_VertexIndex];
                    TextVertex& v2 = *(&v1 + 1);
                    TextVertex& v3 = *(&v1 + 2);
                    TextVertex& v4 = *(&v1 + 3);
                    TextVertex& v5 = *(&v1 + 4);
                    TextVertex& v6 = *(&v1 + 5);
                    text_context.m_VertexIndex += 6;

                    int16_t width = (int16_t)g.m_Width;
                    int16_t descent = (int16_t)g.m_Descent;
                    int16_t ascent = (int16_t)g.m_Ascent;

                    v1.m_Position[0] = x + g.m_LeftBearing + x_offsets[i];
                    v1.m_Position[1] = y - descent + y_offsets[i];

                    v2.m_Position[0] = x + g.m_LeftBearing + x_offsets[i];
                    v2.m_Position[1] = y + ascent + y_offsets[i];

                    v3.m_Position[0] = x + g.m_LeftBearing + width + x_offsets[i];
                    v3.m_Position[1] = y - descent + y_offsets[i];

                    v6.m_Position[0] = x + g.m_LeftBearing + width + x_offsets[i];
                    v6.m_Position[1] = y + ascent + y_offsets[i];

                    float im_recip = 1.0f / font_map->m_TextureWidth;
                    float ih_recip = 1.0f / font_map->m_TextureHeight;

                    v1.m_UV[0] = (g.m_X + g.m_LeftBearing) * im_recip;
                    v1.m_UV[1] = (g.m_Y + descent) * ih_recip;

                    v2.m_UV[0] = (g.m_X + g.m_LeftBearing) * im_recip;
                    v2.m_UV[1] = (g.m_Y - ascent) * ih_recip;

                    v3.m_UV[0] = (g.m_X + g.m_LeftBearing + g.m_Width) * im_recip;
                    v3.m_UV[1] = (g.m_Y + descent) * ih_recip;

                    v6.m_UV[0] = (g.m_X + g.m_LeftBearing + g.m_Width) * im_recip;
                    v6.m_UV[1] = (g.m_Y - ascent) * ih_recip;

                    v4 = v3;
                    v5 = v2;
                }
                x += (int16_t)g.m_Advance;
            }
            ro->m_VertexCount = text_context.m_VertexIndex - ro->m_VertexStart;
            AddToRender(render_context, ro);
        }
    }

    void FlushTexts(HRenderContext render_context)
    {
        TextContext& text_context = render_context->m_TextContext;
        uint32_t buffer_size = 4 * sizeof(float) * text_context.m_VertexIndex;
        dmGraphics::SetVertexBufferData(text_context.m_VertexBuffer, 0, 0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        dmGraphics::SetVertexBufferData(text_context.m_VertexBuffer, buffer_size, text_context.m_ClientBuffer, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    }

    void GetTextMetrics(HFontMap font_map, const char* text, TextMetrics* metrics)
    {
        metrics->m_MaxAscent = font_map->m_MaxAscent;
        metrics->m_MaxDescent = font_map->m_MaxDescent;
        int n = strlen(text);
        float width = 0;
        for (int i = 0; i < n; ++i)
        {
            char c = text[i];
            const Glyph& g = font_map->m_Glyphs[c];
            // NOTE: We round advance here just as above in DrawText
            width += (int16_t) g.m_Advance;
        }
        if (n > 0)
        {
            const Glyph& first = font_map->m_Glyphs[0];
            const Glyph& last = font_map->m_Glyphs[n-1];
            width = width - first.m_LeftBearing - (last.m_Advance - last.m_Advance - last.m_Width);
            if (last.m_Width == 0.0f)
            {
                width += last.m_Advance;
            }
        }

        metrics->m_Width = width;
    }

}
