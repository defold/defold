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

#include "text_layout.h"
#include "fontcollection.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/utf8.h>

static const dmhash_t TAG_SPRITE = dmHashString64("sprite");

uint32_t TextLayoutGetGlyphCount(HTextLayout layout)
{
    return layout->m_Glyphs.Size();
}

TextGlyph* TextLayoutGetGlyphs(HTextLayout layout)
{
    return layout->m_Glyphs.Begin();
}

uint32_t TextLayoutGetLineCount(HTextLayout layout)
{
    return layout->m_Lines.Size();
}

TextLine* TextLayoutGetLines(HTextLayout layout)
{
    return layout->m_Lines.Begin();
}

uint32_t TextLayoutGetParagraphCount(HTextLayout layout)
{
    return layout->m_Paragraphs.Size();
}

TextParagraph* TextLayoutGetParagraphs(HTextLayout layout)
{
    return layout->m_Paragraphs.Begin();
}

uint32_t TextLayoutGetDecorationCount(HTextLayout layout)
{
    return layout->m_Decorations.Size();
}

const TextDecoration* TextLayoutGetDecorations(HTextLayout layout)
{
    return layout->m_Decorations.Begin();
}

static int CompareDecorationGeometry(const void* left, const void* right)
{
    const TextDecorationGeometry& left_geometry = *(const TextDecorationGeometry*)left;
    const TextDecorationGeometry& right_geometry = *(const TextDecorationGeometry*)right;

    if (left_geometry.m_X < right_geometry.m_X)
    {
        return -1;
    }

    if (left_geometry.m_X > right_geometry.m_X)
    {
        return 1;
    }

    if (left_geometry.m_Length > right_geometry.m_Length)
    {
        return -1;
    }

    if (left_geometry.m_Length < right_geometry.m_Length)
    {
        return 1;
    }

    return left_geometry.m_GlyphIndex < right_geometry.m_GlyphIndex ? -1 : left_geometry.m_GlyphIndex > right_geometry.m_GlyphIndex;
}

// A decoration can use one quad when its color is constant or varies only
// across the complete span. Split it at glyph boundaries when glyphs refer to
// different styles/spans, or when a glyph-fitted gradient must be preserved.
static bool DecorationRequiresGlyphSegments(HTextLayout layout, const TextDecoration& decoration)
{
    if (decoration.m_GlyphCount <= 1)
    {
        return false;
    }

    const TextGlyph* glyphs = layout->m_Glyphs.Begin();
    const TextGlyph& first = glyphs[decoration.m_GlyphStart];

    for (uint32_t i = 1; i < decoration.m_GlyphCount; ++i)
    {
        const TextGlyph& glyph = glyphs[decoration.m_GlyphStart + i];

        if (glyph.m_StyleIndex != first.m_StyleIndex || glyph.m_MarkupSpanIndex != first.m_MarkupSpanIndex)
        {
            return true;
        }
    }

    if (first.m_MarkupSpanIndex == UINT16_MAX || first.m_MarkupSpanIndex >= layout->m_ResolvedSpans.Size())
    {
        return false;
    }

    const TextResolvedSpan& span = layout->m_ResolvedSpans[first.m_MarkupSpanIndex];

    for (uint32_t i = 0; i < span.m_EffectCount; ++i)
    {
        const TextEffect& effect = layout->m_Effects[layout->m_SpanEffects[span.m_EffectIndex + i]];

        if (effect.m_Type == TEXT_EFFECT_GRADIENT && effect.m_Gradient.m_Fit == TEXT_EFFECT_FIT_GLYPH)
        {
            return true;
        }
    }

    return false;
}

static uint32_t GetDecorationIndex(HTextLayout layout, const TextDecoration& decoration)
{
    const TextDecoration* decorations = layout->m_Decorations.Begin();
    assert(&decoration >= decorations && &decoration < decorations + layout->m_Decorations.Size());

    return (uint32_t)(&decoration - decorations);
}

void TextLayoutInitializeDecorationGeometry(HTextLayout layout)
{
    const uint32_t decoration_count = layout->m_Decorations.Size();
    uint32_t       segment_count = 0;

    layout->m_DecorationGeometryOffsets.SetSize(0);
    layout->m_DecorationGeometryOffsets.SetCapacity(decoration_count);
    layout->m_DecorationGeometryOffsets.SetSize(decoration_count);

    for (uint32_t i = 0; i < decoration_count; ++i)
    {
        const TextDecoration& decoration = layout->m_Decorations[i];

        if (DecorationRequiresGlyphSegments(layout, decoration))
        {
            layout->m_DecorationGeometryOffsets[i] = segment_count;
            segment_count += decoration.m_GlyphCount;
        }
        else
        {
            layout->m_DecorationGeometryOffsets[i] = UINT32_MAX;
        }
    }

    layout->m_DecorationGeometry.SetSize(0);
    layout->m_DecorationGeometry.SetCapacity(segment_count);

    for (uint32_t decoration_index = 0; decoration_index < decoration_count; ++decoration_index)
    {
        const TextDecoration& decoration = layout->m_Decorations[decoration_index];
        const uint32_t        geometry_start = layout->m_DecorationGeometryOffsets[decoration_index];

        if (geometry_start == UINT32_MAX)
        {
            continue;
        }

        const float decoration_end = decoration.m_X + decoration.m_Length;
        assert(geometry_start == layout->m_DecorationGeometry.Size());

        for (uint32_t glyph_offset = 0; glyph_offset < decoration.m_GlyphCount; ++glyph_offset)
        {
            const uint32_t   glyph_index = decoration.m_GlyphStart + glyph_offset;
            const TextGlyph& glyph = layout->m_Glyphs[glyph_index];
            const float      glyph_end = glyph.m_X + glyph.m_Advance;
            const float      glyph_x0 = fminf(glyph.m_X, glyph_end);
            const float      glyph_x1 = fmaxf(glyph.m_X, glyph_end);
            TextDecorationGeometry geometry;
            geometry.m_GlyphIndex = glyph_index;
            geometry.m_X = fmaxf(decoration.m_X, fminf(decoration_end, glyph_x0));
            geometry.m_Length = fmaxf(geometry.m_X, fminf(decoration_end, glyph_x1)) - geometry.m_X;
            layout->m_DecorationGeometry.Push(geometry);
        }

        if (decoration.m_GlyphCount == 0)
        {
            continue;
        }

        TextDecorationGeometry* geometry = layout->m_DecorationGeometry.Begin() + geometry_start;
        qsort(geometry, decoration.m_GlyphCount, sizeof(TextDecorationGeometry), CompareDecorationGeometry);
        uint32_t first_nonempty = 0;

        while (first_nonempty < decoration.m_GlyphCount && geometry[first_nonempty].m_Length == 0.0f)
        {
            ++first_nonempty;
        }

        if (first_nonempty == decoration.m_GlyphCount)
        {
            geometry[0].m_X = decoration.m_X;
            geometry[0].m_Length = decoration.m_Length;

            continue;
        }

        float partition_x = decoration.m_X;

        for (uint32_t i = first_nonempty; i < decoration.m_GlyphCount; ++i)
        {
            if (geometry[i].m_Length == 0.0f)
            {
                continue;
            }

            uint32_t next_nonempty = i + 1;

            while (next_nonempty < decoration.m_GlyphCount && geometry[next_nonempty].m_Length == 0.0f)
            {
                ++next_nonempty;
            }

            float next_partition_x = decoration_end;

            if (next_nonempty < decoration.m_GlyphCount)
            {
                const float current_end = geometry[i].m_X + geometry[i].m_Length;
                const float next_start = geometry[next_nonempty].m_X;
                next_partition_x = (current_end + next_start) * 0.5f;
                next_partition_x = fmaxf(partition_x, fminf(decoration_end, next_partition_x));
            }

            geometry[i].m_X = partition_x;
            geometry[i].m_Length = next_partition_x - partition_x;
            partition_x = next_partition_x;
        }
    }
}

bool TextLayoutDecorationRequiresGlyphSegments(HTextLayout layout, const TextDecoration& decoration)
{
    const uint32_t decoration_index = GetDecorationIndex(layout, decoration);
    assert(decoration_index < layout->m_DecorationGeometryOffsets.Size());

    return layout->m_DecorationGeometryOffsets[decoration_index] != UINT32_MAX;
}

const TextDecorationGeometry* TextLayoutGetDecorationGeometry(HTextLayout layout, const TextDecoration& decoration, uint32_t segment_index)
{
    const uint32_t decoration_index = GetDecorationIndex(layout, decoration);
    assert(decoration_index < layout->m_DecorationGeometryOffsets.Size());
    assert(segment_index < decoration.m_GlyphCount);
    const uint32_t geometry_offset = layout->m_DecorationGeometryOffsets[decoration_index];
    assert(geometry_offset != UINT32_MAX);
    const uint32_t geometry_index = geometry_offset + segment_index;
    assert(geometry_index < layout->m_DecorationGeometry.Size());

    return &layout->m_DecorationGeometry[geometry_index];
}

uint32_t TextLayoutGetObjectCount(HTextLayout layout)
{
    return layout->m_Objects.Size();
}

const TextLayoutObject* TextLayoutGetObjects(HTextLayout layout)
{
    return layout->m_Objects.Begin();
}

uint8_t TextLayoutGetObjectPosition(HTextLayout layout, const TextLayoutObject* object, float paragraph_x, float paragraph_top, float paragraph_width, float* x, float* y)
{
    if (!layout || !object || !x || !y)
    {
        return 0;
    }

    const uint32_t glyph_count = layout->m_Glyphs.Size();

    for (uint32_t line_index = 0; line_index < layout->m_Lines.Size(); ++line_index)
    {
        const TextLine& line = layout->m_Lines[line_index];
        const TextParagraph& paragraph = layout->m_Paragraphs[line.m_ParagraphIndex];
        const uint32_t paragraph_end = paragraph.m_TextIndex + paragraph.m_TextLength;

        if (object->m_TextOffset < paragraph.m_TextIndex || object->m_TextOffset > paragraph_end)
        {
            continue;
        }

        const bool right_to_left = paragraph.m_Direction == TEXT_DIRECTION_RTL;
        const float line_x = paragraph_x + (right_to_left ? paragraph_width - line.m_Width : 0.0f);

        if (line.m_Length == 0)
        {
            *x = line_x;
            *y = paragraph_top - layout->m_Height + line.m_Baseline - object->m_Height * 0.2f;

            return 1;
        }

        if (line.m_Index + line.m_Length > glyph_count)
        {
            continue;
        }

        uint32_t first_cluster = layout->m_Glyphs[line.m_Index].m_Cluster;
        uint32_t last_cluster = first_cluster;
        float first_x = layout->m_Glyphs[line.m_Index].m_X;

        for (uint32_t i = line.m_Index + 1; i < line.m_Index + line.m_Length; ++i)
        {
            const TextGlyph& glyph = layout->m_Glyphs[i];
            first_cluster = first_cluster < glyph.m_Cluster ? first_cluster : glyph.m_Cluster;
            last_cluster = last_cluster > glyph.m_Cluster ? last_cluster : glyph.m_Cluster;
            first_x = fminf(first_x, glyph.m_X);
        }

        if (object->m_TextOffset > last_cluster && line_index + 1 < paragraph.m_LineIndex + paragraph.m_LineCount)
        {
            continue;
        }

        float object_x = line_x + line.m_Width;
        bool found_glyph = false;
        uint32_t nearest_cluster = UINT32_MAX;

        for (uint32_t i = line.m_Index; i < line.m_Index + line.m_Length; ++i)
        {
            const TextGlyph& glyph = layout->m_Glyphs[i];

            if (glyph.m_Cluster >= object->m_TextOffset && glyph.m_Cluster < nearest_cluster)
            {
                object_x = line_x + glyph.m_X - first_x;
                nearest_cluster = glyph.m_Cluster;
                found_glyph = true;
            }
        }

        if (!found_glyph && object->m_TextOffset <= first_cluster)
        {
            object_x = line_x;
        }

        *x = object_x;
        *y = paragraph_top - layout->m_Height + line.m_Baseline - object->m_Height * 0.2f;

        return 1;
    }

    return 0;
}

const TextLayoutObjectAttribute* TextLayoutGetObjectAttributes(HTextLayout layout)
{
    return layout->m_ObjectAttributes.Begin();
}

const char* TextLayoutGetObjectSource(HTextLayout layout)
{
    return layout->m_ObjectSource.Empty() ? "" : layout->m_ObjectSource.Begin();
}

static bool ObjectAttributeEquals(HTextLayout layout, const TextLayoutObjectAttribute& attribute, const char* name)
{
    const uint32_t name_length = (uint32_t)strlen(name);

    return attribute.m_NameLength == name_length &&
           memcmp(layout->m_ObjectSource.Begin() + attribute.m_NameOffset, name, name_length) == 0;
}

static dmhash_t GetObjectDefaultStyle(HTextLayout layout, const TextLayoutObject& object)
{
    for (uint32_t i = 0; i < object.m_AttributeCount; ++i)
    {
        const TextLayoutObjectAttribute& attribute = layout->m_ObjectAttributes[object.m_AttributeIndex + i];

        if (ObjectAttributeEquals(layout, attribute, "style") && attribute.m_ValueLength)
        {
            return dmHashBuffer64(layout->m_ObjectSource.Begin() + attribute.m_ValueOffset, attribute.m_ValueLength);
        }
    }

    return object.m_Tag;
}

static void OverlayStyle(TextRenderStyle* target, const TextRenderStyle& overlay)
{
    if (overlay.m_Flags & TEXT_RENDER_STYLE_FACE_COLOR)
    {
        memcpy(target->m_FaceColor, overlay.m_FaceColor, sizeof(target->m_FaceColor));
    }

    if (overlay.m_Flags & TEXT_RENDER_STYLE_FONT_SIZE)
    {
        target->m_FontSize = overlay.m_FontSize;
    }

    if (overlay.m_Flags & TEXT_RENDER_STYLE_OUTLINE_COLOR)
    {
        memcpy(target->m_OutlineColor, overlay.m_OutlineColor, sizeof(target->m_OutlineColor));
    }

    if (overlay.m_Flags & TEXT_RENDER_STYLE_OUTLINE_WIDTH)
    {
        target->m_OutlineWidth = overlay.m_OutlineWidth;
    }

    if (overlay.m_Flags & TEXT_RENDER_STYLE_SHADOW_COLOR)
    {
        memcpy(target->m_ShadowColor, overlay.m_ShadowColor, sizeof(target->m_ShadowColor));
    }

    if (overlay.m_Flags & TEXT_RENDER_STYLE_SHADOW_X)
    {
        target->m_ShadowX = overlay.m_ShadowX;
    }

    if (overlay.m_Flags & TEXT_RENDER_STYLE_SHADOW_Y)
    {
        target->m_ShadowY = overlay.m_ShadowY;
    }

    if (overlay.m_Flags & TEXT_RENDER_STYLE_SHADOW_BLUR)
    {
        target->m_ShadowBlur = overlay.m_ShadowBlur;
    }

    target->m_Flags |= overlay.m_Flags;
}

static uint16_t AddLayoutStyle(HTextLayout layout, const TextRenderStyle& style)
{
    for (uint32_t i = 0; i < layout->m_Styles.Size(); ++i)
    {
        if (memcmp(&layout->m_Styles[i], &style, sizeof(style)) == 0)
        {
            return (uint16_t)i;
        }
    }

    if (layout->m_Styles.Size() == 0xffff)
    {
        return 0xffff;
    }

    if (layout->m_Styles.Full())
    {
        layout->m_Styles.OffsetCapacity(1);
    }

    layout->m_Styles.Push(style);

    return (uint16_t)(layout->m_Styles.Size() - 1);
}

static void ApplyNamedStyle(HTextLayout layout, dmhash_t name, TextRenderStyle* style)
{
    const TextRenderStyle* named_style = FontCollectionGetNamedStyle(layout->m_FontCollection, name);

    if (named_style)
    {
        OverlayStyle(style, *named_style);
    }
}

static void AppendNamedStyleEffects(HTextLayout layout, dmhash_t name, const TextLayoutObject& object)
{
    uint32_t          effect_count = 0;
    const TextEffect* effects = FontCollectionGetNamedStyleEffects(layout->m_FontCollection, name, &effect_count);

    for (uint32_t i = 0; i < effect_count; ++i)
    {
        if (layout->m_Effects.Size() == MARKUP_INVALID_INDEX)
        {
            return;
        }

        TextEffect effect = effects[i];
        effect.m_TextOffset = object.m_TextOffset;
        effect.m_TextLength = object.m_TextLength;

        if (layout->m_Effects.Full())
        {
            layout->m_Effects.OffsetCapacity(1);
        }

        layout->m_Effects.Push(effect);
    }
}

struct TextObjectStyle
{
    TextRenderStyle m_Style;
    uint16_t        m_EffectIndex;
    uint16_t        m_EffectCount;
};

// Resolves the default object style followed by its caller-selected override.
static TextObjectStyle ResolveObjectStyle(HTextLayout layout, const TextLayoutObject& object, dmhash_t style_override)
{
    TextObjectStyle result = {};
    result.m_EffectIndex = (uint16_t)layout->m_Effects.Size();

    const dmhash_t base_style = GetObjectDefaultStyle(layout, object);
    AppendNamedStyleEffects(layout, base_style, object);
    ApplyNamedStyle(layout, base_style, &result.m_Style);

    if (style_override && style_override != base_style)
    {
        AppendNamedStyleEffects(layout, style_override, object);
        ApplyNamedStyle(layout, style_override, &result.m_Style);
    }

    result.m_EffectCount = (uint16_t)(layout->m_Effects.Size() - result.m_EffectIndex);

    return result;
}

static bool RefreshObjectStyles(HTextLayout layout, bool restore_base)
{
    const uint32_t revision = FontCollectionGetNamedStyleRevision(layout->m_FontCollection);

    if (layout->m_NamedStyleRevision == revision)
    {
        return false;
    }

    if (restore_base)
    {
        layout->m_Styles.SetSize(layout->m_BaseStyleCount);
        layout->m_Effects.SetSize(layout->m_BaseEffectCount);
        layout->m_SpanEffects.SetSize(layout->m_BaseSpanEffectCount);
        layout->m_ResolvedSpans.SetSize(layout->m_BaseResolvedSpanCount);

        for (uint32_t i = 0; i < layout->m_Glyphs.Size(); ++i)
        {
            layout->m_Glyphs[i].m_StyleIndex = layout->m_Glyphs[i].m_BaseStyleIndex;
            layout->m_Glyphs[i].m_MarkupSpanIndex = layout->m_Glyphs[i].m_BaseMarkupSpanIndex;
        }
    }

    dmArray<TextObjectStyle> object_styles;
    uint32_t text_length = 0;
    bool     has_object_styles = false;

    for (uint32_t object_index = 0; object_index < layout->m_Objects.Size(); ++object_index)
    {
        const TextLayoutObject& object = layout->m_Objects[object_index];

        if (object.m_TextLength == 0)
        {
            continue;
        }

        const dmhash_t style_override = layout->m_ObjectStyleOverrides[object_index];
        const TextObjectStyle object_style = ResolveObjectStyle(layout, object, style_override);

        if (object_style.m_Style.m_Flags != 0 || object_style.m_EffectCount != 0)
        {
            if (object_styles.Empty())
            {
                object_styles.SetCapacity(layout->m_Objects.Size());
                object_styles.SetSize(layout->m_Objects.Size());
                memset(object_styles.Begin(), 0, object_styles.Size() * sizeof(TextObjectStyle));
            }

            object_styles[object_index] = object_style;
            const uint32_t object_end = object.m_TextOffset + object.m_TextLength;
            text_length = text_length > object_end ? text_length : object_end;
            has_object_styles = true;
        }
    }

    if (!has_object_styles)
    {
        layout->m_NamedStyleRevision = revision;

        return true;
    }

    dmArray<uint16_t> text_objects;
    text_objects.SetCapacity(text_length);
    text_objects.SetSize(text_length);

    if (!text_objects.Empty())
    {
        memset(text_objects.Begin(), 0xff, text_objects.Size() * sizeof(uint16_t));
    }

    for (uint32_t object_index = 0; object_index < layout->m_Objects.Size(); ++object_index)
    {
        const TextLayoutObject& object = layout->m_Objects[object_index];
        const TextObjectStyle&  object_style = object_styles[object_index];

        if (object.m_TextLength == 0 || (object_style.m_Style.m_Flags == 0 && object_style.m_EffectCount == 0))
        {
            continue;
        }

        const uint32_t object_end = object.m_TextOffset + object.m_TextLength;

        for (uint32_t text_offset = object.m_TextOffset; text_offset < object_end; ++text_offset)
        {
            text_objects[text_offset] = (uint16_t)object_index;
        }
    }

    for (uint32_t glyph_index = 0; glyph_index < layout->m_Glyphs.Size(); ++glyph_index)
    {
        TextGlyph& glyph = layout->m_Glyphs[glyph_index];

        if (glyph.m_Cluster >= text_objects.Size())
        {
            continue;
        }

        const uint16_t object_index = text_objects[glyph.m_Cluster];

        if (object_index == MARKUP_INVALID_INDEX || glyph.m_StyleIndex >= layout->m_Styles.Size())
        {
            continue;
        }

        const TextObjectStyle& object_style = object_styles[object_index];
        TextRenderStyle        style = layout->m_Styles[glyph.m_StyleIndex];
        OverlayStyle(&style, object_style.m_Style);
        const uint16_t style_index = AddLayoutStyle(layout, style);

        if (style_index != MARKUP_INVALID_INDEX)
        {
            glyph.m_StyleIndex = style_index;
        }

        if (object_style.m_EffectCount != 0 && layout->m_ResolvedSpans.Size() != MARKUP_INVALID_INDEX)
        {
            TextResolvedSpan span = {};
            span.m_EffectIndex = (uint16_t)layout->m_SpanEffects.Size();

            if (glyph.m_MarkupSpanIndex < layout->m_ResolvedSpans.Size())
            {
                const TextResolvedSpan& base_span = layout->m_ResolvedSpans[glyph.m_MarkupSpanIndex];
                span = base_span;
                span.m_EffectIndex = (uint16_t)layout->m_SpanEffects.Size();

                for (uint32_t i = 0; i < base_span.m_EffectCount; ++i)
                {
                    if (layout->m_SpanEffects.Full())
                    {
                        layout->m_SpanEffects.OffsetCapacity(1);
                    }

                    layout->m_SpanEffects.Push(layout->m_SpanEffects[base_span.m_EffectIndex + i]);
                }
            }

            for (uint32_t i = 0; i < object_style.m_EffectCount; ++i)
            {
                if (layout->m_SpanEffects.Full())
                {
                    layout->m_SpanEffects.OffsetCapacity(1);
                }

                layout->m_SpanEffects.Push(object_style.m_EffectIndex + i);
            }

            span.m_EffectCount += object_style.m_EffectCount;

            if (layout->m_ResolvedSpans.Full())
            {
                layout->m_ResolvedSpans.OffsetCapacity(1);
            }

            layout->m_ResolvedSpans.Push(span);
            glyph.m_MarkupSpanIndex = (uint16_t)(layout->m_ResolvedSpans.Size() - 1);
        }
    }

    layout->m_NamedStyleRevision = revision;

    return true;
}

bool TextLayoutRefreshObjectStyles(HTextLayout layout)
{
    const bool changed = RefreshObjectStyles(layout, true);

    if (changed)
    {
        TextLayoutInitializeDecorationGeometry(layout);
    }

    return changed;
}

void TextLayoutInitializeObjectStyles(HTextLayout layout)
{
    layout->m_BaseStyleCount = (uint16_t)layout->m_Styles.Size();
    layout->m_BaseEffectCount = (uint16_t)layout->m_Effects.Size();
    layout->m_BaseSpanEffectCount = (uint16_t)layout->m_SpanEffects.Size();
    layout->m_BaseResolvedSpanCount = (uint16_t)layout->m_ResolvedSpans.Size();
    layout->m_ObjectStyleOverrides.SetCapacity(layout->m_Objects.Size());
    layout->m_ObjectStyleOverrides.SetSize(layout->m_Objects.Size());

    if (!layout->m_ObjectStyleOverrides.Empty())
    {
        memset(layout->m_ObjectStyleOverrides.Begin(), 0, layout->m_ObjectStyleOverrides.Size() * sizeof(dmhash_t));
    }

    if (layout->m_Objects.Empty())
    {
        layout->m_NamedStyleRevision = FontCollectionGetNamedStyleRevision(layout->m_FontCollection);

        return;
    }

    for (uint32_t i = 0; i < layout->m_Glyphs.Size(); ++i)
    {
        layout->m_Glyphs[i].m_BaseStyleIndex = layout->m_Glyphs[i].m_StyleIndex;
        layout->m_Glyphs[i].m_BaseMarkupSpanIndex = layout->m_Glyphs[i].m_MarkupSpanIndex;
    }

    layout->m_NamedStyleRevision = 0xffffffff;
    RefreshObjectStyles(layout, false);
}

uint8_t TextLayoutSetObjectStyle(HTextLayout layout, uint64_t object_id, dmhash_t style)
{
    if (!layout || !object_id)
    {
        return 0;
    }

    bool changed = false;

    for (uint32_t i = 0; i < layout->m_Objects.Size(); ++i)
    {
        const TextLayoutObject& object = layout->m_Objects[i];

        if (object.m_Id != object_id)
        {
            continue;
        }

        if (layout->m_ObjectStyleOverrides[i] != style)
        {
            layout->m_ObjectStyleOverrides[i] = style;
            changed = true;
        }
    }

    if (changed)
    {
        layout->m_NamedStyleRevision = 0xffffffff;
        TextLayoutRefreshObjectStyles(layout);
    }

    return changed;
}

void TextLayoutAdoptResolvedMarkup(HTextLayout layout, ResolvedMarkup* resolved, TextLayoutSettings* settings)
{
    layout->m_Styles.Swap(resolved->m_Styles);
    layout->m_Effects.Swap(resolved->m_Effects);
    layout->m_SpanEffects.Swap(resolved->m_SpanEffects);
    layout->m_ResolvedSpans.Swap(resolved->m_Spans);
    layout->m_ObjectSource.Swap(resolved->m_ObjectSource);
    layout->m_Objects.Swap(resolved->m_Objects);
    layout->m_ObjectAttributes.Swap(resolved->m_ObjectAttributes);
    layout->m_ReleaseObject = settings->m_ReleaseObject;
    layout->m_ObjectContext = settings->m_ObjectContext;
}

void TextLayoutReleaseObjects(HTextLayout layout)
{
    if (layout->m_ReleaseObject)
    {
        for (uint32_t i = 0; i < layout->m_Objects.Size(); ++i)
        {
            const TextLayoutObject& object = layout->m_Objects[i];

            if (object.m_Tag == TAG_SPRITE)
            {
                layout->m_ReleaseObject(layout->m_ObjectContext, &object);
            }
        }
    }
}

void TextLayoutGetBounds(HTextLayout layout, float* width, float* height)
{
    *width = layout->m_Width;
    *height = layout->m_Height;
}

static float TextLayoutOffsetX(uint32_t align, float width)
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

static float TextLayoutOffsetY(uint32_t align, float height, float layout_height)
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

static uint32_t TextLayoutResolveAlign(uint32_t align, TextDirection direction)
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

// Layout objects are source-ordered nested intervals. Resolve the highest-
// priority matching object once per visible text offset, then test each glyph
// only once even when a layout contains thousands of links.
struct TextLayoutObjectSweepEntry
{
    uint32_t m_TextEnd;
    uint16_t m_HitObject;
};

static const uint32_t HIT_TEST_FEW_OBJECT_LIMIT = 2;

static bool TextLayoutBuildHitTestObjects(HTextLayout layout, dmhash_t tag, uint32_t text_length, dmArray<uint16_t>* text_objects)
{
    const TextLayoutObject* objects = layout->m_Objects.Begin();
    const uint32_t          object_count = layout->m_Objects.Size();

    text_objects->SetCapacity(text_length);
    text_objects->SetSize(text_length);

    dmArray<TextLayoutObjectSweepEntry> active_objects;
    active_objects.SetCapacity(object_count);
    uint32_t object_index = 0;

    for (uint32_t text_offset = 0; text_offset < text_length; ++text_offset)
    {
        while (!active_objects.Empty() && active_objects.Back().m_TextEnd <= text_offset)
        {
            active_objects.Pop();
        }

        while (object_index < object_count && objects[object_index].m_TextOffset <= text_offset)
        {
            const TextLayoutObject& object = objects[object_index];
            const uint16_t inherited_object = active_objects.Empty() ? MARKUP_INVALID_INDEX : active_objects.Back().m_HitObject;

            if (object.m_TextLength != 0)
            {
                TextLayoutObjectSweepEntry entry;
                entry.m_TextEnd = object.m_TextOffset + object.m_TextLength;
                entry.m_HitObject = tag == 0 || object.m_Tag == tag ? (uint16_t)object_index : inherited_object;
                active_objects.Push(entry);
            }

            ++object_index;
        }

        (*text_objects)[text_offset] = active_objects.Empty() ? MARKUP_INVALID_INDEX : active_objects.Back().m_HitObject;
    }

    return true;
}

static uint32_t TextLayoutHitTestFewObjects(HTextLayout layout, const TextLayoutHitTestParams& params,
                                            const uint16_t* object_indices, uint32_t object_count)
{
    TextGlyph*              glyphs = layout->m_Glyphs.Begin();
    TextLine*               lines = layout->m_Lines.Begin();
    TextParagraph*          paragraphs = layout->m_Paragraphs.Begin();
    const TextLayoutObject* objects = layout->m_Objects.Begin();
    const float             layout_y = TextLayoutOffsetY(params.m_VAlign, params.m_Height, layout->m_Height);

    // Preserve the original object-first traversal for the common case. It
    // avoids allocating the sweep map and returns as soon as the topmost
    // matching object is hit.
    for (uint32_t object = object_count; object-- > 0;)
    {
        const uint32_t          object_index = object_indices[object];
        const TextLayoutObject& layout_object = objects[object_index];
        const uint32_t          object_end = layout_object.m_TextOffset + layout_object.m_TextLength;

        for (uint32_t line_index = 0; line_index < layout->m_Lines.Size(); ++line_index)
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
                first_x = fminf(first_x, glyphs[i].m_X);
            }

            const uint32_t align = TextLayoutResolveAlign(params.m_Align, paragraphs[line.m_ParagraphIndex].m_Direction);
            const float    line_x = TextLayoutOffsetX(align, params.m_Width) - TextLayoutOffsetX(align, line.m_Width) - params.m_MonospacePadding * 0.5f;
            const float    line_y = layout_y + line.m_Baseline;

            for (uint32_t i = line.m_Index; i < line.m_Index + line.m_Length; ++i)
            {
                const TextGlyph& glyph = glyphs[i];

                if (glyph.m_Cluster < layout_object.m_TextOffset || glyph.m_Cluster >= object_end)
                {
                    continue;
                }

                const float x = line_x + glyph.m_X - first_x;

                if (glyph.m_Flags & TEXT_GLYPH_FLAG_OBJECT)
                {
                    const float y = line_y - glyph.m_Height * 0.2f;

                    if (params.m_X >= x && params.m_X <= x + glyph.m_Width &&
                        params.m_Y >= y && params.m_Y <= y + glyph.m_Height)
                    {
                        return object_index;
                    }

                    continue;
                }

                const float         glyph_size = params.m_FontSize * glyph.m_RenderScale;
                const float         scale = FontGetScaleFromSize(glyph.m_Font, glyph_size);
                const float         ascent = FontGetAscent(glyph.m_Font, scale);
                const float         descent = fabsf(FontGetDescent(glyph.m_Font, scale));
                const float         white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                TextGlyphRenderData render_data;
                TextLayoutGetGlyphRenderData(layout, glyph, white, &render_data);
                const float glyph_x = x + render_data.m_OffsetX;
                const float y = line_y + glyph.m_Y - first_y + render_data.m_OffsetY;
                const float width = fmaxf(glyph.m_Width, glyph_size * 0.35f);

                if (params.m_X >= glyph_x && params.m_X <= glyph_x + width &&
                    params.m_Y >= y - descent && params.m_Y <= y + ascent)
                {
                    return object_index;
                }
            }
        }
    }

    return UINT32_MAX;
}

uint32_t TextLayoutHitTestObject(HTextLayout layout, const TextLayoutHitTestParams& params)
{
    if (!layout)
    {
        return UINT32_MAX;
    }

    TextLayoutRefreshObjectStyles(layout);

    const TextLayoutObject* objects = layout->m_Objects.Begin();
    const uint32_t          object_count = layout->m_Objects.Size();
    uint16_t                few_objects[HIT_TEST_FEW_OBJECT_LIMIT];
    uint32_t                matching_object_count = 0;
    uint32_t                text_length = 0;

    for (uint32_t object_index = 0; object_index < object_count; ++object_index)
    {
        const TextLayoutObject& object = objects[object_index];

        if (object.m_TextLength == 0)
        {
            continue;
        }

        const uint32_t object_end = object.m_TextOffset + object.m_TextLength;
        text_length = text_length > object_end ? text_length : object_end;

        if (params.m_Tag == 0 || object.m_Tag == params.m_Tag)
        {
            if (matching_object_count < HIT_TEST_FEW_OBJECT_LIMIT)
            {
                few_objects[matching_object_count] = (uint16_t)object_index;
            }

            ++matching_object_count;
        }
    }

    if (matching_object_count == 0 || text_length == 0)
    {
        return UINT32_MAX;
    }

    if (matching_object_count <= HIT_TEST_FEW_OBJECT_LIMIT)
    {
        return TextLayoutHitTestFewObjects(layout, params, few_objects, matching_object_count);
    }

    dmArray<uint16_t> text_objects;

    if (!TextLayoutBuildHitTestObjects(layout, params.m_Tag, text_length, &text_objects))
    {
        return UINT32_MAX;
    }

    TextGlyph*     glyphs = layout->m_Glyphs.Begin();
    TextLine*      lines = layout->m_Lines.Begin();
    TextParagraph* paragraphs = layout->m_Paragraphs.Begin();
    const float    layout_y = TextLayoutOffsetY(params.m_VAlign, params.m_Height, layout->m_Height);
    uint32_t       hit_object = UINT32_MAX;

    for (uint32_t line_index = 0; line_index < layout->m_Lines.Size(); ++line_index)
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
            first_x = fminf(first_x, glyphs[i].m_X);
        }

        const uint32_t align = TextLayoutResolveAlign(params.m_Align, paragraphs[line.m_ParagraphIndex].m_Direction);
        const float    line_x = TextLayoutOffsetX(align, params.m_Width) - TextLayoutOffsetX(align, line.m_Width) - params.m_MonospacePadding * 0.5f;
        const float    line_y = layout_y + line.m_Baseline;

        for (uint32_t i = line.m_Index; i < line.m_Index + line.m_Length; ++i)
        {
            const TextGlyph& glyph = glyphs[i];

            const uint16_t object_index = glyph.m_Cluster < text_objects.Size()
                                            ? text_objects[glyph.m_Cluster]
                                            : MARKUP_INVALID_INDEX;

            if (object_index == MARKUP_INVALID_INDEX || (hit_object != UINT32_MAX && object_index <= hit_object))
            {
                continue;
            }

            const float x = line_x + glyph.m_X - first_x;

            if (glyph.m_Flags & TEXT_GLYPH_FLAG_OBJECT)
            {
                const float y = line_y - glyph.m_Height * 0.2f;

                if (params.m_X >= x && params.m_X <= x + glyph.m_Width &&
                    params.m_Y >= y && params.m_Y <= y + glyph.m_Height)
                {
                    hit_object = object_index;
                }

                continue;
            }

            const float         glyph_size = params.m_FontSize * glyph.m_RenderScale;
            const float         scale = FontGetScaleFromSize(glyph.m_Font, glyph_size);
            const float         ascent = FontGetAscent(glyph.m_Font, scale);
            const float         descent = fabsf(FontGetDescent(glyph.m_Font, scale));
            const float         white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            TextGlyphRenderData render_data;
            TextLayoutGetGlyphRenderData(layout, glyph, white, &render_data);
            const float glyph_x = x + render_data.m_OffsetX;
            const float y = line_y + glyph.m_Y - first_y + render_data.m_OffsetY;
            const float width = fmaxf(glyph.m_Width, glyph_size * 0.35f);

            if (params.m_X >= glyph_x && params.m_X <= glyph_x + width &&
                params.m_Y >= y - descent && params.m_Y <= y + ascent)
            {
                hit_object = object_index;
            }
        }
    }

    return hit_object;
}

void TextLayoutAcquire(HTextLayout layout)
{
    assert(layout);
    assert(layout->m_RefCount > 0);
    ++layout->m_RefCount;
}

void TextLayoutRelease(HTextLayout layout)
{
    assert(layout);
    assert(layout->m_RefCount > 0);
    if (--layout->m_RefCount == 0)
    {
        layout->m_Destroy(layout);
    }
}

void TextLayoutUpdate(HTextLayout layout, float delta_time)
{
    assert(layout);

    if (delta_time > 0.0f && isfinite(delta_time))
    {
        layout->m_ElapsedTime += delta_time;
    }
}

void TextLayoutFinalizeLineBaselines(HTextLayout layout, TextLayoutSettings* settings)
{
    TextLine* lines = layout->m_Lines.Begin();
    TextGlyph* glyphs = layout->m_Glyphs.Begin();
    HFont default_font = FontCollectionGetFont(layout->m_FontCollection, 0);
    const float default_scale = FontGetScaleFromSize(default_font, settings->m_Size);
    const float default_ascent = FontGetAscent(default_font, default_scale);
    const float default_descent = fabsf(FontGetDescent(default_font, default_scale));
    float layout_height = 0.0f;
    float last_line_height = 0.0f;
    HFont measured_font = 0;
    float measured_render_scale = 0.0f;
    float measured_ascent = 0.0f;
    float measured_descent = 0.0f;

    for (uint32_t line_index = 0; line_index < layout->m_Lines.Size(); ++line_index)
    {
        TextLine& line = lines[line_index];
        float line_ascent = 0.0f;
        float line_descent = 0.0f;

        // The first glyph establishes the maxima so a negative ascent is preserved.
        for (uint32_t glyph_index = line.m_Index; glyph_index < line.m_Index + line.m_Length; ++glyph_index)
        {
            const TextGlyph& glyph = glyphs[glyph_index];

            if (glyph.m_Flags & TEXT_GLYPH_FLAG_OBJECT)
            {
                line_ascent = glyph_index == line.m_Index ? glyph.m_Height * 0.8f : fmaxf(line_ascent, glyph.m_Height * 0.8f);
                line_descent = glyph_index == line.m_Index ? glyph.m_Height * 0.2f : fmaxf(line_descent, glyph.m_Height * 0.2f);
                continue;
            }

            if (measured_font != glyph.m_Font || measured_render_scale != glyph.m_RenderScale)
            {
                const float font_scale = FontGetScaleFromSize(glyph.m_Font, settings->m_Size) * glyph.m_RenderScale;
                measured_font = glyph.m_Font;
                measured_render_scale = glyph.m_RenderScale;
                measured_ascent = FontGetAscent(glyph.m_Font, font_scale);
                measured_descent = fabsf(FontGetDescent(glyph.m_Font, font_scale));
            }

            line_ascent = glyph_index == line.m_Index ? measured_ascent : fmaxf(line_ascent, measured_ascent);
            line_descent = glyph_index == line.m_Index ? measured_descent : fmaxf(line_descent, measured_descent);
        }

        if (line_ascent == 0.0f && line_descent == 0.0f)
        {
            line_ascent = default_ascent;
            line_descent = default_descent;
        }

        last_line_height = line_ascent + line_descent;
        line.m_Baseline = layout_height + line_ascent;
        layout_height += last_line_height * settings->m_Leading;
    }

    if (!layout->m_Lines.Empty())
    {
        layout_height -= last_line_height * (settings->m_Leading - 1.0f);
    }

    for (uint32_t line_index = 0; line_index < layout->m_Lines.Size(); ++line_index)
    {
        lines[line_index].m_Baseline = layout_height - lines[line_index].m_Baseline;
    }

    layout->m_Height = layout_height;
}

uint32_t TextToCodePoints(const char* text, dmArray<uint32_t>& codepoints)
{
    const char* safe_text = text ? text : "";
    uint32_t len = dmUtf8::StrLen(safe_text);
    if (codepoints.Capacity() < len)
    {
        codepoints.SetCapacity(len);
    }
    codepoints.SetSize(0);

    const char* cursor = safe_text;
    while (uint32_t c = dmUtf8::NextChar(&cursor))
    {
        codepoints.Push(c);
    }
    return len;
}

TextResult TextLayoutCreate(HFontCollection collection,
                            uint32_t* codepoints, uint32_t num_codepoints,
                            TextLayoutSettings* settings, HTextLayout* outlayout)
{
#if defined(FONT_USE_SKRIBIDI)
    TextLayoutType layout_type = FontCollectionGetLayoutType(collection);
    if (layout_type == TEXT_LAYOUT_TYPE_FULL)
        return TextLayoutSkribidiCreate(collection, codepoints, num_codepoints, settings, outlayout);
#endif
    return TextLayoutLegacyCreate(collection, codepoints, num_codepoints, settings, outlayout);
}

TextResult TextLayoutCreateMarkup(HFontCollection collection, HMarkup markup,
                                  TextLayoutSettings* settings, HTextLayout* outlayout)
{
#if defined(FONT_USE_SKRIBIDI)
    if (FontCollectionGetLayoutType(collection) == TEXT_LAYOUT_TYPE_FULL)
    {
        return TextLayoutSkribidiCreateMarkup(collection, markup, settings, outlayout);
    }
#endif

    return TextLayoutLegacyCreateMarkup(collection, markup, settings, outlayout);
}
