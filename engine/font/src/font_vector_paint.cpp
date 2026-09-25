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

#include "font_vector_paint.hpp"
#include "font_ttf.h"
#include "truetype/font_colr.h"
#include <dlib/hash.h>
#include <dlib/log.h>
#include <float.h>
#include <math.h>
#include <string.h>

using dmVMath::Matrix4;
using dmVMath::Vector4;

struct PaintClip
{
    uint32_t m_Path;
    Matrix4  m_Transform;
    Vector4  m_Rect;
};

struct PaintContext
{
    FontVectorPaintCache* m_Cache;
    HFont                 m_Font;
    dmArray<Matrix4>      m_Transforms;
    dmArray<PaintClip>    m_Clips;
    bool                  m_Failed;
};

template <typename T>
static void Append(dmArray<T>& array, const T& value)
{
    if (array.Full())
        array.OffsetCapacity(32);
    array.Push(value);
}

static Vector4 Intersect(const Vector4& a, const Vector4& b)
{
    return Vector4(fmaxf(a.getX(), b.getX()), fmaxf(a.getY(), b.getY()), fminf(a.getZ(), b.getZ()), fminf(a.getW(), b.getW()));
}

static Matrix4& CurrentTransform(PaintContext* context)
{
    return context->m_Transforms[context->m_Transforms.Size() - 1];
}

static PaintClip& CurrentClip(PaintContext* context)
{
    return context->m_Clips[context->m_Clips.Size() - 1];
}

static bool PushTransform(void* data, const float m[6])
{
    PaintContext* context = (PaintContext*)data;
    Matrix4       transform(Vector4(m[0], m[1], 0, 0), Vector4(m[2], m[3], 0, 0), Vector4(0, 0, 1, 0), Vector4(m[4], m[5], 0, 1));
    Append(context->m_Transforms, CurrentTransform(context) * transform);
    return true;
}

static void PopTransform(void* data)
{
    PaintContext* context = (PaintContext*)data;
    context->m_Transforms.SetSize(context->m_Transforms.Size() - 1);
}

static uint64_t GlyphKey(HFont font, uint32_t glyph)
{
    uint64_t key[2] = { (uint64_t)(uintptr_t)font, glyph };
    return dmHashBuffer64(key, sizeof(key));
}

static void Put(dmHashTable64<uint32_t>& table, uint64_t key, uint32_t value)
{
    if (table.Full())
        table.SetCapacity((table.Capacity() + 32) * 2, table.Capacity() + 32);
    table.Put(key, value);
}

static uint32_t AddPath(PaintContext* context, uint32_t glyph_index)
{
    FontVectorPaintCache* cache = context->m_Cache;
    uint64_t              key = GlyphKey(context->m_Font, glyph_index);
    uint32_t*             existing = cache->m_PathLookup.Get(key);
    if (existing)
        return *existing;
    FontGlyphOptions options;
    options.m_GenerateOutline = true;
    FontGlyph glyph = {};
    if (FontGetGlyphByIndex(context->m_Font, glyph_index, &options, &glyph) != FONT_RESULT_OK)
    {
        context->m_Failed = true;
        return ~0u;
    }
    FontVectorPaintPath path;
    path.m_Font = context->m_Font;
    path.m_Glyph = glyph_index;
    const FontGlyphOutline& outline = glyph.m_Outline;
    path.m_Bounds = Vector4(outline.m_LeftBearing, -outline.m_Descent, outline.m_LeftBearing + outline.m_Width, outline.m_Ascent);
    memset(&path.m_Slug, 0, sizeof(path.m_Slug));
    bool ok = !outline.m_CommandCount || FontVectorSlugAddFontGlyph(cache->m_Slug, glyph, 8, &path.m_Slug);
    FontFreeGlyph(context->m_Font, &glyph);
    if (!ok)
    {
        context->m_Failed = true;
        return ~0u;
    }
    Append(cache->m_Paths, path);
    uint32_t index = cache->m_Paths.Size() - 1;
    Put(cache->m_PathLookup, key, index);
    return index;
}

static bool PushClipGlyph(void* data, uint32_t glyph)
{
    PaintContext* context = (PaintContext*)data;
    PaintClip     clip = CurrentClip(context);
    // Each supported layer clips one outline. Do not silently approximate
    // arbitrary intersections of glyph outlines as their bounding boxes.
    if (clip.m_Path != ~0u)
        context->m_Failed = true;
    clip.m_Path = AddPath(context, glyph);
    clip.m_Transform = CurrentTransform(context);
    Append(context->m_Clips, clip);
    return !context->m_Failed;
}

static bool PushClipRectangle(void* data, float x0, float y0, float x1, float y1)
{
    PaintContext*  context = (PaintContext*)data;
    const Matrix4& transform = CurrentTransform(context);
    PaintClip      clip = CurrentClip(context);
    if (transform.getCol0().getY() != 0 || transform.getCol1().getX() != 0)
        context->m_Failed = true;
    Vector4 a = transform * Vector4(x0, y0, 0, 1);
    Vector4 b = transform * Vector4(x1, y1, 0, 1);
    clip.m_Rect = Intersect(clip.m_Rect, Vector4(fminf(a.getX(), b.getX()), fminf(a.getY(), b.getY()), fmaxf(a.getX(), b.getX()), fmaxf(a.getY(), b.getY())));
    Append(context->m_Clips, clip);
    return !context->m_Failed;
}

static void PopClip(void* data)
{
    PaintContext* context = (PaintContext*)data;
    context->m_Clips.SetSize(context->m_Clips.Size() - 1);
}

static void AppendStop(FontVectorPaintCache* cache, const FontColrStop& stop)
{
    Append(cache->m_Paints, Vector4(stop.m_Color[0], stop.m_Color[1], stop.m_Color[2], stop.m_Color[3]));
    Append(cache->m_Paints, Vector4(stop.m_Offset, stop.m_Foreground ? 1.0f : 0.0f, 0, 0));
}

static bool AddPaint(PaintContext* context, uint32_t type, uint32_t stops, uint32_t extend, const Vector4& p0, const Vector4& p1)
{
    if (context->m_Failed)
        return false;
    const PaintClip& clip = CurrentClip(context);
    if (clip.m_Path == ~0u || stops == 0)
    {
        context->m_Failed = true;
        return false;
    }
    FontVectorPaintCache* cache = context->m_Cache;
    if (!cache->m_Paths[clip.m_Path].m_Slug.m_CurveCount)
        return false;
    const Matrix4& paint_transform = CurrentTransform(context);
    if (fabsf(determinant(paint_transform)) < 1e-12f)
        return false;
    if (3 + stops * 2 > cache->m_Slug->m_MaxTexels - cache->m_Paints.Size())
    {
        cache->m_Slug->m_Overflow = true;
        context->m_Failed = true;
        return false;
    }
    FontVectorPaintLayer layer;
    layer.m_Path = clip.m_Path;
    layer.m_Paint = cache->m_Paints.Size();
    layer.m_Transform = clip.m_Transform;
    layer.m_PaintTransform = inverse(paint_transform) * clip.m_Transform;
    layer.m_Clip = clip.m_Rect;
    Append(cache->m_Layers, layer);
    Append(cache->m_Paints, Vector4((float)type, (float)stops, (float)extend, 0));
    Append(cache->m_Paints, p0);
    Append(cache->m_Paints, p1);
    return true;
}

static bool PaintColor(void* data, const FontColrPaint* paint)
{
    PaintContext* context = (PaintContext*)data;
    const float*  p = paint->m_Points;
    Vector4       p0(0), p1(0);
    if (paint->m_Type == FONT_COLR_LINEAR_GRADIENT)
    {
        // The color line is perpendicular to p0->p2, and reaches 1 at p1.
        float dx = p[5] - p[1], dy = p[0] - p[4];
        float denominator = dx * (p[2] - p[0]) + dy * (p[3] - p[1]);
        if (denominator == 0)
            return true;
        p0 = Vector4(dx / denominator, dy / denominator, p[0], p[1]);
    }
    else if (paint->m_Type == FONT_COLR_RADIAL_GRADIENT)
    {
        p0 = Vector4(p[0], p[1], p[2], 0);
        p1 = Vector4(p[3], p[4], p[5], 0);
    }
    if (AddPaint(context, paint->m_Type, paint->m_StopCount, paint->m_Extend, p0, p1))
    {
        for (uint32_t i = 0; i < paint->m_StopCount; ++i)
            AppendStop(context->m_Cache, paint->m_Stops[i]);
    }
    return !context->m_Failed;
}

static const FontColrCallbacks COLR_CALLBACKS = {
    PushTransform,
    PopTransform,
    PushClipGlyph,
    PushClipRectangle,
    PopClip,
    PaintColor
};

void FontVectorPaintClear(FontVectorPaintCache* cache)
{
    cache->m_Paths.SetSize(0);
    cache->m_Layers.SetSize(0);
    cache->m_Glyphs.SetSize(0);
    cache->m_Paints.SetSize(0);
    cache->m_PathLookup.Clear();
    cache->m_GlyphLookup.Clear();
}

bool FontVectorPaintHasGlyph(HFont font, uint32_t glyph)
{
    return (FontGetType(font) == FONT_TYPE_TTF || FontGetType(font) == FONT_TYPE_OTF) &&
    FontColrHasGlyph(FontGetColrTTF(font), glyph);
}

bool FontVectorPaintAddGlyph(FontVectorPaintCache* cache, HFont source, uint32_t glyph_index, uint32_t* index, bool* has_color)
{
    *index = ~0u;
    *has_color = false;
    if (FontGetType(source) != FONT_TYPE_TTF && FontGetType(source) != FONT_TYPE_OTF)
        return true;
    const FontColr* colr = FontGetColrTTF(source);
    *has_color = FontColrHasGlyph(colr, glyph_index);
    uint64_t  key = GlyphKey(source, glyph_index);
    uint32_t* existing = cache->m_GlyphLookup.Get(key);
    if (existing)
    {
        *index = *existing;
        return true;
    }
    FontVectorPaintGlyph glyph = { source, glyph_index, cache->m_Layers.Size(), 0 };
    PaintContext         context;
    context.m_Cache = cache;
    context.m_Font = source;
    context.m_Failed = false;
    Append(context.m_Transforms, Matrix4::identity());
    PaintClip clip = { ~0u, Matrix4::identity(), Vector4(-FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX) };
    Append(context.m_Clips, clip);
    uint32_t first_paint = cache->m_Paints.Size();
    uint32_t first_path = cache->m_Paths.Size();
    uint32_t first_curve = cache->m_Slug->m_Curves.Size();
    uint32_t first_band = cache->m_Slug->m_Bands.Size();
    uint32_t counters[] = { cache->m_Slug->m_CurveTexels, cache->m_Slug->m_BandTexels, cache->m_Slug->m_CurveCount, cache->m_Slug->m_BandReferences, cache->m_Slug->m_MaxBandCurves };
    bool     painted = *has_color && FontColrPaintGlyph(colr, glyph_index, &COLR_CALLBACKS, &context) == FONT_RESULT_OK;
    if (!painted || context.m_Failed)
    {
        // Discard the whole partial graph before falling back. It must never
        // leave colored layers behind a replacement or poison unrelated text.
        cache->m_Layers.SetSize(glyph.m_FirstLayer);
        cache->m_Paints.SetSize(first_paint);
        for (uint32_t i = first_path; i < cache->m_Paths.Size(); ++i)
            cache->m_PathLookup.Erase(GlyphKey(source, cache->m_Paths[i].m_Glyph));
        cache->m_Paths.SetSize(first_path);
        cache->m_Slug->m_Curves.SetSize(first_curve);
        cache->m_Slug->m_Bands.SetSize(first_band);
        cache->m_Slug->m_CurveTexels = counters[0];
        cache->m_Slug->m_BandTexels = counters[1];
        cache->m_Slug->m_CurveCount = counters[2];
        cache->m_Slug->m_BandReferences = counters[3];
        cache->m_Slug->m_MaxBandCurves = counters[4];
        if (cache->m_Slug->m_Overflow)
            return false;
        context.m_Failed = false;
        uint32_t path = AddPath(&context, glyph_index);
        if (path == ~0u)
            return false;
        // Color-only base glyphs have no monochrome outline. Use .notdef.
        if (*has_color && !cache->m_Paths[path].m_Slug.m_CurveCount)
            glyph_index = 0;
        PushClipGlyph(&context, glyph_index);
        FontColrStop  stop = { 0, { 1, 1, 1, 1 }, true };
        FontColrPaint paint = {};
        paint.m_Type = FONT_COLR_SOLID;
        paint.m_Stops = &stop;
        paint.m_StopCount = 1;
        PaintColor(&context, &paint);
        PopClip(&context);
        if (context.m_Failed)
            return false;
        if (*has_color)
            dmLogWarning("Unsupported COLR paint in glyph %u of %s; using an outline fallback", glyph.m_Glyph, FontGetPath(source));
    }
    glyph.m_LayerCount = cache->m_Layers.Size() - glyph.m_FirstLayer;
    Append(cache->m_Glyphs, glyph);
    *index = cache->m_Glyphs.Size() - 1;
    Put(cache->m_GlyphLookup, key, *index);
    return true;
}

void FontVectorPaintPackLayer(const FontVectorPaintCache* cache, uint32_t layer_index, float scale, float pixel_scale, float x, float y, const Matrix4& transform, const Vector4& clip, const TextGlyphFaceColors& colors, FontVectorPaintVertex vertices[6])
{
    static const uint32_t       corners[] = { 0, 1, 2, 2, 1, 3 };
    const float*                rgba[] = { colors.m_BottomLeft, colors.m_BottomRight, colors.m_TopLeft, colors.m_TopRight };
    const FontVectorPaintLayer& layer = cache->m_Layers[layer_index];
    const FontVectorPaintPath&  path = cache->m_Paths[layer.m_Path];
    const float                 width = path.m_Bounds.getZ() - path.m_Bounds.getX();
    const float                 height = path.m_Bounds.getW() - path.m_Bounds.getY();
    // Map a two-pixel AA margin back through the layer transform, including
    // preview zoom, so small or transformed layers keep their edge coverage.
    const float xx = layer.m_Transform.getCol0().getX();
    const float yx = layer.m_Transform.getCol0().getY();
    const float xy = layer.m_Transform.getCol1().getX();
    const float yy = layer.m_Transform.getCol1().getY();
    const float determinant = fabsf(xx * yy - xy * yx);
    if (determinant < 1e-12f)
    {
        memset(vertices, 0, 6 * sizeof(*vertices));
        return;
    }
    const float padding = 2.0f / (pixel_scale * determinant);
    const float padding_x = padding * (fabsf(yy) + fabsf(xy));
    const float padding_y = padding * (fabsf(xx) + fabsf(yx));
    for (uint32_t v = 0; v < 6; ++v)
    {
        uint32_t               corner = corners[v];
        float                  px = corner & 1 ? path.m_Bounds.getZ() + padding_x : path.m_Bounds.getX() - padding_x;
        float                  py = corner & 2 ? path.m_Bounds.getW() + padding_y : path.m_Bounds.getY() - padding_y;
        Vector4                local(px, py, 0, 1);
        Vector4                p = layer.m_Transform * local;
        Vector4                paint = layer.m_PaintTransform * local;
        FontVectorPaintVertex& vertex = vertices[v];
        vertex.m_Position = transform * Vector4(x + p.getX() * scale, y + p.getY() * scale, 0, 1);
        vertex.m_Coords = Vector4((px - path.m_Bounds.getX()) / width, (py - path.m_Bounds.getY()) / height, paint.getX(), paint.getY());
        vertex.m_Banding = Vector4(path.m_Slug.m_BandTransform[0], path.m_Slug.m_BandTransform[1], path.m_Slug.m_BandTransform[2], path.m_Slug.m_BandTransform[3]);
        vertex.m_Info = Vector4((float)path.m_Slug.m_BandTexel, (float)layer.m_Paint, 0, 0);
        vertex.m_Color = Vector4(rgba[corner][0], rgba[corner][1], rgba[corner][2], rgba[corner][3]);
        vertex.m_Clip = clip;
        vertex.m_FontClip = layer.m_Clip;
        vertex.m_FontPosition[0] = p.getX();
        vertex.m_FontPosition[1] = p.getY();
    }
}
