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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "dlib/vmath.h"
#include <dmsdk/dlib/intersection.h>

const float FRUSTUM_WIDTH = 100.0f;
const float FRUSTUM_HEIGHT = 80.0f;
const float FRUSTUM_NEAR = 10.0f;
const float FRUSTUM_FAR = 100.0f;

using namespace dmVMath;

TEST(dmVMath, CreateFrustum)
{
    dmVMath::Point3 cam_pos = dmVMath::Point3(0.0f, 0.0f, 0);
    dmVMath::Matrix4 view = Matrix4::lookAt(cam_pos, dmVMath::Point3(cam_pos.getX(), cam_pos.getY(), cam_pos.getZ()-1), dmVMath::Vector3(0,1,0)); // eye, lookat, up

    dmVMath::Matrix4 proj = dmVMath::Matrix4::orthographic(0.0f, FRUSTUM_WIDTH, 0.0f, FRUSTUM_HEIGHT, 10.0f, 100.0f);

    dmVMath::Matrix4 view_proj = proj * view;

    dmIntersection::Frustum frustum;
    dmIntersection::CreateFrustumFromMatrix(view_proj, true, frustum);

    float px = FRUSTUM_WIDTH / 2.0f;
    float py = FRUSTUM_HEIGHT / 2.0f;
    float pz = -FRUSTUM_NEAR -(FRUSTUM_FAR - FRUSTUM_NEAR) / 2.0f;

    // Making sure all planes point inwards, and are located at the correct distances

    ASSERT_NEAR(-10.0f, dmIntersection::DistanceToPlane(frustum.m_Planes[0], dmVMath::Point3(-10.0f, py, pz)), 0.001f);
    ASSERT_NEAR( 10.0f, dmIntersection::DistanceToPlane(frustum.m_Planes[0], dmVMath::Point3( 10.0f, py, pz)), 0.001f);

    ASSERT_NEAR(-10.0f, dmIntersection::DistanceToPlane(frustum.m_Planes[1], dmVMath::Point3(110.0f, py, pz)), 0.001f);
    ASSERT_NEAR( 10.0f, dmIntersection::DistanceToPlane(frustum.m_Planes[1], dmVMath::Point3( 90.0f, py, pz)), 0.001f);

    ASSERT_NEAR(-10.0f, dmIntersection::DistanceToPlane(frustum.m_Planes[2], dmVMath::Point3(px, -10.0f, pz)), 0.001f);
    ASSERT_NEAR( 10.0f, dmIntersection::DistanceToPlane(frustum.m_Planes[2], dmVMath::Point3(px,  10.0f, pz)), 0.001f);

    ASSERT_NEAR(-10.0f, dmIntersection::DistanceToPlane(frustum.m_Planes[3], dmVMath::Point3(px, 90.0f, pz)), 0.001f);
    ASSERT_NEAR( 10.0f, dmIntersection::DistanceToPlane(frustum.m_Planes[3], dmVMath::Point3(px, 70.0f, pz)), 0.001f);

    ASSERT_NEAR(-10.0f, dmIntersection::DistanceToPlane(frustum.m_Planes[4], dmVMath::Point3(px, py,   0.0f)), 0.001f);
    ASSERT_NEAR( 10.0f, dmIntersection::DistanceToPlane(frustum.m_Planes[4], dmVMath::Point3(px, py, -20.0f)), 0.001f);

    ASSERT_NEAR(-10.0f, dmIntersection::DistanceToPlane(frustum.m_Planes[5], dmVMath::Point3(px, py, -110.0f)), 0.001f);
    ASSERT_NEAR( 10.0f, dmIntersection::DistanceToPlane(frustum.m_Planes[5], dmVMath::Point3(px, py,  -90.0f)), 0.001f);
}

TEST(dmVMath, TestFrustumSphere)
{
    dmVMath::Matrix4 proj = dmVMath::Matrix4::orthographic(0.0f, FRUSTUM_WIDTH, 0.0f, FRUSTUM_HEIGHT, FRUSTUM_NEAR, FRUSTUM_FAR);

    dmIntersection::Frustum frustum;
    dmIntersection::CreateFrustumFromMatrix(proj, true, frustum);

    const float RADIUS = 10.0f;
    float px = FRUSTUM_WIDTH / 2.0f;
    float py = FRUSTUM_HEIGHT / 2.0f;
    float pz = -FRUSTUM_NEAR -(FRUSTUM_FAR - FRUSTUM_NEAR) / 2.0f;

    ASSERT_TRUE( dmIntersection::TestFrustumSphere(frustum, dmVMath::Point3(px, py, pz), RADIUS, false) );

    ASSERT_FALSE( dmIntersection::TestFrustumSphere(frustum, dmVMath::Point3(-11.0f, py, pz), RADIUS, false) );
    ASSERT_TRUE(  dmIntersection::TestFrustumSphere(frustum, dmVMath::Point3(-9.0f, py, pz), RADIUS, false) );

    ASSERT_FALSE( dmIntersection::TestFrustumSphere(frustum, dmVMath::Point3(111.0f, py, pz), RADIUS, false) );
    ASSERT_TRUE(  dmIntersection::TestFrustumSphere(frustum, dmVMath::Point3(109.0f, py, pz), RADIUS, false) );

    ASSERT_FALSE( dmIntersection::TestFrustumSphere(frustum, dmVMath::Point3(px, -11.0f, pz), RADIUS, false) );
    ASSERT_TRUE(  dmIntersection::TestFrustumSphere(frustum, dmVMath::Point3(px,  -9.0f, pz), RADIUS, false) );

    ASSERT_FALSE( dmIntersection::TestFrustumSphere(frustum, dmVMath::Point3(px, 91.0f, pz), RADIUS, false) );
    ASSERT_TRUE(  dmIntersection::TestFrustumSphere(frustum, dmVMath::Point3(px, 89.0f, pz), RADIUS, false) );

    ASSERT_FALSE( dmIntersection::TestFrustumSphere(frustum, dmVMath::Point3(px, py, 1.0f), RADIUS, false) );
    ASSERT_TRUE(  dmIntersection::TestFrustumSphere(frustum, dmVMath::Point3(px, py, -1.0f), RADIUS, false) );

    ASSERT_FALSE( dmIntersection::TestFrustumSphere(frustum, dmVMath::Point3(px, py, -111.0f), RADIUS, false) );
    ASSERT_TRUE(  dmIntersection::TestFrustumSphere(frustum, dmVMath::Point3(px, py, -109.0f), RADIUS, false) );

    // Special test for 2D "spheres"
    // If we're using spheres to define the bounding volume for sprites, then the they will most likely always intersect the near/far
    // planes of the orthographic projecttions (e.g. the range is [0.1f, 1.0f], but the sphere has radius 2.0f).
    ASSERT_TRUE( dmIntersection::TestFrustumSphere(frustum, dmVMath::Point3(px, py, -1000.0f), RADIUS, true) );
    ASSERT_TRUE( dmIntersection::TestFrustumSphere(frustum, dmVMath::Point3(px, py,  1000.0f), RADIUS, true) );

    // If we use all 6 planes, the far plane will reject the same spheres
    ASSERT_FALSE( dmIntersection::TestFrustumSphere(frustum, dmVMath::Point3(px, py, -1000.0f), RADIUS, false) );
    ASSERT_FALSE( dmIntersection::TestFrustumSphere(frustum, dmVMath::Point3(px, py,  1000.0f), RADIUS, false) );

}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
