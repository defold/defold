#include <string.h>
#include <math.h>
#include <float.h>
#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>
#include <dlib/hashtable.h>
#include <dlib/utf8.h>
#include <graphics/graphics_util.h>

#include "font_renderer.h"
#include "font_renderer_private.h"

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
    , m_MaxAscent(0.0f)
    , m_MaxDescent(0.0f)
    , m_SdfScale(1.0f)
    , m_SdfOffset(0)
    , m_SdfOutline(0)
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
        , m_MaxAscent(0.0f)
        , m_MaxDescent(0.0f)
        {

        }

        ~FontMap()
        {
            dmGraphics::DeleteTexture(m_Texture);
        }

        dmGraphics::HTexture    m_Texture;
        HMaterial               m_Material;
        dmHashTable16<Glyph>    m_Glyphs;
        uint32_t                m_TextureWidth;
        uint32_t                m_TextureHeight;
        float                   m_ShadowX;
        float                   m_ShadowY;
        float                   m_MaxAscent;
        float                   m_MaxDescent;
        float                   m_SdfScale;
        float                   m_SdfOffset;
        float                   m_SdfOutline;
    };

    struct GlyphVertex
    {
        float    m_Position[4];
        float    m_UV[2];
        uint32_t m_FaceColor;
        uint32_t m_OutlineColor;
        uint32_t m_ShadowColor;
        float    m_SdfParams[4];
    };

    static float GetLineTextMetrics(HFontMap font_map, const char* text, int n);

    HFontMap NewFontMap(dmGraphics::HContext graphics_context, FontMapParams& params)
    {
        FontMap* font_map = new FontMap();
        font_map->m_Material = 0;

        const dmArray<Glyph>& glyphs = params.m_Glyphs;
        font_map->m_Glyphs.SetCapacity((3 * glyphs.Size()) / 2, glyphs.Size());
        for (uint32_t i = 0; i < glyphs.Size(); ++i) {
            const Glyph& g = glyphs[i];
            font_map->m_Glyphs.Put(g.m_Character, g);
        }

        font_map->m_TextureWidth = params.m_TextureWidth;
        font_map->m_TextureHeight = params.m_TextureHeight;
        font_map->m_ShadowX = params.m_ShadowX;
        font_map->m_ShadowY = params.m_ShadowY;
        font_map->m_MaxAscent = params.m_MaxAscent;
        font_map->m_MaxDescent = params.m_MaxDescent;
        font_map->m_SdfScale = params.m_SdfScale;
        font_map->m_SdfOffset = params.m_SdfOffset;
        font_map->m_SdfOutline = params.m_SdfOutline;
        dmGraphics::TextureCreationParams tex_create_params;
        dmGraphics::TextureParams tex_params;

        tex_create_params.m_Width = params.m_TextureWidth;
        tex_create_params.m_Height = params.m_TextureHeight;
        tex_create_params.m_OriginalWidth = params.m_TextureWidth;
        tex_create_params.m_OriginalHeight = params.m_TextureHeight;

        tex_params.m_Format = dmGraphics::TEXTURE_FORMAT_RGB;
        tex_params.m_Data = params.m_TextureData;
        tex_params.m_DataSize = params.m_TextureDataSize;
        tex_params.m_Width = params.m_TextureWidth;
        tex_params.m_Height = params.m_TextureHeight;
        // NOTE: No mipmap support in fonts yet therefore TEXTURE_FILTER_LINEAR
        tex_params.m_MinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        tex_params.m_MagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        font_map->m_Texture = dmGraphics::NewTexture(graphics_context, tex_create_params);
        dmGraphics::SetTexture(font_map->m_Texture, tex_params);

        return font_map;
    }

    void DeleteFontMap(HFontMap font_map)
    {
        delete font_map;
    }

    void SetFontMap(HFontMap font_map, FontMapParams& params)
    {
        const dmArray<Glyph>& glyphs = params.m_Glyphs;
        font_map->m_Glyphs.SetCapacity((3 * glyphs.Size()) / 2, glyphs.Size());
        font_map->m_Glyphs.Clear();
        for (uint32_t i = 0; i < glyphs.Size(); ++i) {
            const Glyph& g = glyphs[i];
            font_map->m_Glyphs.Put(g.m_Character, g);
        }
        font_map->m_TextureWidth = params.m_TextureWidth;
        font_map->m_TextureHeight = params.m_TextureHeight;
        font_map->m_ShadowX = params.m_ShadowX;
        font_map->m_ShadowY = params.m_ShadowY;
        font_map->m_MaxAscent = params.m_MaxAscent;
        font_map->m_MaxDescent = params.m_MaxDescent;
        font_map->m_SdfScale = params.m_SdfScale;
        font_map->m_SdfOffset = params.m_SdfOffset;
        font_map->m_SdfOutline = params.m_SdfOutline;
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

        text_context.m_MaxVertexCount = max_characters * 6; // 6 vertices per character
        uint32_t buffer_size = sizeof(GlyphVertex) * text_context.m_MaxVertexCount;
        text_context.m_VertexBuffer = dmGraphics::NewVertexBuffer(render_context->m_GraphicsContext, buffer_size, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        text_context.m_ClientBuffer = new char[buffer_size];
        text_context.m_VertexIndex = 0;
        text_context.m_VerticesFlushed = 0;

        dmGraphics::VertexElement ve[] =
        {
                {"position", 0, 4, dmGraphics::TYPE_FLOAT, false},
                {"texcoord0", 1, 2, dmGraphics::TYPE_FLOAT, false},
                {"face_color", 2, 4, dmGraphics::TYPE_UNSIGNED_BYTE, true},
                {"outline_color", 3, 4, dmGraphics::TYPE_UNSIGNED_BYTE, true},
                {"shadow_color", 4, 4, dmGraphics::TYPE_UNSIGNED_BYTE, true},
                {"sdf_params", 5, 4, dmGraphics::TYPE_FLOAT, false},
        };

        text_context.m_VertexDecl = dmGraphics::NewVertexDeclaration(render_context->m_GraphicsContext, ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));

        // Arbitrary number
        const uint32_t max_batches = 128;
        text_context.m_RenderObjects.SetCapacity(max_batches);
        text_context.m_RenderObjectIndex = 0;

        text_context.m_Batches.SetCapacity((max_batches * 3) / 2, max_batches);
        // Approximately as we store terminating '\0'
        text_context.m_TextBuffer.SetCapacity(max_characters);
        // NOTE: 8 is "arbitrary" heuristic
        text_context.m_TextEntries.SetCapacity(max_characters / 8);

        for (uint32_t i = 0; i < text_context.m_RenderObjects.Capacity(); ++i)
        {
            RenderObject ro;
            ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_SRC_ALPHA;
            ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            ro.m_SetBlendFactors = 1;
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
    , m_RenderOrder(0)
    , m_Width(FLT_MAX)
    , m_LineBreak(false)
    , m_Align(TEXT_ALIGN_LEFT)
    , m_VAlign(TEXT_VALIGN_TOP)
    , m_StencilTestParamsSet(0)
    {
        m_StencilTestParams.Init();
    }

    struct LayoutMetrics
    {
        HFontMap m_FontMap;
        LayoutMetrics(HFontMap font_map) : m_FontMap(font_map) {}
        float operator()(const char* text, uint32_t n)
        {
            return GetLineTextMetrics(m_FontMap, text, n);
        }
    };

    static dmhash_t g_ShadowOffsetHash = dmHashString64("offset");
    static dmhash_t g_TextureSizeRecipHash = dmHashString64("texture_size_recip");

    void DrawText(HRenderContext render_context, HFontMap font_map, const DrawTextParams& params)
    {
        DM_PROFILE(Render, "DrawText");

        TextContext* text_context = &render_context->m_TextContext;
        HashState64 key_state;
        dmHashInit64(&key_state, false);
        dmHashUpdateBuffer64(&key_state, &font_map, sizeof(font_map));
        dmHashUpdateBuffer64(&key_state, &params.m_Depth, sizeof(params.m_Depth));
        dmHashUpdateBuffer64(&key_state, &params.m_RenderOrder, sizeof(params.m_RenderOrder));
        if (params.m_StencilTestParamsSet) {
            dmHashUpdateBuffer64(&key_state, &params.m_StencilTestParams, sizeof(params.m_StencilTestParams));
        }
        uint64_t key = dmHashFinal64(&key_state);

        int32_t* head = text_context->m_Batches.Get(key);
        TextEntry* head_entry = 0;
        int32_t next = -1;
        if (!head) {
            if (text_context->m_Batches.Full()) {
                dmLogWarning("Out of text-render batches");
                return;
            }
        } else {
            head_entry = &text_context->m_TextEntries[*head];
            next = *head;
        }

        if (text_context->m_TextEntries.Full()) {
            dmLogWarning("Out of text-render entries");
            return;
        }

        uint32_t text_len = strlen(params.m_Text);
        if (text_context->m_TextBuffer.Capacity() < (text_len + 1)) {
            dmLogWarning("Out of text-render buffer");
            return;
        }

        uint32_t offset = text_context->m_TextBuffer.Size();
        text_context->m_TextBuffer.PushArray(params.m_Text, text_len);
        text_context->m_TextBuffer.Push('\0');

        TextEntry te;
        te.m_Transform = params.m_WorldTransform;
        te.m_StringOffset = offset;
        te.m_FontMap = font_map;
        te.m_Next = -1;
        te.m_Tail = -1;

        te.m_FaceColor = dmGraphics::PackRGBA(params.m_FaceColor);
        te.m_OutlineColor = dmGraphics::PackRGBA(params.m_OutlineColor);
        te.m_ShadowColor = dmGraphics::PackRGBA(params.m_ShadowColor);
        te.m_Depth = params.m_Depth;
        te.m_RenderOrder = params.m_RenderOrder;
        te.m_Width = params.m_Width;
        te.m_Height = params.m_Height;
        te.m_LineBreak = params.m_LineBreak;
        te.m_Align = params.m_Align;
        te.m_VAlign = params.m_VAlign;
        te.m_StencilTestParams = params.m_StencilTestParams;
        te.m_StencilTestParamsSet = params.m_StencilTestParamsSet;

        int32_t index = text_context->m_TextEntries.Size();
        if (head_entry) {
            TextEntry* tail_entry = head_entry;
            if (head_entry->m_Tail != -1) {
                tail_entry = &text_context->m_TextEntries[head_entry->m_Tail];
            }
            tail_entry->m_Next = index;
            head_entry->m_Tail = index;
        } else {
            text_context->m_Batches.Put(key, index);
        }

        text_context->m_TextEntries.Push(te);
    }

    static float OffsetX(uint32_t align, float width)
    {
        switch (align)
        {
            case TEXT_ALIGN_LEFT:
                return 0.0f;
            case TEXT_ALIGN_CENTER:
                return width * 0.5f;
            case TEXT_ALIGN_RIGHT:
                return width;
            default:
                return 0.0f;
        }
    }

    static float OffsetY(uint32_t valign, float height, float ascent, float descent, uint32_t line_count)
    {
        switch (valign)
        {
            case TEXT_VALIGN_TOP:
                return height - ascent;
            case TEXT_VALIGN_MIDDLE:
                return height * 0.5f + (ascent + descent) * line_count * 0.5f - ascent;
            case TEXT_VALIGN_BOTTOM:
                return (ascent + descent) * (line_count - 1) + descent;
            default:
                return height - ascent;
        }
    }

    static const Glyph* GetGlyph(HFontMap font_map, uint16_t c) {
        const Glyph* g = font_map->m_Glyphs.Get(c);
        if (!g)
            g = font_map->m_Glyphs.Get(126U); // Fallback to ~

        if (!g) {
            dmLogWarning("Character code %x not supported by font, nor is fallback '~'", c);
        }

        return g;
    }

    void CreateFontVertexData(HRenderContext render_context, const uint64_t* key, int32_t* batch)
    {
        DM_PROFILE(Render, "CreateFontVertexData");
        TextContext& text_context = render_context->m_TextContext;
        int32_t entry_key = *batch;
        const TextEntry& first_te = text_context.m_TextEntries[entry_key];
        HFontMap font_map = first_te.m_FontMap;
        float im_recip = 1.0f / font_map->m_TextureWidth;
        float ih_recip = 1.0f / font_map->m_TextureHeight;

        GlyphVertex* vertices = (GlyphVertex*)text_context.m_ClientBuffer;

        if (text_context.m_RenderObjectIndex >= text_context.m_RenderObjects.Size()) {
            dmLogWarning("Fontrenderer: character buffer exceeded (size: %d)", text_context.m_VertexIndex / 4);
            return;
        }

        RenderObject* ro = &text_context.m_RenderObjects[text_context.m_RenderObjectIndex++];
        ro->m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
        ro->m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        ro->m_SetBlendFactors = 1;
        ro->m_Material = font_map->m_Material;
        ro->m_Textures[0] = font_map->m_Texture;
        ro->m_VertexStart = text_context.m_VertexIndex;
        ro->m_StencilTestParams = first_te.m_StencilTestParams;
        ro->m_SetStencilTest = first_te.m_StencilTestParamsSet;

        Vector4 texture_size_recip(im_recip, ih_recip, 0, 0);
        EnableRenderObjectConstant(ro, g_TextureSizeRecipHash, texture_size_recip);

        const uint32_t max_lines = 128;
        TextLine lines[max_lines];

        while (entry_key != -1) {
            const TextEntry& te = text_context.m_TextEntries[entry_key];

            float width = te.m_Width;
            if (!te.m_LineBreak) {
                width = FLT_MAX;
            }
            const char* text = &text_context.m_TextBuffer[te.m_StringOffset];

            LayoutMetrics lm(font_map);
            float layout_width;
            int line_count = Layout(text, width, lines, max_lines, &layout_width, lm);
            float x_offset = OffsetX(te.m_Align, te.m_Width);
            float y_offset = OffsetY(te.m_VAlign, te.m_Height, font_map->m_MaxAscent, font_map->m_MaxDescent, line_count);

            uint32_t face_color = te.m_FaceColor;
            uint32_t outline_color = te.m_OutlineColor;
            uint32_t shadow_color = te.m_ShadowColor;

            // No support for non-uniform scale with SDF so just peek at the first
            // row to extracat scale factor. The purpose of this scaling is to have
            // world space distances in the computation, for good 'antialiasing' no matter
            // what scale is being rendered in. Scaling down does, however, not work well.
            const Vectormath::Aos::Vector4 r0 = te.m_Transform.getRow(0);
            float sdf_world_scale = sqrtf(r0.getX() * r0.getX() + r0.getY() * r0.getY());
            float sdf_smoothing = 1.0f;

            // There will be no hope for an outline smaller than a quarter than a pixel
            // so effectively disable it.
            if ((font_map->m_SdfOutline > 0) && (font_map->m_SdfOutline * sdf_world_scale < 0.25f))
            {
                outline_color = face_color;
            }

            // Trade scale for smoothing when scaling down
            if (sdf_world_scale < 1.0f)
            {
                sdf_world_scale = 1.0f;
                sdf_smoothing = sdf_world_scale;
            }

            float sdf_offset  = sdf_world_scale * font_map->m_SdfOffset;
            float sdf_scale   = sdf_world_scale * font_map->m_SdfScale;
            float sdf_outline = sdf_world_scale * font_map->m_SdfOutline;


            for (int line = 0; line < line_count; ++line) {
                TextLine& l = lines[line];
                int16_t x = (int16_t)(x_offset - OffsetX(te.m_Align, l.m_Width) + 0.5f);
                int16_t y = (int16_t) (y_offset - line * (font_map->m_MaxAscent + font_map->m_MaxDescent) + 0.5f);
                const char* cursor = &text[l.m_Index];
                int n = l.m_Count;
                for (int j = 0; j < n; ++j)
                {
                    uint16_t c = (uint16_t) dmUtf8::NextChar(&cursor);

                    const Glyph* g =  GetGlyph(font_map, c);
                    if (!g) {
                        continue;
                    }

                    if (text_context.m_VertexIndex + 6 >= text_context.m_MaxVertexCount)
                    {
                        dmLogWarning("Fontrenderer: character buffer exceeded (size: %d)", text_context.m_VertexIndex / 6);
                        return;
                    }

                    if (g->m_Width > 0)
                    {
                        GlyphVertex& v1 = vertices[text_context.m_VertexIndex];
                        GlyphVertex& v2 = *(&v1 + 1);
                        GlyphVertex& v3 = *(&v1 + 2);
                        GlyphVertex& v4 = *(&v1 + 3);
                        GlyphVertex& v5 = *(&v1 + 4);
                        GlyphVertex& v6 = *(&v1 + 5);
                        text_context.m_VertexIndex += 6;

                        int16_t width = (int16_t)g->m_Width;
                        int16_t descent = (int16_t)g->m_Descent;
                        int16_t ascent = (int16_t)g->m_Ascent;

                        // TODO: 16 bytes alignment and simd (when enabled in vector-math library)
                        //       Legal cast? (strict aliasing)
                        (Vector4&) v1.m_Position = te.m_Transform * Vector4(x + g->m_LeftBearing, y - descent, 0, 1);
                        (Vector4&) v2.m_Position = te.m_Transform * Vector4(x + g->m_LeftBearing, y + ascent, 0, 1);
                        (Vector4&) v3.m_Position = te.m_Transform * Vector4(x + g->m_LeftBearing + width, y - descent, 0, 1);
                        (Vector4&) v6.m_Position = te.m_Transform * Vector4(x + g->m_LeftBearing + width, y + ascent, 0, 1);

                        v1.m_UV[0] = (g->m_X + g->m_LeftBearing) * im_recip;
                        v1.m_UV[1] = (g->m_Y + descent) * ih_recip;

                        v2.m_UV[0] = (g->m_X + g->m_LeftBearing) * im_recip;
                        v2.m_UV[1] = (g->m_Y - ascent) * ih_recip;

                        v3.m_UV[0] = (g->m_X + g->m_LeftBearing + g->m_Width) * im_recip;
                        v3.m_UV[1] = (g->m_Y + descent) * ih_recip;

                        v6.m_UV[0] = (g->m_X + g->m_LeftBearing + g->m_Width) * im_recip;
                        v6.m_UV[1] = (g->m_Y - ascent) * ih_recip;

                        #define SET_VERTEX_PARAMS(v) \
                            v.m_FaceColor = face_color; \
                            v.m_OutlineColor = outline_color; \
                            v.m_ShadowColor = shadow_color; \
                            v.m_SdfParams[0] = sdf_scale; \
                            v.m_SdfParams[1] = sdf_offset; \
                            v.m_SdfParams[2] = sdf_outline; \
                            v.m_SdfParams[3] = sdf_smoothing; \

                        SET_VERTEX_PARAMS(v1)
                        SET_VERTEX_PARAMS(v2)
                        SET_VERTEX_PARAMS(v3)
                        SET_VERTEX_PARAMS(v6)

                        #undef SET_VERTEX_PARAMS

                        v4 = v3;
                        v5 = v2;
                    }
                    x += (int16_t)g->m_Advance;
                }
            }
            entry_key = te.m_Next;
        }

        ro->m_VertexCount = text_context.m_VertexIndex - ro->m_VertexStart;
        AddToRender(render_context, ro);
    }

    void FlushTexts(HRenderContext render_context, bool final)
    {
        DM_PROFILE(Render, "FlushTexts");
        TextContext& text_context = render_context->m_TextContext;

        if (text_context.m_Batches.Size() > 0) {
            text_context.m_Batches.Iterate(CreateFontVertexData, render_context);
            // This might be called multiple times so clear the batch table
            // This function should however only be called once. See case 2261
            text_context.m_Batches.Clear();
        }

        if (final)
        {
            if (text_context.m_VerticesFlushed != text_context.m_VertexIndex)
            {
                uint32_t buffer_size = sizeof(GlyphVertex) * text_context.m_VertexIndex;
                dmGraphics::SetVertexBufferData(text_context.m_VertexBuffer, 0, 0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
                dmGraphics::SetVertexBufferData(text_context.m_VertexBuffer, buffer_size, text_context.m_ClientBuffer, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
                text_context.m_VerticesFlushed = text_context.m_VertexIndex;
            }
        }
    }

    static float GetLineTextMetrics(HFontMap font_map, const char* text, int n)
    {
        float width = 0;
        const char* cursor = text;
        const Glyph* first = 0;
        const Glyph* last = 0;
        for (int i = 0; i < n; ++i)
        {
            uint32_t c = dmUtf8::NextChar(&cursor);
            const Glyph* g = GetGlyph(font_map, c);
            if (!g) {
                continue;
            }

            if (i == 0)
                first = g;
            last = g;
            // NOTE: We round advance here just as above in DrawText
            width += (int16_t) g->m_Advance;
        }
        if (n > 0 && 0 != last)
        {
            float last_end_point = last->m_LeftBearing + last->m_Width;
            float last_right_bearing = last->m_Advance - last_end_point;
            width = width - last_right_bearing;
        }

        return width;
    }

    void GetTextMetrics(HFontMap font_map, const char* text, float width, bool line_break, TextMetrics* metrics)
    {
        metrics->m_MaxAscent = font_map->m_MaxAscent;
        metrics->m_MaxDescent = font_map->m_MaxDescent;

        if (!line_break) {
            width = FLT_MAX;
        }

        const uint32_t max_lines = 128;
        dmRender::TextLine lines[max_lines];

        LayoutMetrics lm(font_map);
        float layout_width;
        Layout(text, width, lines, max_lines, &layout_width, lm);
        metrics->m_Width = layout_width;
    }

}
