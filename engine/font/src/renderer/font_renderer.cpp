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

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <dlib/array.h>
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
static_assert(sizeof(FontRendererParams) == 56, "Unexpected FontRendererParams ABI layout");
static_assert(sizeof(FontRendererLayout) == 20, "Unexpected FontRendererLayout ABI layout");
static_assert(sizeof(FontRendererGlyph) == 48, "Unexpected FontRendererGlyph ABI layout");
static_assert(offsetof(FontRendererGlyph, m_Pixels) == 32, "Unexpected FontRendererGlyph ABI layout");
static_assert(sizeof(FontRendererRenderResult) == 64, "Unexpected FontRendererRenderResult ABI layout");
static_assert(offsetof(FontRendererRenderResult, m_AtlasVersion) == 16, "Unexpected FontRendererRenderResult ABI layout");
static_assert(offsetof(FontRendererRenderResult, m_TexturePixels) == 48, "Unexpected FontRendererRenderResult ABI layout");

struct CachedGlyph
{
    FontGlyph m_Glyph;
    HFont     m_Font;
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

struct FontRendererSession
{
    FontRendererSession()
        : m_Font(0)
        , m_Collection(0)
        , m_Atlas(0)
        , m_AtlasVersion(1)
        , m_CellWidth(1)
        , m_CellHeight(1)
        , m_CellMaxAscent(0)
    {
    }

    HFont                m_Font;
    HFontCollection      m_Collection;
    dmArray<CachedGlyph> m_Glyphs;
    uint8_t*             m_Atlas;
    uint64_t             m_AtlasVersion;

    float                m_Size;
    float                m_SdfBasePadding;
    float                m_SdfSpread;
    float                m_SdfOutline;
    float                m_SdfShadow;
    float                m_OutlineWidth;
    float                m_ShadowBlur;
    float                m_ShadowX;
    float                m_ShadowY;
    uint16_t             m_AtlasWidth;
    uint16_t             m_AtlasHeight;
    uint16_t             m_CellWidth;
    uint16_t             m_CellHeight;
    uint16_t             m_CellMaxAscent;
    uint8_t              m_CellPadding;
    uint8_t              m_SdfEdgeValue;
    uint8_t              m_Channels;
    uint8_t              m_LayerMask;
};

static void DestroySession(FontRendererSession* session)
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

static bool RebuildAtlas(FontRendererSession* session)
{
    memset(session->m_Atlas, 0, session->m_AtlasWidth * session->m_AtlasHeight * session->m_Channels);
    const uint32_t columns = session->m_AtlasWidth / session->m_CellWidth;
    const uint32_t rows = session->m_AtlasHeight / session->m_CellHeight;
    if (columns * rows < session->m_Glyphs.Size())
        return false;

    for (uint32_t i = 0; i < session->m_Glyphs.Size(); ++i)
    {
        CachedGlyph&   cached = session->m_Glyphs[i];
        const uint32_t cell_x = (i % columns) * session->m_CellWidth;
        const uint32_t cell_y = (i / columns) * session->m_CellHeight;
        const uint32_t image_y = cell_y + session->m_CellPadding + session->m_CellMaxAscent - (uint16_t)cached.m_Glyph.m_Ascent;
        const uint32_t width = cached.m_Glyph.m_Bitmap.m_Width;
        const uint32_t height = cached.m_Glyph.m_Bitmap.m_Height;
        const uint32_t row_bytes = width * session->m_Channels;
        if (cell_x + session->m_CellPadding + width > session->m_AtlasWidth || image_y + height > session->m_AtlasHeight)
            return false;
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

static bool WriteGlyphToAtlas(FontRendererSession* session, CachedGlyph* cached, uint32_t glyph_index)
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

static CachedGlyph* FindGlyph(FontRendererSession* session, HFont font, uint32_t glyph_index)
{
    for (uint32_t i = 0; i < session->m_Glyphs.Size(); ++i)
    {
        CachedGlyph& cached = session->m_Glyphs[i];
        if (cached.m_Font == font && cached.m_GlyphIndex == glyph_index)
            return &cached;
    }
    return 0;
}

static void GetGlyphGenParams(FontRendererSession* session, HFont font, FontGlyphGenParams* params)
{
    params->m_Scale = FontGetScaleFromSize(font, session->m_Size);
    params->m_SdfPadding = session->m_SdfBasePadding + session->m_OutlineWidth + session->m_ShadowBlur;
    params->m_SdfEdgeValue = session->m_SdfEdgeValue;
    params->m_OutlineWidth = session->m_OutlineWidth;
    params->m_ShadowBlur = session->m_ShadowBlur;
}

static CachedGlyph* GetOrCreateGlyph(FontRendererSession* session, HFont font, uint32_t glyph_index, TextureUpdateState* texture_update)
{
    CachedGlyph* cached = FindGlyph(session, font, glyph_index);
    if (cached)
        return cached;

    CachedGlyph new_glyph;
    memset(&new_glyph, 0, sizeof(new_glyph));
    new_glyph.m_Font = font;
    new_glyph.m_GlyphIndex = glyph_index;

    FontGlyphGenParams params;
    GetGlyphGenParams(session, font, &params);
    if (FontGenerateGlyph(font, glyph_index, &params, &new_glyph.m_Glyph) != FONT_RESULT_OK)
        return 0;

    const uint16_t old_cell_width = session->m_CellWidth;
    const uint16_t old_cell_height = session->m_CellHeight;
    const uint16_t old_cell_max_ascent = session->m_CellMaxAscent;
    const uint16_t required_width = new_glyph.m_Glyph.m_Bitmap.m_Width + session->m_CellPadding * 2;
    const uint16_t required_height = new_glyph.m_Glyph.m_Bitmap.m_Height + session->m_CellPadding * 2;
    session->m_CellWidth = dmMath::Max(session->m_CellWidth, required_width);
    session->m_CellHeight = dmMath::Max(session->m_CellHeight, required_height);
    session->m_CellMaxAscent = dmMath::Max(session->m_CellMaxAscent, (uint16_t)new_glyph.m_Glyph.m_Ascent);
    if (session->m_Glyphs.Full())
        session->m_Glyphs.OffsetCapacity(32);
    session->m_Glyphs.Push(new_glyph);
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
        FontFreeGlyph(font, &session->m_Glyphs.Back().m_Glyph);
        session->m_Glyphs.Pop();
        session->m_CellWidth = old_cell_width;
        session->m_CellHeight = old_cell_height;
        session->m_CellMaxAscent = old_cell_max_ascent;
        return 0;
    }
    return &session->m_Glyphs.Back();
}

static TextResult CreateLayout(FontRendererSession* session, const uint32_t* codepoints, uint32_t count, bool line_break, float width, float leading, float tracking, HTextLayout* layout)
{
    TextLayoutSettings settings = { 0 };
    settings.m_Size = session->m_Size;
    settings.m_Width = width;
    settings.m_Leading = leading;
    settings.m_Tracking = tracking;
    settings.m_LineBreak = line_break;
    return TextLayoutCreate(session->m_Collection, const_cast<uint32_t*>(codepoints), count, &settings, layout);
}

FontRendererResult FontRendererCreate(const char*               name,
                                      const uint8_t*            font_bytes,
                                      uint32_t                  font_byte_count,
                                      const FontRendererParams* params,
                                      HFontRenderer*            renderer)
{
    if (!name || !font_bytes || font_byte_count == 0 || !params || !renderer ||
        params->m_Size <= 0.0f || params->m_AtlasWidth == 0 || params->m_AtlasWidth > UINT16_MAX ||
        params->m_AtlasHeight == 0 || params->m_AtlasHeight > UINT16_MAX || params->m_CellPadding > UINT8_MAX ||
        params->m_SdfBasePadding <= 0.0f || params->m_SdfSpread <= 0.0f ||
        params->m_SdfEdgeValue == 0 || params->m_SdfEdgeValue > UINT8_MAX ||
        (params->m_LayerMask & ~(FONT_RENDERER_LAYER_FACE | FONT_RENDERER_LAYER_OUTLINE | FONT_RENDERER_LAYER_SHADOW)) != 0 ||
        (params->m_LayerMask & FONT_RENDERER_LAYER_FACE) == 0)
        return FONT_RENDERER_RESULT_INVALID_ARGUMENT;
    *renderer = 0;

    FontRendererSession* session = new FontRendererSession;
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
    session->m_Channels = params->m_ShadowBlur > 0.0f ? 3 : 1;
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

FontRendererResult FontRendererRender(HFontRenderer             renderer,
                                      const uint32_t*           codepoints,
                                      uint32_t                  codepoint_count,
                                      uint32_t                  line_break,
                                      float                     width,
                                      float                     height,
                                      float                     leading,
                                      float                     tracking,
                                      uint32_t                  align,
                                      uint32_t                  vertical_align,
                                      const float*              transform_array,
                                      const float*              face_array,
                                      const float*              outline_array,
                                      const float*              shadow_array,
                                      float                     sdf_scale,
                                      uint64_t                  known_version,
                                      FontRendererRenderResult* output)
{
    if (!renderer || (!codepoints && codepoint_count != 0) || !transform_array || !face_array || !outline_array || !shadow_array || !output)
        return FONT_RENDERER_RESULT_INVALID_ARGUMENT;
    memset(output, 0, sizeof(*output));
    FontRendererSession* session = renderer;
    HTextLayout          layout = 0;
    TextResult           layout_result = CreateLayout(session, codepoints, codepoint_count, line_break != 0, width, leading, tracking, &layout);
    if (layout_result != TEXT_RESULT_OK)
        return FONT_RENDERER_RESULT_TEXT_ERROR;

    TextGlyph*     glyphs = TextLayoutGetGlyphs(layout);
    TextLine*      lines = TextLayoutGetLines(layout);
    const uint32_t glyph_count = TextLayoutGetGlyphCount(layout);
    const uint32_t line_count = TextLayoutGetLineCount(layout);
    uint32_t       visible_count = 0;
    for (uint32_t i = 0; i < glyph_count; ++i)
        visible_count += dmUtf8::IsWhiteSpace(glyphs[i].m_Codepoint) ? 0 : 1;
    const uint32_t     layer_count = 1 + ((session->m_LayerMask & FONT_RENDER_LAYER_OUTLINE) != 0) + ((session->m_LayerMask & FONT_RENDER_LAYER_SHADOW) != 0);
    const uint64_t     initial_atlas_version = session->m_AtlasVersion;
    TextureUpdateState texture_update;

    // Resolve the complete glyph set before writing vertices. Adding a glyph
    // can grow the uniform cache cell and rebuild the atlas, which changes the
    // placement of every previously cached glyph.
    for (uint32_t i = 0; i < glyph_count; ++i)
    {
        TextGlyph& text_glyph = glyphs[i];
        if (dmUtf8::IsWhiteSpace(text_glyph.m_Codepoint))
            continue;
        if (!GetOrCreateGlyph(session, text_glyph.m_Font, text_glyph.m_GlyphIndex, &texture_update))
        {
            TextLayoutRelease(layout);
            return FONT_RENDERER_RESULT_GLYPH_ERROR;
        }
    }

    dmArray<FontGlyphVertex> vertices;
    vertices.SetCapacity(visible_count * layer_count * 6);
    vertices.SetSize(vertices.Capacity());

    Matrix4 transform;
    transform.setCol0(Vector4(transform_array[0], transform_array[1], transform_array[2], transform_array[3]));
    transform.setCol1(Vector4(transform_array[4], transform_array[5], transform_array[6], transform_array[7]));
    transform.setCol2(Vector4(transform_array[8], transform_array[9], transform_array[10], transform_array[11]));
    transform.setCol3(Vector4(transform_array[12], transform_array[13], transform_array[14], transform_array[15]));
    Vector4     face_color(face_array[0], face_array[1], face_array[2], face_array[3]);
    Vector4     outline_color(outline_array[0], outline_array[1], outline_array[2], outline_array[3]);
    Vector4     shadow_color(shadow_array[0], shadow_array[1], shadow_array[2], shadow_array[3]);

    const float font_scale = FontGetScaleFromSize(session->m_Font, session->m_Size);
    const float max_ascent = FontGetAscent(session->m_Font, font_scale);
    const float max_descent = -FontGetDescent(session->m_Font, font_scale);
    const float line_height = max_ascent + max_descent;
    const float x_offset = OffsetX(align, width);
    const float y_offset = OffsetY(vertical_align, height, max_ascent, max_descent, leading, line_count);
    const float smoothing = 0.25f / (session->m_SdfSpread * dmMath::Max(0.000001f, sdf_scale));
    uint32_t    vertex_index = 0;
    for (uint32_t line_index = 0; line_index < line_count; ++line_index)
    {
        TextLine& line = lines[line_index];
        if (line.m_Length == 0)
            continue;
        const float first_x = glyphs[line.m_Index].m_X;
        const float first_y = glyphs[line.m_Index].m_Y;
        const float line_x = x_offset - OffsetX(align, line.m_Width);
        const float line_y = y_offset - line_index * line_height * leading;
        for (uint32_t glyph_index = line.m_Index; glyph_index < line.m_Index + line.m_Length; ++glyph_index)
        {
            TextGlyph& text_glyph = glyphs[glyph_index];
            if (dmUtf8::IsWhiteSpace(text_glyph.m_Codepoint))
                continue;
            CachedGlyph* cached = FindGlyph(session, text_glyph.m_Font, text_glyph.m_GlyphIndex);
            assert(cached);
            FontPackGlyphVertices(&cached->m_Glyph,
                                  1.0f / session->m_AtlasWidth,
                                  1.0f / session->m_AtlasHeight,
                                  cached->m_X,
                                  cached->m_Y,
                                  session->m_CellMaxAscent,
                                  session->m_CellPadding,
                                  layer_count,
                                  session->m_LayerMask,
                                  vertex_index,
                                  visible_count * 6,
                                  transform,
                                  line_x + text_glyph.m_X - first_x,
                                  line_y + text_glyph.m_Y - first_y,
                                  face_color,
                                  outline_color,
                                  shadow_color,
                                  0.75f,
                                  session->m_SdfOutline,
                                  smoothing,
                                  session->m_SdfShadow,
                                  session->m_ShadowX,
                                  session->m_ShadowY,
                                  true,
                                  vertices.Begin());
            vertex_index += 6;
        }
    }
    TextLayoutRelease(layout);

    output->m_VertexCount = vertices.Size();
    output->m_VertexByteCount = vertices.Size() * sizeof(FontGlyphVertex);
    output->m_Vertices = (uint8_t*)malloc(output->m_VertexByteCount);
    if (output->m_VertexByteCount != 0 && !output->m_Vertices)
        return FONT_RENDERER_RESULT_OUT_OF_MEMORY;
    if (output->m_VertexByteCount != 0)
        memcpy(output->m_Vertices, vertices.Begin(), output->m_VertexByteCount);
    output->m_AtlasVersion = session->m_AtlasVersion;
    if (known_version != session->m_AtlasVersion)
    {
        const bool     send_full = known_version != initial_atlas_version || texture_update.m_Full;
        const uint32_t update_x = send_full ? 0 : texture_update.m_X;
        const uint32_t update_y = send_full ? 0 : texture_update.m_Y;
        const uint32_t update_width = send_full ? session->m_AtlasWidth : texture_update.m_Width;
        const uint32_t update_height = send_full ? session->m_AtlasHeight : texture_update.m_Height;
        output->m_HasTextureUpdate = 1;
        output->m_TextureX = update_x;
        output->m_TextureY = update_y;
        output->m_TextureWidth = update_width;
        output->m_TextureHeight = update_height;
        output->m_TextureChannels = session->m_Channels;
        output->m_TexturePixelCount = update_width * update_height * session->m_Channels;
        output->m_TexturePixels = (uint8_t*)malloc(output->m_TexturePixelCount);
        if (output->m_TexturePixelCount != 0 && !output->m_TexturePixels)
        {
            FontRendererFreeRenderResult(output);
            return FONT_RENDERER_RESULT_OUT_OF_MEMORY;
        }
        const uint32_t row_bytes = update_width * session->m_Channels;
        for (uint32_t y = 0; y < update_height; ++y)
        {
            memcpy(output->m_TexturePixels + y * row_bytes,
                   session->m_Atlas + ((update_y + y) * session->m_AtlasWidth + update_x) * session->m_Channels,
                   row_bytes);
        }
    }
    return FONT_RENDERER_RESULT_OK;
}

void FontRendererFreeRenderResult(FontRendererRenderResult* result)
{
    if (!result)
        return;
    free(result->m_Vertices);
    free(result->m_TexturePixels);
    memset(result, 0, sizeof(*result));
}
