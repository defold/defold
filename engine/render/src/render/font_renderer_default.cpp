// Copyright 2020-2025 The Defold Foundation
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

#include <graphics/graphics.h>              // for AddVertexStream etc
#include <graphics/graphics_util.h>         // for UnpackRGBA

#include "font.h"                           // for GetGlyph, TextMetri...
#include "font_renderer_private.h"          // for RenderLayerMask
#include "font_renderer_default.h"          // for Layout
#include "render/render_private.h"          // for TextEntry

namespace dmRender
{
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
        // intentionally empty, as we currently don't need a context.
        uint32_t _unused;
    };

    HFontRenderBackend CreateFontRenderBackend()
    {
        return new FontRenderBackend;
    }

    void DestroyFontRenderBackend(HFontRenderBackend backend)
    {
        delete backend;
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

    static float GetLineTextMetrics(HFontMap font_map, float tracking, const char* text, int n, bool measure_trailing_space)
    {
        float width = 0;
        const char* cursor = text;
        const FontGlyph* last = 0;
        for (int i = 0; i < n; ++i)
        {
            uint32_t c = dmUtf8::NextChar(&cursor);
            const FontGlyph* g = GetGlyph(font_map, c);
            if (!g) {
                continue;
            }

            last = g;
            width += g->m_Advance + tracking;
        }
        if (n > 0 && 0 != last)
        {
            if (font_map->m_IsMonospaced) {
                width += font_map->m_Padding;
            }
            else {
                float last_width = (measure_trailing_space && last->m_Character == ' ') ? last->m_Advance : last->m_Width;
                float last_end_point = last->m_LeftBearing + last_width;
                float last_right_bearing = last->m_Advance - last_end_point;
                width = width - last_right_bearing;
            }
            width -= tracking;
        }

        return width;
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

    void GetTextMetrics(HFontRenderBackend backend, HFontMap font_map, const char* text, TextMetricsSettings* settings, TextMetrics* metrics)
    {
        DM_PROFILE(__FUNCTION__);
        (void)backend;

        metrics->m_MaxAscent = font_map->m_MaxAscent;
        metrics->m_MaxDescent = font_map->m_MaxDescent;

        float width = settings->m_Width;
        if (!settings->m_LineBreak) {
            width = 1000000.0f;
        }

        const uint32_t max_lines = 128;
        dmRender::TextLine lines[max_lines];

        float line_height = font_map->m_MaxAscent + font_map->m_MaxDescent;

        // Trailing space characters should be ignored when measuring and
        // rendering multiline text.
        // For single line text we still want to include spaces when the text
        // layout is calculated (https://github.com/defold/defold/issues/5911)
        bool measure_trailing_space = !settings->m_LineBreak;

        LayoutMetrics lm(font_map, settings->m_Tracking * line_height);
        float layout_width;
        uint32_t num_lines = Layout(text, width, lines, max_lines, &layout_width, lm, measure_trailing_space);
        metrics->m_Width = layout_width;
        metrics->m_Height = num_lines * (line_height * settings->m_Leading) - line_height * (settings->m_Leading - 1.0f);
        metrics->m_LineCount = num_lines;
    }

    uint32_t CreateFontVertexData(HFontRenderBackend backend, HFontMap font_map, uint32_t frame, const char* text, const TextEntry& te, float recip_w, float recip_h, uint8_t* _vertices, uint32_t num_vertices)
    {
        DM_PROFILE(__FUNCTION__);
        (void)backend;

        GlyphVertex* vertices = (GlyphVertex*)_vertices;

        float width = te.m_Width;
        if (!te.m_LineBreak) {
            width = 1000000.0f;
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
        if (font_map->m_IsMonospaced)
        {
            x_offset -= font_map->m_Padding * 0.5f;
        }
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
        float shadow_x              = font_map->m_ShadowX;
        float shadow_y              = font_map->m_ShadowY;

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
                    dmRender::FontGlyph* g = GetGlyph(font_map, c);
                    if (!g)
                    {
                        continue;
                    }

                    // If we get here, then it may be that c != glyph->m_Character
                    c = g->m_Character;

                    if ((vertexindex + vertices_per_quad) * layer_count > num_vertices)
                    {
                        inner_break = true;
                        break;
                    }

                    if (g->m_Width > 0)
                    {
                        int16_t px_cell_offset_y = font_map->m_CacheCellMaxAscent - (int16_t)g->m_Ascent;

                        // Prepare the cache here aswell since we only count glyphs we definitely will render.
                        if (!IsInCache(font_map, c))
                        {
                            AddGlyphToCache(font_map, frame, g, px_cell_offset_y);
                        }

                        CacheGlyph* cache_glyph = GetFromCache(font_map, c);
                        if (cache_glyph)
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
            float x = x_offset - OffsetX(te.m_Align, l.m_Width);
            float y = y_offset - line * leading;
            const char* cursor = &text[l.m_Index];
            int n = l.m_Count;
            for (int j = 0; j < n; ++j)
            {
                uint32_t c = dmUtf8::NextChar(&cursor);

                FontGlyph* glyph = GetGlyph(font_map, c);
                if (!glyph) {
                    continue;
                }

                // If we get here, then it may be that c != glyph->m_Character
                c = glyph->m_Character;

                // Look ahead and see if we can produce vertices for the next glyph or not
                if ((vertexindex + vertices_per_quad) * layer_count > num_vertices)
                {
                    dmLogWarning("Character buffer exceeded (size: %d), increase the \"graphics.max_characters\" property in your game.project file.", num_vertices / 6);
                    return vertexindex * layer_count;
                }

                if (glyph->m_Width > 0)
                {
                    float f_width        = glyph->m_ImageWidth;
                    float f_descent      = glyph->m_Descent;
                    float f_ascent       = glyph->m_Ascent;
                    float f_left_bearing = glyph->m_LeftBearing;
                    // This is the difference in size of the glyph image and glyph size
                    float f_size_diff    = glyph->m_ImageWidth - glyph->m_Width;

                    int16_t width        = (int16_t) f_width;
                    int16_t descent      = (int16_t) f_descent;
                    int16_t ascent       = (int16_t) f_ascent;

                    // Calculate y-offset in cache-cell space by moving glyphs down to baseline
                    int16_t px_cell_offset_y = font_map->m_CacheCellMaxAscent - ascent;

                    if (!IsInCache(font_map, c))
                    {
                        AddGlyphToCache(font_map, frame, glyph, px_cell_offset_y);
                    }

                    CacheGlyph* cache_glyph = GetFromCache(font_map, c);
                    if (cache_glyph)
                    {
                        uint32_t face_index = vertexindex + vertices_per_quad * valid_glyph_count * (layer_count-1);
                        // TODO: Add api to get the uv coords for the glyph
                        uint32_t tx = cache_glyph->m_X;
                        uint32_t ty = cache_glyph->m_Y;

                        // Set face vertices first, this will always hold since we can't have less than 1 layer
                        GlyphVertex& v1_layer_face = vertices[face_index];
                        GlyphVertex& v2_layer_face = vertices[face_index + 1];
                        GlyphVertex& v3_layer_face = vertices[face_index + 2];
                        GlyphVertex& v4_layer_face = vertices[face_index + 3];
                        GlyphVertex& v5_layer_face = vertices[face_index + 4];
                        GlyphVertex& v6_layer_face = vertices[face_index + 5];

                        float xx = x - f_size_diff * 0.5f;
                        (Vector4&) v1_layer_face.m_Position = te.m_Transform * Vector4(xx + f_left_bearing, y - f_descent, 0, 1);
                        (Vector4&) v2_layer_face.m_Position = te.m_Transform * Vector4(xx + f_left_bearing, y + f_ascent, 0, 1);
                        (Vector4&) v3_layer_face.m_Position = te.m_Transform * Vector4(xx + f_left_bearing + f_width, y - f_descent, 0, 1);
                        (Vector4&) v6_layer_face.m_Position = te.m_Transform * Vector4(xx + f_left_bearing + f_width, y + f_ascent, 0, 1);

                        v1_layer_face.m_UV[0] = (tx + font_map->m_CacheCellPadding) * recip_w;
                        v1_layer_face.m_UV[1] = (ty + font_map->m_CacheCellPadding + ascent + descent + px_cell_offset_y) * recip_h;

                        v2_layer_face.m_UV[0] = (tx + font_map->m_CacheCellPadding) * recip_w;
                        v2_layer_face.m_UV[1] = (ty + font_map->m_CacheCellPadding + px_cell_offset_y) * recip_h;

                        v3_layer_face.m_UV[0] = (tx + font_map->m_CacheCellPadding + width) * recip_w;
                        v3_layer_face.m_UV[1] = (ty + font_map->m_CacheCellPadding + ascent + descent + px_cell_offset_y) * recip_h;

                        v6_layer_face.m_UV[0] = (tx + font_map->m_CacheCellPadding + width) * recip_w;
                        v6_layer_face.m_UV[1] = (ty + font_map->m_CacheCellPadding + px_cell_offset_y) * recip_h;

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
                            (Vector4&) v1_layer_shadow.m_Position = te.m_Transform * Vector4(xx + f_left_bearing + shadow_x, y - f_descent + shadow_y, 0, 1);
                            (Vector4&) v2_layer_shadow.m_Position = te.m_Transform * Vector4(xx + f_left_bearing + shadow_x, y + f_ascent + shadow_y, 0, 1);
                            (Vector4&) v3_layer_shadow.m_Position = te.m_Transform * Vector4(xx + f_left_bearing + shadow_x + f_width, y - f_descent + shadow_y, 0, 1);
                            (Vector4&) v6_layer_shadow.m_Position = te.m_Transform * Vector4(xx + f_left_bearing + shadow_x + f_width, y + f_ascent + shadow_y, 0, 1);

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
                x += glyph->m_Advance + tracking;
            }
        }

        #undef HAS_LAYER

        return vertexindex * layer_count;
    }

} // namespace
