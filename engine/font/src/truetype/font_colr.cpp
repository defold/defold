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

#include "font_colr.h"
#include <dlib/vmath.h>
#include <float.h>
#include <dmsdk/dlib/array.h>
#include <math.h>
#include <string.h>

// OpenType COLR/CPAL layouts:
// https://learn.microsoft.com/en-us/typography/opentype/spec/colr
// https://learn.microsoft.com/en-us/typography/opentype/spec/cpal
static uint16_t U16(const uint8_t* p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}
static int16_t S16(const uint8_t* p)
{
    return (int16_t)U16(p);
}
static uint32_t U24(const uint8_t* p)
{
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
}
static uint32_t U32(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static float F2Dot14(const uint8_t* p)
{
    return S16(p) / 16384.0f;
}
static float Fixed(const uint8_t* p)
{
    return (int32_t)U32(p) / 65536.0f;
}

static const uint8_t* Read(const FontColr* font, uint64_t offset, uint64_t size)
{
    return offset <= font->m_Size && size <= font->m_Size - offset ? font->m_Data + offset : 0;
}

static bool ReadList(const FontColr* font, uint32_t offset, uint32_t stride)
{
    if (!offset)
        return true;
    const uint8_t* list = Read(font, offset, 4);
    return list && Read(font, (uint64_t)offset + 4, (uint64_t)U32(list) * stride);
}

bool FontColrInitialize(FontColr* font, const void* colr, uint32_t size, const void* cpal, uint32_t cpal_size)
{
    memset(font, 0, sizeof(*font));
    if (!colr || size < 14 || !cpal || cpal_size < 14)
        return false;
    const uint8_t* p = (const uint8_t*)cpal;
    uint32_t       colors = U16(p + 2), palettes = U16(p + 4), records = U16(p + 6);
    uint32_t       color_offset = U32(p + 8), first = U16(p + 12);
    if (U16(p) > 1 || !palettes || palettes > (cpal_size - 12) / 2 ||
        first > records || colors > records - first || color_offset > cpal_size ||
        records > (cpal_size - color_offset) / 4)
        return false;
    FontColr view = {};
    view.m_Data = (const uint8_t*)colr;
    view.m_Size = size;
    view.m_Colors = p + color_offset + first * 4;
    view.m_ColorCount = colors;
    p = view.m_Data;
    uint32_t version = U16(p);
    if (version > 1 || (version == 1 && size < 34))
        return false;
    view.m_BaseGlyphCount = U16(p + 2);
    view.m_BaseGlyphs = U32(p + 4);
    view.m_Layers = U32(p + 8);
    view.m_LayerCount = U16(p + 12);
    if ((view.m_BaseGlyphCount && !view.m_BaseGlyphs) || (view.m_LayerCount && !view.m_Layers) ||
        !Read(&view, view.m_BaseGlyphs, view.m_BaseGlyphCount * 6) ||
        !Read(&view, view.m_Layers, view.m_LayerCount * 4))
        return false;
    if (version == 1)
    {
        view.m_BaseGlyphList = U32(p + 14);
        view.m_LayerList = U32(p + 18);
        view.m_ClipList = U32(p + 22);
        if (!ReadList(&view, view.m_BaseGlyphList, 6) || !ReadList(&view, view.m_LayerList, 4))
            return false;
        if (view.m_ClipList)
        {
            const uint8_t* clips = Read(&view, view.m_ClipList, 5);
            if (!clips || clips[0] != 1 || !Read(&view, (uint64_t)view.m_ClipList + 5, (uint64_t)U32(clips + 1) * 7))
                return false;
        }
    }
    *font = view;
    return true;
}

static const uint8_t* FindGlyph(const FontColr* font, uint32_t glyph, bool v1)
{
    if (!font->m_Data || glyph > 0xffff || (v1 && !font->m_BaseGlyphList))
        return 0;
    const uint8_t* records = font->m_Data + (v1 ? font->m_BaseGlyphList + 4 : font->m_BaseGlyphs);
    uint32_t       low = 0, high = v1 ? U32(font->m_Data + font->m_BaseGlyphList) : font->m_BaseGlyphCount;
    while (low < high)
    {
        uint32_t       mid = low + (high - low) / 2;
        const uint8_t* record = records + mid * 6;
        uint32_t       id = U16(record);
        if (id == glyph)
            return record;
        if (id < glyph)
            low = mid + 1;
        else
            high = mid;
    }
    return 0;
}

bool FontColrHasGlyph(const FontColr* font, uint32_t glyph)
{
    return FindGlyph(font, glyph, true) || FindGlyph(font, glyph, false);
}

static FontResult ClipBox(const FontColr* font, uint32_t glyph, const uint8_t** box)
{
    *box = 0;
    if (!font->m_ClipList)
        return FONT_RESULT_OK;
    const uint8_t* list = font->m_Data + font->m_ClipList;
    uint32_t       low = 0, high = U32(list + 1);
    while (low < high)
    {
        uint32_t       mid = low + (high - low) / 2;
        const uint8_t* record = list + 5 + mid * 7;
        if (glyph < U16(record))
            high = mid;
        else if (glyph > U16(record + 2))
            low = mid + 1;
        else
        {
            uint32_t       relative = U24(record + 4);
            const uint8_t* clip = relative ? Read(font, (uint64_t)font->m_ClipList + relative, 9) : 0;
            if (!clip)
                return FONT_RESULT_ERROR;
            if (clip[0] != 1)
                return FONT_RESULT_NOT_SUPPORTED;
            if (S16(clip + 1) > S16(clip + 5) || S16(clip + 3) > S16(clip + 7))
                return FONT_RESULT_ERROR;
            *box = clip;
            return FONT_RESULT_OK;
        }
    }
    return FONT_RESULT_OK;
}

struct PaintContext
{
    const FontColr*          m_Font;
    const FontColrCallbacks* m_Callbacks;
    void*                    m_Context;
    uint32_t                 m_WorkLeft;
};

static bool Color(const FontColr* font, uint32_t palette, float alpha, FontColrStop* stop)
{
    stop->m_Foreground = palette == 0xffff;
    alpha = fminf(1.0f, fmaxf(0.0f, alpha));
    if (stop->m_Foreground)
    {
        stop->m_Color[0] = stop->m_Color[1] = stop->m_Color[2] = 1;
        stop->m_Color[3] = alpha;
        return true;
    }
    if (palette >= font->m_ColorCount)
        return false;
    const uint8_t* p = font->m_Colors + palette * 4;
    stop->m_Color[0] = p[2] / 255.0f;
    stop->m_Color[1] = p[1] / 255.0f;
    stop->m_Color[2] = p[0] / 255.0f;
    stop->m_Color[3] = p[3] / 255.0f * alpha;
    return true;
}

static FontResult Solid(PaintContext* c, uint32_t palette, float alpha)
{
    FontColrStop stop = {};
    if (!Color(c->m_Font, palette, alpha, &stop))
        return FONT_RESULT_ERROR;
    FontColrPaint paint = {};
    paint.m_Type = FONT_COLR_SOLID;
    paint.m_Stops = &stop;
    paint.m_StopCount = 1;
    return c->m_Callbacks->m_Paint(c->m_Context, &paint) ? FONT_RESULT_OK : FONT_RESULT_NOT_SUPPORTED;
}

static FontResult Paint(PaintContext* c, uint64_t offset, uint32_t depth);
static FontResult Glyph(PaintContext* c, uint32_t glyph, uint32_t depth);

static FontResult Gradient(PaintContext* c, uint64_t offset, const uint8_t* p)
{
    uint32_t       relative = U24(p + 1);
    const uint8_t* line = relative ? Read(c->m_Font, offset + relative, 3) : 0;
    if (!line || line[0] > 2)
        return FONT_RESULT_ERROR;
    uint32_t       count = U16(line + 1);
    const uint8_t* records = Read(c->m_Font, offset + relative + 3, count * 6);
    if (!records || count > c->m_WorkLeft)
        return FONT_RESULT_ERROR;
    c->m_WorkLeft -= count;
    if (!count)
        return FONT_RESULT_OK; // Empty color lines are transparent.
    dmArray<FontColrStop> stops;
    stops.SetCapacity(count);
    stops.SetSize(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint8_t* record = records + i * 6;
        FontColrStop   stop = {};
        stop.m_Offset = F2Dot14(record);
        if (!Color(c->m_Font, U16(record + 2), F2Dot14(record + 4), &stop))
            return FONT_RESULT_ERROR;
        // Stable insertion preserves the specified ordering of coincident
        // stops. Charge comparisons to the traversal budget as well as nodes.
        uint32_t j = i;
        while (j && stops[j - 1].m_Offset > stop.m_Offset)
        {
            if (!c->m_WorkLeft)
                return FONT_RESULT_ERROR;
            --c->m_WorkLeft;
            stops[j] = stops[j - 1];
            --j;
        }
        stops[j] = stop;
    }
    if (line[0] && stops[0].m_Offset == stops[count - 1].m_Offset)
        return FONT_RESULT_OK; // Degenerate repeat/reflect gradients paint nothing.
    FontColrPaint paint = {};
    paint.m_Type = p[0] == 4 ? FONT_COLR_LINEAR_GRADIENT : FONT_COLR_RADIAL_GRADIENT;
    paint.m_Extend = line[0];
    paint.m_Stops = stops.Begin();
    paint.m_StopCount = count;
    for (uint32_t i = 0; i < 6; ++i)
        paint.m_Points[i] = p[0] == 6 && (i == 2 || i == 5) ? U16(p + 4 + i * 2) : S16(p + 4 + i * 2);
    return c->m_Callbacks->m_Paint(c->m_Context, &paint) ? FONT_RESULT_OK : FONT_RESULT_NOT_SUPPORTED;
}

static FontResult Transform(PaintContext* c, uint64_t offset, const uint8_t* p, uint32_t depth)
{
    uint32_t format = p[0];
    float    m[6] = { 1, 0, 0, 1, 0, 0 };
    float    cx = 0, cy = 0;
    if (format == 12)
    {
        uint32_t       relative = U24(p + 4);
        const uint8_t* matrix = relative ? Read(c->m_Font, offset + relative, 24) : 0;
        if (!matrix)
            return FONT_RESULT_ERROR;
        for (uint32_t i = 0; i < 6; ++i)
            m[i] = Fixed(matrix + i * 4);
    }
    else if (format == 14)
    {
        m[4] = S16(p + 4);
        m[5] = S16(p + 6);
    }
    else if (format <= 22)
    {
        m[0] = F2Dot14(p + 4);
        m[3] = format <= 18 ? F2Dot14(p + 6) : m[0];
        if (format == 18)
        {
            cx = S16(p + 8);
            cy = S16(p + 10);
        }
        if (format == 22)
        {
            cx = S16(p + 6);
            cy = S16(p + 8);
        }
    }
    else if (format <= 26)
    {
        float angle = F2Dot14(p + 4) * 3.14159265358979323846f;
        m[0] = m[3] = cosf(angle);
        m[1] = sinf(angle);
        m[2] = -m[1];
        if (format == 26)
        {
            cx = S16(p + 6);
            cy = S16(p + 8);
        }
    }
    else
    {
        m[1] = tanf(F2Dot14(p + 6) * 3.14159265358979323846f);
        m[2] = -tanf(F2Dot14(p + 4) * 3.14159265358979323846f);
        if (format == 30)
        {
            cx = S16(p + 8);
            cy = S16(p + 10);
        }
    }
    m[4] += cx - m[0] * cx - m[2] * cy;
    m[5] += cy - m[1] * cx - m[3] * cy;
    uint32_t child = U24(p + 1);
    if (!child)
        return FONT_RESULT_ERROR;
    bool       ok = c->m_Callbacks->m_PushTransform(c->m_Context, m);
    FontResult result = ok ? Paint(c, offset + child, depth + 1) : FONT_RESULT_NOT_SUPPORTED;
    c->m_Callbacks->m_PopTransform(c->m_Context);
    return result;
}

static FontResult Paint(PaintContext* c, uint64_t offset, uint32_t depth)
{
    if (depth >= 64 || !c->m_WorkLeft)
        return FONT_RESULT_ERROR;
    --c->m_WorkLeft;
    const uint8_t* p = Read(c->m_Font, offset, 1);
    if (!p)
        return FONT_RESULT_ERROR;
    uint32_t format = p[0];
    // Zero entries represent paints whose semantics are not supported here.
    static const uint8_t sizes[] = { 0, 6, 5, 0, 16, 0, 16, 0, 0, 0, 6, 3, 7, 0, 8, 0, 8, 0, 12, 0, 6, 0, 10, 0, 6, 0, 10, 0, 8, 0, 12, 0, 8 };
    if (format >= sizeof(sizes) || !sizes[format])
        return FONT_RESULT_NOT_SUPPORTED;
    if (!Read(c->m_Font, offset, sizes[format]))
        return FONT_RESULT_ERROR;
    if (format == 1)
    {
        uint32_t list = c->m_Font->m_LayerList;
        if (!list)
            return FONT_RESULT_ERROR;
        uint32_t first = U32(p + 2), count = p[1];
        uint32_t total = U32(c->m_Font->m_Data + list);
        if (first > total || count > total - first)
            return FONT_RESULT_ERROR;
        for (uint32_t i = 0; i < count; ++i)
        {
            uint32_t relative = U32(c->m_Font->m_Data + list + 4 + (first + i) * 4);
            if (!relative)
                return FONT_RESULT_ERROR;
            FontResult result = Paint(c, (uint64_t)list + relative, depth + 1);
            if (result != FONT_RESULT_OK)
                return result;
        }
        return FONT_RESULT_OK;
    }
    if (format == 2)
        return Solid(c, U16(p + 1), F2Dot14(p + 3));
    if (format == 4 || format == 6)
        return Gradient(c, offset, p);
    if (format == 10)
    {
        uint32_t child = U24(p + 1);
        if (!child)
            return FONT_RESULT_ERROR;
        bool       ok = c->m_Callbacks->m_PushClipGlyph(c->m_Context, U16(p + 4));
        FontResult result = ok ? Paint(c, offset + child, depth + 1) : FONT_RESULT_NOT_SUPPORTED;
        c->m_Callbacks->m_PopClip(c->m_Context);
        return result;
    }
    if (format == 11)
    {
        if (!FindGlyph(c->m_Font, U16(p + 1), true))
            return FONT_RESULT_ERROR;
        return Glyph(c, U16(p + 1), depth + 1);
    }
    if (format == 32)
    {
        if (p[4] != 3)
            return FONT_RESULT_NOT_SUPPORTED; // SRC_OVER
        uint32_t source = U24(p + 1), backdrop = U24(p + 5);
        if (!source || !backdrop)
            return FONT_RESULT_ERROR;
        bool       ok = c->m_Callbacks->m_PushComposite(c->m_Context);
        FontResult result = ok ? Paint(c, offset + backdrop, depth + 1) : FONT_RESULT_NOT_SUPPORTED;
        if (result == FONT_RESULT_OK)
            result = Paint(c, offset + source, depth + 1);
        c->m_Callbacks->m_PopComposite(c->m_Context);
        return result;
    }
    return Transform(c, offset, p, depth);
}

static FontResult Glyph(PaintContext* c, uint32_t glyph, uint32_t depth)
{
    if (depth >= 64)
        return FONT_RESULT_ERROR;
    const FontColr* font = c->m_Font;
    const uint8_t*  record = FindGlyph(font, glyph, true);
    if (record)
    {
        const uint8_t* box;
        FontResult     result = ClipBox(font, glyph, &box);
        if (result != FONT_RESULT_OK)
            return result;
        uint32_t relative = U32(record + 2);
        if (!relative)
            return FONT_RESULT_ERROR;
        bool ok = !box || c->m_Callbacks->m_PushClipRectangle(c->m_Context, S16(box + 1), S16(box + 3), S16(box + 5), S16(box + 7));
        result = ok ? Paint(c, (uint64_t)font->m_BaseGlyphList + relative, depth) : FONT_RESULT_NOT_SUPPORTED;
        if (box)
            c->m_Callbacks->m_PopClip(c->m_Context);
        return result;
    }
    record = FindGlyph(font, glyph, false);
    if (!record)
        return FONT_RESULT_NOT_SUPPORTED;
    uint32_t first = U16(record + 2), count = U16(record + 4);
    if (first > font->m_LayerCount || count > font->m_LayerCount - first || count > c->m_WorkLeft)
        return FONT_RESULT_ERROR;
    c->m_WorkLeft -= count;
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint8_t* layer = font->m_Data + font->m_Layers + (first + i) * 4;
        bool           ok = c->m_Callbacks->m_PushClipGlyph(c->m_Context, U16(layer));
        FontResult     result = ok ? Solid(c, U16(layer + 2), 1) : FONT_RESULT_NOT_SUPPORTED;
        c->m_Callbacks->m_PopClip(c->m_Context);
        if (result != FONT_RESULT_OK)
            return result;
    }
    return FONT_RESULT_OK;
}

FontResult FontColrPaintGlyph(const FontColr* font, uint32_t glyph, const FontColrCallbacks* callbacks, void* context)
{
    PaintContext paint = { font, callbacks, context, 100000 };
    return Glyph(&paint, glyph, 0);
}

// Clip rectangles and raw outline boxes are transformed before intersection; paints
// contribute only their current clip, so transforms below PaintGlyph cannot
// move its outline.
struct BoundsState
{
    dmVMath::Matrix4 m_Transform;
    dmVMath::Vector4 m_Clip;
};

struct BoundsContext
{
    FontColrGlyphBoxCallback m_GetBox;
    void*                    m_Context;
    dmArray<BoundsState>     m_Stack;
    dmVMath::Vector4         m_Bounds;
};

static void PushBounds(BoundsContext* c, const BoundsState& state)
{
    if (c->m_Stack.Full())
        c->m_Stack.OffsetCapacity(16);
    c->m_Stack.Push(state);
}

static bool BoundsTransform(void* context, const float m[6])
{
    BoundsContext* c = (BoundsContext*)context;
    BoundsState    state = c->m_Stack.Back();
    using dmVMath::Vector4;
    state.m_Transform = state.m_Transform * dmVMath::Matrix4(Vector4(m[0], m[1], 0, 0), Vector4(m[2], m[3], 0, 0), Vector4(0, 0, 1, 0), Vector4(m[4], m[5], 0, 1));
    PushBounds(c, state);
    return true;
}

static void BoundsPop(void* context)
{
    ((BoundsContext*)context)->m_Stack.Pop();
}

static bool BoundsRectangle(void* context, float x0, float y0, float x1, float y1)
{
    BoundsContext* c = (BoundsContext*)context;
    BoundsState    state = c->m_Stack.Back();
    using dmVMath::Vector4;
    Vector4 bounds(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (uint32_t i = 0; i < 4; ++i)
    {
        Vector4 p = state.m_Transform * Vector4(i & 1 ? x1 : x0, i & 2 ? y1 : y0, 0, 1);
        bounds = Vector4(fminf(bounds.getX(), p.getX()), fminf(bounds.getY(), p.getY()), fmaxf(bounds.getZ(), p.getX()), fmaxf(bounds.getW(), p.getY()));
    }
    state.m_Clip = Vector4(fmaxf(state.m_Clip.getX(), bounds.getX()), fmaxf(state.m_Clip.getY(), bounds.getY()), fminf(state.m_Clip.getZ(), bounds.getZ()), fminf(state.m_Clip.getW(), bounds.getW()));
    PushBounds(c, state);
    return true;
}

static bool BoundsGlyph(void* context, uint32_t glyph)
{
    BoundsContext* c = (BoundsContext*)context;
    int32_t        x0, y0, x1, y1;
    if (c->m_GetBox(c->m_Context, glyph, &x0, &y0, &x1, &y1))
        return BoundsRectangle(context, x0, y0, x1, y1);
    BoundsState state = c->m_Stack.Back();
    state.m_Clip = dmVMath::Vector4(0, 0, 0, 0);
    PushBounds(c, state);
    return true;
}

static bool BoundsComposite(void*)
{
    return true;
}
static void BoundsEndComposite(void*) {}

static bool BoundsPaint(void* context, const FontColrPaint* paint)
{
    BoundsContext*          c = (BoundsContext*)context;
    const dmVMath::Vector4& clip = c->m_Stack.Back().m_Clip;
    if (clip.getX() >= clip.getZ() || clip.getY() >= clip.getW())
        return true;
    for (uint32_t i = 0; i < paint->m_StopCount; ++i)
    {
        if (paint->m_Stops[i].m_Color[3] > 0)
        {
            const dmVMath::Vector4& b = c->m_Bounds;
            c->m_Bounds = dmVMath::Vector4(fminf(b.getX(), clip.getX()), fminf(b.getY(), clip.getY()), fmaxf(b.getZ(), clip.getZ()), fmaxf(b.getW(), clip.getW()));
            break;
        }
    }
    return true;
}

bool FontColrGetGlyphBox(const FontColr* font, uint32_t glyph, FontColrGlyphBoxCallback get_box, void* context, int32_t* x0, int32_t* y0, int32_t* x1, int32_t* y1)
{
    const uint8_t* record = FindGlyph(font, glyph, true);
    if (!record && !FindGlyph(font, glyph, false))
        return false;
    const uint8_t* box = 0;
    if (record && ClipBox(font, glyph, &box) != FONT_RESULT_OK)
        return false;
    if (box)
    {
        *x0 = S16(box + 1);
        *y0 = S16(box + 3);
        *x1 = S16(box + 5);
        *y1 = S16(box + 7);
        return true;
    }
    BoundsContext c;
    c.m_GetBox = get_box;
    c.m_Context = context;
    c.m_Bounds = dmVMath::Vector4(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
    BoundsState initial = { dmVMath::Matrix4::identity(), dmVMath::Vector4(-FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX) };
    PushBounds(&c, initial);
    const FontColrCallbacks callbacks = { BoundsTransform, BoundsPop, BoundsGlyph, BoundsRectangle, BoundsPop, BoundsComposite, BoundsEndComposite, BoundsPaint };
    if (FontColrPaintGlyph(font, glyph, &callbacks, &c) != FONT_RESULT_OK || c.m_Bounds.getX() >= c.m_Bounds.getZ() || c.m_Bounds.getY() >= c.m_Bounds.getW())
        return false;
    // Extreme/malformed transforms must not overflow the integer metrics API.
    for (uint32_t i = 0; i < 4; ++i)
        if (!isfinite(c.m_Bounds[i]) || c.m_Bounds[i] < -2147483648.0 || c.m_Bounds[i] >= 2147483648.0)
            return false;
    *x0 = (int32_t)floorf(c.m_Bounds.getX());
    *y0 = (int32_t)floorf(c.m_Bounds.getY());
    *x1 = (int32_t)ceilf(c.m_Bounds.getZ());
    *y1 = (int32_t)ceilf(c.m_Bounds.getW());
    return true;
}
