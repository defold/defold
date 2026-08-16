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

#if defined(FONT_USE_SKRIBIDI)

#include <stdint.h>
#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <dlib/sys.h>
#include <dlib/utf8.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/hashtable.h>

#include "font.h"
#include "fontcollection.h"
#include "text_layout.h"

#include <skribidi/skb_attributes.h>
#include <skribidi/skb_font_collection.h>
#include <skribidi/skb_layout.h>
#include <SheenBidi/SBAlgorithm.h>
#include <SheenBidi/SBScriptLocator.h>

static const dmhash_t TAG_SPRITE = dmHashString64("sprite");

struct LayoutContext
{
    skb_temp_alloc_t*   m_Alloc;
    skb_layout_t*       m_Layout;
};

static const char* GetSystemLanguage()
{
    static dmSys::SystemInfo info;
    static bool initialized = false;

    if (!initialized)
    {
        dmSys::GetSystemInfo(&info);
        initialized = true;
    }

    return info.m_DeviceLanguage[0] ? info.m_DeviceLanguage : info.m_Language;
}

static bool IsParagraphSeparator(uint32_t codepoint)
{
    return codepoint == '\r' || codepoint == '\n' || codepoint == 0x0085 ||
           codepoint == 0x2028 || codepoint == 0x2029;
}

static bool IsCJKLanguage(const char* language)
{
    return language &&
           ((language[0] == 'j' && language[1] == 'a') ||
            (language[0] == 'k' && language[1] == 'o') ||
            (language[0] == 'z' && language[1] == 'h'));
}

// Unicode scripts do not uniquely identify languages. These tags are fallback hints;
// CJK is resolved from the whole paragraph and ambiguous text retains the system language.
static const char* GetScriptLanguageHint(uint32_t script, const char* paragraph_language,
                                         const char* default_language);

static const char* GetParagraphLanguageHint(const dmArray<TextLayoutRun>& runs, uint32_t first_run, const char* default_language)
{
    bool     has_han = false;
    uint32_t paragraph_script = SBScriptZYYY;

    for (uint32_t i = first_run; i < runs.Size(); ++i)
    {
        if (paragraph_script == SBScriptZYYY &&
            runs[i].m_Script != SBScriptZYYY && runs[i].m_Script != SBScriptZINH)
        {
            paragraph_script = runs[i].m_Script;
        }

        switch (runs[i].m_Script)
        {
            case SBScriptHIRA:
            case SBScriptKANA:
                return "ja";
            case SBScriptHANG:
                return "ko";
            case SBScriptBOPO:
                return "zh-Hant";
            case SBScriptHANI:
                has_han = true;
                break;
            default:
                break;
        }
    }

    if (has_han)
    {
        return IsCJKLanguage(default_language) ? default_language : "zh";
    }

    return GetScriptLanguageHint(paragraph_script, default_language, default_language);
}

static const char* GetScriptLanguageHint(uint32_t script, const char* paragraph_language, const char* default_language)
{
    switch (script)
    {
        case SBScriptARAB:
            return "ar";
        case SBScriptARMN:
            return "hy";
        case SBScriptBENG:
            return "bn";
        case SBScriptBOPO:
            return "zh-Hant";
        case SBScriptCYRL:
            return "ru";
        case SBScriptDEVA:
            return "hi";
        case SBScriptGEOR:
            return "ka";
        case SBScriptGREK:
            return "el";
        case SBScriptGUJR:
            return "gu";
        case SBScriptGURU:
            return "pa";
        case SBScriptHANG:
            return "ko";
        case SBScriptHANI:
        case SBScriptHIRA:
        case SBScriptKANA:
            return paragraph_language;
        case SBScriptZINH:
        case SBScriptZYYY:
            return paragraph_language;
        case SBScriptHEBR:
            return "he";
        case SBScriptKNDA:
            return "kn";
        case SBScriptLAOO:
            return "lo";
        case SBScriptMLYM:
            return "ml";
        case SBScriptORYA:
            return "or";
        case SBScriptTAML:
            return "ta";
        case SBScriptTELU:
            return "te";
        case SBScriptTHAI:
            return "th";
        case SBScriptTIBT:
            return "bo";
        case SBScriptETHI:
            return "am";
        case SBScriptKHMR:
            return "km";
        case SBScriptMYMR:
            return "my";
        case SBScriptSINH:
            return "si";
        default:
            return default_language;
    }
}

static const char* SegmentParagraph(uint32_t* codepoints, uint32_t paragraph_start,
                                    uint32_t paragraph_length, const char* default_language,
                                    SBScriptLocatorRef locator, dmArray<TextLayoutRun>& runs)
{
    uint32_t            first_run = runs.Size();
    SBCodepointSequence sequence = { SBStringEncodingUTF32, codepoints + paragraph_start, paragraph_length };
    SBScriptLocatorLoadCodepoints(locator, &sequence);

    while (SBScriptLocatorMoveNext(locator))
    {
        const SBScriptAgent* agent = SBScriptLocatorGetAgent(locator);
        TextLayoutRun run = { paragraph_start + (uint32_t)agent->offset,
                              (uint32_t)agent->length, agent->script, default_language };
        runs.Push(run);
    }

    const char* paragraph_language = GetParagraphLanguageHint(runs, first_run, default_language);

    for (uint32_t i = first_run; i < runs.Size(); ++i)
    {
        runs[i].m_Language = GetScriptLanguageHint(runs[i].m_Script, paragraph_language, default_language);
    }

    return paragraph_language;
}

static bool IsAsciiText(const uint32_t* codepoints, uint32_t num_codepoints)
{
    for (uint32_t i = 0; i < num_codepoints; ++i)
    {
        if (codepoints[i] > 0x7f)
        {
            return false;
        }
    }

    return true;
}

static void TextLayoutSegmentAsciiRuns(uint32_t* codepoints, uint32_t num_codepoints,
                                       const char* default_language,
                                       dmArray<TextLayoutRun>& runs, dmArray<TextParagraph>& paragraphs)
{
    uint32_t paragraph_start = 0;

    while (paragraph_start < num_codepoints)
    {
        uint32_t paragraph_end = paragraph_start;

        while (paragraph_end < num_codepoints && !IsParagraphSeparator(codepoints[paragraph_end]))
        {
            ++paragraph_end;
        }

        if (paragraph_end > paragraph_start)
        {
            TextLayoutRun run = { paragraph_start, paragraph_end - paragraph_start, SBScriptLATN, default_language };
            runs.Push(run);
        }

        TextParagraph paragraph = { paragraph_start, paragraph_end - paragraph_start, 0, 0, TEXT_DIRECTION_LTR };
        paragraphs.Push(paragraph);

        if (paragraph_end < num_codepoints)
        {
            uint32_t separator_length = 1;

            if (codepoints[paragraph_end] == '\r' && paragraph_end + 1 < num_codepoints &&
                codepoints[paragraph_end + 1] == '\n')
            {
                separator_length = 2;
            }

            TextLayoutRun separator = { paragraph_end, separator_length, SBScriptZYYY, default_language };
            runs.Push(separator);
            paragraph_end += separator_length;
        }

        paragraph_start = paragraph_end;
    }
}

static void TextLayoutSegmentSingleParagraphRuns(uint32_t* codepoints, uint32_t num_codepoints,
                                                 const char* default_language,
                                                 dmArray<TextLayoutRun>& runs, dmArray<TextParagraph>& paragraphs)
{
    runs.SetSize(0);
    paragraphs.SetSize(0);

    if (runs.Capacity() < num_codepoints)
    {
        runs.SetCapacity(num_codepoints);
    }

    if (paragraphs.Capacity() < 1)
    {
        paragraphs.SetCapacity(1);
    }

    SBScriptLocatorRef locator = SBScriptLocatorCreate();
    SegmentParagraph(codepoints, 0, num_codepoints, default_language, locator, runs);
    SBScriptLocatorRelease(locator);
    TextParagraph paragraph = { 0, num_codepoints, 0, 0, TEXT_DIRECTION_LTR };
    paragraphs.Push(paragraph);
}

static bool IsSingleParagraphUnicodeText(const uint32_t* codepoints, uint32_t num_codepoints)
{
    bool unicode = false;

    for (uint32_t i = 0; i < num_codepoints; ++i)
    {
        if (IsParagraphSeparator(codepoints[i]))
        {
            return false;
        }

        unicode |= codepoints[i] > 0x7f;
    }

    return unicode;
}

void TextLayoutSegmentRuns(uint32_t* codepoints, uint32_t num_codepoints,
                           const char* default_language,
                           dmArray<TextLayoutRun>& runs, dmArray<TextParagraph>& paragraphs)
{
    runs.SetSize(0);
    paragraphs.SetSize(0);

    if (runs.Capacity() < num_codepoints)
    {
        runs.SetCapacity(num_codepoints);
    }

    if (paragraphs.Capacity() < num_codepoints)
    {
        paragraphs.SetCapacity(num_codepoints);
    }

    if (IsAsciiText(codepoints, num_codepoints))
    {
        TextLayoutSegmentAsciiRuns(codepoints, num_codepoints, default_language, runs, paragraphs);

        return;
    }

    SBScriptLocatorRef locator = SBScriptLocatorCreate();
    SBCodepointSequence sequence = { SBStringEncodingUTF32, codepoints, num_codepoints };
    SBAlgorithmRef bidi_algorithm = SBAlgorithmCreate(&sequence);
    uint32_t paragraph_start = 0;
    const char* previous_paragraph_language = default_language;
    TextDirection previous_paragraph_direction = TEXT_DIRECTION_LTR;

    while (paragraph_start < num_codepoints)
    {
        uint32_t paragraph_end = paragraph_start;

        while (paragraph_end < num_codepoints && !IsParagraphSeparator(codepoints[paragraph_end]))
        {
            ++paragraph_end;
        }

        const char*   paragraph_language = previous_paragraph_language;
        TextDirection paragraph_direction = previous_paragraph_direction;

        if (paragraph_end > paragraph_start)
        {
            SBParagraphRef bidi_paragraph = SBAlgorithmCreateParagraph(bidi_algorithm, paragraph_start,
                                                                        paragraph_end - paragraph_start,
                                                                        SBLevelDefaultLTR);
            paragraph_direction = (SBParagraphGetBaseLevel(bidi_paragraph) & 1)
                                ? TEXT_DIRECTION_RTL : TEXT_DIRECTION_LTR;
            SBParagraphRelease(bidi_paragraph);
            paragraph_language = SegmentParagraph(codepoints, paragraph_start, paragraph_end - paragraph_start, default_language, locator, runs);
        }

        TextParagraph paragraph = { paragraph_start, paragraph_end - paragraph_start, 0, 0,
                                    paragraph_direction };
        paragraphs.Push(paragraph);

        if (paragraph_end < num_codepoints)
        {
            uint32_t separator_length = 1;

            if (codepoints[paragraph_end] == '\r' && paragraph_end + 1 < num_codepoints &&
                codepoints[paragraph_end + 1] == '\n')
            {
                separator_length = 2;
            }

            TextLayoutRun separator = { paragraph_end, separator_length, SBScriptZYYY, paragraph_language };
            runs.Push(separator);
            paragraph_end += separator_length;
        }

        previous_paragraph_language = paragraph_language;
        previous_paragraph_direction = paragraph_direction;
        paragraph_start = paragraph_end;
    }

    SBAlgorithmRelease(bidi_algorithm);
    SBScriptLocatorRelease(locator);
}

static bool HasSameLayoutAttributes(const TextLayout* layout, const TextResolvedSpan& left, const TextResolvedSpan& right)
{
    const TextRenderStyle& left_style = layout->m_Styles[left.m_StyleIndex];
    const TextRenderStyle& right_style = layout->m_Styles[right.m_StyleIndex];

    return left_style.m_FontSize == right_style.m_FontSize &&
           left.m_DecorationFlags == right.m_DecorationFlags &&
           left.m_UnderlinePattern == right.m_UnderlinePattern &&
           left.m_StrikePattern == right.m_StrikePattern;
}

struct CachedGlyphBounds
{
    skb_rect2_t       m_Bounds;
    skb_font_handle_t m_Font;
    uint32_t          m_Size;
    uint16_t          m_Glyph;
};

static skb_rect2_t GetGlyphBounds(dmHashTable64<CachedGlyphBounds>& cache,
                                  skb_font_collection_t* collection, skb_font_handle_t font,
                                  uint16_t glyph, float size)
{
    uint32_t size_bits;
    memcpy(&size_bits, &size, sizeof(size_bits));
    struct
    {
        skb_font_handle_t m_Font;
        uint32_t          m_Size;
        uint16_t          m_Glyph;
    } key = {};
    key.m_Font  = font;
    key.m_Size  = size_bits;
    key.m_Glyph = glyph;
    const dmhash_t hash = dmHashBuffer64(&key, sizeof(key));
    CachedGlyphBounds* cached = cache.Get(hash);

    if (cached && cached->m_Font == font && cached->m_Size == size_bits && cached->m_Glyph == glyph)
    {
        return cached->m_Bounds;
    }

    CachedGlyphBounds entry = { skb_font_get_glyph_bounds(collection, font, glyph, size), font, size_bits, glyph };

    if (cache.Full())
    {
        cache.OffsetCapacity(32);
    }

    cache.Put(hash, entry);

    return entry.m_Bounds;
}

static void AddLineDecorations(TextLayout* layout, uint16_t line_index,
                               const skb_layout_line_t& source_line,
                               const skb_layout_run_t* source_runs,
                               const skb_decoration_t* source_decorations,
                               const dmArray<uint32_t>& source_glyph_to_output,
                               float origin_x, uint16_t padding)
{
    // A duplicate must be on the same physical line.
    const uint32_t line_decoration_start = layout->m_Decorations.Size();

    for (int32_t decoration_index = source_line.decorations_range.start; decoration_index < source_line.decorations_range.end; ++decoration_index)
    {
        const skb_decoration_t& source = source_decorations[decoration_index];

        if (source.type != SKB_DECORATION_LINE || !isfinite(source.line.thickness) || source.line.thickness <= 0.0f)
        {
            continue;
        }

        const skb_layout_run_t& source_run = source_runs[source.line.layout_run_idx];
        uint32_t glyph_start = UINT32_MAX;
        uint32_t glyph_end = 0;

        for (int32_t glyph_index = source_run.glyph_range.start; glyph_index < source_run.glyph_range.end; ++glyph_index)
        {
            const uint32_t output_glyph_index = source_glyph_to_output[glyph_index];

            if (output_glyph_index == UINT32_MAX)
            {
                continue;
            }

            glyph_start = glyph_start < output_glyph_index ? glyph_start : output_glyph_index;
            glyph_end = glyph_end > output_glyph_index + 1 ? glyph_end : output_glyph_index + 1;
        }

        const float decoration_x      = source.line.x;
        const float decoration_length = source.line.length;

        if (!isfinite(decoration_x) || !isfinite(decoration_length) || decoration_length <= 0.0f ||
            glyph_start == UINT32_MAX || glyph_end - glyph_start > UINT16_MAX)
            continue;
        const float                         pattern_offset   = decoration_x - origin_x;
        const float                         decoration_inset = decoration_length > padding ? (float)padding : 0.0f;
        float                               decoration_y     = source.line.y;

        if (source.line.position != SKB_DECORATION_LINE_THROUGH)
        {
            decoration_y += source.line.thickness * 0.5f;
        }

        TextDecoration decoration = {};
        decoration.m_X             = decoration_x + decoration_inset;
        decoration.m_Y             = -(decoration_y - source_run.ref_baseline);
        decoration.m_Length        = decoration_length;
        decoration.m_Thickness     = source.line.thickness;
        decoration.m_PatternOffset = pattern_offset + decoration_inset;
        decoration.m_GlyphStart    = glyph_start;
        decoration.m_GlyphCount    = (uint16_t)(glyph_end - glyph_start);
        decoration.m_LineIndex     = line_index;
        decoration.m_Pattern       = source.line.style == SKB_DECORATION_STYLE_DASHED ? TEXT_DECORATION_PATTERN_DASHED : TEXT_DECORATION_PATTERN_SOLID;
        bool duplicate = false;

        for (uint32_t i = line_decoration_start; i < layout->m_Decorations.Size(); ++i)
        {
            const TextDecoration& existing = layout->m_Decorations[i];

            if (existing.m_LineIndex == decoration.m_LineIndex &&
                existing.m_GlyphStart == decoration.m_GlyphStart &&
                existing.m_GlyphCount == decoration.m_GlyphCount &&
                existing.m_Pattern == decoration.m_Pattern &&
                fabsf(existing.m_Y - decoration.m_Y) < 0.001f)
            {
                duplicate = true;
                break;
            }
        }

        if (!duplicate)
        {
            layout->m_Decorations.Push(decoration);
            uint32_t decoration_position = layout->m_Decorations.Size() - 1;

            while (decoration_position > line_decoration_start)
            {
                TextDecoration& previous = layout->m_Decorations[decoration_position - 1];
                TextDecoration& current = layout->m_Decorations[decoration_position];

                if (previous.m_LineIndex != current.m_LineIndex ||
                    previous.m_GlyphStart != current.m_GlyphStart ||
                    previous.m_GlyphCount != current.m_GlyphCount ||
                    previous.m_Y <= current.m_Y)
                {
                    break;
                }

                TextDecoration swap = previous;
                previous = current;
                current = swap;
                --decoration_position;
            }
        }
    }
}

static void AllocLayout(LayoutContext* ctx, HFontCollection collection)
{
    ctx->m_Alloc = skb_temp_alloc_create(4*1024);
}

static void FreeLayout(LayoutContext* ctx)
{
    skb_layout_destroy(ctx->m_Layout);
    skb_temp_alloc_destroy(ctx->m_Alloc);
}

static bool LayoutText(LayoutContext* ctx,
                        uint32_t* codepoints, uint32_t num_codepoints,
                        TextLayoutSettings* settings,
                        TextLayout* layout)
{
    HFontCollection font_collection = layout->m_FontCollection;

    const bool wrap_text = settings->m_LineBreak && settings->m_Width > 0.0f;
    const float line_width = wrap_text ? settings->m_Width : SKB_SIZE_AUTO;
    const float tracking = settings->m_Tracking * settings->m_Size;

    skb_attribute_t layout_attributes[] = {
        skb_attribute_make_text_base_direction(SKB_DIRECTION_AUTO),
        skb_attribute_make_font_family(SKB_FONT_FAMILY_DEFAULT),
        skb_attribute_make_font_size(settings->m_Size),
        skb_attribute_make_font_weight(SKB_WEIGHT_NORMAL),
        skb_attribute_make_font_style(SKB_STYLE_NORMAL),
        skb_attribute_make_font_stretch(SKB_STRETCH_NORMAL),
        skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, settings->m_Leading),
        skb_attribute_make_letter_spacing(tracking),
        skb_attribute_make_text_wrap(wrap_text ? SKB_WRAP_WORD : SKB_WRAP_NONE),
        skb_attribute_make_text_overflow(SKB_OVERFLOW_NONE),
        skb_attribute_make_vertical_trim(SKB_VERTICAL_TRIM_DEFAULT),
        skb_attribute_make_horizontal_align(SKB_ALIGN_START),
        skb_attribute_make_vertical_align(SKB_ALIGN_START),
        skb_attribute_make_baseline_align(SKB_BASELINE_MIDDLE),
    };

    skb_layout_params_t params = {0};
    params.font_collection  = FontCollectionGetSkribidiPtr(font_collection);
    params.layout_width     = line_width;
    params.layout_height    = SKB_SIZE_AUTO;

    params.layout_attributes.attributes       = layout_attributes;
    params.layout_attributes.attributes_count = sizeof(layout_attributes) / sizeof(layout_attributes[0]);

    dmArray<TextLayoutRun> segments;
    const char* default_language = GetSystemLanguage();
    const bool resolve_single_paragraph_direction = IsSingleParagraphUnicodeText(codepoints, num_codepoints);

    if (resolve_single_paragraph_direction)
    {
        TextLayoutSegmentSingleParagraphRuns(codepoints, num_codepoints, default_language, segments, layout->m_Paragraphs);
    }
    else
    {
        TextLayoutSegmentRuns(codepoints, num_codepoints, default_language, segments, layout->m_Paragraphs);
    }

    dmArray<uint16_t> resolved_span_indices;

    if (!layout->m_ResolvedSpans.Empty())
    {
        resolved_span_indices.SetCapacity(num_codepoints);
        resolved_span_indices.SetSize(num_codepoints);

        for (uint32_t i = 0; i < num_codepoints; ++i)
        {
            resolved_span_indices[i] = MARKUP_INVALID_INDEX;
        }

        for (uint32_t i = 0; i < layout->m_ResolvedSpans.Size(); ++i)
        {
            const TextResolvedSpan& span = layout->m_ResolvedSpans[i];
            const uint32_t          span_end = span.m_TextOffset + span.m_TextLength;

            for (uint32_t j = span.m_TextOffset; j < span_end; ++j)
            {
                resolved_span_indices[j] = (uint16_t)i;
            }
        }
    }

    struct SkribidiAttributes
    {
        skb_attribute_t m_Attributes[5];
    };

    dmArray<SkribidiAttributes>   attributes;
    dmArray<skb_content_run_t>    runs;
    uint32_t                      sprite_count = 0;

    for (uint32_t i = 0; i < layout->m_Objects.Size(); ++i)
    {
        sprite_count += layout->m_Objects[i].m_Tag == TAG_SPRITE;
    }

    // A sprite needs its own object run and may split an existing text run in two.
    const uint32_t run_capacity = segments.Size() + layout->m_ResolvedSpans.Size() + sprite_count * 2 + 1;
    attributes.SetCapacity(run_capacity);
    runs.SetCapacity(run_capacity);
    uint32_t resolved_span_index = 0;
    uint32_t object_index = 0;

    for (uint32_t i = 0; i < segments.Size(); ++i)
    {
        const TextLayoutRun& segment = segments[i];
        uint32_t             run_offset = segment.m_Offset;
        uint32_t             segment_end = segment.m_Offset + segment.m_Length;

        while (run_offset < segment_end)
        {
            while (resolved_span_index + 1 < layout->m_ResolvedSpans.Size() &&
                   run_offset >= layout->m_ResolvedSpans[resolved_span_index].m_TextOffset +
                   layout->m_ResolvedSpans[resolved_span_index].m_TextLength)
            {
                ++resolved_span_index;
            }

            const TextResolvedSpan* resolved_span = layout->m_ResolvedSpans.Empty() ? 0 : &layout->m_ResolvedSpans[resolved_span_index];
            uint32_t                run_end = segment_end;
            float                   font_size = settings->m_Size;

            if (resolved_span && run_offset >= resolved_span->m_TextOffset &&
                run_offset < resolved_span->m_TextOffset + resolved_span->m_TextLength)
            {
                uint32_t resolved_span_end = resolved_span->m_TextOffset + resolved_span->m_TextLength;
                run_end = run_end < resolved_span_end ? run_end : resolved_span_end;
                font_size = layout->m_Styles[resolved_span->m_StyleIndex].m_FontSize;
                uint32_t layout_span_index = resolved_span_index;

                while (run_end < segment_end && layout_span_index + 1 < layout->m_ResolvedSpans.Size())
                {
                    const TextResolvedSpan& next_span = layout->m_ResolvedSpans[layout_span_index + 1];

                    if (next_span.m_TextOffset != run_end || !HasSameLayoutAttributes(layout, *resolved_span, next_span))
                    {
                        break;
                    }

                    ++layout_span_index;
                    const uint32_t next_span_end = next_span.m_TextOffset + next_span.m_TextLength;
                    run_end = segment_end < next_span_end ? segment_end : next_span_end;
                }
            }

            while (object_index < layout->m_Objects.Size() &&
                   (layout->m_Objects[object_index].m_Tag != TAG_SPRITE ||
                    layout->m_Objects[object_index].m_TextOffset < run_offset))
            {
                ++object_index;
            }

            const TextLayoutObject* object = object_index < layout->m_Objects.Size() &&
                                                     layout->m_Objects[object_index].m_TextOffset == run_offset
                                                 ? &layout->m_Objects[object_index]
                                                 : 0;
            if (object)
            {
                run_end = run_offset + object->m_TextLength;
            }
            else if (object_index < layout->m_Objects.Size() && layout->m_Objects[object_index].m_TextOffset < run_end)
            {
                run_end = layout->m_Objects[object_index].m_TextOffset;
            }

            SkribidiAttributes run_attributes;
            run_attributes.m_Attributes[0] = skb_attribute_make_lang(segment.m_Language);
            run_attributes.m_Attributes[1] = skb_attribute_make_font_size(font_size);
            uint32_t attribute_count = 2;

            if (resolved_span && (resolved_span->m_DecorationFlags & TEXT_RESOLVED_DECORATION_UNDERLINE))
            {
                skb_decoration_style_t pattern = resolved_span->m_UnderlinePattern == TEXT_DECORATION_PATTERN_DASHED ? SKB_DECORATION_STYLE_DASHED : SKB_DECORATION_STYLE_SOLID;
                run_attributes.m_Attributes[attribute_count++] = skb_attribute_make_decoration(SKB_DECORATION_LINE_UNDER, pattern, 0.0f, 0.0f, SKB_PAINT_DECORATION_UNDERLINE);
            }

            if (resolved_span && (resolved_span->m_DecorationFlags & TEXT_RESOLVED_DECORATION_STRIKE))
            {
                skb_decoration_style_t pattern = resolved_span->m_StrikePattern == TEXT_DECORATION_PATTERN_DASHED ? SKB_DECORATION_STYLE_DASHED : SKB_DECORATION_STYLE_SOLID;
                run_attributes.m_Attributes[attribute_count++] = skb_attribute_make_decoration(SKB_DECORATION_LINE_THROUGH, pattern, 0.0f, 0.0f, SKB_PAINT_DECORATION_STRIKETHROUGH);
            }

            if (object)
            {
                run_attributes.m_Attributes[attribute_count++] = skb_attribute_make_object_align(0.8f, SKB_OBJECT_ALIGN_SELF, SKB_BASELINE_ALPHABETIC);
            }

            if (attributes.Full())
            {
                attributes.OffsetCapacity(32);
            }

            if (runs.Full())
            {
                runs.OffsetCapacity(32);
            }

            attributes.Push(run_attributes);
            skb_attribute_set_t run_attribute_set = { attributes.Back().m_Attributes, (int32_t)attribute_count };
            skb_content_run_t run = object
                                  ? skb_content_run_make_object((intptr_t)object->m_Id, object->m_Width, object->m_Height, run_attribute_set, (intptr_t)object->m_Id)
                                  : skb_content_run_make_utf32(codepoints + run_offset, (int32_t)(run_end - run_offset), run_attribute_set, (intptr_t)run_offset + 1);
            runs.Push(run);
            run_offset = run_end;
        }
    }

    for (uint32_t i = 0; i < runs.Size(); ++i)
    {
        runs[i].attributes.attributes = attributes[i].m_Attributes;
    }

    skb_layout_t* skblayout = skb_layout_create_from_runs(ctx->m_Alloc, &params, runs.Begin(), runs.Size());
    ctx->m_Layout = skblayout;

    if (resolve_single_paragraph_direction)
    {
        layout->m_Paragraphs[0].m_Direction = skb_layout_get_resolved_direction(skblayout) == SKB_DIRECTION_RTL
                                            ? TEXT_DIRECTION_RTL : TEXT_DIRECTION_LTR;
    }

    const int32_t glyphs_count = skb_layout_get_glyphs_count(skblayout);
    const skb_glyph_t* glyphs = skb_layout_get_glyphs(skblayout);
    const skb_cluster_t* clusters = skb_layout_get_clusters(skblayout);
    const skb_layout_run_t* layout_runs = skb_layout_get_layout_runs(skblayout);
    const skb_layout_line_t* layout_lines = skb_layout_get_lines(skblayout);
    int32_t lines_count = skb_layout_get_lines_count(skblayout);
    const int32_t decoration_count = skb_layout_get_decorations_count(skblayout);
    const skb_decoration_t* decorations = skb_layout_get_decorations(skblayout);

    // alloc
    uint32_t remaining_glyphs = layout->m_Glyphs.Remaining();
    if (remaining_glyphs < glyphs_count)
    {
        layout->m_Glyphs.OffsetCapacity(glyphs_count - remaining_glyphs);
    }

    uint32_t remaining_lines = layout->m_Lines.Remaining();
    if (remaining_lines < lines_count)
    {
        layout->m_Lines.OffsetCapacity(lines_count - remaining_lines);
    }

    uint32_t num_whitespaces = 0;
    uint32_t paragraph_index = 0;
    dmHashTable64<CachedGlyphBounds> glyph_bounds_cache;
    glyph_bounds_cache.SetCapacity(64);
    layout->m_Decorations.SetCapacity(decoration_count);
    dmArray<uint32_t> source_glyph_to_output;
    source_glyph_to_output.SetCapacity(glyphs_count);
    source_glyph_to_output.SetSize(glyphs_count);

    for (int32_t i = 0; i < glyphs_count; ++i)
    {
        source_glyph_to_output[i] = UINT32_MAX;
    }

    // From example_testbed.c
    for (int li = 0; li < lines_count; li++)
    {
        const skb_layout_line_t* line            = &layout_lines[li];
        const bool               has_decorations = line->decorations_range.start != line->decorations_range.end;

        uint32_t prev_glyph_index = layout->m_Glyphs.Size();
        float    content_advance  = 0.0f;

        for (int32_t ri = line->layout_run_range.start; ri < line->layout_run_range.end; ri++)
        {
            const skb_layout_run_t* source_run = &layout_runs[ri];
            const bool object_run = source_run->type == SKB_CONTENT_RUN_OBJECT || source_run->type == SKB_CONTENT_RUN_ICON;
            const float font_size = object_run ? settings->m_Size : source_run->font_size;
            const skb_rect2_t content_bounds = object_run
                                             ? skb_layout_get_layout_run_content_bounds(skblayout, source_run)
                                             : skb_rect2_t{};
            for (int32_t gi = source_run->glyph_range.start; gi < source_run->glyph_range.end; gi++)
            {
                const skb_glyph_t* skbglyph = &glyphs[gi];

                float gx = skbglyph->offset_x;
                float gy = -skbglyph->offset_y;

                skb_rect2_t bounds;

                if (object_run)
                {
                    bounds = content_bounds;
                }
                else
                {
                    bounds = GetGlyphBounds(glyph_bounds_cache, params.font_collection, source_run->font_handle, skbglyph->gid, font_size);
                }

                const skb_cluster_t& cluster = clusters[skbglyph->cluster_idx];
                uint32_t codepoint_index = object_run ? source_run->cluster_range.start : cluster.text_offset;
                uint32_t cp = codepoints[codepoint_index];

                // Skip explicit line break codepoints. They should not
                // contribute a visible glyph nor count towards line length.

                if (IsParagraphSeparator(cp))
                {
                    continue;
                }

                content_advance += fabsf(skbglyph->advance_x);

                TextGlyph glyph = {0};
                glyph.m_X               = gx;
                glyph.m_Y               = gy;
                glyph.m_GlyphIndex      = skbglyph->gid;
                glyph.m_Cluster         = codepoint_index;
                glyph.m_Codepoint       = cp;
                glyph.m_Width           = bounds.width;
                glyph.m_Height          = bounds.height;
                glyph.m_RenderScale     = font_size / settings->m_Size;
                glyph.m_Font            = object_run ? FontCollectionGetFont(font_collection, 0) : FontCollectionGetFontFromHandle(font_collection, source_run->font_handle);
                glyph.m_Flags           = object_run ? TEXT_GLYPH_FLAG_OBJECT : 0;
                glyph.m_MarkupSpanIndex = resolved_span_indices.Empty() ? MARKUP_INVALID_INDEX : resolved_span_indices[codepoint_index];

                if (glyph.m_MarkupSpanIndex != MARKUP_INVALID_INDEX)
                {
                    glyph.m_StyleIndex = layout->m_ResolvedSpans[glyph.m_MarkupSpanIndex].m_StyleIndex;
                }

                source_glyph_to_output[gi] = layout->m_Glyphs.Size();
                layout->m_Glyphs.Push(glyph);

                num_whitespaces += dmUtf8::IsWhiteSpace(glyph.m_Codepoint) || object_run;

#if defined(DM_DEBUG_TEXT_LAYOUT_SKRIBIDI)
                {
                    printf ("gid: %d  c: '%c'  x/y: (%.3f, %.3f)  idx: %d  w/h: %.2f, %.2f\n",
                            glyph.m_GlyphIndex,
                            glyph.m_Codepoint,
                            glyph.m_X,
                            glyph.m_Y,
                            glyph.m_Cluster,
                            glyph.m_Width,
                            glyph.m_Height);
                }
#endif
            }
        }

        // End of line
        uint32_t glyph_index = layout->m_Glyphs.Size();

        TextLine l;
        l.m_Width   = line->bounds.width - (tracking > 0 ? tracking : 0);
        if (li == lines_count - 2 &&
            IsParagraphSeparator(codepoints[num_codepoints - 1]))
        {
            l.m_Width = content_advance - (tracking > 0 ? tracking : 0);
        }
        l.m_Index          = prev_glyph_index;
        l.m_Length         = glyph_index - prev_glyph_index;

        while (paragraph_index + 1 < layout->m_Paragraphs.Size() &&
               line->text_range.start >= (int32_t)layout->m_Paragraphs[paragraph_index + 1].m_TextIndex)
        {
            ++paragraph_index;
        }

        l.m_ParagraphIndex = paragraph_index;
        TextParagraph& paragraph = layout->m_Paragraphs[l.m_ParagraphIndex];

        if (paragraph.m_LineCount == 0)
        {
            paragraph.m_LineIndex = layout->m_Lines.Size();
        }

        ++paragraph.m_LineCount;
        layout->m_Lines.Push(l);

        if (has_decorations)
        {
            AddLineDecorations(layout, (uint16_t)li, *line, layout_runs, decorations, source_glyph_to_output, 0.0f, settings->m_Padding);
        }
    }

    // SkriBidi includes an empty line after a terminal newline. The legacy
    // layout does not, so discard it here as well.
    if (!layout->m_Lines.Empty() &&
        layout->m_Lines.Back().m_Length == 0 &&
        IsParagraphSeparator(codepoints[num_codepoints - 1]))
    {
        TextParagraph& paragraph = layout->m_Paragraphs[layout->m_Lines.Back().m_ParagraphIndex];
        --paragraph.m_LineCount;
        layout->m_Lines.Pop();
        lines_count--;
    }

    layout->m_NumValidGlyphs = layout->m_Glyphs.Size() - num_whitespaces;

    skb_rect2_t layout_bounds = skb_layout_get_bounds(skblayout);
    layout->m_Width = layout_bounds.width - (tracking > 0 ? tracking : 0);
    if (lines_count != skb_layout_get_lines_count(skblayout))
    {
        layout->m_Width = 0.0f;
        for (uint32_t i = 0; i < layout->m_Lines.Size(); ++i)
            layout->m_Width = fmaxf(layout->m_Width, layout->m_Lines[i].m_Width);
    }
    TextLayoutFinalizeLineBaselines(layout, settings);

    return true;
}

void TextLayoutSkribidiFree(TextLayout* layout)
{
    TextLayoutReleaseObjects(layout);
    layout->m_Glyphs.SetCapacity(0);
    layout->m_Lines.SetCapacity(0);
    layout->m_Paragraphs.SetCapacity(0);
    layout->m_Styles.SetCapacity(0);
    layout->m_Effects.SetCapacity(0);
    layout->m_SpanEffects.SetCapacity(0);
    layout->m_ResolvedSpans.SetCapacity(0);
    layout->m_Decorations.SetCapacity(0);
    delete layout;
}

static TextResult TextLayoutSkribidiCreateInternal(HFontCollection     collection,
                                                   uint32_t*           codepoints,
                                                   uint32_t            num_codepoints,
                                                   TextLayoutSettings* settings,
                                                   ResolvedMarkup*     resolved,
                                                   TextLayout**        outlayout)
{
    TextLayout* layout = new TextLayout;
    layout->m_Destroy = TextLayoutSkribidiFree;
    layout->m_RefCount = 1;

    layout->m_Glyphs.SetCapacity(num_codepoints);
    layout->m_Glyphs.SetSize(0);
    layout->m_Lines.SetSize(0);
    layout->m_Paragraphs.SetSize(0);
    layout->m_FontCollection = collection;
    layout->m_NamedStyleRevision = 0xffffffff;
    layout->m_BaseStyleCount = 0;
    layout->m_BaseEffectCount = 0;
    layout->m_BaseSpanEffectCount = 0;
    layout->m_BaseResolvedSpanCount = 0;
    layout->m_NumValidGlyphs = 0;
    layout->m_MaxGlyphWidth = 0.0f;
    layout->m_MaxGlyphHeight = 0.0f;
    layout->m_Width = 0.0f;
    layout->m_Height = 0.0f;
    layout->m_ElapsedTime = 0.0;
    layout->m_ReleaseObject = 0;
    layout->m_ObjectContext = 0;

    if (resolved)
    {
        TextLayoutAdoptResolvedMarkup(layout, resolved, settings);
    }

    if (num_codepoints == 0) // empty string
    {
        TextLayoutInitializeObjectStyles(layout);
        *outlayout = layout;
        return TEXT_RESULT_OK;
    }

    LayoutContext ctx = {0};
    AllocLayout(&ctx, collection);

    bool result = LayoutText(&ctx,
                                codepoints, num_codepoints,
                                settings,
                                layout);

    FreeLayout(&ctx);

    if (!result)
    {
        TextLayoutSkribidiFree(layout);
        layout = 0;
    }
    else
    {
        TextLayoutInitializeObjectStyles(layout);
    }

    *outlayout = layout;
    return result ? TEXT_RESULT_OK : TEXT_RESULT_ERROR;
}

TextResult TextLayoutSkribidiCreate(HFontCollection collection,
                            uint32_t* codepoints, uint32_t num_codepoints,
                            TextLayoutSettings* settings, TextLayout** outlayout)
{
    return TextLayoutSkribidiCreateInternal(collection, codepoints, num_codepoints, settings, 0, outlayout);
}

TextResult TextLayoutSkribidiCreateMarkup(HFontCollection collection, HMarkup markup,
                                          TextLayoutSettings* settings, TextLayout** outlayout)
{
    ResolvedMarkup resolved;

    if (!TextLayoutResolveMarkup(markup, settings, &resolved))
    {
        *outlayout = 0;

        return TEXT_RESULT_ERROR;
    }

    return TextLayoutSkribidiCreateInternal(collection,
                                            const_cast<uint32_t*>(MarkupGetText(markup)),
                                            MarkupGetTextLength(markup),
                                            settings,
                                            &resolved,
                                            outlayout);
}

#endif // FONT_USE_SKRIBIDI
