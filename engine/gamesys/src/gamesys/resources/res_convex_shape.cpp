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

#include "box2d/res_convex_shape_box2d.h"
#include "bullet3d/res_convex_shape_bullet3d.h"

#include <dlib/log.h>

#include "gamesys.h"
#include <gamesys/physics_ddf.h>

namespace dmGameSystem
{
    dmResource::Result ResConvexShapeCreate(const dmResource::ResourceCreateParams* params)
    {
        PhysicsContext* physics_context = (PhysicsContext*) params->m_Context;
        if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BOX2D)
        {
            return ResConvexShapeBox2DCreate(params);
        }
        else if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BULLET3D)
        {
            return ResConvexShapeBullet3DCreate(params);
        }
        return dmResource::RESULT_DDF_ERROR;
    }

    dmResource::Result ResConvexShapeDestroy(const dmResource::ResourceDestroyParams* params)
    {
        PhysicsContext* physics_context = (PhysicsContext*) params->m_Context;
        if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BOX2D)
        {
            return ResConvexShapeBox2DDestroy(params);
        }
        else if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BULLET3D)
        {
            return ResConvexShapeBullet3DDestroy(params);
        }
        return dmResource::RESULT_DDF_ERROR;
    }

    dmResource::Result ResConvexShapeRecreate(const dmResource::ResourceRecreateParams* params)
    {
        PhysicsContext* physics_context = (PhysicsContext*) params->m_Context;
        if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BOX2D)
        {
            return ResConvexShapeBox2DRecreate(params);
        }
        else if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BULLET3D)
        {
            return ResConvexShapeBullet3DRecreate(params);
        }
        return dmResource::RESULT_DDF_ERROR;
    }
}
