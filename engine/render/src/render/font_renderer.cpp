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
    , m_ShadowX(0.0f)
    , m_ShadowY(0.0f)
    , m_MaxAscent(0.0f)
    , m_MaxDescent(0.0f)
    , m_SdfScale(1.0f)
    , m_SdfOffset(0)
    , m_SdfOutline(0)
    , m_CacheWidth(0)
    , m_CacheHeight(0)
    , m_GlyphData(0)
    , m_CacheCellWidth(0)
    , m_CacheCellHeight(0)
    , m_GlyphChannels(1)
    , m_CacheCellPadding(0)
    {

    }

    struct FontMap
    {
        FontMap()
        : m_Texture(0)
        , m_Material(0)
        , m_Glyphs()
        , m_ShadowX(0.0f)
        , m_ShadowY(0.0f)
        , m_MaxAscent(0.0f)
        , m_MaxDescent(0.0f)
        , m_CacheWidth(0)
        , m_CacheHeight(0)
        , m_GlyphData(0)
        , m_Cache(0)
        , m_CacheCursor(0)
        , m_CacheColumns(0)
        , m_CacheRows(0)
        , m_CacheCellWidth(0)
        , m_CacheCellHeight(0)
        , m_CacheCellPadding(0)
        {

        }

        ~FontMap()
        {
            if (m_GlyphData) {
                free(m_GlyphData);
            }
            if (m_Cache) {
                free(m_Cache);
            }
            dmGraphics::DeleteTexture(m_Texture);
        }

        dmGraphics::HTexture    m_Texture;
        HMaterial               m_Material;
        dmHashTable32<Glyph>    m_Glyphs;
        float                   m_ShadowX;
        float                   m_ShadowY;
        float                   m_MaxAscent;
        float                   m_MaxDescent;
        float                   m_SdfScale;
        float                   m_SdfOffset;
        float                   m_SdfOutline;

        uint32_t                m_CacheWidth;
        uint32_t                m_CacheHeight;
        void*                   m_GlyphData;

        Glyph**                 m_Cache;
        uint32_t                m_CacheCursor;
        dmGraphics::TextureFormat m_CacheFormat;

        uint32_t                m_CacheColumns;
        uint32_t                m_CacheRows;

        uint32_t                m_CacheCellWidth;
        uint32_t                m_CacheCellHeight;
        uint8_t                 m_CacheCellPadding;
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

        font_map->m_ShadowX = params.m_ShadowX;
        font_map->m_ShadowY = params.m_ShadowY;
        font_map->m_MaxAscent = params.m_MaxAscent;
        font_map->m_MaxDescent = params.m_MaxDescent;
        font_map->m_SdfScale = params.m_SdfScale;
        font_map->m_SdfOffset = params.m_SdfOffset;
        font_map->m_SdfOutline = params.m_SdfOutline;

        font_map->m_CacheWidth = params.m_CacheWidth;
        font_map->m_CacheHeight = params.m_CacheHeight;
        font_map->m_GlyphData = params.m_GlyphData;

        font_map->m_CacheCellWidth = params.m_CacheCellWidth;
        font_map->m_CacheCellHeight = params.m_CacheCellHeight;
        font_map->m_CacheCellPadding = params.m_CacheCellPadding;

        font_map->m_CacheColumns = params.m_CacheWidth / params.m_CacheCellWidth;
        font_map->m_CacheRows = params.m_CacheHeight / params.m_CacheCellHeight;
        uint32_t cell_count = font_map->m_CacheColumns * font_map->m_CacheRows;

        switch (params.m_GlyphChannels)
        {
            case 1:
                font_map->m_CacheFormat = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
            break;
            case 3:
                font_map->m_CacheFormat = dmGraphics::TEXTURE_FORMAT_RGB;
            break;
            case 4:
                font_map->m_CacheFormat = dmGraphics::TEXTURE_FORMAT_RGBA;
            break;
            default:
                dmLogError("Invalid channel count for glyph data!");
                delete font_map;
                return 0x0;
        };

        font_map->m_Cache = (Glyph**)malloc(sizeof(Glyph*) * cell_count);
        memset(font_map->m_Cache, 0, sizeof(Glyph*) * cell_count);

        // create new texture to be used as a cache
        dmGraphics::TextureCreationParams tex_create_params;
        dmGraphics::TextureParams tex_params;
        tex_create_params.m_Width = params.m_CacheWidth;
        tex_create_params.m_Height = params.m_CacheHeight;
        tex_create_params.m_OriginalWidth = params.m_CacheWidth;
        tex_create_params.m_OriginalHeight = params.m_CacheHeight;
        tex_params.m_Format = font_map->m_CacheFormat;

        tex_params.m_Data = 0;
        tex_params.m_DataSize = 0;
        tex_params.m_Width = params.m_CacheWidth;
        tex_params.m_Height = params.m_CacheHeight;
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
        font_map->m_Glyphs.Clear();
        font_map->m_Glyphs.SetCapacity((3 * glyphs.Size()) / 2, glyphs.Size());
        for (uint32_t i = 0; i < glyphs.Size(); ++i) {
            const Glyph& g = glyphs[i];
            font_map->m_Glyphs.Put(g.m_Character, g);
        }

        // release previous glyph data bank
        if (font_map->m_GlyphData) {
            free(font_map->m_GlyphData);
            free(font_map->m_Cache);
        }

        font_map->m_ShadowX = params.m_ShadowX;
        font_map->m_ShadowY = params.m_ShadowY;
        font_map->m_MaxAscent = params.m_MaxAscent;
        font_map->m_MaxDescent = params.m_MaxDescent;
        font_map->m_SdfScale = params.m_SdfScale;
        font_map->m_SdfOffset = params.m_SdfOffset;
        font_map->m_SdfOutline = params.m_SdfOutline;

        font_map->m_CacheWidth = params.m_CacheWidth;
        font_map->m_CacheHeight = params.m_CacheHeight;
        font_map->m_GlyphData = params.m_GlyphData;

        font_map->m_CacheCellWidth = params.m_CacheCellWidth;
        font_map->m_CacheCellHeight = params.m_CacheCellHeight;
        font_map->m_CacheCellPadding = params.m_CacheCellPadding;

        font_map->m_CacheColumns = params.m_CacheWidth / params.m_CacheCellWidth;
        font_map->m_CacheRows = params.m_CacheHeight / params.m_CacheCellHeight;
        uint32_t cell_count = font_map->m_CacheColumns * font_map->m_CacheRows;

        switch (params.m_GlyphChannels)
        {
            case 1:
                font_map->m_CacheFormat = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
            break;
            case 3:
                font_map->m_CacheFormat = dmGraphics::TEXTURE_FORMAT_RGB;
            break;
            case 4:
                font_map->m_CacheFormat = dmGraphics::TEXTURE_FORMAT_RGBA;
            break;
            default:
                dmLogError("Invalid channel count for glyph data!");
                delete font_map;
                return;
        };

        font_map->m_Cache = (Glyph**)malloc(sizeof(Glyph*) * cell_count);
        memset(font_map->m_Cache, 0, sizeof(Glyph*) * cell_count);

        dmGraphics::TextureParams tex_params;
        tex_params.m_Format = font_map->m_CacheFormat;
        tex_params.m_Data = 0x0;
        tex_params.m_DataSize = 0x0;
        tex_params.m_Width = params.m_CacheWidth;
        tex_params.m_Height = params.m_CacheHeight;
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
        text_context.m_Frame = 0;

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
        uint32_t offset = text_context->m_TextBuffer.Size();
        if (text_context->m_TextBuffer.Capacity() < (offset + text_len + 1)) {
            dmLogWarning("Out of text-render buffer");
            return;
        }

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

    static Glyph* GetGlyph(HFontMap font_map, uint32_t c) {
        Glyph* g = font_map->m_Glyphs.Get(c);
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
        float im_recip = 1.0f;
        float ih_recip = 1.0f;
        if (font_map->m_Texture) {
            im_recip /= dmGraphics::GetOriginalTextureWidth(font_map->m_Texture);
            ih_recip /= dmGraphics::GetOriginalTextureHeight(font_map->m_Texture);
        }

        GlyphVertex* vertices = (GlyphVertex*)text_context.m_ClientBuffer;

        if (text_context.m_RenderObjectIndex >= text_context.m_RenderObjects.Size()) {
            dmLogWarning("Fontrenderer: Render object count reached limit (%d)", text_context.m_RenderObjectIndex);
            return;
        }

        RenderObject* ro = &text_context.m_RenderObjects[text_context.m_RenderObjectIndex++];
        ro->ClearConstants();
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

        Glyph* missing_glyphs[256];
        uint16_t missing_cursor = 0;

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
                    uint32_t c = dmUtf8::NextChar(&cursor);

                    Glyph* g =  GetGlyph(font_map, c);
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

                        if (!g->m_InCache) {
                            if (missing_cursor < 256) {
                                missing_glyphs[missing_cursor++] = g;
                            }
                        } else {
                            g->m_Frame = text_context.m_Frame;

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

                            v1.m_UV[0] = (g->m_X + font_map->m_CacheCellPadding) * im_recip;
                            v1.m_UV[1] = (g->m_Y + font_map->m_CacheCellPadding + ascent + descent) * ih_recip;

                            v2.m_UV[0] = (g->m_X + font_map->m_CacheCellPadding) * im_recip;
                            v2.m_UV[1] = (g->m_Y + font_map->m_CacheCellPadding) * ih_recip;

                            v3.m_UV[0] = (g->m_X + font_map->m_CacheCellPadding + g->m_Width) * im_recip;
                            v3.m_UV[1] = (g->m_Y + font_map->m_CacheCellPadding + ascent + descent) * ih_recip;

                            v6.m_UV[0] = (g->m_X + font_map->m_CacheCellPadding + g->m_Width) * im_recip;
                            v6.m_UV[1] = (g->m_Y + font_map->m_CacheCellPadding) * ih_recip;

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
                    }
                    x += (int16_t)g->m_Advance;
                }
            }
            entry_key = te.m_Next;
        }

        // Upload missing glyphs
        if (missing_cursor > 0) {
            uint32_t prev_cache_cursor = font_map->m_CacheCursor;
            dmGraphics::TextureParams tex_params;
            tex_params.m_SubUpdate = true;
            tex_params.m_MipMap = 0;
            tex_params.m_Format = font_map->m_CacheFormat;
            tex_params.m_MinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
            tex_params.m_MagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;

            while (missing_cursor > 0) {
                Glyph* g = missing_glyphs[--missing_cursor];

                // Check if glyph is already uploaded
                if (g->m_InCache) {
                    continue;
                }

                // Locate a cache cell candidate
                do {
                    uint32_t cur = font_map->m_CacheCursor++;
                    Glyph* candidate = font_map->m_Cache[cur];
                    font_map->m_CacheCursor = font_map->m_CacheCursor % (font_map->m_CacheColumns * font_map->m_CacheRows);

                    if (candidate == 0x0 || text_context.m_Frame != candidate->m_Frame) {

                        if (candidate) {
                            candidate->m_InCache = false;
                        }
                        font_map->m_Cache[cur] = g;

                        uint32_t col = cur % font_map->m_CacheColumns;
                        uint32_t row = cur / font_map->m_CacheColumns;

                        g->m_X = col * font_map->m_CacheCellWidth;
                        g->m_Y = row * font_map->m_CacheCellHeight;
                        g->m_Frame = text_context.m_Frame;
                        g->m_InCache = true;

                        // Upload glyph data to GPU
                        tex_params.m_X = g->m_X;
                        tex_params.m_Y = g->m_Y;
                        tex_params.m_Width = g->m_Width + font_map->m_CacheCellPadding*2;
                        tex_params.m_Height = g->m_Ascent + g->m_Descent + font_map->m_CacheCellPadding*2;
                        tex_params.m_Data = (uint8_t*)font_map->m_GlyphData + g->m_GlyphDataOffset;
                        SetTexture(font_map->m_Texture, tex_params);
                        break;
                    }

                } while (prev_cache_cursor != font_map->m_CacheCursor);

                if (prev_cache_cursor == font_map->m_CacheCursor) {
                    dmLogError("Out of available cache cells! Consider increasing cache_width or cache_height for the font.");
                    break;
                }
            }
        }

        ro->m_VertexCount = text_context.m_VertexIndex - ro->m_VertexStart;
    }

    static void FontRenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        if (params.m_Operation == dmRender::RENDER_LIST_OPERATION_BATCH)
        {
            for (uint32_t *i=params.m_Begin;i!=params.m_End;i++)
            {
                dmRender::RenderObject *ro = (dmRender::RenderObject*) params.m_Buf[*i].m_UserData;
                dmRender::AddToRender(params.m_Context, ro);
            }
        }
    }

    void FlushTexts(HRenderContext render_context, uint32_t render_order, bool final)
    {
        DM_PROFILE(Render, "FlushTexts");

        TextContext& text_context = render_context->m_TextContext;

        if (text_context.m_Batches.Size() > 0) {

            RenderObject* ro_begin = text_context.m_RenderObjects.Begin() + text_context.m_RenderObjectIndex;
            text_context.m_Batches.Iterate(CreateFontVertexData, render_context);
            text_context.m_Batches.Clear();
            RenderObject* ro_end = text_context.m_RenderObjects.Begin() + text_context.m_RenderObjectIndex;

            const uint32_t count = ro_end - ro_begin;

            dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, count);
            dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &FontRenderListDispatch, 0);
            dmRender::RenderListEntry* write_ptr = render_list;
            for (RenderObject* ro=ro_begin;ro!=ro_end;ro++)
            {
                write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_AFTER_WORLD;
                write_ptr->m_Order = render_order;
                write_ptr->m_UserData = (uintptr_t) ro;
                write_ptr->m_BatchKey = 0;
                write_ptr->m_TagMask = dmRender::GetMaterialTagMask(ro->m_Material);
                write_ptr->m_Dispatch = dispatch;
                write_ptr++;
            }
            dmRender::RenderListSubmit(render_context, render_list, write_ptr);
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
