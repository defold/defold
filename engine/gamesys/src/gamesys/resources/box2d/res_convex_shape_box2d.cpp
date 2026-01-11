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

#include "res_convex_shape_box2d.h"

#include <dlib/log.h>

#include "gamesys.h"
#include <gamesys/physics_ddf.h>

namespace dmGameSystem
{
    bool AcquireResources(dmResource::HFactory factory,
                           PhysicsContextBox2D* context,
                           const void* buffer, uint32_t buffer_size,
                           ConvexShapeResourceBox2D* resource,
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
                resource->m_Shape2D = dmPhysics::NewCircleShape2D(context->m_Context, convex_shape->m_Data[0]);
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
                resource->m_Shape2D = dmPhysics::NewBoxShape2D(context->m_Context, dmVMath::Vector3(convex_shape->m_Data[0], convex_shape->m_Data[1], convex_shape->m_Data[2]));
            }
            break;
        case dmPhysicsDDF::ConvexShape::TYPE_CAPSULE:
            dmLogError("Capsules are not supported in 2D.");
            result = false;
            break;
        case dmPhysicsDDF::ConvexShape::TYPE_HULL:
            if (convex_shape->m_Data.m_Count < 9)
            {
                dmLogError("Invalid hull shape");
                result = false;
            }
            else
            {
                const uint32_t data_size = 2 * convex_shape->m_Data.m_Count / 3;
                float* data_2d = new float[2 * convex_shape->m_Data.m_Count / 3];
                for (uint32_t i = 0; i < data_size; ++i)
                {
                    data_2d[i] = convex_shape->m_Data[i/2*3 + i%2];
                }
                resource->m_Shape2D = dmPhysics::NewPolygonShape2D(context->m_Context, data_2d, data_size/2);
                delete [] data_2d;
            }
            break;
        }

        dmDDF::FreeMessage(convex_shape);
        return result;
    }

    dmResource::Result ResConvexShapeBox2DCreate(const dmResource::ResourceCreateParams* params)
    {
        ConvexShapeResourceBox2D* convex_shape = new ConvexShapeResourceBox2D();
        if (AcquireResources(params->m_Factory, (PhysicsContextBox2D*) params->m_Context, params->m_Buffer, params->m_BufferSize, convex_shape, params->m_Filename))
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

    void ReleaseResources(ConvexShapeResourceBox2D* resource)
    {
        if (resource->m_Shape2D)
        {
            dmPhysics::DeleteCollisionShape2D(resource->m_Shape2D);
        }
    }

    dmResource::Result ResConvexShapeBox2DDestroy(const dmResource::ResourceDestroyParams* params)
    {
        ConvexShapeResourceBox2D* convex_shape = (ConvexShapeResourceBox2D*)dmResource::GetResource(params->m_Resource);
        ReleaseResources(convex_shape);
        delete convex_shape;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResConvexShapeBox2DRecreate(const dmResource::ResourceRecreateParams* params)
    {
        ConvexShapeResourceBox2D* cs_resource = (ConvexShapeResourceBox2D*)dmResource::GetResource(params->m_Resource);
        ConvexShapeResourceBox2D tmp_convex_shape;
        PhysicsContextBox2D* physics_context = (PhysicsContextBox2D*) params->m_Context;

        if (AcquireResources(params->m_Factory, (PhysicsContextBox2D*) params->m_Context, params->m_Buffer, params->m_BufferSize, &tmp_convex_shape, params->m_Filename))
        {
            dmPhysics::ReplaceShape2D(physics_context->m_Context, cs_resource->m_Shape2D, tmp_convex_shape.m_Shape2D);
            ReleaseResources(cs_resource);
            cs_resource->m_Shape2D = tmp_convex_shape.m_Shape2D;
            return dmResource::RESULT_OK;
        }
        else
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }
}
