// Copyright 2020-2026 The Defold Foundation
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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/image.h>
#include <dlib/math.h>
#include <dlib/utf8.h>
#include <dlib/vmath.h>
#include <dmsdk/font/fontcollection.h>
#include <dmsdk/font/text_layout.h>

#include "glyph_gen.h"
#include "glyph_vertex.h"
#include "font_renderer.h"

using dmVMath::Matrix4;
using dmVMath::Vector4;

static_assert(sizeof(void*) == 8, "The font renderer FFM ABI requires a 64-bit target");
static_assert(sizeof(FontRendererParams) == 72, "Unexpected FontRendererParams ABI layout");
static_assert(sizeof(FontRendererLayout) == 20, "Unexpected FontRendererLayout ABI layout");
static_assert(sizeof(FontRendererGlyph) == 48, "Unexpected FontRendererGlyph ABI layout");
static_assert(offsetof(FontRendererGlyph, m_Pixels) == 32, "Unexpected FontRendererGlyph ABI layout");
static_assert(sizeof(FontRendererProperties) == 80, "Unexpected FontRendererProperties ABI layout");
static_assert(sizeof(FontTexture) == 40, "Unexpected FontTexture ABI layout");
static_assert(offsetof(FontTexture, m_AtlasVersion) == 8, "Unexpected FontTexture ABI layout");

struct CachedGlyph
{
    FontGlyph m_Glyph;
    HFont     m_Font;
    uint64_t  m_Frame;
    uint32_t  m_GlyphIndex;
    uint16_t  m_X;
    uint16_t  m_Y;
};

struct TextureUpdateState
{
    TextureUpdateState()
        : m_X(0)
        , m_Y(0)
        , m_Width(0)
        , m_Height(0)
        , m_Full(false)
    {
    }

    uint32_t m_X;
    uint32_t m_Y;
    uint32_t m_Width;
    uint32_t m_Height;
    bool     m_Full;
};

struct FontRendererContext
{
    FontRendererContext()
        : m_Font(0)
        , m_Collection(0)
        , m_Atlas(0)
        , m_AtlasVersion(1)
        , m_Frame(0)
        , m_Hash(0)
        , m_Properties()
        , m_HasProperties(false)
        , m_HasText(false)
        , m_CellWidth(1)
        , m_CellHeight(1)
        , m_CellMaxAscent(0)
    {
    }

    HFont                  m_Font;
    HFontCollection        m_Collection;
    dmArray<CachedGlyph>   m_Glyphs;
    uint8_t*               m_Atlas;
    uint64_t               m_AtlasVersion;
    uint64_t               m_Frame;
    uint64_t               m_Hash;

    FontRendererProperties m_Properties;
    bool                   m_HasProperties;
    dmArray<uint32_t>      m_Codepoints;
    bool                   m_HasText;

    float                  m_Size;
    float                  m_SdfBasePadding;
    float                  m_SdfSpread;
    float                  m_SdfOutline;
    float                  m_SdfShadow;
    float                  m_OutlineWidth;
    float                  m_ShadowBlur;
    float                  m_ShadowX;
    float                  m_ShadowY;
    uint16_t               m_AtlasWidth;
    uint16_t               m_AtlasHeight;
    uint16_t               m_CellWidth;
    uint16_t               m_CellHeight;
    uint16_t               m_CellMaxAscent;
    uint8_t                m_CellPadding;
    uint8_t                m_SdfEdgeValue;
    uint8_t                m_Channels;
    uint8_t                m_LayerMask;
    bool                   m_OutputBitmap;
    bool                   m_Antialias;
    bool                   m_HasOutline;
    bool                   m_HasShadow;
};

static void DestroySession(FontRendererContext* session)
{
    if (!session)
        return;
    for (uint32_t i = 0; i < session->m_Glyphs.Size(); ++i)
        FontFreeGlyph(session->m_Glyphs[i].m_Font, &session->m_Glyphs[i].m_Glyph);
    free(session->m_Atlas);
    FontCollectionDestroy(session->m_Collection);
    FontDestroy(session->m_Font);
    delete session;
}

static bool RebuildAtlas(FontRendererContext* session)
{
    const uint32_t columns = session->m_AtlasWidth / session->m_CellWidth;
    const uint32_t rows = session->m_AtlasHeight / session->m_CellHeight;
    if (columns * rows < session->m_Glyphs.Size())
        return false;

    for (uint32_t i = 0; i < session->m_Glyphs.Size(); ++i)
    {
        const CachedGlyph& cached = session->m_Glyphs[i];
        const uint32_t     cell_x = (i % columns) * session->m_CellWidth;
        const uint32_t     cell_y = (i / columns) * session->m_CellHeight;
        const uint32_t     image_y = cell_y + session->m_CellPadding + session->m_CellMaxAscent - (uint16_t)cached.m_Glyph.m_Ascent;
        const uint32_t     width = cached.m_Glyph.m_Bitmap.m_Width;
        const uint32_t     height = cached.m_Glyph.m_Bitmap.m_Height;
        if (cell_x + session->m_CellPadding + width > session->m_AtlasWidth || image_y + height > session->m_AtlasHeight)
            return false;
    }

    memset(session->m_Atlas, 0, (size_t)session->m_AtlasWidth * session->m_AtlasHeight * session->m_Channels);
    for (uint32_t i = 0; i < session->m_Glyphs.Size(); ++i)
    {
        CachedGlyph&   cached = session->m_Glyphs[i];
        const uint32_t cell_x = (i % columns) * session->m_CellWidth;
        const uint32_t cell_y = (i / columns) * session->m_CellHeight;
        const uint32_t image_y = cell_y + session->m_CellPadding + session->m_CellMaxAscent - (uint16_t)cached.m_Glyph.m_Ascent;
        const uint32_t width = cached.m_Glyph.m_Bitmap.m_Width;
        const uint32_t height = cached.m_Glyph.m_Bitmap.m_Height;
        const uint32_t row_bytes = width * session->m_Channels;
        for (uint32_t y = 0; y < height; ++y)
        {
            memcpy(session->m_Atlas + ((image_y + y) * session->m_AtlasWidth + cell_x + session->m_CellPadding) * session->m_Channels,
                   cached.m_Glyph.m_Bitmap.m_Data + y * row_bytes,
                   row_bytes);
        }
        cached.m_X = cell_x;
        cached.m_Y = cell_y;
    }
    ++session->m_AtlasVersion;
    return true;
}

static bool WriteGlyphToAtlas(FontRendererContext* session, CachedGlyph* cached, uint32_t glyph_index)
{
    const uint32_t columns = session->m_AtlasWidth / session->m_CellWidth;
    if (columns == 0)
        return false;
    const uint32_t cell_x = (glyph_index % columns) * session->m_CellWidth;
    const uint32_t cell_y = (glyph_index / columns) * session->m_CellHeight;
    const uint32_t image_x = cell_x + session->m_CellPadding;
    const uint32_t image_y = cell_y + session->m_CellPadding + session->m_CellMaxAscent - (uint16_t)cached->m_Glyph.m_Ascent;
    const uint32_t width = cached->m_Glyph.m_Bitmap.m_Width;
    const uint32_t height = cached->m_Glyph.m_Bitmap.m_Height;
    const uint32_t row_bytes = width * session->m_Channels;
    if (image_x + width > session->m_AtlasWidth || image_y + height > session->m_AtlasHeight)
        return false;
    for (uint32_t y = 0; y < height; ++y)
    {
        memcpy(session->m_Atlas + ((image_y + y) * session->m_AtlasWidth + image_x) * session->m_Channels,
               cached->m_Glyph.m_Bitmap.m_Data + y * row_bytes,
               row_bytes);
    }
    cached->m_X = cell_x;
    cached->m_Y = cell_y;
    return true;
}

static void AddDirtyRect(TextureUpdateState* update, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (update->m_Width == 0 || update->m_Height == 0)
    {
        update->m_X = x;
        update->m_Y = y;
        update->m_Width = width;
        update->m_Height = height;
        return;
    }
    const uint32_t right = dmMath::Max(update->m_X + update->m_Width, x + width);
    const uint32_t bottom = dmMath::Max(update->m_Y + update->m_Height, y + height);
    update->m_X = dmMath::Min(update->m_X, x);
    update->m_Y = dmMath::Min(update->m_Y, y);
    update->m_Width = right - update->m_X;
    update->m_Height = bottom - update->m_Y;
}

static CachedGlyph* FindGlyph(FontRendererContext* session, HFont font, uint32_t glyph_index)
{
    for (uint32_t i = 0; i < session->m_Glyphs.Size(); ++i)
    {
        CachedGlyph& cached = session->m_Glyphs[i];
        if (cached.m_Font == font && cached.m_GlyphIndex == glyph_index)
            return &cached;
    }
    return 0;
}

static void GetGlyphGenParams(FontRendererContext* session, HFont font, FontGlyphGenParams* params)
{
    params->m_Scale = FontGetScaleFromSize(font, session->m_Size);
    params->m_SdfPadding = session->m_SdfBasePadding + session->m_OutlineWidth + session->m_ShadowBlur;
    params->m_SdfEdgeValue = session->m_SdfEdgeValue;
    params->m_OutlineWidth = session->m_OutlineWidth;
    params->m_ShadowBlur = session->m_ShadowBlur;
    params->m_OutputBitmap = session->m_OutputBitmap;
    params->m_Antialias = session->m_Antialias;
    params->m_HasOutline = session->m_HasOutline;
    params->m_HasShadow = session->m_HasShadow;
}

static bool UpdateCellMetrics(FontRendererContext* session)
{
    uint64_t cell_width = 1;
    uint32_t cell_max_ascent = 0;
    uint32_t cell_max_descent = 0;
    for (uint32_t i = 0; i < session->m_Glyphs.Size(); ++i)
    {
        const FontGlyph& glyph = session->m_Glyphs[i].m_Glyph;
        const uint64_t   glyph_cell_width = (uint64_t)glyph.m_Bitmap.m_Width + (uint32_t)session->m_CellPadding * 2;
        cell_width = dmMath::Max(cell_width, glyph_cell_width);
        if (glyph.m_Ascent < 0.0f || glyph.m_Descent < 0.0f)
            return false;
        cell_max_ascent = dmMath::Max(cell_max_ascent, (uint32_t)glyph.m_Ascent);
        cell_max_descent = dmMath::Max(cell_max_descent, (uint32_t)glyph.m_Descent);
    }
    const uint64_t cell_height = dmMath::Max((uint64_t)1, (uint64_t)cell_max_ascent + cell_max_descent + (uint32_t)session->m_CellPadding * 2);
    if (cell_width > UINT16_MAX || cell_height > UINT16_MAX || cell_max_ascent > UINT16_MAX)
        return false;
    session->m_CellWidth = (uint16_t)cell_width;
    session->m_CellHeight = (uint16_t)cell_height;
    session->m_CellMaxAscent = (uint16_t)cell_max_ascent;
    return true;
}

static int32_t FindOldestEvictableGlyph(FontRendererContext* session, HFont protected_font, uint32_t protected_glyph_index)
{
    int32_t  oldest_index = -1;
    uint64_t oldest_frame = UINT64_MAX;
    for (uint32_t i = 0; i < session->m_Glyphs.Size(); ++i)
    {
        const CachedGlyph& cached = session->m_Glyphs[i];
        const bool         protected_glyph = cached.m_Font == protected_font && cached.m_GlyphIndex == protected_glyph_index;
        const bool         used_in_current_batch = session->m_Frame != 0 && cached.m_Frame == session->m_Frame;
        if (!protected_glyph && !used_in_current_batch && cached.m_Frame < oldest_frame)
        {
            oldest_index = (int32_t)i;
            oldest_frame = cached.m_Frame;
        }
    }
    return oldest_index;
}

static void RemoveCachedGlyph(FontRendererContext* session, uint32_t index)
{
    CachedGlyph& cached = session->m_Glyphs[index];
    FontFreeGlyph(cached.m_Font, &cached.m_Glyph);
    session->m_Glyphs.EraseSwap(index);
}

static CachedGlyph* GetOrCreateGlyph(FontRendererContext* session, HFont font, uint32_t glyph_index, TextureUpdateState* texture_update)
{
    CachedGlyph* cached = FindGlyph(session, font, glyph_index);
    if (cached)
    {
        cached->m_Frame = session->m_Frame;
        return cached;
    }

    CachedGlyph new_glyph;
    memset(&new_glyph, 0, sizeof(new_glyph));
    new_glyph.m_Font = font;
    new_glyph.m_Frame = session->m_Frame;
    new_glyph.m_GlyphIndex = glyph_index;

    FontGlyphGenParams params;
    GetGlyphGenParams(session, font, &params);
    if (FontGenerateGlyph(font, glyph_index, &params, &new_glyph.m_Glyph) != FONT_RESULT_OK)
        return 0;

    const uint16_t old_cell_width = session->m_CellWidth;
    const uint16_t old_cell_height = session->m_CellHeight;
    const uint16_t old_cell_max_ascent = session->m_CellMaxAscent;
    if (session->m_Glyphs.Full())
        session->m_Glyphs.OffsetCapacity(32);
    session->m_Glyphs.Push(new_glyph);
    if (!UpdateCellMetrics(session))
    {
        RemoveCachedGlyph(session, session->m_Glyphs.Size() - 1);
        session->m_CellWidth = old_cell_width;
        session->m_CellHeight = old_cell_height;
        session->m_CellMaxAscent = old_cell_max_ascent;
        return 0;
    }
    const bool cell_changed = old_cell_width != session->m_CellWidth || old_cell_height != session->m_CellHeight || old_cell_max_ascent != session->m_CellMaxAscent;
    bool       atlas_updated;
    if (cell_changed)
    {
        atlas_updated = RebuildAtlas(session);
        texture_update->m_Full = true;
    }
    else
    {
        CachedGlyph* cached_glyph = &session->m_Glyphs.Back();
        atlas_updated = WriteGlyphToAtlas(session, cached_glyph, session->m_Glyphs.Size() - 1);
        if (atlas_updated)
        {
            ++session->m_AtlasVersion;
            AddDirtyRect(texture_update,
                         cached_glyph->m_X + session->m_CellPadding,
                         cached_glyph->m_Y + session->m_CellPadding + session->m_CellMaxAscent - (uint16_t)cached_glyph->m_Glyph.m_Ascent,
                         cached_glyph->m_Glyph.m_Bitmap.m_Width,
                         cached_glyph->m_Glyph.m_Bitmap.m_Height);
        }
    }
    if (!atlas_updated)
    {
        bool    evicted = false;
        int32_t eviction_index;
        while ((eviction_index = FindOldestEvictableGlyph(session, font, glyph_index)) >= 0)
        {
            RemoveCachedGlyph(session, (uint32_t)eviction_index);
            evicted = true;
            if (UpdateCellMetrics(session) && RebuildAtlas(session))
            {
                texture_update->m_Full = true;
                return FindGlyph(session, font, glyph_index);
            }
        }

        cached = FindGlyph(session, font, glyph_index);
        if (cached)
            RemoveCachedGlyph(session, (uint32_t)(cached - session->m_Glyphs.Begin()));
        UpdateCellMetrics(session);
        if (evicted)
        {
            RebuildAtlas(session);
            texture_update->m_Full = true;
        }
        else
        {
            session->m_CellWidth = old_cell_width;
            session->m_CellHeight = old_cell_height;
            session->m_CellMaxAscent = old_cell_max_ascent;
        }
        return 0;
    }
    return &session->m_Glyphs.Back();
}

static TextResult CreateLayout(FontRendererContext* session, const uint32_t* codepoints, uint32_t count, bool line_break, float width, float leading, float tracking, HTextLayout* layout)
{
    TextLayoutSettings settings = { 0 };
    settings.m_Size = session->m_Size;
    settings.m_Width = width;
    settings.m_Leading = leading;
    settings.m_Tracking = tracking;
    settings.m_LineBreak = line_break;
    return TextLayoutCreate(session->m_Collection, const_cast<uint32_t*>(codepoints), count, &settings, layout);
}

static TextResult CreateRetainedLayout(HFontRenderer renderer, HTextLayout* layout)
{
    const FontRendererProperties& properties = renderer->m_Properties;
    return CreateLayout(renderer,
                        renderer->m_Codepoints.Begin(),
                        renderer->m_Codepoints.Size(),
                        properties.m_LineBreak != 0,
                        properties.m_Width,
                        properties.m_Leading,
                        properties.m_Tracking,
                        layout);
}

static void UpdateStateHash(FontRendererContext* renderer)
{
    HashState64 hash_state;
    dmHashInit64(&hash_state, false);
    dmHashUpdateBuffer64(&hash_state, &renderer->m_HasProperties, sizeof(renderer->m_HasProperties));
    if (renderer->m_HasProperties)
        dmHashUpdateBuffer64(&hash_state, &renderer->m_Properties, sizeof(renderer->m_Properties));
    dmHashUpdateBuffer64(&hash_state, &renderer->m_HasText, sizeof(renderer->m_HasText));
    if (renderer->m_HasText)
    {
        const uint32_t codepoint_count = renderer->m_Codepoints.Size();
        dmHashUpdateBuffer64(&hash_state, &codepoint_count, sizeof(codepoint_count));
        const uint32_t max_chunk_codepoints = UINT32_MAX / sizeof(uint32_t);
        uint32_t       codepoint_offset = 0;
        while (codepoint_offset < codepoint_count)
        {
            const uint32_t chunk_codepoints = dmMath::Min(codepoint_count - codepoint_offset, max_chunk_codepoints);
            dmHashUpdateBuffer64(&hash_state, renderer->m_Codepoints.Begin() + codepoint_offset, chunk_codepoints * sizeof(uint32_t));
            codepoint_offset += chunk_codepoints;
        }
    }
    renderer->m_Hash = dmHashFinal64(&hash_state);
}

FontRendererResult FontRendererCreate(const char*               name,
                                      const uint8_t*            font_bytes,
                                      uint32_t                  font_byte_count,
                                      const FontRendererParams* params,
                                      HFontRenderer*            renderer)
{
    const uint32_t channels = params ? FontGetGlyphChannelCount(params->m_OutputBitmap, params->m_HasOutline, params->m_HasShadow, params->m_ShadowBlur) : 1;
    const uint64_t atlas_pixel_count = params ? (uint64_t)params->m_AtlasWidth * params->m_AtlasHeight * channels : 0;
    if (!name || !font_bytes || font_byte_count == 0 || !params || !renderer ||
        params->m_Size <= 0.0f || params->m_AtlasWidth == 0 || params->m_AtlasWidth > UINT16_MAX ||
        params->m_AtlasHeight == 0 || params->m_AtlasHeight > UINT16_MAX || params->m_CellPadding > UINT8_MAX ||
        params->m_SdfBasePadding <= 0.0f || params->m_SdfSpread <= 0.0f ||
        params->m_SdfEdgeValue == 0 || params->m_SdfEdgeValue > UINT8_MAX ||
        atlas_pixel_count > UINT32_MAX ||
        (params->m_LayerMask & ~(FONT_RENDERER_LAYER_FACE | FONT_RENDERER_LAYER_OUTLINE | FONT_RENDERER_LAYER_SHADOW)) != 0 ||
        (params->m_LayerMask & FONT_RENDERER_LAYER_FACE) == 0)
        return FONT_RENDERER_RESULT_INVALID_ARGUMENT;
    *renderer = 0;

    FontRendererContext* session = new FontRendererContext;
    session->m_Font = FontLoadFromMemory(name, const_cast<uint8_t*>(font_bytes), font_byte_count, true);
    if (!session->m_Font)
    {
        delete session;
        return FONT_RENDERER_RESULT_FONT_ERROR;
    }

    session->m_Collection = FontCollectionCreate();
    if (FontCollectionAddFont(session->m_Collection, session->m_Font) != FONT_RESULT_OK)
    {
        DestroySession(session);
        return FONT_RENDERER_RESULT_FONT_ERROR;
    }

    session->m_Size = params->m_Size;
    session->m_AtlasWidth = params->m_AtlasWidth;
    session->m_AtlasHeight = params->m_AtlasHeight;
    session->m_CellPadding = params->m_CellPadding;
    session->m_SdfBasePadding = params->m_SdfBasePadding;
    session->m_SdfEdgeValue = params->m_SdfEdgeValue;
    session->m_SdfSpread = params->m_SdfSpread;
    session->m_SdfOutline = params->m_SdfOutline;
    session->m_SdfShadow = params->m_SdfShadow;
    session->m_OutlineWidth = params->m_OutlineWidth;
    session->m_ShadowBlur = params->m_ShadowBlur;
    session->m_ShadowX = params->m_ShadowX;
    session->m_ShadowY = params->m_ShadowY;
    session->m_LayerMask = params->m_LayerMask;
    session->m_OutputBitmap = params->m_OutputBitmap != 0;
    session->m_Antialias = params->m_Antialias != 0;
    session->m_HasOutline = params->m_HasOutline != 0;
    session->m_HasShadow = params->m_HasShadow != 0;
    session->m_Channels = channels;
    session->m_Atlas = (uint8_t*)calloc((size_t)params->m_AtlasWidth * params->m_AtlasHeight, session->m_Channels);
    if (!session->m_Atlas)
    {
        DestroySession(session);
        return FONT_RENDERER_RESULT_OUT_OF_MEMORY;
    }
    *renderer = session;
    return FONT_RENDERER_RESULT_OK;
}

void FontRendererDestroy(HFontRenderer renderer)
{
    DestroySession(renderer);
}

FontRendererResult FontRendererMeasure(HFontRenderer       renderer,
                                       const uint32_t*     codepoints,
                                       uint32_t            codepoint_count,
                                       uint32_t            line_break,
                                       float               width,
                                       float               leading,
                                       float               tracking,
                                       FontRendererLayout* output)
{
    if (!renderer || (!codepoints && codepoint_count != 0) || !output)
        return FONT_RENDERER_RESULT_INVALID_ARGUMENT;
    HTextLayout layout = 0;
    TextResult  result = CreateLayout(renderer, codepoints, codepoint_count, line_break != 0, width, leading, tracking, &layout);
    if (result != TEXT_RESULT_OK)
        return FONT_RENDERER_RESULT_TEXT_ERROR;

    TextLayoutGetBounds(layout, &output->m_Width, &output->m_Height);
    const float scale = FontGetScaleFromSize(renderer->m_Font, renderer->m_Size);
    output->m_LineCount = TextLayoutGetLineCount(layout);
    output->m_MaxAscent = FontGetAscent(renderer->m_Font, scale);
    output->m_MaxDescent = -FontGetDescent(renderer->m_Font, scale);
    TextLayoutRelease(layout);
    return FONT_RENDERER_RESULT_OK;
}

FontRendererResult FontRendererGenerateGlyph(HFontRenderer renderer, uint32_t codepoint, FontRendererGlyph* output)
{
    if (!renderer || !output)
        return FONT_RENDERER_RESULT_INVALID_ARGUMENT;
    memset(output, 0, sizeof(*output));
    const uint32_t     glyph_index = FontGetGlyphIndex(renderer->m_Font, codepoint);

    FontGlyphGenParams params;
    GetGlyphGenParams(renderer, renderer->m_Font, &params);
    FontGlyph glyph;
    if (FontGenerateGlyph(renderer->m_Font, glyph_index, &params, &glyph) != FONT_RESULT_OK)
        return FONT_RENDERER_RESULT_GLYPH_ERROR;

    output->m_GlyphIndex = glyph.m_GlyphIndex;
    output->m_Width = glyph.m_Bitmap.m_Width;
    output->m_Height = glyph.m_Bitmap.m_Height;
    output->m_Channels = glyph.m_Bitmap.m_Channels;
    output->m_Advance = glyph.m_Advance;
    output->m_LeftBearing = glyph.m_LeftBearing;
    output->m_Ascent = glyph.m_Ascent;
    output->m_Descent = glyph.m_Descent;
    output->m_PixelCount = glyph.m_Bitmap.m_DataSize;
    output->m_Pixels = (uint8_t*)malloc(output->m_PixelCount);
    if (output->m_PixelCount != 0 && !output->m_Pixels)
    {
        FontFreeGlyph(renderer->m_Font, &glyph);
        return FONT_RENDERER_RESULT_OUT_OF_MEMORY;
    }
    if (output->m_PixelCount != 0)
        memcpy(output->m_Pixels, glyph.m_Bitmap.m_Data, output->m_PixelCount);
    FontFreeGlyph(renderer->m_Font, &glyph);
    return FONT_RENDERER_RESULT_OK;
}

void FontRendererFreeGlyph(FontRendererGlyph* glyph)
{
    if (!glyph)
        return;
    free(glyph->m_Pixels);
    memset(glyph, 0, sizeof(*glyph));
}

FontRendererResult FontRendererDecodeImage(const uint8_t*     image_bytes,
                                           uint32_t           image_byte_count,
                                           FontRendererImage* output)
{
    if (!image_bytes || image_byte_count == 0 || !output)
        return FONT_RENDERER_RESULT_INVALID_ARGUMENT;
    memset(output, 0, sizeof(*output));
    dmImage::HImage image = dmImage::NewImage(image_bytes, image_byte_count, false);
    if (!image)
        return FONT_RENDERER_RESULT_GLYPH_ERROR;
    const dmImage::Type type = dmImage::GetType(image);
    const uint32_t      channels = type == dmImage::TYPE_RGBA ? 4 :
         type == dmImage::TYPE_RGB                            ? 3 :
         type == dmImage::TYPE_LUMINANCE_ALPHA                ? 2 :
                                                                1;
    output->m_Width = dmImage::GetWidth(image);
    output->m_Height = dmImage::GetHeight(image);
    output->m_Channels = channels;
    const uint64_t pixel_count = (uint64_t)output->m_Width * output->m_Height * channels;
    if (pixel_count > UINT32_MAX)
    {
        dmImage::DeleteImage(image);
        return FONT_RENDERER_RESULT_OUT_OF_MEMORY;
    }
    output->m_PixelCount = (uint32_t)pixel_count;
    output->m_Pixels = (uint8_t*)malloc(output->m_PixelCount);
    if (!output->m_Pixels)
    {
        dmImage::DeleteImage(image);
        return FONT_RENDERER_RESULT_OUT_OF_MEMORY;
    }
    memcpy(output->m_Pixels, dmImage::GetData(image), output->m_PixelCount);
    dmImage::DeleteImage(image);
    return FONT_RENDERER_RESULT_OK;
}

void FontRendererFreeImage(FontRendererImage* image)
{
    if (!image)
        return;
    free(image->m_Pixels);
    memset(image, 0, sizeof(*image));
}

static float OffsetX(uint32_t align, float width)
{
    if (align == 1)
        return width * 0.5f;
    if (align == 2)
        return width;
    return 0.0f;
}

static float OffsetY(uint32_t align, float height, float ascent, float descent, float leading, uint32_t line_count)
{
    const float line_height = ascent + descent;
    if (align == 1)
        return height * 0.5f + (line_count * line_height * leading - line_height * (leading - 1.0f)) * 0.5f - ascent;
    if (align == 2)
        return line_height * leading * (line_count - 1) + descent;
    return height - ascent;
}

FontRendererResult FontRendererSetProperties(HFontRenderer renderer, const FontRendererProperties* properties)
{
    if (!renderer || !properties)
        return FONT_RENDERER_RESULT_INVALID_ARGUMENT;
    renderer->m_Properties = *properties;
    renderer->m_HasProperties = true;
    UpdateStateHash(renderer);
    return FONT_RENDERER_RESULT_OK;
}

FontRendererResult FontRendererSetText(HFontRenderer renderer, const uint32_t* codepoints, uint32_t codepoint_count)
{
    if (!renderer || (!codepoints && codepoint_count != 0))
        return FONT_RENDERER_RESULT_INVALID_ARGUMENT;
    if (renderer->m_Codepoints.Capacity() < codepoint_count)
        renderer->m_Codepoints.SetCapacity(codepoint_count);
    renderer->m_Codepoints.SetSize(codepoint_count);
    if (codepoint_count != 0)
        memcpy(renderer->m_Codepoints.Begin(), codepoints, (size_t)codepoint_count * sizeof(uint32_t));
    renderer->m_HasText = true;
    UpdateStateHash(renderer);
    return FONT_RENDERER_RESULT_OK;
}

uint64_t FontRendererHash(HFontRenderer renderer)
{
    return renderer ? renderer->m_Hash : 0;
}

FontRendererResult FontRendererBeginBatch(HFontRenderer renderer)
{
    if (!renderer)
        return FONT_RENDERER_RESULT_INVALID_ARGUMENT;
    ++renderer->m_Frame;
    if (renderer->m_Frame == 0)
    {
        renderer->m_Frame = 1;
        for (uint32_t i = 0; i < renderer->m_Glyphs.Size(); ++i)
            renderer->m_Glyphs[i].m_Frame = 0;
    }
    return FONT_RENDERER_RESULT_OK;
}

FontRendererResult FontRendererGenerateTexture(HFontRenderer renderer,
                                               uint64_t      known_atlas_version,
                                               FontTexture*  texture)
{
    if (!renderer || !renderer->m_HasProperties || !renderer->m_HasText || !texture)
        return FONT_RENDERER_RESULT_INVALID_ARGUMENT;
    memset(texture, 0, sizeof(*texture));

    HTextLayout layout = 0;
    TextResult  layout_result = CreateRetainedLayout(renderer, &layout);
    if (layout_result != TEXT_RESULT_OK)
        return FONT_RENDERER_RESULT_TEXT_ERROR;

    TextGlyph*         glyphs = TextLayoutGetGlyphs(layout);
    const uint32_t     glyph_count = TextLayoutGetGlyphCount(layout);
    const uint64_t     initial_atlas_version = renderer->m_AtlasVersion;

    TextureUpdateState texture_update = {};
    for (uint32_t i = 0; i < glyph_count; ++i)
    {
        TextGlyph& text_glyph = glyphs[i];
        if (dmUtf8::IsWhiteSpace(text_glyph.m_Codepoint))
            continue;
        GetOrCreateGlyph(renderer, text_glyph.m_Font, text_glyph.m_GlyphIndex, &texture_update);
    }
    TextLayoutRelease(layout);

    texture->m_AtlasVersion = renderer->m_AtlasVersion;
    if (known_atlas_version == renderer->m_AtlasVersion)
        return FONT_RENDERER_RESULT_OK;

    const bool     send_full = known_atlas_version != initial_atlas_version || texture_update.m_Full;
    const uint32_t update_x = send_full ? 0 : texture_update.m_X;
    const uint32_t update_y = send_full ? 0 : texture_update.m_Y;
    const uint32_t update_width = send_full ? renderer->m_AtlasWidth : texture_update.m_Width;
    const uint32_t update_height = send_full ? renderer->m_AtlasHeight : texture_update.m_Height;
    texture->m_X = update_x;
    texture->m_Y = update_y;
    texture->m_Width = update_width;
    texture->m_Height = update_height;
    texture->m_Channels = renderer->m_Channels;
    const uint64_t pixel_count = (uint64_t)update_width * update_height * renderer->m_Channels;
    if (pixel_count > UINT32_MAX)
        return FONT_RENDERER_RESULT_OUT_OF_MEMORY;
    texture->m_PixelCount = (uint32_t)pixel_count;
    texture->m_Pixels = (uint8_t*)malloc((size_t)pixel_count);
    if (texture->m_PixelCount != 0 && !texture->m_Pixels)
        return FONT_RENDERER_RESULT_OUT_OF_MEMORY;

    const uint32_t row_bytes = update_width * renderer->m_Channels;
    for (uint32_t y = 0; y < update_height; ++y)
    {
        memcpy(texture->m_Pixels + y * row_bytes,
               renderer->m_Atlas + ((update_y + y) * renderer->m_AtlasWidth + update_x) * renderer->m_Channels,
               row_bytes);
    }
    return FONT_RENDERER_RESULT_OK;
}

void FontRendererFreeTexture(FontTexture* texture)
{
    if (!texture)
        return;
    free(texture->m_Pixels);
    memset(texture, 0, sizeof(*texture));
}

struct VertexBufferMetrics
{
    uint32_t m_VisibleGlyphCount;
    uint32_t m_VertexCount;
    uint32_t m_VertexBufferSize;
};

static bool GetVertexBufferMetrics(HFontRenderer renderer, HTextLayout layout, VertexBufferMetrics* metrics)
{
    TextGlyph*     glyphs = TextLayoutGetGlyphs(layout);
    const uint32_t glyph_count = TextLayoutGetGlyphCount(layout);
    uint32_t       visible_glyph_count = 0;
    for (uint32_t i = 0; i < glyph_count; ++i)
    {
        if (!dmUtf8::IsWhiteSpace(glyphs[i].m_Codepoint) && FindGlyph(renderer, glyphs[i].m_Font, glyphs[i].m_GlyphIndex))
            ++visible_glyph_count;
    }

    const uint32_t layer_count = 1 + ((renderer->m_LayerMask & FONT_RENDER_LAYER_OUTLINE) != 0) + ((renderer->m_LayerMask & FONT_RENDER_LAYER_SHADOW) != 0);
    const uint64_t required_vertex_count = (uint64_t)visible_glyph_count * layer_count * 6;
    const uint64_t required_buffer_size = required_vertex_count * sizeof(FontGlyphVertex);
    if (required_vertex_count > UINT32_MAX || required_buffer_size > UINT32_MAX)
        return false;

    metrics->m_VisibleGlyphCount = visible_glyph_count;
    metrics->m_VertexCount = (uint32_t)required_vertex_count;
    metrics->m_VertexBufferSize = (uint32_t)required_buffer_size;
    return true;
}

FontRendererResult FontRendererGetVertexBufferSize(HFontRenderer renderer,
                                                   uint32_t*     vertex_count,
                                                   uint32_t*     vertex_buffer_size)
{
    if (!renderer || !renderer->m_HasProperties || !renderer->m_HasText || !vertex_count || !vertex_buffer_size)
        return FONT_RENDERER_RESULT_INVALID_ARGUMENT;
    *vertex_count = 0;
    *vertex_buffer_size = 0;

    HTextLayout layout = 0;
    TextResult  layout_result = CreateRetainedLayout(renderer, &layout);
    if (layout_result != TEXT_RESULT_OK)
        return FONT_RENDERER_RESULT_TEXT_ERROR;

    VertexBufferMetrics metrics;
    const bool          valid_size = GetVertexBufferMetrics(renderer, layout, &metrics);
    TextLayoutRelease(layout);
    if (valid_size)
    {
        *vertex_count = metrics.m_VertexCount;
        *vertex_buffer_size = metrics.m_VertexBufferSize;
    }
    return valid_size ? FONT_RENDERER_RESULT_OK : FONT_RENDERER_RESULT_OUT_OF_MEMORY;
}

FontRendererResult FontRendererGetVertices(HFontRenderer renderer,
                                           const float*  world_transform,
                                           uint8_t*      vertex_buffer,
                                           uint32_t      vertex_buffer_size)
{
    if (!renderer || !renderer->m_HasProperties || !renderer->m_HasText || !world_transform)
        return FONT_RENDERER_RESULT_INVALID_ARGUMENT;

    const FontRendererProperties& properties = renderer->m_Properties;
    HTextLayout                   layout = 0;
    TextResult                    layout_result = CreateRetainedLayout(renderer, &layout);
    if (layout_result != TEXT_RESULT_OK)
        return FONT_RENDERER_RESULT_TEXT_ERROR;

    TextGlyph*     glyphs = TextLayoutGetGlyphs(layout);
    TextLine*      lines = TextLayoutGetLines(layout);
    const uint32_t line_count = TextLayoutGetLineCount(layout);
    VertexBufferMetrics metrics;
    if (!GetVertexBufferMetrics(renderer, layout, &metrics))
    {
        TextLayoutRelease(layout);
        return FONT_RENDERER_RESULT_OUT_OF_MEMORY;
    }
    if (metrics.m_VertexBufferSize > vertex_buffer_size || (metrics.m_VertexBufferSize != 0 && !vertex_buffer))
    {
        TextLayoutRelease(layout);
        return FONT_RENDERER_RESULT_INVALID_ARGUMENT;
    }

    const uint32_t   layer_count = 1 + ((renderer->m_LayerMask & FONT_RENDER_LAYER_OUTLINE) != 0) + ((renderer->m_LayerMask & FONT_RENDER_LAYER_SHADOW) != 0);
    FontGlyphVertex* vertices = (FontGlyphVertex*)vertex_buffer;

    Matrix4          transform;
    transform.setCol0(Vector4(world_transform[0], world_transform[1], world_transform[2], world_transform[3]));
    transform.setCol1(Vector4(world_transform[4], world_transform[5], world_transform[6], world_transform[7]));
    transform.setCol2(Vector4(world_transform[8], world_transform[9], world_transform[10], world_transform[11]));
    transform.setCol3(Vector4(world_transform[12], world_transform[13], world_transform[14], world_transform[15]));
    Vector4     face_color(properties.m_FaceColor[0], properties.m_FaceColor[1], properties.m_FaceColor[2], properties.m_FaceColor[3]);
    Vector4     outline_color(properties.m_OutlineColor[0], properties.m_OutlineColor[1], properties.m_OutlineColor[2], properties.m_OutlineColor[3]);
    Vector4     shadow_color(properties.m_ShadowColor[0], properties.m_ShadowColor[1], properties.m_ShadowColor[2], properties.m_ShadowColor[3]);

    const float font_scale = FontGetScaleFromSize(renderer->m_Font, renderer->m_Size);
    const float max_ascent = FontGetAscent(renderer->m_Font, font_scale);
    const float max_descent = -FontGetDescent(renderer->m_Font, font_scale);
    const float line_height = max_ascent + max_descent;
    const float x_offset = OffsetX(properties.m_Align, properties.m_Width);
    const float y_offset = OffsetY(properties.m_VerticalAlign, properties.m_Height, max_ascent, max_descent, properties.m_Leading, line_count);
    const float smoothing = 0.25f / (renderer->m_SdfSpread * dmMath::Max(0.000001f, properties.m_SdfScale));
    uint32_t    vertex_index = 0;
    for (uint32_t line_index = 0; line_index < line_count; ++line_index)
    {
        TextLine& line = lines[line_index];
        if (line.m_Length == 0)
            continue;
        const float first_x = glyphs[line.m_Index].m_X;
        const float first_y = glyphs[line.m_Index].m_Y;
        const float line_x = x_offset - OffsetX(properties.m_Align, line.m_Width);
        const float line_y = y_offset - line_index * line_height * properties.m_Leading;
        for (uint32_t glyph_index = line.m_Index; glyph_index < line.m_Index + line.m_Length; ++glyph_index)
        {
            TextGlyph& text_glyph = glyphs[glyph_index];
            if (dmUtf8::IsWhiteSpace(text_glyph.m_Codepoint))
                continue;
            CachedGlyph* cached = FindGlyph(renderer, text_glyph.m_Font, text_glyph.m_GlyphIndex);
            if (!cached)
                continue;
            FontPackGlyphVertices(&cached->m_Glyph,
                                  1.0f / renderer->m_AtlasWidth,
                                  1.0f / renderer->m_AtlasHeight,
                                  cached->m_X,
                                  cached->m_Y,
                                  renderer->m_CellMaxAscent,
                                  renderer->m_CellPadding,
                                  layer_count,
                                  renderer->m_LayerMask,
                                  vertex_index,
                                  metrics.m_VisibleGlyphCount * 6,
                                  transform,
                                  line_x + text_glyph.m_X - first_x,
                                  line_y + text_glyph.m_Y - first_y,
                                  face_color,
                                  outline_color,
                                  shadow_color,
                                  0.75f,
                                  renderer->m_SdfOutline,
                                  smoothing,
                                  renderer->m_SdfShadow,
                                  renderer->m_ShadowX,
                                  renderer->m_ShadowY,
                                  true,
                                  vertices);
            vertex_index += 6;
        }
    }
    TextLayoutRelease(layout);
    return FONT_RENDERER_RESULT_OK;
}
