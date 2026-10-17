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

#include "layout_vertex.h"
#include "font_effect.h"
#include "../font.h"
#include "text_layout.h"

#include <string.h>

#include <dlib/math.h>
#include <dlib/utf8.h>

using namespace dmVMath;

static float OffsetX(uint32_t align, float width)
{
    if (align == 1)
    {
        return width * 0.5f;
    }

    if (align == 2)
    {
        return width;
    }

    return 0.0f;
}

static float OffsetLayoutY(uint32_t align, float height, float layout_height)
{
    if (align == 1)
    {
        return (height - layout_height) * 0.5f;
    }

    if (align == 2)
    {
        return 0.0f;
    }

    return height - layout_height;
}

static uint32_t ResolveAlign(uint32_t align, TextDirection direction)
{
    if (direction == TEXT_DIRECTION_RTL)
    {
        if (align == 0)
        {
            return 2;
        }

        if (align == 2)
        {
            return 0;
        }
    }

    return align;
}

static float GetLineStartX(const FontLayoutVertexConfig& config, const TextLine& line, TextDirection direction)
{
    const uint32_t align = ResolveAlign(config.m_Align, direction);

    return OffsetX(align, config.m_Width) - OffsetX(align, line.m_Width) - config.m_MonospacePadding * 0.5f;
}

static const uint32_t SHADOW_STYLE_FLAGS = TEXT_RENDER_STYLE_SHADOW_COLOR | TEXT_RENDER_STYLE_SHADOW_X | TEXT_RENDER_STYLE_SHADOW_Y | TEXT_RENDER_STYLE_SHADOW_BLUR | TEXT_RENDER_STYLE_SHADOW_ALPHA;
static const uint32_t OUTLINE_STYLE_FLAGS = TEXT_RENDER_STYLE_OUTLINE_COLOR | TEXT_RENDER_STYLE_OUTLINE_WIDTH | TEXT_RENDER_STYLE_OUTLINE_ALPHA;
static const float    FACE_SHADOW_THRESHOLD_BASE = 1.5f;
static const float    FACE_SHADOW_THRESHOLD_SCALE = 0.5f;

static float EncodeFaceShadowThreshold(float threshold)
{
    return FACE_SHADOW_THRESHOLD_BASE + FACE_SHADOW_THRESHOLD_SCALE * threshold;
}

static const TextRenderStyle* GetGlyphStyle(HTextLayout layout, const TextGlyph& glyph)
{
    return glyph.m_StyleIndex < layout->m_Styles.Size() ? &layout->m_Styles[glyph.m_StyleIndex] : 0;
}

static bool HasMarkupOutline(const TextRenderStyle* style)
{
    return style &&
           (style->m_Flags & OUTLINE_STYLE_FLAGS) != 0 &&
           ((style->m_Flags & TEXT_RENDER_STYLE_OUTLINE_WIDTH) == 0 || style->m_OutlineWidth > 0.0f);
}

static bool HasMarkupShadow(const TextRenderStyle* style)
{
    return style && (style->m_Flags & SHADOW_STYLE_FLAGS) != 0;
}

static uint8_t GetGlyphLayerMask(const FontLayoutVertexConfig& config, const TextGlyph& glyph)
{
    const TextRenderStyle* style = GetGlyphStyle(config.m_Layout, glyph);
    uint8_t                mask = FONT_RENDER_LAYER_FACE;
    const bool             use_rich_text = config.m_Layout->m_UseRichText;

    if ((!use_rich_text && (config.m_BaseLayerMask & FONT_RENDER_LAYER_OUTLINE) != 0) ||
        (use_rich_text && !config.m_IsBMFont && config.m_OutlineWidth > 0.0f && HasMarkupOutline(style)))
    {
        mask |= FONT_RENDER_LAYER_OUTLINE;
    }

    if ((!use_rich_text && (config.m_BaseLayerMask & FONT_RENDER_LAYER_SHADOW) != 0) ||
        (use_rich_text && !config.m_IsBMFont && HasMarkupShadow(style)))
    {
        mask |= FONT_RENDER_LAYER_SHADOW;
    }

    // Styles select effects; the font determines whether they use separate quads.
    return mask & config.m_BaseLayerMask;
}

static bool IsRenderableGlyph(const FontLayoutVertexConfig& config, const TextGlyph& glyph)
{
    if (glyph.m_Flags & TEXT_GLYPH_FLAG_OBJECT || dmUtf8::IsWhiteSpace(glyph.m_Codepoint))
    {
        return false;
    }

    if (!config.m_ResolveGlyphsForMetrics)
    {
        return true;
    }

    FontLayoutCachedGlyph cached = {};

    return config.m_ResolveGlyph(config.m_ResolveGlyphContext, glyph, &cached);
}

bool FontGetLayoutVertexMetrics(const FontLayoutVertexConfig& config, FontLayoutVertexMetrics* metrics)
{
    if ((config.m_BaseLayerMask & FONT_RENDER_LAYER_FACE) == 0)
    {
        return false;
    }

    uint32_t       glyph_quad_count = 0;
    uint32_t       outline_quad_count = 0;
    uint32_t       shadow_quad_count = 0;
    uint32_t       object_quad_count = 0;
    TextGlyph*     glyphs = TextLayoutGetGlyphs(config.m_Layout);
    const uint32_t glyph_count = TextLayoutGetGlyphCount(config.m_Layout);
    uint16_t       cached_style_index = 0;
    uint8_t        cached_layer_mask = 0;
    bool           has_cached_layer_mask = false;

    for (uint32_t i = 0; i < glyph_count; ++i)
    {
        const TextGlyph& text_glyph = glyphs[i];

        if (text_glyph.m_Flags & TEXT_GLYPH_FLAG_OBJECT)
        {
            object_quad_count += config.m_RenderObjectOutlines ? 4 : 0;
        }
        else if (IsRenderableGlyph(config, text_glyph))
        {
            if (!has_cached_layer_mask || cached_style_index != text_glyph.m_StyleIndex)
            {
                cached_style_index = text_glyph.m_StyleIndex;
                cached_layer_mask = GetGlyphLayerMask(config, text_glyph);
                has_cached_layer_mask = true;
            }

            ++glyph_quad_count;
            outline_quad_count += (cached_layer_mask & FONT_RENDER_LAYER_OUTLINE) != 0;
            shadow_quad_count += (cached_layer_mask & FONT_RENDER_LAYER_SHADOW) != 0;
        }
    }

    uint32_t              decoration_quad_count = 0;
    const TextDecoration* decorations = TextLayoutGetDecorations(config.m_Layout);
    const uint32_t        decoration_count = TextLayoutGetDecorationCount(config.m_Layout);

    for (uint32_t i = 0; config.m_RenderDecorations && i < decoration_count; ++i)
    {
        const TextDecoration& decoration = decorations[i];
        const bool            glyph_segments = FontDecorationRequiresGlyphSegments(config.m_Layout, decoration);
        const uint32_t        segment_count = glyph_segments ? decoration.m_GlyphCount : 1;
        decoration_quad_count += segment_count;

        for (uint32_t segment = 0; segment < segment_count; ++segment)
        {
            FontDecorationSegment decoration_segment;
            FontGetDecorationSegment(config.m_Layout, decoration, segment, segment_count, &decoration_segment);
            const uint32_t glyph_index = decoration_segment.m_GlyphIndex;
            const uint8_t  layer_mask = GetGlyphLayerMask(config, glyphs[glyph_index]);
            outline_quad_count += (layer_mask & FONT_RENDER_LAYER_OUTLINE) != 0;
            shadow_quad_count += (layer_mask & FONT_RENDER_LAYER_SHADOW) != 0;
        }
    }

    const uint64_t required_face_quad_count = (uint64_t)glyph_quad_count + object_quad_count + decoration_quad_count;
    const uint64_t required_quad_count = required_face_quad_count + outline_quad_count + shadow_quad_count;
    const uint32_t max_quad_count = config.m_MaxVertexCount == 0 ? UINT32_MAX : config.m_MaxVertexCount / 6;
    uint32_t       output_glyph_count = glyph_quad_count;
    uint32_t       output_outline_count = outline_quad_count;
    uint32_t       output_shadow_count = shadow_quad_count;
    uint32_t       output_object_count = object_quad_count;
    uint32_t       output_decoration_count = decoration_quad_count;

    if (required_quad_count > max_quad_count)
    {
        output_glyph_count = 0;
        output_outline_count = 0;
        output_shadow_count = 0;
        uint32_t used_quad_count = 0;

        for (uint32_t i = 0; i < glyph_count; ++i)
        {
            const TextGlyph& text_glyph = glyphs[i];

            if (!IsRenderableGlyph(config, text_glyph))
            {
                continue;
            }

            const uint8_t glyph_layer_mask = GetGlyphLayerMask(config, text_glyph);
            const uint32_t glyph_quad_cost =
                1 +
                ((glyph_layer_mask & FONT_RENDER_LAYER_OUTLINE) != 0) +
                ((glyph_layer_mask & FONT_RENDER_LAYER_SHADOW) != 0);
            if (used_quad_count + glyph_quad_cost > max_quad_count)
            {
                break;
            }

            ++output_glyph_count;
            output_outline_count += (glyph_layer_mask & FONT_RENDER_LAYER_OUTLINE) != 0;
            output_shadow_count += (glyph_layer_mask & FONT_RENDER_LAYER_SHADOW) != 0;
            used_quad_count += glyph_quad_cost;
        }

        output_object_count = dmMath::Min(object_quad_count, max_quad_count - used_quad_count) / 4 * 4;
        used_quad_count += output_object_count;
        output_decoration_count = 0;

        bool decoration_buffer_full = false;

        for (uint32_t i = 0; config.m_RenderDecorations && !decoration_buffer_full && i < decoration_count; ++i)
        {
            const TextDecoration& decoration = decorations[i];
            const bool            glyph_segments = FontDecorationRequiresGlyphSegments(config.m_Layout, decoration);
            const uint32_t        segment_count = glyph_segments ? decoration.m_GlyphCount : 1;

            for (uint32_t segment = 0; segment < segment_count; ++segment)
            {
                FontDecorationSegment decoration_segment;
                FontGetDecorationSegment(config.m_Layout, decoration, segment, segment_count, &decoration_segment);
                const uint32_t glyph_index = decoration_segment.m_GlyphIndex;
                const uint8_t  layer_mask = GetGlyphLayerMask(config, glyphs[glyph_index]);
                const uint32_t decoration_quad_cost =
                    1 +
                    ((layer_mask & FONT_RENDER_LAYER_OUTLINE) != 0) +
                    ((layer_mask & FONT_RENDER_LAYER_SHADOW) != 0);

                if (used_quad_count + decoration_quad_cost > max_quad_count)
                {
                    decoration_buffer_full = true;

                    break;
                }

                ++output_decoration_count;
                output_outline_count += (layer_mask & FONT_RENDER_LAYER_OUTLINE) != 0;
                output_shadow_count += (layer_mask & FONT_RENDER_LAYER_SHADOW) != 0;
                used_quad_count += decoration_quad_cost;
            }
        }
    }

    const uint64_t face_quad_count = (uint64_t)output_glyph_count + output_object_count + output_decoration_count;
    const uint64_t quad_count = face_quad_count + output_outline_count + output_shadow_count;
    const uint64_t vertex_count = quad_count * 6;
    const uint64_t buffer_size = vertex_count * sizeof(FontGlyphVertex);

    if (face_quad_count > UINT32_MAX || quad_count > UINT32_MAX || vertex_count > UINT32_MAX || buffer_size > UINT32_MAX)
    {
        return false;
    }

    metrics->m_GlyphQuadCount = output_glyph_count;
    metrics->m_ObjectQuadCount = output_object_count;
    metrics->m_DecorationQuadCount = output_decoration_count;
    metrics->m_FaceQuadCount = (uint32_t)face_quad_count;
    metrics->m_OutlineQuadCount = output_outline_count;
    metrics->m_ShadowQuadCount = output_shadow_count;
    metrics->m_QuadCount = (uint32_t)quad_count;
    metrics->m_VertexCount = (uint32_t)vertex_count;
    metrics->m_VertexBufferSize = (uint32_t)buffer_size;
    metrics->m_LayerCount = 1 + (output_outline_count != 0) + (output_shadow_count != 0);
    metrics->m_LayerMask = FONT_RENDER_LAYER_FACE |
                           (output_outline_count != 0 ? FONT_RENDER_LAYER_OUTLINE : 0) |
                           (output_shadow_count != 0 ? FONT_RENDER_LAYER_SHADOW : 0);
    metrics->m_Truncated = quad_count < required_quad_count;

    return true;
}

// Packs one face-only line used by the optional layout-object debug outline.
static void PackObjectOutlineLine(const FontLayoutVertexConfig& config,
                                  const TextGlyphFaceColors&    colors,
                                  float                         x0,
                                  float                         y0,
                                  float                         x1,
                                  float                         y1,
                                  FontGlyphVertex*              vertices)
{
    uint32_t packed_colors[4];
    FontPackGlyphFaceColors(colors, packed_colors);

    FontDecorationVertexParams decoration = {};
    decoration.m_TextureU = config.m_DecorationU;
    decoration.m_TextureV = config.m_DecorationV;
    decoration.m_X0 = x0;
    decoration.m_Y0 = y0;
    decoration.m_X1 = x1;
    decoration.m_Y1 = y1;
    decoration.m_Thickness = 1.0f;

    FontVertexLayerParams layers = {};
    layers.m_Transform = &config.m_Transform;
    layers.m_FaceColors = packed_colors;
    layers.m_FaceVertices = vertices;
    layers.m_SdfEdge = 0.75f;
    layers.m_SdfOutline = 0.75f;
    layers.m_SdfSmoothing = 0.01f;
    layers.m_SdfShadow = 0.75f;
    layers.m_LayerCount = 1;
    layers.m_FaceOnly = true;
    FontPackDecorationVertices(decoration, layers);
}

static void EmitObjectOutline(const FontLayoutVertexConfig& config,
                              const TextGlyph&              text_glyph,
                              float                         x,
                              float                         y,
                              uint32_t*                     vertex_index,
                              FontGlyphVertex*              vertices)
{
    TextGlyphFaceColors colors;
    TextLayoutGetGlyphFaceColors(config.m_Layout, text_glyph, config.m_FaceColor, &colors);
    const float x1 = x + text_glyph.m_Width;
    const float y1 = y + text_glyph.m_Height;
    PackObjectOutlineLine(config, colors, x, y, x1, y, vertices + *vertex_index);
    *vertex_index += 6;
    PackObjectOutlineLine(config, colors, x, y1, x1, y1, vertices + *vertex_index);
    *vertex_index += 6;
    PackObjectOutlineLine(config, colors, x, y, x, y1, vertices + *vertex_index);
    *vertex_index += 6;
    PackObjectOutlineLine(config, colors, x1, y, x1, y1, vertices + *vertex_index);
    *vertex_index += 6;
}

struct GlyphColorEffect
{
    float    m_UV[4]; // left, bottom, right, top
    float    m_Phase;
    uint32_t m_Index;
};

static const TextEffect* FindGlyphGradient(const TextLayout* layout, const TextGlyph& glyph)
{
    if (glyph.m_MarkupSpanIndex >= layout->m_ResolvedSpans.Size())
    {
        return 0;
    }
    const TextResolvedSpan& span = layout->m_ResolvedSpans[glyph.m_MarkupSpanIndex];
    const TextEffect*       effect = 0;
    for (uint32_t i = 0; i < span.m_EffectCount; ++i)
    {
        const TextEffect& candidate = layout->m_Effects[layout->m_SpanEffects[span.m_EffectIndex + i]];
        if (candidate.m_Type == TEXT_EFFECT_GRADIENT && candidate.m_TextLength != 0)
        {
            effect = &candidate;
        }
    }
    return effect;
}

static bool ResolveGlyphColorEffect(const FontLayoutVertexConfig& config, const TextGlyph& glyph, GlyphColorEffect* output)
{
    if (!config.m_ResolveEffect)
    {
        return false;
    }
    const TextLayout* layout = config.m_Layout;
    const TextEffect* effect = FindGlyphGradient(layout, glyph);
    if (!effect || effect->m_Gradient.m_Mode == TEXT_GRADIENT_MODE_SPATIAL)
    {
        return false;
    }

    const TextGradientEffect& gradient = effect->m_Gradient;
    FontColorEffect           color = {};
    color.m_Type = gradient.m_Mode == TEXT_GRADIENT_MODE_QUAD ? FONT_COLOR_EFFECT_FOUR_CORNER : FONT_COLOR_EFFECT_LINEAR;
    color.m_StopCount = color.m_Type == FONT_COLOR_EFFECT_FOUR_CORNER ? 4 : 2;
    color.m_Flags = FONT_COLOR_EFFECT_MIRROR;
    const float* corners[] = { gradient.m_Corners.m_BottomLeft, gradient.m_Corners.m_BottomRight, gradient.m_Corners.m_TopLeft, gradient.m_Corners.m_TopRight };
    if (gradient.m_Mode == TEXT_GRADIENT_MODE_VERTICAL)
    {
        corners[1] = gradient.m_Corners.m_TopLeft;
    }
    for (uint32_t i = 0; i < color.m_StopCount; ++i)
    {
        const float*   rgba = corners[i];
        const uint32_t packed = FontPackColor(Vector4(rgba[0], rgba[1], rgba[2], rgba[3]));
        memcpy(color.m_Colors[i], &packed, sizeof(packed));
        color.m_Positions[i] = (float)i / (color.m_StopCount - 1);
    }
    color.m_Parameters[gradient.m_Mode == TEXT_GRADIENT_MODE_VERTICAL ? 1 : 0] = 1.0f;
    FontEffectRecord record;
    FontPackEffect(color, &record);
    output->m_Index = config.m_ResolveEffect(config.m_ResolveEffectContext, record);
    if (output->m_Index == UINT32_MAX)
    {
        return false;
    }

    const float left = dmMath::Clamp((float)((int64_t)glyph.m_Cluster - effect->m_TextOffset) / effect->m_TextLength, 0.0f, 1.0f);
    const float right = dmMath::Clamp((float)((int64_t)glyph.m_Cluster + 1 - effect->m_TextOffset) / effect->m_TextLength, 0.0f, 1.0f);
    output->m_UV[0] = gradient.m_Fit == TEXT_EFFECT_FIT_TEXT ? left : 0.0f;
    output->m_UV[1] = 0.0f;
    output->m_UV[2] = gradient.m_Fit == TEXT_EFFECT_FIT_TEXT ? right : 1.0f;
    output->m_UV[3] = 1.0f;
    double phase = layout->m_ElapsedTime * gradient.m_Hz * 2.0;
    if (gradient.m_Fit == TEXT_EFFECT_FIT_SPAN)
    {
        float x = 0.5f / effect->m_TextLength;
        if (gradient.m_Mode == TEXT_GRADIENT_MODE_HORIZONTAL)
        {
            x = gradient.m_Hz < 0.0f ? 1.0f : 0.0f;
            phase = fabs(phase);
        }
        output->m_UV[0] = output->m_UV[2] = x;
        output->m_UV[1] = output->m_UV[3] = 0.5f;
    }
    else if (gradient.m_Fit == TEXT_EFFECT_FIT_GLYPH && gradient.m_Mode == TEXT_GRADIENT_MODE_HORIZONTAL)
    {
        output->m_UV[0] = output->m_UV[2] = ((float)((int64_t)glyph.m_Cluster - effect->m_TextOffset) + 0.5f) / effect->m_TextLength;
        output->m_UV[1] = output->m_UV[3] = 0.5f;
    }
    // Reduce in double precision before passing time through the vertex format.
    output->m_Phase = (float)(phase - floor(phase * 0.5) * 2.0);
    return true;
}

static void SetGlyphColorEffect(FontGlyphVertex* vertices, const GlyphColorEffect& effect, bool vertex_evaluation)
{
    if (!vertices)
    {
        return;
    }
    static const uint8_t corners[6][2] = { { 0, 1 }, { 2, 1 }, { 0, 3 }, { 0, 3 }, { 2, 1 }, { 2, 3 } };
    for (uint32_t i = 0; i < 6; ++i)
    {
        vertices[i].m_Effect[0] = effect.m_UV[corners[i][0]];
        vertices[i].m_Effect[1] = effect.m_UV[corners[i][1]];
        vertices[i].m_Effect[2] = vertex_evaluation ? -(float)effect.m_Index : (float)effect.m_Index;
        vertices[i].m_Effect[3] = effect.m_Phase;
    }
}

typedef TextGradientBounds SpatialGradientBounds;

// Reuse projection, packed record, and resolver result for adjacent glyphs with the same fit.
struct SpatialGradientCache
{
    const TextEffect* m_Effect;
    float             m_Width, m_Height;
    FontColorEffect   m_Color;
    uint32_t          m_Index;
};

static SpatialGradientBounds GetGlyphUnpaddedBounds(const FontLayoutVertexConfig& config, const TextGlyph& glyph, float x, float y)
{
    FontGlyphOptions options;
    // Measure at the resolved layout size; a named visual style may not declare a size.
    options.m_Scale = FontGetScaleFromSize(glyph.m_Font, config.m_Layout->m_Styles[glyph.m_BaseStyleIndex].m_FontSize);
    options.m_UseUnpaddedBounds = true;
    FontGlyph metrics = {};
    FontGetGlyphByIndex(glyph.m_Font, glyph.m_GlyphIndex, &options, &metrics);
    // Glyph banks retain baked metrics; dynamic fonts were measured at the styled size.
    const float           scale = FontGetType(glyph.m_Font) == FONT_TYPE_GLYPH_BANK ? glyph.m_RenderScale : 1.0f;
    SpatialGradientBounds bounds = { x + metrics.m_LeftBearing * scale, y - metrics.m_Descent * scale, x + (metrics.m_LeftBearing + metrics.m_Width) * scale, y + metrics.m_Ascent * scale };
    return bounds;
}

static void ExpandGradientBounds(SpatialGradientBounds* bounds, const SpatialGradientBounds& unpadded)
{
    bounds->m_MinX = dmMath::Min(bounds->m_MinX, unpadded.m_MinX);
    bounds->m_MinY = dmMath::Min(bounds->m_MinY, unpadded.m_MinY);
    bounds->m_MaxX = dmMath::Max(bounds->m_MaxX, unpadded.m_MaxX);
    bounds->m_MaxY = dmMath::Max(bounds->m_MaxY, unpadded.m_MaxY);
}

static void PrepareSpatialGradientBounds(const FontLayoutVertexConfig& config)
{
    TextLayout* layout = config.m_Layout;
    bool        needed = false;
    bool        span_bounds = false;
    for (uint32_t i = 0; i < layout->m_Effects.Size(); ++i)
    {
        const TextEffect& effect = layout->m_Effects[i];
        needed |= effect.m_Type == TEXT_EFFECT_GRADIENT && effect.m_Gradient.m_Mode == TEXT_GRADIENT_MODE_SPATIAL &&
                  effect.m_Gradient.m_Color.m_Type != FONT_COLOR_EFFECT_COLOR &&
                  (effect.m_Gradient.m_Fit != TEXT_EFFECT_FIT_GLYPH || effect.m_Gradient.m_Color.m_Type == FONT_COLOR_EFFECT_RADIAL);
        span_bounds |= effect.m_Type == TEXT_EFFECT_GRADIENT && effect.m_Gradient.m_Mode == TEXT_GRADIENT_MODE_SPATIAL &&
                       effect.m_Gradient.m_Color.m_Type != FONT_COLOR_EFFECT_COLOR && effect.m_Gradient.m_Fit == TEXT_EFFECT_FIT_SPAN;
    }
    if (!needed)
    {
        return;
    }

    const uint32_t effect_count = layout->m_Effects.Size();
    const uint32_t line_count = layout->m_Lines.Size();
    // A range tree over source clusters includes glyphs hidden by nested color
    // overrides and handles visual reordering in shaped text. Each glyph is
    // measured once; each authored span queries its range in logarithmic time.
    uint32_t       leaves = 0;
    if (span_bounds)
    {
        for (uint32_t i = 0; i < layout->m_Glyphs.Size(); ++i)
        {
            leaves = dmMath::Max(leaves, layout->m_Glyphs[i].m_Cluster + 1);
        }
        leaves = dmMath::Max(leaves, 1u);
    }
    const uint32_t tree_offset = effect_count + line_count + 1;
    layout->m_GradientBounds.EnsureSize(tree_offset + 2 * leaves);
    const SpatialGradientBounds empty = { INFINITY, INFINITY, -INFINITY, -INFINITY };
    for (uint32_t i = 0; i < layout->m_GradientBounds.Size(); ++i)
    {
        layout->m_GradientBounds[i] = empty;
    }
    SpatialGradientBounds& block = layout->m_GradientBounds[effect_count + line_count];
    const float            layout_y = OffsetLayoutY(config.m_VerticalAlign, config.m_Height, layout->m_Height);
    for (uint32_t l = 0; l < line_count; ++l)
    {
        const TextLine& line = layout->m_Lines[l];
        if (!line.m_Length)
        {
            continue;
        }
        const float first_x = TextLayoutGetLineOriginX(layout, l);
        const float first_y = layout->m_Glyphs[line.m_Index].m_Y;
        const float line_x = GetLineStartX(config, line, layout->m_Paragraphs[line.m_ParagraphIndex].m_Direction);
        for (uint32_t i = line.m_Index; i < line.m_Index + line.m_Length; ++i)
        {
            const TextGlyph& glyph = layout->m_Glyphs[i];
            if ((glyph.m_Flags & TEXT_GLYPH_FLAG_OBJECT) || dmUtf8::IsWhiteSpace(glyph.m_Codepoint))
            {
                continue;
            }
            const SpatialGradientBounds unpadded = GetGlyphUnpaddedBounds(config, glyph, line_x + glyph.m_X - first_x, layout_y + line.m_Baseline + glyph.m_Y - first_y);
            ExpandGradientBounds(&block, unpadded);
            ExpandGradientBounds(&layout->m_GradientBounds[effect_count + l], unpadded);
            if (leaves)
            {
                ExpandGradientBounds(&layout->m_GradientBounds[tree_offset + leaves + glyph.m_Cluster], unpadded);
            }
        }
    }
    if (leaves)
    {
        SpatialGradientBounds* tree = layout->m_GradientBounds.Begin() + tree_offset;
        for (uint32_t node = leaves - 1; node; --node)
        {
            tree[node] = tree[node * 2];
            ExpandGradientBounds(&tree[node], tree[node * 2 + 1]);
        }
        for (uint32_t i = 0; i < effect_count; ++i)
        {
            const TextEffect& effect = layout->m_Effects[i];
            if (effect.m_Type != TEXT_EFFECT_GRADIENT || effect.m_Gradient.m_Mode != TEXT_GRADIENT_MODE_SPATIAL ||
                effect.m_Gradient.m_Fit != TEXT_EFFECT_FIT_SPAN)
            {
                continue;
            }
            uint32_t start = leaves + dmMath::Min(effect.m_TextOffset, leaves);
            uint32_t end = leaves + dmMath::Min(effect.m_TextOffset + effect.m_TextLength, leaves);
            while (start < end)
            {
                if (start & 1)
                {
                    ExpandGradientBounds(&layout->m_GradientBounds[i], tree[start++]);
                }
                if (end & 1)
                {
                    ExpandGradientBounds(&layout->m_GradientBounds[i], tree[--end]);
                }
                start /= 2;
                end /= 2;
            }
        }
    }
    // Whitespace-only decorations have no unpadded bounds.
    for (uint32_t i = 0; i < tree_offset; ++i)
    {
        if (layout->m_GradientBounds[i].m_MinX == INFINITY)
        {
            memset(&layout->m_GradientBounds[i], 0, sizeof(SpatialGradientBounds));
        }
    }
}

static float SampleGradientCoordinate(float t, float phase, uint8_t flags)
{
    if (!flags)
    {
        return dmMath::Clamp(t, 0.0f, 1.0f);
    }
    t += phase;
    return 1.0f - fabsf(t - floorf(t * 0.5f) * 2.0f - 1.0f);
}

// Compatibility for materials without the metadata sampler: evaluate at the vertices.
static void ApplySpatialGradientFallback(FontGlyphVertex* vertices, const FontColorEffect& color, const GlyphColorEffect& effect)
{
    if (!vertices)
    {
        return;
    }
    static const uint8_t corners[6][2] = { { 0, 1 }, { 2, 1 }, { 0, 3 }, { 0, 3 }, { 2, 1 }, { 2, 3 } };
    for (uint32_t i = 0; i < 6; ++i)
    {
        float    x = effect.m_UV[corners[i][0]], y = effect.m_UV[corners[i][1]];
        uint32_t left = 0, right = 0;
        float    t = 0.0f;
        if (color.m_Type == FONT_COLOR_EFFECT_FOUR_CORNER)
        {
            x = SampleGradientCoordinate(x, effect.m_Phase, color.m_Flags);
            y = SampleGradientCoordinate(y, effect.m_Phase, color.m_Flags);
        }
        else if (color.m_Type != FONT_COLOR_EFFECT_COLOR)
        {
            if (color.m_Type == FONT_COLOR_EFFECT_RADIAL)
            {
                x -= color.m_Parameters[0];
                y -= color.m_Parameters[1];
                const float u = x * color.m_Parameters[2] + y * color.m_Parameters[4];
                const float v = y * color.m_Parameters[3] + x * color.m_Parameters[5];
                t = sqrtf(u * u + v * v);
            }
            else
            {
                t = x * color.m_Parameters[0] + y * color.m_Parameters[1] + color.m_Parameters[2];
            }
            t = SampleGradientCoordinate(t, effect.m_Phase, color.m_Flags);
            while (right + 1 < color.m_StopCount && t >= color.m_Positions[right])
            {
                ++right;
            }
            left = right ? right - 1 : 0;
            const float a = color.m_Positions[left], b = color.m_Positions[right];
            t = b > a ? dmMath::Clamp((t - a) / (b - a), 0.0f, 1.0f) : 1.0f;
        }
        for (uint32_t channel = 0; channel < 4; ++channel)
        {
            float sample;
            if (color.m_Type == FONT_COLOR_EFFECT_FOUR_CORNER)
            {
                const float bottom = color.m_Colors[0][channel] * (1.0f - x) + color.m_Colors[1][channel] * x;
                const float top = color.m_Colors[2][channel] * (1.0f - x) + color.m_Colors[3][channel] * x;
                sample = bottom * (1.0f - y) + top * y;
            }
            else
            {
                sample = color.m_Colors[left][channel] * (1.0f - t) + color.m_Colors[right][channel] * t;
            }
            vertices[i].m_FaceColor[channel] = (uint8_t)((channel == 3 ? dmMath::Min((float)vertices[i].m_FaceColor[channel], sample) : sample) + 0.5f);
        }
    }
}

static void ApplySpatialGradient(const FontLayoutVertexConfig& config, const TextLine& line, const TextGlyph& glyph, const SpatialGradientBounds& unpadded, const SpatialGradientBounds& quad, SpatialGradientCache* cache, const FontVertexLayerParams& layers)
{
    const TextEffect* effect = FindGlyphGradient(config.m_Layout, glyph);
    if (!effect || effect->m_Gradient.m_Mode != TEXT_GRADIENT_MODE_SPATIAL)
    {
        return;
    }
    const TextGradientEffect& gradient = effect->m_Gradient;
    SpatialGradientBounds     bounds = unpadded;
    const TextLayout*         layout = config.m_Layout;
    if (gradient.m_Color.m_Type == FONT_COLOR_EFFECT_COLOR)
    {
        bounds = quad;
    }
    else if (gradient.m_Fit == TEXT_EFFECT_FIT_BLOCK)
    {
        bounds = layout->m_GradientBounds[layout->m_Effects.Size() + layout->m_Lines.Size()];
    }
    else if (gradient.m_Fit == TEXT_EFFECT_FIT_SPAN)
    {
        bounds = layout->m_GradientBounds[effect - layout->m_Effects.Begin()];
    }
    else if (gradient.m_Color.m_Type == FONT_COLOR_EFFECT_RADIAL)
    {
        const SpatialGradientBounds& line_bounds = layout->m_GradientBounds[layout->m_Effects.Size() + (&line - layout->m_Lines.Begin())];
        // Keep glyph centers aligned vertically, including ascenders and descenders.
        bounds.m_MinY = line_bounds.m_MinY;
        bounds.m_MaxY = line_bounds.m_MaxY;
    }
    const float width = dmMath::Max(bounds.m_MaxX - bounds.m_MinX, 0.0001f);
    const float height = dmMath::Max(bounds.m_MaxY - bounds.m_MinY, 0.0001f);
    if (cache->m_Effect != effect || cache->m_Width != width || cache->m_Height != height)
    {
        FontColorEffect& color = cache->m_Color;
        color = gradient.m_Color;
        if (color.m_Type == FONT_COLOR_EFFECT_LINEAR)
        {
            // Angles rotate counterclockwise: 0 is bottom-to-top; 90 is right-to-left in local text space.
            const float angle = color.m_Parameters[0] * (float)(M_PI / 180.0);
            const float dx = -sinf(angle), dy = cosf(angle);
            const float extent = fabsf(dx) * width + fabsf(dy) * height;
            color.m_Parameters[0] = dx * width / extent;
            color.m_Parameters[1] = dy * height / extent;
            color.m_Parameters[2] = 0.5f - 0.5f * (color.m_Parameters[0] + color.m_Parameters[1]);
            color.m_Parameters[3] = 0.0f;
        }
        else if (color.m_Type == FONT_COLOR_EFFECT_RADIAL)
        {
            const float radius_x = color.m_Parameters[2] * dmMath::Max(width, height);
            const float radius_y = color.m_Parameters[3] * dmMath::Max(width, height);
            const float angle = color.m_Parameters[4] * (float)(M_PI / 180.0);
            const float cosine = cosf(angle), sine = sinf(angle);
            // Undo the counterclockwise rotation in local distance units before
            // dividing by each radius. Rotating normalized UVs would distort the ellipse.
            color.m_Parameters[2] = cosine * width / radius_x;
            color.m_Parameters[3] = cosine * height / radius_y;
            color.m_Parameters[4] = sine * height / radius_x;
            color.m_Parameters[5] = -sine * width / radius_y;
        }
        FontEffectRecord record;
        FontPackEffect(color, &record);
        cache->m_Index = config.m_ResolveEffect ? config.m_ResolveEffect(config.m_ResolveEffectContext, record) : UINT32_MAX;
        cache->m_Effect = effect;
        cache->m_Width = width;
        cache->m_Height = height;
    }
    GlyphColorEffect resolved;
    resolved.m_UV[0] = (quad.m_MinX - bounds.m_MinX) / width;
    resolved.m_UV[1] = (quad.m_MinY - bounds.m_MinY) / height;
    resolved.m_UV[2] = (quad.m_MaxX - bounds.m_MinX) / width;
    resolved.m_UV[3] = (quad.m_MaxY - bounds.m_MinY) / height;
    const double phase = config.m_Layout->m_ElapsedTime * gradient.m_Hz * 2.0;
    resolved.m_Phase = (float)(phase - floor(phase * 0.5) * 2.0);
    resolved.m_Index = cache->m_Index;
    FontGlyphVertex* vertices[] = { layers.m_FaceVertices, layers.m_OutlineVertices, layers.m_ShadowVertices };
    for (uint32_t i = 0; i < 3; ++i)
    {
        if (resolved.m_Index != UINT32_MAX)
        {
            SetGlyphColorEffect(vertices[i], resolved, false);
        }
        else
        {
            ApplySpatialGradientFallback(vertices[i], cache->m_Color, resolved);
        }
    }
}

struct GlyphRenderDataCache
{
    TextGlyphRenderData m_Data;
    uint32_t            m_FaceColors[4];
    uint16_t            m_StyleIndex;
    uint16_t            m_SpanIndex;
    bool                m_Valid;
    bool                m_GPUGradient;
};

static bool IsSpanConstantEffect(const TextEffect& effect)
{
    if (effect.m_Type == TEXT_EFFECT_GRADIENT)
    {
        return effect.m_Gradient.m_Fit == TEXT_EFFECT_FIT_SPAN;
    }

    if (effect.m_Type == TEXT_EFFECT_SHAKE)
    {
        return effect.m_Shake.m_Fit == TEXT_EFFECT_FIT_SPAN;
    }

    if (effect.m_Type == TEXT_EFFECT_WAVE)
    {
        return effect.m_Wave.m_Fit == TEXT_EFFECT_FIT_SPAN;
    }

    return false;
}

static bool GetRenderDataCacheKey(HTextLayout layout, const TextGlyph& glyph, uint16_t* span_index)
{
    *span_index = MARKUP_INVALID_INDEX;

    if (glyph.m_MarkupSpanIndex >= layout->m_ResolvedSpans.Size())
    {
        return true;
    }

    const TextResolvedSpan& span = layout->m_ResolvedSpans[glyph.m_MarkupSpanIndex];

    if (span.m_EffectCount == 0)
    {
        return true;
    }

    for (uint32_t i = 0; i < span.m_EffectCount; ++i)
    {
        const TextEffect& effect = layout->m_Effects[layout->m_SpanEffects[span.m_EffectIndex + i]];

        if (!IsSpanConstantEffect(effect))
        {
            return false;
        }
    }

    *span_index = glyph.m_MarkupSpanIndex;

    return true;
}

static const TextGlyphRenderData* GetGlyphRenderData(const FontLayoutVertexConfig& config,
                                                     const TextGlyph&              glyph,
                                                     bool                          gpu_gradient,
                                                     GlyphRenderDataCache*         cache,
                                                     TextGlyphRenderData*          scratch,
                                                     uint32_t                      scratch_face_colors[4],
                                                     const uint32_t**              face_colors)
{
    if (cache->m_Valid && cache->m_GPUGradient == gpu_gradient && cache->m_SpanIndex != MARKUP_INVALID_INDEX &&
        cache->m_StyleIndex == glyph.m_StyleIndex && cache->m_SpanIndex == glyph.m_MarkupSpanIndex)
    {
        *face_colors = cache->m_FaceColors;

        return &cache->m_Data;
    }

    uint16_t   cache_span_index;
    const bool cacheable = GetRenderDataCacheKey(config.m_Layout, glyph, &cache_span_index);

    if (cacheable && cache->m_Valid && cache->m_GPUGradient == gpu_gradient && cache->m_StyleIndex == glyph.m_StyleIndex && cache->m_SpanIndex == cache_span_index)
    {
        *face_colors = cache->m_FaceColors;

        return &cache->m_Data;
    }

    TextGlyphRenderData* data = cacheable ? &cache->m_Data : scratch;
    TextLayoutGetGlyphRenderData(config.m_Layout, glyph, config.m_FaceColor, data, !gpu_gradient);
    uint32_t* packed_face_colors = cacheable ? cache->m_FaceColors : scratch_face_colors;
    FontPackGlyphFaceColors(data->m_FaceColors, packed_face_colors);
    *face_colors = packed_face_colors;

    if (cacheable)
    {
        cache->m_StyleIndex = glyph.m_StyleIndex;
        cache->m_SpanIndex = cache_span_index;
        cache->m_Valid = true;
        cache->m_GPUGradient = gpu_gradient;
    }

    return data;
}

struct GlyphLayerRenderData
{
    uint32_t m_OutlineColor;
    uint32_t m_ShadowColor;
    float    m_SdfOutline;
    float    m_SdfShadow;
    float    m_OutlineWidth;
    float    m_ShadowX;
    float    m_ShadowY;
    uint8_t  m_LayerMask;
};

// Resolves the final outline and shadow values shared by glyph and decoration
// vertices. Layer presence is derived from the same style flags as metrics.
static void ResolveGlyphLayerRenderData(const FontLayoutVertexConfig& config,
                                        const TextGlyph&              glyph,
                                        const TextGlyphRenderData&    render_data,
                                        GlyphLayerRenderData*         layer_data)
{
    const bool    use_rich_text = config.m_Layout->m_UseRichText;
    const bool    has_base_outline = !use_rich_text &&
                                     ((config.m_BaseLayerMask & FONT_RENDER_LAYER_OUTLINE) != 0 || config.m_OutlineWidth > 0.0f);
    const bool    has_base_shadow = !use_rich_text &&
                                    ((config.m_BaseLayerMask & FONT_RENDER_LAYER_SHADOW) != 0 || config.m_BaseShadowAlpha > 0.0f);
    const bool    has_explicit_outline_width = (render_data.m_StyleFlags & TEXT_RENDER_STYLE_OUTLINE_WIDTH) != 0;
    const bool    markup_outline = use_rich_text &&
                                   (render_data.m_StyleFlags & OUTLINE_STYLE_FLAGS) != 0 &&
                                   (!has_explicit_outline_width || render_data.m_OutlineWidth > 0.0f) &&
                                   config.m_OutlineWidth > 0.0f;
    const float   outline_alpha = !has_base_outline && !markup_outline ? 0.0f : config.m_OutlineColor.getW() * render_data.m_OutlineColor[3];
    const bool    markup_outline_color = markup_outline && (render_data.m_StyleFlags & TEXT_RENDER_STYLE_OUTLINE_COLOR) != 0;
    const Vector4 outline_color(markup_outline_color ? render_data.m_OutlineColor[0] : config.m_OutlineColor.getX(),
                                markup_outline_color ? render_data.m_OutlineColor[1] : config.m_OutlineColor.getY(),
                                markup_outline_color ? render_data.m_OutlineColor[2] : config.m_OutlineColor.getZ(),
                                outline_alpha);
    layer_data->m_SdfOutline = config.m_SdfOutline;
    layer_data->m_OutlineWidth = 0.0f;

    if (markup_outline)
    {
        const float requested_width = has_explicit_outline_width ? render_data.m_OutlineWidth / glyph.m_RenderScale : config.m_OutlineWidth;
        const float width = config.m_IsSdf ? dmMath::Min(requested_width, config.m_OutlineWidth) : config.m_OutlineWidth;
        if (config.m_SdfSpread > 0.0f &&
            (config.m_BaseLayerMask != FONT_RENDER_LAYER_FACE || width < config.m_OutlineWidth))
        {
            layer_data->m_SdfOutline = config.m_SdfEdge - FONT_SDF_DISTANCE_SCALE * width / config.m_SdfSpread;
        }
        layer_data->m_OutlineWidth = width * glyph.m_RenderScale;
    }
    else if (has_base_outline)
    {
        float width = config.m_OutlineWidth;
        if (width <= 0.0f && config.m_SdfSpread > 0.0f)
        {
            width = dmMath::Max(0.0f, config.m_SdfEdge - config.m_SdfOutline) * config.m_SdfSpread / FONT_SDF_DISTANCE_SCALE;
        }
        layer_data->m_OutlineWidth = width * glyph.m_RenderScale;
    }

    const bool    markup_shadow = use_rich_text && (render_data.m_StyleFlags & SHADOW_STYLE_FLAGS) != 0;
    float         shadow_alpha = 0.0f;
    if (markup_shadow)
    {
        shadow_alpha = config.m_ShadowColor.getW() * render_data.m_ShadowColor[3];
    }
    else if (has_base_shadow)
    {
        shadow_alpha = config.m_ShadowColor.getW() * config.m_BaseShadowAlpha * render_data.m_ShadowColor[3];
    }
    const bool    markup_shadow_color = markup_shadow && (render_data.m_StyleFlags & TEXT_RENDER_STYLE_SHADOW_COLOR) != 0;
    const Vector4 shadow_color(markup_shadow_color ? render_data.m_ShadowColor[0] : config.m_ShadowColor.getX(),
                               markup_shadow_color ? render_data.m_ShadowColor[1] : config.m_ShadowColor.getY(),
                               markup_shadow_color ? render_data.m_ShadowColor[2] : config.m_ShadowColor.getZ(),
                               shadow_alpha);
    layer_data->m_SdfShadow = config.m_SdfShadow;

    if (markup_shadow)
    {
        const bool  has_explicit_blur = (render_data.m_StyleFlags & TEXT_RENDER_STYLE_SHADOW_BLUR) != 0;
        const float requested_blur = has_explicit_blur ? render_data.m_ShadowBlur / glyph.m_RenderScale : config.m_ShadowBlur;

        const bool shadow_has_hidden_outline = config.m_ShadowIncludesOutline && !markup_outline;
        // Base-style flags describe inherited font effects, not explicit markup.
        // Preserve their shadow channel in both layer modes; authored shadow
        // overrides retain the face-based coverage used by rich text.
        const uint32_t override_flags = glyph.m_StyleIndex < config.m_Layout->m_StyleOverrideFlags.Size() ?
                                         config.m_Layout->m_StyleOverrideFlags[glyph.m_StyleIndex] : render_data.m_StyleFlags;
        const bool inherited_shadow = (override_flags & SHADOW_STYLE_FLAGS) == 0;
        const bool use_baked_shadow = (inherited_shadow || config.m_BaseLayerMask == FONT_RENDER_LAYER_FACE) &&
                                      config.m_BaseShadowAlpha > 0.0f && requested_blur >= config.m_ShadowBlur &&
                                      !shadow_has_hidden_outline;
        const bool use_face_coverage = requested_blur <= 0.0f || config.m_ShadowBlur <= 0.0f || shadow_has_hidden_outline;
        if (!use_baked_shadow && use_face_coverage)
        {
            layer_data->m_SdfShadow = config.m_SdfEdge;
        }
        else if (!use_baked_shadow && config.m_SdfSpread > 0.0f)
        {
            const float blur = dmMath::Min(requested_blur, config.m_ShadowBlur);
            layer_data->m_SdfShadow = config.m_SdfEdge - FONT_SDF_DISTANCE_SCALE * blur / config.m_SdfSpread;
        }

        // Rich distance-field shadows are derived from face distance, not the
        // remapped shadow channel, which can include the configured outline.
        // Bitmap fonts can only select face coverage for crisp shadows because
        // their positive blur is already baked into the shadow channel.
        if (!use_baked_shadow && (config.m_IsSdf || use_face_coverage))
        {
            layer_data->m_SdfShadow = EncodeFaceShadowThreshold(layer_data->m_SdfShadow);
        }
    }

    layer_data->m_OutlineColor = FontPackColor(outline_color);
    layer_data->m_ShadowColor = FontPackColor(shadow_color);
    layer_data->m_ShadowX = render_data.m_StyleFlags & TEXT_RENDER_STYLE_SHADOW_X ? render_data.m_ShadowX : config.m_ShadowX;
    layer_data->m_ShadowY = render_data.m_StyleFlags & TEXT_RENDER_STYLE_SHADOW_Y ? render_data.m_ShadowY : config.m_ShadowY;
    layer_data->m_LayerMask = GetGlyphLayerMask(config, glyph);
}

// Builds the shared layer parameters for a resolved glyph or decoration span.
static void SetVertexLayerParams(const FontLayoutVertexConfig& config,
                                 const GlyphLayerRenderData&    render_data,
                                 const uint32_t                 face_colors[4],
                                 float                          render_scale,
                                 uint32_t                       layer_count,
                                 FontGlyphVertex*               face_vertices,
                                 FontGlyphVertex*               outline_vertices,
                                 FontGlyphVertex*               shadow_vertices,
                                 FontVertexLayerParams*         params)
{
    params->m_Transform = &config.m_Transform;
    params->m_FaceColors = face_colors;
    params->m_FaceVertices = face_vertices;
    params->m_OutlineVertices = outline_vertices;
    params->m_ShadowVertices = shadow_vertices;
    params->m_OutlineColor = render_data.m_OutlineColor;
    params->m_ShadowColor = render_data.m_ShadowColor;
    params->m_SdfEdge = config.m_SdfEdge;
    params->m_SdfOutline = render_data.m_SdfOutline;
    params->m_SdfSmoothing = config.m_SdfSmoothing / render_scale;
    // Values above the legacy [0, 1] range tell the built-in shaders to reuse
    // face coverage. Keeping the encoding positive makes old custom shaders
    // fall back to their previous outline-shaped shadow instead of overfilling.
    params->m_SdfShadow = config.m_ShadowUsesFaceCoverage ? EncodeFaceShadowThreshold(config.m_SdfEdge) : render_data.m_SdfShadow;
    params->m_ShadowX = render_data.m_ShadowX;
    params->m_ShadowY = render_data.m_ShadowY;
    params->m_LayerCount = layer_count;
    params->m_FaceOnly = config.m_BaseLayerMask != FONT_RENDER_LAYER_FACE;
}

uint32_t FontCreateLayoutVertices(const FontLayoutVertexConfig&  config,
                                  const FontLayoutVertexMetrics& metrics,
                                  FontGlyphVertex*               vertices,
                                  uint32_t                       max_vertices)
{
    if (metrics.m_VertexCount > max_vertices)
    {
        return 0;
    }

    TextGlyph*     glyphs = TextLayoutGetGlyphs(config.m_Layout);
    TextLine*      lines = TextLayoutGetLines(config.m_Layout);
    TextParagraph* paragraphs = TextLayoutGetParagraphs(config.m_Layout);
    float          layout_width;
    float          layout_height;
    TextLayoutGetBounds(config.m_Layout, &layout_width, &layout_height);
    (void)layout_width;
    const float          layout_y = OffsetLayoutY(config.m_VerticalAlign, config.m_Height, layout_height);
    uint32_t             shadow_vertex_index = 0;
    uint32_t             outline_vertex_index = metrics.m_ShadowQuadCount * 6;
    uint32_t             face_vertex_index = (metrics.m_ShadowQuadCount + metrics.m_OutlineQuadCount) * 6;
    uint32_t             emitted_glyphs = 0;
    uint32_t             emitted_outlines = 0;
    uint32_t             emitted_shadows = 0;
    uint32_t             emitted_objects = 0;
    GlyphRenderDataCache render_data_cache = {};
    SpatialGradientCache spatial_cache = {};
    PrepareSpatialGradientBounds(config);
    const uint32_t       line_count = TextLayoutGetLineCount(config.m_Layout);

    for (uint32_t line_index = 0; line_index < line_count; ++line_index)
    {
        const TextLine& line = lines[line_index];

        if (line.m_Length == 0)
        {
            continue;
        }

        const float first_x = TextLayoutGetLineOriginX(config.m_Layout, line_index);
        const float first_y = glyphs[line.m_Index].m_Y;

        const float line_x = GetLineStartX(config, line, paragraphs[line.m_ParagraphIndex].m_Direction);
        const float line_y = layout_y + line.m_Baseline;

        for (uint32_t i = line.m_Index; i < line.m_Index + line.m_Length; ++i)
        {
            const TextGlyph& text_glyph = glyphs[i];
            const float      x = line_x + text_glyph.m_X - first_x;
            const float      y = line_y + text_glyph.m_Y - first_y;

            if (text_glyph.m_Flags & TEXT_GLYPH_FLAG_OBJECT)
            {
                if (config.m_RenderObjectOutlines && emitted_objects < metrics.m_ObjectQuadCount)
                {
                    EmitObjectOutline(config, text_glyph, x, line_y - text_glyph.m_Height * 0.2f, &face_vertex_index, vertices);
                    emitted_objects += 4;
                }

                continue;
            }

            if (dmUtf8::IsWhiteSpace(text_glyph.m_Codepoint))
            {
                continue;
            }

            if (emitted_glyphs >= metrics.m_GlyphQuadCount)
            {
                continue;
            }

            FontLayoutCachedGlyph cached = {};

            if (!config.m_ResolveGlyph(config.m_ResolveGlyphContext, text_glyph, &cached))
            {
                continue;
            }

            GlyphColorEffect           color_effect;
            const bool                 gpu_gradient = ResolveGlyphColorEffect(config, text_glyph, &color_effect);
            TextGlyphRenderData        scratch_render_data;
            uint32_t                   scratch_face_colors[4];
            const uint32_t*            face_colors;
            const TextGlyphRenderData* render_data = GetGlyphRenderData(config, text_glyph, gpu_gradient, &render_data_cache, &scratch_render_data, scratch_face_colors, &face_colors);
            GlyphLayerRenderData       layer_data;
            ResolveGlyphLayerRenderData(config, text_glyph, *render_data, &layer_data);
            FontGlyphVertex* outline_vertices = (layer_data.m_LayerMask & FONT_RENDER_LAYER_OUTLINE) != 0 && emitted_outlines < metrics.m_OutlineQuadCount ? vertices + outline_vertex_index : 0;
            FontGlyphVertex* shadow_vertices = (layer_data.m_LayerMask & FONT_RENDER_LAYER_SHADOW) != 0 && emitted_shadows < metrics.m_ShadowQuadCount ? vertices + shadow_vertex_index : 0;

            FontGlyphVertexParams glyph_params;
            glyph_params.m_Glyph = cached.m_Glyph;
            glyph_params.m_RecipAtlasWidth = config.m_RecipAtlasWidth;
            glyph_params.m_RecipAtlasHeight = config.m_RecipAtlasHeight;
            glyph_params.m_X = x + render_data->m_OffsetX;
            glyph_params.m_Y = y + render_data->m_OffsetY;
            glyph_params.m_RenderScale = text_glyph.m_RenderScale;
            glyph_params.m_CellX = cached.m_CellX;
            glyph_params.m_CellY = cached.m_CellY;
            glyph_params.m_CacheCellMaxAscent = config.m_CacheCellMaxAscent;
            glyph_params.m_CacheCellPadding = config.m_CacheCellPadding;
            glyph_params.m_MetricsFromTtf = config.m_MetricsFromTtf;

            FontVertexLayerParams layers;
            SetVertexLayerParams(config, layer_data, face_colors, text_glyph.m_RenderScale, metrics.m_LayerCount,
                                 vertices + face_vertex_index, outline_vertices, shadow_vertices, &layers);
            FontPackGlyphVertices(glyph_params, layers);
            const TextEffect* spatial_effect = FindGlyphGradient(config.m_Layout, text_glyph);
            if (cached.m_Glyph && spatial_effect && spatial_effect->m_Gradient.m_Mode == TEXT_GRADIENT_MODE_SPATIAL)
            {
                const FontGlyph&            glyph = *cached.m_Glyph;
                const float                 scale = text_glyph.m_RenderScale;
                const float                 width = config.m_MetricsFromTtf ? glyph.m_Bitmap.m_Width : glyph.m_Width;
                const float                 left = x + (glyph.m_LeftBearing - (width - glyph.m_Width) * 0.5f) * scale;
                const SpatialGradientBounds quad = { left, y - glyph.m_Descent * scale, left + width * scale, y + glyph.m_Ascent * scale };
                const SpatialGradientBounds unpadded = spatial_effect->m_Gradient.m_Fit == TEXT_EFFECT_FIT_GLYPH && spatial_effect->m_Gradient.m_Color.m_Type != FONT_COLOR_EFFECT_COLOR ? GetGlyphUnpaddedBounds(config, text_glyph, x, y) : quad;
                ApplySpatialGradient(config, line, text_glyph, unpadded, quad, &spatial_cache, layers);
            }
            if (gpu_gradient)
            {
                SetGlyphColorEffect(layers.m_FaceVertices, color_effect, true);
                SetGlyphColorEffect(outline_vertices, color_effect, true);
                SetGlyphColorEffect(shadow_vertices, color_effect, true);
            }
            face_vertex_index += 6;

            if (outline_vertices)
            {
                outline_vertex_index += 6;
                ++emitted_outlines;
            }

            if (shadow_vertices)
            {
                shadow_vertex_index += 6;
                ++emitted_shadows;
            }

            ++emitted_glyphs;
        }
    }

    // Both layout backends resolve underline and strikethrough into the same
    // baseline-relative geometry. Emit the layers active on the decorated
    // glyph, and split only when needed to preserve per-glyph styling. Position
    // effects remain on glyphs and do not move decorations.
    const TextDecoration* decorations = TextLayoutGetDecorations(config.m_Layout);
    const uint32_t        decoration_vertex_index = face_vertex_index;
    const uint32_t        decoration_count = TextLayoutGetDecorationCount(config.m_Layout);
    uint32_t              emitted_decorations = 0;
    uint32_t              cached_line_index = UINT32_MAX;
    float                 cached_first_x = 0.0f;

    for (uint32_t decoration_index = 0; config.m_RenderDecorations && decoration_index < decoration_count && emitted_decorations < metrics.m_DecorationQuadCount; ++decoration_index)
    {
        const TextDecoration& decoration = decorations[decoration_index];
        const TextLine&       line = lines[decoration.m_LineIndex];

        if (line.m_Length == 0)
        {
            continue;
        }

        if (cached_line_index != decoration.m_LineIndex)
        {
            cached_line_index = decoration.m_LineIndex;
            cached_first_x = TextLayoutGetLineOriginX(config.m_Layout, decoration.m_LineIndex);
        }

        const float         line_x = GetLineStartX(config, line, paragraphs[line.m_ParagraphIndex].m_Direction);
        const float         line_y = layout_y + line.m_Baseline;
        const bool          glyph_segments = FontDecorationRequiresGlyphSegments(config.m_Layout, decoration);
        const uint32_t      segment_count = glyph_segments ? decoration.m_GlyphCount : 1;
        TextGlyphFaceColors decoration_colors;
        uint32_t            packed_decoration_colors[4];

        GlyphColorEffect    decoration_effect;
        bool                gpu_decoration = false;
        if (!glyph_segments && config.m_ResolveEffect)
        {
            uint32_t left, right;
            FontGetDecorationEndGlyphs(config.m_Layout, decoration, &left, &right);
            GlyphColorEffect right_effect;
            gpu_decoration = ResolveGlyphColorEffect(config, glyphs[left], &decoration_effect) &&
                             ResolveGlyphColorEffect(config, glyphs[right], &right_effect) &&
                             decoration_effect.m_Index == right_effect.m_Index;
            if (gpu_decoration)
            {
                decoration_effect.m_UV[2] = right_effect.m_UV[2];
            }
        }
        if (!glyph_segments)
        {
            FontGetDecorationFaceColors(config.m_Layout, decoration, config.m_FaceColor, &decoration_colors, !gpu_decoration);
            FontPackGlyphFaceColors(decoration_colors, packed_decoration_colors);
        }

        for (uint32_t segment = 0; segment < segment_count && emitted_decorations < metrics.m_DecorationQuadCount; ++segment)
        {
            FontDecorationSegment decoration_segment;
            FontGetDecorationSegment(config.m_Layout, decoration, segment, segment_count, &decoration_segment);
            const uint32_t      glyph_index = decoration_segment.m_GlyphIndex;
            const TextGlyph&    glyph = glyphs[glyph_index];
            TextGlyphRenderData render_data;
            GlyphColorEffect    color_effect;
            const bool          gpu_gradient = glyph_segments ? ResolveGlyphColorEffect(config, glyph, &color_effect) : gpu_decoration;
            if (gpu_decoration)
            {
                color_effect = decoration_effect;
            }
            TextLayoutGetGlyphRenderData(config.m_Layout, glyph, config.m_FaceColor, &render_data, !gpu_gradient);
            uint32_t        segment_colors[4];
            const uint32_t* face_colors = packed_decoration_colors;

            if (glyph_segments)
            {
                FontPackGlyphFaceColors(render_data.m_FaceColors, segment_colors);
                face_colors = segment_colors;
            }

            const float           x0 = line_x + decoration_segment.m_X - cached_first_x;
            FontDecorationPattern pattern;
            FontGetDecorationPattern(decoration, decoration_segment, &pattern);
            GlyphLayerRenderData layer_data;
            ResolveGlyphLayerRenderData(config, glyph, render_data, &layer_data);
            FontGlyphVertex* outline_vertices = (layer_data.m_LayerMask & FONT_RENDER_LAYER_OUTLINE) != 0 && emitted_outlines < metrics.m_OutlineQuadCount ? vertices + outline_vertex_index : 0;
            FontGlyphVertex* shadow_vertices = (layer_data.m_LayerMask & FONT_RENDER_LAYER_SHADOW) != 0 && emitted_shadows < metrics.m_ShadowQuadCount ? vertices + shadow_vertex_index : 0;

            FontDecorationVertexParams decoration_params;
            decoration_params.m_TextureU = config.m_DecorationU;
            decoration_params.m_TextureV = config.m_DecorationV;
            decoration_params.m_X0 = x0;
            decoration_params.m_Y0 = line_y + decoration.m_Y;
            decoration_params.m_X1 = x0 + decoration_segment.m_Length;
            decoration_params.m_Y1 = line_y + decoration.m_Y;
            decoration_params.m_Thickness = decoration.m_Thickness;
            decoration_params.m_PatternStart = pattern.m_Start;
            decoration_params.m_PatternEnd = pattern.m_End;
            decoration_params.m_PatternDuty = pattern.m_Duty;
            decoration_params.m_OutlineWidth = layer_data.m_OutlineWidth;

            FontVertexLayerParams layers;
            SetVertexLayerParams(config, layer_data, face_colors, glyph.m_RenderScale, metrics.m_LayerCount,
                                 vertices + face_vertex_index, outline_vertices, shadow_vertices, &layers);
            FontPackDecorationVertices(decoration_params, layers);
            const TextEffect* spatial_effect = FindGlyphGradient(config.m_Layout, glyph);
            if (spatial_effect && spatial_effect->m_Gradient.m_Mode == TEXT_GRADIENT_MODE_SPATIAL)
            {
                const SpatialGradientBounds quad = { decoration_params.m_X0, decoration_params.m_Y0 - decoration.m_Thickness * 0.5f, decoration_params.m_X1, decoration_params.m_Y1 + decoration.m_Thickness * 0.5f };
                const SpatialGradientBounds unpadded = GetGlyphUnpaddedBounds(config, glyph, line_x + glyph.m_X - cached_first_x, line_y + glyph.m_Y - glyphs[line.m_Index].m_Y);
                ApplySpatialGradient(config, line, glyph, unpadded, quad, &spatial_cache, layers);
            }
            if (gpu_gradient)
            {
                SetGlyphColorEffect(layers.m_FaceVertices, color_effect, true);
                SetGlyphColorEffect(outline_vertices, color_effect, true);
                SetGlyphColorEffect(shadow_vertices, color_effect, true);
            }
            face_vertex_index += 6;

            if (outline_vertices)
            {
                outline_vertex_index += 6;
                ++emitted_outlines;
            }

            if (shadow_vertices)
            {
                shadow_vertex_index += 6;
                ++emitted_shadows;
            }

            ++emitted_decorations;
        }
    }

    if (emitted_decorations < metrics.m_DecorationQuadCount)
    {
        const uint32_t   missing_vertex_count = (metrics.m_DecorationQuadCount - emitted_decorations) * 6;
        FontGlyphVertex* missing = vertices + decoration_vertex_index + emitted_decorations * 6;
        memset(missing, 0, missing_vertex_count * sizeof(FontGlyphVertex));
    }

    return metrics.m_VertexCount;
}
