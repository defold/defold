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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h> // INT_MAX

#include <dlib/log.h>
#include <dlib/profile.h>
#include <dlib/math.h>
#include <dlib/time.h>
#include <dlib/utf8.h>

#include <dmsdk/font/font.h>
#include <dmsdk/font/fontcollection.h>

#include "text_layout.h"

static const dmhash_t TAG_SPRITE = dmHashString64("sprite");

static const uint32_t CHAR_NEWLINE = '\n';
static const uint32_t CHAR_FALLBACK = '~'; // 126

static inline uint32_t NextBreak(TextGlyph* glyphs, uint32_t num_glyphs, uint32_t* cursor, uint32_t* n)
{
    uint32_t c = 0;
    do
    {
        c = (*cursor) < num_glyphs ? glyphs[(*cursor)++].m_Codepoint : 0;
        if (c != 0)
            *n = *n + 1;
    } while (c != 0 && !dmUtf8::IsBreaking(c));
    return c;
}

static inline uint32_t SkipWS(TextGlyph* glyphs, uint32_t num_glyphs, uint32_t* cursor, uint32_t* n)
{
    uint32_t c = 0;
    do
    {
        c = (*cursor) < num_glyphs ? glyphs[(*cursor)++].m_Codepoint : 0;
        if (c != 0)
            *n = *n + 1;
    } while (c != 0 && (c == dmUtf8::UTF_WHITESPACE_SPACE || c == dmUtf8::UTF_WHITESPACE_ZERO_WIDTH_SPACE));

    return c;
}

/*
 * Simple text-layout.
 * Single trailing white-space is not accounted for when breaking but the count is included in the lines array
 * and should be skipped when rendering
 */
template <typename Metric>
void Layout(TextLayout*     layout,
                float       width,
                float*      text_width,
                Metric      metrics,
                bool        measure_trailing_space)
{
    TextGlyph*  glyphs = layout->m_Glyphs.Begin();
    uint32_t    num_glyphs = layout->m_Glyphs.Size();
    uint32_t    cursor = 0;
    float       max_width = 0;
    uint32_t    c;
    do
    {
        uint32_t n = 0, last_n = 0;
        uint32_t row_start = cursor;
        uint32_t last_cursor = cursor;
        float    w = 0, last_w = 0;
        do
        {
            c = NextBreak(glyphs, num_glyphs, &cursor, &n);
            if (n > 0)
            {
                int trim = 0;
                if (c != 0)
                    trim = 1;
                w = metrics(row_start, n - trim, measure_trailing_space);
                if (dmMath::Abs(w) <= width)
                {
                    last_n = n - trim;
                    last_w = w;
                    last_cursor = cursor;
                    if (c != CHAR_NEWLINE && !measure_trailing_space)
                        c = SkipWS(glyphs, num_glyphs, &cursor, &n);
                }
                else if (last_n != 0)
                {
                    // rewind if we have more to scan
                    cursor = last_cursor;
                    c = glyphs[last_cursor++].m_Codepoint;
                }
            }
        } while (dmMath::Abs(w) <= width && c != 0 && c != CHAR_NEWLINE);
        if (dmMath::Abs(w) > width && last_n == 0)
        {
            int trim = 0;
            if (c != 0)
                trim = 1;
            last_n = n - trim;
            last_w = w;
        }

        if ((c != 0 || last_n > 0))
        {
            if (layout->m_Lines.Full())
                layout->m_Lines.OffsetCapacity(8);

            TextLine line = {0};
            line.m_Width = last_w;
            line.m_Index = row_start;
            line.m_Length = last_n;
            layout->m_Lines.Push(line);

            if (last_w < 0)
                max_width = dmMath::Min(max_width, last_w);
            else
                max_width = dmMath::Max(max_width, last_w);
        }
    } while (c);

    *text_width = max_width;
}

static float GetLineTextMetrics(TextGlyph* glyphs, uint32_t row_start, uint32_t n, bool monospace, float padding, float tracking, bool measure_trailing_space)
{
    if (n <= 0)
        return 0;

    glyphs += row_start;
    if (!measure_trailing_space)
    {
        if (glyphs[n-1].m_Codepoint == dmUtf8::UTF_WHITESPACE_SPACE)
        {
            --n;
            if (n <= 0)
                return 0;
        }
    }

    // note: tracking is ignored since it's already added in TextLayoutLegacyCreate
    // note: padding is only intended for monospaced fonts, see comment in fontmap.h

    TextGlyph last = glyphs[n-1];

    float row_start_x = glyphs[0].m_X;

    if (monospace)
    {
        float extent_last = last.m_Advance + padding;
        float width = last.m_X - row_start_x + extent_last;
        return width;
    }

    // find the last non-whitespace character while also measuring the width
    // of any trailing whitespace characters
    float trailing_space_width = 0;
    for (int i = n - 1; i >= 0; i--)
    {
        TextGlyph g = glyphs[i];
        if (g.m_Codepoint != dmUtf8::UTF_WHITESPACE_SPACE)
        {
            last = g;
            break;
        }
        trailing_space_width += g.m_Advance;
    }
    float extent_last = last.m_LeftBearing + last.m_Width;
    float width = last.m_X - row_start_x + extent_last + trailing_space_width;
    return width;
}

struct LayoutMetrics
{
    TextGlyph*  m_Glyphs;
    bool        m_Monospace;
    float       m_Padding;
    float       m_Tracking;
    LayoutMetrics(TextGlyph* glyphs, bool monospace, float padding, float tracking)
    : m_Glyphs(glyphs)
    , m_Monospace(monospace)
    , m_Padding(padding)
    , m_Tracking(tracking)
    {}
    float operator()(uint32_t row_start, uint32_t n, bool measure_trailing_space)
    {
        return GetLineTextMetrics(m_Glyphs, row_start, n, m_Monospace, m_Padding, m_Tracking, measure_trailing_space);
    }
};

static void TextLayoutLegacyFree(TextLayout* layout)
{
    TextLayoutReleaseObjects(layout);
    delete layout;
}

static uint32_t GetParagraphIndex(const dmArray<TextParagraph>& paragraphs, uint32_t text_index)
{
    uint32_t paragraph_index = 0;

    while (paragraph_index + 1 < paragraphs.Size() &&
           text_index >= paragraphs[paragraph_index + 1].m_TextIndex)
        ++paragraph_index;

    return paragraph_index;
}

static const TextLayoutObject* FindSpriteObject(const TextLayout* layout, uint32_t text_offset)
{
    for (uint32_t i = 0; i < layout->m_Objects.Size(); ++i)
    {
        const TextLayoutObject& object = layout->m_Objects[i];

        if (object.m_Tag == TAG_SPRITE && object.m_TextOffset == text_offset)
        {
            return &object;
        }
    }

    return 0;
}

static void CreateParagraphs(TextLayout* layout, uint32_t* codepoints, uint32_t num_codepoints)
{
    uint32_t paragraph_start = 0;

    while (paragraph_start < num_codepoints)
    {
        uint32_t paragraph_end = paragraph_start;

        while (paragraph_end < num_codepoints && codepoints[paragraph_end] != CHAR_NEWLINE)
        {
            ++paragraph_end;
        }

        TextParagraph paragraph = { paragraph_start, paragraph_end - paragraph_start, 0, 0,
                                    TEXT_DIRECTION_LTR };
        layout->m_Paragraphs.Push(paragraph);
        paragraph_start = paragraph_end + (paragraph_end < num_codepoints);
    }

    for (uint32_t i = 0; i < layout->m_Lines.Size(); ++i)
    {
        TextLine& line = layout->m_Lines[i];
        line.m_ParagraphIndex = GetParagraphIndex(layout->m_Paragraphs, line.m_Index);
        TextParagraph& paragraph = layout->m_Paragraphs[line.m_ParagraphIndex];

        if (paragraph.m_LineCount == 0)
        {
            paragraph.m_LineIndex = i;
        }

        ++paragraph.m_LineCount;
    }
}

static void CreateDecorations(TextLayout* layout, TextLayoutSettings* settings)
{
    for (uint32_t line_index = 0; line_index < layout->m_Lines.Size(); ++line_index)
    {
        const TextLine& line = layout->m_Lines[line_index];
        const uint32_t line_end = line.m_Index + line.m_Length;

        for (uint32_t span_index = 0; span_index < layout->m_ResolvedSpans.Size(); ++span_index)
        {
            const TextResolvedSpan& span = layout->m_ResolvedSpans[span_index];

            if (span.m_DecorationFlags == 0)
            {
                continue;
            }

            const uint32_t span_end = span.m_TextOffset + span.m_TextLength;
            const uint32_t start = span.m_TextOffset > line.m_Index ? span.m_TextOffset : line.m_Index;
            const uint32_t end = span_end < line_end ? span_end : line_end;

            if (start >= end)
            {
                continue;
            }

            const TextGlyph& first = layout->m_Glyphs[start];
            const TextGlyph& last = layout->m_Glyphs[end - 1];
            const float font_size = settings->m_Size * first.m_RenderScale;
            const float thickness = fmaxf(1.0f, font_size * 0.05f);
            const float padded_length = last.m_X + last.m_Advance - first.m_X;
            const float decoration_inset = padded_length > settings->m_Padding ? (float)settings->m_Padding : 0.0f;
            const float length = padded_length;

            for (uint32_t flag = TEXT_RESOLVED_DECORATION_UNDERLINE; flag <= TEXT_RESOLVED_DECORATION_STRIKE; flag <<= 1)
            {
                if ((span.m_DecorationFlags & flag) == 0)
                {
                    continue;
                }

                TextDecoration decoration = {};
                decoration.m_X = first.m_X + decoration_inset;
                decoration.m_Y = flag == TEXT_RESOLVED_DECORATION_UNDERLINE ? -font_size * 0.1f : font_size * 0.3f;
                decoration.m_Length = length;
                decoration.m_Thickness = thickness;
                decoration.m_PatternOffset = first.m_X + decoration_inset;
                decoration.m_GlyphStart = start;
                decoration.m_GlyphCount = (uint16_t)(end - start);
                decoration.m_LineIndex = (uint16_t)line_index;
                decoration.m_Pattern = flag == TEXT_RESOLVED_DECORATION_UNDERLINE ? span.m_UnderlinePattern : span.m_StrikePattern;

                if (layout->m_Decorations.Full())
                {
                    layout->m_Decorations.OffsetCapacity(8);
                }

                layout->m_Decorations.Push(decoration);
            }
        }
    }
}

static TextResult TextLayoutLegacyCreateInternal(HFontCollection collection,
                                                 uint32_t* codepoints, uint32_t num_codepoints,
                                                 TextLayoutSettings* settings, ResolvedMarkup* resolved,
                                                 HTextLayout* outlayout)
{
    TextLayout* layout = new TextLayout;
    layout->m_Destroy = TextLayoutLegacyFree;
    layout->m_RefCount = 1;

    layout->m_Glyphs.SetCapacity(num_codepoints);
    layout->m_Glyphs.SetSize(num_codepoints);
    layout->m_Lines.SetSize(0);
    layout->m_Paragraphs.SetCapacity(num_codepoints);
    layout->m_Paragraphs.SetSize(0);
    layout->m_FontCollection = collection;
    layout->m_NamedStyleRevision = 0xffffffff;
    layout->m_BaseStyleCount = 0;
    layout->m_BaseEffectCount = 0;
    layout->m_BaseSpanEffectCount = 0;
    layout->m_BaseResolvedSpanCount = 0;
    layout->m_NumValidGlyphs = 0;
    layout->m_ElapsedTime = 0.0;
    layout->m_ReleaseObject = 0;
    layout->m_ObjectContext = 0;

    if (resolved)
    {
        TextLayoutAdoptResolvedMarkup(layout, resolved, settings);
    }

    HFont font = FontCollectionGetFont(collection, 0);
    float scale = FontGetScaleFromSize(font, settings->m_Size);

    uint32_t ascent = (uint32_t)FontGetAscent(font, 1.0f);
    uint32_t descent = (uint32_t)fabsf(FontGetDescent(font, 1.0f));
    uint32_t line_height = ascent + descent;
    float line_height_scaled = line_height * scale;
    float tracking = line_height_scaled * settings->m_Tracking;

    FontGlyphOptions options;
    options.m_Scale = 1.0f; // Return in points
    options.m_GenerateImage = false;

    uint32_t num_whitespaces = 0;
    // Lay them all out in a single line, using points
// TODO: Make this optional, so that user can choose to use pixel alignment
    float x = 0;
    float y = 0; // the legacy "shaping" doesn't support Y offsets
    FontGlyph font_glyph;
    for (uint32_t i = 0; i < num_codepoints; ++i)
    {
        uint32_t c = codepoints[i];
        TextGlyph g = {0};
        g.m_Font = font;
        g.m_Codepoint = c;
        g.m_Cluster = i;
        g.m_RenderScale = 1.0f;
        g.m_MarkupSpanIndex = MARKUP_INVALID_INDEX;
        // make sure to always set the position of the glyph, regardless
        // if FontGetGlyph was successful or not (see #11766)
        g.m_X = x;
        g.m_Y = y;

        const TextLayoutObject* object = c == 0xfffc ? FindSpriteObject(layout, i) : 0;

        if (object)
        {
            g.m_Width = object->m_Width;
            g.m_Height = object->m_Height;
            g.m_Advance = object->m_Width;
            g.m_Flags = TEXT_GLYPH_FLAG_OBJECT;
            x += g.m_Advance + tracking;
            layout->m_Glyphs[i] = g;
            ++num_whitespaces;
            continue;
        }

        FontResult r = FontGetGlyph(font, c, &options, &font_glyph);

        if (FONT_RESULT_OK != r && CHAR_FALLBACK)
        {
            r = FontGetGlyph(font, CHAR_FALLBACK, &options, &font_glyph);
        }

        uint32_t whitespace = dmUtf8::IsWhiteSpace(c);
        num_whitespaces += whitespace;

        if (FONT_RESULT_OK == r)
        {
            if (!whitespace)
            {
                g.m_Codepoint = font_glyph.m_Codepoint;   // may be the correct one, or the fallback one
            }
            g.m_GlyphIndex = font_glyph.m_GlyphIndex;
            g.m_Width = font_glyph.m_Width * scale;
            g.m_Height = font_glyph.m_Height * scale;
            g.m_Advance = font_glyph.m_Advance * scale;
            g.m_LeftBearing = font_glyph.m_LeftBearing * scale;

            x += g.m_Advance + tracking;
        }

        layout->m_Glyphs[i] = g;
    }

    if (!layout->m_ResolvedSpans.Empty())
    {
        uint32_t resolved_span_index = 0;
        float    advance_adjustment = 0.0f;

        for (uint32_t i = 0; i < num_codepoints; ++i)
        {
            while (resolved_span_index < layout->m_ResolvedSpans.Size() &&
                   i >= layout->m_ResolvedSpans[resolved_span_index].m_TextOffset +
                        layout->m_ResolvedSpans[resolved_span_index].m_TextLength)
            {
                ++resolved_span_index;
            }

            if (resolved_span_index < layout->m_ResolvedSpans.Size())
            {
                const TextResolvedSpan& span = layout->m_ResolvedSpans[resolved_span_index];

                if (i >= span.m_TextOffset)
                {
                    layout->m_Glyphs[i].m_MarkupSpanIndex = (uint16_t)resolved_span_index;
                    layout->m_Glyphs[i].m_StyleIndex = span.m_StyleIndex;
                }
            }

            TextGlyph& glyph = layout->m_Glyphs[i];
            glyph.m_X += advance_adjustment;

            if ((glyph.m_Flags & TEXT_GLYPH_FLAG_OBJECT) == 0 && glyph.m_StyleIndex < layout->m_Styles.Size())
            {
                const TextRenderStyle& style = layout->m_Styles[glyph.m_StyleIndex];

                if (style.m_Flags & TEXT_RENDER_STYLE_FONT_SIZE)
                {
                    glyph.m_RenderScale = style.m_FontSize / settings->m_Size;
                    glyph.m_Width *= glyph.m_RenderScale;
                    glyph.m_Height *= glyph.m_RenderScale;
                    const float base_advance = glyph.m_Advance;
                    glyph.m_Advance *= glyph.m_RenderScale;
                    glyph.m_LeftBearing *= glyph.m_RenderScale;
                    advance_adjustment += glyph.m_Advance - base_advance;
                }
            }
        }
    }

    layout->m_NumValidGlyphs = layout->m_Glyphs.Size() - num_whitespaces;

    LayoutMetrics lm(layout->m_Glyphs.Begin(), settings->m_Monospace, settings->m_Padding, settings->m_Tracking);
    float max_line_width;

    float width = settings->m_Width;
    if (!settings->m_LineBreak)
        width = 1000000.0f;
    Layout(layout, width, &max_line_width, lm, !settings->m_LineBreak);
    CreateParagraphs(layout, codepoints, num_codepoints);
    TextLayoutFinalizeLineBaselines(layout, settings);
    CreateDecorations(layout, settings);

    // metrics->m_MaxAscent = ascent;
    // metrics->m_MaxDescent = descent;
    layout->m_Width = max_line_width;
    TextLayoutInitializeObjectStyles(layout);

    *outlayout = layout;
    return TEXT_RESULT_OK;
}

TextResult TextLayoutLegacyCreate(HFontCollection collection,
                                  uint32_t* codepoints, uint32_t num_codepoints,
                                  TextLayoutSettings* settings, HTextLayout* outlayout)
{
    return TextLayoutLegacyCreateInternal(collection, codepoints, num_codepoints, settings, 0, outlayout);
}

TextResult TextLayoutLegacyCreateMarkup(HFontCollection collection, HMarkup markup,
                                        TextLayoutSettings* settings, HTextLayout* outlayout)
{
    ResolvedMarkup resolved;

    if (!TextLayoutResolveMarkup(markup, settings, &resolved))
    {
        *outlayout = 0;

        return TEXT_RESULT_ERROR;
    }

    return TextLayoutLegacyCreateInternal(collection,
                                          const_cast<uint32_t*>(MarkupGetText(markup)),
                                          MarkupGetTextLength(markup),
                                          settings,
                                          &resolved,
                                          outlayout);
}
