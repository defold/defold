// Copyright 2026 The Defold Foundation
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

#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ot.h>

#include <stdlib.h>
#include <string.h>

#include "font.h"
#include "font_harfbuzz.h"

struct FontHarfbuzz
{
    hb_font_t*       m_Font;
    hb_draw_funcs_t* m_DrawFuncs;
    FontOutlineType  m_OutlineType;
};

static bool HasTable(hb_face_t* face, hb_tag_t tag)
{
    hb_blob_t* table = hb_face_reference_table(face, tag);
    bool has_table = table && hb_blob_get_length(table) != 0;
    hb_blob_destroy(table);
    return has_table;
}

static FontOutlineType GetOutlineType(hb_face_t* face)
{
    // Match the outline precedence used by hb_font_draw_glyph_or_fail().
    if (HasTable(face, HB_TAG('g', 'l', 'y', 'f')))
        return FONT_OUTLINE_TYPE_GLYF;

    if (HasTable(face, HB_TAG('C', 'F', 'F', '2')))
        return FONT_OUTLINE_TYPE_CFF2;

    if (HasTable(face, HB_TAG('C', 'F', 'F', ' ')))
        return FONT_OUTLINE_TYPE_CFF1;

    return FONT_OUTLINE_TYPE_UNKNOWN;
}

// HarfBuzz emits one callback per outline command. The context grows the
// common outline representation in small batches and records allocation
// failure because draw callbacks cannot return errors.
struct FontHarfbuzzOutlineContext
{
    FontOutline* m_Outline;
    uint32_t     m_Capacity;
    bool         m_Failed;
};

static FontOutlineCommand* PushOutlineCommand(FontHarfbuzzOutlineContext* context, FontOutlineCommandType type)
{
    if (context->m_Failed)
        return 0;

    if (context->m_Outline->m_CommandCount == context->m_Capacity)
    {
        uint32_t capacity = context->m_Capacity + 32;
        void* commands = realloc(context->m_Outline->m_Commands, capacity * sizeof(FontOutlineCommand));
        if (!commands)
        {
            context->m_Failed = true;
            return 0;
        }

        context->m_Outline->m_Commands = (FontOutlineCommand*)commands;
        context->m_Capacity = capacity;
    }

    FontOutlineCommand* command = &context->m_Outline->m_Commands[context->m_Outline->m_CommandCount++];
    memset(command, 0, sizeof(*command));
    command->m_Type = type;
    return command;
}

static void MoveTo(hb_draw_funcs_t*, void* draw_data, hb_draw_state_t*, float to_x, float to_y, void*)
{
    FontOutlineCommand* command = PushOutlineCommand((FontHarfbuzzOutlineContext*)draw_data, FONT_OUTLINE_MOVE_TO);
    if (command)
        command->m_Points[0] = { to_x, to_y };
}

static void LineTo(hb_draw_funcs_t*, void* draw_data, hb_draw_state_t*, float to_x, float to_y, void*)
{
    FontOutlineCommand* command = PushOutlineCommand((FontHarfbuzzOutlineContext*)draw_data, FONT_OUTLINE_LINE_TO);
    if (command)
        command->m_Points[0] = { to_x, to_y };
}

static void QuadraticTo(hb_draw_funcs_t*, void* draw_data, hb_draw_state_t*, float control_x, float control_y, float to_x, float to_y, void*)
{
    FontOutlineCommand* command = PushOutlineCommand((FontHarfbuzzOutlineContext*)draw_data, FONT_OUTLINE_QUADRATIC_TO);
    if (command)
    {
        command->m_Points[0] = { control_x, control_y };
        command->m_Points[1] = { to_x, to_y };
    }
}

static void CubicTo(hb_draw_funcs_t*, void* draw_data, hb_draw_state_t*, float control_1_x, float control_1_y,
                    float control_2_x, float control_2_y, float to_x, float to_y, void*)
{
    FontOutlineCommand* command = PushOutlineCommand((FontHarfbuzzOutlineContext*)draw_data, FONT_OUTLINE_CUBIC_TO);
    if (command)
    {
        command->m_Points[0] = { control_1_x, control_1_y };
        command->m_Points[1] = { control_2_x, control_2_y };
        command->m_Points[2] = { to_x, to_y };
    }
}

static void ClosePath(hb_draw_funcs_t*, void* draw_data, hb_draw_state_t*, void*)
{
    PushOutlineCommand((FontHarfbuzzOutlineContext*)draw_data, FONT_OUTLINE_CLOSE);
}

FontHarfbuzz* FontHarfbuzzCreate(const void* data, uint32_t data_size, uint32_t face_index)
{
    FontHarfbuzz* font = new FontHarfbuzz;
    memset(font, 0, sizeof(*font));

    hb_blob_t* blob = hb_blob_create((const char*)data, data_size, HB_MEMORY_MODE_READONLY, 0, 0);
    hb_face_t* face = blob ? hb_face_create(blob, face_index) : 0;
    font->m_Font = face ? hb_font_create(face) : 0;
    font->m_DrawFuncs = hb_draw_funcs_create();
    font->m_OutlineType = face ? GetOutlineType(face) : FONT_OUTLINE_TYPE_UNKNOWN;
    hb_face_destroy(face);
    hb_blob_destroy(blob);

    if (!font->m_Font || !font->m_DrawFuncs)
    {
        FontHarfbuzzDestroy(font);
        return 0;
    }

    hb_ot_font_set_funcs(font->m_Font);
    hb_draw_funcs_set_move_to_func(font->m_DrawFuncs, MoveTo, 0, 0);
    hb_draw_funcs_set_line_to_func(font->m_DrawFuncs, LineTo, 0, 0);
    hb_draw_funcs_set_quadratic_to_func(font->m_DrawFuncs, QuadraticTo, 0, 0);
    hb_draw_funcs_set_cubic_to_func(font->m_DrawFuncs, CubicTo, 0, 0);
    hb_draw_funcs_set_close_path_func(font->m_DrawFuncs, ClosePath, 0, 0);
    hb_draw_funcs_make_immutable(font->m_DrawFuncs);
    return font;
}

void FontHarfbuzzDestroy(FontHarfbuzz* font)
{
    if (!font)
        return;

    hb_draw_funcs_destroy(font->m_DrawFuncs);
    hb_font_destroy(font->m_Font);
    delete font;
}

FontResult FontHarfbuzzGetGlyphOutline(FontHarfbuzz* font, uint32_t glyph_index, FontOutline* outline)
{
    memset(outline, 0, sizeof(*outline));
    FontHarfbuzzOutlineContext context = { outline, 0, false };
    bool drawn = hb_font_draw_glyph_or_fail(font->m_Font, glyph_index, font->m_DrawFuncs, &context);
    if (!drawn || context.m_Failed)
    {
        FontFreeGlyphOutline(outline);
        return FONT_RESULT_ERROR;
    }

    return FONT_RESULT_OK;
}

uint32_t FontHarfbuzzGetGlyphIndex(FontHarfbuzz* font, uint32_t codepoint)
{
    hb_codepoint_t glyph_index = 0;
    hb_font_get_nominal_glyph(font->m_Font, codepoint, &glyph_index);
    return glyph_index;
}

float FontHarfbuzzGetScaleFromSize(FontHarfbuzz* font, uint32_t size)
{
    return (float)size / hb_face_get_upem(hb_font_get_face(font->m_Font));
}

bool FontHarfbuzzGetVerticalMetrics(FontHarfbuzz* font, int32_t* ascent, int32_t* descent, int32_t* line_gap)
{
    hb_font_extents_t extents;
    if (!hb_font_get_h_extents(font->m_Font, &extents))
        return false;

    *ascent = extents.ascender;
    *descent = extents.descender;
    *line_gap = extents.line_gap;
    return true;
}

void FontHarfbuzzGetGlyphHMetrics(FontHarfbuzz* font, uint32_t glyph_index, int32_t* advance, int32_t* left_bearing)
{
    *advance = hb_font_get_glyph_h_advance(font->m_Font, glyph_index);

    // HarfBuzz reports extents relative to the horizontal glyph origin, so
    // x_bearing is the horizontal left side bearing used by FontGlyph.
    hb_glyph_extents_t extents;
    *left_bearing = hb_font_get_glyph_extents(font->m_Font, glyph_index, &extents) ? extents.x_bearing : 0;
}

FontOutlineType FontHarfbuzzGetOutlineType(FontHarfbuzz* font)
{
    return font->m_OutlineType;
}

bool FontHarfbuzzGetGlyphBox(FontHarfbuzz* font, uint32_t glyph_index, int32_t* x0, int32_t* y0, int32_t* x1, int32_t* y1)
{
    hb_glyph_extents_t extents;
    if (!hb_font_get_glyph_extents(font->m_Font, glyph_index, &extents))
        return false;

    *x0 = extents.x_bearing;
    *y1 = extents.y_bearing;
    *x1 = extents.x_bearing + extents.width;
    *y0 = extents.y_bearing + extents.height;
    return true;
}

hb_font_t* FontHarfbuzzGetFont(FontHarfbuzz* font)
{
    return font->m_Font;
}
