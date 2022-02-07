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

#ifndef DMSDK_GAMESYS_RES_COLLISION_OBJECT_H
#define DMSDK_GAMESYS_RES_COLLISION_OBJECT_H

#include <stdint.h>

#include <dmsdk/dlib/vmath.h>
#include <gamesys/physics_ddf.h>

namespace dmPhysics
{
    typedef void* HCollisionShape3D;
    typedef void* HCollisionShape2D;
}

namespace dmGameSystem
{
    struct TileGridResource;

    struct CollisionObjectResource
    {
        CollisionObjectResource();

        uint64_t m_Mask[16];
        uint64_t m_Group;
        TileGridResource* m_TileGridResource;

        dmPhysics::HCollisionShape3D* m_Shapes3D;
        dmPhysics::HCollisionShape2D* m_Shapes2D;
        dmVMath::Vector3* m_ShapeTranslation;
        dmVMath::Quat* m_ShapeRotation;
        uint32_t m_TileGridShapeCount;
        uint32_t m_ShapeCount : 31;
        uint32_t m_TileGrid : 1;

        dmPhysicsDDF::CollisionObjectDesc* m_DDF;
    };
}

#endif
