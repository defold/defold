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

#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ot.h>

#include <math.h>

#include "font.h"
#include "font_harfbuzz.h"
#include "truetype/font_truetype.h"
#include "truetype/font_colr.h"

struct FontHarfbuzz
{
    hb_font_t*    m_Font;
    FontTrueType* m_TrueType; // Owned by m_Font's callback data, including retained Skribidi references.
};

static void DestroyTrueType(void* data)
{
    FontTrueTypeDestroy((FontTrueType*)data);
}

static bool GetOutlineBox(void* data, uint32_t glyph, int32_t* x0, int32_t* y0, int32_t* x1, int32_t* y1)
{
    return FontTrueTypeGetGlyphBox((FontTrueType*)data, glyph, x0, y0, x1, y1);
}

static hb_bool_t GetGlyphExtents(hb_font_t* font, void* data, hb_codepoint_t glyph, hb_glyph_extents_t* extents, void*)
{
    int32_t       x0, y0, x1, y1;
    FontTrueType* true_type = (FontTrueType*)data;
    // Shaping uses the color glyph's extents; PaintGlyph uses only its raw outline.
    if (!FontColrGetGlyphBox(FontTrueTypeGetColr(true_type), glyph, GetOutlineBox, true_type, &x0, &y0, &x1, &y1) &&
        !FontTrueTypeGetGlyphBox(true_type, glyph, &x0, &y0, &x1, &y1))
        return false;

    int x_scale, y_scale;
    hb_font_get_scale(font, &x_scale, &y_scale);
    float upem = (float)hb_face_get_upem(hb_font_get_face(font));
    float sx = x_scale / upem, sy = y_scale / upem;
    extents->x_bearing = (int32_t)roundf(x0 * sx);
    extents->y_bearing = (int32_t)roundf(y1 * sy);
    extents->width = (int32_t)roundf(x1 * sx) - extents->x_bearing;
    extents->height = (int32_t)roundf(y0 * sy) - extents->y_bearing;
    return true;
}

FontHarfbuzz* FontHarfbuzzCreate(const void* data, uint32_t data_size, uint32_t face_index)
{
    // HarfBuzz owns cmap selection, including encodings our outline decoder does not map.
    FontTrueType* true_type = FontTrueTypeCreate(data, data_size, face_index, false);
    if (!true_type)
        return 0;

    hb_blob_t* blob = hb_blob_create((const char*)data, data_size, HB_MEMORY_MODE_READONLY, 0, 0);
    hb_face_t* face = hb_face_create(blob, face_index);
    hb_font_t* parent = hb_font_create(face);
    hb_ot_font_set_funcs(parent);

    // Delegate mapping, advances and OpenType metrics to the parent; only
    // outline bounds come from our decoder.
    hb_font_t*       child = hb_font_create_sub_font(parent);
    hb_font_funcs_t* funcs = hb_font_funcs_create();
    hb_font_funcs_set_glyph_extents_func(funcs, GetGlyphExtents, 0, 0);
    hb_font_funcs_make_immutable(funcs);
    hb_font_set_funcs(child, funcs, true_type, DestroyTrueType);
    hb_font_funcs_destroy(funcs);
    hb_font_destroy(parent);
    hb_face_destroy(face);
    hb_blob_destroy(blob);

    FontHarfbuzz* font = new FontHarfbuzz;
    font->m_Font = child;
    font->m_TrueType = true_type;
    return font;
}

void FontHarfbuzzDestroy(FontHarfbuzz* font)
{
    if (!font)
        return;
    hb_font_destroy(font->m_Font);
    delete font;
}

FontResult FontHarfbuzzGetGlyphOutline(FontHarfbuzz* font, uint32_t glyph_index, FontOutline* outline)
{
    return FontTrueTypeGetGlyphOutline(font->m_TrueType, glyph_index, outline);
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
    return FontTrueTypeGetOutlineType(font->m_TrueType);
}

bool FontHarfbuzzGetGlyphBox(FontHarfbuzz* font, uint32_t glyph_index, int32_t* x0, int32_t* y0, int32_t* x1, int32_t* y1)
{
    return FontTrueTypeGetGlyphBox(font->m_TrueType, glyph_index, x0, y0, x1, y1);
}

hb_font_t* FontHarfbuzzGetFont(FontHarfbuzz* font)
{
    return font->m_Font;
}

const FontColr* FontHarfbuzzGetColr(FontHarfbuzz* font)
{
    return FontTrueTypeGetColr(font->m_TrueType);
}
