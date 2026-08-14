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

#include "font.h"
#include "fontcollection.h"
#include "text_layout.h"

#include <skribidi/skb_font_collection.h>
#include <skribidi/skb_layout.h>
#include <SheenBidi/SBAlgorithm.h>
#include <SheenBidi/SBScriptLocator.h>

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
        return IsCJKLanguage(default_language) ? default_language : "zh";
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
        runs[i].m_Language = GetScriptLanguageHint(runs[i].m_Script, paragraph_language, default_language);
    return paragraph_language;
}

void TextLayoutSegmentRuns(uint32_t* codepoints, uint32_t num_codepoints,
                           const char* default_language,
                           dmArray<TextLayoutRun>& runs, dmArray<TextParagraph>& paragraphs)
{
    runs.SetSize(0);
    paragraphs.SetSize(0);
    if (runs.Capacity() < num_codepoints)
        runs.SetCapacity(num_codepoints);
    if (paragraphs.Capacity() < num_codepoints)
        paragraphs.SetCapacity(num_codepoints);

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
            ++paragraph_end;

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

static uint32_t GetParagraphIndex(const dmArray<TextParagraph>& paragraphs, uint32_t text_offset)
{
    uint32_t paragraph_index = 0;
    while (paragraph_index + 1 < paragraphs.Size() &&
           text_offset >= paragraphs[paragraph_index + 1].m_TextIndex)
        ++paragraph_index;
    return paragraph_index;
}

static uint16_t FindResolvedSpan(const dmArray<TextResolvedSpan>& spans, uint32_t text_offset)
{
    uint32_t first = 0;
    uint32_t count = spans.Size();
    while (count > 0)
    {
        uint32_t                step = count / 2;
        uint32_t                index = first + step;
        const TextResolvedSpan& span = spans[index];
        if (text_offset < span.m_TextOffset)
        {
            count = step;
        }
        else if (text_offset >= span.m_TextOffset + span.m_TextLength)
        {
            first = index + 1;
            count -= step + 1;
        }
        else
        {
            return (uint16_t)index;
        }
    }
    return MARKUP_INVALID_INDEX;
}

static void AllocLayout(LayoutContext* ctx, HFontCollection collection)
{
    ctx->m_Alloc = skb_temp_alloc_create(4*1024);
}

static void FreeLayout(LayoutContext* ctx)
{
    // HACK: Due to a bug in SkriBidi (https://github.com/memononen/Skribidi/issues/84)
    // the "lines" member isn't freed. So, for now we do it here:
    free((void*)skb_layout_get_lines(ctx->m_Layout));

    skb_layout_destroy(ctx->m_Layout);
    skb_temp_alloc_destroy(ctx->m_Alloc);
}

static bool LayoutText(LayoutContext* ctx,
                        uint32_t* codepoints, uint32_t num_codepoints,
                        TextLayoutSettings* settings,
                         TextLayout* layout)
{
    HFontCollection font_collection = layout->m_FontCollection;

    float line_width = settings->m_Width;
    // Ensure explicit line breaks are honored without forcing word-wrap.
    // When auto line breaking is disabled or width is zero, use a very large width
    // and keep wrap enabled so the layout engine can still split on '\n'.
    if (!settings->m_LineBreak || line_width <= 0.0f)
        line_width = 1000000.0f;
    skb_layout_params_t params = {0};
    params.font_collection    = FontCollectionGetSkribidiPtr(font_collection),
    params.lang               = GetSystemLanguage(),
    params.origin             = {0, 0},
    params.layout_width       = line_width,
    params.layout_height      = 1000000.0f,
    params.base_direction     = SKB_DIRECTION_AUTO,
    // Always allow wrapping in the layout engine. With a very large width, this
    // does not introduce automatic wraps, but it lets explicit '\n' split lines.
    params.text_wrap          = (uint8_t)SKB_WRAP_WORD,
    params.text_overflow      = SKB_OVERFLOW_NONE,
    params.vertical_trim      = SKB_VERTICAL_TRIM_DEFAULT,
    params.horizontal_align   = SKB_ALIGN_START,          // TODO: support the other way around (ask author for SKB_ALIGN_RIGHT/LEFT ?)
    params.vertical_align     = SKB_ALIGN_START,          // TODO: support the other way around (ask author for SKB_ALIGN_RIGHT/LEFT ?)
    params.baseline_align     = SKB_BASELINE_MIDDLE,
    params.flags              = 0;

    float tracking = settings->m_Tracking * settings->m_Size;

    dmArray<TextLayoutRun> segments;
    TextLayoutSegmentRuns(codepoints, num_codepoints, params.lang, segments, layout->m_Paragraphs);

    struct SkribidiAttributes
    {
        skb_attribute_t m_Attributes[7];
    };

    dmArray<SkribidiAttributes>   attributes;
    dmArray<skb_text_run_utf32_t> runs;
    uint32_t                      sprite_count = 0;
    for (uint32_t i = 0; i < layout->m_Objects.Size(); ++i)
        sprite_count += layout->m_Objects[i].m_Type == TEXT_LAYOUT_OBJECT_SPRITE;
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
            }
            while (object_index < layout->m_Objects.Size() &&
                   (layout->m_Objects[object_index].m_Type != TEXT_LAYOUT_OBJECT_SPRITE ||
                    layout->m_Objects[object_index].m_TextOffset < run_offset))
            {
                ++object_index;
            }
            const TextLayoutObject* object = object_index < layout->m_Objects.Size() &&
                                                     layout->m_Objects[object_index].m_TextOffset == run_offset
                                                 ? &layout->m_Objects[object_index]
                                                 : 0;
            if (object)
                run_end = run_offset + object->m_TextLength;
            else if (object_index < layout->m_Objects.Size() && layout->m_Objects[object_index].m_TextOffset < run_end)
                run_end = layout->m_Objects[object_index].m_TextOffset;

            SkribidiAttributes run_attributes;
            run_attributes.m_Attributes[0] = skb_attribute_make_writing(segment.m_Language, SKB_DIRECTION_AUTO);
            run_attributes.m_Attributes[1] = skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, font_size, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL);
            run_attributes.m_Attributes[2] = skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, settings->m_Leading);
            run_attributes.m_Attributes[3] = skb_attribute_make_spacing(tracking, 0.0f);
            uint32_t attribute_count = 4;
            if (resolved_span && (resolved_span->m_DecorationFlags & TEXT_RESOLVED_DECORATION_UNDERLINE))
            {
                skb_decoration_style_t pattern = resolved_span->m_UnderlinePattern == TEXT_DECORATION_PATTERN_DASHED ? SKB_DECORATION_STYLE_DASHED : SKB_DECORATION_STYLE_SOLID;
                run_attributes.m_Attributes[attribute_count++] = skb_attribute_make_decoration(SKB_DECORATION_UNDERLINE, pattern, 0.0f, 0.0f, skb_rgba(255, 255, 255, 255));
            }
            if (resolved_span && (resolved_span->m_DecorationFlags & TEXT_RESOLVED_DECORATION_STRIKE))
            {
                skb_decoration_style_t pattern = resolved_span->m_StrikePattern == TEXT_DECORATION_PATTERN_DASHED ? SKB_DECORATION_STYLE_DASHED : SKB_DECORATION_STYLE_SOLID;
                run_attributes.m_Attributes[attribute_count++] = skb_attribute_make_decoration(SKB_DECORATION_THROUGHLINE, pattern, 0.0f, 0.0f, skb_rgba(255, 255, 255, 255));
            }
            if (object)
                run_attributes.m_Attributes[attribute_count++] = skb_attribute_make_object(object->m_Width, object->m_Height, object->m_Height * 0.8f, (intptr_t)object->m_Id);
            if (attributes.Full())
                attributes.OffsetCapacity(32);
            if (runs.Full())
                runs.OffsetCapacity(32);
            attributes.Push(run_attributes);
            skb_text_run_utf32_t run = { codepoints + run_offset, (int32_t)(run_end - run_offset), 0, (int32_t)attribute_count };
            runs.Push(run);
            run_offset = run_end;
        }
    }

    for (uint32_t i = 0; i < runs.Size(); ++i)
        runs[i].attributes = attributes[i].m_Attributes;

    skb_layout_t* skblayout = skb_layout_create_from_runs_utf32(ctx->m_Alloc, &params, runs.Begin(), runs.Size());
    ctx->m_Layout = skblayout;

    const int32_t glyphs_count = skb_layout_get_glyphs_count(skblayout);
    const skb_glyph_t* glyphs = skb_layout_get_glyphs(skblayout);
    const skb_glyph_run_t* glyph_runs = skb_layout_get_glyph_runs(skblayout);
    const skb_text_attributes_span_t* attrib_spans = skb_layout_get_attribute_spans(skblayout);
    const skb_layout_line_t* layout_lines = skb_layout_get_lines(skblayout);
    int32_t lines_count = skb_layout_get_lines_count(skblayout);

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

    // From example_testbed.c
    for (int li = 0; li < lines_count; li++)
    {
        const skb_layout_line_t* line = &layout_lines[li];

        uint32_t prev_glyph_index = layout->m_Glyphs.Size();
        float content_advance = 0.0f;

        for (int32_t ri = line->glyph_run_range.start; ri < line->glyph_run_range.end; ri++)
        {
            const skb_glyph_run_t* glyph_run = &glyph_runs[ri];
            const skb_text_attributes_span_t* span = &attrib_spans[glyph_run->span_idx];
            const skb_attribute_font_t attr_font = skb_attributes_get_font(span->attributes, span->attributes_count);
            const bool object_run = (glyph_run->flags & SKB_GLYPH_RUN_IS_OBJECT) != 0;
            for (int32_t gi = glyph_run->glyph_range.start; gi < glyph_run->glyph_range.end; gi++)
            {
                const skb_glyph_t* skbglyph = &glyphs[gi];

                float gx = skbglyph->offset_x;
                float gy = -skbglyph->offset_y;

                skb_rect2_t bounds;
                if (object_run)
                {
                    const skb_attribute_object_t object = skb_attributes_get_object(span->attributes, span->attributes_count);
                    bounds = { skbglyph->offset_x, skbglyph->offset_y, object.width, object.height };
                }
                else
                {
                    bounds = skb_font_get_glyph_bounds(params.font_collection, skbglyph->font_handle, skbglyph->gid, attr_font.size);
                }

                uint32_t codepoint_index = skbglyph->text_range.start;
                uint32_t cp = codepoints[codepoint_index];

                // Skip explicit line break codepoints. They should not
                // contribute a visible glyph nor count towards line length.
                if (IsParagraphSeparator(cp))
                {
                    continue;
                }

                content_advance += fabsf(skbglyph->advance_x);

                TextGlyph glyph = {0};
                glyph.m_X           = gx;
                glyph.m_Y           = gy;
                glyph.m_GlyphIndex  = skbglyph->gid;
                glyph.m_Cluster     = codepoint_index;
                glyph.m_Codepoint   = cp;
                glyph.m_Width       = bounds.width;
                glyph.m_Height      = bounds.height;
                glyph.m_RenderScale = attr_font.size / settings->m_Size;
                glyph.m_Font        = object_run ? FontCollectionGetFont(font_collection, 0) : FontCollectionGetFontFromHandle(font_collection, skbglyph->font_handle);
                glyph.m_Flags       = object_run ? TEXT_GLYPH_FLAG_OBJECT : 0;
                glyph.m_MarkupSpanIndex = FindResolvedSpan(layout->m_ResolvedSpans, codepoint_index);
                if (glyph.m_MarkupSpanIndex != MARKUP_INVALID_INDEX)
                {
                    glyph.m_StyleIndex = layout->m_ResolvedSpans[glyph.m_MarkupSpanIndex].m_StyleIndex;
                }

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
        l.m_ParagraphIndex = GetParagraphIndex(layout->m_Paragraphs, line->text_range.start);
        TextParagraph& paragraph = layout->m_Paragraphs[l.m_ParagraphIndex];
        if (paragraph.m_LineCount == 0)
            paragraph.m_LineIndex = layout->m_Lines.Size();
        ++paragraph.m_LineCount;
        layout->m_Lines.Push(l);
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

    const int32_t decoration_count = skb_layout_get_decorations_count(skblayout);
    const skb_decoration_t* decorations = skb_layout_get_decorations(skblayout);
    layout->m_Decorations.SetCapacity(decoration_count);
    for (int32_t line_index = 0; line_index < lines_count; ++line_index)
    {
        const skb_layout_line_t& source_line = layout_lines[line_index];
        for (int32_t decoration_index = source_line.decorations_range.start; decoration_index < source_line.decorations_range.end; ++decoration_index)
        {
            const skb_decoration_t& source = decorations[decoration_index];
            if (!isfinite(source.thickness) || source.thickness <= 0.0f)
                continue;
            float decoration_x = source.offset_x;
            float decoration_length = source.length;
            float pattern_offset = source.pattern_offset;
            bool recovered_geometry = false;
            if (!isfinite(decoration_x) || !isfinite(decoration_length) || decoration_length <= 0.0f || decoration_length > source_line.bounds.width + 1.0f)
            {
                recovered_geometry = true;
                float decoration_end = -FLT_MAX;
                decoration_x = FLT_MAX;
                for (int32_t run_index = source_line.glyph_run_range.start; run_index < source_line.glyph_run_range.end; ++run_index)
                {
                    const skb_glyph_run_t& run = glyph_runs[run_index];
                    if (run.span_idx != source.span_idx)
                        continue;
                    for (int32_t glyph_index = run.glyph_range.start; glyph_index < run.glyph_range.end; ++glyph_index)
                    {
                        const skb_glyph_t& glyph = glyphs[glyph_index];
                        decoration_x = fminf(decoration_x, fminf(glyph.offset_x, glyph.offset_x + glyph.advance_x));
                        decoration_end = fmaxf(decoration_end, fmaxf(glyph.offset_x, glyph.offset_x + glyph.advance_x));
                    }
                }
                decoration_length = decoration_end - decoration_x;
                if (decoration_length <= 0.0f)
                    continue;
                pattern_offset = decoration_x - params.origin.x;
            }
            uint32_t glyph_start = UINT32_MAX;
            uint32_t glyph_end = 0;
            const skb_text_attributes_span_t& attribute_span = attrib_spans[source.span_idx];
            const TextLine& output_line = layout->m_Lines[line_index];
            const float decoration_end = decoration_x + decoration_length;
            for (uint32_t glyph_index = output_line.m_Index; glyph_index < output_line.m_Index + output_line.m_Length; ++glyph_index)
            {
                const TextGlyph& glyph = layout->m_Glyphs[glyph_index];
                if (glyph.m_Cluster >= (uint32_t)attribute_span.text_range.start &&
                    glyph.m_Cluster < (uint32_t)attribute_span.text_range.end &&
                    glyph.m_X < decoration_end && glyph.m_X + glyph.m_Width >= decoration_x)
                {
                    glyph_start = glyph_start < glyph_index ? glyph_start : glyph_index;
                    glyph_end = glyph_end > glyph_index + 1 ? glyph_end : glyph_index + 1;
                }
            }
            if (glyph_start == UINT32_MAX || glyph_end - glyph_start > UINT16_MAX)
                continue;
            const skb_attribute_decoration_t attribute = attribute_span.attributes[source.attribute_idx].decoration;
            float source_baseline = source_line.baseline;
            for (int32_t run_index = source_line.glyph_run_range.start; run_index < source_line.glyph_run_range.end; ++run_index)
            {
                if (glyph_runs[run_index].span_idx == source.span_idx)
                {
                    source_baseline = glyph_runs[run_index].baseline;
                    break;
                }
            }
            float decoration_y = source.offset_y;
            if (attribute.position != SKB_DECORATION_THROUGHLINE)
                decoration_y += source.thickness * 0.5f;
            TextDecoration decoration = {};
            decoration.m_X = decoration_x;
            decoration.m_Y = -(decoration_y - source_baseline);
            decoration.m_Length = decoration_length;
            decoration.m_Thickness = source.thickness;
            decoration.m_PatternOffset = pattern_offset;
            decoration.m_GlyphStart = glyph_start;
            decoration.m_GlyphCount = (uint16_t)(glyph_end - glyph_start);
            decoration.m_LineIndex = (uint16_t)line_index;
            decoration.m_Pattern = attribute.style == SKB_DECORATION_STYLE_DASHED ? TEXT_DECORATION_PATTERN_DASHED : TEXT_DECORATION_PATTERN_SOLID;
            if (recovered_geometry)
            {
                bool duplicate = false;
                for (uint32_t i = 0; i < layout->m_Decorations.Size(); ++i)
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
                if (duplicate)
                    continue;
            }
            layout->m_Decorations.Push(decoration);
        }
    }
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
        TextLayoutAdoptResolvedMarkup(layout, resolved, settings);

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
