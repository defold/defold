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

const float PI = 3.141592653;

// orthographic frustum
const float FRUSTUM_WIDTH = 100.0f;
const float FRUSTUM_HEIGHT = 80.0f;
const float FRUSTUM_NEAR = 10.0f;
const float FRUSTUM_FAR = 100.0f;

// perspective frustum
const float PER_FRUSTUM_NEAR = 10.0;
const float PER_FRUSTUM_FAR = 100.0;
const float PER_FRUSTUM_RATIO  = 1;       // width/height
const float PER_FRUSTUM_FOV = 0.5*PI;     // radians

const float PER_FRUSTUM_WIDTH_NEAR = PER_FRUSTUM_NEAR * tan(PER_FRUSTUM_FOV/2) * 2;
const float PER_FRUSTUM_HEIGHT_NEAR = PER_FRUSTUM_WIDTH_NEAR / PER_FRUSTUM_RATIO;
const float PER_FRUSTUM_WIDTH_FAR = PER_FRUSTUM_FAR * tan(PER_FRUSTUM_FOV/2) * 2;
const float PER_FRUSTUM_DEPTH = PER_FRUSTUM_FAR - PER_FRUSTUM_NEAR;

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

TEST(dmVMath, TestFrustumOBB)
{
    dmVMath::Matrix4 proj = dmVMath::Matrix4::orthographic(0.0f, FRUSTUM_WIDTH, 0.0f, FRUSTUM_HEIGHT, FRUSTUM_NEAR, FRUSTUM_FAR);

    // frustum lies on positive X axis from 0 to FRUSTUM_WIDTH, on Y from 0 to FRUSTUM_HEIGHT and on Z from -FRUSTUM_NEAR to -FRUSTUM_FAR
    dmIntersection::Frustum frustum;
    dmIntersection::CreateFrustumFromMatrix(proj, true, frustum);

    float BOX_SIDE = 1;
    dmVMath::Vector3 minPoint(0,0,0);
    dmVMath::Vector3 maxPoint(BOX_SIDE,BOX_SIDE,BOX_SIDE); // note, the box lies in the +Z axis

    dmVMath::Matrix4 trans;

    // place BB at (0,0,0), outside the frustum that starts at -FRUSTUM_NEAR
    trans = dmVMath::Matrix4::identity();
    ASSERT_FALSE(dmIntersection::TestFrustumOBB(frustum, trans, minPoint, maxPoint, false));

    //place BB to the left of the frustum
    trans = dmVMath::Matrix4::translation(dmVMath::Vector3(-BOX_SIDE-0.1,0.0,-FRUSTUM_FAR));
    ASSERT_FALSE(dmIntersection::TestFrustumOBB(frustum, trans, minPoint, maxPoint, false));
    trans = dmVMath::Matrix4::translation(dmVMath::Vector3(-BOX_SIDE+0.1,0.0,-FRUSTUM_FAR));
    ASSERT_TRUE(dmIntersection::TestFrustumOBB(frustum, trans, minPoint, maxPoint, false));

    // place BB far deep right after the frustum
    trans = dmVMath::Matrix4::translation(dmVMath::Vector3(0.0,0.0,-FRUSTUM_FAR-BOX_SIDE-0.1));
    ASSERT_FALSE(dmIntersection::TestFrustumOBB(frustum, trans, minPoint, maxPoint, false));
    trans = dmVMath::Matrix4::translation(dmVMath::Vector3(0.0,0.0,-FRUSTUM_FAR-BOX_SIDE+0.1));
    ASSERT_TRUE(dmIntersection::TestFrustumOBB(frustum, trans, minPoint, maxPoint, false));

    // place BB close to the near plane
    trans = dmVMath::Matrix4::translation(dmVMath::Vector3(0.0,0.0,-FRUSTUM_NEAR-0.1));
    ASSERT_TRUE(dmIntersection::TestFrustumOBB(frustum, trans, minPoint, maxPoint, false));
    trans = dmVMath::Matrix4::translation(dmVMath::Vector3(0.0,0.0,-FRUSTUM_NEAR+0.1));
    ASSERT_FALSE(dmIntersection::TestFrustumOBB(frustum, trans, minPoint, maxPoint, false));

    // move BB outside to the far right of the frustum +- 0.1
    trans = dmVMath::Matrix4::translation(dmVMath::Vector3(FRUSTUM_WIDTH + 0.1,0.0,-FRUSTUM_NEAR));
    ASSERT_FALSE(dmIntersection::TestFrustumOBB(frustum, trans, minPoint, maxPoint, false));
    trans = dmVMath::Matrix4::translation(dmVMath::Vector3(FRUSTUM_WIDTH - 0.1,0.0,-FRUSTUM_NEAR));
    ASSERT_TRUE(dmIntersection::TestFrustumOBB(frustum, trans, minPoint, maxPoint, false));

    // move BB outside over the top of the frustum +- 0.1
    trans = dmVMath::Matrix4::translation(dmVMath::Vector3(0.0,FRUSTUM_HEIGHT+0.1,-FRUSTUM_NEAR));
    ASSERT_FALSE(dmIntersection::TestFrustumOBB(frustum, trans, minPoint, maxPoint, false));
    trans = dmVMath::Matrix4::translation(dmVMath::Vector3(0.0,FRUSTUM_HEIGHT-0.1,-FRUSTUM_NEAR));
    ASSERT_TRUE(dmIntersection::TestFrustumOBB(frustum, trans, minPoint, maxPoint, false));

    // rotate 45 degrees and move close to the left plane
    trans = dmVMath::Matrix4::translation(dmVMath::Vector3( -sqrt(2)/2-0.1,0.0,-FRUSTUM_NEAR-BOX_SIDE)) * dmVMath::Matrix4::rotationZ(3.141592653/4); // 45 degrees counter-clockwise
    ASSERT_FALSE(dmIntersection::TestFrustumOBB(frustum, trans, minPoint, maxPoint, false));
    trans = dmVMath::Matrix4::translation(dmVMath::Vector3( -sqrt(2)/2+0.1,0.0,-FRUSTUM_NEAR-BOX_SIDE)) * dmVMath::Matrix4::rotationZ(3.141592653/4); // 45 degrees counter-clockwise
    ASSERT_TRUE(dmIntersection::TestFrustumOBB(frustum, trans, minPoint, maxPoint, false));
}



TEST(dmVMath, TestFrustumOBBPerspective)
{
    float BOX_SIDE = 1;

    dmVMath::Point3 cam_pos = dmVMath::Point3(0.0f, 0.0f, 0);
    dmVMath::Matrix4 view = Matrix4::lookAt(cam_pos, dmVMath::Point3(cam_pos.getX(), cam_pos.getY(), cam_pos.getZ()-1), dmVMath::Vector3(0,1,0)); // eye, lookat, up

    dmVMath::Matrix4 proj = dmVMath::Matrix4::perspective(PER_FRUSTUM_FOV, PER_FRUSTUM_RATIO, PER_FRUSTUM_NEAR, PER_FRUSTUM_FAR);

    dmVMath::Matrix4 view_proj = proj * view;

    dmIntersection::Frustum frustum;
    dmIntersection::CreateFrustumFromMatrix(view_proj, true, frustum);

    dmVMath::Matrix4 world;
    dmVMath::Vector3 minPoint(0, 0, 0);
    dmVMath::Vector3 maxPoint(BOX_SIDE, BOX_SIDE, BOX_SIDE);

    // left of the frustum near plane, Y = 0
    world = dmVMath::Matrix4::translation(dmVMath::Vector3(-PER_FRUSTUM_WIDTH_NEAR/2 - BOX_SIDE - 0.1f, 0,  -PER_FRUSTUM_NEAR + 0.1f));
    ASSERT_FALSE(dmIntersection::TestFrustumOBB(frustum, world, minPoint, maxPoint, false));
    world = dmVMath::Matrix4::translation(dmVMath::Vector3(-PER_FRUSTUM_WIDTH_NEAR/2 - BOX_SIDE + 0.1f, 0,  -PER_FRUSTUM_NEAR - 0.1f));
    ASSERT_TRUE(dmIntersection::TestFrustumOBB(frustum, world, minPoint, maxPoint, false));

    // center of frustum left plane
    world = dmVMath::Matrix4::translation(dmVMath::Vector3(-PER_FRUSTUM_WIDTH_NEAR/2 -PER_FRUSTUM_DEPTH/2 - BOX_SIDE - 0.1f, 0, -PER_FRUSTUM_NEAR -PER_FRUSTUM_DEPTH/2));
    ASSERT_FALSE(dmIntersection::TestFrustumOBB(frustum, world, minPoint, maxPoint, false));
    world = dmVMath::Matrix4::translation(dmVMath::Vector3(-PER_FRUSTUM_WIDTH_NEAR/2 -PER_FRUSTUM_DEPTH/2 - BOX_SIDE + 0.1f, 0, -PER_FRUSTUM_NEAR -PER_FRUSTUM_DEPTH/2));
    ASSERT_TRUE(dmIntersection::TestFrustumOBB(frustum, world, minPoint, maxPoint, false));

    // center of frustum right plane
    world = dmVMath::Matrix4::translation(dmVMath::Vector3(PER_FRUSTUM_WIDTH_NEAR/2 + PER_FRUSTUM_DEPTH/2 + 0.1f, 0, -PER_FRUSTUM_NEAR -PER_FRUSTUM_DEPTH/2));
    ASSERT_FALSE(dmIntersection::TestFrustumOBB(frustum, world, minPoint, maxPoint, false));
    world = dmVMath::Matrix4::translation(dmVMath::Vector3(PER_FRUSTUM_WIDTH_NEAR/2 + PER_FRUSTUM_DEPTH/2 - 0.1f, 0, -PER_FRUSTUM_NEAR -PER_FRUSTUM_DEPTH/2));
    ASSERT_TRUE(dmIntersection::TestFrustumOBB(frustum, world, minPoint, maxPoint, false));

    // center of frustum bottom plane
    world = dmVMath::Matrix4::translation(dmVMath::Vector3(0, -PER_FRUSTUM_HEIGHT_NEAR/2 -PER_FRUSTUM_DEPTH/2 -BOX_SIDE -0.1f, -PER_FRUSTUM_NEAR -PER_FRUSTUM_DEPTH/2));
    ASSERT_FALSE(dmIntersection::TestFrustumOBB(frustum, world, minPoint, maxPoint, false));
    world = dmVMath::Matrix4::translation(dmVMath::Vector3(0, -PER_FRUSTUM_HEIGHT_NEAR/2 -PER_FRUSTUM_DEPTH/2 -BOX_SIDE + 0.1f, -PER_FRUSTUM_NEAR -PER_FRUSTUM_DEPTH/2));
    ASSERT_TRUE(dmIntersection::TestFrustumOBB(frustum, world, minPoint, maxPoint, false));

    // center of frustum top plane
    world = dmVMath::Matrix4::translation(dmVMath::Vector3(0, PER_FRUSTUM_HEIGHT_NEAR/2 +PER_FRUSTUM_DEPTH/2  +0.1f, -PER_FRUSTUM_NEAR -PER_FRUSTUM_DEPTH/2));
    ASSERT_FALSE(dmIntersection::TestFrustumOBB(frustum, world, minPoint, maxPoint, false));
    world = dmVMath::Matrix4::translation(dmVMath::Vector3(0, PER_FRUSTUM_HEIGHT_NEAR/2 +PER_FRUSTUM_DEPTH/2  -0.1f, -PER_FRUSTUM_NEAR -PER_FRUSTUM_DEPTH/2));
    ASSERT_TRUE(dmIntersection::TestFrustumOBB(frustum, world, minPoint, maxPoint, false));

    // center of back plane
    world = dmVMath::Matrix4::translation(dmVMath::Vector3(0, 0, -PER_FRUSTUM_NEAR -PER_FRUSTUM_DEPTH -BOX_SIDE - 0.1f));
    ASSERT_FALSE(dmIntersection::TestFrustumOBB(frustum, world, minPoint, maxPoint, false));
    world = dmVMath::Matrix4::translation(dmVMath::Vector3(0, 0, -PER_FRUSTUM_NEAR -PER_FRUSTUM_DEPTH -BOX_SIDE + 0.1f));
    ASSERT_TRUE(dmIntersection::TestFrustumOBB(frustum, world, minPoint, maxPoint, false));

    // front and back plane should be ingored with skip_near_far = true
    world = dmVMath::Matrix4::translation(dmVMath::Vector3(0, 0, -PER_FRUSTUM_NEAR -PER_FRUSTUM_DEPTH -BOX_SIDE - 0.1f));
    ASSERT_TRUE(dmIntersection::TestFrustumOBB(frustum, world, minPoint, maxPoint, true));
    world = dmVMath::Matrix4::translation(dmVMath::Vector3(0, 0, -PER_FRUSTUM_NEAR + 0.1f));
    ASSERT_TRUE(dmIntersection::TestFrustumOBB(frustum, world, minPoint, maxPoint, true));

}


int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
