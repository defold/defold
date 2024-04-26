// Copyright 2020-2024 The Defold Foundation
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
    /*#
     * Plane struct (currently an alias for dmVMath::Vector4)
     * @typedef
     * @name Plane
     */
    typedef dmVMath::Vector4 Plane;

    /*#
     * Returns the closest distance between a plane and a position
     * @name DistanceToPlane
     * @param plane [type: dmIntersection::Plane] plane equation
     * @param pos [type: dmVMath::Point3] position
     * @return distance [type: float] closest distance between plane and position
     */
    float DistanceToPlane(Plane plane, dmVMath::Point3 pos);

    /*# frustum
     * Frustum
     * @note The plane normals point inwards
     * @struct
     * @name Frustum
     * @member m_Planes [type:dmIntersection::Plane[6]] plane equations: // left, right, bottom, top, near, far
     */
    struct Frustum
    {
        // The plane normals point inwards
        Plane m_Planes[6]; // left, right, bottom, top, near, far
        int   m_NumPlanes; // 4 or 6
    };

    /*#
     * Constructs a dmIntersection::Frustum from a View*Projection matrix
     * @name CreateFrustumFromMatrix
     * @param m [type: dmVMath::Matrix4&] The matrix. Usually a "ViewProj" matrix
     * @param normalize [type: bool] true if the normals should be normalized
     * @return frustum [type: dmIntersection::Frustum&] the frustum output
     */
    void CreateFrustumFromMatrix(const dmVMath::Matrix4& m, bool normalize, int num_planes, Frustum& frustum);

    // Returns true if the objects intersect

    /*#
     * Tests intersection between a frustum and a point
     * @name TestFrustumPoint
     * @param frustum [type: dmIntersection::Frustum&] the frustum
     * @param pos [type: dmVMath::Point3&] the position
     * @param skip_near_far [type: bool] if true, the near+far planes of the frustum are ignored
     * @return intersects [type: bool] Returns true if the objects intersect
     */
    bool TestFrustumPoint(const Frustum& frustum, const dmVMath::Point3& pos);

    /*#
     * Tests intersection between a frustum and a sphere
     * @name TestFrustumSphere
     * @param frustum [type: dmIntersection::Frustum&] the frustum
     * @param pos [type: dmVMath::Point3&] the center position of the sphere
     * @param radius [type: float] the radius of the sphere
     * @param skip_near_far [type: bool] if true, the near+far planes of the frustum are ignored
     * @return intersects [type: bool] Returns true if the objects intersect
     */
    bool TestFrustumSphere(const Frustum& frustum, const dmVMath::Point3& pos, float radius);

    /*#
     * Tests intersection between a frustum and a sphere
     * @name TestFrustumSphere
     * @param frustum [type: dmIntersection::Frustum&] the frustum
     * @param pos [type: dmVMath::Vector4&] the center position of the sphere. The w component must be 1.
     * @param radius [type: float] the radius of the sphere
     * @param skip_near_far [type: bool] if true, the near+far planes of the frustum are ignored
     * @return intersects [type: bool] Returns true if the objects intersect
     */
    bool TestFrustumSphere(const Frustum& frustum, const dmVMath::Vector4& pos, float radius);

    /*#
     * Tests intersection between a frustum and a sphere
     * @name TestFrustumSphereSq
     * @param frustum [type: dmIntersection::Frustum&] the frustum
     * @param pos [type: dmVMath::Point3&] the center position of the sphere
     * @param radius_sq [type: float] the squared radius of the sphere
     * @param skip_near_far [type: bool] if true, the near+far planes of the frustum are ignored
     * @return intersects [type: bool] Returns true if the objects intersect
     */
    bool TestFrustumSphereSq(const Frustum& frustum, const dmVMath::Point3& pos, float radius_sq);

    /*#
     * Tests intersection between a frustum and a sphere
     * @name TestFrustumSphereSq
     * @param frustum [type: dmIntersection::Frustum&] the frustum
     * @param pos [type: dmVMath::Vector4&] the center position of the sphere. The w component must be 1.
     * @param radius_sq [type: float] the squared radius of the sphere
     * @param skip_near_far [type: bool] if true, the near+far planes of the frustum are ignored
     * @return intersects [type: bool] Returns true if the objects intersect
     */
    bool TestFrustumSphereSq(const Frustum& frustum, const dmVMath::Vector4& pos, float radius_sq);

    /*#
     * Tests intersection between a frustum and an oriented bounding box (OBB)
     * @name TestFrustumOBB
     * @param frustum [type: dmIntersection::Frustum&] the frustum
     * @param world [type: dmVMath::Matrix4&] The world transform of the OBB
     * @param aabb_min [type: dmVMath::Vector3&] the minimum corner of the object. In local space.
     * @param aabb_max [type: dmVMath::Vector3&] the maximum corner of the object. In local space.
     * @param skip_near_far [type: bool] if true, the near+far planes of the frustum are ignored
     * @return intersects [type: bool] Returns true if the objects intersect
     */
    bool TestFrustumOBB(const Frustum& frustum, const dmVMath::Matrix4& world, dmVMath::Vector3& aabb_min, dmVMath::Vector3& aabb_max);

} // dmIntersection

#endif // DMSDK_INTERSECTION_H
