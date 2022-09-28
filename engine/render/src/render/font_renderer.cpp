// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <string.h>
#include <math.h>
#include <float.h>
#include <dmsdk/vectormath/cpp/vectormath_aos.h>

#include <dlib/align.h>
#include <dlib/memory.h>
#include <dlib/static_assert.h>
#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>
#include <dlib/hashtable.h>
#include <dlib/utf8.h>
#include <dlib/zlib.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/intersection.h>
#include <graphics/graphics_util.h>

#include "font_renderer.h"
#include "font_renderer_private.h"

#include "render_private.h"
#include "render/font_ddf.h"
#include "render.h"

DM_PROPERTY_EXTERN(rmtp_Render);
DM_PROPERTY_U32(rmtp_FontCharacterCount, 0, FrameReset, "# glyphs", &rmtp_Render);
DM_PROPERTY_U32(rmtp_FontVertexSize, 0, FrameReset, "size of vertices in bytes", &rmtp_Render);

namespace dmRender
{
    using namespace dmVMath;

    // https://en.wikipedia.org/wiki/Delta_encoding
    static void delta_decode(uint8_t* buffer, int length)
    {
        uint8_t last = 0;
        for (int i = 0; i < length; i++)
        {
            uint8_t delta = buffer[i];
            buffer[i] = delta + last;
            last = buffer[i];
        }
    }

    enum RenderLayerMask
    {
        FACE    = 0x1,
        OUTLINE = 0x2,
        SHADOW  = 0x4
    };

    FontMapParams::FontMapParams()
    : m_Glyphs()
    , m_ShadowX(0.0f)
    , m_ShadowY(0.0f)
    , m_MaxAscent(0.0f)
    , m_MaxDescent(0.0f)
    , m_SdfSpread(1.0f)
    , m_SdfOffset(0)
    , m_SdfOutline(0)
    , m_SdfShadow(0)
    , m_CacheWidth(0)
    , m_CacheHeight(0)
    , m_GlyphChannels(1)
    , m_GlyphData(0)
    , m_CacheCellWidth(0)
    , m_CacheCellHeight(0)
    , m_CacheCellPadding(0)
    , m_LayerMask(FACE)
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
        , m_CellTempData(0)
        , m_CacheCellWidth(0)
        , m_CacheCellHeight(0)
        , m_CacheCellMaxAscent(0)
        , m_CacheCellPadding(0)
        , m_LayerMask(FACE)
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
            if (m_CellTempData) {
                free(m_CellTempData);
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
        float                   m_SdfShadow;
        float                   m_Alpha;
        float                   m_OutlineAlpha;
        float                   m_ShadowAlpha;

        uint32_t                m_CacheWidth;
        uint32_t                m_CacheHeight;
        void*                   m_GlyphData;

        Glyph**                 m_Cache;
        uint32_t                m_CacheCursor;
        dmGraphics::TextureFormat m_CacheFormat;
        dmGraphics::TextureFilter m_MinFilter;
        dmGraphics::TextureFilter m_MagFilter;

        uint32_t                m_CacheColumns;
        uint32_t                m_CacheRows;

        uint8_t*                m_CellTempData; // a temporary unpack buffer for the compressed glyphs

        uint32_t                m_CacheCellWidth;
        uint32_t                m_CacheCellHeight;
        uint32_t                m_CacheCellMaxAscent;
        uint8_t                 m_CacheCellPadding;
        uint8_t                 m_LayerMask;
    };

    static float GetLineTextMetrics(HFontMap font_map, float tracking, const char* text, int n, bool measure_trailing_space);

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

    // Font maps have no mips, so we need to make sure we use a supported min filter
    static dmGraphics::TextureFilter ConvertMinTextureFilter(dmGraphics::TextureFilter filter)
    {
        if (filter == dmGraphics::TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST)
        {
            filter = dmGraphics::TEXTURE_FILTER_NEAREST;
        }
        else if (filter == dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST)
        {
            filter = dmGraphics::TEXTURE_FILTER_LINEAR;
        }

        return filter;
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
        font_map->m_SdfShadow = params.m_SdfShadow;
        font_map->m_Alpha = params.m_Alpha;
        font_map->m_OutlineAlpha = params.m_OutlineAlpha;
        font_map->m_ShadowAlpha = params.m_ShadowAlpha;
        font_map->m_LayerMask = params.m_LayerMask;

        font_map->m_CacheWidth = params.m_CacheWidth;
        font_map->m_CacheHeight = params.m_CacheHeight;
        font_map->m_GlyphData = params.m_GlyphData;

        font_map->m_CacheCellWidth = params.m_CacheCellWidth;
        font_map->m_CacheCellHeight = params.m_CacheCellHeight;
        font_map->m_CacheCellMaxAscent = params.m_CacheCellMaxAscent;
        font_map->m_CacheCellPadding = params.m_CacheCellPadding;

        font_map->m_CacheColumns = params.m_CacheWidth / params.m_CacheCellWidth;
        font_map->m_CacheRows = params.m_CacheHeight / params.m_CacheCellHeight;
        uint32_t cell_count = font_map->m_CacheColumns * font_map->m_CacheRows;

        font_map->m_CellTempData = (uint8_t*)malloc(font_map->m_CacheCellWidth*font_map->m_CacheCellHeight*4);

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

        if (params.m_ImageFormat == dmRenderDDF::TYPE_BITMAP)
        {
            dmGraphics::GetDefaultTextureFilters(graphics_context, font_map->m_MinFilter, font_map->m_MagFilter);
            // No mips for font cache
            font_map->m_MinFilter = ConvertMinTextureFilter(font_map->m_MinFilter);
        }
        else // Distance-field font
        {
            font_map->m_MinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
            font_map->m_MagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        }

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
            free(font_map->m_CellTempData);
        }

        font_map->m_ShadowX = params.m_ShadowX;
        font_map->m_ShadowY = params.m_ShadowY;
        font_map->m_MaxAscent = params.m_MaxAscent;
        font_map->m_MaxDescent = params.m_MaxDescent;
        font_map->m_SdfSpread = params.m_SdfSpread;
        font_map->m_SdfOffset = params.m_SdfOffset;
        font_map->m_SdfOutline = params.m_SdfOutline;
        font_map->m_SdfShadow = params.m_SdfShadow;
        font_map->m_Alpha = params.m_Alpha;
        font_map->m_OutlineAlpha = params.m_OutlineAlpha;
        font_map->m_ShadowAlpha = params.m_ShadowAlpha;
        font_map->m_LayerMask = params.m_LayerMask;

        font_map->m_CacheWidth = params.m_CacheWidth;
        font_map->m_CacheHeight = params.m_CacheHeight;
        font_map->m_GlyphData = params.m_GlyphData;

        font_map->m_CacheCellWidth = params.m_CacheCellWidth;
        font_map->m_CacheCellHeight = params.m_CacheCellHeight;
        font_map->m_CacheCellMaxAscent = params.m_CacheCellMaxAscent;
        font_map->m_CacheCellPadding = params.m_CacheCellPadding;

        font_map->m_CacheColumns = params.m_CacheWidth / params.m_CacheCellWidth;
        font_map->m_CacheRows = params.m_CacheHeight / params.m_CacheCellHeight;
        uint32_t cell_count = font_map->m_CacheColumns * font_map->m_CacheRows;

        font_map->m_CellTempData = (uint8_t*)malloc(font_map->m_CacheCellWidth*font_map->m_CacheCellHeight*4);

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
        text_context.m_PreviousFrame = ~0;
        text_context.m_TextEntriesFlushed = 0;

        dmMemory::Result r = dmMemory::AlignedMalloc((void**)&text_context.m_ClientBuffer, 16, buffer_size);
        if (r != dmMemory::RESULT_OK) {
            dmLogError("Could not allocate text vertex buffer of size %u (%d).", buffer_size, r);
            return;
        }

        dmGraphics::VertexElement ve[] =
        {
                {"position", 0, 4, dmGraphics::TYPE_FLOAT, false},
                {"texcoord0", 1, 2, dmGraphics::TYPE_FLOAT, false},
                {"face_color", 2, 4, dmGraphics::TYPE_FLOAT, true},
                {"outline_color", 3, 4, dmGraphics::TYPE_FLOAT, true},
                {"shadow_color", 4, 4, dmGraphics::TYPE_FLOAT, true},
                {"sdf_params", 5, 4, dmGraphics::TYPE_FLOAT, false},
                {"layer_mask", 6, 3, dmGraphics::TYPE_FLOAT, false},
        };

        text_context.m_VertexDecl = dmGraphics::NewVertexDeclaration(render_context->m_GraphicsContext, ve, sizeof(ve) / sizeof(dmGraphics::VertexElement), sizeof(GlyphVertex));
        text_context.m_VertexBuffer = dmGraphics::NewVertexBuffer(render_context->m_GraphicsContext, buffer_size, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

        // Arbitrary number
        const uint32_t max_batches = 128;
        text_context.m_ConstantBuffers.SetCapacity(max_batches); // 1:1 index mapping with render object
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
            text_context.m_ConstantBuffers.Push(dmRender::NewNamedConstantBuffer());
        }
    }

    void FinalizeTextContext(HRenderContext render_context)
    {
        TextContext& text_context = render_context->m_TextContext;
        for (uint32_t i = 0; i < text_context.m_ConstantBuffers.Size(); ++i)
        {
            dmRender::DeleteNamedConstantBuffer(text_context.m_ConstantBuffers[i]);
        }
        dmMemory::AlignedFree(text_context.m_ClientBuffer);
        dmGraphics::DeleteVertexBuffer(text_context.m_VertexBuffer);
        dmGraphics::DeleteVertexDeclaration(text_context.m_VertexDecl);
    }

    DrawTextParams::DrawTextParams()
    : m_WorldTransform(Matrix4::identity())
    , m_FaceColor(0.0f, 0.0f, 0.0f, 1.0f)
    , m_OutlineColor(0.0f, 0.0f, 0.0f, 1.0f)
    , m_ShadowColor(0.0f, 0.0f, 0.0f, 1.0f)
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
        float operator()(const char* text, uint32_t n, bool measure_trailing_space)
        {
            return GetLineTextMetrics(m_FontMap, m_Tracking, text, n, measure_trailing_space);
        }
    };

    static dmhash_t g_TextureSizeRecipHash = dmHashString64("texture_size_recip");

    static dmVMath::Point3 CalcCenterPoint(HFontMap font_map, const TextEntry& te, const TextMetrics& metrics) {
        float x_offset = OffsetX(te.m_Align, te.m_Width);
        float y_offset = OffsetY(te.m_VAlign, te.m_Height, font_map->m_MaxAscent, font_map->m_MaxDescent, te.m_Leading, metrics.m_LineCount);

        // find X,Y local coordinate of text center
        float center_x = x_offset; // start from the X position of the pivot point
        switch (te.m_Align) {
            case TEXT_ALIGN_LEFT:
                center_x += metrics.m_Width/2; // move halfway to the right since we're aligning left
            break;
            case TEXT_ALIGN_RIGHT:
                center_x -= metrics.m_Height/2; // move halfway to the left from pivot since we're aligning right
            break;
            // nothing to do for TEXT_ALIGN_CENTER. Pivot is already at the center of the text X-wise
        }
        float center_y = y_offset + font_map->m_MaxAscent - metrics.m_Height/2; // 'y_offset' to move to the baseline of first letter in text. +'MaAscent' to get to the top of the text. -layout_height to get to the center.

        dmVMath::Point3 center_point(center_x, center_y, 0); // fix at Z 0
        return center_point;
    }

    void DrawText(HRenderContext render_context, HFontMap font_map, HMaterial material, uint64_t batch_key, const DrawTextParams& params)
    {
        DM_PROFILE("DrawText");

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
            dmLogWarning("Out of text-render buffer %u. Modify the graphics.max_characters in game.project.", text_context->m_TextBuffer.Capacity());
            return;
        }

        text_context->m_TextBuffer.PushArray(params.m_Text, text_len);
        text_context->m_TextBuffer.Push('\0');

        material = material ? material : GetFontMapMaterial(font_map);
        TextEntry te;
        te.m_Transform = params.m_WorldTransform;
        te.m_StringOffset = offset;
        te.m_FontMap = font_map;
        te.m_Material = material;
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

        TextMetrics metrics;
        GetTextMetrics(font_map, params.m_Text, params.m_Width, params.m_LineBreak, params.m_Leading, params.m_Tracking, &metrics);

        // find center and radius for frustum culling
        dmVMath::Point3 centerpoint_local = CalcCenterPoint(font_map, te, metrics);
        dmVMath::Point3 cornerpoint_local(centerpoint_local.getX() + metrics.m_Width/2, centerpoint_local.getY() + metrics.m_Height/2, centerpoint_local.getZ());
        dmVMath::Vector4 centerpoint_world = te.m_Transform * centerpoint_local; // transform to world coordinates
        dmVMath::Vector4 cornerpoint_world = te.m_Transform * cornerpoint_local;

        te.m_FrustumCullingRadius = Vectormath::Aos::length(cornerpoint_world - centerpoint_world);
        te.m_FrustumCullingCenter = dmVMath::Point3(centerpoint_world.getXYZ());


        assert( params.m_NumRenderConstants <= dmRender::MAX_FONT_RENDER_CONSTANTS );
        te.m_NumRenderConstants = params.m_NumRenderConstants;
        memcpy( te.m_RenderConstants, params.m_RenderConstants, params.m_NumRenderConstants * sizeof(dmRender::HConstant));

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

    struct FontGlyphInflaterContext {
        uint32_t m_Cursor;
        uint8_t* m_Output;
    };

    static bool FontGlyphInflater(void* context, const void* data, uint32_t data_len)
    {
        FontGlyphInflaterContext* ctx = (FontGlyphInflaterContext*)context;
        memcpy(ctx->m_Output + ctx->m_Cursor, data, data_len);
        ctx->m_Cursor += data_len;
        return true;
    }

    void AddGlyphToCache(HFontMap font_map, TextContext& text_context, Glyph* g, int16_t g_offset_y) {
        uint32_t prev_cache_cursor = font_map->m_CacheCursor;
        dmGraphics::TextureParams tex_params;
        tex_params.m_SubUpdate = true;
        tex_params.m_MipMap = 0;
        tex_params.m_Format = font_map->m_CacheFormat;
        tex_params.m_MinFilter = font_map->m_MinFilter;
        tex_params.m_MagFilter = font_map->m_MagFilter;

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
                tex_params.m_Width = g->m_Width + font_map->m_CacheCellPadding*2;
                tex_params.m_Height = g->m_Ascent + g->m_Descent + font_map->m_CacheCellPadding*2;

                uint8_t* glyph_data = (uint8_t*)(uint8_t*)font_map->m_GlyphData + g->m_GlyphDataOffset;
                uint32_t glyph_data_size = g->m_GlyphDataSize-1; // The first byte is a header
                uint8_t compression_type = *glyph_data++;

                if (compression_type) {

                    // When if came to choosing between the different algorithms, here are some speed/compression tests
                    // Decoding 100 glyphs
                    // lz4:     0.1060 ms  compression: 72%
                    // deflate: 0.2190 ms  compression: 66%
                    // png:     0.6930 ms  compression: 67%
                    // webp:    1.5170 ms  compression: 55%
                    // further improvements (different test, Android, 92 glyphs)
                    // webp          2.9440 ms  compression: 55%
                    // deflate       0.7110 ms  compression: 66%
                    // deflate+delta 0.7680 ms  compression: 62%

                    FontGlyphInflaterContext deflate_context;
                    deflate_context.m_Output = font_map->m_CellTempData;
                    deflate_context.m_Cursor = 0;
                    dmZlib::Result zlib_result = dmZlib::InflateBuffer(glyph_data, glyph_data_size, &deflate_context, FontGlyphInflater);
                    if (zlib_result != dmZlib::RESULT_OK)
                    {
                        dmLogError("Failed to decompress glyph (%c)", g->m_Character);
                        return;
                    }

                    uint32_t uncompressed_size = deflate_context.m_Cursor;
                    delta_decode(font_map->m_CellTempData, uncompressed_size);

                    tex_params.m_Data = font_map->m_CellTempData;
                } else {
                    tex_params.m_Data = glyph_data;
                }

                tex_params.m_X = g->m_X;
                tex_params.m_Y = g->m_Y + g_offset_y;

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

        // Trailing space characters should be ignored when measuring and
        // rendering multiline text.
        // For single line text we still want to include spaces when the text
        // layout is calculated (https://github.com/defold/defold/issues/5911)
        bool measure_trailing_space = !te.m_LineBreak;

        LayoutMetrics lm(font_map, tracking);
        float layout_width;
        int line_count = Layout(text, width, lines, max_lines, &layout_width, lm, measure_trailing_space);
        float x_offset = OffsetX(te.m_Align, te.m_Width);
        float y_offset = OffsetY(te.m_VAlign, te.m_Height, font_map->m_MaxAscent, font_map->m_MaxDescent, te.m_Leading, line_count);

        const Vector4 face_color    = dmGraphics::UnpackRGBA(te.m_FaceColor);
        const Vector4 outline_color = dmGraphics::UnpackRGBA(te.m_OutlineColor);
        const Vector4 shadow_color  = dmGraphics::UnpackRGBA(te.m_ShadowColor);

        // No support for non-uniform scale with SDF so just peek at the first
        // row to extract scale factor. The purpose of this scaling is to have
        // world space distances in the computation, for good 'anti aliasing' no matter
        // what scale is being rendered in.
        const Vector4 r0 = te.m_Transform.getRow(0);
        const float sdf_edge_value = 0.75f;
        float sdf_world_scale = sqrtf(r0.getX() * r0.getX() + r0.getY() * r0.getY());
        float sdf_outline = font_map->m_SdfOutline;
        float sdf_shadow  = font_map->m_SdfShadow;
        // For anti-aliasing, 0.25 represents the single-axis radius of half a pixel.
        float sdf_smoothing = 0.25f / (font_map->m_SdfSpread * sdf_world_scale);

        uint32_t vertexindex        = 0;
        uint32_t valid_glyph_count  = 0;
        uint8_t  vertices_per_quad  = 6;
        uint8_t  layer_count        = 1;
        uint8_t  layer_mask         = font_map->m_LayerMask;

        #define HAS_LAYER(mask,layer) ((mask & layer) == layer)

        if (!HAS_LAYER(layer_mask, FACE))
        {
            dmLogError("Encountered invalid layer mask when rendering font!");
            return 0;
        }

        // Vertex buffer consume strategy:
        // * For single-layered approach, we do as per usual and consume vertices based on offset 0.
        // * For the layered approach, we need to place vertices in sorted order from
        //     back to front layer in the order of shadow -> outline -> face, where the offset of each
        //     layer depends on how many glyphs we actually can place in the buffer. To get a valid count, we
        //     do a dry run first over the input string and place glyphs in the cache if they are renderable.
        if (HAS_LAYER(layer_mask,OUTLINE) || HAS_LAYER(layer_mask,SHADOW))
        {
            layer_count += HAS_LAYER(layer_mask,OUTLINE) + HAS_LAYER(layer_mask,SHADOW);

            // Calculate number of valid glyphs
            for (int line = 0; line < line_count; ++line)
            {
                TextLine& l = lines[line];
                const char* cursor = &text[l.m_Index];
                bool inner_break = false;

                for (int j = 0; j < l.m_Count; ++j)
                {
                    uint32_t c = dmUtf8::NextChar(&cursor);
                    Glyph* g   = GetGlyph(font_map, c);

                    if (!g)
                    {
                        continue;
                    }

                    if ((vertexindex + vertices_per_quad) * layer_count > num_vertices)
                    {
                        inner_break = true;
                        break;
                    }

                    if (g->m_Width > 0)
                    {
                        int16_t px_cell_offset_y = font_map->m_CacheCellMaxAscent - (int16_t)g->m_Ascent;

                        // Prepare the cache here aswell since we only count glyphs we definitely
                        // will render.
                        if (!g->m_InCache)
                        {
                            AddGlyphToCache(font_map, text_context, g, px_cell_offset_y);
                        }

                        if (g->m_InCache)
                        {
                            valid_glyph_count++;

                            vertexindex += vertices_per_quad;
                        }
                    }
                }

                if (inner_break)
                {
                    break;
                }
            }

            vertexindex = 0;
        }

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

                // Look ahead and see if we can produce vertices for the next glyph or not
                if ((vertexindex + vertices_per_quad) * layer_count > num_vertices)
                {
                    dmLogWarning("Character buffer exceeded (size: %d), increase the \"graphics.max_characters\" property in your game.project file.", num_vertices / 6);
                    return vertexindex * layer_count;
                }

                if (g->m_Width > 0)
                {
                    int16_t width   = (int16_t) g->m_Width;
                    int16_t descent = (int16_t) g->m_Descent;
                    int16_t ascent  = (int16_t) g->m_Ascent;

                    // Calculate y-offset in cache-cell space by moving glyphs down to baseline
                    int16_t px_cell_offset_y = font_map->m_CacheCellMaxAscent - ascent;

                    if (!g->m_InCache) {
                        AddGlyphToCache(font_map, text_context, g, px_cell_offset_y);
                    }

                    if (g->m_InCache) {
                        g->m_Frame = text_context.m_Frame;

                        uint32_t face_index = vertexindex + vertices_per_quad * valid_glyph_count * (layer_count-1);

                        // Set face vertices first, this will always hold since we can't have less than 1 layer
                        GlyphVertex& v1_layer_face = vertices[face_index];
                        GlyphVertex& v2_layer_face = vertices[face_index + 1];
                        GlyphVertex& v3_layer_face = vertices[face_index + 2];
                        GlyphVertex& v4_layer_face = vertices[face_index + 3];
                        GlyphVertex& v5_layer_face = vertices[face_index + 4];
                        GlyphVertex& v6_layer_face = vertices[face_index + 5];

                        (Vector4&) v1_layer_face.m_Position = te.m_Transform * Vector4(x + g->m_LeftBearing, y - descent, 0, 1);
                        (Vector4&) v2_layer_face.m_Position = te.m_Transform * Vector4(x + g->m_LeftBearing, y + ascent, 0, 1);
                        (Vector4&) v3_layer_face.m_Position = te.m_Transform * Vector4(x + g->m_LeftBearing + width, y - descent, 0, 1);
                        (Vector4&) v6_layer_face.m_Position = te.m_Transform * Vector4(x + g->m_LeftBearing + width, y + ascent, 0, 1);

                        v1_layer_face.m_UV[0] = (g->m_X + font_map->m_CacheCellPadding) * recip_w;
                        v1_layer_face.m_UV[1] = (g->m_Y + font_map->m_CacheCellPadding + ascent + descent + px_cell_offset_y) * recip_h;

                        v2_layer_face.m_UV[0] = (g->m_X + font_map->m_CacheCellPadding) * recip_w;
                        v2_layer_face.m_UV[1] = (g->m_Y + font_map->m_CacheCellPadding + px_cell_offset_y) * recip_h;

                        v3_layer_face.m_UV[0] = (g->m_X + font_map->m_CacheCellPadding + g->m_Width) * recip_w;
                        v3_layer_face.m_UV[1] = (g->m_Y + font_map->m_CacheCellPadding + ascent + descent + px_cell_offset_y) * recip_h;

                        v6_layer_face.m_UV[0] = (g->m_X + font_map->m_CacheCellPadding + g->m_Width) * recip_w;
                        v6_layer_face.m_UV[1] = (g->m_Y + font_map->m_CacheCellPadding + px_cell_offset_y) * recip_h;

                        #define SET_VERTEX_FONT_PROPERTIES(v) \
                            v.m_FaceColor[0]    = face_color[0]; \
                            v.m_FaceColor[1]    = face_color[1]; \
                            v.m_FaceColor[2]    = face_color[2]; \
                            v.m_FaceColor[3]    = face_color[3]; \
                            v.m_OutlineColor[0] = outline_color[0]; \
                            v.m_OutlineColor[1] = outline_color[1]; \
                            v.m_OutlineColor[2] = outline_color[2]; \
                            v.m_OutlineColor[3] = outline_color[3]; \
                            v.m_ShadowColor[0]  = shadow_color[0]; \
                            v.m_ShadowColor[1]  = shadow_color[1]; \
                            v.m_ShadowColor[2]  = shadow_color[2]; \
                            v.m_ShadowColor[3]  = shadow_color[3]; \
                            v.m_FaceColor[0]    = face_color[0]; \
                            v.m_FaceColor[1]    = face_color[1]; \
                            v.m_FaceColor[2]    = face_color[2]; \
                            v.m_FaceColor[3]    = face_color[3]; \
                            v.m_SdfParams[0]    = sdf_edge_value; \
                            v.m_SdfParams[1]    = sdf_outline; \
                            v.m_SdfParams[2]    = sdf_smoothing; \
                            v.m_SdfParams[3]    = sdf_shadow;

                        SET_VERTEX_FONT_PROPERTIES(v1_layer_face)
                        SET_VERTEX_FONT_PROPERTIES(v2_layer_face)
                        SET_VERTEX_FONT_PROPERTIES(v3_layer_face)
                        SET_VERTEX_FONT_PROPERTIES(v6_layer_face)

                        #undef SET_VERTEX_FONT_PROPERTIES

                        v4_layer_face = v3_layer_face;
                        v5_layer_face = v2_layer_face;

                        #define SET_VERTEX_LAYER_MASK(v,f,o,s) \
                            v.m_LayerMasks[0] = f; \
                            v.m_LayerMasks[1] = o; \
                            v.m_LayerMasks[2] = s;

                        // Set outline vertices
                        if (HAS_LAYER(layer_mask,OUTLINE))
                        {
                            uint32_t outline_index = vertexindex + vertices_per_quad * valid_glyph_count * (layer_count-2);

                            GlyphVertex& v1_layer_outline = vertices[outline_index];
                            GlyphVertex& v2_layer_outline = vertices[outline_index + 1];
                            GlyphVertex& v3_layer_outline = vertices[outline_index + 2];
                            GlyphVertex& v4_layer_outline = vertices[outline_index + 3];
                            GlyphVertex& v5_layer_outline = vertices[outline_index + 4];
                            GlyphVertex& v6_layer_outline = vertices[outline_index + 5];

                            v1_layer_outline = v1_layer_face;
                            v2_layer_outline = v2_layer_face;
                            v3_layer_outline = v3_layer_face;
                            v4_layer_outline = v4_layer_face;
                            v5_layer_outline = v5_layer_face;
                            v6_layer_outline = v6_layer_face;

                            SET_VERTEX_LAYER_MASK(v1_layer_outline,0,1,0)
                            SET_VERTEX_LAYER_MASK(v2_layer_outline,0,1,0)
                            SET_VERTEX_LAYER_MASK(v3_layer_outline,0,1,0)
                            SET_VERTEX_LAYER_MASK(v4_layer_outline,0,1,0)
                            SET_VERTEX_LAYER_MASK(v5_layer_outline,0,1,0)
                            SET_VERTEX_LAYER_MASK(v6_layer_outline,0,1,0)
                        }

                        // Set shadow vertices
                        if (HAS_LAYER(layer_mask,SHADOW))
                        {
                            uint32_t shadow_index = vertexindex;
                            float shadow_x        = font_map->m_ShadowX;
                            float shadow_y        = font_map->m_ShadowY;

                            GlyphVertex& v1_layer_shadow = vertices[shadow_index];
                            GlyphVertex& v2_layer_shadow = vertices[shadow_index + 1];
                            GlyphVertex& v3_layer_shadow = vertices[shadow_index + 2];
                            GlyphVertex& v4_layer_shadow = vertices[shadow_index + 3];
                            GlyphVertex& v5_layer_shadow = vertices[shadow_index + 4];
                            GlyphVertex& v6_layer_shadow = vertices[shadow_index + 5];

                            v1_layer_shadow = v1_layer_face;
                            v2_layer_shadow = v2_layer_face;
                            v3_layer_shadow = v3_layer_face;
                            v6_layer_shadow = v6_layer_face;

                            // Shadow offsets must be calculated since we need to offset in local space (before vertex transformation)
                            (Vector4&) v1_layer_shadow.m_Position = te.m_Transform * Vector4(x + g->m_LeftBearing + shadow_x, y - descent + shadow_y, 0, 1);
                            (Vector4&) v2_layer_shadow.m_Position = te.m_Transform * Vector4(x + g->m_LeftBearing + shadow_x, y + ascent + shadow_y, 0, 1);
                            (Vector4&) v3_layer_shadow.m_Position = te.m_Transform * Vector4(x + g->m_LeftBearing + shadow_x + width, y - descent + shadow_y, 0, 1);
                            (Vector4&) v6_layer_shadow.m_Position = te.m_Transform * Vector4(x + g->m_LeftBearing + shadow_x + width, y + ascent + shadow_y, 0, 1);

                            v4_layer_shadow = v3_layer_shadow;
                            v5_layer_shadow = v2_layer_shadow;

                            SET_VERTEX_LAYER_MASK(v1_layer_shadow,0,0,1)
                            SET_VERTEX_LAYER_MASK(v2_layer_shadow,0,0,1)
                            SET_VERTEX_LAYER_MASK(v3_layer_shadow,0,0,1)
                            SET_VERTEX_LAYER_MASK(v4_layer_shadow,0,0,1)
                            SET_VERTEX_LAYER_MASK(v5_layer_shadow,0,0,1)
                            SET_VERTEX_LAYER_MASK(v6_layer_shadow,0,0,1)
                        }

                        // If we only have one layer, we need to set the mask to (1,1,1)
                        // so that we can use the same calculations for both single and multi.
                        // The mask is set last for layer 1 since we copy the vertices to
                        // all other layers to avoid re-calculating their data.
                        uint8_t is_one_layer = layer_count > 1 ? 0 : 1;
                        SET_VERTEX_LAYER_MASK(v1_layer_face,1,is_one_layer,is_one_layer)
                        SET_VERTEX_LAYER_MASK(v2_layer_face,1,is_one_layer,is_one_layer)
                        SET_VERTEX_LAYER_MASK(v3_layer_face,1,is_one_layer,is_one_layer)
                        SET_VERTEX_LAYER_MASK(v4_layer_face,1,is_one_layer,is_one_layer)
                        SET_VERTEX_LAYER_MASK(v5_layer_face,1,is_one_layer,is_one_layer)
                        SET_VERTEX_LAYER_MASK(v6_layer_face,1,is_one_layer,is_one_layer)

                        #undef SET_VERTEX_LAYER_MASK

                        vertexindex += vertices_per_quad;
                    }
                }
                x += (int16_t)(g->m_Advance + tracking);
            }
        }

        #undef HAS_LAYER

        return vertexindex * layer_count;
    }

    static void CreateFontRenderBatch(HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("FontRenderBatch");
        TextContext& text_context = render_context->m_TextContext;

        const TextEntry& first_te = *(TextEntry*) buf[*begin].m_UserData;

        HFontMap font_map = first_te.m_FontMap;
        float im_recip = 1.0f;
        float ih_recip = 1.0f;
        float cache_cell_width_ratio  = 0.0;
        float cache_cell_height_ratio = 0.0;

        if (font_map->m_Texture) {
            float cache_width  = (float) dmGraphics::GetTextureWidth(font_map->m_Texture);
            float cache_height = (float) dmGraphics::GetTextureHeight(font_map->m_Texture);

            im_recip /= cache_width;
            ih_recip /= cache_height;

            cache_cell_width_ratio  = ((float) font_map->m_CacheCellWidth) / cache_width;
            cache_cell_height_ratio = ((float) font_map->m_CacheCellHeight) / cache_height;
        }

        GlyphVertex* vertices = (GlyphVertex*)text_context.m_ClientBuffer;

        if (text_context.m_RenderObjectIndex >= text_context.m_RenderObjects.Size()) {
            dmLogWarning("Fontrenderer: Render object count reached limit (%d)", text_context.m_RenderObjectIndex);
            return;
        }

        dmRender::HNamedConstantBuffer constants_buffer = text_context.m_ConstantBuffers[text_context.m_RenderObjectIndex];
        RenderObject* ro = &text_context.m_RenderObjects[text_context.m_RenderObjectIndex];
        text_context.m_RenderObjectIndex++;

        ro->m_SourceBlendFactor = first_te.m_SourceBlendFactor;
        ro->m_DestinationBlendFactor = first_te.m_DestinationBlendFactor;
        ro->m_SetBlendFactors = 1;
        ro->m_Material = first_te.m_Material;
        ro->m_Textures[0] = font_map->m_Texture;
        ro->m_VertexStart = text_context.m_VertexIndex;
        ro->m_StencilTestParams = first_te.m_StencilTestParams;
        ro->m_SetStencilTest = first_te.m_StencilTestParamsSet;

        Vector4 texture_size_recip(im_recip, ih_recip, cache_cell_width_ratio, cache_cell_height_ratio);

        dmRender::ClearNamedConstantBuffer(constants_buffer);
        dmRender::SetNamedConstants(constants_buffer, (HConstant*)first_te.m_RenderConstants, first_te.m_NumRenderConstants);
        dmRender::SetNamedConstant(constants_buffer, g_TextureSizeRecipHash, &texture_size_recip, 1);

        ro->m_ConstantBuffer = constants_buffer;

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
                break;
            case dmRender::RENDER_LIST_OPERATION_END:
                if (text_context.m_VerticesFlushed != text_context.m_VertexIndex)
                {
                    uint32_t buffer_size = sizeof(GlyphVertex) * text_context.m_VertexIndex;
                    dmGraphics::SetVertexBufferData(text_context.m_VertexBuffer, 0, 0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
                    dmGraphics::SetVertexBufferData(text_context.m_VertexBuffer, buffer_size, text_context.m_ClientBuffer, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

                    uint32_t num_vertices = text_context.m_VertexIndex - text_context.m_VerticesFlushed;
                    text_context.m_VerticesFlushed = text_context.m_VertexIndex;

                    DM_PROPERTY_ADD_U32(rmtp_FontCharacterCount, num_vertices / 6);
                    DM_PROPERTY_ADD_U32(rmtp_FontVertexSize, num_vertices * sizeof(GlyphVertex));
                }
                break;
            case dmRender::RENDER_LIST_OPERATION_BATCH:
                CreateFontRenderBatch(render_context, params.m_Buf, params.m_Begin, params.m_End);
                break;
            default:
                break;
        }
    }

    static void RenderListFrustumCulling(dmRender::RenderListVisibilityParams const &params)
    {
        DM_PROFILE("Label");

        const dmIntersection::Frustum frustum = *params.m_Frustum;
        uint32_t num_entries = params.m_NumEntries;
        for (uint32_t i = 0; i < num_entries; ++i)
        {
            dmRender::RenderListEntry* entry = &params.m_Entries[i];
            TextEntry* te = ((TextEntry*) entry->m_UserData);

            bool intersect = dmIntersection::TestFrustumSphere(frustum, te->m_FrustumCullingCenter, te->m_FrustumCullingRadius, true);
            entry->m_Visibility = intersect ? dmRender::VISIBILITY_FULL : dmRender::VISIBILITY_NONE;
        }
    }

    void FlushTexts(HRenderContext render_context, uint32_t major_order, uint32_t render_order, bool final)
    {
        DM_PROFILE("FlushTexts");

        (void)final;
        TextContext& text_context = render_context->m_TextContext;

        if (text_context.m_TextEntries.Size() > 0)
        {
            // This function is called for both game object text (labels) and gui
            // Once the sprites are done rendering, it is ok to reuse/reset the state
            // and the gui can start rendering its text
            // See also ClearRenderObjects() in render.cpp for clearing the text entries
            if (text_context.m_Frame != text_context.m_PreviousFrame)
            {
                text_context.m_PreviousFrame = text_context.m_Frame;
                text_context.m_RenderObjectIndex = 0;
                text_context.m_VertexIndex = 0;
                text_context.m_VerticesFlushed = 0;
                text_context.m_TextEntriesFlushed = 0;
            }

            uint32_t count = text_context.m_TextEntries.Size() - text_context.m_TextEntriesFlushed;

            if (count > 0) {
                dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, count);
                dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &FontRenderListDispatch, &RenderListFrustumCulling, render_context);
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
                    write_ptr->m_TagListKey = dmRender::GetMaterialTagListKey(te.m_Material);
                    write_ptr->m_Dispatch = dispatch;
                    write_ptr++;
                }
                dmRender::RenderListSubmit(render_context, render_list, write_ptr);
            }
        }

        // Always update after flushing
        text_context.m_TextEntriesFlushed = text_context.m_TextEntries.Size();
    }

    static float GetLineTextMetrics(HFontMap font_map, float tracking, const char* text, int n, bool measure_trailing_space)
    {
        float width = 0;
        const char* cursor = text;
        const Glyph* last = 0;
        for (int i = 0; i < n; ++i)
        {
            uint32_t c = dmUtf8::NextChar(&cursor);
            const Glyph* g = GetGlyph(font_map, c);
            if (!g) {
                continue;
            }

            last = g;
            // NOTE: We round advance here just as above in DrawText
            width += (int16_t) (g->m_Advance + tracking);
        }
        if (n > 0 && 0 != last)
        {
            uint32_t last_width = (measure_trailing_space && last->m_Character == ' ') ? (int16_t)last->m_Advance : last->m_Width;
            float last_end_point = last->m_LeftBearing + last_width;
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

        // Trailing space characters should be ignored when measuring and
        // rendering multiline text.
        // For single line text we still want to include spaces when the text
        // layout is calculated (https://github.com/defold/defold/issues/5911)
        bool measure_trailing_space = !line_break;

        LayoutMetrics lm(font_map, tracking * line_height);
        float layout_width;
        uint32_t num_lines = Layout(text, width, lines, max_lines, &layout_width, lm, measure_trailing_space);
        metrics->m_Width = layout_width;
        metrics->m_Height = num_lines * (line_height * leading) - line_height * (leading - 1.0f);
        metrics->m_LineCount = num_lines;
    }

    uint32_t GetFontMapResourceSize(HFontMap font_map)
    {
        uint32_t size = sizeof(FontMap);
        size += font_map->m_Glyphs.Capacity()*(sizeof(Glyph)+sizeof(uint32_t));
        size += dmGraphics::GetTextureResourceSize(font_map->m_Texture);
        return size;
    }

    bool VerifyFontMapMinFilter(dmRender::HFontMap font_map, dmGraphics::TextureFilter filter)
    {
        return font_map->m_MinFilter == filter;
    }

    bool VerifyFontMapMagFilter(dmRender::HFontMap font_map, dmGraphics::TextureFilter filter)
    {
        return font_map->m_MagFilter == filter;
    }
}
