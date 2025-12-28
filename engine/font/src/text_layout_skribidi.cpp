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

#if defined(FONT_USE_SKRIBIDI)

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h> // INT_MAX

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/profile.h>
#include <dlib/math.h>
#include <dlib/time.h>
#include <dlib/utf8.h>

#include "font.h"
#include "fontcollection.h"
#include "text_layout.h"

#include <skribidi/skb_font_collection.h>
#include <skribidi/skb_layout.h>

struct LayoutContext
{
    skb_temp_alloc_t*   m_Alloc;
    skb_layout_t*       m_Layout;
};

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
    params.lang               = "en-us",                  // TODO: support setting
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

    // TODO: Allo setting default as italic etc
    const skb_attribute_t attributes[] = {
        skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, settings->m_Size, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL),
        skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, settings->m_Leading),
        skb_attribute_make_spacing(tracking, 0.0f)
    };

    // TODO: Support rich text

    skb_text_run_utf32_t runs[] = {
        { codepoints, (int32_t)num_codepoints, attributes, DM_ARRAY_SIZE(attributes) },
    };

    skb_layout_t* skblayout = skb_layout_create_from_runs_utf32(ctx->m_Alloc, &params, runs, DM_ARRAY_SIZE(runs));
    ctx->m_Layout = skblayout;

    const int32_t glyphs_count = skb_layout_get_glyphs_count(skblayout);
    const skb_glyph_t* glyphs = skb_layout_get_glyphs(skblayout);
    const skb_glyph_run_t* glyph_runs = skb_layout_get_glyph_runs(skblayout);
    const skb_layout_params_t* layout_params = skb_layout_get_params(skblayout);
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

    const float edit_layout_y = 0.0f;
    uint32_t num_whitespaces = 0;

    float ox = 0.0f;
    float oy = 0.0f;

    float max_ascender = 0.0f;
    float max_descender = 0.0f;

    // From example_testbed.c
    for (int li = 0; li < lines_count; li++)
    {
        const skb_layout_line_t* line = &layout_lines[li];

        max_ascender = dmMath::Max(max_ascender, -line->ascender); // make it positive
        max_descender = dmMath::Max(max_descender, line->descender);

        uint32_t prev_glyph_index = layout->m_Glyphs.Size();

        skb_font_handle_t font_handle = 0;

        float max_glyph_bounds_width = 0.0f;
        float max_glyph_bounds_height = 0.0f;

        for (int32_t ri = line->glyph_run_range.start; ri < line->glyph_run_range.end; ri++)
        {
            const skb_glyph_run_t* glyph_run = &glyph_runs[ri];
            const skb_text_attributes_span_t* span = &attrib_spans[glyph_run->span_idx];
            const skb_attribute_font_t attr_font = skb_attributes_get_font(span->attributes, span->attributes_count);
            for (int32_t gi = glyph_run->glyph_range.start; gi < glyph_run->glyph_range.end; gi++)
            {
                const skb_glyph_t* skbglyph = &glyphs[gi];

                float gx = ox + skbglyph->offset_x;
                float gy = oy + edit_layout_y + -skbglyph->offset_y;

                skb_rect2_t bounds = skb_font_get_glyph_bounds(layout_params->font_collection, font_handle, skbglyph->gid, attr_font.size);

                max_glyph_bounds_width = dmMath::Max(max_glyph_bounds_width, bounds.width);
                max_glyph_bounds_height = dmMath::Max(max_glyph_bounds_height, -bounds.height);

                uint32_t codepoint_index = skbglyph->text_range.start;
                uint32_t cp = codepoints[codepoint_index];

                // Skip explicit line break codepoints. They should not
                // contribute a visible glyph nor count towards line length.
                if (cp == dmUtf8::UTF_WHITESPACE_NEW_LINE || cp == dmUtf8::UTF_WHITESPACE_CARRIAGE_RETURN)
                {
                    continue;
                }

                TextGlyph glyph = {0};
                glyph.m_X           = gx;
                glyph.m_Y           = gy;
                glyph.m_GlyphIndex  = skbglyph->gid;
                glyph.m_Cluster     = codepoint_index;
                glyph.m_Codepoint   = cp;
                glyph.m_Width       = bounds.width;
                glyph.m_Height      = bounds.height;
                glyph.m_Font        = FontCollectionGetFontFromHandle(font_collection, skbglyph->font_handle);

                layout->m_Glyphs.Push(glyph);

                num_whitespaces += dmUtf8::IsWhiteSpace(glyph.m_Codepoint);

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
        l.m_Index   = prev_glyph_index;
        l.m_Length  = glyph_index - prev_glyph_index;
        layout->m_Lines.Push(l);
    }

    layout->m_NumValidGlyphs = layout->m_Glyphs.Size() - num_whitespaces;
    layout->m_Direction = skb_is_rtl(skb_layout_get_resolved_direction(skblayout)) ? TEXT_DIRECTION_RTL : TEXT_DIRECTION_LTR;

    skb_rect2_t layout_bounds = skb_layout_get_bounds(skblayout);
    layout->m_Width = layout_bounds.width - (tracking > 0 ? tracking : 0);
    layout->m_Height = layout_bounds.height;
    if (lines_count > 0 && settings->m_Leading > 0.0f)
    {
        // Normalize Skribidi line height so leading affects only inter-line spacing.
        float base_line_height = layout_bounds.height / ((float)lines_count * settings->m_Leading);
        layout->m_Height = layout_bounds.height - base_line_height * (settings->m_Leading - 1.0f);
    }

    return true;
}

void TextLayoutSkribidiFree(TextLayout* layout)
{
    layout->m_Glyphs.SetCapacity(0);
    layout->m_Lines.SetCapacity(0);
    delete layout;
}

TextResult TextLayoutSkribidiCreate(HFontCollection collection,
                            uint32_t* codepoints, uint32_t num_codepoints,
                            TextLayoutSettings* settings, TextLayout** outlayout)
{
    TextLayout* layout = new TextLayout;
    layout->m_Free = TextLayoutSkribidiFree;

    layout->m_Glyphs.SetCapacity(num_codepoints);
    layout->m_Glyphs.SetSize(0);
    layout->m_Lines.SetSize(0);
    layout->m_FontCollection = collection;
    layout->m_Direction = TEXT_DIRECTION_LTR;
    layout->m_NumValidGlyphs = 0;

    if (num_codepoints == 0) // empty string
    {
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

    *outlayout = layout;
    return result ? TEXT_RESULT_OK : TEXT_RESULT_ERROR;
}

#endif // FONT_USE_SKRIBIDI
