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

#include <math.h>                           // for sqrtf
#include <stdint.h>                         // for uint32_t, int16_t
#include <dlib/align.h>                     // for DM_ALIGNED
#include <dlib/log.h>                       // for dmLog*
#include <dlib/profile.h>                   // for DM_PROFILE, DM_PROPERTY_*
#include <dlib/vmath.h>                     // for Vector4
#include <dlib/time.h>                      // for dmTime::GetMonotonicTime()

#include <graphics/graphics.h>              // for AddVertexStream etc
#include <graphics/graphics_util.h>         // for UnpackRGBA

#include "render/render_private.h"          // for TextEntry
#include "render/font/fontmap.h"
#include "render/font/fontmap_private.h"
#include "render/font/font_renderer_private.h"

#include <dmsdk/font/text_layout.h>

namespace dmRender
{

static const uint32_t FALLBACK_CODEPOINT = 126U; // '~'

struct DM_ALIGNED(16) GlyphVertex
{
    // NOTE: The struct *must* be 16-bytes aligned due to SIMD operations.
    float m_Position[4];
    float m_UV[2];
    float m_FaceColor[4];
    float m_OutlineColor[4];
    float m_ShadowColor[4];
    float m_SdfParams[4];
    float m_LayerMasks[3];
};

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
    return sizeof(GlyphVertex);
}

dmGraphics::HVertexDeclaration CreateVertexDeclaration(HFontRenderBackend backend, dmGraphics::HContext context)
{
    (void)backend;

    dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(context);
    dmGraphics::AddVertexStream(stream_declaration, "position", 4, dmGraphics::TYPE_FLOAT, false);
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

static void OutputGlyph(FontGlyph* glyph,
                        float recip_w, float recip_h,
                        uint32_t cell_x, uint32_t cell_y, uint32_t cache_cell_max_ascent, uint32_t cache_cell_padding,
                        uint32_t layer_count, uint32_t layer_mask,
                        uint32_t vertexindex, uint32_t vertex_layer_stride,
                        const dmVMath::Matrix4& transform,
                        float x, float y,
                        const Vector4& face_color,
                        const Vector4& outline_color,
                        const Vector4& shadow_color,
                        float sdf_edge_value,
                        float sdf_outline,
                        float sdf_smoothing,
                        float sdf_shadow,
                        float shadow_x,
                        float shadow_y,
                        GlyphVertex* vertices)
{
    float f_width;
    float f_descent;
    float f_ascent;
    float f_left_bearing;
    float f_size_diff;
    if (glyph)
    {
        assert(glyph->m_Bitmap.m_Width != 0);
        f_width        = glyph->m_Bitmap.m_Width;
        f_descent      = glyph->m_Descent;
        f_ascent       = glyph->m_Ascent;
        f_left_bearing = glyph->m_LeftBearing;
        // This is the difference in size of the glyph image and glyph size
        f_size_diff    = f_width - glyph->m_Width;
    }
    else
    {
        // we're outputting a missing glyph
        f_width        = 0;
        f_descent      = 0;
        f_ascent       = 0;
        f_left_bearing = 0;
        f_size_diff    = 0;
    }
    int16_t width   = (int16_t) f_width;
    int16_t descent = (int16_t) f_descent;
    int16_t ascent  = (int16_t) f_ascent;

    // Calculate y-offset in cache-cell space by moving glyphs down to baseline
    int16_t px_cell_offset_y = cache_cell_max_ascent - ascent;

    uint32_t face_index = vertexindex + vertex_layer_stride * (layer_count-1);
    uint32_t tx = cell_x;
    uint32_t ty = cell_y;

    // Set face vertices first, this will always hold since we can't have less than 1 layer
    GlyphVertex& v1_layer_face = vertices[face_index];
    GlyphVertex& v2_layer_face = vertices[face_index + 1];
    GlyphVertex& v3_layer_face = vertices[face_index + 2];
    GlyphVertex& v4_layer_face = vertices[face_index + 3];
    GlyphVertex& v5_layer_face = vertices[face_index + 4];
    GlyphVertex& v6_layer_face = vertices[face_index + 5];

    float xx = x - f_size_diff * 0.5f;

    (Vector4&) v1_layer_face.m_Position = transform * Vector4(xx + f_left_bearing, y - f_descent, 0, 1);
    (Vector4&) v2_layer_face.m_Position = transform * Vector4(xx + f_left_bearing, y + f_ascent, 0, 1);
    (Vector4&) v3_layer_face.m_Position = transform * Vector4(xx + f_left_bearing + f_width, y - f_descent, 0, 1);
    (Vector4&) v6_layer_face.m_Position = transform * Vector4(xx + f_left_bearing + f_width, y + f_ascent, 0, 1);

    v1_layer_face.m_UV[0] = (tx + cache_cell_padding) * recip_w;
    v1_layer_face.m_UV[1] = (ty + cache_cell_padding + ascent + descent + px_cell_offset_y) * recip_h;

    v2_layer_face.m_UV[0] = (tx + cache_cell_padding) * recip_w;
    v2_layer_face.m_UV[1] = (ty + cache_cell_padding + px_cell_offset_y) * recip_h;

    v3_layer_face.m_UV[0] = (tx + cache_cell_padding + width) * recip_w;
    v3_layer_face.m_UV[1] = (ty + cache_cell_padding + ascent + descent + px_cell_offset_y) * recip_h;

    v6_layer_face.m_UV[0] = (tx + cache_cell_padding + width) * recip_w;
    v6_layer_face.m_UV[1] = (ty + cache_cell_padding + px_cell_offset_y) * recip_h;

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
        uint32_t outline_index = vertexindex + vertex_layer_stride * (layer_count-2);

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
        (Vector4&) v1_layer_shadow.m_Position = transform * Vector4(xx + f_left_bearing + shadow_x, y - f_descent + shadow_y, 0, 1);
        (Vector4&) v2_layer_shadow.m_Position = transform * Vector4(xx + f_left_bearing + shadow_x, y + f_ascent + shadow_y, 0, 1);
        (Vector4&) v3_layer_shadow.m_Position = transform * Vector4(xx + f_left_bearing + shadow_x + f_width, y - f_descent + shadow_y, 0, 1);
        (Vector4&) v6_layer_shadow.m_Position = transform * Vector4(xx + f_left_bearing + shadow_x + f_width, y + f_ascent + shadow_y, 0, 1);

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
}

static uint32_t TextToCodePoints(const char* text, dmArray<uint32_t>& codepoints)
{
    uint32_t len = dmUtf8::StrLen(text);
    codepoints.SetCapacity(len);
    codepoints.SetSize(0);
    const char* cursor = text;
    while (uint32_t c = dmUtf8::NextChar(&cursor))
    {
        codepoints.Push(c);
    }
    return len;
}


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

    TextLayoutFree(layout);
}


uint32_t CreateFontVertexData(HFontRenderBackend backend, HFontMap font_map, uint32_t frame, const char* text, const TextEntry& te, float sdf_screen_scale, float recip_w, float recip_h, uint8_t* _vertices, uint32_t num_vertices)
{
    DM_PROFILE(__FUNCTION__);
    (void)backend;

    GlyphVertex* vertices = (GlyphVertex*)_vertices;

    float line_height = font_map->m_MaxAscent + font_map->m_MaxDescent;
    float leading = line_height * te.m_Leading;
    float tracking = te.m_Tracking;

    const Vector4 face_color    = dmGraphics::UnpackRGBA(te.m_FaceColor);
    const Vector4 outline_color = dmGraphics::UnpackRGBA(te.m_OutlineColor);
    const Vector4 shadow_color  = dmGraphics::UnpackRGBA(te.m_ShadowColor);

    const float sdf_edge_value = 0.75f;
    const float kMinSdfScreenScale = 0.5f;
    const float kMinSdfScale = 1e-6f;
    float sdf_scale = sdf_screen_scale;
    if (sdf_scale > 0.0f)
    {
        sdf_scale = fmaxf(sdf_scale, kMinSdfScreenScale);
    }
    else
    {
        // Fallback to local scale when screen scale is invalid.
        const Vector4 r0 = te.m_Transform.getRow(0);
        sdf_scale = sqrtf(r0.getX() * r0.getX() + r0.getY() * r0.getY());
    }
    if (sdf_scale < kMinSdfScale)
    {
        sdf_scale = kMinSdfScale;
    }
    float sdf_outline = font_map->m_SdfOutline;
    float sdf_shadow  = font_map->m_SdfShadow;
    // For anti-aliasing, 0.25 represents the single-axis radius of half a pixel.
    float sdf_smoothing = 0.25f / (font_map->m_SdfSpread * sdf_scale);

    HFontCollection font_collection = font_map->m_FontCollection;

    // TODO: Create a backend scratch buffer

    uint64_t tstart_seg = dmTime::GetMonotonicTime();

    dmArray<uint32_t> codepoints;
    TextToCodePoints(text, codepoints);

    uint64_t tend_seg_alloc = dmTime::GetMonotonicTime();

    TextLayoutSettings layoutsettings = {0};
    layoutsettings.m_Size = dmRender::GetFontMapSize(font_map);
    layoutsettings.m_LineBreak = te.m_LineBreak;
    layoutsettings.m_Width = te.m_Width;
    layoutsettings.m_Tracking = te.m_Tracking;
    layoutsettings.m_Leading = te.m_Leading;
    // legacy options for glyph bank fonts
    layoutsettings.m_Monospace = dmRender::GetFontMapMonospaced(font_map);
    layoutsettings.m_Padding = dmRender::GetFontMapPadding(font_map);

    TextLayout* layout = 0;
    TextResult r = TextLayoutCreate(font_collection, codepoints.Begin(), codepoints.Size(), &layoutsettings, &layout);
    if (TEXT_RESULT_OK != r)
    {
        if (layout)
            TextLayoutFree(layout);
        return 0;
    }

    uint64_t tend_seg = dmTime::GetMonotonicTime();

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
                if (!IsInCache(font_map, glyph_key))
                {
                    // Calculate y-offset in cache-cell space by moving glyphs down to baseline
                    int16_t px_cell_offset_y = font_map->m_CacheCellMaxAscent - (int16_t)glyph->m_Ascent;
                    AddGlyphToCache(font_map, frame, glyph_key, glyph, px_cell_offset_y);
                }

                CacheGlyph* cache_glyph = GetFromCache(font_map, glyph_key, frame);
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
            OutputGlyph(FONT_RESULT_OK == r ? glyph : 0,
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
                        vertices);

            vertexindex += vertices_per_quad;
        }
    }

    TextLayoutFree(layout);

    #undef HAS_LAYER

    return vertexindex * layer_count;
}

} // namespace
