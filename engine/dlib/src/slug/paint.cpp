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

#include "slug.hpp"
#include <float.h>
#include <math.h>

namespace dmSlug
{
    using dmVMath::Vector4;
    using dmVMath::Matrix4;
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

    static Vector4 PaintRow(const Matrix4& transform, uint32_t row, float value)
    {
        Vector4 r = transform.getRow(row);
        return Vector4(r.getX(), r.getY(), r.getW(), value);
    }

    void EncodePaint(dmArray<Vector4>* paints, const PaintPath* paths, const PaintLayer* layers, PaintImage* image, bool srgb)
    {
        image->m_Bounds = Vector4(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
        image->m_Paint = paints->Size();
        if (!image->m_LayerCount && !srgb)
            return;
        Append(*paints, Vector4((float)image->m_LayerCount, srgb ? 1.0f : 0.0f, 0, 0));
        for (uint32_t i = 0; i < image->m_LayerCount; ++i)
        {
            const PaintLayer& layer = layers[i];
            const PaintPath&  path = paths[layer.m_Path];
            Matrix4           inverse_transform = inverse(layer.m_Transform);
            float             width = path.m_Bounds.getZ() - path.m_Bounds.getX();
            float             height = path.m_Bounds.getW() - path.m_Bounds.getY();
            Matrix4           uv = Matrix4::scale(dmVMath::Vector3(1.0f / width, 1.0f / height, 1)) *
            Matrix4::translation(dmVMath::Vector3(-path.m_Bounds.getX(), -path.m_Bounds.getY(), 0)) * inverse_transform;
            Matrix4 paint = layer.m_PaintTransform * inverse_transform;
            Append(*paints, PaintRow(uv, 0, (float)path.m_Slug.m_BandTexel));
            // A negative paint index marks a shared composite. Ordinary layers can
            // reject uncovered fragments without fetching the paint transform/count.
            float paint_index = layer.m_CompositeCount > 1 ? -(float)(layer.m_Paint + 1) : (float)layer.m_Paint;
            Append(*paints, PaintRow(uv, 1, paint_index));
            Append(*paints, Vector4(path.m_Slug.m_BandTransform[0], path.m_Slug.m_BandTransform[1], path.m_Slug.m_BandTransform[2], path.m_Slug.m_BandTransform[3]));
            Append(*paints, PaintRow(paint, 0, (float)layer.m_CompositeCount));
            Append(*paints, PaintRow(paint, 1, (float)path.m_EvenOdd));
            Append(*paints, layer.m_Clip);
            Vector4 bounds(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
            for (uint32_t corner = 0; corner < 4; ++corner)
            {
                Vector4 p = layer.m_Transform * Vector4(corner & 1 ? path.m_Bounds.getZ() : path.m_Bounds.getX(), corner & 2 ? path.m_Bounds.getW() : path.m_Bounds.getY(), 0, 1);
                bounds = Vector4(fminf(bounds.getX(), p.getX()), fminf(bounds.getY(), p.getY()), fmaxf(bounds.getZ(), p.getX()), fmaxf(bounds.getW(), p.getY()));
            }
            bounds = Intersect(bounds, layer.m_Clip);
            if (bounds.getX() < bounds.getZ() && bounds.getY() < bounds.getW())
                image->m_Bounds = Vector4(fminf(image->m_Bounds.getX(), bounds.getX()), fminf(image->m_Bounds.getY(), bounds.getY()), fmaxf(image->m_Bounds.getZ(), bounds.getZ()), fmaxf(image->m_Bounds.getW(), bounds.getW()));
        }
    }

} // namespace dmSlug
