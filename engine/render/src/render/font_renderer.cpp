#include <string.h>
#include <math.h>
#include <float.h>
#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/align.h>
#include <dlib/memory.h>
#include <dlib/static_assert.h>
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
    , m_SdfSpread(1.0f)
    , m_SdfOffset(0)
    , m_SdfOutline(0)
    , m_CacheWidth(0)
    , m_CacheHeight(0)
    , m_GlyphChannels(1)
    , m_GlyphData(0)
    , m_CacheCellWidth(0)
    , m_CacheCellHeight(0)
    , m_CacheCellPadding(0)
    , m_ImageFormat(dmRenderDDF::TYPE_BITMAP)
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
        float                   m_SdfSpread;
        float                   m_SdfOffset;
        float                   m_SdfOutline;
        float                   m_Alpha;
        float                   m_OutlineAlpha;
        float                   m_ShadowAlpha;

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

    static float GetLineTextMetrics(HFontMap font_map, float tracking, const char* text, int n);

    static void InitFontmap(FontMapParams& params, dmGraphics::TextureParams& tex_params, uint8_t init_val)
    {
        uint8_t bpp = params.m_GlyphChannels;
        uint32_t data_size = tex_params.m_Width * tex_params.m_Height * bpp;
        tex_params.m_Data = malloc(data_size);
        tex_params.m_DataSize = data_size;
        memset((void*)tex_params.m_Data, init_val, tex_params.m_DataSize);
    }

    static void CleanupFontmap(dmGraphics::TextureParams& tex_params)
    {
        free((void*)tex_params.m_Data);
        tex_params.m_DataSize = 0;
    }

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
        font_map->m_SdfSpread = params.m_SdfSpread;
        font_map->m_SdfOffset = params.m_SdfOffset;
        font_map->m_SdfOutline = params.m_SdfOutline;
        font_map->m_Alpha = params.m_Alpha;
        font_map->m_OutlineAlpha = params.m_OutlineAlpha;
        font_map->m_ShadowAlpha = params.m_ShadowAlpha;

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

        InitFontmap(params, tex_params, 0);
        dmGraphics::SetTexture(font_map->m_Texture, tex_params);
        CleanupFontmap(tex_params);

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
        font_map->m_SdfSpread = params.m_SdfSpread;
        font_map->m_SdfOffset = params.m_SdfOffset;
        font_map->m_SdfOutline = params.m_SdfOutline;
        font_map->m_Alpha = params.m_Alpha;
        font_map->m_OutlineAlpha = params.m_OutlineAlpha;
        font_map->m_ShadowAlpha = params.m_ShadowAlpha;

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

        InitFontmap(params, tex_params, 0);
        dmGraphics::SetTexture(font_map->m_Texture, tex_params);
        CleanupFontmap(tex_params);
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
        DM_STATIC_ASSERT(sizeof(GlyphVertex) % 16 == 0, Invalid_Struct_Size);
        DM_STATIC_ASSERT( MAX_FONT_RENDER_CONSTANTS == MAX_TEXT_RENDER_CONSTANTS, Constant_Arrays_Must_Have_Same_Size );

        TextContext& text_context = render_context->m_TextContext;

        text_context.m_MaxVertexCount = max_characters * 6; // 6 vertices per character
        uint32_t buffer_size = sizeof(GlyphVertex) * text_context.m_MaxVertexCount;
        text_context.m_ClientBuffer = 0x0;
        text_context.m_VertexIndex = 0;
        text_context.m_VerticesFlushed = 0;
        text_context.m_Frame = 0;
        text_context.m_TextEntriesFlushed = 0;

        dmMemory::Result r = dmMemory::AlignedMalloc((void**)&text_context.m_ClientBuffer, 16, buffer_size);
        if (r != dmMemory::RESULT_OK) {
            dmLogError("Could not allocate text vertex buffer (%d).", r);
            return;
        }

        dmGraphics::VertexElement ve[] =
        {
                {"position", 0, 4, dmGraphics::TYPE_FLOAT, false},
                {"texcoord0", 1, 2, dmGraphics::TYPE_FLOAT, false},
                {"face_color", 2, 4, dmGraphics::TYPE_UNSIGNED_BYTE, true},
                {"outline_color", 3, 4, dmGraphics::TYPE_UNSIGNED_BYTE, true},
                {"shadow_color", 4, 4, dmGraphics::TYPE_UNSIGNED_BYTE, true},
                {"sdf_params", 5, 4, dmGraphics::TYPE_FLOAT, false},
        };

        text_context.m_VertexDecl = dmGraphics::NewVertexDeclaration(render_context->m_GraphicsContext, ve, sizeof(ve) / sizeof(dmGraphics::VertexElement), sizeof(GlyphVertex));
        text_context.m_VertexBuffer = dmGraphics::NewVertexBuffer(render_context->m_GraphicsContext, buffer_size, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

        // Arbitrary number
        const uint32_t max_batches = 128;
        text_context.m_RenderObjects.SetCapacity(max_batches);
        text_context.m_RenderObjectIndex = 0;

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
        dmMemory::AlignedFree(text_context.m_ClientBuffer);
        dmGraphics::DeleteVertexBuffer(text_context.m_VertexBuffer);
        dmGraphics::DeleteVertexDeclaration(text_context.m_VertexDecl);
    }

    DrawTextParams::DrawTextParams()
    : m_WorldTransform(Matrix4::identity())
    , m_FaceColor(0.0f, 0.0f, 0.0f, -1.0f)
    , m_OutlineColor(0.0f, 0.0f, 0.0f, -1.0f)
    , m_ShadowColor(0.0f, 0.0f, 0.0f, -1.0f)
    , m_Text(0x0)
    , m_SourceBlendFactor(dmGraphics::BLEND_FACTOR_ONE)
    , m_DestinationBlendFactor(dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
    , m_RenderOrder(0)
    , m_NumRenderConstants(0)
    , m_Width(FLT_MAX)
    , m_Height(0)
    , m_Leading(1.0f)
    , m_Tracking(0.0f)
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
        float m_Tracking;
        LayoutMetrics(HFontMap font_map, float tracking) : m_FontMap(font_map), m_Tracking(tracking) {}
        float operator()(const char* text, uint32_t n)
        {
            return GetLineTextMetrics(m_FontMap, m_Tracking, text, n);
        }
    };

    static dmhash_t g_TextureSizeRecipHash = dmHashString64("texture_size_recip");

    void DrawText(HRenderContext render_context, HFontMap font_map, HMaterial material, uint64_t batch_key, const DrawTextParams& params)
    {
        DM_PROFILE(Render, "DrawText");

        TextContext* text_context = &render_context->m_TextContext;

        if (text_context->m_TextEntries.Full()) {
            dmLogWarning("Out of text-render entries: %u", text_context->m_TextEntries.Capacity());
            return;
        }

        // The gui doesn't currently generate a batch key for each gui node, but instead rely on this being generated by the DrawText
        // The label component however, generates a batchkey when the label changes (which is usually not every frame)
        if( batch_key == 0 )
        {
            HashState64 key_state;
            dmHashInit64(&key_state, false);
            dmHashUpdateBuffer64(&key_state, &font_map, sizeof(font_map));
            dmHashUpdateBuffer64(&key_state, &params.m_RenderOrder, sizeof(params.m_RenderOrder));
            if (params.m_StencilTestParamsSet) {
                dmHashUpdateBuffer64(&key_state, &params.m_StencilTestParams, sizeof(params.m_StencilTestParams));
            }
            if (material) {
                dmHashUpdateBuffer64(&key_state, &material, sizeof(material));
            }
            batch_key = dmHashFinal64(&key_state);
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
        te.m_Material = material ? material : GetFontMapMaterial(font_map);
        te.m_BatchKey = batch_key;
        te.m_Next = -1;
        te.m_Tail = -1;

        te.m_FaceColor = dmGraphics::PackRGBA(Vector4(params.m_FaceColor.getXYZ(), params.m_FaceColor.getW() * font_map->m_Alpha));
        te.m_OutlineColor = dmGraphics::PackRGBA(Vector4(params.m_OutlineColor.getXYZ(), params.m_OutlineColor.getW() * font_map->m_OutlineAlpha));
        te.m_ShadowColor = dmGraphics::PackRGBA(Vector4(params.m_ShadowColor.getXYZ(), params.m_ShadowColor.getW() * font_map->m_ShadowAlpha));
        te.m_RenderOrder = params.m_RenderOrder;
        te.m_Width = params.m_Width;
        te.m_Height = params.m_Height;
        te.m_Leading = params.m_Leading;
        te.m_Tracking = params.m_Tracking;
        te.m_LineBreak = params.m_LineBreak;
        te.m_Align = params.m_Align;
        te.m_VAlign = params.m_VAlign;
        te.m_StencilTestParams = params.m_StencilTestParams;
        te.m_StencilTestParamsSet = params.m_StencilTestParamsSet;
        te.m_SourceBlendFactor = params.m_SourceBlendFactor;
        te.m_DestinationBlendFactor = params.m_DestinationBlendFactor;

        assert( params.m_NumRenderConstants <= dmRender::MAX_FONT_RENDER_CONSTANTS );
        te.m_NumRenderConstants = params.m_NumRenderConstants;
        memcpy( te.m_RenderConstants, params.m_RenderConstants, params.m_NumRenderConstants * sizeof(dmRender::Constant));

        text_context->m_TextEntries.Push(te);
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

    void AddGlyphToCache(HFontMap font_map, TextContext& text_context, Glyph* g) {
        uint32_t prev_cache_cursor = font_map->m_CacheCursor;
        dmGraphics::TextureParams tex_params;
        tex_params.m_SubUpdate = true;
        tex_params.m_MipMap = 0;
        tex_params.m_Format = font_map->m_CacheFormat;
        tex_params.m_MinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        tex_params.m_MagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;

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
                dmGraphics::SetTexture(font_map->m_Texture, tex_params);
                break;
            }

        } while (prev_cache_cursor != font_map->m_CacheCursor);

        if (prev_cache_cursor == font_map->m_CacheCursor) {
            dmLogError("Out of available cache cells! Consider increasing cache_width or cache_height for the font.");
        }
    }

    static int CreateFontVertexDataInternal(TextContext& text_context, HFontMap font_map, const char* text, const TextEntry& te, float recip_w, float recip_h, GlyphVertex* vertices, uint32_t num_vertices)
    {
        float width = te.m_Width;
        if (!te.m_LineBreak) {
            width = FLT_MAX;
        }
        float line_height = font_map->m_MaxAscent + font_map->m_MaxDescent;
        float leading = line_height * te.m_Leading;
        float tracking = line_height * te.m_Tracking;

        const uint32_t max_lines = 128;
        TextLine lines[max_lines];

        LayoutMetrics lm(font_map, tracking);
        float layout_width;
        int line_count = Layout(text, width, lines, max_lines, &layout_width, lm);
        float x_offset = OffsetX(te.m_Align, te.m_Width);
        float y_offset = OffsetY(te.m_VAlign, te.m_Height, font_map->m_MaxAscent, font_map->m_MaxDescent, te.m_Leading, line_count);

        uint32_t face_color = te.m_FaceColor;
        uint32_t outline_color = te.m_OutlineColor;
        uint32_t shadow_color = te.m_ShadowColor;

        // No support for non-uniform scale with SDF so just peek at the first
        // row to extract scale factor. The purpose of this scaling is to have
        // world space distances in the computation, for good 'anti aliasing' no matter
        // what scale is being rendered in.
        const Vectormath::Aos::Vector4 r0 = te.m_Transform.getRow(0);
        const float sdf_edge_value = 0.75f;
        float sdf_world_scale = sqrtf(r0.getX() * r0.getX() + r0.getY() * r0.getY());
        float sdf_scale   = font_map->m_SdfSpread;
        float sdf_outline = font_map->m_SdfOutline;
        // For anti-aliasing, 0.25 represents the single-axis radius of half a pixel.
        float sdf_smoothing = 0.25f / (font_map->m_SdfSpread * sdf_world_scale);

        uint32_t vertexindex = 0;
        for (int line = 0; line < line_count; ++line) {
            TextLine& l = lines[line];
            int16_t x = (int16_t)(x_offset - OffsetX(te.m_Align, l.m_Width) + 0.5f);
            int16_t y = (int16_t) (y_offset - line * leading + 0.5f);
            const char* cursor = &text[l.m_Index];
            int n = l.m_Count;
            for (int j = 0; j < n; ++j)
            {
                uint32_t c = dmUtf8::NextChar(&cursor);

                Glyph* g =  GetGlyph(font_map, c);
                if (!g) {
                    continue;
                }

                if (vertexindex + 6 >= num_vertices)
                {
                    dmLogWarning("Fontrenderer: character buffer exceeded (size: %d)", num_vertices / 6);
                    return vertexindex;
                }
                if (g->m_Width > 0)
                {

                    if (!g->m_InCache) {
                        AddGlyphToCache(font_map, text_context, g);
                    }

                    if (g->m_InCache) {
                        g->m_Frame = text_context.m_Frame;

                        GlyphVertex& v1 = vertices[vertexindex];
                        GlyphVertex& v2 = *(&v1 + 1);
                        GlyphVertex& v3 = *(&v1 + 2);
                        GlyphVertex& v4 = *(&v1 + 3);
                        GlyphVertex& v5 = *(&v1 + 4);
                        GlyphVertex& v6 = *(&v1 + 5);
                        vertexindex += 6;

                        int16_t width = (int16_t)g->m_Width;
                        int16_t descent = (int16_t)g->m_Descent;
                        int16_t ascent = (int16_t)g->m_Ascent;

                        (Vector4&) v1.m_Position = te.m_Transform * Vector4(x + g->m_LeftBearing, y - descent, 0, 1);
                        (Vector4&) v2.m_Position = te.m_Transform * Vector4(x + g->m_LeftBearing, y + ascent, 0, 1);
                        (Vector4&) v3.m_Position = te.m_Transform * Vector4(x + g->m_LeftBearing + width, y - descent, 0, 1);
                        (Vector4&) v6.m_Position = te.m_Transform * Vector4(x + g->m_LeftBearing + width, y + ascent, 0, 1);

                        v1.m_UV[0] = (g->m_X + font_map->m_CacheCellPadding) * recip_w;
                        v1.m_UV[1] = (g->m_Y + font_map->m_CacheCellPadding + ascent + descent) * recip_h;

                        v2.m_UV[0] = (g->m_X + font_map->m_CacheCellPadding) * recip_w;
                        v2.m_UV[1] = (g->m_Y + font_map->m_CacheCellPadding) * recip_h;

                        v3.m_UV[0] = (g->m_X + font_map->m_CacheCellPadding + g->m_Width) * recip_w;
                        v3.m_UV[1] = (g->m_Y + font_map->m_CacheCellPadding + ascent + descent) * recip_h;

                        v6.m_UV[0] = (g->m_X + font_map->m_CacheCellPadding + g->m_Width) * recip_w;
                        v6.m_UV[1] = (g->m_Y + font_map->m_CacheCellPadding) * recip_h;

                        #define SET_VERTEX_PARAMS(v) \
                            v.m_FaceColor = face_color; \
                            v.m_OutlineColor = outline_color; \
                            v.m_ShadowColor = shadow_color; \
                            v.m_SdfParams[0] = sdf_edge_value; \
                            v.m_SdfParams[1] = sdf_outline; \
                            v.m_SdfParams[2] = sdf_smoothing; \
                            v.m_SdfParams[3] = sdf_scale; \

                        SET_VERTEX_PARAMS(v1)
                        SET_VERTEX_PARAMS(v2)
                        SET_VERTEX_PARAMS(v3)
                        SET_VERTEX_PARAMS(v6)

                        #undef SET_VERTEX_PARAMS

                        v4 = v3;
                        v5 = v2;
                    }
                }
                x += (int16_t)(g->m_Advance + tracking);
            }
        }

        return vertexindex;
    }

    static void CreateFontRenderBatch(HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(Render, "CreateFontRenderBatch");
        TextContext& text_context = render_context->m_TextContext;

        const TextEntry& first_te = *(TextEntry*) buf[*begin].m_UserData;

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
        ro->m_SourceBlendFactor = first_te.m_SourceBlendFactor;
        ro->m_DestinationBlendFactor = first_te.m_DestinationBlendFactor;
        ro->m_SetBlendFactors = 1;
        ro->m_Material = first_te.m_Material;
        ro->m_Textures[0] = font_map->m_Texture;
        ro->m_VertexStart = text_context.m_VertexIndex;
        ro->m_StencilTestParams = first_te.m_StencilTestParams;
        ro->m_SetStencilTest = first_te.m_StencilTestParamsSet;

        Vector4 texture_size_recip(im_recip, ih_recip, 0, 0);
        EnableRenderObjectConstant(ro, g_TextureSizeRecipHash, texture_size_recip);

        const dmRender::Constant* constants = first_te.m_RenderConstants;
        uint32_t size = first_te.m_NumRenderConstants;
        for (uint32_t i = 0; i < size; ++i)
        {
            const dmRender::Constant& c = constants[i];
            dmRender::EnableRenderObjectConstant(ro, c.m_NameHash, c.m_Value);
        }

        for (uint32_t *i = begin;i != end; ++i)
        {
            const TextEntry& te = *(TextEntry*) buf[*i].m_UserData;
            const char* text = &text_context.m_TextBuffer[te.m_StringOffset];

            int num_indices = CreateFontVertexDataInternal(text_context, font_map, text, te, im_recip, ih_recip, &vertices[text_context.m_VertexIndex], text_context.m_MaxVertexCount - text_context.m_VertexIndex);
            text_context.m_VertexIndex += num_indices;            
        }

        ro->m_VertexCount = text_context.m_VertexIndex - ro->m_VertexStart;

        dmRender::AddToRender(render_context, ro);
    }

    static void FontRenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        HRenderContext render_context = (HRenderContext)params.m_UserData;
        TextContext& text_context = render_context->m_TextContext;

        switch (params.m_Operation)
        {
            case dmRender::RENDER_LIST_OPERATION_BEGIN:
                text_context.m_RenderObjectIndex = 0;
                text_context.m_VertexIndex = 0;
                text_context.m_TextEntriesFlushed = 0;
                break;
            case dmRender::RENDER_LIST_OPERATION_END:
                {
                    uint32_t buffer_size = sizeof(GlyphVertex) * text_context.m_VertexIndex;
                    dmGraphics::SetVertexBufferData(text_context.m_VertexBuffer, 0, 0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
                    dmGraphics::SetVertexBufferData(text_context.m_VertexBuffer, buffer_size, text_context.m_ClientBuffer, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
                    text_context.m_VerticesFlushed = text_context.m_VertexIndex;
                    DM_COUNTER("FontVertexBuffer", buffer_size);
                }
                break;
            default:
                assert(params.m_Operation == dmRender::RENDER_LIST_OPERATION_BATCH);
                CreateFontRenderBatch(render_context, params.m_Buf, params.m_Begin, params.m_End);
        }
    }


    void FlushTexts(HRenderContext render_context, uint32_t major_order, uint32_t render_order, bool final)
    {
        DM_PROFILE(Render, "FlushTexts");

        (void)final;
        TextContext& text_context = render_context->m_TextContext;

        if (text_context.m_TextEntries.Size() > 0) {
            uint32_t count = text_context.m_TextEntries.Size() - text_context.m_TextEntriesFlushed;

            if (count > 0) {
                dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, count);
                dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &FontRenderListDispatch, render_context);
                dmRender::RenderListEntry* write_ptr = render_list;

                for( uint32_t i = 0; i < count; ++i )
                {
                    TextEntry& te = text_context.m_TextEntries[text_context.m_TextEntriesFlushed + i];
                    write_ptr->m_WorldPosition = Point3(te.m_Transform.getTranslation());
                    write_ptr->m_MinorOrder = 0;
                    write_ptr->m_MajorOrder = major_order;
                    write_ptr->m_Order = render_order;
                    write_ptr->m_UserData = (uintptr_t) &te; // The text entry must live until the dispatch is done
                    write_ptr->m_BatchKey = te.m_BatchKey;
                    write_ptr->m_TagMask = dmRender::GetMaterialTagMask(te.m_Material);
                    write_ptr->m_Dispatch = dispatch;
                    write_ptr++;
                }
                dmRender::RenderListSubmit(render_context, render_list, write_ptr);
            }
        }
        
        // Always update after flushing
        text_context.m_TextEntriesFlushed = text_context.m_TextEntries.Size();
    }

    static float GetLineTextMetrics(HFontMap font_map, float tracking, const char* text, int n)
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
            width += (int16_t) (g->m_Advance + tracking);
        }
        if (n > 0 && 0 != last)
        {
            float last_end_point = last->m_LeftBearing + last->m_Width;
            float last_right_bearing = last->m_Advance - last_end_point;
            width = width - last_right_bearing - tracking;
        }

        return width;
    }

    void GetTextMetrics(HFontMap font_map, const char* text, float width, bool line_break, float leading, float tracking, TextMetrics* metrics)
    {
        metrics->m_MaxAscent = font_map->m_MaxAscent;
        metrics->m_MaxDescent = font_map->m_MaxDescent;

        if (!line_break) {
            width = FLT_MAX;
        }

        const uint32_t max_lines = 128;
        dmRender::TextLine lines[max_lines];

        float line_height = font_map->m_MaxAscent + font_map->m_MaxDescent;

        LayoutMetrics lm(font_map, tracking * line_height);
        float layout_width;
        uint32_t num_lines = Layout(text, width, lines, max_lines, &layout_width, lm);
        metrics->m_Width = layout_width;
        metrics->m_Height = num_lines * (line_height * leading) - line_height * (leading - 1.0f);
    }
}
