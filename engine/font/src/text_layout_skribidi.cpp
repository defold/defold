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
    skb_layout_params_t params = {
        .font_collection    = FontCollectionGetSkribidiPtr(font_collection),
        .lang               = "en-us",                  // TODO: support setting
        .origin             = {0, 0},
        .layout_width       = line_width,
        .layout_height      = 1000000.0f,
        .base_direction     = SKB_DIRECTION_AUTO,
        .text_wrap          = (uint8_t)(settings->m_LineBreak ? SKB_WRAP_WORD : SKB_WRAP_NONE),
        .text_overflow      = SKB_OVERFLOW_NONE,
        .vertical_trim      = SKB_VERTICAL_TRIM_DEFAULT,
        .horizontal_align   = SKB_ALIGN_START,          // TODO: support the other way around (ask author for SKB_ALIGN_RIGHT/LEFT ?)
        .vertical_align     = SKB_ALIGN_START,          // TODO: support the other way around (ask author for SKB_ALIGN_RIGHT/LEFT ?)
        .baseline_align     = SKB_BASELINE_MIDDLE,
        .flags              = 0
    };

    // TODO: Allo setting default as italic etc
    const skb_attribute_t attributes[] = {
        skb_attribute_make_font(SKB_FONT_FAMILY_DEFAULT, settings->m_Size, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL),
        skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, settings->m_Leading),
        skb_attribute_make_spacing(settings->m_Tracking, 0.0f)
    };

    skb_text_run_utf32_t runs[] = {
        { codepoints, (int32_t)num_codepoints, attributes, DM_ARRAY_SIZE(attributes) },
        // { "moikkelis!\n", -1, attributes_italic, SKB_COUNTOF(attributes_italic) },

        // { "این یک 😬👀🚨 تست است\n", -1, attributes_deco2, SKB_COUNTOF(attributes_deco2) },

        // { "Donec sodales ", -1, attributes_deco1, SKB_COUNTOF(attributes_deco1) },
        // { "vitae odio ", -1, attributes_deco2, SKB_COUNTOF(attributes_deco2) },
        // { "dapibus pulvinar\n", -1, attributes_deco3, SKB_COUNTOF(attributes_deco3) },

        // { "ہے۔ kofi یہ ایک\n", -1, attributes_small, SKB_COUNTOF(attributes_small) },
        // { "POKS! 🧁\n", -1, attributes_big, SKB_COUNTOF(attributes_big) },
        // { "11/17\n", -1, attributes_fracts, SKB_COUNTOF(attributes_fracts) },
        // { "शकति शक्ति ", -1, attributes_italic, SKB_COUNTOF(attributes_italic) },
        // { "今天天气晴朗。 ", -1, attributes_small, SKB_COUNTOF(attributes_small) },
    };

    skb_layout_t* skblayout = skb_layout_create_from_runs_utf32(ctx->m_Alloc, &params, runs, DM_ARRAY_SIZE(runs));
    ctx->m_Layout = skblayout;

    const int32_t glyphs_count = skb_layout_get_glyphs_count(skblayout);
    const skb_glyph_t* glyphs = skb_layout_get_glyphs(skblayout);
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

    const float edit_layout_y = 0;
    uint32_t num_whitespaces = 0;

    float ox = 0.0f;
    float oy = 0.0f;

    float max_ascender = 0.0f;
    float max_descender = 0.0f;

    int debug = 0;

    // From example_testbed.c
    for (int li = 0; li < lines_count; li++)
    {
        const skb_layout_line_t* line = &layout_lines[li];

        max_ascender = dmMath::Max(max_ascender, -line->ascender); // make it positive
        max_descender = dmMath::Max(max_descender, line->descender);

        //float pen_x = ox + line->bounds.x;

        uint32_t prev_glyph_index = layout->m_Glyphs.Size();

        skb_glyph_run_iterator_t glyph_iter = skb_glyph_run_iterator_make(glyphs, glyphs_count, line->glyph_range.start, line->glyph_range.end);
        skb_range_t glyph_range;
        skb_font_handle_t font_handle = 0;
        uint16_t span_idx = 0;

        float max_glyph_bounds_width = 0;
        float max_glyph_bounds_height = 0;

        while (skb_glyph_run_iterator_next(&glyph_iter, &glyph_range, &font_handle, &span_idx))
        {
            const skb_text_attributes_span_t* span = &attrib_spans[span_idx];
            // const skb_attribute_fill_t attr_fill = skb_attributes_get_fill(span->attributes, span->attributes_count);
            const skb_attribute_font_t attr_font = skb_attributes_get_font(span->attributes, span->attributes_count);
            for (int32_t gi = glyph_range.start; gi < glyph_range.end; ++gi)
            {
                const skb_glyph_t* skbglyph = &glyphs[gi];

                float gx = ox + skbglyph->offset_x;
                float gy = oy + edit_layout_y + -skbglyph->offset_y;

                skb_rect2_t bounds = skb_font_get_glyph_bounds(layout_params->font_collection, font_handle, skbglyph->gid, attr_font.size);

                max_glyph_bounds_width = dmMath::Max(max_glyph_bounds_width, bounds.width);
                max_glyph_bounds_height = dmMath::Max(max_glyph_bounds_height, -bounds.height);

            //     bounds.x += gx;
            //     bounds.y += gy;
            //     bounds = view_transform_rect(&ctx->view, bounds);
            //     draw_rect(bounds.x, bounds.y, bounds.width, bounds.height, skb_rgba(255,128,64,128));
            // }

                uint32_t codepoint_index = skbglyph->text_range.start;

                TextGlyph glyph = {0};
                glyph.m_X           = gx;
                glyph.m_Y           = gy;
                glyph.m_GlyphIndex  = skbglyph->gid;
                glyph.m_Cluster     = codepoint_index;
                glyph.m_Codepoint   = codepoints[codepoint_index];
                glyph.m_Width       = bounds.width;
                glyph.m_Height      = bounds.height;
                glyph.m_Font        = FontCollectionGetFontFromHandle(font_collection, skbglyph->font_handle);

                layout->m_Glyphs.Push(glyph);

                num_whitespaces += dmUtf8::IsWhiteSpace(glyph.m_Codepoint);

                if (debug)
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
            }
        }

        // End of line
        uint32_t glyph_index = layout->m_Glyphs.Size();

        TextLine l;
        l.m_Width   = line->bounds.width;
        l.m_Index   = prev_glyph_index;
        l.m_Length  = glyph_index - prev_glyph_index;
        layout->m_Lines.Push(l);
    }

    layout->m_NumValidGlyphs = layout->m_Glyphs.Size() - num_whitespaces;
    layout->m_Direction = skb_is_rtl(skb_layout_get_resolved_direction(skblayout)) ? TEXT_DIRECTION_RTL : TEXT_DIRECTION_LTR;

    skb_rect2_t layout_bounds = skb_layout_get_bounds(skblayout);
    layout->m_Width = layout_bounds.width;
    layout->m_Height = layout_bounds.height;

    return true;
}

void TextLayoutFreeSkribidi(TextLayout* layout)
{
    delete layout;
}

TextResult TextLayoutCreateSkribidi(HFontCollection collection,
                            uint32_t* codepoints, uint32_t num_codepoints,
                            TextLayoutSettings* settings, TextLayout** outlayout)
{
    TextLayout* layout = new TextLayout;
    layout->m_Free = TextLayoutFreeSkribidi;

    layout->m_Glyphs.SetCapacity(num_codepoints);
    layout->m_Glyphs.SetSize(0);
    layout->m_Lines.SetSize(0);
    layout->m_FontCollection = collection;
    layout->m_Direction = TEXT_DIRECTION_LTR;
    layout->m_NumValidGlyphs = 0;

    LayoutContext ctx = {0};
    AllocLayout(&ctx, collection);

    bool result = LayoutText(&ctx,
                                codepoints, num_codepoints,
                                settings,
                                layout);

    FreeLayout(&ctx);

    if (!result)
    {
        TextLayoutFreeSkribidi(layout);
        layout = 0;
    }

    *outlayout = layout;
    return result ? TEXT_RESULT_OK : TEXT_RESULT_ERROR;
}

#endif // FONT_USE_SKRIBIDI

