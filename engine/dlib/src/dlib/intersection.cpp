// Copyright 2020-2022 The Defold Foundation
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

#include <dmsdk/dlib/intersection.h>

namespace dmIntersection
{
// Keeping this private for now. Making sure that the W component is always 1
static float DistanceToPlane(Plane plane, dmVMath::Vector4 pos)
{
    dmVMath::Vector4 r = Vectormath::Aos::mulPerElem(plane, pos); // nx * px + ny * py + nz * pz + d
    return Vectormath::Aos::sum(r);
}

float DistanceToPlane(Plane plane, dmVMath::Point3 pos)
{
    return DistanceToPlane(plane, dmVMath::Vector4(pos));
}

static Plane NormalizePlane(Plane plane)
{
    float length = Vectormath::Aos::length(plane.getXYZ());
    return plane / length;
}

//#include <stdio.h>
// static void printFrustum(const Frustum& frustum)
// {
//     printf("frustum\n");
//     printf("  left:   %f, %f, %f, %f\n", frustum.m_Planes[0].getX(), frustum.m_Planes[0].getY(), frustum.m_Planes[0].getZ(), frustum.m_Planes[0].getW());
//     printf("  right:  %f, %f, %f, %f\n", frustum.m_Planes[1].getX(), frustum.m_Planes[1].getY(), frustum.m_Planes[1].getZ(), frustum.m_Planes[1].getW());
//     printf("  bottom: %f, %f, %f, %f\n", frustum.m_Planes[2].getX(), frustum.m_Planes[2].getY(), frustum.m_Planes[2].getZ(), frustum.m_Planes[2].getW());
//     printf("  top:    %f, %f, %f, %f\n", frustum.m_Planes[3].getX(), frustum.m_Planes[3].getY(), frustum.m_Planes[3].getZ(), frustum.m_Planes[3].getW());
//     printf("  near:   %f, %f, %f, %f\n", frustum.m_Planes[4].getX(), frustum.m_Planes[4].getY(), frustum.m_Planes[4].getZ(), frustum.m_Planes[4].getW());
//     printf("  far:    %f, %f, %f, %f\n", frustum.m_Planes[5].getX(), frustum.m_Planes[5].getY(), frustum.m_Planes[5].getZ(), frustum.m_Planes[5].getW());
// }

// Gribb-Hartmann
// https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf

void CreateFrustumFromMatrix(const dmVMath::Matrix4& m, bool normalize, Frustum& frustum)
{
    dmVMath::Vector4 x = m.getRow(0);
    dmVMath::Vector4 y = m.getRow(1);
    dmVMath::Vector4 z = m.getRow(2);
    dmVMath::Vector4 w = m.getRow(3);

    frustum.m_Planes[0] = w + x; // left
    frustum.m_Planes[1] = w - x; // right
    frustum.m_Planes[2] = w + y; // bottom
    frustum.m_Planes[3] = w - y; // top
    frustum.m_Planes[4] = w + z; // near
    frustum.m_Planes[5] = w - z; // far

    if (normalize)
    {
        for (int i = 0; i < 6; ++i)
        {
            frustum.m_Planes[i] = NormalizePlane(frustum.m_Planes[i]);
        }
    }
}


bool TestFrustumPoint(const Frustum& frustum, dmVMath::Point3 pos, bool skip_near_far)
{
    dmVMath::Vector4 vpos(pos);

    uint32_t num_planes = skip_near_far ? 4 : 6;
    for (uint32_t i = 0; i < num_planes; ++i)
    {
        float d = DistanceToPlane(frustum.m_Planes[i], vpos);
        if (d < 0.0f)
        {
            return false;
        }
    }
    return true;
}

bool TestFrustumSphere(const Frustum& frustum, dmVMath::Point3 pos, float radius, bool skip_near_far)
{
    dmVMath::Vector4 vpos(pos);

    uint32_t num_planes = skip_near_far ? 4 : 6;
    for (uint32_t i = 0; i < num_planes; ++i)
    {
        float d = DistanceToPlane(frustum.m_Planes[i], vpos);
        if (d < -radius)
        {
            return false;
        }
    }
    return true;
}


} // dmIntersection
