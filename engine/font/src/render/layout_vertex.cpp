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

static const uint32_t SHADOW_STYLE_FLAGS = TEXT_RENDER_STYLE_SHADOW_COLOR | TEXT_RENDER_STYLE_SHADOW_X | TEXT_RENDER_STYLE_SHADOW_Y | TEXT_RENDER_STYLE_SHADOW_BLUR;

static const TextRenderStyle* GetGlyphStyle(HTextLayout layout, const TextGlyph& glyph)
{
    return glyph.m_StyleIndex < layout->m_Styles.Size() ? &layout->m_Styles[glyph.m_StyleIndex] : 0;
}

static uint8_t GetGlyphLayerMask(const FontLayoutVertexConfig& config, const TextGlyph& glyph)
{
    const TextRenderStyle* style = GetGlyphStyle(config.m_Layout, glyph);
    uint8_t                mask = FONT_RENDER_LAYER_FACE;

    if ((config.m_BaseLayerMask & FONT_RENDER_LAYER_OUTLINE) != 0 ||
        (config.m_SdfSpread > 0.0f && style && (style->m_Flags & TEXT_RENDER_STYLE_OUTLINE_WIDTH) && style->m_OutlineWidth > 0.0f))
    {
        mask |= FONT_RENDER_LAYER_OUTLINE;
    }

    if ((config.m_BaseLayerMask & FONT_RENDER_LAYER_SHADOW) != 0 ||
        (config.m_SdfSpread > 0.0f && style && (style->m_Flags & SHADOW_STYLE_FLAGS)))
    {
        mask |= FONT_RENDER_LAYER_SHADOW;
    }

    return mask;
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
        decoration_quad_count += FontGetDecorationQuadCount(config.m_Layout, decorations[i]);
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
        output_decoration_count = dmMath::Min(decoration_quad_count, max_quad_count - used_quad_count);
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

struct GlyphRenderDataCache
{
    TextGlyphRenderData m_Data;
    uint32_t            m_FaceColors[4];
    uint16_t            m_StyleIndex;
    uint16_t            m_SpanIndex;
    bool                m_Valid;
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
                                                     GlyphRenderDataCache*         cache,
                                                     TextGlyphRenderData*          scratch,
                                                     uint32_t                      scratch_face_colors[4],
                                                     const uint32_t**              face_colors)
{
    if (cache->m_Valid && cache->m_SpanIndex != MARKUP_INVALID_INDEX &&
        cache->m_StyleIndex == glyph.m_StyleIndex && cache->m_SpanIndex == glyph.m_MarkupSpanIndex)
    {
        *face_colors = cache->m_FaceColors;

        return &cache->m_Data;
    }

    uint16_t   cache_span_index;
    const bool cacheable = GetRenderDataCacheKey(config.m_Layout, glyph, &cache_span_index);

    if (cacheable && cache->m_Valid && cache->m_StyleIndex == glyph.m_StyleIndex && cache->m_SpanIndex == cache_span_index)
    {
        *face_colors = cache->m_FaceColors;

        return &cache->m_Data;
    }

    TextGlyphRenderData* data = cacheable ? &cache->m_Data : scratch;
    TextLayoutGetGlyphRenderData(config.m_Layout, glyph, config.m_FaceColor, data);
    uint32_t* packed_face_colors = cacheable ? cache->m_FaceColors : scratch_face_colors;
    FontPackGlyphFaceColors(data->m_FaceColors, packed_face_colors);
    *face_colors = packed_face_colors;

    if (cacheable)
    {
        cache->m_StyleIndex = glyph.m_StyleIndex;
        cache->m_SpanIndex = cache_span_index;
        cache->m_Valid = true;
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
    const bool    has_base_outline = (config.m_BaseLayerMask & FONT_RENDER_LAYER_OUTLINE) != 0;
    const bool    has_base_shadow = (config.m_BaseLayerMask & FONT_RENDER_LAYER_SHADOW) != 0;
    const bool    markup_outline = (render_data.m_StyleFlags & TEXT_RENDER_STYLE_OUTLINE_WIDTH) != 0 && render_data.m_OutlineWidth > 0.0f;
    const float   outline_alpha = !has_base_outline && !markup_outline ? 0.0f : config.m_OutlineColor.getW() * render_data.m_OutlineColor[3];
    const bool    markup_outline_color = (render_data.m_StyleFlags & TEXT_RENDER_STYLE_OUTLINE_COLOR) != 0;
    const Vector4 outline_color(markup_outline_color ? render_data.m_OutlineColor[0] : config.m_OutlineColor.getX(),
                                markup_outline_color ? render_data.m_OutlineColor[1] : config.m_OutlineColor.getY(),
                                markup_outline_color ? render_data.m_OutlineColor[2] : config.m_OutlineColor.getZ(),
                                outline_alpha);
    layer_data->m_SdfOutline = config.m_SdfOutline;
    layer_data->m_OutlineWidth = 0.0f;

    if ((render_data.m_StyleFlags & TEXT_RENDER_STYLE_OUTLINE_WIDTH) && config.m_SdfSpread > 0.0f)
    {
        const float width = dmMath::Min(render_data.m_OutlineWidth / glyph.m_RenderScale, config.m_SdfSpread);
        layer_data->m_SdfOutline = config.m_SdfEdge - (191.0f / 255.0f) * width / config.m_SdfSpread;
        layer_data->m_OutlineWidth = render_data.m_OutlineWidth;
    }
    else if (has_base_outline && config.m_SdfSpread > 0.0f)
    {
        layer_data->m_OutlineWidth = dmMath::Max(0.0f, config.m_SdfEdge - config.m_SdfOutline) * config.m_SdfSpread * glyph.m_RenderScale / (191.0f / 255.0f);
    }

    const bool    markup_shadow = (render_data.m_StyleFlags & SHADOW_STYLE_FLAGS) != 0;
    const float   shadow_alpha = !has_base_shadow && !markup_shadow ? 0.0f : config.m_ShadowColor.getW() * render_data.m_ShadowColor[3];
    const bool    markup_shadow_color = (render_data.m_StyleFlags & TEXT_RENDER_STYLE_SHADOW_COLOR) != 0;
    const Vector4 shadow_color(markup_shadow_color ? render_data.m_ShadowColor[0] : config.m_ShadowColor.getX(),
                               markup_shadow_color ? render_data.m_ShadowColor[1] : config.m_ShadowColor.getY(),
                               markup_shadow_color ? render_data.m_ShadowColor[2] : config.m_ShadowColor.getZ(),
                               shadow_alpha);
    layer_data->m_SdfShadow = !has_base_shadow && markup_shadow ? 1.0f : config.m_SdfShadow;

    if (render_data.m_StyleFlags & TEXT_RENDER_STYLE_SHADOW_BLUR)
    {
        const float blur = render_data.m_ShadowBlur / glyph.m_RenderScale;

        if (blur <= 0.0f)
        {
            layer_data->m_SdfShadow = 1.0f;
        }
        else if (has_base_shadow && config.m_ShadowBlur > 0.0f && blur < config.m_ShadowBlur && config.m_SdfSpread > 0.0f)
        {
            layer_data->m_SdfShadow = config.m_SdfEdge - (191.0f / 255.0f) * blur / config.m_SdfSpread;
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
    params->m_SdfShadow = render_data.m_SdfShadow;
    params->m_ShadowX = render_data.m_ShadowX;
    params->m_ShadowY = render_data.m_ShadowY;
    params->m_LayerCount = layer_count;
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
    const uint32_t       line_count = TextLayoutGetLineCount(config.m_Layout);

    for (uint32_t line_index = 0; line_index < line_count; ++line_index)
    {
        const TextLine& line = lines[line_index];

        if (line.m_Length == 0)
        {
            continue;
        }

        float       first_x = glyphs[line.m_Index].m_X;
        const float first_y = glyphs[line.m_Index].m_Y;

        for (uint32_t i = line.m_Index + 1; i < line.m_Index + line.m_Length; ++i)
        {
            first_x = dmMath::Min(first_x, glyphs[i].m_X);
        }

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

            TextGlyphRenderData        scratch_render_data;
            uint32_t                   scratch_face_colors[4];
            const uint32_t*            face_colors;
            const TextGlyphRenderData* render_data = GetGlyphRenderData(config, text_glyph, &render_data_cache, &scratch_render_data, scratch_face_colors, &face_colors);
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
    // baseline-relative geometry. Emit one face-layer quad per decoration when
    // possible, and split at glyph boundaries only to preserve per-glyph
    // styling. Position effects remain on glyphs and do not move decorations.
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
            cached_first_x = glyphs[line.m_Index].m_X;

            for (uint32_t i = line.m_Index + 1; i < line.m_Index + line.m_Length; ++i)
            {
                cached_first_x = dmMath::Min(cached_first_x, glyphs[i].m_X);
            }
        }

        const float         line_x = GetLineStartX(config, line, paragraphs[line.m_ParagraphIndex].m_Direction);
        const float         line_y = layout_y + line.m_Baseline;
        const bool          glyph_segments = FontDecorationRequiresGlyphSegments(config.m_Layout, decoration);
        const uint32_t      segment_count = glyph_segments ? decoration.m_GlyphCount : 1;
        const float         segment_length = decoration.m_Length / segment_count;
        TextGlyphFaceColors decoration_colors;
        uint32_t            packed_decoration_colors[4];

        if (!glyph_segments)
        {
            FontGetDecorationFaceColors(config.m_Layout, decoration, config.m_FaceColor, &decoration_colors);
            FontPackGlyphFaceColors(decoration_colors, packed_decoration_colors);
        }

        for (uint32_t segment = 0; segment < segment_count && emitted_decorations < metrics.m_DecorationQuadCount; ++segment)
        {
            uint32_t        segment_colors[4];
            const uint32_t* face_colors = packed_decoration_colors;

            if (glyph_segments)
            {
                TextGlyphRenderData render_data;
                TextLayoutGetGlyphRenderData(config.m_Layout, glyphs[decoration.m_GlyphStart + segment], config.m_FaceColor, &render_data);
                FontPackGlyphFaceColors(render_data.m_FaceColors, segment_colors);
                face_colors = segment_colors;
            }

            const float           x0 = line_x + decoration.m_X - cached_first_x + segment_length * segment;
            FontDecorationPattern pattern;
            FontGetDecorationPattern(decoration, segment, segment_count, &pattern);

            FontDecorationVertexParams decoration_params = {};
            decoration_params.m_TextureU = config.m_DecorationU;
            decoration_params.m_TextureV = config.m_DecorationV;
            decoration_params.m_X0 = x0;
            decoration_params.m_Y0 = line_y + decoration.m_Y;
            decoration_params.m_X1 = x0 + segment_length;
            decoration_params.m_Y1 = line_y + decoration.m_Y;
            decoration_params.m_Thickness = decoration.m_Thickness;
            decoration_params.m_PatternStart = pattern.m_Start;
            decoration_params.m_PatternEnd = pattern.m_End;
            decoration_params.m_PatternDuty = pattern.m_Duty;

            FontVertexLayerParams layers = {};
            layers.m_Transform = &config.m_Transform;
            layers.m_FaceColors = face_colors;
            layers.m_FaceVertices = vertices + face_vertex_index;
            layers.m_SdfEdge = 0.75f;
            layers.m_SdfOutline = 0.75f;
            layers.m_SdfSmoothing = 0.01f;
            layers.m_SdfShadow = 0.75f;
            layers.m_LayerCount = 1;
            FontPackDecorationVertices(decoration_params, layers);
            face_vertex_index += 6;
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
