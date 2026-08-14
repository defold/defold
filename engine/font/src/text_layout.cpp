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
#include <string.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/utf8.h>

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

uint32_t TextLayoutGetObjectCount(HTextLayout layout)
{
    return layout->m_Objects.Size();
}

const TextLayoutObject* TextLayoutGetObjects(HTextLayout layout)
{
    return layout->m_Objects.Begin();
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

static void GetObjectStyleName(HTextLayout layout, const TextLayoutObject& object, const char** name, uint32_t* name_length)
{
    for (uint32_t i = 0; i < object.m_AttributeCount; ++i)
    {
        const TextLayoutObjectAttribute& attribute = layout->m_ObjectAttributes[object.m_AttributeIndex + i];
        if (ObjectAttributeEquals(layout, attribute, "style") && attribute.m_ValueLength)
        {
            *name = layout->m_ObjectSource.Begin() + attribute.m_ValueOffset;
            *name_length = attribute.m_ValueLength;
            return;
        }
    }
    if (object.m_Type == TEXT_LAYOUT_OBJECT_LINK)
    {
        *name = "link";
        *name_length = 4;
    }
    else
    {
        *name = "sprite";
        *name_length = 6;
    }
}

static dmhash_t HashStyleSelector(const char* name, uint32_t name_length, const char* suffix)
{
    HashState64 state;
    dmHashInit64(&state, false);
    dmHashUpdateBuffer64(&state, name, name_length);
    dmHashUpdateBuffer64(&state, suffix, (uint32_t)strlen(suffix));
    return dmHashFinal64(&state);
}

static void OverlayStyle(TextRenderStyle* target, const TextRenderStyle& overlay)
{
    if (overlay.m_Flags & TEXT_RENDER_STYLE_FACE_COLOR)
        memcpy(target->m_FaceColor, overlay.m_FaceColor, sizeof(target->m_FaceColor));
    if (overlay.m_Flags & TEXT_RENDER_STYLE_FONT_SIZE)
        target->m_FontSize = overlay.m_FontSize;
    if (overlay.m_Flags & TEXT_RENDER_STYLE_OUTLINE_COLOR)
        memcpy(target->m_OutlineColor, overlay.m_OutlineColor, sizeof(target->m_OutlineColor));
    if (overlay.m_Flags & TEXT_RENDER_STYLE_OUTLINE_WIDTH)
        target->m_OutlineWidth = overlay.m_OutlineWidth;
    if (overlay.m_Flags & TEXT_RENDER_STYLE_SHADOW_COLOR)
        memcpy(target->m_ShadowColor, overlay.m_ShadowColor, sizeof(target->m_ShadowColor));
    if (overlay.m_Flags & TEXT_RENDER_STYLE_SHADOW_X)
        target->m_ShadowX = overlay.m_ShadowX;
    if (overlay.m_Flags & TEXT_RENDER_STYLE_SHADOW_Y)
        target->m_ShadowY = overlay.m_ShadowY;
    if (overlay.m_Flags & TEXT_RENDER_STYLE_SHADOW_BLUR)
        target->m_ShadowBlur = overlay.m_ShadowBlur;
    target->m_Flags |= overlay.m_Flags;
}

static uint16_t AddLayoutStyle(HTextLayout layout, const TextRenderStyle& style)
{
    for (uint32_t i = 0; i < layout->m_Styles.Size(); ++i)
    {
        if (memcmp(&layout->m_Styles[i], &style, sizeof(style)) == 0)
            return (uint16_t)i;
    }
    if (layout->m_Styles.Size() == 0xffff)
        return 0xffff;
    if (layout->m_Styles.Full())
        layout->m_Styles.OffsetCapacity(1);
    layout->m_Styles.Push(style);
    return (uint16_t)(layout->m_Styles.Size() - 1);
}

static void ApplyNamedStyle(HTextLayout layout, dmhash_t name, TextRenderStyle* style)
{
    const TextRenderStyle* named_style = FontCollectionGetNamedStyle(layout->m_FontCollection, name);
    if (named_style)
        OverlayStyle(style, *named_style);
}

static void AppendNamedStyleEffects(HTextLayout layout, dmhash_t name, const TextLayoutObject& object)
{
    uint32_t          effect_count = 0;
    const TextEffect* effects = FontCollectionGetNamedStyleEffects(layout->m_FontCollection, name, &effect_count);
    for (uint32_t i = 0; i < effect_count; ++i)
    {
        if (layout->m_Effects.Size() == MARKUP_INVALID_INDEX)
            return;
        TextEffect effect = effects[i];
        effect.m_TextOffset = object.m_TextOffset;
        effect.m_TextLength = object.m_TextLength;
        if (layout->m_Effects.Full())
            layout->m_Effects.OffsetCapacity(1);
        layout->m_Effects.Push(effect);
    }
}

struct TextObjectStyle
{
    TextRenderStyle m_Style;
    uint16_t        m_EffectIndex;
    uint16_t        m_EffectCount;
};

// Resolves the base object style followed by active pseudo-state overlays.
static TextObjectStyle ResolveObjectStyle(HTextLayout layout, const TextLayoutObject& object, const char* style_name, uint32_t style_name_length)
{
    TextObjectStyle result = {};
    result.m_EffectIndex = (uint16_t)layout->m_Effects.Size();

    const dmhash_t base_style = dmHashBuffer64(style_name, style_name_length);
    AppendNamedStyleEffects(layout, base_style, object);
    ApplyNamedStyle(layout, base_style, &result.m_Style);

    static const uint16_t STATE_BITS[] = {
        TEXT_LAYOUT_OBJECT_STATE_VISITED,
        TEXT_LAYOUT_OBJECT_STATE_HOVERED,
        TEXT_LAYOUT_OBJECT_STATE_ACTIVE,
        TEXT_LAYOUT_OBJECT_STATE_DISABLED,
    };
    static const char* const STATE_SUFFIXES[] = {
        ":visited",
        ":hover",
        ":active",
        ":disabled",
    };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(STATE_BITS); ++i)
    {
        if ((object.m_State & STATE_BITS[i]) == 0)
            continue;
        const dmhash_t selector = HashStyleSelector(style_name, style_name_length, STATE_SUFFIXES[i]);
        AppendNamedStyleEffects(layout, selector, object);
        ApplyNamedStyle(layout, selector, &result.m_Style);
    }

    result.m_EffectCount = (uint16_t)(layout->m_Effects.Size() - result.m_EffectIndex);
    return result;
}

static bool RefreshObjectStyles(HTextLayout layout, bool restore_base)
{
    const uint32_t revision = FontCollectionGetNamedStyleRevision(layout->m_FontCollection);
    if (layout->m_NamedStyleRevision == revision)
        return false;

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
            continue;
        const char* style_name;
        uint32_t    style_name_length;
        GetObjectStyleName(layout, object, &style_name, &style_name_length);
        const TextObjectStyle object_style = ResolveObjectStyle(layout, object, style_name, style_name_length);
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
        memset(text_objects.Begin(), 0xff, text_objects.Size() * sizeof(uint16_t));
    for (uint32_t object_index = 0; object_index < layout->m_Objects.Size(); ++object_index)
    {
        const TextLayoutObject& object = layout->m_Objects[object_index];
        const TextObjectStyle&  object_style = object_styles[object_index];
        if (object.m_TextLength == 0 || (object_style.m_Style.m_Flags == 0 && object_style.m_EffectCount == 0))
            continue;
        const uint32_t object_end = object.m_TextOffset + object.m_TextLength;
        for (uint32_t text_offset = object.m_TextOffset; text_offset < object_end; ++text_offset)
            text_objects[text_offset] = (uint16_t)object_index;
    }

    for (uint32_t glyph_index = 0; glyph_index < layout->m_Glyphs.Size(); ++glyph_index)
    {
        TextGlyph& glyph = layout->m_Glyphs[glyph_index];
        if (glyph.m_Cluster >= text_objects.Size())
            continue;
        const uint16_t object_index = text_objects[glyph.m_Cluster];
        if (object_index == MARKUP_INVALID_INDEX || glyph.m_StyleIndex >= layout->m_Styles.Size())
            continue;
        const TextObjectStyle& object_style = object_styles[object_index];
        TextRenderStyle        style = layout->m_Styles[glyph.m_StyleIndex];
        OverlayStyle(&style, object_style.m_Style);
        const uint16_t style_index = AddLayoutStyle(layout, style);
        if (style_index != MARKUP_INVALID_INDEX)
            glyph.m_StyleIndex = style_index;
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
                        layout->m_SpanEffects.OffsetCapacity(1);
                    layout->m_SpanEffects.Push(layout->m_SpanEffects[base_span.m_EffectIndex + i]);
                }
            }
            for (uint32_t i = 0; i < object_style.m_EffectCount; ++i)
            {
                if (layout->m_SpanEffects.Full())
                    layout->m_SpanEffects.OffsetCapacity(1);
                layout->m_SpanEffects.Push(object_style.m_EffectIndex + i);
            }
            span.m_EffectCount += object_style.m_EffectCount;
            if (layout->m_ResolvedSpans.Full())
                layout->m_ResolvedSpans.OffsetCapacity(1);
            layout->m_ResolvedSpans.Push(span);
            glyph.m_MarkupSpanIndex = (uint16_t)(layout->m_ResolvedSpans.Size() - 1);
        }
    }
    layout->m_NamedStyleRevision = revision;
    return true;
}

bool TextLayoutRefreshObjectStyles(HTextLayout layout)
{
    return RefreshObjectStyles(layout, true);
}

void TextLayoutInitializeObjectStyles(HTextLayout layout)
{
    layout->m_BaseStyleCount = (uint16_t)layout->m_Styles.Size();
    layout->m_BaseEffectCount = (uint16_t)layout->m_Effects.Size();
    layout->m_BaseSpanEffectCount = (uint16_t)layout->m_SpanEffects.Size();
    layout->m_BaseResolvedSpanCount = (uint16_t)layout->m_ResolvedSpans.Size();
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

uint8_t TextLayoutSetObjectState(HTextLayout layout, uint64_t object_id, TextLayoutObjectState state, uint8_t enabled)
{
    const uint16_t valid_states = TEXT_LAYOUT_OBJECT_STATE_HOVERED |
                                  TEXT_LAYOUT_OBJECT_STATE_ACTIVE |
                                  TEXT_LAYOUT_OBJECT_STATE_VISITED |
                                  TEXT_LAYOUT_OBJECT_STATE_DISABLED;
    const uint16_t state_bit = (uint16_t)state;
    if (!layout || !object_id || (state_bit & valid_states) == 0 || (state_bit & (state_bit - 1)) != 0)
        return 0;
    bool changed = false;
    for (uint32_t i = 0; i < layout->m_Objects.Size(); ++i)
    {
        TextLayoutObject& object = layout->m_Objects[i];
        if (object.m_Id != object_id)
            continue;
        const uint16_t new_state = enabled ? object.m_State | state_bit : object.m_State & ~state_bit;
        if (new_state != object.m_State)
        {
            object.m_State = new_state;
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
            if (object.m_Type == TEXT_LAYOUT_OBJECT_SPRITE)
                layout->m_ReleaseObject(layout->m_ObjectContext, &object);
        }
    }
}

void TextLayoutGetBounds(HTextLayout layout, float* width, float* height)
{
    *width = layout->m_Width;
    *height = layout->m_Height;
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
        layout->m_ElapsedTime += delta_time;
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
        for (uint32_t glyph_index = line.m_Index; glyph_index < line.m_Index + line.m_Length; ++glyph_index)
        {
            const TextGlyph& glyph = glyphs[glyph_index];
            if (glyph.m_Flags & TEXT_GLYPH_FLAG_OBJECT)
            {
                line_ascent = fmaxf(line_ascent, glyph.m_Height * 0.8f);
                line_descent = fmaxf(line_descent, glyph.m_Height * 0.2f);
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
            line_ascent = fmaxf(line_ascent, measured_ascent);
            line_descent = fmaxf(line_descent, measured_descent);
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
        layout_height -= last_line_height * (settings->m_Leading - 1.0f);
    for (uint32_t line_index = 0; line_index < layout->m_Lines.Size(); ++line_index)
        lines[line_index].m_Baseline = layout_height - lines[line_index].m_Baseline;
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
        return TextLayoutSkribidiCreateMarkup(collection, markup, settings, outlayout);
#endif
    return TextLayoutLegacyCreateMarkup(collection, markup, settings, outlayout);
}
