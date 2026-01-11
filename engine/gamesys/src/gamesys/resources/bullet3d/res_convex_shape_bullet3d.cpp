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

#include "res_convex_shape_bullet3d.h"

#include <dlib/log.h>

#include "gamesys.h"
#include <gamesys/physics_ddf.h>

namespace dmGameSystem
{
    bool AcquireResources(dmResource::HFactory factory,
                           PhysicsContextBullet3D* context,
                           const void* buffer, uint32_t buffer_size,
                           ConvexShapeResourceBullet3D* resource,
                           const char* filename)
    {
        dmPhysicsDDF::ConvexShape* convex_shape;
        dmDDF::Result e = dmDDF::LoadMessage<dmPhysicsDDF::ConvexShape>(buffer, buffer_size, &convex_shape);
        if ( e != dmDDF::RESULT_OK )
        {
            return false;
        }

        bool result = true;
        switch (convex_shape->m_ShapeType)
        {
        case dmPhysicsDDF::ConvexShape::TYPE_SPHERE:
            if (convex_shape->m_Data.m_Count != 1)
            {
                dmLogError("Invalid sphere shape");
                result = false;
            }
            else
            {
                resource->m_Shape3D = dmPhysics::NewSphereShape3D(context->m_Context, convex_shape->m_Data[0]);
            }
            break;
        case dmPhysicsDDF::ConvexShape::TYPE_BOX:
            if (convex_shape->m_Data.m_Count != 3)
            {
                dmLogError("Invalid box shape");
                result = false;
            }
            else
            {
                resource->m_Shape3D = dmPhysics::NewBoxShape3D(context->m_Context, dmVMath::Vector3(convex_shape->m_Data[0], convex_shape->m_Data[1], convex_shape->m_Data[2]));
            }
            break;
        case dmPhysicsDDF::ConvexShape::TYPE_CAPSULE:
            if (convex_shape->m_Data.m_Count != 2)
            {
                dmLogError("Invalid capsule shape");
                result = false;
            }
            else
            {
                resource->m_Shape3D = dmPhysics::NewCapsuleShape3D(context->m_Context, convex_shape->m_Data[0], convex_shape->m_Data[1]);
            }
            break;
        case dmPhysicsDDF::ConvexShape::TYPE_HULL:
            if (convex_shape->m_Data.m_Count < 9)
            {
                dmLogError("Invalid hull shape");
                result = false;
            }
            else
            {
                resource->m_Shape3D = dmPhysics::NewConvexHullShape3D(context->m_Context, &convex_shape->m_Data[0], convex_shape->m_Data.m_Count / 3);
            }
            break;
        }

        dmDDF::FreeMessage(convex_shape);
        return result;
    }

    dmResource::Result ResConvexShapeBullet3DCreate(const dmResource::ResourceCreateParams* params)
    {
        ConvexShapeResourceBullet3D* convex_shape = new ConvexShapeResourceBullet3D();
        if (AcquireResources(params->m_Factory, (PhysicsContextBullet3D*) params->m_Context, params->m_Buffer, params->m_BufferSize, convex_shape, params->m_Filename))
        {
            dmResource::SetResource(params->m_Resource, convex_shape);
            return dmResource::RESULT_OK;
        }
        else
        {
            delete convex_shape;
            return dmResource::RESULT_FORMAT_ERROR;
        }
        return dmResource::RESULT_OK;
    }

    void ReleaseResources(ConvexShapeResourceBullet3D* resource)
    {
        if (resource->m_Shape3D)
        {
            dmPhysics::DeleteCollisionShape3D(resource->m_Shape3D);
        }
    }

    dmResource::Result ResConvexShapeBullet3DDestroy(const dmResource::ResourceDestroyParams* params)
    {
        ConvexShapeResourceBullet3D* convex_shape = (ConvexShapeResourceBullet3D*)dmResource::GetResource(params->m_Resource);
        ReleaseResources(convex_shape);
        delete convex_shape;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResConvexShapeBullet3DRecreate(const dmResource::ResourceRecreateParams* params)
    {
        ConvexShapeResourceBullet3D* cs_resource = (ConvexShapeResourceBullet3D*)dmResource::GetResource(params->m_Resource);
        ConvexShapeResourceBullet3D tmp_convex_shape;
        PhysicsContextBullet3D* physics_context = (PhysicsContextBullet3D*) params->m_Context;

        if (AcquireResources(params->m_Factory, (PhysicsContextBullet3D*) params->m_Context, params->m_Buffer, params->m_BufferSize, &tmp_convex_shape, params->m_Filename))
        {
            dmPhysics::ReplaceShape3D(physics_context->m_Context, cs_resource->m_Shape3D, tmp_convex_shape.m_Shape3D);
            ReleaseResources(cs_resource);
            cs_resource->m_Shape3D = tmp_convex_shape.m_Shape3D;
            return dmResource::RESULT_OK;
        }
        else
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }
}
