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

#ifndef DMSDK_INTERSECTION_H
#define DMSDK_INTERSECTION_H

#include <dmsdk/dlib/vmath.h>

/*# Intersection math structs and functions
 * Intersection math structs and functions
 *
 * @document
 * @name Intersection structs and functions
 * @namespace dmIntersection
 * @path engine/dlib/src/dmsdk/dlib/intersection.h
 */

namespace dmIntersection
{
    typedef dmVMath::Vector4 Plane;

    float DistanceToPlane(Plane plane, dmVMath::Point3 pos);

    struct Frustum
    {
        // The plane normals point inwards
        Plane m_Planes[6]; // left, right, bottom, top, near, far
    };

    void CreateFrustumFromMatrix(const dmVMath::Matrix4& m, bool normalize, Frustum& frustum);

    bool TestFrustumPoint(const Frustum& frustum, const dmVMath::Point3& pos, bool skip_near_far);
    bool TestFrustumSphere(const Frustum& frustum, const dmVMath::Point3& pos, float radius, bool skip_near_far);
    bool TestFrustumSphere(const Frustum& frustum, const dmVMath::Vector4& pos, float radius, bool skip_near_far);

} // dmIntersection

#endif // DMSDK_INTERSECTION_H
