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

#ifndef DMSDK_PHYSICS_BOX3D_HPP
#define DMSDK_PHYSICS_BOX3D_HPP

#include <box3d/id.h>
#include <dmsdk/dlib/vmath.h>

/*# Box3D collision component motion
 * Access engine-owned Box3D bodies between simulation steps.
 * @document
 * @name Box3D motion
 * @namespace dmPhysics
 * @language C++
 */
namespace dmPhysics
{
    /*# world pose and velocities in game units
     * @struct
     * @name Box3DBodyMotion
     * @member m_Position [type:dmVMath::Vector3] world position in game units
     * @member m_Rotation [type:dmVMath::Quat] world orientation as a unit quaternion
     * @member m_LinearVelocity [type:dmVMath::Vector3] game units per second
     * @member m_AngularVelocity [type:dmVMath::Vector3] radians per second
     */
    struct Box3DBodyMotion
    {
        dmVMath::Vector3 m_Position;
        dmVMath::Quat    m_Rotation;
        dmVMath::Vector3 m_LinearVelocity;
        dmVMath::Vector3 m_AngularVelocity;
    };

    /*# read a component body's current motion
     * Reads pending velocities when the collision component is disabled.
     * @name GetBox3DBodyMotion
     * @param id [type:b3BodyId] valid Defold collision component body
     * @param motion [type:dmPhysics::Box3DBodyMotion*] caller-owned output
     */
    void GetBox3DBodyMotion(b3BodyId id, Box3DBodyMotion* motion);

    /*# set a component body's motion and synchronize its owning game object
     * Velocities and the requested sleep state also apply to bodies awaiting factory enable.
     * @name SetBox3DBodyMotion
     * @param id [type:b3BodyId] valid Defold collision component body
     * @param motion [type:dmPhysics::Box3DBodyMotion] world pose and velocities to apply
     * @param awake [type:bool] wake the body when true, put it to sleep when false
     */
    void SetBox3DBodyMotion(b3BodyId id, const Box3DBodyMotion& motion, bool awake);
}

#endif // DMSDK_PHYSICS_BOX3D_HPP
