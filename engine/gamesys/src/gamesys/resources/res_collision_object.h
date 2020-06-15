// Copyright 2020 The Defold Foundation
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

#ifndef DM_GAMESYS_RES_COLLISION_OBJECT_H
#define DM_GAMESYS_RES_COLLISION_OBJECT_H

#include <string.h>
#include <dlib/array.h>
#include <resource/resource.h>
#include <physics/physics.h>

#include "res_convex_shape.h"
#include "res_tilegrid.h"

#include "../proto/physics_ddf.h"

namespace dmGameSystem
{
    struct CollisionObjectResource
    {
        inline CollisionObjectResource()
        {
            memset(this, 0, sizeof(CollisionObjectResource));
        }

        uint64_t m_Mask[16];
        uint64_t m_Group;
        TileGridResource* m_TileGridResource;

        dmPhysics::HCollisionShape3D* m_Shapes3D;
        dmPhysics::HCollisionShape2D* m_Shapes2D;
        Vectormath::Aos::Vector3* m_ShapeTranslation;
        Vectormath::Aos::Quat* m_ShapeRotation;
        uint32_t m_TileGridShapeCount;
        uint32_t m_ShapeCount;

        dmPhysicsDDF::CollisionObjectDesc* m_DDF;
        uint32_t m_TileGrid : 1;
    };

    dmResource::Result ResCollisionObjectCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResCollisionObjectDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResCollisionObjectRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResCollisionObjectRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif
