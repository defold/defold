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

#include <dmsdk/dlib/intersection.h>
#include <stdint.h>

namespace dmIntersection
{
// Keeping this private for now. Making sure that the W component is always 1
static inline float DistanceToPlane(Plane plane, dmVMath::Vector4 pos)
{
    return dmVMath::Dot(plane, pos); // nx * px + ny * py + nz * pz + d*pw , pw - should be 1
}

float DistanceToPlane(Plane plane, dmVMath::Point3 pos)
{
    return DistanceToPlane(plane, dmVMath::Vector4(pos));
}

static Plane NormalizePlane(Plane plane)
{
    float length = dmVMath::Length(plane.getXYZ());
    return plane / length;
}

// Gribb-Hartmann
// https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf

void CreateFrustumFromMatrix(const dmVMath::Matrix4& m, bool normalize, int num_planes, Frustum& frustum)
{
    dmVMath::Vector4 x = m.getRow(0);
    dmVMath::Vector4 y = m.getRow(1);
    dmVMath::Vector4 z = m.getRow(2);
    dmVMath::Vector4 w = m.getRow(3);

    frustum.m_NumPlanes = num_planes;
    frustum.m_Planes[0] = w + x; // left
    frustum.m_Planes[1] = w - x; // right
    frustum.m_Planes[2] = w + y; // bottom
    frustum.m_Planes[3] = w - y; // top
    frustum.m_Planes[4] = w + z; // near
    frustum.m_Planes[5] = w - z; // far

    if (normalize)
    {
        for (int i = 0; i < frustum.m_NumPlanes; ++i)
        {
            frustum.m_Planes[i] = NormalizePlane(frustum.m_Planes[i]);
        }
    }
}


bool TestFrustumPoint(const Frustum& frustum, const dmVMath::Point3& pos)
{
    dmVMath::Vector4 vpos(pos);

    int num_planes = frustum.m_NumPlanes;
    for (int i = 0; i < num_planes; ++i)
    {
        float d = DistanceToPlane(frustum.m_Planes[i], vpos);
        if (d < 0.0f)
        {
            return false;
        }
    }
    return true;
}

bool TestFrustumSphereSq(const Frustum& frustum, const dmVMath::Point3& pos, float radius_sq)
{
    dmVMath::Vector4 vpos(pos);
    return TestFrustumSphereSq(frustum, vpos, radius_sq);
}

bool TestFrustumSphereSq(const Frustum& frustum, const dmVMath::Vector4& pos, float radius_sq)
{
    int num_planes = frustum.m_NumPlanes;
    for (int i = 0; i < num_planes; ++i)
    {
        float d = DistanceToPlane(frustum.m_Planes[i], pos);
        if (d < 0 && (d*d) > radius_sq)
        {
            return false;
        }
    }
    return true;
}

bool TestFrustumSphere(const Frustum& frustum, const dmVMath::Point3& pos, float radius)
{
    return TestFrustumSphereSq(frustum, pos, radius*radius);
}

bool TestFrustumSphere(const Frustum& frustum, const dmVMath::Vector4& pos, float radius)
{
    return TestFrustumSphereSq(frustum, pos, radius*radius);
}

// Returns 'false' if the bounding box is off the frustum. Returning 'true' does not guarantee that the object intersects the frustum or is inside it.
bool TestFrustumOBB(const Frustum& frustum, const dmVMath::Matrix4& world, dmVMath::Vector3& aabb_min, dmVMath::Vector3& aabb_max)
{
    // calculate coordinates of all 8 corners of the bounding cube.
    // To find them we'll take all points P(Xi,Yj,Zk) i=aabb_min.x|aabb_max.x, j=aabb_min.y|aabb_max.y, k=aabb_min.z|aabb_max.z
    dmVMath::Point3 point0 = dmVMath::Point3(aabb_min.getX(), aabb_min.getY(), aabb_min.getZ());
    dmVMath::Point3 point1 = dmVMath::Point3(aabb_min.getX(), aabb_min.getY(), aabb_max.getZ());
    dmVMath::Point3 point2 = dmVMath::Point3(aabb_min.getX(), aabb_max.getY(), aabb_min.getZ());
    dmVMath::Point3 point3 = dmVMath::Point3(aabb_min.getX(), aabb_max.getY(), aabb_max.getZ());
    dmVMath::Point3 point4 = dmVMath::Point3(aabb_max.getX(), aabb_min.getY(), aabb_min.getZ());
    dmVMath::Point3 point5 = dmVMath::Point3(aabb_max.getX(), aabb_min.getY(), aabb_max.getZ());
    dmVMath::Point3 point6 = dmVMath::Point3(aabb_max.getX(), aabb_max.getY(), aabb_min.getZ());
    dmVMath::Point3 point7 = dmVMath::Point3(aabb_max.getX(), aabb_max.getY(), aabb_max.getZ());

    dmVMath::Vector4 corner_points[8]; // corner points in world coords

    corner_points[0] = world * point0;
    corner_points[1] = world * point1;
    corner_points[2] = world * point2;
    corner_points[3] = world * point3;
    corner_points[4] = world * point4;
    corner_points[5] = world * point5;
    corner_points[6] = world * point6;
    corner_points[7] = world * point7;

    // for any of the six frustum planes if the all corner points lie in the negative halfspace, do cull the object
    int num_planes = frustum.m_NumPlanes;
    for (int plane_i = 0; plane_i < num_planes; ++plane_i)
    {
        // get plane normal/equation
        bool positive_found = false;
        for (int corner_i = 0; corner_i < 8; ++corner_i)
        {
            float distance = DistanceToPlane(frustum.m_Planes[plane_i], corner_points[corner_i]);
            if (distance >= 0)
            {
                positive_found = true;
                break; // no need to check for the rest of the points
            }
        }
        if (!positive_found) // if all corners are in the negative halfspace for the plane, we're done
        {
            return false; // no intersection, do cull
        }

    }
    return true; // inside the frustum but false positives may also happen. They are ok when used for frustum culling where the object will be hidden later in the rendering pipeline.
}

} // dmIntersection
