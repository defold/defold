// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0 (https://www.defold.com/license).

#include "font_vector_slug.h"
#include <dlib/math.h>
#include <math.h>
#include <string.h>

struct SlugCurve
{
    FontCurvePoint m_P[3];
    uint32_t m_Texel;
};

static uint16_t ToHalf(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    const uint32_t sign = (bits >> 16) & 0x8000;
    const int exponent = (int)((bits >> 23) & 255) - 127 + 15;
    uint32_t mantissa = bits & 0x7fffff;
    if (exponent >= 31)
        return (uint16_t)(sign | 0x7c00);
    if (exponent < -10)
        return (uint16_t)sign;
    const uint32_t shift = exponent <= 0 ? 14 - exponent : 13;
    if (exponent <= 0)
        mantissa |= 0x800000;
    uint32_t result = mantissa >> shift;
    const uint32_t remainder = mantissa & ((1u << shift) - 1);
    const uint32_t halfway = 1u << (shift - 1);
    result += remainder > halfway || (remainder == halfway && (result & 1));
    if (exponent > 0)
        result += (uint32_t)exponent << 10;
    return (uint16_t)(sign | result);
}

static float FromHalf(uint16_t value)
{
    const uint32_t sign = (uint32_t)(value & 0x8000) << 16;
    uint32_t mantissa = value & 1023;
    int exponent = (value >> 10) & 31;
    if (!exponent && !mantissa)
    {
        float result;
        memcpy(&result, &sign, sizeof(result));
        return result;
    }
    if (!exponent)
    {
        exponent = 1;
        while (!(mantissa & 1024)) { mantissa <<= 1; --exponent; }
        mantissa &= 1023;
    }
    uint32_t bits = sign | ((uint32_t)(exponent + 112) << 23) | (mantissa << 13);
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static bool Same(const FontCurvePoint& a, const FontCurvePoint& b)
{
    return a.m_X == b.m_X && a.m_Y == b.m_Y;
}

static bool AddCurve(dmArray<SlugCurve>* curves, FontCurvePoint p0, FontCurvePoint p1,
                     FontCurvePoint p2, float font_size)
{
    if (Same(p0, p1) && Same(p0, p2))
        return true;
    if (curves->Size() >= 1024)
        return false;
    SlugCurve curve = { {p0, p1, p2}, 0 };
    for (uint32_t i = 0; i < 3; ++i)
    {
        float x = curve.m_P[i].m_X / font_size;
        float y = curve.m_P[i].m_Y / font_size;
        if (!isfinite(x) || !isfinite(y) || fabsf(x) > 16.0f || fabsf(y) > 16.0f)
            return false;
        // Build bands from the same quantized coordinates the GPU reads.
        curve.m_P[i].m_X = FromHalf(ToHalf(x));
        curve.m_P[i].m_Y = FromHalf(ToHalf(y));
    }
    if (Same(curve.m_P[0], curve.m_P[1]) && Same(curve.m_P[0], curve.m_P[2]))
        return true;
    if (curves->Full()) curves->OffsetCapacity(32);
    curves->Push(curve);
    return true;
}

template <typename T> static bool Grow(dmArray<T>* array, uint32_t size, uint32_t limit)
{
    if (size > limit) return false;
    if (array->Capacity() < size)
        array->SetCapacity(dmMath::Min(limit, (size + 4095) & ~4095u));
    const uint32_t previous = array->Size();
    array->SetSize(size);
    memset(array->Begin() + previous, 0, (size - previous) * sizeof(T));
    return true;
}

static uint32_t AlignRow(uint32_t start, uint32_t count)
{
    return start % FONT_VECTOR_SLUG_WIDTH + count > FONT_VECTOR_SLUG_WIDTH
         ? (start + FONT_VECTOR_SLUG_WIDTH - 1) & ~(FONT_VECTOR_SLUG_WIDTH - 1) : start;
}

static float Coordinate(const FontCurvePoint& p, uint32_t axis)
{
    return axis ? p.m_Y : p.m_X;
}

static float Maximum(const SlugCurve& curve, uint32_t axis)
{
    return dmMath::Max(Coordinate(curve.m_P[0], axis),
           dmMath::Max(Coordinate(curve.m_P[1], axis), Coordinate(curve.m_P[2], axis)));
}

FontVectorSlugData::FontVectorSlugData()
{
    m_MaxTexels = FONT_VECTOR_SLUG_WIDTH * FONT_VECTOR_SLUG_MAX_HEIGHT;
    FontVectorSlugBegin(this);
}

void FontVectorSlugBegin(FontVectorSlugData* data)
{
    data->m_Overflow = false;
    data->m_Curves.SetSize(0);
    data->m_Bands.SetSize(0);
    data->m_CurveTexels = data->m_BandTexels = data->m_CurveCount = 0;
    data->m_BandReferences = data->m_MaxBandCurves = 0;
}

static bool AtlasOverflow(FontVectorSlugData* data)
{
    data->m_Overflow = true;
    return false;
}

static bool AppendCurves(FontVectorSlugData* data, dmArray<SlugCurve>& curves,
                         uint32_t bands, FontVectorSlugGlyph* glyph)
{
    if (curves.Empty()) return true;
    const uint32_t limit = data->m_MaxTexels;
    float minimum[2] = { 1e30f, 1e30f }, maximum[2] = { -1e30f, -1e30f };
    for (uint32_t i = 0; i < curves.Size(); ++i)
    {
        SlugCurve& curve = curves[i];
        uint32_t texel = data->m_Curves.Size() / 4;
        const bool share = i && Same(curves[i - 1].m_P[2], curve.m_P[0]) && texel % FONT_VECTOR_SLUG_WIDTH != 0;
        if (share) --texel;
        texel = AlignRow(texel, 2);
        if (!Grow(&data->m_Curves, (texel + 2) * 4, limit * 4)) return AtlasOverflow(data);
        curve.m_Texel = texel;
        uint16_t* out = data->m_Curves.Begin() + texel * 4;
        out[0] = ToHalf(curve.m_P[0].m_X); out[1] = ToHalf(curve.m_P[0].m_Y);
        out[2] = ToHalf(curve.m_P[1].m_X); out[3] = ToHalf(curve.m_P[1].m_Y);
        out[4] = ToHalf(curve.m_P[2].m_X); out[5] = ToHalf(curve.m_P[2].m_Y);
        data->m_CurveTexels += share ? 1 : 2;
        for (uint32_t a = 0; a < 2; ++a) for (uint32_t p = 0; p < 3; ++p)
        {
            minimum[a] = dmMath::Min(minimum[a], Coordinate(curve.m_P[p], a));
            maximum[a] = dmMath::Max(maximum[a], Coordinate(curve.m_P[p], a));
        }
    }
    const uint32_t header = AlignRow(data->m_Bands.Size(), bands * 2);
    if (!Grow(&data->m_Bands, header + bands * 2, limit)) return AtlasOverflow(data);
    glyph->m_BandTexel = header;
    glyph->m_CurveCount = curves.Size();
    data->m_CurveCount += curves.Size();
    data->m_BandTexels += bands * 2;
    for (uint32_t axis = 0; axis < 2; ++axis)
    {
        const float height = dmMath::Max(maximum[axis] - minimum[axis], 1.0f / 65536.0f);
        glyph->m_BandTransform[axis] = bands / height;
        glyph->m_BandTransform[axis + 2] = -minimum[axis] * glyph->m_BandTransform[axis];
    }
    uint32_t indices[1024];
    // Horizontal headers precede vertical headers, as in the reference.
    for (uint32_t direction = 0; direction < 2; ++direction)
    {
        const uint32_t axis = 1 - direction;
        for (uint32_t band = 0; band < bands; ++band)
        {
            const float lo = minimum[axis] + band / glyph->m_BandTransform[axis] - 1.0f / 1024.0f;
            const float hi = minimum[axis] + (band + 1) / glyph->m_BandTransform[axis] + 1.0f / 1024.0f;
            uint32_t count = 0;
            for (uint32_t i = 0; i < curves.Size(); ++i)
            {
                const SlugCurve& c = curves[i];
                const float a = Coordinate(c.m_P[0], axis), b = Coordinate(c.m_P[1], axis), z = Coordinate(c.m_P[2], axis);
                if (a == b && a == z) continue;
                if (dmMath::Min(a, dmMath::Min(b, z)) > hi || dmMath::Max(a, dmMath::Max(b, z)) < lo) continue;
                uint32_t j = count++;
                const float maximum = Maximum(c, direction);
                while (j && Maximum(curves[indices[j - 1]], direction) < maximum)
                {
                    indices[j] = indices[j - 1];
                    --j;
                }
                indices[j] = i;
            }
            const uint32_t list = AlignRow(data->m_Bands.Size(), count);
            if (list - header > 65535 || !Grow(&data->m_Bands, list + count, limit)) return AtlasOverflow(data);
            data->m_Bands[header + direction * bands + band] = count | ((list - header) << 16);
            for (uint32_t i = 0; i < count; ++i)
            {
                const uint32_t texel = curves[indices[i]].m_Texel;
                data->m_Bands[list + i] = (texel % FONT_VECTOR_SLUG_WIDTH) | ((texel / FONT_VECTOR_SLUG_WIDTH) << 16);
            }
            data->m_BandTexels += count;
            data->m_BandReferences += count;
            data->m_MaxBandCurves = dmMath::Max(data->m_MaxBandCurves, count);
        }
    }
    return true;
}

// Appending never changes existing records. Roll back cursors and accounting
// if a glyph cannot fit, so already cached glyphs remain usable.
static bool EncodeCurves(FontVectorSlugData* data, dmArray<SlugCurve>& curves,
                         uint32_t bands, FontVectorSlugGlyph* glyph)
{
    const uint32_t curve_size = data->m_Curves.Size();
    const uint32_t band_size = data->m_Bands.Size();
    const uint32_t curve_texels = data->m_CurveTexels;
    const uint32_t band_texels = data->m_BandTexels;
    const uint32_t curve_count = data->m_CurveCount;
    const uint32_t references = data->m_BandReferences;
    const uint32_t max_curves = data->m_MaxBandCurves;
    if (AppendCurves(data, curves, bands, glyph))
        return true;
    data->m_Curves.SetSize(curve_size);
    data->m_Bands.SetSize(band_size);
    data->m_CurveTexels = curve_texels;
    data->m_BandTexels = band_texels;
    data->m_CurveCount = curve_count;
    data->m_BandReferences = references;
    data->m_MaxBandCurves = max_curves;
    memset(glyph, 0, sizeof(*glyph));
    return false;
}

bool FontVectorSlugAddGlyph(FontVectorSlugData* data, const FontGlyphOutline& outline,
                           float font_size, uint32_t bands, FontVectorSlugGlyph* glyph)
{
    data->m_Overflow = false;
    memset(glyph, 0, sizeof(*glyph));
    if (!isfinite(font_size) || font_size <= 0 || bands < 1 || bands > 32 ||
        outline.m_CommandCount > 4096 || (outline.m_CommandCount && !outline.m_Commands))
        return false;
    dmArray<SlugCurve> curves;
    FontCurvePoint current = {}, start = {};
    bool active = false;
    for (uint32_t i = 0; i < outline.m_CommandCount; ++i)
    {
        const FontCurveCommand& c = outline.m_Commands[i];
        if (c.m_Type == FONT_CURVE_MOVE_TO)
        {
            current = start = c.m_Points[0];
            active = true;
        }
        else if (c.m_Type == FONT_CURVE_LINE_TO && active)
        {
            // The reference recommends duplicating the second endpoint.
            if (!AddCurve(&curves, current, c.m_Points[0], c.m_Points[0], font_size)) return false;
            current = c.m_Points[0];
        }
        else if (c.m_Type == FONT_CURVE_QUADRATIC_TO && active)
        {
            if (!AddCurve(&curves, current, c.m_Points[0], c.m_Points[1], font_size)) return false;
            current = c.m_Points[1];
        }
        else if (c.m_Type == FONT_CURVE_CLOSE)
        {
            if (active && !Same(current, start) && !AddCurve(&curves, current, start, start, font_size)) return false;
            active = false;
        }
        else return false;
    }
    return EncodeCurves(data, curves, bands, glyph);
}

bool FontVectorSlugAddFontGlyph(FontVectorSlugData* data, const FontGlyph& source, uint32_t bands, FontVectorSlugGlyph* glyph)
{
    data->m_Overflow = false;
    memset(glyph, 0, sizeof(*glyph));
    if (bands < 1 || bands > 32)
        return false;
    if (source.m_Vector.m_DataSize)
    {
        const uint32_t stride = 8 * sizeof(float);
        if (!source.m_Vector.m_Data || source.m_Vector.m_CurveCount > 1024 ||
            source.m_Vector.m_DataSize / stride != source.m_Vector.m_CurveCount || source.m_Vector.m_DataSize % stride)
            return false;
        dmArray<SlugCurve> curves;
        for (uint32_t i = 0; i < source.m_Vector.m_CurveCount; ++i)
        {
            float p[8];
            memcpy(p, source.m_Vector.m_Data + i * stride, sizeof(p));
            for (uint32_t j = 0; j < 8; ++j)
                if (!isfinite(p[j])) return false;
            FontCurvePoint p0 = { p[0], p[1] }, p1 = { p[2], p[3] }, p2 = { p[4], p[5] };
            if (!AddCurve(&curves, p0, p1, p2, 1.0f)) return false;
        }
        return EncodeCurves(data, curves, bands, glyph);
    }
    const FontGlyphOutline& outline = source.m_Outline;
    const float height = outline.m_Ascent + outline.m_Descent;
    if (!isfinite(outline.m_Width) || !isfinite(height) || outline.m_Width <= 0 || height <= 0 ||
        outline.m_CommandCount > 4096 || (outline.m_CommandCount && !outline.m_Commands))
        return false;
    dmArray<FontCurveCommand> commands;
    commands.SetCapacity(outline.m_CommandCount);
    commands.SetSize(outline.m_CommandCount);
    for (uint32_t i = 0; i < outline.m_CommandCount; ++i)
    {
        commands[i] = outline.m_Commands[i];
        uint32_t points = commands[i].m_Type == FONT_CURVE_QUADRATIC_TO ? 2 : commands[i].m_Type == FONT_CURVE_CLOSE ? 0 : 1;
        for (uint32_t j = 0; j < points; ++j)
        {
            commands[i].m_Points[j].m_X = (commands[i].m_Points[j].m_X - outline.m_LeftBearing) / outline.m_Width;
            commands[i].m_Points[j].m_Y = (commands[i].m_Points[j].m_Y + outline.m_Descent) / height;
        }
    }
    FontGlyphOutline normalized = outline;
    normalized.m_Commands = commands.Begin();
    return FontVectorSlugAddGlyph(data, normalized, 1.0f, bands, glyph);
}
