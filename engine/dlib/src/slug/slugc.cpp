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

#include "slugc.h"
#include "slug.hpp"
#include <dlib/math.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

using dmVMath::Matrix4;
using dmVMath::Vector4;

struct AtlasContext
{
    dmSlug::Data*               m_Slug;
    dmArray<dmSlug::PaintPath>  m_Paths;
    dmArray<dmSlug::PaintLayer> m_LayerScratch;
    dmArray<Vector4>            m_Paints;
};

template <typename T>
static void Append(dmArray<T>& values, const T& value)
{
    if (values.Full())
        values.OffsetCapacity(32);
    values.Push(value);
}

static bool Finite(const float* values, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
        if (!isfinite(values[i]))
            return false;
    return true;
}

static SlugResult AddPath(AtlasContext* cache, const SlugImage& image, const SlugPath& input)
{
    if (!input.m_Commands || !input.m_CommandCount || !input.m_Stops || !input.m_StopCount ||
        input.m_PaintType > 2 || input.m_Spread > 2 || input.m_EvenOdd > 1 ||
        !Finite(input.m_Parameters, 6) || !Finite(input.m_PaintTransform, 6))
        return SLUG_RESULT_INVALID_ARGUMENT;
    uint32_t remaining = cache->m_Slug->m_MaxTexels - cache->m_Paints.Size();
    uint32_t records = 1 + (cache->m_LayerScratch.Size() + 1) * 6;
    if (input.m_CommandCount > 4096 || input.m_StopCount > 1024 || records + 3 + input.m_StopCount * 2 > remaining)
        return SLUG_RESULT_CAPACITY;

    dmArray<dmSlug::OutlineCommand> commands;
    commands.SetCapacity(input.m_CommandCount);
    commands.SetSize(input.m_CommandCount);
    for (uint32_t i = 0; i < input.m_CommandCount; ++i)
    {
        const float* c = input.m_Commands + i * 7;
        if (!Finite(c, 7) || c[0] < 0 || c[0] > 4 || c[0] != floorf(c[0]))
            return SLUG_RESULT_INVALID_ARGUMENT;
        commands[i].m_Type = (dmSlug::OutlineCommandType)(uint32_t)c[0];
        for (uint32_t p = 0; p < 3; ++p)
        {
            commands[i].m_Points[p].m_X = c[1 + p * 2];
            commands[i].m_Points[p].m_Y = c[2 + p * 2];
        }
    }
    dmSlug::Outline outline = { commands.Begin(), commands.Size() };
    float           x0, y0, x1, y1;
    if (!dmSlug::GetOutlineBounds(&outline, &x0, &y0, &x1, &y1) || x0 >= x1 || y0 >= y1)
        return SLUG_RESULT_OK;
    dmArray<dmSlug::CurveCommand> curves;
    float                         tolerance = dmMath::Max(x1 - x0, y1 - y0) / 16384.0f;
    if (!dmSlug::OutlineToQuadratics(&outline, 1.0f, tolerance, &curves))
        return SLUG_RESULT_PATH_ERROR;
    for (uint32_t i = 0; i < curves.Size(); ++i)
    {
        uint32_t points = curves[i].m_Type == dmSlug::CURVE_QUADRATIC_TO ? 2 : (curves[i].m_Type == dmSlug::CURVE_CLOSE ? 0 : 1);
        for (uint32_t p = 0; p < points; ++p)
        {
            curves[i].m_Points[p].m_X = (curves[i].m_Points[p].m_X - x0) / (x1 - x0);
            curves[i].m_Points[p].m_Y = (curves[i].m_Points[p].m_Y - y0) / (y1 - y0);
        }
    }
    dmSlug::PaintPath path = {};
    path.m_EvenOdd = input.m_EvenOdd;
    path.m_Bounds = Vector4(x0, y0, x1, y1);
    uint32_t curve_count = 0;
    for (uint32_t i = 0; i < curves.Size(); ++i)
        curve_count += curves[i].m_Type != dmSlug::CURVE_MOVE_TO;
    bool capacity = curves.Size() > 4096 || curve_count > 1024;
    bool ok = !capacity && dmSlug::AddPath(cache->m_Slug, curves.Begin(), curves.Size(), 1.0f, 8, &path.m_Slug);
    if (!ok)
        return capacity || cache->m_Slug->m_Overflow ? SLUG_RESULT_CAPACITY : SLUG_RESULT_PATH_ERROR;
    if (!path.m_Slug.m_CurveCount)
        return SLUG_RESULT_OK;

    dmSlug::PaintLayer layer;
    layer.m_Path = cache->m_Paths.Size();
    layer.m_Paint = cache->m_Paints.Size();
    layer.m_CompositeCount = 1;
    layer.m_Transform = Matrix4::identity();
    const float* m = input.m_PaintTransform;
    layer.m_PaintTransform = Matrix4(Vector4(m[0], m[1], 0, 0), Vector4(m[2], m[3], 0, 0), Vector4(0, 0, 1, 0), Vector4(m[4], m[5], 0, 1));
    layer.m_Clip = Vector4(0, 0, image.m_Width, image.m_Height);
    Append(cache->m_Paths, path);
    Append(cache->m_LayerScratch, layer);
    Vector4      p0(0), p1(0);
    const float* p = input.m_Parameters;
    uint32_t     type = input.m_PaintType;
    if (type == 1)
    {
        float dx = p[2] - p[0], dy = p[3] - p[1];
        float denominator = dx * dx + dy * dy;
        if (denominator == 0)
            type = 0; // Degenerate SVG gradients use the last stop.
        else
            p0 = Vector4(dx / denominator, dy / denominator, p[0], p[1]);
    }
    else if (type == 2)
    {
        p0 = Vector4(p[0], p[1], p[2], 0);
        p1 = Vector4(p[3], p[4], p[5], 0);
        if (p[5] <= 0)
            type = 0;
    }
    uint32_t first = type == 0 ? input.m_StopCount - 1 : 0;
    Append(cache->m_Paints, Vector4((float)type, (float)(input.m_StopCount - first), (float)input.m_Spread, 0));
    Append(cache->m_Paints, p0);
    Append(cache->m_Paints, p1);
    float previous = 0;
    for (uint32_t i = 0; i < input.m_StopCount; ++i)
    {
        const float* s = input.m_Stops + i * 5;
        if (!Finite(s, 5) || s[0] < previous || s[0] > 1 || s[4] < 0 || s[4] > 1)
            return SLUG_RESULT_INVALID_ARGUMENT;
        previous = s[0];
        if (i >= first)
        {
            Append(cache->m_Paints, Vector4(s[1], s[2], s[3], s[4]));
            Append(cache->m_Paints, Vector4(s[0], 0, 0, 0));
        }
    }
    return SLUG_RESULT_OK;
}

static bool CopyTexture(SlugTexture* texture, const void* data, uint32_t texels, uint32_t channels, uint32_t component_size)
{
    texture->m_Width = dmSlug::TEXTURE_WIDTH;
    texture->m_Height = dmMath::Max(1u, (texels + dmSlug::TEXTURE_WIDTH - 1) / dmSlug::TEXTURE_WIDTH);
    texture->m_Channels = channels;
    texture->m_PixelCount = texture->m_Width * texture->m_Height * channels * component_size;
    texture->m_Pixels = (uint8_t*)calloc(1, texture->m_PixelCount);
    if (!texture->m_Pixels)
        return false;
    if (texels)
        memcpy(texture->m_Pixels, data, texels * channels * component_size);
    return true;
}

SlugResult SlugCompileAtlas(const SlugImage* images, uint32_t count, SlugTexture* curves, SlugTexture* bands, SlugTexture* paints, uint32_t* image_paints)
{
    if (!curves || !bands || !paints || curves == bands || curves == paints || bands == paints ||
        (count && (!images || !image_paints)))
        return SLUG_RESULT_INVALID_ARGUMENT;
    memset(curves, 0, sizeof(*curves));
    memset(bands, 0, sizeof(*bands));
    memset(paints, 0, sizeof(*paints));
    dmSlug::Data slug;
    AtlasContext cache;
    cache.m_Slug = &slug;
    for (uint32_t i = 0; i < count; ++i)
    {
        const SlugImage& input = images[i];
        if (!isfinite(input.m_Width) || !isfinite(input.m_Height) || input.m_Width <= 0 || input.m_Height <= 0 ||
            (input.m_PathCount && !input.m_Paths))
            return SLUG_RESULT_INVALID_ARGUMENT;
        if (input.m_PathCount > slug.m_MaxTexels / 6 || cache.m_Paints.Size() == slug.m_MaxTexels)
            return SLUG_RESULT_CAPACITY;
        cache.m_Paths.SetSize(0);
        cache.m_LayerScratch.SetSize(0);
        for (uint32_t j = 0; j < input.m_PathCount; ++j)
        {
            SlugResult result = AddPath(&cache, input, input.m_Paths[j]);
            if (result != SLUG_RESULT_OK)
                return result;
        }
        dmSlug::PaintImage paint_image = {};
        paint_image.m_LayerCount = cache.m_LayerScratch.Size();
        dmSlug::EncodePaint(&cache.m_Paints, cache.m_Paths.Begin(), cache.m_LayerScratch.Begin(), &paint_image, true);
        image_paints[i] = paint_image.m_Paint;
    }
    if (!CopyTexture(curves, slug.m_Curves.Begin(), slug.m_Curves.Size() / 4, 4, sizeof(uint16_t)) ||
        !CopyTexture(bands, slug.m_Bands.Begin(), slug.m_Bands.Size(), 1, sizeof(uint32_t)) ||
        !CopyTexture(paints, cache.m_Paints.Begin(), cache.m_Paints.Size(), 4, sizeof(float)))
    {
        SlugFreeTexture(curves);
        SlugFreeTexture(bands);
        SlugFreeTexture(paints);
        return SLUG_RESULT_OUT_OF_MEMORY;
    }
    return SLUG_RESULT_OK;
}

void SlugFreeTexture(SlugTexture* texture)
{
    free(texture->m_Pixels);
    memset(texture, 0, sizeof(*texture));
}
