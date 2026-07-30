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

#include <assert.h>                        // for assert
#include <stdint.h>                         // for uint32_t, int16_t
#include <dlib/log.h>                       // for dmLog*
#include <dlib/profile.h>                   // for DM_PROFILE, DM_PROPERTY_*
#include <dlib/vmath.h>                     // for Vector4

#include <graphics/graphics.h>              // for AddVertexStream etc
#include <graphics/graphics_util.h>         // for UnpackRGBA

#include "render/render_private.h"          // for TextEntry
#include "render/font/fontmap.h"
#include "render/font/fontmap_private.h"
#include "render/font/font_renderer_private.h"

#include <dmsdk/font/text_layout.h>
#include <font/glyph_vertex.h>
#include <font/text_layout.h>

namespace dmRender
{

static const uint32_t FALLBACK_CODEPOINT = 126U; // '~'

struct FontRenderBackend
{
    int dummy; // Making it non empty
};

HFontRenderBackend CreateFontRenderBackend()
{
    FontRenderBackend* ctx = new FontRenderBackend;
    memset(ctx, 0, sizeof(*ctx));
    return ctx;
}

void DestroyFontRenderBackend(HFontRenderBackend ctx)
{
    delete ctx;
}

uint32_t GetFontVertexSize(HFontRenderBackend backend)
{
    (void)backend;
    return sizeof(FontGlyphVertex);
}

dmGraphics::HVertexDeclaration CreateVertexDeclaration(HFontRenderBackend backend, dmGraphics::HContext context)
{
    (void)backend;

    dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(context);
    dmGraphics::AddVertexStream(stream_declaration, "position", 3, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(stream_declaration, "texcoord0", 2, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(stream_declaration, "face_color", 4, dmGraphics::TYPE_FLOAT, true);
    dmGraphics::AddVertexStream(stream_declaration, "outline_color", 4, dmGraphics::TYPE_FLOAT, true);
    dmGraphics::AddVertexStream(stream_declaration, "shadow_color", 4, dmGraphics::TYPE_FLOAT, true);
    dmGraphics::AddVertexStream(stream_declaration, "sdf_params", 4, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(stream_declaration, "layer_mask", 3, dmGraphics::TYPE_FLOAT, false);

    dmGraphics::HVertexDeclaration decl = dmGraphics::NewVertexDeclaration(context, stream_declaration, GetFontVertexSize(backend));

    dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);

    return decl;
}

#define HAS_LAYER(mask,layer) ((mask & layer) == layer)


void GetTextMetrics(HFontRenderBackend backend, HFontMap font_map, const char* text,
                    TextLayoutSettings* settings, TextMetrics* metrics)
{
    DM_PROFILE(__FUNCTION__);
    (void)backend;

    dmArray<uint32_t> codepoints;
    TextToCodePoints(text, codepoints);

    settings->m_Size = GetFontMapSize(font_map);

    TextLayout* layout = 0;
    TextResult r = TextLayoutCreate(font_map->m_FontCollection, codepoints.Begin(), codepoints.Size(), settings, &layout);
    if (TEXT_RESULT_OK == r)
    {
        TextLayoutGetBounds(layout, &metrics->m_Width, &metrics->m_Height);
        metrics->m_LineCount   = TextLayoutGetLineCount(layout);
        metrics->m_MaxAscent   = font_map->m_MaxAscent;
        metrics->m_MaxDescent  = font_map->m_MaxDescent;
    }

    TextLayoutRelease(layout);
}


static uint32_t CreateFontVertexDataFromTextLayout(HFontMap font_map, uint32_t frame, HTextLayout layout, const TextEntry& te, float sdf_scale, float recip_w, float recip_h, uint8_t* _vertices, uint32_t num_vertices)
{
    assert(layout->m_FontCollection == GetFontCollection(font_map));

    FontGlyphVertex* vertices = (FontGlyphVertex*)_vertices;

    float line_height = font_map->m_MaxAscent + font_map->m_MaxDescent;
    float leading = line_height * te.m_Leading;

    const Vector4 face_color    = dmGraphics::UnpackRGBA(te.m_FaceColor);
    const Vector4 outline_color = dmGraphics::UnpackRGBA(te.m_OutlineColor);
    const Vector4 shadow_color  = dmGraphics::UnpackRGBA(te.m_ShadowColor);

    const float sdf_edge_value = 0.75f;
    float sdf_outline = font_map->m_SdfOutline;
    float sdf_shadow  = font_map->m_SdfShadow;
    // For anti-aliasing, 0.25 represents the single-axis radius of half a pixel.
    float sdf_smoothing = 0.25f / (font_map->m_SdfSpread * sdf_scale);
    // if it's generated at runtime, the glyph width is measured using the metrics from the glyphs in the font
    // and not generated from the visual bounds (see Fontc.java)
    bool is_metrics_ttf = font_map->m_IsDynamic;

    uint32_t    glyph_count         = TextLayoutGetGlyphCount(layout);
    TextGlyph*  glyphs              = TextLayoutGetGlyphs(layout);
    uint32_t    line_count          = TextLayoutGetLineCount(layout);
    TextLine*   lines               = TextLayoutGetLines(layout);
    uint32_t    valid_glyph_count   = glyph_count;

    uint32_t vertexindex        = 0;
    uint32_t vertices_per_quad  = 6;
    uint32_t layer_count        = 1;
    uint8_t  layer_mask         = font_map->m_LayerMask;
    float shadow_x              = font_map->m_ShadowX;
    float shadow_y              = font_map->m_ShadowY;

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
    layer_count += HAS_LAYER(layer_mask,OUTLINE) + HAS_LAYER(layer_mask,SHADOW);
    if (layer_count > 1)
    {
        // Calculate number of renderable glyphs.
        // We need this as we're uploading constants to the GPU, with indices referring to faces
        for (uint32_t i = 0; i < glyph_count; ++i)
        {
            if (dmUtf8::IsWhiteSpace(glyphs[i].m_Codepoint))
                valid_glyph_count--;
        }
    }

    int32_t dir = 1;//layout->m_Direction == TEXT_DIRECTION_RTL ? -1 : 1;
    (void)dir;
    uint32_t align = te.m_Align;
    float x_offset = OffsetX(align, te.m_Width); // the box alignment is LTR direction (in pixels)
    if (font_map->m_IsMonospaced)
    {
        x_offset -= font_map->m_Padding * 0.5f;
    }
    float y_offset = OffsetY(te.m_VAlign, te.m_Height, font_map->m_MaxAscent, font_map->m_MaxDescent, te.m_Leading, line_count);

    for (uint32_t i = 0; i < line_count; ++i)
    {
        TextLine& line = lines[i];
        if (line.m_Length == 0)
            continue;

        // all glyphs are positions on an infinite line, so we want the position of the first glyph on the line
        int32_t first_x = glyphs[line.m_Index].m_X;
        int32_t first_y = glyphs[line.m_Index].m_Y;

        const float line_start_x = x_offset - OffsetX(align, line.m_Width);
        const float line_start_y = y_offset - i * leading;

        int gi_end = line.m_Index + line.m_Length;
        for (int gi = line.m_Index; gi < gi_end; ++gi)
        {
            // Look ahead and see if we can produce vertices for the next glyph or not
            if ((vertexindex + vertices_per_quad) * layer_count > num_vertices)
            {
                dmLogWarning("Character buffer exceeded (size: %d), increase the \"graphics.max_characters\" property in your game.project file.", num_vertices / 6);
                return vertexindex * layer_count;
            }

            TextGlyph* g = &glyphs[gi];
            uint32_t c = g->m_Codepoint;
            if (dmUtf8::IsWhiteSpace(c))
                continue;

            int32_t pos_x = g->m_X;
            int32_t pos_y = g->m_Y;

            // We're dealing with absolute coordinates on an infinite line
            float offx = pos_x - first_x;
            float offy = pos_y - first_y;
            float x = line_start_x + offx;
            float y = line_start_y + offy;

            uint32_t cell_x = 0;
            uint32_t cell_y = 0;

            uint32_t glyph_index = g->m_GlyphIndex;

            HFont font = g->m_Font;
            FontGlyph* glyph = 0;
            FontResult r = dmRender::GetOrCreateGlyphByIndex(font_map, font, glyph_index, &glyph);
            if (FONT_RESULT_OK != r)
            {
                glyph_index = FontGetGlyphIndex(font, FALLBACK_CODEPOINT);
                r = dmRender::GetOrCreateGlyphByIndex(font_map, font, glyph_index, &glyph);
            }

            if (glyph && glyph->m_Bitmap.m_Width > 0) // only add glyphs with a size (image) to the glyph cache
            {
                uint64_t glyph_key = dmRender::MakeGlyphIndexKey(font, glyph_index);
                CacheGlyph* cache_glyph = GetFromCache(font_map, glyph_key, frame);
                if (!cache_glyph)
                {
                    // Calculate y-offset in cache-cell space by moving glyphs down to baseline
                    int16_t px_cell_offset_y = font_map->m_CacheCellMaxAscent - (int16_t)glyph->m_Ascent;
                    cache_glyph = AddGlyphToCache(font_map, frame, glyph_key, glyph, px_cell_offset_y);
                }
                if (cache_glyph)
                {
                    cell_x = cache_glyph->m_X;
                    cell_y = cache_glyph->m_Y;
                }
            }
            else
            {
                r = FONT_RESULT_ERROR;
            }

            // We've already discarded whitespaces, but the glyph may not yet be cached.
            // To minimize overall edge case complexity, we output a zero size quad.
            FontPackGlyphVertices(FONT_RESULT_OK == r ? glyph : 0,
                                  recip_w, recip_h,
                                  cell_x, cell_y, font_map->m_CacheCellMaxAscent, font_map->m_CacheCellPadding,
                                  layer_count, layer_mask,
                                  vertexindex, vertices_per_quad * valid_glyph_count,
                                  te.m_Transform,
                                  x, y,
                                  face_color,
                                  outline_color,
                                  shadow_color,
                                  sdf_edge_value,
                                  sdf_outline,
                                  sdf_smoothing,
                                  sdf_shadow,
                                  shadow_x,
                                  shadow_y,
                                  is_metrics_ttf,
                                  vertices);

            vertexindex += vertices_per_quad;
        }
    }

    #undef HAS_LAYER

    return vertexindex * layer_count;
}

uint32_t CreateFontVertexData(HFontRenderBackend backend, HFontMap font_map, uint32_t frame, const char* text, const TextEntry& te, float sdf_scale, float recip_w, float recip_h, uint8_t* _vertices, uint32_t num_vertices)
{
    DM_PROFILE(__FUNCTION__);
    (void)backend;

    if (te.m_TextLayout)
    {
        return CreateFontVertexDataFromTextLayout(font_map, frame, te.m_TextLayout, te, sdf_scale, recip_w, recip_h, _vertices, num_vertices);
    }

    // TODO: Create a backend scratch buffer

    dmArray<uint32_t> codepoints;
    TextToCodePoints(text, codepoints);

    TextLayoutSettings layoutsettings = {0};
    layoutsettings.m_Size = dmRender::GetFontMapSize(font_map);
    layoutsettings.m_LineBreak = te.m_LineBreak;
    layoutsettings.m_Width = te.m_Width;
    layoutsettings.m_Tracking = te.m_Tracking;
    layoutsettings.m_Leading = te.m_Leading;
    // legacy options for glyph bank fonts
    layoutsettings.m_Monospace = dmRender::GetFontMapMonospaced(font_map);
    layoutsettings.m_Padding = dmRender::GetFontMapPadding(font_map);

    HTextLayout layout = 0;
    TextResult r = TextLayoutCreate(font_map->m_FontCollection, codepoints.Begin(), codepoints.Size(), &layoutsettings, &layout);
    if (TEXT_RESULT_OK != r)
    {
        if (layout)
            TextLayoutRelease(layout);
        return 0;
    }

    uint32_t vertex_count = CreateFontVertexDataFromTextLayout(font_map, frame, layout, te, sdf_scale, recip_w, recip_h, _vertices, num_vertices);
    TextLayoutRelease(layout);
    return vertex_count;
}

} // namespace
